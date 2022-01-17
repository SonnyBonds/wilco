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

bool write(std::filesystem::path path, const std::string& data)
{
    bool upToDate = false;
    std::error_code ec;
    size_t fileSize = 0;
    std::filesystem::file_size(path, ec);
    if(!ec && fileSize != data.size())
    {
        std::ifstream inputStream(path);
        std::array<char, 2048> buffer;
        size_t pos = 0;
        while(inputStream.good())
        {
            size_t chunk = std::min(buffer.size(), fileSize - pos);
            inputStream.read(buffer.data(), chunk);
            if(!inputStream.good())
            {
                break;
            }
            if(memcmp(data.data() + pos, buffer.data(), chunk) != 0)
            {
                break;
            }
            if(pos == fileSize)
            {
                return false;
            }
        }
    }

    std::ofstream stream(path);
    stream.write(data.data(), data.size());
    return true;
}

}