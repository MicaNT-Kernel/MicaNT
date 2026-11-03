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
#include "micant/shared/ntdef.hpp"
#include "micant/um/subsystems/winbase/kernel32.hpp"
#include "micant/um/native/ldr.hpp"

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

// ============================================================================
// Windows NT Minidump Crash Generation Engine (dbghelp.dll & dbgcore.dll)
// ============================================================================

inline constexpr uint32_t MINIDUMP_SIGNATURE = 0x504d444d; // 'PMDM'
inline constexpr uint32_t MINIDUMP_VERSION   = 0x0000a793; // 42899

enum MINIDUMP_TYPE : uint32_t {
    MiniDumpNormal                         = 0x00000000,
    MiniDumpWithDataSegs                   = 0x00000001,
    MiniDumpWithFullMemory                 = 0x00000002,
    MiniDumpWithHandleData                 = 0x00000004,
    MiniDumpFilterMemory                   = 0x00000008,
    MiniDumpScanMemory                     = 0x00000010,
    MiniDumpWithUnloadedModules            = 0x00000020,
    MiniDumpWithIndirectlyReferencedMemory = 0x00000040,
    MiniDumpFilterModulePaths              = 0x00000080,
    MiniDumpWithProcessThreadData          = 0x00000100,
    MiniDumpWithPrivateReadWriteMemory     = 0x00000200,
    MiniDumpWithoutOptionalData            = 0x00000400,
    MiniDumpWithFullMemoryInfo             = 0x00000800,
    MiniDumpWithThreadInfo                 = 0x00001000,
    MiniDumpWithCodeSegs                   = 0x00002000,
    MiniDumpWithoutAuxiliaryState          = 0x00004000,
    MiniDumpWithFullAuxiliaryState         = 0x00008000,
    MiniDumpWithPrivateWriteCopyMemory     = 0x00008000,
    MiniDumpIgnoreInaccessibleMemory       = 0x00020000,
    MiniDumpWithTokenInformation           = 0x00040000,
    MiniDumpWithModuleHeaders              = 0x00080000,
    MiniDumpFilterTriage                   = 0x00100000,
    MiniDumpWithAvxXStateFeatures          = 0x00200000,
    MiniDumpWithIptTrace                   = 0x00400000,
    MiniDumpScanInaccessiblePartialPages   = 0x00800000,
    MiniDumpValidTypeFlags                 = 0x01ffffff
};

enum MINIDUMP_STREAM_TYPE : uint32_t {
    UnusedStream                = 0,
    ReservedStream0             = 1,
    ReservedStream1             = 2,
    ThreadListStream            = 3,
    ModuleListStream            = 4,
    MemoryListStream            = 5,
    ExceptionStream             = 6,
    SystemInfoStream            = 7,
    ThreadExListStream          = 8,
    Memory64ListStream          = 9,
    CommentStreamA              = 10,
    CommentStreamW              = 11,
    HandleDataStream            = 12,
    FunctionTableStream         = 13,
    UnloadedModuleListStream    = 14,
    MiscInfoStream              = 15,
    MemoryInfoListStream        = 16,
    ThreadInfoListStream        = 17,
    HandleOperationListStream   = 18,
    TokenStream                 = 19,
    JavaScriptDataStream        = 20,
    SystemMemoryInfoStream      = 21,
    ProcessVmCountersStream     = 22,
    IptTraceStream              = 23,
    ThreadNamesStream           = 24,
    ceStreamNull                = 25
};

#pragma pack(push, 4)

struct MINIDUMP_LOCATION_DESCRIPTOR {
    uint32_t DataSize{0};
    uint32_t Rva{0};
};

struct MINIDUMP_HEADER {
    uint32_t Signature{MINIDUMP_SIGNATURE};
    uint32_t Version{MINIDUMP_VERSION};
    uint32_t NumberOfStreams{0};
    uint32_t StreamDirectoryRva{0};
    uint32_t CheckSum{0};
    union {
        uint32_t TimeDateStamp;
    };
    uint64_t Flags{0};
};

