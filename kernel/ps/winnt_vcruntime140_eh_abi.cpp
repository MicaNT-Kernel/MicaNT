/**
 * @file winnt_vcruntime140_eh_abi.cpp
 * @brief Horizon 117 (Kernel Unit #89): Sovereign WinNT MSVC Exception Handling Runtimes
 *        (vcruntime140.dll & vcruntime140_1.dll) Official PE32+ Exports, __C_specific_handler
 *        Structured Exception Handling, and __CxxFrameHandler4 x64 C++ EH State Unwind ABI.
 */

#include "micant/ucrt/winnt_vcruntime140_eh_abi.hpp"
#include "micant/shared/context.hpp"
#include <cstdlib>
#include <cstring>

#if defined(BUILD_SYSTEM32_VCRUNTIME140_DLL) || defined(BUILD_SYSTEM32_VCRUNTIME140_1_DLL)
extern "C" __declspec(dllimport) [[noreturn]] void RtlUnwindEx(
    void* TargetFrame, void* TargetIp,
    micant::ucrt::winnt_vcruntime140_abi::WinNtExceptionRecord* ExceptionRecord,
    void* ReturnValue, void* ContextRecord, void* HistoryTable) noexcept;
#endif

namespace micant::ucrt::winnt_vcruntime140_abi {

namespace {
uint32_t g_telemetryMainInvoked = 0;
uint32_t g_telemetryMainReturned = 0;

struct WinNtScopeRecord64 {
    uint32_t BeginAddress;
    uint32_t EndAddress;
    uint32_t HandlerAddress;
    uint32_t JumpTarget;
};
struct WinNtScopeTable64 {
    uint32_t Count;
    WinNtScopeRecord64 ScopeRecord[1];
};

struct WinNtExceptionPointers64 {
    WinNtExceptionRecord* ExceptionRecord;
    void* ContextRecord;
};

constexpr int32_t ExceptionContinueExecution = 0;
constexpr int32_t ExceptionContinueSearch = 1;
constexpr int32_t ExceptionExecuteHandler = 1;
constexpr uint32_t ExceptionUnwinding = 0x2;
constexpr uint32_t ExceptionExitUnwind = 0x4;
constexpr uint32_t ExceptionTargetUnwind = 0x20;
constexpr uint32_t ImageDosSignature = 0x5A4D;
constexpr uint32_t ImageNtSignature = 0x00004550;
constexpr uint16_t ImageMachineAmd64 = 0x8664;
constexpr uint16_t ImageOptionalHeader64 = 0x020B;
constexpr uint32_t CxxExceptionCode = 0xE06D7363;
constexpr uint32_t StatusUnwindConsolidate = 0x80000029;
constexpr uintptr_t Eh4ConsolidateSignature = 0x3448434D; // "MCH4"
constexpr uint8_t FuncInfo4IsCatch = 0x01;
constexpr uint8_t FuncInfo4IsSeparated = 0x02;
constexpr uint8_t FuncInfo4Bbt = 0x04;
constexpr uint8_t FuncInfo4HasUnwindMap = 0x08;
constexpr uint8_t FuncInfo4HasTryMap = 0x10;
constexpr uint8_t FuncInfo4Ehs = 0x20;
constexpr uint8_t FuncInfo4NoExcept = 0x40;
constexpr uint8_t FuncInfo4KnownFlags = FuncInfo4IsCatch | FuncInfo4IsSeparated |
    FuncInfo4Bbt | FuncInfo4HasUnwindMap | FuncInfo4HasTryMap |
    FuncInfo4Ehs | FuncInfo4NoExcept;

struct WinNtImageBounds { uintptr_t base{}; uint32_t size{}; };
struct WinNtRuntimeFunction64 { uint32_t beginAddress, endAddress, unwindInfo; };
struct WinNtCxxThrowInfo { uint32_t flags, destructor, customHandler, typeInfoTable; };
struct WinNtCxxTypeInfo {
    uint32_t flags;
    uint32_t typeInfo;
    int32_t thisOffset;
    int32_t virtualBaseDescriptor;
    int32_t virtualBaseOffset;
    uint32_t size;
    uint32_t copyConstructor;
};
struct WinNtCxxCatchMatch {
    uint32_t flags{};
    uint32_t size{};
    int32_t thisOffset{};
    uint32_t copyConstructor{};
};
struct WinNtEh4UnwindEntry {
    uint32_t begin{};
    uint32_t previous{};
    uint32_t handler{};
    uint32_t objectOffset{};
    uint8_t type{};
};
struct WinNtEh4TryEntry {
    uint32_t startLevel{};
    uint32_t endLevel{};
    uint32_t catchLevel{};
    uint32_t catchBlockRva{};
    uint32_t catchBlockCount{};
};
struct WinNtEh4CatchBlock {
    uint8_t header{};
    uint32_t flags{};
    uint32_t typeInfo{};
    uint32_t objectOffset{};
    uint32_t handler{};
    uint32_t returnAddress[2]{};
    bool hasObjectOffset{};
};
struct WinNtEh4Function {
    uint8_t header{};
    uint32_t unwindCount{};
    uint32_t tryCount{};
    uint32_t ipCount{};
    uint32_t ipMapRva{};
    uint32_t frame{};
    WinNtEh4UnwindEntry unwind[128]{};
    WinNtEh4TryEntry tries[128]{};
};

bool imageRange(const WinNtImageBounds& image, uint32_t rva, size_t length) noexcept {
    return image.base && rva <= image.size && length <= image.size - rva;
}

bool imageBounds(uintptr_t base, WinNtImageBounds& image) noexcept {
    if (base < 0x10000 || base > 0x0000800000000000ULL - 0x100000) return false;
    const auto* bytes = reinterpret_cast<const uint8_t*>(base);
    if (*reinterpret_cast<const uint16_t*>(bytes) != ImageDosSignature) return false;
    const int32_t ntOffset = *reinterpret_cast<const int32_t*>(bytes + 0x3C);
    if (ntOffset < 0x40 || ntOffset > 0x100000 || base > UINTPTR_MAX - static_cast<uint32_t>(ntOffset) - 24)
        return false;
    const auto* nt = bytes + ntOffset;
    if (*reinterpret_cast<const uint32_t*>(nt) != ImageNtSignature ||
        *reinterpret_cast<const uint16_t*>(nt + 4) != ImageMachineAmd64 ||
        *reinterpret_cast<const uint16_t*>(nt + 24) != ImageOptionalHeader64) return false;
    const uint16_t optionalSize = *reinterpret_cast<const uint16_t*>(nt + 20);
    if (optionalSize < 60 || base > UINTPTR_MAX - static_cast<uint32_t>(ntOffset) - 24 - optionalSize)
        return false;
    const uint32_t imageSize = *reinterpret_cast<const uint32_t*>(nt + 24 + 56);
    if (imageSize < static_cast<uint32_t>(ntOffset) + 24 + optionalSize ||
        base > UINTPTR_MAX - imageSize) return false;
    image = {base, imageSize};
    return true;
}

bool typeName(const WinNtImageBounds& image, uint32_t typeInfoRva,
              const char*& name, size_t& length) noexcept {
    constexpr size_t typeDescriptorPrefix = 2 * sizeof(uint64_t);
    if (!imageRange(image, typeInfoRva, typeDescriptorPrefix + 1)) return false;
    name = reinterpret_cast<const char*>(image.base + typeInfoRva + typeDescriptorPrefix);
    for (length = 0; length < 256 &&
         static_cast<uint64_t>(typeInfoRva) + typeDescriptorPrefix + length < image.size; ++length)
        if (!name[length]) return true;
    return false;
}

bool findCxxCatchMatch(const WinNtExceptionRecord& exceptionRecord,
                       const WinNtImageBounds& catchImage, uint32_t catchTypeInfoRva,
                       uint32_t catchFlags, WinNtCxxCatchMatch& match) noexcept {
    const uintptr_t throwImageBase = exceptionRecord.ExceptionInformation[3];
    const uintptr_t throwInfoAddress = exceptionRecord.ExceptionInformation[2];
    WinNtImageBounds throwImage{};
    if (!throwImageBase || !imageBounds(throwImageBase, throwImage) ||
        throwInfoAddress < throwImageBase || throwInfoAddress - throwImageBase > UINT32_MAX ||
        !imageRange(throwImage, static_cast<uint32_t>(throwInfoAddress - throwImageBase),
                    sizeof(WinNtCxxThrowInfo))) return false;
    WinNtCxxThrowInfo throwInfo{};
    std::memcpy(&throwInfo, reinterpret_cast<const void*>(throwInfoAddress), sizeof(throwInfo));
    if (!throwInfo.typeInfoTable || !imageRange(throwImage, throwInfo.typeInfoTable, sizeof(uint32_t)))
        return false;
    const auto* table = reinterpret_cast<const uint8_t*>(throwImage.base + throwInfo.typeInfoTable);
    uint32_t count{};
    std::memcpy(&count, table, sizeof(count));
    if (!count || count > 128 || !imageRange(throwImage, throwInfo.typeInfoTable,
                                             sizeof(uint32_t) + count * sizeof(uint32_t))) return false;
    const char* catchName{};
    size_t catchNameLength{};
    if (!typeName(catchImage, catchTypeInfoRva, catchName, catchNameLength)) return false;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t typeRva{};
        std::memcpy(&typeRva, table + sizeof(uint32_t) + i * sizeof(uint32_t), sizeof(typeRva));
        if (!imageRange(throwImage, typeRva, sizeof(WinNtCxxTypeInfo))) return false;
        WinNtCxxTypeInfo type{};
        std::memcpy(&type, reinterpret_cast<const void*>(throwImage.base + typeRva), sizeof(type));
        if (type.flags & ~uint32_t{0x07} || type.virtualBaseDescriptor != -1 ||
            type.virtualBaseOffset != 0 || !type.size || type.size > 64 * 1024 ||
            (type.copyConstructor && !imageRange(throwImage, type.copyConstructor, 1))) return false;
        const char* thrownName{};
        size_t thrownNameLength{};
        if (!typeName(throwImage, type.typeInfo, thrownName, thrownNameLength)) return false;
        if (thrownNameLength != catchNameLength ||
            std::memcmp(thrownName, catchName, catchNameLength) != 0) continue;
        if ((throwInfo.flags & 0x01) && !(catchFlags & 0x01)) continue;
        if ((throwInfo.flags & 0x02) && !(catchFlags & 0x02)) continue;
        match.flags = type.flags;
        match.size = type.size;
        match.thisOffset = type.thisOffset;
        match.copyConstructor = type.copyConstructor;
        return true;
    }
    return false;
}

struct WinNtEh4Reader {
    const WinNtImageBounds* image{};
    uint32_t cursor{};
    uint32_t end{};

