/**
 * @file ntdll.cpp
 * @brief Sovereign NT Layer DLL (ntdll.dll) for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. Zero proprietary headers.
 * Native x64 system call trampolines and runtime library exports.
 */

#include <cstdint>
#include <cstddef>
#include "micant/shared/context.hpp"

extern "C" {

// System time services use the same AMD64 syscall ABI as the rest of NTDLL.
// SSNs match the canonical Ke service table consumed by the bare-metal path.
__attribute__((naked)) __declspec(dllexport) int32_t NtQuerySystemTime(
    int64_t* /*systemTime*/) noexcept {
    __asm__ __volatile__("movq %rcx, %r10\n\tmovl $0x47, %eax\n\tsyscall\n\tretq\n");
}

__attribute__((naked)) __declspec(dllexport) int32_t NtQueryPerformanceCounter(
    int64_t* /*performanceCounter*/, int64_t* /*performanceFrequency*/) noexcept {
    __asm__ __volatile__("movq %rcx, %r10\n\tmovl $0x31, %eax\n\tsyscall\n\tretq\n");
}

// Local freestanding runtime primitives. They satisfy this DLL's compiler
// calls and are not additional public NTDLL exports.
void* memset(void* dest, int c, size_t n) {
    auto d = static_cast<char*>(dest);
    while (n--) *d++ = static_cast<char>(c);
    return dest;
}

void* memcpy(void* dest, const void* src, size_t n) {
    auto d = static_cast<char*>(dest);
    auto s = static_cast<const char*>(src);
    while (n--) *d++ = *s++;
    return dest;
}

void* memmove(void* dest, const void* src, size_t n) {
    auto d = static_cast<char*>(dest);
    const char* s = static_cast<const char*>(src);
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n;
        auto s_end = s + n;
        while (n--) *--d = *--s_end;
    }
    return dest;
}

bool DllMain(void*, unsigned long, void*) noexcept {
    return true;
}

// LLVM-MinGW emits this helper before large AMD64 stack allocations. Touch
// each page in the prospective frame so the kernel can grow the user stack
// one guard page at a time instead of skipping over the guard.
__attribute__((naked)) void ___chkstk_ms() noexcept {
    __asm__ __volatile__(
        "leaq 8(%rsp), %r11\n"
        "movq %rax, %r10\n"
        "1:\n"
        "cmpq $4096, %r10\n"
        "jb 2f\n"
        "subq $4096, %r11\n"
        "testb $0, (%r11)\n"
        "subq $4096, %r10\n"
        "jmp 1b\n"
        "2:\n"
        "subq %r10, %r11\n"
        "testb $0, (%r11)\n"
        "retq\n");
}

// ============================================================================
// 1. Native NT Fast Syscall Trampolines (Ring 3 -> Ring 0)
// ============================================================================

__attribute__((naked)) __declspec(dllexport) int32_t NtTerminateProcess(void* /*processHandle*/, int32_t /*exitStatus*/) noexcept {
#if defined(__x86_64__)
    __asm__ __volatile__(
        "movq %rcx, %r10\n\t"
        "movq $0x2c, %rax\n\t"
        "syscall\n\t"
        "ret\n\t"
    );
#else
    return 0;
#endif
}

__attribute__((naked)) __declspec(dllexport) int32_t NtTerminateThread(void* /*threadHandle*/, int32_t /*exitStatus*/) noexcept {
#if defined(__x86_64__)
    __asm__ __volatile__("movq %rcx, %r10\n\tmovq $0x53, %rax\n\tsyscall\n\tret\n\t");
#else
    return 0;
#endif
}

extern "C" __attribute__((naked)) __declspec(dllexport) int32_t NtContinue(
    micant::CONTEXT* /*context*/, uint8_t /*testAlert*/) noexcept {
    __asm__ __volatile__("movq %rcx, %r10\n\tmovq $0x43, %rax\n\tsyscall\n\tret\n\t");
}

struct ExceptionRecord64 {
    uint32_t code, flags;
    ExceptionRecord64* nestedRecord;
    void* exceptionAddress;
    uint32_t parameterCount, alignment;
    uintptr_t information[15];
};
struct ExceptionPointers64 { ExceptionRecord64* record; micant::CONTEXT* context; };
using VectoredExceptionHandler = int32_t (*)(ExceptionPointers64*);
static_assert(sizeof(ExceptionRecord64) == 0x98);

extern "C" __declspec(dllexport) [[noreturn]] void RtlRestoreContext(
    micant::CONTEXT* context, ExceptionRecord64* exceptionRecord) noexcept;

