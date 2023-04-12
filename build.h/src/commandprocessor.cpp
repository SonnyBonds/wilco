#include "commandprocessor.h"
#include "util/hash.h"
#include "util/string.h"
#include "util/interrupt.h"
#include "fileutil.h"
#include "dependencyparser.h"
#include <assert.h>

#define LOG_DIRTY_REASON 0

Signature computeFileSignature(std::filesystem::path path)
{
    std::error_code ec;
    auto time = std::filesystem::last_write_time(path, ec);
    if(ec)
    {
        return {};
    }

    return hash::md5(reinterpret_cast<const char*>(&time), sizeof(time));    
}

Signature computeDirectorySignature(std::filesystem::path path)
{
    std::error_code ec;
    if(!std::filesystem::is_directory(path))
    {
        return {};
    }

    hash::Md5 hasher;
    for(auto entry : std::filesystem::directory_iterator(path))
    {
        hasher.digest(entry.path().native());
    }
    return hasher.finalize();
}

bool updatePathSignature(SignaturePair& signaturePair, const std::filesystem::path& path)
{
    auto signature = computeFileSignature(path);
    if(signature == EMPTY_SIGNATURE)
    {
#if LOG_DIRTY_REASON
        std::cout << "dirty: " << path << " did not exist.\n";
#endif
        signaturePair.first = {};
        signaturePair.second = {};
        return true;
    }

    if(signature == signaturePair.first)
    {
        return false;
    }

    signaturePair.first = signature;

    signature = computeDirectorySignature(path);
    std::error_code ec;
    if(signature != EMPTY_SIGNATURE && signaturePair.second == signature)
    {
        return false;
    }

#if LOG_DIRTY_REASON
    std::cout << "dirty: " << path << " has been touched.\n";
#endif
    signaturePair.second = signature;
    return true;
}

void checkInputSignatures(std::vector<Signature>& commandSignatures, std::vector<PendingCommand>& filteredCommands, std::vector<FileDependencies>::iterator begin, std::vector<FileDependencies>::iterator end)
{
    for(auto fileDependency = begin; fileDependency != end; ++fileDependency)
    {
        bool dirty = updatePathSignature(fileDependency->signaturePair, fileDependency->path);
        if(dirty)
        {
            for(auto& commandId : fileDependency->dependentCommands)
            {
                commandSignatures[commandId] = {};
            }
        }
    }
}

