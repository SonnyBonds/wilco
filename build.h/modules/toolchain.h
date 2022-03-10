#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "core/option.h"
#include "core/project.h"
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

struct ToolchainProvider
{
    StringId name;
    ToolchainProvider(StringId name) 
        : name(name) 
    {
        Toolchains::install(this);
    }

    virtual std::vector<std::filesystem::path> process(Project& project, OptionCollection& resolvedOptions, StringId config, const std::filesystem::path& workingDir) const = 0;
};

Option<ToolchainProvider*> Toolchain{"Toolchain"};
