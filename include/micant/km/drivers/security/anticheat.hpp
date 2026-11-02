// ============================================================================
// MicaNT: Sovereign Kernel Anti-Cheat & Process Protection Subsystem
// (include/micant/km/drivers/security/anticheat.hpp)
//
// Sovereign Subsystem: TitanAntiCheat / AegisProcessShield
//
// Strict Clean-Room Implementation based on:
//   - Operating System Kernel Security Architecture & Object Manager Callbacks
//   - ObRegisterCallbacks Handle Creation & Duplication Filtering (PsProcessType, PsThreadType)
//   - Process, Thread & Image Notifications (PsSetCreateProcessNotifyRoutineEx,
//     PsSetCreateThreadNotifyRoutine, PsSetLoadImageNotifyRoutine)
//   - Access Mask Stripping (PROCESS_VM_READ, PROCESS_VM_WRITE, PROCESS_CREATE_THREAD)
//   - Process Integrity Levels & Protected Process Light (PPL) Boundaries
//   - Supreme Court of the United States: Google LLC v. Oracle America, Inc.
//     (141 S. Ct. 1183, 2021) - API interoperability doctrine
//
// Subsystem Overview:
//   TitanAntiCheat delivers kernel-mode process protection and memory anti-tamper
//   capabilities required by competitive gaming and high-security workloads:
//   1. ObRegisterCallbacks Handle Filtering:
//      - Intercepts handle open and duplication requests targeting protected processes.
//      - Strips dangerous memory read, write, and thread creation access bits while
//        preserving benign query access for system processes (csrss.exe, taskmgr.exe).
//   2. Cross-Process Remote Thread Blocking:
//      - Intercepts thread creation requests and blocks remote thread injection (CreateRemoteThread)
//        originating from external, untrusted processes.
//   3. Dynamic Image Load Verification:
//      - Inspects DLL and module load notifications (PsSetLoadImageNotifyRoutine).
//      - Enforces digital signature verification and whitelists approved game overlays
//        (Steam Overlay, Discord Hook, GPU Performance HUD) while blocking unsigned hooks.
//   4. SCM Service & Version Database Parity:
//      - Full SCM driver registration ("AntiCheat", SERVICE_KERNEL_DRIVER) and VersionDatabase
//        module inventory at 10.0.26100.1.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <shared_mutex>
#include <memory>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>

#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/um/servers/scm.hpp"
#include "micant/um/subsystems/winbase/version.hpp"

