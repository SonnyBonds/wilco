#include "core/eventhandler.h"

static std::vector<EventHandler*>& getEventHandlers()
{
    static std::vector<EventHandler*> EventHandlers;
    return EventHandlers;
}

void EventHandlers::install(EventHandler* EventHandler)
{
    getEventHandlers().push_back(EventHandler);
}

const std::vector<EventHandler*>& EventHandlers::list()
{
    return getEventHandlers();
}

EventHandler::EventHandler()
{
    EventHandlers::install(this);
}
