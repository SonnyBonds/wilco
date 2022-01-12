#pragma once

#include <filesystem>
#include <string>

#include "core/project.h"
#include "modules/command.h"
#include "modules/toolchain.h"

struct GccLikeToolchainProvider : public ToolchainProvider
{
    std::string compiler;
    std::string linker;
    std::string archiver;

    GccLikeToolchainProvider(std::string name, std::string compiler, std::string linker, std::string archiver)
        : ToolchainProvider(name) 
        , compiler(compiler)
        , linker(linker)
        , archiver(archiver)
    {
    }

    virtual std::string getCompiler(Project& project, ProjectConfig& resolvedConfig, std::filesystem::path pathOffset) const override 
    {
        return compiler;
    }

    virtual std::string getCommonCompilerFlags(Project& project, ProjectConfig& resolvedConfig, std::filesystem::path pathOffset) const override
    {
        std::string flags;

        for(auto& define : resolvedConfig[Defines])
        {
            flags += " -D\"" + define + "\"";
        }
        for(auto& path : resolvedConfig[IncludePaths])
        {
            flags += " -I\"" + (pathOffset / path).string() + "\"";
        }
        if(resolvedConfig[Platform] == "x64")
        {
            flags += " -m64 -arch x86_64";
        }

        std::map<std::string, std::string> featureMap = {
            { "c++17", " -std=c++17"},
            { "libc++", " -stdlib=libc++"},
            { "optimize", " -O3"},
            { "debuginfo", " -g"},
        };
        for(auto& feature : resolvedConfig[Features])
        {
            auto it = featureMap.find(feature);
            if(it != featureMap.end())
            {
                flags += it->second;
            }
        }

        return flags;
    }

    virtual std::string getCompilerFlags(Project& project, ProjectConfig& resolvedConfig, std::filesystem::path pathOffset, const std::string& input, const std::string& output) const override
    {
        return " -MMD -MF " + output + ".d " + " -c -o " + output + " " + input;
    }

    virtual std::string getLinker(Project& project, ProjectConfig& resolvedConfig, std::filesystem::path pathOffset) const override
    {
        if(project.type == StaticLib)
        {
            return archiver;
        }
        else
        {
            return linker;
        }
    }

    virtual std::string getCommonLinkerFlags(Project& project, ProjectConfig& resolvedConfig, std::filesystem::path pathOffset) const override
    {
        std::string flags;

        switch(*project.type)
        {
        default:
            throw std::runtime_error("Project type in '" + project.name + "' not supported by toolchain.");
        case StaticLib:
            flags += " -rcs";
            break;
        case Executable:
        case SharedLib:
            for(auto& path : resolvedConfig[Libs])
            {
                flags += " " + (pathOffset / path).string();
            }

            for(auto& framework : resolvedConfig[Frameworks])
            {
                flags += " -framework " + framework;
            }

            if(project.type == SharedLib)
            {
                auto features = resolvedConfig[Features];
                if(std::find(features.begin(), features.end(), "bundle") != features.end())
                {
                    flags += " -bundle";
                }
                else
                {
                    flags += " -shared";
                }
            }
            break;
        }

        return flags;
    }

    virtual std::string getLinkerFlags(Project& project, ProjectConfig& resolvedConfig, std::filesystem::path pathOffset, const std::vector<std::string>& inputs, const std::string& output) const override
    {
        std::string flags;

        switch(*project.type)
        {
        default:
            throw std::runtime_error("Project type in '" + project.name + "' not supported by toolchain.");
        case StaticLib:
            flags += " \"" + output + "\"";
            for(auto& input : inputs)
            {
                flags += " \"" + input + "\"";
            }
            break;
        case Executable:
        case SharedLib:
            flags += " -o \"" + output + "\"";
            for(auto& input : inputs)
            {
                flags += " \"" + input + "\"";
            }
            break;
        }

        return flags;
    }

