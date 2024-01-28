#pragma once

#include <filesystem>
#include <string>

#include "modules/command.h"
#include "util/string.h"

namespace commands
{

// TODO: Do flags better

namespace flags
{

namespace copy
{

enum Flags
{
    removeDestination = 1,
    touchDestination = 2,
};

}

namespace move
{

enum Flags
{
    removeDestination = 1,
    touchDestination = 2,
};

}

}

inline CommandEntry chain(const std::vector<CommandEntry>& commands, std::string newDescription = {})
{
    if(commands.empty())
    {
        throw std::invalid_argument("No commands to chain");
    }

    CommandEntry result = commands[0];
    for(size_t i = 1; i < commands.size(); ++i)
    {
        auto& command = commands[i];
        if(result.workingDirectory != command.workingDirectory)
        {
            throw std::invalid_argument("Can't chain commands with different working directories.");
        }        

        result.command += " && " + command.command;
        result.inputs.insert(result.inputs.end(), command.inputs.begin(), command.inputs.end());
        result.outputs.insert(result.outputs.end(), command.outputs.begin(), command.outputs.end());

        if(newDescription.empty())
        {
            result.description = result.description + ", " + command.description;
        }
    }

    // Remove intermediate steps from inputs
    result.inputs.erase(std::remove_if(result.inputs.begin(), result.inputs.end(), [&](const auto& input) {
        return std::find(result.outputs.begin(), result.outputs.end(), input) != result.outputs.end();
    }), result.inputs.end());

    if(!newDescription.empty())
    {
        result.description = newDescription;
    }

    return result;
}

inline CommandEntry ifExists(std::filesystem::path path, CommandEntry commandEntry)
{
    if(OperatingSystem::current() == Windows)
    {
        commandEntry.command = "(if exist " + str::quote(path.make_preferred().string(), '"', "\"") + " " + commandEntry.command + ")";
    }
    else
    {
        commandEntry.command = "if test -e " + str::quote(path.make_preferred().string()) + "; then " + commandEntry.command + "; fi";
    }
    return commandEntry;
}

inline CommandEntry ifNotExists(std::filesystem::path path, CommandEntry commandEntry)
{
    if(OperatingSystem::current() == Windows)
    {
        commandEntry.command = "(if not exist " + str::quote(path.make_preferred().string(), '"', "\"") + " " + commandEntry.command + ")";
    }
    else
    {
        commandEntry.command = "if ! test -e " + str::quote(path.make_preferred().string()) + "; then " + commandEntry.command + "; fi";
    }
    return commandEntry;
}

inline CommandEntry remove(std::filesystem::path path)
{
    CommandEntry commandEntry;
    commandEntry.inputs = { path };

    if(OperatingSystem::current() == Windows)
    {
        commandEntry.command = "del /Q " + str::quote(path.make_preferred().string(), '"', "\"");
    }
    else
    {
        commandEntry.command = "rm -rf " + str::quote(path.make_preferred().string()) + "";
    }
    
    commandEntry.description += "Removing '" + path.string() + "'";
    return commandEntry;
}

inline CommandEntry mkdir(std::filesystem::path dir)
{
    CommandEntry commandEntry;

    if(OperatingSystem::current() == Windows)
    {
        commandEntry.command = "mkdir " + str::quote(dir.make_preferred().string(), '"', "\"");
        commandEntry = ifNotExists(dir, commandEntry);
    }
    else
    {
        auto dirStr = str::quote(dir.make_preferred().string());
        commandEntry.command = "mkdir -p " + dirStr + "";
    }
    commandEntry.description += "Creating directory '" + dir.string() + "'";
    return commandEntry;
}

inline CommandEntry touch(std::filesystem::path path)
{
    CommandEntry commandEntry;
    commandEntry.outputs = { path };

    if(OperatingSystem::current() == Windows)
    {
        auto toStr = str::quote(path.make_preferred().string(), '"', "\"");
        commandEntry.command += "type NUL >> " + toStr + " && copy /Y /b " + toStr + " +,, " + toStr;
    }
    else
    {
        commandEntry.command += "touch " + str::quote(path.make_preferred().string());
    }

    commandEntry.description = "Touching '" + path.string();
    return commandEntry;
}

inline CommandEntry copy(std::filesystem::path from, std::filesystem::path to, flags::copy::Flags flags = {})
{
    CommandEntry commandEntry;
    commandEntry.inputs = { from };
    commandEntry.outputs = { to };

    if(OperatingSystem::current() == Windows)
    {
        auto fromStr = str::quote(from.make_preferred().string(), '"', "\"");
        auto toStr = str::quote(to.make_preferred().string(), '"', "\"");
        commandEntry.command = "copy " + fromStr + " " + toStr + "";
    }
    else
    {
        auto fromStr = str::quote(from.make_preferred().string());
        auto toStr = str::quote(to.make_preferred().string());
        commandEntry.command = "cp -R " + fromStr + " " + toStr + "";
    }

    if(flags & flags::copy::touchDestination)
    {
        commandEntry = chain({commandEntry, touch(to)});
    }

    auto toParent = to.parent_path();
    if(!toParent.empty())
    {
        commandEntry = chain({mkdir(toParent), commandEntry});
    }

    if(flags & flags::copy::removeDestination)
    {
        commandEntry = chain({ifExists(to, ::commands::remove(to)), commandEntry});
    }

    commandEntry.description = "Copying '" + from.string() + "' -> '" + to.string() + "'";
    return commandEntry;
}

inline CommandEntry copyRelative(std::filesystem::path from, std::filesystem::path relativeBase, std::filesystem::path targetBase, std::optional<std::string> filenameOverride = {})
{
    auto relativeFrom = std::filesystem::relative(from, relativeBase);
    if(relativeFrom.empty() || !relativeFrom.is_relative() || (*relativeFrom.begin() == ".."))
    {
        throw std::runtime_error("Failed to find a proper relative subpath from '" + relativeBase.string() + "' to '" + from.string() + "'.");
    }

    auto relativeTo = relativeFrom;
    if(filenameOverride)
    {
        relativeTo.replace_filename(*filenameOverride);
    }

    return commands::copy(from, targetBase / relativeTo);
}

inline CommandEntry move(std::filesystem::path from, std::filesystem::path to, flags::move::Flags flags = {})
{
    CommandEntry commandEntry;
    commandEntry.inputs = { from };
    commandEntry.outputs = { to };

    if(OperatingSystem::current() == Windows)
    {
        auto fromStr = str::quote(from.make_preferred().string(), '"', "\"");
        auto toStr = str::quote(to.make_preferred().string(), '"', "\"");
        commandEntry.command = "move /Y " + fromStr + " " + toStr;
    }
    else
    {
        auto fromStr = str::quote(from.make_preferred().string());
        auto toStr = str::quote(to.make_preferred().string());
        commandEntry.command = "mv " + fromStr + " " + toStr;
    }

    if(flags & flags::move::touchDestination)
    {
        commandEntry = chain({commandEntry, touch(to)});
    }

    auto toParent = to.parent_path();
    if(!toParent.empty())
    {
        commandEntry = chain({mkdir(toParent), commandEntry});
    }

    if(flags & flags::move::removeDestination)
    {
        commandEntry = chain({ifExists(to, ::commands::remove(to)), commandEntry});
    }

    commandEntry.description = "Moving '" + from.string() + "' -> '" + to.string() + "'";
    return commandEntry;
}

bool runCommands(std::vector<CommandEntry> commands, std::filesystem::path databasePath);

}
