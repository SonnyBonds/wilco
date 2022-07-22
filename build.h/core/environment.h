#pragma once

#include <memory>

#include "core/project.h"
#include "util/cli.h"
#include "util/process.h"

struct Environment
{
    Environment(cli::Context& cliContext)
        : defaults(createProject())
        , configurationFile(process::findCurrentModulePath().replace_extension(".cpp")) // Wish this was a bit more robust but __BASE_FILE__ isn't available everywhere...
        , startupDir(std::filesystem::current_path())
        , buildHDir(std::filesystem::absolute(__FILE__).parent_path().parent_path())
        , cliContext(cliContext)
    {
        defaults(Public).output.dir = "bin";
        defaults(Public, Linux, Executable).output.extension = "";
        defaults(Public, Linux, StaticLib).output.extension = ".a";
        defaults(Public, Linux, SharedLib).output.extension = ".so";
        defaults(Public, MacOS, Executable).output.extension = "";
        defaults(Public, MacOS, StaticLib).output.extension = ".a";
        defaults(Public, MacOS, SharedLib).output.extension = ".so";
        defaults(Public, Windows, Executable).output.extension = ".exe";
        defaults(Public, Windows, StaticLib).output.extension = ".lib";
        defaults(Public, Windows, SharedLib).output.extension = ".dll";
    }

    ~Environment()
    {
    }

    Project& createProject(std::string name = {}, std::optional<ProjectType> type = {})
    {
        _projects.emplace_back(new Project(name, type));
        if(_projects.size() > 1)
        {
            _projects.back()->links += &defaults;
        }
        return *_projects.back();
    }

    std::vector<Project*> collectProjects()
    {
        // TODO: Probably possible to do this more efficiently

        std::vector<Project*> orderedProjects;
        std::set<Project*> collectedProjects;

        for(auto& project : _projects)
        {
            collectOrderedProjects(project.get(), collectedProjects, orderedProjects);
        }

        return orderedProjects;
    }

    std::vector<StringId> collectConfigs()
    {
        std::set<StringId> configs;

        for(auto& project : _projects)
        {
            for(auto& config : project->configs)
            {
                if(config.first.name.has_value())
                {
                    configs.insert(*config.first.name);
                }
            }
        }

        if(configs.empty())
        {
            return {StringId()};
        }

        std::vector<StringId> result;
        result.reserve(configs.size());
        for(auto& config : configs)
        {
            result.push_back(config);
        }

        return result;
    }

private:
    static void collectOrderedProjects(Project* project, std::set<Project*>& collectedProjects, std::vector<Project*>& orderedProjects)
    {
        for(auto link : project->links)
        {
            collectOrderedProjects(link, collectedProjects, orderedProjects);
        }

        if(collectedProjects.insert(project).second)
        {
            orderedProjects.push_back(project);
        }
    }

    std::vector<std::unique_ptr<Project>> _projects;

public:
    Project& defaults;
    const std::filesystem::path configurationFile;
    const std::filesystem::path startupDir;
    const std::filesystem::path buildHDir;
    cli::Context& cliContext;
};