namespace {
struct VectoredEntry { uintptr_t cookie{}; VectoredExceptionHandler handler{}; };
VectoredEntry g_vectoredHandlers[32]{};
VectoredEntry g_vectoredContinueHandlers[32]{};
uintptr_t g_nextVectoredCookie{1};
volatile uint8_t g_vectoredLock{};

struct RuntimeFunction64 { uint32_t beginAddress, endAddress, unwindInfo; };
struct UnwindState64 {
    uint64_t registers[16]{};
    uint64_t rip{};
    micant::M128A xmm[16]{};
    [[nodiscard]] uint64_t& rsp() noexcept { return registers[4]; }
};
struct alignas(64) HandlerContextStorage {
    micant::CONTEXT context{};
    micant::CONTEXT_EX contextEx{};
    alignas(64) uint8_t xstateTail[4096]{};
};
static_assert(offsetof(HandlerContextStorage, contextEx) == sizeof(micant::CONTEXT));
struct ImageView64 {
    uintptr_t base{};
    uint32_t size{};
    uint32_t exceptionRva{};
    uint32_t exceptionSize{};
};
struct DispatcherContext64 {
    uint64_t controlPc{};
    uint64_t imageBase{};
    RuntimeFunction64* functionEntry{};
    uint64_t establisherFrame{};
    uint64_t targetIp{};
    micant::CONTEXT* contextRecord{};
    void* languageHandler{};
    void* handlerData{};
    void* historyTable{};
    uint32_t scopeIndex{};
    uint32_t controlPcIsUnwound{};
    void* nonVolatileRegisters{};
};

constexpr uint32_t UnwindFlagExceptionHandler = 0x1;
constexpr uint32_t UnwindFlagTerminationHandler = 0x2;
constexpr uint32_t UnwindFlagChainInfo = 0x4;
constexpr int32_t ExceptionNestedException = 2;
constexpr int32_t ExceptionCollidedUnwind = 3;
constexpr int32_t VectoredContinueExecution = -1;
constexpr int32_t ExceptionContinueExecution = 0; // EXCEPTION_DISPOSITION
constexpr int32_t ExceptionContinueSearch = 1;    // EXCEPTION_DISPOSITION
constexpr uint32_t StatusNoncontinuableException = 0xC0000025u;
constexpr uint32_t StatusInvalidDisposition = 0xC0000026u;
constexpr uint32_t StatusUnwindConsolidate = 0x80000029u;

bool locateXstateChunk(micant::CONTEXT_EX* contextEx,
                       micant::XSTATE_HEADER*& header,
                       uint32_t& length) noexcept;

bool buildHandlerContext(const micant::CONTEXT& source, const UnwindState64& walk,
                         HandlerContextStorage& storage) noexcept {
    storage.context = source;
    auto* sourceRegisters = &storage.context.Rax;
    for (size_t i = 0; i < 16; ++i) sourceRegisters[i] = walk.registers[i];
    storage.context.Rip = walk.rip;
    for (size_t i = 0; i < 16; ++i)
        storage.context.FltSave.XmmRegisters[i] = walk.xmm[i];

    storage.contextEx = {};
    storage.contextEx.All.Offset = -static_cast<int32_t>(sizeof(micant::CONTEXT));
    storage.contextEx.Legacy.Offset = storage.contextEx.All.Offset;
    storage.contextEx.Legacy.Length = sizeof(micant::CONTEXT);
    if (!(storage.context.ContextFlags & CONTEXT_XSTATE)) {
        storage.contextEx.All.Length = sizeof(micant::CONTEXT) + sizeof(micant::CONTEXT_EX);
        storage.contextEx.XState.Offset = sizeof(micant::CONTEXT_EX) + 1;
        return true;
    }

    auto* sourceEx = reinterpret_cast<micant::CONTEXT_EX*>(
        reinterpret_cast<uintptr_t>(const_cast<micant::CONTEXT*>(&source)) + sizeof(micant::CONTEXT));
    micant::XSTATE_HEADER* sourceHeader{};
    uint32_t xstateLength{};
    if (!locateXstateChunk(sourceEx, sourceHeader, xstateLength) ||
        xstateLength > sizeof(storage.xstateTail)) return false;
    std::memcpy(storage.xstateTail, sourceHeader, xstateLength);
    const uintptr_t offset = reinterpret_cast<uintptr_t>(storage.xstateTail) -
                             reinterpret_cast<uintptr_t>(&storage.contextEx);
    if (offset > INT32_MAX || offset > UINT32_MAX - sizeof(micant::CONTEXT) - xstateLength)
        return false;
    storage.contextEx.XState.Offset = static_cast<int32_t>(offset);
    storage.contextEx.XState.Length = xstateLength;
    storage.contextEx.All.Length = static_cast<uint32_t>(sizeof(micant::CONTEXT) + offset + xstateLength);
    return true;
}

bool commitHandlerContext(const HandlerContextStorage& storage,
                          micant::CONTEXT& destination) noexcept {
    destination = storage.context;
    if (!(destination.ContextFlags & CONTEXT_XSTATE)) return true;
    auto* destinationEx = reinterpret_cast<micant::CONTEXT_EX*>(
        reinterpret_cast<uintptr_t>(&destination) + sizeof(micant::CONTEXT));
    auto* sourceEx = const_cast<micant::CONTEXT_EX*>(&storage.contextEx);
    micant::XSTATE_HEADER* destinationHeader{};
    micant::XSTATE_HEADER* sourceHeader{};
    uint32_t destinationLength{}, sourceLength{};
    if (!locateXstateChunk(destinationEx, destinationHeader, destinationLength) ||
        !locateXstateChunk(sourceEx, sourceHeader, sourceLength) ||
        destinationLength != sourceLength) return false;
    std::memcpy(destinationHeader, sourceHeader, sourceLength);
    return true;
}

bool imageRange(const ImageView64& image, uint32_t rva, size_t length) noexcept {
    return image.base && rva <= image.size && length <= image.size - rva;
}

bool nonvolatileRegister(uint8_t reg) noexcept {
    return reg == 3 || (reg >= 5 && reg <= 7) || reg >= 12;
}

bool inspectImage(uintptr_t base, uint32_t imageSize, ImageView64& image) noexcept {
    if (!base || imageSize < 0x100 || base > UINTPTR_MAX - imageSize) return false;
    const auto* dos = reinterpret_cast<const uint8_t*>(base);
    if (*reinterpret_cast<const uint16_t*>(dos) != 0x5A4D) return false;
    const int32_t ntOffset = *reinterpret_cast<const int32_t*>(dos + 0x3C);
    if (ntOffset < 0x40 || ntOffset > 0x100000 ||
        static_cast<uint32_t>(ntOffset) > imageSize - 24) return false;
    const auto* nt = dos + ntOffset;
    if (*reinterpret_cast<const uint32_t*>(nt) != 0x00004550u ||
        *reinterpret_cast<const uint16_t*>(nt + 4) != 0x8664) return false;
    const uint16_t optionalSize = *reinterpret_cast<const uint16_t*>(nt + 20);
    const auto* optional = nt + 24;
    if (optionalSize < 144 || optionalSize > imageSize - 24 ||
        static_cast<uint32_t>(ntOffset) > imageSize - 24 - optionalSize ||
        *reinterpret_cast<const uint16_t*>(optional) != 0x020B) return false;
    const uint32_t declaredSize = *reinterpret_cast<const uint32_t*>(optional + 56);
    const uint32_t directoryCount = *reinterpret_cast<const uint32_t*>(optional + 108);
    if (!declaredSize || declaredSize > imageSize || directoryCount <= 3) return false;
    const uint32_t exceptionRva = *reinterpret_cast<const uint32_t*>(optional + 112 + 3 * 8);
    const uint32_t exceptionSize = *reinterpret_cast<const uint32_t*>(optional + 112 + 3 * 8 + 4);
    image = {base, declaredSize, exceptionRva, exceptionSize};
    if (!exceptionRva) return exceptionSize == 0;
    if (exceptionSize < sizeof(RuntimeFunction64) ||
        exceptionSize % sizeof(RuntimeFunction64) != 0 ||
        !imageRange(image, exceptionRva, exceptionSize)) return false;
    return true;
}

bool findImage(uintptr_t controlPc, ImageView64& image) noexcept {
    uintptr_t teb{};
    __asm__ __volatile__("movq %%gs:0x30, %0" : "=r"(teb));
    if (!teb) return false;
    const uintptr_t peb = *reinterpret_cast<const uintptr_t*>(teb + 0x60);
    if (!peb) return false;

    // PEB_LDR_DATA.InMemoryOrderModuleList and LDR_DATA_TABLE_ENTRY's
    // InMemoryOrderLinks are at their public AMD64 ABI offsets.
    const uintptr_t loader = *reinterpret_cast<const uintptr_t*>(peb + 0x18);
    if (loader) {
        const uintptr_t head = loader + 0x20;
        uintptr_t link = *reinterpret_cast<const uintptr_t*>(head);
        for (size_t count = 0; link && link != head && count < 256; ++count) {
            if (link < 0x10000 || link >= 0x0000800000000000ULL) return false;
            const uintptr_t next = *reinterpret_cast<const uintptr_t*>(link);
            const uintptr_t entry = link - 0x10;
            const uintptr_t base = *reinterpret_cast<const uintptr_t*>(entry + 0x30);
            const uint32_t imageSize = *reinterpret_cast<const uint32_t*>(entry + 0x40);
            ImageView64 candidate{};
            if (base && imageSize >= 0x100 && controlPc >= base &&
                inspectImage(base, imageSize, candidate) &&
                controlPc - base < candidate.size) {
                image = candidate;
                return true;
            }
            link = next;
        }
    }

    // Without a loader entry there is no trusted mapped-image bound. Never
    // probe arbitrary user memory based on PE-declared sizes alone.
    return false;
}

RuntimeFunction64* lookupFunction(const ImageView64& image, uintptr_t controlPc) noexcept {
    if (!image.exceptionRva || controlPc < image.base ||
        controlPc - image.base > UINT32_MAX) return nullptr;
    const uint32_t rva = static_cast<uint32_t>(controlPc - image.base);
    const size_t count = image.exceptionSize / sizeof(RuntimeFunction64);
    auto* table = reinterpret_cast<RuntimeFunction64*>(image.base + image.exceptionRva);
    size_t low = 0, high = count;
    while (low < high) {
        const size_t mid = low + (high - low) / 2;
        const auto& entry = table[mid];
        if (rva < entry.beginAddress) high = mid;
        else if (rva >= entry.endAddress) low = mid + 1;
        else return entry.beginAddress < entry.endAddress &&
                    entry.endAddress <= image.size ? &table[mid] : nullptr;
    }
    return nullptr;
}

uint64_t& unwindRegister(UnwindState64& context, uint8_t reg) noexcept {
    return context.registers[reg];
}

bool tryUnwindEpilog(const ImageView64& image, const RuntimeFunction64& function,
                     uintptr_t controlPc, UnwindState64& context,
                     uint64_t stackLimit, uint64_t stackBase,
                     bool& recognized) noexcept {
    recognized = false;
    if (controlPc < image.base + function.beginAddress ||
        controlPc >= image.base + function.endAddress) return false;
    const uint32_t endRva = function.endAddress;
    uint32_t cursor = static_cast<uint32_t>(controlPc - image.base);
    UnwindState64 candidate = context;
    auto byteAt = [&](uint32_t rva, uint8_t& value) noexcept {
        if (rva >= endRva || !imageRange(image, rva, 1)) return false;
        value = *reinterpret_cast<const uint8_t*>(image.base + rva);
        return true;
    };
    auto readImmediate = [&](uint32_t rva, unsigned bytes, uint32_t& value) noexcept {
        if (rva > endRva || bytes > endRva - rva || !imageRange(image, rva, bytes)) return false;
        value = 0;
        std::memcpy(&value, reinterpret_cast<const void*>(image.base + rva), bytes);
        return true;
    };

    // The optional first instruction adjusts RSP with ADD or LEA.
    uint8_t b0{}, b1{}, b2{};
    if (byteAt(cursor, b0) && b0 == 0x48 && byteAt(cursor + 1, b1) &&
        (b1 == 0x83 || b1 == 0x81) && byteAt(cursor + 2, b2) && b2 == 0xC4) {
        recognized = true;
        uint32_t amount{};
        const unsigned immediateBytes = b1 == 0x83 ? 1 : 4;
        if (!readImmediate(cursor + 3, immediateBytes, amount)) return false;
        if (immediateBytes == 1) amount = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(amount)));
        const int64_t delta = immediateBytes == 1 ? static_cast<int8_t>(amount) : static_cast<int32_t>(amount);
        const uint64_t rsp = candidate.rsp();
        if ((delta < 0 && static_cast<uint64_t>(-delta) > rsp) ||
            (delta > 0 && static_cast<uint64_t>(delta) > UINT64_MAX - rsp)) return false;
        candidate.rsp() = delta < 0 ? rsp - static_cast<uint64_t>(-delta) : rsp + static_cast<uint64_t>(delta);
        cursor += 3 + immediateBytes;
    } else if (byteAt(cursor, b0) && (b0 == 0x48 || b0 == 0x49) &&
               byteAt(cursor + 1, b1) && b1 == 0x8D && byteAt(cursor + 2, b2) &&
               ((b2 >> 3) & 7) == 4 && ((b2 >> 6) & 3) != 3) {
        recognized = true;
        const uint8_t mod = b2 >> 6;
        const uint8_t rm = b2 & 7;
        if (rm == 4 || (mod == 0 && rm == 5)) return false; // SIB/RIP-relative forms are unsupported.
        const uint8_t baseRegister = static_cast<uint8_t>(rm | ((b0 & 1) ? 8 : 0));
        int64_t displacement{};
        uint32_t raw{};
        unsigned immediateBytes{};
        if (mod == 1) immediateBytes = 1;
        else if (mod == 2) immediateBytes = 4;
        if (immediateBytes) {
            if (!readImmediate(cursor + 3, immediateBytes, raw)) return false;
            displacement = immediateBytes == 1 ? static_cast<int8_t>(raw) : static_cast<int32_t>(raw);
        }
        const uint64_t base = candidate.registers[baseRegister];
        if ((displacement < 0 && static_cast<uint64_t>(-displacement) > base) ||
            (displacement > 0 && static_cast<uint64_t>(displacement) > UINT64_MAX - base)) return false;
        candidate.rsp() = displacement < 0 ? base - static_cast<uint64_t>(-displacement)
                                             : base + static_cast<uint64_t>(displacement);
        cursor += 3 + immediateBytes;
    }

    // Remaining legal epilogue operations are pops of nonvolatile GPRs,
    // followed by RET/RET imm16. Execute only instructions inside this image.
    for (size_t instruction = 0; instruction < 32; ++instruction) {
        uint8_t opcode{};
        if (!byteAt(cursor, opcode)) return false;
        uint8_t reg{};
        uint32_t length{};
        if (opcode >= 0x58 && opcode <= 0x5F) {
            recognized = true;
            reg = opcode & 7;
            length = 1;
        } else if (opcode == 0x41) {
            uint8_t extended{};
            if (!byteAt(cursor + 1, extended) || extended < 0x58 || extended > 0x5F) return false;
            recognized = true;
            reg = static_cast<uint8_t>((extended & 7) + 8);
            length = 2;
        } else if (opcode == 0xFF || (opcode >= 0x40 && opcode <= 0x4F)) {
            recognized = true;
            uint8_t rex{};
            uint32_t opcodeRva = cursor;
            if (opcode != 0xFF) {
                rex = opcode;
                if (!byteAt(++opcodeRva, opcode) || opcode != 0xFF) return false;
            }
            uint8_t modrm{};
            if (!byteAt(opcodeRva + 1, modrm) || (modrm >> 6) != 0 ||
                ((modrm >> 3) & 7) != 4) return false;
            uint32_t addressRva = opcodeRva + 2;
            uint64_t effectiveAddress{};
            const uint8_t rm = modrm & 7;
            if (rm == 4) {
                uint8_t sib{};
                if (!byteAt(addressRva++, sib)) return false;
                const uint8_t baseField = sib & 7;
                const uint8_t indexField = (sib >> 3) & 7;
                const bool noBase = baseField == 5;
                const bool noIndex = indexField == 4 && !(rex & 2);
                uint64_t base = 0;
                if (!noBase) base = candidate.registers[baseField | ((rex & 1) ? 8 : 0)];
                uint64_t index = 0;
                if (!noIndex) index = candidate.registers[indexField | ((rex & 2) ? 8 : 0)];
                const uint64_t scaledIndex = index << (sib >> 6);
                if (scaledIndex > UINT64_MAX - base) return false;
                effectiveAddress = base + scaledIndex;
                if (noBase) {
                    uint32_t displacement{};
                    if (!readImmediate(addressRva, 4, displacement)) return false;
                    const int64_t signedDisplacement = static_cast<int32_t>(displacement);
                    if ((signedDisplacement < 0 && static_cast<uint64_t>(-signedDisplacement) > effectiveAddress) ||
                        (signedDisplacement > 0 && static_cast<uint64_t>(signedDisplacement) > UINT64_MAX - effectiveAddress)) return false;
                    effectiveAddress = signedDisplacement < 0
                        ? effectiveAddress - static_cast<uint64_t>(-signedDisplacement)
                        : effectiveAddress + static_cast<uint64_t>(signedDisplacement);
                    addressRva += 4;
                }
            } else if (rm == 5) {
                uint32_t displacement{};
                if (!readImmediate(addressRva, 4, displacement)) return false;
                const int64_t signedDisplacement = static_cast<int32_t>(displacement);
                const uint64_t nextRip = image.base + addressRva + 4;
                if ((signedDisplacement < 0 && static_cast<uint64_t>(-signedDisplacement) > nextRip) ||
                    (signedDisplacement > 0 && static_cast<uint64_t>(signedDisplacement) > UINT64_MAX - nextRip)) return false;
                effectiveAddress = signedDisplacement < 0
                    ? nextRip - static_cast<uint64_t>(-signedDisplacement)
                    : nextRip + static_cast<uint64_t>(signedDisplacement);
                addressRva += 4;
            } else {
                effectiveAddress = candidate.registers[rm | ((rex & 1) ? 8 : 0)];
            }
            // Do not dereference an indirect tail-jump slot outside a PE image
            // whose mapping bounds are known to this dispatcher.
            if (effectiveAddress < image.base || effectiveAddress - image.base > UINT32_MAX ||
                !imageRange(image, static_cast<uint32_t>(effectiveAddress - image.base), sizeof(uint64_t))) return false;
            std::memcpy(&candidate.rip, reinterpret_cast<const void*>(effectiveAddress), sizeof(candidate.rip));
            if (!candidate.rip || (candidate.rsp() < stackLimit || candidate.rsp() >= stackBase)) return false;
            context = candidate;
            return true;
        } else if (opcode == 0xC3 || opcode == 0xC2) {
            recognized = true;
            if (candidate.rsp() < stackLimit || candidate.rsp() > stackBase - sizeof(uint64_t)) return false;
            candidate.rip = *reinterpret_cast<const uint64_t*>(candidate.rsp());
            candidate.rsp() += sizeof(uint64_t);
            if (opcode == 0xC2) {
                uint32_t amount{};
                if (!readImmediate(cursor + 1, 2, amount) || amount > stackBase - candidate.rsp()) return false;
                candidate.rsp() += amount;
            }
            if (!candidate.rip || (candidate.rsp() & 7)) return false;
            context = candidate;
            return true;
        } else {
            return false;
        }
        if (!nonvolatileRegister(reg) || candidate.rsp() < stackLimit ||
            candidate.rsp() > stackBase - sizeof(uint64_t)) return false;
        candidate.registers[reg] = *reinterpret_cast<const uint64_t*>(candidate.rsp());
        candidate.rsp() += sizeof(uint64_t);
        cursor += length;
    }
    return false;
}

