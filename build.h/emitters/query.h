#pragma once

#include <string>
#include <vector>

#include "core/emitter.h"

class Query : public Emitter
{
public:
    static Query instance;

    cli::BoolArgument listProjects{arguments, "projects", "List all defined projects."};
    cli::BoolArgument listConfigs{arguments, "configs", "List all defined configurations."};

    Query()
        : Emitter("query", "Retrieve information about the build configuration.")
    {
        arguments.erase(std::remove(arguments.begin(), arguments.end(), &targetPath), arguments.end());
    }

    virtual void emit(Environment& env) override
    {
        if(!listProjects && !listConfigs)
        {
            throw cli::argument_error("No query type specified.");  
        }

        if(listProjects)
        {
            emitProjects(env);
        }
        if(listConfigs)
        {
            emitConfigs(env);
        };
    }

    void emitProjects(Environment& env)
    {
        for(auto project : env.collectProjects())
        {
            if(project->type)
            {
                std::cout << project->name << "\n";
            }
        }
    }

    void emitConfigs(Environment& env)
    {
        for(auto& config : env.collectConfigs())
        {
            std::cout << std::string(config) << "\n";
        }            
    }
};

Query Query::instance;