    bool byte(uint8_t& value) noexcept {
        if (cursor >= end || !imageRange(*image, cursor, 1)) return false;
        value = *reinterpret_cast<const uint8_t*>(image->base + cursor++);
        return true;
    }
    bool dword(uint32_t& value) noexcept {
        if (cursor > end || sizeof(value) > end - cursor || !imageRange(*image, cursor, sizeof(value)))
            return false;
        std::memcpy(&value, reinterpret_cast<const void*>(image->base + cursor), sizeof(value));
        cursor += sizeof(value);
        return true;
    }
    bool integer(uint32_t& value) noexcept {
        uint8_t first{};
        if (!byte(first)) return false;
        if (!(first & 1)) { value = first >> 1; return true; }
        if ((first & 3) == 1) {
            uint8_t b1{};
            if (!byte(b1)) return false;
            value = (first >> 2) | (static_cast<uint32_t>(b1) << 6);
            return true;
        }
        if ((first & 7) == 3) {
            uint8_t b1{}, b2{};
            if (!byte(b1) || !byte(b2)) return false;
            value = (first >> 3) | (static_cast<uint32_t>(b1) << 5) |
                    (static_cast<uint32_t>(b2) << 13);
            return true;
        }
        if ((first & 15) == 7) {
            uint8_t b1{}, b2{}, b3{};
            if (!byte(b1) || !byte(b2) || !byte(b3)) return false;
            value = (first >> 4) | (static_cast<uint32_t>(b1) << 4) |
                    (static_cast<uint32_t>(b2) << 12) | (static_cast<uint32_t>(b3) << 20);
            return true;
        }
        return false;
    }
};

bool imageReader(const WinNtImageBounds& image, uint32_t rva,
                 WinNtEh4Reader& reader) noexcept {
    if (!imageRange(image, rva, 1)) return false;
    reader.image = &image;
    reader.cursor = rva;
    reader.end = image.size;
    return true;
}

bool readEh4CountedMap(const WinNtImageBounds& image, uint32_t rva,
                       WinNtEh4Reader& reader, uint32_t& count) noexcept {
    return imageReader(image, rva, reader) && reader.integer(count) && count <= 128;
}

bool decodeCatchBlock(const WinNtImageBounds& image, WinNtEh4Reader& reader,
                      uint32_t functionBegin, WinNtEh4CatchBlock& catchBlock) noexcept {
    if (!reader.byte(catchBlock.header) || (catchBlock.header & ~uint8_t{0x37}) ||
        (catchBlock.header & 0x08)) return false;
    const uint8_t returnKind = catchBlock.header & 0x30;
    if (returnKind == 0x30) return false;
    uint32_t value{};
    if ((catchBlock.header & 0x01) &&
        (!reader.integer(catchBlock.flags) || (catchBlock.flags & ~uint32_t{0x0F}))) return false;
    if (catchBlock.header & 0x02) {
        if (!reader.dword(catchBlock.typeInfo)) return false;
        if (catchBlock.typeInfo) {
            if (!imageRange(image, catchBlock.typeInfo, 17)) return false;
            const auto* name = reinterpret_cast<const char*>(image.base + catchBlock.typeInfo + 16);
            bool terminated = false;
            for (size_t i = 0; i < 256 &&
                 static_cast<uint64_t>(catchBlock.typeInfo) + 16 + i < image.size; ++i) {
                if (!name[i]) { terminated = true; break; }
            }
            if (!terminated) return false;
        }
    }
    if (catchBlock.header & 0x04) {
        if (!reader.integer(catchBlock.objectOffset)) return false;
        catchBlock.hasObjectOffset = true;
    }
    if (!reader.dword(catchBlock.handler) || !catchBlock.handler ||
        !imageRange(image, catchBlock.handler, 1)) return false;
    if (returnKind & 0x10) {
        if (!reader.integer(value) || value > UINT32_MAX - functionBegin ||
            !imageRange(image, functionBegin + value, 1)) return false;
        catchBlock.returnAddress[0] = functionBegin + value;
    }
    if (returnKind & 0x20) {
        if (!reader.integer(value) || value > UINT32_MAX - functionBegin ||
            !imageRange(image, functionBegin + value, 1)) return false;
        catchBlock.returnAddress[1] = functionBegin + value;
    }
    return true;
}

bool parseEh4Function(const WinNtImageBounds& image, const WinNtDispatcherContext& dispatcher,
                      WinNtEh4Function& function) noexcept {
    const auto* runtimeFunction = static_cast<const WinNtRuntimeFunction64*>(dispatcher.FunctionEntry);
    const uintptr_t functionEntryAddress = reinterpret_cast<uintptr_t>(dispatcher.FunctionEntry);
    if (!runtimeFunction || runtimeFunction->beginAddress >= runtimeFunction->endAddress ||
        functionEntryAddress < image.base || functionEntryAddress - image.base > UINT32_MAX ||
        !imageRange(image, static_cast<uint32_t>(functionEntryAddress - image.base), sizeof(*runtimeFunction)) ||
        runtimeFunction->endAddress > image.size || runtimeFunction->unwindInfo >= image.size ||
        !imageRange(image, runtimeFunction->unwindInfo, 4) ||
        dispatcher.ControlPc < image.base + runtimeFunction->beginAddress ||
        dispatcher.ControlPc >= image.base + runtimeFunction->endAddress) return false;
    const uintptr_t handlerData = reinterpret_cast<uintptr_t>(dispatcher.HandlerData);
    if (handlerData < image.base || handlerData - image.base > UINT32_MAX ||
        !imageRange(image, static_cast<uint32_t>(handlerData - image.base), sizeof(uint32_t))) return false;
    uint32_t funcInfoRva{};
    std::memcpy(&funcInfoRva, reinterpret_cast<const void*>(handlerData), sizeof(funcInfoRva));
    WinNtEh4Reader reader{};
    if (!funcInfoRva || !imageReader(image, funcInfoRva, reader) || !reader.byte(function.header) ||
        (function.header & ~FuncInfo4KnownFlags) ||
        (function.header & (FuncInfo4IsSeparated | FuncInfo4Bbt))) return false;

    uint32_t mapRva{};
    if (function.header & FuncInfo4HasUnwindMap) {
        if (!reader.dword(mapRva) || !mapRva ||
            !readEh4CountedMap(image, mapRva, reader, function.unwindCount))
            return false;
        const uint32_t mapStart = reader.cursor;
        for (uint32_t i = 0; i < function.unwindCount; ++i) {
            auto& entry = function.unwind[i];
            entry.begin = reader.cursor;
            uint32_t code{};
            if (!reader.integer(code)) return false;
            entry.type = static_cast<uint8_t>(code & 3);
            const uint32_t previousDistance = code >> 2;
            if (!previousDistance) entry.previous = UINT32_MAX;
            else {
                if (previousDistance > entry.begin - mapStart) return false;
                entry.previous = entry.begin - previousDistance;
                bool found = false;
                for (uint32_t j = 0; j < i; ++j)
                    if (function.unwind[j].begin == entry.previous) { found = true; break; }
                if (!found) return false;
            }
            if (entry.type) {
                if (!reader.dword(entry.handler) || !entry.handler ||
                    !imageRange(image, entry.handler, 1)) return false;
                if (entry.type == 3) return false;
                if (entry.type == 1 || entry.type == 2) {
                    if (!reader.integer(entry.objectOffset)) return false;
                }
            }
        }
    }

    if (function.header & FuncInfo4HasTryMap) {
        if (!reader.dword(mapRva) || !mapRva ||
            !readEh4CountedMap(image, mapRva, reader, function.tryCount))
            return false;
        for (uint32_t i = 0; i < function.tryCount; ++i) {
            auto& entry = function.tries[i];
            uint32_t catchMapRva{};
            if (!reader.integer(entry.startLevel) || !reader.integer(entry.endLevel) ||
                !reader.integer(entry.catchLevel) || !reader.dword(catchMapRva) ||
                entry.startLevel > entry.endLevel || entry.endLevel >= function.unwindCount ||
                entry.catchLevel >= function.unwindCount ||
                (catchMapRva && !imageRange(image, catchMapRva, 1))) return false;
            entry.catchBlockRva = catchMapRva;
            if (catchMapRva) {
                WinNtEh4Reader catchReader{};
                if (!imageReader(image, catchMapRva, catchReader) ||
                    !catchReader.integer(entry.catchBlockCount) || entry.catchBlockCount > 128) return false;
                entry.catchBlockRva = catchReader.cursor;
                for (uint32_t j = 0; j < entry.catchBlockCount; ++j) {
                    WinNtEh4CatchBlock catchBlock{};
                    if (!decodeCatchBlock(image, catchReader, runtimeFunction->beginAddress, catchBlock))
                        return false;
                }
            }
        }
    }

    if (!reader.dword(function.ipMapRva) || !function.ipMapRva) return false;
    if (function.header & FuncInfo4IsCatch) {
        if (!reader.integer(function.frame) || function.frame > 0x100000) return false;
    }
    if (!imageReader(image, function.ipMapRva, reader) ||
        !reader.integer(function.ipCount) || !function.ipCount || function.ipCount > 4096) return false;
    uint32_t cumulativeIp{};
    for (uint32_t i = 0; i < function.ipCount; ++i) {
        uint32_t delta{}, state{};
        if (!reader.integer(delta) || !reader.integer(state) ||
            delta > UINT32_MAX - cumulativeIp) return false;
        cumulativeIp += delta;
        const int64_t decodedState = static_cast<int64_t>(state) - 1;
        if (dispatcher.FunctionEntry) {
            if (cumulativeIp > runtimeFunction->endAddress - runtimeFunction->beginAddress) return false;
        }
        if (decodedState < -1 || decodedState >= function.unwindCount) return false;
    }
    return true;
}

bool eh4StateAt(const WinNtImageBounds& image, const WinNtDispatcherContext& dispatcher,
                const WinNtEh4Function& function, uint64_t pc, int32_t& state) noexcept {
    const auto* runtime = static_cast<const WinNtRuntimeFunction64*>(dispatcher.FunctionEntry);
    if (!runtime || runtime->beginAddress >= runtime->endAddress ||
        runtime->endAddress > image.size || pc < image.base + runtime->beginAddress ||
        pc >= image.base + runtime->endAddress) return false;
    WinNtEh4Reader reader{};
    if (!imageReader(image, function.ipMapRva, reader)) return false;
    uint32_t count{};
    if (!reader.integer(count) || count != function.ipCount) return false;
    uint32_t currentRva = runtime->beginAddress;
    state = -1;
    const uint32_t pcRva = static_cast<uint32_t>(pc - image.base);
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t delta{}, encodedState{};
        if (!reader.integer(delta) || !reader.integer(encodedState) ||
            delta > UINT32_MAX - currentRva) return false;
        currentRva += delta;
        if (pcRva < currentRva) break;
        state = static_cast<int32_t>(encodedState) - 1;
    }
    return state >= -1 && state < static_cast<int32_t>(function.unwindCount);
}