bool unwindFrame(const ImageView64& image, uintptr_t controlPc,
                 UnwindState64& context, RuntimeFunction64* function,
                 uint32_t handlerFlag, uint64_t stackLimit, uint64_t stackBase,
                 uint64_t& establisher,
                 void*& handler, void*& handlerData) noexcept {
    handler = nullptr;
    handlerData = nullptr;
    if (!function) {
        if (context.rsp() < stackLimit || context.rsp() > stackBase - sizeof(uint64_t)) return false;
        context.rip = *reinterpret_cast<const uint64_t*>(context.rsp());
        context.rsp() += sizeof(uint64_t);
        establisher = context.rsp();
        return context.rip != 0;
    }
    if (!imageRange(image, function->unwindInfo, 4) ||
        function->beginAddress >= function->endAddress || function->endAddress > image.size ||
        controlPc < image.base + function->beginAddress || controlPc >= image.base + function->endAddress)
        return false;

    const auto* info = reinterpret_cast<const uint8_t*>(image.base + function->unwindInfo);
    const uint32_t initialCodeOffset = static_cast<uint32_t>(controlPc - (image.base + function->beginAddress));
    const uint64_t epilogEstablisher = context.rsp();
    if (initialCodeOffset > info[1]) {
        bool recognizedEpilog{};
        if (tryUnwindEpilog(image, *function, controlPc, context, stackLimit, stackBase,
                            recognizedEpilog)) {
            establisher = epilogEstablisher;
            return true;
        }
        // Once an epilog opcode has been recognized, reject unsupported or
        // malformed forms rather than applying body unwind codes to a partly
        // dismantled frame.
        if (recognizedEpilog) return false;
    }
    const uint8_t initialFrameRegister = info[3] & 0xF;
    const uint8_t initialFrameOffset = info[3] >> 4;
    if (initialFrameRegister && !nonvolatileRegister(initialFrameRegister)) return false;
    establisher = initialFrameRegister
        ? unwindRegister(context, initialFrameRegister) - initialFrameOffset * 16
        : context.rsp();
    bool machineFrame = false;
    uint32_t codeOffset = static_cast<uint32_t>(controlPc - (image.base + function->beginAddress));
    uint8_t finalFlags{};
    RuntimeFunction64 chainedFunction{};

    // Chained records describe one logical frame split across function
    // fragments. Apply the active fragment's prologue offset first, then all
    // parent records in full, matching RtlVirtualUnwind's chain traversal.
    for (size_t chainDepth = 0; ; ++chainDepth) {
        if (chainDepth >= 16 || !imageRange(image, function->unwindInfo, 4)) return false;
        info = reinterpret_cast<const uint8_t*>(image.base + function->unwindInfo);
        const uint8_t version = info[0] & 7;
        const uint8_t flags = info[0] >> 3;
        const uint8_t codeCount = info[2];
        const uint8_t frameRegister = info[3] & 0xF;
        const uint8_t frameOffset = info[3] >> 4;
        const size_t codeBytes = static_cast<size_t>(codeCount) * 2;
        if ((version != 1 && version != 2) ||
            !imageRange(image, function->unwindInfo, 4 + codeBytes) ||
            (flags & ~(UnwindFlagExceptionHandler | UnwindFlagTerminationHandler | UnwindFlagChainInfo)) ||
            ((flags & UnwindFlagChainInfo) && (flags & (UnwindFlagExceptionHandler | UnwindFlagTerminationHandler))) ||
            (frameRegister && !nonvolatileRegister(frameRegister))) return false;
        const auto* codes = info + 4;
        size_t i = 0;
        if (chainDepth == 0) {
            while (i < codeCount && codes[i * 2] > codeOffset) {
                const uint8_t op = codes[i * 2 + 1] & 0xF;
                const uint8_t opInfo = codes[i * 2 + 1] >> 4;
                const size_t slots = op == 1 && opInfo ? 3 :
                    (op == 1 || op == 4 || op == 6 || op == 8 || op == 11) ? 2 :
                    (op == 5 || op == 9 || op == 10) ? 3 : 1;
                if (slots > codeCount - i) return false;
                i += slots;
            }
        }
        while (i < codeCount) {
        if (context.rsp() < stackLimit || context.rsp() >= stackBase) return false;
        const uint8_t op = codes[i * 2 + 1] & 0xF;
        const uint8_t opInfo = codes[i * 2 + 1] >> 4;
        uint32_t offset{};
        switch (op) {
            case 0: // UWOP_PUSH_NONVOL
                if (!nonvolatileRegister(opInfo) || context.rsp() < stackLimit ||
                    context.rsp() > stackBase - sizeof(uint64_t)) return false;
                unwindRegister(context, opInfo) = *reinterpret_cast<const uint64_t*>(context.rsp());
                context.rsp() += 8;
                ++i;
                break;
            case 1: // UWOP_ALLOC_LARGE
                if (!opInfo) {
                    if (i + 1 >= codeCount) return false;
                    offset = *reinterpret_cast<const uint16_t*>(codes + (i + 1) * 2) * 8u;
                    i += 2;
                } else if (opInfo == 1) {
                    if (i + 2 >= codeCount) return false;
                    offset = *reinterpret_cast<const uint32_t*>(codes + (i + 1) * 2);
                    i += 3;
                } else return false;
                if (context.rsp() > stackBase || offset > stackBase - context.rsp() ||
                    sizeof(uint64_t) > stackBase - context.rsp() - offset) return false;
                context.rsp() += offset;
                break;
            case 2: // UWOP_ALLOC_SMALL
                offset = (opInfo + 1u) * 8u;
                if (context.rsp() > stackBase || offset > stackBase - context.rsp()) return false;
                context.rsp() += offset;
                ++i;
                break;
            case 3: // UWOP_SET_FPREG
                if (!frameRegister) return false;
                context.rsp() = unwindRegister(context, frameRegister) - frameOffset * 16;
                ++i;
                break;
            case 4: // UWOP_SAVE_NONVOL
            case 5: // UWOP_SAVE_NONVOL_FAR
                if (!nonvolatileRegister(opInfo) || (op == 4 && i + 1 >= codeCount) ||
                    (op == 5 && i + 2 >= codeCount)) return false;
                offset = op == 4 ?
                    static_cast<uint32_t>(*reinterpret_cast<const uint16_t*>(codes + (i + 1) * 2)) * 8u :
                    *reinterpret_cast<const uint32_t*>(codes + (i + 1) * 2);
                if (context.rsp() > stackBase || offset > stackBase - context.rsp() ||
                    sizeof(uint64_t) > stackBase - context.rsp() - offset) return false;
                unwindRegister(context, opInfo) = *reinterpret_cast<const uint64_t*>(context.rsp() + offset);
                i += op == 4 ? 2 : 3;
                break;
            case 6: // UWOP_EPILOG (version 2)
                if (version != 2 || i + 1 >= codeCount) return false;
                i += 2;
                break;
            case 8: // UWOP_SAVE_XMM128
            case 9: // UWOP_SAVE_XMM128_FAR
                if (opInfo < 6 || (op == 8 && i + 1 >= codeCount) ||
                    (op == 9 && i + 2 >= codeCount)) return false;
                offset = op == 8 ?
                    static_cast<uint32_t>(*reinterpret_cast<const uint16_t*>(codes + (i + 1) * 2)) * 16u :
                    *reinterpret_cast<const uint32_t*>(codes + (i + 1) * 2);
                if (context.rsp() > stackBase || offset > stackBase - context.rsp() ||
                    sizeof(micant::M128A) > stackBase - context.rsp() - offset) return false;
                std::memcpy(&context.xmm[opInfo],
                    reinterpret_cast<const void*>(context.rsp() + offset), sizeof(micant::M128A));
                i += op == 8 ? 2 : 3;
                break;
            case 10: // UWOP_PUSH_MACHFRAME
                if (opInfo > 1) return false;
                if (context.rsp() < stackLimit || context.rsp() > stackBase - (opInfo ? 0x28 : 0x20))
                    return false;
                context.rsp() += opInfo ? 8 : 0;
                context.rip = *reinterpret_cast<const uint64_t*>(context.rsp());
                context.rsp() = *reinterpret_cast<const uint64_t*>(context.rsp() + 0x18);
                machineFrame = true;
                i = codeCount;
                break;
                default: return false;
            }
        }

        finalFlags = flags;
        if (!(flags & UnwindFlagChainInfo)) break;
        const size_t chainedOffset = 4 + ((static_cast<size_t>(codeCount) + 1) & ~size_t{1}) * 2;
        if (!imageRange(image, function->unwindInfo, chainedOffset + sizeof(RuntimeFunction64))) return false;
        RuntimeFunction64 chained{};
        std::memcpy(&chainedFunction, info + chainedOffset, sizeof(chainedFunction));
        if (chainedFunction.beginAddress >= chainedFunction.endAddress ||
            chainedFunction.endAddress > image.size ||
            !imageRange(image, chainedFunction.unwindInfo, 4)) return false;
        function = &chainedFunction;
        codeOffset = 0;
    }

    if (!machineFrame) {
        if (context.rsp() < stackLimit || context.rsp() > stackBase - sizeof(uint64_t)) return false;
        context.rip = *reinterpret_cast<const uint64_t*>(context.rsp());
        context.rsp() += 8;
    }
    if (!context.rip || (context.rsp() & 7)) return false;

    if (finalFlags & handlerFlag) {
        const uint8_t codeCount = info[2];
        const size_t handlerOffset = (4 + static_cast<size_t>(codeCount) * 2 + 3) & ~size_t{3};
        if (!imageRange(image, function->unwindInfo, handlerOffset + 4)) return false;
        const uint32_t handlerRva = *reinterpret_cast<const uint32_t*>(info + handlerOffset);
        if (!imageRange(image, handlerRva, 1)) return false;
        handler = reinterpret_cast<void*>(image.base + handlerRva);
        handlerData = const_cast<uint8_t*>(info + handlerOffset + 4);
    }
    return true;
}

