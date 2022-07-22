#pragma once

#include <filesystem>
#include <functional>
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

struct Extension;
struct Project;

struct ProjectSettings : public PropertyBag
{
    ListProperty<Project*> links{this};
    ListProperty<CommandEntry> commands{this};
    Property<StringId> platform{this};
    ListProperty<std::filesystem::path> includePaths{this};
    ListProperty<std::filesystem::path> libPaths{this};
    ListProperty<SourceFile> files{this};
    ListProperty<std::filesystem::path> generatorDependencies{this};
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

private:
    struct ExtensionEntry
    {
        virtual ~ExtensionEntry() = default;
        virtual Extension& get() = 0;
        virtual std::unique_ptr<ExtensionEntry> clone() = 0;
    };

    template<typename ExtensionType>
    struct ExtensionEntryImpl : public ExtensionEntry
    {
        virtual Extension& get() override
        {
            return extension;
        }

        virtual std::unique_ptr<ExtensionEntry> clone() override
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

struct Extension : public PropertyBag
{ };


struct Project : public ProjectSettings
{
    const std::string name;
    const std::optional<ProjectType> type;

    std::map<ConfigSelector, ProjectSettings> configs;

    Project(std::string name, std::optional<ProjectType> type)
        : name(std::move(name))
        , type(type)
    { }

    Project(const Project& other) = delete;

    ~Project()
    { }

    ProjectSettings resolve(StringId configName, OperatingSystem targetOS)
    {
        ProjectSettings result;
        internalResolve(result, type, configName, targetOS, true);
        return result;
    }

    template<typename... Selectors>
    ProjectSettings& operator()(ConfigSelector selector, Selectors... selectors)
    {
        return configs[(selector + ... + ConfigSelector(selectors))];
    }

    std::filesystem::path calcOutputPath(ProjectSettings& resolvedSettings)
    {
        if(!resolvedSettings.output.path.value().empty())
        {
            return resolvedSettings.output.path;
        }

        std::string stem = resolvedSettings.output.stem;
        if(stem.empty())
        {
            stem = name;
        }

        return resolvedSettings.output.dir.value() / (resolvedSettings.output.prefix.value() + stem + resolvedSettings.output.suffix.value() + resolvedSettings.output.extension.value());
    }

private:
    void internalResolve(ProjectSettings& result, std::optional<ProjectType> projectType, StringId configName, OperatingSystem targetOS, bool local)
    {
        for(auto& link : links)
        {
            link->internalResolve(result, projectType, configName, targetOS, false);
        }

        if(local)
        {
            applyOverlay(result, *this);
        }

        for(auto& entry : configs)
        {
            if(local)
            {
                if(entry.first.transitivity && entry.first.transitivity == PublicOnly) continue;
            }
            else
            {
                if(!entry.first.transitivity || entry.first.transitivity == Local) continue;
            }
            if(entry.first.projectType && entry.first.projectType != projectType) continue;
            if(entry.first.name && entry.first.name != configName) continue;
            if(entry.first.targetOS && entry.first.targetOS != targetOS) continue;

            applyOverlay(result, entry.second);
        }
    }
    
    void applyOverlay(ProjectSettings& base, const ProjectSettings& overlay)
    {
        auto apply = [](PropertyBag& base, const PropertyBag& overlay)
        {
            // This whole thing doesn't have enforced safety,
            // it just assumes caller knows what it's doing
            assert(base.properties.size() == overlay.properties.size());
            for(size_t i=0; i<base.properties.size(); ++i)
            {
                base.properties[i]->applyOverlay(*overlay.properties[i]);
            }
        };

        apply(base, overlay);

        for(auto& extension : overlay._extensions)
        {
            auto it = base._extensions.find(extension.first);
            if(it != base._extensions.end())
            {
                apply(it->second->get(), extension.second->get());
            }
            else
            {
                base._extensions.insert({extension.first, extension.second->clone()});
            }
        }
    }
};