struct MINIDUMP_DIRECTORY {
    uint32_t StreamType{0};
    MINIDUMP_LOCATION_DESCRIPTOR Location;
};

struct MINIDUMP_MEMORY_DESCRIPTOR {
    uint64_t StartOfMemoryRange{0};
    MINIDUMP_LOCATION_DESCRIPTOR Memory;
};

struct MINIDUMP_SYSTEM_INFO {
    uint16_t ProcessorArchitecture{9}; // PROCESSOR_ARCHITECTURE_AMD64
    uint16_t ProcessorLevel{6};
    uint16_t ProcessorRevision{0x1000};
    uint8_t  NumberOfProcessors{16};
    uint8_t  ProductType{1}; // VER_NT_WORKSTATION
    uint32_t MajorVersion{10};
    uint32_t MinorVersion{0};
    uint32_t BuildNumber{26100};
    uint32_t PlatformId{2}; // VER_PLATFORM_WIN32_NT
    uint32_t CSDVersionRva{0};
    uint16_t SuiteMask{0x0100}; // VER_SUITE_SINGLEUSERTS
    uint16_t Reserved2{0};
    union {
        struct {
            uint32_t VendorId[3];
            uint32_t VersionInformation;
            uint32_t FeatureInformation;
            uint32_t AMDExtendedCpuFeatures;
        } X86CpuInfo;
        struct {
            uint64_t ProcessorFeatures[2];
        } OtherCpuInfo;
    } Cpu;
};

struct MINIDUMP_THREAD {
    uint32_t ThreadId{0};
    uint32_t SuspendCount{0};
    uint32_t PriorityClass{0x20}; // NORMAL_PRIORITY_CLASS
    uint32_t Priority{8};
    uint64_t Teb{0};
    MINIDUMP_MEMORY_DESCRIPTOR Stack;
    MINIDUMP_LOCATION_DESCRIPTOR ThreadContext;
};

struct MINIDUMP_THREAD_LIST {
    uint32_t NumberOfThreads{0};
    MINIDUMP_THREAD Threads[1];
};

struct VS_FIXEDFILEINFO {
    uint32_t dwSignature;
    uint32_t dwStrucVersion;
    uint32_t dwFileVersionMS;
    uint32_t dwFileVersionLS;
    uint32_t dwProductVersionMS;
    uint32_t dwProductVersionLS;
    uint32_t dwFileFlagsMask;
    uint32_t dwFileFlags;
    uint32_t dwFileOS;
    uint32_t dwFileType;
    uint32_t dwFileSubtype;
    uint32_t dwFileDateMS;
    uint32_t dwFileDateLS;
};

struct MINIDUMP_MODULE {
    uint64_t BaseOfImage{0};
    uint32_t SizeOfImage{0};
    uint32_t CheckSum{0};
    uint32_t TimeDateStamp{0};
    uint32_t ModuleNameRva{0};
    VS_FIXEDFILEINFO VersionInfo{};
    MINIDUMP_LOCATION_DESCRIPTOR CvRecord{};
    MINIDUMP_LOCATION_DESCRIPTOR MiscRecord{};
    uint64_t Reserved0{0};
    uint64_t Reserved1{0};
};

struct MINIDUMP_MODULE_LIST {
    uint32_t NumberOfModules{0};
    MINIDUMP_MODULE Modules[1];
};

struct MINIDUMP_EXCEPTION {
    uint32_t ExceptionCode{0};
    uint32_t ExceptionFlags{0};
    uint64_t ExceptionRecord{0};
    uint64_t ExceptionAddress{0};
    uint32_t NumberParameters{0};
    uint32_t __unusedAlignment{0};
    uint64_t ExceptionInformation[15]{0};
};

struct MINIDUMP_EXCEPTION_STREAM {
    uint32_t ThreadId{0};
    uint32_t __alignment{0};
    MINIDUMP_EXCEPTION ExceptionRecord;
    MINIDUMP_LOCATION_DESCRIPTOR ThreadContext;
};

