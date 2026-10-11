#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <deque>
#include <mutex>
#include <atomic>
#include <memory>
#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <span>
#include "ntdef.hpp"
#include "ntstatus.hpp"
#include "ke.hpp"
#include "trap.hpp"

namespace micant::kd {

// ============================================================================
// 1. KD64 Packet Protocol Constants
// ============================================================================
inline constexpr uint32_t PACKET_LEADER            = 0x30303030; // "0000" Data Packet Leader
inline constexpr uint32_t CONTROL_PACKET_LEADER    = 0x69696969; // "iiii" Control Packet Leader
inline constexpr uint32_t BREAKIN_PACKET_LEADER    = 0x62626262; // "bbbb" Breakin Request Leader

inline constexpr uint16_t PACKET_TYPE_KD_STATE_CHANGE32   = 0x0001;
inline constexpr uint16_t PACKET_TYPE_KD_STATE_MANIPULATE = 0x0002;
inline constexpr uint16_t PACKET_TYPE_KD_DEBUG_IO         = 0x0003;
inline constexpr uint16_t PACKET_TYPE_KD_ACKNOWLEDGE      = 0x0004;
inline constexpr uint16_t PACKET_TYPE_KD_RESEND           = 0x0005;
inline constexpr uint16_t PACKET_TYPE_KD_RESET            = 0x0006;
inline constexpr uint16_t PACKET_TYPE_KD_STATE_CHANGE64   = 0x0007;
inline constexpr uint16_t PACKET_TYPE_KD_POLL_BREAKIN     = 0x0008;
inline constexpr uint16_t PACKET_TYPE_KD_TRACE_IO         = 0x0009;
inline constexpr uint16_t PACKET_TYPE_KD_CONTROL_REQUEST  = 0x000A;
inline constexpr uint16_t PACKET_TYPE_KD_FILE_IO          = 0x000B;

inline constexpr uint32_t INITIAL_PACKET_ID        = 0x80800000;
inline constexpr uint32_t SYNC_PACKET_ID           = 0x00000800;

// KD64 State Manipulate API Numbers
inline constexpr uint32_t DbgKdReadVirtualMemoryApi64     = 0x00003130;
inline constexpr uint32_t DbgKdWriteVirtualMemoryApi64    = 0x00003131;
inline constexpr uint32_t DbgKdGetRegisterApi             = 0x00003132;
inline constexpr uint32_t DbgKdSetRegisterApi             = 0x00003133;
inline constexpr uint32_t DbgKdGetVersionApi              = 0x00003134;
inline constexpr uint32_t DbgKdContinueApi                = 0x00003136;
inline constexpr uint32_t DbgKdRestoreBreakPointApi       = 0x00003137;
inline constexpr uint32_t DbgKdWriteBreakPointApi         = 0x00003138;
inline constexpr uint32_t DbgKdReadControlSpaceApi        = 0x00003139;
inline constexpr uint32_t DbgKdWriteControlSpaceApi       = 0x0000313A;
inline constexpr uint32_t DbgKdReadPhysicalMemoryApi64    = 0x0000313D;
inline constexpr uint32_t DbgKdWritePhysicalMemoryApi64   = 0x0000313E;
inline constexpr uint32_t DbgKdGetContextApi              = 0x00003140;
inline constexpr uint32_t DbgKdSetContextApi              = 0x00003141;
inline constexpr uint32_t DbgKdCauseBugCheckApi           = 0x00003144;

// ============================================================================
// 2. KD64 Packet Framing Structures
// ============================================================================
#pragma pack(push, 1)

struct KD_PACKET {
    uint32_t packetLeader{PACKET_LEADER};
    uint16_t packetType{0};
    uint16_t byteCount{0};
    uint32_t packetId{INITIAL_PACKET_ID};
    uint32_t checksum{0};
};

struct DBGKD_GET_VERSION64 {
    uint16_t majorVersion{0x000F}; // 0xF = Free build
    uint16_t minorVersion{26100};  // Windows NT 24H2 build equivalent
    uint8_t protocolVersion{6};    // KD64 Version 6
    uint8_t kdSecondaryVersion{0};
    uint16_t flags{0x0002};        // DBGKD_VERS_FLAG_DATA
    uint16_t machineType{0x8664};  // IMAGE_FILE_MACHINE_AMD64
    uint8_t maxPacketType{0x0C};
    uint8_t maxStateChange{0x03};
    uint8_t maxManipulate{0x20};
    uint8_t simulation{0};
    uint16_t pad{0};
    uint64_t kernBase{0xFFFFF80000000000ULL};
    uint64_t psLoadedModuleList{0xFFFFF80000100000ULL};
    uint64_t debuggerDataList{0xFFFFF80000200000ULL};
};

struct DBGKD_READ_MEMORY64 {
    uint64_t targetBaseAddress{0};
    uint32_t transferCount{0};
    uint32_t actualBytesRead{0};
};

struct DBGKD_WRITE_MEMORY64 {
    uint64_t targetBaseAddress{0};
    uint32_t transferCount{0};
    uint32_t actualBytesWritten{0};
};

struct DBGKD_BREAKPOINT64 {
    uint64_t breakPointAddress{0};
    uint32_t breakPointHandle{0};
};

struct DBGKD_CONTINUE64 {
    uint32_t continueStatus{0x00010002}; // DBG_CONTINUE
};

struct DBGKD_MANIPULATE_STATE64 {
    uint32_t apiNumber{0};
    uint16_t processorLevel{0};
    uint16_t processor{0};
    uint32_t returnStatus{0};
    union {
        DBGKD_READ_MEMORY64 readMemory;
        DBGKD_WRITE_MEMORY64 writeMemory;
        DBGKD_GET_VERSION64 getVersion;
        DBGKD_BREAKPOINT64 breakPoint;
        DBGKD_CONTINUE64 continue64;
    };
};

struct DBGKD_DEBUG_IO {
    uint32_t apiNumber{0x00003230}; // DbgKdPrintStringApi
    uint16_t processorLevel{0};
    uint16_t processor{0};
    uint16_t stringLength{0};
    uint16_t pad{0};
};

#pragma pack(pop)

// KD Checksum: arithmetic sum of payload bytes
inline uint32_t KdComputeChecksum(const uint8_t* data, size_t length) noexcept {
    uint32_t sum = 0;
    for (size_t i = 0; i < length; ++i) {
        sum += data[i];
    }
    return sum;
}

// ============================================================================
// 3. Hardware Breakpoint Trap Engine (DR0-DR7)
// ============================================================================
enum class HwBreakpointType : uint8_t {
    Execute   = 0, // 00b = Execution only
    Write     = 1, // 01b = Data Writes only
    IoPort    = 2, // 10b = I/O Ports
    ReadWrite = 3  // 11b = Data Reads and Writes (not instruction fetch)
};

enum class HwBreakpointLength : uint8_t {
    Byte1 = 0, // 00b = 1 byte
    Byte2 = 1, // 01b = 2 bytes
    Byte8 = 2, // 10b = 8 bytes
    Byte4 = 3  // 11b = 4 bytes
};

struct DebugRegisters {
    uint64_t dr0{0};
    uint64_t dr1{0};
    uint64_t dr2{0};
    uint64_t dr3{0};
    uint64_t dr6{0xFFFF0FF0ULL}; // DR6 reset state (bits 4-11 and 16-31 are 1s)
    uint64_t dr7{0x00000400ULL}; // DR7 reset state (bit 10 must always be 1)
};

class HardwareBreakpointEngine {
public:
    HardwareBreakpointEngine() = default;

