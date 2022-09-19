#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "core/property.h"

struct CommandEntry
{
    std::string command;
    std::vector<std::filesystem::path> inputs;
    std::vector<std::filesystem::path> outputs;
    std::filesystem::path workingDirectory;
    std::filesystem::path depFile;
    std::string description;
    std::filesystem::path rspFile;
    std::string rspContents;

    bool operator ==(const CommandEntry& other) const
    {
        return command == other.command &&
               outputs == other.outputs &&
               inputs == other.inputs &&
               workingDirectory == other.workingDirectory &&
               depFile == other.depFile;
    }
};

template<>
struct std::hash<CommandEntry>
{
    std::size_t operator()(CommandEntry const& command) const
    {
        std::size_t h = std::hash<std::string>{}(command.command);
        for(auto& output : command.outputs)
        {
            h = h ^ (std::filesystem::hash_value(output) << 1);
        }
        for(auto& input : command.inputs)
        {
            h = h ^ (std::filesystem::hash_value(input) << 1);
        }
        h = h ^ (std::filesystem::hash_value(command.workingDirectory) << 1);
        h = h ^ (std::filesystem::hash_value(command.depFile) << 1);
        return h;
    }
};

