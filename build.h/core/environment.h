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
public:
    const std::filesystem::path configurationFile;
    const std::filesystem::path startupDir;
    const std::filesystem::path buildHDir;
    cli::Context& cliContext;
    std::set<std::filesystem::path> configurationDependencies;
    std::set<StringId> configurations;
};

struct Configuration
{
    Configuration(StringId name);
    
    const StringId name;
    Project& createProject(std::string name, ProjectType type);
    const std::vector<std::unique_ptr<Project>>& getProjects() const;

private:
    std::vector<std::unique_ptr<Project>> _projects;
};

void setup(Environment& env);
void configure(Environment& env, Configuration& config);