bool dispatchFrameHandlers(ExceptionRecord64* record, micant::CONTEXT* context) noexcept {
    uintptr_t teb{};
    __asm__ __volatile__("movq %%gs:0x30, %0" : "=r"(teb));
    if (!teb) return false;
    const uint64_t stackBase = *reinterpret_cast<const uint64_t*>(teb + 8);
    const uint64_t stackLimit = *reinterpret_cast<const uint64_t*>(teb + 16);
    if (stackBase < sizeof(uint64_t) || stackLimit >= stackBase ||
        stackBase - stackLimit < sizeof(uint64_t) || context->Rsp < stackLimit ||
        context->Rsp >= stackBase || (context->Rsp & 7)) return false;

    constexpr uint32_t ExceptionNoncontinuable = 0x01;
    constexpr uint32_t ExceptionNestedCall = 0x10;
    const uint32_t originalNestedCall = record->flags & ExceptionNestedCall;
    bool nestedCallActive = originalNestedCall != 0;
    struct NestedCallFlagRestore {
        ExceptionRecord64* record;
        uint32_t original;
        ~NestedCallFlagRestore() noexcept {
            record->flags = (record->flags & ~uint32_t{0x10}) | original;
        }
    } nestedCallFlagRestore{record, originalNestedCall};

    UnwindState64 walk{};
    auto* sourceRegisters = &context->Rax;
    for (size_t i = 0; i < 16; ++i) walk.registers[i] = sourceRegisters[i];
    walk.rip = context->Rip;
    for (size_t i = 0; i < 16; ++i) walk.xmm[i] = context->FltSave.XmmRegisters[i];
    uint64_t lastRsp{};
    for (size_t frame = 0; frame < 128 && walk.rip && walk.rsp() >= stackLimit && walk.rsp() < stackBase; ++frame) {
        if (frame && walk.rsp() <= lastRsp) return false;
        lastRsp = walk.rsp();
        const uintptr_t controlPc = static_cast<uintptr_t>(walk.rip - (frame && walk.rip ? 1 : 0));
        ImageView64 image{};
        if (!findImage(controlPc, image)) return false;
        RuntimeFunction64* function = lookupFunction(image, controlPc);
        const uint64_t currentRip = walk.rip;
        uint64_t establisher{};
        void* languageHandler{};
        void* handlerData{};
        if (!unwindFrame(image, controlPc, walk, function, UnwindFlagExceptionHandler,
                         stackLimit, stackBase, establisher,
                         languageHandler, handlerData)) return false;
        if (establisher < stackLimit || establisher >= stackBase || (establisher & 7)) {
            // Never invoke a language handler with a frame derived from
            // malformed unwind metadata or outside the active user stack.
            record->flags |= 0x8; // EXCEPTION_STACK_INVALID
            return false;
        }
        if (languageHandler) {
            HandlerContextStorage handlerStorage{};
            if (!buildHandlerContext(*context, walk, handlerStorage)) return false;
            DispatcherContext64 dispatcher{};
            dispatcher.controlPc = controlPc;
            dispatcher.imageBase = image.base;
            dispatcher.functionEntry = function;
            dispatcher.establisherFrame = establisher;
            dispatcher.contextRecord = &handlerStorage.context;
            dispatcher.languageHandler = languageHandler;
            dispatcher.handlerData = handlerData;
            using LanguageHandler = int32_t (*)(ExceptionRecord64*, void*, micant::CONTEXT*, DispatcherContext64*);
            if (nestedCallActive) record->flags |= ExceptionNestedCall;
            else record->flags &= ~ExceptionNestedCall;
            const auto disposition = reinterpret_cast<LanguageHandler>(languageHandler)(
                record, reinterpret_cast<void*>(establisher), context, &dispatcher);
            if (disposition == ExceptionNestedException) {
                nestedCallActive = true;
                record->flags |= ExceptionNestedCall;
            } else {
                nestedCallActive = false;
                record->flags &= ~ExceptionNestedCall;
            }
            if (disposition == ExceptionContinueExecution) {
                // The language handler receives the original CPU CONTEXT and
                // may edit it for continuation. The separately unwound copy
                // in DispatcherContext is only for RtlUnwindEx; committing it
                // here would resume in the caller frame instead of the fault.
                if (!(record->flags & ExceptionNoncontinuable)) return true;
                record->code = StatusNoncontinuableException;
                record->flags |= ExceptionNoncontinuable;
                return false;
            }
            if (disposition == ExceptionNestedException) {
                // Keep EXCEPTION_NESTED_CALL visible to the next frame's
                // language handler; it is cleared after that invocation
                // unless that handler reports another nested exception.
            } else if (disposition != ExceptionContinueSearch) {
                record->code = StatusInvalidDisposition;
                record->flags |= ExceptionNoncontinuable;
                return false;
            }
        }
        if (walk.rsp() <= stackLimit || walk.rsp() >= stackBase) return false;
    }
    return false;
}

bool locateXstateChunk(micant::CONTEXT_EX* contextEx,
                       micant::XSTATE_HEADER*& header,
                       uint32_t& length) noexcept {
    if (!contextEx) return false;
    auto* context = reinterpret_cast<micant::CONTEXT*>(contextEx) - 1;
    if (!(context->ContextFlags & CONTEXT_XSTATE) ||
        contextEx->All.Offset != -static_cast<int32_t>(sizeof(micant::CONTEXT)) ||
        contextEx->Legacy.Offset != contextEx->All.Offset ||
        contextEx->Legacy.Length != sizeof(micant::CONTEXT) ||
        contextEx->XState.Offset < static_cast<int32_t>(sizeof(micant::CONTEXT_EX)) ||
        contextEx->XState.Length < sizeof(micant::XSTATE_HEADER)) return false;
    const uint64_t chunkEnd = static_cast<uint64_t>(contextEx->XState.Offset) +
                              contextEx->XState.Length;
    if (contextEx->All.Length < sizeof(micant::CONTEXT) ||
        chunkEnd > contextEx->All.Length - sizeof(micant::CONTEXT)) return false;
    auto* address = reinterpret_cast<uint8_t*>(contextEx) + contextEx->XState.Offset;
    if (reinterpret_cast<uintptr_t>(address) & 63) return false;
    header = reinterpret_cast<micant::XSTATE_HEADER*>(address);
    length = contextEx->XState.Length;
    return true;
}