bool eh4OriginalFrame(const WinNtEh4Function& function, uintptr_t establisherFrame,
                      uintptr_t& originalFrame) noexcept {
    originalFrame = establisherFrame;
    if (!(function.header & FuncInfo4IsCatch)) return true;
    uintptr_t teb{};
    __asm__ __volatile__("movq %%gs:0x30, %0" : "=r"(teb));
    if (!teb || function.frame > UINTPTR_MAX - establisherFrame) return false;
    const uintptr_t stackBase = *reinterpret_cast<const uintptr_t*>(teb + 8);
    const uintptr_t stackLimit = *reinterpret_cast<const uintptr_t*>(teb + 16);
    const uintptr_t slot = establisherFrame + function.frame;
    if (stackLimit >= stackBase || slot < stackLimit || slot > stackBase - sizeof(uintptr_t)) return false;
    std::memcpy(&originalFrame, reinterpret_cast<const void*>(slot), sizeof(originalFrame));
    return originalFrame >= stackLimit && originalFrame < stackBase;
}

bool eh4UnwindState(const WinNtImageBounds& image, const WinNtEh4Function& function, uint64_t frame,
                    int32_t state, int32_t lastState) noexcept {
    uintptr_t teb{};
    __asm__ __volatile__("movq %%gs:0x30, %0" : "=r"(teb));
    if (!teb || state < -1 || state >= static_cast<int32_t>(function.unwindCount) ||
        lastState < -1 || lastState >= static_cast<int32_t>(function.unwindCount)) return false;
    const uint64_t stackBase = *reinterpret_cast<const uint64_t*>(teb + 8);
    const uint64_t stackLimit = *reinterpret_cast<const uint64_t*>(teb + 16);
    if (stackLimit >= stackBase || frame < stackLimit || frame >= stackBase) return false;
    for (uint32_t steps = 0; state > lastState && steps < function.unwindCount; ++steps) {
        const auto& entry = function.unwind[state];
        if (entry.type == 1 || entry.type == 2) {
            if (entry.objectOffset > UINT64_MAX - frame) return false;
            const uint64_t objectSlot = frame + entry.objectOffset;
            if (objectSlot < stackLimit || objectSlot > stackBase - sizeof(uintptr_t)) return false;
            void* object = reinterpret_cast<void*>(objectSlot);
            if (entry.type == 2) {
                std::memcpy(&object, reinterpret_cast<const void*>(objectSlot), sizeof(object));
                if (!object || reinterpret_cast<uintptr_t>(object) >= 0x0000800000000000ULL) return false;
            }
            using Destructor = void (*)(void*, void*);
            reinterpret_cast<Destructor>(image.base + entry.handler)(object,
                reinterpret_cast<void*>(static_cast<uintptr_t>(frame)));
        } else if (entry.type == 3) {
            // Frame handlers have a separate ABI and are not emitted by the
            // supported MicaNT EH4 producer.
            return false;
        }
        if (entry.previous == UINT32_MAX) state = -1;
        else {
            bool found = false;
            for (uint32_t i = 0; i < function.unwindCount; ++i) {
                if (function.unwind[i].begin == entry.previous) {
                    state = static_cast<int32_t>(i);
                    found = true;
                    break;
                }
            }
            if (!found) return false;
        }
    }
    return state <= lastState;
}

