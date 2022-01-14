#pragma once

#include "core/stringid.h"

struct OperatingSystem : public StringId {
    static inline OperatingSystem current();
};

OperatingSystem Windows{"Windows"};
OperatingSystem MacOS{"MacOS"};
OperatingSystem Linux{"Linux"};

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
#error "Can't determine what Operating System we're compiling on
#endif
}
