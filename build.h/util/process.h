#pragma once

#include <array>
#include <future>
#include <stdio.h>
#include <string>

#if _WIN32
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

namespace process
{

#if _WIN32
// TODO: Windows implementation
#else
static std::string findCurrentModulePath()
{
    Dl_info info;
	dladdr((void*)&findCurrentModulePath, &info);

    return info.dli_fname;
}
#endif

struct ProcessResult
{
    int exitCode;
    std::string output;
};

ProcessResult run(std::string command, bool echoOutput = false)
{
    ProcessResult result;
    {
        auto processPipe = popen(command.c_str(), "r");
        auto pipeFd = fileno(processPipe);
        try
        {
            std::array<char, 2048> buffer;
            while(auto bytesRead = read(pipeFd, buffer.data(), buffer.size()))
            {
                if(bytesRead < 0)
                {
                    break;
                }
                result.output.append(buffer.data(), bytesRead);
                if(echoOutput)
                {
                    std::cout.write(buffer.data(), bytesRead);
                    std::cout.flush();
                }
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
}

}