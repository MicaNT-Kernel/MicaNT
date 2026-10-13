#pragma once
/**
 * @file dbgk.hpp
 * @brief Horizon 44: Sovereign NT User-Mode Debugging Executive (Dbgk* — base/ntos/dbgk),
 *        DEBUG_OBJECT Kernel Architecture, DbgUi* NTDLL Bridge & Win32 Debugging Engine.
 *
 * Implements the complete clean-room Windows NT User-Mode Debugging Executive (base/ntos/dbgk):
 *   1. Executive Debug Object Architecture (DEBUG_OBJECT / DbgkDebugObjectType):
 *      - NtCreateDebugObject, NtDebugActiveProcess, NtRemoveProcessDebug,
 *        NtWaitForDebugEvent, NtDebugContinue, NtSetInformationDebugObject.
 *      - DEBUG_KILL_ON_CLOSE (DEBUG_OBJECT_KILL_ON_CLOSE) lifecycle management:
 *        terminates or cleanly detaches all debugged processes when the debugger closes its
 *        DEBUG_OBJECT handle.
 *   2. Kernel-to-Debugger State Change Event Pipeline (DBGKM_APINUMBER -> DBG_STATE):
 *      - DbgkCreateThread (synthesizes DbgKmCreateProcessApi for initial thread,
 *        DbgKmCreateThreadApi for subsequent threads).
 *      - DbgkExitThread (DbgKmExitThreadApi) & DbgkExitProcess (DbgKmExitProcessApi).
 *      - DbgkMapViewOfSection (DbgKmLoadDllApi) & DbgkUnMapViewOfSection (DbgKmUnloadDllApi).
 *      - DbgkForwardException (DbgKmExceptionApi with FirstChance / SecondChance routing
 *        for STATUS_BREAKPOINT, STATUS_SINGLE_STEP, STATUS_ACCESS_VIOLATION, etc.).
 *   3. Initial Process Attach State Synthesis:
 *      - Attaching to a live process via NtDebugActiveProcess sets PEB.BeingDebugged = 1,
 *        binds the target process's DebugPort, and synthesizes initial CreateProcess,
 *        LoadDll (ntdll.dll, kernel32.dll, etc.), and CreateThread state change events.
 *   4. NTDLL DbgUi* & Win32 kernel32.dll Debugging Bridge:
 *      - DbgUiConnectToDbg, DbgUiGetThreadDebugObject, DbgUiSetThreadDebugObject,
 *        DbgUiWaitStateChange, DbgUiContinue, DbgUiDebugActiveProcess, DbgUiStopDebugging,
 *        DbgUiConvertStateChangeStructure (translates DBGUI_WAIT_STATE_CHANGE -> WIN32_DEBUG_EVENT).
 *      - Win32 DebugActiveProcess, DebugActiveProcessStop, WaitForDebugEvent,
 *        ContinueDebugEvent, DebugSetProcessKillOnExit, CheckRemoteDebuggerPresent.
 *
 * Clean-room ISO C++23 implementation. Zero proprietary or decompiled code.
 */

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <deque>
#include <unordered_map>
#include <mutex>
#include <memory>
#include <algorithm>

#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/um/native/janusldr.hpp"
#include "micant/um/servers/scm.hpp"
#include "micant/um/subsystems/winbase/version.hpp"

namespace micant::km::dbgk {

using PVOID    = void*;
using ULONG    = uint32_t;
using USHORT   = uint16_t;
using UCHAR    = uint8_t;
using BOOLEAN  = uint8_t;
using LONGLONG = int64_t;
using ULONGLONG = uint64_t;
using LARGE_INTEGER  = micant::LargeInteger;
using PLARGE_INTEGER = micant::LargeInteger*;

inline constexpr BOOLEAN FALSE_B = 0;
inline constexpr BOOLEAN TRUE_B  = 1;

// ============================================================================
// 1. DEBUG_OBJECT Access Masks, Flags & Information Classes
// ============================================================================

inline constexpr uint32_t DEBUG_READ_EVENT        = 0x0001;
inline constexpr uint32_t DEBUG_PROCESS_ASSIGN    = 0x0002;
inline constexpr uint32_t DEBUG_SET_INFORMATION   = 0x0004;
inline constexpr uint32_t DEBUG_QUERY_INFORMATION = 0x0008;
inline constexpr uint32_t DEBUG_ALL_ACCESS        = 0x001F000F;

// NtCreateDebugObject Flags
inline constexpr uint32_t DEBUG_KILL_ON_CLOSE     = 0x00000001;

// Internal DEBUG_OBJECT->Flags
inline constexpr uint32_t DEBUG_OBJECT_DELETE_PENDING = 0x00000001;
inline constexpr uint32_t DEBUG_OBJECT_KILL_ON_CLOSE  = 0x00000002;

enum DEBUGOBJECTINFOCLASS : uint32_t {
    DebugObjectUnusedInformation            = 0,
    DebugObjectKillProcessOnExitInformation = 1,
    DebugObjectMaxInfoClass                 = 2
};

// ============================================================================
// 2. DBGKM_APINUMBER, DBG_STATE & Exception / Event Payloads
// ============================================================================

enum DBGKM_APINUMBER : uint32_t {
    DbgKmExceptionApi     = 0,
    DbgKmCreateThreadApi  = 1,
    DbgKmCreateProcessApi = 2,
    DbgKmExitThreadApi    = 3,
    DbgKmExitProcessApi   = 4,
    DbgKmLoadDllApi       = 5,
    DbgKmUnloadDllApi     = 6,
    DbgKmMaxApiNumber     = 7
};

enum DBG_STATE : uint32_t {
    DbgIdle                     = 0,
    DbgReplyPending             = 1,
    DbgCreateThreadStateChange  = 2,
    DbgCreateProcessStateChange = 3,
    DbgExitThreadStateChange    = 4,
    DbgExitProcessStateChange   = 5,
    DbgExceptionStateChange     = 6,
    DbgBreakpointStateChange    = 7,
    DbgSingleStepStateChange    = 8,
    DbgLoadDllStateChange       = 9,
    DbgUnloadDllStateChange     = 10
};

struct DBG_CLIENT_ID {
    uint64_t UniqueProcess{0};
    uint64_t UniqueThread{0};
};

inline constexpr uint32_t DBG_EXCEPTION_MAXIMUM_PARAMETERS = 15;

struct DBGKM_EXCEPTION_RECORD64 {
    NtStatus ExceptionCode{NtStatus::Success};
    uint32_t ExceptionFlags{0};
    uint64_t ExceptionRecord{0};
    uint64_t ExceptionAddress{0};
    uint32_t NumberParameters{0};
    uint32_t UnusedAlignment{0};
    uint64_t ExceptionInformation[DBG_EXCEPTION_MAXIMUM_PARAMETERS]{0};
};

struct DBGKM_EXCEPTION {
    DBGKM_EXCEPTION_RECORD64 ExceptionRecord{};
    uint32_t                 FirstChance{1};
};

struct DBGKM_CREATE_THREAD {
    uint32_t SubSystemKey{0};
    PVOID    StartAddress{nullptr};
};

struct DBGKM_CREATE_PROCESS {
    uint32_t            SubSystemKey{0};
    Handle              FileHandle{0};
    PVOID               BaseOfImage{nullptr};
    uint32_t            DebugInfoFileOffset{0};
    uint32_t            DebugInfoSize{0};
    DBGKM_CREATE_THREAD InitialThread{};
};

struct DBGKM_EXIT_THREAD {
    NtStatus ExitStatus{NtStatus::Success};
};

struct DBGKM_EXIT_PROCESS {
    NtStatus ExitStatus{NtStatus::Success};
};

struct DBGKM_LOAD_DLL {
    Handle   FileHandle{0};
    PVOID    BaseOfDll{nullptr};
    uint32_t DebugInfoFileOffset{0};
    uint32_t DebugInfoSize{0};
    PVOID    NamePointer{nullptr};
};

struct DBGKM_UNLOAD_DLL {
    PVOID BaseAddress{nullptr};
};

// ============================================================================
// 3. DBGUI_WAIT_STATE_CHANGE (Returned by NtWaitForDebugEvent / DbgUiWaitStateChange)
// ============================================================================

struct DBGUI_CREATE_THREAD {
    Handle              HandleToThread{0};
    DBGKM_CREATE_THREAD NewThread{};
};

struct DBGUI_CREATE_PROCESS {
    Handle               HandleToProcess{0};
    Handle               HandleToThread{0};
    DBGKM_CREATE_PROCESS NewProcess{};
};

struct DBGUI_WAIT_STATE_CHANGE {
    DBG_STATE     NewState{DbgIdle};
    DBG_CLIENT_ID AppClientId{};
    union {
        DBGKM_EXCEPTION      Exception;
        DBGUI_CREATE_THREAD  CreateThread;
        DBGUI_CREATE_PROCESS CreateProcessInfo;
        DBGKM_EXIT_THREAD    ExitThread;
        DBGKM_EXIT_PROCESS   ExitProcess;
        DBGKM_LOAD_DLL       LoadDll;
        DBGKM_UNLOAD_DLL     UnloadDll;
    } StateInfo{};

