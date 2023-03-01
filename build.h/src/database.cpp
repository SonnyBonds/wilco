#include "database.h"
#include "modules/command.h"
#include "fileutil.h"

#include <fstream>
#include <string_view>
#include <iostream>

#pragma pack(1)
struct Header
{
    uint32_t magic = 'bldh';
    uint32_t version = 1;
    char str[8] = {'b', 'u', 'i', 'l', 'd', 'd', 'b', '\0'};
};
#pragma pack()

static void writeStr(std::ostream& stream, const std::string& str)
{
    stream.write(str.c_str(), str.size()+1);
};

static void writeUInt(std::ostream& stream, uint32_t value)
{
    stream.write(reinterpret_cast<const char*>(&value), sizeof(uint32_t));
}

static void readData(std::string_view data, size_t& pos, char* output, size_t amount)
{
    if(data.size() < amount || data.size()-amount < pos)
    {
        throw std::runtime_error("Reading past the end of input.");
    }

    memcpy(output, data.data() + pos, amount);
    pos += amount;
}

static std::string_view readString(std::string_view data, size_t& pos)
{
    size_t endPos = data.find((char)0, pos);
    if(endPos == std::string::npos)
    {
        throw std::runtime_error("Failed to find end of string in input.");
    }

    std::string_view result = data.substr(pos, endPos-pos);
    pos = endPos + 1;
    return result;
}

static uint32_t readUInt(std::string_view data, size_t& pos)
{
    uint32_t result = 0;
    readData(data, pos, reinterpret_cast<char*>(&result), sizeof(result));
    return result;
}

Database::Database()
{ }

bool Database::load(std::filesystem::path path)
{
    try
    {
        _commands.clear();
        _dependencies.clear();

        // TODO: Memory map the inputs?
        _commandData = readFile(path.string() + ".commands");
        if(_commandData.size() == 0)
        {
            return false;
        }

        size_t pos = 0;
        Header loadedHeader = {};
        readData(_commandData, pos, (char*)(&loadedHeader), sizeof(Header));
        Header referenceHeader = {};
        if(memcmp(&referenceHeader, &loadedHeader, sizeof(Header)) != 0)
        {
            throw std::runtime_error("Mismatching header.");
        }


        uint32_t numCommands = readUInt(_commandData, pos);
        _commands.reserve(numCommands);
        std::vector<std::vector<uint32_t>> dependencies;
        _dependencies.resize(numCommands);
        for(uint32_t index = 0; index < numCommands; ++index)
        {
            CommandEntry command;
            // TODO: Reference into the loaded command data instead of copying
            command.command = readString(_commandData, pos);
            command.description = readString(_commandData, pos);
            command.workingDirectory = readString(_commandData, pos);
            command.depFile = readString(_commandData, pos);
            command.rspFile = readString(_commandData, pos);
            command.rspContents = readString(_commandData, pos);
            auto numInputs = readUInt(_commandData, pos);
            command.inputs.reserve(numInputs);
            for(uint32_t input = 0; input < numInputs; ++input)
            {
                command.inputs.push_back(readString(_commandData, pos));
            }
            auto numOutputs = readUInt(_commandData, pos);
            command.outputs.reserve(numOutputs);
            for(uint32_t output = 0; output < numOutputs; ++output)
            {
                command.outputs.push_back(readString(_commandData, pos));
            }

            auto numDependencies = readUInt(_commandData, pos);
            if(numDependencies > numCommands)
            {
                throw std::runtime_error("Dependency count out of bounds.");
            }
            _dependencies[index].resize(numDependencies);
            readData(_commandData, pos, reinterpret_cast<char*>(_dependencies[index].data()), sizeof(uint32_t) * numDependencies);
            for(auto dep : _dependencies[index])
            {
                if(dep >= index)
                {
                    throw std::runtime_error("Dependency index out of bounds.");
                }
            }

            _commands.push_back(std::move(command));
        }
    }
    catch(std::exception& e)
    {
        std::cout << "Existing build database incompatible or corrupted. (" << e.what() << ")" << std::endl;
        _commandData.clear();
        _commands.clear();
        _dependencies.clear();
        return false;
    }

    return true;
}

