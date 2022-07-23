#pragma once

#include <filesystem>
#include <string>

#include "core/project.h"
#include "modules/command.h"
#include "modules/feature.h"
#include "modules/language.h"
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

    virtual std::string getCompiler(Project& project, ProjectSettings& resolvedSettings, std::filesystem::path pathOffset, Language language) const 
    {
        return compiler;
    }

    virtual std::string getCommonCompilerFlags(Project& project, ProjectSettings& resolvedSettings, std::filesystem::path pathOffset, Language language) const
    {
        std::string flags;

        flags += " /nologo";

        for(auto& define : resolvedSettings.defines)
        {
            flags += " /D" + str::quote(define) + "";
        }
        for(auto& path : sysIncludePaths)
        {
            flags += " /I" + str::quote(path.string()) + "";
        }
        for(auto& path : resolvedSettings.includePaths)
        {
            flags += " /I" + str::quote((pathOffset / path).string());
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
        for(auto& feature : resolvedSettings.features)
        {
            auto it = featureMap.find(feature);
            if(it != featureMap.end())
            {
                flags += it->second;
            }
        }

        return flags;
    }

    virtual std::string getCompilerFlags(Project& project, ProjectSettings& resolvedSettings, std::filesystem::path pathOffset, Language language, const std::string& input, const std::string& output) const
    {
        return " /sourceDependencies " + str::quote(output) + ".d /c /Fd:" + str::quote(output) + ".pdb /Fo:" + str::quote(output) + " " + str::quote(input);
    }

    virtual std::string getLinker(Project& project, ProjectSettings& resolvedSettings, std::filesystem::path pathOffset) const
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

    virtual std::string getCommonLinkerFlags(Project& project, ProjectSettings& resolvedSettings, std::filesystem::path pathOffset) const
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
            for(auto& path : resolvedSettings.libs)
            {
                flags += " " + (pathOffset / path).string();
            }

            std::map<Feature, std::string> featureMap = {
                { feature::DebugSymbols, " /DEBUG"},
            };
            for(auto& feature : resolvedSettings.features)
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

    virtual std::string getLinkerFlags(Project& project, ProjectSettings& resolvedSettings, std::filesystem::path pathOffset, const std::vector<std::string>& inputs, const std::string& output) const
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

    std::vector<std::filesystem::path> process(Project& project, ProjectSettings& resolvedSettings, StringId config, const std::filesystem::path& workingDir) const override
    {
        struct ClInternal : public PropertyBag
        {
            ListProperty<std::filesystem::path> linkedOutputs{this, true};
        };

        std::filesystem::path pathOffset = std::filesystem::proximate(std::filesystem::current_path(), workingDir);

        if(project.type != Executable &&
           project.type != SharedLib &&
           project.type != StaticLib)
        {
            return {};
        }

        auto dataDir = resolvedSettings.dataDir;

        std::unordered_map<Language, std::string, std::hash<StringId>> commonCompilerFlags;
        auto getCommonCompilerCommand = [&](Language language) -> const std::string& {
            auto it = commonCompilerFlags.find(language);
            if(it != commonCompilerFlags.end())
            {
                return it->second;
            }

            return commonCompilerFlags[language] = str::quote(getCompiler(project, resolvedSettings, pathOffset, language)) +
                                                   getCommonCompilerFlags(project, resolvedSettings, pathOffset, language);
        };

        auto linkerCommand = str::quote(getLinker(project, resolvedSettings, pathOffset)) + getCommonLinkerFlags(project, resolvedSettings, pathOffset);

        auto buildPch = resolvedSettings.buildPch;
        auto importPch = resolvedSettings.importPch;
        // TODO: PCH

        std::vector<std::filesystem::path> linkerInputs;
        for(auto& input : resolvedSettings.files)
        {
            auto language = input.language != lang::Auto ? input.language : Language::getByPath(input.path);
            if(language == lang::None)
            {
                continue;
            }

            auto inputStr = (pathOffset / input.path).string();
            auto output = dataDir / std::filesystem::path("obj") / project.name / (input.path.string() + ".o");
            auto outputStr = (pathOffset / output).string();

            CommandEntry command;
            command.command = getCommonCompilerCommand(language) + 
                              getCompilerFlags(project, resolvedSettings, pathOffset, language, inputStr, outputStr);
            command.inputs = { input.path };
            command.outputs = { output };
            command.workingDirectory = workingDir;
            command.depFile = output.string() + ".d";
            command.description = "Compiling " + project.name + ": " + input.path.string();
            resolvedSettings.commands += std::move(command);

            linkerInputs.push_back(output);
        }

        std::vector<std::filesystem::path> outputs;

        if(!linker.empty())
        {
            for(auto& output : resolvedSettings.ext<ClInternal>().linkedOutputs)
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

            auto output = project.calcOutputPath(resolvedSettings);
            auto outputStr = (pathOffset / output).string();

            CommandEntry command;
            command.command = linkerCommand + getLinkerFlags(project, resolvedSettings, pathOffset, linkerInputStrs, outputStr);
            command.inputs = std::move(linkerInputs);
            command.outputs = { output };
            command.workingDirectory = workingDir;
            command.description = "Linking " + project.name + ": " + output.string();
            resolvedSettings.commands += std::move(command);

            outputs.push_back(output);

            if(project.type == StaticLib)
            {
                project(PublicOnly, config).ext<ClInternal>().linkedOutputs += output;
            }
        }

        return outputs;
    }
};
