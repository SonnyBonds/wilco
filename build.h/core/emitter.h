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

#if TODO
    static std::pair<Project, std::filesystem::path> createGeneratorProject(std::filesystem::path targetPath)
    {
        targetPath = targetPath / ".build.h";
        std::string ext;
        if(OperatingSystem::current() == Windows)
        {
            ext = ".exe";
        }
        auto tempOutput = targetPath / std::filesystem::path(BUILD_FILE).filename().replace_extension(ext);
        auto prevOutput = targetPath / std::filesystem::path(BUILD_FILE).filename().replace_extension(ext + ".prev");
        auto buildOutput = std::filesystem::path(BUILD_FILE).replace_extension(ext);
        Project project("_generator", Executable);
        project[Features] += { feature::Cpp17, feature::DebugSymbols, feature::Exceptions };
        project[IncludePaths] += BUILD_H_DIR;
        project[OutputPath] = tempOutput;
        project[Defines] += {
            "START_DIR=\"" START_DIR "\"",
            "BUILD_H_DIR=\"" BUILD_H_DIR "\"",
            "BUILD_DIR=\"" BUILD_DIR "\"",
            "BUILD_FILE=\"" BUILD_FILE "\"",
            "BUILD_ARGS=\"" BUILD_ARGS "\"",
        };
        project[Files] += BUILD_FILE;
        project[Commands] += commands::chain({commands::move(buildOutput, prevOutput), commands::copy(tempOutput, buildOutput)}, "Replacing '" + buildOutput.filename().string() + "'.");

        return { std::move(project), buildOutput };
    }
#endif
};
