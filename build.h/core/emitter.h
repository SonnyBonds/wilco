#pragma once

#include <filesystem>
#include <functional>
#include <set>
#include <string>
#include <vector>

#include "core/os.h"
#include "core/project.h"
#include "core/stringid.h"

struct EmitterArgs
{
    std::vector<Project*> projects;
    std::filesystem::path targetPath;
    StringId config;
};

struct Emitter
{
    std::string name;
    std::function<void(const EmitterArgs&)> emit;

    void operator ()(EmitterArgs args) const
    {
        emit(args);
    }

    static std::vector<Project*> discoverProjects(const std::vector<Project*>& projects)
    {
        std::vector<Project*> orderedProjects;
        std::set<Project*> discoveredProjects;
        for(auto project : projects)
        {
            discover(project, discoveredProjects, orderedProjects);
        }

        return orderedProjects;
    }
protected:

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

    static const std::vector<Emitter>& list()
    {
        return getEmitters();
    }

private:
    static std::vector<Emitter>& getEmitters()
    {
        static std::vector<Emitter> emitters;
        return emitters;
    }
};