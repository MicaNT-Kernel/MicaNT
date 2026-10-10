#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <atomic>
#include <unordered_map>
#include <chrono>
#include "ntdef.hpp"
#include "ntstatus.hpp"
#include "ob.hpp"
#include "mm.hpp"
#include "se.hpp"

namespace micant::ps {

// ============================================================================
// 1. Thread Execution State & Security Impersonation Level
// ============================================================================
enum class ThreadState : uint32_t {
    Initialized,
    Ready,
    Running,
    Standby,
    Terminated,
    Waiting,
    Transition,
    DeferredReady
};

enum class SecurityImpersonationLevel : uint32_t {
    SecurityAnonymous     = 0,
    SecurityIdentification= 1,
    SecurityImpersonation = 2,
    SecurityDelegation    = 3
};

/**
 * @brief x86-64 Machine Architecture Context Frame.
 */
struct ContextFrame {
    uint64_t rip{0};
    uint64_t rsp{0};
    uint64_t rflags{0x202}; // IF (Interrupt Flag) enabled
    uint64_t rax{0};
    uint64_t rcx{0};
    uint64_t rdx{0};
    uint64_t rbx{0};
    uint64_t rbp{0};
    uint64_t rsi{0};
    uint64_t rdi{0};
    uint64_t r8{0};
    uint64_t r9{0};
    uint64_t r10{0};
    uint64_t r11{0};
    uint64_t r12{0};
    uint64_t r13{0};
    uint64_t r14{0};
    uint64_t r15{0};
    uint16_t cs{0x33}; // User code segment (Ring 3 64-bit)
    uint16_t ss{0x2B}; // User data segment (Ring 3)
};

/**
 * @brief Process Environment Block (PEB).
 * Standard userland structure pointed to by TEB->ProcessEnvironmentBlock (GS:[0x60]).
 */
struct Peb {
    uint8_t  inheritedAddressSpace{0};
    uint8_t  readImageFileExecOptions{0};
    uint8_t  beingDebugged{0};
    uint8_t  bitField{0};
    uint64_t mutant{0};
    uint64_t imageBaseAddress{0};
    uint64_t ldr{0};
    uint64_t processParameters{0};
    uint64_t subSystemData{0};
    uint64_t processHeap{0};
    uint64_t fastPebLock{0};
    uint32_t numberOfProcessors{1};
    uint32_t ntGlobalFlag{0};
    uint16_t osMajorVersion{10};
    uint16_t osMinorVersion{0};
    uint16_t osBuildNumber{26100}; // Windows 11 base build
    uint16_t osCsdVersion{0};
    uint32_t osPlatformId{2};      // VER_PLATFORM_WIN32_NT
    uint32_t imageSubsystem{3};    // IMAGE_SUBSYSTEM_WINDOWS_CUI
    uint32_t imageSubsystemMajorVersion{10};
    uint32_t imageSubsystemMinorVersion{0};
    uint32_t sessionId{0};
};

/**
 * @brief Thread Environment Block (TEB).
 * Standard userland per-thread block mapped at GS:[0x30] on x86_64.
 */
struct Teb {
    struct {
        uint64_t exceptionList{0};
        uint64_t stackBase{0};
        uint64_t stackLimit{0};
        uint64_t subSystemTib{0};
        uint64_t fiberData{0};
        uint64_t arbitraryUserPointer{0};
        uint64_t self{0}; // Points to TEB itself (GS:[0x30])
    } ntTib;

    uint64_t environmentPointer{0};
    ClientId clientId{};
    uint64_t activeRpcHandle{0};
    uint64_t threadLocalStoragePointer{0};
    uint64_t processEnvironmentBlock{0}; // Pointer to PEB (GS:[0x60])
    uint32_t lastErrorValue{0};
    uint32_t countOfOwnedCriticalSections{0};
    uint32_t hardErrorMode{0};
};

class EProcess;

/**
 * @brief Executive Thread (ETHREAD).
 * Represents a single execution thread within a process.
 */
class EThread {
public:
    EThread(Handle tid, EProcess* owner, uintptr_t entryPoint, uintptr_t stackBase, uintptr_t stackLimit)
        : tid_(tid), owner_(owner), stackBase_(stackBase), stackLimit_(stackLimit), state_(ThreadState::Initialized) {
        context_.rip = entryPoint;
        context_.rsp = stackBase - 0x28; // Standard 32-byte shadow space + return address align
        createTime_ = std::chrono::steady_clock::now();
    }