    std::vector<std::filesystem::path> process(Project& project, ProjectConfig& resolvedConfig, StringId config, const std::filesystem::path& workingDir) const override
    {
        Option<std::vector<std::filesystem::path>> LinkedOutputs{"_LinkedOutputs"};
        std::filesystem::path pathOffset = std::filesystem::proximate(std::filesystem::current_path(), workingDir);

        if(project.type != Executable &&
           project.type != SharedLib &&
           project.type != StaticLib)
        {
            return {};
        }

        auto dataDir = resolvedConfig[DataDir];

        auto compiler = getCompiler(project, resolvedConfig, pathOffset);
        auto commonCompilerFlags = getCommonCompilerFlags(project, resolvedConfig, pathOffset);
        auto linker = getLinker(project, resolvedConfig, pathOffset);
        auto commonLinkerFlags = getCommonLinkerFlags(project, resolvedConfig, pathOffset);

        auto buildPch = resolvedConfig[BuildPch];
        auto importPch = resolvedConfig[ImportPch];

        if(!buildPch.empty())
        {
            auto input = buildPch;
            auto inputStr = (pathOffset / input).string();
            auto output = dataDir / std::filesystem::path("pch") / (input.string() + ".pch");
            auto outputStr = (pathOffset / output).string();

            CommandEntry command;
            command.command = compiler + commonCompilerFlags + " -x c++-header -Xclang -emit-pch " + getCompilerFlags(project, resolvedConfig, pathOffset, inputStr, outputStr);
            command.inputs = { input };
            command.outputs = { output };
            command.workingDirectory = workingDir;
            command.depFile = output.string() + ".d";
            command.description = "Compiling " + project.name + " PCH: " + input.string();
            resolvedConfig[Commands] += std::move(command);
        }

        std::vector<std::filesystem::path> pchInputs;
        if(!importPch.empty())
        {
            auto input = dataDir / std::filesystem::path("pch") / (importPch.string() + ".pch");
            auto inputStr = (pathOffset / input).string();
            commonCompilerFlags += " -Xclang -include-pch -Xclang " + inputStr;

            pchInputs.push_back(input);
        }

        std::vector<std::filesystem::path> linkerInputs;
        for(auto& input : resolvedConfig[Files])
        {
            auto ext = std::filesystem::path(input).extension().string();
            auto exts = { ".c", ".cpp", ".mm" }; // TODO: Not hardcode these maybe
            if(std::find(exts.begin(), exts.end(), ext) == exts.end()) continue;

            auto inputStr = (pathOffset / input).string();
            auto output = dataDir / std::filesystem::path("obj") / project.name / (input.string() + ".o");
            auto outputStr = (pathOffset / output).string();

            CommandEntry command;
            command.command = compiler + commonCompilerFlags + getCompilerFlags(project, resolvedConfig, pathOffset, inputStr, outputStr);
            command.inputs = { input };
            command.inputs += pchInputs;
            command.outputs = { output };
            command.workingDirectory = workingDir;
            command.depFile = output.string() + ".d";
            command.description = "Compiling " + project.name + ": " + input.string();
            resolvedConfig[Commands] += std::move(command);

            linkerInputs.push_back(output);
        }

        std::vector<std::filesystem::path> outputs;

        if(!linker.empty())
        {
            for(auto& output : resolvedConfig[LinkedOutputs])
            {
                linkerInputs.push_back(output);
            }

            std::vector<std::string> linkerInputStrs;
            linkerInputStrs.reserve(linkerInputs.size());
            for(auto& input : linkerInputs)
            {
                linkerInputStrs.push_back((pathOffset / input).string());
            }

            auto output = project.calcOutputPath(resolvedConfig);
            auto outputStr = (pathOffset / output).string();

            CommandEntry command;
            command.command = linker + commonLinkerFlags + getLinkerFlags(project, resolvedConfig, pathOffset, linkerInputStrs, outputStr);
            command.inputs = std::move(linkerInputs);
            command.outputs = { output };
            command.workingDirectory = workingDir;
            command.description = "Linking " + project.name + ": " + output.string();
            resolvedConfig[Commands] += std::move(command);

            outputs.push_back(output);

            if(project.type == StaticLib)
            {
                project[Public / config][LinkedOutputs] += output;
            }
        }

        return outputs;
    }
};

struct GccToolchainProvider : public GccLikeToolchainProvider
{
    using GccLikeToolchainProvider::GccLikeToolchainProvider;
    static GccToolchainProvider* getInstance()
    {
        static GccToolchainProvider instance("gcc", "g++", "g++", "ar");
        return &instance;
    }
    static Toolchains::Token installToken;
};

Toolchains::Token GccToolchainProvider::installToken = Toolchains::install(GccToolchainProvider::getInstance());