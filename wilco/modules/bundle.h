#pragma once

#include <functional>
#include <string>
#include <variant>

#include "core/project.h"
#include "core/property.h"
#include "modules/feature.h"
#include "util/commands.h"

namespace macos
{

#if TODO

struct VerbatimPlistValue
{
    std::string valueString;
};

using PlistValue = std::variant<std::string, bool, int, VerbatimPlistValue>;

#endif

struct BundleBuilder
{
    struct Item
    {
        std::filesystem::path source;
        std::filesystem::path target;
    };

    PathBuilder output;
    std::filesystem::path plistFile;
    std::vector<Item> items;

    void addBinary(const Project& sourceProject, std::optional<std::string_view> filenameOverride = {});
    void addBinary(std::filesystem::path binaryPath, std::optional<std::string_view> filenameOverride = {});
    void addResource(std::filesystem::path resourcePath, std::optional<std::string_view> filenameOverride = {});
    void addResource(std::filesystem::path resourcePath, std::filesystem::path targetPath, std::optional<std::string_view> filenameOverride = {});
    void addResources(Environment& env, std::filesystem::path resourceRoot);
    void addResources(Environment& env, std::filesystem::path resourceRoot, std::filesystem::path targetPath);
    void addGeneric(std::filesystem::path sourcePath, std::filesystem::path targetPath, std::optional<std::string_view> filenameOverride = {});

    CommandEntry generateCommand();
};

}