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
#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "Config.h"
#include "Database.h"
#include "GameDb.h"
#include "GameTypes.h"
#include "Network.h"
#include "Opcodes.h"
#include "TokenStore.h"
#include "World.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "libmysql.lib")

namespace
{
    struct Character
    {
        uint32_t guid = 0;
        std::string name;
        uint8_t race = 0;
        uint8_t klass = 0;
        uint8_t level = 0;
        uint32_t mapId = 0;
        float x = 0.f, y = 0.f, z = 0.f, o = 0.f;
    };

    // Однопоточный select()-цикл: зависимости процесса на уровне файла.
    Database* g_charDb = nullptr;
    TokenStore* g_tokens = nullptr;
    World* g_world = nullptr;

    uint32_t NowMs()
    {
        using namespace std::chrono;
        static const auto start = steady_clock::now();
        return static_cast<uint32_t>(
            duration_cast<milliseconds>(steady_clock::now() - start).count());
    }

    void BuildCharacterBind(MYSQL_BIND* res, bool* isNull, unsigned long* lens,
                            uint32_t& guid, char* nameBuf, size_t nameBufSize,
                            unsigned char& race, unsigned char& klass, unsigned char& level,
                            uint32_t& mapId, float& x, float& y, float& z, float& o)
    {
        std::memset(res, 0, sizeof(MYSQL_BIND) * 10);
        std::memset(isNull, 0, sizeof(bool) * 10);

        lens[0] = sizeof(guid);
        lens[1] = static_cast<unsigned long>(nameBufSize);
        lens[2] = sizeof(race);
        lens[3] = sizeof(klass);
        lens[4] = sizeof(level);
        lens[5] = sizeof(mapId);
        lens[6] = sizeof(x);
        lens[7] = sizeof(y);
        lens[8] = sizeof(z);
        lens[9] = sizeof(o);

        res[0].buffer_type = MYSQL_TYPE_LONG;   res[0].buffer = &guid;
        res[1].buffer_type = MYSQL_TYPE_STRING; res[1].buffer = nameBuf;
        res[1].buffer_length = static_cast<unsigned long>(nameBufSize);
        res[2].buffer_type = MYSQL_TYPE_TINY;   res[2].buffer = &race;   res[2].is_unsigned = 1;
        res[3].buffer_type = MYSQL_TYPE_TINY;   res[3].buffer = &klass;  res[3].is_unsigned = 1;
        res[4].buffer_type = MYSQL_TYPE_TINY;   res[4].buffer = &level;  res[4].is_unsigned = 1;
        res[5].buffer_type = MYSQL_TYPE_LONG;   res[5].buffer = &mapId;
        res[6].buffer_type = MYSQL_TYPE_FLOAT;  res[6].buffer = &x;
        res[7].buffer_type = MYSQL_TYPE_FLOAT;  res[7].buffer = &y;
        res[8].buffer_type = MYSQL_TYPE_FLOAT;  res[8].buffer = &z;
        res[9].buffer_type = MYSQL_TYPE_FLOAT;  res[9].buffer = &o;

        for (int i = 0; i < 10; ++i)
        {
            res[i].length = &lens[i];
            res[i].is_null = &isNull[i];
        }
    }

    std::vector<Character> LoadCharacters(Database& db, uint32_t accountId)
    {
        std::vector<Character> result;

        MYSQL_STMT* stmt = db.Prepare(
            "SELECT id, name, race, class, level, map_id, x, y, z, o "
            "FROM characters WHERE account_id = ? ORDER BY id");
        if (!stmt) return result;

        unsigned long idLen = sizeof(accountId);
        MYSQL_BIND in[1];
        std::memset(in, 0, sizeof(in));
        in[0].buffer_type = MYSQL_TYPE_LONG;
        in[0].buffer = &accountId;
        in[0].length = &idLen;

        if (mysql_stmt_bind_param(stmt, in) != 0 || mysql_stmt_execute(stmt) != 0)
        {
            mysql_stmt_close(stmt);
            return result;
        }

        uint32_t guid = 0, mapId = 0;
        char nameBuf[64] = {0};
        unsigned char race = 0, klass = 0, level = 0;
        float x = 0, y = 0, z = 0, o = 0;

        bool isNull[10];
        unsigned long lens[10];
        MYSQL_BIND res[10];
        BuildCharacterBind(res, isNull, lens, guid, nameBuf, sizeof(nameBuf),
                           race, klass, level, mapId, x, y, z, o);

        if (mysql_stmt_bind_result(stmt, res) != 0)
        {
            mysql_stmt_close(stmt);
            return result;
        }

        while (mysql_stmt_fetch(stmt) == 0)
        {
            Character c;
            c.guid = guid;
            c.name.assign(nameBuf, std::min<unsigned long>(lens[1], sizeof(nameBuf)));
            c.race = race;
            c.klass = klass;
            c.level = level;
            c.mapId = mapId;
            c.x = x;
            c.y = y;
            c.z = z;
            c.o = o;
            result.push_back(std::move(c));
        }

        mysql_stmt_close(stmt);
        return result;
    }

