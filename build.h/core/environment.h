#pragma once

#include <memory>

#include "core/project.h"

struct Environment
{
    Environment()
        : defaults(createProject())
    {
        defaults[Public][OutputDir] = "bin";
        defaults[Public / Linux / Executable][OutputExtension] = "";
        defaults[Public / Linux / StaticLib][OutputExtension] = ".a";
        defaults[Public / Linux / SharedLib][OutputExtension] = ".so";
        defaults[Public / MacOS / Executable][OutputExtension] = "";
        defaults[Public / MacOS / StaticLib][OutputExtension] = ".a";
        defaults[Public / MacOS / SharedLib][OutputExtension] = ".so";
        defaults[Public / Windows / Executable][OutputExtension] = ".exe";
        defaults[Public / Windows / StaticLib][OutputExtension] = ".lib";
        defaults[Public / Windows / SharedLib][OutputExtension] = ".dll";
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
};