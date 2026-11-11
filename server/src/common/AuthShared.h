#pragma once
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>

// Shared authentication between authserver and worldserver.
// Simple file based token store (dev). In production this is a DB row / redis.

inline const char* kAuthSharedFile = "authShared.txt";

// returns token, or 0 on error
inline uint32_t readSharedToken()
{
    std::ifstream in(kAuthSharedFile);
    if (!in) return 0;
    uint32_t t = 0;
    in.read(reinterpret_cast<char*>(&t), sizeof(t));
    return in && t ? t : 0;
}

inline void writeSharedToken(uint32_t token)
{
    std::ofstream out(kAuthSharedFile, std::ios::trunc | std::ios::binary);
    if (out) out.write(reinterpret_cast<const char*>(&token), sizeof(token));
}
