#pragma once

#include "modules/command.h"

class Database
{
public:
    Database();

    bool load(std::filesystem::path path);
    void save(std::filesystem::path path);

    void setCommands(std::vector<CommandEntry> commands);

    // TODO: Better data structure for this
    const std::vector<std::vector<uint32_t>>& getDependencies() const;
    const std::vector<CommandEntry>& getCommands() const;

private:
    std::vector<CommandEntry> _commands;
    std::vector<std::vector<uint32_t>> _dependencies;
    std::string _commandData;
};