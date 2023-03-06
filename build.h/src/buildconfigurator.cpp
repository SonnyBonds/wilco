#include "buildconfigurator.h"
#include "core/environment.h"
#include "modules/toolchain.h"
#include "util/commands.h"
#include "actions/direct.h"

void BuildConfigurator::collectCommands(Environment& env, std::vector<CommandEntry>& collectedCommands, const std::filesystem::path& projectDir, Project& project)
{
    std::filesystem::path dataDir = project.dataDir;
    if(dataDir.empty())
    {
        dataDir = projectDir;
    }

    if(project.name.empty())
    {
        throw std::runtime_error("Trying to build project with no name.");
    }

    std::filesystem::create_directories(dataDir);
    std::filesystem::path pathOffset = std::filesystem::proximate(std::filesystem::current_path(), dataDir);

    auto& commands = project.commands;
    if(project.type == Command && commands.empty())
    {
        throw std::runtime_error("Command project '" + project.name + "' has no commands.");
    }

    {
        std::filesystem::path output = project.output;
        if(output.has_parent_path())
        {
            std::filesystem::create_directories(output.parent_path());
        }
    }

    const ToolchainProvider* toolchain = project.toolchain;
    if(!toolchain)
    {
        toolchain = defaultToolchain;
    }

    auto toolchainOutputs = toolchain->process(project, {}, dataDir);

    collectedCommands.reserve(collectedCommands.size() + commands.size());
    for(auto& command : commands)
    {
        CommandEntry adjustedCommand = command;

        std::filesystem::path cwd = command.workingDirectory;
        if(cwd.empty())
        {
            cwd = ".";
        }
        std::string cwdStr = cwd.string();
        adjustedCommand.command = "cd \"" + cwdStr + "\" && " + command.command;
        collectedCommands.push_back(std::move(adjustedCommand));
    }
}

BuildConfigurator::BuildConfigurator(Environment& env, bool verboser)
    : _dependencyChecker(env, *targetPath / ".generator/configure")
{
    dataPath = *targetPath;

    bool reconfigure = false;
    _databasePath = dataPath / ".build_db";
    if(_dependencyChecker.isDirty() || !database.load(_databasePath))
    {
        std::cout << "Reconfiguring " << (*targetPath).string() << "...\n";

        configure(env);

        std::vector<CommandEntry> commands;
        for(auto& project : env.projects)
        {
            collectCommands(env, commands, dataPath, *project);
        }
        database.setCommands(std::move(commands));
        std::cout << "Done.\n";
    }
    else
    {
        if(verboser)
        {
            std::cout << "Configuration in " << (*targetPath).string() << " is up to date\n";
        }
    }
}

BuildConfigurator::~BuildConfigurator()
{
    if(!_databasePath.empty())
    {
        database.save(_databasePath);
    }
}
