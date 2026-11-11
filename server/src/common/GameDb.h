#pragma once
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

#include "Database.h"
#include "GameTypes.h"

// Операции с персонажем в ndv_world.characters и инвентарём.

namespace GameDb
{
    inline bool IsValidCharacterName(std::string const& name)
    {
        if (name.size() < 2 || name.size() > 16) return false;
        if (!std::isupper(static_cast<unsigned char>(name[0]))) return false;
        for (char c : name)
        {
            if (c >= 'a' && c <= 'z') continue;
            if (c >= 'A' && c <= 'Z') continue;
            return false;
        }
        return true;
    }

    inline uint32_t CountCharacters(Database& db, uint32_t accountId)
    {
        std::vector<std::vector<std::string>> rows;
        const std::string sql = "SELECT COUNT(*) FROM characters WHERE account_id = " +
                                std::to_string(accountId);
        if (!db.Query(sql, rows) || rows.empty() || rows[0].empty()) return 0;
        return static_cast<uint32_t>(std::stoul(rows[0][0]));
    }

    inline bool CharacterExists(Database& db, uint32_t accountId, std::string const& name,
                                uint32_t& existingGuid)
    {
        MYSQL_STMT* stmt = db.Prepare("SELECT id FROM characters WHERE name = ? LIMIT 1");
        if (!stmt) return false;

        unsigned long nameLen = static_cast<unsigned long>(name.size());
        MYSQL_BIND in[1];
        std::memset(in, 0, sizeof(in));
        in[0].buffer_type = MYSQL_TYPE_STRING;
        in[0].buffer = const_cast<char*>(name.c_str());
        in[0].buffer_length = nameLen;
        in[0].length = &nameLen;

        if (mysql_stmt_bind_param(stmt, in) != 0 || mysql_stmt_execute(stmt) != 0)
        {
            mysql_stmt_close(stmt);
            return false;
        }

        uint32_t id = 0;
        unsigned long idLen = sizeof(id);
        bool isNull = false;
        MYSQL_BIND out[1];
        std::memset(out, 0, sizeof(out));
        out[0].buffer_type = MYSQL_TYPE_LONG;
        out[0].buffer = &id;
        out[0].length = &idLen;
        out[0].is_null = &isNull;

        bool found = false;
        if (mysql_stmt_bind_result(stmt, out) == 0)
            found = (mysql_stmt_fetch(stmt) == 0 && !isNull);

        mysql_stmt_close(stmt);
        if (found) existingGuid = id;
        return found;
    }

    inline bool CreateCharacter(Database& db, uint32_t accountId, std::string const& name,
                                uint8_t race, uint8_t klass, uint32_t& newGuid)
    {
        MYSQL_STMT* stmt = db.Prepare(
            "INSERT INTO characters (account_id, name, race, class, level, map_id, x, y, z, o) "
            "VALUES (?, ?, ?, ?, 1, 0, 0.0, 0.0, 100.0, 0.0)");
        if (!stmt) return false;

        unsigned long accLen = sizeof(accountId);
        unsigned long nameLen = static_cast<unsigned long>(name.size());
        unsigned long raceLen = sizeof(race);
        unsigned long klassLen = sizeof(klass);

        MYSQL_BIND bind[4];
        std::memset(bind, 0, sizeof(bind));

        bind[0].buffer_type = MYSQL_TYPE_LONG;
        bind[0].buffer = &accountId;
        bind[0].length = &accLen;

        bind[1].buffer_type = MYSQL_TYPE_STRING;
        bind[1].buffer = const_cast<char*>(name.c_str());
        bind[1].buffer_length = nameLen;
        bind[1].length = &nameLen;

        bind[2].buffer_type = MYSQL_TYPE_TINY;
        bind[2].buffer = &race;
        bind[2].is_unsigned = 1;
        bind[2].length = &raceLen;

        bind[3].buffer_type = MYSQL_TYPE_TINY;
        bind[3].buffer = &klass;
        bind[3].is_unsigned = 1;
        bind[3].length = &klassLen;

        bool ok = mysql_stmt_bind_param(stmt, bind) == 0 && mysql_stmt_execute(stmt) == 0;
        if (ok)
            newGuid = static_cast<uint32_t>(mysql_stmt_insert_id(stmt));
        else
            std::cerr << "[char] insert failed: " << mysql_stmt_error(stmt) << "\n";

        mysql_stmt_close(stmt);
        return ok;
    }

