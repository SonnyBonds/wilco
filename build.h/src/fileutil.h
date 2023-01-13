#include <array>
#include <fstream>

inline std::string readFile(std::filesystem::path path)
{
    std::ifstream stream(path, std::ios::binary);
    if(!stream)
    {
        return {};
    }
    
    stream.seekg(0, std::ios_base::end);
    size_t size = stream.tellg();
    stream.seekg(0, std::ios_base::beg);

    std::string buffer;
    buffer.resize(size);
    stream.read(buffer.data(), size);
    return buffer;
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
                if(memcmp(data.data() + pos, buffer.data(), chunk) != 0)
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
    std::ofstream stream(path, std::ios::trunc | std::ios::binary);
    stream.write(data.data(), data.size());
    return true;
}