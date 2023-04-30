#include "actions/direct.h"
#include "dependencyparser.h"
#include "fileutil.h"
#include "util/commands.h"
#include "database.h"
#include "buildconfigurator.h"
#include <iostream>
#include <chrono>
#include "util/hash.h"
#include "util/interrupt.h"
#include "commandprocessor.h"

static const int EXIT_RESTART = 10;

DirectBuilder::TargetArgument::TargetArgument(std::vector<cli::Argument*>& argumentList)
{
    this->example = "[targets]";
    this->description = "Build specific targets. [default:all]";

    argumentList.push_back(this);
}

void DirectBuilder::TargetArgument::extract(std::vector<std::string>& inputValues)
{
    auto it = inputValues.begin();
    while(it != inputValues.end())
    {
        if(it->size() > 2 &&
           it->substr(0, 2) == "--")
        {
            ++it;
            continue;            
        }
        values.push_back(std::move(*it));
        inputValues.erase(it);
    }
}

void DirectBuilder::TargetArgument::reset()
{
    values.clear();
}

DirectBuilder::DirectBuilder()
    : Action("build", "Build output binaries.")
{ }

void DirectBuilder::run(cli::Context cliContext)
{
    cliContext.extractArguments(arguments);
    
    BuildConfigurator configurator(cliContext);

    auto filteredCommands = filterCommands(cliContext.startPath, configurator.database, configurator.dataPath, targets.values);

    if(filteredCommands.empty())
    {
        std::cout << "Nothing to do. (Everything up to date.)\n" << std::flush;
    }
    else
    {
        size_t maxConcurrentCommands = std::max((size_t)1, (size_t)std::thread::hardware_concurrency());
        std::cout << "Building using " << maxConcurrentCommands << " concurrent tasks.";
        size_t completedCommands = runCommands(filteredCommands, configurator.database, maxConcurrentCommands, verbose.value);

        std::cout << "\n" << std::to_string(completedCommands) << " of " << filteredCommands.size() << " targets rebuilt.\n" << std::flush;

        // TODO: Error exit code on failure
    }
}

void DirectBuilder::buildSelf(cli::Context cliContext)
{
    Environment env(cliContext);

    auto isSubProcess = false;
    {
        auto rebuildIt = std::find(cliContext.unusedArguments.begin(), cliContext.unusedArguments.end(), "--internal-restart");
        if(rebuildIt != cliContext.unusedArguments.end())
        {
            isSubProcess = true;
            cliContext.unusedArguments.erase(rebuildIt);
        }
    }


    auto outputPath = *targetPath / ".generator";
    std::string ext;
    if(OperatingSystem::current() == Windows)
    {
        ext = ".exe";
    }

    auto tempPath = outputPath / std::filesystem::path(cliContext.configurationFile).filename().replace_extension(ext + (isSubProcess ? ".running_sub" : ".running"));
    auto buildOutput = std::filesystem::path(cliContext.configurationFile).replace_extension(ext);
    
    auto buildHDir = std::filesystem::absolute(__FILE__).parent_path().parent_path();

    Project& project = env.createProject("Generator", Executable);
    project.features += { 
        feature::Cpp17, 
        feature::DebugSymbols, 
        feature::Exceptions, 
        feature::Optimize,
        feature::msvc::SharedRuntime
    };
    project.ext<extensions::Gcc>().compilerFlags += "-static";
    project.ext<extensions::Gcc>().linkerFlags += "-static";
    project.includePaths += buildHDir;
    project.output = buildOutput;
    project.files += cliContext.configurationFile;
    project.files += env.listFiles(buildHDir / "src");

    for(auto& file : project.files)
    {
        env.addConfigurationDependency(file.path);
    }
    env.addConfigurationDependency(buildOutput);

    Database database;
    auto databasePath = outputPath / ".build_db";
    database.load(databasePath);
    {
        std::vector<CommandEntry> commands;
        BuildConfigurator::collectCommands(env, commands, outputPath, project);
        database.setCommands(std::move(commands));
    }

    auto filteredCommands = filterCommands(cliContext.startPath, database, outputPath, {});

    // If nothing is to be done...
    if(filteredCommands.empty())
    {
        // Exit with OK if we're in a subprocess
        if(isSubProcess)
        {
            std::exit(0);
        }
        // ...and just continue otherwise.
        return;
    }

    // Something has changed, so we rebuild
    std::cout << "\nRebuilding generator." << std::flush;

    // ...but first we need to move ourselves out of the way.
    std::filesystem::rename(buildOutput, tempPath);

    try
    {
        size_t maxConcurrentCommands = std::max((size_t)1, (size_t)std::thread::hardware_concurrency());
        size_t completedCommands = runCommands(filteredCommands, database, maxConcurrentCommands, false);

        database.save(databasePath);

        // Exit with failure if it fails.
        if(completedCommands < filteredCommands.size())
        {
            
            // (and return the running binary since we didn't get a working new one)
            std::filesystem::rename(tempPath, buildOutput);
            std::exit(EXIT_FAILURE);
        }
    } catch(...)
    {
        // In case of emergency, return the running binary with exception-less error handling
        std::error_code ec;
        std::filesystem::rename(tempPath, buildOutput, ec);

        database.save(databasePath);
        throw;
    }

    // If it didn't fail, but we're in a subprocess, we exit with a code signalling
    // that we're good, but want to go again.
    if(isSubProcess)
    {
        std::exit(EXIT_RESTART);
    }

    // If we've gotten this far, we've rebuilt and aren't in a subprocess already.
    // Run the built result until it says there are no changes.
    std::string argumentString;
    for(auto& arg : cliContext.allArguments)
    {
        argumentString += " " + str::quote(arg);
    }

    std::string restartCommandLine = "cd " + str::quote(cliContext.startPath.string()) + " && " + str::quote((cliContext.configurationFile.parent_path() / buildOutput).string());
    restartCommandLine += argumentString;
    restartCommandLine += " --internal-restart";

    int iterations = 0;
    while(true)
    {
        if(iterations >= 10)
        {
            throw std::runtime_error("Stuck rebuilding the build configuration more than 10 times, which seems wrong.");
        }
        auto result = process::run(restartCommandLine, true);
        if(result.exitCode == 0)
        {
            break;
        }
        else if(result.exitCode != EXIT_RESTART)
        {
            std::exit(result.exitCode);
        }
        iterations++;
    }

    std::string buildCommandLine = "cd " + str::quote(cliContext.startPath.string()) + " && " + str::quote((cliContext.configurationFile.parent_path() / buildOutput).string());
    buildCommandLine += argumentString;

    auto result = process::run(buildCommandLine, true);
    std::exit(result.exitCode);
}


ActionInstance<DirectBuilder> DirectBuilder::instance;
