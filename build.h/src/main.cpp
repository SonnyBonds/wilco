#include "core/environment.h"
#include "core/os.h"
#include "core/stringid.h"
#include "emitters/direct.h"
#include "util/commands.h"
#include "util/cli.h"
#include "dependencyparser.h"
#include "fileutil.h"

#include <sstream>
#include <chrono>

void printUsage(cli::Context& cliContext)
{
    std::cout << "Usage: " + cliContext.invocation + " [action] [options]\n\n";

    std::cout << "\nAvailable actions:\n";
    for(auto emitter : Emitters::list())
    {
        std::cout << "\n" << emitter->name << ": " << emitter->description << "\n";
        for(auto argument : emitter->arguments)
        {
            std::cout << "  " << str::padRightToSize(argument->example, 30) + "  " + argument->description + "\n";
        }
    }
    std::cout << "\n";

    bool first = true;
    for(auto argument : cli::Argument::globalList())
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
    auto startTime = std::chrono::high_resolution_clock::now();

    cli::Context cliContext(
        std::filesystem::current_path(),
        argc > 0 ? argv[0] : "", 
        std::vector<std::string>(argv+std::min(1, argc), argv+argc));
    try
    {
        Environment env(cliContext);

        DirectBuilder::buildSelf(cliContext, env);

        std::filesystem::current_path(env.configurationFile.parent_path());

        if(cliContext.action.empty())
        {
            throw cli::argument_error("No action specified.");
        }

        auto& availableEmitters = Emitters::list();        

        Emitter* chosenEmitter = nullptr;
        for(auto emitter : availableEmitters)
        {
            if(emitter->name == cliContext.action)
            {
                chosenEmitter = emitter;
                break;
            }
        }

        if(!chosenEmitter)
        {
            throw cli::argument_error("Unknown action \"" + std::string(cliContext.action) + "\"");
        }

        // We want path arguments to be relative the start path, but
        // it's a bit messy to switch this back and forth. The various
        // relevant paths should probably be made available so things can use
        // them explicitly
        std::filesystem::current_path(cliContext.startPath);
        cliContext.extractArguments(chosenEmitter->arguments);
        cliContext.extractArguments(cli::Argument::globalList());

        for(auto& argument : cliContext.unusedArguments)
        {
            throw cli::argument_error("Argument \"" + argument + "\" was not recognized.");
        }

        std::filesystem::current_path(env.configurationFile.parent_path());

        chosenEmitter->emit(env);
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

    // TODO: Add an option to output build time?
#if 0
    auto endTime = std::chrono::high_resolution_clock::now();
    auto msDuration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    if(msDuration > 1000)
    {
        std::cout << "--- " << (msDuration*0.001f) << "s ---" << std::endl;
    }
    else
    {
        std::cout << "--- " << msDuration << "ms ---" << std::endl;
    }
#endif

    return 0;
}