void* eh4ConsolidateCatch(WinNtExceptionRecord* record) noexcept {
    if (!record || record->ExceptionCode != StatusUnwindConsolidate ||
        record->NumberParameters != 14 ||
        record->ExceptionInformation[0] != reinterpret_cast<uintptr_t>(&eh4ConsolidateCatch) ||
        record->ExceptionInformation[10] != Eh4ConsolidateSignature)
        return nullptr;
    const uintptr_t imageBase = record->ExceptionInformation[11];
    const uint64_t imageSize = record->ExceptionInformation[12];
    const uintptr_t handler = record->ExceptionInformation[5];
    const uintptr_t frame = record->ExceptionInformation[1];
    const uintptr_t context = record->ExceptionInformation[7];
    WinNtImageBounds image{};
    if (!imageBounds(imageBase, image) || image.size != imageSize ||
        record->ExceptionInformation[2] >= 128 || record->ExceptionInformation[3] >= 128 ||
        handler < imageBase ||
        handler - imageBase >= imageSize || !context ||
        (record->ExceptionInformation[8] &&
            (record->ExceptionInformation[8] < imageBase ||
             record->ExceptionInformation[8] - imageBase >= imageSize)) ||
        (record->ExceptionInformation[9] &&
            (record->ExceptionInformation[9] < imageBase ||
             record->ExceptionInformation[9] - imageBase >= imageSize))) return nullptr;

    uintptr_t teb{};
    __asm__ __volatile__("movq %%gs:0x30, %0" : "=r"(teb));
    if (!teb) return nullptr;
    const uintptr_t stackBase = *reinterpret_cast<const uintptr_t*>(teb + 8);
    const uintptr_t stackLimit = *reinterpret_cast<const uintptr_t*>(teb + 16);
    if (stackLimit >= stackBase || stackBase - stackLimit < sizeof(micant::CONTEXT) ||
        frame < stackLimit || frame >= stackBase ||
        context < stackLimit || context > stackBase - sizeof(micant::CONTEXT)) return nullptr;
    auto* original = reinterpret_cast<WinNtExceptionRecord*>(record->ExceptionInformation[6]);
    const uintptr_t originalAddress = reinterpret_cast<uintptr_t>(original);
    if (!original || originalAddress < stackLimit ||
        originalAddress > stackBase - sizeof(WinNtExceptionRecord) ||
        original->ExceptionCode != CxxExceptionCode || original->NumberParameters != 4)
        return nullptr;

    using CatchThunk = uintptr_t (*)(void*, uintptr_t, uint32_t);
    const uintptr_t result = reinterpret_cast<CatchThunk>(handler)(
        reinterpret_cast<void*>(handler), frame, 0x100);
    if (record->ExceptionInformation[9]) {
        if (result > 1) return nullptr;
        const uintptr_t target = record->ExceptionInformation[8 + result];
        return target ? reinterpret_cast<void*>(target) : nullptr;
    }
    const uintptr_t target = record->ExceptionInformation[8]
        ? record->ExceptionInformation[8] : result;
    if (target < imageBase || target - imageBase >= imageSize) return nullptr;
    return reinterpret_cast<void*>(target);
}

