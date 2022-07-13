#pragma once

class Configurator;

struct Configurators
{
    static void install(Configurator* configurator)
    {
        getConfigurators().push_back(configurator);
    }

    static const std::vector<Configurator*>& list()
    {
        return getConfigurators();
    }

private:
    static std::vector<Configurator*>& getConfigurators()
    {
        static std::vector<Configurator*> configurators;
        return configurators;
    }
};

namespace cli
{
    struct Argument;
}

struct Configurator
{
    Configurator()
    {
        Configurators::install(this);
    }

    virtual void configure(Environment& environment) = 0;

    std::vector<cli::Argument*> arguments;
};