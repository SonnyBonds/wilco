#include "build.h"

static StringId debug = "debug";
static StringId release = "release";

void generate(fs::path startPath, std::vector<std::string> args)
{
    Project config;
    config[Public][OutputDir] = "bin";
    config[Public / debug][OutputSuffix] = "Debug";
    config[Public / debug][Features] += "debuginfo";
    config[Public / release][Features] += "optimize";

    Project helloPrinter("HelloLibrary", StaticLib);
    helloPrinter.links = { &config };
    helloPrinter += sourceList("hellolib");
    helloPrinter[Public][IncludePaths] += "hellolib";

    Project hello("Hello", Executable);
    hello.links = { &helloPrinter };
    hello += sourceList("helloapp");

    parseCommandLineAndEmit(startPath, args, {&hello}, {debug, release});
}