#pragma once
#include <cstdint>
#include <iostream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <openssl/rand.h>
#include "Database.h"
#include "SRP6.h"

// РўР°Р±Р»РёС†Р° СЃРµСЃСЃРёРѕРЅРЅС‹С… С‚РѕРєРµРЅРѕРІ (ndv_auth.session_token).
//
// Р—Р°РјРµРЅСЏРµС‚ С„Р°Р№Р»РѕРІС‹Р№ authShared.txt: С‚РѕРєРµРЅ РїСЂРёРІСЏР·Р°РЅ Рє Р°РєРєР°СѓРЅС‚Сѓ, РѕРґРЅРѕСЂР°Р·РѕРІС‹Р№,
// РёРјРµРµС‚ TTL Рё РЅРµ РєРѕРЅС„Р»РёРєС‚СѓРµС‚ РјРµР¶РґСѓ РѕРґРЅРѕРІСЂРµРјРµРЅРЅС‹РјРё Р»РѕРіРёРЅР°РјРё.
struct SessionToken
{
    uint64_t value = 0;
    uint32_t accountId = 0;
    uint32_t expiresAt = 0; // unix time
};

class TokenStore
{
public:
    explicit TokenStore(Database& db) : _db(db) {}

    static bool EnsureTable(Database& db)
    {
        const char* sql =
            "CREATE TABLE IF NOT EXISTS `session_token` ("
            " `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,"
            " `token` BINARY(8) NOT NULL,"
            " `account_id` INT UNSIGNED NOT NULL,"
            " `created_at` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
            " `expires_at` BIGINT UNSIGNED NOT NULL,"
            " `consumed_at` TIMESTAMP NULL DEFAULT NULL,"
            " PRIMARY KEY (`id`),"
            " UNIQUE KEY `uk_token` (`token`),"
            " KEY `idx_account` (`account_id`),"
            " CONSTRAINT `fk_token_account` FOREIGN KEY (`account_id`)"
            "   REFERENCES `account`(`id`) ON DELETE CASCADE"
            ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4";
        std::vector<std::vector<std::string>> ignored;
        return db.Query(sql, ignored);
    }

    // Р’С‹РґР°С‘С‚ С‚РѕРєРµРЅ Р°РєРєР°СѓРЅС‚Сѓ. returns 0 РїСЂРё РѕС€РёР±РєРµ.
    uint64_t Issue(uint32_t accountId, uint32_t ttlSeconds)
    {
        uint8_t raw[8];
        if (RAND_bytes(raw, sizeof(raw)) != 1) return 0;

        uint64_t token = 0;
        for (size_t i = 0; i < sizeof(raw); ++i)
            token |= static_cast<uint64_t>(raw[i]) << (8 * i);
        if (token == 0) return 0;

        uint64_t now = static_cast<uint64_t>(time(nullptr));
        uint64_t expires = now + ttlSeconds;

        PurgeExpired();

        MYSQL_STMT* stmt = _db.Prepare(
            "INSERT INTO session_token (token, account_id, expires_at) VALUES (?, ?, ?)");
        if (!stmt) return 0;

        MYSQL_BIND bind[3];
        memset(bind, 0, sizeof(bind));

        uint8_t tokenBytes[8];
        for (size_t i = 0; i < 8; ++i)
            tokenBytes[i] = static_cast<uint8_t>((token >> (8 * (7 - i))) & 0xFF);

        unsigned long tokenLen = 8;
        uint32_t acc = accountId;
        unsigned long accLen = sizeof(acc);
        unsigned long long exp = expires;
        unsigned long expLen = sizeof(exp);

        bind[0].buffer_type = MYSQL_TYPE_BLOB;
        bind[0].buffer = tokenBytes;
        bind[0].length = &tokenLen;

        bind[1].buffer_type = MYSQL_TYPE_LONG;
        bind[1].buffer = &acc;
        bind[1].length = &accLen;

        bind[2].buffer_type = MYSQL_TYPE_LONGLONG;
        bind[2].buffer = &exp;
        bind[2].is_unsigned = 1;
        bind[2].length = &expLen;

        bool ok = mysql_stmt_bind_param(stmt, bind) == 0 && mysql_stmt_execute(stmt) == 0;
        if (!ok) std::cerr << "[token] insert failed: " << mysql_stmt_error(stmt) << "\n";
        mysql_stmt_close(stmt);
        return ok ? token : 0;
    }

