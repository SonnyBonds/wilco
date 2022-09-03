#pragma once

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/stringid.h"
#include "core/environment.h"
#include "core/emitter.h"
#include "core/project.h"
#include "core/stringid.h"
#include "modules/command.h"
#include "modules/toolchain.h"
#include "toolchains/detected.h"
#include "util/process.h"
#include "util/string.h"

class CompileCommands : public Emitter
{
public:
    static CompileCommands instance;

    CompileCommands();

    virtual void emit(Environment& env) override;

private:
    static void emitCommands(Environment& env, std::ostream& stream, const std::filesystem::path& root, Project& project, StringId config, bool first);
};
