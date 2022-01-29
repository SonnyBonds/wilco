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

struct Emitter;

struct Emitters
{
    static void install(Emitter* emitter)
    {
        getEmitters().push_back(emitter);
    }

    static const std::vector<Emitter*>& list()
    {
        return getEmitters();
    }

private:
    static std::vector<Emitter*>& getEmitters()
    {
        static std::vector<Emitter*> emitters;
        return emitters;
    }
};

struct Emitter
{
    StringId name;
    Emitter(std::string name)
        : name(std::move(name)) 
    {
        Emitters::install(this);
    }

    virtual void emit(const EmitterArgs& args) = 0;

protected:
    static std::vector<Project*> discoverProjects(const std::vector<Project*>& projects)
    {
        std::vector<Project*> orderedProjects;
        std::set<Project*> discoveredProjects;

        discover(&Project::defaults, discoveredProjects, orderedProjects);
        for(auto project : projects)
        {
            discover(project, discoveredProjects, orderedProjects);
        }

        return orderedProjects;
    }

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