    DBGUI_WAIT_STATE_CHANGE() noexcept {
        std::memset(&StateInfo, 0, sizeof(StateInfo));
    }
};

// ============================================================================
// 4. Win32 DEBUG_EVENT (Translated by DbgUiConvertStateChangeStructure)
// ============================================================================

inline constexpr uint32_t WIN32_EXCEPTION_DEBUG_EVENT      = 1;
inline constexpr uint32_t WIN32_CREATE_THREAD_DEBUG_EVENT  = 2;
inline constexpr uint32_t WIN32_CREATE_PROCESS_DEBUG_EVENT = 3;
inline constexpr uint32_t WIN32_EXIT_THREAD_DEBUG_EVENT    = 4;
inline constexpr uint32_t WIN32_EXIT_PROCESS_DEBUG_EVENT   = 5;
inline constexpr uint32_t WIN32_LOAD_DLL_DEBUG_EVENT       = 6;
inline constexpr uint32_t WIN32_UNLOAD_DLL_DEBUG_EVENT     = 7;
inline constexpr uint32_t WIN32_OUTPUT_DEBUG_STRING_EVENT  = 8;
inline constexpr uint32_t WIN32_RIP_EVENT                  = 9;

struct WIN32_DEBUG_EVENT {
    uint32_t dwDebugEventCode{0};
    uint32_t dwProcessId{0};
    uint32_t dwThreadId{0};
    union {
        DBGKM_EXCEPTION      Exception;
        DBGUI_CREATE_THREAD  CreateThread;
        DBGUI_CREATE_PROCESS CreateProcessInfo;
        DBGKM_EXIT_THREAD    ExitThread;
        DBGKM_EXIT_PROCESS   ExitProcess;
        DBGKM_LOAD_DLL       LoadDll;
        DBGKM_UNLOAD_DLL     UnloadDll;
    } u{};

    WIN32_DEBUG_EVENT() noexcept {
        std::memset(&u, 0, sizeof(u));
    }
};

// ============================================================================
// 5. Internal DEBUG_EVENT_RECORD, Debugged Process State & DEBUG_OBJECT
// ============================================================================

inline constexpr uint32_t DEBUG_EVENT_READ            = 0x00000001;
inline constexpr uint32_t DEBUG_EVENT_NOWAIT          = 0x00000002;
inline constexpr uint32_t DEBUG_EVENT_INACTIVE        = 0x00000004;
inline constexpr uint32_t DEBUG_EVENT_RELEASE         = 0x00000008;
inline constexpr uint32_t DEBUG_EVENT_PROTECT_FAILED  = 0x00000010;
inline constexpr uint32_t DEBUG_EVENT_SUSPEND         = 0x00000020;

struct DEBUG_EVENT_RECORD {
    uint64_t                SequenceId{0};
    DBG_CLIENT_ID           ClientId{};
    DBGKM_APINUMBER         ApiNumber{DbgKmExceptionApi};
    uint32_t                Flags{0};
    NtStatus                Status{NtStatus::Pending};
    bool                    ReadByDebugger{false};
    bool                    Completed{false};
    std::wstring            ModuleName{};
    DBGUI_WAIT_STATE_CHANGE WaitStateChange{};
};

struct DEBUGGED_MODULE_ENTRY {
    PVOID        BaseOfDll{nullptr};
    uint32_t     DebugInfoFileOffset{0};
    uint32_t     DebugInfoSize{0};
    std::wstring ModuleName{};
};

struct DEBUGGED_THREAD_ENTRY {
    uint64_t ThreadId{0};
    PVOID    StartAddress{nullptr};
    bool     InitialThreadReported{false};
    bool     SuspendedForDebugEvent{false};
    NtStatus LastContinueStatus{NtStatus::Success};
};

struct DEBUGGED_PROCESS_ENTRY {
    uint64_t                           ProcessId{0};
    Handle                             ProcessHandle{0};
    Handle                             DebugObjectHandle{0};
    PVOID                              ImageBase{nullptr};
    std::wstring                       ImageFileName{L"\\SystemRoot\\System32\\target.exe"};
    bool                               BeingDebugged{false};
    bool                               InitialAttachSent{false};
    bool                               Terminated{false};
    bool                               TerminatedByKillOnClose{false};
    NtStatus                           ExitStatus{NtStatus::Success};
    std::vector<DEBUGGED_THREAD_ENTRY> Threads{};
    std::vector<DEBUGGED_MODULE_ENTRY> LoadedModules{};
};

struct DEBUG_OBJECT {
    uint16_t                       Type{0x00D0 /* OB_TYPE_DEBUG_OBJECT */};
    uint16_t                       Size{sizeof(DEBUG_OBJECT)};
    Handle                         DebugObjectHandle{0};
    uint32_t                       Flags{0};
    uint32_t                       DesiredAccess{DEBUG_ALL_ACCESS};
    bool                           EventsPresentSignaled{false};
    std::deque<DEBUG_EVENT_RECORD> EventList{};
    std::vector<uint64_t>          AttachedProcessIds{};
};

// ============================================================================
// 6. Dbgk Executive Telemetry
// ============================================================================

struct DBGK_TELEMETRY {
    uint64_t TotalDebugObjectsCreated{0};
    uint64_t TotalDebugObjectsClosed{0};
    uint64_t TotalProcessesAttached{0};
    uint64_t TotalProcessesDetached{0};
    uint64_t TotalProcessesKilledOnClose{0};
    uint64_t TotalEventsQueued{0};
    uint64_t TotalEventsWaited{0};
    uint64_t TotalEventsContinued{0};
    uint64_t TotalExceptionsForwarded{0};
    uint64_t TotalBreakpointTraps{0};
    uint64_t TotalSingleStepTraps{0};
    uint64_t TotalDllLoadEvents{0};
    uint64_t TotalDllUnloadEvents{0};
    uint64_t TotalThreadCreateEvents{0};
    uint64_t TotalThreadExitEvents{0};
    uint64_t TotalProcessExitEvents{0};
};

// ============================================================================
// 7. Sovereign NT User-Mode Debugging Executive Engine (TitanDbgkManager)
// ============================================================================

class TitanDbgkManager {
public:
    static TitanDbgkManager& Instance() noexcept {
        static TitanDbgkManager s_instance;
        return s_instance;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_debugObjects.clear();
        m_processes.clear();
        m_handleToPid.clear();
        m_threadDebugObject = 0;
        m_nextHandle        = 0xDB00;
        m_nextSeqId         = 1;
        m_telemetry         = DBGK_TELEMETRY{};
    }

    // ------------------------------------------------------------------------
    // Target Process Registration Helper (for attaching by Handle or PID)
    // ------------------------------------------------------------------------

    Handle registerTargetProcess(
        uint64_t       processId,
        uint64_t       initialThreadId,
        PVOID          imageBase,
        PVOID          initialStartAddress,
        const wchar_t* imageName = L"\\Device\\HarddiskVolume1\\Windows\\System32\\notepad.exe"
    ) {
        std::lock_guard<std::mutex> lock(m_mutex);
        Handle hProc = allocateHandleUnlocked();

        DEBUGGED_PROCESS_ENTRY entry{};
        entry.ProcessId     = processId;
        entry.ProcessHandle = hProc;
        entry.ImageBase     = imageBase ? imageBase : reinterpret_cast<PVOID>(0x00007FF600000000ULL);
        entry.ImageFileName = imageName ? imageName : L"\\Device\\HarddiskVolume1\\Windows\\System32\\notepad.exe";

        DEBUGGED_THREAD_ENTRY mainThread{};
        mainThread.ThreadId     = initialThreadId;
        mainThread.StartAddress = initialStartAddress ? initialStartAddress : reinterpret_cast<PVOID>(0x00007FF600001000ULL);
        entry.Threads.push_back(mainThread);

        // Standard initial system DLLs mapped in every NT process
        entry.LoadedModules.push_back({
            reinterpret_cast<PVOID>(0x00007FFB80000000ULL),
            0,
            0,
            L"\\SystemRoot\\System32\\ntdll.dll"
        });
        entry.LoadedModules.push_back({
            reinterpret_cast<PVOID>(0x00007FFB7F000000ULL),
            0,
            0,
            L"\\SystemRoot\\System32\\kernel32.dll"
        });

        m_processes[processId]  = std::move(entry);
        m_handleToPid[hProc]    = processId;
        return hProc;
    }

