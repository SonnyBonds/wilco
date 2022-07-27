#pragma once

#include <filesystem>

#include "core/property.h"
#include "modules/bundle.h"

namespace glob
{

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

template<typename T>
std::vector<std::filesystem::path> files(const std::filesystem::path& path, const T& filter, bool recurse = true)
{
    std::vector<std::filesystem::path> result;

    // TODO: Add path as a configuration dependency  

    if(!std::filesystem::exists(path))
    {
        return result;
    }

    auto scan = [&](auto&& iterator)
    {
        for(auto entry : iterator)
        {
            if(entry.is_directory()) {
                // TODO: Add entry as a configuration dependency  
                continue;
            }
            if(!entry.is_regular_file()) continue;

            if(filter(entry.path()))
            {
                result.push_back(entry.path());
            }
        }
    };

    if(recurse)
    {
        scan(std::filesystem::recursive_directory_iterator(path));
    }
    else
    {
        scan(std::filesystem::directory_iterator(path));
    }

    return result;
}

inline std::vector<std::filesystem::path> files(const std::filesystem::path& path, bool recurse = true)
{
    return files(path, [](const std::filesystem::path& path) { return true; }, recurse);
}

}
