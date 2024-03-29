#pragma once

#include "database.h"
#include "core/environment.h"

#include <string>
#include <optional>
#include <filesystem>

struct Project;

class BuildConfigurator
{
public:
    BuildConfigurator(cli::Context cliContext, bool useExisting = true);
    ~BuildConfigurator();

    static void collectCommands(Environment& env, std::vector<CommandEntry>& collectedCommands, const std::filesystem::path& projectDir, Project& project);
    static std::optional<std::vector<std::string>> getPreviousConfigDatabaseArguments(const Database& database);
    static void updateConfigDatabase(std::set<std::filesystem::path> configDependencies, Database& database, const std::vector<std::string>& args);
    static Environment configureEnvironment(cli::Context& cliContext);

    cli::Context cliContext;
    Database database;
    Database configDatabase;
    std::filesystem::path dataPath;
private:
    std::filesystem::path _databasePath;
    std::filesystem::path _configDatabasePath;
};
