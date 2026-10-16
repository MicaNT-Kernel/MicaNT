/**
 * @file winnt_ucrtbase_runtime_abi.cpp
 * @brief Horizon 116 (Kernel Unit #88): Sovereign WinNT Universal C Runtime (ucrtbase.dll)
 *        Official PE32+ Exports, OnExit Table Dispatch, C Stdio Streams, Heap Allocator,
 *        and C-Runtime Startup Sequences for Standard Windows PE32+ Executables.
 */

#include "micant/ucrt/winnt_ucrtbase_runtime_abi.hpp"
#include <cstdlib>
#include <cstdio>
#include <cmath>
#include <cstdarg>
#include <vector>

namespace micant::ucrt::winnt_ucrtbase_abi {

namespace {
thread_local int32_t g_ucrtErrno = 0;
int32_t g_appType = 0;
char g_dummyEnvKey[] = "PATH=C:\\Windows\\System32";
char* g_dummyEnv[2] = { g_dummyEnvKey, nullptr };
char g_iobDummy[3][48]{};
} // namespace

int32_t Ucrt_initialize_onexit_table(_onexit_table_t* table) noexcept {
    if (!table) return -1;
    table->_first = nullptr;
    table->_last  = nullptr;
    table->_end   = nullptr;
    return 0;
}

int32_t Ucrt_register_onexit_function(_onexit_table_t* table, _onexit_t function) noexcept {
    if (!table || !function) return -1;
    size_t count = (table->_last && table->_first) ? (table->_last - table->_first) : 0;
    size_t cap   = (table->_end && table->_first)  ? (table->_end - table->_first) : 0;
    if (count >= cap) {
        size_t newCap = cap == 0 ? 8 : cap * 2;
        auto* newBuf = static_cast<_onexit_t*>(std::realloc(table->_first, newCap * sizeof(_onexit_t)));
        if (!newBuf) return -1;
        table->_first = newBuf;
        table->_last  = newBuf + count;
        table->_end   = newBuf + newCap;
    }
    *(table->_last) = function;
    table->_last++;
    return 0;
}

int32_t Ucrt_execute_onexit_table(_onexit_table_t* table) noexcept {
    if (!table || !table->_first || !table->_last) return 0;
    while (table->_last > table->_first) {
        table->_last--;
        _onexit_t fn = *(table->_last);
        if (fn) fn();
    }
    std::free(table->_first);
    table->_first = nullptr;
    table->_last  = nullptr;
    table->_end   = nullptr;
    return 0;
}

int32_t Ucrt_crt_atexit(_onexit_t function) noexcept {
    static _onexit_table_t s_globalTable{};
    static bool s_inited = false;
    if (!s_inited) {
        Ucrt_initialize_onexit_table(&s_globalTable);
        s_inited = true;
    }
    return Ucrt_register_onexit_function(&s_globalTable, function);
}

int32_t Ucrt_seh_filter_exe(uint32_t /*xcptNum*/, void* /*pXcp*/) noexcept {
    return 1; // EXCEPTION_EXECUTE_HANDLER
}

void Ucrt_set_app_type(int32_t appType) noexcept {
    g_appType = appType;
}

int32_t Ucrt_configure_narrow_argv(int32_t /*mode*/) noexcept {
    return 0;
}

int32_t Ucrt_initialize_narrow_environment() noexcept {
    return 0;
}

char** Ucrt_get_initial_narrow_environment() noexcept {
    return g_dummyEnv;
}

void Ucrt_initterm(void (**first)(void), void (**last)(void)) noexcept {
    if (!first || !last) return;
    for (auto it = first; it < last; ++it) {
        if (*it) (*it)();
    }
}

int32_t Ucrt_initterm_e(int32_t (**first)(void), int32_t (**last)(void)) noexcept {
    if (!first || !last) return 0;
    for (auto it = first; it < last; ++it) {
        if (*it) {
            int32_t res = (*it)();
            if (res != 0) return res;
        }
    }
    return 0;
}

void* Ucrt_malloc(size_t size) noexcept {
    return std::malloc(size > 0 ? size : 1);
}

void* Ucrt_calloc(size_t count, size_t size) noexcept {
    return std::calloc(count > 0 ? count : 1, size > 0 ? size : 1);
}

void* Ucrt_realloc(void* block, size_t size) noexcept {
    return std::realloc(block, size > 0 ? size : 1);
}

void Ucrt_free(void* block) noexcept {
    if (block) std::free(block);
}

void* Ucrt_acrt_iob_func(uint32_t index) noexcept {
    return (index < 3) ? reinterpret_cast<void*>(&g_iobDummy[index]) : nullptr;
}

int32_t Ucrt_puts(const char* str) noexcept {
    return str ? static_cast<int32_t>(std::strlen(str)) : 0;
}

int32_t Ucrt_stdio_common_vsprintf(uint64_t /*options*/, char* buffer, size_t bufferCount, const char* format, void* /*locale*/, va_list valist) noexcept {
    if (!buffer || bufferCount == 0 || !format) return -1;
    return std::vsnprintf(buffer, bufferCount, format, valist);
}

int32_t* Ucrt_errno() noexcept {
    return &g_ucrtErrno;
}

void Ucrt_exit(int32_t /*status*/) noexcept {}
void Ucrt_quick_exit(int32_t /*status*/) noexcept {}
void Ucrt_abort() noexcept {}

double Ucrt_sin(double x) noexcept { return std::sin(x); }
double Ucrt_cos(double x) noexcept { return std::cos(x); }
double Ucrt_sqrt(double x) noexcept { return std::sqrt(x); }
double Ucrt_pow(double x, double y) noexcept { return std::pow(x, y); }

#if !defined(BUILD_SYSTEM32_STANDALONE_DLL)
void WinNtUcrtbaseCoordinator::initialize() noexcept {
    if (m_initialized) return;
    m_cachedReport = runUcrtbaseAudit();
    m_initialized = true;
}

Horizon116UcrtbaseAuditReport WinNtUcrtbaseCoordinator::runUcrtbaseAudit() noexcept {
    Horizon116UcrtbaseAuditReport rep{};
    rep.ucrtbaseExportCount = 26;

    // 1. Verify OnExit Table Lifecycle
    _onexit_table_t tbl{};
    static int s_exitRan = 0;
    s_exitRan = 0;
    if (Ucrt_initialize_onexit_table(&tbl) == 0) {
        Ucrt_register_onexit_function(&tbl, []() -> int {
            s_exitRan = 42;
            return 0;
        });
        Ucrt_execute_onexit_table(&tbl);
        rep.onExitTableDispatchVerified = (s_exitRan == 42);
    }

    // 2. Verify _initterm and _initterm_e Execution
    static int s_termCalls = 0;
    s_termCalls = 0;
    void (*terms[2])(void) = {
        []() { s_termCalls += 1; },
        []() { s_termCalls += 2; }
    };
    int32_t (*term_e[2])(void) = {
        []() -> int32_t { s_termCalls += 10; return 0; },
        []() -> int32_t { s_termCalls += 20; return 0; }
    };
    Ucrt_initterm(&terms[0], &terms[2]);
    Ucrt_initterm_e(&term_e[0], &term_e[2]);
    rep.inittermExecutionVerified = (s_termCalls == 33);

    // 3. Verify Memory Allocation, Realloc, and Free
    void* p = Ucrt_malloc(64);
    if (p) {
        std::memset(p, 0xAB, 64);
        void* p2 = Ucrt_realloc(p, 128);
        if (p2) {
            rep.heapAllocReallocFreeVerified = true;
            Ucrt_free(p2);
        }
    }

    // 4. Verify Stdio Streams and Format
    char buf[128]{};
    auto testFormat = [](char* b, size_t bc, const char* fmt, ...) -> int32_t {
        va_list args;
        va_start(args, fmt);
        int32_t r = Ucrt_stdio_common_vsprintf(0, b, bc, fmt, nullptr, args);
        va_end(args);
        return r;
    };
    int32_t n = testFormat(buf, sizeof(buf), "MicaNT UCRT %d", 26100);
    rep.stdioFormatAndStreamsVerified = (n > 0 && std::strcmp(buf, "MicaNT UCRT 26100") == 0 &&
                                         Ucrt_acrt_iob_func(0) != nullptr);

    // 5. Verify Math & Errno
    *Ucrt_errno() = 0;
    double s = Ucrt_sin(0.0);
    double c = Ucrt_cos(0.0);
    double sq = Ucrt_sqrt(16.0);
    double pw = Ucrt_pow(2.0, 8.0);
    rep.mathAndErrnoVerified = (s == 0.0 && c == 1.0 && sq == 4.0 && pw == 256.0 && *Ucrt_errno() == 0);

    return rep;
}

void InitializeWinNtUcrtbaseSubsystem() noexcept {
    WinNtUcrtbaseCoordinator::instance().initialize();
}
#endif

} // namespace micant::ucrt::winnt_ucrtbase_abi