    bool LoadCharacterByName(Database& db, uint32_t accountId, const std::string& name,
                             Character& out)
    {
        MYSQL_STMT* stmt = db.Prepare(
            "SELECT id, name, race, class, level, map_id, x, y, z, o "
            "FROM characters WHERE account_id = ? AND name = ? LIMIT 1");
        if (!stmt) return false;

        unsigned long idLen = sizeof(accountId);
        unsigned long nameLen = static_cast<unsigned long>(name.size());

        MYSQL_BIND in[2];
        std::memset(in, 0, sizeof(in));
        in[0].buffer_type = MYSQL_TYPE_LONG;
        in[0].buffer = &accountId;
        in[0].length = &idLen;
        in[1].buffer_type = MYSQL_TYPE_STRING;
        in[1].buffer = const_cast<char*>(name.c_str());
        in[1].buffer_length = nameLen;
        in[1].length = &nameLen;

        if (mysql_stmt_bind_param(stmt, in) != 0 || mysql_stmt_execute(stmt) != 0)
        {
            mysql_stmt_close(stmt);
            return false;
        }

        uint32_t guid = 0, mapId = 0;
        char nameBuf[64] = {0};
        unsigned char race = 0, klass = 0, level = 0;
        float x = 0, y = 0, z = 0, o = 0;

        bool isNull[10];
        unsigned long lens[10];
        MYSQL_BIND res[10];
        BuildCharacterBind(res, isNull, lens, guid, nameBuf, sizeof(nameBuf),
                           race, klass, level, mapId, x, y, z, o);

        if (mysql_stmt_bind_result(stmt, res) != 0)
        {
            mysql_stmt_close(stmt);
            return false;
        }

        bool ok = mysql_stmt_fetch(stmt) == 0 && !isNull[0];
        if (ok)
        {
            out.guid = guid;
            out.name.assign(nameBuf, std::min<unsigned long>(lens[1], sizeof(nameBuf)));
            out.race = race;
            out.klass = klass;
            out.level = level;
            out.mapId = mapId;
            out.x = x;
            out.y = y;
            out.z = z;
            out.o = o;
        }

        mysql_stmt_close(stmt);
        return ok;
    }
}

// Реестр «персонаж -> сессия»: заполняется при входе в мир, нужен для доставки пакетов.
class WorldSession;
namespace { std::unordered_map<uint32_t, WorldSession*> g_sessionsByCharacter; }

