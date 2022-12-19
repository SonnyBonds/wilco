#pragma once

#include <atomic>
#include <string>
#include <unordered_set>

// Launders strings into directly comparable pointers
struct StringId
{
    StringId() : _cstr(EMPTY) {}
    StringId(const StringId& id) = default;
    StringId(const char* id) : StringId(get(id)) {}
    StringId(const std::string& id) : StringId(get(id)) {}
    StringId(std::string&& id) : StringId(get(std::move(id))) {}
    StringId(const std::string_view& id) : StringId(get(id)) {}

    bool empty() const;
    const char* cstr() const;
    operator const char*() const;
    operator std::string_view() const;
    static size_t getStorageSize();

private:
    static const char* EMPTY;
    const char* _cstr;

    static StringId get(std::string_view str);
    static StringId get(std::string&& str);
    static StringId get(const std::string& str);
    static StringId get(const char* str);
};

template<>
struct std::hash<StringId>
{
    size_t operator()(const StringId& id) const
    {
        return std::hash<const void*>{}(id.cstr());
    }
};

inline bool operator ==(const StringId& a, const StringId& b)
{
    return a.cstr() == b.cstr();
}

inline bool operator !=(const StringId& a, const StringId& b)
{
    return a.cstr() != b.cstr();
}