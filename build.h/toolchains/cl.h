#pragma once

#include <filesystem>
#include <string>

#include "core/project.h"
#include "modules/command.h"
#include "modules/feature.h"
#include "modules/language.h"
#include "modules/toolchain.h"
#include "util/string.h"

namespace feature::msvc
{
    inline Feature StaticRuntime{ "StaticRuntime" };
    inline Feature StaticDebugRuntime{ "StaticDebugRuntime" };
    inline Feature SharedRuntime{ "SharedRuntime" };
    inline Feature SharedDebugRuntime{ "SharedDebugRuntime" };
}

namespace extensions
{
    struct Msvc : public PropertyBag
    {
        ListProperty<StringId> compilerFlags{ this };
        ListProperty<StringId> linkerFlags{ this };
        ListProperty<StringId> archiverFlags{ this };
        Property<std::string> solutionFolder{ this };

        struct Pch : public PropertyGroup
        {
            Property<std::filesystem::path> header{ this };
            Property<std::filesystem::path> source{ this };
            ListProperty<std::filesystem::path> ignoredFiles{ this };
        } pch{ this };
    };
}

struct ClToolchainProvider : public ToolchainProvider
{
    const std::string compiler;
    const std::string linker;
    const std::string archiver;
    const std::string resourceCompiler;
    const std::vector<std::filesystem::path> sysIncludePaths;
    const std::vector<std::filesystem::path> sysLibPaths;

    ClToolchainProvider(std::string name, std::string compiler, std::string resourceCompiler, std::string linker, std::string archiver, std::vector<std::filesystem::path> sysIncludePaths, std::vector<std::filesystem::path> sysLibPaths);
    std::string getCompiler(Project& project, StringId config, std::filesystem::path pathOffset, Language language) const;
    std::string getCommonCompilerFlags(Project& project, StringId config, std::filesystem::path pathOffset, Language language) const;
    std::string getCompilerFlags(Project& project, StringId config, std::filesystem::path pathOffset, Language language, const std::string& input, const std::string& output) const;
    std::string getLinker(Project& project, StringId config, std::filesystem::path pathOffset) const;
    std::string getCommonLinkerFlags(Project& project, StringId config, std::filesystem::path pathOffset) const;
    std::string getLinkerFlags(Project& project, StringId config, std::filesystem::path pathOffset, const std::vector<std::string>& inputs, const std::string& output) const;
    std::vector<std::filesystem::path> process(Project& project, StringId config, const std::filesystem::path& workingDir, const std::filesystem::path& dataDir) const override;
};
