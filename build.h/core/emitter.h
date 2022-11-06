#pragma once

#include <string>
#include <vector>

#include "core/environment.h"
#include "core/project.h"
#include "core/stringid.h"
#include "util/cli.h"

extern cli::PathArgument targetPath;

struct Emitter;

struct Emitters
{
    static void install(Emitter* emitter);
    static const std::vector<Emitter*>& list();
};

template<typename T>
struct EmitterInstance
{
    EmitterInstance()
    {
        static T instance;
        Emitters::install(&instance);
    }
};

struct Emitter
{
    const StringId name;
    const std::string description;
    std::vector<cli::Argument*> arguments;
    std::vector<std::string> generatorCliArguments;

    Emitter(StringId name, std::string description);

    Emitter(const Emitter& other) = delete;
    Emitter& operator=(const Emitter& other) = delete;

    virtual void emit(Environment& env) = 0;

protected:
};
