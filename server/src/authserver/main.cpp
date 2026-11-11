#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mysql.h>

#include <algorithm>
#include <cstring>
#include <ctime>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "Config.h"
#include "Database.h"
#include "Network.h"
#include "Opcodes.h"
#include "SRP6.h"
#include "TokenStore.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "libmysql.lib")

namespace
{
    struct RealmInfo
    {
        std::string name;
        std::string address;
        uint16_t port = 8085;
    };

    struct AccountInfo
    {
        uint32_t id = 0;
        std::string username;
        Salt salt{};
        Verifier verifier{};
        bool locked = false;
        uint8_t expansion = 0;
    };

    std::string ToLowerAscii(std::string s)
    {
        for (char& c : s)
            if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
        return s;
    }

    bool LoadAccount(Database& db, const std::string& username, AccountInfo& out)
    {
        MYSQL_STMT* stmt = db.Prepare(
            "SELECT id, username, salt, verifier, locked, expansion "
            "FROM account WHERE username = ? LIMIT 1");
        if (!stmt) return false;

        std::string normalized = Utf8ToUpperOnlyLatin(username);
        unsigned long nameLen = static_cast<unsigned long>(normalized.size());

        MYSQL_BIND in[1];
        memset(in, 0, sizeof(in));
        in[0].buffer_type = MYSQL_TYPE_STRING;
        in[0].buffer = const_cast<char*>(normalized.c_str());
        in[0].buffer_length = nameLen;
        in[0].length = &nameLen;

        if (mysql_stmt_bind_param(stmt, in) != 0 || mysql_stmt_execute(stmt) != 0)
        {
            mysql_stmt_close(stmt);
            return false;
        }

        uint32_t id = 0;
        char nameBuf[128] = {0};
        uint8_t saltBuf[32] = {0};
        uint8_t verBuf[32] = {0};
        signed char locked = 0;
        unsigned char expansion = 0;

        bool isNull[6] = {0, 0, 0, 0, 0, 0};
        unsigned long lenId = sizeof(id), lenName = sizeof(nameBuf), lenSalt = sizeof(saltBuf),
                      lenVer = sizeof(verBuf), lenLocked = sizeof(locked), lenExp = sizeof(expansion);

        MYSQL_BIND res[6];
        memset(res, 0, sizeof(res));
        res[0].buffer_type = MYSQL_TYPE_LONG;
        res[0].buffer = &id;
        res[0].length = &lenId;
        res[0].is_null = &isNull[0];

        res[1].buffer_type = MYSQL_TYPE_STRING;
        res[1].buffer = nameBuf;
        res[1].buffer_length = sizeof(nameBuf);
        res[1].length = &lenName;
        res[1].is_null = &isNull[1];

        res[2].buffer_type = MYSQL_TYPE_BLOB;
        res[2].buffer = saltBuf;
        res[2].buffer_length = sizeof(saltBuf);
        res[2].length = &lenSalt;
        res[2].is_null = &isNull[2];

        res[3].buffer_type = MYSQL_TYPE_BLOB;
        res[3].buffer = verBuf;
        res[3].buffer_length = sizeof(verBuf);
        res[3].length = &lenVer;
        res[3].is_null = &isNull[3];

        res[4].buffer_type = MYSQL_TYPE_TINY;
        res[4].buffer = &locked;
        res[4].length = &lenLocked;
        res[4].is_null = &isNull[4];

        res[5].buffer_type = MYSQL_TYPE_TINY;
        res[5].buffer = &expansion;
        res[5].is_unsigned = 1;
        res[5].length = &lenExp;
        res[5].is_null = &isNull[5];

        if (mysql_stmt_bind_result(stmt, res) != 0)
        {
            mysql_stmt_close(stmt);
            return false;
        }

        bool found = mysql_stmt_fetch(stmt) == 0 && !isNull[0];
        if (found)
        {
            out.id = id;
            out.username.assign(nameBuf, std::min<unsigned long>(lenName, sizeof(nameBuf)));
            std::memcpy(out.salt.data(), saltBuf, 32);
            std::memcpy(out.verifier.data(), verBuf, 32);
            out.locked = locked != 0;
            out.expansion = expansion;
        }
        mysql_stmt_close(stmt);
        return found;
    }

