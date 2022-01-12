#pragma once

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace file
{

std::string read(std::filesystem::path path)
{
    std::ifstream stream(path);
    std::stringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

void write(std::filesystem::path path, const std::string& data)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path);
    stream.write(data.data(), data.size());
}

}