#pragma once

#include <array>
#include <future>
#include <stdio.h>
#include <string>

namespace process
{

struct ProcessResult
{
    int exitCode;
    std::string output;
};

std::future<ProcessResult> run(std::string command)
{
    return std::async(std::launch::async, [command]()
    {
        ProcessResult result;
        {
            auto processPipe = popen(command.c_str(), "r");
            try
            {
                std::array<char, 2048> buffer;
                while(auto bytesRead = fread(buffer.data(), 1, buffer.size(), processPipe))
                {
                    result.output.append(buffer.data(), bytesRead);
                }
                
            }
            catch(...)
            {
                pclose(processPipe);
                throw;
            }
            auto status = pclose(processPipe);
            result.exitCode = WEXITSTATUS(status);
        }

        return result;
    });
}

}