#pragma once

#include <string>
#include <vector>

#include "core/environment.h"
#include "core/project.h"
#include "core/stringid.h"
#include "util/cli.h"

struct Emitter;

struct Emitters
{
    static void install(Emitter* emitter);
    static const std::vector<Emitter*>& list();
};

struct Emitter
{
    const StringId name;
    const std::string description;
    std::vector<cli::Argument*> arguments;
    std::vector<std::string> generatorCliArguments;

    cli::PathArgument targetPath{arguments, "output-path", "Target path for build files.", "buildfiles"};

    Emitter(StringId name, std::string description);

    Emitter(const Emitter& other) = delete;
    Emitter& operator=(const Emitter& other) = delete;

    virtual void emit(Environment& env) = 0;

protected:

    static std::pair<Project*, std::filesystem::path> createGeneratorProject(Environment& env, std::filesystem::path targetPath);
};
