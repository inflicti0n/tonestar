#include "App/AppLog.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <mutex>

#if JUCE_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#include <shlobj.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace
{
constexpr int kPathChars = 1024;
char startupPathA[kPathChars] {};
char crashPathA[kPathChars] {};
char dumpPathA[kPathChars] {};
char readyPathA[kPathChars] {};
char lastStep[512] {};
std::mutex noteLock;
bool installed = false;
bool previousOk = false;

void writeAll(const char* path, const char* text, bool append)
{
    if (path == nullptr || path[0] == 0 || text == nullptr)
        return;

    FILE* file = nullptr;
#if JUCE_WINDOWS
    if (fopen_s(&file, path, append ? "ab" : "wb") != 0 || file == nullptr)
        return;
#else
    file = std::fopen(path, append ? "ab" : "wb");
    if (file == nullptr)
        return;
#endif
    std::fwrite(text, 1, std::strlen(text), file);
    std::fclose(file);
}

void makeDir(const char* path)
{
    if (path == nullptr || path[0] == 0)
        return;
#if JUCE_WINDOWS
    CreateDirectoryA(path, nullptr);
#else
    mkdir(path, 0755);
#endif
}

void fillChild(char* dest, const char* dir, const char* name)
{
#if JUCE_WINDOWS
    std::snprintf(dest, kPathChars, "%s\\%s", dir, name);
#else
    std::snprintf(dest, kPathChars, "%s/%s", dir, name);
#endif
}

void makePaths()
{
    char dir[kPathChars] {};

#if JUCE_WINDOWS
    char appdata[MAX_PATH] {};
    if (FAILED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appdata)))
        return;
    std::snprintf(dir, sizeof(dir), "%s\\ToneStar", appdata);
#else
    const char* home = std::getenv("HOME");
    if (home == nullptr || home[0] == 0)
        return;

#if JUCE_MAC
    char parent[kPathChars] {};
    std::snprintf(parent, sizeof(parent), "%s/Library/Application Support", home);
    makeDir(parent);
    std::snprintf(dir, sizeof(dir), "%s/ToneStar", parent);
#else
    const char* xdg = std::getenv("XDG_CONFIG_HOME");
    if (xdg != nullptr && xdg[0] != 0)
    {
        std::snprintf(dir, sizeof(dir), "%s/ToneStar", xdg);
    }
    else
    {
        char parent[kPathChars] {};
        std::snprintf(parent, sizeof(parent), "%s/.config", home);
        makeDir(parent);
        std::snprintf(dir, sizeof(dir), "%s/ToneStar", parent);
    }
#endif
#endif

    makeDir(dir);
    fillChild(startupPathA, dir, "startup.log");
    fillChild(crashPathA, dir, "crash.log");
    fillChild(dumpPathA, dir, "crash.dmp");
    fillChild(readyPathA, dir, "last-run-ok");
}

void consumeReadyFlag()
{
    if (readyPathA[0] == 0)
        return;

    FILE* file = nullptr;
#if JUCE_WINDOWS
    previousOk = fopen_s(&file, readyPathA, "rb") == 0 && file != nullptr;
#else
    file = std::fopen(readyPathA, "rb");
    previousOk = file != nullptr;
#endif
    if (file != nullptr)
        std::fclose(file);
    std::remove(readyPathA);
}

void writeCrashReason(const char* reason)
{
    char timeLine[80] {};
#if JUCE_WINDOWS
    SYSTEMTIME st {};
    GetLocalTime(&st);
    std::snprintf(timeLine, sizeof(timeLine),
                  "time %04u-%02u-%02u %02u:%02u:%02u.%03u\n",
                  (unsigned) st.wYear, (unsigned) st.wMonth, (unsigned) st.wDay,
                  (unsigned) st.wHour, (unsigned) st.wMinute, (unsigned) st.wSecond,
                  (unsigned) st.wMilliseconds);
#else
    std::time_t now = std::time(nullptr);
    std::tm local {};
    if (localtime_r(&now, &local) != nullptr)
        std::snprintf(timeLine, sizeof(timeLine),
                      "time %04d-%02d-%02d %02d:%02d:%02d\n",
                      local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
                      local.tm_hour, local.tm_min, local.tm_sec);
#endif

    char buf[2048];
    std::snprintf(buf, sizeof(buf),
                  "ToneStar crash\n"
                  "%s"
                  "reason %s\n"
                  "last step: %s\n",
                  timeLine, reason != nullptr ? reason : "unknown", lastStep);
    writeAll(crashPathA, buf, false);
}

