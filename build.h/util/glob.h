#pragma once

#include <filesystem>

#include "core/option.h"
#include "modules/bundle.h"

namespace glob
{

OptionCollection bundleResources(const std::filesystem::path& path, const std::filesystem::path& subPath = {})
{
    OptionCollection result;
    for(auto entry : std::filesystem::recursive_directory_iterator(path))
    {            
        if(!entry.is_regular_file()) continue;
        result[BundleContents] += BundleEntry{ entry.path(), "Contents/Resources/" / subPath / std::filesystem::relative(entry, path) };
    }

    return result;
}

OptionCollection files(const std::filesystem::path& path, const std::vector<std::string>& extensions, bool recurse = true)
{
    OptionCollection result;
    auto& files = result[Files];
    auto& generatorDeps = result[GeneratorDependencies];

    // Add the directory as a dependency to rescan if the contents change
    generatorDeps += path;

    if(!std::filesystem::exists(path))
    {
        return result;
    }

    for(auto entry : std::filesystem::recursive_directory_iterator(path))
    {
        if(entry.is_directory())
        {
            // Add subdirectories as dependencies to rescan if the contents change
            generatorDeps += path;
            continue;
        }
        if(!entry.is_regular_file()) continue;

        auto ext = entry.path().extension().string();
        if(std::find(extensions.begin(), extensions.end(), ext) != extensions.end())
        {
            files += entry.path();
        }
    }

    return result;
}

OptionCollection sources(const std::filesystem::path& path, bool recurse = true)
{
    return files(path, { ".c", ".cpp", ".mm", ".h", ".hpp" }, recurse);
}

}