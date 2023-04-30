#pragma once

#include <array>
#include <string>
#include <string_view>

namespace hash
{

    namespace detail
    {
        typedef struct {
            uint32_t lo, hi;
            uint32_t a, b, c, d;
            unsigned char buffer[64];
            uint32_t block[16];
        } MD5_CTX;
    }

struct Md5
{
public:
    Md5();

    void digest(const char* data, size_t size);
    void digest(std::string_view input);
	void digest(const wchar_t* data, size_t size);
    void digest(std::wstring_view input);

    std::array<unsigned char, 16> finalize();
private:
    detail::MD5_CTX _context; 
};

std::array<unsigned char, 16> md5(const char* data, size_t size);
std::array<unsigned char, 16> md5(std::string_view input);
std::string md5String(std::string_view input);
std::string md5String(std::array<unsigned char, 16> hash);

}