bool copyContextRecord(const micant::CONTEXT* source,
                       micant::CONTEXT* destination) noexcept {
    if (!source || !destination) return false;
    const bool sourceHasXstate = (source->ContextFlags & CONTEXT_XSTATE) != 0;
    if (!sourceHasXstate) {
        std::memmove(destination, source, sizeof(*destination));
        return true;
    }

    auto* sourceEx = reinterpret_cast<micant::CONTEXT_EX*>(
        reinterpret_cast<uintptr_t>(const_cast<micant::CONTEXT*>(source)) + sizeof(*source));
    auto* destinationEx = reinterpret_cast<micant::CONTEXT_EX*>(
        reinterpret_cast<uintptr_t>(destination) + sizeof(*destination));
    micant::XSTATE_HEADER* sourceHeader{};
    micant::XSTATE_HEADER* destinationHeader{};
    uint32_t sourceLength{}, destinationLength{};
    if (!locateXstateChunk(sourceEx, sourceHeader, sourceLength) ||
        !locateXstateChunk(destinationEx, destinationHeader, destinationLength) ||
        sourceLength != destinationLength || sourceLength > 4096) return false;

    alignas(64) uint8_t xstateCopy[4096]{};
    std::memcpy(xstateCopy, sourceHeader, sourceLength);

    std::memmove(destination, source, sizeof(*destination));
    // Re-resolve the destination after copying ContextFlags and its extension
    // descriptor, then copy only the bounded XSTATE chunk.
    if (!locateXstateChunk(destinationEx, destinationHeader, destinationLength) ||
        destinationLength != sourceLength) return false;
    std::memcpy(destinationHeader, xstateCopy, sourceLength);
    return true;
}

uint64_t readXcr0() noexcept {
    uint32_t low{}, high{};
    __asm__ __volatile__("xorl %%ecx, %%ecx; xgetbv" : "=a"(low), "=d"(high) : : "ecx");
    return static_cast<uint64_t>(low) | (static_cast<uint64_t>(high) << 32);
}

void queryXstateFeature(uint32_t feature, uint32_t& size, uint32_t& offset) noexcept {
    uint32_t eax = 0x0D, ebx{}, ecx = feature, edx{};
    __asm__ __volatile__("cpuid" : "+a"(eax), "=b"(ebx), "+c"(ecx), "=d"(edx));
    size = eax;
    offset = ebx;
}

void lockVectoredHandlers() noexcept {
    while (__atomic_test_and_set(&g_vectoredLock, __ATOMIC_ACQUIRE)) __asm__ __volatile__("pause");
}
void unlockVectoredHandlers() noexcept { __atomic_clear(&g_vectoredLock, __ATOMIC_RELEASE); }

void* addVectoredHandler(VectoredEntry (&handlers)[32], unsigned long first,
                         VectoredExceptionHandler handler) noexcept {
    if (!handler) return nullptr;
    lockVectoredHandlers();
    size_t slot = 32;
    for (size_t i = 0; i < 32; ++i) if (!handlers[i].handler) { slot = i; break; }
    if (slot == 32) { unlockVectoredHandlers(); return nullptr; }
    uintptr_t cookie = g_nextVectoredCookie++;
    if (!cookie) cookie = g_nextVectoredCookie++;
    if (first) {
        for (size_t i = slot; i > 0; --i) handlers[i] = handlers[i - 1];
        slot = 0;
    }
    handlers[slot] = {cookie, handler};
    unlockVectoredHandlers();
    return reinterpret_cast<void*>(cookie);
}

unsigned long removeVectoredHandler(VectoredEntry (&handlers)[32], void* cookie) noexcept {
    if (!cookie) return 0;
    lockVectoredHandlers();
    for (size_t i = 0; i < 32; ++i) {
        if (handlers[i].cookie != reinterpret_cast<uintptr_t>(cookie)) continue;
        for (size_t j = i; j + 1 < 32; ++j) handlers[j] = handlers[j + 1];
        handlers[31] = {};
        unlockVectoredHandlers();
        return 1;
    }
    unlockVectoredHandlers();
    return 0;
}

void callVectoredContinueHandlers(ExceptionPointers64& pointers) noexcept {
    uintptr_t cookies[32]{};
    lockVectoredHandlers();
    for (size_t i = 0; i < 32; ++i) cookies[i] = g_vectoredContinueHandlers[i].cookie;
    unlockVectoredHandlers();
    for (const auto cookie : cookies) {
        if (!cookie) continue;
        VectoredExceptionHandler handler{};
        lockVectoredHandlers();
        for (const auto& entry : g_vectoredContinueHandlers)
            if (entry.cookie == cookie) { handler = entry.handler; break; }
        unlockVectoredHandlers();
        if (handler) (void)handler(&pointers);
    }
}
}

extern "C" __declspec(dllexport) int32_t RtlDispatchException(
    ExceptionRecord64* record, micant::CONTEXT* context) noexcept {
    if (!record || !context) return 0;
    ExceptionPointers64 pointers{record, context};
    uintptr_t cookies[32]{};
    lockVectoredHandlers();
    for (size_t i = 0; i < 32; ++i) cookies[i] = g_vectoredHandlers[i].cookie;
    unlockVectoredHandlers();
    for (const auto cookie : cookies) {
        if (!cookie) continue;
        VectoredExceptionHandler handler{};
        lockVectoredHandlers();
        for (const auto& entry : g_vectoredHandlers)
            if (entry.cookie == cookie) { handler = entry.handler; break; }
        unlockVectoredHandlers();
        if (handler && handler(&pointers) == VectoredContinueExecution) {
            if (record->flags & 1) {
                record->code = StatusNoncontinuableException;
                record->flags &= ~0x10u;
                return 0;
            }
            callVectoredContinueHandlers(pointers);
            return 1;
        }
    }
    const bool handled = dispatchFrameHandlers(record, context);
    if (handled) callVectoredContinueHandlers(pointers);
    return handled ? 1 : 0;
}

extern "C" __declspec(dllexport) [[noreturn]] void RtlRestoreContext(
    micant::CONTEXT* context, ExceptionRecord64* exceptionRecord) noexcept {
    if (!context) {
        (void)NtTerminateProcess(reinterpret_cast<void*>(~uintptr_t{0}),
                                 static_cast<int32_t>(0xC000000Du));
        __builtin_trap();
    }

    constexpr uint32_t StatusUnwindConsolidate = 0x80000029u;
    constexpr uint32_t StatusLongJump = 0x80000026u;
    if (exceptionRecord && exceptionRecord->code == StatusUnwindConsolidate) {
        using ConsolidateRoutine = void* (*)(ExceptionRecord64*) noexcept;
        const uintptr_t callbackAddress = exceptionRecord->information[0];
        ImageView64 callbackImage{};
        if (!exceptionRecord->parameterCount || !callbackAddress ||
            !findImage(callbackAddress, callbackImage)) {
            (void)NtTerminateProcess(reinterpret_cast<void*>(~uintptr_t{0}),
                                     static_cast<int32_t>(0xC0000028u));
            __builtin_trap();
        }
        const auto continuation = reinterpret_cast<ConsolidateRoutine>(callbackAddress)(exceptionRecord);
        ImageView64 continuationImage{};
        const uintptr_t continuationAddress = reinterpret_cast<uintptr_t>(continuation);
        if (!continuation || !findImage(continuationAddress, continuationImage)) {
            (void)NtTerminateProcess(reinterpret_cast<void*>(~uintptr_t{0}),
                                     static_cast<int32_t>(0xC0000028u));
            __builtin_trap();
        }
        context->Rip = continuationAddress;
    } else if (exceptionRecord && exceptionRecord->code == StatusLongJump) {
        struct JumpBuffer64 {
            uint64_t frame, rbx, rsp, rbp, rsi, rdi, r12, r13, r14, r15, rip;
            uint32_t mxcsr;
            uint16_t fpCsr, spare;
            micant::M128A xmm[10];
        };
        static_assert(sizeof(JumpBuffer64) == 256);
        const uintptr_t jumpAddress = exceptionRecord->information[0];
        uintptr_t teb{};
        __asm__ __volatile__("movq %%gs:0x30, %0" : "=r"(teb));
        if (exceptionRecord->parameterCount < 1 || !jumpAddress || !teb) {
            (void)NtTerminateProcess(reinterpret_cast<void*>(~uintptr_t{0}),
                                     static_cast<int32_t>(0xC0000028u));
            __builtin_trap();
        }
        const uintptr_t stackBase = *reinterpret_cast<const uintptr_t*>(teb + 8);
        const uintptr_t stackLimit = *reinterpret_cast<const uintptr_t*>(teb + 16);
        if (stackLimit >= stackBase || jumpAddress < stackLimit ||
            jumpAddress > stackBase - sizeof(JumpBuffer64)) {
            (void)NtTerminateProcess(reinterpret_cast<void*>(~uintptr_t{0}),
                                     static_cast<int32_t>(0xC0000028u));
            __builtin_trap();
        }
        JumpBuffer64 jump{};
        memcpy(&jump, reinterpret_cast<const void*>(jumpAddress), sizeof(jump));
        context->Rbx = jump.rbx; context->Rsp = jump.rsp; context->Rbp = jump.rbp;
        context->Rsi = jump.rsi; context->Rdi = jump.rdi;
        context->R12 = jump.r12; context->R13 = jump.r13;
        context->R14 = jump.r14; context->R15 = jump.r15;
        context->Rip = jump.rip;
        context->MxCsr = jump.mxcsr;
        context->FltSave.MxCsr = jump.mxcsr;
        context->FltSave.ControlWord = jump.fpCsr;
        for (size_t i = 0; i < 10; ++i)
            context->FltSave.XmmRegisters[6 + i] = jump.xmm[i];
        context->ContextFlags |= CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_FLOATING_POINT;
    }

    const auto status = NtContinue(context, 0);
    (void)NtTerminateProcess(reinterpret_cast<void*>(~uintptr_t{0}), status);
    __builtin_trap();
}

__declspec(dllexport) uint64_t RtlGetExtendedFeaturesMask(
    micant::CONTEXT_EX* contextEx) noexcept {
    micant::XSTATE_HEADER* header{};
    uint32_t length{};
    return locateXstateChunk(contextEx, header, length) ? header->Mask & ~uint64_t{3} : 0;
}

__declspec(dllexport) void RtlSetExtendedFeaturesMask(
    micant::CONTEXT_EX* contextEx, uint64_t featureMask) noexcept {
    micant::XSTATE_HEADER* header{};
    uint32_t length{};
    if (!locateXstateChunk(contextEx, header, length)) return;
    const uint64_t enabled = readXcr0() & ~uint64_t{3};
    header->Mask = (header->Mask & 3) | (featureMask & enabled);
}