class WorldSession : public Session
{
public:
    using Session::Session;

protected:
    void HandlePacket(uint16_t opcode, ByteBuffer& p) override
    {
        if (opcode != CMSG_WORLD_AUTH && !_authed)
        {
            SendResult(AUTH_NOT_LOGGED);
            Close();
            return;
        }

        switch (opcode)
        {
            case CMSG_WORLD_AUTH:
            {
                uint64_t token = 0;
                p >> token;
                HandleWorldAuth(token);
                break;
            }
            case CMSG_PING:
            {
                uint32_t counter = 0;
                p >> counter;
                ByteBuffer out;
                out << counter;
                Send(SMSG_PONG, out);
                break;
            }
            case CMSG_CHAR_ENUM:
            {
                std::vector<Character> chars = LoadCharacters(*g_charDb, _accountId);
                ByteBuffer out;
                out << static_cast<uint8_t>(std::min<size_t>(chars.size(), 255));
                for (size_t i = 0; i < chars.size() && i < 255; ++i)
                {
                    const Character& c = chars[i];
                    out << c.guid << c.name << c.race << c.klass << c.level
                        << c.mapId << c.x << c.y << c.z << c.o;
                }
                Send(SMSG_CHAR_ENUM, out);
                break;
            }
            case CMSG_CHAR_CREATE:
            {
                std::string name;
                uint8_t race = 1, klass = 1;
                p >> name >> race >> klass;
                HandleCharCreate(name, race, klass);
                break;
            }
            case CMSG_CHAR_DELETE:
            {
                uint32_t guid = 0;
                p >> guid;
                HandleCharDelete(guid);
                break;
            }
            case CMSG_PLAYER_LOGIN:
            {
                std::string name;
                p >> name;
                HandlePlayerLogin(name);
                break;
            }
            case CMSG_MOVE:
            {
                Player* pl = ActivePlayer();
                if (!pl) break;
                float x = 0, y = 0, z = 0, o = 0;
                p >> x >> y >> z >> o;
                ApplyMove(*pl, Vector3{x, y, z, o});
                break;
            }
            case CMSG_REQUEST_OBJECTS:
            {
                Player* pl = ActivePlayer();
                if (!pl) break;
                SendVisibleObjects(*pl);
                break;
            }
            case CMSG_CHAT_SAY:
            {
                Player* pl = ActivePlayer();
                if (!pl) break;
                std::string msg;
                p.ReadString(msg, 255);
                BroadcastChat(*pl, msg);
                break;
            }
            case CMSG_ATTACK:
            {
                Player* pl = ActivePlayer();
                if (!pl) break;
                uint32_t targetGuid = 0;
                uint8_t kind = TARGET_CREATURE;
                p >> targetGuid >> kind;
                BeginAttack(*pl, targetGuid, kind);
                break;
            }
            case CMSG_STOP_ATTACK:
            {
                Player* pl = ActivePlayer();
                if (!pl) break;
                StopAttack(*pl);
                break;
            }
            case CMSG_LOOT:
            {
                Player* pl = ActivePlayer();
                if (!pl) break;
                uint32_t guid = 0;
                p >> guid;
                SendLootList(*pl, guid);
                break;
            }
            case CMSG_LOOT_TAKE:
            {
                Player* pl = ActivePlayer();
                if (!pl) break;
                uint32_t guid = 0, entry = 0;
                p >> guid >> entry;
                TakeLoot(*pl, guid, entry);
                break;
            }
            case CMSG_USE_ITEM:
            {
                Player* pl = ActivePlayer();
                if (!pl) break;
                uint32_t entry = 0;
                p >> entry;
                UseItem(*pl, entry);
                break;
            }
            case CMSG_EQUIP_ITEM:
            {
                Player* pl = ActivePlayer();
                if (!pl) break;
                uint32_t entry = 0;
                p >> entry;
                EquipItem(*pl, entry);
                break;
            }
            default:
                break;
        }
    }

public:
    void TickPlayer(uint32_t nowMs)
    {
        if (!_inWorld) return;
        Player* pl = g_world->FindPlayer(_characterGuid);
        if (!pl || !pl->inWorld) return;
        if (pl->targetGuid == 0) return;
        if (pl->nextAttackAtMs > nowMs) return;

        ResolvePlayerAttack(*pl, nowMs);
    }

    void OnSessionClosed()
    {
        if (!_inWorld) return;
        Player* pl = g_world->FindPlayer(_characterGuid);
        if (pl)
        {
            for (auto& [guid, other] : g_world->Players())
                if (other.targetGuid == _characterGuid) other.targetGuid = 0;

            GameDb::SavePosition(*g_charDb, _characterGuid, pl->position, pl->mapId);
            std::cout << "[world] player '" << pl->name << "' left the world\n";
        }
        g_world->RemovePlayer(_characterGuid);
        g_sessionsByCharacter.erase(_characterGuid);
        _inWorld = false;
    }

    uint32_t CharacterGuid() const { return _characterGuid; }
    bool InWorld() const { return _inWorld; }

private:
    void SendResult(uint8_t result, uint16_t opcode = SMSG_WORLD_AUTH_RESULT)
    {
        ByteBuffer out;
        out << result;
        Send(opcode, out);
    }

    Player* ActivePlayer()
    {
        if (!_inWorld) return nullptr;
        return g_world->FindPlayer(_characterGuid);
    }

    void HandleWorldAuth(uint64_t token)
    {
        if (_authed)
        {
            SendResult(AUTH_OK);
            return;
        }

        uint32_t accountId = 0;
        if (!g_tokens->Consume(token, accountId))
        {
            std::cout << "[world] rejected token\n";
            SendResult(AUTH_BAD_TOKEN);
            Close();
            return;
        }

        _accountId = accountId;
        _authed = true;
        SendResult(AUTH_OK);
        std::cout << "[world] session authed for account " << accountId << "\n";
    }

