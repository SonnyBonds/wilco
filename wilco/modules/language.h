#pragma once

#include <string>
#include <string_view>
#include <filesystem>
#include <unordered_map>
#include <core/typedid.h>

struct Language : public TypedId<Language>
{
    static Language getByExtension(const std::string& extension);

    static Language getByPath(const std::filesystem::path& path);

    static std::unordered_map<std::string, Language>& getExtensionMap();
};

template<> struct std::hash<Language> : public Language::Hash {};

namespace lang
{

inline Language Auto{"Auto"};
inline Language C{"C"};
inline Language Cpp{ "C++" };
inline Language Rc{"Rc"};
inline Language ObjectiveC{"Objective-C"};
inline Language ObjectiveCpp{"Objective-C++"};
inline Language None{"None"};

}
