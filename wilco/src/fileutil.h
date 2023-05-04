#include <array>
#include <fstream>
#include <iostream>
#include <cstring>

inline std::string readFile(std::filesystem::path path)
{
    // Turns out C-style file reading for various reasons
    // is a lot faster than std::fstream on MSVC's CRT. 
#if 1
    FILE* file = fopen(path.string().c_str(), "rb");
    if (!file)
    {
        return {};
    }
    fseek(file, 0, SEEK_END);
    auto size = ftell(file);
    fseek(file, 0, SEEK_SET);
    std::string buffer;
    buffer.resize(size);
    if (fread(buffer.data(), size, 1, file) != 1)
    {
        buffer.clear();
    }
    fclose(file);
    return buffer;
#else
    std::ifstream stream(path, std::ios::binary);
    if(!stream)
    {
        return {};
    }
    stream.sync_with_stdio(false);
    stream.seekg(0, std::ios_base::end);
    size_t size = stream.tellg();
    stream.seekg(0, std::ios_base::beg);

    std::string buffer;
    buffer.resize(size);
    stream.read(buffer.data(), size);
    return buffer;
#endif
}

inline bool writeFile(std::filesystem::path path, const std::string& data, bool onlyWriteIfDifferent = true)
{
    if(onlyWriteIfDifferent)
    {
        std::error_code ec;
        size_t fileSize = std::filesystem::file_size(path, ec);
        if(!ec && fileSize == data.size())
        {
            std::ifstream inputStream(path, std::ios::binary);
            std::array<char, 2048> buffer;
            size_t pos = 0;
            while(inputStream.good())
            {
                size_t chunk = std::min(buffer.size(), fileSize - pos);
                inputStream.read(buffer.data(), chunk);
                if(!inputStream)
                {
                    break;
                }
                if(std::memcmp(data.data() + pos, buffer.data(), chunk) != 0)
                {
                    break;
                }
                pos += chunk;
                if(pos == fileSize)
                {
                    return false;
                }
            }
        }
    }

    if(path.has_parent_path())
    {
        std::filesystem::create_directories(path.parent_path());
    }
#if 1
    FILE* file = fopen(path.string().c_str(), "wb");
    if (!file)
    {
        throw std::system_error(errno, std::generic_category(), "Failed to open file \"" + path.string() + "\" for writing.");
    }
    // fwrite (on msvc at least) returns zero elements written if the element size is zero
    if(data.size() > 0)
    {
        if (fwrite(data.data(), data.size(), 1, file) != 1)
        {
            throw std::system_error(errno, std::generic_category(), "Failed to write file \"" + path.string() + "\".");
        }
    }
    fclose(file);
    return true;
#else
    std::ofstream stream(path, std::ios::trunc | std::ios::binary);
    stream.write(data.data(), data.size());
#endif
    return true;
}