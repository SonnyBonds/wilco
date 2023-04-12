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

bool updatePathSignature(SignaturePair& signaturePair, const std::filesystem::path& path);
size_t runCommands(std::vector<PendingCommand>& filteredCommands, Database& database, size_t maxConcurrentCommands, bool verbose);
std::vector<PendingCommand> filterCommands(std::filesystem::path invocationPath, Database& database, const std::filesystem::path& dataPath, std::vector<StringId> targets);
