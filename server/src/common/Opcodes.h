#pragma once
#include <cstdint>

// Keep in sync with client/Source/NightOfDarkRaven/NDVOpcodes.h
enum Opcodes : uint16_t
{
    // auth
    CMSG_AUTH_LOGON          = 0x0001, // string account, string password
    SMSG_AUTH_LOGON_RESULT   = 0x0002, // u8 result, [u64 token, string realmName, string host, u16 port]

    // server -> server token forward (authserver -> worldserver)
    CMSG_AUTH_TOKEN_FORWARD  = 0x0004, // u32 token
    SMSG_AUTH_TOKEN_FORWARD  = 0x0005, // u8 result

    // world / character selection
    CMSG_WORLD_AUTH          = 0x0100, // u64 token
    SMSG_WORLD_AUTH_RESULT   = 0x0101, // u8 result
    CMSG_PING                = 0x0102, // u32 counter
    SMSG_PONG                = 0x0103, // u32 counter
    CMSG_PLAYER_LOGIN        = 0x0104, // string characterName
    SMSG_LOGIN_VERIFY_WORLD  = 0x0105, // u32 map, f32 x, f32 y, f32 z, f32 o
    CMSG_CHAR_ENUM           = 0x0106, // no body
    SMSG_CHAR_ENUM           = 0x0107, // u8 count, per char: u32 guid, string name, u8 race, u8 class, u8 level, u32 map, f32 x, f32 y, f32 z, f32 o
    CMSG_CHAR_CREATE         = 0x0108, // string name, u8 race, u8 class
    SMSG_CHAR_CREATE_RESULT  = 0x0109, // u8 result, [u32 guid]
    CMSG_CHAR_DELETE         = 0x010A, // u32 guid
    SMSG_CHAR_DELETE_RESULT  = 0x010B, // u8 result

    // in-world: stats
    SMSG_UPDATE_STATS        = 0x0110, // u32 health, u32 maxHealth, u32 mana, u32 maxMana, u32 level, u32 xp, u32 xpNext, u32 money, u32 attackPower, u32 armor

    // in-world: movement
    CMSG_MOVE                = 0x0120, // f32 x, f32 y, f32 z, f32 o
    SMSG_MOVE                = 0x0121, // u32 guid, f32 x, f32 y, f32 z, f32 o

    // in-world: visibility
    SMSG_PLAYER_VISIBLE      = 0x0122, // u32 guid, string name, u8 race, u8 klass, u8 level, f32 x, f32 y, f32 z, f32 o
    SMSG_PLAYER_GONE         = 0x0123, // u32 guid
    CMSG_REQUEST_OBJECTS     = 0x0124, // no body — запросить список существ рядом
    SMSG_CREATURE_VISIBLE    = 0x0125, // u32 guid, u32 entry, string name, u32 health, u32 maxHealth, u8 level, f32 x, f32 y, f32 z, f32 o
    SMSG_CREATURE_GONE       = 0x0126, // u32 guid
    SMSG_CREATURE_MOVE       = 0x0127, // u32 guid, f32 x, f32 y, f32 z, f32 o

    // in-world: chat
    CMSG_CHAT_SAY            = 0x0130, // string message
    SMSG_CHAT_SAY            = 0x0131, // u32 senderGuid, string senderName, string message

    // in-world: combat
    CMSG_ATTACK              = 0x0140, // u32 targetGuid, u8 targetKind (0 = player, 1 = creature)
    CMSG_STOP_ATTACK         = 0x0141, // no body
    SMSG_ATTACK_START        = 0x0142, // u32 attackerGuid, u32 targetGuid
    SMSG_ATTACK_STOP         = 0x0143, // u32 attackerGuid
    SMSG_DAMAGE              = 0x0144, // u32 attackerGuid, u32 victimGuid, u32 amount, u8 victimKind
    SMSG_UNIT_DIED           = 0x0145, // u32 guid, u8 kind
    SMSG_CREATURE_RESPAWN    = 0x0146, // u32 guid, u32 health, f32 x, f32 y, f32 z, f32 o

    // in-world: loot / inventory
    CMSG_LOOT                = 0x0150, // u32 creatureGuid
    SMSG_LOOT_LIST           = 0x0151, // u8 count, per item: u32 itemEntry, string name, u32 count
    CMSG_LOOT_TAKE           = 0x0152, // u32 creatureGuid, u32 itemEntry
    SMSG_INVENTORY           = 0x0153, // u8 count, per item: u32 itemEntry, string name, u32 count, u8 itemClass, u8 quality
    CMSG_USE_ITEM            = 0x0154, // u32 itemEntry
    CMSG_EQUIP_ITEM          = 0x0155, // u32 itemEntry
    SMSG_ACTION_RESULT       = 0x0156, // u8 action, u8 result, [u32 value]
};

// Коды результата авторизации / простых действий
enum AuthResult : uint8_t
{
    AUTH_OK          = 0,
    AUTH_FAILED      = 1,
    AUTH_BAD_TOKEN   = 2,
    AUTH_BANNED      = 3,
    AUTH_NOT_LOGGED  = 4,
    AUTH_NO_CHAR     = 5,
    AUTH_CHAR_EXISTS = 6,
    AUTH_BAD_NAME    = 7,
    AUTH_TOO_MANY    = 8,
    AUTH_DB_ERROR    = 9,
};

// Результаты инвентарных действий
enum ActionResult : uint8_t
{
    ACTION_LOOT_OK     = 1,
    ACTION_NO_ITEM     = 2,
    ACTION_FULL        = 3,
    ACTION_OK          = 4,
};

enum TargetKind : uint8_t
{
    TARGET_PLAYER   = 0,
    TARGET_CREATURE = 1,
};
