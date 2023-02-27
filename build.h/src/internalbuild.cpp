#include "emitters/direct.h"
#include "dependencyparser.h"
#include "fileutil.h"
#include "util/commands.h"
#include <iostream>

static const int EXIT_RESTART = 10;

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

struct PendingCommand
{
    std::vector<StringId> inputs;
    std::vector<StringId> outputs;
    StringId depFile;
    std::string commandString;
    std::string description;
    std::string rspFile;
    std::string rspContents;
    bool dirty = false;
    bool included = false;
    int depth = 0;
    std::vector<PendingCommand*> dependencies;
    std::future<process::ProcessResult> result;
};

static void collectCommands(Environment& env, std::vector<PendingCommand>& pendingCommands, const std::filesystem::path& projectDir, Project& project, StringId config)
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

    auto toolchainOutputs = toolchain->process(project, config, {}, dataDir);

    std::string prologue;
    if(OperatingSystem::current() == Windows)
    {
        prologue += "cmd /c ";
    }
    prologue += "cd \"$cwd\" && ";

    auto cmdFilePath = dataDir / (project.name + ".cmdlines");
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
    std::ofstream cmdFile(cmdFilePath, std::ostream::binary);

    pendingCommands.reserve(pendingCommands.size() + commands.size());
    for(auto& command : commands)
    {
        std::filesystem::path cwd = command.workingDirectory;
        if(cwd.empty())
        {
            cwd = ".";
        }
        std::string cwdStr = cwd.string();

        std::vector<StringId> inputStrs;
        inputStrs.reserve(command.inputs.size());
        for(auto& path : command.inputs)
        {
            inputStrs.push_back(path.string());
        }

        bool dirty = false;

        std::vector<StringId> outputStrs;
        outputStrs.reserve(command.outputs.size());
        for(auto& path : command.outputs)
        {
            auto str = path.string();
            auto strId = StringId(str);
            outputStrs.push_back(strId);
            if(cmdLines[strId] != command.command || rspContents[strId] != command.rspContents)
            {
                // LOG std::cout << "\"" << cmdLines[strId] << "\" vs \"" << command.command << "\"\n";
                dirty = true;
            }
            
            cmdFile.write(str.c_str(), str.size()+1);
            cmdFile.write(command.command.c_str(), command.command.size()+1);
            cmdFile.write(command.rspContents.c_str(), command.rspContents.size());
            cmdFile.write("\n", 1);
        }

        std::string depfileStr;
        if(!command.depFile.empty())
        {
            depfileStr = command.depFile.string();
        }

        std::string rspfileStr;
        if(!command.rspFile.empty())
        {
            rspfileStr = command.rspFile.string();
        }

        pendingCommands.push_back({
            inputStrs,
            outputStrs,
            depfileStr,
            "cd \"" + cwdStr + "\" && " + command.command,
            command.description, // Safe to move?
            rspfileStr,
            command.rspContents, // Safe to move?
            dirty
        });
    }
}