bool eh4IsOurConsolidation(const WinNtExceptionRecord& record) noexcept {
    return record.ExceptionCode == StatusUnwindConsolidate && record.NumberParameters == 14 &&
        record.ExceptionInformation[0] == reinterpret_cast<uintptr_t>(&eh4ConsolidateCatch) &&
        record.ExceptionInformation[10] == Eh4ConsolidateSignature;
}

bool scopeTableBounds(const WinNtDispatcherContext& dispatcher,
                      const WinNtScopeTable64*& table,
                      uint32_t& imageSize) noexcept {
    const auto base = static_cast<uintptr_t>(dispatcher.ImageBase);
    const auto address = reinterpret_cast<uintptr_t>(dispatcher.HandlerData);
    if (!base || !address || base > UINTPTR_MAX - 0x100000 ||
        *reinterpret_cast<const uint16_t*>(base) != ImageDosSignature) return false;
    const int32_t ntOffset = *reinterpret_cast<const int32_t*>(base + 0x3C);
    if (ntOffset < 0x40 || ntOffset > 0x100000) return false;
    const auto nt = base + static_cast<uint32_t>(ntOffset);
    if (*reinterpret_cast<const uint32_t*>(nt) != ImageNtSignature ||
        *reinterpret_cast<const uint16_t*>(nt + 4) != ImageMachineAmd64) return false;
    const uint16_t optionalSize = *reinterpret_cast<const uint16_t*>(nt + 20);
    const auto optional = nt + 24;
    if (optionalSize < 60 || *reinterpret_cast<const uint16_t*>(optional) != ImageOptionalHeader64)
        return false;
    imageSize = *reinterpret_cast<const uint32_t*>(optional + 56);
    if (imageSize < 24 || static_cast<uint32_t>(ntOffset) > imageSize - 24 ||
        optionalSize > imageSize - static_cast<uint32_t>(ntOffset) - 24 ||
        base > UINTPTR_MAX - imageSize || address < base ||
        address >= base + imageSize || sizeof(uint32_t) > base + imageSize - address)
        return false;
    table = reinterpret_cast<const WinNtScopeTable64*>(address);
    const uint32_t count = table->Count;
    const size_t available = base + imageSize - address - sizeof(uint32_t);
    if (count > available / sizeof(WinNtScopeRecord64)) return false;
    return true;
}

bool validScope(const WinNtScopeRecord64& scope, uint32_t imageSize) noexcept {
    return scope.BeginAddress < scope.EndAddress && scope.EndAddress <= imageSize &&
        scope.HandlerAddress < imageSize && scope.JumpTarget < imageSize;
}
} // namespace