    // ------------------------------------------------------------------------
    // 7.1 NtCreateDebugObject / NtSetInformationDebugObject / Close
    // ------------------------------------------------------------------------

    NtStatus createDebugObject(
        Handle*           outDebugObjectHandle,
        uint32_t          desiredAccess,
        ObjectAttributes* objectAttributes,
        uint32_t          flags
    ) {
        (void)objectAttributes;
        if (!outDebugObjectHandle) return NtStatus::InvalidParameter;
        if ((flags & ~DEBUG_KILL_ON_CLOSE) != 0) return NtStatus::InvalidParameter;

        std::lock_guard<std::mutex> lock(m_mutex);
        Handle hDbg = allocateHandleUnlocked();

        auto obj = std::make_unique<DEBUG_OBJECT>();
        obj->DebugObjectHandle = hDbg;
        obj->DesiredAccess     = desiredAccess ? desiredAccess : DEBUG_ALL_ACCESS;
        if ((flags & DEBUG_KILL_ON_CLOSE) != 0) {
            obj->Flags |= DEBUG_OBJECT_KILL_ON_CLOSE;
        }

        m_debugObjects[hDbg] = std::move(obj);
        *outDebugObjectHandle = hDbg;
        m_telemetry.TotalDebugObjectsCreated++;
        return NtStatus::Success;
    }

    NtStatus setInformationDebugObject(
        Handle               debugObjectHandle,
        DEBUGOBJECTINFOCLASS infoClass,
        PVOID                debugInformation,
        uint32_t             debugInformationLength,
        uint32_t*            returnLength
    ) {
        if (returnLength) *returnLength = 0;
        if (infoClass != DebugObjectKillProcessOnExitInformation) {
            return NtStatus::InvalidInfoClass;
        }
        if (!debugInformation || debugInformationLength < sizeof(ULONG)) {
            return NtStatus::InfoLengthMismatch;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_debugObjects.find(debugObjectHandle);
        if (it == m_debugObjects.end()) return NtStatus::InvalidHandle;

        ULONG killOnExit = *static_cast<const ULONG*>(debugInformation);
        if (killOnExit != 0) {
            it->second->Flags |= DEBUG_OBJECT_KILL_ON_CLOSE;
        } else {
            it->second->Flags &= ~DEBUG_OBJECT_KILL_ON_CLOSE;
        }
        if (returnLength) *returnLength = sizeof(ULONG);
        return NtStatus::Success;
    }

    NtStatus closeDebugObject(Handle debugObjectHandle) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_debugObjects.find(debugObjectHandle);
        if (it == m_debugObjects.end()) return NtStatus::InvalidHandle;

        DEBUG_OBJECT* dbgObj = it->second.get();
        dbgObj->Flags |= DEBUG_OBJECT_DELETE_PENDING;
        bool killOnClose = (dbgObj->Flags & DEBUG_OBJECT_KILL_ON_CLOSE) != 0;

        for (uint64_t pid : dbgObj->AttachedProcessIds) {
            auto pit = m_processes.find(pid);
            if (pit != m_processes.end()) {
                pit->second.BeingDebugged     = false;
                pit->second.DebugObjectHandle = 0;
                for (auto& th : pit->second.Threads) {
                    th.SuspendedForDebugEvent = false;
                }
                if (killOnClose && !pit->second.Terminated) {
                    pit->second.Terminated              = true;
                    pit->second.TerminatedByKillOnClose = true;
                    pit->second.ExitStatus              = NtStatus::DebuggerInactive;
                    m_telemetry.TotalProcessesKilledOnClose++;
                } else {
                    m_telemetry.TotalProcessesDetached++;
                }
            }
        }

        m_debugObjects.erase(it);
        if (m_threadDebugObject == debugObjectHandle) {
            m_threadDebugObject = 0;
        }
        m_telemetry.TotalDebugObjectsClosed++;
        return NtStatus::Success;
    }

    // ------------------------------------------------------------------------
    // 7.2 NtDebugActiveProcess & NtRemoveProcessDebug
    // ------------------------------------------------------------------------

    NtStatus debugActiveProcess(Handle processHandle, Handle debugObjectHandle) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto dbgIt = m_debugObjects.find(debugObjectHandle);
        if (dbgIt == m_debugObjects.end()) return NtStatus::InvalidHandle;

        DEBUG_OBJECT* dbgObj = dbgIt->second.get();
        if ((dbgObj->Flags & DEBUG_OBJECT_DELETE_PENDING) != 0) {
            return NtStatus::DebuggerInactive;
        }

        uint64_t targetPid = resolvePidUnlocked(processHandle);
        if (targetPid == 0) return NtStatus::InvalidHandle;

        auto procIt = m_processes.find(targetPid);
        if (procIt == m_processes.end()) {
            // Auto-register a synthetic process entry if attached by raw PID/handle
            DEBUGGED_PROCESS_ENTRY synth{};
            synth.ProcessId     = targetPid;
            synth.ProcessHandle = processHandle;
            synth.ImageBase     = reinterpret_cast<PVOID>(0x00007FF600000000ULL);
            DEBUGGED_THREAD_ENTRY mainTh{};
            mainTh.ThreadId     = targetPid + 4;
            mainTh.StartAddress = reinterpret_cast<PVOID>(0x00007FF600001000ULL);
            synth.Threads.push_back(mainTh);
            synth.LoadedModules.push_back({
                reinterpret_cast<PVOID>(0x00007FFB80000000ULL),
                0,
                0,
                L"\\SystemRoot\\System32\\ntdll.dll"
            });
            m_processes[targetPid] = std::move(synth);
            procIt = m_processes.find(targetPid);
        }

        DEBUGGED_PROCESS_ENTRY& proc = procIt->second;
        if (proc.Terminated) return NtStatus::ProcessIsTerminating;
        if (proc.BeingDebugged && proc.DebugObjectHandle != 0) {
            return NtStatus::PortConnectionRefused; // Already being debugged
        }

        proc.BeingDebugged     = true;
        proc.DebugObjectHandle = debugObjectHandle;
        proc.InitialAttachSent = true;
        dbgObj->AttachedProcessIds.push_back(targetPid);
        m_telemetry.TotalProcessesAttached++;

        // Synthesize initial DbgKmCreateProcessApi event for the primary thread
        uint64_t mainTid = proc.Threads.empty() ? (targetPid + 4) : proc.Threads.front().ThreadId;
        PVOID    mainStart = proc.Threads.empty() ? nullptr : proc.Threads.front().StartAddress;
        if (!proc.Threads.empty()) {
            proc.Threads.front().InitialThreadReported = true;
        }

        enqueueCreateProcessEventUnlocked(dbgObj, proc, mainTid, mainStart);

        // Synthesize DbgKmLoadDllApi events for already-mapped modules (e.g., ntdll.dll, kernel32.dll)
        for (const auto& mod : proc.LoadedModules) {
            enqueueLoadDllEventUnlocked(dbgObj, targetPid, mainTid, mod.BaseOfDll, mod.DebugInfoFileOffset, mod.DebugInfoSize, mod.ModuleName);
        }

        // Synthesize DbgKmCreateThreadApi for any additional threads already running in the process
        for (size_t i = 1; i < proc.Threads.size(); ++i) {
            proc.Threads[i].InitialThreadReported = true;
            enqueueCreateThreadEventUnlocked(dbgObj, targetPid, proc.Threads[i].ThreadId, proc.Threads[i].StartAddress);
        }

        // Synthesize initial attach breakpoint (STATUS_BREAKPOINT / DbgBreakpointStateChange)
        DBGKM_EXCEPTION_RECORD64 attachBp{};
        attachBp.ExceptionCode    = NtStatus::Breakpoint;
        attachBp.ExceptionAddress = reinterpret_cast<uint64_t>(proc.ImageBase) + 0x1050ULL;
        enqueueExceptionEventUnlocked(dbgObj, proc, mainTid, attachBp, true /* FirstChance */);

