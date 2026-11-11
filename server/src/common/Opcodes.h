#pragma once
#include <cstdint>

// Keep in sync with client/Source/NightOfDarkRaven/NDVOpcodes.h
// Extended protocol opcodes (client 0x00-0x0F, server 0x80-0x8F)

// --- Authentication opcodes (client -> server) ---
enum eAuthClientOpcode : uint8_t
{
    CMSG_AUTH_LOGON_CHALLENGE      = 0x00, // string account, string password (login + password)
    CMSG_AUTH_LOGON_PROOF          = 0x01, // A (32), M1 (20), crc_hash (20)
    CMSG_REALM_LIST                = 0x10, // empty body
};

// --- Authentication opcodes (server -> client) ---
enum eAuthServerOpcode : uint8_t
{
    SMSG_AUTH_LOGON_CHALLENGE      = 0x80, // result (u8), B (32), g_len (u8), g (u8), N_len (u8), N (32), salt (32), crc_salt (16)
    SMSG_AUTH_LOGON_PROOF          = 0x81, // result (u8), M2 (20), account_id (u32)
    SMSG_AUTH_REALM_LIST           = 0x90, // realm_count (u32), [{id(u32), name(string), address(string), port(u16), type(u8), flags(u8), population(f32)}]
};

// Shared auth result codes
enum AuthResult : uint8_t
{
    AUTH_OK             = 0,
    AUTH_FAILED         = 1,
    AUTH_UNKNOWN_ACCOUNT = 2,
    AUTH_INCORRECT_PASS  = 3,
    AUTH_PROOF_FAILED   = 4,
    AUTH_REALM_SENT     = 5,
};

// --- World opcodes (existing, kept for compatibility) ---
enum Opcodes : uint16_t
{
    // auth
    CMSG_AUTH_LOGON          = 0x0001, // string account, string password
    SMSG_AUTH_LOGON_RESULT   = 0x0002, // [u8 result, [u32 token, string realmName, string host, u16 port]]

    // server -> server
    CMSG_AUTH_TOKEN_FORWARD  = 0x0004, // u32 token
    SMSG_AUTH_TOKEN_FORWARD  = 0x0005, // u8 result

    // world
    CMSG_WORLD_AUTH          = 0x0100, // u32 token
    SMSG_WORLD_AUTH_RESULT   = 0x0101, // u8 result
    CMSG_PING                = 0x0102, // u32 counter
    SMSG_PONG                = 0x0103, // u32 counter
    CMSG_PLAYER_LOGIN        = 0x0104, // string characterName
    SMSG_LOGIN_VERIFY_WORLD  = 0x0105, // u32 map, f32 x, f32 y, f32 z, f32 o
};
#pragma once
#include <cstdint>

// Keep in sync with client/Source/NightOfDarkRaven/NDVOpcodes.h
// Extended protocol opcodes (client 0x00-0x0F, server 0x80-0x8F)

// --- Authentication opcodes (client -> server) ---
enum eAuthClientOpcode : uint8_t
{
    CMSG_AUTH_LOGON_CHALLENGE      = 0x00, // string account, string password (login + password)
    CMSG_AUTH_LOGON_PROOF          = 0x01, // A (32), M1 (20), crc_hash (20)
    CMSG_REALM_LIST                = 0x10, // empty body
};

// --- Authentication opcodes (server -> client) ---
enum eAuthServerOpcode : uint8_t
{
    SMSG_AUTH_LOGON_CHALLENGE      = 0x80, // result (u8), B (32), g_len (u8), g (u8), N_len (u8), N (32), salt (32), crc_salt (16)
    SMSG_AUTH_LOGON_PROOF          = 0x81, // result (u8), M2 (20), account_id (u32)
    SMSG_AUTH_REALM_LIST           = 0x90, // realm_count (u32), [{id(u32), name(string), address(string), port(u16), type(u8), flags(u8), population(f32)}]
};

// Shared auth result codes
enum AuthResult : uint8_t
{
    AUTH_OK             = 0,
    AUTH_FAILED         = 1,
    AUTH_UNKNOWN_ACCOUNT = 2,
    AUTH_INCORRECT_PASS  = 3,
    AUTH_PROOF_FAILED   = 4,
    AUTH_REALM_SENT     = 5,
};