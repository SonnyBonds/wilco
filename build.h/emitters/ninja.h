#pragma once

#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "core/stringid.h"
#include "core/project.h"
#include "modules/command.h"
#include "modules/postprocess.h"
#include "modules/toolchain.h"
#include "util/operators.h"

// TODO: Not depend on this specifically
#include "toolchains/gcclike.h"

class NinjaEmitter
{
public:
    static void emit(std::filesystem::path targetPath, std::set<Project*> projects, StringId config = {})
    {
        std::filesystem::create_directories(targetPath);

        auto outputFile = targetPath / "build.ninja";
        NinjaEmitter ninja(outputFile);

        std::vector<Project*> orderedProjects;
        std::set<Project*> discoveredProjects;
        for(auto project : projects)
        {
            project->discover(discoveredProjects, orderedProjects);
        }

        std::vector<std::filesystem::path> generatorDependencies;
        for(auto& project : orderedProjects)
        {
            generatorDependencies += (*project)[GeneratorDependencies];
            for(auto& entry : project->configs)
            {
                generatorDependencies += (*project)[GeneratorDependencies];
            }
        }

        auto buildOutput = std::filesystem::path(BUILD_FILE).replace_extension("");
        Project generator("_generator", Executable);
        generator[Features] += { "c++17", "optimize" };
        generator[IncludePaths] += BUILD_H_DIR;
        generator[OutputPath] = buildOutput;
        generator[Defines] += {
            "START_DIR=\\\"" START_DIR "\\\"",
            "BUILD_H_DIR=\\\"" BUILD_H_DIR "\\\"",
            "BUILD_DIR=\\\"" BUILD_DIR "\\\"",
            "BUILD_FILE=\\\"" BUILD_FILE "\\\"",
            "BUILD_ARGS=\\\"" BUILD_ARGS "\\\"",
        };
        generator[Files] += BUILD_FILE;

        generatorDependencies += buildOutput;
        generator[Commands] += { "\"" + (BUILD_DIR / buildOutput).string() + "\" " BUILD_ARGS, generatorDependencies, { outputFile }, START_DIR, {}, "Running build generator." };

        orderedProjects.push_back(&generator);

        for(auto project : orderedProjects)
        {
            auto outputName = emitProject(targetPath, *project, config);
            if(!outputName.empty())
            {
                ninja.subninja(outputName);
            }
        }
    }

private:
    static std::string emitProject(std::filesystem::path& root, Project& project, StringId config)
    {
        auto resolved = project.resolve(project.type, config);
        resolved[DataDir] = root;

        for(auto& processor : resolved[PostProcess])
        {
            processor(project, resolved);
        }

        if(!project.type.has_value())
        {
            return {};
        }

        if(project.name.empty())
        {
            throw std::runtime_error("Trying to emit project with no name.");
        }

        std::cout << "Emitting '" << project.name << "'";
        if(!config.empty())
        {
            std::cout << " (" << config << ")";
        }
        std::cout << "\n";

        auto ninjaName = project.name + ".ninja";
        NinjaEmitter ninja(root / ninjaName);

        std::filesystem::path pathOffset = std::filesystem::proximate(std::filesystem::current_path(), root);

        auto& commands = resolved[Commands];
        if(project.type == Command && commands.empty())
        {
            throw std::runtime_error("Command project '" + project.name + "' has no commands.");
        }

        std::vector<std::string> projectOutputs;

        const ToolchainProvider* toolchain = resolved[Toolchain];
        if(!toolchain)
        {
            // TODO: Will be set up elsewhere later
            static GccLikeToolchainProvider defaultToolchainProvider("g++", "g++", "ar");
            toolchain = &defaultToolchainProvider; 
        }

        auto toolchainOutputs = toolchain->process(project, resolved, config, root);
        for(auto& output : toolchainOutputs)
        {
            projectOutputs.push_back((pathOffset / output).string());
        }

        std::string prologue;
        // TODO: Target platform
        /*if(windows)
        {
            prologue += "cmd /c ";
        }*/
        prologue += "cd \"$cwd\" && ";
        ninja.rule("command", prologue + "$cmd", "$depfile", "", "$desc");

        std::vector<std::string> generatorDep = { "_generator" };
        std::vector<std::string> emptyDep = {};

        for(auto& command : commands)
        {
            std::filesystem::path cwd = command.workingDirectory;
            if(cwd.empty())
            {
                cwd = ".";
            }
            std::string cwdStr = (pathOffset / cwd).string();

            std::vector<std::string> inputStrs;
            inputStrs.reserve(command.inputs.size());
            for(auto& path : command.inputs)
            {
                inputStrs.push_back((pathOffset / path).string());
            }

            std::vector<std::string> outputStrs;
            outputStrs.reserve(command.outputs.size());
            for(auto& path : command.outputs)
            {
                outputStrs.push_back((pathOffset / path).string());
            }

            projectOutputs += outputStrs;

            std::string depfileStr;
            if(!command.depFile.empty())
            {
                depfileStr = (pathOffset / command.depFile).string();
            }

            std::vector<std::pair<std::string_view, std::string_view>> variables;
            variables.push_back({"cmd", command.command});
            variables.push_back({"cwd", cwdStr});
            variables.push_back({"depfile", depfileStr});
            if(!command.description.empty())
            {
                variables.push_back({"desc", command.description});
            }
            ninja.build(outputStrs, "command", inputStrs, {}, project.name == "_generator" ? emptyDep : generatorDep, variables);
        }

        if(!projectOutputs.empty())
        {
            ninja.build({ project.name }, "phony", projectOutputs);
        }

        return ninjaName;
    }

private:
    NinjaEmitter(std::filesystem::path path)
        : _stream(path)
    {
    }

    std::ofstream _stream;

    void subninja(std::string_view name)
    {
        _stream << "subninja " << name << "\n";
    }

    void variable(std::string_view name, std::string_view value)
    {
        _stream << name << " = " << value << "\n";
    }

    void rule(std::string_view name, std::string_view command, std::string_view depfile = {}, std::string_view deps = {}, std::string_view description = {})
    {
        _stream << "rule " << name << "\n";
        _stream << "  command = " << command << "\n";
        if(!depfile.empty())
        {
            _stream << "  depfile = " << depfile << "\n";
        }
        if(!deps.empty())
        {
            _stream << "  deps = " << deps << "\n";
        }
        if(!description.empty())
        {
            _stream << "  description = " << description << "\n";
        }
        _stream << "\n";
    }

    void build(const std::vector<std::string>& outputs, std::string_view rule, const std::vector<std::string>& inputs, const std::vector<std::string>& implicitInputs = {}, const std::vector<std::string>& orderInputs = {}, std::vector<std::pair<std::string_view, std::string_view>> variables = {})
    {
        _stream << "build ";
        for(auto& output : outputs)
        {
            _stream << output << " ";
        }

        _stream << ": " << rule << " ";

        for(auto& input : inputs)
        {
            _stream << input << " ";
        }

        if(!implicitInputs.empty())
        {
            _stream << "| ";
            for(auto& implicitInput : implicitInputs)
            {
                _stream << implicitInput << " ";
            }
        }
        if(!orderInputs.empty())
        {
            _stream << "|| ";
            for(auto& orderInput : orderInputs)
            {
                _stream << orderInput << " ";
            }
        }
        _stream << "\n";
        for(auto& variable : variables)
        {
            _stream << "  " << variable.first << " = " << variable.second << "\n";
        }

        _stream << "\n";
    }
};