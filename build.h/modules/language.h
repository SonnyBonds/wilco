#pragma once

#include <unordered_map>

#include "core/stringid.h"

struct Language : public StringId {
    static std::unordered_map<StringId, Language> extensionMap;

    static Language getByExtension(StringId extension);
    static Language getByPath(const std::filesystem::path& path)
    {
        return getByExtension(StringId(path.extension().string()));
    }
};

namespace lang
{

inline Language Auto{"Auto"};
inline Language C{"C"};
inline Language Cpp{"C++"};
inline Language ObjectiveC{"Objective-C"};
inline Language ObjectiveCpp{"Objective-C++"};
inline Language None{"None"};

}

inline std::unordered_map<StringId, Language> Language::extensionMap = 
{
    { ".c", lang::C },
    { ".cpp", lang::Cpp },
    { ".cxx", lang::Cpp },
    { ".m", lang::ObjectiveC },
    { ".mm", lang::ObjectiveCpp },
};

inline Language Language::getByExtension(StringId extension)
{
    auto it = extensionMap.find(extension);
    if(it != extensionMap.end())
    {
        return it->second;
    }

    return lang::None;
}