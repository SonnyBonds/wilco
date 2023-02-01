#pragma once

#include <string>
#include <vector>

#include "core/emitter.h"

class Query : public Emitter
{
public:
    static EmitterInstance<Query> instance;

    cli::BoolArgument listProjects{arguments, "projects", "List all defined projects."};
    cli::BoolArgument listProfiles{arguments, "profiles", "List all defined profiles."};

    Query();

    virtual void emit(Environment& env) override;
    void emitProjects(Environment& env);
    void emitProfiles(Environment& env);
};
