#include "buildconfigurator.h"
#include "core/environment.h"
#include "modules/toolchain.h"
#include "actions/direct.h"
#include "actions/configure.h"
#include "fileutil.h"
#include "commandprocessor.h"
#include "util/string.h"
#include <algorithm>
#include <iostream>
#include <memory>
#include <sstream>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

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

    const ToolchainProvider* toolchain = project.toolchain;
    if(!toolchain)
    {
        toolchain = defaultToolchain;
    }

	toolchain->process(project, {}, dataDir);

	if (project.type == Command && project.commands.empty())
	{
		throw std::runtime_error("Command project '" + project.name + "' has no commands.");
	}

	collectedCommands.reserve(collectedCommands.size() + project.commands.size() + 1);
	for (auto& command : project.commands)
	{
		if (command.command.empty() && !command.outputs.empty())
		{
			throw std::runtime_error("Command '" + command.description + "' in project " + project.name + " has outputs but no actual command to produce them.");
		}
		if (!command.rspContents.empty() && command.rspFile.empty())
		{
			throw std::runtime_error("Command '" + command.description + "' in project " + project.name + " has rspContents set but no corresponding rspFile.");
		}
		if (!command.rspFile.empty() && command.command.find("@" + str::quote(command.rspFile)) == std::string::npos)
		{
			throw std::runtime_error("Command '" + command.description + "' in project " + project.name + " has rspFile set but no corresponding @rspFile in the command string. Currently @\"quoted/path\" is the only supported rsp file flag format, and path must be lexically identical to rspFile.");
		}
		collectedCommands.push_back(command);
	}

	CommandEntry phonyProjectCommand;
	std::set<std::filesystem::path> inputs;
	std::set<std::filesystem::path> outputs;
	for (auto& command : collectedCommands)
	{
		inputs.insert(command.inputs.begin(), command.inputs.end());
		outputs.insert(command.outputs.begin(), command.outputs.end());
	}

	// List all outputs that haven't been consumed within the project (e.g. intermediate obj files)
	// as inputs to a phony command that can be used for partial builds.
	// We could list all outputs, but this makes the resulting command a bit cleaner.
	std::set_difference(outputs.begin(), outputs.end(), inputs.begin(), inputs.end(), std::back_inserter(phonyProjectCommand.inputs));
	phonyProjectCommand.description = project.name;
    collectedCommands.push_back(std::move(phonyProjectCommand));
}

static void generateCompileCommandsJson(std::ostream& stream, const Database& database)
{
    auto absCwd = std::filesystem::absolute(std::filesystem::current_path());
    bool first = true;
    stream << "[\n";

    auto writeCommands = [&](const std::vector<CommandEntry>& commands)
    {
        for(auto& command : commands)
        {
			if (command.command.empty() || command.inputs.empty())
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
			if (command.rspFile.empty())
			{
				stream << "    \"command\": " << str::quote(command.command) << "\n";
			}
			else
			{
				std::string rspFlag = "@" + str::quote(command.rspFile);
				// Expand rsp contents since the rsp files are transient and won't be
				// available to whatever consumes the compile commands file.
				stream << "    \"command\": " << str::quote(str::replaceAll(command.command, rspFlag, command.rspContents)) << "\n";
			}
			stream << "  }";
		}
    };

    writeCommands(database.getCommands());

    // TODO: Get these commands some nicer way maybe
    Database selfBuildDatabase;
    selfBuildDatabase.load(DirectBuilder::getSelfBuildDatabasePath());
    writeCommands(selfBuildDatabase.getCommands());

    stream << "\n]";
}

