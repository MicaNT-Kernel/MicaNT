#pragma once
/**
 * @file winnt_ucrtbase_runtime_abi.hpp
 * @brief Horizon 116 (Kernel Unit #88): Sovereign WinNT Universal C Runtime (ucrtbase.dll)
 *        Official PE32+ Exports, OnExit Table Dispatch, C Stdio Streams, Heap Allocator,
 *        and C-Runtime Startup Sequences for Standard Windows PE32+ Executables.
 */

#include <cstdint>
#include <cstddef>
#include <cstring>

#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"

namespace micant::ucrt::winnt_ucrtbase_abi {

using _onexit_t = int (__cdecl *)(void);

struct _onexit_table_t {
    _onexit_t* _first{nullptr};
    _onexit_t* _last{nullptr};
    _onexit_t* _end{nullptr};
};

// --- ucrtbase.dll official exports (26 functions) ---
int32_t  Ucrt_initialize_onexit_table(_onexit_table_t* table) noexcept;
int32_t  Ucrt_register_onexit_function(_onexit_table_t* table, _onexit_t function) noexcept;
int32_t  Ucrt_execute_onexit_table(_onexit_table_t* table) noexcept;
int32_t  Ucrt_crt_atexit(_onexit_t function) noexcept;
int32_t  Ucrt_seh_filter_exe(uint32_t xcptNum, void* pXcp) noexcept;
void     Ucrt_set_app_type(int32_t appType) noexcept;
int32_t  Ucrt_configure_narrow_argv(int32_t mode) noexcept;
int32_t  Ucrt_initialize_narrow_environment() noexcept;
char**   Ucrt_get_initial_narrow_environment() noexcept;
void     Ucrt_initterm(void (**first)(void), void (**last)(void)) noexcept;
int32_t  Ucrt_initterm_e(int32_t (**first)(void), int32_t (**last)(void)) noexcept;
void*    Ucrt_malloc(size_t size) noexcept;
void*    Ucrt_calloc(size_t count, size_t size) noexcept;
void*    Ucrt_realloc(void* block, size_t size) noexcept;
void     Ucrt_free(void* block) noexcept;
void*    Ucrt_acrt_iob_func(uint32_t index) noexcept;
int32_t  Ucrt_puts(const char* str) noexcept;
int32_t  Ucrt_stdio_common_vsprintf(uint64_t options, char* buffer, size_t bufferCount, const char* format, void* locale, va_list valist) noexcept;
int32_t* Ucrt_errno() noexcept;
void     Ucrt_exit(int32_t status) noexcept;
void     Ucrt_quick_exit(int32_t status) noexcept;
void     Ucrt_abort() noexcept;
double   Ucrt_sin(double x) noexcept;
double   Ucrt_cos(double x) noexcept;
double   Ucrt_sqrt(double x) noexcept;
double   Ucrt_pow(double x, double y) noexcept;

struct Horizon116UcrtbaseAuditReport {
    uint32_t ucrtbaseExportCount{0};
    bool     onExitTableDispatchVerified{false};
    bool     inittermExecutionVerified{false};
    bool     heapAllocReallocFreeVerified{false};
    bool     stdioFormatAndStreamsVerified{false};
    bool     mathAndErrnoVerified{false};
};

class WinNtUcrtbaseCoordinator {
public:
    static WinNtUcrtbaseCoordinator& instance() noexcept {
        static WinNtUcrtbaseCoordinator s_inst;
        return s_inst;
    }

    void initialize() noexcept;
    [[nodiscard]] bool isInitialized() const noexcept { return m_initialized; }

    [[nodiscard]] Horizon116UcrtbaseAuditReport runUcrtbaseAudit() noexcept;

private:
    WinNtUcrtbaseCoordinator() = default;
    bool m_initialized{false};
    Horizon116UcrtbaseAuditReport m_cachedReport{};
};

void InitializeWinNtUcrtbaseSubsystem() noexcept;

} // namespace micant::ucrt::winnt_ucrtbase_abi