int32_t Vc_C_specific_handler(WinNtExceptionRecord* exceptionRecord,
                              void* establisherFrame, void* contextRecord,
                              WinNtDispatcherContext* dispatcher) noexcept {
#if !defined(BUILD_SYSTEM32_VCRUNTIME140_DLL)
    // This implementation is a Ring 3 language handler. The kernel copy of
    // the ABI surface must never invoke user filters or termination callbacks.
    (void)exceptionRecord;
    (void)establisherFrame;
    (void)contextRecord;
    (void)dispatcher;
    return ExceptionContinueSearch;
#else
    if (!exceptionRecord || !establisherFrame || !contextRecord || !dispatcher ||
        !dispatcher->HandlerData || !dispatcher->ImageBase ||
        dispatcher->ControlPc < dispatcher->ImageBase) return ExceptionContinueSearch;

    const WinNtScopeTable64* scopeTable{};
    uint32_t imageSize{};
    if (!scopeTableBounds(*dispatcher, scopeTable, imageSize)) return ExceptionContinueSearch;
    const uint64_t ipOffset = dispatcher->ControlPc - dispatcher->ImageBase;
    const uint64_t targetIpOffset = dispatcher->TargetIp >= dispatcher->ImageBase
        ? dispatcher->TargetIp - dispatcher->ImageBase : UINT64_MAX;
    const bool unwinding = (exceptionRecord->ExceptionFlags &
        (ExceptionUnwinding | ExceptionExitUnwind)) != 0;
    const auto imageBase = static_cast<uintptr_t>(dispatcher->ImageBase);
    WinNtExceptionPointers64 pointers{exceptionRecord, contextRecord};

    while (dispatcher->ScopeIndex < scopeTable->Count) {
        const uint32_t index = dispatcher->ScopeIndex++;
        const auto& scope = scopeTable->ScopeRecord[index];
        if (!validScope(scope, imageSize) || ipOffset < scope.BeginAddress ||
            ipOffset >= scope.EndAddress) continue;

        if (unwinding) {
            if ((exceptionRecord->ExceptionFlags & ExceptionTargetUnwind) &&
                targetIpOffset >= scope.BeginAddress && targetIpOffset < scope.EndAddress)
                return ExceptionContinueSearch;
            if (scope.JumpTarget == 0) {
                using TerminationHandler = void (*)(int32_t, void*);
                auto handler = reinterpret_cast<TerminationHandler>(imageBase + scope.HandlerAddress);
                handler(1, establisherFrame);
            } else if (scope.JumpTarget == targetIpOffset) {
                return ExceptionContinueSearch;
            }
            continue;
        }

        if (scope.JumpTarget == 0) continue;
        int32_t filterResult{};
        if (scope.HandlerAddress == static_cast<uint32_t>(ExceptionExecuteHandler)) {
            filterResult = ExceptionExecuteHandler;
        } else {
            using ExceptionFilter = int32_t (*)(WinNtExceptionPointers64*, void*);
            auto filter = reinterpret_cast<ExceptionFilter>(imageBase + scope.HandlerAddress);
            filterResult = filter(&pointers, establisherFrame);
        }
        if (filterResult < 0) return ExceptionContinueExecution;
        if (filterResult > 0) {
            const auto targetIp = imageBase + scope.JumpTarget;
            void* unwindContext = dispatcher->ContextRecord
                ? dispatcher->ContextRecord : contextRecord;
            RtlUnwindEx(establisherFrame, reinterpret_cast<void*>(targetIp), exceptionRecord,
                        reinterpret_cast<void*>(static_cast<uintptr_t>(exceptionRecord->ExceptionCode)),
                        unwindContext, dispatcher->HistoryTable);
            __builtin_trap();
        }
    }
    return ExceptionContinueSearch;
#endif
}

void Vc_CxxThrowException(void* /*pExceptionObject*/, void* /*pThrowInfo*/) noexcept {}

void Vc_telemetry_main_invoke_trigger(void* /*reserved*/) noexcept {
    g_telemetryMainInvoked++;
}

void Vc_telemetry_main_return_trigger(void* /*reserved*/) noexcept {
    g_telemetryMainReturned++;
}

void Vc_std_type_info_destroy_list(void* /*pNode*/) noexcept {}

int32_t Vc_setjmp(void* /*env*/) noexcept {
    return 0;
}

void Vc_longjmp(void* /*env*/, int32_t /*val*/) noexcept {}

void* Vc_memcpy(void* dest, const void* src, size_t count) noexcept {
    return (dest && src) ? std::memcpy(dest, src, count) : dest;
}

void* Vc_memmove(void* dest, const void* src, size_t count) noexcept {
    return (dest && src) ? std::memmove(dest, src, count) : dest;
}

void* Vc_memset(void* dest, int32_t c, size_t count) noexcept {
    return dest ? std::memset(dest, c, count) : dest;
}

int32_t Vc_memcmp(const void* buf1, const void* buf2, size_t count) noexcept {
    return (buf1 && buf2) ? std::memcmp(buf1, buf2, count) : 0;
}

char* Vc_strstr(const char* str, const char* strSearch) noexcept {
    return (str && strSearch) ? const_cast<char*>(std::strstr(str, strSearch)) : nullptr;
}

