#include "buildconfigurator.h"
#include "core/environment.h"
#include "modules/toolchain.h"
#include "util/commands.h"
#include "actions/direct.h"
#include "actions/configure.h"
#include "fileutil.h"
#include "dependencyparser.h"
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
    , _updateDependencies(false)
{
    dataPath = *targetPath;

    std::vector<std::string> args;
    if(updateExisting)
    {
        auto previousCommandLine = readFile(*targetPath / ".generator/configure.cmdline");
        args = str::splitAll(previousCommandLine, '\n');
#if DEBUG_LOG
        std::cout << "using previous: " << str::join(args, " ") << std::endl;
#endif
    }
    else
    {
        args = cliContext.allArguments;
#if DEBUG_LOG
        std::cout << "using actual: " << str::join(args, " ") << std::endl;
#endif
    }

    cli::Context configureContext(cliContext.startPath, cliContext.invocation, args);

    std::filesystem::current_path(configureContext.configurationFile.parent_path());
    bool configDirty = checkDependencies(configureContext, *targetPath / ".generator/configure");

    _databasePath = dataPath / ".build_db";
    // TODO: Loading the file dependencies would be enough if the config is dirty anyway
    if (!database.load(_databasePath))
    {
        configDirty = true;
    }

    if(configDirty)
    {
        std::cout << "Reconfiguring " << (*targetPath).string() << "..." << std::endl;

        Environment env = configureEnvironment(configureContext);

        std::vector<CommandEntry> commands;
        for(auto& project : env.projects)
        {
            collectCommands(env, commands, dataPath, *project);
        }
        database.setCommands(std::move(commands));

        std::ofstream compileCommandsStream(*targetPath / "compile_commands.json");
        generateCompileCommandsJson(compileCommandsStream, database);
        env.addConfigurationDependency(*targetPath / "compile_commands.json");

        _updateDependencies = true;
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

    if(_updateDependencies)
    {
        writeDependencies(*targetPath / ".generator/configure");
    }
}

bool BuildConfigurator::checkDependencies(cli::Context& cliContext, std::filesystem::path cachePath)
{
    std::string argumentString = str::join(cliContext.allArguments, "\n");

    auto cmdFilePath = cachePath.string() + ".cmdline";
    Environment::addConfigurationDependency(cmdFilePath);
    auto depFilePath{cachePath.string() + ".confdeps"};
    Environment::addConfigurationDependency(depFilePath);

    if(writeFile(cmdFilePath, argumentString))
    {
#if DEBUG_LOG
        std::cout << "differing command line\n";
#endif
        return true;
    }

    auto depData = readFile(depFilePath);
    if(depData.empty())
    {
#if DEBUG_LOG
        std::cout << "no depdata\n";
#endif
        return true;
    }
    else
    {
        std::error_code ec;
        auto outputTime = std::filesystem::last_write_time(depFilePath, ec);
        bool dirty = parseDependencyData(depData, [outputTime](std::string_view path){
            std::error_code ec;
            auto time = std::filesystem::last_write_time(path, ec);
#if DEBUG_LOG
            if(ec)
            {
                std::cout << path << " error\n";
            }
            if(time > outputTime)
            {
                std::cout << path << " dirty\n";
            }
#endif
            return ec || time > outputTime;
        });
        if(dirty)
        {
            return true;
        }
    }

    return false;
}

void BuildConfigurator::writeDependencies(std::filesystem::path cachePath)
{
    std::stringstream depData;
    depData << ":\n";
    for(auto& dep : Environment::configurationDependencies)
    {
        depData << "  " << str::replaceAll(dep.string(), " ", "\\ ") << " \\\n";
    }

    auto depFilePath{cachePath.string() + ".confdeps"};
    writeFile(depFilePath, depData.str(), false);
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
