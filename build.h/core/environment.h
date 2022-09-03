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

    Project& createProject(std::string name = {}, std::optional<ProjectType> type = {});
    std::vector<Project*> collectProjects();
    std::vector<StringId> collectConfigs();

    std::string readFile(std::filesystem::path path);
    bool writeFile(std::filesystem::path path, const std::string& data);

    std::vector<std::filesystem::path> listFiles(const std::filesystem::path& path, bool recurse = true);

    void addConfigurationDependency(std::filesystem::path path);

private:
    static void collectOrderedProjects(Project* project, std::set<Project*>& collectedProjects, std::vector<Project*>& orderedProjects);
    
    std::vector<std::unique_ptr<Project>> _projects;

public:
    Project& defaults;
    const std::filesystem::path configurationFile;
    const std::filesystem::path startupDir;
    const std::filesystem::path buildHDir;
    cli::Context& cliContext;
    std::set<std::filesystem::path> configurationDependencies;
};