#include "util/uuid.h"
#include "util/hash.h"
#include <iostream>
#include <cstring>

namespace uuid
{

uuid::uuid(std::string str)
{
    if(str.size() != 32+4)
    {
        return;
    }

    if(str[8] != '-' ||
       str[13] != '-' ||
       str[18] != '-' ||
       str[23] != '-')
    {
        return;
    }

    int offsets[16] = { 0, 2, 4, 6, 9, 11, 14, 16, 19, 21, 24, 26, 28, 30, 32, 34 };

    int i = 0;
    for(; i<16; ++i)
    {
        char a = str[offsets[i]];
        if(a >= '0' && a <= '9') a -= '0';
        else if(a >= 'a' && a <= 'f') a -= 'a' - 10;
        else if(a >= 'A' && a <= 'F') a -= 'A' - 10;
        else
        {
            break;
        }

        char b = str[offsets[i]+1];
        if(b >= '0' && b <= '9') b -= '0';
        else if(b >= 'a' && b <= 'f') b -= 'a' - 10;
        else if(b >= 'A' && b <= 'F') b -= 'A' - 10;
        else
        {
            break;
        }

        data[i] = (a << 4) | b;
    }

    if(i < 16)
    {
        memset(data.data(), 0, data.size());
    }
}

uuid::uuid(uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    data[ 0] = (a >> 24) & 0xff;
    data[ 1] = (a >> 16) & 0xff;
    data[ 2] = (a >>  8) & 0xff;
    data[ 3] = (a      ) & 0xff;
    data[ 4] = (b >> 24) & 0xff;
    data[ 5] = (b >> 16) & 0xff;
    data[ 6] = (b >>  8) & 0xff;
    data[ 7] = (b      ) & 0xff;
    data[ 8] = (c >> 24) & 0xff;
    data[ 9] = (c >> 16) & 0xff;
    data[10] = (c >>  8) & 0xff;
    data[11] = (c      ) & 0xff;
    data[12] = (d >> 24) & 0xff;
    data[13] = (d >> 16) & 0xff;
    data[14] = (d >>  8) & 0xff;
    data[15] = (d      ) & 0xff;
}

uuid::operator std::string() const {
    static const char* symbols = "0123456789abcdef";
    std::string result;
    result.reserve(32+4);

    for(size_t i=0; i<4; ++i)
    {
        result.push_back(symbols[(data[i] >> 4) & 0xf]);
        result.push_back(symbols[data[i] & 0xf]);
    }

    result.push_back('-');

    for(size_t i=4; i<6; ++i)
    {
        result.push_back(symbols[(data[i] >> 4) & 0xf]);
        result.push_back(symbols[data[i] & 0xf]);
    }

    result.push_back('-');

    for(size_t i=6; i<8; ++i)
    {
        result.push_back(symbols[(data[i] >> 4) & 0xf]);
        result.push_back(symbols[data[i] & 0xf]);
    }

    result.push_back('-');

    for(size_t i=8; i<10; ++i)
    {
        result.push_back(symbols[(data[i] >> 4) & 0xf]);
        result.push_back(symbols[data[i] & 0xf]);
    }

    result.push_back('-');

    for(size_t i=10; i<16; ++i)
    {
        result.push_back(symbols[(data[i] >> 4) & 0xf]);
        result.push_back(symbols[data[i] & 0xf]);
    }

    return result;
}

bool uuid::operator==(const uuid& other) const {
    return data == other.data;
}

uuid generateV3(uuid nameSpace, std::string_view name) {
    std::string str;
    str.reserve(16 + name.size());
    str.resize(16);
    std::memcpy(str.data(), nameSpace.data.data(), 16);
    str += name;
    std::array<unsigned char, 16> data = hash::md5(str);
    data[6] = (data[6] & 0x0f) | (3 << 4);
    data[8] = (data[8] & 0x3f) | (1 << 7);
    return { data };
}

}


