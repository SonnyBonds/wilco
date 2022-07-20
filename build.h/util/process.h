#pragma once

#include <array>
#include <iostream>
#include <future>
#include <stdio.h>
#include <string>

#if _WIN32
#define NOMINMAX
#include <Shlobj.h>
#include <io.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

#include "core/os.h"

namespace process
{

#if _WIN32
static std::filesystem::path findCurrentModulePath()
{
	wchar_t moduleFileName[2048];

    HMODULE hMod = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&findCurrentModulePath),
                       &hMod);
	GetModuleFileNameW(hMod, moduleFileName, (DWORD)2048);
	return moduleFileName;
}
#else
static std::filesystem::path findCurrentModulePath()
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

// TODO: Maybe a fully separate OpenProcess or ShellExecute implementation on Windows
#if _WIN32
#define popen _popen
#define pclose _pclose
#define read _read
#define fileno _fileno
#define execvp _execvp
#define WEXITSTATUS
#endif

void runAndExit(const std::string& command)
{
    auto newCString = [](const std::string& str)
    {
        auto result = std::make_unique<char[]>(str.size()+1);
        memcpy(result.get(), str.c_str(), str.size()+1);
        return result;
    };

    std::vector<std::unique_ptr<char[]>> args;
    args.push_back(newCString(OperatingSystem::current() == Windows ? "cmd" : "sh"));
    args.push_back(newCString(OperatingSystem::current() == Windows ? "/C" : "-c"));
    args.push_back(newCString(command));
    args.push_back(nullptr);

    std::vector<char*> cArgs;
    cArgs.reserve(args.size());
    for(auto& arg : args)
    {
        cArgs.push_back(arg.get());
    }

    execvp(cArgs[0], cArgs.data());
}

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

#if _WIN32
#undef popen
#undef pclose
#undef read
#undef fileno
#undef execvp
#undef WEXITSTATUS
#endif

}