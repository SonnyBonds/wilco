#include "build.h"

static StringId debug = "debug";
static StringId release = "release";

class Example : public Configurator
{
    void configure(Environment& env)
    {
        Project& config = env.createProject();
        config[Public / debug][OutputSuffix] = "Debug";
        config[Public][Features] += feature::Exceptions;
        config[Public / debug][Features] += feature::DebugSymbols;
        config[Public / release][Features] += feature::Optimize;

        Project& helloPrinter = env.createProject("HelloLibrary", StaticLib);
        helloPrinter.links += { &config };
        helloPrinter += glob::files("hellolib");
        helloPrinter[Public][IncludePaths] += "hellolib";

        Project& hello = env.createProject("Hello", Executable);
        hello.links += { &helloPrinter };
        hello += glob::files("helloapp");
        hello[MacOS] += bundle();
    }
} example;