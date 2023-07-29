#include "core/action.h"
#include "util/commands.h"

cli::PathArgument targetPath{"build-path", "Target path for build files.", "buildfiles"};
cli::BoolArgument noRebuild{"no-self-update", "Don't rebuild the builder itself even if it has changed."};

static std::vector<Action*>& getActions()
{
    static std::vector<Action*> actions;
    return actions;
}

void Actions::install(Action* action)
{
    getActions().push_back(action);
}

const std::vector<Action*>& Actions::list()
{
    return getActions();
}

Action::Action(std::string name, std::string description)
    : name(name)
    , description(std::move(description))
{
}