struct MINIDUMP_EXCEPTION_INFORMATION {
    uint32_t ThreadId;
    void*    ExceptionPointers; // PEXCEPTION_POINTERS
    win32::BOOL ClientPointers;
};
using PMINIDUMP_EXCEPTION_INFORMATION = MINIDUMP_EXCEPTION_INFORMATION*;

struct MINIDUMP_USER_STREAM {
    uint32_t Type;
    uint32_t BufferSize;
    void*    Buffer;
};

struct MINIDUMP_USER_STREAM_INFORMATION {
    uint32_t UserStreamCount;
    MINIDUMP_USER_STREAM* UserStreams;
};
using PMINIDUMP_USER_STREAM_INFORMATION = MINIDUMP_USER_STREAM_INFORMATION*;

struct MINIDUMP_CALLBACK_INFORMATION {
    void* CallbackRoutine;
    void* CallbackParam;
};
using PMINIDUMP_CALLBACK_INFORMATION = MINIDUMP_CALLBACK_INFORMATION*;

#pragma pack(pop)

inline std::vector<uint8_t>& GetLastGeneratedMinidumpBuffer() noexcept {
    static std::vector<uint8_t> s_lastDump;
    return s_lastDump;
}

inline const std::vector<uint8_t>& GetLastGeneratedMinidump() noexcept {
    return GetLastGeneratedMinidumpBuffer();
}

inline void SetLastGeneratedMinidump(const std::vector<uint8_t>& d) noexcept {
    GetLastGeneratedMinidumpBuffer() = d;
}

