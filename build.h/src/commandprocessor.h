#pragma once

#include <filesystem>
#include <future>
#include "util/process.h"
#include "database.h"

struct PendingCommand
{
    uint32_t command;
    bool included = false;
    std::future<process::ProcessResult> result;
};

Signature computeFileSignature(std::filesystem::path path);
void checkInputSignatures(std::vector<Signature>& commandSignatures, std::vector<PendingCommand>& filteredCommands, std::vector<FileDependencies>::iterator begin, std::vector<FileDependencies>::iterator end);
size_t runCommands(std::vector<PendingCommand>& filteredCommands, Database& database, size_t maxConcurrentCommands, bool verbose);
std::vector<PendingCommand> filterCommands(std::filesystem::path invocationPath, Database& database, const std::filesystem::path& dataPath, std::vector<StringId> targets);
