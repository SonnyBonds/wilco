#pragma once

struct Feature {
    std::string id;

    bool operator ==(const Feature& other) const { return id == other.id; }
    bool operator !=(const Feature& other) const { return id != other.id; }
    bool operator <(const Feature& other) const { return id < other.id; }
    operator const std::string&() const { return id; }
};

template<>
struct std::hash<Feature>
{
    std::size_t operator()(const Feature& feature) const
    {
        return std::hash<std::string>{}(feature.id);
    }
};

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