namespace micant::anticheat {

// ============================================================================
// 1. Core Enumerations & Access Constants
// ============================================================================

// Standard Windows NT Process Access Masks
constexpr uint32_t PROCESS_TERMINATE                  = 0x0001;
constexpr uint32_t PROCESS_CREATE_THREAD              = 0x0002;
constexpr uint32_t PROCESS_SET_SESSIONID              = 0x0004;
constexpr uint32_t PROCESS_VM_OPERATION               = 0x0008;
constexpr uint32_t PROCESS_VM_READ                    = 0x0010;
constexpr uint32_t PROCESS_VM_WRITE                   = 0x0020;
constexpr uint32_t PROCESS_DUP_HANDLE                 = 0x0040;
constexpr uint32_t PROCESS_CREATE_PROCESS             = 0x0080;
constexpr uint32_t PROCESS_SET_QUOTA                  = 0x0100;
constexpr uint32_t PROCESS_SET_INFORMATION            = 0x0200;
constexpr uint32_t PROCESS_QUERY_INFORMATION          = 0x0400;
constexpr uint32_t PROCESS_SUSPEND_RESUME             = 0x0800;
constexpr uint32_t PROCESS_QUERY_LIMITED_INFORMATION  = 0x1000;
constexpr uint32_t PROCESS_ALL_ACCESS                 = 0x1FFFFF;

// Dangerous access mask that must be stripped from untrusted handles
constexpr uint32_t DANGEROUS_PROCESS_ACCESS_MASK =
    PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION |
    PROCESS_CREATE_THREAD | PROCESS_SUSPEND_RESUME | PROCESS_TERMINATE;

// Thread Access Masks
constexpr uint32_t THREAD_TERMINATE                   = 0x0001;
constexpr uint32_t THREAD_SUSPEND_RESUME              = 0x0002;
constexpr uint32_t THREAD_GET_CONTEXT                 = 0x0008;
constexpr uint32_t THREAD_SET_CONTEXT                 = 0x0010;
constexpr uint32_t THREAD_SET_INFORMATION             = 0x0020;
constexpr uint32_t THREAD_QUERY_INFORMATION           = 0x0040;
constexpr uint32_t THREAD_SET_THREAD_TOKEN            = 0x0080;
constexpr uint32_t THREAD_IMPERSONATE                 = 0x0100;
constexpr uint32_t THREAD_DIRECT_IMPERSONATION        = 0x0200;
constexpr uint32_t THREAD_ALL_ACCESS                  = 0x1FFFFF;

constexpr uint32_t DANGEROUS_THREAD_ACCESS_MASK =
    THREAD_TERMINATE | THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT |
    THREAD_SET_CONTEXT | THREAD_SET_INFORMATION;

enum class ProcessProtectionLevel : uint32_t {
    None                = 0, // Unprotected standard process
    AntiCheatProtected  = 1, // Active kernel anti-cheat handle & injection shield
    GamePpl             = 2, // Protected Process Light (PPL) for gaming executables
    SystemPpl           = 3  // Kernel / System critical process
};

inline const char* ProtectionLevelToString(ProcessProtectionLevel level) {
    switch (level) {
        case ProcessProtectionLevel::None:               return "None (Unprotected)";
        case ProcessProtectionLevel::AntiCheatProtected: return "Anti-Cheat Protected";
        case ProcessProtectionLevel::GamePpl:            return "Protected Process Light (PPL)";
        case ProcessProtectionLevel::SystemPpl:          return "System PPL (High Integrity)";
        default:                                         return "Unknown";
    }
}

// ============================================================================
// 2. Data Structures
// ============================================================================

struct ProtectedProcessEntry {
    uint32_t processId{0};
    std::string processName{"cod.exe"};
    std::string binaryPath{"C:\\Games\\CallOfDuty\\cod.exe"};
    std::string binaryHashSha256{"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"};
    ProcessProtectionLevel level{ProcessProtectionLevel::AntiCheatProtected};
    uint64_t protectedStartTimeEpoch{0};
    uint32_t handleOpenAttemptsCount{0};
    uint32_t handleAccessStrippedCount{0};
    uint32_t remoteThreadBlocksCount{0};
    uint32_t blockedImageInjectionsCount{0};
};

struct AntiCheatSubsystemVitals {
    uint32_t protectedProcessesCount{0};
    uint32_t totalHandleAttemptsIntercepted{0};
    uint32_t totalHandleAccessBitsStripped{0};
    uint32_t totalRemoteThreadsBlocked{0};
    uint32_t totalUnauthorizedImagesBlocked{0};
    bool obCallbacksActive{true};
    bool processNotifyActive{true};
    bool threadNotifyActive{true};
    bool imageNotifyActive{true};
    uint32_t filterAltitude{328000}; // Canonical Anti-Cheat Filter Altitude
};

// ============================================================================
// 3. Sovereign Kernel Anti-Cheat Subsystem Manager (Singleton)
// ============================================================================

class AntiCheatManager {
public:
    static AntiCheatManager& get() {
        static AntiCheatManager instance;
        return instance;
    }

    // Reset subsystem to clean default state (testing & lifecycle)
    void reset() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        protectedProcesses_.clear();
        whitelistedOverlays_.clear();
        totalHandleAttemptsIntercepted_ = 0;
        totalHandleAccessBitsStripped_ = 0;
        totalRemoteThreadsBlocked_ = 0;
        totalUnauthorizedImagesBlocked_ = 0;
        filterAltitude_ = 328000;
        obCallbacksActive_ = true;
        processNotifyActive_ = true;
        threadNotifyActive_ = true;
        imageNotifyActive_ = true;

