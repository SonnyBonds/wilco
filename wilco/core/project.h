#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "core/os.h"
#include "core/property.h"
#include "modules/command.h"
#include "modules/feature.h"
#include "modules/toolchain.h"
#include "modules/sourcefile.h"

struct Environment;
struct Project;

enum ProjectType
{
    Executable,
    StaticLib,
    SharedLib,
    Command
};

inline std::string outputExtension(ProjectType type, OperatingSystem os = OperatingSystem::current())
{
    if(os == Windows)
    {
        if(type == Executable) return ".exe";
        else if(type == StaticLib) return ".lib";
        else if(type == SharedLib) return ".dll";
        else return {};
    }
    else
    {
        if(type == Executable) return {};
        else if(type == StaticLib) return ".a";
        else if(type == SharedLib) return ".so";
        else return {};
    }

    return {};
}

struct ProjectSettings
{
    ListPropertyValue<CommandEntry> commands;
    ListPropertyValue<std::filesystem::path> includePaths;
    ListPropertyValue<std::filesystem::path> libPaths;
    ListPropertyValue<SourceFile> files;
    ListPropertyValue<std::filesystem::path> libs;
    ListPropertyValue<std::filesystem::path> systemLibs;
    ListPropertyValue<std::string> defines;
    ListPropertyValue<Feature> features;
    ListPropertyValue<std::string> frameworks;
    std::filesystem::path dataDir;
    ListPropertyValue<const Project*> dependencies;
    const ToolchainProvider* toolchain = nullptr;

    struct Output
    {
        std::filesystem::path dir;
        std::string prefix;
        std::string name;
        std::string suffix;
        std::string extension;

        std::filesystem::path fullPath() const
        {
            return dir / (prefix + name + suffix + extension);
        }

        operator std::filesystem::path() const
        {
            return fullPath();
        }

        Output& operator =(std::filesystem::path path)
        {
            dir = path.parent_path();
            prefix = "";
            name = path.stem().string();
            suffix = "";
            extension = path.extension().string();
            return *this;
        }
    } output;

    void import(const ProjectSettings& other)
    {
        commands += other.commands;
        includePaths += other.includePaths;
        libPaths += other.libPaths;
        files += other.files;
        libs += other.libs;
        systemLibs += other.systemLibs;
        defines += other.defines;
        features += other.features;
        frameworks += other.frameworks;
        if(!other.dataDir.empty()) dataDir = other.dataDir;
        if(other.toolchain) toolchain = other.toolchain;
        dependencies += other.dependencies;

        if(!other.output.dir.empty()) output.dir = other.output.dir;
        if(!other.output.prefix.empty()) output.prefix = other.output.prefix;
        if(!other.output.name.empty()) output.name = other.output.name;
        if(!other.output.suffix.empty()) output.suffix = other.output.suffix;
        if(!other.output.extension.empty()) output.extension = other.output.extension;
        
        importExtensions(other);
    }
    
    template<typename ExtensionType>
    ExtensionType& ext()
    {
        // TODO: Better key, maybe? hash_code doesn't have to be unique.
        auto key = typeid(ExtensionType).hash_code();

        auto it = _extensions.find(key);
        if(it != _extensions.end())
        {
            return static_cast<ExtensionEntryImpl<ExtensionType>*>(it->second.get())->extensionData;
        }
        auto extensionEntry = new ExtensionEntryImpl<ExtensionType>();
        _extensions.insert({key, std::unique_ptr<ExtensionEntry>(extensionEntry)});
        return extensionEntry->extensionData;
    }

    template<typename ExtensionType>
    bool hasExt() const
    {
        // TODO: Better key, maybe? hash_code doesn't have to be unique.
        auto key = typeid(ExtensionType).hash_code();

        return _extensions.find(key) != _extensions.end();
    }

    ProjectSettings()
    { }

    ProjectSettings(const ProjectSettings& other)
    {
        import(other);
    }
    
private:
    void importExtensions(const ProjectSettings& other);

    struct ExtensionEntry
    {
        virtual ~ExtensionEntry() = default;
        virtual void import(const ExtensionEntry& other) = 0;
        virtual std::unique_ptr<ExtensionEntry> clone() const = 0;
    };

    template<typename ExtensionType>
    struct ExtensionEntryImpl : public ExtensionEntry
    {
        void import(const ExtensionEntry& other) override
        {
            extensionData.import(static_cast<const ExtensionEntryImpl&>(other).extensionData);
        }

        virtual std::unique_ptr<ExtensionEntry> clone() const override
        {
            auto newExtension = new ExtensionEntryImpl<ExtensionType>();
            newExtension->extensionData.import(extensionData);
            return std::unique_ptr<ExtensionEntry>(newExtension);
        }

        ExtensionType extensionData;
    };
    
    std::map<size_t, std::unique_ptr<ExtensionEntry>> _extensions;
};

struct Project : public ProjectSettings
{
    const std::string name;
    const ProjectType type;
    ProjectSettings exports;

    Project(std::string name, ProjectType type);
    Project(const Project& other) = delete;
    ~Project();

    using ProjectSettings::import;
    void import(const Project& other, bool reexport = true);
};