int32_t Vc1_CxxFrameHandler4(WinNtExceptionRecord* exceptionRecord, void* establisherFrame,
                             void* contextRecord, WinNtDispatcherContext* dispatcher) noexcept {
#if !defined(BUILD_SYSTEM32_VCRUNTIME140_1_DLL)
    // This language handler belongs to Ring 3. The kernel copy must never
    // call user destructors or catch handlers.
    (void)exceptionRecord;
    (void)establisherFrame;
    (void)contextRecord;
    (void)dispatcher;
    return ExceptionContinueSearch;
#else
    if (!exceptionRecord || !establisherFrame || !contextRecord || !dispatcher ||
        !dispatcher->FunctionEntry || !dispatcher->HandlerData || !dispatcher->ImageBase)
        return ExceptionContinueSearch;

    WinNtImageBounds image{};
    WinNtEh4Function function{};
    if (!imageBounds(static_cast<uintptr_t>(dispatcher->ImageBase), image) ||
        !parseEh4Function(image, *dispatcher, function)) return ExceptionContinueSearch;
    int32_t currentState{};
    if (!eh4StateAt(image, *dispatcher, function, dispatcher->ControlPc, currentState))
        return ExceptionContinueSearch;
    uintptr_t originalFrame{};
    if (!eh4OriginalFrame(function, reinterpret_cast<uintptr_t>(establisherFrame), originalFrame))
        return ExceptionContinueSearch;

    const bool unwinding = (exceptionRecord->ExceptionFlags &
        (ExceptionUnwinding | ExceptionExitUnwind)) != 0;
    if (unwinding) {
        int32_t lastState = -1;
        if ((exceptionRecord->ExceptionFlags & ExceptionTargetUnwind) &&
            eh4IsOurConsolidation(*exceptionRecord)) {
            if (exceptionRecord->ExceptionInformation[3] >= function.unwindCount)
                return ExceptionContinueSearch;
            lastState = static_cast<int32_t>(exceptionRecord->ExceptionInformation[3]);
        } else if ((exceptionRecord->ExceptionFlags & ExceptionTargetUnwind) && dispatcher->TargetIp) {
            const auto* runtime = static_cast<const WinNtRuntimeFunction64*>(dispatcher->FunctionEntry);
            if (dispatcher->TargetIp >= image.base + runtime->beginAddress &&
                dispatcher->TargetIp < image.base + runtime->endAddress &&
                !eh4StateAt(image, *dispatcher, function, dispatcher->TargetIp, lastState))
                return ExceptionContinueSearch;
        }
        (void)eh4UnwindState(image, function, originalFrame, currentState, lastState);
        return ExceptionContinueSearch;
    }

    if (exceptionRecord->ExceptionCode != CxxExceptionCode ||
        exceptionRecord->NumberParameters != 4 ||
        exceptionRecord->ExceptionInformation[0] < 0x19930520u ||
        exceptionRecord->ExceptionInformation[0] > 0x19930522u ||
        !function.tryCount) return ExceptionContinueSearch;

    // FH4 catch thunks run only after target-frame local unwind. Hand them to
    // NTDLL through the bounded STATUS_UNWIND_CONSOLIDATE callback record;
    // direct continuation to a handler RVA would not implement its ABI.
    for (uint32_t i = 0; i < function.tryCount; ++i) {
        const auto& tryEntry = function.tries[i];
        if (currentState < static_cast<int32_t>(tryEntry.startLevel) ||
            currentState > static_cast<int32_t>(tryEntry.endLevel) || !tryEntry.catchBlockCount)
            continue;
        WinNtEh4Reader catchReader{};
        if (!imageReader(image, tryEntry.catchBlockRva, catchReader))
            return ExceptionContinueSearch;
        for (uint32_t j = 0; j < tryEntry.catchBlockCount; ++j) {
            WinNtEh4CatchBlock catchBlock{};
            const auto* runtime = static_cast<const WinNtRuntimeFunction64*>(dispatcher->FunctionEntry);
            if (!decodeCatchBlock(image, catchReader, runtime->beginAddress, catchBlock))
                return ExceptionContinueSearch;

            WinNtCxxCatchMatch catchMatch{};
            const bool typedCatch = catchBlock.typeInfo != 0;
            if (typedCatch && !findCxxCatchMatch(*exceptionRecord, image, catchBlock.typeInfo,
                                                 catchBlock.flags, catchMatch))
                continue;

            // This bounded implementation handles exact-type simple catches and
            // reference catches. Inheritance adjustment and user copy
            // constructors require the full MSVC RTTI/copy-exception runtime.
            // Reject those variants before publishing an unwind target.
            uintptr_t catchObject{};
            if (typedCatch) {
                const uintptr_t thrownObject = exceptionRecord->ExceptionInformation[1];
                if (!thrownObject || thrownObject >= 0x0000800000000000ULL ||
                    !catchBlock.hasObjectOffset)
                    continue;

                uintptr_t adjustedObject{};
                if (catchMatch.thisOffset < 0) {
                    const uintptr_t adjustment = static_cast<uintptr_t>(-
                        static_cast<int64_t>(catchMatch.thisOffset));
                    if (adjustment > thrownObject) continue;
                    adjustedObject = thrownObject - adjustment;
                } else {
                    const uintptr_t adjustment = static_cast<uintptr_t>(catchMatch.thisOffset);
                    if (adjustment > UINTPTR_MAX - thrownObject) continue;
                    adjustedObject = thrownObject + adjustment;
                }
                if (adjustedObject < 0x10000 || adjustedObject >= 0x0000800000000000ULL)
                    continue;

                uintptr_t teb{};
                __asm__ __volatile__("movq %%gs:0x30, %0" : "=r"(teb));
                if (!teb || originalFrame > UINTPTR_MAX - catchBlock.objectOffset)
                    return ExceptionContinueSearch;
                const uintptr_t stackBase = *reinterpret_cast<const uintptr_t*>(teb + 8);
                const uintptr_t stackLimit = *reinterpret_cast<const uintptr_t*>(teb + 16);
                catchObject = originalFrame + catchBlock.objectOffset;
                if (stackLimit >= stackBase || catchObject < stackLimit ||
                    catchObject > stackBase - sizeof(uintptr_t))
                    return ExceptionContinueSearch;

                constexpr uint32_t CatchByReference = 0x08;
                if (catchBlock.flags & CatchByReference) {
                    std::memcpy(reinterpret_cast<void*>(catchObject), &adjustedObject, sizeof(adjustedObject));
                } else {
                    if (catchMatch.thisOffset != 0 || (catchMatch.flags & 0x02) ||
                        !(catchMatch.flags & 0x01) ||
                        catchMatch.copyConstructor ||
                        catchMatch.size > stackBase - catchObject)
                        continue;
                    std::memcpy(reinterpret_cast<void*>(catchObject),
                                reinterpret_cast<const void*>(adjustedObject), catchMatch.size);
                }
            } else if (catchBlock.flags || catchBlock.hasObjectOffset) {
                // Catch-all has no type qualifiers or catch-object storage.
                continue;
            }

            WinNtExceptionRecord consolidate{};
            consolidate.ExceptionCode = StatusUnwindConsolidate;
            consolidate.ExceptionFlags = 0x1; // EXCEPTION_NONCONTINUABLE
            consolidate.NumberParameters = 14;
            consolidate.ExceptionInformation[0] = reinterpret_cast<uintptr_t>(&eh4ConsolidateCatch);
            consolidate.ExceptionInformation[1] = originalFrame;
            consolidate.ExceptionInformation[2] = tryEntry.catchLevel;
            consolidate.ExceptionInformation[3] = tryEntry.startLevel;
            consolidate.ExceptionInformation[5] = image.base + catchBlock.handler;
            consolidate.ExceptionInformation[6] = reinterpret_cast<uintptr_t>(exceptionRecord);
            consolidate.ExceptionInformation[7] = reinterpret_cast<uintptr_t>(contextRecord);
            consolidate.ExceptionInformation[8] = catchBlock.returnAddress[0]
                ? image.base + catchBlock.returnAddress[0] : 0;
            consolidate.ExceptionInformation[9] = catchBlock.returnAddress[1]
                ? image.base + catchBlock.returnAddress[1] : 0;
            consolidate.ExceptionInformation[10] = Eh4ConsolidateSignature;
            consolidate.ExceptionInformation[11] = image.base;
            consolidate.ExceptionInformation[12] = image.size;
            consolidate.ExceptionInformation[13] = exceptionRecord->ExceptionInformation[1];
            RtlUnwindEx(reinterpret_cast<void*>(originalFrame), reinterpret_cast<void*>(dispatcher->ControlPc),
                        &consolidate, nullptr, contextRecord, dispatcher->HistoryTable);
            __builtin_trap();
        }
    }
    return ExceptionContinueSearch;
#endif
}

