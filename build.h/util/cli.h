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

void parseCommandLineAndEmit(std::filesystem::path startPath, const std::vector<std::string> arguments, std::vector<Project*> projects)
{
    auto optionArgs = parseOptionArguments(arguments);
    auto positionalArgs = parsePositionalArguments(arguments);

    auto& availableEmitters = Emitters::list();

    if(availableEmitters.empty())
    {
        throw std::runtime_error("No emitters available.");
    }

    std::filesystem::path outputPath = "buildfiles";

    std::vector<Emitter*> emitters;
    for(auto& arg : optionArgs)
    {
        auto emitterIt = std::find_if(availableEmitters.begin(), availableEmitters.end(), [&arg](auto& v){ return v->name == StringId(arg.first); });
        if(emitterIt != availableEmitters.end())
        {
            emitters.push_back(*emitterIt);
        }
        if(arg.first == "outputPath" && !arg.second.empty())
        {
            outputPath = arg.second;
        }
    }

    if(emitters.empty())
    {
        std::cout << "Usage: " << arguments[0] << "[--outputPath=<target path for build files>] --emitter\n";
        std::cout << "Example: " << arguments[0] << " --outputPath=buildoutput --" << availableEmitters[0]->name << "\n\n";
        std::cout << "Available emitters: \n";
        for(auto& emitter : availableEmitters)
        {
            std::cout << "  --" << emitter->name << "\n";
        }
        std::cout << "\n\n";
        throw std::runtime_error("No emitters specified.");
    }

    for(auto& emitter : emitters)
    {
        if(!outputPath.is_absolute())
        {
            outputPath = startPath / outputPath;
        }
        EmitterArgs args;
        args.targetPath = outputPath;
        args.projects = projects;
        emitter->emit(args);
    }
}

}