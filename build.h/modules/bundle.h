#pragma once

#include <functional>
#include <string>
#include <variant>

#include "core/project.h"
#include "core/property.h"
#include "modules/feature.h"
#include "util/commands.h"

struct VerbatimPlistValue
{
    std::string valueString;
};

using PlistValue = std::variant<std::string, bool, int, VerbatimPlistValue>;

struct BundleEntry
{
    std::filesystem::path source;
    std::filesystem::path target;

    bool operator <(const BundleEntry& other) const
    {
        {
            int v = source.compare(other.source);
            if(v != 0) return v < 0;
        }

        return target < other.target;
    }

    bool operator ==(const BundleEntry& other) const
    {
        return source == other.source &&
               target == other.target;
    }
};

template<>
struct std::hash<BundleEntry>
{
    std::size_t operator()(BundleEntry const& entry) const
    {
        std::size_t h = std::filesystem::hash_value(entry.source);
        h = h ^ (std::filesystem::hash_value(entry.target) << 1);
        return h;
    }
};

namespace extensions
{
    struct MacOSBundle : public PropertyBag
    {
        Property<bool> create{this};
        Property<std::string> extension{this};
        MapProperty<std::string, PlistValue> plistEntries{this};
        ListProperty<BundleEntry> contents{this};
    };
}

#if TODO
inline OptionCollection bundleResources(const std::filesystem::path& path, const std::filesystem::path& subPath = {})
{
    OptionCollection result;

    if(!std::filesystem::exists(path))
    {
        return result;
    }

    for(auto entry : std::filesystem::recursive_directory_iterator(path))
    {            
        if(!entry.is_regular_file()) continue;
        result[BundleContents] += BundleEntry{ entry.path(), "Contents/Resources/" / subPath / std::filesystem::relative(entry, path) };
    }

    return result;
}
#endif