    // РџРѕРіР°С€РµРЅРёРµ: РЅР°С…РѕРґРёС‚ С‚РѕРєРµРЅ, РїСЂРѕРІРµСЂСЏРµС‚ СЃСЂРѕРє, РїРѕРјРµС‡Р°РµС‚ consumed, РІРѕР·РІСЂР°С‰Р°РµС‚ accountId.
    bool Consume(uint64_t token, uint32_t& accountId)
    {
        uint8_t tokenBytes[8];
        for (size_t i = 0; i < 8; ++i)
            tokenBytes[i] = static_cast<uint8_t>((token >> (8 * (7 - i))) & 0xFF);

        MYSQL_STMT* stmt = _db.Prepare(
            "SELECT account_id, expires_at FROM session_token "
            "WHERE token = ? AND consumed_at IS NULL LIMIT 1");
        if (!stmt) return false;

        MYSQL_BIND in[1];
        memset(in, 0, sizeof(in));
        unsigned long tokenLen = 8;
        in[0].buffer_type = MYSQL_TYPE_BLOB;
        in[0].buffer = tokenBytes;
        in[0].length = &tokenLen;

        if (mysql_stmt_bind_param(stmt, in) != 0 || mysql_stmt_execute(stmt) != 0)
        {
            mysql_stmt_close(stmt);
            return false;
        }

        uint32_t acc = 0;
        unsigned long long exp = 0;
        bool isNull[2] = { 0, 0 };
        unsigned long accLen = sizeof(acc);
        unsigned long expLen = sizeof(exp);

        MYSQL_BIND out[2];
        memset(out, 0, sizeof(out));
        out[0].buffer_type = MYSQL_TYPE_LONG;
        out[0].buffer = &acc;
        out[0].length = &accLen;
        out[0].is_null = &isNull[0];
        out[1].buffer_type = MYSQL_TYPE_LONGLONG;
        out[1].buffer = &exp;
        out[1].is_unsigned = 1;
        out[1].length = &expLen;
        out[1].is_null = &isNull[1];

        if (mysql_stmt_bind_result(stmt, out) != 0)
        {
            mysql_stmt_close(stmt);
            return false;
        }

        bool found = mysql_stmt_fetch(stmt) == 0 && !isNull[0];
        mysql_stmt_close(stmt);
        if (!found) return false;

        if (exp < static_cast<unsigned long long>(time(nullptr)))
            return false;

        // РћРґРЅРѕСЂР°Р·РѕРІРѕСЃС‚СЊ: UPDATE СЃ СѓСЃР»РѕРІРёРµРј consumed_at IS NULL вЂ” Р°С‚РѕРјР°СЂРµРЅ.
        MYSQL_STMT* upd = _db.Prepare(
            "UPDATE session_token SET consumed_at = NOW() "
            "WHERE token = ? AND consumed_at IS NULL");
        if (!upd) return false;

        MYSQL_BIND uin[1];
        memset(uin, 0, sizeof(uin));
        uin[0].buffer_type = MYSQL_TYPE_BLOB;
        uin[0].buffer = tokenBytes;
        uin[0].length = &tokenLen;

        bool ok = mysql_stmt_bind_param(upd, uin) == 0 && mysql_stmt_execute(upd) == 0 &&
                  mysql_stmt_affected_rows(upd) == 1;
        mysql_stmt_close(upd);

        if (ok)
            accountId = acc;
        return ok;
    }

    void PurgeExpired()
    {
        std::vector<std::vector<std::string>> ignored;
        _db.Query("DELETE FROM session_token WHERE expires_at < UNIX_TIMESTAMP()", ignored);
    }

private:
    Database& _db;
};
