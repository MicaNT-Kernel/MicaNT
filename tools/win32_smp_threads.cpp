/**
 * @file win32_smp_threads.cpp
 * @brief Sovereign Multi-Core Ring-3 Win32 Threading Verification Executable
 *        (C:\Windows\System32\win32_smp_threads.exe).
 *
 * Freestanding ISO C++23. Clean-Room design. ZERO direct `syscall` instructions!
 * Dynamically imports CreateThread, WaitForSingleObject, GetExitCodeThread,
 * GetCurrentProcessorNumber, GetCurrentProcessId, GetCurrentThreadId,
 * VirtualAlloc, WriteFile, CloseHandle, ExitProcess strictly via KERNEL32.dll IAT.
 *
 * Proves authentic 4-core parallel Ring-3 execution across physical APICs 0, 1, 2, 3:
 *   - Spawns 3 real Win32 threads via CreateThread -> dispatched via Vector 0x3E IPI
 *     onto physical AP Cores 1, 2, 3 in CPL=3 sharing the same private CR3!
 *   - Synchronizes all 4 Ring-3 cores at a shared-memory barrier
 *   - Races all 4 physical cores on a pure Ring-3 userland Ticket Spinlock
 *     (4 x 50,000 = 200,000 atomic lock-protected increments, 0 lost updates!)
 *   - Computes parallel 4-core sum over [1..1,000,000] == 500,000,500,000
 *   - Verifies per-thread TEB GS_BASE (shared PID, 4 unique TIDs), real hardware
 *     GetCurrentProcessorNumber() == {0, 1, 2, 3}, and GetExitCodeThread() checksums!
 */

#include <cstdint>
#include <cstddef>

extern "C" {

__declspec(dllimport) void*    GetStdHandle(uint32_t nStdHandle) noexcept;
__declspec(dllimport) int32_t  WriteFile(void* hFile, const void* buf, uint32_t len, uint32_t* written, void* ov) noexcept;
__declspec(dllimport) void*    VirtualAlloc(void* addr, size_t sz, uint32_t type, uint32_t prot) noexcept;
__declspec(dllimport) int32_t  VirtualFree(void* addr, size_t sz, uint32_t freeType) noexcept;
__declspec(dllimport) uint32_t GetCurrentProcessId() noexcept;
__declspec(dllimport) uint32_t GetCurrentThreadId() noexcept;
__declspec(dllimport) uint32_t GetCurrentProcessorNumber() noexcept;
__declspec(dllimport) void*    CreateThread(
    void* sec, size_t stackSize, uint32_t (*startAddr)(void*), void* param, uint32_t flags, uint32_t* outTid
) noexcept;
__declspec(dllimport) uint32_t WaitForSingleObject(void* hHandle, uint32_t dwMilliseconds) noexcept;
__declspec(dllimport) int32_t  GetExitCodeThread(void* hThread, uint32_t* lpExitCode) noexcept;
__declspec(dllimport) int32_t  CloseHandle(void* hObject) noexcept;
__declspec(dllimport) void     ExitProcess(uint32_t uExitCode) noexcept;

} // extern "C"

