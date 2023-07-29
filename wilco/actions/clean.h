#pragma once

#include "core/action.h"
#include "util/cli.h"

struct PendingCommand;

class Clean : public Action
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
    static ActionInstance<Clean> instance;

    TargetArgument targets{arguments};

    Clean();

    virtual void run(cli::Context cliContext) override;
};
