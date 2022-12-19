#include "build.h"

cli::StringArgument printerMessage{"printer-message", "Specify the message to print.", "Hello World!"};

static StringId debug = "debug";
static StringId release = "release";

void configure(Environment& env)
{
    env.configurations = { debug, release };
    env.defaults.features += { feature::Exceptions, feature::DebugSymbols };
    env.defaults.features(release) += feature::Optimize;
    
    Project& helloPrinter = env.createProject("HelloLibrary", StaticLib);
    helloPrinter.output(debug) = "bin/helloprinter_d.a"; // Can easily be made into a function like ...output = generateOutputName(project)
    helloPrinter.output(release) = "bin/helloprinter.a"; // to get any wanted naming scheme
    helloPrinter.files += env.listFiles("hellolib");
    helloPrinter.defines += "MESSAGE=" + str::quote(std::string(*printerMessage));
    helloPrinter.includePaths += "hellolib";
    helloPrinter.exports.includePaths = helloPrinter.includePaths;
    helloPrinter.exports.libs += helloPrinter.output;

    Project& hello = env.createProject("Hello", Executable);
    hello.import(helloPrinter); // Import adds all properties in the "exports" section of the imported project
    hello.output(debug) = "bin/hello_d";
    hello.output(release) = "bin/hello";
    hello.files += env.listFiles("helloapp");
}
