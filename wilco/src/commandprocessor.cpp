#include "commandprocessor.h"
#include "core/action.h"
#include "util/hash.h"
#include "util/string.h"
#include "util/interrupt.h"
#include "fileutil.h"
#include "dependencyparser.h"
#include <assert.h>
#include <thread>
#include <filesystem>

#define LOG_DIRTY_REASON 0

namespace 
{
    // Some implementations provide a std::hash specialization for std::filesystem::path and some
    // don't, so we roll our own (using std::filesystem::hash_value)
    struct PathHash
    {
        std::size_t operator()(const std::filesystem::path& path) const
        {
            return std::filesystem::hash_value(path);
        }
    };
}

// Computes a signature for a file. Currently bases it on write time stamp only, but could use other info as well.
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

// Computes a signature for a directory based on th directory file listing
Signature computeDirectorySignature(std::filesystem::path path)
{
    if(!std::filesystem::is_directory(path))
    {
        return {};
    }

    hash::Md5 hasher;
    std::error_code ec;
    for(auto entry : std::filesystem::directory_iterator(path, ec))
    {
        hasher.digest(entry.path().native());
    }
    if(ec)
    {
        return {};
    }

    return hasher.finalize();
}

// Update the signature pair for a path if needed, returning true if it has changed
// The path may be a file or a directory
bool updatePathSignature(SignaturePair& signaturePair, const std::filesystem::path& path)
{
    // First compute the signature for the file or directory entry itself.
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

    // If the first signature is correct, things have not changed even if this is a directory,
    // and we don't have to do the full file listing.
    if(signature == signaturePair.first)
    {
        return false;
    }

    // If the signature was new we update it and continue
    signaturePair.first = signature;

    // Second, try computing a directory signature.
    signature = computeDirectorySignature(path);
    // If this actually was a directory, and the signature was the same, we're done as well.
    if(signature != EMPTY_SIGNATURE && signaturePair.second == signature)
    {
        return false;
    }

#if LOG_DIRTY_REASON
    std::cout << "dirty: " << path << " has been touched.\n";
#endif
    // If it _wasn't_ a directory, or the directory signature was wrong, the path was dirty.
    // We'll update the second signature even if it wasn't a directory, in case the path has
    // changed from a directory to a file.
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

// This currently doesn't actually check the _signatures_ of the outputs, just the existence
void checkOutputSignatures(std::vector<Signature>& commandSignatures, const std::vector<CommandEntry>& commands, int beginIndex, int endIndex)
{
    for(int i = beginIndex; i != endIndex; ++i)
    {
        if(commandSignatures[i] == EMPTY_SIGNATURE)
        {
            continue;
        }
        for(auto& output : commands[i].outputs)
        {
            std::error_code ec;
            bool exists = std::filesystem::exists(output, ec);
            if(ec || !exists)
            {
#if LOG_DIRTY_REASON
                std::cout << "dirty: Output " << output << " missing for " << commands[i].description << std::endl;
#endif
                commandSignatures[i] = {};
                break;
            }
        }
    }
}

static process::ProcessResult runCommand(const CommandEntry& command)
{
    if(command.command.empty())
    {
        return { 0 };
    }

    for(auto& output : command.outputs)
    {
        std::filesystem::path path(output);
        if(path.has_parent_path())
        {
            std::filesystem::create_directories(path.parent_path());
        }
    }

    if(!command.rspFile.empty())
    {
        // TODO: Error handling
#if _WIN32
        FILE* file = nullptr;
        auto err = fopen_s(&file, command.rspFile.string().c_str(), "wbN");
        if(err)
        {
            throw std::system_error(err, std::generic_category(), "Failed to open rsp file \"" + command.rspFile.string() + "\" for writing");
        }
#else
        FILE* file = fopen(command.rspFile.string().c_str(), "wbe");
        if(!file)
        {
            throw std::system_error(errno, std::generic_category(), "Failed to open rsp file \"" + command.rspFile.string() + "\" for writing");
        }
#endif
        
        auto writeResult = fwrite(command.rspContents.data(), 1, command.rspContents.size(), file);
        if(writeResult < command.rspContents.size())
        {
            throw std::system_error(errno, std::generic_category(), "Failed to write rsp file \"" + command.rspFile.string() + "\"");
        }

        auto closeResult = fclose(file);
        if(closeResult != 0)
        {
            throw std::system_error(errno, std::generic_category(), "Failed to write rsp file \"" + command.rspFile.string() + "\".");
        }
    }

    // TODO: The cd "." isn't necessariy if workingDirectory is empty, but for some reason
    // the command doesn't run properly without it on Windows. Need to figure out why.
    std::string cwdString = command.workingDirectory.empty() ? "." : command.workingDirectory.string();
    std::string commandString = "cd \"" + cwdString + "\" && " + command.command + " 2>&1";

    process::ProcessResult result = process::run(commandString);

    if(!command.rspFile.empty())
    {
        std::error_code ec;
        std::filesystem::remove(command.rspFile, ec);
    }

    return result;
}

size_t runCommands(std::vector<PendingCommand>& filteredCommands, Database& database, size_t maxConcurrentCommands, bool verbose)
{
    const auto& commandDefinitions = database.getCommands();
    const auto& dependencies = database.getCommandDependencies();
    auto& commandSignatures = database.getCommandSignatures();
    auto& depFileSignatures = database.getDepFileSignatures();

    std::unordered_map<std::filesystem::path, SignaturePair, PathHash> newInputSignatures;

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
                                    updatePathSignature(newInputSignatures[path], std::filesystem::path(path).lexically_normal());
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
                        result = runCommand(commandDefinition);
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

std::vector<PendingCommand> filterCommands(Database& database, std::filesystem::path invocationPath, std::vector<std::string> targets)
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
        std::string target;
        std::string expanded;
    };

    std::vector<ExpandedTarget> expandedTargets;
    expandedTargets.reserve(targets.size());
    for(auto& target : targets)
    {
        expandedTargets.push_back({target, (invocationPath / target.c_str()).lexically_normal().string()});
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
            if(target.target == commands[commandIndex].description)
            {
                markIncluded(commandIndex);
                found = true;
            }
            for(auto& input : commands[commandIndex].inputs)
            {
                if(target.expanded == input.string())
                {
                    markIncluded(commandIndex);
                    found = true;
                    break;
                }
            }
            for(auto& output : commands[commandIndex].outputs)
            {
                if(target.expanded == output.string())
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
            throw std::runtime_error("The specified target could not be found:\n  " + std::string(target.target) + " (" + target.expanded.c_str() + ")");
        }
    }
    
    // Split all file dependencies in N buckets and do an input signature check on them in parallel
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

    // Split all commands in N buckets and do an output signature check on them in parallel
    {
        size_t maxConcurrentCommands = std::max((size_t)1, (size_t)std::thread::hardware_concurrency());
        std::vector<std::future<void>> futures;
        size_t numEntries = commands.size();
        for(size_t i = 0; i < maxConcurrentCommands; ++i)
        {
            int start = i * numEntries / maxConcurrentCommands;
            int end = (i+1) * numEntries / maxConcurrentCommands;
            futures.push_back(std::async(std::launch::async, [
                start, 
                end, 
                &commandSignatures,
                &commands]()
            {
                checkOutputSignatures(commandSignatures, commands, start, end);
            }));
        }
        for(auto& future : futures)
        {
            future.wait();
        }
    }

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

    filteredCommands.erase(std::remove_if(filteredCommands.begin(), filteredCommands.end(), [&commands, &commandSignatures](auto& command) { 
            // TODO: commands[command.command].command is clearly an indicator some stuff needs renaming...
            return commands[command.command].command.empty() || commandSignatures[command.command] != EMPTY_SIGNATURE || !command.included;
        }), filteredCommands.end());

    return filteredCommands;
}

bool commands::runCommands(std::vector<CommandEntry> commands, std::string databaseName) {
    auto databasePath = *targetPath / ("." + databaseName + "_db");
    Database database;
    database.load(databasePath);
    database.setCommands(std::move(commands));
    auto filteredCommands = filterCommands(database);
    if(filteredCommands.empty())
    {
        return false;
    }
    
    size_t maxConcurrentCommands = std::max((size_t)1, (size_t)std::thread::hardware_concurrency());
    size_t completedCommands = runCommands(filteredCommands, database, maxConcurrentCommands, false);
    database.save(databasePath);
    return true;
}
