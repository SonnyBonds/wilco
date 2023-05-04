#include "database.h"
#include "modules/command.h"
#include "fileutil.h"

#include <fstream>
#include <string_view>
#include <iostream>
#include <filesystem>

#include "util/hash.h"
#include "dependencyparser.h"

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

#pragma pack(1)
struct Header
{
    uint32_t magic = 'bldh';
    uint32_t version = 4;
    char str[8] = {'b', 'u', 'i', 'l', 'd', 'd', 'b', '\0'};
};
#pragma pack()

static void writeString(std::ostream& stream, const std::string& str)
{
    stream.write(str.c_str(), str.size()+1);
};

static void writeUInt(std::ostream& stream, uint32_t value)
{
    stream.write(reinterpret_cast<const char*>(&value), sizeof(uint32_t));
}

static void writeSignature(std::ostream& stream, Signature signature)
{
    stream.write(reinterpret_cast<const char*>(&signature), sizeof(Signature));
}

static void writeStringList(std::ostream& stream, const std::vector<std::string>& list)
{
    writeUInt(stream, list.size());
    for(auto& item : list)
    {
        writeString(stream, item);
    }
}

static void writePathList(std::ostream& stream, const std::vector<std::filesystem::path>& list)
{
    writeUInt(stream, list.size());
    for(auto& item : list)
    {
        writeString(stream, item.string());
    }
}

static void writeIdList(std::ostream& stream, const std::vector<CommandId>& list)
{
    writeUInt(stream, list.size());
    stream.write(reinterpret_cast<const char*>(list.data()), sizeof(CommandId) * list.size());
}

static void writeDepFile(std::ostream& stream, const DepFile& depFile)
{
    writeString(stream, depFile.path.string());
    if(!depFile.path.empty())
    {
        writeUInt(stream, depFile.format);
    }
}

