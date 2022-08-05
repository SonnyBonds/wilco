#pragma once

#include <filesystem>
#include <string>

#include "modules/command.h"
#include "util/string.h"

namespace commands
{

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

inline CommandEntry mkdir(std::filesystem::path dir)
{
    CommandEntry commandEntry;

    auto dirStr = dir.make_preferred().string();
    if(OperatingSystem::current() == Windows)
    {
        commandEntry.command = "(if not exist " + dirStr + " mkdir " + dirStr + ")";
    }
    else
    {
        commandEntry.command = "mkdir -p " + dirStr + "";
    }
    commandEntry.description += "Creating directory '" + dir.string() + "'";
    return commandEntry;
}

inline CommandEntry copy(std::filesystem::path from, std::filesystem::path to)
{
    CommandEntry commandEntry;
    commandEntry.inputs = { from };
    commandEntry.outputs = { to };

    auto fromStr = str::quote(from.make_preferred().string());
    auto toStr = str::quote(to.make_preferred().string());
    if(OperatingSystem::current() == Windows)
    {
        commandEntry.command = "copy " + fromStr + " " + toStr + "";
    }
    else
    {
        commandEntry.command = "cp " + fromStr + " " + toStr + "";
    }

    auto toParent = to.parent_path();
    if(!toParent.empty())
    {
        commandEntry = chain({mkdir(toParent), commandEntry});
    }

    commandEntry.description = "Copying '" + from.string() + "' -> '" + to.string() + "'";
    return commandEntry;
}

inline CommandEntry move(std::filesystem::path from, std::filesystem::path to)
{
    CommandEntry commandEntry;
    commandEntry.inputs = { from };
    commandEntry.outputs = { to };

    auto fromStr = str::quote(from.make_preferred().string());
    auto toStr = str::quote(to.make_preferred().string());
    if(OperatingSystem::current() == Windows)
    {
        commandEntry.command = "move " + fromStr + " " + toStr + " && copy /b " + toStr + " +,,";
    }
    else
    {
        commandEntry.command = "mv " + fromStr + " " + toStr + " && touch " + toStr;
    }

    auto toParent = to.parent_path();
    if(!toParent.empty())
    {
        commandEntry = chain({mkdir(toParent), commandEntry});
    }

    commandEntry.description = "Moving '" + from.string() + "' -> '" + to.string() + "'";
    return commandEntry;
}

}