        return NtStatus::Success;
    }

    NtStatus removeProcessDebug(Handle processHandle, Handle debugObjectHandle) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto dbgIt = m_debugObjects.find(debugObjectHandle);
        if (dbgIt == m_debugObjects.end()) return NtStatus::InvalidHandle;

        uint64_t targetPid = resolvePidUnlocked(processHandle);
        auto procIt = m_processes.find(targetPid);
        if (procIt == m_processes.end()) return NtStatus::InvalidHandle;

        DEBUGGED_PROCESS_ENTRY& proc = procIt->second;
        if (!proc.BeingDebugged || proc.DebugObjectHandle != debugObjectHandle) {
            return NtStatus::PortNotSet;
        }

        proc.BeingDebugged     = false;
        proc.DebugObjectHandle = 0;
        for (auto& th : proc.Threads) {
            th.SuspendedForDebugEvent = false;
            th.LastContinueStatus     = NtStatus::DbgContinue;
        }

        DEBUG_OBJECT* dbgObj = dbgIt->second.get();
        std::erase(dbgObj->AttachedProcessIds, targetPid);

        // Remove any unread/pending events belonging to this detached process
        std::erase_if(dbgObj->EventList, [targetPid](const DEBUG_EVENT_RECORD& ev) {
            return ev.ClientId.UniqueProcess == targetPid;
        });
        dbgObj->EventsPresentSignaled = hasUnreadEventsUnlocked(dbgObj);

