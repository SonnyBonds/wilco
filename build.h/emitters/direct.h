#pragma once

#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/stringid.h"
#include "core/emitter.h"
#include "core/project.h"
#include "core/stringid.h"
#include "modules/command.h"
#include "modules/postprocess.h"
#include "modules/toolchain.h"
#include "toolchains/detected.h"
#include "util/operators.h"
#include "util/process.h"
#include "util/file.h"

class DirectBuilder
{
public:
    static void emit(const EmitterArgs& args)
    {
        auto projects = args.projects;

        std::vector<std::filesystem::path> generatorDependencies;
        for(auto& project : projects)
        {
            generatorDependencies += (*project)[GeneratorDependencies];
            for(auto& entry : project->configs)
            {
                generatorDependencies += (*project)[GeneratorDependencies];
            }
        }

        auto buildOutput = std::filesystem::path(BUILD_FILE).replace_extension("");
        Project generator("_generator", Executable);
        generator[Features] += { "c++17", "optimize" };
        generator[IncludePaths] += BUILD_H_DIR;
        generator[OutputPath] = buildOutput;
        generator[Defines] += {
            "START_DIR=\\\"" START_DIR "\\\"",
            "BUILD_H_DIR=\\\"" BUILD_H_DIR "\\\"",
            "BUILD_DIR=\\\"" BUILD_DIR "\\\"",
            "BUILD_FILE=\\\"" BUILD_FILE "\\\"",
            "BUILD_ARGS=\\\"" BUILD_ARGS "\\\"",
        };
        generator[Files] += BUILD_FILE;

        generatorDependencies += buildOutput;
        //generator[Commands] += { "\"" + (BUILD_DIR / buildOutput).string() + "\" " BUILD_ARGS, generatorDependencies, { }, START_DIR, {}, "Running build generator." };

        projects.push_back(&generator);

        projects = Emitter::discoverProjects(projects);

        std::vector<PendingCommand> pendingCommands;
        using It = decltype(pendingCommands.begin());

        for(auto project : projects)
        {
            build(pendingCommands, args.targetPath, *project, args.config);
        }

        std::unordered_map<StringId, PendingCommand*> commandMap;
        for(auto& command : pendingCommands)
        {
            for(auto& output : command.outputs)
            {
                commandMap[output] = &command;
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

        It next = pendingCommands.begin();
        std::vector<std::pair<PendingCommand*, int>> stack;
        stack.reserve(pendingCommands.size());
        std::vector<PendingCommand*> commands;
        commands.reserve(pendingCommands.size());
        while(next != pendingCommands.end() || !stack.empty())
        {
            PendingCommand* command;
            int depth = 0;
            if(stack.empty())
            {
                command = &*next;
                depth = command->depth;
                ++next;
                commands.push_back(command);
            }
            else
            {
                command = stack.back().first;
                depth = stack.back().second;
                stack.pop_back();
            }

            command->depth = depth;

            for(auto& dependency : command->dependencies)
            {
                if(dependency->depth < depth+1)
                {
                    stack += {dependency, depth+1};
                }
            }
        }

        std::sort(commands.begin(), commands.end(), [](auto a, auto b) { return a->depth > b->depth; });
        
        TimeCache timeCache;
        
        for(auto command : commands)
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
                    command->dirty = true;
                    break;
                }
            }
            if(command->dirty) continue;

            if(!command->depFile.empty())
            {
                auto data = file::read(command->depFile.cstr());
                if(data.empty())
                {
                    command->dirty = true;
                }
                else
                {
                    command->dirty = parseDependencyData(data, [&outputTime, &timeCache](std::string_view path)
                    {
                        std::error_code ec;
                        auto inputTime = timeCache.get(path, ec);
                        return ec || inputTime > outputTime;
                    });
                }            
            }
        }

        commands.erase(std::remove_if(commands.begin(), commands.end(), [](auto command) { return !command->dirty; }), commands.end());

        std::string line;

        size_t count = 0;
        size_t firstPending = 0;
        std::vector<PendingCommand*> runningCommands;
        bool halt = false;
        std::mutex doneMutex;
        std::vector<PendingCommand*> doneCommands;
        size_t maxConcurrentCommands = std::max((size_t)1, (size_t)std::thread::hardware_concurrency());
        std::cout << "Building using " << maxConcurrentCommands << " concurrent tasks.";
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
                    if(result.exitCode != 0)
                    {
                        std::cout << "\n" << result.output;
                        std::cout << "\nCommand returned " + std::to_string(result.exitCode) << std::flush;
                        halt = true;
                    }
                    it = doneCommands.erase(it);

                    auto runIt = std::find(runningCommands.begin(), runningCommands.end(), command);
                    assert(runIt != runningCommands.end());
                    if(runIt != runningCommands.end())
                    {
                        runningCommands.erase(runIt);
                    }
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

