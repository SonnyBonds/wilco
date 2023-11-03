#include "core/action.h"
#include "util/commands.h"
#include "util/process.h"


static std::filesystem::path getWilcoCachePath()
{
    auto currentModulePath = process::findCurrentModulePath();
    if(currentModulePath.has_parent_path())
    {
        return std::filesystem::proximate(currentModulePath.parent_path() / ".wilcofiles", ".");
    }
    else
    {
        return ".wilcofiles";
    }
}

cli::PathArgument wilcoFilesPath{"wilco-cache-path", "Target path for build files related to the build configuration itself.", getWilcoCachePath()};
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

