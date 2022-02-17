#pragma once

#include <filesystem>
#include <functional>
#include <set>
#include <string>
#include <vector>

#include "core/os.h"
#include "core/project.h"
#include "core/stringid.h"
#include "util/cli.h"


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
    const StringId name;
    std::vector<std::string> generatorCliArguments;

    Emitter(StringId name)
        : name(name)
    {
        Emitters::install(this);
    }

    Emitter(const Emitter& other) = delete;
    Emitter& operator=(const Emitter& other) = delete;

    virtual void emit(std::vector<Project*> projects) = 0;

    virtual void populateCliUsage(cli::Context& context)
    {
        context.addArgumentDescriptions(argumentDefinitions, str::padRightToSize(std::string(name), 20));
        context.usage += "\n";
    }

    virtual void initFromCli(cli::Context& context)
    {
        generatorCliArguments = context.allArguments;
        context.extractArguments(argumentDefinitions);
        if(!targetPath.is_absolute())
        {
            targetPath = context.startPath / targetPath;
        }
    }

protected:
    std::filesystem::path targetPath = "buildfiles";
    cli::ArgumentDefinition targetPathDefinition = cli::stringArgument("--output-path", targetPath, "Target path for build files.");

    std::vector<cli::ArgumentDefinition> argumentDefinitions;

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

    static std::vector<StringId> discoverConfigs(const std::vector<Project*> projects)
    {
        std::set<StringId> configs;

        for(auto project : projects)
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

    static Project createGeneratorProject()
    {
        auto buildOutput = std::filesystem::path(BUILD_FILE).replace_extension("");
        Project project("_generator", Executable);
        project[Features] += { feature::Cpp17, feature::DebugSymbols };
        project[IncludePaths] += BUILD_H_DIR;
        project[OutputPath] = buildOutput;
        project[Defines] += {
            "START_DIR=\\\"" START_DIR "\\\"",
            "BUILD_H_DIR=\\\"" BUILD_H_DIR "\\\"",
            "BUILD_DIR=\\\"" BUILD_DIR "\\\"",
            "BUILD_FILE=\\\"" BUILD_FILE "\\\"",
            "BUILD_ARGS=\\\"" BUILD_ARGS "\\\"",
        };
        project[Files] += BUILD_FILE;
        return project;
    }
};
