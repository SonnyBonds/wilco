#pragma once

#include <string>

struct OperatingSystem {
    std::string id;

    static inline OperatingSystem current();

    bool operator ==(const OperatingSystem& other) const { return id == other.id; }
    bool operator !=(const OperatingSystem& other) const { return id != other.id; }
    bool operator <(const OperatingSystem& other) const { return id < other.id; }
    operator const std::string&() const { return id; }
};

template<>
struct std::hash<OperatingSystem>
{
    std::size_t operator()(const OperatingSystem& os) const
    {
        return std::hash<std::string>{}(os.id);
    }
};

inline OperatingSystem Windows{"Windows"};
inline OperatingSystem MacOS{"MacOS"};
inline OperatingSystem Linux{"Linux"};

inline OperatingSystem OperatingSystem::current()
{
    // TODO: Support a notion of different build and host systems?
#if defined(_WIN32)
    return Windows;
#elif defined(__APPLE__)
    return MacOS;
#elif defined(__linux__)
    return Linux;
#else
#error "Can't determine what Operating System we're compiling on"
#endif
}
