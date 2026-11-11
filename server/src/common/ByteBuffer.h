#pragma once
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

// Little-endian packet buffer (TrinityCore ByteBuffer style).
class ByteBuffer
{
public:
    ByteBuffer() = default;
    explicit ByteBuffer(std::vector<uint8_t> data) : _s(std::move(data)) {}

    template <typename T, typename std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, int> = 0>
    ByteBuffer& operator<<(T v)
    {
        static_assert(sizeof(T) <= 8, "unsupported integer size");
        uint8_t tmp[sizeof(T)];
        using U = std::make_unsigned_t<T>;
        U u = static_cast<U>(v);
        for (size_t i = 0; i < sizeof(T); ++i)
            tmp[i] = static_cast<uint8_t>((u >> (8 * i)) & 0xFF);
        _s.insert(_s.end(), tmp, tmp + sizeof(T));
        return *this;
    }

    ByteBuffer& operator<<(bool v) { return *this << static_cast<uint8_t>(v ? 1 : 0); }

    template <typename T, typename std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
    ByteBuffer& operator<<(T v)
    {
        static_assert(std::is_same_v<T, float> || std::is_same_v<T, double>, "unsupported float size");
        uint8_t tmp[sizeof(T)];
        std::memcpy(tmp, &v, sizeof(T));
        _s.insert(_s.end(), tmp, tmp + sizeof(T));
        return *this;
    }

    ByteBuffer& operator<<(std::string const& v)
    {
        _s.insert(_s.end(), v.begin(), v.end());
        _s.push_back(0);
        return *this;
    }

    template <typename T, typename std::enable_if_t<std::is_integral_v<T> && !std::is_same_v<T, bool>, int> = 0>
    ByteBuffer& operator>>(T& v)
    {
        need(sizeof(T));
        using U = std::make_unsigned_t<T>;
        U u = 0;
        for (size_t i = 0; i < sizeof(T); ++i)
            u |= static_cast<U>(_s[_r + i]) << (8 * i);
        _r += sizeof(T);
        v = static_cast<T>(u);
        return *this;
    }

    template <typename T, typename std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
    ByteBuffer& operator>>(T& v)
    {
        need(sizeof(T));
        std::memcpy(&v, _s.data() + _r, sizeof(T));
        _r += sizeof(T);
        return *this;
    }

    ByteBuffer& operator>>(std::string& v) { return ReadString(v, 512); }

    ByteBuffer& ReadString(std::string& v, size_t maxLen)
    {
        v.clear();
        while (true)
        {
            need(1);
            char c = static_cast<char>(_s[_r++]);
            if (!c) break;
            if (v.size() >= maxLen)
                throw std::out_of_range("ByteBuffer string too long");
            v.push_back(c);
        }
        return *this;
    }

    std::vector<uint8_t> const& contents() const { return _s; }
    size_t size() const { return _s.size(); }
    size_t remaining() const { return _s.size() - _r; }
    size_t readPos() const { return _r; }
    void resetRead() { _r = 0; }

private:
    void need(size_t n) const
    {
        if (_r + n > _s.size()) throw std::out_of_range("ByteBuffer underflow");
    }
    std::vector<uint8_t> _s;
    size_t _r = 0;
};
