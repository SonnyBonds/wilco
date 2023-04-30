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
#include "util/string.h"

class MsvcEmitter : public Action
{
public:
    static ActionInstance<MsvcEmitter> instance;

    MsvcEmitter();

    virtual void run(cli::Context cliContext) override;
};
