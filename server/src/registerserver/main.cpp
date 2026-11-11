#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cstring>
#include <cstdlib>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mysql.h>
#include "SRP6.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "mswsock.lib")
#pragma comment(lib, "libmysql.lib")

struct HttpRequest
{
    std::string method;
    std::string path;
    std::string body;
    std::map<std::string, std::string> headers;
};

static bool ParseHttpRequest(const std::string& raw, HttpRequest& req)
{
    size_t pos = raw.find("\r\n");
    if (pos == std::string::npos) return false;

    std::string requestLine = raw.substr(0, pos);
    size_t sp1 = requestLine.find(' ');
    if (sp1 == std::string::npos) return false;
    size_t sp2 = requestLine.find(' ', sp1 + 1);
    if (sp2 == std::string::npos) return false;

    req.method = requestLine.substr(0, sp1);
    req.path = requestLine.substr(sp1 + 1, sp2 - sp1 - 1);

    size_t headerEnd = raw.find("\r\n\r\n");
    if (headerEnd == std::string::npos) return false;

    std::string headerBlock = raw.substr(pos + 2, headerEnd - pos - 2);
    size_t hPos = 0;
    while (hPos < headerBlock.size())
    {
        size_t hEnd = headerBlock.find("\r\n", hPos);
        if (hEnd == std::string::npos) hEnd = headerBlock.size();
        std::string headerLine = headerBlock.substr(hPos, hEnd - hPos);
        size_t colon = headerLine.find(':');
        if (colon != std::string::npos)
        {
            std::string key = headerLine.substr(0, colon);
            std::string val = headerLine.substr(colon + 1);
            if (!val.empty() && val[0] == ' ') val = val.substr(1);
            req.headers[key] = val;
        }
        hPos = hEnd + 2;
        if (hPos >= headerBlock.size()) break;
    }

    size_t bodyStart = headerEnd + 4;
    if (bodyStart < raw.size())
        req.body = raw.substr(bodyStart);

    return true;
}

static std::string JsonGetString(const std::string& json, const std::string& key)
{
    std::string search = "\"" + key + "\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";

    size_t colon = json.find(':', pos + search.size());
    if (colon == std::string::npos) return "";

    size_t quoteStart = json.find('"', colon + 1);
    if (quoteStart == std::string::npos) return "";

    size_t quoteEnd = json.find('"', quoteStart + 1);
    if (quoteEnd == std::string::npos) return "";

    return json.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
}

static std::string BuildHttpResponse(int statusCode, const std::string& statusText,
                                     const std::string& contentType,
                                     const std::string& body)
{
    std::string resp;
    resp += "HTTP/1.1 " + std::to_string(statusCode) + " " + statusText + "\r\n";
    resp += "Content-Type: " + contentType + "\r\n";
    resp += "Content-Length: " + std::to_string(body.size()) + "\r\n";
    resp += "Access-Control-Allow-Origin: *\r\n";
    resp += "Connection: close\r\n";
    resp += "\r\n";
    resp += body;
    return resp;
}

