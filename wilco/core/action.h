#pragma once

#include <string>
#include <vector>

#include "core/environment.h"
#include "core/project.h"
#include "util/cli.h"

extern cli::PathArgument wilcoFilesPath;
extern cli::PathArgument targetPath;
extern cli::BoolArgument noRebuild;

struct Action;

struct Actions
{
    static void install(Action* action);
    static const std::vector<Action*>& list();
};

template<typename T>
struct ActionInstance
{
    ActionInstance()
    {
        static T instance;
        Actions::install(&instance);
    }
};

struct Action
{
    const std::string name;
    const std::string description;
    std::vector<cli::Argument*> arguments;
    std::vector<std::string> generatorCliArguments;

    Action(std::string name, std::string description);

    Action(const Action& other) = delete;
    Action& operator=(const Action& other) = delete;

    virtual void run(cli::Context cliContext) = 0;

protected:
};
