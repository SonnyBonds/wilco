#pragma once

#include <cstdlib>
#include <assert.h>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <set>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "core/action.h"
#include "core/project.h"
#include "core/stringid.h"
#include "modules/command.h"
#include "modules/toolchain.h"
#include "toolchains/detected.h"
#include "util/cli.h"
#include "util/process.h"
#include "util/string.h"

struct PendingCommand;

class DirectBuilder : public Action
{
private:
    struct TargetArgument : public cli::Argument
    {
        TargetArgument(std::vector<cli::Argument*>& argumentList);

        virtual void extract(std::vector<std::string>& values) override;

        std::vector<StringId> values;
    };

    struct ProfileArgument : public cli::Argument
    {
        ProfileArgument(std::vector<cli::Argument*>& argumentList);

        virtual void extract(std::vector<std::string>& values) override;
    };

public:
    static ActionInstance<DirectBuilder> instance;

    ProfileArgument profile{arguments};
    cli::BoolArgument verbose{arguments, "verbose", "Display full command line of commands as they are executed."};
    TargetArgument targets{arguments};

    DirectBuilder();

    static void buildSelf(cli::Context cliContext, Environment& outputEnv);

    virtual void run(Environment& env) override;
};