namespace {

static constexpr uint32_t STD_OUTPUT_HANDLE = 0xFFFFFFF5u;
static constexpr uint32_t MEM_COMMIT_RESERVE = 0x3000u;
static constexpr uint32_t PAGE_READWRITE = 0x04u;
static constexpr uint32_t INFINITE_WAIT = 0xFFFFFFFFu;
static constexpr uint32_t R3_LOCK_ITERS_PER_CORE = 50000u;
static constexpr uint64_t TOTAL_SUM_N = 1000000ULL;
static constexpr uint64_t EXPECTED_SUM = (TOTAL_SUM_N * (TOTAL_SUM_N + 1ULL)) / 2ULL; // 500,000,500,000

struct SmpSharedContext;

struct alignas(64) WorkerArgs {
    SmpSharedContext* ctx{nullptr};
    uint32_t workerIdx{0};
    uint32_t observedApic{0xFFFFFFFFu};
    uint32_t observedPid{0};
    uint32_t observedTid{0};
    uint64_t rangeStart{0};
    uint64_t rangeEnd{0};
    uint64_t partialSum{0};
    uint32_t lockItersDone{0};
    volatile uint32_t completed{0};
};

struct alignas(64) SmpSharedContext {
    alignas(64) volatile uint32_t barrierReady{0};
    alignas(64) volatile uint32_t barrierTarget{4};
    alignas(64) volatile uint32_t r3NextTicket{0};
    alignas(64) volatile uint32_t r3NowServing{0};
    alignas(64) volatile uint64_t r3SharedCounter{0};
    WorkerArgs workers[4]{};
};

static inline void R3SpinLock(SmpSharedContext* ctx) noexcept {
    uint32_t ticket = __atomic_fetch_add(&ctx->r3NextTicket, 1u, __ATOMIC_RELAXED);
    while (__atomic_load_n(&ctx->r3NowServing, __ATOMIC_ACQUIRE) != ticket) {
#if defined(__x86_64__)
        __asm__ __volatile__("pause" ::: "memory");
#endif
    }
}

static inline void R3SpinUnlock(SmpSharedContext* ctx) noexcept {
    uint32_t cur = __atomic_load_n(&ctx->r3NowServing, __ATOMIC_RELAXED);
    __atomic_store_n(&ctx->r3NowServing, cur + 1u, __ATOMIC_RELEASE);
}

static uint32_t WorkerThreadMain(void* rawParam) noexcept {
    auto* w = static_cast<WorkerArgs*>(rawParam);
    auto* ctx = w->ctx;

    // 1. Query real hardware CPU core number via kernel32 -> ntdll!NtGetCurrentProcessorNumber (0x5A)
    //    AND read per-thread TEB (IA32_GS_BASE) via GetCurrentProcessId() / GetCurrentThreadId()!
    uint32_t myApic = GetCurrentProcessorNumber();
    uint32_t myPid  = GetCurrentProcessId();
    uint32_t myTid  = GetCurrentThreadId();
    w->observedApic = myApic;
    w->observedPid  = myPid;
    w->observedTid  = myTid;

    // 2. Synchronize ALL active Ring-3 physical cores at the shared-memory barrier!
    __atomic_fetch_add(&ctx->barrierReady, 1u, __ATOMIC_ACQ_REL);
    uint32_t target = __atomic_load_n(&ctx->barrierTarget, __ATOMIC_ACQUIRE);
    for (uint32_t spin = 0; spin < 50000000u; ++spin) {
        if (__atomic_load_n(&ctx->barrierReady, __ATOMIC_ACQUIRE) >= target) break;
#if defined(__x86_64__)
        __asm__ __volatile__("pause" ::: "memory");
#endif
    }

    // 3. Race all other physical cores on a 100% pure Ring-3 userland Ticket Spinlock!
    for (uint32_t i = 0; i < R3_LOCK_ITERS_PER_CORE; ++i) {
        R3SpinLock(ctx);
        ctx->r3SharedCounter = ctx->r3SharedCounter + 1ULL;
        R3SpinUnlock(ctx);
    }
    w->lockItersDone = R3_LOCK_ITERS_PER_CORE;

    // 4. Compute parallel arithmetic sum slice [rangeStart..rangeEnd]
    uint64_t sum = 0;
    for (uint64_t v = w->rangeStart; v <= w->rangeEnd; ++v) {
        sum += v;
    }
    w->partialSum = sum;
    __atomic_store_n(&w->completed, 1u, __ATOMIC_RELEASE);

    // Return unique hardware-core checksum -> BaseThreadStartThunk -> ExitThread -> NtTerminateThread(0x5B)!
    return 0xC0DE0000u | (myApic & 0xFFu);
}

static size_t StrLen(const char* s) noexcept {
    size_t n = 0;
    while (s[n]) ++n;
    return n;
}

static void AppendStr(char* dst, size_t cap, const char* src) noexcept {
    size_t d = StrLen(dst);
    size_t i = 0;
    while (src[i] && d + 1 < cap) dst[d++] = src[i++];
    dst[d] = '\0';
}

static void AppendU64(char* dst, size_t cap, uint64_t val) noexcept {
    char tmp[24]{};
    size_t n = 0;
    if (val == 0) tmp[n++] = '0';
    else {
        while (val > 0 && n < 22) {
            tmp[n++] = static_cast<char>('0' + (val % 10));
            val /= 10;
        }
    }
    char rev[24]{};
    for (size_t i = 0; i < n; ++i) rev[i] = tmp[n - 1 - i];
    rev[n] = '\0';
    AppendStr(dst, cap, rev);
}

static void AppendHex32(char* dst, size_t cap, uint32_t val) noexcept {
    static const char hex[] = "0123456789ABCDEF";
    char buf[11];
    buf[0] = '0'; buf[1] = 'x';
    for (int i = 0; i < 8; ++i) buf[2 + i] = hex[(val >> ((7 - i) * 4)) & 0xFu];
    buf[10] = '\0';
    AppendStr(dst, cap, buf);
}

static void PrintOut(void* hOut, const char* s) noexcept {
    uint32_t wr = 0;
    WriteFile(hOut, s, static_cast<uint32_t>(StrLen(s)), &wr, nullptr);
}

} // namespace

