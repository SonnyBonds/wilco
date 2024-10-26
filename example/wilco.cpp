#include "wilco.h"

namespace arguments
{
    cli::StringArgument printerMessage{"printer-message", "Specify the message to print.", "Hello World!"};
    cli::BoolArgument optimize{"optimize", "Enable optimizations."};
    cli::BoolArgument debug{"debug", "Build with debug flags enabled."};
}

namespace profiles
{
    cli::Profile release("release", { "--optimize" });
    cli::Profile debug("debug", { "--debug" });
}

void configure(Environment& env)
{
    ProjectSettings defaults;
    defaults.features += { feature::Exceptions, feature::DebugSymbols };
    defaults.output.dir = "bin";
    if(arguments::optimize)
    {
        defaults.features += feature::Optimize;
    }

    if(arguments::debug)
    {
        defaults.output.suffix = "_d";
    }

    Project& helloPrinter = env.createProject("HelloPrinter", StaticLib);
    helloPrinter.import(defaults);
    helloPrinter.files += env.listFiles("hellolib");
    helloPrinter.defines += "MESSAGE=" + str::quote(std::string(*arguments::printerMessage));
	helloPrinter.exports.includePaths = "hellolib";

	Project& hello = env.createProject("Hello", Executable);
	hello.import(defaults);
	hello.import(helloPrinter); // Import adds all properties in the "exports" section of the imported project
    hello.files += env.listFiles("helloapp");
}
