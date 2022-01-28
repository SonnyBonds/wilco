#pragma once

#include <filesystem>
#include <string>

#include "core/project.h"
#include "modules/command.h"
#include "modules/toolchain.h"
#include "modules/feature.h"

Option<std::vector<StringId>> GccCompilerFlags{"GccCompilerFlags"};
Option<std::vector<StringId>> GccLinkerFlags{"GccLinkerFlags"};
Option<std::vector<StringId>> GccArchiverFlags{"GccArchiverFlags"};

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

    virtual std::string getCompiler(Project& project, OptionCollection& resolvedOptions, std::filesystem::path pathOffset) const override 
    {
        return compiler;
    }

    virtual std::string getCommonCompilerFlags(Project& project, OptionCollection& resolvedOptions, std::filesystem::path pathOffset) const override
    {
        std::string flags;

        for(auto& define : resolvedOptions[Defines])
        {
            flags += " -D\"" + define + "\"";
        }
        for(auto& path : resolvedOptions[IncludePaths])
        {
            flags += " -I\"" + (pathOffset / path).string() + "\"";
        }
        if(resolvedOptions[Platform] == "x64")
        {
            flags += " -m64 -arch x86_64";
        }

        std::map<Feature, std::string> featureMap = {
            { feature::Cpp11, " -std=c++11"},
            { feature::Cpp14, " -std=c++14"},
            { feature::Cpp17, " -std=c++17"},
            { feature::Cpp20, " -std=c++20"},
            { feature::Cpp23, " -std=c++23"},
            { feature::Optimize, " -O2"},
            { feature::OptimizeSize, " -Os"},
            { feature::DebugSymbols, " -g"},
            { feature::WarningsAsErrors, " -Werror"},
            { feature::FastMath, " -ffast-math"},
            { feature::Exceptions, " -fexceptions"},
        };
        for(auto& feature : resolvedOptions[Features])
        {
            auto it = featureMap.find(feature);
            if(it != featureMap.end())
            {
                flags += it->second;
            }
        }

        for(auto& flag : resolvedOptions[GccCompilerFlags])
        {
            flags += " " + std::string(flag);
        }

        return flags;
    }

    virtual std::string getCompilerFlags(Project& project, OptionCollection& resolvedOptions, std::filesystem::path pathOffset, const std::string& input, const std::string& output) const override
    {
        return " -MMD -MF " + output + ".d " + " -c -o " + output + " " + input;
    }

    virtual std::string getLinker(Project& project, OptionCollection& resolvedOptions, std::filesystem::path pathOffset) const override
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

    virtual std::string getCommonLinkerFlags(Project& project, OptionCollection& resolvedOptions, std::filesystem::path pathOffset) const override
    {
        std::string flags;

        switch(*project.type)
        {
        default:
            throw std::runtime_error("Project type in '" + project.name + "' not supported by toolchain.");
        case StaticLib:
            flags += " -rcs";
            for(auto& flag : resolvedOptions[GccArchiverFlags])
            {
                flags += " " + std::string(flag);
            }
            break;
        case Executable:
        case SharedLib:
            for(auto& path : resolvedOptions[Libs])
            {
                flags += " " + (pathOffset / path).string();
            }

            for(auto& framework : resolvedOptions[Frameworks])
            {
                flags += " -framework " + framework;
            }

            if(project.type == SharedLib)
            {
                auto features = resolvedOptions[Features];
                if(std::find(features.begin(), features.end(), feature::MacOSBundle) != features.end())
                {
                    flags += " -bundle";
                }
                else
                {
                    flags += " -shared";
                }
            }

            for(auto& flag : resolvedOptions[GccLinkerFlags])
            {
                flags += " " + std::string(flag);
            }

            break;
        }

        return flags;
    }

    virtual std::string getLinkerFlags(Project& project, OptionCollection& resolvedOptions, std::filesystem::path pathOffset, const std::vector<std::string>& inputs, const std::string& output) const override
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

    std::vector<std::filesystem::path> process(Project& project, OptionCollection& resolvedOptions, StringId config, const std::filesystem::path& workingDir) const override
    {
        Option<std::vector<std::filesystem::path>> LinkedOutputs{"_LinkedOutputs"};
        std::filesystem::path pathOffset = std::filesystem::proximate(std::filesystem::current_path(), workingDir);

        if(project.type != Executable &&
           project.type != SharedLib &&
           project.type != StaticLib)
        {
            return {};
        }

        auto dataDir = resolvedOptions[DataDir];

        auto compiler = getCompiler(project, resolvedOptions, pathOffset);
        auto commonCompilerFlags = getCommonCompilerFlags(project, resolvedOptions, pathOffset);
        auto commonCompilerFlagsObjC = commonCompilerFlags;
        auto linker = getLinker(project, resolvedOptions, pathOffset);
        auto commonLinkerFlags = getCommonLinkerFlags(project, resolvedOptions, pathOffset);

        auto buildPch = resolvedOptions[BuildPch];
        auto importPch = resolvedOptions[ImportPch];

        if(!buildPch.empty())
        {
            auto input = buildPch;
            auto inputStr = (pathOffset / input).string();

            auto output = dataDir / std::filesystem::path("pch") / (input.string() + ".pch");
            auto outputStr = (pathOffset / output).string();

            auto outputObjC = dataDir / std::filesystem::path("pch") / (input.string() + ".pchmm");
            auto outputObjCStr = (pathOffset / outputObjC).string();

            {
                CommandEntry command;
                command.command = compiler + commonCompilerFlags + " -x c++-header -Xclang -emit-pch " + getCompilerFlags(project, resolvedOptions, pathOffset, inputStr, outputStr);
                command.inputs = { input };
                command.outputs = { output };
                command.workingDirectory = workingDir;
                command.depFile = output.string() + ".d";
                command.description = "Compiling " + project.name + " PCH: " + input.string();
                resolvedOptions[Commands] += std::move(command);
            }

            {
                CommandEntry command;
                command.command = compiler + commonCompilerFlags + " -x objective-c++-header -Xclang -emit-pch " + getCompilerFlags(project, resolvedOptions, pathOffset, inputStr, outputObjCStr);
                command.inputs = { input };
                command.outputs = { outputObjC };
                command.workingDirectory = workingDir;
                command.depFile = outputObjC.string() + ".d";
                command.description = "Compiling " + project.name + " PCH (Objective-C++): " + input.string();
                resolvedOptions[Commands] += std::move(command);
            }
        }

        std::vector<std::filesystem::path> pchInputs;
        if(!importPch.empty())
        {
            auto input = dataDir / std::filesystem::path("pch") / (importPch.string() + ".pch");
            auto inputStr = (pathOffset / input).string();
            commonCompilerFlags += " -Xclang -include-pch -Xclang " + inputStr;
            pchInputs.push_back(input);

            auto inputObjC = dataDir / std::filesystem::path("pch") / (importPch.string() + ".pchmm");
            auto inputObjCStr = (pathOffset / inputObjC).string();
            commonCompilerFlagsObjC += " -Xclang -include-pch -Xclang " + inputObjCStr;
            pchInputs.push_back(inputObjC);
        }

        std::vector<std::filesystem::path> linkerInputs;
        for(auto& input : resolvedOptions[Files])
        {
            auto ext = std::filesystem::path(input).extension().string();
            auto exts = { ".c", ".cpp", ".mm" }; // TODO: Not hardcode these maybe
            if(std::find(exts.begin(), exts.end(), ext) == exts.end()) continue;

            auto inputStr = (pathOffset / input).string();
            auto output = dataDir / std::filesystem::path("obj") / project.name / (input.string() + ".o");
            auto outputStr = (pathOffset / output).string();

            CommandEntry command;
            command.command = compiler + (ext == ".mm" ? commonCompilerFlagsObjC : commonCompilerFlags) + getCompilerFlags(project, resolvedOptions, pathOffset, inputStr, outputStr);
            command.inputs = { input };
            command.inputs += pchInputs;
            command.outputs = { output };
            command.workingDirectory = workingDir;
            command.depFile = output.string() + ".d";
            command.description = "Compiling " + project.name + ": " + input.string();
            resolvedOptions[Commands] += std::move(command);

            linkerInputs.push_back(output);
        }

        std::vector<std::filesystem::path> outputs;

        if(!linker.empty())
        {
            if(project.type != StaticLib)
            {
                for(auto& output : resolvedOptions[LinkedOutputs])
                {
                    linkerInputs.push_back(output);
                }
            }

            std::vector<std::string> linkerInputStrs;
            linkerInputStrs.reserve(linkerInputs.size());
            for(auto& input : linkerInputs)
            {
                linkerInputStrs.push_back((pathOffset / input).string());
            }

            auto output = project.calcOutputPath(resolvedOptions);
            auto outputStr = (pathOffset / output).string();

            CommandEntry command;
            command.command = linker + commonLinkerFlags + getLinkerFlags(project, resolvedOptions, pathOffset, linkerInputStrs, outputStr);
            command.inputs = std::move(linkerInputs);
            command.outputs = { output };
            command.workingDirectory = workingDir;
            command.description = "Linking " + project.name + ": " + output.string();
            resolvedOptions[Commands] += std::move(command);

            outputs.push_back(output);

            if(project.type == StaticLib)
            {
                project[PublicOnly / config][LinkedOutputs] += output;
            }
        }

        return outputs;
    }
};
