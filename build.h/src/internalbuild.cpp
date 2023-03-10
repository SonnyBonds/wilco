#include "actions/direct.h"
#include "dependencyparser.h"
#include "fileutil.h"
#include "util/commands.h"
#include "database.h"
#include "buildconfigurator.h"
#include <iostream>
#include <chrono>

static const int EXIT_RESTART = 10;

#define LOG_DIRTY_REASON 0

struct TimeCache
{
public:
    std::filesystem::file_time_type get(StringId path, std::error_code& errorCode)
    {
        auto it = _times.find(path);
        if(it != _times.end())
        {
            errorCode = it->second.second;
            return it->second.first;
        }

        auto time = std::filesystem::last_write_time(path.cstr(), errorCode);
        _times.insert(std::make_pair(path, std::make_pair(time, errorCode)));
        return time;
    }
private:        
    std::unordered_map<StringId, std::pair<std::filesystem::file_time_type, std::error_code>> _times;
};

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

struct PendingCommand
{
    uint32_t command;
    bool dirty = false;
    bool included = false;
    std::future<process::ProcessResult> result;
};

static size_t runCommands(std::vector<PendingCommand>& filteredCommands, Database& database, size_t maxConcurrentCommands, bool verbose)
{
    const auto& commandDefinitions = database.getCommands();
    const auto& dependencies = database.getDependencies();

    // Not loving this, but since the dependency map are indices
    // in the unfiltered commands we need the full list
    // TODO: Test if a mapping table is faster or not
    std::vector<bool> commandCompleted;
    commandCompleted.resize(database.getCommands().size(), true);
    for(auto& filteredCommands : filteredCommands)
    {
        commandCompleted[filteredCommands.command] = false;
    }

    size_t count = 0;
    size_t completed = 0;
    size_t firstPending = 0;
    std::vector<PendingCommand*> runningCommands;
    bool halt = false;
    std::mutex doneMutex;
    std::vector<PendingCommand*> doneCommands;
    while((!halt && firstPending < filteredCommands.size()) || !runningCommands.empty())
    {
        // TODO: Semaphore of some kind instead of semi-spin lock
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(10ms);

        {
            std::scoped_lock doneLock(doneMutex);
            for(auto it = doneCommands.begin(); it != doneCommands.end(); )
            {
                auto command = *it;
                commandCompleted[command->command] = true;

                auto result = command->result.get();
                auto output = str::trim(std::string_view(result.output));
                if(!output.empty())
                {
                    std::cout << "\n" << output;
                }
                if(result.exitCode != 0)
                {
                    std::cout << "\nCommand returned " + std::to_string(result.exitCode);
                    halt = true;
                }
                else
                {
                    ++completed;
                }
                it = doneCommands.erase(it);

                auto runIt = std::find(runningCommands.begin(), runningCommands.end(), command);
                assert(runIt != runningCommands.end());
                if(runIt != runningCommands.end())
                {
                    runningCommands.erase(runIt);
                }
                else
                {
                    throw std::runtime_error("Internal error. (Completed command not found in running list.)");
                }

                std::cout << std::flush;
            }
        }

        if(halt)
        {
            continue;
        }

        bool skipped = false;
        for(size_t i = firstPending; i < filteredCommands.size(); ++i)
        {
            if(runningCommands.size() >= maxConcurrentCommands)
            {
                break;
            }

            auto& command = filteredCommands[i];
            if(!commandCompleted[command.command] && !command.result.valid())
            {
                bool ready = true;
                for(auto dependency : dependencies[command.command])
                {
                    if(!commandCompleted[dependency])
                    {
                        ready = false;
                        break;
                    }
                }

                if(!ready)
                {
                    skipped = true;
                    continue;
                }

                auto& commandDefinition = commandDefinitions[command.command];
                std::cout << "\n["/*"\33[2K\r["*/ << (++count) << "/" << filteredCommands.size() << "] " << commandDefinition.description << std::flush;
                if(verbose)
                {
                    std::cout << "\n" << commandDefinition.command << "\n";
                    if(!commandDefinition.rspFile.empty())
                    {
                        std::cout << "rsp:\n" << commandDefinition.rspContents << "\n";
                    }
                }

                command.result = std::async(std::launch::async, [&command, &commandDefinition, &doneMutex, &doneCommands](){
                    for(auto& output : commandDefinition.outputs)
                    {
                        std::filesystem::path path(output);
                        if(path.has_parent_path())
                        {
                            std::filesystem::create_directories(path.parent_path());
                        }
                    }

                    if(!commandDefinition.rspFile.empty())
                    {
                        // TODO: Error handling
#if _WIN32
                        FILE* file = fopen(commandDefinition.rspFile.string().c_str(), "wbN");
#else
                        FILE* file = fopen(commandDefinition.rspFile.string().c_str(), "wbe");
#endif
                        fwrite(commandDefinition.rspContents.data(), 1, commandDefinition.rspContents.size(), file);
                        fclose(file);
                    }

                    auto result = process::run(commandDefinition.command + " 2>&1");

                    if(!commandDefinition.rspFile.empty())
                    {
                        std::error_code ec;
                        std::filesystem::remove(commandDefinition.rspFile, ec);
                    }
                    
                    {
                        std::scoped_lock doneLock(doneMutex);
                        doneCommands.push_back(&command);
                    }
                    return result;
                });
                runningCommands.push_back(&command);
            }

            if((commandCompleted[command.command] || command.result.valid()) && !skipped)
            {
                firstPending = i+1;
            }
        }
    }

    std::cout << "\n" << std::flush;

    return completed;
}

