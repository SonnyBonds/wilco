#include "modules/language.h"

Language Language::getByExtension(const std::string& extension)
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
    return getByExtension(path.extension().string());
}

std::unordered_map<std::string, Language>& Language::getExtensionMap()
{
    static std::unordered_map<std::string, Language> extensionMap = {
        { ".c", lang::C },
        { ".cpp", lang::Cpp },
        { ".cxx", lang::Cpp },
        { ".m", lang::ObjectiveC },
        { ".mm", lang::ObjectiveCpp },
        { ".rc", lang::Rc},
    };
    return extensionMap;
}
