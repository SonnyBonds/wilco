#pragma once

#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "core/emitter.h"
#include "core/project.h"
#include "core/stringid.h"
#include "modules/command.h"
#include "modules/postprocess.h"
#include "modules/toolchain.h"
#include "toolchains/detected.h"
#include "util/string.h"

class MsvcEmitter : public Emitter
{
public:
    static MsvcEmitter instance;

    MsvcEmitter();

    virtual void emit(Environment& env) override;
};
