#pragma once
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <map>
#include <stdexcept>
#include <string>

// Минимальный INI-парсер: [Section] и Key = Value, '#' и ';' — комментарии.
class Config
{
public:
    bool Load(const std::string& path)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in) return false;
        _path = path;

        std::string section;
        std::string line;
        while (std::getline(in, line))
        {
            Trim(line);
            if (line.empty() || line[0] == '#' || line[0] == ';') continue;

            if (line.front() == '[' && line.back() == ']')
            {
                section = line.substr(1, line.size() - 2);
                Trim(section);
                continue;
            }

            size_t eq = line.find('=');
            if (eq == std::string::npos) continue;

            std::string key = line.substr(0, eq);
            std::string val = line.substr(eq + 1);
            Trim(key);
            Trim(val);
            _values[section + "." + key] = val;
        }
        return true;
    }

    std::string GetString(const std::string& section, const std::string& key,
                          const std::string& def = "") const
    {
        auto it = _values.find(section + "." + key);
        return it == _values.end() ? def : it->second;
    }

    int GetInt(const std::string& section, const std::string& key, int def) const
    {
        auto it = _values.find(section + "." + key);
        if (it == _values.end()) return def;
        try { return std::stoi(it->second); }
        catch (...) { return def; }
    }

    bool GetBool(const std::string& section, const std::string& key, bool def) const
    {
        std::string v = GetString(section, key);
        if (v.empty()) return def;
        std::transform(v.begin(), v.end(), v.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return v == "true" || v == "1" || v == "yes" || v == "on";
    }

    std::string const& Path() const { return _path; }

private:
    static void Trim(std::string& s)
    {
        size_t b = s.find_first_not_of(" \t\r\n");
        size_t e = s.find_last_not_of(" \t\r\n");
        if (b == std::string::npos) { s.clear(); return; }
        s = s.substr(b, e - b + 1);
    }

    std::string _path;
    std::map<std::string, std::string> _values;
};
