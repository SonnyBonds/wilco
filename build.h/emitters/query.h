#pragma once

#include <string>
#include <vector>

#include "core/emitter.h"

class Query : public Emitter
{
public:
    static Query instance;

    Query()
        : Emitter(
            "query",
            "configs|projects"
        )
    {
    }

    virtual void emit(const EmitterArgs& args) override
    {
        if(args.cliArgs.empty())
        {
            throw std::runtime_error("Expected query argument.");
        }

        auto projects = Emitter::discoverProjects(args.projects);

        if(args.cliArgs[0] == "projects")
        {
            for(auto project : projects)
            {
                if(project->type)
                {
                    std::cout << project->name << "\n";
                }
            }            
        }
        else if(args.cliArgs[0] == "configs")
        {
            auto configs = discoverConfigs(projects);
            for(auto config : configs)
            {
                std::cout << std::string(config) << "\n";
            }            
        }
        else
        {
            throw std::runtime_error("Unknown query: '" + args.cliArgs[0] + "'");            
        }
    }
};

Query Query::instance;
