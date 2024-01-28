#pragma once

#include <filesystem>
#include <string>

#include "core/project.h"
#include "modules/command.h"
#include "modules/feature.h"
#include "modules/language.h"
#include "modules/toolchain.h"
#include "util/string.h"

namespace extensions
{
    struct Msvc
    {
        ListPropertyValue<std::string> compilerFlags;
        ListPropertyValue<std::string> linkerFlags;
        ListPropertyValue<std::string> archiverFlags;
        std::string solutionFolder;

        struct Pch
        {
            std::filesystem::path header;
            std::filesystem::path source;
            ListPropertyValue<std::filesystem::path> ignoredFiles;
            std::optional<bool> forceInclude;
        } pch;

        virtual void import(const Msvc& other)
        {
            compilerFlags += other.compilerFlags;
            linkerFlags += other.linkerFlags;
            archiverFlags += other.archiverFlags;
            if(!other.solutionFolder.empty()) solutionFolder = other.solutionFolder;

            if(!pch.header.empty()) pch.header = other.pch.header;
            if(!pch.source.empty()) pch.source = other.pch.source;
            pch.ignoredFiles += other.pch.ignoredFiles;
            if(other.pch.forceInclude) pch.forceInclude = other.pch.forceInclude;
        }
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
    std::string getCompiler(Project& project, std::filesystem::path pathOffset, Language language) const;
    std::string getCommonCompilerFlags(Project& project, std::filesystem::path pathOffset, Language language) const;
    std::string getCompilerFlags(Project& project, std::filesystem::path pathOffset, Language language, const std::string& input, const std::string& output) const;
    std::string getLinker(Project& project, std::filesystem::path pathOffset) const;
    std::string getCommonLinkerFlags(Project& project, std::filesystem::path pathOffset) const;
    std::string getLinkerFlags(Project& project, std::filesystem::path pathOffset, const std::vector<std::string>& inputs, const std::string& output) const;
    std::vector<std::filesystem::path> process(Project& project, const std::filesystem::path& workingDir, const std::filesystem::path& dataDir) const override;
};
