#pragma once

#include "database.h"
#include "core/environment.h"
#include "core/stringid.h"

#include <optional>
#include <filesystem>

class Project;

class BuildConfigurator
{
public:
    BuildConfigurator(Environment& env, bool verboser = false);
    ~BuildConfigurator();

    static void collectCommands(Environment& env, std::vector<CommandEntry>& collectedCommands, const std::filesystem::path& projectDir, Project& project);

    Database database;
    std::filesystem::path dataPath;
private:
    std::filesystem::path _databasePath;
    ConfigDependencyChecker _dependencyChecker;
};