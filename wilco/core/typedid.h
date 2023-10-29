#pragma once

#include <string>

template<typename Derived>
struct TypedId {
    std::string id;

    bool operator ==(const Derived& other) const { return id == other.id; }
    bool operator !=(const Derived& other) const { return id != other.id; }
    bool operator <(const Derived& other) const { return id < other.id; }
    operator const std::string&() const { return id; }    

    struct Hash
    {
        std::size_t operator()(const Derived& derived) const
        {
            return std::hash<std::string>{}(derived.id);
        }
    };
};