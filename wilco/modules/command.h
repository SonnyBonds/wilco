#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "core/property.h"

struct DepFile
{
    std::filesystem::path path;
    enum Format
    {
        GCC,
        MSVC
    } format = GCC;

    DepFile& operator =(std::filesystem::path path)
    {
        this->path = std::move(path);
        return *this;
    }

    operator const std::filesystem::path&() const
    {
        return path;
    }

    operator bool() const
    {
        return !path.empty();
    }
};

struct CommandEntry
{
    std::string command;
    std::vector<std::filesystem::path> inputs;
    std::vector<std::filesystem::path> outputs;
    std::filesystem::path workingDirectory;
    DepFile depFile;
    std::string description;
    std::filesystem::path rspFile;
    std::string rspContents;

    bool operator ==(const CommandEntry& other) const
    {
        return command == other.command &&
               outputs == other.outputs &&
               inputs == other.inputs &&
               workingDirectory == other.workingDirectory &&
               depFile.path == other.depFile.path;
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

