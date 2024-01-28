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

        virtual void reset() override;

        std::vector<std::string> values;
    };

public:
    static ActionInstance<DirectBuilder> instance;

    cli::BoolArgument verbose{arguments, "verbose", "Display full command line of commands as they are executed."};
    cli::BoolArgument displayTime{arguments, "display-time", "Display total build time after finishing a build."};
    TargetArgument targets{arguments};

    DirectBuilder();

    static void buildSelf(cli::Context cliContext);

    virtual void run(cli::Context cliContext) override;

    static std::filesystem::path getSelfBuildDatabasePath();
};
