#pragma once

#include <filesystem>
#include <functional>
#include <set>
#include <string>
#include <vector>

#include "core/os.h"
#include "core/project.h"
#include "core/stringid.h"
#include "util/cli.h"
#include "util/commands.h"


struct Emitter;

struct Emitters
{
    static void install(Emitter* emitter)
    {
        getEmitters().push_back(emitter);
    }

    static const std::vector<Emitter*>& list()
    {
        return getEmitters();
    }

private:
    static std::vector<Emitter*>& getEmitters()
    {
        static std::vector<Emitter*> emitters;
        return emitters;
    }
};

struct Emitter
{
    const StringId name;
    const std::string description;
    std::vector<cli::Argument*> arguments;
    std::vector<std::string> generatorCliArguments;

    cli::PathArgument targetPath{arguments, "output-path", "Target path for build files.", "buildfiles"};

    Emitter(StringId name, std::string description)
        : name(name)
        , description(std::move(description))
    {
        Emitters::install(this);
    }

    Emitter(const Emitter& other) = delete;
    Emitter& operator=(const Emitter& other) = delete;

    virtual void emit(Environment& env) = 0;

protected:

    static std::pair<Project*, std::filesystem::path> createGeneratorProject(Environment& env, std::filesystem::path targetPath)
    {
        targetPath = targetPath / ".build.h";
        std::string ext;
        if(OperatingSystem::current() == Windows)
        {
            ext = ".exe";
        }
        auto tempOutput = targetPath / std::filesystem::path(env.configurationFile).filename().replace_extension(ext);
        auto prevOutput = targetPath / std::filesystem::path(env.configurationFile).filename().replace_extension(ext + ".prev");
        auto buildOutput = std::filesystem::path(env.configurationFile).replace_extension(ext);
        Project& project = env.createProject("_generator", Executable);
        project.features += { feature::Cpp17, feature::DebugSymbols, feature::Exceptions };
        project.includePaths += env.buildHDir;
        project.output.path = tempOutput;
        project.files += env.configurationFile;
        project.commands += commands::chain({commands::move(buildOutput, prevOutput), commands::copy(tempOutput, buildOutput)}, "Replacing '" + buildOutput.filename().string() + "'.");

        return { &project, buildOutput };
    }
};