    /**
     * @brief Configures a hardware breakpoint in one of the 4 debug slots (DR0-DR3).
     */
    bool setBreakpoint(
        uint8_t slot,
        uintptr_t address,
        HwBreakpointType type,
        HwBreakpointLength length
    ) {
        if (slot > 3) return false;
        std::lock_guard<std::mutex> guard(mutex_);

        // Assign linear address to DR0-DR3
        switch (slot) {
            case 0: regs_.dr0 = address; break;
            case 1: regs_.dr1 = address; break;
            case 2: regs_.dr2 = address; break;
            case 3: regs_.dr3 = address; break;
        }

        // Enable Local breakpoint bit in DR7 (L0-L3: bits 0, 2, 4, 6)
        regs_.dr7 |= (1ULL << (slot * 2));

        // Set R/W condition (bits 16-17, 20-21, 24-25, 28-29)
        uint64_t rwShift = 16 + (slot * 4);
        regs_.dr7 &= ~(3ULL << rwShift);
        regs_.dr7 |= (static_cast<uint64_t>(type) & 3ULL) << rwShift;

        // Set LEN condition (bits 18-19, 22-23, 26-27, 30-31)
        uint64_t lenShift = 18 + (slot * 4);
        regs_.dr7 &= ~(3ULL << lenShift);
        regs_.dr7 |= (static_cast<uint64_t>(length) & 3ULL) << lenShift;

        slotActive_[slot] = true;
        slotType_[slot] = type;
        slotLen_[slot] = length;
        return true;
    }

