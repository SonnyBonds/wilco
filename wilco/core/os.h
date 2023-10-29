#pragma once

#include <core/typedid.h>

struct OperatingSystem : public TypedId<OperatingSystem> {
    static inline OperatingSystem current();
};

template<>
struct std::hash<OperatingSystem> : public OperatingSystem::Hash { };

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
