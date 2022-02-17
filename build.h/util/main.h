#pragma once

#include <filesystem>

#include "core/emitter.h"
#include "util/cli.h"

Emitter* processCommandLine(cli::Context& cliContext, std::vector<cli::ArgumentDefinition> generatorArguments = {})
{
    auto& availableEmitters = Emitters::list();
    if(availableEmitters.empty())
    {
        throw std::runtime_error("No emitters available.");
    }

    cliContext.usage += "Usage: " + cliContext.invocation + " [options to generator] <emitter> [options to emitter] \n";
    cliContext.usage += "\nThese are the available emitters and their options:\n";
    for(auto emitter : availableEmitters)
    {
        emitter->populateCliUsage(cliContext);
    }

    cliContext.usage += "Generator options:\n";
    cliContext.addArgumentDescriptions(generatorArguments);
    cliContext.usage += "\n";

    cliContext.extractArguments(generatorArguments);

    Emitter* selectedEmitter = nullptr;
    {
        auto it = cliContext.unusedArguments.begin();
        while(it != cliContext.unusedArguments.end())
        {
            StringId argumentId = *it;
            for(auto emitter : availableEmitters)
            {
                if(argumentId == emitter->name)
                {
                    selectedEmitter = emitter;
                    cliContext.unusedArguments.erase(it);
                    break;
                }
            }
            if(selectedEmitter)
            {
                break;
            }
            ++it;
        }

        if(selectedEmitter == nullptr)
        {
            throw cli::argument_error("No emitters specified.");
        }
    }
    
    selectedEmitter->initFromCli(cliContext);

    if(!cliContext.unusedArguments.empty())
    {
        throw cli::argument_error("Unknown argument \"" + cliContext.unusedArguments.front() + "\"");
    }
    return selectedEmitter;
}

#ifndef CUSTOM_BUILD_H_MAIN
void generate(cli::Context& cliContext);
int main(int argc, const char** argv)
{
    cli::Context cliContext(
        std::filesystem::proximate(std::filesystem::current_path(), BUILD_DIR), 
        argc > 0 ? argv[0] : "", 
        std::vector<std::string>(argv+std::min(1, argc), argv+argc));

    try
    {
        std::filesystem::current_path(BUILD_DIR);
        generate(cliContext);
    }
    catch(const cli::argument_error& e)
    {
        std::cout << cliContext.usage;
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