    [[nodiscard]] Handle getTid() const noexcept { return tid_; }
    [[nodiscard]] EProcess* getOwnerProcess() const noexcept { return owner_; }
    [[nodiscard]] ThreadState getState() const noexcept { return state_; }
    void setState(ThreadState s) noexcept { state_ = s; }

    [[nodiscard]] ContextFrame& getContext() noexcept { return context_; }
    [[nodiscard]] const ContextFrame& getContext() const noexcept { return context_; }

    [[nodiscard]] uintptr_t getStackBase() const noexcept { return stackBase_; }
    [[nodiscard]] uintptr_t getStackLimit() const noexcept { return stackLimit_; }
    [[nodiscard]] uintptr_t getTebAddress() const noexcept { return tebAddress_; }
    void setTebAddress(uintptr_t addr) noexcept { tebAddress_ = addr; }

    [[nodiscard]] const Teb& getTeb() const noexcept { return teb_; }
    [[nodiscard]] Teb& getTeb() noexcept { return teb_; }
    void setTeb(const Teb& teb) noexcept { teb_ = teb; }

    [[nodiscard]] uint32_t getPriority() const noexcept { return priority_; }
    void setPriority(uint32_t p) noexcept { priority_ = p; }


    [[nodiscard]] uint32_t getBasePriority() const noexcept { return basePriority_; }
    void setBasePriority(uint32_t p) noexcept { basePriority_ = p; priority_ = p; }

    [[nodiscard]] NtStatus getExitStatus() const noexcept { return exitStatus_; }
    void setExitStatus(NtStatus status) noexcept { exitStatus_ = status; }

    // Thread Impersonation
    void impersonateToken(std::shared_ptr<se::TokenObject> token, SecurityImpersonationLevel level) noexcept {
        impersonationToken_ = std::move(token);
        impersonationLevel_ = level;
    }

    void revertToSelf() noexcept {
        impersonationToken_.reset();
        impersonationLevel_ = SecurityImpersonationLevel::SecurityAnonymous;
    }

    [[nodiscard]] bool isImpersonating() const noexcept {
        return impersonationToken_ != nullptr;
    }

    [[nodiscard]] std::shared_ptr<se::TokenObject> getActiveToken();

    [[nodiscard]] SecurityImpersonationLevel getImpersonationLevel() const noexcept {
        return impersonationLevel_;
    }

private:
    Handle tid_{0};
    EProcess* owner_{nullptr};
    uintptr_t stackBase_{0};
    uintptr_t stackLimit_{0};
    uintptr_t tebAddress_{0};
    ThreadState state_{ThreadState::Initialized};
    ContextFrame context_{};
    uint32_t priority_{8};
    uint32_t basePriority_{8};
    NtStatus exitStatus_{NtStatus::Success};
    std::shared_ptr<se::TokenObject> impersonationToken_;
    SecurityImpersonationLevel impersonationLevel_{SecurityImpersonationLevel::SecurityAnonymous};
    Teb teb_{};
    std::chrono::steady_clock::time_point createTime_;
};

/**
 * @brief Executive Process (EPROCESS).
 * The primary container for address space, handles, security tokens, and threads.
 */
class EProcess {
public:
    EProcess(Handle pid, std::wstring imageFileName, Handle parentPid = 0)
        : pid_(pid), parentPid_(parentPid), imageFileName_(std::move(imageFileName)),
          directoryTableBase_(0x1000 + (static_cast<uint64_t>(pid) * 0x1000)),
          exitStatus_(NtStatus::Success), terminated_(false) {
        createTime_ = std::chrono::steady_clock::now();
    }

    [[nodiscard]] Handle getPid() const noexcept { return pid_; }
    [[nodiscard]] Handle getParentPid() const noexcept { return parentPid_; }
    void setParentPid(Handle parent) noexcept { parentPid_ = parent; }

    [[nodiscard]] const std::wstring& getImageFileName() const noexcept { return imageFileName_; }

    [[nodiscard]] mm::ProcessAddressSpace& getAddressSpace() noexcept { return addressSpace_; }
    [[nodiscard]] ob::HandleTable& getHandleTable() noexcept { return handleTable_; }

    [[nodiscard]] uintptr_t getDirectoryTableBase() const noexcept { return directoryTableBase_; }
    void setDirectoryTableBase(uintptr_t cr3) noexcept { directoryTableBase_ = cr3; }

