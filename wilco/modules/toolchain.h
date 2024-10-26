#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "core/project.h"
#include "core/property.h"
#include "modules/language.h"

struct ToolchainProvider;

namespace extensions::internal
{
struct ToolchainOutputs
{
	ListPropertyValue<std::filesystem::path> objectFiles;
	ListPropertyValue<std::filesystem::path> libraryFiles;

	virtual void import(const ToolchainOutputs& other)
	{
		// These properties are used internally by toolchains and never imported
	}
};
} // namespace extensions::internal

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
    std::string name;
    ToolchainProvider(std::string name) 
        : name(name) 
    {
        Toolchains::install(this);
    }

	virtual void process(Project& project, const std::filesystem::path& workingDir, const std::filesystem::path& dataDir) const = 0;
};
