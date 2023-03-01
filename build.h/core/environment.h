#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "core/project.h"
#include "util/cli.h"

struct Environment
{
    Environment(cli::Context& cliContext); 
    ~Environment();

    std::string readFile(std::filesystem::path path);
    bool writeFile(std::filesystem::path path, const std::string& data);

    std::vector<std::filesystem::path> listFiles(const std::filesystem::path& path, bool recurse = true);

    void addConfigurationDependency(std::filesystem::path path);

    Project& createProject(std::string name, ProjectType type);

    const std::filesystem::path configurationFile;
    const std::filesystem::path startupDir;
    const std::filesystem::path buildHDir;
    cli::Context& cliContext;
    std::set<std::filesystem::path> configurationDependencies;
    std::vector<std::unique_ptr<Project>> projects;
};

class ConfigDependencyChecker
{
public:
    ConfigDependencyChecker(Environment& env, std::filesystem::path path);
    ~ConfigDependencyChecker();

    bool isDirty();

private:
    bool _dirty;
    Environment& _env;
    std::filesystem::path _path;
};

void configure(Environment& env);
