#include "toolchains/cl.h"

ClToolchainProvider::ClToolchainProvider(std::string name, std::string compiler, std::string resourceCompiler, std::string linker, std::string archiver, std::vector<std::filesystem::path> sysIncludePaths, std::vector<std::filesystem::path> sysLibPaths)
    : ToolchainProvider(name)
    , compiler(std::move(compiler))
    , resourceCompiler(std::move(resourceCompiler))
    , linker(std::move(linker))
    , archiver(std::move(archiver))
    , sysIncludePaths(std::move(sysIncludePaths))
    , sysLibPaths(std::move(sysLibPaths))
{
}

std::string ClToolchainProvider::getCompiler(Project& project, std::filesystem::path pathOffset, Language language) const 
{
    if(language == lang::Cpp || language == lang::C)
    {
        return compiler;
    }
    else if (language == lang::Rc)
    {
        return resourceCompiler;
    }
    else
    {
        throw std::runtime_error("Toolchain does not support language '" + std::string(language) + "'.");
    }
}

std::string ClToolchainProvider::getCommonCompilerFlags(Project& project, std::filesystem::path pathOffset, Language language) const
{
    std::string flags;

    flags += " /nologo";

    for(auto& define : project.defines)
    {
        flags += " /D" + str::quote(define) + "";
    }
    for(auto& path : sysIncludePaths)
    {
        flags += " /I" + str::quote(path.string()) + "";
    }
    for(auto& path : project.includePaths)
    {
        flags += " /I" + str::quote((pathOffset / path).string());
    }

    if(language != lang::Rc)
    {
        std::map<Feature, std::string> featureMap = {
            { feature::Cpp11, " /std:c++11"},
            { feature::Cpp14, " /std:c++14"},
            { feature::Cpp17, " /std:c++17"},
            { feature::Cpp20, " /std:c++20"},
            { feature::Cpp23, " /std:c++23"},
            { feature::Optimize, " /O2"},
            { feature::DebugSymbols, " /Zi"},
            { feature::WarningsAsErrors, " /WX"},
            { feature::FastMath, " /fp:fast"},
            { feature::Exceptions, " /EHsc"},
            { feature::windows::StaticRuntime, " /MT"},
            { feature::windows::StaticDebugRuntime, " /MTd"},
            { feature::windows::SharedRuntime, " /MD"},
            { feature::windows::SharedDebugRuntime, " /MDd"},
        };
        for(auto& feature : project.features)
        {
            auto it = featureMap.find(feature);
            if(it != featureMap.end())
            {
                flags += it->second;
            }
        }

        auto& msvc = project.ext<extensions::Msvc>();
        for (auto& flag : msvc.compilerFlags)
        {
            flags += " " + std::string(flag);
        }

        if(!msvc.pch.header.empty())
        {
            flags += " /Yu" + str::quote(msvc.pch.header.filename().string());
        }

        // TODO: Maybe not hardcode this path
        std::filesystem::path pdbPath = pathOffset / project.output;
        pdbPath.replace_extension(".pdb");
        flags += " /Fd" + str::quote(pdbPath.string());
    }

    return flags;
}

std::string ClToolchainProvider::getCompilerFlags(Project& project, std::filesystem::path pathOffset, Language language, const std::string& input, const std::string& output) const
{
    if(language == lang::Rc)
    {
        return " /fo" + str::quote(output) + " " + str::quote(input);
    }
    else
    {
        auto langFlag = language == lang::C ? " /Tc" : " /Tp";
        return " /sourceDependencies " + str::quote(output) + ".d /c /FS /Fo:" + str::quote(output) + langFlag + " " + str::quote(input);
    }
}

std::string ClToolchainProvider::getLinker(Project& project, std::filesystem::path pathOffset) const
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

std::string ClToolchainProvider::getCommonLinkerFlags(Project& project, std::filesystem::path pathOffset) const
{
    std::string flags;

    flags += " /nologo /MACHINE:X64";

    switch(project.type)
    {
    default:
        throw std::runtime_error("Project type in '" + project.name + "' not supported by toolchain.");
    case StaticLib:
        flags += "";
        break;
    case SharedLib:
        flags += " /DLL";
    case Executable:
        for(auto& path : sysLibPaths)
        {
            flags += " /LIBPATH:\"" + path.string() + "\"";
        }
        for(auto& path : project.libPaths)
        {
            flags += " /LIBPATH:\"" + path.string() + "\"";
        }
        for(auto& lib : project.systemLibs)
        {
            flags += " " + lib.string();
            if(!str::endsWith(flags, ".lib"))
            {
                flags += ".lib";
            }
        }

        std::map<Feature, std::string> featureMap = {
            { feature::DebugSymbols, " /DEBUG"},
        };
        for(auto& feature : project.features)
        {
            auto it = featureMap.find(feature);
            if(it != featureMap.end())
            {
                flags += it->second;
            }
        }

        break;
    }

    if(project.type == StaticLib)
    {
        for (auto& flag : project.ext<extensions::Msvc>().archiverFlags)
        {
            flags += " " + std::string(flag);
        }
    }
    else
    {
        for (auto& flag : project.ext<extensions::Msvc>().linkerFlags)
        {
            flags += " " + std::string(flag);
        }
    }

    return flags;
}

