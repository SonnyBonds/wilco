#pragma once

#include <filesystem>
#include <functional>
#include <set>
#include <string>
#include <vector>

#include <core/project.h>
#include <core/stringid.h>

struct EmitterArgs
{
    std::filesystem::path targetPath;
    std::vector<Project*> projects;
    StringId config;
};

struct Emitter
{
    std::string name;
    std::function<void(const EmitterArgs&)> emit;

    void operator ()(EmitterArgs args) const
    {
        std::vector<Project*> orderedProjects;
        std::set<Project*> discoveredProjects;
        for(auto project : args.projects)
        {
            discover(project, discoveredProjects, orderedProjects);
        }

        args.projects = orderedProjects;

        emit(args);
    }

private:
    static void discover(Project* project, std::set<Project*>& discoveredProjects, std::vector<Project*>& orderedProjects)
    {
        for(auto& link : project->links)
        {
            discover(link, discoveredProjects, orderedProjects);
        }

        if(discoveredProjects.insert(project).second)
        {
            orderedProjects.push_back(project);
        }
    }
};

struct Emitters
{
    using Token = int;

    static Token install(Emitter emitter)
    {
        getEmitters().push_back(emitter);
        return {};
    }

    static const std::vector<const Emitter>& list()
    {
        return getEmitters();
    }

private:
    static std::vector<const Emitter>& getEmitters()
    {
        static std::vector<const Emitter> emitters;
        return emitters;
    }
};