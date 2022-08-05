#include "emitters/compilecommands.h"
CompileCommands::CompileCommands()
    : Emitter("compilecommands", "Generate a compilecommands.json file.")
{ }

void CompileCommands::emit(Environment& env)
{
    std::filesystem::create_directories(*targetPath.value);
    std::ofstream stream(*targetPath.value / "compile_commands.json");
    
    stream << "[\n";

#if TODO
    auto [generator, buildOutput] = createGeneratorProject(targetPath);
    emitCommands(stream, targetPath, generator, "", true);
#endif

    auto projects = env.collectProjects();
    auto configs = env.collectConfigs();
    for(auto config : configs)
    {
        for(auto project : projects)
        {
            emitCommands(stream, *targetPath.value, *project, config, false);
        }
    }
    
    stream << "\n]\n";
}

void CompileCommands::emitCommands(std::ostream& stream, const std::filesystem::path& root, Project& project, StringId config, bool first)
{
    auto resolved = project.resolve(config, OperatingSystem::current());

    if(!project.type.has_value())
    {
        return;
    }

    if(project.name.empty())
    {
        throw std::runtime_error("Trying to build project with no name.");
    }

    auto& commands = resolved.commands;
    if(project.type == Command && commands.value().empty())
    {
        throw std::runtime_error("Command project '" + project.name + "' has no commands.");
    }

    const ToolchainProvider* toolchain = resolved.toolchain;
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

CompileCommands CompileCommands::instance;
