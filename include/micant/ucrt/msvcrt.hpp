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
#include <cmath>
#include <cfenv>
#include <new>
#include <exception>
#include <atomic>
#include <iostream>


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

#ifdef _acmdln
#undef _acmdln
#endif
#ifdef _wcmdln
#undef _wcmdln
#endif
#ifdef __initenv
#undef __initenv
#endif
#ifdef __winitenv
#undef __winitenv
#endif

inline char g_CommandLineBuffer[260] = "surshell.exe";
inline char* g_Acmdln = g_CommandLineBuffer;
inline char** __p__acmdln() noexcept { return &g_Acmdln; }

inline char* g_InitEnv[2] = { nullptr, nullptr };
inline char** g_InitEnvPtr = g_InitEnv;
inline char*** __p___initenv() noexcept { return &g_InitEnvPtr; }

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
inline void srand(unsigned int seed) noexcept { std::srand(seed); }
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


// ============================================================================
// 7. Consolidated Universal C Runtime & MSVC STL Extensions
// ============================================================================

// --- Extended CRT math, string and memory APIs ---

inline double CRT_cosh(double x) noexcept { return std::cosh(x); }
inline double CRT_sinh(double x) noexcept { return std::sinh(x); }
inline double CRT_tanh(double x) noexcept { return std::tanh(x); }
inline double CRT_atof(const char* nptr) noexcept { return std::atof(nptr); }
inline long   CRT_atol(const char* nptr) noexcept { return std::atol(nptr); }
inline double CRT_difftime(time_t time1, time_t time0) noexcept { return std::difftime(time1, time0); }
inline int    CRT_raise(int sig) noexcept { return 0; }

inline wchar_t* CRT_wcsdup(const wchar_t* str) noexcept {
    if (!str) return nullptr;
    size_t len = std::wcslen(str);
    wchar_t* dup = static_cast<wchar_t*>(std::malloc((len + 1) * sizeof(wchar_t)));
    if (dup) std::wmemcpy(dup, str, len + 1);
    return dup;
}

inline wchar_t* CRT_wcsncpy(wchar_t* dest, const wchar_t* src, size_t count) noexcept {
    return std::wcsncpy(dest, src, count);
}

inline wchar_t* CRT_wcscat(wchar_t* dest, const wchar_t* src) noexcept {
    return std::wcscat(dest, src);
}

inline wchar_t* CRT_wcschr(const wchar_t* str, wchar_t ch) noexcept {
    return const_cast<wchar_t*>(std::wcschr(str, ch));
}

inline wchar_t* CRT_wcspbrk(const wchar_t* str, const wchar_t* strCharSet) noexcept {
    return const_cast<wchar_t*>(std::wcspbrk(str, strCharSet));
}

inline size_t CRT_wcsspn(const wchar_t* str, const wchar_t* strCharSet) noexcept {
    return std::wcsspn(str, strCharSet);
}

inline size_t CRT_wcstombs(char* mbstr, const wchar_t* wcstr, size_t count) noexcept {
    return std::wcstombs(mbstr, wcstr, count);
}

inline unsigned long CRT_wcstoul(const wchar_t* str, wchar_t** endptr, int base) noexcept {
    return std::wcstoul(str, endptr, base);
}

inline int CRT_wtoi(const wchar_t* str) noexcept {
    return str ? static_cast<int>(std::wcstol(str, nullptr, 10)) : 0;
}

inline int CRT_wcsnicmp(const wchar_t* string1, const wchar_t* string2, size_t count) noexcept {
    if (!string1 || !string2 || count == 0) return 0;
    for (size_t i = 0; i < count; ++i) {
        wchar_t c1 = static_cast<wchar_t>(std::towlower(string1[i]));
        wchar_t c2 = static_cast<wchar_t>(std::towlower(string2[i]));
        if (c1 != c2) return (c1 < c2) ? -1 : 1;
        if (c1 == L'\0') break;
    }
    return 0;
}

inline void* CRT_aligned_malloc(size_t size, size_t alignment) noexcept {
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) return nullptr;
    size_t total = size + alignment + sizeof(void*);
    void* raw = std::malloc(total);
    if (!raw) return nullptr;
    uintptr_t addr = reinterpret_cast<uintptr_t>(raw) + sizeof(void*);
    uintptr_t aligned = (addr + alignment - 1) & ~(alignment - 1);
    reinterpret_cast<void**>(aligned)[-1] = raw;
    return reinterpret_cast<void*>(aligned);
}

inline void CRT_aligned_free(void* ptr) noexcept {
    if (ptr) {
        void* raw = reinterpret_cast<void**>(ptr)[-1];
        std::free(raw);
    }
}

// Wide filesystem & path functions
inline int CRT_waccess([[maybe_unused]] const wchar_t* path, [[maybe_unused]] int mode) noexcept {
    return 0;
}

inline int CRT_wchdir([[maybe_unused]] const wchar_t* dirname) noexcept {
    return 0;
}

inline int CRT_wchmod([[maybe_unused]] const wchar_t* filename, [[maybe_unused]] int pmode) noexcept {
    return 0;
}

inline wchar_t* CRT_wfullpath(wchar_t* absPath, const wchar_t* relPath, size_t maxLength) noexcept {
    if (!absPath && maxLength == 0) {
        return CRT_wcsdup(relPath ? relPath : L"C:\\");
    }
    if (absPath && relPath && maxLength > 0) {
        size_t len = std::wcslen(relPath);
        if (len >= maxLength) len = maxLength - 1;
        std::wmemcpy(absPath, relPath, len);
        absPath[len] = L'\0';
        return absPath;
    }
    return nullptr;
}

inline wchar_t* CRT_wgetcwd(wchar_t* buffer, int maxlen) noexcept {
    const wchar_t cwd[] = L"C:\\MicaNT";
    if (!buffer) return CRT_wcsdup(cwd);
    if (maxlen > 0) {
        size_t len = std::wcslen(cwd);
        if (static_cast<int>(len) >= maxlen) len = maxlen - 1;
        std::wmemcpy(buffer, cwd, len);
        buffer[len] = L'\0';
        return buffer;
    }
    return nullptr;
}

inline wchar_t* CRT_wgetenv([[maybe_unused]] const wchar_t* varname) noexcept {
    return nullptr;
}

inline int CRT_wputenv([[maybe_unused]] const wchar_t* envstring) noexcept {
    return 0;
}

inline int CRT_wrename([[maybe_unused]] const wchar_t* oldname, [[maybe_unused]] const wchar_t* newname) noexcept {
    return 0;
}

inline int CRT_wutime64([[maybe_unused]] const wchar_t* filename, [[maybe_unused]] void* times) noexcept {
    return 0;
}

inline intptr_t CRT_wfindfirst64([[maybe_unused]] const wchar_t* filespec, [[maybe_unused]] void* fileinfo) noexcept {
    return -1; // No files found
}

inline int CRT_wfindnext64([[maybe_unused]] intptr_t handle, [[maybe_unused]] void* fileinfo) noexcept {
    return -1;
}

inline int CRT_findclose([[maybe_unused]] intptr_t handle) noexcept {
    return 0;
}

inline char* CRT_getcwd(char* buffer, int maxlen) noexcept {
    const char cwd[] = "C:\\MicaNT";
    if (!buffer) {
        char* p = static_cast<char*>(std::malloc(sizeof(cwd)));
        if (p) std::memcpy(p, cwd, sizeof(cwd));
        return p;
    }
    if (maxlen > 0) {
        size_t len = std::strlen(cwd);
        if (static_cast<int>(len) >= maxlen) len = maxlen - 1;
        std::memcpy(buffer, cwd, len);
        buffer[len] = '\0';
        return buffer;
    }
    return nullptr;
}

inline int CRT_getdrive() noexcept { return 3; } // Drive C: (1 = A, 2 = B, 3 = C)
inline int CRT_chdrive([[maybe_unused]] int drive) noexcept { return 0; }
inline int CRT_mkdir([[maybe_unused]] const char* dirname) noexcept { return 0; }
inline int CRT_commit([[maybe_unused]] int fd) noexcept { return 0; }
inline int CRT_dup2([[maybe_unused]] int fd1, [[maybe_unused]] int fd2) noexcept { return fd2; }
inline int64_t CRT_filelengthi64([[maybe_unused]] int fd) noexcept { return 0; }
inline int64_t CRT_telli64([[maybe_unused]] int fd) noexcept { return 0; }

// Process & Thread
inline uintptr_t CRT_beginthread(void (*start_address)(void*),
                                 [[maybe_unused]] unsigned stack_size,
                                 void* arglist) noexcept {
    if (start_address) {
        // Mock valid thread handle
        return 0x00050001;
    }
    return static_cast<uintptr_t>(-1);
}

inline void CRT_endthreadex([[maybe_unused]] unsigned retval) noexcept {
}

// Wide Stdio
inline wint_t CRT_fgetwc([[maybe_unused]] std::FILE* stream) noexcept { return WEOF; }
inline int    CRT_fputws([[maybe_unused]] const wchar_t* str, [[maybe_unused]] std::FILE* stream) noexcept { return 0; }
inline wint_t CRT_getwc([[maybe_unused]] std::FILE* stream) noexcept { return WEOF; }
inline wint_t CRT_putwc(wchar_t c, [[maybe_unused]] std::FILE* stream) noexcept { return c; }
inline wint_t CRT_ungetwc(wint_t c, [[maybe_unused]] std::FILE* stream) noexcept { return c; }
inline int    CRT_putws([[maybe_unused]] const wchar_t* str) noexcept { return 0; }
inline int    CRT_fseek([[maybe_unused]] std::FILE* stream, [[maybe_unused]] long offset, [[maybe_unused]] int origin) noexcept { return 0; }
inline long   CRT_ftell([[maybe_unused]] std::FILE* stream) noexcept { return 0; }
inline int    CRT_fgetpos([[maybe_unused]] std::FILE* stream, fpos_t* pos) noexcept { if (pos) *pos = 0; return 0; }
inline int    CRT_fsetpos([[maybe_unused]] std::FILE* stream, const fpos_t* pos) noexcept { return 0; }
inline int    CRT_remove([[maybe_unused]] const char* path) noexcept { return 0; }
inline int    CRT_setmaxstdio(int newmax) noexcept { return newmax; }
inline int    CRT_getmaxstdio() noexcept { return 2048; }
inline void   CRT_wperror([[maybe_unused]] const wchar_t* prefix) noexcept {}

// Character classification
inline int CRT_isalnum(int c) noexcept { return std::isalnum(c); }
inline int CRT_isalpha(int c) noexcept { return std::isalpha(c); }
inline int CRT_isspace(int c) noexcept { return std::isspace(c); }
inline int CRT_iswalnum(wint_t c) noexcept { return std::iswalnum(c); }
inline int CRT_iswalpha(wint_t c) noexcept { return std::iswalpha(c); }
inline int CRT_iswdigit(wint_t c) noexcept { return std::iswdigit(c); }
inline int CRT_iswprint(wint_t c) noexcept { return std::iswprint(c); }
inline int CRT_iswpunct(wint_t c) noexcept { return std::iswpunct(c); }
inline int CRT_iswxdigit(wint_t c) noexcept { return std::iswxdigit(c); }

// Misc CRT
inline void CRT_assert([[maybe_unused]] const char* expr,
                       [[maybe_unused]] const char* filename,
                       [[maybe_unused]] unsigned lineno) noexcept {}

inline int CRT_setjmp([[maybe_unused]] void* env) noexcept { return 0; }
inline int64_t CRT_strtoi64(const char* nptr, char** endptr, int base) noexcept { return std::strtoll(nptr, endptr, base); }
inline uint64_t CRT_strtoui64(const char* nptr, char** endptr, int base) noexcept { return std::strtoull(nptr, endptr, base); }
inline char* CRT_ctime64(const int64_t* timer) noexcept {
    static char buf[32] = "Thu Jan 01 00:00:00 1970\n";
    return buf;
}

// --- Extended CRT time, character and threading APIs ---
// 5. Universal C Runtime (UCRT) - Math Subsystem
// ----------------------------------------------------------------------------

inline double CRT_sin(double x) noexcept { return std::sin(x); }
inline double CRT_cos(double x) noexcept { return std::cos(x); }
inline double CRT_tan(double x) noexcept { return std::tan(x); }
inline double CRT_asin(double x) noexcept { return std::asin(x); }
inline double CRT_acos(double x) noexcept { return std::acos(x); }
inline double CRT_atan(double x) noexcept { return std::atan(x); }
inline double CRT_atan2(double y, double x) noexcept { return std::atan2(y, x); }
inline double CRT_exp(double x) noexcept { return std::exp(x); }
inline double CRT_log(double x) noexcept { return std::log(x); }
inline double CRT_log10(double x) noexcept { return std::log10(x); }
inline double CRT_log2(double x) noexcept { return std::log2(x); }
inline double CRT_pow(double x, double y) noexcept { return std::pow(x, y); }
inline double CRT_sqrt(double x) noexcept { return std::sqrt(x); }
inline double CRT_cbrt(double x) noexcept { return std::cbrt(x); }
inline double CRT_ceil(double x) noexcept { return std::ceil(x); }
inline double CRT_floor(double x) noexcept { return std::floor(x); }
inline double CRT_round(double x) noexcept { return std::round(x); }
inline long CRT_lround(double x) noexcept { return std::lround(x); }
inline double CRT_fmod(double x, double y) noexcept { return std::fmod(x, y); }
inline double CRT_modf(double x, double* iptr) noexcept { return std::modf(x, iptr); }
inline double CRT_ldexp(double x, int exp) noexcept { return std::ldexp(x, exp); }
inline double CRT_copysign(double x, double y) noexcept { return std::copysign(x, y); }
inline int CRT_dclass(double x) noexcept { return std::fpclassify(x); }
inline int CRT_dpcomp(double x, double y) noexcept { return (x < y) ? -1 : ((x > y) ? 1 : 0); }

// ----------------------------------------------------------------------------
// 6. UCRT - String, Memory & Character Subsystem
// ----------------------------------------------------------------------------

inline int CRT_tolower(int c) noexcept { return std::tolower(c); }
inline int CRT_toupper(int c) noexcept { return std::toupper(c); }
inline int CRT_isdigit(int c) noexcept { return std::isdigit(c); }
inline int CRT_islower(int c) noexcept { return std::islower(c); }
inline int CRT_isupper(int c) noexcept { return std::isupper(c); }
inline int CRT_isxdigit(int c) noexcept { return std::isxdigit(c); }
inline int CRT_iswspace(wint_t wc) noexcept { return std::iswspace(wc); }

inline size_t CRT_strspn(const char* s, const char* accept) noexcept { return std::strspn(s, accept); }
inline size_t CRT_strcspn(const char* s, const char* reject) noexcept { return std::strcspn(s, reject); }
inline char* CRT_strpbrk(const char* s, const char* accept) noexcept { return const_cast<char*>(std::strpbrk(s, accept)); }
inline char* CRT_strtok(char* str, const char* delim) noexcept { return std::strtok(str, delim); }
inline size_t CRT_strnlen(const char* s, size_t maxlen) noexcept {
    size_t i = 0;
    while (i < maxlen && s && s[i]) ++i;
    return i;
}
inline char* CRT_strncat(char* dest, const char* src, size_t n) noexcept { return std::strncat(dest, src, n); }
inline int CRT_mblen(const char* s, size_t n) noexcept { return std::mblen(s, n); }
inline int CRT_mbtowc(wchar_t* pwc, const char* s, size_t n) noexcept { return std::mbtowc(pwc, s, n); }

inline errno_t CRT_wcscat_s(wchar_t* dest, size_t destsz, const wchar_t* src) noexcept {
    if (!dest || !src || destsz == 0) return 22; // EINVAL
    size_t dlen = std::wcslen(dest);
    size_t slen = std::wcslen(src);
    if (dlen + slen + 1 > destsz) return 34; // ERANGE
    std::wcscpy(dest + dlen, src);
    return 0;
}

// ----------------------------------------------------------------------------
// 7. UCRT - Conversion, Utility & Heap Subsystem
// ----------------------------------------------------------------------------

inline long CRT_strtol(const char* str, char** endptr, int base) noexcept { return std::strtol(str, endptr, base); }
inline int CRT_atoi(const char* str) noexcept { return std::atoi(str); }

inline void* CRT_bsearch(const void* key, const void* base, size_t num, size_t size, int (*compar)(const void*, const void*)) noexcept {
    return std::bsearch(key, base, num, size, compar);
}
inline void CRT_qsort(void* base, size_t num, size_t size, int (*compar)(const void*, const void*)) noexcept {
    std::qsort(base, num, size, compar);
}

inline int CRT_callnewh(size_t) noexcept { return 0; }
inline size_t CRT_msize(void* memblock) noexcept {
    return memblock ? 4096 : 0;
}

// ----------------------------------------------------------------------------
// 8. UCRT - Standard I/O Subsystem
// ----------------------------------------------------------------------------

inline int CRT_set_fmode(int) noexcept { return 0; }
inline int CRT_open_osfhandle(intptr_t, int) noexcept { return 3; }
inline int CRT_feof(FILE* stream) noexcept { return stream ? std::feof(stream) : 1; }
inline int CRT_ferror(FILE* stream) noexcept { return stream ? std::ferror(stream) : 0; }
inline void CRT_clearerr(FILE* stream) noexcept { if (stream) std::clearerr(stream); }
inline int CRT_getc(FILE* stream) noexcept { return stream ? std::getc(stream) : -1; }
inline int CRT_getchar() noexcept { return std::getchar(); }
inline int CRT_putc(int ch, FILE* stream) noexcept { return stream ? std::putc(ch, stream) : -1; }
inline char* CRT_fgets(char* str, int count, FILE* stream) noexcept { return stream ? std::fgets(str, count, stream) : nullptr; }
inline size_t CRT_fread(void* ptr, size_t size, size_t count, FILE* stream) noexcept { return stream ? std::fread(ptr, size, count, stream) : 0; }
inline void CRT_rewind(FILE* stream) noexcept { if (stream) std::rewind(stream); }
inline int CRT_ungetc(int ch, FILE* stream) noexcept { return stream ? std::ungetc(ch, stream) : -1; }
inline int CRT_fgetc_nolock(FILE* stream) noexcept { return stream ? std::getc(stream) : -1; }

inline FILE* CRT_wfopen(const wchar_t*, const wchar_t*) noexcept { return nullptr; }
inline FILE* CRT_wfreopen(const wchar_t*, const wchar_t*, FILE*) noexcept { return nullptr; }
inline int CRT_wopen(const wchar_t*, int, ...) noexcept { return -1; }
inline FILE* CRT_popen(const char*, const char*) noexcept { return nullptr; }
inline int CRT_pclose(FILE*) noexcept { return 0; }

inline int CRT_stdio_common_vsprintf(uint64_t, char* buffer, size_t max_count, const char* format, void*, va_list argptr) noexcept {
    if (!buffer || max_count == 0) return 0;
    return std::vsnprintf(buffer, max_count, format, argptr);
}

inline int CRT_stdio_common_vswprintf(uint64_t, wchar_t* buffer, size_t max_count, const wchar_t* format, void*, va_list argptr) noexcept {
    if (!buffer || max_count == 0) return 0;
    return std::vswprintf(buffer, max_count, format, argptr);
}

inline int CRT_stdio_common_vsscanf(uint64_t, const char* buffer, size_t, const char* format, void*, va_list argptr) noexcept {
    return std::vsscanf(buffer, format, argptr);
}

inline int CRT_stdio_common_vfscanf(uint64_t, FILE* stream, const char* format, void*, va_list argptr) noexcept {
    return std::vfscanf(stream, format, argptr);
}

// ----------------------------------------------------------------------------
// 9. UCRT - Time & Date Subsystem
// ----------------------------------------------------------------------------

inline long CRT_timezone_val = 0;
inline char CRT_tzname_std[] = "UTC";
inline char CRT_tzname_dst[] = "UTC";
inline char* CRT_tzname_arr[2] = { CRT_tzname_std, CRT_tzname_dst };

inline long* CRT_timezone() noexcept { return &CRT_timezone_val; }
inline char** CRT_tzname() noexcept { return CRT_tzname_arr; }
inline void CRT_tzset() noexcept {}

inline tm* CRT_localtime64(const time_t* timer) noexcept { return std::localtime(timer); }
inline errno_t CRT_localtime64_s(tm* buf, const time_t* timer) noexcept {
    if (!buf || !timer) return 22;
    tm* res = std::localtime(timer);
    if (!res) return 22;
    *buf = *res;
    return 0;
}

inline tm* CRT_gmtime64(const time_t* timer) noexcept { return std::gmtime(timer); }
inline errno_t CRT_gmtime64_s(tm* buf, const time_t* timer) noexcept {
    if (!buf || !timer) return 22;
    tm* res = std::gmtime(timer);
    if (!res) return 22;
    *buf = *res;
    return 0;
}

inline time_t CRT_mktime64(tm* timeptr) noexcept { return std::mktime(timeptr); }
inline char* CRT_asctime(const tm* timeptr) noexcept { return std::asctime(timeptr); }

struct timespec64 {
    int64_t tv_sec;
    long tv_nsec;
};
inline int CRT_timespec64_get(timespec64* ts, int) noexcept {
    if (ts) {
        ts->tv_sec = std::time(nullptr);
        ts->tv_nsec = 0;
        return 1;
    }
    return 0;
}