        m_telemetry.TotalProcessesDetached++;
        return NtStatus::Success;
    }

    // ------------------------------------------------------------------------
    // 7.3 NtWaitForDebugEvent & NtDebugContinue
    // ------------------------------------------------------------------------

    NtStatus waitForDebugEvent(
        Handle                   debugObjectHandle,
        BOOLEAN                  alertable,
        PLARGE_INTEGER           timeout,
        DBGUI_WAIT_STATE_CHANGE* outWaitStateChange
    ) {
        (void)alertable;
        (void)timeout;
        if (!outWaitStateChange) return NtStatus::InvalidParameter;

        std::lock_guard<std::mutex> lock(m_mutex);
        auto dbgIt = m_debugObjects.find(debugObjectHandle);
        if (dbgIt == m_debugObjects.end()) return NtStatus::InvalidHandle;

        DEBUG_OBJECT* dbgObj = dbgIt->second.get();
        if ((dbgObj->Flags & DEBUG_OBJECT_DELETE_PENDING) != 0) {
            return NtStatus::DebuggerInactive;
        }

        for (auto& ev : dbgObj->EventList) {
            if (!ev.ReadByDebugger && !ev.Completed) {
                ev.ReadByDebugger  = true;
                ev.Flags          |= DEBUG_EVENT_READ;
                *outWaitStateChange = ev.WaitStateChange;

                auto procIt = m_processes.find(ev.ClientId.UniqueProcess);
                if (procIt != m_processes.end()) {
                    for (auto& th : procIt->second.Threads) {
                        if (th.ThreadId == ev.ClientId.UniqueThread) {
                            th.SuspendedForDebugEvent = true;
                            break;
                        }
                    }
                }

                dbgObj->EventsPresentSignaled = hasUnreadEventsUnlocked(dbgObj);
                m_telemetry.TotalEventsWaited++;
                return NtStatus::Success;
            }
        }

        return NtStatus::Timeout;
    }

    NtStatus debugContinue(
        Handle               debugObjectHandle,
        const DBG_CLIENT_ID* clientId,
        NtStatus             continueStatus
    ) {
        if (!clientId) return NtStatus::InvalidParameter;

        switch (continueStatus) {
            case NtStatus::DbgContinue:
            case NtStatus::DbgExceptionHandled:
            case NtStatus::DbgExceptionNotHandled:
            case NtStatus::DbgTerminateThread:
            case NtStatus::DbgTerminateProcess:
                break;
            default:
                return NtStatus::InvalidParameter;
        }

        std::lock_guard<std::mutex> lock(m_mutex);
        auto dbgIt = m_debugObjects.find(debugObjectHandle);
        if (dbgIt == m_debugObjects.end()) return NtStatus::InvalidHandle;

        DEBUG_OBJECT* dbgObj = dbgIt->second.get();
        for (auto it = dbgObj->EventList.begin(); it != dbgObj->EventList.end(); ++it) {
            if (it->ReadByDebugger &&
                !it->Completed &&
                it->ClientId.UniqueProcess == clientId->UniqueProcess &&
                it->ClientId.UniqueThread  == clientId->UniqueThread) {

                it->Completed = true;
                it->Status    = continueStatus;

                // Resume the suspended target thread and record its continuation status
                auto procIt = m_processes.find(clientId->UniqueProcess);
                if (procIt != m_processes.end()) {
                    for (auto& th : procIt->second.Threads) {
                        if (th.ThreadId == clientId->UniqueThread) {
                            th.SuspendedForDebugEvent = false;
                            th.LastContinueStatus     = continueStatus;
                            break;
                        }
                    }
                    if (continueStatus == NtStatus::DbgTerminateProcess) {
                        procIt->second.Terminated = true;
                        procIt->second.ExitStatus = NtStatus::DbgTerminateProcess;
                    }
                }

                dbgObj->EventList.erase(it);
                dbgObj->EventsPresentSignaled = hasUnreadEventsUnlocked(dbgObj);
                m_telemetry.TotalEventsContinued++;
                return NtStatus::Success;
            }
        }

        return NtStatus::InvalidCid;
    }

    // ------------------------------------------------------------------------
    // 7.4 Kernel Executive Callbacks (DbgkCreateThread, DbgkForwardException, etc.)
    // ------------------------------------------------------------------------

    void dbgkCreateThread(
        uint64_t       processId,
        uint64_t       threadId,
        PVOID          startAddress,
        PVOID          imageBase = nullptr,
        const wchar_t* imageName = nullptr
    ) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto procIt = m_processes.find(processId);
        if (procIt == m_processes.end()) return;

        DEBUGGED_PROCESS_ENTRY& proc = procIt->second;
        if (imageBase) proc.ImageBase = imageBase;
        if (imageName) proc.ImageFileName = imageName;

        bool exists = false;
        for (auto& th : proc.Threads) {
            if (th.ThreadId == threadId) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            DEBUGGED_THREAD_ENTRY newTh{};
            newTh.ThreadId     = threadId;
            newTh.StartAddress = startAddress;
            proc.Threads.push_back(newTh);
        }

        if (!proc.BeingDebugged || proc.DebugObjectHandle == 0) return;
        auto dbgIt = m_debugObjects.find(proc.DebugObjectHandle);
        if (dbgIt == m_debugObjects.end()) return;

        DEBUG_OBJECT* dbgObj = dbgIt->second.get();
        if (!proc.InitialAttachSent) {
            proc.InitialAttachSent = true;
            enqueueCreateProcessEventUnlocked(dbgObj, proc, threadId, startAddress);
        } else {
            enqueueCreateThreadEventUnlocked(dbgObj, processId, threadId, startAddress);
        }
    }

    void dbgkExitThread(uint64_t processId, uint64_t threadId, NtStatus exitStatus) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto procIt = m_processes.find(processId);
        if (procIt == m_processes.end()) return;

        DEBUGGED_PROCESS_ENTRY& proc = procIt->second;
        if (!proc.BeingDebugged || proc.DebugObjectHandle == 0) return;

        auto dbgIt = m_debugObjects.find(proc.DebugObjectHandle);
        if (dbgIt == m_debugObjects.end()) return;

        DEBUG_EVENT_RECORD ev{};
        ev.SequenceId                          = m_nextSeqId++;
        ev.ClientId                            = { processId, threadId };
        ev.ApiNumber                           = DbgKmExitThreadApi;
        ev.WaitStateChange.NewState            = DbgExitThreadStateChange;
        ev.WaitStateChange.AppClientId         = ev.ClientId;
        ev.WaitStateChange.StateInfo.ExitThread.ExitStatus = exitStatus;

        dbgIt->second->EventList.push_back(std::move(ev));
        dbgIt->second->EventsPresentSignaled = true;
        m_telemetry.TotalEventsQueued++;
        m_telemetry.TotalThreadExitEvents++;
    }

    void dbgkExitProcess(uint64_t processId, uint64_t threadId, NtStatus exitStatus) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto procIt = m_processes.find(processId);
        if (procIt == m_processes.end()) return;

        DEBUGGED_PROCESS_ENTRY& proc = procIt->second;
        proc.Terminated = true;
        proc.ExitStatus = exitStatus;

        if (!proc.BeingDebugged || proc.DebugObjectHandle == 0) return;
        auto dbgIt = m_debugObjects.find(proc.DebugObjectHandle);
        if (dbgIt == m_debugObjects.end()) return;

        DEBUG_EVENT_RECORD ev{};
        ev.SequenceId                           = m_nextSeqId++;
        ev.ClientId                             = { processId, threadId };
        ev.ApiNumber                            = DbgKmExitProcessApi;
        ev.WaitStateChange.NewState             = DbgExitProcessStateChange;
        ev.WaitStateChange.AppClientId          = ev.ClientId;
        ev.WaitStateChange.StateInfo.ExitProcess.ExitStatus = exitStatus;

        dbgIt->second->EventList.push_back(std::move(ev));
        dbgIt->second->EventsPresentSignaled = true;
        m_telemetry.TotalEventsQueued++;
        m_telemetry.TotalProcessExitEvents++;
    }

    void dbgkMapViewOfSection(
        uint64_t       processId,
        uint64_t       threadId,
        PVOID          baseAddress,
        uint32_t       debugOffset,
        uint32_t       debugSize,
        const wchar_t* dllName
    ) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto procIt = m_processes.find(processId);
        if (procIt == m_processes.end()) return;

        DEBUGGED_PROCESS_ENTRY& proc = procIt->second;
        std::wstring modName = dllName ? dllName : L"\\SystemRoot\\System32\\module.dll";
        proc.LoadedModules.push_back({ baseAddress, debugOffset, debugSize, modName });

        if (!proc.BeingDebugged || proc.DebugObjectHandle == 0) return;
        auto dbgIt = m_debugObjects.find(proc.DebugObjectHandle);
        if (dbgIt == m_debugObjects.end()) return;

        enqueueLoadDllEventUnlocked(dbgIt->second.get(), processId, threadId, baseAddress, debugOffset, debugSize, modName);
    }

    void dbgkUnMapViewOfSection(uint64_t processId, uint64_t threadId, PVOID baseAddress) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto procIt = m_processes.find(processId);
        if (procIt == m_processes.end()) return;

        DEBUGGED_PROCESS_ENTRY& proc = procIt->second;
        std::erase_if(proc.LoadedModules, [baseAddress](const DEBUGGED_MODULE_ENTRY& m) {
            return m.BaseOfDll == baseAddress;
        });

        if (!proc.BeingDebugged || proc.DebugObjectHandle == 0) return;
        auto dbgIt = m_debugObjects.find(proc.DebugObjectHandle);
        if (dbgIt == m_debugObjects.end()) return;

        DEBUG_EVENT_RECORD ev{};
        ev.SequenceId                          = m_nextSeqId++;
        ev.ClientId                            = { processId, threadId };
        ev.ApiNumber                           = DbgKmUnloadDllApi;
        ev.WaitStateChange.NewState            = DbgUnloadDllStateChange;
        ev.WaitStateChange.AppClientId         = ev.ClientId;
        ev.WaitStateChange.StateInfo.UnloadDll.BaseAddress = baseAddress;

        dbgIt->second->EventList.push_back(std::move(ev));
        dbgIt->second->EventsPresentSignaled = true;
        m_telemetry.TotalEventsQueued++;
        m_telemetry.TotalDllUnloadEvents++;
    }

    BOOLEAN dbgkForwardException(
        uint64_t                        processId,
        uint64_t                        threadId,
        const DBGKM_EXCEPTION_RECORD64* exceptionRecord,
        BOOLEAN                         debugException,
        BOOLEAN                         secondChance
    ) {
        (void)debugException;
        if (!exceptionRecord) return FALSE_B;

        std::lock_guard<std::mutex> lock(m_mutex);
        auto procIt = m_processes.find(processId);
        if (procIt == m_processes.end()) return FALSE_B;

        DEBUGGED_PROCESS_ENTRY& proc = procIt->second;
        if (!proc.BeingDebugged || proc.DebugObjectHandle == 0) return FALSE_B;

        auto dbgIt = m_debugObjects.find(proc.DebugObjectHandle);
        if (dbgIt == m_debugObjects.end()) return FALSE_B;

        enqueueExceptionEventUnlocked(
            dbgIt->second.get(),
            proc,
            threadId,
            *exceptionRecord,
            secondChance == FALSE_B
        );
        return TRUE_B;
    }

    NtStatus openProcessDebugPort(Handle processHandle, Handle* outDebugObjectHandle) {
        if (!outDebugObjectHandle) return NtStatus::InvalidParameter;
        std::lock_guard<std::mutex> lock(m_mutex);
        uint64_t pid = resolvePidUnlocked(processHandle);
        auto it = m_processes.find(pid);
        if (it == m_processes.end()) return NtStatus::InvalidHandle;
        if (!it->second.BeingDebugged || it->second.DebugObjectHandle == 0) {
            *outDebugObjectHandle = 0;
            return NtStatus::PortNotSet;
        }
        *outDebugObjectHandle = it->second.DebugObjectHandle;
        return NtStatus::Success;
    }

    // ------------------------------------------------------------------------
    // 7.5 NTDLL DbgUi* & Process State Inspection Helpers
    // ------------------------------------------------------------------------

    NtStatus dbgUiConnectToDbg() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_threadDebugObject != 0 && m_debugObjects.contains(m_threadDebugObject)) {
            return NtStatus::Success;
        }
        Handle hDbg = allocateHandleUnlocked();
        auto obj = std::make_unique<DEBUG_OBJECT>();
        obj->DebugObjectHandle = hDbg;
        obj->DesiredAccess     = DEBUG_ALL_ACCESS;
        obj->Flags             = DEBUG_OBJECT_KILL_ON_CLOSE;
        m_debugObjects[hDbg]   = std::move(obj);
        m_threadDebugObject    = hDbg;
        m_telemetry.TotalDebugObjectsCreated++;
        return NtStatus::Success;
    }

    Handle dbgUiGetThreadDebugObject() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_threadDebugObject;
    }

    void dbgUiSetThreadDebugObject(Handle hDebugObject) noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_threadDebugObject = hDebugObject;
    }

    static NtStatus convertStateChangeToWin32(
        const DBGUI_WAIT_STATE_CHANGE* stateChange,
        WIN32_DEBUG_EVENT*             win32Event
    ) noexcept {
        if (!stateChange || !win32Event) return NtStatus::InvalidParameter;
        std::memset(win32Event, 0, sizeof(WIN32_DEBUG_EVENT));
        win32Event->dwProcessId = static_cast<uint32_t>(stateChange->AppClientId.UniqueProcess);
        win32Event->dwThreadId  = static_cast<uint32_t>(stateChange->AppClientId.UniqueThread);

        switch (stateChange->NewState) {
            case DbgCreateProcessStateChange:
                win32Event->dwDebugEventCode    = WIN32_CREATE_PROCESS_DEBUG_EVENT;
                win32Event->u.CreateProcessInfo = stateChange->StateInfo.CreateProcessInfo;
                break;
            case DbgCreateThreadStateChange:
                win32Event->dwDebugEventCode = WIN32_CREATE_THREAD_DEBUG_EVENT;
                win32Event->u.CreateThread   = stateChange->StateInfo.CreateThread;
                break;
            case DbgExceptionStateChange:
            case DbgBreakpointStateChange:
            case DbgSingleStepStateChange:
                win32Event->dwDebugEventCode = WIN32_EXCEPTION_DEBUG_EVENT;
                win32Event->u.Exception      = stateChange->StateInfo.Exception;
                break;
            case DbgExitThreadStateChange:
                win32Event->dwDebugEventCode = WIN32_EXIT_THREAD_DEBUG_EVENT;
                win32Event->u.ExitThread     = stateChange->StateInfo.ExitThread;
                break;
            case DbgExitProcessStateChange:
                win32Event->dwDebugEventCode = WIN32_EXIT_PROCESS_DEBUG_EVENT;
                win32Event->u.ExitProcess    = stateChange->StateInfo.ExitProcess;
                break;
            case DbgLoadDllStateChange:
                win32Event->dwDebugEventCode = WIN32_LOAD_DLL_DEBUG_EVENT;
                win32Event->u.LoadDll        = stateChange->StateInfo.LoadDll;
                break;
            case DbgUnloadDllStateChange:
                win32Event->dwDebugEventCode = WIN32_UNLOAD_DLL_DEBUG_EVENT;
                win32Event->u.UnloadDll      = stateChange->StateInfo.UnloadDll;
                break;
            default:
                return NtStatus::InvalidParameter;
        }
        return NtStatus::Success;
    }

    bool isProcessBeingDebugged(uint64_t processId) const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_processes.find(processId);
        return (it != m_processes.end()) ? it->second.BeingDebugged : false;
    }

    bool isProcessTerminatedByKillOnClose(uint64_t processId) const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_processes.find(processId);
        return (it != m_processes.end()) ? it->second.TerminatedByKillOnClose : false;
    }

    bool isThreadSuspendedForDebugEvent(uint64_t processId, uint64_t threadId) const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_processes.find(processId);
        if (it == m_processes.end()) return false;
        for (const auto& th : it->second.Threads) {
            if (th.ThreadId == threadId) return th.SuspendedForDebugEvent;
        }
        return false;
    }

    DBGK_TELEMETRY getTelemetry() const noexcept {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_telemetry;
    }

