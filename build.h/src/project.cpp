#include <assert.h>

#include "core/project.h"
#include "core/eventhandler.h"

Project::Project(std::string name, ProjectType type)
    : name(name)
    , type(type)
{
    output.name = std::move(name);
    output.extension = outputExtension(type);
}

Project::~Project()
{ }

void ProjectSettings::importExtensions(const ProjectSettings& other)
{
    for(auto& extension : other._extensions)
    {
        auto it = _extensions.find(extension.first);
        if(it != _extensions.end())
        {
            it->second->import(*extension.second);
        }
        else
        {
            _extensions.insert({extension.first, extension.second->clone()});
        }
    }
}

void Project::import(const Project& other, bool reexport)
{
    dependencies += &other;
    ProjectSettings::import(other.exports);
    if(reexport)
    {
        exports.import(other.exports);
    }
}