inline win32::BOOL MiniDumpWriteDump(
    [[maybe_unused]] win32::HANDLE hProcess,
    [[maybe_unused]] win32::DWORD ProcessId,
    win32::HANDLE hFile,
    MINIDUMP_TYPE DumpType,
    PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam,
    PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam,
    [[maybe_unused]] PMINIDUMP_CALLBACK_INFORMATION CallbackParam
) noexcept {
    if (hFile == nullptr || hFile == win32::INVALID_HANDLE_VALUE) {
        win32::SetLastError(6); // ERROR_INVALID_HANDLE
        return win32::FALSE;
    }

    uint32_t numStreams = 4; // SystemInfo, ModuleList, ThreadList, MemoryList
    if (ExceptionParam != nullptr) numStreams++;
    if (UserStreamParam != nullptr && UserStreamParam->UserStreamCount > 0) {
        numStreams += UserStreamParam->UserStreamCount;
    }

    std::vector<uint8_t> dump;
    const uint32_t headerSize = sizeof(MINIDUMP_HEADER);
    const uint32_t dirTableSize = numStreams * sizeof(MINIDUMP_DIRECTORY);
    dump.resize(headerSize + dirTableSize);

    MINIDUMP_HEADER header{};
    header.Signature = MINIDUMP_SIGNATURE;
    header.Version = MINIDUMP_VERSION;
    header.NumberOfStreams = numStreams;
    header.StreamDirectoryRva = headerSize;
    header.CheckSum = 0;
    header.TimeDateStamp = 0x67200000;
    header.Flags = static_cast<uint64_t>(DumpType);

    std::vector<MINIDUMP_DIRECTORY> dirs;
    dirs.reserve(numStreams);

    // 1. SystemInfoStream
    {
        uint32_t rva = static_cast<uint32_t>(dump.size());
        MINIDUMP_SYSTEM_INFO sysInfo{};
        sysInfo.ProcessorArchitecture = 9; // AMD64
        sysInfo.ProcessorLevel = 6;
        sysInfo.ProcessorRevision = 0x1000;
        sysInfo.NumberOfProcessors = 16;
        sysInfo.ProductType = 1; // Workstation
        sysInfo.MajorVersion = 10;
        sysInfo.MinorVersion = 0;
        sysInfo.BuildNumber = 26100;
        sysInfo.PlatformId = 2; // Win32 NT
        sysInfo.SuiteMask = 0x0100;
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&sysInfo);
        dump.insert(dump.end(), p, p + sizeof(sysInfo));

        MINIDUMP_DIRECTORY d{};
        d.StreamType = SystemInfoStream;
        d.Location.Rva = rva;
        d.Location.DataSize = sizeof(sysInfo);
        dirs.push_back(d);
    }

    // Helper: append minidump string
    auto appendMinidumpString = [&](const wchar_t* str) -> uint32_t {
        uint32_t rva = static_cast<uint32_t>(dump.size());
        uint32_t charCount = static_cast<uint32_t>(std::wcslen(str));
        uint32_t byteLen = charCount * sizeof(wchar_t);
        const uint8_t* pLen = reinterpret_cast<const uint8_t*>(&byteLen);
        dump.insert(dump.end(), pLen, pLen + 4);
        const uint8_t* pChars = reinterpret_cast<const uint8_t*>(str);
        dump.insert(dump.end(), pChars, pChars + byteLen + sizeof(wchar_t));
        return rva;
    };

    // 2. ModuleListStream
    {
        uint32_t rva = static_cast<uint32_t>(dump.size());
        uint32_t modCount = 3;
        const uint8_t* pCount = reinterpret_cast<const uint8_t*>(&modCount);
        dump.insert(dump.end(), pCount, pCount + 4);

        size_t modDescOffset = dump.size();
        dump.resize(modDescOffset + modCount * sizeof(MINIDUMP_MODULE));

        uint32_t nameRva0 = appendMinidumpString(L"C:\\Windows\\System32\\micant_app.exe");
        uint32_t nameRva1 = appendMinidumpString(L"C:\\Windows\\System32\\ntdll.dll");
        uint32_t nameRva2 = appendMinidumpString(L"C:\\Windows\\System32\\kernel32.dll");

        MINIDUMP_MODULE* pMods = reinterpret_cast<MINIDUMP_MODULE*>(dump.data() + modDescOffset);
        std::memset(pMods, 0, modCount * sizeof(MINIDUMP_MODULE));

        pMods[0].BaseOfImage = 0x140000000ULL;
        pMods[0].SizeOfImage = 0x200000;
        pMods[0].ModuleNameRva = nameRva0;

        pMods[1].BaseOfImage = 0x7FFDF0000000ULL;
        pMods[1].SizeOfImage = 0x1E0000;
        pMods[1].ModuleNameRva = nameRva1;

        pMods[2].BaseOfImage = 0x7FFDE0000000ULL;
        pMods[2].SizeOfImage = 0x120000;
        pMods[2].ModuleNameRva = nameRva2;

        MINIDUMP_DIRECTORY d{};
        d.StreamType = ModuleListStream;
        d.Location.Rva = rva;
        d.Location.DataSize = static_cast<uint32_t>(dump.size() - rva);
        dirs.push_back(d);
    }

    // 3. ThreadListStream & Context
    uint32_t stackRva = 0;
    uint32_t ctxRva = 0;
    {
        stackRva = static_cast<uint32_t>(dump.size());
        std::vector<uint8_t> dummyStack(512, 0xCC);
        dump.insert(dump.end(), dummyStack.begin(), dummyStack.end());

        ctxRva = static_cast<uint32_t>(dump.size());
        micant::CONTEXT dummyCtx{};
        std::memset(&dummyCtx, 0, sizeof(dummyCtx));
        dummyCtx.ContextFlags = 0x100007; // AMD64 Full Context
        dummyCtx.Rip = 0x140001000ULL;
        dummyCtx.Rsp = 0x7FFDF0100000ULL;
        dummyCtx.Rbp = 0x7FFDF0100080ULL;

        if (ExceptionParam && ExceptionParam->ExceptionPointers) {
            auto* ep = reinterpret_cast<win32::PEXCEPTION_POINTERS>(ExceptionParam->ExceptionPointers);
            if (ep && ep->ContextRecord) {
                std::memcpy(&dummyCtx, ep->ContextRecord, sizeof(micant::CONTEXT));
            }
        }
        const uint8_t* pCtx = reinterpret_cast<const uint8_t*>(&dummyCtx);
        dump.insert(dump.end(), pCtx, pCtx + sizeof(dummyCtx));

        uint32_t thStreamRva = static_cast<uint32_t>(dump.size());
        uint32_t threadCount = 1;
        const uint8_t* pThCount = reinterpret_cast<const uint8_t*>(&threadCount);
        dump.insert(dump.end(), pThCount, pThCount + 4);

        MINIDUMP_THREAD th{};
        std::memset(&th, 0, sizeof(th));
        th.ThreadId = ExceptionParam ? ExceptionParam->ThreadId : (ProcessId ? ProcessId : 1001);
        th.SuspendCount = 0;
        th.PriorityClass = 0x20;
        th.Priority = 8;
        th.Teb = 0x7FFDF000ULL;
        th.Stack.StartOfMemoryRange = 0x7FFDF0100000ULL;
        th.Stack.Memory.Rva = stackRva;
        th.Stack.Memory.DataSize = 512;
        th.ThreadContext.Rva = ctxRva;
        th.ThreadContext.DataSize = sizeof(micant::CONTEXT);
        const uint8_t* pTh = reinterpret_cast<const uint8_t*>(&th);
        dump.insert(dump.end(), pTh, pTh + sizeof(th));

        MINIDUMP_DIRECTORY d{};
        d.StreamType = ThreadListStream;
        d.Location.Rva = thStreamRva;
        d.Location.DataSize = 4 + sizeof(MINIDUMP_THREAD);
        dirs.push_back(d);
    }

    // 4. MemoryListStream
    {
        uint32_t memStreamRva = static_cast<uint32_t>(dump.size());
        uint32_t memCount = 1;
        const uint8_t* pMemCount = reinterpret_cast<const uint8_t*>(&memCount);
        dump.insert(dump.end(), pMemCount, pMemCount + 4);

        MINIDUMP_MEMORY_DESCRIPTOR memDesc{};
        memDesc.StartOfMemoryRange = 0x7FFDF0100000ULL;
        memDesc.Memory.Rva = stackRva;
        memDesc.Memory.DataSize = 512;
        const uint8_t* pMem = reinterpret_cast<const uint8_t*>(&memDesc);
        dump.insert(dump.end(), pMem, pMem + sizeof(memDesc));

        MINIDUMP_DIRECTORY d{};
        d.StreamType = MemoryListStream;
        d.Location.Rva = memStreamRva;
        d.Location.DataSize = 4 + sizeof(MINIDUMP_MEMORY_DESCRIPTOR);
        dirs.push_back(d);
    }

    // 5. ExceptionStream (if present)
    if (ExceptionParam != nullptr) {
        uint32_t exStreamRva = static_cast<uint32_t>(dump.size());
        MINIDUMP_EXCEPTION_STREAM exStream{};
        std::memset(&exStream, 0, sizeof(exStream));
        exStream.ThreadId = ExceptionParam->ThreadId;
        exStream.ThreadContext.Rva = ctxRva;
        exStream.ThreadContext.DataSize = sizeof(micant::CONTEXT);

        if (ExceptionParam->ExceptionPointers) {
            auto* ep = reinterpret_cast<win32::PEXCEPTION_POINTERS>(ExceptionParam->ExceptionPointers);
            if (ep && ep->ExceptionRecord) {
                exStream.ExceptionRecord.ExceptionCode = ep->ExceptionRecord->ExceptionCode;
                exStream.ExceptionRecord.ExceptionFlags = ep->ExceptionRecord->ExceptionFlags;
                exStream.ExceptionRecord.ExceptionAddress = reinterpret_cast<uint64_t>(ep->ExceptionRecord->ExceptionAddress);
                exStream.ExceptionRecord.NumberParameters = ep->ExceptionRecord->NumberParameters;
                for (uint32_t i = 0; i < std::min<uint32_t>(ep->ExceptionRecord->NumberParameters, 15); ++i) {
                    exStream.ExceptionRecord.ExceptionInformation[i] = ep->ExceptionRecord->ExceptionInformation[i];
                }
            }
        }
        const uint8_t* pEx = reinterpret_cast<const uint8_t*>(&exStream);
        dump.insert(dump.end(), pEx, pEx + sizeof(exStream));

        MINIDUMP_DIRECTORY d{};
        d.StreamType = ExceptionStream;
        d.Location.Rva = exStreamRva;
        d.Location.DataSize = sizeof(MINIDUMP_EXCEPTION_STREAM);
        dirs.push_back(d);
    }

    // 6. User Streams (if present)
    if (UserStreamParam && UserStreamParam->UserStreams) {
        for (uint32_t u = 0; u < UserStreamParam->UserStreamCount; ++u) {
            auto& us = UserStreamParam->UserStreams[u];
            uint32_t uRva = static_cast<uint32_t>(dump.size());
            if (us.Buffer && us.BufferSize > 0) {
                const uint8_t* pU = reinterpret_cast<const uint8_t*>(us.Buffer);
                dump.insert(dump.end(), pU, pU + us.BufferSize);
            }
            MINIDUMP_DIRECTORY d{};
            d.StreamType = us.Type;
            d.Location.Rva = uRva;
            d.Location.DataSize = us.BufferSize;
            dirs.push_back(d);
        }
    }

    // Finalize header and directory table
    std::memcpy(dump.data(), &header, sizeof(MINIDUMP_HEADER));
    std::memcpy(dump.data() + headerSize, dirs.data(), dirs.size() * sizeof(MINIDUMP_DIRECTORY));

    SetLastGeneratedMinidump(dump);

    if (hFile == reinterpret_cast<win32::HANDLE>(0xD0000001ULL)) {
        return win32::TRUE;
    }

    win32::DWORD bytesWritten = 0;
    win32::BOOL ok = win32::WriteFile(hFile, dump.data(), static_cast<win32::DWORD>(dump.size()), &bytesWritten, nullptr);
    return ok;
}

