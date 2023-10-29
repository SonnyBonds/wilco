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

/** Combines a number of path partials into a full path.
    The full path is [dir][prefix][name][suffix][extension].
    This makes it possible to have sensible defaults for the output.
*/
struct PathBuilder
{
    /** Base directory of the output path. */
    std::filesystem::path dir;
    /** Prefix to add to the start of the output filename. */
    std::string prefix;
    /** Stem of the output file name. */
    std::string name;
    /** Suffix to add to the end of the output filename. */
    std::string suffix;
    /** Extension of the output filename, including dot. */
    std::string extension;

    /** Returns the full combined result of the path partials. */
    std::filesystem::path fullPath() const
    {
        return dir / (prefix + name + suffix + extension);
    }

    /** Returns the full combined result of the path partials. */
    operator std::filesystem::path() const
    {
        return fullPath();
    }

    /** Assigns a full path to the output, separating it into the appropriate partials. */
    PathBuilder& operator =(std::filesystem::path path)
    {
        dir = path.parent_path();
        prefix = "";
        name = path.stem().string();
        suffix = "";
        extension = path.extension().string();
        return *this;
    }

    void import(const PathBuilder& other)
    {
        if(!other.dir.empty()) dir = other.dir;
        if(!other.prefix.empty()) prefix = other.prefix;
        if(!other.name.empty()) name = other.name;
        if(!other.suffix.empty()) suffix = other.suffix;
        if(!other.extension.empty()) extension = other.extension;
    }
};

/**
    Properties describing a Project.
*/
struct ProjectSettings
{
    /** List of commands to execute when building this project.
        The toolchain will translate files into commands, but this
        can be used either for additional commands or in non-toolchain
        based command projects.
    */
    ListPropertyValue<CommandEntry> commands;
    /** List of include search paths. */
    ListPropertyValue<std::filesystem::path> includePaths;
    /** List of library search paths. */
    ListPropertyValue<std::filesystem::path> libPaths;
    /** List of source files to compile.
        These files are processed by the toolchain if applicable,
        or added to the project of project generator actions like msvc.*/
    ListPropertyValue<SourceFile> files;
    /** List of libraries to link with, resolved as a path to a file. */
    ListPropertyValue<std::filesystem::path> libs;
    /** List of libraries to link with, resolved using library search paths. */
    ListPropertyValue<std::filesystem::path> systemLibs;
    /** List of preprocessor defines to define when compiling. 
        Typically an option like ENABLE_FEATURE_ABC or a value
        like VERSION_STRING="1.2.3". */
    ListPropertyValue<std::string> defines;
    /** List of features to enable. */
    ListPropertyValue<Feature> features;
    /** List of macOS frameworks to link. */
    ListPropertyValue<std::string> frameworks;
    /** Path to directory to use for intermediate files.
        TODO: This is not properly functional. */
    std::filesystem::path dataDir;
    /** List of projects that this project are dependent on.
        Typically command dependencies are resolved on a file level,
        but for some build actions (e.g. msvc) this can be used for explicit
        project dependency information. */
    ListPropertyValue<const Project*> dependencies;
    /** Toolchain to use to build source files.
        If left set to nullptr the default toolchain will be used. */
    const ToolchainProvider* toolchain = nullptr;

    /** Path for the resulting output of this project, e.g. the output library or executable. */
    PathBuilder output;

    /** Imports another ProjectSettings, adding any properties set in the other ProjectSettings onto this. */
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

        output.import(other.output);
        
        importExtensions(other);
    }
    
    /** Fetches the extension of type ExtensionType in this ProjectSettings, adding it if it does not already exist. */
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

    /** Checks if an extension of type ExtensionType is present in this ProjectSettings. */
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

/**
    The main node in a build configuration, containing information on how to build a "Project".
    A Project is typically 1:1 to a build output, e.g. an executable or a library. It can however
    also be just a set of commands performing some build task.

    @see [ProjectSettings](/ProjectSettings)
*/
struct Project : public ProjectSettings
{
    /** The name of this project.
        The name will be used for things like specifying what to build, informational printouts and
        as the default stem of the output file name.
    */
    const std::string name;
    
    /** The type of this project, specifying what the expected output of the project is. */
    const ProjectType type;

    /** Properties to be imported by other projects importing this project.
        This typically contains libraries to link, include paths to set, etc. */
    ProjectSettings exports;

    Project(std::string name, ProjectType type);
    Project(const Project& other) = delete;
    ~Project();

    using ProjectSettings::import;
    /** Imports another Project, adding any properties set in the other Project's exports onto this.
        Optionally also adds the exported properties to the exports section of this Project. */
    void import(const Project& other, bool reexport = true);
};
