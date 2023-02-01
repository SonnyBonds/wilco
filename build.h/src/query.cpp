#include "emitters/query.h"

Query::Query()
    : Emitter("query", "Retrieve information about the build configuration.")
{
    arguments.erase(std::remove(arguments.begin(), arguments.end(), &targetPath), arguments.end());
}

void Query::emit(Environment& env)
{
    if(!listProjects && !listProfiles)
    {
        throw cli::argument_error("No query type specified.");  
    }

    if(listProjects)
    {
        emitProjects(env);
    }
    if(listProfiles)
    {
        emitProfiles(env);
    };
}

void Query::emitProjects(Environment& env)
{
    configure(env);
    
    for(auto& project : env.projects)
    {
        std::cout << project->name << "\n";
    }
}

void Query::emitProfiles(Environment& env)
{
    for(auto& profile : cli::Profile::list())
    {
        std::cout << std::string(profile.name) << "\n";
    }
}

EmitterInstance<Query> Query::instance;
