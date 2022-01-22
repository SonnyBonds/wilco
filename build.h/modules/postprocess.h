#pragma once

#include <functional>

#include "core/option.h"
#include "core/project.h"

struct PostProcessor
{
    std::function<void(Project& project, OptionCollection& resolvedOptions)> func;

    void operator ()(Project& project, OptionCollection& resolvedOptions)
    {
        func(project, resolvedOptions);
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

    friend struct std::hash<PostProcessor>;
};

template<>
struct std::hash<PostProcessor>
{
    size_t operator()(const PostProcessor& processor)
    {
        return processor._id;
    }
};

Option<std::vector<PostProcessor>> PostProcess{"PostProcess"};
