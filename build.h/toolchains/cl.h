#pragma once

#include <filesystem>
#include <string>

#include "core/project.h"
#include "modules/command.h"
#include "modules/feature.h"
#include "modules/toolchain.h"
#include "util/string.h"

struct ClToolchainProvider : public ToolchainProvider
{
    const std::string compiler;
    const std::string linker;
    const std::string archiver;
    const std::vector<std::filesystem::path> sysIncludePaths;
    const std::vector<std::filesystem::path> sysLibPaths;

    ClToolchainProvider(std::string name, std::string compiler, std::string linker, std::string archiver, std::vector<std::filesystem::path> sysIncludePaths, std::vector<std::filesystem::path> sysLibPaths)
        : ToolchainProvider(name)
        , compiler(std::move(compiler))
        , linker(std::move(linker))
        , archiver(std::move(archiver))
        , sysIncludePaths(std::move(sysIncludePaths))
        , sysLibPaths(std::move(sysLibPaths))
    {
    }

    virtual std::string getCompiler(Project& project, OptionCollection& resolvedOptions, std::filesystem::path pathOffset) const override 
    {
        return compiler;
    }

    virtual std::string getCommonCompilerFlags(Project& project, OptionCollection& resolvedOptions, std::filesystem::path pathOffset) const override
    {
        std::string flags;

        flags += " /nologo";

        for(auto& define : resolvedOptions[Defines])
        {
            flags += " /D\"" + define + "\"";
        }
        for(auto& path : sysIncludePaths)
        {
            flags += " /I\"" + path.string() + "\"";
        }
        for(auto& path : resolvedOptions[IncludePaths])
        {
            flags += " /I\"" + (pathOffset / path).string() + "\"";
        }

        std::map<Feature, std::string> featureMap = {
            { feature::Cpp11, " /std:c++11"},
            { feature::Cpp14, " /std:c++14"},
            { feature::Cpp17, " /std:c++17"},
            { feature::Cpp20, " /std:c++20"},
            { feature::Cpp23, " /std:c++23"},
            { feature::Optimize, " /Ox"},
            { feature::OptimizeSize, " /Os"},
            { feature::DebugSymbols, " /Zi"},
            { feature::WarningsAsErrors, " /WX"},
            { feature::FastMath, " /fp:fast"},
            { feature::Exceptions, " /EHsc"},
        };
        for(auto& feature : resolvedOptions[Features])
        {
            auto it = featureMap.find(feature);
            if(it != featureMap.end())
            {
                flags += it->second;
            }
        }

        return flags;
    }

    virtual std::string getCompilerFlags(Project& project, OptionCollection& resolvedOptions, std::filesystem::path pathOffset, const std::string& input, const std::string& output) const override
    {
        return " /sourceDependencies \"" + output + ".d\" /c /Fd:\"" + output + "\".pdb /Fo:\"" + output + "\" " + str::quote(input);
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

        flags += " /nologo";

        switch(*project.type)
        {
        default:
            throw std::runtime_error("Project type in '" + project.name + "' not supported by toolchain.");
        case StaticLib:
            flags += "";
            break;
        case Executable:
        case SharedLib:
            for(auto& path : sysLibPaths)
            {
                flags += " /LIBPATH:\"" + path.string() + "\"";
            }
            for(auto& path : resolvedOptions[Libs])
            {
                flags += " " + (pathOffset / path).string();
            }

            std::map<Feature, std::string> featureMap = {
                { feature::DebugSymbols, " /DEBUG"},
            };
            for(auto& feature : resolvedOptions[Features])
            {
                auto it = featureMap.find(feature);
                if(it != featureMap.end())
                {
                    flags += it->second;
                }
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
            flags += " /OUT:\"" + output + "\"";
            for(auto& input : inputs)
            {
                flags += " \"" + input + "\"";
            }
            break;
        case Executable:
        case SharedLib:
            flags += " /OUT:\"" + output + "\"";
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
        Option<std::vector<std::filesystem::path>> LinkedOutputs{"_LinkedCLOutputs"};
        std::filesystem::path pathOffset = std::filesystem::proximate(std::filesystem::current_path(), workingDir);

        if(project.type != Executable &&
           project.type != SharedLib &&
           project.type != StaticLib)
        {
            return {};
        }

        auto dataDir = resolvedOptions[DataDir];

        auto compiler = str::quote(getCompiler(project, resolvedOptions, pathOffset));
        auto commonCompilerFlags = getCommonCompilerFlags(project, resolvedOptions, pathOffset);
        auto linker = str::quote(getLinker(project, resolvedOptions, pathOffset));
        auto commonLinkerFlags = getCommonLinkerFlags(project, resolvedOptions, pathOffset);

        auto buildPch = resolvedOptions[BuildPch];
        auto importPch = resolvedOptions[ImportPch];
        // TODO: PCH

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
            command.command = compiler + commonCompilerFlags + getCompilerFlags(project, resolvedOptions, pathOffset, inputStr, outputStr);
            command.inputs = { input };
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
            for(auto& output : resolvedOptions[LinkedOutputs])
            {
                linkerInputs.push_back(output);
            }

            std::vector<std::string> linkerInputStrs;
            linkerInputStrs.reserve(linkerInputs.size());
            bool windows = OperatingSystem::current() == Windows;
            for(auto& input : linkerInputs)
            {
                if(windows && input.extension().empty())
                {
                    linkerInputStrs.push_back((pathOffset / input).string() + ".");
                }
                else
                {
                    linkerInputStrs.push_back((pathOffset / input).string());
                }
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
                project[Public / config][LinkedOutputs] += output;
            }
        }

        return outputs;
    }
};

#if 0
struct ClToolchainProvider : public ClToolchainProviderBase
{
    using ClToolchainProviderBase::ClToolchainProviderBase;
    static ClToolchainProvider* getInstance()
    {
        static ClToolchainProvider instance("msvc", "cl", "link", "lib", {}, {});
        return &instance;
    }
    static Toolchains::Token installToken;
};

Toolchains::Token ClToolchainProvider::installToken = Toolchains::install(ClToolchainProvider::getInstance());
#endif