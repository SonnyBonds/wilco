#include <assert.h>

#include "core/project.h"
#include "core/eventhandler.h"

Project::Project(std::string name, ProjectType type)
    : name(std::move(name))
    , type(type)
{ }

Project::~Project()
{ }

void ProjectSettings::import(const ProjectSettings& other)
{
    auto apply = [](PropertyBag& base, const PropertyBag& overlay)
    {
        // This whole thing doesn't have enforced safety,
        // it just assumes caller knows what it's doing
        assert(base.properties.size() == overlay.properties.size());
        for(size_t i=0; i<base.properties.size(); ++i)
        {
            base.properties[i]->import(*overlay.properties[i]);
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
}

void Project::import(const Project& other, bool reexport)
{
    ProjectSettings::import(other.exports);
    if(reexport)
    {
        exports.import(other.exports);
    }
}