    /**
     * @brief Clears an active hardware breakpoint.
     */
    bool clearBreakpoint(uint8_t slot) {
        if (slot > 3) return false;
        std::lock_guard<std::mutex> guard(mutex_);

        switch (slot) {
            case 0: regs_.dr0 = 0; break;
            case 1: regs_.dr1 = 0; break;
            case 2: regs_.dr2 = 0; break;
            case 3: regs_.dr3 = 0; break;
        }

        // Clear Local and Global enable bits for slot
        regs_.dr7 &= ~(3ULL << (slot * 2));
        slotActive_[slot] = false;
        return true;
    }

    /**
     * @brief Evaluates whether a memory access or instruction fetch matches an active DR breakpoint.
     */
    bool evaluateAccess(uintptr_t address, size_t size, bool isWrite, bool isExecute, uint64_t& outDr6) {
        std::lock_guard<std::mutex> guard(mutex_);
        bool hit = false;
        uint64_t status = 0xFFFF0FF0ULL;

        for (uint8_t i = 0; i < 4; ++i) {
            if (!slotActive_[i]) continue;
            uintptr_t bpAddr = 0;
            switch (i) {
                case 0: bpAddr = regs_.dr0; break;
                case 1: bpAddr = regs_.dr1; break;
                case 2: bpAddr = regs_.dr2; break;
                case 3: bpAddr = regs_.dr3; break;
            }

            size_t bpSize = 1;
            switch (slotLen_[i]) {
                case HwBreakpointLength::Byte1: bpSize = 1; break;
                case HwBreakpointLength::Byte2: bpSize = 2; break;
                case HwBreakpointLength::Byte4: bpSize = 4; break;
                case HwBreakpointLength::Byte8: bpSize = 8; break;
            }

            // Check address range overlap: [address, address + size) intersects [bpAddr, bpAddr + bpSize)
            bool overlaps = (address < bpAddr + bpSize) && (address + size > bpAddr);
            if (!overlaps) continue;

            // Check condition match
            bool condMatch = false;
            if (isExecute && slotType_[i] == HwBreakpointType::Execute) {
                condMatch = true;
            } else if (isWrite && (slotType_[i] == HwBreakpointType::Write || slotType_[i] == HwBreakpointType::ReadWrite)) {
                condMatch = true;
            } else if (!isWrite && !isExecute && slotType_[i] == HwBreakpointType::ReadWrite) {
                condMatch = true;
            }

            if (condMatch) {
                status |= (1ULL << i); // Set B0-B3 in DR6
                hit = true;
            }
        }

        if (hit) {
            regs_.dr6 = status;
            outDr6 = status;
        }
        return hit;
    }