        // Initialize whitelisted game overlays
        whitelistedOverlays_.insert("GameOverlayRenderer64.dll"); // Steam Overlay
        whitelistedOverlays_.insert("DiscordHook64.dll");          // Discord Overlay
        whitelistedOverlays_.insert("nvspcap64.dll");              // Nvidia GeForce Experience Overlay
        whitelistedOverlays_.insert("PrismX_Hud64.dll");           // Sovereign PrismX Graphics HUD

        // Seed Canonical Protected Game Process: PID 5400 (cod.exe)
        {
            ProtectedProcessEntry gameEntry{};
            gameEntry.processId = 5400;
            gameEntry.processName = "cod.exe";
            gameEntry.binaryPath = "C:\\Games\\CallOfDuty\\cod.exe";
            gameEntry.binaryHashSha256 = "8f4e2b1c9a8d7e6f5a4b3c2d1e0f9a8b7c6d5e4f3a2b1c0d9e8f7a6b5c4d3e2f";
            gameEntry.level = ProcessProtectionLevel::AntiCheatProtected;
            gameEntry.protectedStartTimeEpoch = 1730000000;
            protectedProcesses_[gameEntry.processId] = gameEntry;
        }
    }

    // Register / Protect Process
    bool protectProcess(uint32_t processId,
                        std::string_view processName,
                        std::string_view binaryPath = "",
                        ProcessProtectionLevel level = ProcessProtectionLevel::AntiCheatProtected) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        if (processId == 0) return false;

