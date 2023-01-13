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
    std::set<StringId> projectNames;
    for(auto& configName : env.configurations)
    {
        Configuration config{configName};
        configure(env, config);
        
        for(auto& project : config.getProjects())
        {
            projectNames.insert(project->name);
        }
    }
    for(auto& name : projectNames)
    {
        std::cout << name << "\n";
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
