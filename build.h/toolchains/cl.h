#pragma once

#include <filesystem>
#include <string>

#include "core/project.h"
#include "modules/command.h"
#include "modules/feature.h"
#include "modules/language.h"
#include "modules/toolchain.h"
#include "util/string.h"

struct ClToolchainProvider : public ToolchainProvider
{
    const std::string compiler;
    const std::string linker;
    const std::string archiver;
    const std::vector<std::filesystem::path> sysIncludePaths;
    const std::vector<std::filesystem::path> sysLibPaths;

    ClToolchainProvider(std::string name, std::string compiler, std::string linker, std::string archiver, std::vector<std::filesystem::path> sysIncludePaths, std::vector<std::filesystem::path> sysLibPaths);
    virtual std::string getCompiler(Project& project, ProjectSettings& resolvedSettings, std::filesystem::path pathOffset, Language language) const;
    virtual std::string getCommonCompilerFlags(Project& project, ProjectSettings& resolvedSettings, std::filesystem::path pathOffset, Language language) const;
    virtual std::string getCompilerFlags(Project& project, ProjectSettings& resolvedSettings, std::filesystem::path pathOffset, Language language, const std::string& input, const std::string& output) const;
    virtual std::string getLinker(Project& project, ProjectSettings& resolvedSettings, std::filesystem::path pathOffset) const;
    virtual std::string getCommonLinkerFlags(Project& project, ProjectSettings& resolvedSettings, std::filesystem::path pathOffset) const;
    virtual std::string getLinkerFlags(Project& project, ProjectSettings& resolvedSettings, std::filesystem::path pathOffset, const std::vector<std::string>& inputs, const std::string& output) const;
    std::vector<std::filesystem::path> process(Project& project, ProjectSettings& resolvedSettings, StringId config, const std::filesystem::path& workingDir) const override;
};
