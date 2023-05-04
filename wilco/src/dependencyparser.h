#pragma once

#include <cstring>

template<typename Callable>
bool parseDependencyData(std::string& data, Callable callable)
{
    auto isspace = [](char c){
        return c == ' ' || c == '\n' || c == '\r';
    };
    size_t pos = 0;
    auto skipWhitespace = [&](){
        while(pos < data.size())
        {
            char c = data[pos];
            if(!isspace(data[pos]) && 
                (data[pos] != '\\' || pos == data.size()-1 || !isspace(data[pos+1])))
            {
                break;
            }
            ++pos;
        }
    };

    std::string_view spaces(" \n\r");
    auto readGccPath = [&](){
        size_t start = pos;
        size_t lastBreak = pos;
        size_t offset = 0;
        bool stop = false;
        while(!stop)
        {
            bool space = false;
            pos = data.find_first_of(spaces, pos+1);
            if(pos == std::string::npos)
            {
                pos = data.size();
                stop = true;
            }
            else
            {
                if(data[pos-1] != '\\')
                {
                    stop = true;
                }
                else
                {
                    space = true;
                }
            }
            if(offset > 0)
            {
                memmove(data.data()+lastBreak-offset, data.data()+lastBreak, pos-lastBreak);
            }
            if(space)
            {
                ++offset;
            }
            lastBreak = pos;
        }

        return std::string_view(data.data() + start, pos-offset-start);
    };

    // This is a quick and ugly parser that will do the wrong thing on all escape sequences but \\ and \"
    std::string_view escapeOrQuote("\\\"");
    auto readClPath = [&](){
        size_t start = pos;
        size_t lastBreak = pos;
        size_t offset = 0;
        bool stop = false;
        while(!stop)
        {
            bool escape = false;
            pos = data.find_first_of(escapeOrQuote, pos+1);
            if(pos == std::string::npos)
            {
                pos = data.size();
                stop = true;
            }
            else
            {
                if(data[pos] == '\"')
                {
                    stop = true;
                }
                else
                {
                    ++pos;
                    escape = true;
                }
            }
            if(offset > 0)
            {
                std::memmove(data.data()+lastBreak-offset, data.data()+lastBreak, pos-lastBreak);
            }
            if(escape)
            {
                ++offset;
            }
            lastBreak = pos;
        }

        return std::string_view(data.data() + start, pos-offset-start);
    };

    auto consume = [&](char expected)
    {
        if(pos < data.size() && data[pos] == expected)
        {
            ++pos;
            return true;
        }
        return false;
    };

    auto readString = [&]()
    {
        size_t endPos = data.find((char)0, pos);
        if(endPos == std::string::npos)
        {
            throw std::runtime_error("Failed to find end of string in input.");
        }

        std::string_view result = std::string_view(data).substr(pos, endPos-pos);
        pos = endPos + 1;
        return result;
    };

    skipWhitespace();
    if(pos < data.size() && data[pos] != '{')
    {
        bool scanningOutputs = true;
        while(pos < data.size())
        {
            skipWhitespace();
            auto pathString = readGccPath();
            if(pathString.empty())
            {
                continue;
            }

            if(pathString.back() == ':')
            {
                scanningOutputs = false;
                continue;
            }

            if(scanningOutputs)
            {
                continue;
            }

            if(callable(pathString))
            {
                return true;
            }
        }
    }
    else
    {
        std::string_view includeTag = "\"Includes\"";
        pos = data.find(includeTag, pos);
        if(pos == data.npos)
        {
            return true;
        }
        pos += includeTag.size();
        skipWhitespace();
        if(!consume(':'))
        {
            return true;
        }
        skipWhitespace();
        if(!consume('['))
        {
            return true;
        }
        
        while(pos < data.size())
        {
            skipWhitespace();
            
            if(consume(']'))
            {
                break;
            }

            if(!consume('"'))
            {
                return true;
            }

            auto pathString = readClPath();

            if(!consume('"'))
            {
                return true;
            }

            if(pathString.empty())
            {
                continue;
            }

            if(callable(pathString))
            {
                return true;
            }

            skipWhitespace();
            if(consume(']'))
            {
                break;
            }
            else if(!consume(','))
            {
                return true;
            }
        }
    }

    return false;
}