    void HandleCharCreate(std::string const& name, uint8_t race, uint8_t klass)
    {
        if (!GameDb::IsValidCharacterName(name))
        {
            SendResult(AUTH_BAD_NAME, SMSG_CHAR_CREATE_RESULT);
            return;
        }

        if (race < 1 || race > 5 || klass < 1 || klass > 10)
        {
            SendResult(AUTH_BAD_NAME, SMSG_CHAR_CREATE_RESULT);
            return;
        }

        if (GameDb::CountCharacters(*g_charDb, _accountId) >= 5)
        {
            SendResult(AUTH_TOO_MANY, SMSG_CHAR_CREATE_RESULT);
            return;
        }

        uint32_t existing = 0;
        uint32_t guid = 0;
        if (GameDb::CharacterExists(*g_charDb, _accountId, name, existing))
        {
            SendResult(AUTH_CHAR_EXISTS, SMSG_CHAR_CREATE_RESULT);
            return;
        }

        if (!GameDb::CreateCharacter(*g_charDb, _accountId, name, race, klass, guid))
        {
            SendResult(AUTH_DB_ERROR, SMSG_CHAR_CREATE_RESULT);
            return;
        }

        ByteBuffer out;
        out << static_cast<uint8_t>(AUTH_OK) << guid;
        Send(SMSG_CHAR_CREATE_RESULT, out);
        std::cout << "[world] created character '" << name << "' (guid " << guid << ")\n";
    }

    void HandleCharDelete(uint32_t guid)
    {
        uint8_t result = GameDb::DeleteCharacter(*g_charDb, _accountId, guid) ? AUTH_OK : AUTH_NO_CHAR;
        SendResult(result, SMSG_CHAR_DELETE_RESULT);
    }

    void HandlePlayerLogin(std::string const& name)
    {
        if (_inWorld)
        {
            SendResult(AUTH_OK);
            return;
        }

        Character c;
        if (name.empty() || !LoadCharacterByName(*g_charDb, _accountId, name, c))
        {
            SendResult(AUTH_NO_CHAR);
            return;
        }

        Player pl;
        pl.guid = c.guid;
        pl.accountId = _accountId;
        pl.name = c.name;
        pl.race = c.race;
        pl.klass = c.klass;
        pl.level = std::max<uint8_t>(c.level, 1);
        pl.maxHealth = 80 + pl.level * 20;
        pl.health = pl.maxHealth;
        pl.maxMana = 60 + pl.level * 10;
        pl.mana = pl.maxMana;
        pl.xp = 0;
        pl.xpNext = XpForNextLevel(pl.level);
        pl.position = {c.x, c.y, c.z, c.o};
        pl.mapId = c.mapId;
        pl.inWorld = true;

        GameDb::LoadInventory(*g_charDb, pl.guid, pl.inventory);
        for (auto const& it : pl.inventory)
        {
            if (it.slot != -1) continue;
            if (auto tpl = g_world->FindItem(it.entry))
            {
                pl.baseDamage += tpl->maxDamage > 0 ? tpl->maxDamage : 0;
                pl.armor += tpl->armor;
            }
        }

        Player* stored = g_world->AddPlayer(pl);
        _characterGuid = c.guid;
        _inWorld = true;
        g_sessionsByCharacter[_characterGuid] = this;

        ByteBuffer out;
        out << c.mapId << c.x << c.y << c.z << c.o;
        Send(SMSG_LOGIN_VERIFY_WORLD, out);

        SendStats(*stored);
        SendInventory(*stored);
        SendVisibleObjects(*stored);
        BroadcastVisibility(*stored, true);

        std::cout << "[world] account " << _accountId << " entered world as '"
                  << c.name << "' (guid " << c.guid << ")\n";
    }

    // ---------- отправка ----------
    void SendStats(Player const& pl)
    {
        ByteBuffer out;
        out << pl.health << pl.maxHealth << pl.mana << pl.maxMana << uint32_t(pl.level)
            << pl.xp << pl.xpNext << pl.money << pl.TotalAttackPower() << pl.armor;
        Send(SMSG_UPDATE_STATS, out);
    }

    void SendInventory(Player const& pl)
    {
        ByteBuffer out;
        uint8_t count = static_cast<uint8_t>(std::min<size_t>(pl.inventory.size(), 255));
        out << count;
        for (size_t i = 0; i < pl.inventory.size() && i < 255; ++i)
        {
            auto const& it = pl.inventory[i];
            auto tpl = g_world->FindItem(it.entry);
            out << it.entry << (tpl ? tpl->name : std::string()) << it.count
                << (tpl ? tpl->itemClass : 0) << (tpl ? tpl->quality : 0);
        }
        Send(SMSG_INVENTORY, out);
    }

    void SendVisibleObjects(Player const& pl)
    {
        for (auto& [guid, c] : g_world->Creatures())
        {
            if (!c.alive) continue;
            if (c.mapId != pl.mapId) continue;
            if (pl.position.DistanceTo2D(c.position) > 100.f) continue;

            ByteBuffer out;
            out << c.guid << c.entry << c.name << c.health << c.maxHealth
                << c.level << c.position.x << c.position.y << c.position.z << c.position.o;
            Send(SMSG_CREATURE_VISIBLE, out);
        }

        for (auto& [guid, other] : g_world->Players())
        {
            if (other.guid == pl.guid || !other.inWorld) continue;
            if (other.mapId != pl.mapId) continue;
            if (pl.position.DistanceTo2D(other.position) > 100.f) continue;

            ByteBuffer out;
            out << other.guid << other.name << other.race << other.klass << other.level
                << other.position.x << other.position.y << other.position.z << other.position.o;
            Send(SMSG_PLAYER_VISIBLE, out);
        }
    }

