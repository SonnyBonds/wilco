#pragma once

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "core/option.h"
#include "core/stringid.h"
#include "modules/standardoptions.h"
#include "util/operators.h"

enum ProjectType
{
    Executable,
    StaticLib,
    SharedLib,
    Command
};

enum Transitivity
{
    Local,
    Public,
    PublicOnly
};

struct ConfigSelector
{
    ConfigSelector(StringId name)
        : name(name)
    {}

    ConfigSelector(Transitivity transitivity)
        : transitivity(transitivity)
    {}

    ConfigSelector(ProjectType projectType)
        : projectType(projectType)
    {}

    std::optional<Transitivity> transitivity;
    std::optional<StringId> name;
    std::optional<ProjectType> projectType;

    bool operator <(const ConfigSelector& other) const
    {
        if(transitivity != other.transitivity) return transitivity < other.transitivity;
        if(projectType != other.projectType) return projectType < other.projectType;
        if(name != other.name) return name < other.name;

        return false;
    }
};

ConfigSelector operator/(Transitivity a, ConfigSelector b)
{
    if(b.transitivity) throw std::invalid_argument("Transitivity was specified twice.");
    b.transitivity = a;

    return b;
}

ConfigSelector operator/(ProjectType a, ConfigSelector b)
{
    if(b.projectType) throw std::invalid_argument("Project type was specified twice.");
    b.projectType = a;

    return b;
}

ConfigSelector operator/(StringId a, ConfigSelector b)
{
    if(b.name) throw std::invalid_argument("Configuration name was specified twice.");
    b.name = a;

    return b;
}

struct Project;

struct ProjectConfig
{
    OptionCollection options;
    std::vector<Project*> links;

    template<typename T>
    T& operator[](Option<T> option)
    {
        return options[option];
    }

    ProjectConfig& operator +=(const OptionCollection& collection)
    {
        options.combine(collection);
        return *this;
    }
};

struct Project : public ProjectConfig
{
    std::string name;
    std::optional<ProjectType> type;
    std::map<ConfigSelector, ProjectConfig, std::less<>> configs;

    Project(std::string name = {}, std::optional<ProjectType> type = {})
        : name(std::move(name)), type(type)
    {
    }

    ProjectConfig resolve(std::optional<ProjectType> projectType, StringId configName)
    {
        auto config = internalResolve(projectType, configName, true);
        config.options.deduplicate();
        return config;
    }

    ProjectConfig& operator[](ConfigSelector selector)
    {
        return configs[selector];
    }
    
    template<typename T>
    T& operator[](Option<T> option)
    {
        return options[option];
    }

    std::filesystem::path calcOutputPath(ProjectConfig& resolvedConfig)
    {
        auto path = resolvedConfig[OutputPath];
        if(!path.empty())
        {
            return path;
        }

        auto stem = resolvedConfig[OutputStem];
        if(stem.empty())
        {
            stem = name;
        }

        return resolvedConfig[OutputDir] / (resolvedConfig[OutputPrefix] + stem + resolvedConfig[OutputSuffix] + resolvedConfig[OutputStem]);
    }

private:
    ProjectConfig internalResolve(std::optional<ProjectType> projectType, StringId configName, bool local)
    {
        std::vector<ProjectConfig*> resolveConfigs;

        for(auto& entry : configs)
        {
            if(local)
            {
                if(entry.first.transitivity && entry.first.transitivity == PublicOnly) continue;
            }
            else
            {
                if(!entry.first.transitivity || entry.first.transitivity == Local) continue;
            }
            if(entry.first.projectType && entry.first.projectType != projectType) continue;
            if(entry.first.name && entry.first.name != configName) continue;
            resolveConfigs.push_back(&entry.second);
        }

        ProjectConfig result;

        auto resolveLink = [&](Project* link)
        {
            auto resolved = link->internalResolve(projectType, configName, false);
            result.links += resolved.links;
            result.options.combine(resolved.options);
        };

        for(auto& link : links)
        {
            resolveLink(link);
        }

        for(auto config : resolveConfigs)
        {
            for(auto& link : config->links)
            {
                resolveLink(link);
            }
        }

        auto addOptions = [](auto& a, auto& b)
        {
            a.combine(b);
        };

        if(local)
        {
            addOptions(result.options, options);
        }
        for(auto config : resolveConfigs)
        {
            addOptions(result.options, config->options);
        }
        
        return result;
    }
};