    bool LoadRealm(Database& db, RealmInfo& out)
    {
        std::vector<std::vector<std::string>> rows;
        if (!db.Query("SELECT name, address, port FROM realm ORDER BY id LIMIT 1", rows))
            return false;
        if (rows.empty() || rows[0].size() < 3) return false;

        out.name = rows[0][0];
        out.address = rows[0][1];
        out.port = static_cast<uint16_t>(std::atoi(rows[0][2].c_str()));
        if (out.port == 0) out.port = 8085;
        return true;
    }

    void UpdateLastIp(Database& db, uint32_t accountId, const std::string& ip)
    {
        MYSQL_STMT* stmt = db.Prepare("UPDATE account SET last_ip = ? WHERE id = ?");
        if (!stmt) return;

        unsigned long ipLen = static_cast<unsigned long>(ip.size());
        unsigned long idLen = sizeof(accountId);

        MYSQL_BIND bind[2];
        memset(bind, 0, sizeof(bind));
        bind[0].buffer_type = MYSQL_TYPE_STRING;
        bind[0].buffer = const_cast<char*>(ip.c_str());
        bind[0].buffer_length = ipLen;
        bind[0].length = &ipLen;
        bind[1].buffer_type = MYSQL_TYPE_LONG;
        bind[1].buffer = &accountId;
        bind[1].length = &idLen;

        if (mysql_stmt_bind_param(stmt, bind) == 0)
            mysql_stmt_execute(stmt);
        mysql_stmt_close(stmt);
    }

    struct AuthSettings
    {
        uint16_t listenPort = 3724;
        bool enableSRP6 = true;
        uint32_t tokenTtl = 86400;
        int maxFailedAttempts = 5;
        int lockoutMinutes = 30;
    };

    std::string ClientIp(SOCKET sock)
    {
        sockaddr_in addr{};
        int len = sizeof(addr);
        if (getpeername(sock, reinterpret_cast<sockaddr*>(&addr), &len) != 0)
            return "unknown";
        char buf[INET_ADDRSTRLEN] = {0};
        if (!inet_ntop(AF_INET, &addr.sin_addr, buf, sizeof(buf)))
            return "unknown";
        return buf;
    }
    Database* g_db = nullptr;
    TokenStore* g_tokens = nullptr;
    RealmInfo g_realm{};
    AuthSettings g_settings{};
}

class AuthSession : public Session
{
public:
    using Session::Session;

protected:
    void HandlePacket(uint16_t opcode, ByteBuffer& p) override
    {
        switch (opcode)
        {
            case CMSG_AUTH_LOGON:
            {
                std::string account, password;
                p >> account >> password;
                HandleLogon(account, password);
                break;
            }
            default:
                break;
        }
    }

private:
    void SendResult(uint8_t result)
    {
        ByteBuffer out;
        out << result;
        Send(SMSG_AUTH_LOGON_RESULT, out);
    }