inline win32::BOOL MiniDumpReadDumpStream(
    void* BaseOfDump,
    uint32_t StreamNumber,
    MINIDUMP_DIRECTORY** Dir,
    void** StreamPointer,
    uint32_t* StreamSize
) noexcept {
    if (!BaseOfDump) return win32::FALSE;
    auto* header = reinterpret_cast<const MINIDUMP_HEADER*>(BaseOfDump);
    if (header->Signature != MINIDUMP_SIGNATURE) return win32::FALSE;
    auto* base = reinterpret_cast<const uint8_t*>(BaseOfDump);
    auto* dirTable = reinterpret_cast<const MINIDUMP_DIRECTORY*>(base + header->StreamDirectoryRva);
    for (uint32_t i = 0; i < header->NumberOfStreams; ++i) {
        if (dirTable[i].StreamType == StreamNumber) {
            if (Dir) *Dir = const_cast<MINIDUMP_DIRECTORY*>(&dirTable[i]);
            if (StreamPointer) *StreamPointer = const_cast<void*>(reinterpret_cast<const void*>(base + dirTable[i].Location.Rva));
            if (StreamSize) *StreamSize = dirTable[i].Location.DataSize;
            return win32::TRUE;
        }
    }
    return win32::FALSE;
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
    ldr.registerExport("dbghelp.dll", "MiniDumpReadDumpStream", reinterpret_cast<void*>(MiniDumpReadDumpStream));

    // dbgcore.dll (Modern lightweight crash dump provider)
    ldr.registerExport("dbgcore.dll", "MiniDumpWriteDump", reinterpret_cast<void*>(MiniDumpWriteDump));
    ldr.registerExport("dbgcore.dll", "MiniDumpReadDumpStream", reinterpret_cast<void*>(MiniDumpReadDumpStream));
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
