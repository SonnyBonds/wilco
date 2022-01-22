#include "build.h"

namespace fs = std::filesystem;

static StringId debug = "debug";
static StringId release = "release";

void generate(fs::path startPath, std::vector<std::string> args)
{
    Project config;
    config[Public][OutputDir] = "bin";
    config[Public / debug][OutputSuffix] = "Debug";
    config[Public / debug][Features] += feature::DebugSymbols;
    config[Public / release][Features] += feature::Optimize;

    Project helloPrinter("HelloLibrary", StaticLib);
    helloPrinter.links = { &config };
    helloPrinter += glob::sources("hellolib");
    helloPrinter[Public][IncludePaths] += "hellolib";

    Project hello("Hello", Executable);
    hello.links = { &helloPrinter };
    hello += glob::sources("helloapp");
    hello[MacOS] += bundle();

    cli::parseCommandLineAndEmit(startPath, args, {&hello}, {debug, release});
}