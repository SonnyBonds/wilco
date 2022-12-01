#include "modules/language.h"

Language Language::getByExtension(StringId extension)
{
    auto it = getExtensionMap().find(extension);
    if(it != getExtensionMap().end())
    {
        return it->second;
    }

    return lang::None;
}

Language Language::getByPath(const std::filesystem::path& path)
{
    return getByExtension(StringId(path.extension().string()));
}

std::unordered_map<StringId, Language>& Language::getExtensionMap()
{
    static std::unordered_map<StringId, Language> extensionMap = {
        { ".c", lang::C },
        { ".cpp", lang::Cpp },
        { ".cxx", lang::Cpp },
        { ".m", lang::ObjectiveC },
        { ".mm", lang::ObjectiveCpp },
        { ".rc", lang::Rc},
    };
    return extensionMap;
}