private:
    TitanDbgkManager() = default;

    Handle allocateHandleUnlocked() noexcept {
        Handle h = m_nextHandle;
        m_nextHandle += 4;
        return h;
    }

    uint64_t resolvePidUnlocked(Handle processHandleOrPid) const noexcept {
        auto it = m_handleToPid.find(processHandleOrPid);
        if (it != m_handleToPid.end()) return it->second;
        return static_cast<uint64_t>(processHandleOrPid);
    }

    static bool hasUnreadEventsUnlocked(const DEBUG_OBJECT* dbgObj) noexcept {
        if (!dbgObj) return false;
        for (const auto& ev : dbgObj->EventList) {
            if (!ev.ReadByDebugger && !ev.Completed) return true;
        }
        return false;
    }

    void enqueueCreateProcessEventUnlocked(
        DEBUG_OBJECT*           dbgObj,
        DEBUGGED_PROCESS_ENTRY& proc,
        uint64_t                initialThreadId,
        PVOID                   startAddress
    ) {
        DEBUG_EVENT_RECORD ev{};
        ev.SequenceId                  = m_nextSeqId++;
        ev.ClientId                    = { proc.ProcessId, initialThreadId };
        ev.ApiNumber                   = DbgKmCreateProcessApi;
        ev.ModuleName                  = proc.ImageFileName;
        ev.WaitStateChange.NewState    = DbgCreateProcessStateChange;
        ev.WaitStateChange.AppClientId = ev.ClientId;

        auto& cp = ev.WaitStateChange.StateInfo.CreateProcessInfo;
        cp.HandleToProcess                       = proc.ProcessHandle;
        cp.HandleToThread                        = static_cast<Handle>(initialThreadId);
        cp.NewProcess.BaseOfImage                = proc.ImageBase;
        cp.NewProcess.InitialThread.StartAddress = startAddress;

        dbgObj->EventList.push_back(std::move(ev));
        dbgObj->EventsPresentSignaled = true;
        m_telemetry.TotalEventsQueued++;
    }

    void enqueueCreateThreadEventUnlocked(
        DEBUG_OBJECT* dbgObj,
        uint64_t      processId,
        uint64_t      threadId,
        PVOID         startAddress
    ) {
        DEBUG_EVENT_RECORD ev{};
        ev.SequenceId                  = m_nextSeqId++;
        ev.ClientId                    = { processId, threadId };
        ev.ApiNumber                   = DbgKmCreateThreadApi;
        ev.WaitStateChange.NewState    = DbgCreateThreadStateChange;
        ev.WaitStateChange.AppClientId = ev.ClientId;

        auto& ct = ev.WaitStateChange.StateInfo.CreateThread;
        ct.HandleToThread         = static_cast<Handle>(threadId);
        ct.NewThread.StartAddress = startAddress;

        dbgObj->EventList.push_back(std::move(ev));
        dbgObj->EventsPresentSignaled = true;
        m_telemetry.TotalEventsQueued++;
        m_telemetry.TotalThreadCreateEvents++;
    }

    void enqueueLoadDllEventUnlocked(
        DEBUG_OBJECT*      dbgObj,
        uint64_t           processId,
        uint64_t           threadId,
        PVOID              baseOfDll,
        uint32_t           debugOffset,
        uint32_t           debugSize,
        const std::wstring& moduleName
    ) {
        DEBUG_EVENT_RECORD ev{};
        ev.SequenceId                  = m_nextSeqId++;
        ev.ClientId                    = { processId, threadId };
        ev.ApiNumber                   = DbgKmLoadDllApi;
        ev.ModuleName                  = moduleName;
        ev.WaitStateChange.NewState    = DbgLoadDllStateChange;
        ev.WaitStateChange.AppClientId = ev.ClientId;

        auto& ld = ev.WaitStateChange.StateInfo.LoadDll;
        ld.BaseOfDll           = baseOfDll;
        ld.DebugInfoFileOffset = debugOffset;
        ld.DebugInfoSize       = debugSize;

        dbgObj->EventList.push_back(std::move(ev));
        dbgObj->EventsPresentSignaled = true;
        m_telemetry.TotalEventsQueued++;
        m_telemetry.TotalDllLoadEvents++;
    }

    void enqueueExceptionEventUnlocked(
        DEBUG_OBJECT*                   dbgObj,
        DEBUGGED_PROCESS_ENTRY&         proc,
        uint64_t                        threadId,
        const DBGKM_EXCEPTION_RECORD64& rec,
        bool                            firstChance
    ) {
        DEBUG_EVENT_RECORD ev{};
        ev.SequenceId                  = m_nextSeqId++;
        ev.ClientId                    = { proc.ProcessId, threadId };
        ev.ApiNumber                   = DbgKmExceptionApi;
        ev.Flags                       = DEBUG_EVENT_SUSPEND;
        ev.WaitStateChange.AppClientId = ev.ClientId;

        if (rec.ExceptionCode == NtStatus::Breakpoint) {
            ev.WaitStateChange.NewState = DbgBreakpointStateChange;
            m_telemetry.TotalBreakpointTraps++;
        } else if (rec.ExceptionCode == NtStatus::SingleStep) {
            ev.WaitStateChange.NewState = DbgSingleStepStateChange;
            m_telemetry.TotalSingleStepTraps++;
        } else {
            ev.WaitStateChange.NewState = DbgExceptionStateChange;
        }

        ev.WaitStateChange.StateInfo.Exception.ExceptionRecord = rec;
        ev.WaitStateChange.StateInfo.Exception.FirstChance     = firstChance ? 1u : 0u;

        // Suspend the faulting debuggee thread until NtDebugContinue
        for (auto& th : proc.Threads) {
            if (th.ThreadId == threadId) {
                th.SuspendedForDebugEvent = true;
                break;
            }
        }

        dbgObj->EventList.push_back(std::move(ev));
        dbgObj->EventsPresentSignaled = true;
        m_telemetry.TotalEventsQueued++;
        m_telemetry.TotalExceptionsForwarded++;
    }

    mutable std::mutex                                      m_mutex{};
    std::unordered_map<Handle, std::unique_ptr<DEBUG_OBJECT>> m_debugObjects{};
    std::unordered_map<uint64_t, DEBUGGED_PROCESS_ENTRY>    m_processes{};
    std::unordered_map<Handle, uint64_t>                    m_handleToPid{};
    Handle                                                  m_threadDebugObject{0};
    Handle                                                  m_nextHandle{0xDB00};
    uint64_t                                                m_nextSeqId{1};
    DBGK_TELEMETRY                                          m_telemetry{};
};

// ============================================================================
// 8. Exported NT Kernel & NTDLL / Win32 Debugging Functions
// ============================================================================

inline NtStatus NtCreateDebugObject(
    Handle*           DebugObjectHandle,
    uint32_t          DesiredAccess,
    ObjectAttributes* ObjectAttributes,
    uint32_t          Flags
) {
    return TitanDbgkManager::Instance().createDebugObject(DebugObjectHandle, DesiredAccess, ObjectAttributes, Flags);
}

inline NtStatus NtDebugActiveProcess(Handle ProcessHandle, Handle DebugObjectHandle) {
    return TitanDbgkManager::Instance().debugActiveProcess(ProcessHandle, DebugObjectHandle);
}