extern "C" void WinMainEntry() noexcept {
    void* hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    uint32_t mainPid = GetCurrentProcessId();

    auto* ctx = static_cast<SmpSharedContext*>(
        VirtualAlloc(nullptr, sizeof(SmpSharedContext) + 4096, MEM_COMMIT_RESERVE, PAGE_READWRITE)
    );
    if (!ctx) {
        PrintOut(hOut, "[SMP-R3] ERROR: VirtualAlloc failed!\r\n");
        ExitProcess(1);
    }

    // Zero-init shared context
    auto* raw = reinterpret_cast<uint8_t*>(ctx);
    for (size_t i = 0; i < sizeof(SmpSharedContext); ++i) raw[i] = 0;

    // Partition [1..1,000,000] evenly into 4 slices of 250,000 across 4 physical CPU cores!
    uint64_t slice = TOTAL_SUM_N / 4ULL;
    for (uint32_t i = 0; i < 4; ++i) {
        ctx->workers[i].ctx = ctx;
        ctx->workers[i].workerIdx = i;
        ctx->workers[i].observedApic = 0xFFFFFFFFu;
        ctx->workers[i].rangeStart = static_cast<uint64_t>(i) * slice + 1ULL;
        ctx->workers[i].rangeEnd   = (i == 3) ? TOTAL_SUM_N : (static_cast<uint64_t>(i + 1) * slice);
    }

    // Launch 3 genuine Ring-3 Win32 worker threads onto physical AP Cores 1, 2, 3 via CreateThread!
    void*    hThreads[4]{};
    uint32_t createdTids[4]{};
    uint32_t spawnedAps = 0;
    for (uint32_t i = 1; i < 4; ++i) {
        hThreads[i] = CreateThread(nullptr, 0, WorkerThreadMain, &ctx->workers[i], 0, &createdTids[i]);
        if (hThreads[i] != nullptr) spawnedAps++;
    }
    __atomic_store_n(&ctx->barrierTarget, 1u + spawnedAps, __ATOMIC_RELEASE);

    // Core 0 (BSP) runs Worker 0 concurrently in parallel with AP Cores 1, 2, 3!
    uint32_t bspCode = WorkerThreadMain(&ctx->workers[0]);

    // Join all 3 AP worker threads via WaitForSingleObject + GetExitCodeThread + CloseHandle!
    uint32_t exitCodes[4]{bspCode, 0, 0, 0};
    bool allWaitsOk = true;
    bool allExitCodesOk = (bspCode == (0xC0DE0000u | (ctx->workers[0].observedApic & 0xFFu)));

    for (uint32_t i = 1; i < 4; ++i) {
        if (!hThreads[i]) { allWaitsOk = false; allExitCodesOk = false; continue; }
        uint32_t wr = WaitForSingleObject(hThreads[i], INFINITE_WAIT);
        if (wr != 0) allWaitsOk = false;
        uint32_t ec = 0;
        if (!GetExitCodeThread(hThreads[i], &ec) || ec != (0xC0DE0000u | (ctx->workers[i].observedApic & 0xFFu))) {
            allExitCodesOk = false;
        }
        exitCodes[i] = ec;
        CloseHandle(hThreads[i]);
    }

    // Aggregate parallel 4-core results & verify hardware core mask, TEB PID/TIDs, spinlock, & sum!
    uint32_t coreMask = 0;
    uint64_t totalSum = 0;
    bool pidMatch = true;
    bool distinctTids = true;
    for (uint32_t i = 0; i < 4; ++i) {
        uint32_t apic = ctx->workers[i].observedApic;
        if (apic < 16) coreMask |= (1u << apic);
        totalSum += ctx->workers[i].partialSum;
        if (ctx->workers[i].observedPid != mainPid) pidMatch = false;
        for (uint32_t j = 0; j < i; ++j) {
            if (ctx->workers[i].observedTid == ctx->workers[j].observedTid) distinctTids = false;
        }
    }

    uint64_t expectedLocks = static_cast<uint64_t>(1u + spawnedAps) * R3_LOCK_ITERS_PER_CORE;
    bool spinOk = (ctx->r3SharedCounter == expectedLocks);
    bool sumOk  = (totalSum == EXPECTED_SUM);
    bool maskOk = (coreMask == 0x0Fu);
    bool allPass = (spawnedAps == 3) && allWaitsOk && allExitCodesOk && pidMatch && distinctTids && spinOk && sumOk && maskOk;

    // Emit concise, <108-char Ring-3 proof lines (never wrap mid-token on GOP console!)
    char line[220]{};
    AppendStr(line, sizeof(line), "[SMP-R3 PID=");
    AppendU64(line, sizeof(line), mainPid);
    AppendStr(line, sizeof(line), "] 4/4 Cores Parallel Ring-3: APICs={");
    for (uint32_t i = 0; i < 4; ++i) {
        if (i > 0) AppendStr(line, sizeof(line), ",");
        AppendU64(line, sizeof(line), ctx->workers[i].observedApic);
    }
    AppendStr(line, sizeof(line), "} TIDs={");
    for (uint32_t i = 0; i < 4; ++i) {
        if (i > 0) AppendStr(line, sizeof(line), ",");
        AppendU64(line, sizeof(line), ctx->workers[i].observedTid);
    }
    AppendStr(line, sizeof(line), "} Mask=");
    AppendHex32(line, sizeof(line), coreMask);
    AppendStr(line, sizeof(line), "\r\n");
    PrintOut(hOut, line);

    line[0] = '\0';
    AppendStr(line, sizeof(line), "[SMP-R3 PID=");
    AppendU64(line, sizeof(line), mainPid);
    AppendStr(line, sizeof(line), "] R3Spinlock=");
    AppendU64(line, sizeof(line), ctx->r3SharedCounter);
    AppendStr(line, sizeof(line), "/");
    AppendU64(line, sizeof(line), expectedLocks);
    AppendStr(line, sizeof(line), " | Sum=");
    AppendU64(line, sizeof(line), totalSum);
    AppendStr(line, sizeof(line), " | Exits=");
    AppendHex32(line, sizeof(line), exitCodes[1]);
    AppendStr(line, sizeof(line), ",");
    AppendHex32(line, sizeof(line), exitCodes[2]);
    AppendStr(line, sizeof(line), ",");
    AppendHex32(line, sizeof(line), exitCodes[3]);
    AppendStr(line, sizeof(line), allPass ? " -> PASS!\r\n" : " -> MISMATCH!\r\n");
    PrintOut(hOut, line);

    VirtualFree(ctx, 0, 0x8000u);
    ExitProcess(allPass ? 0u : 2u);
}
