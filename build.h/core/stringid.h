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

    static size_t getStorageSize()
    {
        return getStorage().size();
    }

private:
    const char* _cstr;
    
    static std::unordered_set<std::string>& getStorage()
    {
        static std::unordered_set<std::string> storage;
        return storage;
    }

    static StringId get(const char* str)
    {
        if(str == nullptr || str[0] == 0)
        {
            return StringId();
        }
        
        auto& storage = getStorage();
        auto entry = storage.insert(str).first;

        StringId result;
        result._cstr = entry->c_str();
        return result;
    }
};