    [[nodiscard]] uintptr_t getImageBase() const noexcept { return imageBase_; }
    void setImageBase(uintptr_t base) noexcept { imageBase_ = base; }

    [[nodiscard]] uintptr_t getEntryPoint() const noexcept { return entryPoint_; }
    void setEntryPoint(uintptr_t ep) noexcept { entryPoint_ = ep; }

    [[nodiscard]] uintptr_t getPebAddress() const noexcept { return pebAddress_; }
    void setPebAddress(uintptr_t addr) noexcept { pebAddress_ = addr; }

    [[nodiscard]] const Peb& getPeb() const noexcept { return peb_; }
    [[nodiscard]] Peb& getPeb() noexcept { return peb_; }
    void setPeb(const Peb& peb) noexcept { peb_ = peb; }

    [[nodiscard]] bool isTerminated() const noexcept { return terminated_; }
    [[nodiscard]] NtStatus getExitStatus() const noexcept { return exitStatus_; }

    void terminate(NtStatus status) noexcept {
        exitStatus_ = status;
        terminated_ = true;
        for (auto& t : threads_) {
            t->setState(ThreadState::Terminated);
            t->setExitStatus(status);
        }
    }

    std::shared_ptr<EThread> createThread(uintptr_t entryPoint, size_t stackSize = 1024 * 1024) {
        static std::atomic<uint32_t> s_GlobalTid{1};
        Handle tid = static_cast<Handle>(s_GlobalTid.fetch_add(1, std::memory_order_relaxed));

        
        // Allocate stack in process address space
        uintptr_t stackBase = 0;
        addressSpace_.allocate(stackBase, stackSize, mm::MEM_RESERVE | mm::MEM_COMMIT, mm::PAGE_READWRITE);
        uintptr_t stackTop = stackBase + stackSize;

        // Allocate and setup TEB in process address space
        uintptr_t tebAddr = 0;
        addressSpace_.allocate(tebAddr, sizeof(Teb), mm::MEM_RESERVE | mm::MEM_COMMIT, mm::PAGE_READWRITE);

        auto thread = std::make_shared<EThread>(tid, this, entryPoint, stackTop, stackBase);
        thread->setTebAddress(tebAddr);

        // Populate TEB structure
        Teb userTeb{};
        userTeb.ntTib.self = tebAddr;
        userTeb.ntTib.stackBase = stackTop;
        userTeb.ntTib.stackLimit = stackBase;
        userTeb.clientId.uniqueProcess = pid_;
        userTeb.clientId.uniqueThread = tid;
        userTeb.processEnvironmentBlock = pebAddress_;
        thread->setTeb(userTeb);

        threads_.push_back(thread);
        return thread;
    }

    [[nodiscard]] std::shared_ptr<se::TokenObject> getToken() const noexcept { return token_; }
    void setToken(std::shared_ptr<se::TokenObject> token) noexcept { token_ = std::move(token); }

    [[nodiscard]] const std::vector<std::shared_ptr<EThread>>& getThreads() const noexcept {
        return threads_;
    }

    [[nodiscard]] size_t getActiveThreadCount() const noexcept {
        size_t count = 0;
        for (const auto& t : threads_) {
            if (t->getState() != ThreadState::Terminated) ++count;
        }
        return count;
    }

private:
    Handle pid_{0};
    Handle parentPid_{0};
    std::wstring imageFileName_;
    uintptr_t directoryTableBase_{0x1000};
    mm::ProcessAddressSpace addressSpace_;
    ob::HandleTable handleTable_;
    std::shared_ptr<se::TokenObject> token_;
    uintptr_t imageBase_{0};
    uintptr_t entryPoint_{0};
    uintptr_t pebAddress_{0};
    Peb peb_{};
    NtStatus exitStatus_{NtStatus::Success};
    bool terminated_{false};
    std::vector<std::shared_ptr<EThread>> threads_;
    std::chrono::steady_clock::time_point createTime_;
};



inline std::shared_ptr<se::TokenObject> EThread::getActiveToken() {
    if (impersonationToken_) return impersonationToken_;
    if (owner_) return owner_->getToken();
    return nullptr;
}

// ============================================================================
// 2. Process & Thread Manager Engine (Ps Subsystem)
// ============================================================================
class ProcessManager {
public:
    static ProcessManager& get() {
        static ProcessManager instance;
        return instance;
    }