    void BroadcastVisibility(Player const& pl, bool visible)
    {
        for (auto& [guid, other] : g_world->Players())
        {
            if (other.guid == pl.guid || !other.inWorld) continue;
            if (other.mapId != pl.mapId) continue;
            if (pl.position.DistanceTo2D(other.position) > 100.f) continue;

            Player* target = g_world->FindPlayer(other.guid);
            if (!target) continue;

            ByteBuffer out;
            if (visible)
                out << pl.guid << pl.name << pl.race << pl.klass << pl.level
                    << pl.position.x << pl.position.y << pl.position.z << pl.position.o;
            else
                out << pl.guid;

            BroadcastToSession(other.guid, visible ? SMSG_PLAYER_VISIBLE : SMSG_PLAYER_GONE, out);
        }
    }

    void BroadcastToSession(uint32_t guid, uint16_t opcode, ByteBuffer const& body);

    void BroadcastChat(Player const& pl, std::string const& msg)
    {
        if (msg.empty()) return;
        for (auto& [guid, other] : g_world->Players())
        {
            if (!other.inWorld || other.mapId != pl.mapId) continue;
            if (pl.position.DistanceTo2D(other.position) > 40.f) continue;

            ByteBuffer out;
            out << pl.guid << pl.name << msg;
            BroadcastToSession(other.guid, SMSG_CHAT_SAY, out);
        }
    }

    // ---------- движение ----------
    void ApplyMove(Player& pl, Vector3 const& to)
    {
        const float dist = pl.position.DistanceTo2D(to);
        if (dist > 50.f) return; // телепорт-чит: отбрасываем

        pl.position = to;
        pl.position.o = NormalizeAngle(to.o);

        for (auto& [guid, other] : g_world->Players())
        {
            if (other.guid == pl.guid || !other.inWorld) continue;
            if (other.mapId != pl.mapId) continue;
            if (pl.position.DistanceTo2D(other.position) > 100.f) continue;

            ByteBuffer out;
            out << pl.guid << pl.position.x << pl.position.y << pl.position.z << pl.position.o;
            BroadcastToSession(other.guid, SMSG_MOVE, out);
        }
    }

    // ---------- бой ----------
    void BeginAttack(Player& pl, uint32_t targetGuid, uint8_t kind)
    {
        Creature* cr = (kind == TARGET_CREATURE) ? g_world->FindCreature(targetGuid) : nullptr;
        if (kind == TARGET_CREATURE && (!cr || !cr->alive))
        {
            SendActionResult(CMSG_ATTACK, ACTION_NO_ITEM);
            return;
        }

        pl.targetGuid = targetGuid;
        pl.targetKind = kind;
        pl.nextAttackAtMs = NowMs();

        ByteBuffer out;
        out << pl.guid << targetGuid;
        Send(SMSG_ATTACK_START, out);

        if (cr)
        {
            cr->targetGuid = pl.guid;
            cr->targetKind = TARGET_PLAYER;
        }
    }

    void StopAttack(Player& pl)
    {
        pl.targetGuid = 0;
        ByteBuffer out;
        out << pl.guid;
        Send(SMSG_ATTACK_STOP, out);
    }

    void ResolvePlayerAttack(Player& pl, uint32_t nowMs)
    {
        Creature* cr = g_world->FindCreature(pl.targetGuid);
        if (!cr || !cr->alive)
        {
            StopAttack(pl);
            return;
        }

        const float distance = pl.position.DistanceTo2D(cr->position);
        if (distance > 7.f)
        {
            StopAttack(pl);
            return;
        }

        pl.nextAttackAtMs = nowMs + 2000;

        const uint32_t power = pl.TotalAttackPower();
        const uint32_t damage = Rng::Range(power / 2, power);

        // Урон применяем к существу в хранилище мира, а не к локальной копии.
        auto& store = g_world->Creatures();
        auto found = store.find(cr->guid);
        if (found == store.end())
        {
            StopAttack(pl);
            return;
        }

        Creature& victim = found->second;
        if (!victim.alive || victim.health == 0)
        {
            // Существо уже мертво: ждём респавна и не бьём труп.
            StopAttack(pl);
            return;
        }

        const uint32_t before = victim.health;
        const uint32_t applied = std::min(damage, victim.health);
        victim.health -= applied;

        std::cout << "[world] hit '" << victim.name << "' for " << applied
                  << " (hp " << before << " -> " << victim.health << "/"
                  << victim.maxHealth << ")" << std::endl;

        SendDamage(pl.guid, victim.guid, applied, TARGET_CREATURE);

        // Смерть фиксируем здесь: элемент найден в хранилище, ссылка валидна.
        if (victim.health == 0)
            HandleCreatureDeath(victim, &pl);
    }

