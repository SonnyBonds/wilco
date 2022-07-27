#pragma once

#include <string>
#include <vector>

#include "core/emitter.h"

class Query : public Emitter
{
public:
    static Query instance;

    cli::BoolArgument listProjects{arguments, "projects", "List all defined projects."};
    cli::BoolArgument listConfigs{arguments, "configs", "List all defined configurations."};

    Query();

    virtual void emit(Environment& env) override;
    void emitProjects(Environment& env);
    void emitConfigs(Environment& env);
};
