#include "emitters/query.h"

Query::Query()
    : Emitter("query", "Retrieve information about the build configuration.")
{
    arguments.erase(std::remove(arguments.begin(), arguments.end(), &targetPath), arguments.end());
}

void Query::emit(Environment& env)
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

void Query::emitProjects(Environment& env)
{
    for(auto project : env.collectProjects())
    {
        if(project->type)
        {
            std::cout << project->name << "\n";
        }
    }
}

void Query::emitConfigs(Environment& env)
{
    for(auto& config : env.configurations)
    {
        std::cout << std::string(config) << "\n";
    }            
}

EmitterInstance<Query> Query::instance;
