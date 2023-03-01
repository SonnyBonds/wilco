#include "emitters/compilecommands.h"

CompileCommands::CompileCommands()
    : Emitter("compilecommands", "Generate a compilecommands.json file.")
{ }

void CompileCommands::emit(Environment& env)
{
    ConfigDependencyChecker configChecker(env, *targetPath / ".generator/configure");
    if(!configChecker.isDirty())
    {
        return;
    }

    std::filesystem::create_directories(*targetPath.value);
    std::ofstream stream(*targetPath.value / "compile_commands.json");
    
    stream << "[\n";

    bool first = true;
    configure(env);

    for(auto& project : env.projects)
    {
        emitCommands(env, stream, *targetPath.value, *project, first);
    }
    
    stream << "\n]\n";
}

void CompileCommands::emitCommands(Environment& env, std::ostream& stream, const std::filesystem::path& projectDir, Project& project, bool& first)
{
    if(project.name.empty())
    {
        throw std::runtime_error("Trying to build project with no name.");
    }

    auto dataDir = project.dataDir;
    if(dataDir.empty())
    {
        dataDir = projectDir;
    }

    auto& commands = project.commands;
    if(project.type == Command && commands.empty())
    {
        throw std::runtime_error("Command project '" + project.name + "' has no commands.");
    }

    const ToolchainProvider* toolchain = project.toolchain;
    if(!toolchain)
    {
        toolchain = defaultToolchain;
    }

    auto toolchainOutputs = toolchain->process(project, {}, dataDir);

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
