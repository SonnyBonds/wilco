#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "core/project.h"
#include "core/property.h"
#include "modules/language.h"

struct ToolchainProvider;

struct Toolchains
{
    static void install(ToolchainProvider* toolchain)
    {
        getToolchains().push_back(toolchain);
    }

    static const std::vector<const ToolchainProvider*>& list()
    {
        return getToolchains();
    }

private:
    static std::vector<const ToolchainProvider*>& getToolchains()
    {
        static std::vector<const ToolchainProvider*> toolchains;
        return toolchains;
    }
};

struct Project;
struct ProjectSettings;

struct ToolchainProvider
{
    StringId name;
    ToolchainProvider(StringId name) 
        : name(name) 
    {
        Toolchains::install(this);
    }

    virtual std::vector<std::filesystem::path> process(Project& project, StringId config, const std::filesystem::path& workingDir) const = 0;
};
