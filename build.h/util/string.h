#pragma once

namespace str
{

inline bool startsWith(std::string_view haystack, std::string_view needle)
{
    if(haystack.size() < needle.size())
    {
        return false;
    }

    return haystack.compare(0, needle.size(), needle) == 0;
}

inline bool endsWith(std::string_view haystack, std::string_view needle)
{
    if(haystack.size() < needle.size())
    {
        return false;
    }

    return haystack.compare(haystack.size() - needle.size(), needle.size(), needle) == 0;
}

inline std::string padLeft(std::string str, size_t padding, char padChar = ' ')
{
    str.insert(str.begin(), padding, padChar);
    return str;
}

inline std::string padLeftToSize(std::string str, size_t size, char padChar = ' ')
{
    if(str.size() >= size)
    {
        return str;
    }
    size_t padding = size - str.size();
    return padLeft(std::move(str), padding, padChar);
}

inline std::string padRight(std::string str, size_t padding, char padChar = ' ')
{
    str.insert(str.end(), padding, padChar);
    return str;
}

inline std::string padRightToSize(std::string str, size_t size, char padChar = ' ')
{
    if(str.size() >= size)
    {
        return str;
    }
    size_t padding = size - str.size();
    return padRight(std::move(str), padding, padChar);
}

inline std::string trimStart(std::string str)
{
    {
        auto it = std::find_if_not(str.begin(), str.end(), [](char c) { return std::isspace(c); });
        if(it != str.begin())
        {
            str.erase(str.begin(), it);
        }
    }

    return str;
}

inline std::string trimEnd(std::string str)
{
    {
        auto it = std::find_if_not(str.rbegin(), str.rend(), [](char c) { return std::isspace(c); });
        if(it != str.rbegin())
        {
            str.erase(it.base(), str.end());
        }
    }

    return str;
}

inline std::string trim(std::string str)
{
    {
        auto it = std::find_if_not(str.begin(), str.end(), [](char c) { return std::isspace(c); });
        if(it != str.begin())
        {
            str.erase(str.begin(), it);
        }
    }

    {
        auto it = std::find_if_not(str.rbegin(), str.rend(), [](char c) { return std::isspace(c); });
        if(it != str.rbegin())
        {
            str.erase(it.base(), str.end());
        }
    }

    return str;
}

inline std::string_view trimStart(std::string_view str)
{
    {
        auto it = std::find_if_not(str.begin(), str.end(), [](char c) { return std::isspace(c); });
        if(it != str.begin())
        {
            str = std::string_view(&*it, str.end()-it);
        }
    }
    
    return str;
}

inline std::string_view trimEnd(std::string_view str)
{
    {
        auto it = std::find_if_not(str.rbegin(), str.rend(), [](char c) { return std::isspace(c); });
        if(it != str.rbegin())
        {
            str = std::string_view(&*str.begin(), it.base()-str.begin());
        }
    }

    return str;
}

inline std::string_view trim(std::string_view str)
{
    {
        auto it = std::find_if_not(str.begin(), str.end(), [](char c) { return std::isspace(c); });
        if(it != str.begin())
        {
            str = std::string_view(&*it, str.end()-it);
        }
    }
    
    {
        auto it = std::find_if_not(str.rbegin(), str.rend(), [](char c) { return std::isspace(c); });
        if(it != str.rbegin())
        {
            str = std::string_view(&*str.begin(), it.base()-str.begin());
        }
    }

    return str;
}

inline std::pair<std::string_view, std::string_view> split(std::string_view str, char delimiter)
{
    auto pos = str.find(delimiter);
    if(pos != str.npos)
    {
        return { str.substr(0, pos), str.substr(pos+1, str.size()-pos-1) };
    }
    else
    {
        return { str, "" };
    }
}

inline std::pair<std::string, std::string> split(const std::string& str, char delimiter)
{
    auto pos = str.find(delimiter);
    if(pos != str.npos)
    {
        return { std::string(str.substr(0, pos)), std::string(str.substr(pos+1, str.size()-pos-1)) };
    }
    else
    {
        return { std::string(str), "" };
    }
}

inline std::string quote(std::string str, char escapeChar = '\\', std::string_view escapedChars = "\"\\")
{
    for(auto it = str.begin(); it != str.end(); ++it)
    {
        if(escapedChars.find(*it) != std::string_view::npos)
        {
            it = str.insert(it, escapeChar);
            ++it;
        }
    }

    str.insert(str.begin(), '"');
    str.insert(str.end(), '"');
    return str;
}

inline std::string quote(std::string_view str, char escapeChar = '\\', std::string_view escapedChars = "\"\\")
{
    return quote(std::string(str), escapeChar, escapedChars);
}

inline std::string wrap(std::string_view str, size_t maxLength, size_t indent)
{
    std::string result;
    bool first = true;
    while(true)
    {
        auto emit = [&](std::string_view v)
        {
            if(!first)
            {
                result += "\n";
                result.insert(result.end(), indent, ' ');
            }
            result += v;
            first = false;
        };

        if(str.size() <= maxLength)
        {
            emit(str);
            break;
        }

        auto row = str.substr(0, maxLength);
        auto space = std::find_if(row.rbegin(), str.rend(), [](char c) { return std::isspace(c); });
        size_t split = (size_t)std::distance(space, str.rend());
        emit(trimEnd(str.substr(0, split)));
        str = trimStart(str.substr(split));
    }

    return result;
}

inline void replaceFirstInPlace(std::string& input, std::string_view searchString, std::string_view replacementString)
{
    size_t pos = input.find(searchString);
    if(pos != std::string::npos)
    {
        input.replace(pos, searchString.size(), replacementString.data(), replacementString.size());
    }
}

inline void replaceAllInPlace(std::string& input, std::string_view searchString, std::string_view replacementString)
{
    size_t pos = 0;
    while(pos < input.size())
    {
        pos = input.find(searchString, pos);
        if(pos == std::string::npos)
        {
            break;
        }

        input.replace(pos, searchString.size(), replacementString.data(), replacementString.size());
        // Avoid infinite loop in case we replace with something containing the search string
        pos += replacementString.size();
    }
}

inline std::string replaceFirst(std::string input, std::string_view searchString, std::string_view replacementString)
{
    replaceFirstInPlace(input, searchString, replacementString);
    return input;
}

inline std::string replaceAll(std::string input, std::string_view searchString, std::string_view replacementString)
{
    replaceAllInPlace(input, searchString, replacementString);
    return input;
}

}