                    std::cout << "\n["/*"\33[2K\r["*/ << (++count) << "/" << commands.size() << "] " << command->desciption << std::flush;
                    command->result = std::async(std::launch::async, [command, &doneMutex, &doneCommands](){
                        for(auto& output : command->outputs)
                        {
                            std::filesystem::path path(output.cstr());
                            if(path.has_parent_path())
                            {
                                std::filesystem::create_directories(path.parent_path());
                            }
                        }

                        auto result = process::run(command->commandString + " 2>&1");
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

        std::cout << "\n" << std::string(args.config) + ": " + std::to_string(commands.size()) << " targets rebuilt.";
        if(commands.size() == 0)
        {
            std::cout << " (Everything up to date.)";
        }
        std::cout << "\n";
    }

    template<typename Callable>
    static bool parseDependencyData(std::string& data, Callable callable)
    {
        size_t pos = 0;
        auto skipWhitespace = [&](){
            while(pos < data.size())
            {
                char c = data[pos];
                if(!std::isspace(data[pos]) && 
                   (data[pos] != '\\' || pos == data.size()-1 || !std::isspace(data[pos+1])))
                {
                    break;
                }
                ++pos;
            }
        };

        auto readPath = [&](){
            size_t start = pos;
            size_t offs = false;
            bool escaped = false;
            while(pos < data.size())
            {
                auto c = data[pos];
                if(c == '\\')
                {
                    escaped = true;
                }
                else if(std::isspace(c))
                {
                    if(escaped)
                    {
                        offs += 1;
                    }
                    else
                    {
                        return std::string_view(data.data()+start, pos-offs-start);
                    }
                    escaped = false;
                }
                else
                {
                    escaped = false;
                }

                data[pos-offs] = c;
                ++pos;
            }

            return std::string_view(data.data() + start, pos-offs-start);
        };

        bool scanningOutputs = true;
        while(pos < data.size())
        {
            skipWhitespace();
            auto pathString = readPath();
            if(pathString.empty())
            {
                continue;
            }

            if(pathString.back() == ':')
            {
                scanningOutputs = false;
                continue;
            }

            if(scanningOutputs)
            {
                continue;
            }

            if(callable(pathString))
            {
                return true;
            }
        }

        return false;
    }

private:
    struct PendingCommand
    {
        std::vector<StringId> inputs;
        std::vector<StringId> outputs;
        StringId depFile;
        std::string commandString;
        std::string desciption;
        int depth = 0;
        bool dirty = false;
        std::vector<PendingCommand*> dependencies;
        std::future<process::ProcessResult> result;
    };

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

    static void build(std::vector<PendingCommand>& pendingCommands, const std::filesystem::path& root, Project& project, StringId config)
    {
        auto resolved = project.resolve(config, OperatingSystem::current());
        resolved[DataDir] = root;

        {
            // Avoiding range-based for loop here since it breaks
            // if a post processor adds more post processors. 
            auto postProcessors = resolved[PostProcess];
            for(size_t i = 0; i < postProcessors.size(); ++i)
            {
                postProcessors[i](project, resolved);
            }
        }

        if(!project.type.has_value())
        {
            return;
        }

        if(project.name.empty())
        {
            throw std::runtime_error("Trying to build project with no name.");
        }

        std::filesystem::create_directories(root);
        std::filesystem::path pathOffset = std::filesystem::proximate(std::filesystem::current_path(), root);

        auto& commands = resolved[Commands];
        if(project.type == Command && commands.empty())
        {
            throw std::runtime_error("Command project '" + project.name + "' has no commands.");
        }

        const ToolchainProvider* toolchain = resolved[Toolchain];
        if(!toolchain)
        {
            toolchain = defaultToolchain;
        }

        auto toolchainOutputs = toolchain->process(project, resolved, config, {});

        std::string prologue;
        if(OperatingSystem::current() == Windows)
        {
            prologue += "cmd /c ";
        }
        prologue += "cd \"$cwd\" && ";

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

            std::vector<StringId> outputStrs;
            outputStrs.reserve(command.outputs.size());
            for(auto& path : command.outputs)
            {
                outputStrs.push_back(path.string());
            }

            std::string depfileStr;
            if(!command.depFile.empty())
            {
                depfileStr = command.depFile.string();
            }

            pendingCommands.push_back({
                inputStrs,
                outputStrs,
                depfileStr,
                "cd \"" + cwdStr + "\" && " + command.command,
                command.description,
            });
        }
    }

    static Emitters::Token installToken;
};
Emitters::Token DirectBuilder::installToken = Emitters::install({ "direct", &DirectBuilder::emit });