        ProtectedProcessEntry entry{};
        entry.processId = processId;
        entry.processName = processName.empty() ? ("Process_" + std::to_string(processId)) : std::string(processName);
        entry.binaryPath = binaryPath.empty() ? ("C:\\Windows\\System32\\" + entry.processName) : std::string(binaryPath);
        entry.level = level;
        entry.protectedStartTimeEpoch = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()
        );

        protectedProcesses_[processId] = entry;
        return true;
    }

    // Unregister Process
    bool unprotectProcess(uint32_t processId) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        return protectedProcesses_.erase(processId) > 0;
    }

    bool isProcessProtected(uint32_t processId) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return protectedProcesses_.find(processId) != protectedProcesses_.end();
    }

    // Core ObRegisterCallbacks Handle Filtering Engine
    // Filters incoming DesiredAccess and strips dangerous access masks
    uint32_t filterProcessHandleCreation(uint32_t callerProcessId,
                                         uint32_t targetProcessId,
                                         uint32_t desiredAccess) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = protectedProcesses_.find(targetProcessId);
        if (it == protectedProcesses_.end()) {
            return desiredAccess; // Unprotected target: allow requested access
        }

        it->second.handleOpenAttemptsCount++;
        totalHandleAttemptsIntercepted_++;

        // Self-access or system process (csrss.exe / lsass.exe / caller == target)
        if (callerProcessId == targetProcessId || callerProcessId == 4 /* System */) {
            return desiredAccess;
        }

        // External process attempting to access protected game process
        uint32_t maskedAccess = desiredAccess;
        if ((maskedAccess & DANGEROUS_PROCESS_ACCESS_MASK) != 0) {
            // Strip dangerous read/write/injection bits
            maskedAccess &= ~DANGEROUS_PROCESS_ACCESS_MASK;
            // Ensure minimum safe query information is preserved
            maskedAccess |= PROCESS_QUERY_LIMITED_INFORMATION;

            it->second.handleAccessStrippedCount++;
            totalHandleAccessBitsStripped_++;
        }

        return maskedAccess;
    }

    // Filter Thread Handle Access
    uint32_t filterThreadHandleCreation(uint32_t callerProcessId,
                                        uint32_t targetThreadProcessId,
                                        uint32_t desiredAccess) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = protectedProcesses_.find(targetThreadProcessId);
        if (it == protectedProcesses_.end()) {
            return desiredAccess;
        }

        if (callerProcessId == targetThreadProcessId || callerProcessId == 4 /* System */) {
            return desiredAccess;
        }

        uint32_t masked = desiredAccess;
        if ((masked & DANGEROUS_THREAD_ACCESS_MASK) != 0) {
            masked &= ~DANGEROUS_THREAD_ACCESS_MASK;
            masked |= THREAD_QUERY_INFORMATION;
            totalHandleAccessBitsStripped_++;
        }
        return masked;
    }

    // PsSetCreateThreadNotifyRoutine: Intercepts & Blocks Remote Thread Injections
    bool validateThreadCreation(uint32_t creatorProcessId, uint32_t targetProcessId) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = protectedProcesses_.find(targetProcessId);
        if (it == protectedProcesses_.end()) {
            return true; // Target is not protected, allow thread creation
        }

        // Thread created internally by the process itself or System is permitted
        if (creatorProcessId == targetProcessId || creatorProcessId == 4) {
            return true;
        }

        // Cross-process remote thread injection detected (CreateRemoteThread)!
        it->second.remoteThreadBlocksCount++;
        totalRemoteThreadsBlocked_++;
        return false; // BLOCK INJECTION
    }

    // PsSetLoadImageNotifyRoutine: Validates DLL Loads & Whitelist Overlays
    bool validateImageLoad(uint32_t targetProcessId, std::string_view imageName, bool isSigned) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        auto it = protectedProcesses_.find(targetProcessId);
        if (it == protectedProcesses_.end()) {
            return true; // Target not protected
        }

        // Extract filename from full path
        std::string filename(imageName);
        size_t lastSlash = filename.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            filename = filename.substr(lastSlash + 1);
        }

        // Check if image is an allowed overlay
        if (whitelistedOverlays_.find(filename) != whitelistedOverlays_.end()) {
            return true; // Permitted gaming overlay (Steam/Discord/PrismX)
        }

        // If signed by trusted authority, allow
        if (isSigned) {
            return true;
        }

        // Unsigned hook DLL or unknown injected library -> BLOCK
        it->second.blockedImageInjectionsCount++;
        totalUnauthorizedImagesBlocked_++;
        return false;
    }

    // Overlay Management
    bool addWhitelistedOverlay(std::string_view dllName) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        if (dllName.empty()) return false;
        return whitelistedOverlays_.insert(std::string(dllName)).second;
    }

    bool isOverlayWhitelisted(std::string_view dllName) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return whitelistedOverlays_.find(std::string(dllName)) != whitelistedOverlays_.end();
    }

    // Query Operations
    AntiCheatSubsystemVitals getVitals() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        AntiCheatSubsystemVitals v{};
        v.protectedProcessesCount = static_cast<uint32_t>(protectedProcesses_.size());
        v.totalHandleAttemptsIntercepted = totalHandleAttemptsIntercepted_;
        v.totalHandleAccessBitsStripped = totalHandleAccessBitsStripped_;
        v.totalRemoteThreadsBlocked = totalRemoteThreadsBlocked_;
        v.totalUnauthorizedImagesBlocked = totalUnauthorizedImagesBlocked_;
        v.obCallbacksActive = obCallbacksActive_;
        v.processNotifyActive = processNotifyActive_;
        v.threadNotifyActive = threadNotifyActive_;
        v.imageNotifyActive = imageNotifyActive_;
        v.filterAltitude = filterAltitude_;
        return v;
    }

    std::vector<ProtectedProcessEntry> getAllProtectedProcesses() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        std::vector<ProtectedProcessEntry> list;
        list.reserve(protectedProcesses_.size());
        for (const auto& [_, e] : protectedProcesses_) {
            list.push_back(e);
        }
        return list;
    }

    bool getProtectedProcess(uint32_t processId, ProtectedProcessEntry& outEntry) const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto it = protectedProcesses_.find(processId);
        if (it != protectedProcesses_.end()) {
            outEntry = it->second;
            return true;
        }
        return false;
    }

private:
    AntiCheatManager() {
        reset();
    }

    mutable std::shared_mutex mutex_;
    std::unordered_map<uint32_t, ProtectedProcessEntry> protectedProcesses_;
    std::unordered_set<std::string> whitelistedOverlays_;
    uint32_t totalHandleAttemptsIntercepted_{0};
    uint32_t totalHandleAccessBitsStripped_{0};
    uint32_t totalRemoteThreadsBlocked_{0};
    uint32_t totalUnauthorizedImagesBlocked_{0};
    uint32_t filterAltitude_{328000};
    bool obCallbacksActive_{true};
    bool processNotifyActive_{true};
    bool threadNotifyActive_{true};
    bool imageNotifyActive_{true};
};

