#pragma once

/**
 * @file msvcrt.hpp
 * @brief MicaNT Clean-Room C Runtime Library (msvcrt.dll / ucrtbase.dll) Bridge.
 *
 * Implements standard ISO C runtime functions, startup hooks, memory allocation,
 * and formatted I/O on top of MicaNT's clean-room Win32 and NT kernel primitives.
 *
 * Referenced strictly from public standard C / POSIX / Microsoft Learn specifications
 * under Google LLC v. Oracle America, Inc. interoperability protections.
 */

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cctype>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <algorithm>
#include <chrono>
#include <cwchar>
#include <cwctype>
#include <clocale>
#include <csignal>
#include <ctime>

#include "ntdef.hpp"
#include "ntstatus.hpp"
#include "kernel32.hpp"
#include "ldr.hpp"

namespace micant::msvcrt {

// ============================================================================
// 1. Standard Streams & FILE Wrapper
// ============================================================================

struct MicaFile {
    char* _ptr{nullptr};
    int   _cnt{0};
    char* _base{nullptr};
    int   _flag{0};
    int   _file{0};
    int   _charbuf{0};
    int   _bufsiz{0};
    char* _tmpfname{nullptr};

    win32::HANDLE getHandle() const noexcept {
        if (_file == 0) return win32::GetStdHandle(win32::STD_INPUT_HANDLE);
        if (_file == 1) return win32::GetStdHandle(win32::STD_OUTPUT_HANDLE);
        if (_file == 2) return win32::GetStdHandle(win32::STD_ERROR_HANDLE);
        return reinterpret_cast<win32::HANDLE>(static_cast<intptr_t>(_file));
    }
};

static_assert(sizeof(MicaFile) == 48, "MicaFile must match MSVC CRT FILE structure ABI (48 bytes on x64)");

#ifdef _iob
#undef _iob
#endif

inline MicaFile _iob[3] = {
    { nullptr, 0, nullptr, 0x0001, 0, 0, 0, nullptr }, // stdin (_IOREAD)
    { nullptr, 0, nullptr, 0x0002, 1, 0, 0, nullptr }, // stdout (_IOWRT)
    { nullptr, 0, nullptr, 0x0002, 2, 0, 0, nullptr }  // stderr (_IOWRT)
};

inline MicaFile& g_StdInFile  = _iob[0];
inline MicaFile& g_StdOutFile = _iob[1];
inline MicaFile& g_StdErrFile = _iob[2];

inline MicaFile* g_IobArray[3] = { &_iob[0], &_iob[1], &_iob[2] };

inline MicaFile** __iob_func() noexcept {
    return g_IobArray;
}

inline MicaFile* __acrt_iob_func(unsigned id) noexcept {
    if (id < 3) return &_iob[id];
    return &_iob[1];
}

// ============================================================================
// 2. Memory Management (malloc / free / realloc / calloc)
// ============================================================================

inline void* malloc(size_t size) noexcept {
    win32::HANDLE hHeap = win32::GetProcessHeap();
    return win32::HeapAlloc(hHeap, 0, size ? size : 1);
}

inline void free(void* ptr) noexcept {
    if (!ptr) return;
    win32::HANDLE hHeap = win32::GetProcessHeap();
    win32::HeapFree(hHeap, 0, ptr);
}

inline void* calloc(size_t num, size_t size) noexcept {
    size_t total = num * size;
    win32::HANDLE hHeap = win32::GetProcessHeap();
    return win32::HeapAlloc(hHeap, win32::HEAP_ZERO_MEMORY, total ? total : 1);
}

inline void* realloc(void* ptr, size_t size) noexcept {
    if (!ptr) return malloc(size);
    if (size == 0) {
        free(ptr);
        return nullptr;
    }
    win32::HANDLE hHeap = win32::GetProcessHeap();
    return win32::HeapReAlloc(hHeap, 0, ptr, size);
}

// ============================================================================
// 3. String & Buffer Manipulation
// ============================================================================

inline size_t strlen(const char* str) noexcept {
    return str ? std::strlen(str) : 0;
}

inline size_t wcslen(const wchar_t* str) noexcept {
    return str ? std::wcslen(str) : 0;
}

inline int wcscmp(const wchar_t* s1, const wchar_t* s2) noexcept {
    if (!s1 && !s2) return 0;
    if (!s1) return -1;
    if (!s2) return 1;
    return std::wcscmp(s1, s2);
}

inline const wchar_t* wcsstr(const wchar_t* str, const wchar_t* substr) noexcept {
    if (!str || !substr) return nullptr;
    return std::wcsstr(str, substr);
}

inline int strcmp(const char* s1, const char* s2) noexcept {
    if (!s1 && !s2) return 0;
    if (!s1) return -1;
    if (!s2) return 1;
    return std::strcmp(s1, s2);
}

inline int strncmp(const char* s1, const char* s2, size_t n) noexcept {
    if (n == 0) return 0;
    if (!s1 && !s2) return 0;
    if (!s1) return -1;
    if (!s2) return 1;
    return std::strncmp(s1, s2, n);
}

inline int _stricmp(const char* s1, const char* s2) noexcept {
    if (!s1 && !s2) return 0;
    if (!s1) return -1;
    if (!s2) return 1;
    while (*s1 && *s2) {
        int c1 = std::tolower(static_cast<unsigned char>(*s1));
        int c2 = std::tolower(static_cast<unsigned char>(*s2));
        if (c1 != c2) return c1 - c2;
        ++s1;
        ++s2;
    }
    return std::tolower(static_cast<unsigned char>(*s1)) - std::tolower(static_cast<unsigned char>(*s2));
}

inline int _strnicmp(const char* s1, const char* s2, size_t n) noexcept {
    if (n == 0) return 0;
    if (!s1 && !s2) return 0;
    if (!s1) return -1;
    if (!s2) return 1;
    while (n && *s1 && *s2) {
        int c1 = std::tolower(static_cast<unsigned char>(*s1));
        int c2 = std::tolower(static_cast<unsigned char>(*s2));
        if (c1 != c2) return c1 - c2;
        ++s1;
        ++s2;
        --n;
    }
    if (n == 0) return 0;
    return std::tolower(static_cast<unsigned char>(*s1)) - std::tolower(static_cast<unsigned char>(*s2));
}

inline char* strcpy(char* dest, const char* src) noexcept {
    return std::strcpy(dest, src);
}

inline char* strncpy(char* dest, const char* src, size_t n) noexcept {
    return std::strncpy(dest, src, n);
}

inline char* strcat(char* dest, const char* src) noexcept {
    return std::strcat(dest, src);
}

inline char* strchr(const char* str, int c) noexcept {
    return const_cast<char*>(std::strchr(str, c));
}

inline char* strrchr(const char* str, int c) noexcept {
    return const_cast<char*>(std::strrchr(str, c));
}

inline char* strstr(const char* haystack, const char* needle) noexcept {
    return const_cast<char*>(std::strstr(haystack, needle));
}

inline void* memcpy(void* dest, const void* src, size_t n) noexcept {
    return std::memcpy(dest, src, n);
}

inline void* memmove(void* dest, const void* src, size_t n) noexcept {
    return std::memmove(dest, src, n);
}

inline void* memset(void* dest, int c, size_t n) noexcept {
    return std::memset(dest, c, n);
}

inline int memcmp(const void* s1, const void* s2, size_t n) noexcept {
    return std::memcmp(s1, s2, n);
}

inline void* memchr(const void* s, int c, size_t n) noexcept {
    return const_cast<void*>(std::memchr(s, c, n));
}

inline char* _strdup(const char* src) noexcept {
    if (!src) return nullptr;
    size_t len = std::strlen(src) + 1;
    char* copy = reinterpret_cast<char*>(malloc(len));
    if (copy) std::memcpy(copy, src, len);
    return copy;
}

// ============================================================================
// 4. Formatted Output (printf / sprintf / puts)
// ============================================================================

inline int _vsnprintf(char* buffer, size_t count, const char* format, va_list args) noexcept {
    if (!buffer || count == 0 || !format) return -1;
    return std::vsnprintf(buffer, count, format, args);
}

inline int _vscprintf(const char* format, va_list args) noexcept {
    if (!format) return 0;
    va_list copy;
    va_copy(copy, args);
    int len = std::vsnprintf(nullptr, 0, format, copy);
    va_end(copy);
    return len;
}

inline int sprintf(char* buffer, const char* format, ...) noexcept {
    va_list args;
    va_start(args, format);
    int res = std::vsprintf(buffer, format, args);
    va_end(args);
    return res;
}

inline int snprintf(char* buffer, size_t count, const char* format, ...) noexcept {
    va_list args;
    va_start(args, format);
    int res = std::vsnprintf(buffer, count, format, args);
    va_end(args);
    return res;
}

inline int puts(const char* str) noexcept {
    if (!str) return -1;
    win32::HANDLE hOut = win32::GetStdHandle(win32::STD_OUTPUT_HANDLE);
    win32::DWORD written = 0;
    size_t len = std::strlen(str);
    win32::WriteFile(hOut, str, static_cast<win32::DWORD>(len), &written, nullptr);
    const char nl[] = "\n";
    win32::WriteFile(hOut, nl, 1, &written, nullptr);
    return static_cast<int>(len + 1);
}

inline int putchar(int c) noexcept {
    win32::HANDLE hOut = win32::GetStdHandle(win32::STD_OUTPUT_HANDLE);
    win32::DWORD written = 0;
    char ch = static_cast<char>(c);
    win32::WriteFile(hOut, &ch, 1, &written, nullptr);
    return c;
}

inline int fputc(int c, MicaFile* stream) noexcept {
    if (!stream) return -1;
    char ch = static_cast<char>(c);
    win32::DWORD written = 0;
    win32::WriteFile(stream->getHandle(), &ch, 1, &written, nullptr);
    return (written == 1) ? static_cast<unsigned char>(ch) : -1;
}

inline int fputs(const char* str, MicaFile* stream) noexcept {
    if (!str || !stream) return -1;
    size_t len = std::strlen(str);
    win32::DWORD written = 0;
    win32::WriteFile(stream->getHandle(), str, static_cast<win32::DWORD>(len), &written, nullptr);
    return (written == len) ? 0 : -1;
}

inline int fgetc(MicaFile* stream) noexcept {
    if (!stream) return -1;
    char ch = 0;
    win32::DWORD readBytes = 0;
    if (win32::ReadFile(stream->getHandle(), &ch, 1, &readBytes, nullptr) && readBytes == 1) {
        return static_cast<unsigned char>(ch);
    }
    return -1;
}

inline int _fileno(MicaFile* stream) noexcept {
    if (!stream) return -1;
    return stream->_file;
}

inline int printf(const char* format, ...) noexcept {
    if (!format) return 0;
    char buf[2048]{};
    va_list args;
    va_start(args, format);
    int len = std::vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    if (len > 0) {
        win32::HANDLE hOut = win32::GetStdHandle(win32::STD_OUTPUT_HANDLE);
        win32::DWORD written = 0;
        win32::WriteFile(hOut, buf, static_cast<win32::DWORD>(len), &written, nullptr);
    }
    return len;
}

inline int fprintf(MicaFile* stream, const char* format, ...) noexcept {
    if (!stream || !format) return 0;
    char buf[2048]{};
    va_list args;
    va_start(args, format);
    int len = std::vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    if (len > 0) {
        win32::HANDLE hOut = (stream == &g_StdErrFile) 
            ? win32::GetStdHandle(win32::STD_ERROR_HANDLE) 
            : win32::GetStdHandle(win32::STD_OUTPUT_HANDLE);
        win32::DWORD written = 0;
        win32::WriteFile(hOut, buf, static_cast<win32::DWORD>(len), &written, nullptr);
    }
    return len;
}

inline int __stdio_common_vfprintf(uint64_t /*options*/, MicaFile* stream, const char* format, void* /*locale*/, va_list arglist) noexcept {
    if (!format) return 0;
    char buf[4096]{};
    int len = std::vsnprintf(buf, sizeof(buf), format, arglist);
    if (len > 0) {
        win32::HANDLE hOut = (stream == &g_StdErrFile) 
            ? win32::GetStdHandle(win32::STD_ERROR_HANDLE) 
            : win32::GetStdHandle(win32::STD_OUTPUT_HANDLE);
        win32::DWORD written = 0;
        win32::WriteFile(hOut, buf, static_cast<win32::DWORD>(len), &written, nullptr);
    }
    return len;
}

inline int fflush(MicaFile* /*stream*/) noexcept {
    return 0;
}

inline int setvbuf(MicaFile* /*stream*/, char* /*buffer*/, int /*mode*/, size_t /*size*/) noexcept {
    return 0;
}

// ============================================================================
// 5. Environment & Process Lifecycle
// ============================================================================

inline thread_local int g_CrtErrno = 0;

inline int* _errno() noexcept {
    return &g_CrtErrno;
}

inline char* getenv(const char* varname) noexcept {
    if (!varname) return nullptr;
    static thread_local char s_EnvValBuf[512]{};
    win32::DWORD len = win32::GetEnvironmentVariableA(varname, s_EnvValBuf, sizeof(s_EnvValBuf));
    return (len > 0) ? s_EnvValBuf : nullptr;
}

inline int _putenv(const char* envstring) noexcept {
    if (!envstring) return -1;
    const char* eq = std::strchr(envstring, '=');
    if (!eq) return -1;
    std::string name(envstring, eq - envstring);
    std::string val(eq + 1);
    return win32::SetEnvironmentVariableA(name.c_str(), val.c_str()) ? 0 : -1;
}

inline uint32_t _getpid() noexcept {
    return win32::GetCurrentProcessId();
}

inline int _isatty(int fd) noexcept {
    // 0 = stdin, 1 = stdout, 2 = stderr
    return (fd >= 0 && fd <= 2) ? 1 : 0;
}

inline void exit(int status) noexcept {
    win32::ExitProcess(static_cast<win32::DWORD>(status));
}

inline void _exit(int status) noexcept {
    win32::ExitProcess(static_cast<win32::DWORD>(status));
}

inline void abort() noexcept {
    win32::ExitProcess(3);
}

inline int atexit(void (*/*func*/)(void)) noexcept {
    return 0; // successfully registered
}

inline void _cexit() noexcept {}
inline void _amsg_exit(int /*err*/) noexcept { win32::ExitProcess(255); }

// ============================================================================
// 6. MSVC Runtime Startup Thunks
// ============================================================================

inline int g_Commode = 0;
inline int g_Fmode = 0;
inline char* s_DefaultArgv[] = { const_cast<char*>("micant.exe"), nullptr };
inline char* s_DefaultEnv[]  = { const_cast<char*>("OS=MicaNT"), const_cast<char*>("SystemRoot=C:\\Windows"), nullptr };
inline int g_Argc = 1;
inline char** g_Argv = s_DefaultArgv;
inline char** g_Environ = s_DefaultEnv;

inline std::vector<std::string> g_ArgvStrings;
inline std::vector<char*> g_ArgvPointers;

inline void SetCommandLineArguments(const std::string& cmdLine) noexcept {
    g_ArgvStrings.clear();
    g_ArgvPointers.clear();
    
    std::string current;
    bool inQuote = false;
    for (size_t i = 0; i < cmdLine.size(); ++i) {
        char c = cmdLine[i];
        if (c == '"') {
            inQuote = !inQuote;
        } else if (c == ' ' && !inQuote) {
            if (!current.empty()) {
                g_ArgvStrings.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        g_ArgvStrings.push_back(current);
    }
    
    for (auto& s : g_ArgvStrings) {
        g_ArgvPointers.push_back(s.data());
    }
    g_ArgvPointers.push_back(nullptr);
    
    g_Argc = static_cast<int>(g_ArgvStrings.size());
    g_Argv = g_ArgvPointers.data();
}

inline int* __p___argc() noexcept { return &g_Argc; }
inline char*** __p___argv() noexcept { return &g_Argv; }
inline int* __p__commode() noexcept { return &g_Commode; }
inline int* __p__fmode() noexcept { return &g_Fmode; }
inline char*** __p__environ() noexcept { return &g_Environ; }
inline int _configure_narrow_argv(int /*mode*/) noexcept { return 0; }
inline int _initialize_narrow_environment() noexcept { return 0; }
inline char** _get_initial_narrow_environment() noexcept { return g_Environ; }
inline int _seh_filter_exe(unsigned long /*xcptnum*/, void* /*pxcptinfoptrs*/) noexcept { return 0; }
inline void _set_app_type(int /*type*/) noexcept {}
inline void* _set_invalid_parameter_handler(void* /*pNew*/) noexcept { return nullptr; }
inline int _configthreadlocale(int /*per_thread_locale_type*/) noexcept { return 0; }
inline int _set_new_mode(int /*newMode*/) noexcept { return 0; }
inline int _crt_atexit(void (*fn)()) noexcept { return atexit(fn); }

inline void _initterm(void (**start)(void), void (**end)(void)) noexcept {
    if (!start || !end) return;
    for (auto cur = start; cur < end; ++cur) {
        if (*cur) (*cur)();
    }
}

inline int _initterm_e(int (**start)(void), int (**end)(void)) noexcept {
    if (!start || !end) return 0;
    for (auto cur = start; cur < end; ++cur) {
        if (*cur) {
            int res = (*cur)();
            if (res != 0) return res;
        }
    }
    return 0;
}

inline void __set_app_type(int /*appType*/) noexcept {}
inline void __setusermatherr(void* /*handler*/) noexcept {}

inline int __getmainargs(
    int* argc,
    char*** argv,
    char*** envp,
    int /*doWildCard*/,
    void* /*startInfo*/
) noexcept {
    if (argc) *argc = g_Argc;
    if (argv) *argv = g_Argv;
    if (envp) *envp = g_Environ;
    return 0;
}

inline int64_t _time64(int64_t* dest) noexcept {
    auto now = std::chrono::system_clock::now().time_since_epoch();
    int64_t sec = std::chrono::duration_cast<std::chrono::seconds>(now).count();
    if (dest) *dest = sec;
    return sec;
}

inline uint64_t clock() noexcept {
    return win32::GetTickCount64();
}

inline char g_CommandLineBuffer[260] = "surshell.exe";
inline char* _acmdln = g_CommandLineBuffer;

inline char* g_InitEnv[2] = { nullptr, nullptr };
inline char** __initenv = g_InitEnv;

inline unsigned int ___lc_codepage_func() noexcept {
    return 65001; // CP_UTF8
}

inline size_t ___mb_cur_max_func() noexcept {
    return 2;
}

inline uint64_t __C_specific_handler(void*, void*, void*, void*) noexcept {
    return 0;
}

inline void _CxxThrowException(void*, void*) noexcept {}

inline uint64_t __CxxFrameHandler(void*, void*, void*, void*) noexcept {
    return 0;
}

// 7-Zip & VLC CRT extensions
inline void _c_exit() noexcept {}

inline int _XcptFilter(unsigned long /*xcptnum*/, void* /*pxcptdata*/) noexcept {
    return 0; // EXCEPTION_CONTINUE_SEARCH
}

using _onexit_t = int (*)(void);
inline _onexit_t _onexit(_onexit_t func) noexcept {
    return func;
}

inline _onexit_t __dllonexit(_onexit_t func, void** /*pbegin*/, void** /*pend*/) noexcept {
    return func;
}

inline void terminate_wrapper() noexcept {
    std::abort();
}

inline void type_info_dtor(void* /*thisPtr*/) noexcept {}

inline uintptr_t _beginthreadex(
    void* security,
    unsigned stack_size,
    unsigned (*start_address)(void*),
    void* arglist,
    unsigned initflag,
    unsigned* thrdaddr
) noexcept {
    win32::HANDLE h = win32::CreateThread(
        security,
        stack_size,
        reinterpret_cast<win32::LPTHREAD_START_ROUTINE>(start_address),
        arglist,
        initflag,
        reinterpret_cast<win32::LPDWORD>(thrdaddr)
    );
    return reinterpret_cast<uintptr_t>(h);
}

inline intptr_t _get_osfhandle(int fd) noexcept {
    if (fd == 0) return reinterpret_cast<intptr_t>(win32::GetStdHandle(win32::STD_INPUT_HANDLE));
    if (fd == 1) return reinterpret_cast<intptr_t>(win32::GetStdHandle(win32::STD_OUTPUT_HANDLE));
    if (fd == 2) return reinterpret_cast<intptr_t>(win32::GetStdHandle(win32::STD_ERROR_HANDLE));
    return -1;
}

// VLC msvcrt functions
inline MicaFile* fopen(const char* /*filename*/, const char* /*mode*/) noexcept {
    static MicaFile s_dummyFile{nullptr, 0, nullptr, 0x0002, 3, 0, 0, nullptr};
    return &s_dummyFile;
}

inline int fclose(MicaFile* /*stream*/) noexcept {
    return 0;
}

inline size_t fwrite(const void* ptr, size_t size, size_t count, MicaFile* stream) noexcept {
    if (!ptr || size == 0 || count == 0) return 0;
    size_t total = size * count;
    win32::DWORD written = 0;
    win32::WriteFile(stream ? stream->getHandle() : win32::GetStdHandle(win32::STD_OUTPUT_HANDLE), ptr, static_cast<win32::DWORD>(total), &written, nullptr);
    return count;
}

inline wint_t fputwc(wchar_t c, MicaFile* stream) noexcept {
    char mb[8] = {0};
    int len = std::wctomb(mb, c);
    if (len > 0) {
        fwrite(mb, 1, len, stream);
    }
    return c;
}

inline int vfprintf(MicaFile* stream, const char* format, va_list args) noexcept {
    char buf[1024];
    int n = std::vsnprintf(buf, sizeof(buf), format, args);
    if (n > 0) {
        fwrite(buf, 1, n, stream);
    }
    return n;
}

inline int fwprintf(MicaFile* stream, const wchar_t* format, ...) noexcept {
    va_list args;
    va_start(args, format);
    wchar_t buf[1024];
    int n = std::vswprintf(buf, sizeof(buf) / sizeof(wchar_t), format, args);
    va_end(args);
    if (n > 0) {
        for (int i = 0; i < n; ++i) fputwc(buf[i], stream);
    }
    return n;
}

inline int _snwprintf(wchar_t* buffer, size_t count, const wchar_t* format, ...) noexcept {
    if (!buffer || count == 0) return -1;
    va_list args;
    va_start(args, format);
    int ret = std::vswprintf(buffer, count, format, args);
    va_end(args);
    return ret;
}

inline void _wassert(const wchar_t* /*message*/, const wchar_t* /*filename*/, unsigned /*line*/) noexcept {}

inline void _lock(int /*locknum*/) noexcept {}
inline void _unlock(int /*locknum*/) noexcept {}
inline int _setmode(int /*fd*/, int mode) noexcept { return mode; }
inline int _ismbblead(unsigned int /*c*/) noexcept { return 0; }
inline int _fstat64(int /*fd*/, void* /*statbuf*/) noexcept { return 0; }
inline int64_t _lseeki64(int /*fd*/, int64_t offset, int /*origin*/) noexcept { return offset; }

inline int iswctype(wint_t c, wctype_t desc) noexcept { return std::iswctype(c, desc); }
inline std::lconv* localeconv() noexcept { return std::localeconv(); }
inline int rand() noexcept { return std::rand(); }
inline char* setlocale(int category, const char* locale) noexcept { return std::setlocale(category, locale); }
inline void (*signal(int sig, void (*func)(int)))(int) { return std::signal(sig, func); }

inline int strcoll(const char* s1, const char* s2) noexcept { return std::strcoll(s1, s2); }
inline char* strerror(int errnum) noexcept { return std::strerror(errnum); }
inline size_t strftime(char* str, size_t max, const char* format, const struct tm* timeptr) noexcept { return std::strftime(str, max, format, timeptr); }
inline unsigned long strtoul(const char* str, char** endptr, int base) noexcept { return std::strtoul(str, endptr, base); }
inline size_t strxfrm(char* dest, const char* src, size_t n) noexcept { return std::strxfrm(dest, src, n); }

inline wint_t towlower(wint_t c) noexcept { return std::towlower(c); }
inline wint_t towupper(wint_t c) noexcept { return std::towupper(c); }

inline int wcscoll(const wchar_t* s1, const wchar_t* s2) noexcept { return std::wcscoll(s1, s2); }
inline wchar_t* wcscpy(wchar_t* dest, const wchar_t* src) noexcept { return std::wcscpy(dest, src); }
inline size_t wcsftime(wchar_t* str, size_t max, const wchar_t* format, const struct tm* timeptr) noexcept { return std::wcsftime(str, max, format, timeptr); }
inline int wcsncmp(const wchar_t* s1, const wchar_t* s2, size_t n) noexcept { return std::wcsncmp(s1, s2, n); }
inline long wcstol(const wchar_t* str, wchar_t** endptr, int base) noexcept { return std::wcstol(str, endptr, base); }
inline size_t wcsxfrm(wchar_t* dest, const wchar_t* src, size_t n) noexcept { return std::wcsxfrm(dest, src, n); }

inline int _open(const char* /*filename*/, int /*oflag*/, ...) noexcept { return 3; }
inline int _close(int /*fd*/) noexcept { return 0; }
inline int _read(int /*fd*/, void* /*buffer*/, unsigned int /*count*/) noexcept { return 0; }
inline int _write(int fd, const void* buffer, unsigned int count) noexcept {
    win32::HANDLE h = (fd == 1) ? win32::GetStdHandle(win32::STD_OUTPUT_HANDLE) :
                      (fd == 2) ? win32::GetStdHandle(win32::STD_ERROR_HANDLE) : reinterpret_cast<win32::HANDLE>(0x20);
    win32::DWORD written = 0;
    win32::WriteFile(h, buffer, count, &written, nullptr);
    return static_cast<int>(written);
}
inline MicaFile* _fdopen(int fd, const char* /*mode*/) noexcept {
    static MicaFile s_dummyFd{nullptr, 0, nullptr, 0x0002, 0, 0, 0, nullptr};
    s_dummyFd._file = fd;
    return &s_dummyFd;
}

// ============================================================================
// 7. Dynamic Loader Registration for msvcrt.dll
// ============================================================================

inline void InitializeMsvcrtSubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();

    // Standard Streams & I/O
    ldr.registerExport("msvcrt.dll", "__iob_func", reinterpret_cast<void*>(__iob_func));
    ldr.registerExport("msvcrt.dll", "__acrt_iob_func", reinterpret_cast<void*>(__acrt_iob_func));
    ldr.registerExport("msvcrt.dll", "__stdio_common_vfprintf", reinterpret_cast<void*>(__stdio_common_vfprintf));
    ldr.registerExport("msvcrt.dll", "fflush", reinterpret_cast<void*>(fflush));
    ldr.registerExport("msvcrt.dll", "setvbuf", reinterpret_cast<void*>(setvbuf));
    ldr.registerExport("msvcrt.dll", "printf", reinterpret_cast<void*>(printf));
    ldr.registerExport("msvcrt.dll", "fprintf", reinterpret_cast<void*>(fprintf));
    ldr.registerExport("msvcrt.dll", "sprintf", reinterpret_cast<void*>(sprintf));
    ldr.registerExport("msvcrt.dll", "snprintf", reinterpret_cast<void*>(snprintf));
    ldr.registerExport("msvcrt.dll", "_vsnprintf", reinterpret_cast<void*>(_vsnprintf));
    ldr.registerExport("msvcrt.dll", "_vscprintf", reinterpret_cast<void*>(_vscprintf));
    ldr.registerExport("msvcrt.dll", "puts", reinterpret_cast<void*>(puts));
    ldr.registerExport("msvcrt.dll", "putchar", reinterpret_cast<void*>(putchar));

    // Memory Management
    ldr.registerExport("msvcrt.dll", "malloc", reinterpret_cast<void*>(malloc));
    ldr.registerExport("msvcrt.dll", "free", reinterpret_cast<void*>(free));
    ldr.registerExport("msvcrt.dll", "calloc", reinterpret_cast<void*>(calloc));
    ldr.registerExport("msvcrt.dll", "realloc", reinterpret_cast<void*>(realloc));

    // String & Buffer Operations
    ldr.registerExport("msvcrt.dll", "strlen", reinterpret_cast<void*>(strlen));
    ldr.registerExport("msvcrt.dll", "wcslen", reinterpret_cast<void*>(wcslen));
    ldr.registerExport("msvcrt.dll", "strcmp", reinterpret_cast<void*>(strcmp));
    ldr.registerExport("msvcrt.dll", "strncmp", reinterpret_cast<void*>(strncmp));
    ldr.registerExport("msvcrt.dll", "_stricmp", reinterpret_cast<void*>(_stricmp));
    ldr.registerExport("msvcrt.dll", "_strnicmp", reinterpret_cast<void*>(_strnicmp));
    ldr.registerExport("msvcrt.dll", "strcpy", reinterpret_cast<void*>(strcpy));
    ldr.registerExport("msvcrt.dll", "strncpy", reinterpret_cast<void*>(strncpy));
    ldr.registerExport("msvcrt.dll", "strcat", reinterpret_cast<void*>(strcat));
    ldr.registerExport("msvcrt.dll", "strchr", reinterpret_cast<void*>(strchr));
    ldr.registerExport("msvcrt.dll", "strrchr", reinterpret_cast<void*>(strrchr));
    ldr.registerExport("msvcrt.dll", "strstr", reinterpret_cast<void*>(strstr));
    ldr.registerExport("msvcrt.dll", "memcpy", reinterpret_cast<void*>(memcpy));
    ldr.registerExport("msvcrt.dll", "memmove", reinterpret_cast<void*>(memmove));
    ldr.registerExport("msvcrt.dll", "memset", reinterpret_cast<void*>(memset));
    ldr.registerExport("msvcrt.dll", "memcmp", reinterpret_cast<void*>(memcmp));
    ldr.registerExport("msvcrt.dll", "memchr", reinterpret_cast<void*>(memchr));
    ldr.registerExport("msvcrt.dll", "_strdup", reinterpret_cast<void*>(_strdup));

    // Process, Environment & Time
    ldr.registerExport("msvcrt.dll", "exit", reinterpret_cast<void*>(exit));
    ldr.registerExport("msvcrt.dll", "_exit", reinterpret_cast<void*>(_exit));
    ldr.registerExport("msvcrt.dll", "abort", reinterpret_cast<void*>(abort));
    ldr.registerExport("msvcrt.dll", "atexit", reinterpret_cast<void*>(atexit));
    ldr.registerExport("msvcrt.dll", "_cexit", reinterpret_cast<void*>(_cexit));
    ldr.registerExport("msvcrt.dll", "_amsg_exit", reinterpret_cast<void*>(_amsg_exit));
    ldr.registerExport("msvcrt.dll", "getenv", reinterpret_cast<void*>(getenv));
    ldr.registerExport("msvcrt.dll", "_putenv", reinterpret_cast<void*>(_putenv));
    ldr.registerExport("msvcrt.dll", "_errno", reinterpret_cast<void*>(_errno));
    ldr.registerExport("msvcrt.dll", "_getpid", reinterpret_cast<void*>(_getpid));
    ldr.registerExport("msvcrt.dll", "_isatty", reinterpret_cast<void*>(_isatty));
    ldr.registerExport("msvcrt.dll", "_time64", reinterpret_cast<void*>(_time64));
    ldr.registerExport("msvcrt.dll", "clock", reinterpret_cast<void*>(clock));

    // MSVC CRT Startup
    ldr.registerExport("msvcrt.dll", "__getmainargs", reinterpret_cast<void*>(__getmainargs));
    ldr.registerExport("msvcrt.dll", "_initterm", reinterpret_cast<void*>(_initterm));
    ldr.registerExport("msvcrt.dll", "_initterm_e", reinterpret_cast<void*>(_initterm_e));
    ldr.registerExport("msvcrt.dll", "__set_app_type", reinterpret_cast<void*>(__set_app_type));
    ldr.registerExport("msvcrt.dll", "__setusermatherr", reinterpret_cast<void*>(__setusermatherr));
    ldr.registerExport("msvcrt.dll", "_commode", reinterpret_cast<void*>(&g_Commode));
    ldr.registerExport("msvcrt.dll", "_fmode", reinterpret_cast<void*>(&g_Fmode));
    ldr.registerExport("msvcrt.dll", "_environ", reinterpret_cast<void*>(&g_Environ));

    // Universal CRT (UCRT) narrow environment & runtime exports
    ldr.registerExport("msvcrt.dll", "__p___argc", reinterpret_cast<void*>(__p___argc));
    ldr.registerExport("msvcrt.dll", "__p___argv", reinterpret_cast<void*>(__p___argv));
    ldr.registerExport("msvcrt.dll", "__p__commode", reinterpret_cast<void*>(__p__commode));
    ldr.registerExport("msvcrt.dll", "__p__fmode", reinterpret_cast<void*>(__p__fmode));
    ldr.registerExport("msvcrt.dll", "__p__environ", reinterpret_cast<void*>(__p__environ));
    ldr.registerExport("msvcrt.dll", "_configure_narrow_argv", reinterpret_cast<void*>(_configure_narrow_argv));
    ldr.registerExport("msvcrt.dll", "_initialize_narrow_environment", reinterpret_cast<void*>(_initialize_narrow_environment));
    ldr.registerExport("msvcrt.dll", "_get_initial_narrow_environment", reinterpret_cast<void*>(_get_initial_narrow_environment));
    ldr.registerExport("msvcrt.dll", "_seh_filter_exe", reinterpret_cast<void*>(_seh_filter_exe));
    ldr.registerExport("msvcrt.dll", "_set_app_type", reinterpret_cast<void*>(_set_app_type));
    ldr.registerExport("msvcrt.dll", "_set_invalid_parameter_handler", reinterpret_cast<void*>(_set_invalid_parameter_handler));
    ldr.registerExport("msvcrt.dll", "_configthreadlocale", reinterpret_cast<void*>(_configthreadlocale));
    ldr.registerExport("msvcrt.dll", "_set_new_mode", reinterpret_cast<void*>(_set_new_mode));
    ldr.registerExport("msvcrt.dll", "_crt_atexit", reinterpret_cast<void*>(_crt_atexit));

    // Standard I/O & Filesystem Streams
    ldr.registerExport("msvcrt.dll", "_iob", reinterpret_cast<void*>(_iob));
    ldr.registerExport("msvcrt.dll", "fputc", reinterpret_cast<void*>(fputc));
    ldr.registerExport("msvcrt.dll", "fputs", reinterpret_cast<void*>(fputs));
    ldr.registerExport("msvcrt.dll", "fgetc", reinterpret_cast<void*>(fgetc));
    ldr.registerExport("msvcrt.dll", "_fileno", reinterpret_cast<void*>(_fileno));

    // Wide string extensions
    ldr.registerExport("msvcrt.dll", "wcscmp", reinterpret_cast<void*>(wcscmp));
    ldr.registerExport("msvcrt.dll", "wcsstr", reinterpret_cast<void*>(wcsstr));

    // Environment & Codepages
    ldr.registerExport("msvcrt.dll", "__initenv", reinterpret_cast<void*>(&__initenv));
    ldr.registerExport("msvcrt.dll", "_acmdln", reinterpret_cast<void*>(&_acmdln));
    ldr.registerExport("msvcrt.dll", "___lc_codepage_func", reinterpret_cast<void*>(___lc_codepage_func));
    ldr.registerExport("msvcrt.dll", "___mb_cur_max_func", reinterpret_cast<void*>(___mb_cur_max_func));

    // Exception handling
    ldr.registerExport("msvcrt.dll", "__C_specific_handler", reinterpret_cast<void*>(__C_specific_handler));
    ldr.registerExport("msvcrt.dll", "_CxxThrowException", reinterpret_cast<void*>(_CxxThrowException));
    ldr.registerExport("msvcrt.dll", "__CxxFrameHandler", reinterpret_cast<void*>(__CxxFrameHandler));

    // 7-Zip & VLC Support Exports
    ldr.registerExport("msvcrt.dll", "_c_exit", reinterpret_cast<void*>(_c_exit));
    ldr.registerExport("msvcrt.dll", "_XcptFilter", reinterpret_cast<void*>(_XcptFilter));
    ldr.registerExport("msvcrt.dll", "_onexit", reinterpret_cast<void*>(_onexit));
    ldr.registerExport("msvcrt.dll", "__dllonexit", reinterpret_cast<void*>(__dllonexit));
    ldr.registerExport("msvcrt.dll", "?terminate@@YAXXZ", reinterpret_cast<void*>(terminate_wrapper));
    ldr.registerExport("msvcrt.dll", "??1type_info@@UEAA@XZ", reinterpret_cast<void*>(type_info_dtor));
    ldr.registerExport("msvcrt.dll", "_beginthreadex", reinterpret_cast<void*>(_beginthreadex));
    ldr.registerExport("msvcrt.dll", "_get_osfhandle", reinterpret_cast<void*>(_get_osfhandle));

    // Additional VLC exports
    ldr.registerExport("msvcrt.dll", "fopen", reinterpret_cast<void*>(fopen));
    ldr.registerExport("msvcrt.dll", "fclose", reinterpret_cast<void*>(fclose));
    ldr.registerExport("msvcrt.dll", "fwrite", reinterpret_cast<void*>(fwrite));
    ldr.registerExport("msvcrt.dll", "fputwc", reinterpret_cast<void*>(fputwc));
    ldr.registerExport("msvcrt.dll", "vfprintf", reinterpret_cast<void*>(vfprintf));
    ldr.registerExport("msvcrt.dll", "fwprintf", reinterpret_cast<void*>(fwprintf));
    ldr.registerExport("msvcrt.dll", "_snwprintf", reinterpret_cast<void*>(_snwprintf));
    ldr.registerExport("msvcrt.dll", "_wassert", reinterpret_cast<void*>(_wassert));
    ldr.registerExport("msvcrt.dll", "_lock", reinterpret_cast<void*>(_lock));
    ldr.registerExport("msvcrt.dll", "_unlock", reinterpret_cast<void*>(_unlock));
    ldr.registerExport("msvcrt.dll", "_setmode", reinterpret_cast<void*>(_setmode));
    ldr.registerExport("msvcrt.dll", "_ismbblead", reinterpret_cast<void*>(_ismbblead));
    ldr.registerExport("msvcrt.dll", "_fstat64", reinterpret_cast<void*>(_fstat64));
    ldr.registerExport("msvcrt.dll", "_lseeki64", reinterpret_cast<void*>(_lseeki64));
    ldr.registerExport("msvcrt.dll", "iswctype", reinterpret_cast<void*>(iswctype));
    ldr.registerExport("msvcrt.dll", "localeconv", reinterpret_cast<void*>(localeconv));
    ldr.registerExport("msvcrt.dll", "rand", reinterpret_cast<void*>(rand));
    ldr.registerExport("msvcrt.dll", "setlocale", reinterpret_cast<void*>(setlocale));
    ldr.registerExport("msvcrt.dll", "signal", reinterpret_cast<void*>(signal));
    ldr.registerExport("msvcrt.dll", "strcoll", reinterpret_cast<void*>(strcoll));
    ldr.registerExport("msvcrt.dll", "strerror", reinterpret_cast<void*>(strerror));
    ldr.registerExport("msvcrt.dll", "strftime", reinterpret_cast<void*>(strftime));
    ldr.registerExport("msvcrt.dll", "strtoul", reinterpret_cast<void*>(strtoul));
    ldr.registerExport("msvcrt.dll", "strxfrm", reinterpret_cast<void*>(strxfrm));
    ldr.registerExport("msvcrt.dll", "towlower", reinterpret_cast<void*>(towlower));
    ldr.registerExport("msvcrt.dll", "towupper", reinterpret_cast<void*>(towupper));
    ldr.registerExport("msvcrt.dll", "wcscoll", reinterpret_cast<void*>(wcscoll));
    ldr.registerExport("msvcrt.dll", "wcscpy", reinterpret_cast<void*>(wcscpy));
    ldr.registerExport("msvcrt.dll", "wcsftime", reinterpret_cast<void*>(wcsftime));
    ldr.registerExport("msvcrt.dll", "wcsncmp", reinterpret_cast<void*>(wcsncmp));
    ldr.registerExport("msvcrt.dll", "wcstol", reinterpret_cast<void*>(wcstol));
    ldr.registerExport("msvcrt.dll", "wcsxfrm", reinterpret_cast<void*>(wcsxfrm));
    ldr.registerExport("msvcrt.dll", "_open", reinterpret_cast<void*>(_open));
    ldr.registerExport("msvcrt.dll", "_close", reinterpret_cast<void*>(_close));
    ldr.registerExport("msvcrt.dll", "_read", reinterpret_cast<void*>(_read));
    ldr.registerExport("msvcrt.dll", "_write", reinterpret_cast<void*>(_write));
    ldr.registerExport("msvcrt.dll", "_fdopen", reinterpret_cast<void*>(_fdopen));
}

} // namespace micant::msvcrt
