#pragma once

#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "core/action.h"
#include "core/project.h"
#include "core/stringid.h"
#include "modules/command.h"
#include "modules/toolchain.h"
#include "toolchains/detected.h"

class NinjaEmitter : public Action
{
public:
    static ActionInstance<NinjaEmitter> instance;

    NinjaEmitter();

    virtual void run(Environment& env) override;
};
