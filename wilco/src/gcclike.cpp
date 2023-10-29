#include "toolchains/gcclike.h"

GccLikeToolchainProvider::GccLikeToolchainProvider(std::string name, std::string compiler, std::string linker, std::string archiver)
    : ToolchainProvider(name) 
    , compiler(compiler)
    , linker(linker)
    , archiver(archiver)
{
}

std::string GccLikeToolchainProvider::getCompiler(Project& project, std::filesystem::path pathOffset, Language language) const
{
    return compiler;
}

std::string GccLikeToolchainProvider::getCommonCompilerFlags(Project& project, std::filesystem::path pathOffset, Language language, bool pch) const
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

    for(auto& define : project.defines)
    {
        flags += " -D" + str::quote(define);
    }
    for(auto& path : project.includePaths)
    {
        flags += " -I\"" + (pathOffset / path).string() + "\"";
    }
    
    std::unordered_map<Feature, std::string> featureMap = {
        { feature::Optimize, " -O3"},
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

    for(auto& feature : project.features)
    {
        auto it = featureMap.find(feature);
        if(it != featureMap.end())
        {
            flags += it->second;
        }
    }

    for(auto& flag : project.ext<extensions::Gcc>().compilerFlags)
    {
        flags += " " + std::string(flag);
    }

    return flags;
}

std::string GccLikeToolchainProvider::getCompilerFlags(Project& project, std::filesystem::path pathOffset, Language language, const std::string& input, const std::string& output) const
{
    return " -MMD -MF " + output + ".d " + " -c -o " + output + " " + input;
}

std::string GccLikeToolchainProvider::getLinker(Project& project, std::filesystem::path pathOffset) const
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

std::string GccLikeToolchainProvider::getCommonLinkerFlags(Project& project, std::filesystem::path pathOffset) const
{
    std::string flags;

    switch(project.type)
    {
    default:
        throw std::runtime_error("Project type in '" + project.name + "' not supported by toolchain.");
    case StaticLib:
        flags += " -rcs";
        for(auto& flag : project.ext<extensions::Gcc>().archiverFlags)
        {
            flags += " " + std::string(flag);
        }
        break;
    case Executable:
    case SharedLib:
        flags += " -L\"" + pathOffset.string() + "\"";
        for(auto& path : project.libPaths)
        {
            flags += " -L\"" + (pathOffset / path).string() + "\"";
        }

        for(auto& path : project.systemLibs)
        {
            flags += " -l" + path.string();
        }

        for(auto& framework : project.frameworks)
        {
            flags += " -framework " + framework;
        }

        if(project.type == SharedLib)
        {
            const auto& features = project.features;
            if(std::find(features.begin(), features.end(), feature::macos::Bundle) != features.end())
            {
                flags += " -bundle";
            }
            else
            {
                flags += " -shared";
            }
        }

        for(auto& flag : project.ext<extensions::Gcc>().linkerFlags)
        {
            flags += " " + std::string(flag);
        }

        break;
    }

    return flags;
}