BuildConfigurator::BuildConfigurator(cli::Context cliContext, bool useExisting)
    : cliContext(std::move(cliContext))
{
    dataPath = *targetPath;

    _configDatabasePath = dataPath / ".config_db";
    bool hasDatabase = configDatabase.load(_configDatabasePath);
    if(useExisting && !hasDatabase)
    {
        throw std::runtime_error("No configuration found in " + str::quote(dataPath.string()) + ". Run \"wilco configure\" first.");
    }

    bool configDirty = !hasDatabase;

    std::vector<std::string> args;
    auto previousArgs = getPreviousConfigDatabaseArguments(configDatabase);
    if(useExisting)
    {
        if(!previousArgs)
        {
            throw std::runtime_error("Configuration in " + str::quote(dataPath.string()) + " seems corrupted. Run \"wilco configure\".");
        }
        args = *previousArgs;
    }
    else
    {
        args = cliContext.allArguments;
        if(!previousArgs || args != *previousArgs)
        {
            configDirty = true;
        }
    }

    if(!configDirty)
    {
        auto configCommands = filterCommands(configDatabase, cliContext.startPath, {});
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
        std::cout << (hasDatabase ? "Reconfiguring " : "Configuring ") << (*targetPath).string() << "..." << std::endl;

        cli::Context configureContext(cliContext.startPath, cliContext.invocation, args);
        Environment env = configureEnvironment(configureContext);

		std::vector<CommandEntry> commands;
		for (auto& project : env.projects)
		{
			collectCommands(env, commands, dataPath, *project);
		}
		database.setCommands(std::move(commands));

		std::stringstream compileCommandsStream;
		generateCompileCommandsJson(compileCommandsStream, database);
		env.writeFile(*targetPath / "compile_commands.json", compileCommandsStream.str());

		updateConfigDatabase(env.configurationDependencies, configDatabase, args);

		std::cout << "Done.\n";
	}
	else
	{
		if (!useExisting)
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

std::optional<std::vector<std::string>> BuildConfigurator::getPreviousConfigDatabaseArguments(const Database& database)
{
    if(database.getCommands().empty())
    {
        return {};
    }

    auto args = str::splitAll(database.getCommands()[0].command, '\n');
    if(args.empty() || args[0] != "wilco")
    {
        return {};
    }

    args.erase(args.begin());
    return args;
}

void BuildConfigurator::updateConfigDatabase(std::set<std::filesystem::path> configDependencies, Database& database, const std::vector<std::string>& args)
{
    // This command is never meant to be executed as an actual shell command, but uses the same
    // mechanics for dependency checking.
    CommandEntry configCommand;
    configCommand.description = "Configure";
    // The "wilco" bogus command is needed because empty commands get filtered out
    configCommand.command = "wilco";
    if(!args.empty())
    {
        configCommand.command += "\n" + str::join(args, "\n");
    }
    configCommand.inputs.reserve(configDependencies.size());
    for(auto& input : configDependencies)
    {
        configCommand.inputs.push_back(std::move(input));
    }
    configCommand.inputs.push_back(process::findCurrentModulePath());

    database.setCommands({configCommand});
    database.getCommandSignatures()[0] = computeCommandSignature(database.getCommands()[0]);

    // Recompute all input signatures. If a file has changed _while_ the configuration
    // is running, those changes will not trigger a new run. Maybe there is a better
    // scheme for this.
    for(auto& input : database.getFileDependencies())
    {
        updatePathSignature(input.signaturePair, input.path);
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

	// Reorder the projects list so that dependencies are guaranteed to be
	// before their dependent projects.

	std::unordered_map<const Project*, size_t> projectIndices;
	std::unordered_set<size_t> addedIndices;
	for (size_t i = 0; i < env.projects.size(); ++i)
	{
		projectIndices.insert({env.projects[i].get(), i});
	}
	std::vector<size_t> sortedIndices;
	auto pushProject = [&](size_t index) {
		if (addedIndices.find(index) == addedIndices.end())
		{
			sortedIndices.push_back(index);
			addedIndices.insert(index);
		}
	};
	for (size_t i = 0; i < env.projects.size(); ++i)
	{
		for (auto dependency : env.projects[i]->dependencies)
		{
			pushProject(projectIndices[dependency]);
		}
		pushProject(i);
	}

	std::vector<std::unique_ptr<Project>> sortedProjects;
	for (auto index : sortedIndices)
	{
		sortedProjects.emplace_back(std::move(env.projects[index]));
	}

	swap(env.projects, sortedProjects);

	return env;
}
