#include "build.h"

cli::StringArgument printerMessage{"printer-message", "Specify the message to print.", "Hello World!"};

static StringId debug = "debug";
static StringId release = "release";

void setup(Environment& env)
{
    env.configurations = { debug, release };
}

void configure(Environment& env, Configuration& config)
{
    ProjectSettings defaults;
    defaults.features += { feature::Exceptions, feature::DebugSymbols };
    defaults.output.dir = "bin";
    if(config.name == release)
    {
        defaults.features += feature::Optimize;
    }

    if(config.name == debug)
    {
        defaults.output.suffix = "_d";
    }

    Project& helloPrinter = config.createProject("HelloPrinter", StaticLib);
    helloPrinter.import(defaults);
    helloPrinter.files += env.listFiles("hellolib");
    helloPrinter.defines += "MESSAGE=" + str::quote(std::string(*printerMessage));
    helloPrinter.exports.includePaths = "hellolib";
    helloPrinter.exports.libs += helloPrinter.output;

    Project& hello = config.createProject("Hello", Executable);
    hello.import(defaults);
    hello.import(helloPrinter); // Import adds all properties in the "exports" section of the imported project
    hello.files += env.listFiles("helloapp");
}
