#include "actions/clean.h"
#include "fileutil.h"
#include "util/commands.h"
#include "database.h"
#include "buildconfigurator.h"
#include <iostream>
#include <chrono>
#include "util/hash.h"
#include "util/interrupt.h"
#include "commandprocessor.h"
#include "toolchains/cl.h"
#include <filesystem>

Clean::TargetArgument::TargetArgument(std::vector<cli::Argument*>& argumentList)
{
    this->example = "[targets]";
    this->description = "Clean specific targets. [default:all]";

    argumentList.push_back(this);
}

void Clean::TargetArgument::extract(std::vector<std::string>& inputValues)
{
    rawValue.clear();

    auto it = inputValues.begin();
    while(it != inputValues.end())
    {
        if(it->size() > 2 &&
           it->substr(0, 2) == "--")
        {
            ++it;
            continue;
        }
        values.push_back(std::move(*it));
        rawValue += (rawValue.empty() ? "" : " ") + *it;
        inputValues.erase(it);
    }
}

void Clean::TargetArgument::reset()
{
    values.clear();
}

Clean::Clean()
    : Action("clean", "Clean build outputs.")
{ }

void Clean::run(cli::Context cliContext)
{
    cliContext.extractArguments(arguments);
    
    BuildConfigurator configurator(cliContext);

    if(!targets.values.empty())
    {        
        throw std::runtime_error("Cleaning specific targets is currently not implemented.");
    }

    std::cout << "Cleaning..." << std::endl;
    for(auto& command : configurator.database.getCommands())
    {
        for(auto& output : command.outputs)
        {
            std::filesystem::remove(output);
        }
    }
    std::cout << "Done." << std::endl;
}

ActionInstance<Clean> Clean::instance;
