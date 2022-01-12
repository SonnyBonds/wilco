#pragma once

#include <functional>

#include "core/option.h"
#include "core/project.h"

struct PostProcessor
{
    std::function<void(Project& project, ProjectConfig& resolvedConfig)> func;

    void operator ()(Project& project, ProjectConfig& resolvedConfig)
    {
        func(project, resolvedConfig);
    }

    bool operator ==(const PostProcessor& other) const
    {
        return _id == other._id;
    }

    bool operator <(const PostProcessor& other) const
    {
        return _id < other._id;
    }
private:
    static unsigned int getUniqueId()
    {
        static std::atomic_uint idCounter = 0;
        return idCounter.fetch_add(1);
    }

    // UINT_MAX ids ought to be enough for anybody...
    unsigned int _id = getUniqueId();
};

Option<std::vector<PostProcessor>> PostProcess{"PostProcess"};