static std::vector<PendingCommand> filterCommands(std::filesystem::path invocationPath, Database& database, const std::filesystem::path& dataPath, std::vector<StringId> targets)
{
    bool allIncluded = targets.empty();

    auto& commands = database.getCommands();
    auto& dependencies = database.getDependencies();

    std::vector<PendingCommand> filteredCommands;
    filteredCommands.reserve(commands.size());
    for(uint32_t commandIndex = 0; commandIndex < commands.size(); ++commandIndex)
    {
        filteredCommands.push_back({commandIndex});
        filteredCommands.back().included = allIncluded;
    }

    struct ExpandedTarget
    {
        StringId target;
        StringId expanded;
    };

    std::vector<ExpandedTarget> expandedTargets;
    expandedTargets.reserve(targets.size());
    for(auto& target : targets)
    {
        bool done = false;
#if TODO
        for(auto& project : env.projects)
        {
            if(project->name == target)
            {
                // TODO: There could be more outputs from other commands in the project.
                expandedTargets.push_back({project->output.fullPath().string(), std::filesystem::absolute(project->output).lexically_normal().string()});
                done = true;
                break;
            }
        }
#endif
        if(!done)
        {
            expandedTargets.push_back({target, (invocationPath / target.cstr()).lexically_normal().string()});
        }
    }

    std::vector<size_t> stack;
    stack.reserve(commands.size());
    auto markIncluded = [&filteredCommands, &stack, &dependencies](size_t includeIndex){
        stack.push_back(includeIndex);
        while(!stack.empty())
        {
            auto commandIndex = stack.back();
            stack.pop_back();
            
            filteredCommands[commandIndex].included = true;
            stack.insert(stack.end(), dependencies[commandIndex].begin(), dependencies[commandIndex].end());
        }
    };

    for(auto target : expandedTargets)
    {
        bool found = false;
        for(uint32_t commandIndex = 0; commandIndex < commands.size(); ++commandIndex)
        {
            for(auto& output : commands[commandIndex].outputs)
            {
                if(target.expanded == StringId(output.string()))
                {
                    markIncluded(commandIndex);
                    found = true;
                    break;
                }
            }
            if(found)
            {
                break;
            }
        }
        if(!found)
        {
            throw std::runtime_error("The specified target could not be found:\n  " + std::string(target.target) + " (" + target.expanded.cstr() + ")");
        }
    }
    
    TimeCache timeCache;

    // TODO: Move this into the database
    auto cmdFilePath = dataPath / "cmdlines.db";
    auto cmdData = readFile(cmdFilePath);
    std::unordered_map<StringId, std::string_view> cmdLines;
    std::unordered_map<StringId, std::string_view> rspContents;

    auto cmdDataView = std::string_view(cmdData);
    while(!cmdDataView.empty())
    {
        std::string_view line;
        std::tie(line, cmdDataView) = str::split(cmdDataView, '\n');
        auto [file, tmp] = str::split(line, 0);
        auto [cmdLine, rspContent] = str::split(tmp, 0);
        cmdLines[file] = cmdLine;
        rspContents[file] = rspContent;
    }

    bool commandLinesChanged = false;
    for(uint32_t commandIndex = 0; commandIndex < commands.size(); ++commandIndex)
    {
        // TODO: More descriptive names for the different concepts
        const auto& command = commands[commandIndex];
        const auto& commandDependencies = dependencies[commandIndex];
        auto& filteredCommand = filteredCommands[commandIndex];

        for(auto& path : command.outputs)
        {
            auto str = path.string();
            auto strId = StringId(str);
            if(cmdLines[strId] != command.command || rspContents[strId] != command.rspContents)
            {
#if LOG_DIRTY_REASON
                std::cout << str << "\n";
                std::cout << "\"" << cmdLines[strId] << "\" vs \"" << command.command << "\"\n";
#endif
                filteredCommand.dirty = true;
                commandLinesChanged = true;
            }
        }
        if(filteredCommand.dirty) continue;

        for(auto dependency : dependencies[commandIndex])
        {
            if(filteredCommands[dependency].dirty)
            {
                filteredCommand.dirty = true;
                break;
            }
        }
        if(filteredCommand.dirty) continue;

        std::filesystem::file_time_type outputTime;
        outputTime = outputTime.max();
        std::error_code ec;
        for(auto& output : command.outputs)
        {
            outputTime = std::min(outputTime, timeCache.get(StringId(output.string()), ec));
            if(ec)
            {
#if LOG_DIRTY_REASON
                std::cout << "dirty: " << output << " did not exist.\n";
#endif
                filteredCommand.dirty = true;
                break;
            }
        }
        if(filteredCommand.dirty) continue;

        for(auto& input : command.inputs)
        {
            auto inputTime = timeCache.get(StringId(input.string()), ec);
            if(ec || inputTime > outputTime)
            {
                if(ec)
                {
#if LOG_DIRTY_REASON
                    std::cout << "dirty: " << input << " did not exist.\n";
#endif
                }
                else
                {
#if LOG_DIRTY_REASON
                    std::cout << "dirty: " << input << " was newer than output.\n";
#endif
                }
                filteredCommand.dirty = true;
                break;
            }
        }
        if(filteredCommand.dirty) continue;

        if(!command.depFile.empty())
        {
            auto data = readFile(command.depFile);
            if(data.empty())
            {
#if LOG_DIRTY_REASON
                std::cout << "dirty: \"" << command.depFile << "\" did not exist.\n";
#endif
                filteredCommand.dirty = true;
            }
            else
            {
                bool dirty = parseDependencyData(data, [&outputTime, &timeCache](std::string_view path)
                {
                    std::error_code ec;
                    auto inputTime = timeCache.get(path, ec);
                    if(ec)
                    {
#if LOG_DIRTY_REASON
                        std::cout << "dirty: \"" << path << "\" did not exist.\n";
#endif
                    }
                    else if(inputTime > outputTime)
                    {
#if LOG_DIRTY_REASON
                        std::cout << "dirty: " << path << " was newer than output.\n";
#endif
                    }
                    return ec || inputTime > outputTime;
                });
                if(dirty)
                {
#if LOG_DIRTY_REASON
                    std::cout << "dirty: Dependency file \"" << command.depFile << "\" returned true.\n";
#endif
                }
                filteredCommand.dirty = dirty;
            }
        }
    }

    if(commandLinesChanged)
    {
        std::ofstream cmdFile(cmdFilePath, std::ostream::binary);
        
        for(auto& command : commands)
        {
            for(auto& path : command.outputs)
            {
                auto str = path.string();
                cmdFile.write(str.c_str(), str.size()+1);
                cmdFile.write(command.command.c_str(), command.command.size()+1);
                cmdFile.write(command.rspContents.c_str(), command.rspContents.size());
                cmdFile.write("\n", 1);
            }
        }
    }

    filteredCommands.erase(std::remove_if(filteredCommands.begin(), filteredCommands.end(), [](auto& command) { 
            return !command.dirty || !command.included;
        }), filteredCommands.end());

    return filteredCommands;
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
    project.features += { feature::Cpp17, feature::DebugSymbols, feature::Exceptions, feature::Optimize };
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
