#pragma once
#include <algorithm>
#include <cstdint>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "Database.h"
#include "GameTypes.h"

// Хранилище игрового мира: шаблоны, спавны, игроки в мире.
// Однопоточная модель: весь доступ идёт из цикла select(), поэтому без мьютексов.
class World
{
public:
    using SendFn = std::function<void(uint32_t guid, uint16_t opcode, ByteBuffer const& body)>;

    bool LoadFromDb(Database& db)
    {
        _monsters.clear();
        _items.clear();
        _creatures.clear();
        _nextCreatureGuid = 1;

        if (!LoadItemTemplates(db)) return false;
        if (!LoadCreatureTemplates(db)) return false;
        if (!LoadCreatureSpawns(db)) return false;
        if (!LoadCreatureLoot(db)) return false;
        return true;
    }

    // ---- шаблоны ----
    ItemTemplate const* FindItem(uint32_t entry) const
    {
        auto it = _items.find(entry);
        return it == _items.end() ? nullptr : &it->second;
    }

    CreatureTemplate const* FindCreatureTemplate(uint32_t entry) const
    {
        auto it = _monsters.find(entry);
        return it == _monsters.end() ? nullptr : &it->second;
    }

    // ---- существа ----
    Creature* FindCreature(uint32_t guid)
    {
        auto it = _creatures.find(guid);
        return it == _creatures.end() ? nullptr : &it->second;
    }

    std::unordered_map<uint32_t, Creature>& Creatures() { return _creatures; }

    // ---- игроки ----
    Player* AddPlayer(Player player)
    {
        uint32_t guid = player.guid;
        _players[guid] = std::move(player);
        return &_players[guid];
    }

    void RemovePlayer(uint32_t guid) { _players.erase(guid); }

    Player* FindPlayer(uint32_t guid)
    {
        auto it = _players.find(guid);
        return it == _players.end() ? nullptr : &it->second;
    }

    std::unordered_map<uint32_t, Player>& Players() { return _players; }

    // Монстр в радиусе видимости (для агрессии)
    Creature* FindNearestCreature(Vector3 const& pos, float range, uint32_t excludeGuid = 0)
    {
        Creature* best = nullptr;
        float bestDist = range;
        for (auto& [guid, c] : _creatures)
        {
            if (!c.alive || guid == excludeGuid) continue;
            if (c.mapId != 0) continue;
            const float d = c.position.DistanceTo2D(pos);
            if (d < bestDist)
            {
                bestDist = d;
                best = &c;
            }
        }
        return best;
    }

    // Шаблоны, доступные другим модулям
    uint32_t NextCharacterGuidHint = 1;

private:
    bool LoadItemTemplates(Database& db)
    {
        std::vector<std::vector<std::string>> rows;
        if (!db.Query("SELECT entry, name, item_class, quality, stackable, armor, "
                      "min_damage, max_damage, heal_amount, buy_price FROM item_template", rows))
            return false;

        for (auto& r : rows)
        {
            if (r.size() < 10) continue;
            ItemTemplate t;
            t.entry = static_cast<uint32_t>(std::stoul(r[0]));
            t.name = r[1];
            t.itemClass = static_cast<uint8_t>(std::stoi(r[2]));
            t.quality = static_cast<uint8_t>(std::stoi(r[3]));
            t.stackable = static_cast<uint32_t>(std::stoul(r[4]));
            t.armor = static_cast<uint32_t>(std::stoul(r[5]));
            t.minDamage = static_cast<uint32_t>(std::stoul(r[6]));
            t.maxDamage = static_cast<uint32_t>(std::stoul(r[7]));
            t.healAmount = static_cast<uint32_t>(std::stoul(r[8]));
            t.buyPrice = static_cast<uint32_t>(std::stoul(r[9]));
            _items[t.entry] = t;
        }
        std::cout << "[world] item templates: " << _items.size() << "\n";
        return true;
    }

    bool LoadCreatureTemplates(Database& db)
    {
        std::vector<std::vector<std::string>> rows;
        if (!db.Query("SELECT entry, name, min_level, max_level, health, min_damage, max_damage, "
                      "attack_speed_ms, attack_range, xp_reward, aggro_range, faction "
                      "FROM creature_template", rows))
            return false;

        for (auto& r : rows)
        {
            if (r.size() < 12) continue;
            CreatureTemplate t;
            t.entry = static_cast<uint32_t>(std::stoul(r[0]));
            t.name = r[1];
            t.minLevel = static_cast<uint8_t>(std::stoi(r[2]));
            t.maxLevel = static_cast<uint8_t>(std::stoi(r[3]));
            t.health = static_cast<uint32_t>(std::stoul(r[4]));
            t.minDamage = static_cast<uint32_t>(std::stoul(r[5]));
            t.maxDamage = static_cast<uint32_t>(std::stoul(r[6]));
            t.attackSpeedMs = static_cast<uint32_t>(std::stoul(r[7]));
            t.attackRange = std::stof(r[8]);
            t.xpReward = static_cast<uint32_t>(std::stoul(r[9]));
            t.aggroRange = std::stof(r[10]);
            t.faction = static_cast<uint8_t>(std::stoi(r[11]));
            _monsters[t.entry] = t;
        }
        std::cout << "[world] creature templates: " << _monsters.size() << "\n";
        return true;
    }

