#pragma once

#include <array>
#include <string>
#include <string_view>

namespace hash
{

std::array<unsigned char, 16> md5(std::string_view input);
std::string md5String(std::string_view input);

}