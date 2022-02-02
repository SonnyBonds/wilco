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
    auto& availableEmitters = Emitters::list();

    if(availableEmitters.empty())
    {
        throw std::runtime_error("No emitters available.");
    }

    auto printUsage = [&arguments, &availableEmitters](){
        std::cout << "Usage: " << arguments[0] << " [--outputPath=<target path for build files>] <emitter> [options to emitter] \n";
        std::cout << "Example: " << arguments[0] << " direct\n\n";
        std::cout << "Available emitters: \n";
        for(auto& emitter : availableEmitters)
        {
            std::cout << "  " << emitter->name << " " << emitter->usage << "\n";
        }
        std::cout << "\n\n";
    };

    Emitter* selectedEmitter = nullptr;
    std::filesystem::path outputPath = "buildfiles";
    int consumedArguments = 0;

    try
    {
        for(size_t i = 1; i<arguments.size(); ++i)
        {
            auto& arg = arguments[i];
            if(arg == "--help" || arg == "-h")
            {
                printUsage();
                return;
            }
            else if(arg.find("--outputPath") == 0)
            {
                outputPath = str::split(arg, '=').second;
                if(outputPath.empty())
                {
                    throw std::runtime_error("Expected value for option '--outputPath', e.g. '--outputPath=buildfiles'.");
                }
                consumedArguments = i+1;
                continue;
            }
            else
            {
                for(auto emitter : availableEmitters)
                {
                    if(emitter->name.cstr() == arg)
                    {
                        selectedEmitter = emitter;
                        break;
                    }
                }
                if(!selectedEmitter)
                {
                    throw std::runtime_error("Unknown argument '" + arg + "'.");
                }
                consumedArguments = i+1;
                break;
            }
        }

        if(selectedEmitter == nullptr)
        {
            throw std::runtime_error("No emitters specified.");
        }
    }
    catch(const std::exception& e)
    {
        printUsage();

        throw;
    }
    
    if(!outputPath.is_absolute())
    {
        outputPath = startPath / outputPath;
    }
    
    EmitterArgs args;
    args.targetPath = outputPath;
    args.projects = projects;
    args.allCliArgs = arguments;
    args.cliArgs = arguments;
    args.cliArgs.erase(args.cliArgs.begin(), args.cliArgs.begin() + consumedArguments);
    selectedEmitter->emit(args);
}

}