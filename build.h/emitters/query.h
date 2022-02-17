#pragma once

#include <string>
#include <vector>

#include "core/emitter.h"

class Query : public Emitter
{
public:
    static Query instance;

    enum Type : int
    {
        None,
        Projects,
        Configs
    } type = None;

    Query()
        : Emitter(
            "query"
        )
    {
        argumentDefinitions += cli::selectionArgument({{"configs", Configs}, {"projects", Projects}}, type, "Select information to list.");
    }

    virtual void emit(std::vector<Project*> projects) override
    {
        if(type == None)
        {
            throw cli::argument_error("No valid query type specified.");  
        }

        projects = discoverProjects(projects);

        switch(type)
        {
        case Projects:
            emitProjects(projects);
            return;
        case Configs:
            emitConfigs(projects);
            return;
        case None:
        default:
            throw cli::argument_error("No query type specified.");  
        };
    }

    void emitProjects(const std::vector<Project*>& projects)
    {
        for(auto project : projects)
        {
            if(project->type)
            {
                std::cout << project->name << "\n";
            }
        }
    }

    void emitConfigs(const std::vector<Project*>& projects)
    {
        auto configs = discoverConfigs(projects);
        for(auto config : configs)
        {
            std::cout << std::string(config) << "\n";
        }            
    }
};

Query Query::instance;
