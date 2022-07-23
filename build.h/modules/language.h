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

Language Auto{"Auto"};
Language C{"C"};
Language Cpp{"C++"};
Language ObjectiveC{"Objective-C"};
Language ObjectiveCpp{"Objective-C++"};
Language None{"None"};

}

std::unordered_map<StringId, Language> Language::extensionMap = 
{
    { ".c", lang::C },
    { ".cpp", lang::Cpp },
    { ".cxx", lang::Cpp },
    { ".m", lang::ObjectiveC },
    { ".mm", lang::ObjectiveCpp },
};

Language Language::getByExtension(StringId extension)
{
    auto it = extensionMap.find(extension);
    if(it != extensionMap.end())
    {
        return it->second;
    }

    return lang::None;
}