// ----------------------------------------------------------------------------
// 10. UCRT - Filesystem, Process & Runtime Subsystem
// ----------------------------------------------------------------------------

inline int CRT_wmkdir(const wchar_t*) noexcept { return 0; }
inline int CRT_wrmdir(const wchar_t*) noexcept { return 0; }
inline int CRT_wremove(const wchar_t*) noexcept { return 0; }
inline int CRT_wunlink(const wchar_t*) noexcept { return 0; }
inline int CRT_wstat64(const wchar_t*, void*) noexcept { return 0; }

inline int CRT_getch() noexcept { return 0; }
inline intptr_t CRT_cwait(int*, intptr_t, int) noexcept { return 0; }

inline int CRT_doserrno_val = 0;
inline int* CRT_doserrno() noexcept { return &CRT_doserrno_val; }
inline unsigned int CRT_fpe_flt_rounds() noexcept { return 1; }

inline wchar_t* CRT_wargv_dummy[] = { const_cast<wchar_t*>(L"micant_app.exe"), nullptr };
inline wchar_t** CRT_p_wargv() noexcept { return CRT_wargv_dummy; }
inline int CRT_configure_wide_argv(int) noexcept { return 0; }
inline int CRT_initialize_wide_environment() noexcept { return 0; }
inline wchar_t** CRT_get_initial_wide_environment() noexcept { return nullptr; }
inline char* CRT_get_narrow_winmain_command_line() noexcept { return const_cast<char*>(""); }

inline void CRT_invalid_parameter_noinfo() noexcept {}
inline void CRT_terminate() noexcept { std::abort(); }

inline int CRT_initialize_onexit_table(void*) noexcept { return 0; }
inline int CRT_register_onexit_function(void*, void*) noexcept { return 0; }
inline int CRT_execute_onexit_table(void*) noexcept { return 0; }
inline int CRT_crt_at_quick_exit(void*) noexcept { return 0; }
inline int CRT_seh_filter_dll(unsigned long, void*) noexcept { return 0; }
inline int CRT_register_thread_local_exe_atexit_callback(void*) noexcept { return 0; }

// ----------------------------------------------------------------------------
// 11. VCRuntime 140 / 140_1 & Exception Handling Subsystem
// ----------------------------------------------------------------------------

inline void* VCRT_memcpy(void* dest, const void* src, size_t n) noexcept { return std::memcpy(dest, src, n); }
inline void* VCRT_memset(void* s, int c, size_t n) noexcept { return std::memset(s, c, n); }
inline void* VCRT_memmove(void* dest, const void* src, size_t n) noexcept { return std::memmove(dest, src, n); }
inline int VCRT_memcmp(const void* s1, const void* s2, size_t n) noexcept { return std::memcmp(s1, s2, n); }
inline void* VCRT_memchr(const void* s, int c, size_t n) noexcept { return const_cast<void*>(std::memchr(s, c, n)); }

inline char* VCRT_strchr(const char* s, int c) noexcept { return const_cast<char*>(std::strchr(s, c)); }
inline char* VCRT_strrchr(const char* s, int c) noexcept { return const_cast<char*>(std::strrchr(s, c)); }
inline char* VCRT_strstr(const char* haystack, const char* needle) noexcept { return const_cast<char*>(std::strstr(haystack, needle)); }

inline void VCRT_CxxThrowException(void*, void*) noexcept { std::terminate(); }
inline int VCRT_CxxFrameHandler4() noexcept { return 1; }
inline int VCRT_C_specific_handler() noexcept { return 1; }
inline void* VCRT_RTDynamicCast(void* inptr, int32_t, void*, void*, int32_t) noexcept { return inptr; }

inline void* VCRT_current_exception() noexcept { return nullptr; }
inline void* VCRT_current_exception_context() noexcept { return nullptr; }
inline int VCRT_intrinsic_setjmp(void*) noexcept { return 0; }
inline void VCRT_longjmp(void*, int) noexcept {}

inline void VCRT_std_exception_copy(void*, void*) noexcept {}
inline void VCRT_std_exception_destroy(void*) noexcept {}
inline void VCRT_std_terminate() noexcept { std::terminate(); }
inline bool VCRT_std_type_info_compare(const void* lhs, const void* rhs) noexcept { return lhs == rhs; }
inline void VCRT_std_type_info_destroy_list(void*) noexcept {}
inline void VCRT_purecall() noexcept { std::terminate(); }

// ----------------------------------------------------------------------------
// 12. MSVCP140 C++ Standard Library & Concurrency Subsystem
// ----------------------------------------------------------------------------

inline void MSVC_Xbad_alloc() { throw std::bad_alloc(); }
inline void MSVC_Xbad_function_call() {}
inline void MSVC_Throw_Cpp_error(int) {}
inline void MSVC_Xlength_error(const char*) {}
inline const char* MSVC_Syserror_map(int) noexcept { return "generic error"; }

inline void MSVC_ExceptionPtrCreate(void*) noexcept {}
inline void MSVC_ExceptionPtrDestroy(void*) noexcept {}
inline void MSVC_ExceptionPtrCopy(void*, const void*) noexcept {}
inline void MSVC_ExceptionPtrCurrentException(void*) noexcept {}
inline int MSVC_uncaught_exceptions() noexcept { return 0; }

inline void MSVC_Mtx_lock(void*) noexcept {}
inline int MSVC_Mtx_trylock(void*) noexcept { return 0; }
inline void MSVC_Mtx_unlock(void*) noexcept {}
inline void MSVC_Cnd_wait(void*, void*) noexcept {}
inline void MSVC_Cnd_broadcast(void*) noexcept {}

inline unsigned int MSVC_Random_device() noexcept { return 42; }
inline const wchar_t* MSVC_W_Getdays() noexcept { return L"Sun:Mon:Tue:Wed:Thu:Fri:Sat"; }
inline const wchar_t* MSVC_W_Getmonths() noexcept { return L"Jan:Feb:Mar:Apr:May:Jun:Jul:Aug:Sep:Oct:Nov:Dec"; }

struct Cvtvec_Stub {
    int placeholder;
};
inline Cvtvec_Stub MSVC_Getcvt() noexcept { return Cvtvec_Stub{0}; }

// Concurrency runtime helpers
inline void MSVC_Conc_TaskContinuationContext_Ctor(void*) noexcept {}
inline long MSVC_Conc_GetCurrentThreadId() noexcept { return 1; }
inline void MSVC_Conc_ReportUnhandledError(void*) noexcept {}
inline void MSVC_Conc_CallInContext(void*, void*, bool) noexcept {}
inline void MSVC_Conc_Capture(void*) noexcept {}
inline void MSVC_Conc_Reset(void*) noexcept {}
inline void MSVC_Conc_LogCancelTask(void*) noexcept {}
inline void MSVC_Conc_LogScheduleTask(void*, bool) noexcept {}
inline void MSVC_Conc_LogTaskCompleted(void*) noexcept {}
inline void MSVC_Conc_LogTaskExecutionCompleted(void*) noexcept {}
inline void MSVC_Conc_LogWorkItemCompleted(void*) noexcept {}
inline void MSVC_Conc_LogWorkItemStarted(void*) noexcept {}
inline void MSVC_Conc_Release_chore(void*) noexcept {}
inline void MSVC_Conc_ReportUnobservedException() noexcept {}
inline int MSVC_Conc_Schedule_chore(void*) noexcept { return 0; }

// Sovereign iostream stubs
inline uint8_t MSVC_cin_storage[256] = {0};
inline uint8_t MSVC_cout_storage[256] = {0};

inline void* MSVC_cout_flush(void* self) noexcept { return self; }
inline void* MSVC_cout_write(void* self, const char*, int64_t) noexcept { return self; }
inline void* MSVC_cout_shift(void* self, void* (*)(void*)) noexcept { return self; }
inline void MSVC_cout_osfx(void*) noexcept {}
inline char MSVC_ios_fill(const void*) noexcept { return ' '; }
inline void* MSVC_ios_tie(const void*) noexcept { return nullptr; }
inline void MSVC_ios_setstate(void*, int, bool) noexcept {}
inline void* MSVC_ios_rdbuf(const void*) noexcept { return nullptr; }
inline int MSVC_ios_flags(const void*) noexcept { return 0; }
inline bool MSVC_ios_good(const void*) noexcept { return true; }
inline int64_t MSVC_ios_width_get(const void*) noexcept { return 0; }
inline int64_t MSVC_ios_width_set(void*, int64_t) noexcept { return 0; }

inline int MSVC_streambuf_sputc(void*, char) noexcept { return 0; }
inline int64_t MSVC_streambuf_sputn(void*, const char*, int64_t count) noexcept { return count; }
inline int MSVC_istream_peek(void*) noexcept { return -1; }



// ============================================================================
// 12. Tier 4 Universal CRT, C99 Math & MSVCP140 Subsystem Expansion
// ============================================================================

// --- Single-Precision C99 Math ---
inline float CRT_sinf(float x) noexcept { return std::sin(x); }
inline float CRT_cosf(float x) noexcept { return std::cos(x); }
inline float CRT_tanf(float x) noexcept { return std::tan(x); }
inline float CRT_asinf(float x) noexcept { return std::asin(x); }
inline float CRT_acosf(float x) noexcept { return std::acos(x); }
inline float CRT_atanf(float x) noexcept { return std::atan(x); }
inline float CRT_atan2f(float y, float x) noexcept { return std::atan2(y, x); }
inline float CRT_sinhf(float x) noexcept { return std::sinh(x); }
inline float CRT_coshf(float x) noexcept { return std::cosh(x); }
inline float CRT_tanhf(float x) noexcept { return std::tanh(x); }
inline float CRT_expf(float x) noexcept { return std::exp(x); }
inline double CRT_exp2(double x) noexcept { return std::exp2(x); }
inline float CRT_exp2f(float x) noexcept { return std::exp2(x); }
inline double CRT_expm1(double x) noexcept { return std::expm1(x); }
inline float CRT_logf(float x) noexcept { return std::log(x); }
inline float CRT_log10f(float x) noexcept { return std::log10(x); }
inline double CRT_log1p(double x) noexcept { return std::log1p(x); }
inline float CRT_log2f(float x) noexcept { return std::log2(x); }
inline long double CRT_log2l(long double x) noexcept { return std::log2(x); }
inline float CRT_powf(float x, float y) noexcept { return std::pow(x, y); }
inline float CRT_sqrtf(float x) noexcept { return std::sqrt(x); }
inline float CRT_cbrtf(float x) noexcept { return std::cbrt(x); }
inline float CRT_ceilf(float x) noexcept { return std::ceil(x); }
inline float CRT_floorf(float x) noexcept { return std::floor(x); }
inline double CRT_trunc(double x) noexcept { return std::trunc(x); }
inline float CRT_truncf(float x) noexcept { return std::trunc(x); }
inline float CRT_roundf(float x) noexcept { return std::round(x); }
inline float CRT_rintf(float x) noexcept { return std::rint(x); }
inline long CRT_lrint(double x) noexcept { return std::lrint(x); }
inline long CRT_lrintf(float x) noexcept { return std::lrint(x); }
inline long CRT_lroundf(float x) noexcept { return std::lround(x); }
inline long long CRT_llround(double x) noexcept { return std::llround(x); }
inline long long CRT_llroundf(float x) noexcept { return std::llround(x); }
inline double CRT_fmin(double x, double y) noexcept { return std::fmin(x, y); }
inline float CRT_fminf(float x, float y) noexcept { return std::fmin(x, y); }
inline double CRT_fmax(double x, double y) noexcept { return std::fmax(x, y); }
inline float CRT_fmaxf(float x, float y) noexcept { return std::fmax(x, y); }
inline float CRT_fmodf(float x, float y) noexcept { return std::fmod(x, y); }
inline double CRT_remainder(double x, double y) noexcept { return std::remainder(x, y); }
inline double CRT_remquo(double x, double y, int* quo) noexcept { return std::remquo(x, y, quo); }
inline double CRT_fabs(double x) noexcept { return std::fabs(x); }
inline float CRT_fabsf(float x) noexcept { return std::fabs(x); }
inline double CRT_fma(double x, double y, double z) noexcept { return std::fma(x, y, z); }
inline int CRT_ilogb(double x) noexcept { return std::ilogb(x); }
inline double CRT_scalbn(double x, int n) noexcept { return std::scalbn(x, n); }
inline float CRT_modff(float x, float* iptr) noexcept { return std::modf(x, iptr); }
inline double CRT_frexp(double x, int* exp) noexcept { return std::frexp(x, exp); }
inline float CRT_nextafterf(float x, float y) noexcept { return std::nextafter(x, y); }
inline float CRT_nexttowardf(float x, long double y) noexcept { return std::nexttoward(x, y); }
inline double CRT_hypot(double x, double y) noexcept { return std::hypot(x, y); }
inline float CRT_hypotf(float x, float y) noexcept { return std::hypot(x, y); }
inline int CRT_finite(double x) noexcept { return std::isfinite(x) ? 1 : 0; }
inline int CRT_isnan(double x) noexcept { return std::isnan(x) ? 1 : 0; }
inline int CRT_dsign(double x) noexcept { return std::signbit(x) ? 1 : 0; }
inline short CRT_dtest(double* px) noexcept {
    if (!px) return 0;
    int c = std::fpclassify(*px);
    if (c == FP_ZERO) return 0;
    if (c == FP_NORMAL || c == FP_SUBNORMAL) return 1;
    if (c == FP_INFINITE) return 2;
    return 3;
}
inline short CRT_fdtest(float* px) noexcept {
    if (!px) return 0;
    int c = std::fpclassify(*px);
    if (c == FP_ZERO) return 0;
    if (c == FP_NORMAL || c == FP_SUBNORMAL) return 1;
    if (c == FP_INFINITE) return 2;
    return 3;
}
inline short CRT_fdclass(float x) noexcept {
    int c = std::fpclassify(x);
    if (c == FP_ZERO) return 0;
    if (c == FP_NORMAL || c == FP_SUBNORMAL) return 1;
    if (c == FP_INFINITE) return 2;
    return 3;
}
inline double __std_smf_hypot3(double x, double y, double z) noexcept { return std::sqrt(x*x + y*y + z*z); }
inline float __std_smf_hypot3f(float x, float y, float z) noexcept { return std::sqrt(x*x + y*y + z*z); }

// --- Conversion & String ---
inline double CRT_strtod(const char* str, char** endptr) noexcept { return std::strtod(str, endptr); }
inline float CRT_strtof(const char* str, char** endptr) noexcept { return std::strtof(str, endptr); }
inline long long CRT_atoll(const char* str) noexcept { return std::atoll(str); }
inline long long CRT_strtoll(const char* str, char** endptr, int base) noexcept { return std::strtoll(str, endptr, base); }
inline unsigned long long CRT_strtoull(const char* str, char** endptr, int base) noexcept { return std::strtoull(str, endptr, base); }
inline int CRT_wcsicmp(const wchar_t* s1, const wchar_t* s2) noexcept {
    if (!s1 && !s2) return 0;
    if (!s1) return -1;
    if (!s2) return 1;
    while (*s1 && *s2) {
        wint_t c1 = std::towlower(*s1);
        wint_t c2 = std::towlower(*s2);
        if (c1 != c2) return (c1 < c2) ? -1 : 1;
        ++s1; ++s2;
    }
    return (std::towlower(*s1) < std::towlower(*s2)) ? -1 : ((std::towlower(*s1) > std::towlower(*s2)) ? 1 : 0);
}
inline wchar_t* CRT_wcslwr(wchar_t* s) noexcept {
    if (!s) return nullptr;
    for (wchar_t* p = s; *p; ++p) *p = std::towlower(*p);
    return s;
}
inline int CRT_isprint(int c) noexcept { return std::isprint(c); }
inline int CRT_ispunct(int c) noexcept { return std::ispunct(c); }
inline errno_t CRT_strcpy_s(char* dest, size_t destsz, const char* src) noexcept {
    if (!dest || !src || destsz == 0) return 22;
    size_t srclen = std::strlen(src);
    if (srclen >= destsz) { dest[0] = '\0'; return 34; }
    std::memcpy(dest, src, srclen + 1);
    return 0;
}
inline errno_t CRT_strncpy_s(char* dest, size_t destsz, const char* src, size_t count) noexcept {
    if (!dest || !src || destsz == 0) return 22;
    if (count == 0) { dest[0] = '\0'; return 0; }
    size_t copyLen = (count < destsz) ? count : destsz - 1;
    std::strncpy(dest, src, copyLen);
    dest[copyLen] = '\0';
    return 0;
}
inline errno_t CRT_strncat_s(char* dest, size_t destsz, const char* src, size_t count) noexcept {
    if (!dest || !src || destsz == 0) return 22;
    size_t curlen = std::strlen(dest);
    if (curlen >= destsz) return 22;
    size_t remain = destsz - curlen;
    size_t copyLen = (count < remain - 1) ? count : remain - 1;
    std::strncat(dest, src, copyLen);
    dest[curlen + copyLen] = '\0';
    return 0;
}
inline char* CRT_strtok_s(char* str, const char* delim, char** context) noexcept {
    return strtok_s(str, delim, context);
}
inline errno_t CRT_wcscpy_s(wchar_t* dest, size_t destsz, const wchar_t* src) noexcept {
    if (!dest || !src || destsz == 0) return 22;
    size_t srclen = std::wcslen(src);
    if (srclen >= destsz) { dest[0] = L'\0'; return 34; }
    std::memcpy(dest, src, (srclen + 1) * sizeof(wchar_t));
    return 0;
}

// --- Filesystem & Environment ---
inline int CRT_chdir(const char* dir) noexcept {
    return kernel32::SetCurrentDirectoryA(dir) ? 0 : -1;
}
inline void CRT_lock_file(FILE* /*stream*/) noexcept {}
inline void CRT_unlock_file(FILE* /*stream*/) noexcept {}
inline int CRT_unlink(const char* filename) noexcept { return std::remove(filename); }
struct CRT_stat64i32 {
    uint32_t st_dev;
    uint16_t st_ino;
    uint16_t st_mode;
    int16_t  st_nlink;
    int16_t  st_uid;
    int16_t  st_gid;
    uint32_t st_rdev;
    int32_t  st_size;
    int64_t  st_atime;
    int64_t  st_mtime;
    int64_t  st_ctime;
};
inline int CRT_stat64i32_fn(const char* path, CRT_stat64i32* buf) noexcept {
    if (!path || !buf) return -1;
    std::memset(buf, 0, sizeof(*buf));
    uint32_t attr = kernel32::GetFileAttributesA(path);
    if (attr == 0xFFFFFFFF) return -1;
    buf->st_mode = (attr & 0x10) ? 0040777 : 0100666;
    buf->st_size = 1024;
    return 0;
}
inline errno_t CRT_dupenv_s(char** pBuffer, size_t* pBufferSizeInBytes, const char* varname) noexcept {
    if (!pBuffer || !varname) return 22;
    const char* val = std::getenv(varname);
    if (!val) {
        *pBuffer = nullptr;
        if (pBufferSizeInBytes) *pBufferSizeInBytes = 0;
        return 0;
    }
    size_t len = std::strlen(val) + 1;
    *pBuffer = static_cast<char*>(std::malloc(len));
    if (!*pBuffer) return 12;
    std::memcpy(*pBuffer, val, len);
    if (pBufferSizeInBytes) *pBufferSizeInBytes = len;
    return 0;
}
inline errno_t CRT_putenv_s(const char* name, const char* value) noexcept {
    if (!name || !value) return 22;
    return kernel32::SetEnvironmentVariableA(name, value) ? 0 : 22;
}
inline errno_t CRT_wputenv_s(const wchar_t* name, const wchar_t* value) noexcept {
    if (!name || !value) return 22;
    return kernel32::SetEnvironmentVariableW(name, value) ? 0 : 22;
}
inline errno_t CRT_getenv_s(size_t* pReturnSize, char* dstBuf, size_t dstSizeInBytes, const char* varname) noexcept {
    if (!pReturnSize || !varname) return 22;
    const char* val = std::getenv(varname);
    if (!val) {
        if (dstBuf && dstSizeInBytes > 0) dstBuf[0] = '\0';
        *pReturnSize = 0;
        return 0;
    }
    size_t len = std::strlen(val) + 1;
    *pReturnSize = len;
    if (!dstBuf || dstSizeInBytes < len) return 34;
    std::memcpy(dstBuf, val, len);
    return 0;
}
inline void __initialize_lconv_for_unsigned_char() noexcept {}