inline NtStatus NtRemoveProcessDebug(Handle ProcessHandle, Handle DebugObjectHandle) {
    return TitanDbgkManager::Instance().removeProcessDebug(ProcessHandle, DebugObjectHandle);
}

inline NtStatus NtSetInformationDebugObject(
    Handle               DebugObjectHandle,
    DEBUGOBJECTINFOCLASS DebugObjectInformationClass,
    PVOID                DebugInformation,
    uint32_t             DebugInformationLength,
    uint32_t*            ReturnLength
) {
    return TitanDbgkManager::Instance().setInformationDebugObject(
        DebugObjectHandle,
        DebugObjectInformationClass,
        DebugInformation,
        DebugInformationLength,
        ReturnLength
    );
}

inline NtStatus NtWaitForDebugEvent(
    Handle                   DebugObjectHandle,
    BOOLEAN                  Alertable,
    PLARGE_INTEGER           Timeout,
    DBGUI_WAIT_STATE_CHANGE* WaitStateChange
) {
    return TitanDbgkManager::Instance().waitForDebugEvent(DebugObjectHandle, Alertable, Timeout, WaitStateChange);
}

inline NtStatus NtDebugContinue(
    Handle               DebugObjectHandle,
    const DBG_CLIENT_ID* ClientId,
    NtStatus             ContinueStatus
) {
    return TitanDbgkManager::Instance().debugContinue(DebugObjectHandle, ClientId, ContinueStatus);
}

inline NtStatus DbgkCloseDebugObject(Handle DebugObjectHandle) {
    return TitanDbgkManager::Instance().closeDebugObject(DebugObjectHandle);
}

// Kernel Executive Callbacks (Dbgk*)
inline void DbgkCreateThread(
    uint64_t       ProcessId,
    uint64_t       ThreadId,
    PVOID          StartAddress,
    PVOID          ImageBase = nullptr,
    const wchar_t* ImageName = nullptr
) {
    TitanDbgkManager::Instance().dbgkCreateThread(ProcessId, ThreadId, StartAddress, ImageBase, ImageName);
}

inline void DbgkExitThread(uint64_t ProcessId, uint64_t ThreadId, NtStatus ExitStatus) {
    TitanDbgkManager::Instance().dbgkExitThread(ProcessId, ThreadId, ExitStatus);
}

inline void DbgkExitProcess(uint64_t ProcessId, uint64_t ThreadId, NtStatus ExitStatus) {
    TitanDbgkManager::Instance().dbgkExitProcess(ProcessId, ThreadId, ExitStatus);
}

inline void DbgkMapViewOfSection(
    uint64_t       ProcessId,
    uint64_t       ThreadId,
    PVOID          BaseAddress,
    uint32_t       DebugInfoFileOffset,
    uint32_t       DebugInfoSize,
    const wchar_t* DllName
) {
    TitanDbgkManager::Instance().dbgkMapViewOfSection(ProcessId, ThreadId, BaseAddress, DebugInfoFileOffset, DebugInfoSize, DllName);
}

inline void DbgkUnMapViewOfSection(uint64_t ProcessId, uint64_t ThreadId, PVOID BaseAddress) {
    TitanDbgkManager::Instance().dbgkUnMapViewOfSection(ProcessId, ThreadId, BaseAddress);
}

inline BOOLEAN DbgkForwardException(
    uint64_t                        ProcessId,
    uint64_t                        ThreadId,
    const DBGKM_EXCEPTION_RECORD64* ExceptionRecord,
    BOOLEAN                         DebugException,
    BOOLEAN                         SecondChance
) {
    return TitanDbgkManager::Instance().dbgkForwardException(ProcessId, ThreadId, ExceptionRecord, DebugException, SecondChance);
}

inline NtStatus DbgkOpenProcessDebugPort(Handle ProcessHandle, Handle* OutDebugObjectHandle) {
    return TitanDbgkManager::Instance().openProcessDebugPort(ProcessHandle, OutDebugObjectHandle);
}

// NTDLL DbgUi* User-Mode Debugging Bridge
inline NtStatus DbgUiConnectToDbg() {
    return TitanDbgkManager::Instance().dbgUiConnectToDbg();
}

inline Handle DbgUiGetThreadDebugObject() noexcept {
    return TitanDbgkManager::Instance().dbgUiGetThreadDebugObject();
}

inline void DbgUiSetThreadDebugObject(Handle DebugObject) noexcept {
    TitanDbgkManager::Instance().dbgUiSetThreadDebugObject(DebugObject);
}

inline NtStatus DbgUiDebugActiveProcess(Handle Process) {
    NtStatus st = DbgUiConnectToDbg();
    if (!NT_SUCCESS(st)) return st;
    return NtDebugActiveProcess(Process, DbgUiGetThreadDebugObject());
}

inline NtStatus DbgUiStopDebugging(Handle Process) {
    Handle hDbg = DbgUiGetThreadDebugObject();
    if (hDbg == 0) return NtStatus::DebuggerInactive;
    return NtRemoveProcessDebug(Process, hDbg);
}

inline NtStatus DbgUiWaitStateChange(DBGUI_WAIT_STATE_CHANGE* StateChange, PLARGE_INTEGER Timeout) {
    Handle hDbg = DbgUiGetThreadDebugObject();
    if (hDbg == 0) return NtStatus::DebuggerInactive;
    return NtWaitForDebugEvent(hDbg, FALSE_B, Timeout, StateChange);
}

inline NtStatus DbgUiContinue(const DBG_CLIENT_ID* AppClientId, NtStatus ContinueStatus) {
    Handle hDbg = DbgUiGetThreadDebugObject();
    if (hDbg == 0) return NtStatus::DebuggerInactive;
    return NtDebugContinue(hDbg, AppClientId, ContinueStatus);
}

inline NtStatus DbgUiConvertStateChangeStructure(
    const DBGUI_WAIT_STATE_CHANGE* StateChange,
    WIN32_DEBUG_EVENT*             Win32DebugEvent
) noexcept {
    return TitanDbgkManager::convertStateChangeToWin32(StateChange, Win32DebugEvent);
}

// Win32 kernel32.dll Debugging Wrappers
inline BOOLEAN DebugActiveProcessWin32(uint32_t dwProcessId) {
    return NT_SUCCESS(DbgUiDebugActiveProcess(static_cast<Handle>(dwProcessId))) ? TRUE_B : FALSE_B;
}

inline BOOLEAN DebugActiveProcessStopWin32(uint32_t dwProcessId) {
    return NT_SUCCESS(DbgUiStopDebugging(static_cast<Handle>(dwProcessId))) ? TRUE_B : FALSE_B;
}

inline BOOLEAN WaitForDebugEventExWin32(WIN32_DEBUG_EVENT* lpDebugEvent, uint32_t dwMilliseconds) {
    LARGE_INTEGER timeout{};
    timeout.quadPart = static_cast<int64_t>(dwMilliseconds) * -10000LL;
    DBGUI_WAIT_STATE_CHANGE stateChange{};
    NtStatus st = DbgUiWaitStateChange(&stateChange, &timeout);
    if (!NT_SUCCESS(st)) return FALSE_B;
    return NT_SUCCESS(DbgUiConvertStateChangeStructure(&stateChange, lpDebugEvent)) ? TRUE_B : FALSE_B;
}

inline BOOLEAN ContinueDebugEventWin32(uint32_t dwProcessId, uint32_t dwThreadId, NtStatus dwContinueStatus) {
    DBG_CLIENT_ID cid{ dwProcessId, dwThreadId };
    return NT_SUCCESS(DbgUiContinue(&cid, dwContinueStatus)) ? TRUE_B : FALSE_B;
}

inline BOOLEAN DebugSetProcessKillOnExitWin32(BOOLEAN KillOnExit) {
    Handle hDbg = DbgUiGetThreadDebugObject();
    if (hDbg == 0) return FALSE_B;
    ULONG val = (KillOnExit != FALSE_B) ? 1u : 0u;
    return NT_SUCCESS(NtSetInformationDebugObject(hDbg, DebugObjectKillProcessOnExitInformation, &val, sizeof(val), nullptr))
        ? TRUE_B
        : FALSE_B;
}

inline BOOLEAN CheckRemoteDebuggerPresentWin32(Handle hProcess, BOOLEAN* pbDebuggerPresent) {
    if (!pbDebuggerPresent) return FALSE_B;
    Handle hPort = 0;
    NtStatus st = DbgkOpenProcessDebugPort(hProcess, &hPort);
    if (st == NtStatus::InvalidHandle) return FALSE_B;
    *pbDebuggerPresent = (st == NtStatus::Success && hPort != 0) ? TRUE_B : FALSE_B;
    return TRUE_B;
}

