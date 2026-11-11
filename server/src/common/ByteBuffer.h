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

    template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
    ByteBuffer& operator<<(T v)
    {
        auto p = reinterpret_cast<uint8_t const*>(&v); // assumes little-endian host
        _s.insert(_s.end(), p, p + sizeof(T));
        return *this;
    }
    ByteBuffer& operator<<(std::string const& v)
    {
        _s.insert(_s.end(), v.begin(), v.end());
        _s.push_back(0);
        return *this;
    }

    template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>>
    ByteBuffer& operator>>(T& v)
    {
        need(sizeof(T));
        std::memcpy(&v, _s.data() + _r, sizeof(T));
        _r += sizeof(T);
        return *this;
    }
    ByteBuffer& operator>>(std::string& v)
    {
        v.clear();
        while (true)
        {
            need(1);
            char c = static_cast<char>(_s[_r++]);
            if (!c) break;
            v.push_back(c);
        }
        return *this;
    }

    std::vector<uint8_t> const& contents() const { return _s; }
    size_t size() const { return _s.size(); }

private:
    void need(size_t n) const
    {
        if (_r + n > _s.size()) throw std::out_of_range("ByteBuffer underflow");
    }
    std::vector<uint8_t> _s;
    size_t _r = 0;
};