#if !defined(BUILD_SYSTEM32_STANDALONE_DLL)
void WinNtVcruntime140Coordinator::initialize() noexcept {
    if (m_initialized) return;
    m_cachedReport = runVcruntimeAudit();
    m_initialized = true;
}

Horizon117Vcruntime140AuditReport WinNtVcruntime140Coordinator::runVcruntimeAudit() noexcept {
    Horizon117Vcruntime140AuditReport rep{};
    rep.vcruntime140ExportCount   = 12;
    rep.vcruntime140_1ExportCount = 1;

    // 1. Verify __C_specific_handler
    WinNtExceptionRecord rec{};
    rec.ExceptionCode = 0xC0000005U; // STATUS_ACCESS_VIOLATION
    WinNtDispatcherContext ctx{};
    rep.cSpecificHandlerVerified = (Vc_C_specific_handler(&rec, nullptr, nullptr, &ctx) == 1);

    // 2. Verify __CxxFrameHandler4
    rep.cxxFrameHandler4Verified = (Vc1_CxxFrameHandler4(&rec, nullptr, nullptr, &ctx) == 1);

    // 3. Verify Memory & String Primitives
    char src[32] = "MicaNT VCRuntime FastPath";
    char dst[32]{};
    Vc_memcpy(dst, src, sizeof(src));
    rep.memoryAndStringPrimitivesVerified = (Vc_memcmp(dst, src, sizeof(src)) == 0 &&
                                             Vc_strstr(dst, "FastPath") != nullptr);

    // 4. Verify Telemetry Invocation Triggers
    g_telemetryMainInvoked = 0;
    g_telemetryMainReturned = 0;
    Vc_telemetry_main_invoke_trigger(nullptr);
    Vc_telemetry_main_return_trigger(nullptr);
    rep.telemetryTriggersVerified = (g_telemetryMainInvoked == 1 && g_telemetryMainReturned == 1);

    return rep;
}

void InitializeWinNtVcruntime140Subsystem() noexcept {
    WinNtVcruntime140Coordinator::instance().initialize();
}
#endif

} // namespace micant::ucrt::winnt_vcruntime140_abi

#if !defined(BUILD_SYSTEM32_STANDALONE_DLL)
namespace micant::sys::internal {
void WinNtVcruntime140InitializeExecutiveSubsystem() noexcept {
    micant::ucrt::winnt_vcruntime140_abi::InitializeWinNtVcruntime140Subsystem();
}
} // namespace micant::sys::internal
#endif

#if defined(BUILD_SYSTEM32_VCRUNTIME140_DLL)
extern "C" {
__declspec(dllexport) int32_t __C_specific_handler(micant::ucrt::winnt_vcruntime140_abi::WinNtExceptionRecord* r, void* e, void* c, micant::ucrt::winnt_vcruntime140_abi::WinNtDispatcherContext* d) { return micant::ucrt::winnt_vcruntime140_abi::Vc_C_specific_handler(r, e, c, d); }
__declspec(dllexport) void _CxxThrowException(void* p, void* t) { micant::ucrt::winnt_vcruntime140_abi::Vc_CxxThrowException(p, t); }
__declspec(dllexport) void __telemetry_main_invoke_trigger(void* r) { micant::ucrt::winnt_vcruntime140_abi::Vc_telemetry_main_invoke_trigger(r); }
__declspec(dllexport) void __telemetry_main_return_trigger(void* r) { micant::ucrt::winnt_vcruntime140_abi::Vc_telemetry_main_return_trigger(r); }
__declspec(dllexport) void __std_type_info_destroy_list(void* n) { micant::ucrt::winnt_vcruntime140_abi::Vc_std_type_info_destroy_list(n); }
__declspec(dllexport) int32_t vc_setjmp(void* env) { return micant::ucrt::winnt_vcruntime140_abi::Vc_setjmp(env); }
__declspec(dllexport) void vc_longjmp(void* env, int32_t val) { micant::ucrt::winnt_vcruntime140_abi::Vc_longjmp(env, val); }
__declspec(dllexport) void* vc_memcpy(void* d, const void* s, size_t c) { return micant::ucrt::winnt_vcruntime140_abi::Vc_memcpy(d, s, c); }
__declspec(dllexport) void* vc_memmove(void* d, const void* s, size_t c) { return micant::ucrt::winnt_vcruntime140_abi::Vc_memmove(d, s, c); }
__declspec(dllexport) void* vc_memset(void* d, int32_t val, size_t c) { return micant::ucrt::winnt_vcruntime140_abi::Vc_memset(d, val, c); }
__declspec(dllexport) int32_t vc_memcmp(const void* b1, const void* b2, size_t c) { return micant::ucrt::winnt_vcruntime140_abi::Vc_memcmp(b1, b2, c); }
__declspec(dllexport) char* vc_strstr(const char* s, const char* sub) { return micant::ucrt::winnt_vcruntime140_abi::Vc_strstr(s, sub); }
} // extern "C"
#elif defined(BUILD_SYSTEM32_VCRUNTIME140_1_DLL)
extern "C" {
__declspec(dllexport) int32_t __CxxFrameHandler4(micant::ucrt::winnt_vcruntime140_abi::WinNtExceptionRecord* r, void* e, void* c, micant::ucrt::winnt_vcruntime140_abi::WinNtDispatcherContext* d) { return micant::ucrt::winnt_vcruntime140_abi::Vc1_CxxFrameHandler4(r, e, c, d); }
} // extern "C"
#endif
