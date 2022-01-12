#pragma once

#include <filesystem>

#include "core/option.h"

namespace glob
{

OptionCollection files(const std::filesystem::path& path, const std::vector<std::string>& extensions, bool recurse = true)
{
    if(!std::filesystem::exists(path) || !std::filesystem::is_directory(path))
    {
        throw std::runtime_error("Source directory '" + path.string() + "' does not exist.");
    }

    OptionCollection result;
    auto& files = result[Files];
    auto generatorDeps = result[GeneratorDependencies];

    // Add the directory as a dependency to rescan if the contents change
    generatorDeps += path;

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