    bool LoadCreatureSpawns(Database& db)
    {
        std::vector<std::vector<std::string>> rows;
        if (!db.Query("SELECT guid, entry, map_id, x, y, z, o, respawn_seconds "
                      "FROM creature_spawn", rows))
            return false;

        for (auto& r : rows)
        {
            if (r.size() < 8) continue;
            const uint32_t entry = static_cast<uint32_t>(std::stoul(r[1]));
            auto it = _monsters.find(entry);
            if (it == _monsters.end()) continue;

            Creature c;
            c.guid = static_cast<uint32_t>(std::stoul(r[0]));
            c.entry = entry;
            c.name = it->second.name;
            c.level = static_cast<uint8_t>(Rng::Range(it->second.minLevel, it->second.maxLevel));
            c.maxHealth = it->second.health + (c.level - 1) * 20;
            c.health = c.maxHealth;
            c.minDamage = it->second.minDamage;
            c.maxDamage = it->second.maxDamage;
            c.attackSpeedMs = it->second.attackSpeedMs;
            c.attackRange = it->second.attackRange;
            c.xpReward = it->second.xpReward;
            c.aggroRange = it->second.aggroRange;
            c.mapId = static_cast<uint32_t>(std::stoul(r[2]));
            c.position.x = std::stof(r[3]);
            c.position.y = std::stof(r[4]);
            c.position.z = std::stof(r[5]);
            c.position.o = std::stof(r[6]);
            c.homePosition = c.position;
            c.respawnSeconds = static_cast<uint32_t>(std::stoul(r[7]));
            _creatures[c.guid] = c;
        }
        std::cout << "[world] creature spawns: " << _creatures.size() << "\n";
        return true;
    }

    bool LoadCreatureLoot(Database& db)
    {
        std::vector<std::vector<std::string>> rows;
        if (!db.Query("SELECT creature_entry, item_entry, chance_percent, min_count, max_count "
                      "FROM creature_loot", rows))
            return false;

        for (auto& r : rows)
        {
            if (r.size() < 5) continue;
            LootEntry e;
            e.creatureEntry = static_cast<uint32_t>(std::stoul(r[0]));
            e.itemEntry = static_cast<uint32_t>(std::stoul(r[1]));
            e.chance = static_cast<uint32_t>(std::stoul(r[2]));
            e.minCount = static_cast<uint32_t>(std::stoul(r[3]));
            e.maxCount = static_cast<uint32_t>(std::stoul(r[4]));
            _loot[e.creatureEntry].push_back(e);
        }
        std::cout << "[world] loot tables: " << _loot.size() << "\n";
        return true;
    }

public:
    struct LootEntry
    {
        uint32_t creatureEntry = 0;
        uint32_t itemEntry = 0;
        uint32_t chance = 100;
        uint32_t minCount = 1;
        uint32_t maxCount = 1;
    };

    // Бросок лута при смерти существа
    void RollLoot(Creature& c)
    {
        c.loot.clear();
        auto it = _loot.find(c.entry);
        if (it == _loot.end()) return;

        for (auto const& e : it->second)
        {
            if (e.chance < 100 && Rng::Next(100) >= e.chance) continue;
            const uint32_t count = Rng::Range(e.minCount, e.maxCount);
            if (count > 0)
                c.loot[e.itemEntry] += count;
        }
    }

    std::vector<LootEntry> const* GetLootTable(uint32_t creatureEntry) const
    {
        auto it = _loot.find(creatureEntry);
        return it == _loot.end() ? nullptr : &it->second;
    }

private:
    std::unordered_map<uint32_t, ItemTemplate> _items;
    std::unordered_map<uint32_t, CreatureTemplate> _monsters;
    std::unordered_map<uint32_t, Creature> _creatures;
    std::unordered_map<uint32_t, std::vector<LootEntry>> _loot;
    std::unordered_map<uint32_t, Player> _players;
    uint32_t _nextCreatureGuid = 1;
};
