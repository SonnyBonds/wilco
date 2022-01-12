#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "core/option.h"
#include "core/project.h"

struct ToolchainProvider
{
    virtual std::string getCompiler(Project& project, ProjectConfig& resolvedConfig, std::filesystem::path pathOffset) const = 0;
    virtual std::string getCommonCompilerFlags(Project& project, ProjectConfig& resolvedConfig, std::filesystem::path pathOffset) const = 0;
    virtual std::string getCompilerFlags(Project& project, ProjectConfig& resolvedConfig, std::filesystem::path pathOffset, const std::string& input, const std::string& output) const = 0;

    virtual std::string getLinker(Project& project, ProjectConfig& resolvedConfig, std::filesystem::path pathOffset) const = 0;
    virtual std::string getCommonLinkerFlags(Project& project, ProjectConfig& resolvedConfig, std::filesystem::path pathOffset) const = 0;
    virtual std::string getLinkerFlags(Project& project, ProjectConfig& resolvedConfig, std::filesystem::path pathOffset, const std::vector<std::string>& inputs, const std::string& output) const = 0;

    virtual std::vector<std::filesystem::path> process(Project& project, ProjectConfig& resolvedConfig, StringId config, const std::filesystem::path& workingDir) const = 0;
};

Option<ToolchainProvider*> Toolchain{"Toolchain"};