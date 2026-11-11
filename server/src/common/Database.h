#pragma once
#include <cstdint>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>
#include <mysql.h>

#pragma comment(lib, "libmysql.lib")

// РћР±С‘СЂС‚РєР° РЅР°Рґ РѕРґРЅРёРј MYSQL-СЃРѕРµРґРёРЅРµРЅРёРµРј. РћРґРёРЅ СЌРєР·РµРјРїР»СЏСЂ РЅР° РїСЂРѕС†РµСЃСЃ/РїРѕС‚РѕРє:
// РІРµСЃСЊ СЃРµС‚РµРІРѕР№ С†РёРєР» РѕРґРЅРѕРїРѕС‚РѕС‡РЅС‹Р№, РїРѕСЌС‚РѕРјСѓ РґРѕСЃС‚Р°С‚РѕС‡РЅРѕ РјСЊСЋС‚РµРєСЃР° РЅР° СЃР»СѓС‡Р°Р№
// Р±СѓРґСѓС‰РµРіРѕ РїСѓР»Р°.
class Database
{
public:
    Database() = default;
    ~Database() { Close(); }

    Database(Database const&) = delete;
    Database& operator=(Database const&) = delete;

    bool Open(const std::string& host, unsigned port, const std::string& user,
              const std::string& password, const std::string& db)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        CloseLocked();

        _conn = mysql_init(nullptr);
        if (!_conn) return false;

        mysql_options(_conn, MYSQL_SET_CHARSET_NAME, "utf8mb4");

        if (!mysql_real_connect(_conn, host.c_str(), user.c_str(), password.c_str(),
                                db.c_str(), port, nullptr, 0))
        {
            std::cerr << "[db] connect failed: " << mysql_error(_conn) << "\n";
            mysql_close(_conn);
            _conn = nullptr;
            return false;
        }
        return true;
    }

    void Close()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        CloseLocked();
    }

    bool IsOpen() const { return _conn != nullptr; }

    MYSQL* Raw() { return _conn; }

    MYSQL_STMT* Prepare(const std::string& sql)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_conn) return nullptr;
        MYSQL_STMT* stmt = mysql_stmt_init(_conn);
        if (!stmt) return nullptr;
        if (mysql_stmt_prepare(stmt, sql.c_str(), static_cast<unsigned long>(sql.size())) != 0)
        {
            std::cerr << "[db] prepare failed: " << mysql_stmt_error(stmt) << "\n";
            mysql_stmt_close(stmt);
            return nullptr;
        }
        return stmt;
    }

    // РџРѕР»РЅРѕСЃС‚СЊСЋ Р±СѓС„РµСЂРёР·РѕРІР°РЅРЅС‹Р№ СЂРµР·СѓР»СЊС‚Р°С‚ РѕР±С‹С‡РЅРѕРіРѕ Р·Р°РїСЂРѕСЃР° (Р±РµР· РїРѕР»СЊР·РѕРІР°С‚РµР»СЊСЃРєРёС… РґР°РЅРЅС‹С…).
    bool Query(const std::string& sql, std::vector<std::vector<std::string>>& out)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        out.clear();
        if (!_conn) return false;

        if (mysql_real_query(_conn, sql.c_str(), static_cast<unsigned long>(sql.size())) != 0)
        {
            std::cerr << "[db] query failed: " << mysql_error(_conn) << "\n";
            return false;
        }

        MYSQL_RES* res = mysql_store_result(_conn);
        if (!res) return true;

        unsigned fields = mysql_num_fields(res);
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res)) != nullptr)
        {
            unsigned long* lengths = mysql_fetch_lengths(res);
            std::vector<std::string> r;
            r.reserve(fields);
            for (unsigned i = 0; i < fields; ++i)
                r.emplace_back(row[i] ? std::string(row[i], lengths[i]) : std::string());
            out.push_back(std::move(r));
        }
        mysql_free_result(res);
        return true;
    }

    std::string Escape(const std::string& s)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (!_conn) return std::string();
        std::vector<char> buf(s.size() * 2 + 1);
        unsigned long len = mysql_real_escape_string(_conn, buf.data(), s.c_str(),
                                                    static_cast<unsigned long>(s.size()));
        return std::string(buf.data(), len);
    }

    std::string LastError() const { return _conn ? mysql_error(_conn) : "not connected"; }

private:
    void CloseLocked()
    {
        if (_conn)
        {
            mysql_close(_conn);
            _conn = nullptr;
        }
    }

    MYSQL* _conn = nullptr;
    mutable std::mutex _mutex;
};