    [[nodiscard]] const DebugRegisters& getRegisters() const noexcept { return regs_; }

private:
    mutable std::mutex mutex_;
    DebugRegisters regs_{};
    std::array<bool, 4> slotActive_{false, false, false, false};
    std::array<HwBreakpointType, 4> slotType_{HwBreakpointType::Execute, HwBreakpointType::Execute, HwBreakpointType::Execute, HwBreakpointType::Execute};
    std::array<HwBreakpointLength, 4> slotLen_{HwBreakpointLength::Byte1, HwBreakpointLength::Byte1, HwBreakpointLength::Byte1, HwBreakpointLength::Byte1};
};

// ============================================================================
// 4. KD Ring Buffer & DbgPrint Engine
// ============================================================================
class KdPrintRingBuffer {
public:
    static constexpr size_t MAX_CAPACITY = 256;

    void append(std::string_view msg) {
        std::lock_guard<std::mutex> guard(mutex_);
        if (entries_.size() >= MAX_CAPACITY) {
            entries_.pop_front();
        }
        entries_.emplace_back(msg);
    }

    [[nodiscard]] std::vector<std::string> getLogs() const {
        std::lock_guard<std::mutex> guard(mutex_);
        return std::vector<std::string>(entries_.begin(), entries_.end());
    }

    void clear() {
        std::lock_guard<std::mutex> guard(mutex_);
        entries_.clear();
    }

    [[nodiscard]] size_t size() const {
        std::lock_guard<std::mutex> guard(mutex_);
        return entries_.size();
    }

private:
    mutable std::mutex mutex_;
    std::deque<std::string> entries_;
};

// ============================================================================
// 5. KD Transport Channel & Protocol Engine
// ============================================================================
class KdTransport {
public:
    KdTransport() = default;

    /**
     * @brief Transmits a KD packet over the transport framing layer.
     */
    bool sendPacket(uint16_t packetType, const void* payload, uint16_t byteCount) {
        std::lock_guard<std::mutex> guard(mutex_);
        KD_PACKET pkt{};
        pkt.packetLeader = PACKET_LEADER;
        pkt.packetType = packetType;
        pkt.byteCount = byteCount;
        pkt.packetId = currentPacketId_++;
        pkt.checksum = KdComputeChecksum(reinterpret_cast<const uint8_t*>(payload), byteCount);

        std::vector<uint8_t> raw(sizeof(KD_PACKET) + byteCount);
        std::memcpy(raw.data(), &pkt, sizeof(KD_PACKET));
        if (byteCount > 0 && payload) {
            std::memcpy(raw.data() + sizeof(KD_PACKET), payload, byteCount);
        }

        outboundPackets_.push_back(std::move(raw));
        return true;
    }

    /**
     * @brief Transmits a KD acknowledge control packet.
     */
    bool sendAcknowledge(uint32_t packetId) {
        std::lock_guard<std::mutex> guard(mutex_);
        KD_PACKET pkt{};
        pkt.packetLeader = CONTROL_PACKET_LEADER;
        pkt.packetType = PACKET_TYPE_KD_ACKNOWLEDGE;
        pkt.byteCount = 0;
        pkt.packetId = packetId;
        pkt.checksum = 0;

        std::vector<uint8_t> raw(sizeof(KD_PACKET));
        std::memcpy(raw.data(), &pkt, sizeof(KD_PACKET));
        outboundPackets_.push_back(std::move(raw));
        return true;
    }

    /**
     * @brief Delivers a raw packet into the inbound queue (e.g. from host debugger).
     */
    void deliverInbound(const std::vector<uint8_t>& packetBytes) {
        std::lock_guard<std::mutex> guard(mutex_);
        inboundPackets_.push_back(packetBytes);
    }

    /**
     * @brief Reads and validates next inbound packet.
     */
    bool receivePacket(KD_PACKET& outHeader, std::vector<uint8_t>& outPayload) {
        std::lock_guard<std::mutex> guard(mutex_);
        if (inboundPackets_.empty()) return false;

        auto raw = std::move(inboundPackets_.front());
        inboundPackets_.pop_front();

        if (raw.size() < sizeof(KD_PACKET)) return false;
        std::memcpy(&outHeader, raw.data(), sizeof(KD_PACKET));

        if (outHeader.packetLeader != PACKET_LEADER && outHeader.packetLeader != CONTROL_PACKET_LEADER) {
            return false;
        }

        size_t expectedSize = sizeof(KD_PACKET) + outHeader.byteCount;
        if (raw.size() < expectedSize) return false;

        outPayload.resize(outHeader.byteCount);
        if (outHeader.byteCount > 0) {
            std::memcpy(outPayload.data(), raw.data() + sizeof(KD_PACKET), outHeader.byteCount);
            uint32_t calcCrc = KdComputeChecksum(outPayload.data(), outPayload.size());
            if (calcCrc != outHeader.checksum) {
                return false; // Checksum error
            }
        }
        return true;
    }

