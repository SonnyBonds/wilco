#pragma once

#include <filesystem>
#include <string>

#include "core/project.h"
#include "modules/command.h"
#include "modules/language.h"
#include "modules/feature.h"
#include "modules/toolchain.h"
#include "util/string.h"

namespace extensions
{
    struct Gcc : public Extension
    {
        ListProperty<StringId> compilerFlags{this};
        ListProperty<StringId> linkerFlags{this};
        ListProperty<StringId> archiverFlags{this};
    };
}

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

    virtual std::string getCompiler(Project& project, ProjectSettings& resolvedSettings, std::filesystem::path pathOffset, Language language) const
    {
        return compiler;
    }

    virtual std::string getCommonCompilerFlags(Project& project, ProjectSettings& resolvedSettings, std::filesystem::path pathOffset, Language language, bool pch) const
    {
        std::string flags;

        if(language == lang::C)
        {
            flags += pch ? " -x c-header -Xclang -emit-pch " : " -x c ";
        }
        else if(language == lang::Cpp)
        {
            flags += pch ? " -x c++-header -Xclang -emit-pch " : " -x c++ ";
        }
        else if(language == lang::ObjectiveC)
        {
            flags += pch ? " -x objective-c-header -Xclang -emit-pch " : " -x objective-c ";
        }
        else if(language == lang::ObjectiveCpp)
        {
            flags += pch ? " -x objective-c++-header -Xclang -emit-pch " : " -x objective-c++ ";
        }
        else
        {
            throw std::runtime_error("Toolchain does not support language '" + std::string(language) + "'.");
        }

        for(auto& define : resolvedSettings.defines)
        {
            flags += " -D" + str::quote(define);
        }
        for(auto& path : resolvedSettings.includePaths)
        {
            flags += " -I\"" + (pathOffset / path).string() + "\"";
        }
        if(resolvedSettings.platform.value() == StringId("x64"))
        {
            flags += " -m64 -arch x86_64";
        }

        std::unordered_map<Feature, std::string, std::hash<StringId>> featureMap = {
            { feature::Optimize, " -O2"},
            { feature::OptimizeSize, " -Os"},
            { feature::DebugSymbols, " -g"},
            { feature::WarningsAsErrors, " -Werror"},
            { feature::FastMath, " -ffast-math"},
            { feature::Exceptions, " -fexceptions"},
        };

        if(language == lang::Cpp || language == lang::ObjectiveCpp)
        {
            featureMap.insert({
                {feature::Cpp11, " -std=c++11"},
                { feature::Cpp14, " -std=c++14"},
                { feature::Cpp17, " -std=c++17"},
                { feature::Cpp20, " -std=c++20"},
                { feature::Cpp23, " -std=c++23"}
                });
        }

        for(auto& feature : resolvedSettings.features)
        {
            auto it = featureMap.find(feature);
            if(it != featureMap.end())
            {
                flags += it->second;
            }
        }

        for(auto& flag : resolvedSettings.ext<extensions::Gcc>().compilerFlags)
        {
            flags += " " + std::string(flag);
        }

        return flags;
    }

    virtual std::string getCompilerFlags(Project& project, ProjectSettings& resolvedSettings, std::filesystem::path pathOffset, Language language, const std::string& input, const std::string& output) const
    {
        return " -MMD -MF " + output + ".d " + " -c -o " + output + " " + input;
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

        switch(*project.type)
        {
        default:
            throw std::runtime_error("Project type in '" + project.name + "' not supported by toolchain.");
        case StaticLib:
            flags += " -rcs";
            for(auto& flag : resolvedSettings.ext<extensions::Gcc>().archiverFlags)
            {
                flags += " " + std::string(flag);
            }
            break;
        case Executable:
        case SharedLib:
            flags += " -L\"" + pathOffset.string() + "\"";
            for(auto& path : resolvedSettings.libPaths)
            {
                flags += " -L\"" + (pathOffset / path).string() + "\"";
            }

            for(auto& path : resolvedSettings.libs)
            {
                // It's hard to know if the user wants "libfoo.a" when they say "foo" or not.
                // Might need better heuristics, but assuming having a path specified
                // implies a specific name
                if(path.has_parent_path())
                {
                    flags += " " + (pathOffset / path).string();
                }
                else
                {
                    flags += " -l" + path.string();
                }
            }

            for(auto& framework : resolvedSettings.frameworks)
            {
                flags += " -framework " + framework;
            }

            if(project.type == SharedLib)
            {
                auto features = resolvedSettings.features;
                if(std::find(features.begin(), features.end(), feature::MacOSBundle) != features.end())
                {
                    flags += " -bundle";
                }
                else
                {
                    flags += " -shared";
                }
            }

            for(auto& flag : resolvedSettings.ext<extensions::Gcc>().linkerFlags)
            {
                flags += " " + std::string(flag);
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

    std::vector<std::filesystem::path> process(Project& project, ProjectSettings& resolvedSettings, StringId config, const std::filesystem::path& workingDir) const override
    {
        struct GccInternal : public Extension
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

        auto buildPch = resolvedSettings.buildPch;
        auto importPch = resolvedSettings.importPch;

        // TODO: Do PCH management less hard coded, and only build PCHs for different languages if needed
        if(!buildPch.value().empty())
        {
            auto input = buildPch;
            auto inputStr = (pathOffset / input).string();

            auto output = dataDir / std::filesystem::path("pch") / (input.value().string() + ".pch");
            auto outputStr = (pathOffset / output).string();

            auto outputObjC = dataDir / std::filesystem::path("pch") / (input.value().string() + ".pchmm");
            auto outputObjCStr = (pathOffset / outputObjC).string();

            {
                CommandEntry command;
                command.command = getCompiler(project, resolvedSettings, pathOffset, lang::Cpp) + 
                                  getCommonCompilerFlags(project, resolvedSettings, pathOffset, lang::Cpp, true) + 
                                  getCompilerFlags(project, resolvedSettings, pathOffset, lang::Cpp, inputStr, outputStr);
                command.inputs = { input };
                command.outputs = { output };
                command.workingDirectory = workingDir;
                command.depFile = output.string() + ".d";
                command.description = "Compiling " + project.name + " PCH: " + input.value().string();
                resolvedSettings.commands += std::move(command);
            }

            {
                CommandEntry command;
                command.command = getCompiler(project, resolvedSettings, pathOffset, lang::ObjectiveCpp) + 
                                  getCommonCompilerFlags(project, resolvedSettings, pathOffset, lang::ObjectiveCpp, true) + 
                                  getCompilerFlags(project, resolvedSettings, pathOffset, lang::ObjectiveCpp, inputStr, outputObjCStr);
                command.inputs = { input };
                command.outputs = { outputObjC };
                command.workingDirectory = workingDir;
                command.depFile = outputObjC.string() + ".d";
                command.description = "Compiling " + project.name + " PCH (Objective-C++): " + input.value().string();
                resolvedSettings.commands += std::move(command);
            }
        }

        std::string cppPchFlags;
        std::string objCppPchFlags;
        std::vector<std::filesystem::path> pchInputs;
        if(!importPch.value().empty())
        {
            auto input = dataDir / std::filesystem::path("pch") / (importPch.value().string() + ".pch");
            auto inputStr = (pathOffset / input).string();
            cppPchFlags += " -Xclang -include-pch -Xclang " + inputStr;
            pchInputs.push_back(input);

            auto inputObjCpp = dataDir / std::filesystem::path("pch") / (importPch.value().string() + ".pchmm");
            auto inputObjCppStr = (pathOffset / inputObjCpp).string();
            objCppPchFlags += " -Xclang -include-pch -Xclang " + inputObjCppStr;
            pchInputs.push_back(inputObjCpp);
        }

        std::unordered_map<Language, std::string, std::hash<StringId>> commonCompilerFlags;
        auto getCommonCompilerCommand = [&](Language language) -> const std::string& {
            auto it = commonCompilerFlags.find(language);
            if(it != commonCompilerFlags.end())
            {
                return it->second;
            }

            auto flags = str::quote(getCompiler(project, resolvedSettings, pathOffset, language)) +
                         getCommonCompilerFlags(project, resolvedSettings, pathOffset, language, false);
            
            // TODO: Do PCH management less hard coded, and only build PCHs for different languages if needed
            if(language == lang::Cpp)
            {
                flags += cppPchFlags;
            }
            else if(language == lang::ObjectiveCpp)
            {
                flags += objCppPchFlags;
            }

            return commonCompilerFlags[language] = flags;
        };

        auto linkerCommand = str::quote(getLinker(project, resolvedSettings, pathOffset)) + getCommonLinkerFlags(project, resolvedSettings, pathOffset);

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
            command.inputs.insert(command.inputs.end(), pchInputs.begin(), pchInputs.end());
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
            if(project.type != StaticLib)
            {
                for(auto& output : resolvedSettings.ext<GccInternal>().linkedOutputs)
                {
                    linkerInputs.push_back(output);
                }
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
                project(PublicOnly, config).ext<GccInternal>().linkedOutputs += output;
            }
        }

        return outputs;
    }
};