__declspec(dllexport) void* RtlLocateExtendedFeature(
    micant::CONTEXT_EX* contextEx, uint32_t featureId, uint32_t* featureLength) noexcept {
    if (!contextEx) return nullptr;
    auto* context = reinterpret_cast<micant::CONTEXT*>(contextEx) - 1;
    if (!(context->ContextFlags & CONTEXT_XSTATE)) return nullptr;
    if (featureId == 0) {
        if (featureLength)
            *featureLength = offsetof(micant::XMM_SAVE_AREA32, XmmRegisters);
        return &context->FltSave;
    }
    if (featureId == 1) {
        if (featureLength)
            *featureLength = sizeof(context->FltSave.XmmRegisters);
        return context->FltSave.XmmRegisters;
    }

    micant::XSTATE_HEADER* header{};
    uint32_t xstateLength{};
    if (featureId >= 64 || !locateXstateChunk(contextEx, header, xstateLength) ||
        !(readXcr0() & (uint64_t{1} << featureId))) return nullptr;
    uint32_t size{}, standardOffset{};
    queryXstateFeature(featureId, size, standardOffset);
    if (!size || standardOffset < 512) return nullptr;
    const uint32_t offset = standardOffset - 512;
    if (offset > xstateLength || size > xstateLength - offset) return nullptr;
    if (featureLength) *featureLength = size;
    return reinterpret_cast<uint8_t*>(header) + offset;
}

// Synthetic user APC entry: RSP points at the saved CONTEXT whose first four
// home slots carry NormalContext, SystemArgument1, SystemArgument2 and the
// NormalRoutine. Run only the user callback, then restore the interrupted
// thread's complete CONTEXT/XSTATE through NtContinue.
__attribute__((naked, noreturn)) __declspec(dllexport) void KiUserApcDispatcher() noexcept {
    __asm__ __volatile__(
        "movq %rsp, %r12\n"
        "movq 32(%r12), %r13\n"
        "movq 40(%r12), %r14\n"
        "xorl %r15d, %r15d\n"
        "subq $32, %rsp\n"
        "1:\n"
        "cmpq %r14, %r15\n"
        "jae 2f\n"
        "movq 8(%r13), %rcx\n"
        "movq 16(%r13), %rdx\n"
        "movq 24(%r13), %r8\n"
        "movq %r12, %r9\n"
        "callq *0(%r13)\n"
        "addq $32, %r13\n"
        "incq %r15\n"
        "jmp 1b\n"
        "2:\n"
        "movq %r12, %rcx\n"
        "movl $1, %edx\n"
        "callq NtContinue\n"
        "ud2\n");
}

__declspec(dllexport) [[noreturn]] void RtlUnwindEx(
    void* targetFrame, void* targetIp, ExceptionRecord64* record,
    void* returnValue, micant::CONTEXT* context, void* /*historyTable*/) noexcept {
    if (!targetFrame || !targetIp || !context) {
        (void)NtTerminateProcess(reinterpret_cast<void*>(~uintptr_t{0}),
                                 static_cast<int32_t>(0xC0000028u)); // STATUS_BAD_STACK
        __builtin_trap();
    }

    ExceptionRecord64 unwindRecord{};
    if (!record) {
        unwindRecord.code = 0;
        record = &unwindRecord;
    }
    record->flags |= 0x2; // EXCEPTION_UNWINDING

    uintptr_t teb{};
    __asm__ __volatile__("movq %%gs:0x30, %0" : "=r"(teb));
    if (!teb) __builtin_trap();
    const uint64_t stackBase = *reinterpret_cast<const uint64_t*>(teb + 8);
    const uint64_t stackLimit = *reinterpret_cast<const uint64_t*>(teb + 16);
    UnwindState64 walk{};
    auto* sourceRegisters = &context->Rax;
    for (size_t i = 0; i < 16; ++i) walk.registers[i] = sourceRegisters[i];
    walk.rip = context->Rip;
    for (size_t i = 0; i < 16; ++i) walk.xmm[i] = context->FltSave.XmmRegisters[i];

    uint32_t unwindFailureStatus = 0xC0000028u; // STATUS_BAD_STACK
    uint64_t lastRsp{};
    uint64_t lastRip{};
    for (size_t frame = 0; frame < 128 && walk.rip &&
         walk.rsp() >= stackLimit && walk.rsp() < stackBase; ++frame) {
        if (frame && (walk.rsp() < lastRsp ||
            (walk.rsp() == lastRsp && walk.rip == lastRip))) break;
        lastRsp = walk.rsp();
        lastRip = walk.rip;
        const uintptr_t controlPc = static_cast<uintptr_t>(walk.rip - (frame && walk.rip ? 1 : 0));
        ImageView64 image{};
        if (!findImage(controlPc, image)) break;
        RuntimeFunction64* function = lookupFunction(image, controlPc);
        const uint64_t currentRip = walk.rip;
        uint64_t establisher{};
        void* languageHandler{};
        void* handlerData{};
        if (!unwindFrame(image, controlPc, walk, function, UnwindFlagTerminationHandler,
                         stackLimit, stackBase,
                         establisher, languageHandler, handlerData)) break;
        bool targetReached = establisher == reinterpret_cast<uintptr_t>(targetFrame);
        if (languageHandler) {
            HandlerContextStorage handlerStorage{};
            if (!buildHandlerContext(*context, walk, handlerStorage) ||
                !commitHandlerContext(handlerStorage, *context)) break;
            DispatcherContext64 dispatcher{};
            dispatcher.controlPc = controlPc;
            dispatcher.imageBase = image.base;
            dispatcher.functionEntry = function;
            dispatcher.establisherFrame = establisher;
            dispatcher.targetIp = reinterpret_cast<uintptr_t>(targetIp);
            dispatcher.contextRecord = &handlerStorage.context;
            dispatcher.languageHandler = languageHandler;
            dispatcher.handlerData = handlerData;
            using LanguageHandler = int32_t (*)(ExceptionRecord64*, void*, micant::CONTEXT*, DispatcherContext64*);
            int32_t disposition{};
            size_t collidedRetries{};
            bool collidedPending{};
            for (;;) {
                record->flags &= ~0x60u; // TARGET/COLLIDED flags are per invocation.
                if (targetReached) record->flags |= 0x20u;
                if (collidedPending) record->flags |= 0x40u;
                collidedPending = false;
                disposition = reinterpret_cast<LanguageHandler>(languageHandler)(
                    record, reinterpret_cast<void*>(establisher),
                    &handlerStorage.context, &dispatcher);
                record->flags &= ~0x60u;
                if (disposition != ExceptionCollidedUnwind) break;

                if (++collidedRetries > 32) {
                    unwindFailureStatus = 0xC0000028u; // STATUS_BAD_STACK
                    break;
                }
                const uintptr_t collidedAddress = reinterpret_cast<uintptr_t>(dispatcher.contextRecord);
                if (stackBase < sizeof(micant::CONTEXT) ||
                    !dispatcher.contextRecord || (collidedAddress & 15) ||
                    collidedAddress < stackLimit ||
                    collidedAddress > stackBase - sizeof(micant::CONTEXT)) {
                    unwindFailureStatus = 0xC0000028u; // STATUS_BAD_STACK
                    break;
                }

                // ReactOS accepts the context selected by the language
                // handler, provided it belongs to this thread's active stack.
                // Copy it into the caller's context, then perform the virtual
                // unwind again before retrying the collided handler.
                const uint64_t previousRsp = walk.rsp();
                const uint64_t previousRip = walk.rip;
                const uintptr_t sourceContextAddress =
                    reinterpret_cast<uintptr_t>(dispatcher.contextRecord);
                const uintptr_t destinationContextAddress = reinterpret_cast<uintptr_t>(context);
                bool xstateRangesValid = true;
                if (dispatcher.contextRecord->ContextFlags & CONTEXT_XSTATE) {
                    constexpr size_t contextWithExtension = sizeof(micant::CONTEXT) +
                                                            sizeof(micant::CONTEXT_EX);
                    if (!(context->ContextFlags & CONTEXT_XSTATE) ||
                        stackBase < contextWithExtension ||
                        sourceContextAddress < stackLimit ||
                        sourceContextAddress > stackBase - contextWithExtension ||
                        destinationContextAddress < stackLimit ||
                        destinationContextAddress > stackBase - contextWithExtension) {
                        xstateRangesValid = false;
                    } else {
                        auto* sourceEx = reinterpret_cast<micant::CONTEXT_EX*>(
                            sourceContextAddress + sizeof(micant::CONTEXT));
                        auto* destinationEx = reinterpret_cast<micant::CONTEXT_EX*>(
                            destinationContextAddress + sizeof(micant::CONTEXT));
                        const size_t sourceAllLength = sourceEx->All.Length;
                        const size_t destinationAllLength = destinationEx->All.Length;
                        xstateRangesValid =
                            sourceAllLength >= sizeof(micant::CONTEXT) + sizeof(micant::CONTEXT_EX) &&
                            destinationAllLength >= sizeof(micant::CONTEXT) + sizeof(micant::CONTEXT_EX) &&
                            sourceAllLength <= stackBase - sourceContextAddress &&
                            destinationAllLength <= stackBase - destinationContextAddress;
                    }
                }
                if ((reinterpret_cast<uintptr_t>(context) & 15) ||
                    reinterpret_cast<uintptr_t>(context) < stackLimit ||
                    reinterpret_cast<uintptr_t>(context) > stackBase - sizeof(micant::CONTEXT) ||
                    !xstateRangesValid ||
                    !copyContextRecord(dispatcher.contextRecord, context) ||
                    !(context->ContextFlags & 0x00100000u) ||
                    !(context->ContextFlags & CONTEXT_CONTROL) ||
                    context->Rsp < stackLimit || context->Rsp >= stackBase ||
                    !context->Rip) {
                    unwindFailureStatus = 0xC0000028u; // STATUS_BAD_STACK
                    break;
                }
                auto* collidedRegisters = &context->Rax;
                for (size_t i = 0; i < 16; ++i) walk.registers[i] = collidedRegisters[i];
                walk.rip = context->Rip;
                for (size_t i = 0; i < 16; ++i)
                    walk.xmm[i] = context->FltSave.XmmRegisters[i];
                const uintptr_t collidedControlPc = static_cast<uintptr_t>(walk.rip);
                ImageView64 collidedImage{};
                if (!findImage(collidedControlPc, collidedImage)) {
                    unwindFailureStatus = 0xC0000028u; // STATUS_BAD_STACK
                    break;
                }
                RuntimeFunction64* collidedFunction = lookupFunction(collidedImage, collidedControlPc);
                uint64_t collidedEstablisher{};
                void* collidedHandler{};
                void* collidedHandlerData{};
                if (!unwindFrame(collidedImage, collidedControlPc, walk, collidedFunction,
                                 UnwindFlagTerminationHandler, stackLimit, stackBase,
                                 collidedEstablisher, collidedHandler, collidedHandlerData) ||
                    collidedEstablisher < stackLimit || collidedEstablisher >= stackBase ||
                    (collidedEstablisher & 7) ||
                    walk.rsp() < stackLimit || walk.rsp() >= stackBase ||
                    (walk.rsp() <= previousRsp && walk.rip == previousRip)) {
                    unwindFailureStatus = 0xC0000028u; // STATUS_BAD_STACK
                    break;
                }
                if (!buildHandlerContext(*context, walk, handlerStorage) ||
                    !commitHandlerContext(handlerStorage, *context)) {
                    unwindFailureStatus = 0xC0000028u; // STATUS_BAD_STACK
                    break;
                }
                establisher = collidedEstablisher;
                targetReached = establisher == reinterpret_cast<uintptr_t>(targetFrame);
                image = collidedImage;
                if (collidedHandler) languageHandler = collidedHandler;
                handlerData = collidedHandlerData;
                dispatcher.controlPc = collidedControlPc;
                dispatcher.imageBase = image.base;
                dispatcher.functionEntry = collidedFunction;
                dispatcher.establisherFrame = establisher;
                dispatcher.handlerData = handlerData;
                dispatcher.languageHandler = languageHandler;
                dispatcher.contextRecord = &handlerStorage.context;
                record->flags |= 0x40u; // Retry this handler with the collided context.
                collidedPending = true;
                if (!collidedHandler) {
                    disposition = ExceptionContinueSearch;
                    break;
                }
            }
            if (disposition == ExceptionCollidedUnwind) break;
            if (disposition != ExceptionContinueSearch) {
                unwindFailureStatus = StatusInvalidDisposition;
                break;
            }
        }
        if (targetReached) record->flags |= 0x20u; // Retain target unwind for the transfer.
        if (targetReached) {
            for (size_t i = 0; i < 16; ++i) sourceRegisters[i] = walk.registers[i];
            for (size_t i = 0; i < 16; ++i) context->FltSave.XmmRegisters[i] = walk.xmm[i];
            if (record->code != StatusUnwindConsolidate)
                context->Rip = reinterpret_cast<uintptr_t>(targetIp);
            context->Rsp = reinterpret_cast<uintptr_t>(targetFrame);
            context->Rax = reinterpret_cast<uintptr_t>(returnValue);
            RtlRestoreContext(context, record);
        }
        if (walk.rsp() <= stackLimit || walk.rsp() >= stackBase) break;
    }

    (void)NtTerminateProcess(reinterpret_cast<void*>(~uintptr_t{0}),
                             static_cast<int32_t>(unwindFailureStatus));
    __builtin_trap();
}

