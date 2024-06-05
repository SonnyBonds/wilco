#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace uuid
{

    struct uuid
    {
        uuid() {}
        uuid(std::array<unsigned char, 16> data) : data(data) { }
        uuid(uint32_t a, uint32_t b, uint32_t c, uint32_t d);
        uuid(std::string str);

        std::array<unsigned char, 16> data;

        bool operator==(const uuid& other) const;

        operator std::string() const;
    };

    uuid generateV3(uuid nameSpace, std::string_view name);
}