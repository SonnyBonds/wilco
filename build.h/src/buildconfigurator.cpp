#include "buildconfigurator.h"
#include "core/environment.h"
#include "modules/toolchain.h"
#include "util/commands.h"
#include "actions/direct.h"
#include "actions/configure.h"
#include "fileutil.h"
#include "dependencyparser.h"
#include "commandprocessor.h"
#include <sstream>
#include <fstream>

#define DEBUG_LOG 0

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

static void generateCompileCommandsJson(std::ostream& stream, const Database& database)
{
    auto absCwd = std::filesystem::absolute(std::filesystem::current_path());
    bool first = true;
    stream << "[\n";
    for(auto& command : database.getCommands())
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
    stream << "\n]";
}

BuildConfigurator::BuildConfigurator(cli::Context cliContext, bool updateExisting)
    : cliContext(std::move(cliContext))
{
    dataPath = *targetPath;

    _configDatabasePath = dataPath / ".config_db";
    bool configDirty = !configDatabase.load(_configDatabasePath);

    std::vector<std::string> args;
    if(updateExisting)
    {
        if(!configDatabase.getCommands().empty())
        {
            args = str::splitAll(configDatabase.getCommands()[0].command, '\n');
        }
        else
        {
            configDirty = true;
        }
    }
    else
    {
        args = cliContext.allArguments;
        if(configDatabase.getCommands().empty() || configDatabase.getCommands()[0].command != str::join(args, "\n"))
        {
            configDirty = true;
        }
    }

    if(!configDirty)
    {
        auto configCommands = filterCommands(cliContext.startPath, configDatabase, dataPath, {});
        configDirty = !configCommands.empty();
    }

    _databasePath = dataPath / ".build_db";
    if (!database.load(_databasePath))
    {
        configDirty = true;
    }

    std::filesystem::current_path(cliContext.configurationFile.parent_path());

    if(configDirty)
    {
        std::cout << "Reconfiguring " << (*targetPath).string() << "..." << std::endl;

        cli::Context configureContext(cliContext.startPath, cliContext.invocation, args);
        Environment env = configureEnvironment(configureContext);

        std::vector<CommandEntry> commands;
        for(auto& project : env.projects)
        {
            collectCommands(env, commands, dataPath, *project);
        }
        database.setCommands(std::move(commands));

        std::stringstream compileCommandsStream;
        generateCompileCommandsJson(compileCommandsStream, database);
        env.writeFile(*targetPath / "compile_commands.json", compileCommandsStream.str());

        updateConfigDatabase(configDatabase, args);

        std::cout << "Done.\n";
    }
    else
    {
        if(!updateExisting)
        {
            std::cout << "Configuration in " << (*targetPath).string() << " is up to date\n";
        }
    }
}

BuildConfigurator::~BuildConfigurator()
{
    std::filesystem::current_path(cliContext.startPath);

    if(!_databasePath.empty())
    {
        database.save(_databasePath);
    }

    if(!_configDatabasePath.empty())
    {
        configDatabase.save(_configDatabasePath);
    }
}

void BuildConfigurator::updateConfigDatabase(Database& database, const std::vector<std::string>& args)
{
    // This command is never meant to be executed as an actual shell command, but uses the same
    // mechanics for dependency checking.
    CommandEntry configCommand;
    configCommand.description = "Configure";
    configCommand.command = str::join(args, "\n");
    configCommand.inputs.reserve(Environment::configurationDependencies.size());
    for(auto& input : Environment::configurationDependencies)
    {
        configCommand.inputs.push_back(input);
    }
    database.setCommands({configCommand});
    database.getCommandSignatures()[0] = computeCommandSignature(database.getCommands()[0]);

    // Recompute all input signatures. If a file has changed _while_ the configuration
    // is running, those changes will not trigger a new run. Maybe there is a better
    // scheme for this.
    for(auto& input : database.getFileDependencies())
    {
        input.signature = computeFileSignature(input.path);
    }
}

Environment BuildConfigurator::configureEnvironment(cli::Context &cliContext)
{
    // TODO: This coupling and the responsibility for expanding profiles
    // is a bit weird.
    Configure configureAction;
    cliContext.extractArguments(configureAction.arguments);
    cliContext.extractArguments(cli::Argument::globalList());
    cliContext.requireAllArgumentsUsed();

    Environment env(cliContext);
    std::filesystem::current_path(cliContext.configurationFile.parent_path());
    configure(env);
    return env;
}