    void HandleCreatureDeath(Creature& c, Player* killer)
    {
        c.alive = false;
        c.health = 0;
        c.targetGuid = 0;
        c.respawnAtMs = NowMs() + c.respawnSeconds * 1000;
        g_world->RollLoot(c);

        for (auto& [guid, pl] : g_world->Players())
        {
            if (!pl.inWorld) continue;
            if (pl.position.DistanceTo2D(c.position) > 100.f) continue;

            ByteBuffer out;
            out << c.guid << uint8_t(TARGET_CREATURE);
            BroadcastToSession(pl.guid, SMSG_UNIT_DIED, out);
        }

        if (killer && killer->position.DistanceTo2D(c.position) <= 100.f)
        {
            killer->xp += c.xpReward;
            while (killer->xp >= killer->xpNext)
            {
                killer->xp -= killer->xpNext;
                if (killer->level < 80) ++killer->level;
                killer->xpNext = XpForNextLevel(killer->level);
                killer->maxHealth = 80 + killer->level * 20;
                killer->health = killer->maxHealth;
            }

            for (auto& [guid, other] : g_world->Players())
                if (other.guid == killer->guid) SendStats(other);
        }

        std::cout << "[world] creature '" << c.name << "' (" << c.guid << ") died, respawn in "
                  << c.respawnSeconds << "s\n";
    }

    void SendDamage(uint32_t attacker, uint32_t victim, uint32_t amount, uint8_t kind)
    {
        for (auto& [guid, pl] : g_world->Players())
        {
            if (!pl.inWorld) continue;
            ByteBuffer out;
            out << attacker << victim << amount << kind;
            BroadcastToSession(pl.guid, SMSG_DAMAGE, out);
        }
    }

    void SendActionResult(uint8_t action, uint8_t result)
    {
        ByteBuffer out;
        out << action << result;
        Send(SMSG_ACTION_RESULT, out);
    }

    // ---------- лут и инвентарь ----------
    void SendLootList(Player& pl, uint32_t creatureGuid)
    {
        Creature* c = g_world->FindCreature(creatureGuid);
        if (!c || c->alive)
        {
            SendActionResult(CMSG_LOOT, ACTION_NO_ITEM);
            return;
        }
        if (pl.position.DistanceTo2D(c->position) > 10.f)
        {
            SendActionResult(CMSG_LOOT, ACTION_NO_ITEM);
            return;
        }

        ByteBuffer out;
        out << static_cast<uint8_t>(std::min<size_t>(c->loot.size(), 255));
        size_t i = 0;
        for (auto const& [entry, count] : c->loot)
        {
            if (i++ >= 255) break;
            auto tpl = g_world->FindItem(entry);
            out << entry << (tpl ? tpl->name : std::string()) << count;
        }
        Send(SMSG_LOOT_LIST, out);
    }

    void TakeLoot(Player& pl, uint32_t creatureGuid, uint32_t itemEntry)
    {
        Creature* c = g_world->FindCreature(creatureGuid);
        if (!c || c->alive)
        {
            SendActionResult(CMSG_LOOT_TAKE, ACTION_NO_ITEM);
            return;
        }
        if (pl.position.DistanceTo2D(c->position) > 10.f)
        {
            SendActionResult(CMSG_LOOT_TAKE, ACTION_NO_ITEM);
            return;
        }

        auto it = c->loot.find(itemEntry);
        if (it == c->loot.end() || it->second == 0)
        {
            SendActionResult(CMSG_LOOT_TAKE, ACTION_NO_ITEM);
            return;
        }

        int slot = GameDb::FindFreeSlot(pl.inventory);
        if (slot < 0)
        {
            SendActionResult(CMSG_LOOT_TAKE, ACTION_FULL);
            return;
        }

        const uint32_t count = it->second;
        if (!GameDb::AddItemToInventory(*g_charDb, pl.guid, itemEntry, count,
                                        static_cast<int16_t>(slot)))
        {
            SendActionResult(CMSG_LOOT_TAKE, ACTION_FULL);
            return;
        }

        InventoryItem inv;
        inv.entry = itemEntry;
        inv.count = count;
        inv.slot = static_cast<int16_t>(slot);
        pl.inventory.push_back(inv);

        c->loot.erase(it);

        SendActionResult(CMSG_LOOT_TAKE, ACTION_LOOT_OK);
        SendInventory(pl);
    }

