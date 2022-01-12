#pragma once

#include <atomic>
#include <string>
#include <unordered_set>

// Launders strings into directly comparable pointers
struct StringId
{
    StringId() : _cstr("") {}
    StringId(const StringId& id) = default;
    StringId(const char* id) : StringId(get(id)) {}
    StringId(const std::string& id) : StringId(get(id.c_str())) {}

    bool empty() const
    {
        return _cstr == nullptr || _cstr[0] == 0;
    }

    const char* cstr() const
    {
        return _cstr;
    }

    operator const char*() const
    {
        return _cstr;
    }

private:
    const char* _cstr;

    static StringId get(const char* str)
    {
        if(str == nullptr || str[0] == 0)
        {
            return StringId();
        }
        
        static std::unordered_set<std::string> storage;
        auto entry = storage.insert(str).first;

        StringId result;
        result._cstr = entry->c_str();
        return result;
    }
};