extern "C" __declspec(dllexport) void* RtlAddVectoredExceptionHandler(
    unsigned long first, VectoredExceptionHandler handler) noexcept {
    return addVectoredHandler(g_vectoredHandlers, first, handler);
}

extern "C" __declspec(dllexport) unsigned long RtlRemoveVectoredExceptionHandler(void* cookie) noexcept {
    return removeVectoredHandler(g_vectoredHandlers, cookie);
}

extern "C" __declspec(dllexport) void* RtlAddVectoredContinueHandler(
    unsigned long first, VectoredExceptionHandler handler) noexcept {
    return addVectoredHandler(g_vectoredContinueHandlers, first, handler);
}

extern "C" __declspec(dllexport) unsigned long RtlRemoveVectoredContinueHandler(void* cookie) noexcept {
    return removeVectoredHandler(g_vectoredContinueHandlers, cookie);
}

extern "C" __declspec(dllexport) [[noreturn]] void KiUserExceptionDispatcher(
    ExceptionRecord64* record, micant::CONTEXT* context) noexcept {
    if (record && context && RtlDispatchException(record, context))
        RtlRestoreContext(context, nullptr);
    const auto code = record ? record->code : static_cast<uint32_t>(0xC0000005u);
    (void)NtTerminateProcess(reinterpret_cast<void*>(~uintptr_t{0}), static_cast<int32_t>(code));
    __builtin_trap();
}

__attribute__((naked)) __declspec(dllexport) int32_t NtResumeThread(void*, uint32_t*) noexcept {
    __asm__ __volatile__("movq %rcx, %r10\n\tmovq $0x52, %rax\n\tsyscall\n\tret\n\t");
}

__attribute__((naked)) __declspec(dllexport) int32_t NtCreateThreadEx(
    void**, uint32_t, void*, void*, void*, void*, uint32_t, uintptr_t, size_t, size_t, void*) noexcept {
    __asm__ __volatile__("movq %rcx, %r10\n\tmovq $0xbd, %rax\n\tsyscall\n\tret\n\t");
}

__attribute__((naked)) __declspec(dllexport) int32_t NtCreateProcessEx(
    void**, uint32_t, void*, void*, uint32_t, void*, void*, void*, uint8_t) noexcept {
#if defined(__x86_64__)
    __asm__ __volatile__("movq %rcx, %r10\n\tmovq $0xb4, %rax\n\tsyscall\n\tret\n\t");
#else
    return 0;
#endif
}

// Initial AMD64 thread entry receives the caller's routine and parameter in
// RCX/RDX. Returning from the routine terminates this thread with its result.
__attribute__((naked, noreturn)) __declspec(dllexport) void RtlUserThreadStart(void*, void*) noexcept {
    __asm__ __volatile__(
        "andq $-16, %rsp\nsubq $32, %rsp\nmovq %rcx, %rax\nmovq %rdx, %rcx\n"
        "callq *%rax\nmovl %eax, %edx\nmovq $-2, %rcx\nmovq %rcx, %r10\n"
        "movq $0x53, %rax\nsyscall\nud2\n");
}

__attribute__((naked)) __declspec(dllexport) uint64_t NtShutdownSystem(uint32_t /*action*/) noexcept {
#if defined(__x86_64__)
    __asm__ __volatile__(
        "movq %rcx, %r10\n\t"
        "movq $0x08, %rax\n\t"
        "movq %rdx, %r8\n\t"
        "syscall\n\t"
        "ret\n\t"
    );
#else
    return 0;
#endif
}

__attribute__((naked)) __declspec(dllexport) uint64_t NtQuerySystemInformation(
    uint32_t /*infoClass*/,
    void* /*infoBuffer*/,
    uint32_t /*length*/,
    uint32_t* /*returnLength*/
) noexcept {
#if defined(__x86_64__)
    __asm__ __volatile__(
        "movq %rcx, %r10\n\t"
        "movq $0x11, %rax\n\t"
        "movq %rdx, %r8\n\t"
        "syscall\n\t"
        "ret\n\t"
    );
#else
    return 0;
#endif
}

__attribute__((naked)) __declspec(dllexport) int32_t NtAllocateVirtualMemory(
    void* /*processHandle*/,
    void** /*baseAddress*/,
    uint64_t /*zeroBits*/,
    size_t* /*regionSize*/,
    uint32_t /*allocationType*/,
    uint32_t /*protect*/
) noexcept {
#if defined(__x86_64__)
    __asm__ __volatile__(
        "movq %rcx, %r10\n\t"
        "movq $0x18, %rax\n\t"
        "syscall\n\t"
        "ret\n\t"
    );
#else
    return 0;
#endif
}

__attribute__((naked)) __declspec(dllexport) int32_t NtFreeVirtualMemory(
    void* /*processHandle*/,
    void** /*baseAddress*/,
    size_t* /*regionSize*/,
    uint32_t /*freeType*/
) noexcept {
#if defined(__x86_64__)
    __asm__ __volatile__(
        "movq %rcx, %r10\n\t"
        "movq $0x1E, %rax\n\t"
        "syscall\n\t"
        "ret\n\t"
    );
#else
    return 0;
#endif
}

__attribute__((naked)) __declspec(dllexport) int32_t NtClose(void* /*handle*/) noexcept {
#if defined(__x86_64__)
    __asm__ __volatile__(
        "movq %rcx, %r10\n\t"
        "movl $0x0F, %eax\n\t"
        "syscall\n\t"
        "ret\n\t"
    );
#else
    return 0;
#endif
}

__attribute__((naked)) __declspec(dllexport) int32_t NtCreateEvent(
    void** /*handle*/, uint32_t /*access*/, void* /*attributes*/, uint32_t /*type*/, uint8_t /*initial*/) noexcept {
    __asm__ __volatile__("movq %rcx, %r10\n\tmovl $0x48, %eax\n\tsyscall\n\tret\n\t");
}

__attribute__((naked)) __declspec(dllexport) int32_t NtSetEvent(
    void* /*handle*/, int32_t* /*previous*/) noexcept {
    __asm__ __volatile__("movq %rcx, %r10\n\tmovl $0x4e, %eax\n\tsyscall\n\tret\n\t");
}

__attribute__((naked)) __declspec(dllexport) int32_t NtWaitForSingleObject(
    void* /*handle*/, uint8_t /*alertable*/, int64_t* /*timeout*/) noexcept {
    __asm__ __volatile__("movq %rcx, %r10\n\tmovl $0x04, %eax\n\tsyscall\n\tret\n\t");
}

__attribute__((naked)) __declspec(dllexport) int32_t NtQueueApcThread(
    void* /*threadHandle*/, void* /*apcRoutine*/, void* /*apcContext*/,
    void* /*systemArgument1*/, void* /*systemArgument2*/) noexcept {
    __asm__ __volatile__("movq %rcx, %r10\n\tmovl $0xbf, %eax\n\tsyscall\n\tret\n\t");
}

// These service numbers are the MicaNT NTDLL/kernel profile, not a stable
// Windows ABI. The x64 home area and arguments after the fourth stay on the
// caller's stack for the kernel to capture.
__attribute__((naked)) __declspec(dllexport) int32_t NtCreateSection(
    void** /*sectionHandle*/, uint32_t /*desiredAccess*/, void* /*objectAttributes*/,
    int64_t* /*maximumSize*/, uint32_t /*pageProtection*/,
    uint32_t /*allocationAttributes*/, void* /*fileHandle*/) noexcept {
    __asm__ __volatile__("movq %rcx, %r10\n\tmovl $0x4a, %eax\n\tsyscall\n\tret\n\t");
}

