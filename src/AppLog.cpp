#include "AppLog.h"

#include <cstdio>
#include <cstring>
#include <exception>
#include <mutex>

#if JUCE_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#include <shlobj.h>
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
#if JUCE_WINDOWS
    if (path == nullptr || path[0] == 0 || text == nullptr)
        return;
    const DWORD mode = append ? OPEN_ALWAYS : CREATE_ALWAYS;
    HANDLE file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                              mode, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return;
    if (append)
        SetFilePointer(file, 0, nullptr, FILE_END);
    else
        SetEndOfFile(file);
    DWORD written = 0;
    WriteFile(file, text, (DWORD) std::strlen(text), &written, nullptr);
    CloseHandle(file);
#else
    juce::ignoreUnused(path, text, append);
#endif
}

#if JUCE_WINDOWS
void makePaths()
{
    char appdata[MAX_PATH] {};
    if (FAILED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appdata)))
        return;

    char dir[MAX_PATH] {};
    std::snprintf(dir, sizeof(dir), "%s\\ToneStar", appdata);
    CreateDirectoryA(dir, nullptr);
    std::snprintf(startupPathA, sizeof(startupPathA), "%s\\startup.log", dir);
    std::snprintf(crashPathA, sizeof(crashPathA), "%s\\crash.log", dir);
    std::snprintf(dumpPathA, sizeof(dumpPathA), "%s\\crash.dmp", dir);
    std::snprintf(readyPathA, sizeof(readyPathA), "%s\\last-run-ok", dir);
}

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

void terminateHandler()
{
    writeAll(crashPathA, "std::terminate\n", true);
    writeAll(startupPathA, "TERMINATE\n", true);
    std::abort();
}

struct EarlyInstall
{
    EarlyInstall()
    {
        makePaths();
        const DWORD attr = GetFileAttributesA(readyPathA);
        previousOk = attr != INVALID_FILE_ATTRIBUTES
                     && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
        DeleteFileA(readyPathA);
        SetUnhandledExceptionFilter(crashFilter);
        std::set_terminate(terminateHandler);
        writeAll(startupPathA, "early install\n", false);
    }
};

EarlyInstall earlyInstall;
#endif
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
#if JUCE_WINDOWS
    if (startupPathA[0] == 0)
        makePaths();
#else
    directory().createDirectory();
#endif
    note("process start ToneStar");
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
#if JUCE_WINDOWS
    writeAll(startupPathA, line.toRawUTF8(), true);
#else
    startupFile().appendText(line);
#endif
    DBG("AppLog " << step);
}

void markReady()
{
#if JUCE_WINDOWS
    writeAll(readyPathA, "ok\n", false);
#else
    directory().getChildFile("last-run-ok").replaceWithText("ok\n");
#endif
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
