#include "emitters/compilecommands.h"

CompileCommands::CompileCommands()
    : Emitter("compilecommands", "Generate a compilecommands.json file.")
{ }

void CompileCommands::emit(Environment& env)
{
    std::filesystem::create_directories(*targetPath.value);
    std::ofstream stream(*targetPath.value / "compile_commands.json");
    
    stream << "[\n";

    auto projects = env.collectProjects();
    auto configs = env.configurations;
    bool first = true;
    for(auto config : configs)
    {
        for(auto project : projects)
        {
            emitCommands(env, stream, *targetPath.value, *project, config, first);
        }
    }
    
    stream << "\n]\n";
}

void CompileCommands::emitCommands(Environment& env, std::ostream& stream, const std::filesystem::path& projectDir, Project& project, StringId config, bool& first)
{
    if(project.name.empty())
    {
        throw std::runtime_error("Trying to build project with no name.");
    }

    auto dataDir = project.dataDir(config).value();
    if(dataDir.empty())
    {
        dataDir = projectDir;
    }

    auto& commands = project.commands(config);
    if(project.type == Command && commands.empty())
    {
        throw std::runtime_error("Command project '" + project.name + "' has no commands.");
    }

    const ToolchainProvider* toolchain = project.toolchain(config);
    if(!toolchain)
    {
        toolchain = defaultToolchain;
    }

    auto toolchainOutputs = toolchain->process(project, config, {}, dataDir);

    auto absCwd = std::filesystem::absolute(std::filesystem::current_path());
    for(auto& command : commands)
    {
        if(command.inputs.empty())
        {
            continue;
        }

        std::filesystem::path cwd = absCwd / command.workingDirectory;

        if(!first)
        {
            stream << ",\n";
        }
        first = false;
        stream << "  {\n";
        stream << "    \"directory\": " << cwd << ",\n";
        // Assuming first input is the main input
        stream << "    \"file\": " << command.inputs.front() << ",\n";
        stream << "    \"command\": " << str::quote(command.command) << "\n";
        stream << "  }";
    }
}

EmitterInstance<CompileCommands> CompileCommands::instance;
