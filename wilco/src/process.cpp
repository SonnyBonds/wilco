#include "util/process.h"

#if _WIN32
#define NOMINMAX
#include <Shlobj.h>
#include <io.h>
#include <process.h>

// TODO: Maybe a fully separate OpenProcess or ShellExecute implementation on Windows
#define popen _popen
#define pclose _pclose
#define read _read
#define fileno _fileno
#define WEXITSTATUS

#else

#include <dlfcn.h>
#include <unistd.h>
#include <errno.h>

#if __APPLE__
#include <mach-o/dyld.h>
#endif

#endif

namespace process
{

#if _WIN32

std::filesystem::path findCurrentModulePath()
{
	wchar_t moduleFileName[2048];

    HMODULE hMod = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&findCurrentModulePath),
                       &hMod);
	GetModuleFileNameW(hMod, moduleFileName, (DWORD)2048);
	return moduleFileName;
}

#elif __APPLE__

// TODO: A more portable way of doing this?
std::filesystem::path findCurrentModulePath()
{
    static std::filesystem::path currentModulePath = [](){
        uint32_t maxSize = PATH_MAX;
        char buf[PATH_MAX] = {};
        _NSGetExecutablePath(buf, &maxSize);
        return std::filesystem::absolute(std::filesystem::path(buf));
    }();

    return currentModulePath;
}

#else 

// TODO: A more portable way of doing this?
std::filesystem::path findCurrentModulePath()
{
    static std::filesystem::path currentModulePath = [](){
        return std::filesystem::absolute(program_invocation_name);
    }();

    return currentModulePath;
}

#endif

ProcessResult run(std::string command, bool echoOutput)
{
    ProcessResult result;
    {
        auto processPipe = popen(command.c_str(), "r");
        auto pipeFd = fileno(processPipe);
        try
        {
            std::array<char, 2048> buffer;
            while(auto bytesRead = read(pipeFd, buffer.data(), (unsigned int)buffer.size()))
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