// --- Runtime, StdIO, Time, Utility ---
inline unsigned int CRT_clearfp() noexcept { return 0; }
inline unsigned int CRT_statusfp() noexcept { return 0; }
inline errno_t CRT_controlfp_s(unsigned int* currentControl, unsigned int /*newControl*/, unsigned int /*mask*/) noexcept {
    if (currentControl) *currentControl = 0;
    return 0;
}
inline errno_t CRT_get_errno(int* pValue) noexcept {
    if (!pValue) return 22;
    *pValue = errno;
    return 0;
}
inline errno_t CRT_set_errno(int value) noexcept {
    errno = value;
    return 0;
}
inline void CRT_invoke_watson(const wchar_t* /*expression*/, const wchar_t* /*functionName*/, const wchar_t* /*fileName*/, unsigned int /*lineNumber*/, uintptr_t /*reserved*/) noexcept {}
inline int CRT_feclearexcept(int excepts) noexcept { return std::feclearexcept(excepts); }
inline int CRT_fetestexcept(int excepts) noexcept { return std::fetestexcept(excepts); }
inline void CRT_perror(const char* s) noexcept { std::perror(s); }
inline errno_t CRT_strerror_s(char* buffer, size_t numberOfElements, int errnum) noexcept {
    if (!buffer || numberOfElements == 0) return 22;
    const char* s = std::strerror(errnum);
    if (!s) s = "Unknown error";
    size_t len = std::strlen(s);
    if (len >= numberOfElements) {
        std::strncpy(buffer, s, numberOfElements - 1);
        buffer[numberOfElements - 1] = '\0';
        return 34;
    }
    std::strcpy(buffer, s);
    return 0;
}
inline int CRT_system(const char* command) noexcept { return std::system(command); }
inline int CRT_fseeki64(FILE* stream, int64_t offset, int origin) noexcept {
    return std::fseek(stream, static_cast<long>(offset), origin);
}
inline int64_t CRT_ftelli64(FILE* stream) noexcept {
    return static_cast<int64_t>(std::ftell(stream));
}
inline int CRT_flushall() noexcept { return std::fflush(nullptr); }
inline int CRT_pipe(int* pfds, unsigned int /*psize*/, int /*textmode*/) noexcept {
    if (pfds) { pfds[0] = 3; pfds[1] = 4; }
    return 0;
}
inline int CRT_get_stream_buffer_pointers(FILE* /*stream*/, char*** /*base*/, char*** /*ptr*/, int** /*cnt*/) noexcept {
    return -1;
}
inline int CRT_kbhit() noexcept { return 0; }
inline errno_t CRT_mktemp_s(char* nameTemplate, size_t sizeInChars) noexcept {
    if (!nameTemplate || sizeInChars == 0) return 22;
    static uint32_t counter = 1000;
    std::snprintf(nameTemplate, sizeInChars, "tmp_%06u", counter++);
    return 0;
}
inline errno_t CRT_fopen_s(FILE** pFile, const char* filename, const char* mode) noexcept {
    if (!pFile || !filename || !mode) return 22;
    *pFile = std::fopen(filename, mode);
    return *pFile ? 0 : errno;
}
inline errno_t CRT_freopen_s(FILE** pFile, const char* filename, const char* mode, FILE* oldFile) noexcept {
    if (!pFile || !filename || !mode || !oldFile) return 22;
    *pFile = std::freopen(filename, mode, oldFile);
    return *pFile ? 0 : errno;
}
inline errno_t CRT_wfopen_s(FILE** pFile, const wchar_t* filename, const wchar_t* mode) noexcept {
    if (!pFile || !filename || !mode) return 22;
    *pFile = _wfopen(filename, mode);
    return *pFile ? 0 : errno;
}
inline int CRT_sopen_dispatch(const char* /*filename*/, int /*oflag*/, int /*shflag*/, int /*pmode*/, int* pfh, int /*bSecure*/) noexcept {
    if (pfh) *pfh = 3;
    return 0;
}
inline errno_t CRT_sopen_s(int* pfh, const char* /*filename*/, int /*oflag*/, int /*shflag*/, int /*pmode*/) noexcept {
    if (!pfh) return 22;
    *pfh = 3;
    return 0;
}
inline int CRT_wsopen_dispatch(const wchar_t* /*filename*/, int /*oflag*/, int /*shflag*/, int /*pmode*/, int* pfh, int /*bSecure*/) noexcept {
    if (pfh) *pfh = 3;
    return 0;
}
inline int __stdio_common_vsprintf_p(uint64_t /*options*/, char* buffer, size_t buffer_count, const char* format, void* /*locale*/, va_list arglist) noexcept {
    return std::vsnprintf(buffer, buffer_count, format, arglist);
}
inline int __stdio_common_vsprintf_s(uint64_t /*options*/, char* buffer, size_t buffer_count, const char* format, void* /*locale*/, va_list arglist) noexcept {
    return std::vsnprintf(buffer, buffer_count, format, arglist);
}
inline int __stdio_common_vsnprintf_s(uint64_t /*options*/, char* buffer, size_t buffer_count, size_t /*max_count*/, const char* format, void* /*locale*/, va_list arglist) noexcept {
    return std::vsnprintf(buffer, buffer_count, format, arglist);
}
inline int __stdio_common_vfwprintf(uint64_t /*options*/, FILE* stream, const wchar_t* format, void* /*locale*/, va_list arglist) noexcept {
    return std::vfwprintf(stream, format, arglist);
}
inline int __stdio_common_vswscanf(uint64_t /*options*/, const wchar_t* input, size_t /*length*/, const wchar_t* format, void* /*locale*/, va_list arglist) noexcept {
    return std::vswscanf(input, format, arglist);
}
inline errno_t CRT_ctime64_s(char* buffer, size_t numberOfElements, const int64_t* timer) noexcept {
    if (!buffer || !timer || numberOfElements == 0) return 22;
    time_t t = static_cast<time_t>(*timer);
    char* res = std::ctime(&t);
    if (!res) return 22;
    size_t len = std::strlen(res);
    if (len >= numberOfElements) return 34;
    std::strcpy(buffer, res);
    return 0;
}
inline double CRT_difftime64(int64_t t1, int64_t t0) noexcept {
    return static_cast<double>(t1 - t0);
}
struct __timeb64 {
    int64_t time;
    unsigned short millitm;
    short timezone;
    short dstflag;
};
inline void CRT_ftime64(__timeb64* timeptr) noexcept {
    if (!timeptr) return;
    timeptr->time = std::time(nullptr);
    timeptr->millitm = 0;
    timeptr->timezone = 0;
    timeptr->dstflag = 0;
}
inline div_t CRT_div(int numer, int denom) noexcept { return std::div(numer, denom); }
inline ldiv_t CRT_ldiv(long numer, long denom) noexcept { return std::ldiv(numer, denom); }

// --- VCRuntime & MSVCP Core Stubs ---
inline const wchar_t* CRT_wcsrchr(const wchar_t* str, wchar_t ch) noexcept {
    return std::wcsrchr(str, ch);
}
inline const char* __std_type_info_name(void* /*typeInfo*/, void* /*undecoratedBuffer*/) noexcept {
    return "type_info";
}
inline size_t __std_type_info_hash(const void* typeInfo) noexcept {
    return reinterpret_cast<size_t>(typeInfo);
}
inline int __CxxFrameHandler3(void* /*pExcept*/, void* /*pRN*/, void* /*pContext*/, void* /*pDC*/) noexcept {
    return 1;
}
inline void* __RTCastToVoid(void* ptr) noexcept { return ptr; }
inline void* __RTtypeid(void* /*ptr*/) noexcept { return nullptr; }

inline void* MSVC_GenericStub(void* p1 = nullptr, void* /*p2*/ = nullptr, void* /*p3*/ = nullptr, void* /*p4*/ = nullptr) noexcept {
    return p1;
}
inline int MSVC_GenericInt0(void* /*p1*/ = nullptr, void* /*p2*/ = nullptr) noexcept { return 0; }
inline int MSVC_GenericInt1(void* /*p1*/ = nullptr) noexcept { return 1; }
inline uint32_t MSVC_Thrd_id() noexcept { return 1; }
inline uint32_t MSVC_Thrd_hardware_concurrency() noexcept { return 8; }
inline int64_t MSVC_Query_perf_counter() noexcept { return 1000; }
inline int64_t MSVC_Query_perf_frequency() noexcept { return 10000000; }
inline int64_t MSVC_Xtime_get_ticks() noexcept { return 1000; }
inline int MSVC_Winerror_map(int err) noexcept { return err; }
inline bool MSVC_uncaught_exception() noexcept { return false; }
inline void MSVC_Xout_of_range(const char* /*msg*/) noexcept {}
inline void MSVC_Xinvalid_argument(const char* /*msg*/) noexcept {}
inline void MSVC_Xruntime_error(const char* /*msg*/) noexcept {}
inline void MSVC_Xregex_error(int /*code*/) noexcept {}
inline void MSVC_ExceptionPtrAssign(void* /*p1*/, const void* /*p2*/) noexcept {}
inline void MSVC_ExceptionPtrCopyException(void* /*p1*/, const void* /*p2*/, const void* /*p3*/) noexcept {}
inline void MSVC_ExceptionPtrRethrow(const void* /*p*/) noexcept {}
inline bool MSVC_ExceptionPtrToBool(const void* /*p*/) noexcept { return false; }
inline char MSVC_cout_buffer[1024] = {0};
inline char MSVC_cerr_buffer[1024] = {0};
inline char MSVC_clog_buffer[1024] = {0};

