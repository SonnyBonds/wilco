#pragma once

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <stdexcept>
#include <utility>
#include <variant>

#include "core/emitter.h"
#include "util/string.h"

namespace cli
{

struct argument_error : public std::runtime_error
{
    argument_error(std::string message)
        : runtime_error(message)
    {}
};

struct ArgumentDefinition
{
    // Processes the argument argStr and returns true if it was used
    std::function<bool(std::string_view argStr)> argumentProcessor;
    std::string example;
    std::string description;
};

namespace detail
{
    template<typename U>
    std::optional<std::string> grabValue(const std::optional<U>& value)
    {
        if(value)
        {
            return std::string(*value);
        }
        return {};
    }

    std::optional<std::string> grabValue(const std::filesystem::path& value)
    {
        return value.string();
    }

    template<typename U>
    std::optional<std::string> grabValue(const U& value)
    {
        return std::string(value);
    }
}

template<typename T>
ArgumentDefinition stringArgument(std::string trigger, T& value, std::string description)
{
    ArgumentDefinition def;
    def.example = trigger + "=<value>";
    def.description = description;

    std::optional<std::string> defaultValue = detail::grabValue(value);
    if(defaultValue)
    {
        def.description += " [default: \"" + *defaultValue + "\"]";
    } 

    def.argumentProcessor = [trigger, &value](std::string_view argStr)
    {
        if(argStr.compare(0, trigger.size(), trigger) != 0)
        {
            return false;
        }

        if(argStr.size() <= trigger.size())
        {
            throw argument_error("Expected value for option '" + trigger + "'.");
        }
        
        if(argStr[trigger.size()] != '=')
        {
            return false;
        }

        value = argStr.substr(trigger.size()+1);
        return true;
    };

    return def;
}


ArgumentDefinition boolArgument(std::string trigger, bool& value, std::string description)
{
    ArgumentDefinition def;
    def.example = trigger;
    def.description = description;

    def.argumentProcessor = [trigger, &value](std::string_view argStr)
    {
        if(argStr == trigger)
        {
            value = true;
            return true;
        }
        return false;
    };

    return def;
}

template<typename T>
ArgumentDefinition selectionArgument(std::vector<std::pair<StringId, T>> possibleValues, T& value, std::string description)
{
    ArgumentDefinition def;
    def.description = description;

    def.example = "{";
    bool first = true;
    for(auto& valuePair : possibleValues)
    {
        if(!first)
        {
            def.example += "|";                        
        }
        first = false;
        def.example += std::string(valuePair.first);
    }
    def.example += "}";

    def.argumentProcessor = [possibleValues, &value](std::string_view argStr)
    {
        StringId argId(argStr);
        for(auto valuePair : possibleValues)
        {
            if(argId == valuePair.first)
            {
                value = valuePair.second;
                return true;
            }
        }
        return false;
    }; 

    return def;
}

void extractArguments(std::vector<std::string>& argumentStrings, const std::vector<ArgumentDefinition>& argumentDefinitions)
{
    auto it = argumentStrings.begin();
    while(it != argumentStrings.end())
    {
        auto& argStr = *it;
        bool found = false;
        for(auto& def : argumentDefinitions)
        {
            if(def.argumentProcessor(argStr))
            {
                found = true;
                break;
            }
        }
        if(found)
        {
            it = argumentStrings.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

struct Context
{
    Context(std::filesystem::path startPath, std::string invocation, std::vector<std::string> arguments)
        : startPath(std::move(startPath))
        , invocation(std::move(invocation))
        , allArguments(std::move(arguments))
    {
        unusedArguments = allArguments;
    }

    void addArgumentDescription(const std::string& argument, const std::string& doc, std::string prefix = "  ")
    {
        usage += prefix + str::padRightToSize(argument, 30) + "  " + doc + "\n";
    }

    void addArgumentDescriptions(const std::vector<ArgumentDefinition>& argumentDefinitions, std::string prefix = "  ")
    {
        bool first = true;
        for(auto& def : argumentDefinitions)
        {
            addArgumentDescription(def.example, def.description, prefix);
            if(first)
            {
                first = false;
                prefix = std::string(prefix.size(), ' ');
            }
        }
    }

    void extractArguments(const std::vector<ArgumentDefinition>& argumentDefinitions)
    {
        cli::extractArguments(unusedArguments, argumentDefinitions);
    }

    const std::filesystem::path startPath;
    const std::string invocation;
    const std::vector<std::string> allArguments;
    std::vector<std::string> unusedArguments;
    std::string usage;
};

}