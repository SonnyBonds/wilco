#pragma once

#include <core/typedid.h>

struct Architecture : public TypedId<Architecture> {
    static inline Architecture current();
};

template<> struct std::hash<Architecture> : public Architecture::Hash { };

namespace architecture
{
    inline Architecture X86{"x86"};
    inline Architecture X64{"x64"};
    inline Architecture Arm64{"arm64"};
}

inline Architecture Architecture::current()
{
#if defined(__x86_64__) || defined(_M_X64)
    return architecture::X64;
#elif defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
    return architecture::X86;
#elif defined(__aarch64__)
    return architecture::Arm64;
#else
    #error "Can't determine what architecture we're compiling on"
#endif
}
