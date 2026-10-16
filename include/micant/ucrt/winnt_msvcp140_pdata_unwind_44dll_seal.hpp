#pragma once
/**
 * @file winnt_msvcp140_pdata_unwind_44dll_seal.hpp
 * @brief Horizon 118 (Kernel Unit #90): Sovereign WinNT C++ Standard Library (msvcp140.dll),
 *        PE32+ .pdata / .xdata Runtime Function Table & x64 SEH Virtual Stack Unwinder,
 *        and Complete 44-DLL System32 Ecosystem Seal (24 WinNT Core/CRT + 20 PrismX DirectX).
 */

#include <cstdint>
#include <cstddef>
#include <cstring>

#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/um/subsystems/winuser/winnt_shell32_dbghelp_version_40dll_iat_seal.hpp"
#include "micant/ucrt/winnt_ucrtbase_runtime_abi.hpp"
#include "micant/ucrt/winnt_vcruntime140_eh_abi.hpp"

namespace micant::ucrt::winnt_msvcp140_pdata_seal {

// x64 PE32+ .pdata (IMAGE_RUNTIME_FUNCTION_ENTRY)
struct WinNtRuntimeFunction {
    uint32_t BeginAddress{0};
    uint32_t EndAddress{0};
    uint32_t UnwindData{0};
};

// x64 PE32+ .xdata (UNWIND_INFO)
struct WinNtUnwindCode {
    uint8_t CodeOffset{0};
    uint8_t UnwindOp : 4 {0};
    uint8_t OpInfo   : 4 {0};
    uint16_t FrameOffset{0};
};

struct WinNtUnwindInfo {
    uint8_t Version : 3 {1};
    uint8_t Flags   : 5 {0};
    uint8_t SizeOfProlog{0};
    uint8_t CountOfCodes{0};
    uint8_t FrameRegister : 4 {0};
    uint8_t FrameOffset   : 4 {0};
    WinNtUnwindCode UnwindCodes[4]{};
};

struct WinNtContext64 {
    uint64_t Rip{0};
    uint64_t Rsp{0};
    uint64_t Rbp{0};
    uint64_t Rbx{0};
    uint64_t Rsi{0};
    uint64_t Rdi{0};
    uint64_t R12{0};
    uint64_t R13{0};
    uint64_t R14{0};
    uint64_t R15{0};
};

// --- msvcp140.dll official exports (7 functions) ---
void        Msvcp_Xlength_error(const char* message) noexcept;
void        Msvcp_Xout_of_range(const char* message) noexcept;
void        Msvcp_Xbad_alloc() noexcept;
void        Msvcp_Xbad_function_call() noexcept;
const char* Msvcp_Syserror_map(int32_t err) noexcept;
const char* Msvcp_Winerror_map(int32_t err) noexcept;
void        Msvcp_Throw_Cpp_error(int32_t code) noexcept;

// --- PE32+ .pdata / x64 SEH Stack Unwinder ---
[[nodiscard]] const WinNtRuntimeFunction* RtlLookupFunctionEntry(uint64_t ControlPc, uint64_t* ImageBase) noexcept;
[[nodiscard]] NTSTATUS RtlVirtualUnwind(uint64_t ImageBase, uint64_t ControlPc, const WinNtRuntimeFunction* FunctionEntry, WinNtContext64* ContextRecord, void** HandlerData, uint64_t* EstablisherFrame) noexcept;
int32_t RtlAddFunctionTable(WinNtRuntimeFunction* FunctionTable, uint32_t EntryCount, uint64_t BaseAddress) noexcept;
int32_t RtlDeleteFunctionTable(WinNtRuntimeFunction* FunctionTable) noexcept;

struct Horizon118Complete44DllSystem32SealReport {
    uint32_t msvcpExportCount{0};
    uint32_t winntCoreAndCrtSystem32DllCount{0}; // 24: 20 WinNT Core + 4 UCRT/MSVC
    uint32_t prismxDirectXSystem32DllCount{0};   // 20 DirectX/Media
    uint32_t totalSystem32PeDllCount{0};         // 44 total PE32+ .dll binaries
    uint32_t totalWinNtAndCrtExports{0};         // 217 + 26 + 13 + 7 = 263
    uint32_t totalHorizons{118};
    uint32_t totalKernelUnits{90};
    uint32_t totalTestSuites{334};
    uint32_t prismxStandaloneSuites{41};
    bool     msvcpExceptionMappingVerified{false};
    bool     pdataRuntimeFunctionLookupVerified{false};
    bool     virtualStackUnwindVerified{false};
    bool     all44System32DllsSealed{false};
};

class WinNtMsvcp140PdataSealCoordinator {
public:
    static WinNtMsvcp140PdataSealCoordinator& instance() noexcept {
        static WinNtMsvcp140PdataSealCoordinator s_inst;
        return s_inst;
    }

    void initialize() noexcept;
    [[nodiscard]] bool isInitialized() const noexcept { return m_initialized; }

    [[nodiscard]] Horizon118Complete44DllSystem32SealReport runComplete44DllSealAudit() noexcept;

private:
    WinNtMsvcp140PdataSealCoordinator() = default;
    bool m_initialized{false};
    Horizon118Complete44DllSystem32SealReport m_cachedReport{};
};

void InitializeWinNtMsvcp140PdataSealSubsystem() noexcept;

} // namespace micant::ucrt::winnt_msvcp140_pdata_seal
