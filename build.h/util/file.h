#pragma once

#include <array>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace file
{

inline std::string read(std::filesystem::path path)
{
    std::ifstream stream(path);
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

inline bool write(std::filesystem::path path, const std::string& data)
{
    bool upToDate = false;
    std::error_code ec;
    size_t fileSize = std::filesystem::file_size(path, ec);
    if(!ec && fileSize == data.size())
    {
        std::ifstream inputStream(path);
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

    if(path.has_parent_path())
    {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream stream(path);
    stream.write(data.data(), data.size());
    return true;
}

#if 0
struct MappedFile
{
    MappedFile(MappedFile&& other)
        : data(other.data)
        , size(other.size)
    {
        data = other.data;
        size = other.size;
        other.data = nullptr;
    } 

    ~MappedFile()
    {
        if(data)
        {
            munmap(data, size);
        }
    }
}

template<typename T = char>
std::unique_ptr<T> map(const std::filesystem::path& path)
{
    template<typename T>
    struct Destructor
    {
        Destructor(T callable) : _callable(callable) {}
        ~Destructor() { _callable(); }

    private:
        T _callable;
    };

    int fileDescriptor = open(path.c_str(), O_RDONLY);
    if(fileDescriptor < 0)
    {
        throw std::runtime_error("Could not open file " + path.string() + " for mapping.");
    }
    Destructor fileDestructor([fileDescriptor](){ close(fileDescriptor); });

    struct stat statData;
    int err = fstat(fileDescriptor, &statData);
    if(err < 0)
    {
        throw std::runtime_error("Could not stat file " + path.string() + " for mapping.");
    }


    struct Unmapper {
        void operator()(T*) {  };
    };
    void* ptr = mmap(0, statData.st_size, PROT_READ, MAP_PRIVATE, fileDescriptor, 0);

}

#endif
}