#include "actions/configure.h"
#include "buildconfigurator.h"

Configure::ProfileArgument::ProfileArgument(std::vector<cli::Argument*>& argumentList)
{
    example = "--profile=[profile]";
    description = "Apply a predefined set of configuration arguments. Available profiles are ";

    auto& list = cli::Profile::list();
    for(size_t i = 0; i < list.size(); ++i)
    {
        if(i > 0)
        {
            description += ", ";
        }
        if(i > 0 && i+1 == list.size())
        {
            description += "and ";
        }
        description += "\"" + std::string(list[i].name) + "\"";
    }

    argumentList.push_back(this);
}

void Configure::ProfileArgument::extract(std::vector<std::string>& inputValues)
{
    static const size_t len = strlen("--profile=");
    
    auto it = inputValues.begin();
    while(it != inputValues.end())
    {
        if(!str::startsWith(*it, "--profile="))
        {
            ++it;
            continue;
        }

        if(*it == "--profile" || *it == "--profile=")
        {
            throw cli::argument_error("Expected value for option 'profile'.");
        }

        std::string profileName = it->substr(len);
        inputValues.erase(it);

        for(auto& profile : cli::Profile::list())
        {
            if(profile.name == profileName)
            {
                inputValues.insert(inputValues.end(), profile.arguments.begin(), profile.arguments.end());
                return;
            }
        }

        throw cli::argument_error("Profile '" + profileName + "' not found.");
    }
}

void Configure::ProfileArgument::reset()
{
}

Configure::Configure()
    : Action("configure", "Configure build")
{ }

void Configure::run(cli::Context context)
{
    BuildConfigurator configurator(context, false);
}

ActionInstance<Configure> Configure::instance;