    void UseItem(Player& pl, uint32_t itemEntry)
    {
        auto it = std::find_if(pl.inventory.begin(), pl.inventory.end(),
                               [&](InventoryItem const& i) { return i.entry == itemEntry; });
        if (it == pl.inventory.end())
        {
            SendActionResult(CMSG_USE_ITEM, ACTION_NO_ITEM);
            return;
        }

        auto tpl = g_world->FindItem(itemEntry);
        if (!tpl || tpl->itemClass != 3 || tpl->healAmount == 0)
        {
            SendActionResult(CMSG_USE_ITEM, ACTION_NO_ITEM);
            return;
        }

        pl.health = std::min(pl.maxHealth, pl.health + tpl->healAmount);
        if (--it->count == 0)
            pl.inventory.erase(it);

        SendStats(pl);
        SendInventory(pl);
        SendActionResult(CMSG_USE_ITEM, ACTION_OK);
    }

    void EquipItem(Player& pl, uint32_t itemEntry)
    {
        auto it = std::find_if(pl.inventory.begin(), pl.inventory.end(),
                               [&](InventoryItem const& i) { return i.entry == itemEntry; });
        if (it == pl.inventory.end())
        {
            SendActionResult(CMSG_EQUIP_ITEM, ACTION_NO_ITEM);
            return;
        }

        auto tpl = g_world->FindItem(itemEntry);
        if (!tpl || (tpl->itemClass != 1 && tpl->itemClass != 2))
        {
            SendActionResult(CMSG_EQUIP_ITEM, ACTION_NO_ITEM);
            return;
        }

        pl.inventory.erase(it);
        pl.baseDamage += (tpl->minDamage + tpl->maxDamage) / 2;
        pl.armor += tpl->armor;

        SendStats(pl);
        SendInventory(pl);
        SendActionResult(CMSG_EQUIP_ITEM, ACTION_OK);
    }

    bool _authed = false;
    bool _inWorld = false;
    uint32_t _accountId = 0;
    uint32_t _characterGuid = 0;
};

// Реестр «персонаж -> сессия»: нужен для доставки пакетов от тика и соседним игрокам.
void WorldTick(uint32_t nowMs);
void WorldSession::BroadcastToSession(uint32_t guid, uint16_t opcode, ByteBuffer const& body)
{
    auto it = g_sessionsByCharacter.find(guid);
    if (it == g_sessionsByCharacter.end() || !it->second) return;
    it->second->Send(opcode, body);
}