void terminateHandler()
{
    writeCrashReason("std::terminate");
    writeAll(startupPathA, "TERMINATE\n", true);
#if JUCE_WINDOWS
    std::abort();
#else
    std::_Exit(1);
#endif
}

#if JUCE_WINDOWS
void writeCrashText(EXCEPTION_POINTERS* info)
{
    char buf[2048];
    const auto* rec = info != nullptr ? info->ExceptionRecord : nullptr;
    const unsigned code = rec != nullptr ? (unsigned) rec->ExceptionCode : 0;
    const void* addr = rec != nullptr ? rec->ExceptionAddress : nullptr;
    SYSTEMTIME st {};
    GetLocalTime(&st);
    std::snprintf(buf, sizeof(buf),
                  "ToneStar crash\n"
                  "time %04u-%02u-%02u %02u:%02u:%02u.%03u\n"
                  "exception 0x%08X\n"
                  "address %p\n"
                  "last step: %s\n",
                  (unsigned) st.wYear, (unsigned) st.wMonth, (unsigned) st.wDay,
                  (unsigned) st.wHour, (unsigned) st.wMinute, (unsigned) st.wSecond,
                  (unsigned) st.wMilliseconds,
                  code, addr, lastStep);
    writeAll(crashPathA, buf, false);

    void* stack[32] {};
    const USHORT n = CaptureStackBackTrace(0, 32, stack, nullptr);
    char line[128];
    for (USHORT i = 0; i < n; ++i)
    {
        std::snprintf(line, sizeof(line), "  #%02u %p\n", (unsigned) i, stack[i]);
        writeAll(crashPathA, line, true);
    }
}

void writeMiniDump(EXCEPTION_POINTERS* info)
{
    HMODULE dbg = LoadLibraryA("dbghelp.dll");
    if (dbg == nullptr)
        return;

    using WriteDumpFn = BOOL (WINAPI*)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE,
                                       PMINIDUMP_EXCEPTION_INFORMATION,
                                       PMINIDUMP_USER_STREAM_INFORMATION,
                                       PMINIDUMP_CALLBACK_INFORMATION);
    auto writeDump = (WriteDumpFn) GetProcAddress(dbg, "MiniDumpWriteDump");
    if (writeDump == nullptr)
    {
        FreeLibrary(dbg);
        return;
    }

    HANDLE file = CreateFileA(dumpPathA, GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        FreeLibrary(dbg);
        return;
    }

    MINIDUMP_EXCEPTION_INFORMATION mei {};
    mei.ThreadId = GetCurrentThreadId();
    mei.ExceptionPointers = info;
    mei.ClientPointers = FALSE;
    writeDump(GetCurrentProcess(), GetCurrentProcessId(), file,
              MiniDumpWithIndirectlyReferencedMemory, info != nullptr ? &mei : nullptr,
              nullptr, nullptr);
    CloseHandle(file);
    FreeLibrary(dbg);
}