static std::string HandleRegister(const std::string& body, MYSQL* mysql)
{
    std::string username = JsonGetString(body, "username");
    std::string password = JsonGetString(body, "password");
    std::string email = JsonGetString(body, "email");

    if (username.empty() || password.empty())
    {
        return BuildHttpResponse(400, "Bad Request", "application/json",
            "{\"error\":\"username and password are required\"}");
    }

    if (username.size() > 32 || password.size() > 64)
    {
        return BuildHttpResponse(400, "Bad Request", "application/json",
            "{\"error\":\"username or password too long\"}");
    }

    std::string checkQuery = "SELECT id FROM ndv_auth.account WHERE username = '" + username + "'";
    if (mysql_query(mysql, checkQuery.c_str()) != 0)
    {
        return BuildHttpResponse(500, "Internal Server Error", "application/json",
            "{\"error\":\"database query failed\"}");
    }
    MYSQL_RES* res = mysql_store_result(mysql);
    if (res && mysql_num_rows(res) > 0)
    {
        mysql_free_result(res);
        return BuildHttpResponse(409, "Conflict", "application/json",
            "{\"error\":\"username already exists\"}");
    }
    if (res) mysql_free_result(res);

    Salt salt = SRP6_GenerateSalt();
    Verifier verifier = SRP6_CalculateVerifier(username, password, salt);

    MYSQL_STMT* stmt = mysql_stmt_init(mysql);
    if (!stmt)
    {
        return BuildHttpResponse(500, "Internal Server Error", "application/json",
            "{\"error\":\"stmt init failed\"}");
    }

    const char* insertSql =
        "INSERT INTO ndv_auth.account (username, salt, verifier, email, joindate, expansion) "
        "VALUES (?, ?, ?, ?, NOW(), 0)";
    if (mysql_stmt_prepare(stmt, insertSql, strlen(insertSql)) != 0)
    {
        mysql_stmt_close(stmt);
        return BuildHttpResponse(500, "Internal Server Error", "application/json",
            "{\"error\":\"stmt prepare failed\"}");
    }

    MYSQL_BIND bind[4];
    memset(bind, 0, sizeof(bind));

    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (char*)username.c_str();
    bind[0].length = new unsigned long(username.size());

    bind[1].buffer_type = MYSQL_TYPE_BLOB;
    bind[1].buffer = salt.data();
    bind[1].length = new unsigned long(salt.size());

    bind[2].buffer_type = MYSQL_TYPE_BLOB;
    bind[2].buffer = verifier.data();
    bind[2].length = new unsigned long(verifier.size());

    bind[3].buffer_type = MYSQL_TYPE_STRING;
    bind[3].buffer = (char*)(email.empty() ? "" : email.c_str());
    bind[3].length = new unsigned long(email.size());

    if (mysql_stmt_bind_param(stmt, bind) != 0)
    {
        mysql_stmt_close(stmt);
        return BuildHttpResponse(500, "Internal Server Error", "application/json",
            "{\"error\":\"stmt bind failed\"}");
    }

    if (mysql_stmt_execute(stmt) != 0)
    {
        mysql_stmt_close(stmt);
        return BuildHttpResponse(500, "Internal Server Error", "application/json",
            "{\"error\":\"stmt execute failed (maybe duplicate username)\"}");
    }

    mysql_stmt_close(stmt);

    std::string saltHex = HexEncode(salt.data(), salt.size());
    std::string verHex = HexEncode(verifier.data(), verifier.size());
    std::string jsonBody = "{\"status\":\"ok\",\"username\":\"" + username +
        "\",\"salt\":\"" + saltHex +
        "\",\"verifier\":\"" + verHex + "\"}";

    return BuildHttpResponse(201, "Created", "application/json", jsonBody);
}