void Database::save(std::filesystem::path path)
{
    std::ofstream commandFile(path.string() + ".commands", std::ios::binary);
    Header header;
    commandFile.write(reinterpret_cast<const char*>(&header), sizeof(Header));

    writeUInt(commandFile, _commands.size());
    for(uint32_t index = 0; index < _commands.size(); ++index)
    {
        auto& command = _commands[index];
        writeStr(commandFile, command.command);
        writeStr(commandFile, command.description);
        writeStr(commandFile, command.workingDirectory.string());
        writeStr(commandFile, command.depFile.string());
        writeStr(commandFile, command.rspFile.string());
        writeStr(commandFile, command.rspContents);
        writeUInt(commandFile, command.inputs.size());
        for(auto& input : command.inputs)
        {
            writeStr(commandFile, input.string());
        }
        writeUInt(commandFile, command.outputs.size());
        for(auto& output : command.outputs)
        {
            writeStr(commandFile, output.string());
        }
        writeUInt(commandFile, _dependencies[index].size());
        commandFile.write(reinterpret_cast<const char*>(_dependencies[index].data()), sizeof(uint32_t) * _dependencies[index].size());
    }
}

const std::vector<std::vector<uint32_t>>& Database::getDependencies()
{
    return _dependencies;
}

const std::vector<CommandEntry>& Database::getCommands()
{
    return _commands;
}

void Database::setCommands(std::vector<CommandEntry> commands)
{
    if(commands.size() >= UINT32_MAX)
    {
        throw std::runtime_error(std::to_string(commands.size()) + " is a lot of commands.");
    }

    using Id = uint32_t;
    struct CommandProxy
    {
        Id commandId;
        bool dirty = false;
        bool included = false;
        int depth = 0;
        std::vector<uint32_t> dependencies;
    };

    std::vector<CommandProxy> proxies;
    proxies.reserve(commands.size());

    std::unordered_map<std::filesystem::path, Id> commandMap;
    for(uint32_t i=0; i<commands.size(); ++i)
    {
        proxies.push_back({i});
        auto& command = commands[i];

        for(auto& output : command.outputs)
        {
            output = std::filesystem::absolute(output).lexically_normal().string();
            commandMap[output] = i;
        }

        for(auto& input : command.inputs)
        {
            input = std::filesystem::absolute(input).lexically_normal().string();
        }
    }

    for(auto& proxy : proxies)
    {
        auto& command = commands[proxy.commandId];
        proxy.dependencies.reserve(command.inputs.size());
        for(auto& input : command.inputs)
        {
            auto it = commandMap.find(input);
            if(it != commandMap.end())
            {
                proxy.dependencies.push_back(it->second);
            }
        }
    }

    using It = decltype(proxies.begin());
    It next = proxies.begin();
    struct StackEntry
    {
        Id commandId;
        int depth;
    };
    std::vector<StackEntry> stack;
    stack.reserve(proxies.size());
    while(next != proxies.end() || !stack.empty())
    {
        Id commandId;
        int depth = 0;
        if(stack.empty())
        {
            commandId = next->commandId;
            depth = proxies[commandId].depth;
            ++next;
        }
        else
        {
            commandId = stack.back().commandId;
            depth = stack.back().depth;
            stack.pop_back();
        }

        proxies[commandId].depth = depth;

        for(auto dependency : proxies[commandId].dependencies)
        {
            if(proxies[dependency].depth < depth+1)
            {
                stack.push_back({dependency, depth+1});
            }
        }
    }

    std::sort(proxies.begin(), proxies.end(), [](const auto& a, const auto& b) { return a.depth > b.depth; });

    _commands.clear();
    _commands.reserve(commands.size());
    _dependencies.reserve(commands.size());
    for(auto& proxy : proxies)
    {
        _commands.push_back(std::move(commands[proxy.commandId]));
        _dependencies.push_back(std::move(proxy.dependencies));
    }
}
