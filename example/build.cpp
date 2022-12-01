#include "build.h"

cli::StringArgument printerMessage{"printer-message", "Specify the message to print.", "Hello World!"};

static StringId debug = "debug";
static StringId release = "release";

std::string suffix(StringId config)
{
    return config == debug ? "_d" : "";
}

std::string extension(ProjectType type, OperatingSystem os = OperatingSystem::current())
{
    if(os == Windows)
    {
        if(type == Executable) return ".exe";
        else if(type == StaticLib) return ".lib";
        else if(type == SharedLib) return ".dll";
        else return {};
    }
    else
    {
        if(type == Executable) return {};
        else if(type == StaticLib) return ".a";
        else if(type == SharedLib) return ".so";
        else return {};
    }

    return {};
}

void configure(Environment& env)
{
    /*auto configs = { debug, release };
    env.defaults(Public).output.dir = "bin";
    env.defaults(Public).features += { feature::Exceptions, feature::DebugSymbols };
    env.defaults(release, Public).features += feature::Optimize;*/
    
    Project& helloPrinter = env.createProject("HelloLibrary", StaticLib);
    helloPrinter.output(debug) = "bin/hellolib_d" + extension(StaticLib);
    helloPrinter.output(release) = "bin/hellolib" + extension(StaticLib);
    helloPrinter.files += env.listFiles("hellolib");
    helloPrinter.defines += "MESSAGE=" + str::quote(std::string(*printerMessage));
    helloPrinter.includePaths += "hellolib";

    Project& hello = env.createProject("Hello", Executable);
    //hello.links += &helloPrinter;
    hello.output(debug) = "bin/hello_d" + extension(Executable);
    hello.output(release) = "bin/hello" + extension(Executable);
    hello.files += env.listFiles("helloapp");
    hello.includePaths += "helloapp";
}
