#pragma once

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/stringid.h"
#include "core/emitter.h"
#include "core/project.h"
#include "core/stringid.h"
#include "modules/command.h"
#include "modules/postprocess.h"
#include "modules/toolchain.h"
#include "toolchains/detected.h"
#include "util/operators.h"
#include "util/process.h"
#include "util/file.h"
#include "util/string.h"

class CompileCommands : public Emitter
{
public:
    static CompileCommands instance;

    CompileCommands()
        : Emitter("compilecommands")
    {
    }

    virtual void emit(const EmitterArgs& args) override
    {
        std::filesystem::create_directories(args.targetPath);
        std::ofstream stream(args.targetPath / "compile_commands.json");
        
        stream << "[\n";

        Project generator = createGeneratorProject();
        emitCommands(stream, args.targetPath, generator, args.config, true);

        auto projects = Emitter::discoverProjects(args.projects);
        for(auto project : projects)
        {
            emitCommands(stream, args.targetPath, *project, args.config, false);
        }
        
        stream << "\n]\n";
    }

private:
    static void emitCommands(std::ostream& stream, const std::filesystem::path& root, Project& project, StringId config, bool first)
    {
        auto resolved = project.resolve(config, OperatingSystem::current());

        {
            // Avoiding range-based for loop here since it breaks
            // if a post processor adds more post processors. 
            auto postProcessors = resolved[PostProcess];
            for(size_t i = 0; i < postProcessors.size(); ++i)
            {
                postProcessors[i](project, resolved);
            }
        }

        if(!project.type.has_value())
        {
            return;
        }

        if(project.name.empty())
        {
            throw std::runtime_error("Trying to build project with no name.");
        }

        auto& commands = resolved[Commands];
        if(project.type == Command && commands.empty())
        {
            throw std::runtime_error("Command project '" + project.name + "' has no commands.");
        }

        const ToolchainProvider* toolchain = resolved[Toolchain];
        if(!toolchain)
        {
            toolchain = defaultToolchain;
        }

        auto toolchainOutputs = toolchain->process(project, resolved, config, {});

        auto absCwd = std::filesystem::absolute(std::filesystem::current_path());
        for(auto& command : commands)
        {
            if(command.inputs.empty())
            {
                continue;
            }

            std::filesystem::path cwd = absCwd / command.workingDirectory;

            // Assuming first input is the main input
            if(!first)
            {
                stream << ",\n";
            }
            first = false;
            stream << "  {\n";
            stream << "    \"directory\": " << cwd << ",\n";
            stream << "    \"file\": " << command.inputs.front() << ",\n";
            stream << "    \"command\": " << str::quote(command.command) << "\n";
            stream << "  }";
        }
    }
};

CompileCommands CompileCommands::instance;
