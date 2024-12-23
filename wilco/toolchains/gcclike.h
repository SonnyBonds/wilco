#pragma once

#include <filesystem>
#include <string>

#include "core/project.h"
#include "modules/command.h"
#include "modules/language.h"
#include "modules/feature.h"
#include "modules/toolchain.h"
#include "util/string.h"

namespace extensions
{
    struct Gcc
    {
        ListPropertyValue<std::string> compilerFlags;
        ListPropertyValue<std::string> linkerFlags;
        ListPropertyValue<std::string> archiverFlags;
    
        struct Pch
        {
            std::filesystem::path build;
            std::filesystem::path use;
            ListPropertyValue<std::filesystem::path> ignoredFiles;
        } pch;

        virtual void import(const Gcc& other)
        {
            compilerFlags += other.compilerFlags;
            linkerFlags += other.linkerFlags;
            archiverFlags += other.archiverFlags;

            if(!other.pch.build.empty()) pch.build = other.pch.build;
            if(!other.pch.use.empty()) pch.use = other.pch.use;
            pch.ignoredFiles += other.pch.ignoredFiles;
        }
    };
}

struct GccLikeToolchainProvider : public ToolchainProvider
{
    std::string compiler;
    std::string resourceCompiler;
    std::string linker;
    std::string archiver;

    GccLikeToolchainProvider(std::string name, std::string compiler, std::string resourceCompiler, std::string linker, std::string archiver);

    std::string getCompiler(Project& project, std::filesystem::path pathOffset, Language language) const;
    std::string getCommonCompilerFlags(Project& project, Architecture arch, std::filesystem::path pathOffset, Language language, bool pch) const;
    std::string getCompilerFlags(Project& project, Architecture arch, std::filesystem::path pathOffset, Language language, const std::string& input, const std::string& output) const;
    std::string getLinker(Project& project, std::filesystem::path pathOffset) const;
    std::string getCommonLinkerFlags(Project& project, Architecture arch, std::filesystem::path pathOffset) const;
    std::string getLinkerFlags(Project& project, Architecture arch, std::filesystem::path pathOffset, const std::vector<std::string>& inputs, const std::string& output) const;
	void process(Project& project, const std::filesystem::path& workingDir, const std::filesystem::path& dataDir) const override;
};
