#include "actions/query.h"
#include "buildconfigurator.h"

static void emitProjects(cli::Context& cliContext)
{
    BuildConfigurator configurator(cliContext);
    
    throw std::runtime_error("Project listing is currently broken.");
#if TODO
    for(auto& project : configurator.environment.projects)
    {
        std::cout << project->name << "\n";
    }
#endif
}

static void emitProfiles(cli::Context& cliContext)
{
    for(auto& profile : cli::Profile::list())
    {
        std::cout << std::string(profile.name) << "\n";
    }
}

Query::Query()
    : Action("query", "Retrieve information about the build configuration.")
{
    arguments.erase(std::remove(arguments.begin(), arguments.end(), &targetPath), arguments.end());
}

void Query::run(cli::Context cliContext)
{
    cliContext.extractArguments(arguments);
    cliContext.requireAllArgumentsUsed();

    if(!listProjects && !listProfiles)
    {
        throw cli::argument_error("No query type specified.");  
    }

    if(listProjects)
    {
        emitProjects(cliContext);
    }
    if(listProfiles)
    {
        emitProfiles(cliContext);
    };
}

ActionInstance<Query> Query::instance;
