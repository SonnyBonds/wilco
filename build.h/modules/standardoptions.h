#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "core/option.h"
#include "modules/feature.h"

Option<std::string> Platform{"Platform"};
Option<std::vector<std::filesystem::path>> IncludePaths{"IncludePaths"};
Option<std::vector<std::filesystem::path>> LibPaths{"LibPaths"};
Option<std::vector<std::filesystem::path>> Files{"Files"};
Option<std::vector<std::filesystem::path>> GeneratorDependencies{"GeneratorDependencies"};
Option<std::vector<std::filesystem::path>> Libs{"Libs"};
Option<std::vector<std::string>> Defines{"Defines"};
Option<std::vector<Feature>> Features{"Features"};
Option<std::vector<std::string>> Frameworks{"Frameworks"};
Option<std::filesystem::path> OutputDir{"OutputDir"};
Option<std::string> OutputStem{"OutputStem"};
Option<std::string> OutputExtension{"OutputExtension"};
Option<std::string> OutputPrefix{"OutputPrefix"};
Option<std::string> OutputSuffix{"OutputSuffix"};
Option<std::filesystem::path> OutputPath{"OutputPath"};
Option<std::filesystem::path> BuildPch{"BuildPch"};
Option<std::filesystem::path> ImportPch{"ImportPch"};
Option<std::filesystem::path> DataDir{"DataDir"};
