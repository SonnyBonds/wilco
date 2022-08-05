#include <assert.h>

#include "core/project.h"
#include "core/eventhandler.h"

Project::Project(std::string name, std::optional<ProjectType> type)
    : name(std::move(name))
    , type(type)
{ }

Project::~Project()
{ }

ProjectSettings Project::resolve(StringId configName, OperatingSystem targetOS)
{
    ProjectSettings result;
    internalResolve(result, type, configName, targetOS, true);

    for(auto eventHandler : EventHandlers::list())
    {
        eventHandler->postResolve(result, type, configName, targetOS);
    }
    return result;
}

std::filesystem::path Project::calcOutputPath(ProjectSettings& resolvedSettings)
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

void Project::internalResolve(ProjectSettings& result, std::optional<ProjectType> projectType, StringId configName, OperatingSystem targetOS, bool local)
{
    for(auto& link : links)
    {
        link->internalResolve(result, projectType, configName, targetOS, false);
    }

    if(local)
    {
        result += *this;
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

        result += entry.second;
    }
}

ProjectSettings& ProjectSettings::operator +=(const ProjectSettings& other)
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

    apply(*this, other);

    for(auto& extension : other._extensions)
    {
        auto it = _extensions.find(extension.first);
        if(it != _extensions.end())
        {
            apply(it->second->get(), extension.second->get());
        }
        else
        {
            _extensions.insert({extension.first, extension.second->clone()});
        }
    }

    return *this;
}

ProjectSettings ProjectSettings::operator+(const ProjectSettings& other) const
{
    ProjectSettings result;
    result += *this;
    result += other;
    return result;
}