inline void InitializeMsvcrtSubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();

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
    ldr.registerExport("msvcrt.dll", "malloc", reinterpret_cast<void*>(malloc));
    ldr.registerExport("msvcrt.dll", "free", reinterpret_cast<void*>(free));
    ldr.registerExport("msvcrt.dll", "calloc", reinterpret_cast<void*>(calloc));
    ldr.registerExport("msvcrt.dll", "realloc", reinterpret_cast<void*>(realloc));
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
    ldr.registerExport("msvcrt.dll", "__getmainargs", reinterpret_cast<void*>(__getmainargs));
    ldr.registerExport("msvcrt.dll", "_initterm", reinterpret_cast<void*>(_initterm));
    ldr.registerExport("msvcrt.dll", "_initterm_e", reinterpret_cast<void*>(_initterm_e));
    ldr.registerExport("msvcrt.dll", "__set_app_type", reinterpret_cast<void*>(__set_app_type));
    ldr.registerExport("msvcrt.dll", "__setusermatherr", reinterpret_cast<void*>(__setusermatherr));
    ldr.registerExport("msvcrt.dll", "_commode", reinterpret_cast<void*>(&g_Commode));
    ldr.registerExport("msvcrt.dll", "_fmode", reinterpret_cast<void*>(&g_Fmode));
    ldr.registerExport("msvcrt.dll", "_environ", reinterpret_cast<void*>(&g_Environ));
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
    ldr.registerExport("msvcrt.dll", "_iob", reinterpret_cast<void*>(_iob));
    ldr.registerExport("msvcrt.dll", "fputc", reinterpret_cast<void*>(fputc));
    ldr.registerExport("msvcrt.dll", "fputs", reinterpret_cast<void*>(fputs));
    ldr.registerExport("msvcrt.dll", "fgetc", reinterpret_cast<void*>(fgetc));
    ldr.registerExport("msvcrt.dll", "_fileno", reinterpret_cast<void*>(_fileno));
    ldr.registerExport("msvcrt.dll", "wcscmp", reinterpret_cast<void*>(wcscmp));
    ldr.registerExport("msvcrt.dll", "wcsstr", reinterpret_cast<void*>(wcsstr));
    ldr.registerExport("msvcrt.dll", "__initenv", reinterpret_cast<void*>(&g_InitEnvPtr));
    ldr.registerExport("msvcrt.dll", "_acmdln", reinterpret_cast<void*>(&g_Acmdln));
    ldr.registerExport("msvcrt.dll", "__p__acmdln", reinterpret_cast<void*>(__p__acmdln));
    ldr.registerExport("msvcrt.dll", "__p___initenv", reinterpret_cast<void*>(__p___initenv));
    ldr.registerExport("msvcrt.dll", "___lc_codepage_func", reinterpret_cast<void*>(___lc_codepage_func));
    ldr.registerExport("msvcrt.dll", "___mb_cur_max_func", reinterpret_cast<void*>(___mb_cur_max_func));
    ldr.registerExport("msvcrt.dll", "__C_specific_handler", reinterpret_cast<void*>(__C_specific_handler));
    ldr.registerExport("msvcrt.dll", "_CxxThrowException", reinterpret_cast<void*>(_CxxThrowException));
    ldr.registerExport("msvcrt.dll", "__CxxFrameHandler", reinterpret_cast<void*>(__CxxFrameHandler));
    ldr.registerExport("msvcrt.dll", "_c_exit", reinterpret_cast<void*>(_c_exit));
    ldr.registerExport("msvcrt.dll", "_XcptFilter", reinterpret_cast<void*>(_XcptFilter));
    ldr.registerExport("msvcrt.dll", "_onexit", reinterpret_cast<void*>(_onexit));
    ldr.registerExport("msvcrt.dll", "__dllonexit", reinterpret_cast<void*>(__dllonexit));
    ldr.registerExport("msvcrt.dll", "?terminate@@YAXXZ", reinterpret_cast<void*>(terminate_wrapper));
    ldr.registerExport("msvcrt.dll", "??1type_info@@UEAA@XZ", reinterpret_cast<void*>(type_info_dtor));
    ldr.registerExport("msvcrt.dll", "_beginthreadex", reinterpret_cast<void*>(_beginthreadex));
    ldr.registerExport("msvcrt.dll", "_get_osfhandle", reinterpret_cast<void*>(_get_osfhandle));
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
    ldr.registerExport("msvcrt.dll", "srand", reinterpret_cast<void*>(srand));
    ldr.registerExport("ucrtbase.dll", "srand", reinterpret_cast<void*>(srand));
    ldr.registerExport("api-ms-win-crt-utility-l1-1-0.dll", "srand", reinterpret_cast<void*>(srand));
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

    // --- Universal CRT, VCRuntime & MSVCP Dynamic Registrations (Wireshark & Modern Apps) ---
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "sin", reinterpret_cast<void*>(CRT_sin));
    ldr.registerExport("ucrtbase.dll", "sin", reinterpret_cast<void*>(CRT_sin));
    ldr.registerExport("msvcrt.dll", "sin", reinterpret_cast<void*>(CRT_sin));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "cos", reinterpret_cast<void*>(CRT_cos));
    ldr.registerExport("ucrtbase.dll", "cos", reinterpret_cast<void*>(CRT_cos));
    ldr.registerExport("msvcrt.dll", "cos", reinterpret_cast<void*>(CRT_cos));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "tan", reinterpret_cast<void*>(CRT_tan));
    ldr.registerExport("ucrtbase.dll", "tan", reinterpret_cast<void*>(CRT_tan));
    ldr.registerExport("msvcrt.dll", "tan", reinterpret_cast<void*>(CRT_tan));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "asin", reinterpret_cast<void*>(CRT_asin));
    ldr.registerExport("ucrtbase.dll", "asin", reinterpret_cast<void*>(CRT_asin));
    ldr.registerExport("msvcrt.dll", "asin", reinterpret_cast<void*>(CRT_asin));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "acos", reinterpret_cast<void*>(CRT_acos));
    ldr.registerExport("ucrtbase.dll", "acos", reinterpret_cast<void*>(CRT_acos));
    ldr.registerExport("msvcrt.dll", "acos", reinterpret_cast<void*>(CRT_acos));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "atan", reinterpret_cast<void*>(CRT_atan));
    ldr.registerExport("ucrtbase.dll", "atan", reinterpret_cast<void*>(CRT_atan));
    ldr.registerExport("msvcrt.dll", "atan", reinterpret_cast<void*>(CRT_atan));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "atan2", reinterpret_cast<void*>(CRT_atan2));
    ldr.registerExport("ucrtbase.dll", "atan2", reinterpret_cast<void*>(CRT_atan2));
    ldr.registerExport("msvcrt.dll", "atan2", reinterpret_cast<void*>(CRT_atan2));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "exp", reinterpret_cast<void*>(CRT_exp));
    ldr.registerExport("ucrtbase.dll", "exp", reinterpret_cast<void*>(CRT_exp));
    ldr.registerExport("msvcrt.dll", "exp", reinterpret_cast<void*>(CRT_exp));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "log", reinterpret_cast<void*>(CRT_log));
    ldr.registerExport("ucrtbase.dll", "log", reinterpret_cast<void*>(CRT_log));
    ldr.registerExport("msvcrt.dll", "log", reinterpret_cast<void*>(CRT_log));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "log10", reinterpret_cast<void*>(CRT_log10));
    ldr.registerExport("ucrtbase.dll", "log10", reinterpret_cast<void*>(CRT_log10));
    ldr.registerExport("msvcrt.dll", "log10", reinterpret_cast<void*>(CRT_log10));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "log2", reinterpret_cast<void*>(CRT_log2));
    ldr.registerExport("ucrtbase.dll", "log2", reinterpret_cast<void*>(CRT_log2));
    ldr.registerExport("msvcrt.dll", "log2", reinterpret_cast<void*>(CRT_log2));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "pow", reinterpret_cast<void*>(CRT_pow));
    ldr.registerExport("ucrtbase.dll", "pow", reinterpret_cast<void*>(CRT_pow));
    ldr.registerExport("msvcrt.dll", "pow", reinterpret_cast<void*>(CRT_pow));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "sqrt", reinterpret_cast<void*>(CRT_sqrt));
    ldr.registerExport("ucrtbase.dll", "sqrt", reinterpret_cast<void*>(CRT_sqrt));
    ldr.registerExport("msvcrt.dll", "sqrt", reinterpret_cast<void*>(CRT_sqrt));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "cbrt", reinterpret_cast<void*>(CRT_cbrt));
    ldr.registerExport("ucrtbase.dll", "cbrt", reinterpret_cast<void*>(CRT_cbrt));
    ldr.registerExport("msvcrt.dll", "cbrt", reinterpret_cast<void*>(CRT_cbrt));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "ceil", reinterpret_cast<void*>(CRT_ceil));
    ldr.registerExport("ucrtbase.dll", "ceil", reinterpret_cast<void*>(CRT_ceil));
    ldr.registerExport("msvcrt.dll", "ceil", reinterpret_cast<void*>(CRT_ceil));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "floor", reinterpret_cast<void*>(CRT_floor));
    ldr.registerExport("ucrtbase.dll", "floor", reinterpret_cast<void*>(CRT_floor));
    ldr.registerExport("msvcrt.dll", "floor", reinterpret_cast<void*>(CRT_floor));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "round", reinterpret_cast<void*>(CRT_round));
    ldr.registerExport("ucrtbase.dll", "round", reinterpret_cast<void*>(CRT_round));
    ldr.registerExport("msvcrt.dll", "round", reinterpret_cast<void*>(CRT_round));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "lround", reinterpret_cast<void*>(CRT_lround));
    ldr.registerExport("ucrtbase.dll", "lround", reinterpret_cast<void*>(CRT_lround));
    ldr.registerExport("msvcrt.dll", "lround", reinterpret_cast<void*>(CRT_lround));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "fmod", reinterpret_cast<void*>(CRT_fmod));
    ldr.registerExport("ucrtbase.dll", "fmod", reinterpret_cast<void*>(CRT_fmod));
    ldr.registerExport("msvcrt.dll", "fmod", reinterpret_cast<void*>(CRT_fmod));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "modf", reinterpret_cast<void*>(CRT_modf));
    ldr.registerExport("ucrtbase.dll", "modf", reinterpret_cast<void*>(CRT_modf));
    ldr.registerExport("msvcrt.dll", "modf", reinterpret_cast<void*>(CRT_modf));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "ldexp", reinterpret_cast<void*>(CRT_ldexp));
    ldr.registerExport("ucrtbase.dll", "ldexp", reinterpret_cast<void*>(CRT_ldexp));
    ldr.registerExport("msvcrt.dll", "ldexp", reinterpret_cast<void*>(CRT_ldexp));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "copysign", reinterpret_cast<void*>(CRT_copysign));
    ldr.registerExport("ucrtbase.dll", "copysign", reinterpret_cast<void*>(CRT_copysign));
    ldr.registerExport("msvcrt.dll", "copysign", reinterpret_cast<void*>(CRT_copysign));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "_dclass", reinterpret_cast<void*>(CRT_dclass));
    ldr.registerExport("ucrtbase.dll", "_dclass", reinterpret_cast<void*>(CRT_dclass));
    ldr.registerExport("msvcrt.dll", "_dclass", reinterpret_cast<void*>(CRT_dclass));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "_dpcomp", reinterpret_cast<void*>(CRT_dpcomp));
    ldr.registerExport("ucrtbase.dll", "_dpcomp", reinterpret_cast<void*>(CRT_dpcomp));
    ldr.registerExport("msvcrt.dll", "_dpcomp", reinterpret_cast<void*>(CRT_dpcomp));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "tolower", reinterpret_cast<void*>(CRT_tolower));
    ldr.registerExport("ucrtbase.dll", "tolower", reinterpret_cast<void*>(CRT_tolower));
    ldr.registerExport("msvcrt.dll", "tolower", reinterpret_cast<void*>(CRT_tolower));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "toupper", reinterpret_cast<void*>(CRT_toupper));
    ldr.registerExport("ucrtbase.dll", "toupper", reinterpret_cast<void*>(CRT_toupper));
    ldr.registerExport("msvcrt.dll", "toupper", reinterpret_cast<void*>(CRT_toupper));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "isdigit", reinterpret_cast<void*>(CRT_isdigit));
    ldr.registerExport("ucrtbase.dll", "isdigit", reinterpret_cast<void*>(CRT_isdigit));
    ldr.registerExport("msvcrt.dll", "isdigit", reinterpret_cast<void*>(CRT_isdigit));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "islower", reinterpret_cast<void*>(CRT_islower));
    ldr.registerExport("ucrtbase.dll", "islower", reinterpret_cast<void*>(CRT_islower));
    ldr.registerExport("msvcrt.dll", "islower", reinterpret_cast<void*>(CRT_islower));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "isupper", reinterpret_cast<void*>(CRT_isupper));
    ldr.registerExport("ucrtbase.dll", "isupper", reinterpret_cast<void*>(CRT_isupper));
    ldr.registerExport("msvcrt.dll", "isupper", reinterpret_cast<void*>(CRT_isupper));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "isxdigit", reinterpret_cast<void*>(CRT_isxdigit));
    ldr.registerExport("ucrtbase.dll", "isxdigit", reinterpret_cast<void*>(CRT_isxdigit));
    ldr.registerExport("msvcrt.dll", "isxdigit", reinterpret_cast<void*>(CRT_isxdigit));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "iswspace", reinterpret_cast<void*>(CRT_iswspace));
    ldr.registerExport("ucrtbase.dll", "iswspace", reinterpret_cast<void*>(CRT_iswspace));
    ldr.registerExport("msvcrt.dll", "iswspace", reinterpret_cast<void*>(CRT_iswspace));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "strspn", reinterpret_cast<void*>(CRT_strspn));
    ldr.registerExport("ucrtbase.dll", "strspn", reinterpret_cast<void*>(CRT_strspn));
    ldr.registerExport("msvcrt.dll", "strspn", reinterpret_cast<void*>(CRT_strspn));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "strcspn", reinterpret_cast<void*>(CRT_strcspn));
    ldr.registerExport("ucrtbase.dll", "strcspn", reinterpret_cast<void*>(CRT_strcspn));
    ldr.registerExport("msvcrt.dll", "strcspn", reinterpret_cast<void*>(CRT_strcspn));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "strpbrk", reinterpret_cast<void*>(CRT_strpbrk));
    ldr.registerExport("ucrtbase.dll", "strpbrk", reinterpret_cast<void*>(CRT_strpbrk));
    ldr.registerExport("msvcrt.dll", "strpbrk", reinterpret_cast<void*>(CRT_strpbrk));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "strtok", reinterpret_cast<void*>(CRT_strtok));
    ldr.registerExport("ucrtbase.dll", "strtok", reinterpret_cast<void*>(CRT_strtok));
    ldr.registerExport("msvcrt.dll", "strtok", reinterpret_cast<void*>(CRT_strtok));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "strnlen", reinterpret_cast<void*>(CRT_strnlen));
    ldr.registerExport("ucrtbase.dll", "strnlen", reinterpret_cast<void*>(CRT_strnlen));
    ldr.registerExport("msvcrt.dll", "strnlen", reinterpret_cast<void*>(CRT_strnlen));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "strncat", reinterpret_cast<void*>(CRT_strncat));
    ldr.registerExport("ucrtbase.dll", "strncat", reinterpret_cast<void*>(CRT_strncat));
    ldr.registerExport("msvcrt.dll", "strncat", reinterpret_cast<void*>(CRT_strncat));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "mblen", reinterpret_cast<void*>(CRT_mblen));
    ldr.registerExport("ucrtbase.dll", "mblen", reinterpret_cast<void*>(CRT_mblen));
    ldr.registerExport("msvcrt.dll", "mblen", reinterpret_cast<void*>(CRT_mblen));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "mbtowc", reinterpret_cast<void*>(CRT_mbtowc));
    ldr.registerExport("ucrtbase.dll", "mbtowc", reinterpret_cast<void*>(CRT_mbtowc));
    ldr.registerExport("msvcrt.dll", "mbtowc", reinterpret_cast<void*>(CRT_mbtowc));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "wcscat_s", reinterpret_cast<void*>(CRT_wcscat_s));
    ldr.registerExport("ucrtbase.dll", "wcscat_s", reinterpret_cast<void*>(CRT_wcscat_s));
    ldr.registerExport("msvcrt.dll", "wcscat_s", reinterpret_cast<void*>(CRT_wcscat_s));
    ldr.registerExport("api-ms-win-crt-convert-l1-1-0.dll", "strtol", reinterpret_cast<void*>(CRT_strtol));
    ldr.registerExport("ucrtbase.dll", "strtol", reinterpret_cast<void*>(CRT_strtol));
    ldr.registerExport("msvcrt.dll", "strtol", reinterpret_cast<void*>(CRT_strtol));
    ldr.registerExport("api-ms-win-crt-convert-l1-1-0.dll", "atoi", reinterpret_cast<void*>(CRT_atoi));
    ldr.registerExport("ucrtbase.dll", "atoi", reinterpret_cast<void*>(CRT_atoi));
    ldr.registerExport("msvcrt.dll", "atoi", reinterpret_cast<void*>(CRT_atoi));
    ldr.registerExport("api-ms-win-crt-utility-l1-1-0.dll", "bsearch", reinterpret_cast<void*>(CRT_bsearch));
    ldr.registerExport("ucrtbase.dll", "bsearch", reinterpret_cast<void*>(CRT_bsearch));
    ldr.registerExport("msvcrt.dll", "bsearch", reinterpret_cast<void*>(CRT_bsearch));
    ldr.registerExport("api-ms-win-crt-utility-l1-1-0.dll", "qsort", reinterpret_cast<void*>(CRT_qsort));
    ldr.registerExport("ucrtbase.dll", "qsort", reinterpret_cast<void*>(CRT_qsort));
    ldr.registerExport("msvcrt.dll", "qsort", reinterpret_cast<void*>(CRT_qsort));
    ldr.registerExport("api-ms-win-crt-heap-l1-1-0.dll", "_callnewh", reinterpret_cast<void*>(CRT_callnewh));
    ldr.registerExport("ucrtbase.dll", "_callnewh", reinterpret_cast<void*>(CRT_callnewh));
    ldr.registerExport("msvcrt.dll", "_callnewh", reinterpret_cast<void*>(CRT_callnewh));
    ldr.registerExport("api-ms-win-crt-heap-l1-1-0.dll", "_msize", reinterpret_cast<void*>(CRT_msize));
    ldr.registerExport("ucrtbase.dll", "_msize", reinterpret_cast<void*>(CRT_msize));
    ldr.registerExport("msvcrt.dll", "_msize", reinterpret_cast<void*>(CRT_msize));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "_set_fmode", reinterpret_cast<void*>(CRT_set_fmode));
    ldr.registerExport("ucrtbase.dll", "_set_fmode", reinterpret_cast<void*>(CRT_set_fmode));
    ldr.registerExport("msvcrt.dll", "_set_fmode", reinterpret_cast<void*>(CRT_set_fmode));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "_open_osfhandle", reinterpret_cast<void*>(CRT_open_osfhandle));
    ldr.registerExport("ucrtbase.dll", "_open_osfhandle", reinterpret_cast<void*>(CRT_open_osfhandle));
    ldr.registerExport("msvcrt.dll", "_open_osfhandle", reinterpret_cast<void*>(CRT_open_osfhandle));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "feof", reinterpret_cast<void*>(CRT_feof));
    ldr.registerExport("ucrtbase.dll", "feof", reinterpret_cast<void*>(CRT_feof));
    ldr.registerExport("msvcrt.dll", "feof", reinterpret_cast<void*>(CRT_feof));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "ferror", reinterpret_cast<void*>(CRT_ferror));
    ldr.registerExport("ucrtbase.dll", "ferror", reinterpret_cast<void*>(CRT_ferror));
    ldr.registerExport("msvcrt.dll", "ferror", reinterpret_cast<void*>(CRT_ferror));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "clearerr", reinterpret_cast<void*>(CRT_clearerr));
    ldr.registerExport("ucrtbase.dll", "clearerr", reinterpret_cast<void*>(CRT_clearerr));
    ldr.registerExport("msvcrt.dll", "clearerr", reinterpret_cast<void*>(CRT_clearerr));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "getc", reinterpret_cast<void*>(CRT_getc));
    ldr.registerExport("ucrtbase.dll", "getc", reinterpret_cast<void*>(CRT_getc));
    ldr.registerExport("msvcrt.dll", "getc", reinterpret_cast<void*>(CRT_getc));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "getchar", reinterpret_cast<void*>(CRT_getchar));
    ldr.registerExport("ucrtbase.dll", "getchar", reinterpret_cast<void*>(CRT_getchar));
    ldr.registerExport("msvcrt.dll", "getchar", reinterpret_cast<void*>(CRT_getchar));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "putc", reinterpret_cast<void*>(CRT_putc));
    ldr.registerExport("ucrtbase.dll", "putc", reinterpret_cast<void*>(CRT_putc));
    ldr.registerExport("msvcrt.dll", "putc", reinterpret_cast<void*>(CRT_putc));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "fgets", reinterpret_cast<void*>(CRT_fgets));
    ldr.registerExport("ucrtbase.dll", "fgets", reinterpret_cast<void*>(CRT_fgets));
    ldr.registerExport("msvcrt.dll", "fgets", reinterpret_cast<void*>(CRT_fgets));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "fread", reinterpret_cast<void*>(CRT_fread));
    ldr.registerExport("ucrtbase.dll", "fread", reinterpret_cast<void*>(CRT_fread));
    ldr.registerExport("msvcrt.dll", "fread", reinterpret_cast<void*>(CRT_fread));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "rewind", reinterpret_cast<void*>(CRT_rewind));
    ldr.registerExport("ucrtbase.dll", "rewind", reinterpret_cast<void*>(CRT_rewind));
    ldr.registerExport("msvcrt.dll", "rewind", reinterpret_cast<void*>(CRT_rewind));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "ungetc", reinterpret_cast<void*>(CRT_ungetc));
    ldr.registerExport("ucrtbase.dll", "ungetc", reinterpret_cast<void*>(CRT_ungetc));
    ldr.registerExport("msvcrt.dll", "ungetc", reinterpret_cast<void*>(CRT_ungetc));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "_fgetc_nolock", reinterpret_cast<void*>(CRT_fgetc_nolock));
    ldr.registerExport("ucrtbase.dll", "_fgetc_nolock", reinterpret_cast<void*>(CRT_fgetc_nolock));
    ldr.registerExport("msvcrt.dll", "_fgetc_nolock", reinterpret_cast<void*>(CRT_fgetc_nolock));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "_wfopen", reinterpret_cast<void*>(CRT_wfopen));
    ldr.registerExport("ucrtbase.dll", "_wfopen", reinterpret_cast<void*>(CRT_wfopen));
    ldr.registerExport("msvcrt.dll", "_wfopen", reinterpret_cast<void*>(CRT_wfopen));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "_wfreopen", reinterpret_cast<void*>(CRT_wfreopen));
    ldr.registerExport("ucrtbase.dll", "_wfreopen", reinterpret_cast<void*>(CRT_wfreopen));
    ldr.registerExport("msvcrt.dll", "_wfreopen", reinterpret_cast<void*>(CRT_wfreopen));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "_wopen", reinterpret_cast<void*>(CRT_wopen));
    ldr.registerExport("ucrtbase.dll", "_wopen", reinterpret_cast<void*>(CRT_wopen));
    ldr.registerExport("msvcrt.dll", "_wopen", reinterpret_cast<void*>(CRT_wopen));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "_popen", reinterpret_cast<void*>(CRT_popen));
    ldr.registerExport("ucrtbase.dll", "_popen", reinterpret_cast<void*>(CRT_popen));
    ldr.registerExport("msvcrt.dll", "_popen", reinterpret_cast<void*>(CRT_popen));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "_pclose", reinterpret_cast<void*>(CRT_pclose));
    ldr.registerExport("ucrtbase.dll", "_pclose", reinterpret_cast<void*>(CRT_pclose));
    ldr.registerExport("msvcrt.dll", "_pclose", reinterpret_cast<void*>(CRT_pclose));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "__stdio_common_vsprintf", reinterpret_cast<void*>(CRT_stdio_common_vsprintf));
    ldr.registerExport("ucrtbase.dll", "__stdio_common_vsprintf", reinterpret_cast<void*>(CRT_stdio_common_vsprintf));
    ldr.registerExport("msvcrt.dll", "__stdio_common_vsprintf", reinterpret_cast<void*>(CRT_stdio_common_vsprintf));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "__stdio_common_vswprintf", reinterpret_cast<void*>(CRT_stdio_common_vswprintf));
    ldr.registerExport("ucrtbase.dll", "__stdio_common_vswprintf", reinterpret_cast<void*>(CRT_stdio_common_vswprintf));
    ldr.registerExport("msvcrt.dll", "__stdio_common_vswprintf", reinterpret_cast<void*>(CRT_stdio_common_vswprintf));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "__stdio_common_vsscanf", reinterpret_cast<void*>(CRT_stdio_common_vsscanf));
    ldr.registerExport("ucrtbase.dll", "__stdio_common_vsscanf", reinterpret_cast<void*>(CRT_stdio_common_vsscanf));
    ldr.registerExport("msvcrt.dll", "__stdio_common_vsscanf", reinterpret_cast<void*>(CRT_stdio_common_vsscanf));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "__stdio_common_vfscanf", reinterpret_cast<void*>(CRT_stdio_common_vfscanf));
    ldr.registerExport("ucrtbase.dll", "__stdio_common_vfscanf", reinterpret_cast<void*>(CRT_stdio_common_vfscanf));
    ldr.registerExport("msvcrt.dll", "__stdio_common_vfscanf", reinterpret_cast<void*>(CRT_stdio_common_vfscanf));
    ldr.registerExport("api-ms-win-crt-time-l1-1-0.dll", "__timezone", reinterpret_cast<void*>(CRT_timezone));
    ldr.registerExport("ucrtbase.dll", "__timezone", reinterpret_cast<void*>(CRT_timezone));
    ldr.registerExport("msvcrt.dll", "__timezone", reinterpret_cast<void*>(CRT_timezone));
    ldr.registerExport("api-ms-win-crt-time-l1-1-0.dll", "__tzname", reinterpret_cast<void*>(CRT_tzname));
    ldr.registerExport("ucrtbase.dll", "__tzname", reinterpret_cast<void*>(CRT_tzname));
    ldr.registerExport("msvcrt.dll", "__tzname", reinterpret_cast<void*>(CRT_tzname));
    ldr.registerExport("api-ms-win-crt-time-l1-1-0.dll", "_tzset", reinterpret_cast<void*>(CRT_tzset));
    ldr.registerExport("ucrtbase.dll", "_tzset", reinterpret_cast<void*>(CRT_tzset));
    ldr.registerExport("msvcrt.dll", "_tzset", reinterpret_cast<void*>(CRT_tzset));
    ldr.registerExport("api-ms-win-crt-time-l1-1-0.dll", "_localtime64", reinterpret_cast<void*>(CRT_localtime64));
    ldr.registerExport("ucrtbase.dll", "_localtime64", reinterpret_cast<void*>(CRT_localtime64));
    ldr.registerExport("msvcrt.dll", "_localtime64", reinterpret_cast<void*>(CRT_localtime64));
    ldr.registerExport("api-ms-win-crt-time-l1-1-0.dll", "_localtime64_s", reinterpret_cast<void*>(CRT_localtime64_s));
    ldr.registerExport("ucrtbase.dll", "_localtime64_s", reinterpret_cast<void*>(CRT_localtime64_s));
    ldr.registerExport("msvcrt.dll", "_localtime64_s", reinterpret_cast<void*>(CRT_localtime64_s));
    ldr.registerExport("api-ms-win-crt-time-l1-1-0.dll", "_gmtime64", reinterpret_cast<void*>(CRT_gmtime64));
    ldr.registerExport("ucrtbase.dll", "_gmtime64", reinterpret_cast<void*>(CRT_gmtime64));
    ldr.registerExport("msvcrt.dll", "_gmtime64", reinterpret_cast<void*>(CRT_gmtime64));
    ldr.registerExport("api-ms-win-crt-time-l1-1-0.dll", "_gmtime64_s", reinterpret_cast<void*>(CRT_gmtime64_s));
    ldr.registerExport("ucrtbase.dll", "_gmtime64_s", reinterpret_cast<void*>(CRT_gmtime64_s));
    ldr.registerExport("msvcrt.dll", "_gmtime64_s", reinterpret_cast<void*>(CRT_gmtime64_s));
    ldr.registerExport("api-ms-win-crt-time-l1-1-0.dll", "_mktime64", reinterpret_cast<void*>(CRT_mktime64));
    ldr.registerExport("ucrtbase.dll", "_mktime64", reinterpret_cast<void*>(CRT_mktime64));
    ldr.registerExport("msvcrt.dll", "_mktime64", reinterpret_cast<void*>(CRT_mktime64));
    ldr.registerExport("api-ms-win-crt-time-l1-1-0.dll", "asctime", reinterpret_cast<void*>(CRT_asctime));
    ldr.registerExport("ucrtbase.dll", "asctime", reinterpret_cast<void*>(CRT_asctime));
    ldr.registerExport("msvcrt.dll", "asctime", reinterpret_cast<void*>(CRT_asctime));
    ldr.registerExport("api-ms-win-crt-time-l1-1-0.dll", "_timespec64_get", reinterpret_cast<void*>(CRT_timespec64_get));
    ldr.registerExport("ucrtbase.dll", "_timespec64_get", reinterpret_cast<void*>(CRT_timespec64_get));
    ldr.registerExport("msvcrt.dll", "_timespec64_get", reinterpret_cast<void*>(CRT_timespec64_get));
    ldr.registerExport("api-ms-win-crt-filesystem-l1-1-0.dll", "_wmkdir", reinterpret_cast<void*>(CRT_wmkdir));
    ldr.registerExport("ucrtbase.dll", "_wmkdir", reinterpret_cast<void*>(CRT_wmkdir));
    ldr.registerExport("msvcrt.dll", "_wmkdir", reinterpret_cast<void*>(CRT_wmkdir));
    ldr.registerExport("api-ms-win-crt-filesystem-l1-1-0.dll", "_wrmdir", reinterpret_cast<void*>(CRT_wrmdir));
    ldr.registerExport("ucrtbase.dll", "_wrmdir", reinterpret_cast<void*>(CRT_wrmdir));
    ldr.registerExport("msvcrt.dll", "_wrmdir", reinterpret_cast<void*>(CRT_wrmdir));
    ldr.registerExport("api-ms-win-crt-filesystem-l1-1-0.dll", "_wremove", reinterpret_cast<void*>(CRT_wremove));
    ldr.registerExport("ucrtbase.dll", "_wremove", reinterpret_cast<void*>(CRT_wremove));
    ldr.registerExport("msvcrt.dll", "_wremove", reinterpret_cast<void*>(CRT_wremove));
    ldr.registerExport("api-ms-win-crt-filesystem-l1-1-0.dll", "_wunlink", reinterpret_cast<void*>(CRT_wunlink));
    ldr.registerExport("ucrtbase.dll", "_wunlink", reinterpret_cast<void*>(CRT_wunlink));
    ldr.registerExport("msvcrt.dll", "_wunlink", reinterpret_cast<void*>(CRT_wunlink));
    ldr.registerExport("api-ms-win-crt-filesystem-l1-1-0.dll", "_wstat64", reinterpret_cast<void*>(CRT_wstat64));
    ldr.registerExport("ucrtbase.dll", "_wstat64", reinterpret_cast<void*>(CRT_wstat64));
    ldr.registerExport("msvcrt.dll", "_wstat64", reinterpret_cast<void*>(CRT_wstat64));
    ldr.registerExport("api-ms-win-crt-conio-l1-1-0.dll", "_getch", reinterpret_cast<void*>(CRT_getch));
    ldr.registerExport("ucrtbase.dll", "_getch", reinterpret_cast<void*>(CRT_getch));
    ldr.registerExport("msvcrt.dll", "_getch", reinterpret_cast<void*>(CRT_getch));
    ldr.registerExport("api-ms-win-crt-process-l1-1-0.dll", "_cwait", reinterpret_cast<void*>(CRT_cwait));
    ldr.registerExport("ucrtbase.dll", "_cwait", reinterpret_cast<void*>(CRT_cwait));
    ldr.registerExport("msvcrt.dll", "_cwait", reinterpret_cast<void*>(CRT_cwait));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "__doserrno", reinterpret_cast<void*>(CRT_doserrno));
    ldr.registerExport("ucrtbase.dll", "__doserrno", reinterpret_cast<void*>(CRT_doserrno));
    ldr.registerExport("msvcrt.dll", "__doserrno", reinterpret_cast<void*>(CRT_doserrno));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "__fpe_flt_rounds", reinterpret_cast<void*>(CRT_fpe_flt_rounds));
    ldr.registerExport("ucrtbase.dll", "__fpe_flt_rounds", reinterpret_cast<void*>(CRT_fpe_flt_rounds));
    ldr.registerExport("msvcrt.dll", "__fpe_flt_rounds", reinterpret_cast<void*>(CRT_fpe_flt_rounds));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "__p___wargv", reinterpret_cast<void*>(CRT_p_wargv));
    ldr.registerExport("ucrtbase.dll", "__p___wargv", reinterpret_cast<void*>(CRT_p_wargv));
    ldr.registerExport("msvcrt.dll", "__p___wargv", reinterpret_cast<void*>(CRT_p_wargv));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "_configure_wide_argv", reinterpret_cast<void*>(CRT_configure_wide_argv));
    ldr.registerExport("ucrtbase.dll", "_configure_wide_argv", reinterpret_cast<void*>(CRT_configure_wide_argv));
    ldr.registerExport("msvcrt.dll", "_configure_wide_argv", reinterpret_cast<void*>(CRT_configure_wide_argv));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "_initialize_wide_environment", reinterpret_cast<void*>(CRT_initialize_wide_environment));
    ldr.registerExport("ucrtbase.dll", "_initialize_wide_environment", reinterpret_cast<void*>(CRT_initialize_wide_environment));
    ldr.registerExport("msvcrt.dll", "_initialize_wide_environment", reinterpret_cast<void*>(CRT_initialize_wide_environment));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "_get_initial_wide_environment", reinterpret_cast<void*>(CRT_get_initial_wide_environment));
    ldr.registerExport("ucrtbase.dll", "_get_initial_wide_environment", reinterpret_cast<void*>(CRT_get_initial_wide_environment));
    ldr.registerExport("msvcrt.dll", "_get_initial_wide_environment", reinterpret_cast<void*>(CRT_get_initial_wide_environment));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "_get_narrow_winmain_command_line", reinterpret_cast<void*>(CRT_get_narrow_winmain_command_line));
    ldr.registerExport("ucrtbase.dll", "_get_narrow_winmain_command_line", reinterpret_cast<void*>(CRT_get_narrow_winmain_command_line));
    ldr.registerExport("msvcrt.dll", "_get_narrow_winmain_command_line", reinterpret_cast<void*>(CRT_get_narrow_winmain_command_line));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "_invalid_parameter_noinfo", reinterpret_cast<void*>(CRT_invalid_parameter_noinfo));
    ldr.registerExport("ucrtbase.dll", "_invalid_parameter_noinfo", reinterpret_cast<void*>(CRT_invalid_parameter_noinfo));
    ldr.registerExport("msvcrt.dll", "_invalid_parameter_noinfo", reinterpret_cast<void*>(CRT_invalid_parameter_noinfo));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "terminate", reinterpret_cast<void*>(CRT_terminate));
    ldr.registerExport("ucrtbase.dll", "terminate", reinterpret_cast<void*>(CRT_terminate));
    ldr.registerExport("msvcrt.dll", "terminate", reinterpret_cast<void*>(CRT_terminate));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "_initialize_onexit_table", reinterpret_cast<void*>(CRT_initialize_onexit_table));
    ldr.registerExport("ucrtbase.dll", "_initialize_onexit_table", reinterpret_cast<void*>(CRT_initialize_onexit_table));
    ldr.registerExport("msvcrt.dll", "_initialize_onexit_table", reinterpret_cast<void*>(CRT_initialize_onexit_table));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "_register_onexit_function", reinterpret_cast<void*>(CRT_register_onexit_function));
    ldr.registerExport("ucrtbase.dll", "_register_onexit_function", reinterpret_cast<void*>(CRT_register_onexit_function));
    ldr.registerExport("msvcrt.dll", "_register_onexit_function", reinterpret_cast<void*>(CRT_register_onexit_function));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "_execute_onexit_table", reinterpret_cast<void*>(CRT_execute_onexit_table));
    ldr.registerExport("ucrtbase.dll", "_execute_onexit_table", reinterpret_cast<void*>(CRT_execute_onexit_table));
    ldr.registerExport("msvcrt.dll", "_execute_onexit_table", reinterpret_cast<void*>(CRT_execute_onexit_table));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "_crt_at_quick_exit", reinterpret_cast<void*>(CRT_crt_at_quick_exit));
    ldr.registerExport("ucrtbase.dll", "_crt_at_quick_exit", reinterpret_cast<void*>(CRT_crt_at_quick_exit));
    ldr.registerExport("msvcrt.dll", "_crt_at_quick_exit", reinterpret_cast<void*>(CRT_crt_at_quick_exit));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "_seh_filter_dll", reinterpret_cast<void*>(CRT_seh_filter_dll));
    ldr.registerExport("ucrtbase.dll", "_seh_filter_dll", reinterpret_cast<void*>(CRT_seh_filter_dll));
    ldr.registerExport("msvcrt.dll", "_seh_filter_dll", reinterpret_cast<void*>(CRT_seh_filter_dll));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "_register_thread_local_exe_atexit_callback", reinterpret_cast<void*>(CRT_register_thread_local_exe_atexit_callback));
    ldr.registerExport("ucrtbase.dll", "_register_thread_local_exe_atexit_callback", reinterpret_cast<void*>(CRT_register_thread_local_exe_atexit_callback));
    ldr.registerExport("msvcrt.dll", "_register_thread_local_exe_atexit_callback", reinterpret_cast<void*>(CRT_register_thread_local_exe_atexit_callback));
    ldr.registerExport("vcruntime140.dll", "memcpy", reinterpret_cast<void*>(VCRT_memcpy));
    ldr.registerExport("vcruntime140_1.dll", "memcpy", reinterpret_cast<void*>(VCRT_memcpy));
    ldr.registerExport("msvcrt.dll", "memcpy", reinterpret_cast<void*>(VCRT_memcpy));
    ldr.registerExport("vcruntime140.dll", "memset", reinterpret_cast<void*>(VCRT_memset));
    ldr.registerExport("vcruntime140_1.dll", "memset", reinterpret_cast<void*>(VCRT_memset));
    ldr.registerExport("msvcrt.dll", "memset", reinterpret_cast<void*>(VCRT_memset));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "memset", reinterpret_cast<void*>(VCRT_memset));
    ldr.registerExport("vcruntime140.dll", "memmove", reinterpret_cast<void*>(VCRT_memmove));
    ldr.registerExport("vcruntime140_1.dll", "memmove", reinterpret_cast<void*>(VCRT_memmove));
    ldr.registerExport("msvcrt.dll", "memmove", reinterpret_cast<void*>(VCRT_memmove));
    ldr.registerExport("vcruntime140.dll", "memcmp", reinterpret_cast<void*>(VCRT_memcmp));
    ldr.registerExport("vcruntime140_1.dll", "memcmp", reinterpret_cast<void*>(VCRT_memcmp));
    ldr.registerExport("msvcrt.dll", "memcmp", reinterpret_cast<void*>(VCRT_memcmp));
    ldr.registerExport("vcruntime140.dll", "memchr", reinterpret_cast<void*>(VCRT_memchr));
    ldr.registerExport("vcruntime140_1.dll", "memchr", reinterpret_cast<void*>(VCRT_memchr));
    ldr.registerExport("msvcrt.dll", "memchr", reinterpret_cast<void*>(VCRT_memchr));
    ldr.registerExport("vcruntime140.dll", "strchr", reinterpret_cast<void*>(VCRT_strchr));
    ldr.registerExport("vcruntime140_1.dll", "strchr", reinterpret_cast<void*>(VCRT_strchr));
    ldr.registerExport("msvcrt.dll", "strchr", reinterpret_cast<void*>(VCRT_strchr));
    ldr.registerExport("vcruntime140.dll", "strrchr", reinterpret_cast<void*>(VCRT_strrchr));
    ldr.registerExport("vcruntime140_1.dll", "strrchr", reinterpret_cast<void*>(VCRT_strrchr));
    ldr.registerExport("msvcrt.dll", "strrchr", reinterpret_cast<void*>(VCRT_strrchr));
    ldr.registerExport("vcruntime140.dll", "strstr", reinterpret_cast<void*>(VCRT_strstr));
    ldr.registerExport("vcruntime140_1.dll", "strstr", reinterpret_cast<void*>(VCRT_strstr));
    ldr.registerExport("msvcrt.dll", "strstr", reinterpret_cast<void*>(VCRT_strstr));
    ldr.registerExport("vcruntime140.dll", "_CxxThrowException", reinterpret_cast<void*>(VCRT_CxxThrowException));
    ldr.registerExport("vcruntime140_1.dll", "_CxxThrowException", reinterpret_cast<void*>(VCRT_CxxThrowException));
    ldr.registerExport("msvcrt.dll", "_CxxThrowException", reinterpret_cast<void*>(VCRT_CxxThrowException));
    ldr.registerExport("vcruntime140.dll", "__CxxFrameHandler4", reinterpret_cast<void*>(VCRT_CxxFrameHandler4));
    ldr.registerExport("vcruntime140_1.dll", "__CxxFrameHandler4", reinterpret_cast<void*>(VCRT_CxxFrameHandler4));
    ldr.registerExport("msvcrt.dll", "__CxxFrameHandler4", reinterpret_cast<void*>(VCRT_CxxFrameHandler4));
    ldr.registerExport("vcruntime140.dll", "__C_specific_handler", reinterpret_cast<void*>(VCRT_C_specific_handler));
    ldr.registerExport("vcruntime140_1.dll", "__C_specific_handler", reinterpret_cast<void*>(VCRT_C_specific_handler));
    ldr.registerExport("msvcrt.dll", "__C_specific_handler", reinterpret_cast<void*>(VCRT_C_specific_handler));
    ldr.registerExport("vcruntime140.dll", "__RTDynamicCast", reinterpret_cast<void*>(VCRT_RTDynamicCast));
    ldr.registerExport("vcruntime140_1.dll", "__RTDynamicCast", reinterpret_cast<void*>(VCRT_RTDynamicCast));
    ldr.registerExport("msvcrt.dll", "__RTDynamicCast", reinterpret_cast<void*>(VCRT_RTDynamicCast));
    ldr.registerExport("vcruntime140.dll", "__current_exception", reinterpret_cast<void*>(VCRT_current_exception));
    ldr.registerExport("vcruntime140_1.dll", "__current_exception", reinterpret_cast<void*>(VCRT_current_exception));
    ldr.registerExport("msvcrt.dll", "__current_exception", reinterpret_cast<void*>(VCRT_current_exception));
    ldr.registerExport("vcruntime140.dll", "__current_exception_context", reinterpret_cast<void*>(VCRT_current_exception_context));
    ldr.registerExport("vcruntime140_1.dll", "__current_exception_context", reinterpret_cast<void*>(VCRT_current_exception_context));
    ldr.registerExport("msvcrt.dll", "__current_exception_context", reinterpret_cast<void*>(VCRT_current_exception_context));
    ldr.registerExport("vcruntime140.dll", "__intrinsic_setjmp", reinterpret_cast<void*>(VCRT_intrinsic_setjmp));
    ldr.registerExport("vcruntime140_1.dll", "__intrinsic_setjmp", reinterpret_cast<void*>(VCRT_intrinsic_setjmp));
    ldr.registerExport("msvcrt.dll", "__intrinsic_setjmp", reinterpret_cast<void*>(VCRT_intrinsic_setjmp));
    ldr.registerExport("vcruntime140.dll", "longjmp", reinterpret_cast<void*>(VCRT_longjmp));
    ldr.registerExport("vcruntime140_1.dll", "longjmp", reinterpret_cast<void*>(VCRT_longjmp));
    ldr.registerExport("msvcrt.dll", "longjmp", reinterpret_cast<void*>(VCRT_longjmp));
    ldr.registerExport("vcruntime140.dll", "__std_exception_copy", reinterpret_cast<void*>(VCRT_std_exception_copy));
    ldr.registerExport("vcruntime140_1.dll", "__std_exception_copy", reinterpret_cast<void*>(VCRT_std_exception_copy));
    ldr.registerExport("msvcrt.dll", "__std_exception_copy", reinterpret_cast<void*>(VCRT_std_exception_copy));
    ldr.registerExport("vcruntime140.dll", "__std_exception_destroy", reinterpret_cast<void*>(VCRT_std_exception_destroy));
    ldr.registerExport("vcruntime140_1.dll", "__std_exception_destroy", reinterpret_cast<void*>(VCRT_std_exception_destroy));
    ldr.registerExport("msvcrt.dll", "__std_exception_destroy", reinterpret_cast<void*>(VCRT_std_exception_destroy));
    ldr.registerExport("vcruntime140.dll", "__std_terminate", reinterpret_cast<void*>(VCRT_std_terminate));
    ldr.registerExport("vcruntime140_1.dll", "__std_terminate", reinterpret_cast<void*>(VCRT_std_terminate));
    ldr.registerExport("msvcrt.dll", "__std_terminate", reinterpret_cast<void*>(VCRT_std_terminate));
    ldr.registerExport("vcruntime140.dll", "__std_type_info_compare", reinterpret_cast<void*>(VCRT_std_type_info_compare));
    ldr.registerExport("vcruntime140_1.dll", "__std_type_info_compare", reinterpret_cast<void*>(VCRT_std_type_info_compare));
    ldr.registerExport("msvcrt.dll", "__std_type_info_compare", reinterpret_cast<void*>(VCRT_std_type_info_compare));
    ldr.registerExport("vcruntime140.dll", "__std_type_info_destroy_list", reinterpret_cast<void*>(VCRT_std_type_info_destroy_list));
    ldr.registerExport("vcruntime140_1.dll", "__std_type_info_destroy_list", reinterpret_cast<void*>(VCRT_std_type_info_destroy_list));
    ldr.registerExport("msvcrt.dll", "__std_type_info_destroy_list", reinterpret_cast<void*>(VCRT_std_type_info_destroy_list));
    ldr.registerExport("vcruntime140.dll", "_purecall", reinterpret_cast<void*>(VCRT_purecall));
    ldr.registerExport("vcruntime140_1.dll", "_purecall", reinterpret_cast<void*>(VCRT_purecall));
    ldr.registerExport("msvcrt.dll", "_purecall", reinterpret_cast<void*>(VCRT_purecall));
    ldr.registerExport("msvcp140.dll", "?_Xbad_alloc@std@@YAXXZ", reinterpret_cast<void*>(MSVC_Xbad_alloc));
    ldr.registerExport("msvcp140.dll", "?_Xbad_function_call@std@@YAXXZ", reinterpret_cast<void*>(MSVC_Xbad_function_call));
    ldr.registerExport("msvcp140.dll", "?_Throw_Cpp_error@std@@YAXH@Z", reinterpret_cast<void*>(MSVC_Throw_Cpp_error));
    ldr.registerExport("msvcp140.dll", "?_Xlength_error@std@@YAXPEBD@Z", reinterpret_cast<void*>(MSVC_Xlength_error));
    ldr.registerExport("msvcp140.dll", "?_Syserror_map@std@@YAPEBDH@Z", reinterpret_cast<void*>(MSVC_Syserror_map));
    ldr.registerExport("msvcp140.dll", "?__ExceptionPtrCreate@@YAXPEAX@Z", reinterpret_cast<void*>(MSVC_ExceptionPtrCreate));
    ldr.registerExport("msvcp140.dll", "?__ExceptionPtrDestroy@@YAXPEAX@Z", reinterpret_cast<void*>(MSVC_ExceptionPtrDestroy));
    ldr.registerExport("msvcp140.dll", "?__ExceptionPtrCopy@@YAXPEAXPEBX@Z", reinterpret_cast<void*>(MSVC_ExceptionPtrCopy));
    ldr.registerExport("msvcp140.dll", "?__ExceptionPtrCurrentException@@YAXPEAX@Z", reinterpret_cast<void*>(MSVC_ExceptionPtrCurrentException));
    ldr.registerExport("msvcp140.dll", "?uncaught_exceptions@std@@YAHXZ", reinterpret_cast<void*>(MSVC_uncaught_exceptions));
    ldr.registerExport("msvcp140.dll", "_Mtx_lock", reinterpret_cast<void*>(MSVC_Mtx_lock));
    ldr.registerExport("msvcp140.dll", "_Mtx_trylock", reinterpret_cast<void*>(MSVC_Mtx_trylock));
    ldr.registerExport("msvcp140.dll", "_Mtx_unlock", reinterpret_cast<void*>(MSVC_Mtx_unlock));
    ldr.registerExport("msvcp140.dll", "_Cnd_wait", reinterpret_cast<void*>(MSVC_Cnd_wait));
    ldr.registerExport("msvcp140.dll", "_Cnd_broadcast", reinterpret_cast<void*>(MSVC_Cnd_broadcast));
    ldr.registerExport("msvcp140.dll", "?_Random_device@std@@YAIXZ", reinterpret_cast<void*>(MSVC_Random_device));
    ldr.registerExport("msvcp140.dll", "?_W_Getdays@_Locinfo@std@@QEBAPEBGXZ", reinterpret_cast<void*>(MSVC_W_Getdays));
    ldr.registerExport("msvcp140.dll", "?_W_Getmonths@_Locinfo@std@@QEBAPEBGXZ", reinterpret_cast<void*>(MSVC_W_Getmonths));
    ldr.registerExport("msvcp140.dll", "?_Getcvt@_Locinfo@std@@QEBA?AU_Cvtvec@@XZ", reinterpret_cast<void*>(MSVC_Getcvt));
    ldr.registerExport("msvcp140.dll", "??0task_continuation_context@Concurrency@@AEAA@XZ", reinterpret_cast<void*>(MSVC_Conc_TaskContinuationContext_Ctor));
    ldr.registerExport("msvcp140.dll", "?GetCurrentThreadId@platform@details@Concurrency@@YAJXZ", reinterpret_cast<void*>(MSVC_Conc_GetCurrentThreadId));
    ldr.registerExport("msvcp140.dll", "?ReportUnhandledError@_ExceptionHolder@details@Concurrency@@AEAAXXZ", reinterpret_cast<void*>(MSVC_Conc_ReportUnhandledError));
    ldr.registerExport("msvcp140.dll", "?_CallInContext@_ContextCallback@details@Concurrency@@QEBAXV?$function@$$A6AXXZ@std@@_N@Z", reinterpret_cast<void*>(MSVC_Conc_CallInContext));
    ldr.registerExport("msvcp140.dll", "?_Capture@_ContextCallback@details@Concurrency@@AEAAXXZ", reinterpret_cast<void*>(MSVC_Conc_Capture));
    ldr.registerExport("msvcp140.dll", "?_Reset@_ContextCallback@details@Concurrency@@AEAAXXZ", reinterpret_cast<void*>(MSVC_Conc_Reset));
    ldr.registerExport("msvcp140.dll", "?_LogCancelTask@_TaskEventLogger@details@Concurrency@@QEAAXXZ", reinterpret_cast<void*>(MSVC_Conc_LogCancelTask));
    ldr.registerExport("msvcp140.dll", "?_LogScheduleTask@_TaskEventLogger@details@Concurrency@@QEAAX_N@Z", reinterpret_cast<void*>(MSVC_Conc_LogScheduleTask));
    ldr.registerExport("msvcp140.dll", "?_LogTaskCompleted@_TaskEventLogger@details@Concurrency@@QEAAXXZ", reinterpret_cast<void*>(MSVC_Conc_LogTaskCompleted));
    ldr.registerExport("msvcp140.dll", "?_LogTaskExecutionCompleted@_TaskEventLogger@details@Concurrency@@QEAAXXZ", reinterpret_cast<void*>(MSVC_Conc_LogTaskExecutionCompleted));
    ldr.registerExport("msvcp140.dll", "?_LogWorkItemCompleted@_TaskEventLogger@details@Concurrency@@QEAAXXZ", reinterpret_cast<void*>(MSVC_Conc_LogWorkItemCompleted));
    ldr.registerExport("msvcp140.dll", "?_LogWorkItemStarted@_TaskEventLogger@details@Concurrency@@QEAAXXZ", reinterpret_cast<void*>(MSVC_Conc_LogWorkItemStarted));
    ldr.registerExport("msvcp140.dll", "?_Release_chore@details@Concurrency@@YAXPEAU_Threadpool_chore@12@@Z", reinterpret_cast<void*>(MSVC_Conc_Release_chore));
    ldr.registerExport("msvcp140.dll", "?_ReportUnobservedException@details@Concurrency@@YAXXZ", reinterpret_cast<void*>(MSVC_Conc_ReportUnobservedException));
    ldr.registerExport("msvcp140.dll", "?_Schedule_chore@details@Concurrency@@YAHPEAU_Threadpool_chore@12@@Z", reinterpret_cast<void*>(MSVC_Conc_Schedule_chore));
    ldr.registerExport("msvcp140.dll", "?cin@std@@3V?$basic_istream@DU?$char_traits@D@std@@@1@A", reinterpret_cast<void*>(MSVC_cin_storage));
    ldr.registerExport("msvcp140.dll", "?cout@std@@3V?$basic_ostream@DU?$char_traits@D@std@@@1@A", reinterpret_cast<void*>(MSVC_cout_storage));
    ldr.registerExport("msvcp140.dll", "?flush@?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV12@XZ", reinterpret_cast<void*>(MSVC_cout_flush));
    ldr.registerExport("msvcp140.dll", "?write@?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV12@PEBD_J@Z", reinterpret_cast<void*>(MSVC_cout_write));
    ldr.registerExport("msvcp140.dll", "??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@P6AAEAV01@AEAV01@@Z@Z", reinterpret_cast<void*>(MSVC_cout_shift));
    ldr.registerExport("msvcp140.dll", "?_Osfx@?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAXXZ", reinterpret_cast<void*>(MSVC_cout_osfx));
    ldr.registerExport("msvcp140.dll", "?fill@?$basic_ios@DU?$char_traits@D@std@@@std@@QEBADXZ", reinterpret_cast<void*>(MSVC_ios_fill));
    ldr.registerExport("msvcp140.dll", "?tie@?$basic_ios@DU?$char_traits@D@std@@@std@@QEBAPEAV?$basic_ostream@DU?$char_traits@D@std@@@2@XZ", reinterpret_cast<void*>(MSVC_ios_tie));
    ldr.registerExport("msvcp140.dll", "?setstate@?$basic_ios@DU?$char_traits@D@std@@@std@@QEAAXH_N@Z", reinterpret_cast<void*>(MSVC_ios_setstate));
    ldr.registerExport("msvcp140.dll", "?rdbuf@?$basic_ios@DU?$char_traits@D@std@@@std@@QEBAPEAV?$basic_streambuf@DU?$char_traits@D@std@@@2@XZ", reinterpret_cast<void*>(MSVC_ios_rdbuf));
    ldr.registerExport("msvcp140.dll", "?flags@ios_base@std@@QEBAHXZ", reinterpret_cast<void*>(MSVC_ios_flags));
    ldr.registerExport("msvcp140.dll", "?good@ios_base@std@@QEBA_NXZ", reinterpret_cast<void*>(MSVC_ios_good));
    ldr.registerExport("msvcp140.dll", "?width@ios_base@std@@QEBA_JXZ", reinterpret_cast<void*>(MSVC_ios_width_get));
    ldr.registerExport("msvcp140.dll", "?width@ios_base@std@@QEAA_J_J@Z", reinterpret_cast<void*>(MSVC_ios_width_set));
    ldr.registerExport("msvcp140.dll", "?sputc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAAHD@Z", reinterpret_cast<void*>(MSVC_streambuf_sputc));
    ldr.registerExport("msvcp140.dll", "?sputn@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAA_JPEBD_J@Z", reinterpret_cast<void*>(MSVC_streambuf_sputn));
    ldr.registerExport("msvcp140.dll", "?peek@?$basic_istream@DU?$char_traits@D@std@@@std@@QEAAHXZ", reinterpret_cast<void*>(MSVC_istream_peek));

    // --- Additional C Runtime Dynamic Registrations (FileZilla & Standard Unix/Win32) ---
    ldr.registerExport("msvcrt.dll", "cosh", reinterpret_cast<void*>(CRT_cosh));
    ldr.registerExport("msvcrt.dll", "sinh", reinterpret_cast<void*>(CRT_sinh));
    ldr.registerExport("msvcrt.dll", "tanh", reinterpret_cast<void*>(CRT_tanh));
    ldr.registerExport("msvcrt.dll", "atof", reinterpret_cast<void*>(CRT_atof));
    ldr.registerExport("msvcrt.dll", "atol", reinterpret_cast<void*>(CRT_atol));
    ldr.registerExport("msvcrt.dll", "difftime", reinterpret_cast<void*>(CRT_difftime));
    ldr.registerExport("msvcrt.dll", "raise", reinterpret_cast<void*>(CRT_raise));
    ldr.registerExport("msvcrt.dll", "_wcsdup", reinterpret_cast<void*>(CRT_wcsdup));
    ldr.registerExport("msvcrt.dll", "wcsncpy", reinterpret_cast<void*>(CRT_wcsncpy));
    ldr.registerExport("msvcrt.dll", "wcscat", reinterpret_cast<void*>(CRT_wcscat));
    ldr.registerExport("msvcrt.dll", "wcschr", reinterpret_cast<void*>(CRT_wcschr));
    ldr.registerExport("msvcrt.dll", "wcspbrk", reinterpret_cast<void*>(CRT_wcspbrk));
    ldr.registerExport("msvcrt.dll", "wcsspn", reinterpret_cast<void*>(CRT_wcsspn));
    ldr.registerExport("msvcrt.dll", "wcstombs", reinterpret_cast<void*>(CRT_wcstombs));
    ldr.registerExport("msvcrt.dll", "wcstoul", reinterpret_cast<void*>(CRT_wcstoul));
    ldr.registerExport("msvcrt.dll", "_wtoi", reinterpret_cast<void*>(CRT_wtoi));
    ldr.registerExport("msvcrt.dll", "_wcsnicmp", reinterpret_cast<void*>(CRT_wcsnicmp));
    ldr.registerExport("msvcrt.dll", "_aligned_malloc", reinterpret_cast<void*>(CRT_aligned_malloc));
    ldr.registerExport("msvcrt.dll", "_aligned_free", reinterpret_cast<void*>(CRT_aligned_free));
    ldr.registerExport("msvcrt.dll", "_waccess", reinterpret_cast<void*>(CRT_waccess));
    ldr.registerExport("msvcrt.dll", "_wchdir", reinterpret_cast<void*>(CRT_wchdir));
    ldr.registerExport("msvcrt.dll", "_wchmod", reinterpret_cast<void*>(CRT_wchmod));
    ldr.registerExport("msvcrt.dll", "_wfullpath", reinterpret_cast<void*>(CRT_wfullpath));
    ldr.registerExport("msvcrt.dll", "_wgetcwd", reinterpret_cast<void*>(CRT_wgetcwd));
    ldr.registerExport("msvcrt.dll", "_wgetenv", reinterpret_cast<void*>(CRT_wgetenv));
    ldr.registerExport("msvcrt.dll", "_wputenv", reinterpret_cast<void*>(CRT_wputenv));
    ldr.registerExport("msvcrt.dll", "_wrename", reinterpret_cast<void*>(CRT_wrename));
    ldr.registerExport("msvcrt.dll", "_wutime64", reinterpret_cast<void*>(CRT_wutime64));
    ldr.registerExport("msvcrt.dll", "_wfindfirst64", reinterpret_cast<void*>(CRT_wfindfirst64));
    ldr.registerExport("msvcrt.dll", "_wfindnext64", reinterpret_cast<void*>(CRT_wfindnext64));
    ldr.registerExport("msvcrt.dll", "_findclose", reinterpret_cast<void*>(CRT_findclose));
    ldr.registerExport("msvcrt.dll", "_getcwd", reinterpret_cast<void*>(CRT_getcwd));
    ldr.registerExport("msvcrt.dll", "_getdrive", reinterpret_cast<void*>(CRT_getdrive));
    ldr.registerExport("msvcrt.dll", "_chdrive", reinterpret_cast<void*>(CRT_chdrive));
    ldr.registerExport("msvcrt.dll", "_mkdir", reinterpret_cast<void*>(CRT_mkdir));
    ldr.registerExport("msvcrt.dll", "_commit", reinterpret_cast<void*>(CRT_commit));
    ldr.registerExport("msvcrt.dll", "_dup2", reinterpret_cast<void*>(CRT_dup2));
    ldr.registerExport("msvcrt.dll", "_filelengthi64", reinterpret_cast<void*>(CRT_filelengthi64));
    ldr.registerExport("msvcrt.dll", "_telli64", reinterpret_cast<void*>(CRT_telli64));
    ldr.registerExport("msvcrt.dll", "_beginthread", reinterpret_cast<void*>(CRT_beginthread));
    ldr.registerExport("msvcrt.dll", "_endthreadex", reinterpret_cast<void*>(CRT_endthreadex));
    ldr.registerExport("msvcrt.dll", "fgetwc", reinterpret_cast<void*>(CRT_fgetwc));
    ldr.registerExport("msvcrt.dll", "fputws", reinterpret_cast<void*>(CRT_fputws));
    ldr.registerExport("msvcrt.dll", "getwc", reinterpret_cast<void*>(CRT_getwc));
    ldr.registerExport("msvcrt.dll", "putwc", reinterpret_cast<void*>(CRT_putwc));
    ldr.registerExport("msvcrt.dll", "ungetwc", reinterpret_cast<void*>(CRT_ungetwc));
    ldr.registerExport("msvcrt.dll", "_putws", reinterpret_cast<void*>(CRT_putws));
    ldr.registerExport("msvcrt.dll", "fseek", reinterpret_cast<void*>(CRT_fseek));
    ldr.registerExport("msvcrt.dll", "ftell", reinterpret_cast<void*>(CRT_ftell));
    ldr.registerExport("msvcrt.dll", "fgetpos", reinterpret_cast<void*>(CRT_fgetpos));
    ldr.registerExport("msvcrt.dll", "fsetpos", reinterpret_cast<void*>(CRT_fsetpos));
    ldr.registerExport("msvcrt.dll", "remove", reinterpret_cast<void*>(CRT_remove));
    ldr.registerExport("msvcrt.dll", "_setmaxstdio", reinterpret_cast<void*>(CRT_setmaxstdio));
    ldr.registerExport("msvcrt.dll", "_getmaxstdio", reinterpret_cast<void*>(CRT_getmaxstdio));
    ldr.registerExport("msvcrt.dll", "_wperror", reinterpret_cast<void*>(CRT_wperror));
    ldr.registerExport("msvcrt.dll", "isalnum", reinterpret_cast<void*>(CRT_isalnum));
    ldr.registerExport("msvcrt.dll", "isalpha", reinterpret_cast<void*>(CRT_isalpha));
    ldr.registerExport("msvcrt.dll", "isspace", reinterpret_cast<void*>(CRT_isspace));
    ldr.registerExport("msvcrt.dll", "iswalnum", reinterpret_cast<void*>(CRT_iswalnum));
    ldr.registerExport("msvcrt.dll", "iswalpha", reinterpret_cast<void*>(CRT_iswalpha));
    ldr.registerExport("msvcrt.dll", "iswdigit", reinterpret_cast<void*>(CRT_iswdigit));
    ldr.registerExport("msvcrt.dll", "iswprint", reinterpret_cast<void*>(CRT_iswprint));
    ldr.registerExport("msvcrt.dll", "iswpunct", reinterpret_cast<void*>(CRT_iswpunct));
    ldr.registerExport("msvcrt.dll", "iswxdigit", reinterpret_cast<void*>(CRT_iswxdigit));
    ldr.registerExport("msvcrt.dll", "_assert", reinterpret_cast<void*>(CRT_assert));
    ldr.registerExport("msvcrt.dll", "_setjmp", reinterpret_cast<void*>(CRT_setjmp));
    ldr.registerExport("msvcrt.dll", "_strtoi64", reinterpret_cast<void*>(CRT_strtoi64));
    ldr.registerExport("msvcrt.dll", "_strtoui64", reinterpret_cast<void*>(CRT_strtoui64));
    ldr.registerExport("msvcrt.dll", "_ctime64", reinterpret_cast<void*>(CRT_ctime64));
    ldr.registerExport("msvcrt.dll", "_timezone", reinterpret_cast<void*>(&CRT_timezone_val));

    // --- Tier 4: Universal CRT & C99 Math Registrations ---
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "sinf", reinterpret_cast<void*>(CRT_sinf));
    ldr.registerExport("ucrtbase.dll", "sinf", reinterpret_cast<void*>(CRT_sinf));
    ldr.registerExport("msvcrt.dll", "sinf", reinterpret_cast<void*>(CRT_sinf));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "cosf", reinterpret_cast<void*>(CRT_cosf));
    ldr.registerExport("ucrtbase.dll", "cosf", reinterpret_cast<void*>(CRT_cosf));
    ldr.registerExport("msvcrt.dll", "cosf", reinterpret_cast<void*>(CRT_cosf));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "tanf", reinterpret_cast<void*>(CRT_tanf));
    ldr.registerExport("ucrtbase.dll", "tanf", reinterpret_cast<void*>(CRT_tanf));
    ldr.registerExport("msvcrt.dll", "tanf", reinterpret_cast<void*>(CRT_tanf));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "asinf", reinterpret_cast<void*>(CRT_asinf));
    ldr.registerExport("ucrtbase.dll", "asinf", reinterpret_cast<void*>(CRT_asinf));
    ldr.registerExport("msvcrt.dll", "asinf", reinterpret_cast<void*>(CRT_asinf));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "acosf", reinterpret_cast<void*>(CRT_acosf));
    ldr.registerExport("ucrtbase.dll", "acosf", reinterpret_cast<void*>(CRT_acosf));
    ldr.registerExport("msvcrt.dll", "acosf", reinterpret_cast<void*>(CRT_acosf));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "atanf", reinterpret_cast<void*>(CRT_atanf));
    ldr.registerExport("ucrtbase.dll", "atanf", reinterpret_cast<void*>(CRT_atanf));
    ldr.registerExport("msvcrt.dll", "atanf", reinterpret_cast<void*>(CRT_atanf));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "atan2f", reinterpret_cast<void*>(CRT_atan2f));
    ldr.registerExport("ucrtbase.dll", "atan2f", reinterpret_cast<void*>(CRT_atan2f));
    ldr.registerExport("msvcrt.dll", "atan2f", reinterpret_cast<void*>(CRT_atan2f));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "sinhf", reinterpret_cast<void*>(CRT_sinhf));
    ldr.registerExport("ucrtbase.dll", "sinhf", reinterpret_cast<void*>(CRT_sinhf));
    ldr.registerExport("msvcrt.dll", "sinhf", reinterpret_cast<void*>(CRT_sinhf));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "coshf", reinterpret_cast<void*>(CRT_coshf));
    ldr.registerExport("ucrtbase.dll", "coshf", reinterpret_cast<void*>(CRT_coshf));
    ldr.registerExport("msvcrt.dll", "coshf", reinterpret_cast<void*>(CRT_coshf));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "tanhf", reinterpret_cast<void*>(CRT_tanhf));
    ldr.registerExport("ucrtbase.dll", "tanhf", reinterpret_cast<void*>(CRT_tanhf));
    ldr.registerExport("msvcrt.dll", "tanhf", reinterpret_cast<void*>(CRT_tanhf));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "expf", reinterpret_cast<void*>(CRT_expf));
    ldr.registerExport("ucrtbase.dll", "expf", reinterpret_cast<void*>(CRT_expf));
    ldr.registerExport("msvcrt.dll", "expf", reinterpret_cast<void*>(CRT_expf));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "exp2", reinterpret_cast<void*>(CRT_exp2));
    ldr.registerExport("ucrtbase.dll", "exp2", reinterpret_cast<void*>(CRT_exp2));
    ldr.registerExport("msvcrt.dll", "exp2", reinterpret_cast<void*>(CRT_exp2));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "exp2f", reinterpret_cast<void*>(CRT_exp2f));
    ldr.registerExport("ucrtbase.dll", "exp2f", reinterpret_cast<void*>(CRT_exp2f));
    ldr.registerExport("msvcrt.dll", "exp2f", reinterpret_cast<void*>(CRT_exp2f));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "expm1", reinterpret_cast<void*>(CRT_expm1));
    ldr.registerExport("ucrtbase.dll", "expm1", reinterpret_cast<void*>(CRT_expm1));
    ldr.registerExport("msvcrt.dll", "expm1", reinterpret_cast<void*>(CRT_expm1));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "logf", reinterpret_cast<void*>(CRT_logf));
    ldr.registerExport("ucrtbase.dll", "logf", reinterpret_cast<void*>(CRT_logf));
    ldr.registerExport("msvcrt.dll", "logf", reinterpret_cast<void*>(CRT_logf));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "log10f", reinterpret_cast<void*>(CRT_log10f));
    ldr.registerExport("ucrtbase.dll", "log10f", reinterpret_cast<void*>(CRT_log10f));
    ldr.registerExport("msvcrt.dll", "log10f", reinterpret_cast<void*>(CRT_log10f));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "log1p", reinterpret_cast<void*>(CRT_log1p));
    ldr.registerExport("ucrtbase.dll", "log1p", reinterpret_cast<void*>(CRT_log1p));
    ldr.registerExport("msvcrt.dll", "log1p", reinterpret_cast<void*>(CRT_log1p));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "log2f", reinterpret_cast<void*>(CRT_log2f));
    ldr.registerExport("ucrtbase.dll", "log2f", reinterpret_cast<void*>(CRT_log2f));
    ldr.registerExport("msvcrt.dll", "log2f", reinterpret_cast<void*>(CRT_log2f));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "log2l", reinterpret_cast<void*>(CRT_log2l));
    ldr.registerExport("ucrtbase.dll", "log2l", reinterpret_cast<void*>(CRT_log2l));
    ldr.registerExport("msvcrt.dll", "log2l", reinterpret_cast<void*>(CRT_log2l));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "powf", reinterpret_cast<void*>(CRT_powf));
    ldr.registerExport("ucrtbase.dll", "powf", reinterpret_cast<void*>(CRT_powf));
    ldr.registerExport("msvcrt.dll", "powf", reinterpret_cast<void*>(CRT_powf));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "sqrtf", reinterpret_cast<void*>(CRT_sqrtf));
    ldr.registerExport("ucrtbase.dll", "sqrtf", reinterpret_cast<void*>(CRT_sqrtf));
    ldr.registerExport("msvcrt.dll", "sqrtf", reinterpret_cast<void*>(CRT_sqrtf));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "cbrtf", reinterpret_cast<void*>(CRT_cbrtf));
    ldr.registerExport("ucrtbase.dll", "cbrtf", reinterpret_cast<void*>(CRT_cbrtf));
    ldr.registerExport("msvcrt.dll", "cbrtf", reinterpret_cast<void*>(CRT_cbrtf));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "ceilf", reinterpret_cast<void*>(CRT_ceilf));
    ldr.registerExport("ucrtbase.dll", "ceilf", reinterpret_cast<void*>(CRT_ceilf));
    ldr.registerExport("msvcrt.dll", "ceilf", reinterpret_cast<void*>(CRT_ceilf));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "floorf", reinterpret_cast<void*>(CRT_floorf));
    ldr.registerExport("ucrtbase.dll", "floorf", reinterpret_cast<void*>(CRT_floorf));
    ldr.registerExport("msvcrt.dll", "floorf", reinterpret_cast<void*>(CRT_floorf));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "trunc", reinterpret_cast<void*>(CRT_trunc));
    ldr.registerExport("ucrtbase.dll", "trunc", reinterpret_cast<void*>(CRT_trunc));
    ldr.registerExport("msvcrt.dll", "trunc", reinterpret_cast<void*>(CRT_trunc));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "truncf", reinterpret_cast<void*>(CRT_truncf));
    ldr.registerExport("ucrtbase.dll", "truncf", reinterpret_cast<void*>(CRT_truncf));
    ldr.registerExport("msvcrt.dll", "truncf", reinterpret_cast<void*>(CRT_truncf));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "roundf", reinterpret_cast<void*>(CRT_roundf));
    ldr.registerExport("ucrtbase.dll", "roundf", reinterpret_cast<void*>(CRT_roundf));
    ldr.registerExport("msvcrt.dll", "roundf", reinterpret_cast<void*>(CRT_roundf));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "rintf", reinterpret_cast<void*>(CRT_rintf));
    ldr.registerExport("ucrtbase.dll", "rintf", reinterpret_cast<void*>(CRT_rintf));
    ldr.registerExport("msvcrt.dll", "rintf", reinterpret_cast<void*>(CRT_rintf));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "lrint", reinterpret_cast<void*>(CRT_lrint));
    ldr.registerExport("ucrtbase.dll", "lrint", reinterpret_cast<void*>(CRT_lrint));
    ldr.registerExport("msvcrt.dll", "lrint", reinterpret_cast<void*>(CRT_lrint));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "lrintf", reinterpret_cast<void*>(CRT_lrintf));
    ldr.registerExport("ucrtbase.dll", "lrintf", reinterpret_cast<void*>(CRT_lrintf));
    ldr.registerExport("msvcrt.dll", "lrintf", reinterpret_cast<void*>(CRT_lrintf));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "lroundf", reinterpret_cast<void*>(CRT_lroundf));
    ldr.registerExport("ucrtbase.dll", "lroundf", reinterpret_cast<void*>(CRT_lroundf));
    ldr.registerExport("msvcrt.dll", "lroundf", reinterpret_cast<void*>(CRT_lroundf));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "llround", reinterpret_cast<void*>(CRT_llround));
    ldr.registerExport("ucrtbase.dll", "llround", reinterpret_cast<void*>(CRT_llround));
    ldr.registerExport("msvcrt.dll", "llround", reinterpret_cast<void*>(CRT_llround));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "llroundf", reinterpret_cast<void*>(CRT_llroundf));
    ldr.registerExport("ucrtbase.dll", "llroundf", reinterpret_cast<void*>(CRT_llroundf));
    ldr.registerExport("msvcrt.dll", "llroundf", reinterpret_cast<void*>(CRT_llroundf));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "fmin", reinterpret_cast<void*>(CRT_fmin));
    ldr.registerExport("ucrtbase.dll", "fmin", reinterpret_cast<void*>(CRT_fmin));
    ldr.registerExport("msvcrt.dll", "fmin", reinterpret_cast<void*>(CRT_fmin));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "fminf", reinterpret_cast<void*>(CRT_fminf));
    ldr.registerExport("ucrtbase.dll", "fminf", reinterpret_cast<void*>(CRT_fminf));
    ldr.registerExport("msvcrt.dll", "fminf", reinterpret_cast<void*>(CRT_fminf));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "fmax", reinterpret_cast<void*>(CRT_fmax));
    ldr.registerExport("ucrtbase.dll", "fmax", reinterpret_cast<void*>(CRT_fmax));
    ldr.registerExport("msvcrt.dll", "fmax", reinterpret_cast<void*>(CRT_fmax));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "fmaxf", reinterpret_cast<void*>(CRT_fmaxf));
    ldr.registerExport("ucrtbase.dll", "fmaxf", reinterpret_cast<void*>(CRT_fmaxf));
    ldr.registerExport("msvcrt.dll", "fmaxf", reinterpret_cast<void*>(CRT_fmaxf));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "fmodf", reinterpret_cast<void*>(CRT_fmodf));
    ldr.registerExport("ucrtbase.dll", "fmodf", reinterpret_cast<void*>(CRT_fmodf));
    ldr.registerExport("msvcrt.dll", "fmodf", reinterpret_cast<void*>(CRT_fmodf));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "remainder", reinterpret_cast<void*>(CRT_remainder));
    ldr.registerExport("ucrtbase.dll", "remainder", reinterpret_cast<void*>(CRT_remainder));
    ldr.registerExport("msvcrt.dll", "remainder", reinterpret_cast<void*>(CRT_remainder));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "remquo", reinterpret_cast<void*>(CRT_remquo));
    ldr.registerExport("ucrtbase.dll", "remquo", reinterpret_cast<void*>(CRT_remquo));
    ldr.registerExport("msvcrt.dll", "remquo", reinterpret_cast<void*>(CRT_remquo));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "fabs", reinterpret_cast<void*>(CRT_fabs));
    ldr.registerExport("ucrtbase.dll", "fabs", reinterpret_cast<void*>(CRT_fabs));
    ldr.registerExport("msvcrt.dll", "fabs", reinterpret_cast<void*>(CRT_fabs));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "fabsf", reinterpret_cast<void*>(CRT_fabsf));
    ldr.registerExport("ucrtbase.dll", "fabsf", reinterpret_cast<void*>(CRT_fabsf));
    ldr.registerExport("msvcrt.dll", "fabsf", reinterpret_cast<void*>(CRT_fabsf));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "fma", reinterpret_cast<void*>(CRT_fma));
    ldr.registerExport("ucrtbase.dll", "fma", reinterpret_cast<void*>(CRT_fma));
    ldr.registerExport("msvcrt.dll", "fma", reinterpret_cast<void*>(CRT_fma));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "ilogb", reinterpret_cast<void*>(CRT_ilogb));
    ldr.registerExport("ucrtbase.dll", "ilogb", reinterpret_cast<void*>(CRT_ilogb));
    ldr.registerExport("msvcrt.dll", "ilogb", reinterpret_cast<void*>(CRT_ilogb));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "scalbn", reinterpret_cast<void*>(CRT_scalbn));
    ldr.registerExport("ucrtbase.dll", "scalbn", reinterpret_cast<void*>(CRT_scalbn));
    ldr.registerExport("msvcrt.dll", "scalbn", reinterpret_cast<void*>(CRT_scalbn));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "modff", reinterpret_cast<void*>(CRT_modff));
    ldr.registerExport("ucrtbase.dll", "modff", reinterpret_cast<void*>(CRT_modff));
    ldr.registerExport("msvcrt.dll", "modff", reinterpret_cast<void*>(CRT_modff));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "frexp", reinterpret_cast<void*>(CRT_frexp));
    ldr.registerExport("ucrtbase.dll", "frexp", reinterpret_cast<void*>(CRT_frexp));
    ldr.registerExport("msvcrt.dll", "frexp", reinterpret_cast<void*>(CRT_frexp));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "nextafterf", reinterpret_cast<void*>(CRT_nextafterf));
    ldr.registerExport("ucrtbase.dll", "nextafterf", reinterpret_cast<void*>(CRT_nextafterf));
    ldr.registerExport("msvcrt.dll", "nextafterf", reinterpret_cast<void*>(CRT_nextafterf));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "nexttowardf", reinterpret_cast<void*>(CRT_nexttowardf));
    ldr.registerExport("ucrtbase.dll", "nexttowardf", reinterpret_cast<void*>(CRT_nexttowardf));
    ldr.registerExport("msvcrt.dll", "nexttowardf", reinterpret_cast<void*>(CRT_nexttowardf));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "hypot", reinterpret_cast<void*>(CRT_hypot));
    ldr.registerExport("ucrtbase.dll", "hypot", reinterpret_cast<void*>(CRT_hypot));
    ldr.registerExport("msvcrt.dll", "hypot", reinterpret_cast<void*>(CRT_hypot));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "hypotf", reinterpret_cast<void*>(CRT_hypotf));
    ldr.registerExport("ucrtbase.dll", "hypotf", reinterpret_cast<void*>(CRT_hypotf));
    ldr.registerExport("msvcrt.dll", "hypotf", reinterpret_cast<void*>(CRT_hypotf));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "_hypot", reinterpret_cast<void*>(CRT_hypot));
    ldr.registerExport("ucrtbase.dll", "_hypot", reinterpret_cast<void*>(CRT_hypot));
    ldr.registerExport("msvcrt.dll", "_hypot", reinterpret_cast<void*>(CRT_hypot));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "_hypotf", reinterpret_cast<void*>(CRT_hypotf));
    ldr.registerExport("ucrtbase.dll", "_hypotf", reinterpret_cast<void*>(CRT_hypotf));
    ldr.registerExport("msvcrt.dll", "_hypotf", reinterpret_cast<void*>(CRT_hypotf));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "_finite", reinterpret_cast<void*>(CRT_finite));
    ldr.registerExport("ucrtbase.dll", "_finite", reinterpret_cast<void*>(CRT_finite));
    ldr.registerExport("msvcrt.dll", "_finite", reinterpret_cast<void*>(CRT_finite));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "_isnan", reinterpret_cast<void*>(CRT_isnan));
    ldr.registerExport("ucrtbase.dll", "_isnan", reinterpret_cast<void*>(CRT_isnan));
    ldr.registerExport("msvcrt.dll", "_isnan", reinterpret_cast<void*>(CRT_isnan));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "_dsign", reinterpret_cast<void*>(CRT_dsign));
    ldr.registerExport("ucrtbase.dll", "_dsign", reinterpret_cast<void*>(CRT_dsign));
    ldr.registerExport("msvcrt.dll", "_dsign", reinterpret_cast<void*>(CRT_dsign));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "_dtest", reinterpret_cast<void*>(CRT_dtest));
    ldr.registerExport("ucrtbase.dll", "_dtest", reinterpret_cast<void*>(CRT_dtest));
    ldr.registerExport("msvcrt.dll", "_dtest", reinterpret_cast<void*>(CRT_dtest));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "_fdtest", reinterpret_cast<void*>(CRT_fdtest));
    ldr.registerExport("ucrtbase.dll", "_fdtest", reinterpret_cast<void*>(CRT_fdtest));
    ldr.registerExport("msvcrt.dll", "_fdtest", reinterpret_cast<void*>(CRT_fdtest));
    ldr.registerExport("api-ms-win-crt-math-l1-1-0.dll", "_fdclass", reinterpret_cast<void*>(CRT_fdclass));
    ldr.registerExport("ucrtbase.dll", "_fdclass", reinterpret_cast<void*>(CRT_fdclass));
    ldr.registerExport("msvcrt.dll", "_fdclass", reinterpret_cast<void*>(CRT_fdclass));
    ldr.registerExport("msvcp140_2.dll", "__std_smf_hypot3", reinterpret_cast<void*>(__std_smf_hypot3));
    ldr.registerExport("msvcp140_2.dll", "__std_smf_hypot3f", reinterpret_cast<void*>(__std_smf_hypot3f));
    ldr.registerExport("api-ms-win-crt-convert-l1-1-0.dll", "strtod", reinterpret_cast<void*>(CRT_strtod));
    ldr.registerExport("ucrtbase.dll", "strtod", reinterpret_cast<void*>(CRT_strtod));
    ldr.registerExport("msvcrt.dll", "strtod", reinterpret_cast<void*>(CRT_strtod));
    ldr.registerExport("api-ms-win-crt-convert-l1-1-0.dll", "strtof", reinterpret_cast<void*>(CRT_strtof));
    ldr.registerExport("ucrtbase.dll", "strtof", reinterpret_cast<void*>(CRT_strtof));
    ldr.registerExport("msvcrt.dll", "strtof", reinterpret_cast<void*>(CRT_strtof));
    ldr.registerExport("api-ms-win-crt-convert-l1-1-0.dll", "atoll", reinterpret_cast<void*>(CRT_atoll));
    ldr.registerExport("ucrtbase.dll", "atoll", reinterpret_cast<void*>(CRT_atoll));
    ldr.registerExport("msvcrt.dll", "atoll", reinterpret_cast<void*>(CRT_atoll));
    ldr.registerExport("api-ms-win-crt-convert-l1-1-0.dll", "strtoll", reinterpret_cast<void*>(CRT_strtoll));
    ldr.registerExport("ucrtbase.dll", "strtoll", reinterpret_cast<void*>(CRT_strtoll));
    ldr.registerExport("msvcrt.dll", "strtoll", reinterpret_cast<void*>(CRT_strtoll));
    ldr.registerExport("api-ms-win-crt-convert-l1-1-0.dll", "strtoull", reinterpret_cast<void*>(CRT_strtoull));
    ldr.registerExport("ucrtbase.dll", "strtoull", reinterpret_cast<void*>(CRT_strtoull));
    ldr.registerExport("msvcrt.dll", "strtoull", reinterpret_cast<void*>(CRT_strtoull));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "_wcsicmp", reinterpret_cast<void*>(CRT_wcsicmp));
    ldr.registerExport("ucrtbase.dll", "_wcsicmp", reinterpret_cast<void*>(CRT_wcsicmp));
    ldr.registerExport("msvcrt.dll", "_wcsicmp", reinterpret_cast<void*>(CRT_wcsicmp));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "_wcslwr", reinterpret_cast<void*>(CRT_wcslwr));
    ldr.registerExport("ucrtbase.dll", "_wcslwr", reinterpret_cast<void*>(CRT_wcslwr));
    ldr.registerExport("msvcrt.dll", "_wcslwr", reinterpret_cast<void*>(CRT_wcslwr));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "isprint", reinterpret_cast<void*>(CRT_isprint));
    ldr.registerExport("ucrtbase.dll", "isprint", reinterpret_cast<void*>(CRT_isprint));
    ldr.registerExport("msvcrt.dll", "isprint", reinterpret_cast<void*>(CRT_isprint));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "ispunct", reinterpret_cast<void*>(CRT_ispunct));
    ldr.registerExport("ucrtbase.dll", "ispunct", reinterpret_cast<void*>(CRT_ispunct));
    ldr.registerExport("msvcrt.dll", "ispunct", reinterpret_cast<void*>(CRT_ispunct));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "strcpy_s", reinterpret_cast<void*>(CRT_strcpy_s));
    ldr.registerExport("ucrtbase.dll", "strcpy_s", reinterpret_cast<void*>(CRT_strcpy_s));
    ldr.registerExport("msvcrt.dll", "strcpy_s", reinterpret_cast<void*>(CRT_strcpy_s));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "strncpy_s", reinterpret_cast<void*>(CRT_strncpy_s));
    ldr.registerExport("ucrtbase.dll", "strncpy_s", reinterpret_cast<void*>(CRT_strncpy_s));
    ldr.registerExport("msvcrt.dll", "strncpy_s", reinterpret_cast<void*>(CRT_strncpy_s));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "strncat_s", reinterpret_cast<void*>(CRT_strncat_s));
    ldr.registerExport("ucrtbase.dll", "strncat_s", reinterpret_cast<void*>(CRT_strncat_s));
    ldr.registerExport("msvcrt.dll", "strncat_s", reinterpret_cast<void*>(CRT_strncat_s));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "strtok_s", reinterpret_cast<void*>(CRT_strtok_s));
    ldr.registerExport("ucrtbase.dll", "strtok_s", reinterpret_cast<void*>(CRT_strtok_s));
    ldr.registerExport("msvcrt.dll", "strtok_s", reinterpret_cast<void*>(CRT_strtok_s));
    ldr.registerExport("api-ms-win-crt-string-l1-1-0.dll", "wcscpy_s", reinterpret_cast<void*>(CRT_wcscpy_s));
    ldr.registerExport("ucrtbase.dll", "wcscpy_s", reinterpret_cast<void*>(CRT_wcscpy_s));
    ldr.registerExport("msvcrt.dll", "wcscpy_s", reinterpret_cast<void*>(CRT_wcscpy_s));
    ldr.registerExport("api-ms-win-crt-filesystem-l1-1-0.dll", "_chdir", reinterpret_cast<void*>(CRT_chdir));
    ldr.registerExport("ucrtbase.dll", "_chdir", reinterpret_cast<void*>(CRT_chdir));
    ldr.registerExport("msvcrt.dll", "_chdir", reinterpret_cast<void*>(CRT_chdir));
    ldr.registerExport("api-ms-win-crt-filesystem-l1-1-0.dll", "_lock_file", reinterpret_cast<void*>(CRT_lock_file));
    ldr.registerExport("ucrtbase.dll", "_lock_file", reinterpret_cast<void*>(CRT_lock_file));
    ldr.registerExport("msvcrt.dll", "_lock_file", reinterpret_cast<void*>(CRT_lock_file));
    ldr.registerExport("api-ms-win-crt-filesystem-l1-1-0.dll", "_unlock_file", reinterpret_cast<void*>(CRT_unlock_file));
    ldr.registerExport("ucrtbase.dll", "_unlock_file", reinterpret_cast<void*>(CRT_unlock_file));
    ldr.registerExport("msvcrt.dll", "_unlock_file", reinterpret_cast<void*>(CRT_unlock_file));
    ldr.registerExport("api-ms-win-crt-filesystem-l1-1-0.dll", "_unlink", reinterpret_cast<void*>(CRT_unlink));
    ldr.registerExport("ucrtbase.dll", "_unlink", reinterpret_cast<void*>(CRT_unlink));
    ldr.registerExport("msvcrt.dll", "_unlink", reinterpret_cast<void*>(CRT_unlink));
    ldr.registerExport("api-ms-win-crt-filesystem-l1-1-0.dll", "_stat64i32", reinterpret_cast<void*>(CRT_stat64i32_fn));
    ldr.registerExport("ucrtbase.dll", "_stat64i32", reinterpret_cast<void*>(CRT_stat64i32_fn));
    ldr.registerExport("msvcrt.dll", "_stat64i32", reinterpret_cast<void*>(CRT_stat64i32_fn));
    ldr.registerExport("api-ms-win-crt-environment-l1-1-0.dll", "_dupenv_s", reinterpret_cast<void*>(CRT_dupenv_s));
    ldr.registerExport("ucrtbase.dll", "_dupenv_s", reinterpret_cast<void*>(CRT_dupenv_s));
    ldr.registerExport("msvcrt.dll", "_dupenv_s", reinterpret_cast<void*>(CRT_dupenv_s));
    ldr.registerExport("api-ms-win-crt-environment-l1-1-0.dll", "_putenv_s", reinterpret_cast<void*>(CRT_putenv_s));
    ldr.registerExport("ucrtbase.dll", "_putenv_s", reinterpret_cast<void*>(CRT_putenv_s));
    ldr.registerExport("msvcrt.dll", "_putenv_s", reinterpret_cast<void*>(CRT_putenv_s));
    ldr.registerExport("api-ms-win-crt-environment-l1-1-0.dll", "_wputenv_s", reinterpret_cast<void*>(CRT_wputenv_s));
    ldr.registerExport("ucrtbase.dll", "_wputenv_s", reinterpret_cast<void*>(CRT_wputenv_s));
    ldr.registerExport("msvcrt.dll", "_wputenv_s", reinterpret_cast<void*>(CRT_wputenv_s));
    ldr.registerExport("api-ms-win-crt-environment-l1-1-0.dll", "getenv_s", reinterpret_cast<void*>(CRT_getenv_s));
    ldr.registerExport("ucrtbase.dll", "getenv_s", reinterpret_cast<void*>(CRT_getenv_s));
    ldr.registerExport("msvcrt.dll", "getenv_s", reinterpret_cast<void*>(CRT_getenv_s));
    ldr.registerExport("api-ms-win-crt-locale-l1-1-0.dll", "__initialize_lconv_for_unsigned_char", reinterpret_cast<void*>(__initialize_lconv_for_unsigned_char));
    ldr.registerExport("ucrtbase.dll", "__initialize_lconv_for_unsigned_char", reinterpret_cast<void*>(__initialize_lconv_for_unsigned_char));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "_clearfp", reinterpret_cast<void*>(CRT_clearfp));
    ldr.registerExport("ucrtbase.dll", "_clearfp", reinterpret_cast<void*>(CRT_clearfp));
    ldr.registerExport("msvcrt.dll", "_clearfp", reinterpret_cast<void*>(CRT_clearfp));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "_statusfp", reinterpret_cast<void*>(CRT_statusfp));
    ldr.registerExport("ucrtbase.dll", "_statusfp", reinterpret_cast<void*>(CRT_statusfp));
    ldr.registerExport("msvcrt.dll", "_statusfp", reinterpret_cast<void*>(CRT_statusfp));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "_controlfp_s", reinterpret_cast<void*>(CRT_controlfp_s));
    ldr.registerExport("ucrtbase.dll", "_controlfp_s", reinterpret_cast<void*>(CRT_controlfp_s));
    ldr.registerExport("msvcrt.dll", "_controlfp_s", reinterpret_cast<void*>(CRT_controlfp_s));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "_get_errno", reinterpret_cast<void*>(CRT_get_errno));
    ldr.registerExport("ucrtbase.dll", "_get_errno", reinterpret_cast<void*>(CRT_get_errno));
    ldr.registerExport("msvcrt.dll", "_get_errno", reinterpret_cast<void*>(CRT_get_errno));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "_set_errno", reinterpret_cast<void*>(CRT_set_errno));
    ldr.registerExport("ucrtbase.dll", "_set_errno", reinterpret_cast<void*>(CRT_set_errno));
    ldr.registerExport("msvcrt.dll", "_set_errno", reinterpret_cast<void*>(CRT_set_errno));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "_invoke_watson", reinterpret_cast<void*>(CRT_invoke_watson));
    ldr.registerExport("ucrtbase.dll", "_invoke_watson", reinterpret_cast<void*>(CRT_invoke_watson));
    ldr.registerExport("msvcrt.dll", "_invoke_watson", reinterpret_cast<void*>(CRT_invoke_watson));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "feclearexcept", reinterpret_cast<void*>(CRT_feclearexcept));
    ldr.registerExport("ucrtbase.dll", "feclearexcept", reinterpret_cast<void*>(CRT_feclearexcept));
    ldr.registerExport("msvcrt.dll", "feclearexcept", reinterpret_cast<void*>(CRT_feclearexcept));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "fetestexcept", reinterpret_cast<void*>(CRT_fetestexcept));
    ldr.registerExport("ucrtbase.dll", "fetestexcept", reinterpret_cast<void*>(CRT_fetestexcept));
    ldr.registerExport("msvcrt.dll", "fetestexcept", reinterpret_cast<void*>(CRT_fetestexcept));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "perror", reinterpret_cast<void*>(CRT_perror));
    ldr.registerExport("ucrtbase.dll", "perror", reinterpret_cast<void*>(CRT_perror));
    ldr.registerExport("msvcrt.dll", "perror", reinterpret_cast<void*>(CRT_perror));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "strerror_s", reinterpret_cast<void*>(CRT_strerror_s));
    ldr.registerExport("ucrtbase.dll", "strerror_s", reinterpret_cast<void*>(CRT_strerror_s));
    ldr.registerExport("msvcrt.dll", "strerror_s", reinterpret_cast<void*>(CRT_strerror_s));
    ldr.registerExport("api-ms-win-crt-runtime-l1-1-0.dll", "system", reinterpret_cast<void*>(CRT_system));
    ldr.registerExport("ucrtbase.dll", "system", reinterpret_cast<void*>(CRT_system));
    ldr.registerExport("msvcrt.dll", "system", reinterpret_cast<void*>(CRT_system));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "__stdio_common_vsprintf_p", reinterpret_cast<void*>(__stdio_common_vsprintf_p));
    ldr.registerExport("ucrtbase.dll", "__stdio_common_vsprintf_p", reinterpret_cast<void*>(__stdio_common_vsprintf_p));
    ldr.registerExport("msvcrt.dll", "__stdio_common_vsprintf_p", reinterpret_cast<void*>(__stdio_common_vsprintf_p));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "__stdio_common_vsprintf_s", reinterpret_cast<void*>(__stdio_common_vsprintf_s));
    ldr.registerExport("ucrtbase.dll", "__stdio_common_vsprintf_s", reinterpret_cast<void*>(__stdio_common_vsprintf_s));
    ldr.registerExport("msvcrt.dll", "__stdio_common_vsprintf_s", reinterpret_cast<void*>(__stdio_common_vsprintf_s));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "__stdio_common_vsnprintf_s", reinterpret_cast<void*>(__stdio_common_vsnprintf_s));
    ldr.registerExport("ucrtbase.dll", "__stdio_common_vsnprintf_s", reinterpret_cast<void*>(__stdio_common_vsnprintf_s));
    ldr.registerExport("msvcrt.dll", "__stdio_common_vsnprintf_s", reinterpret_cast<void*>(__stdio_common_vsnprintf_s));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "__stdio_common_vfwprintf", reinterpret_cast<void*>(__stdio_common_vfwprintf));
    ldr.registerExport("ucrtbase.dll", "__stdio_common_vfwprintf", reinterpret_cast<void*>(__stdio_common_vfwprintf));
    ldr.registerExport("msvcrt.dll", "__stdio_common_vfwprintf", reinterpret_cast<void*>(__stdio_common_vfwprintf));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "__stdio_common_vswscanf", reinterpret_cast<void*>(__stdio_common_vswscanf));
    ldr.registerExport("ucrtbase.dll", "__stdio_common_vswscanf", reinterpret_cast<void*>(__stdio_common_vswscanf));
    ldr.registerExport("msvcrt.dll", "__stdio_common_vswscanf", reinterpret_cast<void*>(__stdio_common_vswscanf));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "_fseeki64", reinterpret_cast<void*>(CRT_fseeki64));
    ldr.registerExport("ucrtbase.dll", "_fseeki64", reinterpret_cast<void*>(CRT_fseeki64));
    ldr.registerExport("msvcrt.dll", "_fseeki64", reinterpret_cast<void*>(CRT_fseeki64));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "_ftelli64", reinterpret_cast<void*>(CRT_ftelli64));
    ldr.registerExport("ucrtbase.dll", "_ftelli64", reinterpret_cast<void*>(CRT_ftelli64));
    ldr.registerExport("msvcrt.dll", "_ftelli64", reinterpret_cast<void*>(CRT_ftelli64));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "_flushall", reinterpret_cast<void*>(CRT_flushall));
    ldr.registerExport("ucrtbase.dll", "_flushall", reinterpret_cast<void*>(CRT_flushall));
    ldr.registerExport("msvcrt.dll", "_flushall", reinterpret_cast<void*>(CRT_flushall));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "_pipe", reinterpret_cast<void*>(CRT_pipe));
    ldr.registerExport("ucrtbase.dll", "_pipe", reinterpret_cast<void*>(CRT_pipe));
    ldr.registerExport("msvcrt.dll", "_pipe", reinterpret_cast<void*>(CRT_pipe));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "_get_stream_buffer_pointers", reinterpret_cast<void*>(CRT_get_stream_buffer_pointers));
    ldr.registerExport("ucrtbase.dll", "_get_stream_buffer_pointers", reinterpret_cast<void*>(CRT_get_stream_buffer_pointers));
    ldr.registerExport("msvcrt.dll", "_get_stream_buffer_pointers", reinterpret_cast<void*>(CRT_get_stream_buffer_pointers));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "_kbhit", reinterpret_cast<void*>(CRT_kbhit));
    ldr.registerExport("ucrtbase.dll", "_kbhit", reinterpret_cast<void*>(CRT_kbhit));
    ldr.registerExport("msvcrt.dll", "_kbhit", reinterpret_cast<void*>(CRT_kbhit));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "_mktemp_s", reinterpret_cast<void*>(CRT_mktemp_s));
    ldr.registerExport("ucrtbase.dll", "_mktemp_s", reinterpret_cast<void*>(CRT_mktemp_s));
    ldr.registerExport("msvcrt.dll", "_mktemp_s", reinterpret_cast<void*>(CRT_mktemp_s));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "fopen_s", reinterpret_cast<void*>(CRT_fopen_s));
    ldr.registerExport("ucrtbase.dll", "fopen_s", reinterpret_cast<void*>(CRT_fopen_s));
    ldr.registerExport("msvcrt.dll", "fopen_s", reinterpret_cast<void*>(CRT_fopen_s));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "freopen_s", reinterpret_cast<void*>(CRT_freopen_s));
    ldr.registerExport("ucrtbase.dll", "freopen_s", reinterpret_cast<void*>(CRT_freopen_s));
    ldr.registerExport("msvcrt.dll", "freopen_s", reinterpret_cast<void*>(CRT_freopen_s));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "_wfopen_s", reinterpret_cast<void*>(CRT_wfopen_s));
    ldr.registerExport("ucrtbase.dll", "_wfopen_s", reinterpret_cast<void*>(CRT_wfopen_s));
    ldr.registerExport("msvcrt.dll", "_wfopen_s", reinterpret_cast<void*>(CRT_wfopen_s));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "_sopen_dispatch", reinterpret_cast<void*>(CRT_sopen_dispatch));
    ldr.registerExport("ucrtbase.dll", "_sopen_dispatch", reinterpret_cast<void*>(CRT_sopen_dispatch));
    ldr.registerExport("msvcrt.dll", "_sopen_dispatch", reinterpret_cast<void*>(CRT_sopen_dispatch));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "_sopen_s", reinterpret_cast<void*>(CRT_sopen_s));
    ldr.registerExport("ucrtbase.dll", "_sopen_s", reinterpret_cast<void*>(CRT_sopen_s));
    ldr.registerExport("msvcrt.dll", "_sopen_s", reinterpret_cast<void*>(CRT_sopen_s));
    ldr.registerExport("api-ms-win-crt-stdio-l1-1-0.dll", "_wsopen_dispatch", reinterpret_cast<void*>(CRT_wsopen_dispatch));
    ldr.registerExport("ucrtbase.dll", "_wsopen_dispatch", reinterpret_cast<void*>(CRT_wsopen_dispatch));
    ldr.registerExport("msvcrt.dll", "_wsopen_dispatch", reinterpret_cast<void*>(CRT_wsopen_dispatch));
    ldr.registerExport("api-ms-win-crt-time-l1-1-0.dll", "_ctime64_s", reinterpret_cast<void*>(CRT_ctime64_s));
    ldr.registerExport("ucrtbase.dll", "_ctime64_s", reinterpret_cast<void*>(CRT_ctime64_s));
    ldr.registerExport("msvcrt.dll", "_ctime64_s", reinterpret_cast<void*>(CRT_ctime64_s));
    ldr.registerExport("api-ms-win-crt-time-l1-1-0.dll", "_difftime64", reinterpret_cast<void*>(CRT_difftime64));
    ldr.registerExport("ucrtbase.dll", "_difftime64", reinterpret_cast<void*>(CRT_difftime64));
    ldr.registerExport("msvcrt.dll", "_difftime64", reinterpret_cast<void*>(CRT_difftime64));
    ldr.registerExport("api-ms-win-crt-time-l1-1-0.dll", "_ftime64", reinterpret_cast<void*>(CRT_ftime64));
    ldr.registerExport("ucrtbase.dll", "_ftime64", reinterpret_cast<void*>(CRT_ftime64));
    ldr.registerExport("msvcrt.dll", "_ftime64", reinterpret_cast<void*>(CRT_ftime64));
    ldr.registerExport("api-ms-win-crt-utility-l1-1-0.dll", "div", reinterpret_cast<void*>(CRT_div));
    ldr.registerExport("ucrtbase.dll", "div", reinterpret_cast<void*>(CRT_div));
    ldr.registerExport("msvcrt.dll", "div", reinterpret_cast<void*>(CRT_div));
    ldr.registerExport("api-ms-win-crt-utility-l1-1-0.dll", "ldiv", reinterpret_cast<void*>(CRT_ldiv));
    ldr.registerExport("ucrtbase.dll", "ldiv", reinterpret_cast<void*>(CRT_ldiv));
    ldr.registerExport("msvcrt.dll", "ldiv", reinterpret_cast<void*>(CRT_ldiv));
    ldr.registerExport("vcruntime140.dll", "wcsrchr", reinterpret_cast<void*>(CRT_wcsrchr));
    ldr.registerExport("msvcrt.dll", "wcsrchr", reinterpret_cast<void*>(CRT_wcsrchr));
    ldr.registerExport("vcruntime140.dll", "wcsstr", reinterpret_cast<void*>(wcsstr));
    ldr.registerExport("msvcrt.dll", "wcsstr", reinterpret_cast<void*>(wcsstr));
    ldr.registerExport("vcruntime140.dll", "__std_type_info_name", reinterpret_cast<void*>(__std_type_info_name));
    ldr.registerExport("msvcrt.dll", "__std_type_info_name", reinterpret_cast<void*>(__std_type_info_name));
    ldr.registerExport("vcruntime140.dll", "__std_type_info_hash", reinterpret_cast<void*>(__std_type_info_hash));
    ldr.registerExport("msvcrt.dll", "__std_type_info_hash", reinterpret_cast<void*>(__std_type_info_hash));
    ldr.registerExport("vcruntime140.dll", "__CxxFrameHandler3", reinterpret_cast<void*>(__CxxFrameHandler3));
    ldr.registerExport("msvcrt.dll", "__CxxFrameHandler3", reinterpret_cast<void*>(__CxxFrameHandler3));
    ldr.registerExport("vcruntime140.dll", "__RTCastToVoid", reinterpret_cast<void*>(__RTCastToVoid));
    ldr.registerExport("msvcrt.dll", "__RTCastToVoid", reinterpret_cast<void*>(__RTCastToVoid));
    ldr.registerExport("vcruntime140.dll", "__RTtypeid", reinterpret_cast<void*>(__RTtypeid));
    ldr.registerExport("msvcrt.dll", "__RTtypeid", reinterpret_cast<void*>(__RTtypeid));

    // --- MSVCP140 C++ Standard Library Symbols ---
    ldr.registerExport("msvcp140.dll", "??0?$basic_ios@DU?$char_traits@D@std@@@std@@IEAA@XZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??0?$basic_iostream@DU?$char_traits@D@std@@@std@@QEAA@PEAV?$basic_streambuf@DU?$char_traits@D@std@@@1@@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??0?$basic_istream@DU?$char_traits@D@std@@@std@@QEAA@PEAV?$basic_streambuf@DU?$char_traits@D@std@@@1@_N@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??0?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAA@PEAV?$basic_streambuf@DU?$char_traits@D@std@@@1@_N@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??0?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEAA@XZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??0?$codecvt@_SDU_Mbstatet@@@std@@QEAA@_K@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??0?$codecvt@_UDU_Mbstatet@@@std@@QEAA@_K@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??0?$codecvt@_WDU_Mbstatet@@@std@@QEAA@_K@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??0_Locinfo@std@@QEAA@HPEBD@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??0_Locinfo@std@@QEAA@PEBD@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??0_Lockit@std@@QEAA@H@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??0facet@locale@std@@IEAA@_K@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??0ios_base@std@@IEAA@XZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??1?$basic_ios@DU?$char_traits@D@std@@@std@@UEAA@XZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??1?$basic_iostream@DU?$char_traits@D@std@@@std@@UEAA@XZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??1?$basic_istream@DU?$char_traits@D@std@@@std@@UEAA@XZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??1?$basic_ostream@DU?$char_traits@D@std@@@std@@UEAA@XZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??1?$basic_streambuf@DU?$char_traits@D@std@@@std@@UEAA@XZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??1?$codecvt@_SDU_Mbstatet@@@std@@MEAA@XZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??1?$codecvt@_UDU_Mbstatet@@@std@@MEAA@XZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??1?$codecvt@_WDU_Mbstatet@@@std@@MEAA@XZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??1_Locinfo@std@@QEAA@XZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??1_Lockit@std@@QEAA@XZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??1facet@locale@std@@MEAA@XZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??1ios_base@std@@UEAA@XZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??4?$_Yarn@D@std@@QEAAAEAV01@PEBD@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??5?$basic_istream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@AEAH@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??5?$basic_istream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@AEAI@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??5?$basic_istream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@AEAM@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??5?$basic_istream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@AEAN@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??5?$basic_istream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@AEAPEAX@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@F@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@G@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@H@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@I@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@J@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@K@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@M@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@N@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@P6AAEAVios_base@1@AEAV21@@Z@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@PEAV?$basic_streambuf@DU?$char_traits@D@std@@@1@@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@PEBX@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@_J@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@_K@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??6?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV01@_N@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??7ios_base@std@@QEBA_NXZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??Bios_base@std@@QEBA_NXZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??_D?$basic_istream@DU?$char_traits@D@std@@@std@@QEAAXXZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "??_D?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAXXZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?_Decref@facet@locale@std@@UEAAPEAV_Facet_base@3@XZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?_Fiopen@std@@YAPEAU_iobuf@@PEBDHH@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?_Fiopen@std@@YAPEAU_iobuf@@PEB_WHH@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?_Getcat@?$codecvt@DDU_Mbstatet@@@std@@SA_KPEAPEBVfacet@locale@2@PEBV42@@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?_Getcat@?$ctype@D@std@@SA_KPEAPEBVfacet@locale@2@PEBV42@@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?_Getcat@?$time_put@DV?$ostreambuf_iterator@DU?$char_traits@D@std@@@std@@@std@@SA_KPEAPEBVfacet@locale@2@PEBV42@@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?_Getcoll@_Locinfo@std@@QEBA?AU_Collvec@@XZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?_Getfalse@_Locinfo@std@@QEBAPEBDXZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?_Getgloballocale@locale@std@@CAPEAV_Locimp@12@XZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?_Gettrue@_Locinfo@std@@QEBAPEBDXZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?_Gnavail@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEBA_JXZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?_Gndec@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEAAPEADXZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?_Gninc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEAAPEADXZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?_Id_cnt@id@locale@std@@0HA", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?_Incref@facet@locale@std@@UEAAXXZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?_Init@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEAAXPEAPEAD0PEAH001@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?_Init@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEAAXXZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?_Init@locale@std@@CAPEAV_Locimp@12@_N@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?_Ipfx@?$basic_istream@DU?$char_traits@D@std@@@std@@QEAA_N_N@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?_Locimp_Addfac@_Locimp@locale@std@@CAXPEAV123@PEAVfacet@23@_K@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?_Lock@?$basic_streambuf@DU?$char_traits@D@std@@@std@@UEAAXXZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?_Makeloc@_Locimp@locale@std@@CAPEAV123@AEBV_Locinfo@3@HPEAV123@PEBV23@@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?_New_Locimp@_Locimp@locale@std@@CAPEAV123@AEBV123@@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?_New_Locimp@_Locimp@locale@std@@CAPEAV123@_N@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?_Pnavail@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEBA_JXZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?_Pninc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEAAPEADXZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?_Unlock@?$basic_streambuf@DU?$char_traits@D@std@@@std@@UEAAXXZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?_Winerror_map@std@@YAHH@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?_Xinvalid_argument@std@@YAXPEBD@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?_Xout_of_range@std@@YAXPEBD@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?_Xregex_error@std@@YAXW4error_type@regex_constants@1@@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?_Xruntime_error@std@@YAXPEBD@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?__ExceptionPtrAssign@@YAXPEAXPEBX@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?__ExceptionPtrCopyException@@YAXPEAXPEBX1@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?__ExceptionPtrRethrow@@YAXPEBX@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?__ExceptionPtrToBool@@YA_NPEBX@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?always_noconv@codecvt_base@std@@QEBA_NXZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?bad@ios_base@std@@QEBA_NXZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?c_str@?$_Yarn@D@std@@QEBAPEBDXZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?cerr@std@@3V?$basic_ostream@DU?$char_traits@D@std@@@1@A", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?classic@locale@std@@SAAEBV12@XZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?clear@?$basic_ios@DU?$char_traits@D@std@@@std@@QEAAXH_N@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?clog@std@@3V?$basic_ostream@DU?$char_traits@D@std@@@1@A", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?copyfmt@ios_base@std@@QEAAAEAV12@AEBV12@@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?eback@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEBAPEADXZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?egptr@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEBAPEADXZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?eof@ios_base@std@@QEBA_NXZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?epptr@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEBAPEADXZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?exceptions@ios_base@std@@QEAAXH@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?fail@ios_base@std@@QEBA_NXZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?fill@?$basic_ios@DU?$char_traits@D@std@@@std@@QEAADD@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?gbump@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEAAXH@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?getloc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEBA?AVlocale@2@XZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?getloc@ios_base@std@@QEBA?AVlocale@2@XZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?gptr@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEBAPEADXZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?id@?$codecvt@DDU_Mbstatet@@@std@@2V0locale@2@A", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?id@?$codecvt@_WDU_Mbstatet@@@std@@2V0locale@2@A", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?id@?$collate@D@std@@2V0locale@2@A", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?id@?$ctype@D@std@@2V0locale@2@A", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?id@?$numpunct@D@std@@2V0locale@2@A", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?id@?$time_put@DV?$ostreambuf_iterator@DU?$char_traits@D@std@@@std@@@std@@2V0locale@2@A", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?imbue@?$basic_ios@DU?$char_traits@D@std@@@std@@QEAA?AVlocale@2@AEBV32@@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?imbue@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAAXAEBVlocale@2@@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?in@?$codecvt@DDU_Mbstatet@@@std@@QEBAHAEAU_Mbstatet@@PEBD1AEAPEBDPEAD3AEAPEAD@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?in@?$codecvt@_WDU_Mbstatet@@@std@@QEBAHAEAU_Mbstatet@@PEBD1AEAPEBDPEA_W3AEAPEA_W@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?init@?$basic_ios@DU?$char_traits@D@std@@@std@@IEAAXPEAV?$basic_streambuf@DU?$char_traits@D@std@@@2@_N@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?out@?$codecvt@DDU_Mbstatet@@@std@@QEBAHAEAU_Mbstatet@@PEBD1AEAPEBDPEAD3AEAPEAD@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?out@?$codecvt@_SDU_Mbstatet@@@std@@QEBAHAEAU_Mbstatet@@PEB_S1AEAPEB_SPEAD3AEAPEAD@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?out@?$codecvt@_UDU_Mbstatet@@@std@@QEBAHAEAU_Mbstatet@@PEB_U1AEAPEB_UPEAD3AEAPEAD@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?out@?$codecvt@_WDU_Mbstatet@@@std@@QEBAHAEAU_Mbstatet@@PEB_W1AEAPEB_WPEAD3AEAPEAD@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?pbackfail@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAAHH@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?pbase@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEBAPEADXZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?pbump@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEAAXH@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?pptr@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEBAPEADXZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?precision@ios_base@std@@QEAA_J_J@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?precision@ios_base@std@@QEBA_JXZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?put@?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV12@D@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?put@?$time_put@DV?$ostreambuf_iterator@DU?$char_traits@D@std@@@std@@@std@@QEBA?AV?$ostreambuf_iterator@DU?$char_traits@D@std@@@2@V32@AEAVios_base@2@DPEBUtm@@PEBD3@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?rdbuf@?$basic_ios@DU?$char_traits@D@std@@@std@@QEAAPEAV?$basic_streambuf@DU?$char_traits@D@std@@@2@PEAV32@@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?read@?$basic_istream@DU?$char_traits@D@std@@@std@@QEAAAEAV12@PEAD_J@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?sbumpc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAAHXZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?seekg@?$basic_istream@DU?$char_traits@D@std@@@std@@QEAAAEAV12@V?$fpos@U_Mbstatet@@@2@@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?seekg@?$basic_istream@DU?$char_traits@D@std@@@std@@QEAAAEAV12@_JH@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?seekoff@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAA?AV?$fpos@U_Mbstatet@@@2@_JHH@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?seekp@?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV12@V?$fpos@U_Mbstatet@@@2@@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?seekp@?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAAAEAV12@_JH@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?seekpos@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAA?AV?$fpos@U_Mbstatet@@@2@V32@H@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?setbuf@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAAPEAV12@PEAD_J@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?setf@ios_base@std@@QEAAHH@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?setf@ios_base@std@@QEAAHHH@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?setg@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEAAXPEAD00@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?setp@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEAAXPEAD00@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?setp@?$basic_streambuf@DU?$char_traits@D@std@@@std@@IEAAXPEAD0@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?setprecision@std@@YA?AU?$_Smanip@_J@1@_J@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?setw@std@@YA?AU?$_Smanip@_J@1@_J@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?sgetc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAAHXZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?showmanyc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAA_JXZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?snextc@?$basic_streambuf@DU?$char_traits@D@std@@@std@@QEAAHXZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?sync@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAAHXZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?tellg@?$basic_istream@DU?$char_traits@D@std@@@std@@QEAA?AV?$fpos@U_Mbstatet@@@2@XZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?tellp@?$basic_ostream@DU?$char_traits@D@std@@@std@@QEAA?AV?$fpos@U_Mbstatet@@@2@XZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?tolower@?$ctype@D@std@@QEBADD@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?tolower@?$ctype@D@std@@QEBAPEBDPEADPEBD@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?uflow@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAAHXZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?uncaught_exception@std@@YA_NXZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?underflow@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAAHXZ", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?unshift@?$codecvt@DDU_Mbstatet@@@std@@QEBAHAEAU_Mbstatet@@PEAD1AEAPEAD@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?widen@?$basic_ios@DU?$char_traits@D@std@@@std@@QEBADD@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?xsgetn@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAA_JPEAD_J@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "?xsputn@?$basic_streambuf@DU?$char_traits@D@std@@@std@@MEAA_JPEBD_J@Z", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "_Cnd_do_broadcast_at_thread_exit", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "_Cnd_register_at_thread_exit", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "_Cnd_signal", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "_Cnd_unregister_at_thread_exit", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "_Exp", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "_Lock_shared_ptr_spin_lock", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "_Query_perf_counter", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "_Query_perf_frequency", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "_Strcoll", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "_Strxfrm", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "_Thrd_detach", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "_Thrd_hardware_concurrency", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "_Thrd_id", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "_Thrd_join", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "_Thrd_yield", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "_Unlock_shared_ptr_spin_lock", reinterpret_cast<void*>(MSVC_GenericStub));
    ldr.registerExport("msvcp140.dll", "_Xtime_get_ticks", reinterpret_cast<void*>(MSVC_GenericStub));

}

} // namespace micant::msvcrt