// ============================================================================
// 4. Subsystem SCM & VersionDatabase Registration
// ============================================================================

inline void RegisterAntiCheatSubsystem() {
    // 1. SCM Service: AntiCheat ("Kernel Anti-Cheat and Memory Protection Filter Driver")
    auto& scm = scm::ServiceControlManager::get();
    auto acRecord = std::make_shared<scm::ServiceRecord>();
    acRecord->serviceName = L"AntiCheat";
    acRecord->displayName = L"Kernel Anti-Cheat and Memory Protection Filter Driver";
    acRecord->serviceType = scm::SERVICE_KERNEL_DRIVER;
    acRecord->startType = scm::SERVICE_SYSTEM_START;
    acRecord->errorControl = scm::SERVICE_ERROR_NORMAL;
    acRecord->binaryPath = L"C:\\Windows\\System32\\drivers\\anticheat.sys";
    acRecord->status.dwServiceType = acRecord->serviceType;
    acRecord->status.dwCurrentState = scm::SERVICE_RUNNING;
    acRecord->status.dwProcessId = 0;
    scm.registerServiceRecord(acRecord);

    // 2. Version Database PE Module Registration (6 modules)
    auto& vdb = micant::version::VersionDatabase::Instance();
    vdb.RegisterModule("anticheat.sys", "10.0.26100.1", "Kernel Anti-Cheat and Memory Protection Filter Driver");
    vdb.RegisterModule("accore.dll",    "10.0.26100.1", "Anti-Cheat User-Mode Client Runtime");
    vdb.RegisterModule("accli.exe",     "10.0.26100.1", "Anti-Cheat Diagnostics and Security Administration Tool");
    vdb.RegisterModule("acfilter.sys",  "10.0.26100.1", "Object Manager Handle Filtering Extension Driver");
    vdb.RegisterModule("acapi.dll",     "10.0.26100.1", "Anti-Cheat Management and Telemetry API");
    vdb.RegisterModule("achook.dll",    "10.0.26100.1", "Process Integrity and Image Verification Library");

    // 3. Reset Manager to clean state
    AntiCheatManager::get().reset();
}

} // namespace micant::anticheat

// ============================================================================
// 5. Sovereign Win32 C ABI Exports (acapi.dll)
// ============================================================================

extern "C" {

inline uint32_t AntiCheatGetProtectedProcessesCount() {
    return micant::anticheat::AntiCheatManager::get().getVitals().protectedProcessesCount;
}

inline int32_t AntiCheatProtectProcess(uint32_t pid, const char* name) {
    if (!name) return -1;
    bool ok = micant::anticheat::AntiCheatManager::get().protectProcess(pid, name);
    return ok ? 0 : -1;
}

inline int32_t AntiCheatUnprotectProcess(uint32_t pid) {
    bool ok = micant::anticheat::AntiCheatManager::get().unprotectProcess(pid);
    return ok ? 0 : -1;
}

inline uint32_t AntiCheatFilterHandleAccess(uint32_t callerPid, uint32_t targetPid, uint32_t desiredAccess) {
    return micant::anticheat::AntiCheatManager::get().filterProcessHandleCreation(callerPid, targetPid, desiredAccess);
}

inline int32_t AntiCheatValidateThreadCreation(uint32_t creatorPid, uint32_t targetPid) {
    bool ok = micant::anticheat::AntiCheatManager::get().validateThreadCreation(creatorPid, targetPid);
    return ok ? 0 : -1; // 0 = allowed, -1 = blocked
}

inline int32_t AntiCheatValidateImageLoad(uint32_t targetPid, const char* imageName, int32_t isSigned) {
    if (!imageName) return -1;
    bool ok = micant::anticheat::AntiCheatManager::get().validateImageLoad(targetPid, imageName, isSigned != 0);
    return ok ? 0 : -1;
}

} // extern "C"
