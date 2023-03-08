#pragma once

#include "core/action.h"

class Configure : public Action
{
private:
    struct ProfileArgument : public cli::Argument
    {
        ProfileArgument(std::vector<cli::Argument*>& argumentList);

        virtual void extract(std::vector<std::string>& values) override;

        virtual void reset() override;
    };

public:
    ProfileArgument profile{arguments};

    static ActionInstance<Configure> instance;

    Configure();

    virtual void run(cli::Context cliContext);
};