__attribute__((naked)) __declspec(dllexport) int32_t NtCreateFile(
    void** /*fileHandle*/, uint32_t /*desiredAccess*/, void* /*objectAttributes*/,
    void* /*ioStatusBlock*/, int64_t* /*allocationSize*/, uint32_t /*fileAttributes*/,
    uint32_t /*shareAccess*/, uint32_t /*createDisposition*/, uint32_t /*createOptions*/,
    void* /*eaBuffer*/, uint32_t /*eaLength*/) noexcept {
    __asm__ __volatile__("movq %rcx, %r10\nmovl $0x55, %eax\nsyscall\nretq\n");
}

__attribute__((naked)) __declspec(dllexport) int32_t NtDeviceIoControlFile(
    void* /*fileHandle*/, void* /*event*/, void* /*apcRoutine*/, void* /*apcContext*/,
    void* /*ioStatusBlock*/, uint32_t /*ioControlCode*/, const void* /*inputBuffer*/,
    uint32_t /*inputBufferLength*/, void* /*outputBuffer*/, uint32_t /*outputBufferLength*/) noexcept {
    __asm__ __volatile__("movq %rcx, %r10\nmovl $0x07, %eax\nsyscall\nretq\n");
}

__attribute__((naked)) __declspec(dllexport) int32_t NtMapViewOfSection(
    void* /*sectionHandle*/, void* /*processHandle*/, void** /*baseAddress*/,
    uintptr_t /*zeroBits*/, size_t /*commitSize*/, int64_t* /*sectionOffset*/,
    size_t* /*viewSize*/, uint32_t /*inheritDisposition*/, uint32_t /*allocationType*/,
    uint32_t /*win32Protect*/) noexcept {
    __asm__ __volatile__("movq %rcx, %r10\n\tmovl $0x28, %eax\n\tsyscall\n\tret\n\t");
}

__attribute__((naked)) __declspec(dllexport) int32_t NtUnmapViewOfSection(
    void* /*processHandle*/, void* /*baseAddress*/) noexcept {
    __asm__ __volatile__("movq %rcx, %r10\n\tmovl $0x2a, %eax\n\tsyscall\n\tret\n\t");
}

__attribute__((naked)) __declspec(dllexport) uint64_t NtReadFile(
    void* /*fileHandle*/,
    void* /*event*/,
    void* /*apcRoutine*/,
    void* /*apcContext*/,
    void* /*ioStatusBlock*/,
    void* /*buffer*/,
    uint32_t /*length*/,
    void* /*byteOffset*/,
    uint32_t* /*key*/
) noexcept {
#if defined(__x86_64__)
    __asm__ __volatile__(
        "movq %rcx, %r10\n\t"
        "movq $0x06, %rax\n\t"
        "syscall\n\t"
        "ret\n\t"
    );
#else
    return 0;
#endif
}

__attribute__((naked)) __declspec(dllexport) int32_t NtSetInformationFile(
    void* /*handle*/, void* /*status*/, void* /*information*/, uint32_t /*length*/, uint32_t /*class*/) noexcept {
    __asm__ __volatile__("movq %rcx, %r10\n\tmovl $0x27, %eax\n\tsyscall\n\tretq\n");
}
__attribute__((naked)) __declspec(dllexport) int32_t NtCreateIoCompletion(
    void** /*handle*/, uint32_t /*access*/, void* /*attributes*/, uint32_t /*concurrency*/) noexcept {
    __asm__ __volatile__("movq %rcx, %r10\n\tmovl $0x164, %eax\n\tsyscall\n\tretq\n");
}
__attribute__((naked)) __declspec(dllexport) int32_t NtSetIoCompletion(
    void* /*handle*/, uintptr_t /*key*/, uintptr_t /*context*/, int32_t /*status*/, uintptr_t /*information*/) noexcept {
    __asm__ __volatile__("movq %rcx, %r10\n\tmovl $0x165, %eax\n\tsyscall\n\tretq\n");
}
__attribute__((naked)) __declspec(dllexport) int32_t NtRemoveIoCompletion(
    void* /*handle*/, uintptr_t* /*key*/, uintptr_t* /*context*/, void* /*status*/, int64_t* /*timeout*/) noexcept {
    __asm__ __volatile__("movq %rcx, %r10\n\tmovl $0x09, %eax\n\tsyscall\n\tretq\n");
}

__attribute__((naked)) __declspec(dllexport) int32_t NtCancelIoFileEx(
    void* /*fileHandle*/, void* /*requestStatus*/, void* /*ioStatus*/) noexcept {
    __asm__ __volatile__("movq %rcx, %r10\n\tmovl $0x90, %eax\n\tsyscall\n\tretq\n");
}

__attribute__((naked)) __declspec(dllexport) uint64_t NtWriteFile(
    void* /*fileHandle*/,
    void* /*event*/,
    void* /*apcRoutine*/,
    void* /*apcContext*/,
    void* /*ioStatusBlock*/,
    const void* /*buffer*/,
    uint32_t /*length*/,
    void* /*byteOffset*/,
    uint32_t* /*key*/
) noexcept {
#if defined(__x86_64__)
    __asm__ __volatile__(
        "movq %rcx, %r10\n\t"
        "movq $0x25, %rax\n\t"
        "movq %rdx, %r8\n\t"
        "syscall\n\t"
        "ret\n\t"
    );
#else
    return 0;
#endif
}

__attribute__((naked)) __declspec(dllexport) uint64_t NtIcmpSendEcho(
    uint32_t /*destIp*/,
    const void* /*buffer*/,
    uint32_t /*size*/,
    uint32_t /*timeoutMs*/
) noexcept {
#if defined(__x86_64__)
    __asm__ __volatile__(
        "movq %rcx, %r10\n\t"
        "movq $0x26, %rax\n\t"
        "movq %rdx, %r8\n\t"
        "syscall\n\t"
        "ret\n\t"
    );
#else
    return 0;
#endif
}

__attribute__((naked)) __declspec(dllexport) uint64_t NtYieldExecution() noexcept {
#if defined(__x86_64__)
    __asm__ __volatile__(
        "movq $0x46, %rax\n\t"
        "syscall\n\t"
        "ret\n\t"
    );
#else
    return 0;
#endif
}

__attribute__((naked)) __declspec(dllexport) uint64_t NtGdiGetVideoInfo(void* /*videoInfo*/) noexcept {
#if defined(__x86_64__)
    __asm__ __volatile__(
        "movq %rcx, %r10\n\t"
        "movq $0x80, %rax\n\t"
        "movq %rdx, %r8\n\t"
        "syscall\n\t"
        "ret\n\t"
    );
#else
    return 0;
#endif
}

__attribute__((naked)) __declspec(dllexport) uint64_t NtGdiPresentSurface(const void* /*params*/) noexcept {
#if defined(__x86_64__)
    __asm__ __volatile__(
        "movq %rcx, %r10\n\t"
        "movq $0x81, %rax\n\t"
        "movq %rdx, %r8\n\t"
        "syscall\n\t"
        "ret\n\t"
    );
#else
    return 0;
#endif
}

__declspec(dllexport) uint64_t NtQueryVirtualMemory() noexcept { return 0; }

// ============================================================================
// 2. RTL Runtime Library Functions
// ============================================================================

struct UnicodeString {
    uint16_t length{0};
    uint16_t maximumLength{0};
    wchar_t* buffer{nullptr};
};

__declspec(dllexport) void RtlInitUnicodeString(UnicodeString* dest, const wchar_t* source) noexcept {
    if (!dest) return;
    dest->buffer = const_cast<wchar_t*>(source);
    if (!source) {
        dest->length = 0;
        dest->maximumLength = 0;
        return;
    }
    uint16_t len = 0;
    while (source[len] != L'\0') len++;
    dest->length = static_cast<uint16_t>(len * sizeof(wchar_t));
    dest->maximumLength = static_cast<uint16_t>((len + 1) * sizeof(wchar_t));
}

__declspec(dllexport) void RtlZeroMemory(void* dest, size_t length) noexcept {
    memset(dest, 0, length);
}

__declspec(dllexport) void RtlCopyMemory(void* dest, const void* src, size_t length) noexcept {
    memcpy(dest, src, length);
}

__declspec(dllexport) void RtlMoveMemory(void* dest, const void* src, size_t length) noexcept {
    memmove(dest, src, length);
}

__declspec(dllexport) bool RtlEqualUnicodeString(const UnicodeString* s1, const UnicodeString* s2, bool caseInSensitive) noexcept {
    if (!s1 || !s2) return s1 == s2;
    if (s1->length != s2->length) return false;
    size_t chars = s1->length / sizeof(wchar_t);
    for (size_t i = 0; i < chars; ++i) {
        wchar_t c1 = s1->buffer[i];
        wchar_t c2 = s2->buffer[i];
        if (caseInSensitive) {
            if (c1 >= L'A' && c1 <= L'Z') c1 += (L'a' - L'A');
            if (c2 >= L'A' && c2 <= L'Z') c2 += (L'a' - L'A');
        }
        if (c1 != c2) return false;
    }
    return true;
}

// Sovereign Static Heap Pool (64 KB)
static uint8_t s_heapStorage[65536];
static size_t s_heapOffset = 0;

__declspec(dllexport) void* RtlAllocateHeap(void* /*heapHandle*/, uint32_t /*flags*/, size_t size) noexcept {
    size_t aligned = (size + 15) & ~15ULL;
    if (s_heapOffset + aligned <= sizeof(s_heapStorage)) {
        void* ptr = &s_heapStorage[s_heapOffset];
        s_heapOffset += aligned;
        return ptr;
    }
    return nullptr;
}

__declspec(dllexport) bool RtlFreeHeap(void* /*heapHandle*/, uint32_t /*flags*/, void* /*baseAddress*/) noexcept {
    return true;
}

// ============================================================================
// 3. Loader Thunks
// ============================================================================

__declspec(dllexport) void LdrInitializeThunk() noexcept {}
__declspec(dllexport) void LdrLoadDll() noexcept {}
__declspec(dllexport) void LdrGetProcedureAddress() noexcept {}

} // extern "C"
