#pragma once

#include <string>
#include <vector>

#include "core/action.h"

class Query : public Action
{
public:
    static ActionInstance<Query> instance;

    cli::BoolArgument listProjects{arguments, "projects", "List all defined projects."};
    cli::BoolArgument listProfiles{arguments, "profiles", "List all defined profiles."};

    Query();

    virtual void run(cli::Context cliContext) override;
};
