#include <assert.h>

#include "core/project.h"
#include "core/eventhandler.h"

Project::Project(std::string name, std::optional<ProjectType> type)
    : name(std::move(name))
    , type(type)
{ }

Project::~Project()
{ }

ProjectSettings& ProjectSettings::operator +=(const ProjectSettings& other)
{
    auto apply = [](PropertyBag& base, const PropertyBag& overlay)
    {
        // This whole thing doesn't have enforced safety,
        // it just assumes caller knows what it's doing
        assert(base.properties.size() == overlay.properties.size());
        for(size_t i=0; i<base.properties.size(); ++i)
        {
#if TODO
            base.properties[i]->applyOverlay(*overlay.properties[i]);
#endif
        }
    };

    apply(*this, other);

#if TODO
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
#endif

    return *this;
}

ProjectSettings ProjectSettings::operator+(const ProjectSettings& other) const
{
    ProjectSettings result;
    result += *this;
    result += other;
    return result;
}
