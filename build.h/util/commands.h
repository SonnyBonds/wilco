#pragma once

#include <filesystem>
#include <string>

#include "modules/command.h"

namespace commands
{

CommandEntry copy(std::filesystem::path from, std::filesystem::path to)
{
    CommandEntry commandEntry;
    commandEntry.inputs = { from };
    commandEntry.outputs = { to };
    commandEntry.command = "mkdir -p \"" + from.parent_path().string() + "\" && cp \"" + from.string() + "\" \"" + to.string() + "\"";
    commandEntry.description += "Copying '" + from.string() + "' -> '" + to.string() + "'";
    return commandEntry;
}

CommandEntry mkdir(std::filesystem::path dir)
{
    CommandEntry commandEntry;
    commandEntry.outputs = { dir };
    commandEntry.command = "mkdir -p \"" + dir.string() + "\"";
    commandEntry.description += "Creating directory '" + dir.string() + "'";
    return commandEntry;
}

}
