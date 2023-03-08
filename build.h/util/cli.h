#pragma once

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <map>
#include <string>
#include <stdexcept>
#include <utility>
#include <variant>

#include "util/process.h"
#include "util/string.h"

namespace cli
{

struct argument_error : public std::runtime_error
{
    argument_error(std::string message)
        : runtime_error(message)
    {}
};

struct Profile
{
    Profile(StringId name, std::vector<std::string> arguments)
        : name(name), arguments(arguments)
    {
        list().push_back(*this);
    }

    static std::vector<Profile>& list()
    {
        static std::vector<Profile> profiles;
        return profiles;
    }

    StringId name;
    std::vector<std::string> arguments;  
};

struct Argument
{
    virtual void extract(std::vector<std::string>& values)
    {
        auto it = values.begin();
        while(it != values.end())
        {
            auto& argStr = *it;
            if(tryExtractArgument(argStr))
            {
                values.erase(it);
                return;
            }
            ++it;
        }
    }

    virtual bool tryExtractArgument(std::string_view argStr)
    {
        return false;
    }

    virtual void reset() = 0;

    std::string example;
    std::string description;

    static inline std::vector<cli::Argument*>& globalList()
    {
        static std::vector<cli::Argument*> arguments;
        return arguments;
    }
};

struct BoolArgument : public Argument
{
    BoolArgument(std::string name, std::string description)
        : BoolArgument(Argument::globalList(), name, description)
    { }

    BoolArgument(std::vector<cli::Argument*>& argumentList, std::string name, std::string description)
    {
        this->name = std::move(name);
        this->example = "--" + this->name;
        this->description = std::move(description);

        argumentList.push_back(this);
    }

    bool tryExtractArgument(std::string_view argStr) override
    {
        if(argStr.size() == name.size() + 2 &&
           argStr.substr(0, 2) == "--" &&
           argStr.substr(2) == name)
        {
            value = true;
            return true;
        }

        return false;
    }

    void reset() override
    {
        value = false;
    }

    explicit operator bool() const { return value; }

    std::string name;
    bool value = false;
};

struct StringArgument : public Argument
{
    StringArgument(std::string name, std::string description, std::optional<std::string> defaultValue = {})
        : StringArgument(Argument::globalList(), name, description, defaultValue)
    { }

    StringArgument(std::vector<cli::Argument*>& argumentList, std::string name, std::string description, std::optional<std::string> defaultValue = {})
    {
        this->name = std::move(name);
        this->example = "--" + this->name + "=<value>";
        this->description = std::move(description);
        this->defaultValue = defaultValue;
        
        value = defaultValue;
        if(value)
        {
            this->description += " [default:" + std::string(*value) + "]";
        }

        argumentList.push_back(this);
    }

    bool tryExtractArgument(std::string_view argStr) override
    {
        if(argStr.size() < name.size() + 2 ||
           argStr.substr(0, 2) != "--")
        {
            return false;
        }
        argStr = argStr.substr(2);

        if(argStr.compare(0, name.size(), name) != 0)
        {
            return false;
        }

        if(argStr.size() <= name.size())
        {
            throw argument_error("Expected value for option '" + name + "'.");
        }
        
        if(argStr[name.size()] != '=')
        {
            return false;
        }

        value = argStr.substr(name.size()+1);
        return true;        
    }

    void reset() override
    {
        value = defaultValue;
    }


    explicit operator bool() const { return value.has_value(); }

    StringId operator*() { return *value; }

    std::string name;
    std::optional<StringId> value;
    std::optional<StringId> defaultValue;
};

struct PathArgument : public Argument
{
    PathArgument(std::string name, std::string description, std::optional<std::filesystem::path> defaultValue = {})
        : PathArgument(Argument::globalList(), name, description, defaultValue)
    { }

    PathArgument(std::vector<cli::Argument*>& argumentList, std::string name, std::string description, std::optional<std::filesystem::path> defaultValue = {})
    {
        this->name = std::move(name);
        this->example = "--" + this->name + "=<value>";
        this->description = std::move(description);
        this->defaultValue = defaultValue;
        
        value = defaultValue;
        if(value)
        {
            this->description += " [default:" + value->string() + "]";
            value = std::filesystem::absolute(*value);
        }
        
        argumentList.push_back(this);
    }

    bool tryExtractArgument(std::string_view argStr) override
    {
        if(argStr.size() < name.size() + 2 ||
           argStr.substr(0, 2) != "--")
        {
            return false;
        }
        argStr = argStr.substr(2);

        if(argStr.compare(0, name.size(), name) != 0)
        {
            return false;
        }

        if(argStr.size() <= name.size())
        {
            throw argument_error("Expected value for option '" + name + "'.");
        }
        
        if(argStr[name.size()] != '=')
        {
            return false;
        }

        value = std::filesystem::absolute(argStr.substr(name.size()+1));
        return true;
    }

    void reset() override
    {
        value = defaultValue;
        if(value)
        {
            value = std::filesystem::absolute(*value);
        }
    }

    explicit operator bool() const { return value.has_value(); }

    std::filesystem::path& operator*() { return *value; }

    std::string name;
    std::optional<std::filesystem::path> value;
    std::optional<std::filesystem::path> defaultValue;
};


struct Context
{
    Context(std::filesystem::path startPath, std::string invocation, std::vector<std::string> arguments)
        : startPath(std::move(startPath))
        , invocation(std::move(invocation))
        , configurationFile(process::findCurrentModulePath().replace_extension(".cpp")) // Wish this was a bit more robust but __BASE_FILE__ isn't available everywhere...
        , allArguments(std::move(arguments))
    {
        unusedArguments = allArguments;
        if(!unusedArguments.empty() && !str::startsWith(unusedArguments[0], "--"))
        {
            action = unusedArguments[0];
            unusedArguments.erase(unusedArguments.begin());
        }
    }

    void requireAllArgumentsUsed()
    {
        for(auto& argument : unusedArguments)
        {
            throw cli::argument_error("Argument \"" + argument + "\" was not recognized.");
        }
    }

    void extractArguments(const std::vector<Argument*>& arguments)
    {
        // We want path arguments to be relative the start path, but
        // it's a bit messy to switch this back and forth. The various
        // relevant paths should probably be made available so things can use
        // them explicitly
        auto originalPath = std::filesystem::current_path();
        std::filesystem::current_path(startPath);
        for(auto argument : arguments)
        {
            argument->reset();
            argument->extract(unusedArguments);
        }
        std::filesystem::current_path(originalPath);
    }
    
    const std::filesystem::path startPath;
    const std::filesystem::path configurationFile;
    const std::string invocation;
    const std::vector<std::string> allArguments;
    std::vector<std::string> unusedArguments;
    StringId action;
};

}