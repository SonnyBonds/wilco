#pragma once

#include <array>
#include <iostream>
#include <future>
#include <stdio.h>
#include <string>
#include <filesystem>
#include "core/os.h"

namespace process
{

struct ProcessResult
{
    int exitCode;
    std::string output;
};

std::filesystem::path findCurrentModulePath();
ProcessResult run(std::string command, bool echoOutput = false);

}