    [[nodiscard]] size_t getOutboundQueueCount() const {
        std::lock_guard<std::mutex> guard(mutex_);
        return outboundPackets_.size();
    }

    [[nodiscard]] std::vector<uint8_t> popOutboundPacket() {
        std::lock_guard<std::mutex> guard(mutex_);
        if (outboundPackets_.empty()) return {};
        auto pkt = std::move(outboundPackets_.front());
        outboundPackets_.pop_front();
        return pkt;
    }

    void reset() {
        std::lock_guard<std::mutex> guard(mutex_);
        inboundPackets_.clear();
        outboundPackets_.clear();
        currentPacketId_ = INITIAL_PACKET_ID;
    }

private:
    mutable std::mutex mutex_;
    std::deque<std::vector<uint8_t>> inboundPackets_;
    std::deque<std::vector<uint8_t>> outboundPackets_;
    uint32_t currentPacketId_{INITIAL_PACKET_ID};
};

// ============================================================================
// 6. Kernel Debugger Subsystem Coordinator (KdDebugger)
// ============================================================================
class KdDebugger {
public:
    static KdDebugger& get() {
        static KdDebugger instance;
        return instance;
    }

    void initialize(bool enabled = true) {
        debuggerEnabled_.store(enabled, std::memory_order_relaxed);
        debuggerNotPresent_.store(!enabled, std::memory_order_relaxed);
        transport_.reset();
        ringBuffer_.clear();
    }

    [[nodiscard]] bool isDebuggerEnabled() const noexcept {
        return debuggerEnabled_.load(std::memory_order_relaxed);
    }

    [[nodiscard]] bool isDebuggerNotPresent() const noexcept {
        return debuggerNotPresent_.load(std::memory_order_relaxed);
    }

    void setDebuggerEnabled(bool enabled) noexcept {
        debuggerEnabled_.store(enabled, std::memory_order_relaxed);
        debuggerNotPresent_.store(!enabled, std::memory_order_relaxed);
    }

    KdTransport& getTransport() noexcept { return transport_; }
    KdPrintRingBuffer& getRingBuffer() noexcept { return ringBuffer_; }
    HardwareBreakpointEngine& getHwBreakpoints() noexcept { return hwBreakpoints_; }

    /**
     * @brief Checks if a break-in request is pending (KD64 breakin leader 0x62626262).
     */
    [[nodiscard]] bool pollBreakIn() noexcept {
        return breakinPending_.exchange(false, std::memory_order_relaxed);
    }

    void signalBreakIn() noexcept {
        breakinPending_.store(true, std::memory_order_relaxed);
    }

    /**
     * @brief Dispatches DbgPrint message to ring buffer and KD transport.
     */
    void logPrint(std::string_view msg) {
        ringBuffer_.append(msg);
        if (isDebuggerEnabled()) {
            DBGKD_DEBUG_IO ioHdr{};
            ioHdr.stringLength = static_cast<uint16_t>(msg.size());
            std::vector<uint8_t> payload(sizeof(DBGKD_DEBUG_IO) + msg.size());
            std::memcpy(payload.data(), &ioHdr, sizeof(DBGKD_DEBUG_IO));
            std::memcpy(payload.data() + sizeof(DBGKD_DEBUG_IO), msg.data(), msg.size());
            transport_.sendPacket(PACKET_TYPE_KD_DEBUG_IO, payload.data(), static_cast<uint16_t>(payload.size()));
        }
    }

