#pragma once

struct Feature : public StringId {};

namespace feature
{

Feature Cpp11{"Cpp11"};
Feature Cpp14{"Cpp14"};
Feature Cpp17{"Cpp17"};
Feature Cpp20{"Cpp20"};
Feature Cpp23{"Cpp23"};
Feature WarningsAsErrors{"WarningsAsErrors"};
Feature FastMath{"FastMath"};
Feature DebugSymbols{"DebugSymbols"};
Feature Exceptions{"Exceptions"};
Feature Optimize{"Optimize"};
Feature OptimizeSize{"OptimizeSize"};

}
