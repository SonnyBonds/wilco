#pragma once

#include "core/typedid.h"

struct Feature : public TypedId<Feature> {};
template<> struct std::hash<Feature> : public Feature::Hash {};

namespace feature
{

inline Feature Cpp11{"Cpp11"};
inline Feature Cpp14{"Cpp14"};
inline Feature Cpp17{"Cpp17"};
inline Feature Cpp20{"Cpp20"};
inline Feature Cpp23{"Cpp23"};
inline Feature WarningsAsErrors{"WarningsAsErrors"};
inline Feature FastMath{"FastMath"};
inline Feature DebugSymbols{"DebugSymbols"};
inline Feature Exceptions{"Exceptions"};
inline Feature Optimize{"Optimize"};
inline Feature OptimizeSize{"OptimizeSize"};
inline Feature MacOSBundle{"MacOSBundle"};

}
