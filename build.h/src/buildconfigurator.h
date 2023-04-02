#pragma once

#include "database.h"
#include "core/environment.h"
#include "core/stringid.h"

#include <optional>
#include <filesystem>

struct Project;

class BuildConfigurator
{
public:
    BuildConfigurator(cli::Context cliContext, bool updateExisting = true);
    ~BuildConfigurator();

    static void collectCommands(Environment& env, std::vector<CommandEntry>& collectedCommands, const std::filesystem::path& projectDir, Project& project);
    static bool checkDependencies(cli::Context& cliContext, std::filesystem::path cachePath);
    static void writeDependencies(std::filesystem::path cachePath);
    static Environment configureEnvironment(cli::Context& cliContext);

    cli::Context cliContext;
    Database database;
    std::filesystem::path dataPath;
private:
    std::filesystem::path _databasePath;
    bool _updateDependencies;
};