static void readData(std::string_view data, size_t& pos, char* output, size_t amount)
{
    if(data.size() < amount || data.size()-amount < pos)
    {
        throw std::runtime_error("Reading past the end of input.");
    }

    std::memcpy(output, data.data() + pos, amount);
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

static Signature readSignature(std::string_view data, size_t& pos)
{
    Signature result = {};
    readData(data, pos, reinterpret_cast<char*>(&result), sizeof(result));
    return result;
}

static std::vector<std::string> readStringList(std::string_view data, size_t& pos)
{
    uint32_t size = readUInt(data, pos);
    std::vector<std::string> result;
    result.reserve(size);
    for(uint32_t i=0; i < size; ++i)
    {
        result.push_back(std::string(readString(data, pos)));
    }
    return result;
}

static std::vector<std::filesystem::path> readPathList(std::string_view data, size_t& pos)
{
    uint32_t size = readUInt(data, pos);
    std::vector<std::filesystem::path> result;
    result.reserve(size);
    for(uint32_t i=0; i < size; ++i)
    {
        result.push_back(std::filesystem::path(readString(data, pos)));
    }
    return result;
}

static std::vector<CommandId> readIdList(std::string_view data, size_t& pos)
{
    uint32_t size = readUInt(data, pos);
    std::vector<CommandId> result;
    result.resize(size);
    readData(data, pos, (char*)result.data(), sizeof(CommandId) * size);
    return result;
}

static DepFile readDepFile(std::string_view data, size_t& pos)
{
    DepFile result;
    result.path = readString(data, pos);
    if(!result.path.empty())
    {
        uint32_t format = readUInt(data, pos);
        switch (format)
        {
        case DepFile::Format::GCC:
            result.format = DepFile::Format::GCC;
            break;
        case DepFile::Format::MSVC:
            result.format = DepFile::Format::MSVC;
            break;
        default:
            throw std::runtime_error("Unknown depfile format type for " + result.path.string() + ".");
        }
    }
    return result;
}

Signature computeCommandSignature(const CommandEntry& command)
{
    hash::Md5 hasher;
    hasher.digest(command.command);
    hasher.digest(command.rspContents);
    for(auto& input : command.inputs)
    {
        hasher.digest(reinterpret_cast<const char*>(input.native().data()), input.native().size() * sizeof(std::filesystem::path::string_type::value_type));
    }
    for(auto& output : command.outputs)
    {
        hasher.digest(reinterpret_cast<const char*>(output.native().data()), output.native().size() * sizeof(std::filesystem::path::string_type::value_type));
    }
    return hasher.finalize();
}

Database::Database()
{ }

bool Database::load(std::filesystem::path path)
{
    try
    {
        _commands.clear();
        _commandDependencies.clear();
        _commandSignatures.clear();
        _fileDependencies.clear();

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
        if(std::memcmp(&referenceHeader, &loadedHeader, sizeof(Header)) != 0)
        {
            throw std::runtime_error("Mismatching header.");
        }


        uint32_t numCommands = readUInt(_commandData, pos);
        _commands.reserve(numCommands);
        _commandDependencies.resize(numCommands);
        _commandSignatures.reserve(numCommands);
        _depFileSignatures.reserve(numCommands);
        for(uint32_t index = 0; index < numCommands; ++index)
        {
            CommandEntry command;
            // TODO: Reference into the loaded command data instead of copying
            command.command = readString(_commandData, pos);
            command.description = readString(_commandData, pos);
            command.workingDirectory = readString(_commandData, pos);
            command.depFile = readDepFile(_commandData, pos);
            command.rspFile = readString(_commandData, pos);
            command.rspContents = readString(_commandData, pos);
            command.inputs = readPathList(_commandData, pos);
            command.outputs = readPathList(_commandData, pos);
            _commandSignatures.push_back(readSignature(_commandData, pos));
            _depFileSignatures.push_back(readSignature(_commandData, pos));

            _commandDependencies[index] = readIdList(_commandData, pos);
            if(_commandDependencies[index].size() > numCommands)
            {
                throw std::runtime_error("Dependency count out of bounds.");
            }
            for(auto dep : _commandDependencies[index])
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
        _commandDependencies.clear();
        _fileDependencies.clear();
        return false;
    }

    try
    {
        // TODO: Memory map the inputs?
        _dependencyData = readFile(path.string() + ".deps");
        if(_dependencyData.size() == 0)
        {
            rebuildFileDependencies();
            return true;
        }

        size_t pos = 0;
        Header loadedHeader = {};
        readData(_dependencyData, pos, (char*)(&loadedHeader), sizeof(Header));
        Header referenceHeader = {};
        if(std::memcmp(&referenceHeader, &loadedHeader, sizeof(Header)) != 0)
        {
            throw std::runtime_error("Mismatching header.");
        }

        uint32_t numDependencies = readUInt(_dependencyData, pos);
        _fileDependencies.reserve(numDependencies);
        for(uint32_t index = 0; index < numDependencies; ++index)
        {
            FileDependencies fileDeps;
            // TODO: Reference into the loaded command data instead of copying
            fileDeps.path = readString(_dependencyData, pos);
            fileDeps.dependentCommands = readIdList(_dependencyData, pos);
            for(auto dep : fileDeps.dependentCommands)
            {
                if(dep >= _commands.size())
                {
                    throw std::runtime_error("Dependency index out of bounds.");
                }
            }
            fileDeps.signaturePair.first = readSignature(_dependencyData, pos);
            fileDeps.signaturePair.second = readSignature(_dependencyData, pos);
            _fileDependencies.push_back(std::move(fileDeps));
        }
    }
    catch(const std::exception& e)
    {
        std::cout << "Existing dependency database incompatible or corrupted. (" << e.what() << ")" << std::endl;
        rebuildFileDependencies();
    }
    
    return true;
}

void Database::save(std::filesystem::path path)
{
    {
        std::ofstream commandFile(path.string() + ".commands", std::ios::binary);
        Header header;
        commandFile.write(reinterpret_cast<const char*>(&header), sizeof(Header));

        writeUInt(commandFile, _commands.size());
        for(uint32_t index = 0; index < _commands.size(); ++index)
        {
            auto& command = _commands[index];
            writeString(commandFile, command.command);
            writeString(commandFile, command.description);
            writeString(commandFile, command.workingDirectory.string());
            writeDepFile(commandFile, command.depFile);
            writeString(commandFile, command.rspFile.string());
            writeString(commandFile, command.rspContents);
            writePathList(commandFile, command.inputs);
            writePathList(commandFile, command.outputs);
            writeSignature(commandFile, _commandSignatures[index]);
            writeSignature(commandFile, _depFileSignatures[index]);
            writeIdList(commandFile, _commandDependencies[index]);
        }
    }

    {
        std::ofstream dependencyFile(path.string() + ".deps", std::ios::binary);
        Header header;
        dependencyFile.write(reinterpret_cast<const char*>(&header), sizeof(Header));

        writeUInt(dependencyFile, _fileDependencies.size());
        for(uint32_t index = 0; index < _fileDependencies.size(); ++index)
        {
            auto& fileDeps = _fileDependencies[index];
            writeString(dependencyFile, fileDeps.path.string());
            writeIdList(dependencyFile, fileDeps.dependentCommands);
            writeSignature(dependencyFile, fileDeps.signaturePair.first);
            writeSignature(dependencyFile, fileDeps.signaturePair.second);
        }
    }
}

const std::vector<CommandDependencies>& Database::getCommandDependencies() const
{
    return _commandDependencies;
}

std::vector<FileDependencies>& Database::getFileDependencies()
{
    return _fileDependencies;
}

const std::vector<CommandEntry>& Database::getCommands() const
{
    return _commands;
}

std::vector<Signature>& Database::getCommandSignatures()
{
    return _commandSignatures;
}

std::vector<Signature>& Database::getDepFileSignatures()
{
    return _depFileSignatures;
}

void Database::setCommands(std::vector<CommandEntry> commands)
{
    if(commands.size() >= UINT32_MAX)
    {
        throw std::runtime_error(std::to_string(commands.size()) + " is a lot of commands.");
    }

    struct CommandSortProxy
    {
        CommandId id;
        bool dirty = false;
        bool included = false;
        int depth = 0;
        CommandDependencies dependencies;
    };

    std::vector<CommandSortProxy> sortProxies;
    sortProxies.reserve(commands.size());

    std::unordered_map<std::filesystem::path, CommandId, PathHash> commandMap;
    for(uint32_t i=0; i<commands.size(); ++i)
    {
        sortProxies.push_back({i});
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

    for(auto& sortProxy : sortProxies)
    {
        auto& command = commands[sortProxy.id];
        sortProxy.dependencies.reserve(command.inputs.size());
        for(auto& input : command.inputs)
        {
            auto it = commandMap.find(input);
            if(it != commandMap.end())
            {
                sortProxy.dependencies.push_back(it->second);
            }
        }
    }

    using It = decltype(sortProxies.begin());
    It next = sortProxies.begin();
    struct StackEntry
    {
        CommandId id;
        int depth;
    };
    std::vector<StackEntry> stack;
    stack.reserve(sortProxies.size());
    while(next != sortProxies.end() || !stack.empty())
    {
        CommandId id;
        int depth = 0;
        if(stack.empty())
        {
            id = next->id;
            depth = sortProxies[id].depth;
            ++next;
        }
        else
        {
            id = stack.back().id;
            depth = stack.back().depth;
            stack.pop_back();
        }

        sortProxies[id].depth = depth;

        for(auto dependency : sortProxies[id].dependencies)
        {
            if(sortProxies[dependency].depth < depth+1)
            {
                stack.push_back({dependency, depth+1});
            }
        }
    }

    std::sort(sortProxies.begin(), sortProxies.end(), [](const auto& a, const auto& b) { return a.depth > b.depth; });

    std::vector<CommandId> idRemap;
    idRemap.resize(commands.size());

    _commands.clear();
    _depFileSignatures.clear();
    _commands.reserve(commands.size());
    _commandDependencies.clear();
    _commandDependencies.reserve(commands.size());
    
    {
        CommandId id = 0;
        for(auto& sortProxy : sortProxies)
        {
            idRemap[sortProxy.id] = id;
            ++id;
            _commands.push_back(std::move(commands[sortProxy.id]));
            _commandDependencies.push_back(std::move(sortProxy.dependencies));
            for(auto& dependency : _commandDependencies.back())
            {
                dependency = idRemap[dependency];
            }
        }
    }

    for(size_t index = 0; index < _commandDependencies.size(); ++index)
    {
        for(auto dep : _commandDependencies[index])
        {
            if(dep >= _commands.size())
            {
                throw std::runtime_error("Internal error - dependency index out of bounds.");
            }
            if(dep >= index)
            {
                throw std::runtime_error("Invalid command dependency:\n  \"" + _commands[index].description + "\"\n  depends on \n  \"" + _commands[dep].description + "\"\nEither there is a cyclic dependency, or an internal error in the dependency resolution.");
            }
        }
    }

    // Transfer any previous recorded signatures, and add blank entries for non-existing (because they should be rebuilt) 
    std::unordered_set<Signature> existingSignatures;
    existingSignatures.insert(_commandSignatures.begin(), _commandSignatures.end());
    _commandSignatures.clear();
    _commandSignatures.reserve(_commands.size());
    for(auto& command : _commands)
    {
        auto signature = computeCommandSignature(command);
        if(existingSignatures.find(signature) != existingSignatures.end())
        {
            _commandSignatures.push_back(signature);
        }
        else
        {
            _commandSignatures.push_back(Signature{});
        }
    }

    rebuildFileDependencies();
}

void Database::rebuildFileDependencies()
{
    std::unordered_set<std::filesystem::path, PathHash> outputs;
    for(size_t index = 0; index < _commands.size(); ++index)
    {
        for(auto& output : _commands[index].outputs)
        {
            outputs.insert(output);
        }
    }

    _depFileSignatures.clear();
    _depFileSignatures.reserve(_commands.size());

    std::unordered_map<std::filesystem::path, std::vector<uint32_t>, PathHash> depCommands;
    for(size_t index = 0; index < _commands.size(); ++index)
    {
        auto& command = _commands[index];
        Signature depFileSignature = {};
        if(command.depFile)
        {
            auto depContents = readFile(command.depFile);
            depFileSignature = hash::md5(depContents);
            // parseDependencyData is destructive, so do the hash first
            parseDependencyData(depContents, [&outputs, &depCommands, index](std::string_view path) {
                if(outputs.find(path) == outputs.end())
                {
                    depCommands[path].push_back(index);
                }
                return false;
            });
        }
        _depFileSignatures.push_back(depFileSignature);

        for(auto& input : command.inputs)
        {
            if(outputs.find(input) == outputs.end())
            {
                depCommands[input].push_back(index);
            }
        }
    }

    std::unordered_map<std::filesystem::path, SignaturePair, PathHash> existingSignatures;
    for(auto& dep : _fileDependencies)
    {
        existingSignatures[dep.path] = dep.signaturePair;
    }

    _fileDependencies.clear();
    _fileDependencies.reserve(depCommands.size());

    for(auto& entry : depCommands)
    {
        SignaturePair signaturePair = {};
        auto it = existingSignatures.find(entry.first);
        if(it != existingSignatures.end())
        {
            signaturePair = it->second;
        }
        _fileDependencies.push_back({std::move(entry.first), std::move(entry.second), signaturePair});
    }
}