LONG WINAPI crashFilter(EXCEPTION_POINTERS* info)
{
    writeCrashText(info);
    writeMiniDump(info);
    writeAll(startupPathA, "CRASH\n", true);
    return EXCEPTION_CONTINUE_SEARCH;
}
#else
void signalHandler(int sig)
{
    char buf[768];
    const int n = std::snprintf(buf, sizeof(buf),
                                "ToneStar crash\nreason signal %d\nlast step: %s\n",
                                sig, lastStep);
    if (n > 0 && crashPathA[0] != 0)
    {
        const int fd = open(crashPathA, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd >= 0)
        {
            const auto bytes = n < (int) sizeof(buf) ? n : (int) sizeof(buf) - 1;
            { auto ignored = write(fd, buf, (size_t) bytes); (void) ignored; }
            close(fd);
        }
    }
    if (startupPathA[0] != 0)
    {
        const int fd = open(startupPathA, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (fd >= 0)
        {
            { auto ignored = write(fd, "CRASH\n", 6); (void) ignored; }
            close(fd);
        }
    }

    signal(sig, SIG_DFL);
    raise(sig);
}

void installSignalHandlers()
{
    signal(SIGSEGV, signalHandler);
    signal(SIGABRT, signalHandler);
    signal(SIGILL, signalHandler);
    signal(SIGFPE, signalHandler);
}
#endif

struct EarlyInstall
{
    EarlyInstall()
    {
        makePaths();
        consumeReadyFlag();
        std::set_terminate(terminateHandler);
#if JUCE_WINDOWS
        SetUnhandledExceptionFilter(crashFilter);
#else
        installSignalHandlers();
#endif
        writeAll(startupPathA, "early install\n", false);
    }
};

EarlyInstall earlyInstall;

void makePathsFromJuce()
{
    auto dir = AppLog::directory();
    dir.createDirectory();
    if (dir.getFullPathName().isEmpty())
        return;

    auto setPath = [&dir] (char* dest, const char* name)
    {
        dir.getChildFile(name).getFullPathName().copyToUTF8(dest, kPathChars);
    };

    if (startupPathA[0] == 0)
        setPath(startupPathA, "startup.log");
    if (crashPathA[0] == 0)
        setPath(crashPathA, "crash.log");
    if (dumpPathA[0] == 0)
        setPath(dumpPathA, "crash.dmp");
    if (readyPathA[0] == 0)
        setPath(readyPathA, "last-run-ok");
}

juce::String identityLine()
{
    juce::String arch = juce::SystemStats::isOperatingSystem64Bit() ? "64-bit" : "32-bit";
#if JUCE_ARM
    arch += " ARM";
#elif JUCE_INTEL
    arch += " x86";
#endif

    juce::String line = "version " + juce::String(JUCE_APPLICATION_VERSION_STRING)
                      + "  " + juce::SystemStats::getOperatingSystemName()
                      + "  " + arch;
    const auto cpu = juce::SystemStats::getCpuModel();
    if (cpu.isNotEmpty())
        line += "  " + cpu;
    return line;
}
}

namespace AppLog
{
juce::File directory()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("ToneStar");
}

juce::File startupFile() { return directory().getChildFile("startup.log"); }
juce::File crashFile() { return directory().getChildFile("crash.log"); }
juce::File selfTestFile() { return directory().getChildFile("self-test.txt"); }

void install()
{
    if (installed)
        return;
    installed = true;

    if (startupPathA[0] == 0)
        makePaths();
    if (startupPathA[0] == 0)
        makePathsFromJuce();
    else
        directory().createDirectory();

    if (! previousOk && readyPathA[0] != 0)
        consumeReadyFlag();

    note("process start ToneStar");
    note(identityLine());
}

void note(const juce::String& step)
{
    const auto line = juce::Time::getCurrentTime().formatted("%Y-%m-%d %H:%M:%S")
                    + "  " + step + "\n";
    {
        std::lock_guard<std::mutex> g(noteLock);
        const auto* src = step.toRawUTF8();
        const auto n = juce::jmin(sizeof(lastStep) - 1, std::strlen(src));
        std::memcpy(lastStep, src, n);
        lastStep[n] = 0;
    }
    if (startupPathA[0] != 0)
        writeAll(startupPathA, line.toRawUTF8(), true);
    else
        startupFile().appendText(line);
    DBG("AppLog " << step);
}

void markReady()
{
    if (readyPathA[0] != 0)
        writeAll(readyPathA, "ok\n", false);
    else
        directory().getChildFile("last-run-ok").replaceWithText("ok\n");
    note("startup complete");
}

bool previousRunFinished()
{
    return previousOk;
}

bool runSeh(void (*fn)(void*), void* ctx, unsigned int* exceptionCode)
{
    if (fn == nullptr)
        return false;

#if JUCE_WINDOWS
    unsigned int code = 0;
    __try
    {
        fn(ctx);
        if (exceptionCode != nullptr)
            *exceptionCode = 0;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        code = (unsigned int) GetExceptionCode();
        if (exceptionCode != nullptr)
            *exceptionCode = code;
        return false;
    }
#else
    fn(ctx);
    if (exceptionCode != nullptr)
        *exceptionCode = 0;
    return true;
#endif
}
}