static size_t runCommands(const std::vector<PendingCommand*>& commands, size_t maxConcurrentCommands, bool verbose)
{
    size_t count = 0;
    size_t completed = 0;
    size_t firstPending = 0;
    std::vector<PendingCommand*> runningCommands;
    bool halt = false;
    std::mutex doneMutex;
    std::vector<PendingCommand*> doneCommands;
    while((!halt && firstPending < commands.size()) || !runningCommands.empty())
    {
        // TODO: Semaphore of some kind instead of semi-spin lock
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(10ms);

        {
            std::scoped_lock doneLock(doneMutex);
            for(auto it = doneCommands.begin(); it != doneCommands.end(); )
            {
                auto command = *it;
                command->dirty = false;

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

                std::cout << std::flush;
            }
        }

        if(halt)
        {
            continue;
        }

        bool skipped = false;
        for(size_t i = firstPending; i < commands.size(); ++i)
        {
            if(runningCommands.size() >= maxConcurrentCommands)
            {
                break;
            }

            auto command = commands[i];
            if(command->dirty && !command->result.valid())
            {
                bool ready = true;
                for(auto dependency : command->dependencies)
                {
                    if(dependency->dirty)
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

                std::cout << "\n["/*"\33[2K\r["*/ << (++count) << "/" << commands.size() << "] " << command->description << std::flush;
                if(verbose)
                {
                    std::cout << "\n" << command->commandString << "\n";
                    if(!command->rspFile.empty())
                    {
                        std::cout << "rsp:\n" << command->rspContents << "\n";
                    }
                }

                command->result = std::async(std::launch::async, [command, &doneMutex, &doneCommands](){
                    for(auto& output : command->outputs)
                    {
                        std::filesystem::path path(output.cstr());
                        if(path.has_parent_path())
                        {
                            std::filesystem::create_directories(path.parent_path());
                        }
                    }

                    if(!command->rspFile.empty())
                    {
                        // TODO: Error handling
#if _WIN32
                        FILE* file = fopen(command->rspFile.c_str(), "wbN");
#else
                        FILE* file = fopen(command->rspFile.c_str(), "wbe");
#endif
                        fwrite(command->rspContents.data(), 1, command->rspContents.size(), file);
                        fclose(file);
                    }

                    auto result = process::run(command->commandString + " 2>&1");

                    if(!command->rspFile.empty())
                    {
                        std::error_code ec;
                        std::filesystem::remove(command->rspFile, ec);
                    }
                    
                    {
                        std::scoped_lock doneLock(doneMutex);
                        doneCommands.push_back(command);
                    }
                    return result;
                });
                runningCommands.push_back(command);
            }

            if((!command->dirty || command->result.valid()) && !skipped)
            {
                firstPending = i+1;
            }
        }
    }

    std::cout << "\n" << std::flush;

    return completed;
}

static std::vector<PendingCommand*> processCommands(Environment& env, const Configuration& config, std::vector<PendingCommand>& pendingCommands, std::vector<StringId> targets)
{
    std::unordered_map<StringId, PendingCommand*> commandMap;
    for(auto& command : pendingCommands)
    {
        for(auto& output : command.outputs)
        {
            output = std::filesystem::absolute(output.cstr()).lexically_normal().string();
            commandMap[output] = &command;
        }

        for(auto& input : command.inputs)
        {
            input = std::filesystem::absolute(input.cstr()).lexically_normal().string();
        }
    }

    for(auto& command : pendingCommands)
    {
        command.dependencies.reserve(command.inputs.size());
        for(auto& input : command.inputs)
        {
            auto it = commandMap.find(input);
            if(it != commandMap.end())
            {
                command.dependencies.push_back(it->second);
            }
        }
    }

    struct ExpandedTarget
    {
        StringId target;
        StringId expanded;
    };

    std::filesystem::path invocationPath = env.startupDir;

    std::vector<ExpandedTarget> expandedTargets;
    expandedTargets.reserve(targets.size());
    for(auto& target : targets)
    {
        bool done = false;
        for(auto& project : config.getProjects())
        {
            if(project->name == target)
            {
                // TODO: There could be more outputs from other commands in the project.
                expandedTargets.push_back({project->output.fullPath().string(), std::filesystem::absolute(project->output).lexically_normal().string()});
                done = true;
                break;
            }
        }
        if(!done)
        {
            expandedTargets.push_back({target, (invocationPath / target.cstr()).lexically_normal().string()});
        }
    }

    bool includeAll = expandedTargets.empty();
    using It = decltype(pendingCommands.begin());
    It next = pendingCommands.begin();
    struct StackEntry
    {
        PendingCommand* command;
        int depth;
        bool included;
    };
    std::vector<StackEntry> stack;
    stack.reserve(pendingCommands.size());
    std::vector<PendingCommand*> outputCommands;
    outputCommands.reserve(pendingCommands.size());
    while(next != pendingCommands.end() || !stack.empty())
    {
        PendingCommand* command;
        int depth = 0;
        bool included = includeAll;
        if(stack.empty())
        {
            command = &*next;
            depth = command->depth;
            included = includeAll || command->included;
            ++next;
            outputCommands.push_back(command);
        }
        else
        {
            command = stack.back().command;
            depth = stack.back().depth;
            included = command->included || stack.back().included;
            stack.pop_back();
        }

        for(auto targetIt = expandedTargets.begin(); targetIt != expandedTargets.end(); ++targetIt)
        {
            bool done = false;
            for(auto& output : command->outputs)
            {
                if(targetIt->expanded == output)
                {
                    expandedTargets.erase(targetIt);
                    included = true;
                    done = true;
                }
            }
            if(done)
            {
                break;
            }
        }

        command->included = included;
        command->depth = depth;

        for(auto& dependency : command->dependencies)
        {
            if(dependency->depth < depth+1)
            {
                stack.push_back({dependency, depth+1, included});
            }
        }
    }

    if(!expandedTargets.empty())
    {
        std::vector<std::string> targetStrings;
        targetStrings.reserve(targets.size());
        for(auto& expandedTarget : expandedTargets)
        {
            targetStrings.push_back(std::string(expandedTarget.target) + " (" + expandedTarget.expanded.cstr() + ")");
        }
        throw std::runtime_error("The specified targets were not found:\n  " + str::join(targetStrings, "\n  "));
    }

    std::sort(outputCommands.begin(), outputCommands.end(), [](auto a, auto b) { return a->depth > b->depth; });
    
    TimeCache timeCache;
    
    for(auto command : outputCommands)
    {
        for(auto dependency : command->dependencies)
        {
            if(dependency->dirty)
            {
                command->dirty = true;
                break;
            }
        }
        if(command->dirty) continue;

        std::filesystem::file_time_type outputTime;
        outputTime = outputTime.max();
        std::error_code ec;
        for(auto& output : command->outputs)
        {
            outputTime = std::min(outputTime, timeCache.get(output, ec));
            if(ec)
            {
                // LOG std::cout << "dirty: " << output << " did not exist.\n";
                command->dirty = true;
                break;
            }
        }
        if(command->dirty) continue;

        for(auto& input : command->inputs)
        {
            auto inputTime = timeCache.get(input, ec);
            if(ec || inputTime > outputTime)
            {
                if(ec)
                {
                    // LOG std::cout << "dirty: " << input << " did not exist.\n";
                }
                else
                {
                    // LOG std::cout << "dirty: " << input << " was newer than output.\n";
                }
                command->dirty = true;
                break;
            }
        }
        if(command->dirty) continue;

        if(!command->depFile.empty())
        {
            auto data = readFile(command->depFile.cstr());
            if(data.empty())
            {
                // LOG std::cout << "dirty: \"" << command->depFile << "\" did not exist.\n";
                command->dirty = true;
            }
            else
            {
                bool dirty = parseDependencyData(data, [&outputTime, &timeCache](std::string_view path)
                {
                    std::error_code ec;
                    auto inputTime = timeCache.get(path, ec);
                    if(ec)
                    {
                        // LOG std::cout << "dirty: \"" << path << "\" did not exist.\n";
                    }
                    else if(inputTime > outputTime)
                    {
                        // LOG std::cout << "dirty: " << path << " was newer than output.\n";
                    }
                    return ec || inputTime > outputTime;
                });
                if(dirty)
                {
                    // LOG std::cout << "dirty: Dependency file \"" << command->depFile << "\" returned true.\n";
                }
                command->dirty = dirty;
            }
        }
    }

    outputCommands.erase(std::remove_if(outputCommands.begin(), outputCommands.end(), [](auto command) { 
            return !command->dirty || !command->included; 
        }), outputCommands.end());

    return outputCommands;
}

DirectBuilder::DirectBuilder()
    : Emitter("build", "Build output binaries.")
{ }

void DirectBuilder::emit(Environment& env)
{
    size_t maxConcurrentCommands = std::max((size_t)1, (size_t)std::thread::hardware_concurrency());

    auto configs = env.configurations;

    if(selectedConfig)
    {
        if(std::find(configs.begin(), configs.end(), *selectedConfig) == configs.end())
        {
            throw std::runtime_error("Selected config '" + std::string(*selectedConfig) + "' has not been used in build configuration.");
        }
    }

    std::vector<PendingCommand> pendingCommands;
    for(auto& configName : configs)
    {
        if(selectedConfig && configName != *selectedConfig)
        {
            continue;
        }

        Configuration config{configName};
        configure(env, config);

        std::string configPrefix = config.name.empty() ? "" : std::string(config.name) + ": ";

        pendingCommands.clear();

        for(auto& project : config.getProjects())
        {
            collectCommands(env, pendingCommands, *targetPath / config.name.cstr(), *project, config.name);
        }
        auto commands = processCommands(env, config, pendingCommands, targets.values);

        if(commands.empty())
        {
            std::cout << configPrefix + "Nothing to do. (Everything up to date.)\n" << std::flush;
        }
        else
        {
            std::cout << "Building using " << maxConcurrentCommands << " concurrent tasks.";
            size_t completedCommands = runCommands(commands, maxConcurrentCommands, verbose.value);

            std::cout << "\n" << configPrefix + std::to_string(completedCommands) << " of " << commands.size() << " targets rebuilt.\n" << std::flush;

            // TODO: Error exit code on failure
        }
    }
}

void DirectBuilder::buildSelf(cli::Context cliContext, Environment& outputEnv)
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

    auto tempPath = outputPath / std::filesystem::path(env.configurationFile).filename().replace_extension(ext + (isSubProcess ? ".running_sub" : ".running"));
    auto buildOutput = std::filesystem::path(env.configurationFile).replace_extension(ext);

    Configuration config{{}};
    Project& project = config.createProject("Generator", Executable);
    project.features += { feature::Cpp17, feature::DebugSymbols, feature::Exceptions/*, feature::Optimize*/ };
    project.ext<extensions::Gcc>().compilerFlags += "-static";
    project.ext<extensions::Gcc>().linkerFlags += "-static";
    project.includePaths += env.buildHDir;
    project.output = buildOutput;
    project.files += env.configurationFile;
    project.files += env.listFiles(env.buildHDir / "src");

    for(auto& file : project.files)
    {
        outputEnv.addConfigurationDependency(file.path);
    }
    outputEnv.addConfigurationDependency(buildOutput);

    std::vector<PendingCommand> pendingCommands;
    collectCommands(env, pendingCommands, outputPath, project, {});

    auto commands = processCommands(env, config, pendingCommands, {});

    // If nothing is to be done...
    if(commands.empty())
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
        size_t completedCommands = runCommands(commands, maxConcurrentCommands, false);

        // Exit with failure if it fails.
        if(completedCommands < commands.size())
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

    std::string restartCommandLine = "cd " + str::quote(env.startupDir.string()) + " && " + str::quote((env.configurationFile.parent_path() / buildOutput).string());
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

    std::string buildCommandLine = "cd " + str::quote(env.startupDir.string()) + " && " + str::quote((env.configurationFile.parent_path() / buildOutput).string());
    buildCommandLine += argumentString;

    auto result = process::run(buildCommandLine, true);
    std::exit(result.exitCode);
}


EmitterInstance<DirectBuilder> DirectBuilder::instance;