std::string GccLikeToolchainProvider::getLinkerFlags(Project& project, std::filesystem::path pathOffset, const std::vector<std::string>& inputs, const std::string& output) const
{
    std::string flags;

    switch(project.type)
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

std::vector<std::filesystem::path> GccLikeToolchainProvider::process(Project& project, const std::filesystem::path& workingDir, const std::filesystem::path& dataDir) const
{
    std::filesystem::path pathOffset = std::filesystem::proximate(std::filesystem::current_path(), workingDir);

    if(project.type != Executable &&
        project.type != SharedLib &&
        project.type != StaticLib)
    {
        return {};
    }

    const auto& gccExt = project.ext<extensions::Gcc>();
    const auto& buildPch = gccExt.pch.build;
    const auto& importPch = gccExt.pch.use;

    // TODO: Do PCH management less hard coded, and only build PCHs for different languages if needed
    if(!buildPch.empty())
    {
        auto input = buildPch;
        auto inputStr = (pathOffset / input).string();

        auto pchPath = input.relative_path().string();
        str::replaceAllInPlace(pchPath, ":", "_");
        str::replaceAllInPlace(pchPath, "..", "__");

        auto output = dataDir / std::filesystem::path("pch") / (pchPath + ".pch");
        auto outputStr = (pathOffset / output).string();

        auto outputObjC = dataDir / std::filesystem::path("pch") / (pchPath + ".pchmm");
        auto outputObjCStr = (pathOffset / outputObjC).string();

        {
            CommandEntry command;
            command.command = str::quote(getCompiler(project, pathOffset, lang::Cpp)) + 
                                getCommonCompilerFlags(project, pathOffset, lang::Cpp, true) + 
                                getCompilerFlags(project, pathOffset, lang::Cpp, inputStr, outputStr);
            command.inputs = { input };
            command.outputs = { output };
            command.workingDirectory = workingDir;
            command.depFile = output.string() + ".d";
            command.description = "Compiling " + project.name + " PCH: " + input.string();
            project.commands += std::move(command);
        }

        // TODO: This should be dependent on if it's needed rather than what OS we're on
        if(OperatingSystem::current() == MacOS)
        {
            CommandEntry command;
            command.command = str::quote(getCompiler(project, pathOffset, lang::ObjectiveCpp)) + 
                                getCommonCompilerFlags(project, pathOffset, lang::ObjectiveCpp, true) + 
                                getCompilerFlags(project, pathOffset, lang::ObjectiveCpp, inputStr, outputObjCStr);
            command.inputs = { input };
            command.outputs = { outputObjC };
            command.workingDirectory = workingDir;
            command.depFile = outputObjC.string() + ".d";
            command.description = "Compiling " + project.name + " PCH (Objective-C++): " + input.string();
            project.commands += std::move(command);
        }
    }

    std::string cppPchFlags;
    std::string objCppPchFlags;
    std::vector<std::filesystem::path> pchInputs;
    if(!importPch.empty())
    {
        auto pchPath = importPch.relative_path().string();
        str::replaceAllInPlace(pchPath, ":", "_");
        str::replaceAllInPlace(pchPath, "..", "__");

        auto input = dataDir / std::filesystem::path("pch") / (pchPath + ".pch");
        auto inputStr = (pathOffset / input).string();
        cppPchFlags += " -Xclang -include-pch -Xclang " + inputStr;
        pchInputs.push_back(input);

        // TODO: This should be dependent on if it's needed rather than what OS we're on
        if(OperatingSystem::current() == MacOS)
        {
            auto inputObjCpp = dataDir / std::filesystem::path("pch") / (pchPath + ".pchmm");
            auto inputObjCppStr = (pathOffset / inputObjCpp).string();
            objCppPchFlags += " -Xclang -include-pch -Xclang " + inputObjCppStr;
            pchInputs.push_back(inputObjCpp);
        }
    }

    std::unordered_map<Language, std::string, std::hash<std::string>> commonCompilerFlags;
    auto getCommonCompilerCommand = [&](Language language) -> const std::string& {
        auto it = commonCompilerFlags.find(language);
        if(it != commonCompilerFlags.end())
        {
            return it->second;
        }

        auto flags = str::quote(getCompiler(project, pathOffset, language)) +
                        getCommonCompilerFlags(project, pathOffset, language, false);
        
        return commonCompilerFlags[language] = flags;
    };

    auto linkerCommand = str::quote(getLinker(project, pathOffset)) + getCommonLinkerFlags(project, pathOffset);

    std::unordered_set<std::string> ignorePch;
    ignorePch.reserve(gccExt.pch.ignoredFiles.size());
    for (auto& file : gccExt.pch.ignoredFiles)
    {
        ignorePch.insert(file.lexically_normal().string());
    }

    std::vector<std::filesystem::path> linkerInputs;
    for(auto& input : project.files)
    {
        auto language = input.language != lang::Auto ? input.language : Language::getByPath(input.path);
        if(language == lang::None)
        {
            continue;
        }

        auto inputStr = (pathOffset / input.path).string();
        auto objPath = input.path.relative_path().string();
        str::replaceAllInPlace(objPath, ":", "_");
        str::replaceAllInPlace(objPath, "..", "__");
        auto output = dataDir / std::filesystem::path("obj") / project.name / (objPath + ".o");
        auto outputStr = (pathOffset / output).string();

        CommandEntry command;
        command.command = getCommonCompilerCommand(language) + 
                            getCompilerFlags(project, pathOffset, language, inputStr, outputStr);

        if(ignorePch.find(input.path.lexically_normal().string()) == ignorePch.end())
        {
            // TODO: Do PCH management less hard coded, and only build PCHs for different languages if needed
            if(language == lang::Cpp)
            {
                command.command += cppPchFlags;
            }
            else if(language == lang::ObjectiveCpp)
            {
                command.command += objCppPchFlags;
            }
        }
        command.inputs = { input.path };
        command.inputs.insert(command.inputs.end(), pchInputs.begin(), pchInputs.end());
        command.outputs = { output };
        command.workingDirectory = workingDir;
        command.depFile = output.string() + ".d";
        command.description = "Compiling " + project.name + ": " + input.path.string();
        project.commands += std::move(command);

        linkerInputs.push_back(output);
    }

    std::vector<std::filesystem::path> outputs;

    if(!linker.empty())
    {
        if(project.type == Executable || project.type == SharedLib)
        {
            for(auto& path : project.libs)
            {
                linkerInputs.push_back(path);
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

        std::filesystem::path output = project.output;
        if(output.empty())
        {
            throw std::runtime_error("Project '" + project.name + "' has no output set.");
        }
        auto outputStr = (pathOffset / output).string();

        CommandEntry command;
        command.command = linkerCommand + getLinkerFlags(project, pathOffset, linkerInputStrs, outputStr);
        command.inputs = std::move(linkerInputs);
        command.outputs = { output };
        command.workingDirectory = workingDir;
        command.description = "Linking " + project.name + ": " + output.string();
        project.commands += std::move(command);

        outputs.push_back(output);
    }

    return outputs;
}