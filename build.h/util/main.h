#pragma once

#include <filesystem>

#include "core/emitter.h"
#include "core/configurator.h"
#include "util/cli.h"


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
    for(auto configurator : Configurators::list())
    {
        for(auto argument : configurator->arguments)
        {
            if(first)
            {
                std::cout << "Configuration options:\n";
                first = false;
            }
            std::cout << "  " << str::padRightToSize(argument->example, 30) + "  " + argument->description + "\n";
        }
    }
    if(!first)
    {
        std::cout << "\n";
    }
}

#ifndef CUSTOM_BUILD_H_MAIN

int main(int argc, const char** argv)
{
    cli::Context cliContext(
        std::filesystem::proximate(std::filesystem::current_path(), BUILD_DIR), 
        argc > 0 ? argv[0] : "", 
        std::vector<std::string>(argv+std::min(1, argc), argv+argc));

    try
    {
        auto& availableEmitters = Emitters::list();        

        if(cliContext.action.empty())
        {
            throw cli::argument_error("No action specified.");
        }

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

        for(auto configurator : Configurators::list())
        {
            for(auto argument : configurator->arguments)
            {
                cliContext.extractArgument(argument);
            }
        }

        for(auto argument : chosenEmitter->arguments)
        {
            cliContext.extractArgument(argument);
        }

        std::filesystem::current_path(BUILD_DIR);

        Environment env;
        for(auto configurator : Configurators::list())
        {
            configurator->configure(env);
        }

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
}
#endif