int main()
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }

    MYSQL* mysql = mysql_init(nullptr);
    if (!mysql)
    {
        std::cerr << "mysql_init failed\n";
        WSACleanup();
        return 1;
    }

    if (!mysql_real_connect(mysql, "127.0.0.1", "root", "root", "ndv_auth", 3306, nullptr, 0))
    {
        std::cerr << "mysql_connect failed: " << mysql_error(mysql) << "\n";
        std::cerr << "Попытка без пароля...\n";
        mysql_close(mysql);
        mysql = mysql_init(nullptr);
        if (!mysql_real_connect(mysql, "127.0.0.1", "root", nullptr, "ndv_auth", 3306, nullptr, 0))
        {
            std::cerr << "mysql_connect (no password) failed: " << mysql_error(mysql) << "\n";
            mysql_close(mysql);
            WSACleanup();
            return 1;
        }
    }

    std::cout << "registerserver: подключено к MySQL ndv_auth\n";

    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET)
    {
        std::cerr << "socket() failed\n";
        mysql_close(mysql);
        WSACleanup();
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8080);

    if (bind(listenSock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        std::cerr << "bind() failed\n";
        closesocket(listenSock);
        mysql_close(mysql);
        WSACleanup();
        return 1;
    }

    if (listen(listenSock, SOMAXCONN) == SOCKET_ERROR)
    {
        std::cerr << "listen() failed\n";
        closesocket(listenSock);
        mysql_close(mysql);
        WSACleanup();
        return 1;
    }

    u_long nonblocking = 1;
    ioctlsocket(listenSock, FIONBIO, &nonblocking);

    std::cout << "registerserver: listening on port 8080 (POST /api/register)\n";

    while (true)
    {
        fd_set readSet, exceptSet;
        FD_ZERO(&readSet);
        FD_ZERO(&exceptSet);
        FD_SET(listenSock, &readSet);
        FD_SET(listenSock, &exceptSet);

        timeval tv{};
        tv.tv_sec = 0;
        tv.tv_usec = 100000;

        int result = select(0, &readSet, nullptr, &exceptSet, &tv);
        if (result == SOCKET_ERROR) continue;

        if (FD_ISSET(listenSock, &readSet) || FD_ISSET(listenSock, &exceptSet))
        {
            sockaddr_in clientAddr{};
            int clientLen = sizeof(clientAddr);
            SOCKET clientSock = accept(listenSock, (sockaddr*)&clientAddr, &clientLen);
            if (clientSock == INVALID_SOCKET) continue;

            u_long nb = 1;
            ioctlsocket(clientSock, FIONBIO, &nb);

            fd_set rset;
            FD_ZERO(&rset);
            FD_SET(clientSock, &rset);
            timeval timeout{};
            timeout.tv_sec = 5;
            timeout.tv_usec = 0;

            if (select(0, &rset, nullptr, nullptr, &timeout) <= 0)
            {
                closesocket(clientSock);
                continue;
            }

            std::string rawRequest;
            char buf[4096];

            while (rawRequest.size() < 65536)
            {
                fd_set rset2;
                FD_ZERO(&rset2);
                FD_SET(clientSock, &rset2);
                timeval tmo{};
                tmo.tv_sec = 2;
                tmo.tv_usec = 0;

                if (select(0, &rset2, nullptr, nullptr, &tmo) <= 0) break;

                int n = recv(clientSock, buf, sizeof(buf), 0);
                if (n <= 0) break;
                rawRequest.append(buf, n);

                if (rawRequest.find("\r\n\r\n") != std::string::npos)
                {
                    size_t clPos = rawRequest.find("Content-Length: ");
                    if (clPos != std::string::npos)
                    {
                        size_t clEnd = rawRequest.find("\r\n", clPos);
                        std::string clStr = rawRequest.substr(clPos + 16, clEnd - clPos - 16);
                        int contentLength = std::atoi(clStr.c_str());
                        size_t bodyStart = rawRequest.find("\r\n\r\n");
                        if (bodyStart != std::string::npos)
                        {
                            size_t bodySize = rawRequest.size() - bodyStart - 4;
                            if (bodySize >= (size_t)contentLength) break;
                        }
                    }
                    else
                    {
                        break;
                    }
                }
            }

            HttpRequest req;
            if (ParseHttpRequest(rawRequest, req))
            {
                if (req.method == "POST" && req.path == "/api/register")
                {
                    std::string response = HandleRegister(req.body, mysql);
                    send(clientSock, response.c_str(), (int)response.size(), 0);
                }
                else if (req.method == "GET" && req.path == "/")
                {
                    std::string response = BuildHttpResponse(200, "OK", "text/plain",
                        "Night of the Dark Raven — Registration API v1.0\nPOST /api/register {\"username\",\"password\",\"email\"}");
                    send(clientSock, response.c_str(), (int)response.size(), 0);
                }
                else
                {
                    std::string response = BuildHttpResponse(404, "Not Found", "application/json",
                        "{\"error\":\"endpoint not found\"}");
                    send(clientSock, response.c_str(), (int)response.size(), 0);
                }
            }
            else
            {
                std::string response = BuildHttpResponse(400, "Bad Request", "text/plain", "Bad request");
                send(clientSock, response.c_str(), (int)response.size(), 0);
            }

            closesocket(clientSock);
        }
    }

    closesocket(listenSock);
    mysql_close(mysql);
    WSACleanup();
    return 0;
}