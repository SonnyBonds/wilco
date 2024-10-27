#include "toolchains/gcclike.h"
#include "core/arch.h"
#include "core/project.h"
#include "modules/toolchain.h"
#include "util/commands.h"
#include <filesystem>

GccLikeToolchainProvider::GccLikeToolchainProvider(std::string name, std::string compiler, std::string resourceCompiler, std::string linker, std::string archiver)
    : ToolchainProvider(name) 
    , compiler(compiler)
    , resourceCompiler(resourceCompiler)
    , linker(linker)
    , archiver(archiver)
{
}

std::string GccLikeToolchainProvider::getCompiler(Project& project, std::filesystem::path pathOffset, Language language) const
{
    if(language == lang::Cpp || language == lang::C || language == lang::ObjectiveC || language == lang::ObjectiveCpp)
    {
        return compiler;
    }
    else if (language == lang::Rc && !resourceCompiler.empty())
    {
        return resourceCompiler;
    }
    else
    {
        throw std::runtime_error("Toolchain does not support language '" + std::string(language) + "'.");
    }
}

std::string GccLikeToolchainProvider::getCommonCompilerFlags(Project& project, Architecture arch, std::filesystem::path pathOffset, Language language, bool pch) const
{
    std::string flags;
    
    if(language != lang::Rc)
    {
        if(OperatingSystem::current() == MacOS)
        {
            // TODO: -arch doesn't seem to be supported on Windows - need to figure out what's right here.
            if(arch == architecture::X64)
            {
                flags += " -arch x86_64";
            }
            else if(arch == architecture::Arm64)
            {
                flags += " -arch arm64";
            }
        }
    
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
    }

    auto getFlags = [&](BuildSettings& settings)
    {
        for(auto& define : settings.defines)
        {
            flags += " -D" + str::quote(define);
        }
        for(auto& path : settings.includePaths)
        {
            flags += " -I\"" + (pathOffset / path).string() + "\"";
        }

        if(language == lang::Rc)
        {
            return;
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

        for(auto& feature : settings.features)
        {
            auto it = featureMap.find(feature);
            if(it != featureMap.end())
            {
                flags += it->second;
            }
        }

        for(auto& flag : settings.ext<extensions::Gcc>().compilerFlags)
        {
            flags += " " + std::string(flag);
        }
    };

    getFlags(project);
    if(auto it = project.archSettings.find(arch); it != project.archSettings.end())
    {
        getFlags(it->second);
    }

    return flags;
}

std::string GccLikeToolchainProvider::getCompilerFlags(Project& project, Architecture arch, std::filesystem::path pathOffset, Language language, const std::string& input, const std::string& output) const
{
    if(language == lang::Rc)
    {
        return " -fo" + str::quote(output) + " " + str::quote(input);
    }
    else
    {
        return " -MMD -MF " + output + ".d " + " -c -o " + output + " " + input;
    }
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

std::string GccLikeToolchainProvider::getCommonLinkerFlags(Project& project, Architecture arch, std::filesystem::path pathOffset) const
{
    std::string flags;

    if(OperatingSystem::current() == MacOS && project.type != StaticLib)
    {
        if(arch == architecture::X64)
        {
            flags += " -arch x86_64";
        }
        else if(arch == architecture::Arm64)
        {
            flags += " -arch arm64";
        }
    }

    if(project.type == StaticLib)
    {
        flags += " qc";
    }

    bool sawBundleFeature = false;

    auto getFlags = [&](BuildSettings& settings)
    {
        switch(project.type)
        {
        default:
            throw std::runtime_error("Project type in '" + project.name + "' not supported by toolchain.");
        case StaticLib:
            for(auto& flag : settings.ext<extensions::Gcc>().archiverFlags)
            {
                flags += " " + std::string(flag);
            }
            break;
        case Executable:
        case SharedLib:
            flags += " -L\"" + pathOffset.string() + "\"";
            for(auto& path : settings.libPaths)
            {
                flags += " -L\"" + (pathOffset / path).string() + "\"";
            }

            for(auto& path : settings.systemLibs)
            {
                flags += " -l" + path.string();
            }

            for(auto& framework : settings.frameworks)
            {
                flags += " -framework " + framework;
            }

            std::unordered_map<Feature, std::string> featureMap = {
                { feature::DebugSymbols, " -g"},
            };

            for(auto& feature : settings.features)
            {
                if(feature == feature::macos::Bundle)
                {
                    sawBundleFeature = true;
                }

                auto it = featureMap.find(feature);
                if(it != featureMap.end())
                {
                    flags += it->second;
                }
            }

            for(auto& flag : settings.ext<extensions::Gcc>().linkerFlags)
            {
                flags += " " + std::string(flag);
            }

            break;
        }  
    };
    
    getFlags(project);
    if(auto it = project.archSettings.find(arch); it != project.archSettings.end())
    {
        getFlags(it->second);
    }

    if(project.type == SharedLib)
    {
        if(sawBundleFeature)
        {
            flags += " -bundle";
        }
        else
        {
            flags += " -shared";
        }
    }

    return flags;
}

std::string GccLikeToolchainProvider::getLinkerFlags(Project& project, Architecture arch, std::filesystem::path pathOffset, const std::vector<std::string>& inputs, const std::string& output) const
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

void GccLikeToolchainProvider::process(Project& project, const std::filesystem::path& workingDir, const std::filesystem::path& dataDir) const
{       
    std::filesystem::path pathOffset = std::filesystem::proximate(std::filesystem::current_path(), workingDir);

    if(project.type != Executable &&
        project.type != SharedLib &&
        project.type != StaticLib)
    {
		return;
	}

	std::set<Architecture> archs = project.architectures;
	if (archs.empty())
	{
		archs = {Architecture::current()};
	}

	std::filesystem::path finalOutput = project.output;
	if (finalOutput.empty())
	{
		throw std::runtime_error("Project '" + project.name + "' has no output set.");
	}

	std::vector<std::filesystem::path> archOutputs;

	for (auto arch : archs)
	{
		auto archMessage = archs.size() > 1 ? " (" + arch.id + ")" : "";
		const auto& gccExt = project.ext<extensions::Gcc>();
		const auto& buildPch = gccExt.pch.build;
		const auto& importPch = gccExt.pch.use;
		auto& toolchainOutputs = project.archSettings[arch].ext<extensions::internal::ToolchainOutputs>();

		// TODO: Do PCH management less hard coded, and only build PCHs for different languages if needed
		if (!buildPch.empty())
		{
			auto input = buildPch;
			auto inputStr = (pathOffset / input).string();

			auto pchPath = input.relative_path().string();
			str::replaceAllInPlace(pchPath, ":", "_");
			str::replaceAllInPlace(pchPath, "..", "__");

			auto output = dataDir / arch.id / std::filesystem::path("pch") / (pchPath + ".pch");
			auto outputStr = (pathOffset / output).string();

			auto outputObjC = dataDir / arch.id / std::filesystem::path("pch") / (pchPath + ".pchmm");
			auto outputObjCStr = (pathOffset / outputObjC).string();

			{
				CommandEntry command;
				command.command = str::quote(getCompiler(project, pathOffset, lang::Cpp)) +
				                  getCommonCompilerFlags(project, arch, pathOffset, lang::Cpp, true) +
				                  getCompilerFlags(project, arch, pathOffset, lang::Cpp, inputStr, outputStr);
				command.inputs = {input};
				command.outputs = {output};
				command.workingDirectory = workingDir;
				command.depFile = output.string() + ".d";
				command.description = "Compiling " + project.name + " PCH" + archMessage + ": " + input.string();
				project.commands += std::move(command);
			}

			// TODO: This should be dependent on if it's needed rather than what OS we're on
			if (OperatingSystem::current() == MacOS)
			{
				CommandEntry command;
				command.command = str::quote(getCompiler(project, pathOffset, lang::ObjectiveCpp)) +
				                  getCommonCompilerFlags(project, arch, pathOffset, lang::ObjectiveCpp, true) +
				                  getCompilerFlags(project, arch, pathOffset, lang::ObjectiveCpp, inputStr, outputObjCStr);
				command.inputs = {input};
				command.outputs = {outputObjC};
				command.workingDirectory = workingDir;
				command.depFile = outputObjC.string() + ".d";
				command.description = "Compiling " + project.name + " PCH (Objective-C++)" + archMessage + ": " + input.string();
				project.commands += std::move(command);
			}
		}

		std::string cppPchFlags;
		std::string objCppPchFlags;
		std::vector<std::filesystem::path> pchInputs;
		if (!importPch.empty())
		{
			auto pchPath = importPch.relative_path().string();
			str::replaceAllInPlace(pchPath, ":", "_");
			str::replaceAllInPlace(pchPath, "..", "__");

			auto input = dataDir / arch.id / std::filesystem::path("pch") / (pchPath + ".pch");
			auto inputStr = (pathOffset / input).string();
			cppPchFlags += " -Xclang -include-pch -Xclang " + inputStr;
			pchInputs.push_back(input);

			// TODO: This should be dependent on if it's needed rather than what OS we're on
			if (OperatingSystem::current() == MacOS)
			{
				auto inputObjCpp = dataDir / arch.id / std::filesystem::path("pch") / (pchPath + ".pchmm");
				auto inputObjCppStr = (pathOffset / inputObjCpp).string();
				objCppPchFlags += " -Xclang -include-pch -Xclang " + inputObjCppStr;
				pchInputs.push_back(inputObjCpp);
			}
		}

		std::unordered_map<Language, std::string, std::hash<std::string>> commonCompilerFlags;
		auto getCommonCompilerCommand = [&](Language language) -> const std::string& {
			auto it = commonCompilerFlags.find(language);
			if (it != commonCompilerFlags.end())
			{
				return it->second;
			}

			auto flags = str::quote(getCompiler(project, pathOffset, language)) +
			             getCommonCompilerFlags(project, arch, pathOffset, language, false);

			return commonCompilerFlags[language] = flags;
		};

		auto linkerCommand = str::quote(getLinker(project, pathOffset)) + getCommonLinkerFlags(project, arch, pathOffset);

		std::unordered_set<std::string> ignorePch;
		ignorePch.reserve(gccExt.pch.ignoredFiles.size());
		for (auto& file : gccExt.pch.ignoredFiles)
		{
			ignorePch.insert(file.lexically_normal().string());
		}

		std::vector<std::filesystem::path> linkerInputs;
		for (auto& input : project.files)
		{
			auto language = input.language != lang::Auto ? input.language : Language::getByPath(input.path);
			if (language == lang::None)
			{
				continue;
			}

			auto inputStr = (pathOffset / input.path).string();
			auto objPath = input.path.relative_path().string();
			str::replaceAllInPlace(objPath, ":", "_");
            str::replaceAllInPlace(objPath, "..", "__");
            auto output = dataDir / arch.id / std::filesystem::path("obj") / project.name / (objPath + (language == lang::Rc ? ".res" : ".o"));
            auto outputStr = (pathOffset / output).string();

            CommandEntry command;
            // TODO: These rsp & escaping rules needs something less hardcoded probably.
            if(language != lang::Rc)
            {
                command.command = getCommonCompilerCommand(language);
                command.rspContents = getCompilerFlags(project, arch, pathOffset, language, inputStr, outputStr);

                if(ignorePch.find(input.path.lexically_normal().string()) == ignorePch.end())
                {
                    // TODO: Do PCH management less hard coded, and only build PCHs for different languages if needed
                    if(language == lang::Cpp)
                    {
                        command.rspContents += cppPchFlags;
                    }
                    else if(language == lang::ObjectiveCpp)
                    {
                        command.rspContents += objCppPchFlags;
                    }
                }

                str::replaceAllInPlace(command.rspContents, "\\", "\\\\");
				command.rspFile = std::filesystem::absolute(output.string() + ".rsp").lexically_normal();
				command.command += " @" + str::quote(command.rspFile, '"', "\"");
				command.depFile = output.string() + ".d";
			}
            else
            {
                command.command = getCommonCompilerCommand(language) + getCompilerFlags(project, arch, pathOffset, language, inputStr, outputStr);
            }
            command.inputs = { input.path };
            command.inputs.insert(command.inputs.end(), pchInputs.begin(), pchInputs.end());
            command.outputs = { output };
            command.workingDirectory = workingDir;
            command.description = "Compiling " + project.name + archMessage + ": " + input.path.string();
            project.commands += std::move(command);

			toolchainOutputs.objectFiles += output;
			linkerInputs.push_back(output);
		}

		if (!linker.empty())
		{
			if (project.type == Executable || project.type == SharedLib)
			{
				for (auto& path : project.libs)
				{
					linkerInputs.push_back(path);
				}

				if (auto it = project.archSettings.find(arch); it != project.archSettings.end())
				{
					for (auto& path : it->second.libs)
					{
						linkerInputs.push_back(path);
					}
				}

				for (auto& dependency : project.dependencies)
				{
					const auto& dependencyOutputs = dependency->ext<extensions::internal::ToolchainOutputs>();
					for (auto& lib : dependencyOutputs.libraryFiles)
					{
						linkerInputs.push_back(lib);
					}
					if (auto it = dependency->archSettings.find(arch); it != dependency->archSettings.end())
					{
						const auto& dependencyArchOutputs = it->second.ext<extensions::internal::ToolchainOutputs>();
						for (auto& lib : dependencyArchOutputs.libraryFiles)
						{
							linkerInputs.push_back(lib);
						}
					}
				}
			}

			std::vector<std::string> linkerInputStrs;
			linkerInputStrs.reserve(linkerInputs.size());
			bool windows = OperatingSystem::current() == Windows;
			for (auto& input : linkerInputs)
			{
				if (windows && input.extension().empty())
				{
					linkerInputStrs.push_back((pathOffset / input).string() + ".");
				}
				else
				{
					linkerInputStrs.push_back((pathOffset / input).string());
				}
			}

			std::filesystem::path output;

			if (archs.size() == 1)
			{
				output = finalOutput;
			}
			else
			{
				auto tmpPath = finalOutput.relative_path().string();
				str::replaceAllInPlace(tmpPath, ":", "_");
				str::replaceAllInPlace(tmpPath, "..", "__");
				output = dataDir / arch.id / std::filesystem::path("link") / project.name / tmpPath;
			}

			std::string outputStr;
			outputStr = (pathOffset / output).string();
			archOutputs.push_back(output);

			if (archs.size() == 1)
			{
				toolchainOutputs.libraryFiles += output;
			}

			CommandEntry command;
			command.command = linkerCommand;
			// TODO: ar on macOS doesn't support rsp files. This should not be hardcoded,
			// the capabilities of the toolchain should be queried one way or another.
			if (OperatingSystem::current() != MacOS)
			{
				command.rspContents = getLinkerFlags(project, arch, pathOffset, linkerInputStrs, outputStr);
				str::replaceAllInPlace(command.rspContents, "\\", "\\\\");
				command.rspFile = std::filesystem::absolute(output.string() + ".rsp").lexically_normal();
				command.command += " @" + str::quote(command.rspFile, '"', "\"");
			}
			else
			{
				command.command += getLinkerFlags(project, arch, pathOffset, linkerInputStrs, outputStr);
			}
			command.inputs = std::move(linkerInputs);
			command.outputs = {output};
			command.workingDirectory = workingDir;
			command.description = "Linking " + project.name + archMessage + ": " + output.string();
			// ar just adds stuff to existing files, so we need to clean it ourselves first.
			if (project.type == StaticLib)
			{
				auto removeCommand = commands::remove(pathOffset / output);
				removeCommand.workingDirectory = workingDir;
				command = commands::chain({removeCommand, command}, command.description);
			}
			project.commands += std::move(command);
		}
	}

	if (archs.size() > 1)
	{
		auto& toolchainOutputs = project.ext<extensions::internal::ToolchainOutputs>();
		toolchainOutputs.libraryFiles += finalOutput;

		// TODO: Make lipo part of the toolchain conf?
		CommandEntry command;
		command.inputs = archOutputs;
		command.outputs = {finalOutput};
		command.workingDirectory = workingDir;
		command.command = "lipo -create";
		for (auto input : command.inputs)
		{
			command.command += " " + str::quote((pathOffset / input).string());
		}
		command.command += " -output ";
		command.command += " " + str::quote((pathOffset / finalOutput).string());
		command.description = "Linking " + project.name + ": " + finalOutput.string();
		project.commands += std::move(command);
	}
}