    /**
     * @brief Processes a DbgKdStateManipulate command from the debugger host.
     */
    bool handleStateManipulate(
        const DBGKD_MANIPULATE_STATE64& req,
        const std::vector<uint8_t>& inData,
        DBGKD_MANIPULATE_STATE64& reply,
        std::vector<uint8_t>& outData
    ) {
        reply = req;
        outData.clear();

        switch (req.apiNumber) {
            case DbgKdGetVersionApi: {
                reply.returnStatus = static_cast<uint32_t>(NtStatus::Success);
                reply.getVersion = DBGKD_GET_VERSION64{};
                return true;
            }

            case DbgKdReadVirtualMemoryApi64: {
                uint64_t addr = req.readMemory.targetBaseAddress;
                uint32_t count = req.readMemory.transferCount;
                reply.readMemory.actualBytesRead = count;
                reply.returnStatus = static_cast<uint32_t>(NtStatus::Success);
                outData.resize(count);

                // Simulated memory read
                for (uint32_t i = 0; i < count; ++i) {
                    outData[i] = static_cast<uint8_t>((addr + i) & 0xFF);
                }
                return true;
            }

            case DbgKdWriteVirtualMemoryApi64: {
                uint32_t count = req.writeMemory.transferCount;
                reply.writeMemory.actualBytesWritten = count;
                reply.returnStatus = static_cast<uint32_t>(NtStatus::Success);
                return true;
            }

            case DbgKdWriteBreakPointApi: {
                uint32_t bpHandle = nextBreakpointHandle_++;
                reply.breakPoint.breakPointHandle = bpHandle;
                reply.returnStatus = static_cast<uint32_t>(NtStatus::Success);
                return true;
            }

            case DbgKdRestoreBreakPointApi: {
                reply.returnStatus = static_cast<uint32_t>(NtStatus::Success);
                return true;
            }

            case DbgKdContinueApi: {
                reply.returnStatus = static_cast<uint32_t>(NtStatus::Success);
                return true;
            }

            case DbgKdCauseBugCheckApi: {
                reply.returnStatus = static_cast<uint32_t>(NtStatus::Success);
                ke::KeBugCheckEx(ke::SYSTEM_SERVICE_EXCEPTION, 0x1, 0x2, 0x3, 0x4);
                return true;
            }

            default:
                reply.returnStatus = static_cast<uint32_t>(NtStatus::NotImplemented);
                return false;
        }
    }

private:
    KdDebugger() = default;
    std::atomic<bool> debuggerEnabled_{true};
    std::atomic<bool> debuggerNotPresent_{false};
    std::atomic<bool> breakinPending_{false};
    uint32_t nextBreakpointHandle_{1};
    KdTransport transport_;
    KdPrintRingBuffer ringBuffer_;
    HardwareBreakpointEngine hwBreakpoints_;
};

// ============================================================================
// 7. Kernel DbgPrint APIs
// ============================================================================
inline void DbgPrintEx(uint32_t /*componentId*/, uint32_t /*level*/, const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    KdDebugger::get().logPrint(buffer);
}

inline void DbgPrint(const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    KdDebugger::get().logPrint(buffer);
}

inline void KdBreakPointWithStatus(uint32_t status) {
    if (KdDebugger::get().isDebuggerEnabled()) {
        ke::ExceptionRecord record{
            .exceptionCode = ke::EXCEPTION_BREAKPOINT,
            .exceptionAddress = reinterpret_cast<void*>(0x7FFF0000ULL),
            .numberParameters = 1
        };
        record.exceptionInformation[0] = status;
        auto dummyProc = std::make_unique<ps::EProcess>(4, L"System");
        auto dummyThread = std::make_unique<ps::EThread>(1, dummyProc.get(), 0x7FFF0000ULL, 0x80000000ULL, 0x7FFF0000ULL);
        ke::TrapEngine::get().dispatchException(record, dummyThread->getContext(), *dummyThread, true);
    }
}

} // namespace micant::kd

namespace micant {
    using kd::DbgPrint;
    using kd::DbgPrintEx;
}
