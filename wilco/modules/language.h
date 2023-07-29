#pragma once

#include <string>
#include <string_view>
#include <filesystem>
#include <unordered_map>

struct Language
{
    std::string id;

    static Language getByExtension(const std::string& extension);

    static Language getByPath(const std::filesystem::path& path);

    static std::unordered_map<std::string, Language>& getExtensionMap();

    bool operator ==(const Language& other) const { return id == other.id; }
    bool operator !=(const Language& other) const { return id != other.id; }
    bool operator <(const Language& other) const { return id < other.id; }
    operator const std::string&() const { return id; }
};

template<>
struct std::hash<Language>
{
    std::size_t operator()(const Language& language) const
    {
        return std::hash<std::string>{}(language.id);
    }
};

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
