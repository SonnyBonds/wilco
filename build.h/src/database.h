#pragma once

#include "modules/command.h"
#include <array>

using CommandId = uint32_t;

using Signature = std::array<unsigned char, 16>;
static constexpr Signature EMPTY_SIGNATURE = {}; 

template<>
struct std::hash<Signature>
{
    std::size_t operator()(Signature const& signature) const
    {
        // Just use the first N bytes as hash
        std::size_t val;
        static_assert(sizeof(val) <= sizeof(Signature));
        memcpy(&val, &signature, sizeof(val));
        return val;
    }
};

// TODO: Better data structure for this
struct FileDependencies
{
    std::filesystem::path path;
    std::vector<CommandId> dependentCommands;
    Signature signature;
};

using CommandDependencies = std::vector<CommandId>;

Signature computeCommandSignature(const CommandEntry& command);

class Database
{
public:
    Database();

    bool load(std::filesystem::path path);
    void save(std::filesystem::path path);

    void setCommands(std::vector<CommandEntry> commands);

    void rebuildFileDependencies();

    const std::vector<CommandDependencies>& getCommandDependencies() const;
    const std::vector<CommandEntry>& getCommands() const;
    std::vector<Signature>& getCommandSignatures();
    std::vector<Signature>& getDepFileSignatures();
    std::vector<FileDependencies>& getFileDependencies();

private:
    std::vector<CommandEntry> _commands;
    std::vector<CommandDependencies> _commandDependencies;
    std::vector<FileDependencies> _fileDependencies;
    std::vector<Signature> _commandSignatures;
    std::vector<Signature> _depFileSignatures;
    std::string _commandData;
    std::string _dependencyData;
};