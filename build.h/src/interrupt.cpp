#include "util/interrupt.h"
#include <iostream>

#if _WIN32

#include <windows.h>
#include <mutex>
#include <io.h>
#include <signal.h>

namespace interrupt
{

void logSignal(int signal)
{
    const char buf[] = "Signal caught: ";
    auto stdoutFileno = _fileno(stdout);
    _write(stdoutFileno, buf, sizeof(buf));
    switch(signal)
    {
        case SIGINT:
            _write(stdoutFileno, "SIGINT\n", 7);
            break;
        case SIGILL:
            _write(stdoutFileno, "SIGILL\n", 7);
            break;
        case SIGFPE:
            _write(stdoutFileno, "SIGFPE\n", 7);
            break;
        case SIGSEGV:
            _write(stdoutFileno, "SIGSEGV\n", 8);
            break;
        case SIGABRT_COMPAT:
        case SIGABRT:
            _write(stdoutFileno, "SIGABRT\n", 8);
            break;
        default:
            _write(stdoutFileno, "(unknown)\n", 10);
            break;
    }
}

static bool interrupted = false;

static std::mutex runLock;

void notifyExit()
{
    runLock.unlock();
}

void waitForExit()
{
    runLock.lock();
    runLock.unlock();
    std::cout << "Shutdown complete." << std::endl;
}

static BOOL WINAPI handler(DWORD event)
{
    interrupted = true;
    std::cout << "\nAborting... ";
    switch(event)
    {
    case CTRL_C_EVENT:
        std::cout << "(ctrl-c)" << std::endl;
        waitForExit();
        break;
    case CTRL_BREAK_EVENT:
        std::cout << "(ctrl-break)" << std::endl;
        break;
    case CTRL_CLOSE_EVENT:
        std::cout << "(close)" << std::endl;
        break;
    case CTRL_LOGOFF_EVENT:
        std::cout << "(logoff)" << std::endl;
        break;
    case CTRL_SHUTDOWN_EVENT:
        std::cout << "(shutdown)" << std::endl;
        break;
    default:
        std::cout << "(unknown signal)" << std::endl;
        break;
    }

    waitForExit();
    return FALSE;
}

void installHandlers()
{
    if(!runLock.try_lock())
    {
        throw std::runtime_error("Internal error - unable to lock main mutex.");
    }
    SetConsoleCtrlHandler(handler, TRUE);
    signal(SIGSEGV, &logSignal);
    signal(SIGINT, &logSignal);
    signal(SIGILL, &logSignal);
    signal(SIGFPE, &logSignal);
    signal(SIGABRT, &logSignal);
    signal(SIGABRT_COMPAT, &logSignal);
    atexit(&notifyExit);
}

bool isInterrupted()
{
    return interrupted;
}

}

#else

namespace interrupt
{

void installHandlers()
{
    // TODO
}

bool isInterrupted()
{
    // TODO
    return false;
}

}

#endif
