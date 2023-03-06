#pragma once

#include <string>
#include <vector>

#include "core/environment.h"
#include "core/project.h"
#include "core/stringid.h"
#include "util/cli.h"

extern cli::PathArgument targetPath;

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
    const StringId name;
    const std::string description;
    std::vector<cli::Argument*> arguments;
    std::vector<std::string> generatorCliArguments;

    Action(StringId name, std::string description);

    Action(const Action& other) = delete;
    Action& operator=(const Action& other) = delete;

    virtual void run(Environment& env) = 0;

protected:
};
