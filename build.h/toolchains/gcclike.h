#pragma once

#include <filesystem>
#include <string>

#include "core/project.h"
#include "modules/command.h"
#include "modules/language.h"
#include "modules/feature.h"
#include "modules/toolchain.h"
#include "util/string.h"

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

    virtual std::string getCompiler(Project& project, OptionCollection& resolvedOptions, std::filesystem::path pathOffset, Language language) const
    {
        return compiler;
    }

    virtual std::string getCommonCompilerFlags(Project& project, OptionCollection& resolvedOptions, std::filesystem::path pathOffset, Language language, bool pch) const
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

        for(auto& define : resolvedOptions[Defines])
        {
            flags += " -D" + str::quote(define);
        }
        for(auto& path : resolvedOptions[IncludePaths])
        {
            flags += " -I\"" + (pathOffset / path).string() + "\"";
        }
        if(resolvedOptions[Platform] == "x64")
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

    virtual std::string getCompilerFlags(Project& project, OptionCollection& resolvedOptions, std::filesystem::path pathOffset, Language language, const std::string& input, const std::string& output) const
    {
        return " -MMD -MF " + output + ".d " + " -c -o " + output + " " + input;
    }

    virtual std::string getLinker(Project& project, OptionCollection& resolvedOptions, std::filesystem::path pathOffset) const
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

    virtual std::string getCommonLinkerFlags(Project& project, OptionCollection& resolvedOptions, std::filesystem::path pathOffset) const
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
            flags += " -L\"" + pathOffset.string() + "\"";
            for(auto& path : resolvedOptions[LibPaths])
            {
                flags += " -L\"" + (pathOffset / path).string() + "\"";
            }

            for(auto& path : resolvedOptions[Libs])
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

    virtual std::string getLinkerFlags(Project& project, OptionCollection& resolvedOptions, std::filesystem::path pathOffset, const std::vector<std::string>& inputs, const std::string& output) const
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
        Option<std::vector<std::filesystem::path>> LinkedOutputs{"_LinkedGCCOutputs"};
        std::filesystem::path pathOffset = std::filesystem::proximate(std::filesystem::current_path(), workingDir);

        if(project.type != Executable &&
           project.type != SharedLib &&
           project.type != StaticLib)
        {
            return {};
        }

        auto dataDir = resolvedOptions[DataDir];

        auto buildPch = resolvedOptions[BuildPch];
        auto importPch = resolvedOptions[ImportPch];

        // TODO: Do PCH management less hard coded, and only build PCHs for different languages if needed
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
                command.command = getCompiler(project, resolvedOptions, pathOffset, lang::Cpp) + 
                                  getCommonCompilerFlags(project, resolvedOptions, pathOffset, lang::Cpp, true) + 
                                  getCompilerFlags(project, resolvedOptions, pathOffset, lang::Cpp, inputStr, outputStr);
                command.inputs = { input };
                command.outputs = { output };
                command.workingDirectory = workingDir;
                command.depFile = output.string() + ".d";
                command.description = "Compiling " + project.name + " PCH: " + input.string();
                resolvedOptions[Commands] += std::move(command);
            }

            {
                CommandEntry command;
                command.command = getCompiler(project, resolvedOptions, pathOffset, lang::ObjectiveCpp) + 
                                  getCommonCompilerFlags(project, resolvedOptions, pathOffset, lang::ObjectiveCpp, true) + 
                                  getCompilerFlags(project, resolvedOptions, pathOffset, lang::ObjectiveCpp, inputStr, outputStr);
                command.inputs = { input };
                command.outputs = { outputObjC };
                command.workingDirectory = workingDir;
                command.depFile = outputObjC.string() + ".d";
                command.description = "Compiling " + project.name + " PCH (Objective-C++): " + input.string();
                resolvedOptions[Commands] += std::move(command);
            }
        }

        std::string cppPchFlags;
        std::string objCppPchFlags;
        std::vector<std::filesystem::path> pchInputs;
        if(!importPch.empty())
        {
            auto input = dataDir / std::filesystem::path("pch") / (importPch.string() + ".pch");
            auto inputStr = (pathOffset / input).string();
            cppPchFlags += " -Xclang -include-pch -Xclang " + inputStr;
            pchInputs.push_back(input);

            auto inputObjCpp = dataDir / std::filesystem::path("pch") / (importPch.string() + ".pchmm");
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

            return commonCompilerFlags[language] = str::quote(getCompiler(project, resolvedOptions, pathOffset, language)) +
                                                   getCommonCompilerFlags(project, resolvedOptions, pathOffset, language, false);
        };

        auto linkerCommand = str::quote(getLinker(project, resolvedOptions, pathOffset)) + getCommonLinkerFlags(project, resolvedOptions, pathOffset);

        std::vector<std::filesystem::path> linkerInputs;
        for(auto& input : resolvedOptions[Files])
        {
            auto language = input.language != lang::Auto ? input.language : Language::getByPath(input.path);
            if(language == lang::None)
            {
                continue;
            }

            /*std::string langFlag;
            if(ext == ".c")
            {
                langFlag = " -x c";
            }
            else if(ext == ".m")
            {
                langFlag = " -x objective-c";
            }*/

            auto inputStr = (pathOffset / input.path).string();
            auto output = dataDir / std::filesystem::path("obj") / project.name / (input.path.string() + ".o");
            auto outputStr = (pathOffset / output).string();

            CommandEntry command;
            command.command = getCommonCompilerCommand(language) + 
                              getCompilerFlags(project, resolvedOptions, pathOffset, language, inputStr, outputStr);
            command.inputs = { input.path };
            command.inputs += pchInputs;
            command.outputs = { output };
            command.workingDirectory = workingDir;
            command.depFile = output.string() + ".d";
            command.description = "Compiling " + project.name + ": " + input.path.string();
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
            command.command = linkerCommand + getLinkerFlags(project, resolvedOptions, pathOffset, linkerInputStrs, outputStr);
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