inline BOOLEAN DbgkGetTelemetry(DBGK_TELEMETRY* outTelemetry) noexcept {
    if (!outTelemetry) return FALSE_B;
    *outTelemetry = TitanDbgkManager::Instance().getTelemetry();
    return TRUE_B;
}

// ============================================================================
// 9. Subsystem Initialization & Dynamic Export Registration
// ============================================================================

inline void InitializeDbgkSubsystemExports() {
    auto& ldr = ldr::DynamicLoader::get();

    // Register ntoskrnl.exe Dbgk* and Nt* Debug Object exports
    struct ExportEntry {
        const char* name;
        void*       fn;
    };

    const ExportEntry kNtosExports[] = {
        { "NtCreateDebugObject",         reinterpret_cast<void*>(&NtCreateDebugObject) },
        { "NtDebugActiveProcess",        reinterpret_cast<void*>(&NtDebugActiveProcess) },
        { "NtRemoveProcessDebug",        reinterpret_cast<void*>(&NtRemoveProcessDebug) },
        { "NtSetInformationDebugObject", reinterpret_cast<void*>(&NtSetInformationDebugObject) },
        { "NtWaitForDebugEvent",         reinterpret_cast<void*>(&NtWaitForDebugEvent) },
        { "NtDebugContinue",             reinterpret_cast<void*>(&NtDebugContinue) },
        { "DbgkCreateThread",            reinterpret_cast<void*>(&DbgkCreateThread) },
        { "DbgkExitThread",              reinterpret_cast<void*>(&DbgkExitThread) },
        { "DbgkExitProcess",             reinterpret_cast<void*>(&DbgkExitProcess) },
        { "DbgkMapViewOfSection",        reinterpret_cast<void*>(&DbgkMapViewOfSection) },
        { "DbgkUnMapViewOfSection",      reinterpret_cast<void*>(&DbgkUnMapViewOfSection) },
        { "DbgkForwardException",        reinterpret_cast<void*>(&DbgkForwardException) },
        { "DbgkOpenProcessDebugPort",    reinterpret_cast<void*>(&DbgkOpenProcessDebugPort) },
        { "DbgkGetTelemetry",            reinterpret_cast<void*>(&DbgkGetTelemetry) }
    };

    for (const auto& exp : kNtosExports) {
        ldr.registerExport("ntoskrnl.exe", exp.name, exp.fn);
    }

    // Register ntdll.dll DbgUi* and Nt* Debug Object exports
    const ExportEntry kNtdllExports[] = {
        { "NtCreateDebugObject",              reinterpret_cast<void*>(&NtCreateDebugObject) },
        { "NtDebugActiveProcess",             reinterpret_cast<void*>(&NtDebugActiveProcess) },
        { "NtRemoveProcessDebug",             reinterpret_cast<void*>(&NtRemoveProcessDebug) },
        { "NtSetInformationDebugObject",      reinterpret_cast<void*>(&NtSetInformationDebugObject) },
        { "NtWaitForDebugEvent",              reinterpret_cast<void*>(&NtWaitForDebugEvent) },
        { "NtDebugContinue",                  reinterpret_cast<void*>(&NtDebugContinue) },
        { "DbgUiConnectToDbg",                reinterpret_cast<void*>(&DbgUiConnectToDbg) },
        { "DbgUiGetThreadDebugObject",        reinterpret_cast<void*>(&DbgUiGetThreadDebugObject) },
        { "DbgUiSetThreadDebugObject",        reinterpret_cast<void*>(&DbgUiSetThreadDebugObject) },
        { "DbgUiDebugActiveProcess",          reinterpret_cast<void*>(&DbgUiDebugActiveProcess) },
        { "DbgUiStopDebugging",               reinterpret_cast<void*>(&DbgUiStopDebugging) },
        { "DbgUiWaitStateChange",             reinterpret_cast<void*>(&DbgUiWaitStateChange) },
        { "DbgUiContinue",                    reinterpret_cast<void*>(&DbgUiContinue) },
        { "DbgUiConvertStateChangeStructure", reinterpret_cast<void*>(&DbgUiConvertStateChangeStructure) }
    };

    for (const auto& exp : kNtdllExports) {
        ldr.registerExport("ntdll.dll", exp.name, exp.fn);
    }

    // Register kernel32.dll Win32 Debugging API exports
    const ExportEntry kKernel32Exports[] = {
        { "DebugActiveProcess",          reinterpret_cast<void*>(&DebugActiveProcessWin32) },
        { "DebugActiveProcessStop",      reinterpret_cast<void*>(&DebugActiveProcessStopWin32) },
        { "WaitForDebugEventEx",         reinterpret_cast<void*>(&WaitForDebugEventExWin32) },
        { "ContinueDebugEvent",          reinterpret_cast<void*>(&ContinueDebugEventWin32) },
        { "DebugSetProcessKillOnExit",   reinterpret_cast<void*>(&DebugSetProcessKillOnExitWin32) },
        { "CheckRemoteDebuggerPresent",  reinterpret_cast<void*>(&CheckRemoteDebuggerPresentWin32) }
    };

    for (const auto& exp : kKernel32Exports) {
        ldr.registerExport("kernel32.dll", exp.name, exp.fn);
    }

    auto& scm = scm::ServiceControlManager::get();
    auto dbgkSvc = std::make_shared<scm::ServiceRecord>();
    dbgkSvc->serviceName           = L"DbgkDebugObject";
    dbgkSvc->displayName           = L"Windows NT User-Mode Debugging Executive (Dbgk / DEBUG_OBJECT)";
    dbgkSvc->serviceType           = scm::SERVICE_KERNEL_DRIVER;
    dbgkSvc->startType             = scm::SERVICE_BOOT_START;
    dbgkSvc->errorControl          = scm::SERVICE_ERROR_CRITICAL;
    dbgkSvc->binaryPath            = L"\\SystemRoot\\System32\\ntoskrnl.exe";
    dbgkSvc->status.dwCurrentState = scm::SERVICE_RUNNING;
    scm.registerServiceRecord(dbgkSvc);

    version::VersionDatabase::Instance().RegisterModule(
        "dbgk_executive",
        "10.0.26100.4400",
        "Sovereign NT User-Mode Debugging Executive (Dbgk* / DEBUG_OBJECT / DbgUi*)"
    );
}

} // namespace micant::km::dbgk

namespace micant {
    using km::dbgk::DEBUG_OBJECT;
    using km::dbgk::DBGKM_APINUMBER;
    using km::dbgk::DBG_STATE;
    using km::dbgk::DBG_CLIENT_ID;
    using km::dbgk::DBGKM_EXCEPTION_RECORD64;
    using km::dbgk::DBGUI_WAIT_STATE_CHANGE;
    using km::dbgk::WIN32_DEBUG_EVENT;
    using km::dbgk::DBGK_TELEMETRY;
    using km::dbgk::TitanDbgkManager;
    using km::dbgk::NtCreateDebugObject;
    using km::dbgk::NtDebugActiveProcess;
    using km::dbgk::NtRemoveProcessDebug;
    using km::dbgk::NtSetInformationDebugObject;
    using km::dbgk::NtWaitForDebugEvent;
    using km::dbgk::NtDebugContinue;
    using km::dbgk::DbgkCloseDebugObject;
    using km::dbgk::DbgkCreateThread;
    using km::dbgk::DbgkExitThread;
    using km::dbgk::DbgkExitProcess;
    using km::dbgk::DbgkMapViewOfSection;
    using km::dbgk::DbgkUnMapViewOfSection;
    using km::dbgk::DbgkForwardException;
    using km::dbgk::DbgkOpenProcessDebugPort;
    using km::dbgk::DbgUiConnectToDbg;
    using km::dbgk::DbgUiGetThreadDebugObject;
    using km::dbgk::DbgUiSetThreadDebugObject;
    using km::dbgk::DbgUiDebugActiveProcess;
    using km::dbgk::DbgUiStopDebugging;
    using km::dbgk::DbgUiWaitStateChange;
    using km::dbgk::DbgUiContinue;
    using km::dbgk::DbgUiConvertStateChangeStructure;
    using km::dbgk::InitializeDbgkSubsystemExports;
} // namespace micant