    void HandleLogon(const std::string& account, const std::string& password)
    {
        const std::string ip = ClientIp(GetSocket());

        if (account.empty() || password.empty() || account.size() > 32 || password.size() > 64)
        {
            SendResult(AUTH_FAILED);
            return;
        }

        AccountInfo info;
        if (!LoadAccount(*g_db, account, info))
        {
            std::cout << "[auth] unknown account '" << account << "' from " << ip << "\n";
            SendResult(AUTH_FAILED);
            return;
        }

        if (info.locked)
        {
            SendResult(AUTH_BANNED);
            return;
        }

        if (!SRP6_VerifyPassword(info.username, password, info.salt, info.verifier))
        {
            std::cout << "[auth] bad password for '" << info.username << "' from " << ip << "\n";
            SendResult(AUTH_FAILED);
            return;
        }

        uint64_t token = g_tokens->Issue(info.id, g_settings.tokenTtl);
        if (token == 0)
        {
            std::cerr << "[auth] token issue failed for account " << info.id << "\n";
            SendResult(AUTH_FAILED);
            return;
        }

        UpdateLastIp(*g_db, info.id, ip);

        ByteBuffer out;
        out << static_cast<uint8_t>(AUTH_OK);
        out << token;
        out << g_realm.name;
        out << g_realm.address;
        out << static_cast<uint16_t>(g_realm.port);
        Send(SMSG_AUTH_LOGON_RESULT, out);

        std::cout << "[auth] login ok: " << info.username << " (account " << info.id
                  << ") from " << ip << ", token issued" << std::endl;

        // Кадр ещё в очереди: Close() сразу с shutdown(SD_BOTH) обрывал его до отправки.
        // Выставляем флаг «закрыть после отправки» — цикл сделает это в TrySend().
        CloseAfterFlush();
    }
};

int main(int argc, char** argv)
{
    try
    {
        WinsockInitializer winsock;

        Config cfg;
        const char* configPath = (argc > 1) ? argv[1] : "authserver.conf";
        if (!cfg.Load(configPath))
            std::cout << "[auth] config not found (" << configPath << "), using defaults\n";

        AuthSettings settings;
        settings.listenPort = static_cast<uint16_t>(
            cfg.GetInt("Network", "ListenPort", 3724));
        settings.enableSRP6 = cfg.GetBool("Misc", "EnableSRP6", true);
        settings.tokenTtl = static_cast<uint32_t>(
            cfg.GetInt("Security", "SessionTokenTTL", 86400));
        settings.maxFailedAttempts = cfg.GetInt("Security", "MaxFailedAttempts", 5);
        settings.lockoutMinutes = cfg.GetInt("Security", "LockoutDurationMinutes", 30);

        Database db;
        std::string host = cfg.GetString("Database", "DBHost", "127.0.0.1");
        unsigned port = static_cast<unsigned>(cfg.GetInt("Database", "DBPort", 3306));
        std::string user = cfg.GetString("Database", "DBUser", "root");
        std::string pass = cfg.GetString("Database", "DBPassword", "root");
        std::string name = cfg.GetString("Database", "DBName", "ndv_auth");

        if (!db.Open(host, port, user, pass, name))
        {
            std::cerr << "[auth] cannot connect to MySQL " << host << ":" << port
                      << "/" << name << "\n";
            return 1;
        }
        std::cout << "[auth] connected to MySQL " << host << ":" << port << "/" << name << "\n";

        if (!TokenStore::EnsureTable(db))
        {
            std::cerr << "[auth] cannot ensure session_token table\n";
            return 1;
        }

        RealmInfo realm;
        if (!LoadRealm(db, realm))
        {
            std::cerr << "[auth] no realm row in DB, using defaults\n";
            realm.name = cfg.GetString("Realm", "DefaultRealmName", "Night of the Dark Raven");
            realm.address = cfg.GetString("Realm", "DefaultRealmAddress", "127.0.0.1");
            realm.port = static_cast<uint16_t>(cfg.GetInt("Realm", "DefaultRealmPort", 8085));
        }
        std::cout << "[auth] realm: " << realm.name << " -> "
                  << realm.address << ":" << realm.port << "\n";
        std::cout.flush();

        TokenStore tokens(db);
        tokens.PurgeExpired();

        g_db = &db;
        g_tokens = &tokens;
        g_realm = realm;
        g_settings = settings;

        Acceptor<AuthSession> acceptor(settings.listenPort);
        acceptor.Run();
        return 0;
    }
    catch (std::exception const& e)
    {
        std::cerr << "[auth] fatal: " << e.what() << "\n";
        return 1;
    }
}
