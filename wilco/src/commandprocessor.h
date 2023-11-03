#pragma once

#include <filesystem>
#include <future>
#include "modules/command.h"
#include "util/process.h"
#include "database.h"

struct PendingCommand
{
    uint32_t command;
    bool included = false;
    std::future<process::ProcessResult> result;
};

bool updatePathSignature(SignaturePair& signaturePair, const std::filesystem::path& path);
size_t runCommands(std::vector<PendingCommand>& filteredCommands, Database& database, size_t maxConcurrentCommands, bool verbose);
std::vector<PendingCommand> filterCommands(Database& database, std::filesystem::path invocationPath = {}, std::vector<std::string> targets = {});

// TODO: Need to clean up namespaces and code structure in general
namespace commands
{
    bool runCommands(std::vector<CommandEntry> commands, std::filesystem::path databasePath);
}