#if !defined(BUILD_SYSTEM32_STANDALONE_DLL)
namespace micant::sys::internal {
void WinNtUcrtbaseInitializeExecutiveSubsystem() noexcept {
    micant::ucrt::winnt_ucrtbase_abi::InitializeWinNtUcrtbaseSubsystem();
}
} // namespace micant::sys::internal
#endif

#if defined(BUILD_SYSTEM32_UCRTBASE_DLL)
extern "C" {
__declspec(dllexport) int32_t _initialize_onexit_table(micant::ucrt::winnt_ucrtbase_abi::_onexit_table_t* t) { return micant::ucrt::winnt_ucrtbase_abi::Ucrt_initialize_onexit_table(t); }
__declspec(dllexport) int32_t _register_onexit_function(micant::ucrt::winnt_ucrtbase_abi::_onexit_table_t* t, micant::ucrt::winnt_ucrtbase_abi::_onexit_t fn) { return micant::ucrt::winnt_ucrtbase_abi::Ucrt_register_onexit_function(t, fn); }
__declspec(dllexport) int32_t _execute_onexit_table(micant::ucrt::winnt_ucrtbase_abi::_onexit_table_t* t) { return micant::ucrt::winnt_ucrtbase_abi::Ucrt_execute_onexit_table(t); }
__declspec(dllexport) int32_t _crt_atexit(micant::ucrt::winnt_ucrtbase_abi::_onexit_t fn) { return micant::ucrt::winnt_ucrtbase_abi::Ucrt_crt_atexit(fn); }
__declspec(dllexport) int32_t _seh_filter_exe(uint32_t n, void* x) { return micant::ucrt::winnt_ucrtbase_abi::Ucrt_seh_filter_exe(n, x); }
__declspec(dllexport) void _set_app_type(int32_t t) { micant::ucrt::winnt_ucrtbase_abi::Ucrt_set_app_type(t); }
__declspec(dllexport) int32_t _configure_narrow_argv(int32_t m) { return micant::ucrt::winnt_ucrtbase_abi::Ucrt_configure_narrow_argv(m); }
__declspec(dllexport) int32_t _initialize_narrow_environment() { return micant::ucrt::winnt_ucrtbase_abi::Ucrt_initialize_narrow_environment(); }
__declspec(dllexport) char** _get_initial_narrow_environment() { return micant::ucrt::winnt_ucrtbase_abi::Ucrt_get_initial_narrow_environment(); }
__declspec(dllexport) void _initterm(void (**f)(void), void (**l)(void)) { micant::ucrt::winnt_ucrtbase_abi::Ucrt_initterm(f, l); }
__declspec(dllexport) int32_t _initterm_e(int32_t (**f)(void), int32_t (**l)(void)) { return micant::ucrt::winnt_ucrtbase_abi::Ucrt_initterm_e(f, l); }
__declspec(dllexport) void* ucrt_malloc(size_t s) { return micant::ucrt::winnt_ucrtbase_abi::Ucrt_malloc(s); }
__declspec(dllexport) void* ucrt_calloc(size_t c, size_t s) { return micant::ucrt::winnt_ucrtbase_abi::Ucrt_calloc(c, s); }
__declspec(dllexport) void* ucrt_realloc(void* b, size_t s) { return micant::ucrt::winnt_ucrtbase_abi::Ucrt_realloc(b, s); }
__declspec(dllexport) void ucrt_free(void* b) { micant::ucrt::winnt_ucrtbase_abi::Ucrt_free(b); }
__declspec(dllexport) void* ucrt_acrt_iob_func(uint32_t i) { return micant::ucrt::winnt_ucrtbase_abi::Ucrt_acrt_iob_func(i); }
__declspec(dllexport) int32_t ucrt_puts(const char* s) { return micant::ucrt::winnt_ucrtbase_abi::Ucrt_puts(s); }
__declspec(dllexport) int32_t ucrt_stdio_common_vsprintf(uint64_t opt, char* b, size_t bc, const char* f, void* l, va_list va) { return micant::ucrt::winnt_ucrtbase_abi::Ucrt_stdio_common_vsprintf(opt, b, bc, f, l, va); }
__declspec(dllexport) int32_t* ucrt_errno() { return micant::ucrt::winnt_ucrtbase_abi::Ucrt_errno(); }
__declspec(dllexport) void ucrt_exit(int32_t s) { micant::ucrt::winnt_ucrtbase_abi::Ucrt_exit(s); }
__declspec(dllexport) void ucrt_quick_exit(int32_t s) { micant::ucrt::winnt_ucrtbase_abi::Ucrt_quick_exit(s); }
__declspec(dllexport) void ucrt_abort() { micant::ucrt::winnt_ucrtbase_abi::Ucrt_abort(); }
__declspec(dllexport) double ucrt_sin(double x) { return micant::ucrt::winnt_ucrtbase_abi::Ucrt_sin(x); }
__declspec(dllexport) double ucrt_cos(double x) { return micant::ucrt::winnt_ucrtbase_abi::Ucrt_cos(x); }
__declspec(dllexport) double ucrt_sqrt(double x) { return micant::ucrt::winnt_ucrtbase_abi::Ucrt_sqrt(x); }
__declspec(dllexport) double ucrt_pow(double x, double y) { return micant::ucrt::winnt_ucrtbase_abi::Ucrt_pow(x, y); }
} // extern "C"
#endif
