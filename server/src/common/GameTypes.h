#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

// Игровые типы мира: существа, игроки, бой.
// Координаты в метрах, ориентация в радианах.

struct Vector3
{
    float x = 0.f, y = 0.f, z = 0.f, o = 0.f;

    float DistanceTo(Vector3 const& other) const
    {
        const float dx = x - other.x;
        const float dy = y - other.y;
        const float dz = z - other.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    float DistanceTo2D(Vector3 const& other) const
    {
        const float dx = x - other.x;
        const float dy = y - other.y;
        return std::sqrt(dx * dx + dy * dy);
    }
};

// ------------------------------------------------------------
// Во время выполнения
// ------------------------------------------------------------
struct CreatureTemplate
{
    uint32_t entry = 0;
    std::string name;
    uint8_t minLevel = 1;
    uint8_t maxLevel = 1;
    uint32_t health = 100;
    uint32_t minDamage = 1;
    uint32_t maxDamage = 2;
    uint32_t attackSpeedMs = 2000;
    float attackRange = 5.f;
    uint32_t xpReward = 10;
    float aggroRange = 10.f;
    uint8_t faction = 0;
};

struct Creature
{
    uint32_t guid = 0;
    uint32_t entry = 0;
    std::string name;
    uint8_t level = 1;
    uint32_t health = 100;
    uint32_t maxHealth = 100;
    uint32_t minDamage = 1;
    uint32_t maxDamage = 2;
    uint32_t attackSpeedMs = 2000;
    float attackRange = 5.f;
    uint32_t xpReward = 10;
    float aggroRange = 10.f;

    Vector3 position;
    Vector3 homePosition;
    uint32_t mapId = 0;

    uint32_t respawnSeconds = 30;
    bool alive = true;
    uint32_t respawnAtMs = 0;

    // бой
    uint32_t targetGuid = 0;
    uint8_t targetKind = 1;
    uint32_t nextAttackAtMs = 0;

    // лут, выпавший при смерти (itemEntry -> count)
    std::unordered_map<uint32_t, uint32_t> loot;
    bool lootClaimed = false;
};

struct ItemTemplate
{
    uint32_t entry = 0;
    std::string name;
    uint8_t itemClass = 0;
    uint8_t quality = 1;
    uint32_t stackable = 1;
    uint32_t armor = 0;
    uint32_t minDamage = 0;
    uint32_t maxDamage = 0;
    uint32_t healAmount = 0;
    uint32_t buyPrice = 0;
};

struct InventoryItem
{
    uint32_t entry = 0;
    uint32_t count = 1;
    int16_t slot = 0;
};

struct Player
{
    uint32_t guid = 0;
    uint32_t accountId = 0;
    std::string name;
    uint8_t race = 1;
    uint8_t klass = 1;
    uint8_t level = 1;

    uint32_t health = 100;
    uint32_t maxHealth = 100;
    uint32_t mana = 100;
    uint32_t maxMana = 100;
    uint32_t xp = 0;
    uint32_t xpNext = 100;
    uint32_t money = 0;

    uint32_t baseDamage = 3;
    uint32_t armor = 0;

    Vector3 position;
    uint32_t mapId = 0;

    bool inWorld = false;

    // бой
    uint32_t targetGuid = 0;
    uint8_t targetKind = 0;
    uint32_t nextAttackAtMs = 0;

    std::vector<InventoryItem> inventory;

    uint32_t TotalAttackPower() const { return baseDamage + level * 2; }
};

inline float NormalizeAngle(float a)
{
    constexpr float pi = 3.14159265358979f;
    while (a > pi) a -= 2 * pi;
    while (a < -pi) a += 2 * pi;
    return a;
}

inline float AngleTowards(Vector3 const& from, Vector3 const& to)
{
    return std::atan2(to.y - from.y, to.x - from.x);
}

inline bool InFrontArc(Vector3 const& attacker, Vector3 const& victim, float arcRadians)
{
    const float facing = attacker.o;
    const float angleTo = AngleTowards(attacker, victim);
    return std::fabs(NormalizeAngle(angleTo - facing)) <= arcRadians * 0.5f;
}

class Rng
{
public:
    static uint32_t Next(uint32_t maxExclusive)
    {
        if (maxExclusive == 0) return 0;
        return std::uniform_int_distribution<uint32_t>(0, maxExclusive - 1)(Engine());
    }

    static uint32_t Range(uint32_t minValue, uint32_t maxValue)
    {
        if (maxValue <= minValue) return minValue;
        return std::uniform_int_distribution<uint32_t>(minValue, maxValue)(Engine());
    }

    static float RangeF(float minValue, float maxValue)
    {
        if (maxValue <= minValue) return minValue;
        return std::uniform_real_distribution<float>(minValue, maxValue)(Engine());
    }

private:
    static std::mt19937& Engine()
    {
        static std::mt19937 gen{ std::random_device{}() };
        return gen;
    }
};

// Опыт, необходимый для следующего уровня
inline uint32_t XpForNextLevel(uint8_t level)
{
    return 100u * level + (level > 1 ? 20u * (level - 1) : 0u);
}
