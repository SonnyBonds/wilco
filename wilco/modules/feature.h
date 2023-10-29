#pragma once

#include "core/typedid.h"

struct Feature : public TypedId<Feature> {};
template<> struct std::hash<Feature> : public Feature::Hash {};

namespace feature
{

/** Enable C++11 language features. */
inline Feature Cpp11{"Cpp11"};
/** Enable C++14 language features. */
inline Feature Cpp14{"Cpp14"};
/** Enable C++17 language features. */
inline Feature Cpp17{"Cpp17"};
/** Enable C++20 language features. */
inline Feature Cpp20{"Cpp20"};
/** Enable C++23 language features. */
inline Feature Cpp23{"Cpp23"};
/** Treat warnings as errors. */
inline Feature WarningsAsErrors{"WarningsAsErrors"};
/** Enable default debug symbol output.
 * Corresponds to "-g" on gcc/clang and "/Zi" on msvc/cl.
 * Use compiler specific flags for more detailed control.
*/
inline Feature DebugSymbols{"DebugSymbols"};
/** Enable default exception handling.
 * Corresponds to "-fexceptions" on gcc/clang and "/EHsc" on msvc/cl.
 * Use compiler specific flags for more detailed control.
*/
inline Feature Exceptions{"Exceptions"};
/** Enable default optimization (maximizing speed).
 * Corresponds to "-O3" on gcc/clang and "/O2" on msvc/cl.
 * Use compiler specific flags for more detailed control.
*/
inline Feature Optimize{"Optimize"};
/** Enable fast floating point model. 
 * Corresponds to "-ffast-math" on gcc/clang and "/fp:fast" on msvc/cl.
 * Use compiler specific flags for more detailed control.
*/
inline Feature FastMath{"FastMath"};

namespace windows
{
    /** Link with static c++ runtime on windows. */
    inline Feature StaticRuntime{ "StaticRuntime" };
    /** Link with static debug c++ runtime on windows. */
    inline Feature StaticDebugRuntime{ "StaticDebugRuntime" };
    /** Link with shared c++ runtime on windows. */
    inline Feature SharedRuntime{ "SharedRuntime" };
    /** Link with shared debug c++ runtime on windows. */
    inline Feature SharedDebugRuntime{ "SharedDebugRuntime" };
}

namespace macos
{
    /** Output shared libraries as mach-o bundles. */
    inline Feature Bundle{"Bundle"};
}

}
