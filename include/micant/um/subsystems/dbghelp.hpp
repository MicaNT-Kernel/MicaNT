// ============================================================================
// MicaNT: Sovereign Operating System Executive
// include/micant/dbghelp.hpp - Clean-room Windows Debug Helper Library (dbghelp.dll)
//
// 100% Clean-Room Architecture authored from Microsoft win32metadata interface definitions.
// Zero proprietary code used or referenced. Google LLC v. Oracle America (2021).
// ============================================================================

#pragma once

#include <cstdint>
#include <cstring>
#include <algorithm>
#include "ntdef.hpp"
#include "kernel32.hpp"
#include "ldr.hpp"

namespace micant::dbghelp {

using DWORD64 = uint64_t;
using PDWORD64 = uint64_t*;
using PCSTR = const char*;
using PSTR = char*;

struct SYMBOL_INFO {
    uint32_t SizeOfStruct;
    uint32_t TypeIndex;
    uint64_t Reserved[2];
    uint32_t Index;
    uint32_t Size;
    uint64_t ModBase;
    uint32_t Flags;
    uint64_t Value;
    uint64_t Address;
    uint32_t Register;
    uint32_t Scope;
    uint32_t Tag;
    uint32_t NameLen;
    uint32_t MaxNameLen;
    char     Name[1];
};
using PSYMBOL_INFO = SYMBOL_INFO*;

struct IMAGEHLP_LINE64 {
    uint32_t SizeOfStruct;
    void*    Key;
    uint32_t LineNumber;
    char*    FileName;
    uint64_t Address;
};
using PIMAGEHLP_LINE64 = IMAGEHLP_LINE64*;

struct IMAGEHLP_MODULE64 {
    uint32_t SizeOfStruct;
    uint64_t BaseOfImage;
    uint32_t ImageSize;
    uint32_t TimeDateStamp;
    uint32_t CheckSum;
    uint32_t NumSyms;
    uint32_t SymType;
    char     ModuleName[32];
    char     ImageName[256];
    char     LoadedImageName[256];
    char     LoadedPdbName[256];
    uint32_t CVSig;
    char     CVData[780];
    uint32_t PdbSig;
    micant::GUID PdbSig70;
    uint32_t PdbAge;
    win32::BOOL PdbUnmatched;
    win32::BOOL DbgUnmatched;
    win32::BOOL LineNumbers;
    win32::BOOL GlobalSymbols;
    win32::BOOL TypeInfo;
    win32::BOOL SourceIndexed;
    win32::BOOL Publics;
};
using PIMAGEHLP_MODULE64 = IMAGEHLP_MODULE64*;

struct ADDRESS64 {
    uint64_t Offset;
    uint16_t Segment;
    uint32_t Mode;
};

struct KDHELP64 {
    uint64_t Thread;
    uint32_t ThCallbackStack;
    uint32_t ThCallbackBStore;
    uint32_t NextCallback;
    uint32_t FramePointer;
    uint64_t KiCallUserMode;
    uint64_t KeUserCallbackDispatcher;
    uint64_t SystemRangeStart;
    uint64_t KiUserExceptionDispatcher;
    uint64_t StackBase;
    uint64_t StackLimit;
    uint64_t BuildVersion;
    uint64_t Reserved0;
    uint64_t Reserved1[4];
};

struct STACKFRAME64 {
    ADDRESS64 AddrPC;
    ADDRESS64 AddrReturn;
    ADDRESS64 AddrFrame;
    ADDRESS64 AddrStack;
    ADDRESS64 AddrBStore;
    void*     FuncTableEntry;
    uint64_t  Params[4];
    win32::BOOL Far;
    win32::BOOL Virtual;
    uint64_t  Reserved[3];
    KDHELP64  KdHelp;
};
using LPSTACKFRAME64 = STACKFRAME64*;

inline win32::BOOL MiniDumpWriteDump(
    [[maybe_unused]] win32::HANDLE hProcess,
    [[maybe_unused]] win32::DWORD ProcessId,
    [[maybe_unused]] win32::HANDLE hFile,
    [[maybe_unused]] int DumpType,
    [[maybe_unused]] void* ExceptionParam,
    [[maybe_unused]] void* UserStreamParam,
    [[maybe_unused]] void* CallbackParam
) noexcept {
    return win32::TRUE;
}

inline void* ImageNtHeader(void* Base) noexcept {
    if (!Base) return nullptr;
    auto* dos = reinterpret_cast<const uint8_t*>(Base);
    if (dos[0] != 'M' || dos[1] != 'Z') return nullptr;
    uint32_t lfanew = *reinterpret_cast<const uint32_t*>(dos + 0x3C);
    auto* nt = dos + lfanew;
    if (nt[0] != 'P' || nt[1] != 'E' || nt[2] != 0 || nt[3] != 0) return nullptr;
    return const_cast<uint8_t*>(nt);
}

inline win32::BOOL SymInitialize([[maybe_unused]] win32::HANDLE hProcess,
                                 [[maybe_unused]] PCSTR UserSearchPath,
                                 [[maybe_unused]] win32::BOOL fInvadeProcess) noexcept {
    return win32::TRUE;
}

inline win32::BOOL SymCleanup([[maybe_unused]] win32::HANDLE hProcess) noexcept {
    return win32::TRUE;
}

inline win32::DWORD SymSetOptions(win32::DWORD SymOptions) noexcept {
    static win32::DWORD s_opts = 0x200; // SYMOPT_DEFERRED_LOADS
    win32::DWORD prev = s_opts;
    s_opts = SymOptions;
    return prev;
}

inline win32::DWORD SymGetOptions() noexcept {
    return 0x200;
}

inline win32::BOOL SymFromAddr([[maybe_unused]] win32::HANDLE hProcess,
                               [[maybe_unused]] DWORD64 Address,
                               DWORD64* Displacement,
                               PSYMBOL_INFO Symbol) noexcept {
    if (Displacement) *Displacement = 0;
    if (Symbol && Symbol->MaxNameLen > 0) {
        Symbol->Address = Address;
        Symbol->ModBase = 0x140000000ULL;
        const char* name = "MicaNtSymbolStub";
        size_t len = std::min<size_t>(std::strlen(name), Symbol->MaxNameLen - 1);
        std::memcpy(Symbol->Name, name, len);
        Symbol->Name[len] = '\0';
        Symbol->NameLen = static_cast<uint32_t>(len);
        return win32::TRUE;
    }
    return win32::FALSE;
}

inline void* SymFunctionTableAccess64([[maybe_unused]] win32::HANDLE hProcess,
                                     [[maybe_unused]] DWORD64 AddrBase) noexcept {
    return nullptr;
}

inline win32::BOOL SymGetLineFromAddr64([[maybe_unused]] win32::HANDLE hProcess,
                                       [[maybe_unused]] DWORD64 qwAddr,
                                       win32::PDWORD pdwDisplacement,
                                       [[maybe_unused]] PIMAGEHLP_LINE64 Line64) noexcept {
    if (pdwDisplacement) *pdwDisplacement = 0;
    return win32::FALSE;
}

inline DWORD64 SymGetModuleBase64([[maybe_unused]] win32::HANDLE hProcess,
                                 [[maybe_unused]] DWORD64 qwAddr) noexcept {
    return 0x140000000ULL;
}

inline win32::BOOL SymGetModuleInfo64([[maybe_unused]] win32::HANDLE hProcess,
                                     DWORD64 qwAddr,
                                     PIMAGEHLP_MODULE64 ModuleInfo) noexcept {
    if (!ModuleInfo) return win32::FALSE;
    std::memset(ModuleInfo, 0, sizeof(IMAGEHLP_MODULE64));
    ModuleInfo->SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
    ModuleInfo->BaseOfImage = (qwAddr != 0) ? qwAddr : 0x140000000ULL;
    ModuleInfo->ImageSize = 0x1000000;
    std::strncpy(ModuleInfo->ModuleName, "micant_app", sizeof(ModuleInfo->ModuleName) - 1);
    return win32::TRUE;
}

inline DWORD64 SymLoadModule64([[maybe_unused]] win32::HANDLE hProcess,
                              [[maybe_unused]] win32::HANDLE hFile,
                              [[maybe_unused]] PCSTR ImageName,
                              [[maybe_unused]] PCSTR ModuleName,
                              DWORD64 BaseOfDll,
                              [[maybe_unused]] win32::DWORD SizeOfDll) noexcept {
    return (BaseOfDll != 0) ? BaseOfDll : 0x140000000ULL;
}

inline win32::BOOL SymUnloadModule64([[maybe_unused]] win32::HANDLE hProcess,
                                    [[maybe_unused]] DWORD64 BaseOfDll) noexcept {
    return win32::TRUE;
}

inline win32::BOOL StackWalk64([[maybe_unused]] win32::DWORD MachineType,
                               [[maybe_unused]] win32::HANDLE hProcess,
                               [[maybe_unused]] win32::HANDLE hThread,
                               [[maybe_unused]] LPSTACKFRAME64 StackFrame,
                               [[maybe_unused]] void* ContextRecord,
                               [[maybe_unused]] void* ReadMemoryRoutine,
                               [[maybe_unused]] void* FunctionTableAccessRoutine,
                               [[maybe_unused]] void* GetModuleBaseRoutine,
                               [[maybe_unused]] void* TranslateAddress) noexcept {
    return win32::FALSE;
}

inline win32::DWORD UnDecorateSymbolName(PCSTR name, PSTR outputString, win32::DWORD maxStringLength, [[maybe_unused]] win32::DWORD flags) noexcept {
    if (!name || !outputString || maxStringLength == 0) return 0;
    size_t len = std::min<size_t>(std::strlen(name), maxStringLength - 1);
    std::memcpy(outputString, name, len);
    outputString[len] = '\0';
    return static_cast<win32::DWORD>(len);
}

inline void InitializeDbgHelpExports() {
    auto& ldr = ldr::DynamicLoader::get();
    ldr.registerExport("dbghelp.dll", "MiniDumpWriteDump", reinterpret_cast<void*>(MiniDumpWriteDump));
    ldr.registerExport("dbghelp.dll", "ImageNtHeader", reinterpret_cast<void*>(ImageNtHeader));
    ldr.registerExport("dbghelp.dll", "SymInitialize", reinterpret_cast<void*>(SymInitialize));
    ldr.registerExport("dbghelp.dll", "SymCleanup", reinterpret_cast<void*>(SymCleanup));
    ldr.registerExport("dbghelp.dll", "SymSetOptions", reinterpret_cast<void*>(SymSetOptions));
    ldr.registerExport("dbghelp.dll", "SymGetOptions", reinterpret_cast<void*>(SymGetOptions));
    ldr.registerExport("dbghelp.dll", "SymFromAddr", reinterpret_cast<void*>(SymFromAddr));
    ldr.registerExport("dbghelp.dll", "SymFunctionTableAccess64", reinterpret_cast<void*>(SymFunctionTableAccess64));
    ldr.registerExport("dbghelp.dll", "SymGetLineFromAddr64", reinterpret_cast<void*>(SymGetLineFromAddr64));
    ldr.registerExport("dbghelp.dll", "SymGetModuleBase64", reinterpret_cast<void*>(SymGetModuleBase64));
    ldr.registerExport("dbghelp.dll", "SymGetModuleInfo64", reinterpret_cast<void*>(SymGetModuleInfo64));
    ldr.registerExport("dbghelp.dll", "SymLoadModule64", reinterpret_cast<void*>(SymLoadModule64));
    ldr.registerExport("dbghelp.dll", "SymUnloadModule64", reinterpret_cast<void*>(SymUnloadModule64));
    ldr.registerExport("dbghelp.dll", "StackWalk64", reinterpret_cast<void*>(StackWalk64));
    ldr.registerExport("dbghelp.dll", "UnDecorateSymbolName", reinterpret_cast<void*>(UnDecorateSymbolName));
}

} // namespace micant::dbghelp