    inline bool DeleteCharacter(Database& db, uint32_t accountId, uint32_t guid)
    {
        MYSQL_STMT* stmt = db.Prepare("DELETE FROM characters WHERE id = ? AND account_id = ?");
        if (!stmt) return false;

        unsigned long guidLen = sizeof(guid);
        unsigned long accLen = sizeof(accountId);

        MYSQL_BIND bind[2];
        std::memset(bind, 0, sizeof(bind));
        bind[0].buffer_type = MYSQL_TYPE_LONG;
        bind[0].buffer = &guid;
        bind[0].length = &guidLen;
        bind[1].buffer_type = MYSQL_TYPE_LONG;
        bind[1].buffer = &accountId;
        bind[1].length = &accLen;

        bool ok = mysql_stmt_bind_param(stmt, bind) == 0 && mysql_stmt_execute(stmt) == 0 &&
                  mysql_stmt_affected_rows(stmt) == 1;
        mysql_stmt_close(stmt);
        return ok;
    }

    inline void SavePosition(Database& db, uint32_t guid, Vector3 const& pos, uint32_t mapId)
    {
        MYSQL_STMT* stmt = db.Prepare(
            "UPDATE characters SET map_id = ?, x = ?, y = ?, z = ?, o = ? WHERE id = ?");
        if (!stmt) return;

        unsigned long mapLen = sizeof(mapId);
        unsigned long guidLen = sizeof(guid);

        MYSQL_BIND bind[6];
        std::memset(bind, 0, sizeof(bind));
        bind[0].buffer_type = MYSQL_TYPE_LONG;
        bind[0].buffer = &mapId;
        bind[0].length = &mapLen;
        bind[1].buffer_type = MYSQL_TYPE_FLOAT;
        bind[1].buffer = const_cast<float*>(&pos.x);
        bind[1].length = &mapLen;
        bind[2].buffer_type = MYSQL_TYPE_FLOAT;
        bind[2].buffer = const_cast<float*>(&pos.y);
        bind[2].length = &mapLen;
        bind[3].buffer_type = MYSQL_TYPE_FLOAT;
        bind[3].buffer = const_cast<float*>(&pos.z);
        bind[3].length = &mapLen;
        bind[4].buffer_type = MYSQL_TYPE_FLOAT;
        bind[4].buffer = const_cast<float*>(&pos.o);
        bind[4].length = &mapLen;
        bind[5].buffer_type = MYSQL_TYPE_LONG;
        bind[5].buffer = &guid;
        bind[5].length = &guidLen;

        if (mysql_stmt_bind_param(stmt, bind) == 0)
            mysql_stmt_execute(stmt);
        mysql_stmt_close(stmt);
    }

    inline void LoadInventory(Database& db, uint32_t guid, std::vector<InventoryItem>& out)
    {
        out.clear();
        std::vector<std::vector<std::string>> rows;
        const std::string sql =
            "SELECT item_entry, count, slot FROM character_inventory WHERE character_guid = " +
            std::to_string(guid) + " ORDER BY slot";
        if (!db.Query(sql, rows)) return;

        for (auto& r : rows)
        {
            if (r.size() < 3) continue;
            InventoryItem it;
            it.entry = static_cast<uint32_t>(std::stoul(r[0]));
            it.count = static_cast<uint32_t>(std::stoul(r[1]));
            it.slot = static_cast<int16_t>(std::stoi(r[2]));
            out.push_back(it);
        }
    }

    inline bool AddItemToInventory(Database& db, uint32_t guid, uint32_t itemEntry, uint32_t count,
                                   int16_t slot)
    {
        MYSQL_STMT* stmt = db.Prepare(
            "INSERT INTO character_inventory (character_guid, slot, item_entry, count) "
            "VALUES (?, ?, ?, ?) "
            "ON DUPLICATE KEY UPDATE count = count + VALUES(count)");
        if (!stmt) return false;

        unsigned long guidLen = sizeof(guid);
        unsigned long slotLen = sizeof(slot);
        unsigned long entryLen = sizeof(itemEntry);
        unsigned long countLen = sizeof(count);

        MYSQL_BIND bind[4];
        std::memset(bind, 0, sizeof(bind));
        bind[0].buffer_type = MYSQL_TYPE_LONG;
        bind[0].buffer = &guid;
        bind[0].length = &guidLen;
        bind[1].buffer_type = MYSQL_TYPE_SHORT;
        bind[1].buffer = &slot;
        bind[1].length = &slotLen;
        bind[2].buffer_type = MYSQL_TYPE_LONG;
        bind[2].buffer = &itemEntry;
        bind[2].is_unsigned = 1;
        bind[2].length = &entryLen;
        bind[3].buffer_type = MYSQL_TYPE_LONG;
        bind[3].buffer = &count;
        bind[3].is_unsigned = 1;
        bind[3].length = &countLen;

        bool ok = mysql_stmt_bind_param(stmt, bind) == 0 && mysql_stmt_execute(stmt) == 0;
        mysql_stmt_close(stmt);
        return ok;
    }

    inline int FindFreeSlot(std::vector<InventoryItem> const& inv)
    {
        for (int slot = 0; slot < 24; ++slot)
        {
            bool used = false;
            for (auto const& it : inv)
                if (it.slot == slot) { used = true; break; }
            if (!used) return slot;
        }
        return -1;
    }
}
