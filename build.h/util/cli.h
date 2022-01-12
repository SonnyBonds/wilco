#pragma once

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <stdexcept>
#include <utility>

#include "core/emitter.h"
#include "util/string.h"

namespace cli
{

std::vector<std::pair<std::string, std::string>> parseOptionArguments(const std::vector<std::string> arguments)
{
    std::vector<std::pair<std::string, std::string>> result;
    for(auto& arg : arguments)
    {
        if(arg.size() > 1 && arg[0] == '-' && arg[1] == '-')
        {
            result.push_back(str::split(arg.substr(2), '='));
        }
    }

    return result;
}

std::vector<std::string> parsePositionalArguments(const std::vector<std::string> arguments, bool skipFirst = true)
{
    std::vector<std::string> result;
    for(auto& arg : arguments)
    {
        if(skipFirst)
        {
            skipFirst = false;
            continue;
        }
        if(arg.size() < 2 || arg[0] != '-' || arg[1] != '-')
        {
            result.push_back(arg);
        }
    }

    return result;
}

void parseCommandLineAndEmit(std::filesystem::path startPath, const std::vector<std::string> arguments, std::vector<Project*> projects, std::set<StringId> configs)
{
    auto optionArgs = parseOptionArguments(arguments);
    auto positionalArgs = parsePositionalArguments(arguments);

    if(configs.empty())
    {
        throw std::runtime_error("No configurations available.");
    }

    auto& availableEmitters = Emitters::list();

    if(availableEmitters.empty())
    {
        throw std::runtime_error("No emitters available.");
    }

    std::vector<std::pair<Emitter, std::filesystem::path>> emitters;
    for(auto& arg : optionArgs)
    {
        auto emitterIt = std::find_if(availableEmitters.begin(), availableEmitters.end(), [&arg](auto& v){ return v.name == arg.first; });
        if(emitterIt != availableEmitters.end())
        {
            auto targetDir = arg.second;
            if(targetDir.empty())
            {
                targetDir = arg.first + "build";
            }
            emitters.push_back({ *emitterIt, targetDir });
        }
    }

    if(emitters.empty())
    {
        std::cout << "Usage: " << arguments[0] << " --emitter[=†argetDir]\n";
        std::cout << "Example: " << arguments[0] << " --" << availableEmitters[0].name << "[=" << availableEmitters[0].name << "build]\n\n";
        std::cout << "Available emitters: \n";
        for(auto& emitter : availableEmitters)
        {
            std::cout << "  --" << emitter.name << "\n";
        }
        std::cout << "\n\n";
        throw std::runtime_error("No emitters specified.");
    }

    for(auto& emitter : emitters)
    {
        for(auto& config : configs)
        {
            auto outputPath = emitter.second / config.cstr();
            if(!outputPath.is_absolute())
            {
                outputPath = startPath / outputPath;
            }
            EmitterArgs args;
            args.targetPath = outputPath;
            args.projects = projects;
            args.config = config;
            emitter.first(args);
        }
    }
}

}