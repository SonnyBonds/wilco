#include "build.h"

static const char* debug = "debug";
static const char* release = "release";

void generate(fs::path startPath, std::vector<std::string> args)
{
    Project config;
    config[Public][OutputDir] = "bin";
    config[Public / debug][OutputSuffix] = "Debug";
    config[Public / debug][Features] += "debuginfo";
    config[Public / release][Features] += "optimize";

    Project helloPrinter("HelloLibrary", StaticLib);
    helloPrinter.links = { &config };
    helloPrinter[Files] += sourceList("hellolib");
    helloPrinter[Public][IncludePaths] += "hellolib";

    Project hello("Hello", Executable);
    hello.links = { &helloPrinter };
    hello[Files] += sourceList("helloapp");

    parseCommandLineAndEmit(startPath, args, {&hello}, {debug, release});
}