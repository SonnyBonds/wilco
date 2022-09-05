#pragma once

#include <vector>

#include "core/project.h"
#include "core/os.h"
#include "core/stringid.h"

struct EventHandler;
struct Environment;

struct EventHandlers
{
    static void install(EventHandler* eventHandler);
    static const std::vector<EventHandler*>& list();
};

struct EventHandler
{
    EventHandler();

    EventHandler(const EventHandler& other) = delete;
    EventHandler& operator=(const EventHandler& other) = delete;

    virtual void postResolve(Environment& env, const Project& project, ProjectSettings& resolvedSettings, std::optional<ProjectType> type, StringId configName, OperatingSystem targetOS)
    { }
};