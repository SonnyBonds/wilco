#include "core/environment.h"
#include "core/os.h"
#include "actions/direct.h"
#include "util/commands.h"
#include "util/cli.h"
#include "util/interrupt.h"
#include "dependencyparser.h"
#include "fileutil.h"

#include <sstream>
#include <chrono>
#include <string>

void printUsage(cli::Context& cliContext)
{
    std::cout << "Usage: " + cliContext.invocation + " [action] [options]\n\n";

    auto argumentSorter = [](const cli::Argument* a, const cli::Argument* b)
    {
        return a->example < b->example;
    };

    std::cout << "\nAvailable actions:\n";
    for(auto action : Actions::list())
    {
        std::cout << "\n" << action->name << ": " << action->description << "\n";
        auto arguments = action->arguments;
        sort(arguments.begin(), arguments.end(), argumentSorter);
        for(auto argument : arguments)
        {
            std::cout << "  " << str::padRightToSize(argument->example, 30) + "  " + argument->description + "\n";
        }
    }
    std::cout << "\n";

    bool first = true;
    auto arguments = cli::Argument::globalList();
    sort(arguments.begin(), arguments.end(), argumentSorter);
    for(auto argument : arguments)
    {
        if(first)
        {
            std::cout << "Configuration options:\n";
            first = false;
        }
        std::cout << "  " << str::padRightToSize(argument->example, 30) + "  " + argument->description + "\n";
    }

    if(!first)
    {
        std::cout << "\n";
    }
}

int defaultMain(int argc, const char** argv) {
    interrupt::installHandlers();
    
    cli::Context cliContext(
        std::filesystem::current_path(),
        argc > 0 ? argv[0] : "", 
        std::vector<std::string>(argv+std::min(1, argc), argv+argc));
    try
    {
        cliContext.extractArguments(cli::Argument::globalList());
        if(!noRebuild)
        {
            DirectBuilder::buildSelf(cliContext);
        }

        std::filesystem::current_path(cliContext.configurationFile.parent_path());

        if(cliContext.action.empty())
        {
            throw cli::argument_error("No action specified.");
        }

        auto& availableActions = Actions::list();        

        Action* chosenAction = nullptr;
        for(auto action : availableActions)
        {
            if(action->name == cliContext.action)
            {
                chosenAction = action;
                break;
            }
        }

        if(!chosenAction)
        {
            throw cli::argument_error("Unknown action \"" + std::string(cliContext.action) + "\"");
        }

        chosenAction->run(cliContext);
    }
    catch(const cli::argument_error& e)
    {
        printUsage(cliContext);
        std::cerr << "ERROR: " << e.what() << '\n';
        return -1;
    }
    catch(const std::exception& e)
    {
        std::cerr << "ERROR: " << e.what() << '\n';
        return -1;
    }

    return 0;
}