size_t runCommands(std::vector<PendingCommand>& filteredCommands, Database& database, size_t maxConcurrentCommands, bool verbose)
{
    const auto& commandDefinitions = database.getCommands();
    const auto& dependencies = database.getCommandDependencies();
    auto& commandSignatures = database.getCommandSignatures();
    auto& depFileSignatures = database.getDepFileSignatures();

    std::unordered_map<std::filesystem::path, SignaturePair> newInputSignatures;

    // Not loving this, but since the dependency map are indices
    // in the unfiltered commands we need the full list
    // TODO: Test if a mapping table is faster or not
    std::vector<bool> commandCompleted;
    commandCompleted.resize(database.getCommands().size(), true);
    for(auto& filteredCommands : filteredCommands)
    {
        commandCompleted[filteredCommands.command] = false;
    }

    bool rebuildDependencies = false;

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

                // TODO: Make something better than a hardcoded filter for CL filename echo
                if(!commandDefinitions[command->command].inputs.empty() && output == commandDefinitions[command->command].inputs.front().filename())
                {
                    output = {};
                }

                if(!output.empty() && !interrupt::isInterrupted())
                {
                    std::cout << "\n" << output;
                }

                if(interrupt::isInterrupted())
                {
                    halt = true;
                }
                else if(result.exitCode != 0)
                {
                    std::cout << "\nCommand returned " + std::to_string(result.exitCode);
                    halt = true;
                }
                else
                {
                    if(commandDefinitions[command->command].depFile)
                    {
                        auto depFileContents = readFile(commandDefinitions[command->command].depFile);
                        auto depFileSignature = hash::md5(depFileContents);
                        if(depFileSignature != depFileSignatures[command->command])
                        {
                            parseDependencyData(depFileContents, [&newInputSignatures](std::string_view path){
                                auto it = newInputSignatures.find(path);
                                if(it == newInputSignatures.end())
                                {
                                    updatePathSignature(newInputSignatures[path], path);
                                }

                                return false;
                            });
                            rebuildDependencies = true;
                        }
                    }
                    commandSignatures[command->command] = computeCommandSignature(commandDefinitions[command->command]);
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

                command.result = std::async(std::launch::async, [&command, &commandDefinition, &doneMutex, &doneCommands]() -> process::ProcessResult
                {
                    process::ProcessResult result = {1, "Unknown error."};
                    try
                    {
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
                            if(!file)
                            {
                                throw std::system_error(errno, std::generic_category(), "Failed to open rsp file \"" + commandDefinition.rspFile.string() + "\" for writing");
                            }
                            
                            auto writeResult = fwrite(commandDefinition.rspContents.data(), 1, commandDefinition.rspContents.size(), file);
                            if(writeResult < commandDefinition.rspContents.size())
                            {
                                throw std::system_error(errno, std::generic_category(), "Failed to write rsp file \"" + commandDefinition.rspFile.string() + "\"");
                            }

                            auto closeResult = fclose(file);
                            if(closeResult != 0)
                            {
                                throw std::system_error(errno, std::generic_category(), "Failed to write rsp file \"" + commandDefinition.rspFile.string() + "\".");
                            }
                        }

                        result = process::run(commandDefinition.command + " 2>&1");

                        if(!commandDefinition.rspFile.empty())
                        {
                            std::error_code ec;
                            std::filesystem::remove(commandDefinition.rspFile, ec);
                        }
                    }
                    catch(const std::exception& e)
                    {
                        result = {1, e.what()};
                    }
                    catch(...)
                    {
                        result = {1, "Unknown error."};
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

    if(rebuildDependencies)
    {
        if(!newInputSignatures.empty())
        {
            auto& fileDependencies = database.getFileDependencies();
            for(auto& input : fileDependencies)
            {
                auto it = newInputSignatures.find(input.path);
                if(it != newInputSignatures.end())
                {
                    input.signaturePair = it->second;
                    newInputSignatures.erase(it);
                }
            }

            for(auto& signature : newInputSignatures)
            {
                fileDependencies.push_back({signature.first, {}, signature.second});
            }
        }

        std::cout << "Updating dependency graph." << std::endl;
        database.rebuildFileDependencies(); 
    }

    return completed;
}

std::vector<PendingCommand> filterCommands(std::filesystem::path invocationPath, Database& database, const std::filesystem::path& dataPath, std::vector<StringId> targets)
{
    bool allIncluded = targets.empty();

    auto& commands = database.getCommands();
    auto& dependencies = database.getCommandDependencies();
    auto& commandSignatures = database.getCommandSignatures();
    auto& fileDependencies = database.getFileDependencies();

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
    
    //std::cout << fileDependencies.size() << " file dependencies.\n";
    {
        size_t maxConcurrentCommands = std::max((size_t)1, (size_t)std::thread::hardware_concurrency());
        std::vector<std::future<void>> futures;
        size_t numEntries = fileDependencies.size();
        for(size_t i = 0; i < maxConcurrentCommands; ++i)
        {
            int start = i * numEntries / maxConcurrentCommands;
            int end = (i+1) * numEntries / maxConcurrentCommands;
            futures.push_back(std::async(std::launch::async, [
                start, 
                end, 
                &filteredCommands, 
                &commandSignatures,
                &fileDependencies]()
            {
                checkInputSignatures(commandSignatures, filteredCommands, fileDependencies.begin() + start, fileDependencies.begin() + end);
            }));
        }
        for(auto& future : futures)
        {
            future.wait();
        }
    }

    bool commandLinesChanged = false;
    for(uint32_t commandIndex = 0; commandIndex < commands.size(); ++commandIndex)
    {
        // TODO: More descriptive names for the different concepts
        const auto& command = commands[commandIndex];
        const auto& commandDependencies = dependencies[commandIndex];
        auto& filteredCommand = filteredCommands[commandIndex];
        auto& commandSignature = commandSignatures[commandIndex];

        if (commandSignature == EMPTY_SIGNATURE)
        {
#if LOG_DIRTY_REASON
            std::cout << "dirty: Signature missing for " << command.description << std::endl;
#endif
            continue;
        }
        if(commandSignature != computeCommandSignature(command))
        {
#if LOG_DIRTY_REASON
            std::cout << "dirty: Signature mismatching for " << command.description << std::endl;
#endif
            commandSignature = {};
            continue;
        }

        for(auto dependency : dependencies[commandIndex])
        {
            if(commandSignatures[dependency] == EMPTY_SIGNATURE)
            {
#if LOG_DIRTY_REASON
                std::cout << "dirty: Transitive " << command.description << std::endl;
#endif
                commandSignature = {};
                break;
            }
        }
    }

    filteredCommands.erase(std::remove_if(filteredCommands.begin(), filteredCommands.end(), [&commandSignatures](auto& command) { 
            return commandSignatures[command.command] != EMPTY_SIGNATURE || !command.included;
        }), filteredCommands.end());

    return filteredCommands;
}