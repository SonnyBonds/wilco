#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "core/os.h"
#include "core/property.h"
#include "core/stringid.h"
#include "modules/command.h"
#include "modules/feature.h"
#include "modules/toolchain.h"
#include "modules/sourcefile.h"

struct Environment;
struct Project;

struct ProjectSettings : public PropertyBag
{
    ListProperty<Project*> links{this};
    ListProperty<CommandEntry> commands{this};
    Property<StringId> platform{this};
    ListProperty<std::filesystem::path> includePaths{this};
    ListProperty<std::filesystem::path> libPaths{this};
    ListProperty<SourceFile> files{this};
    ListProperty<std::filesystem::path> libs{this};
    ListProperty<std::string> defines{this};
    ListProperty<Feature> features{this};
    ListProperty<std::string> frameworks{this};
    Property<std::filesystem::path> buildPch{this};
    Property<std::filesystem::path> importPch{this};
    Property<std::filesystem::path> dataDir{this};
    Property<const ToolchainProvider*> toolchain{this};

    struct Output : public PropertyGroup
    {
        Property<std::filesystem::path> path{this};
        Property<std::filesystem::path> dir{this};
        Property<std::string> stem{this};
        Property<std::string> extension{this};
        Property<std::string> prefix{this};
        Property<std::string> suffix{this};
    } output{this};

    template<typename ExtensionType>
    ExtensionType& ext()
    {
        // TODO: Better key, maybe? hash_code doesn't have to be unique.
        auto key = typeid(ExtensionType).hash_code();

        auto it = _extensions.find(key);
        if(it != _extensions.end())
        {
            return static_cast<ExtensionType&>(it->second->get());
        }
        ExtensionEntry* extensionEntry = new ExtensionEntryImpl<ExtensionType>();
        _extensions.insert({key, std::unique_ptr<ExtensionEntry>(extensionEntry)});
        return static_cast<ExtensionType&>(extensionEntry->get());
    }

    template<typename ExtensionType>
    bool hasExt() const
    {
        // TODO: Better key, maybe? hash_code doesn't have to be unique.
        auto key = typeid(ExtensionType).hash_code();

        return _extensions.find(key) != _extensions.end();
    }

    ProjectSettings& operator +=(const ProjectSettings& other);

    ProjectSettings operator+(const ProjectSettings& other) const;

    ProjectSettings()
    { }

    ProjectSettings(const ProjectSettings& other)
    {
        *this += other;
    }
    
private:
    struct ExtensionEntry
    {
        virtual ~ExtensionEntry() = default;
        virtual PropertyBag& get() = 0;
        virtual const PropertyBag& get() const = 0;
        virtual std::unique_ptr<ExtensionEntry> clone() const = 0;
    };

    template<typename ExtensionType>
    struct ExtensionEntryImpl : public ExtensionEntry
    {
        virtual PropertyBag& get() override
        {
            return extension;
        }

        virtual const PropertyBag& get() const override
        {
            return extension;
        }

        virtual std::unique_ptr<ExtensionEntry> clone() const override
        {
            auto newExtension = new ExtensionEntryImpl<ExtensionType>();
            for(size_t i=0; i<extension.properties.size(); ++i)
            {
                newExtension->extension.properties[i]->applyOverlay(*extension.properties[i]);
            }
            return std::unique_ptr<ExtensionEntry>(newExtension);
        }

        ExtensionType extension;
    };
    
    std::map<size_t, std::unique_ptr<ExtensionEntry>> _extensions;

    friend class Project;
};

struct Project : public ProjectSettings
{
    const std::string name;
    const std::optional<ProjectType> type;

    std::map<ConfigSelector, ProjectSettings> configs;

    Project(std::string name, std::optional<ProjectType> type);
    Project(const Project& other) = delete;
    ~Project();

    ProjectSettings resolve(Environment& env, std::filesystem::path suggestedDataDir, StringId configName, OperatingSystem targetOS) const;

    template<typename... Selectors>
    ProjectSettings& operator()(ConfigSelector selector, Selectors... selectors)
    {
        return configs[(selector + ... + ConfigSelector(selectors))];
    }

    std::filesystem::path calcOutputPath(ProjectSettings& resolvedSettings) const;

private:
    void internalResolve(ProjectSettings& result, std::optional<ProjectType> projectType, StringId configName, OperatingSystem targetOS, bool local) const;
};
