#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "core/project.h"
#include "util/cli.h"

struct Environment
{
    Environment(cli::Context& cliContext); 
    Environment(Environment&& other) = default; 
    ~Environment();

    std::string readFile(std::filesystem::path path);
    bool writeFile(std::filesystem::path path, const std::string& data);

    std::vector<std::filesystem::path> listFiles(const std::filesystem::path& path, bool recurse = true);

    static void addConfigurationDependency(std::filesystem::path path);

    Project& createProject(std::string name, ProjectType type);

    cli::Context& cliContext;
    static std::set<std::filesystem::path> configurationDependencies;
    std::vector<std::unique_ptr<Project>> projects;
};

void configure(Environment& env);
