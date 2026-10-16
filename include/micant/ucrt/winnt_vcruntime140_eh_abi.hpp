#pragma once
/**
 * @file winnt_vcruntime140_eh_abi.hpp
 * @brief Horizon 117 (Kernel Unit #89): Sovereign WinNT MSVC Exception Handling Runtimes
 *        (vcruntime140.dll & vcruntime140_1.dll) Official PE32+ Exports, __C_specific_handler
 *        Structured Exception Handling, and __CxxFrameHandler4 x64 C++ EH State Unwind ABI.
 */

#include <cstdint>
#include <cstddef>
#include <cstring>

#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"

namespace micant::ucrt::winnt_vcruntime140_abi {

struct WinNtDispatcherContext {
    uint64_t ControlPc{0};
    uint64_t ImageBase{0};
    void*    FunctionEntry{nullptr};
    uint64_t EstablisherFrame{0};
    uint64_t TargetIp{0};
    void*    ContextRecord{nullptr};
    void*    LanguageHandler{nullptr};
    void*    HandlerData{nullptr};
    void*    HistoryTable{nullptr};
    uint32_t ScopeIndex{0};
    uint32_t ControlPcIsUnwound{0};
    void*    NonVolatileRegisters{nullptr};
};

struct WinNtExceptionRecord {
    uint32_t              ExceptionCode{0};
    uint32_t              ExceptionFlags{0};
    WinNtExceptionRecord* ExceptionRecord{nullptr};
    void*                 ExceptionAddress{nullptr};
    uint32_t              NumberParameters{0};
    uintptr_t             ExceptionInformation[15]{};
};

// --- vcruntime140.dll official exports (12 functions) ---
int32_t Vc_C_specific_handler(WinNtExceptionRecord* ExceptionRecord, void* EstablisherFrame, void* ContextRecord, WinNtDispatcherContext* DispatcherContext) noexcept;
void    Vc_CxxThrowException(void* pExceptionObject, void* pThrowInfo) noexcept;
void    Vc_telemetry_main_invoke_trigger(void* reserved) noexcept;
void    Vc_telemetry_main_return_trigger(void* reserved) noexcept;
void    Vc_std_type_info_destroy_list(void* pNode) noexcept;
int32_t Vc_setjmp(void* env) noexcept;
void    Vc_longjmp(void* env, int32_t val) noexcept;
void*   Vc_memcpy(void* dest, const void* src, size_t count) noexcept;
void*   Vc_memmove(void* dest, const void* src, size_t count) noexcept;
void*   Vc_memset(void* dest, int32_t c, size_t count) noexcept;
int32_t Vc_memcmp(const void* buf1, const void* buf2, size_t count) noexcept;
char*   Vc_strstr(const char* str, const char* strSearch) noexcept;

// --- vcruntime140_1.dll official export (1 function) ---
int32_t Vc1_CxxFrameHandler4(WinNtExceptionRecord* ExceptionRecord, void* EstablisherFrame, void* ContextRecord, WinNtDispatcherContext* DispatcherContext) noexcept;

struct Horizon117Vcruntime140AuditReport {
    uint32_t vcruntime140ExportCount{0};
    uint32_t vcruntime140_1ExportCount{0};
    bool     cSpecificHandlerVerified{false};
    bool     cxxFrameHandler4Verified{false};
    bool     memoryAndStringPrimitivesVerified{false};
    bool     telemetryTriggersVerified{false};
};

class WinNtVcruntime140Coordinator {
public:
    static WinNtVcruntime140Coordinator& instance() noexcept {
        static WinNtVcruntime140Coordinator s_inst;
        return s_inst;
    }

    void initialize() noexcept;
    [[nodiscard]] bool isInitialized() const noexcept { return m_initialized; }

    [[nodiscard]] Horizon117Vcruntime140AuditReport runVcruntimeAudit() noexcept;

private:
    WinNtVcruntime140Coordinator() = default;
    bool m_initialized{false};
    Horizon117Vcruntime140AuditReport m_cachedReport{};
};

void InitializeWinNtVcruntime140Subsystem() noexcept;

} // namespace micant::ucrt::winnt_vcruntime140_abi
