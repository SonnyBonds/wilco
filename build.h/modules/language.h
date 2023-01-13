#pragma once

#include "core/stringid.h"

#include <filesystem>
#include <unordered_map>

struct Language : public StringId {
    static Language getByExtension(StringId extension);

    static Language getByPath(const std::filesystem::path& path);

    static std::unordered_map<StringId, Language>& getExtensionMap();
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