std::string ClToolchainProvider::getLinkerFlags(Project& project, std::filesystem::path pathOffset, const std::vector<std::string>& inputs, const std::string& output) const
{
    std::string flags;

    switch(project.type)
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

std::vector<std::filesystem::path> ClToolchainProvider::process(Project& project, const std::filesystem::path& workingDir, const std::filesystem::path& dataDir) const
{
    std::filesystem::path pathOffset = std::filesystem::proximate(std::filesystem::current_path(), workingDir);

    if(project.type != Executable &&
       project.type != SharedLib &&
       project.type != StaticLib)
    {
        return {};
    }

    const auto& msvcExt = project.ext<extensions::Msvc>();

    std::filesystem::path pchOutput;
    std::string pchOutputStr;

    if(!msvcExt.pch.source.empty())
    {
        auto pchPath = msvcExt.pch.source.relative_path().string();
        str::replaceAllInPlace(pchPath, ":", "_");
        str::replaceAllInPlace(pchPath, "..", "__");     
        pchOutput = dataDir / std::filesystem::path("obj") / project.name / (pchPath + ".pch");
        pchOutputStr = str::quote(pchOutput.string());
    }

    std::unordered_map<Language, std::string> commonCompilerArgs;
    std::unordered_map<Language, std::string> commonCompilerCommand;
    auto getCommonCompilerArgs = [&](Language language) -> const std::string& {
        auto it = commonCompilerArgs.find(language);
        if(it != commonCompilerArgs.end())
        {
            return it->second;
        }

        std::string pchFlag;
        if(language != lang::Rc)
        {
            if(!pchOutputStr.empty())
            {
                pchFlag = " /Fp:" + pchOutputStr;
            }
        }

        return commonCompilerArgs[language] = getCommonCompilerFlags(project, pathOffset, language) + pchFlag;
    };

    auto getCommonCompilerCommand = [&](Language language) -> const std::string& {
        auto it = commonCompilerCommand.find(language);
        if(it != commonCompilerCommand.end())
        {
            return it->second;
        }


        return commonCompilerCommand[language] = str::quote(getCompiler(project, pathOffset, language));
    };


    auto linkerCommand = str::quote(getLinker(project, pathOffset)) + getCommonLinkerFlags(project, pathOffset);

    std::unordered_set<std::string> ignorePch;
    ignorePch.reserve(msvcExt.pch.ignoredFiles.size());
    for (auto& file : msvcExt.pch.ignoredFiles)
    {
        ignorePch.insert(file.lexically_normal().string());
    }

    std::optional<std::filesystem::path> pdbPath;
 
    // TODO: Better logic for this
    for(auto& feature : project.features)
    {
        if(feature == feature::DebugSymbols)
        {
            // TODO: Maybe not hardcode this path
            auto computedPdbPath = pathOffset / project.output;
            computedPdbPath.replace_extension(".pdb");
            pdbPath = computedPdbPath;
            break;
        }
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
        auto output = dataDir / std::filesystem::path("obj") / project.name / (objPath + (language == lang::Rc ? ".res" : ".obj"));
        auto outputStr = (pathOffset / output).string();

        CommandEntry command;

        command.inputs = { input.path };
        command.outputs = { output };

        if(pdbPath)
        {
            command.outputs.push_back(*pdbPath);
        }

        if(language != lang::Rc)
        {
            command.command = getCommonCompilerCommand(language);
            command.rspContents = getCommonCompilerArgs(language) + getCompilerFlags(project, pathOffset, language, inputStr, outputStr);

            if(!msvcExt.pch.header.empty() && msvcExt.pch.source == input.path)
            {
                command.rspContents += " /Yc" + str::quote(msvcExt.pch.header.filename().string());
                command.outputs.push_back(pchOutput);
            }
            else if(ignorePch.find(input.path.lexically_normal().string()) != ignorePch.end())
            {
                command.rspContents += " /Y-";
            }
            else if(!pchOutput.empty())
            {
                command.inputs.push_back(pchOutput);
            }
            command.depFile = { output.string() + ".d", DepFile::Format::MSVC };
        }
        else
        {
            // There is rsp support in RC, but it requires different escaping somehow so just ignore that for now
            command.command = getCommonCompilerCommand(language) + getCommonCompilerArgs(language) + getCompilerFlags(project, pathOffset, language, inputStr, outputStr);
        }
        command.workingDirectory = workingDir;
        if(!command.rspContents.empty())
        {
            command.rspFile = output.string() + ".rsp";
            command.command += " @" + str::quote((pathOffset / command.rspFile).string(), '"', "\"");
        }
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
        command.command = linkerCommand;
        command.inputs = std::move(linkerInputs);
        command.outputs = { output };
        command.workingDirectory = workingDir;
        command.rspContents = getLinkerFlags(project, pathOffset, linkerInputStrs, outputStr);
        if(!command.rspContents.empty())
        {
            command.rspFile = output.string() + ".rsp";
            command.command += " @" + str::quote((pathOffset / command.rspFile).string(), '"', "\"");
        }
        command.description = "Linking " + project.name + ": " + output.string();
        project.commands += std::move(command);

        outputs.push_back(output);
    }

    return outputs;
}

