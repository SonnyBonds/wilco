#pragma once

#include <filesystem>

#include "core/emitter.h"
#include "util/cli.h"

#ifndef CUSTOM_BUILD_H_MAIN

int main(int argc, const char** argv)
{
    int defaultMain(int argc, const char** argv);
    return defaultMain(argc, argv);
}
#endif