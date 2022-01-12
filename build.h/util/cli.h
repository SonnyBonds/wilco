#pragma once

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <stdexcept>
#include <utility>

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

void parseCommandLineAndEmit(std::filesystem::path startPath, const std::vector<std::string> arguments, std::set<Project*> projects, std::set<StringId> configs)
{
    auto optionArgs = parseOptionArguments(arguments);
    auto positionalArgs = parsePositionalArguments(arguments);

    if(configs.empty())
    {
        throw std::runtime_error("No configurations available.");
    }

    std::vector<std::string> availableEmitters = { "ninja" };
    std::vector<std::pair<std::string, std::filesystem::path>> emitters;
    for(auto& arg : optionArgs)
    {
        if(std::find(availableEmitters.begin(), availableEmitters.end(), arg.first) != availableEmitters.end())
        {
            auto targetDir = arg.second;
            if(targetDir.empty())
            {
                targetDir = arg.first + "build";
            }
            emitters.push_back({arg.first, targetDir});
        }
    }

    if(emitters.empty())
    {
        std::cout << "Usage: " << arguments[0] << " --emitter[=†argetDir]\n";
        std::cout << "Example: " << arguments[0] << " --ninja=ninjabuild\n\n";
        std::cout << "Available emitters: \n";
        for(auto& emitter : availableEmitters)
        {
            std::cout << "  --" << emitter << "\n";
        }
        std::cout << "\n\n";
        throw std::runtime_error("No emitters specified.");
    }

    for(auto& emitter : emitters)
    {
        if(emitter.first == "ninja")
        {
            for(auto& config : configs)
            {
                auto outputPath = emitter.second / config.cstr();
                if(!outputPath.is_absolute())
                {
                    outputPath = startPath / outputPath;
                }
                NinjaEmitter::emit(outputPath, projects, config);
            }
        }
    }
}

}