// ------------------------------------------------------------
// Игровой тик: респавн существ и атаки мобов.
// Вызывается из цикла select() по таймауту.
// ------------------------------------------------------------
void WorldTick(uint32_t nowMs)
{
    // Атаки игроков по выбранной цели (пакет CMSG_ATTACK только задаёт цель).
    for (auto& [pguid, pl] : g_world->Players())
    {
        if (!pl.inWorld) continue;
        auto it = g_sessionsByCharacter.find(pguid);
        if (it != g_sessionsByCharacter.end() && it->second)
            it->second->TickPlayer(nowMs);
    }

    for (auto& [guid, c] : g_world->Creatures())
    {
        if (!c.alive)
        {
            if (nowMs >= c.respawnAtMs)
            {
                c.alive = true;
                c.health = c.maxHealth;
                c.position = c.homePosition;
                c.loot.clear();
                c.targetGuid = 0;

                for (auto& [pguid, pl] : g_world->Players())
                {
                    if (!pl.inWorld) continue;
                    if (pl.position.DistanceTo2D(c.position) > 100.f) continue;

                    ByteBuffer out;
                    out << c.guid << c.health << c.position.x << c.position.y
                        << c.position.z << c.position.o;
                    auto it = g_sessionsByCharacter.find(pl.guid);
                    if (it != g_sessionsByCharacter.end() && it->second)
                        it->second->Send(SMSG_CREATURE_RESPAWN, out);
                }
            }
            continue;
        }

        // агрессия на ближайшего игрока
        if (c.targetGuid == 0)
        {
            Player* victim = nullptr;
            float best = c.aggroRange;
            for (auto& [pguid, pl] : g_world->Players())
            {
                if (!pl.inWorld || pl.mapId != c.mapId) continue;
                const float d = c.position.DistanceTo2D(pl.position);
                if (d < best)
                {
                    best = d;
                    victim = &pl;
                }
            }
            if (victim)
            {
                c.targetGuid = victim->guid;
                c.targetKind = TARGET_PLAYER;
                c.nextAttackAtMs = nowMs;
            }
        }

        if (c.targetGuid == 0) continue;
        if (nowMs < c.nextAttackAtMs) continue;

        Player* victim = g_world->FindPlayer(c.targetGuid);
        if (!victim || !victim->inWorld || victim->health == 0)
        {
            c.targetGuid = 0;
            continue;
        }

        const float d = c.position.DistanceTo2D(victim->position);
        if (d > c.attackRange)
        {
            // подтягиваемся к цели
            const float speed = 2.5f; // м в тик
            const float dx = victim->position.x - c.position.x;
            const float dy = victim->position.y - c.position.y;
            const float len = std::sqrt(dx * dx + dy * dy);
            if (len > 0.001f)
            {
                c.position.x += dx / len * speed;
                c.position.y += dy / len * speed;
                c.position.o = AngleTowards(c.position, victim->position);
            }
            continue;
        }

        c.nextAttackAtMs = nowMs + c.attackSpeedMs;
        const uint32_t dmg = std::max<uint32_t>(1, Rng::Range(c.minDamage, c.maxDamage) * 100 /
                                                   (100 + victim->armor));
        victim->health = (victim->health > dmg) ? victim->health - dmg : 0;

        for (auto& [pguid, pl] : g_world->Players())
        {
            if (!pl.inWorld) continue;
            ByteBuffer out;
            out << c.guid << victim->guid << dmg << uint8_t(TARGET_PLAYER);
            auto it = g_sessionsByCharacter.find(pl.guid);
            if (it != g_sessionsByCharacter.end() && it->second)
                it->second->Send(SMSG_DAMAGE, out);
        }

        auto sit = g_sessionsByCharacter.find(victim->guid);
        if (sit != g_sessionsByCharacter.end() && sit->second)
        {
            ByteBuffer stats;
            stats << victim->health << victim->maxHealth << victim->mana << victim->maxMana
                  << uint32_t(victim->level) << victim->xp << victim->xpNext << victim->money
                  << victim->TotalAttackPower() << victim->armor;
            sit->second->Send(SMSG_UPDATE_STATS, stats);
        }

        if (victim->health == 0)
        {
            std::cout << "[world] player '" << victim->name << "' died to '" << c.name << "'\n";
            victim->health = victim->maxHealth;
            victim->position = {0.f, 0.f, 100.f, 0.f};
            victim->targetGuid = 0;
            c.targetGuid = 0;
        }
    }
}

int main(int argc, char** argv)
{
    try
    {
        WinsockInitializer winsock;

        Config cfg;
        const char* configPath = (argc > 1) ? argv[1] : "gameserver.conf";
        if (!cfg.Load(configPath))
            std::cout << "[world] config not found (" << configPath << "), using defaults\n";

        std::string host = cfg.GetString("WorldDatabase", "DBHost", "127.0.0.1");
        unsigned port = static_cast<unsigned>(cfg.GetInt("WorldDatabase", "DBPort", 3306));
        std::string user = cfg.GetString("WorldDatabase", "DBUser", "root");
        std::string pass = cfg.GetString("WorldDatabase", "DBPassword", "root");
        std::string authDbName = cfg.GetString("WorldDatabase", "AuthDBName", "ndv_auth");
        std::string charDbName = cfg.GetString("WorldDatabase", "DBName", "ndv_world");
        int listenPort = cfg.GetInt("Network", "ListenPort", 8085);

        Database authDb;
        Database charDb;

        if (!authDb.Open(host, port, user, pass, authDbName))
        {
            std::cerr << "[world] cannot connect to auth DB " << authDbName << "\n";
            return 1;
        }
        if (!charDb.Open(host, port, user, pass, charDbName))
        {
            std::cerr << "[world] cannot connect to characters DB " << charDbName << "\n";
            return 1;
        }

        World world;
        if (!world.LoadFromDb(charDb))
        {
            std::cerr << "[world] failed to load world content\n";
            return 1;
        }

        std::cout << "[world] databases ready: " << authDbName << " + " << charDbName << std::endl;
        std::cout.flush();

        TokenStore tokens(authDb);
        tokens.PurgeExpired();

        g_charDb = &charDb;
        g_tokens = &tokens;
        g_world = &world;

        Acceptor<WorldSession> acceptor(static_cast<uint16_t>(listenPort));
        acceptor.SetTick([] { WorldTick(NowMs()); });
        acceptor.Run();
        return 0;
    }
    catch (std::exception const& e)
    {
        std::cerr << "[world] fatal: " << e.what() << "\n";
        return 1;
    }
}