    std::shared_ptr<EProcess> createProcess(
        std::wstring_view imageName,
        std::shared_ptr<se::TokenObject> token = nullptr,
        Handle parentPid = 0
    ) {
        Handle pid = static_cast<Handle>(nextPid_++);
        auto proc = std::make_shared<EProcess>(pid, std::wstring(imageName), parentPid);

        // Allocate and setup PEB at standard base
        uintptr_t pebAddr = 0x00007FFDF0000000ULL + (static_cast<uint64_t>(pid) * 0x10000);
        proc->getAddressSpace().allocate(pebAddr, sizeof(Peb), mm::MEM_COMMIT | mm::MEM_RESERVE, mm::PAGE_READWRITE);
        proc->setPebAddress(pebAddr);

        // Populate PEB defaults
        Peb userPeb{};
        userPeb.numberOfProcessors = 4;
        userPeb.osMajorVersion = 10;
        userPeb.osMinorVersion = 0;
        userPeb.osBuildNumber = 26100;
        userPeb.osPlatformId = 2;
        userPeb.imageSubsystem = 3;
        proc->setPeb(userPeb);


        if (token) {
            proc->setToken(std::move(token));
        } else {
            proc->setToken(se::TokenObject::createSystemToken());
        }

        processes_[pid] = proc;
        return proc;
    }

    void initializeSystemProcesses() {
        if (initializedSystem_) return;
        initializedSystem_ = true;

        // 1. PID 0: System Idle Process (Idle)
        idleProcess_ = std::make_shared<EProcess>(0, L"Idle");
        auto idleThread = idleProcess_->createThread(0);
        idleThread->setState(ThreadState::Running);
        idleThread->setBasePriority(0);
        processes_[0] = idleProcess_;

        // 2. PID 4: System Process (System / ntoskrnl.exe)
        systemProcess_ = std::make_shared<EProcess>(4, L"System");
        systemProcess_->setToken(se::TokenObject::createSystemToken());
        auto sysWorkerThread = systemProcess_->createThread(0);
        sysWorkerThread->setState(ThreadState::Ready);
        sysWorkerThread->setBasePriority(8);
        processes_[4] = systemProcess_;
    }

    [[nodiscard]] std::shared_ptr<EProcess> getIdleProcess() const noexcept {
        return idleProcess_;
    }

    [[nodiscard]] std::shared_ptr<EProcess> getSystemProcess() const noexcept {
        return systemProcess_;
    }

    [[nodiscard]] std::shared_ptr<EProcess> getProcess(Handle pid) const {
        auto it = processes_.find(pid);
        if (it != processes_.end()) return it->second;
        return nullptr;
    }

    [[nodiscard]] std::shared_ptr<EThread> getThread(Handle tid) const {
        for (const auto& [pid, proc] : processes_) {
            for (const auto& t : proc->getThreads()) {
                if (t->getTid() == tid) return t;
            }
        }
        return nullptr;
    }

    [[nodiscard]] size_t getActiveProcessCount() const noexcept {
        return processes_.size();
    }

private:
    ProcessManager() : nextPid_(1000) {
        initializeSystemProcesses();
    }
    bool initializedSystem_{false};
    uint32_t nextPid_;
    std::shared_ptr<EProcess> idleProcess_;
    std::shared_ptr<EProcess> systemProcess_;
    std::unordered_map<Handle, std::shared_ptr<EProcess>> processes_;
};

// ============================================================================
// 3. Standard NT Executive Process/Thread API (Ps*)
// ============================================================================
inline NtStatus PsLookupProcessByProcessId(Handle pid, std::shared_ptr<EProcess>& outProcess) {
    outProcess = ProcessManager::get().getProcess(pid);
    if (!outProcess) return NtStatus::NoSuchProcess;
    return NtStatus::Success;
}

inline NtStatus PsLookupThreadByThreadId(Handle tid, std::shared_ptr<EThread>& outThread) {
    outThread = ProcessManager::get().getThread(tid);
    if (!outThread) return NtStatus::InvalidParameter;
    return NtStatus::Success;
}

inline Handle PsGetProcessId(const EProcess* process) {
    return process ? process->getPid() : 0;
}

inline Handle PsGetThreadId(const EThread* thread) {
    return thread ? thread->getTid() : 0;
}

inline std::wstring_view PsGetProcessImageFileName(const EProcess* process) {
    if (process) {
        return process->getImageFileName();
    }
    return std::wstring_view{};
}


} // namespace micant::ps
