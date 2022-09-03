#include "build.h"

cli::StringArgument printerMessage{"printer-message", "Specify the message to print.", "Hello World!"};

static StringId debug = "debug";
static StringId release = "release";

void configure(Environment& env)
{
    env.defaults(Public).output.dir = "bin";
    env.defaults(Public, debug).output.suffix = "Debug";
    env.defaults(Public).features += { feature::Exceptions, feature::DebugSymbols };
    env.defaults(Public, release).features += feature::Optimize;
    
    Project& helloPrinter = env.createProject("HelloLibrary", StaticLib);
    helloPrinter.files += env.listFiles("hellolib");
    helloPrinter.defines += "MESSAGE=" + str::quote(*printerMessage);
    helloPrinter(Public).includePaths += "hellolib";

    Project& hello = env.createProject("Hello", Executable);
    hello.links += &helloPrinter;
    hello.files += env.listFiles("helloapp");

}
