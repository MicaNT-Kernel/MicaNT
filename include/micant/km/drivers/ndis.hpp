// ============================================================================
// MicaNT: Sovereign Network Driver Interface Specification (NDIS 6.88)
// File: include/micant/ndis.hpp
// Sovereign System Domain: TitanNDIS / RazzleNet
//
// Description:
//   Clean-room implementation of the Microsoft Windows Network Driver
//   Interface Specification (NDIS 6.0 through NDIS 6.88) architecture,
//   NDIS miniport driver model (ndis.sys), NET_BUFFER (NB) & NET_BUFFER_LIST
//   (NBL) memory pools, Hardware Offload Engine (IPv4/IPv6 Checksum Offload,
//   Large Send Offload v2 / LSOv2 / TSO, Receive Segment Coalescing / RSC),
//   Receive Side Scaling (RSS) with 40-byte Toeplitz Hash and 128-entry
//   indirection table, Single Root I/O Virtualization (SR-IOV) Virtual
//   Function management, and the high-speed RazzleNet 10GbE / 40GbE / 100GbE
//   PCIe Miniport Adapter (razzlenet.sys) at PCIe BDF 00:04.0 with dual
//   512-entry DMA ring descriptors (TX/RX), Adaptive Interrupt Moderation (AIM),
//   and complete C ABI export thunks.
//
// Clean-Room Engineering Reference & Standards:
//   - Microsoft Open win32metadata repository: Windows.Win32.NetworkDrivers.Ndis
//   - IEEE 802.3 Ethernet Specification (10GbE / 40GbE / 100GbE Framing)
//   - PCI Express Base Specification Rev 5.0 / 6.0 & PCI-SIG SR-IOV Rev 1.1
//   - IETF RFC 791 (IPv4), RFC 793 (TCP), RFC 768 (UDP), RFC 1071 (Checksum)
//   - ISO/IEC 14882:2023 C++ Standard
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <array>
#include <unordered_map>
#include <map>
#include <queue>
#include <mutex>
#include <atomic>
#include <span>
#include <functional>
#include <iomanip>
#include <sstream>
#include <algorithm>

#include "ntdef.hpp"
#include "ntstatus.hpp"
#include "janusldr.hpp"
#include "scm.hpp"
#include "version.hpp"
#include "pci.hpp"

#ifndef WINAPI
#define WINAPI __stdcall
#endif

namespace micant::ndis {

// ============================================================================
// 1. NDIS 6.88 Constants, Standards & Versioning
// ============================================================================

// Standard Ethernet II Framing Constants (IEEE 802.3 / DIX Ethernet)
inline constexpr uint16_t ETHERTYPE_IPV4 = 0x0800;
inline constexpr uint16_t ETHERTYPE_ARP  = 0x0806;
inline constexpr uint16_t ETHERTYPE_VLAN = 0x8100;
inline constexpr uint16_t ETHERTYPE_IPV6 = 0x86DD;

inline constexpr size_t ETH_ALEN         = 6;
inline constexpr size_t ETH_HLEN         = 14;
inline constexpr size_t ETH_VLAN_HLEN    = 18;
inline constexpr size_t ETH_MIN_LEN      = 60;
inline constexpr size_t ETH_MAX_LEN      = 1514;
inline constexpr size_t DEFAULT_MTU      = 1500;
inline constexpr size_t JUMBO_MTU        = 9014;

// NDIS Packet Filter Types
inline constexpr uint32_t NDIS_PACKET_TYPE_DIRECTED     = 0x00000001;
inline constexpr uint32_t NDIS_PACKET_TYPE_MULTICAST    = 0x00000002;
inline constexpr uint32_t NDIS_PACKET_TYPE_ALL_MULTICAST= 0x00000004;
inline constexpr uint32_t NDIS_PACKET_TYPE_BROADCAST    = 0x00000008;
inline constexpr uint32_t NDIS_PACKET_TYPE_PROMISCUOUS  = 0x00000020;

// NDIS Status Codes (ndis.sys)
inline constexpr uint32_t NDIS_STATUS_SUCCESS           = 0x00000000;
inline constexpr uint32_t NDIS_STATUS_PENDING           = 0x00000103;
inline constexpr uint32_t NDIS_STATUS_FAILURE           = 0xC0000001;
inline constexpr uint32_t NDIS_STATUS_INVALID_PARAMETER = 0xC000000D;
inline constexpr uint32_t NDIS_STATUS_RESOURCES         = 0xC000009A;
inline constexpr uint32_t NDIS_STATUS_BUFFER_TOO_SHORT  = 0xC00000F3;
inline constexpr uint32_t NDIS_STATUS_NOT_SUPPORTED     = 0xC00000BB;
inline constexpr uint32_t NDIS_STATUS_DEVICE_FAILED     = 0xC000000E;

// NDIS Driver Specification Versions
enum class NdisVersion : uint32_t {
    NDIS_6_0  = 0x0600, // Windows Vista
    NDIS_6_20 = 0x0620, // Windows 7
    NDIS_6_30 = 0x0630, // Windows 8
    NDIS_6_40 = 0x0640, // Windows 8.1
    NDIS_6_50 = 0x0650, // Windows 10 (1507)
    NDIS_6_80 = 0x0680, // Windows 10 (1709)
    NDIS_6_85 = 0x0685, // Windows 11 (21H2)
    NDIS_6_86 = 0x0686, // Windows 11 (22H2)
    NDIS_6_87 = 0x0687, // Windows 11 (23H2)
    NDIS_6_88 = 0x0688  // Windows 11 24H2 / Sovereign MicaNT NDIS
};

enum class NdisMedium : uint32_t {
    Medium802_3       = 0, // Ethernet
    MediumWan         = 1,
    MediumWirelessWan = 8
};

enum class NdisPhysicalMedium : uint32_t {
    Unspecified  = 0,
    WirelessLan  = 1,
    Ethernet8023 = 14,
    TenGigabit   = 18,
    FortyGigabit = 19,
    HundredGig   = 20
};

enum class MiniportState : uint32_t {
    Halted       = 0,
    Initializing = 1,
    Paused       = 2,
    Restarting   = 3,
    Running      = 4
};

// Media Connect States
enum class MediaConnectState : uint32_t {
    Unknown      = 0,
    Connected    = 1,
    Disconnected = 2
};

// ============================================================================
// 2. Ethernet Header & MAC Address Representation
// ============================================================================

#pragma pack(push, 1)

/**
 * @brief 6-byte IEEE 802.3 MAC Address.
 */
struct MacAddress {
    uint8_t bytes[ETH_ALEN]{0, 0, 0, 0, 0, 0};

    constexpr MacAddress() = default;
    constexpr MacAddress(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5)
        : bytes{b0, b1, b2, b3, b4, b5} {}

    [[nodiscard]] bool isBroadcast() const noexcept {
        for (uint8_t b : bytes) {
            if (b != 0xFF) return false;
        }
        return true;
    }

    [[nodiscard]] bool isMulticast() const noexcept {
        return (bytes[0] & 0x01) != 0;
    }

    [[nodiscard]] bool isZero() const noexcept {
        for (uint8_t b : bytes) {
            if (b != 0) return false;
        }
        return true;
    }

    [[nodiscard]] bool operator==(const MacAddress& other) const noexcept {
        return std::memcmp(bytes, other.bytes, ETH_ALEN) == 0;
    }

    [[nodiscard]] bool operator!=(const MacAddress& other) const noexcept {
        return !(*this == other);
    }

    [[nodiscard]] std::string toString() const {
        std::ostringstream oss;
        for (size_t i = 0; i < ETH_ALEN; ++i) {
            if (i > 0) oss << "-";
            oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(bytes[i]);
        }
        return oss.str();
    }

    static MacAddress broadcast() noexcept {
        return MacAddress(0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF);
    }

    static MacAddress defaultMica() noexcept {
        return MacAddress(0x02, 0x00, 0x4D, 0x49, 0x43, 0x41); // Locally administered: "MICA"
    }

    static MacAddress razzleNetDefault() noexcept {
        return MacAddress(0x52, 0x5A, 0x00, 0x04, 0x00, 0x01); // "RZ" 00:04.0 NIC 1
    }
};

/**
 * @brief 14-byte Standard Ethernet II Header.
 */
struct EthernetHeader {
    MacAddress destMac;
    MacAddress srcMac;
    uint16_t   etherType; // Network byte order (big endian)

    [[nodiscard]] uint16_t getEtherType() const noexcept {
        return (static_cast<uint16_t>(etherType >> 8) | static_cast<uint16_t>(etherType << 8));
    }

    void setEtherType(uint16_t type) noexcept {
        etherType = (static_cast<uint16_t>(type >> 8) | static_cast<uint16_t>(type << 8));
    }
};

/**
 * @brief 18-byte IEEE 802.1Q Tagged Ethernet Header.
 */
struct VlanEthernetHeader {
    MacAddress destMac;
    MacAddress srcMac;
    uint16_t   tpid{0x0081}; // 0x8100 in network order
    uint16_t   tci{0};       // Priority (3b), DEI (1b), VLAN ID (12b)
    uint16_t   etherType{0}; // Encapsulated EtherType

    [[nodiscard]] uint16_t getVlanId() const noexcept {
        uint16_t hostTci = (static_cast<uint16_t>(tci >> 8) | static_cast<uint16_t>(tci << 8));
        return hostTci & 0x0FFF;
    }

    void setVlanId(uint16_t vlanId, uint8_t priority = 0) noexcept {
        uint16_t hostTci = ((static_cast<uint16_t>(priority & 0x07) << 13) | (vlanId & 0x0FFF));
        tci = (static_cast<uint16_t>(hostTci >> 8) | static_cast<uint16_t>(hostTci << 8));
    }
};

#pragma pack(pop)

// ============================================================================
// 3. Memory Descriptor List (MDL) & NET_BUFFER (NB) / NET_BUFFER_LIST (NBL)
// ============================================================================

/**
 * @brief Memory Descriptor List (MDL) describing physical memory pages.
 */
struct Mdl {
    uint64_t physicalAddress{0};
    void*    mappedVa{nullptr};
    uint32_t byteCount{0};
    uint32_t byteOffset{0};
    std::shared_ptr<Mdl> nextMdl{nullptr};
};

/**
 * @brief NDIS 6.x NET_BUFFER structure representing packet payload buffers.
 */
struct NetBuffer {
    std::vector<uint8_t> data;
    uint32_t dataOffset{0};
    uint32_t dataLength{0};
    std::shared_ptr<Mdl> currentMdl{nullptr};
    std::shared_ptr<NetBuffer> next{nullptr};

    NetBuffer() = default;
    explicit NetBuffer(std::span<const uint8_t> payload)
        : data(payload.begin(), payload.end()),
          dataOffset(0),
          dataLength(static_cast<uint32_t>(payload.size())) {}
};

/**
 * @brief NDIS OOB Metadata Information Identifiers.
 */
enum NetBufferListInfoId : size_t {
    TcpIpChecksumNetBufferListInfo = 0,
    TcpLargeSendNetBufferListInfo  = 1,
    TcpReceiveSegmentCoalescingInfo = 2,
    NetBufferListFilteringInfo     = 3,
    MediaSpecificInformation       = 4,
    MaxNetBufferListInfo           = 8
};

struct NetBufferListInfoValue {
    uint64_t value{0};
};

// Checksum Offload Metadata Flags for NBL Info
inline constexpr uint32_t NDIS_TX_CSUM_IPV4_HEADER = 0x0001;
inline constexpr uint32_t NDIS_TX_CSUM_TCP         = 0x0002;
inline constexpr uint32_t NDIS_TX_CSUM_UDP         = 0x0004;
inline constexpr uint32_t NDIS_RX_CSUM_IP_VALID    = 0x0010;
inline constexpr uint32_t NDIS_RX_CSUM_TCP_VALID   = 0x0020;
inline constexpr uint32_t NDIS_RX_CSUM_UDP_VALID   = 0x0040;

/**
 * @brief NDIS 6.88 NET_BUFFER_LIST (NBL) Packet Descriptor & Chain Container.
 */
struct NetBufferList {
    std::shared_ptr<NetBufferList> next{nullptr};
    std::shared_ptr<NetBuffer>     firstNetBuffer{nullptr};
    uint32_t                       status{NDIS_STATUS_SUCCESS};
    std::array<NetBufferListInfoValue, MaxNetBufferListInfo> info{};
    void*                          miniportReserved{nullptr};
    void*                          protocolReserved{nullptr};
    uint64_t                       timestamp{0};
    uint32_t                       sourceCoreId{0};
};

/**
 * @brief High-Throughput NET_BUFFER_LIST Pool Allocator.
 */
class NetBufferListPool {
public:
    explicit NetBufferListPool(size_t initialCapacity = 256)
        : capacity_(initialCapacity) {
        pool_.reserve(initialCapacity);
    }

    std::shared_ptr<NetBufferList> allocate(std::span<const uint8_t> payload = {}) {
        std::lock_guard<std::mutex> lock(mutex_);
        std::shared_ptr<NetBufferList> nbl;

        if (!pool_.empty()) {
            nbl = pool_.back();
            pool_.pop_back();
            nbl->next = nullptr;
            nbl->status = NDIS_STATUS_SUCCESS;
            for (auto& inf : nbl->info) inf.value = 0;
            nbl->miniportReserved = nullptr;
            nbl->protocolReserved = nullptr;
        } else {
            nbl = std::make_shared<NetBufferList>();
        }

        auto nb = std::make_shared<NetBuffer>(payload);
        nbl->firstNetBuffer = nb;
        activeAllocations_++;
        totalAllocations_++;
        return nbl;
    }

    void free(std::shared_ptr<NetBufferList> nbl) {
        if (!nbl) return;
        std::lock_guard<std::mutex> lock(mutex_);
        nbl->firstNetBuffer = nullptr;
        nbl->next = nullptr;
        if (pool_.size() < capacity_) {
            pool_.push_back(nbl);
        }
        if (activeAllocations_ > 0) activeAllocations_--;
        totalFreed_++;
    }

    [[nodiscard]] size_t getActiveCount() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return activeAllocations_;
    }

    [[nodiscard]] size_t getFreeCount() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return pool_.size();
    }

    [[nodiscard]] size_t getCapacity() const noexcept {
        return capacity_;
    }

    [[nodiscard]] size_t getTotalAllocations() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return totalAllocations_;
    }

    [[nodiscard]] size_t getTotalFreed() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return totalFreed_;
    }

    void resetForTesting() {
        std::lock_guard<std::mutex> lock(mutex_);
        activeAllocations_ = 0;
        totalAllocations_ = 0;
        totalFreed_ = 0;
    }

private:
    size_t capacity_{256};
    size_t activeAllocations_{0};
    size_t totalAllocations_{0};
    size_t totalFreed_{0};
    std::vector<std::shared_ptr<NetBufferList>> pool_;
    mutable std::mutex mutex_;
};

// ============================================================================
// 4. Hardware Offload Engine (Checksum, LSOv2, RSC)
// ============================================================================

/**
 * @brief Internet Standard 16-bit 1's Complement Checksum (RFC 1071).
 */
inline uint16_t calculateIpChecksum(const void* data, size_t length) noexcept {
    auto* ptr = reinterpret_cast<const uint8_t*>(data);
    uint32_t sum = 0;

    while (length > 1) {
        sum += (static_cast<uint16_t>(ptr[0]) << 8) | static_cast<uint16_t>(ptr[1]);
        ptr += 2;
        length -= 2;
    }

    if (length > 0) {
        sum += (static_cast<uint16_t>(ptr[0]) << 8);
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return static_cast<uint16_t>(~sum);
}

struct ChecksumOffloadCaps {
    bool txIpv4Header{true};
    bool txTcpIpv4{true};
    bool txUdpIpv4{true};
    bool txTcpIpv6{true};
    bool txUdpIpv6{true};
    bool rxIpv4Header{true};
    bool rxTcpIpv4{true};
    bool rxUdpIpv4{true};
    bool rxTcpIpv6{true};
    bool rxUdpIpv6{true};
};

struct LsoV2Caps {
    bool     enabled{true};
    uint32_t maxOffloadSize{65536}; // 64 KB Jumbo Segment
    uint32_t minSegmentCount{2};
};

struct RscCaps {
    bool     enabled{true};
    bool     ipv4Supported{true};
    bool     ipv6Supported{true};
    uint32_t maxCoalescedSize{65536};
};

struct OffloadCapabilities {
    ChecksumOffloadCaps checksum{};
    LsoV2Caps           lsoV2{};
    RscCaps             rsc{};
};

// ============================================================================
// 5. Receive Side Scaling (RSS) Engine & Toeplitz Hash
// ============================================================================

// Standard Microsoft NDIS 40-byte Default Toeplitz RSS Secret Key
inline constexpr std::array<uint8_t, 40> DEFAULT_RSS_KEY = {
    0x6D, 0x5A, 0x56, 0xDA, 0x25, 0x5B, 0x0E, 0xC2,
    0x41, 0x67, 0x25, 0x3D, 0x43, 0xA3, 0x8F, 0xB0,
    0xD0, 0xCA, 0x2B, 0xCB, 0xAE, 0x7B, 0x30, 0xB4,
    0x77, 0xCB, 0x2D, 0xA3, 0x80, 0x30, 0xF2, 0x0C,
    0x6A, 0x42, 0xB7, 0x3B, 0xBE, 0xAC, 0x01, 0xFA
};

/**
 * @brief Computes 32-bit Toeplitz Hash across packet 4-tuple and 40-byte secret key.
 */
inline uint32_t computeToeplitzHash(
    const std::vector<uint8_t>& inputData,
    const std::array<uint8_t, 40>& secretKey = DEFAULT_RSS_KEY
) noexcept {
    if (inputData.empty()) return 0;

    uint32_t hash = 0;
    for (size_t i = 0; i < inputData.size(); ++i) {
        uint8_t byteVal = inputData[i];
        for (int bit = 7; bit >= 0; --bit) {
            if ((byteVal >> bit) & 1) {
                size_t bitIdx = i * 8 + (7 - bit);
                size_t byteIdx = bitIdx / 8;
                size_t bitShift = bitIdx % 8;
                if (byteIdx + 4 <= secretKey.size()) {
                    uint32_t keyWindow = (static_cast<uint32_t>(secretKey[byteIdx]) << 24) |
                                         (static_cast<uint32_t>(secretKey[byteIdx + 1]) << 16) |
                                         (static_cast<uint32_t>(secretKey[byteIdx + 2]) << 8) |
                                         (static_cast<uint32_t>(secretKey[byteIdx + 3]));
                    if (bitShift > 0 && byteIdx + 4 < secretKey.size()) {
                        keyWindow = (keyWindow << bitShift) | (secretKey[byteIdx + 4] >> (8 - bitShift));
                    } else {
                        keyWindow <<= bitShift;
                    }
                    hash ^= keyWindow;
                }
            }
        }
    }
    return hash;
}

/**
 * @brief RSS Configuration Parameters and Indirection Table.
 */
struct RssParameters {
    bool enabled{true};
    uint32_t hashType{0x00000003}; // IPv4 & TCP IPv4
    std::array<uint8_t, 40> secretKey{DEFAULT_RSS_KEY};
    std::array<uint8_t, 128> indirectionTable{};
    uint32_t numCpuQueues{4};
    std::array<uint64_t, 16> perCorePacketCount{};

    RssParameters() {
        for (size_t i = 0; i < indirectionTable.size(); ++i) {
            indirectionTable[i] = static_cast<uint8_t>(i % numCpuQueues);
        }
    }

    [[nodiscard]] uint8_t getTargetCpu(uint32_t hash) const noexcept {
        if (!enabled || numCpuQueues == 0) return 0;
        size_t idx = (hash & 0x7F) % indirectionTable.size();
        return indirectionTable[idx];
    }
};

// ============================================================================
// 6. High-Speed DMA Descriptor Rings (TX / RX)
// ============================================================================

// TX Descriptor Command Bits
inline constexpr uint8_t RAZZLE_TXD_CMD_EOP  = 0x01; // End of Packet
inline constexpr uint8_t RAZZLE_TXD_CMD_IFCS = 0x02; // Insert FCS
inline constexpr uint8_t RAZZLE_TXD_CMD_RS   = 0x08; // Report Status
inline constexpr uint8_t RAZZLE_TXD_CMD_TSE  = 0x20; // TCP Segmentation Enable (LSOv2)
inline constexpr uint8_t RAZZLE_TXD_STAT_DD  = 0x01; // Descriptor Done

struct RazzleTxDescriptor {
    uint64_t bufferAddress{0};
    uint16_t length{0};
    uint8_t  cso{0};       // Checksum offset
    uint8_t  cmd{0};       // Command flags
    uint8_t  status{0};    // Status (DD: Descriptor Done)
    uint8_t  css{0};       // Checksum start
    uint16_t vlanOrMss{0}; // VLAN tag or MSS for LSOv2
};

// RX Descriptor Status & Error Bits
inline constexpr uint8_t RAZZLE_RXD_STAT_DD  = 0x01; // Descriptor Done
inline constexpr uint8_t RAZZLE_RXD_STAT_EOP = 0x02; // End of Packet
inline constexpr uint8_t RAZZLE_RXD_STAT_IPCS= 0x20; // IPv4 Checksum Evaluated
inline constexpr uint8_t RAZZLE_RXD_STAT_L4CS= 0x40; // L4 (TCP/UDP) Checksum Evaluated

struct RazzleRxDescriptor {
    uint64_t bufferAddress{0};
    uint16_t length{0};
    uint16_t checksum{0};
    uint8_t  status{0};    // DD, EOP, IPCS, L4CS
    uint8_t  errors{0};
    uint16_t vlanTag{0};
    uint16_t rscCount{1};  // Coalesced segment count
};

/**
 * @brief High-Speed 512-Entry Circular DMA Ring Buffer.
 */
class RazzleNetRingBuffer {
public:
    static constexpr size_t RING_SIZE = 512;

    // TX Ring
    std::array<RazzleTxDescriptor, RING_SIZE> txRing{};
    uint32_t txHead{0};
    uint32_t txTail{0};
    uint64_t txCompleted{0};

    // RX Ring
    std::array<RazzleRxDescriptor, RING_SIZE> rxRing{};
    uint32_t rxHead{0};
    uint32_t rxTail{0};
    uint64_t rxCompleted{0};

    // Internal simulated physical buffers
    std::vector<std::vector<uint8_t>> txBuffers;
    std::vector<std::vector<uint8_t>> rxBuffers;

    RazzleNetRingBuffer() {
        txBuffers.resize(RING_SIZE);
        rxBuffers.resize(RING_SIZE);
        for (size_t i = 0; i < RING_SIZE; ++i) {
            rxBuffers[i].resize(JUMBO_MTU + ETH_HLEN);
            rxRing[i].bufferAddress = 0x10000000ULL + (i * 0x4000);
            rxRing[i].length = static_cast<uint16_t>(rxBuffers[i].size());
        }
    }

    [[nodiscard]] size_t getTxAvailable() const noexcept {
        if (txTail >= txHead) {
            return RING_SIZE - 1 - (txTail - txHead);
        }
        return txHead - txTail - 1;
    }

    [[nodiscard]] size_t getRxAvailable() const noexcept {
        if (rxTail >= rxHead) {
            return RING_SIZE - 1 - (rxTail - rxHead);
        }
        return rxHead - rxTail - 1;
    }
};

// ============================================================================
// 7. Single Root I/O Virtualization (SR-IOV)
// ============================================================================

struct VirtualFunctionConfig {
    uint32_t   vfId{0};          // 0 .. 15
    MacAddress vfMac{};
    uint16_t   vlanId{0};        // 0 = untagged, 1..4095
    uint32_t   maxRateMbps{0};   // 0 = unlimited, or rate limit in Mbps
    bool       spoofCheck{true};
    bool       allocated{false};
    uint64_t   txBytes{0};
    uint64_t   rxBytes{0};
    uint64_t   txPackets{0};
    uint64_t   rxPackets{0};
};

class SriovManager {
public:
    static constexpr size_t MAX_VFS = 16;

    SriovManager() {
        for (uint32_t i = 0; i < MAX_VFS; ++i) {
            vfs_[i].vfId = i;
            vfs_[i].vfMac = MacAddress(0x52, 0x5A, 0x00, 0x04, 0x01, static_cast<uint8_t>(i));
            vfs_[i].allocated = false;
        }
    }

    bool enableSriov(uint32_t numVfs) {
        if (numVfs > MAX_VFS) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        numActiveVfs_ = numVfs;
        sriovEnabled_ = (numVfs > 0);
        return true;
    }

    bool configureVf(uint32_t vfId, MacAddress mac, uint16_t vlanId, uint32_t maxRateMbps, bool spoofCheck) {
        if (vfId >= MAX_VFS) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        vfs_[vfId].vfMac = mac;
        vfs_[vfId].vlanId = vlanId;
        vfs_[vfId].maxRateMbps = maxRateMbps;
        vfs_[vfId].spoofCheck = spoofCheck;
        vfs_[vfId].allocated = true;
        return true;
    }

    [[nodiscard]] const VirtualFunctionConfig* getVf(uint32_t vfId) const {
        if (vfId >= MAX_VFS) return nullptr;
        return &vfs_[vfId];
    }

    bool releaseVf(uint32_t vfId) {
        if (vfId >= MAX_VFS) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        vfs_[vfId].allocated = false;
        return true;
    }

    bool recordVfTx(uint32_t vfId, uint64_t bytes) {
        if (vfId >= MAX_VFS) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        vfs_[vfId].txPackets++;
        vfs_[vfId].txBytes += bytes;
        return true;
    }

    bool recordVfRx(uint32_t vfId, uint64_t bytes) {
        if (vfId >= MAX_VFS) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        vfs_[vfId].rxPackets++;
        vfs_[vfId].rxBytes += bytes;
        return true;
    }

    [[nodiscard]] bool isSriovEnabled() const noexcept { return sriovEnabled_; }
    [[nodiscard]] uint32_t getActiveVfCount() const noexcept { return numActiveVfs_; }

private:
    bool sriovEnabled_{false};
    uint32_t numActiveVfs_{0};
    std::array<VirtualFunctionConfig, MAX_VFS> vfs_{};
    mutable std::mutex mutex_;
};

// ============================================================================
// 8. Abstract NDIS Adapter Interface (INdisAdapter) & Statistics
// ============================================================================

/**
 * @brief Adapter Telemetry Statistics.
 */
struct AdapterStatistics {
    uint64_t rxBytes{0};
    uint64_t txBytes{0};
    uint64_t rxPackets{0};
    uint64_t txPackets{0};
    uint64_t rxErrors{0};
    uint64_t txErrors{0};
    uint64_t rxDrops{0};
    uint64_t lsoPackets{0};
    uint64_t rscPackets{0};
    uint64_t csumOffloadTx{0};
    uint64_t csumOffloadRx{0};
};

/**
 * @brief Abstract NDIS Network Adapter Interface (INdisAdapter).
 */
class INdisAdapter {
public:
    using ReceiveCallback = std::function<void(std::span<const uint8_t>)>;

    virtual ~INdisAdapter() = default;

    [[nodiscard]] virtual const std::wstring& getAdapterName() const noexcept = 0;
    [[nodiscard]] virtual const std::wstring& getFriendlyName() const noexcept = 0;
    [[nodiscard]] virtual MacAddress getMacAddress() const noexcept = 0;
    [[nodiscard]] virtual uint32_t getMtu() const noexcept = 0;
    [[nodiscard]] virtual uint64_t getSpeedBps() const noexcept = 0;
    [[nodiscard]] virtual MediaConnectState getLinkState() const noexcept = 0;
    [[nodiscard]] virtual AdapterStatistics getStatistics() const noexcept = 0;

    [[nodiscard]] virtual NtStatus sendPacket(std::span<const uint8_t> frame) = 0;
    virtual void registerReceiveHandler(ReceiveCallback callback) = 0;
};

// ============================================================================
// 9. Sovereign Virtual Network Adapter (Loopback & Virtual Switch)
// ============================================================================

/**
 * @brief High-Performance In-Memory Virtual Ethernet Adapter.
 * Provides virtual loopback, packet forwarding, and frame inspection.
 */
class VirtualNetworkAdapter : public INdisAdapter {
public:
    VirtualNetworkAdapter(
        std::wstring_view name,
        std::wstring_view friendlyName,
        MacAddress mac = MacAddress::defaultMica(),
        uint32_t mtu = DEFAULT_MTU,
        uint64_t speedBps = 10'000'000'000ULL // 10 Gbps Virtual Bus
    ) : name_(name),
        friendlyName_(friendlyName),
        mac_(mac),
        mtu_(mtu),
        speedBps_(speedBps) {}

    [[nodiscard]] const std::wstring& getAdapterName() const noexcept override { return name_; }
    [[nodiscard]] const std::wstring& getFriendlyName() const noexcept override { return friendlyName_; }
    [[nodiscard]] MacAddress getMacAddress() const noexcept override { return mac_; }
    [[nodiscard]] uint32_t getMtu() const noexcept override { return mtu_; }
    [[nodiscard]] uint64_t getSpeedBps() const noexcept override { return speedBps_; }
    [[nodiscard]] MediaConnectState getLinkState() const noexcept override { return linkState_; }

    [[nodiscard]] AdapterStatistics getStatistics() const noexcept override {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }

    void setLinkState(MediaConnectState state) noexcept {
        linkState_ = state;
        linkStateChangeCount_++;
    }

    [[nodiscard]] uint32_t getLinkStateChangeCount() const noexcept {
        return linkStateChangeCount_;
    }

    void registerReceiveHandler(ReceiveCallback callback) override {
        std::lock_guard<std::mutex> lock(mutex_);
        rxCallback_ = std::move(callback);
    }

    /**
     * @brief Transmit an Ethernet frame.
     */
    [[nodiscard]] NtStatus sendPacket(std::span<const uint8_t> frame) override {
        if (frame.size() < ETH_HLEN) return NtStatus::InvalidParameter;
        if (frame.size() > mtu_ + ETH_HLEN) return NtStatus::BufferOverflow;
        if (linkState_ != MediaConnectState::Connected) return NtStatus::DeviceNotReady;

        {
            std::lock_guard<std::mutex> lock(mutex_);
            stats_.txPackets++;
            stats_.txBytes += frame.size();
        }

        // Forward to connected peer or loopback
        if (peerAdapter_) {
            peerAdapter_->injectPacket(frame);
        }

        return NtStatus::Success;
    }

    /**
     * @brief Directly injects a received packet into this adapter's RX pipeline.
     */
    void injectPacket(std::span<const uint8_t> frame) {
        if (frame.size() < ETH_HLEN) {
            std::lock_guard<std::mutex> lock(mutex_);
            stats_.rxErrors++;
            return;
        }

        ReceiveCallback cb;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stats_.rxPackets++;
            stats_.rxBytes += frame.size();
            cb = rxCallback_;
        }

        if (cb) {
            cb(frame);
        } else {
            std::lock_guard<std::mutex> lock(mutex_);
            stats_.rxDrops++;
        }
    }

    /**
     * @brief Connects this virtual adapter to another virtual adapter as a virtual Ethernet cable.
     */
    void connectPeer(std::shared_ptr<VirtualNetworkAdapter> peer) {
        peerAdapter_ = peer;
        if (peer) {
            linkState_ = MediaConnectState::Connected;
        }
    }

private:
    std::wstring name_;
    std::wstring friendlyName_;
    MacAddress mac_;
    uint32_t mtu_{DEFAULT_MTU};
    uint64_t speedBps_{10'000'000'000ULL};
    MediaConnectState linkState_{MediaConnectState::Connected};
    uint32_t linkStateChangeCount_{0};
    mutable std::mutex mutex_;
    AdapterStatistics stats_{};
    ReceiveCallback rxCallback_;
    std::shared_ptr<VirtualNetworkAdapter> peerAdapter_;
};

// ============================================================================
// 10. RazzleNet 10GbE / 40GbE / 100GbE PCIe Miniport Adapter (razzlenet.sys)
// ============================================================================

/**
 * @brief RazzleNet High-Speed Hardware PCIe Miniport Driver (razzlenet.sys).
 * Binds to PCIe BDF 00:04.0 (VEN_8086&DEV_1563).
 */
class RazzleNetAdapter : public INdisAdapter {
public:
    // MMIO Register Offsets (BAR0 512KB)
    static constexpr uint32_t REG_CTRL   = 0x0000;
    static constexpr uint32_t REG_STATUS = 0x0008;
    static constexpr uint32_t REG_EIMS   = 0x0880;
    static constexpr uint32_t REG_EIMC   = 0x0888;
    static constexpr uint32_t REG_RDBAL  = 0x1000;
    static constexpr uint32_t REG_RDBAH  = 0x1004;
    static constexpr uint32_t REG_RDLEN  = 0x1008;
    static constexpr uint32_t REG_RDH    = 0x1010;
    static constexpr uint32_t REG_RDT    = 0x1018;
    static constexpr uint32_t REG_TDBAL  = 0x6000;
    static constexpr uint32_t REG_TDBAH  = 0x6004;
    static constexpr uint32_t REG_TDLEN  = 0x6008;
    static constexpr uint32_t REG_TDH    = 0x6010;
    static constexpr uint32_t REG_TDT    = 0x6018;
    static constexpr uint32_t REG_MRQC   = 0xEC00;
    static constexpr uint32_t REG_RSSRK  = 0x5480;
    static constexpr uint32_t REG_RETA   = 0x5C00;

    explicit RazzleNetAdapter(
        std::wstring_view name = L"\\Device\\RazzleNet0",
        std::wstring_view friendlyName = L"RazzleNet 10-Gigabit Ethernet Adapter (Titan 10G-SR)",
        MacAddress mac = MacAddress::razzleNetDefault(),
        pci::PciAddress pciAddr = pci::PciAddress(0, 4, 0)
    ) : name_(name),
        friendlyName_(friendlyName),
        mac_(mac),
        pciAddr_(pciAddr),
        mtu_(DEFAULT_MTU),
        speedBps_(10'000'000'000ULL), // 10 Gbps default
        linkState_(MediaConnectState::Connected),
        ringBuffer_(std::make_unique<RazzleNetRingBuffer>()),
        sriov_(std::make_unique<SriovManager>())
    {
        mmioRegisters_.resize(512 * 1024 / 4, 0); // 512 KB MMIO in 32-bit words
        writeMmio32(REG_STATUS, 0x00000003);    // Link Up, 10G Full Duplex
    }

    [[nodiscard]] const std::wstring& getAdapterName() const noexcept override { return name_; }
    [[nodiscard]] const std::wstring& getFriendlyName() const noexcept override { return friendlyName_; }
    [[nodiscard]] MacAddress getMacAddress() const noexcept override { return mac_; }
    [[nodiscard]] uint32_t getMtu() const noexcept override { return mtu_; }
    [[nodiscard]] uint64_t getSpeedBps() const noexcept override { return speedBps_; }
    [[nodiscard]] MediaConnectState getLinkState() const noexcept override { return linkState_; }
    [[nodiscard]] pci::PciAddress getPciAddress() const noexcept { return pciAddr_; }

    [[nodiscard]] AdapterStatistics getStatistics() const noexcept override {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }

    void setLinkSpeed(uint64_t speedBps) noexcept {
        speedBps_ = speedBps;
    }

    void setMtu(uint32_t mtu) noexcept {
        mtu_ = std::clamp(mtu, static_cast<uint32_t>(ETH_MIN_LEN), static_cast<uint32_t>(JUMBO_MTU));
    }

    void setLinkState(MediaConnectState state) noexcept {
        linkState_ = state;
        linkStateChangeCount_++;
        writeMmio32(REG_STATUS, (state == MediaConnectState::Connected) ? 0x00000003 : 0x00000000);
    }

    [[nodiscard]] uint32_t getLinkStateChangeCount() const noexcept {
        return linkStateChangeCount_;
    }

    bool resetHardware() {
        std::lock_guard<std::mutex> lock(mutex_);
        resetCount_++;
        writeMmio32(REG_CTRL, 0x04000000); // Master Reset
        writeMmio32(REG_STATUS, 0x00000003); // Link Restored
        return true;
    }

    [[nodiscard]] uint32_t getResetCount() const noexcept {
        return resetCount_;
    }

    void registerReceiveHandler(ReceiveCallback callback) override {
        std::lock_guard<std::mutex> lock(mutex_);
        rxCallback_ = std::move(callback);
    }

    // MMIO Register Access
    [[nodiscard]] uint32_t readMmio32(uint32_t offset) const noexcept {
        size_t idx = offset / 4;
        if (idx < mmioRegisters_.size()) return mmioRegisters_[idx];
        return 0xFFFFFFFF;
    }

    void writeMmio32(uint32_t offset, uint32_t value) noexcept {
        size_t idx = offset / 4;
        if (idx < mmioRegisters_.size()) {
            mmioRegisters_[idx] = value;
        }
    }

    [[nodiscard]] RazzleNetRingBuffer& getRingBuffer() noexcept { return *ringBuffer_; }
    [[nodiscard]] const RazzleNetRingBuffer& getRingBuffer() const noexcept { return *ringBuffer_; }
    [[nodiscard]] SriovManager& getSriovManager() noexcept { return *sriov_; }
    [[nodiscard]] const SriovManager& getSriovManager() const noexcept { return *sriov_; }
    [[nodiscard]] OffloadCapabilities& getOffloadCapabilities() noexcept { return offloads_; }
    [[nodiscard]] const OffloadCapabilities& getOffloadCapabilities() const noexcept { return offloads_; }
    [[nodiscard]] RssParameters& getRssParameters() noexcept { return rss_; }
    [[nodiscard]] const RssParameters& getRssParameters() const noexcept { return rss_; }

    /**
     * @brief Transmit frame through high-speed DMA TX descriptor ring.
     */
    [[nodiscard]] NtStatus sendPacket(std::span<const uint8_t> frame) override {
        if (frame.size() < ETH_HLEN) return NtStatus::InvalidParameter;
        if (linkState_ != MediaConnectState::Connected) return NtStatus::DeviceNotReady;

        std::lock_guard<std::mutex> lock(mutex_);

        // Check if Large Send Offload v2 (LSOv2) should be triggered for payloads > MTU
        if (frame.size() > (mtu_ + ETH_HLEN)) {
            if (offloads_.lsoV2.enabled && frame.size() <= offloads_.lsoV2.maxOffloadSize) {
                return transmitLsoSegments(frame);
            }
            return NtStatus::BufferOverflow;
        }

        // Place on TX DMA Ring
        uint32_t tail = ringBuffer_->txTail;
        auto& desc = ringBuffer_->txRing[tail];
        desc.bufferAddress = 0x20000000ULL + (tail * 0x4000);
        desc.length = static_cast<uint16_t>(frame.size());
        desc.cmd = RAZZLE_TXD_CMD_EOP | RAZZLE_TXD_CMD_IFCS | RAZZLE_TXD_CMD_RS;
        desc.status = 0; // In flight

        // Copy buffer payload
        ringBuffer_->txBuffers[tail].assign(frame.begin(), frame.end());

        // Update TX Tail doorbell
        ringBuffer_->txTail = (tail + 1) % RazzleNetRingBuffer::RING_SIZE;
        writeMmio32(REG_TDT, ringBuffer_->txTail);

        // Hardware DMA complete simulation
        desc.status |= RAZZLE_TXD_STAT_DD;
        ringBuffer_->txHead = ringBuffer_->txTail;
        writeMmio32(REG_TDH, ringBuffer_->txHead);
        ringBuffer_->txCompleted++;

        stats_.txPackets++;
        stats_.txBytes += frame.size();

        return NtStatus::Success;
    }

    /**
     * @brief Send batch of NET_BUFFER_LIST structures.
     */
    [[nodiscard]] NtStatus sendNetBufferLists(std::shared_ptr<NetBufferList> nblList) {
        if (!nblList) return NtStatus::InvalidParameter;

        auto currNbl = nblList;
        while (currNbl) {
            auto currNb = currNbl->firstNetBuffer;
            while (currNb) {
                if (!currNb->data.empty()) {
                    auto status = sendPacket(currNb->data);
                    if (status != NtStatus::Success) {
                        currNbl->status = NDIS_STATUS_FAILURE;
                        return status;
                    }
                }
                currNb = currNb->next;
            }
            currNbl->status = NDIS_STATUS_SUCCESS;
            currNbl = currNbl->next;
        }

        return NtStatus::Success;
    }

    /**
     * @brief Receive packet through high-speed DMA RX descriptor ring with RSS and RSC.
     */
    void receivePacket(std::span<const uint8_t> frame) {
        if (frame.size() < ETH_HLEN) {
            std::lock_guard<std::mutex> lock(mutex_);
            stats_.rxErrors++;
            return;
        }

        ReceiveCallback cb;
        uint32_t targetCpu = 0;

        {
            std::lock_guard<std::mutex> lock(mutex_);

            // Place on RX DMA Ring
            uint32_t head = ringBuffer_->rxHead;
            auto& desc = ringBuffer_->rxRing[head];
            desc.length = static_cast<uint16_t>(frame.size());
            desc.status = RAZZLE_RXD_STAT_DD | RAZZLE_RXD_STAT_EOP;

            // RSS Hash calculation over IPv4 4-tuple if present
            if (frame.size() >= ETH_HLEN + 20) {
                uint16_t ethType = (static_cast<uint16_t>(frame[12]) << 8) | frame[13];
                if (ethType == ETHERTYPE_IPV4) {
                    desc.status |= RAZZLE_RXD_STAT_IPCS;
                    desc.status |= RAZZLE_RXD_STAT_L4CS;
                    stats_.csumOffloadRx++;

                    // Extract IP 4-tuple for Toeplitz hash
                    std::vector<uint8_t> tupleKey;
                    tupleKey.reserve(12);
                    // Src IP (4B) + Dst IP (4B)
                    tupleKey.insert(tupleKey.end(), frame.begin() + ETH_HLEN + 12, frame.begin() + ETH_HLEN + 20);
                    // If TCP/UDP, add Src Port (2B) + Dst Port (2B)
                    if (frame.size() >= ETH_HLEN + 24) {
                        tupleKey.insert(tupleKey.end(), frame.begin() + ETH_HLEN + 20, frame.begin() + ETH_HLEN + 24);
                    }

                    uint32_t hash = computeToeplitzHash(tupleKey, rss_.secretKey);
                    targetCpu = rss_.getTargetCpu(hash);
                    rss_.perCorePacketCount[targetCpu % 16]++;
                }
            }

            // Copy to RX buffer
            ringBuffer_->rxBuffers[head].assign(frame.begin(), frame.end());

            // Advance RX ring
            ringBuffer_->rxHead = (head + 1) % RazzleNetRingBuffer::RING_SIZE;
            ringBuffer_->rxTail = ringBuffer_->rxHead;
            writeMmio32(REG_RDH, ringBuffer_->rxHead);
            writeMmio32(REG_RDT, ringBuffer_->rxTail);
            ringBuffer_->rxCompleted++;

            stats_.rxPackets++;
            stats_.rxBytes += frame.size();
            cb = rxCallback_;
        }

        if (cb) {
            cb(frame);
        } else {
            std::lock_guard<std::mutex> lock(mutex_);
            stats_.rxDrops++;
        }
    }

private:
    /**
     * @brief Performs Large Send Offload v2 (LSOv2) segmentation in hardware DMA.
     */
    NtStatus transmitLsoSegments(std::span<const uint8_t> largeFrame) {
        if (largeFrame.size() < ETH_HLEN + 40) return NtStatus::InvalidParameter;

        // Ethernet (14) + IP Header (20) + TCP Header (20) = 54 bytes
        const size_t headerLen = ETH_HLEN + 40;
        const size_t payloadLen = largeFrame.size() - headerLen;
        const size_t mss = mtu_ - 40; // Max Segment Size

        size_t offset = 0;
        uint32_t segCount = 0;

        while (offset < payloadLen) {
            size_t segSize = std::min(mss, payloadLen - offset);
            std::vector<uint8_t> segPacket(headerLen + segSize);

            // Copy Headers
            std::memcpy(segPacket.data(), largeFrame.data(), headerLen);
            // Copy Segment Payload
            std::memcpy(segPacket.data() + headerLen, largeFrame.data() + headerLen + offset, segSize);

            // Update IP Length
            uint16_t totalLen = static_cast<uint16_t>(20 + 20 + segSize);
            segPacket[ETH_HLEN + 2] = static_cast<uint8_t>(totalLen >> 8);
            segPacket[ETH_HLEN + 3] = static_cast<uint8_t>(totalLen & 0xFF);

            // Hardware Checksum Calculation Offload
            if (offloads_.checksum.txIpv4Header) {
                segPacket[ETH_HLEN + 10] = 0;
                segPacket[ETH_HLEN + 11] = 0;
                uint16_t ipCsum = calculateIpChecksum(segPacket.data() + ETH_HLEN, 20);
                segPacket[ETH_HLEN + 10] = static_cast<uint8_t>(ipCsum >> 8);
                segPacket[ETH_HLEN + 11] = static_cast<uint8_t>(ipCsum & 0xFF);
                stats_.csumOffloadTx++;
            }

            // Put on TX Ring
            uint32_t tail = ringBuffer_->txTail;
            auto& desc = ringBuffer_->txRing[tail];
            desc.length = static_cast<uint16_t>(segPacket.size());
            desc.cmd = RAZZLE_TXD_CMD_EOP | RAZZLE_TXD_CMD_IFCS | RAZZLE_TXD_CMD_TSE;
            desc.vlanOrMss = static_cast<uint16_t>(mss);
            desc.status = RAZZLE_TXD_STAT_DD;
            ringBuffer_->txBuffers[tail] = std::move(segPacket);

            ringBuffer_->txTail = (tail + 1) % RazzleNetRingBuffer::RING_SIZE;
            ringBuffer_->txHead = ringBuffer_->txTail;
            ringBuffer_->txCompleted++;

            stats_.txPackets++;
            stats_.txBytes += (headerLen + segSize);
            offset += segSize;
            segCount++;
        }

        stats_.lsoPackets += segCount;
        writeMmio32(REG_TDH, ringBuffer_->txHead);
        writeMmio32(REG_TDT, ringBuffer_->txTail);

        return NtStatus::Success;
    }

    std::wstring name_;
    std::wstring friendlyName_;
    MacAddress mac_;
    pci::PciAddress pciAddr_;
    uint32_t mtu_{DEFAULT_MTU};
    uint64_t speedBps_{10'000'000'000ULL};
    MediaConnectState linkState_{MediaConnectState::Connected};
    mutable std::mutex mutex_;
    AdapterStatistics stats_{};
    ReceiveCallback rxCallback_;

    std::vector<uint32_t> mmioRegisters_;
    std::unique_ptr<RazzleNetRingBuffer> ringBuffer_;
    std::unique_ptr<SriovManager> sriov_;
    OffloadCapabilities offloads_{};
    RssParameters rss_{};
    uint32_t resetCount_{0};
    uint32_t linkStateChangeCount_{0};
};

// ============================================================================
// 10A. Intel Gigabit / 2.5GbE Ethernet Miniport Adapter (e1000e.sys)
// ============================================================================

class IntelE1000eAdapter : public INdisAdapter {
public:
    static constexpr uint32_t REG_CTRL   = 0x0000;
    static constexpr uint32_t REG_STATUS = 0x0008;
    static constexpr uint32_t REG_EECD   = 0x0010;
    static constexpr uint32_t REG_IMS    = 0x00D0;
    static constexpr uint32_t REG_IMC    = 0x00D8;
    static constexpr uint32_t REG_RDBAL  = 0x2800;
    static constexpr uint32_t REG_RDBAH  = 0x2804;
    static constexpr uint32_t REG_RDLEN  = 0x2808;
    static constexpr uint32_t REG_RDH    = 0x2810;
    static constexpr uint32_t REG_RDT    = 0x2818;
    static constexpr uint32_t REG_TDBAL  = 0x3800;
    static constexpr uint32_t REG_TDBAH  = 0x3804;
    static constexpr uint32_t REG_TDLEN  = 0x3808;
    static constexpr uint32_t REG_TDH    = 0x3810;
    static constexpr uint32_t REG_TDT    = 0x3818;

    explicit IntelE1000eAdapter(
        std::wstring_view name = L"\\Device\\E1000e0",
        std::wstring_view friendlyName = L"Intel(R) 82574L Gigabit Network Connection",
        MacAddress mac = MacAddress(0x00, 0x1B, 0x21, 0xAA, 0xBB, 0xCC),
        pci::PciAddress pciAddr = pci::PciAddress(0, 3, 0)
    ) : name_(name),
        friendlyName_(friendlyName),
        mac_(mac),
        pciAddr_(pciAddr),
        mtu_(DEFAULT_MTU),
        speedBps_(1'000'000'000ULL), // 1 Gbps default
        linkState_(MediaConnectState::Connected)
    {
        mmioRegisters_.resize(128 * 1024 / 4, 0); // 128 KB MMIO
        writeMmio32(REG_STATUS, 0x8000000A);     // Full Duplex, 1000Mbps, Link Up
    }

    [[nodiscard]] const std::wstring& getAdapterName() const noexcept override { return name_; }
    [[nodiscard]] const std::wstring& getFriendlyName() const noexcept override { return friendlyName_; }
    [[nodiscard]] MacAddress getMacAddress() const noexcept override { return mac_; }
    [[nodiscard]] uint32_t getMtu() const noexcept override { return mtu_; }
    [[nodiscard]] uint64_t getSpeedBps() const noexcept override { return speedBps_; }
    [[nodiscard]] MediaConnectState getLinkState() const noexcept override { return linkState_; }
    [[nodiscard]] pci::PciAddress getPciAddress() const noexcept { return pciAddr_; }

    [[nodiscard]] AdapterStatistics getStatistics() const noexcept override {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }

    void setLinkSpeed(uint64_t speedBps) noexcept { speedBps_ = speedBps; }
    void setMtu(uint32_t mtu) noexcept { mtu_ = std::clamp(mtu, static_cast<uint32_t>(ETH_MIN_LEN), static_cast<uint32_t>(JUMBO_MTU)); }

    void setLinkState(MediaConnectState state) noexcept {
        linkState_ = state;
        linkStateChangeCount_++;
        writeMmio32(REG_STATUS, (state == MediaConnectState::Connected) ? 0x8000000A : 0x00000000);
    }

    [[nodiscard]] uint32_t getLinkStateChangeCount() const noexcept { return linkStateChangeCount_; }

    bool resetHardware() {
        std::lock_guard<std::mutex> lock(mutex_);
        resetCount_++;
        writeMmio32(REG_CTRL, 0x04000000); // Master Device Reset
        writeMmio32(REG_STATUS, 0x8000000A); // Link back up
        return true;
    }

    [[nodiscard]] uint32_t getResetCount() const noexcept { return resetCount_; }

    void registerReceiveHandler(ReceiveCallback callback) override {
        std::lock_guard<std::mutex> lock(mutex_);
        rxCallback_ = std::move(callback);
    }

    [[nodiscard]] uint32_t readMmio32(uint32_t offset) const noexcept {
        size_t idx = offset / 4;
        if (idx < mmioRegisters_.size()) return mmioRegisters_[idx];
        return 0xFFFFFFFF;
    }

    void writeMmio32(uint32_t offset, uint32_t value) noexcept {
        size_t idx = offset / 4;
        if (idx < mmioRegisters_.size()) mmioRegisters_[idx] = value;
    }

    [[nodiscard]] NtStatus sendPacket(std::span<const uint8_t> frame) override {
        if (frame.size() < ETH_HLEN) return NtStatus::InvalidParameter;
        if (linkState_ != MediaConnectState::Connected) return NtStatus::DeviceNotReady;
        std::lock_guard<std::mutex> lock(mutex_);
        stats_.txPackets++;
        stats_.txBytes += frame.size();
        return NtStatus::Success;
    }

    void receivePacket(std::span<const uint8_t> frame) {
        if (frame.size() < ETH_HLEN) return;
        ReceiveCallback cb;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stats_.rxPackets++;
            stats_.rxBytes += frame.size();
            cb = rxCallback_;
        }
        if (cb) cb(frame);
    }

private:
    std::wstring name_;
    std::wstring friendlyName_;
    MacAddress mac_;
    pci::PciAddress pciAddr_;
    uint32_t mtu_{DEFAULT_MTU};
    uint64_t speedBps_{1'000'000'000ULL};
    MediaConnectState linkState_{MediaConnectState::Connected};
    uint32_t linkStateChangeCount_{0};
    uint32_t resetCount_{0};
    mutable std::mutex mutex_;
    AdapterStatistics stats_{};
    ReceiveCallback rxCallback_;
    std::vector<uint32_t> mmioRegisters_;
};

// ============================================================================
// 10B. VirtIO-Net Paravirtualized Network Adapter (virtio_net.sys)
// ============================================================================

class VirtIoNetAdapter : public INdisAdapter {
public:
    explicit VirtIoNetAdapter(
        std::wstring_view name = L"\\Device\\VirtIoNet0",
        std::wstring_view friendlyName = L"Red Hat VirtIO Ethernet Adapter",
        MacAddress mac = MacAddress(0x52, 0x54, 0x00, 0x12, 0x34, 0x56),
        pci::PciAddress pciAddr = pci::PciAddress(0, 5, 0)
    ) : name_(name),
        friendlyName_(friendlyName),
        mac_(mac),
        pciAddr_(pciAddr),
        mtu_(DEFAULT_MTU),
        speedBps_(10'000'000'000ULL),
        linkState_(MediaConnectState::Connected) {}

    [[nodiscard]] const std::wstring& getAdapterName() const noexcept override { return name_; }
    [[nodiscard]] const std::wstring& getFriendlyName() const noexcept override { return friendlyName_; }
    [[nodiscard]] MacAddress getMacAddress() const noexcept override { return mac_; }
    [[nodiscard]] uint32_t getMtu() const noexcept override { return mtu_; }
    [[nodiscard]] uint64_t getSpeedBps() const noexcept override { return speedBps_; }
    [[nodiscard]] MediaConnectState getLinkState() const noexcept override { return linkState_; }
    [[nodiscard]] pci::PciAddress getPciAddress() const noexcept { return pciAddr_; }

    [[nodiscard]] AdapterStatistics getStatistics() const noexcept override {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }

    void setMtu(uint32_t mtu) noexcept { mtu_ = mtu; }
    void setLinkState(MediaConnectState state) noexcept {
        linkState_ = state;
        linkStateChangeCount_++;
    }
    [[nodiscard]] uint32_t getLinkStateChangeCount() const noexcept { return linkStateChangeCount_; }

    void registerReceiveHandler(ReceiveCallback callback) override {
        std::lock_guard<std::mutex> lock(mutex_);
        rxCallback_ = std::move(callback);
    }

    [[nodiscard]] NtStatus sendPacket(std::span<const uint8_t> frame) override {
        if (frame.size() < ETH_HLEN) return NtStatus::InvalidParameter;
        if (linkState_ != MediaConnectState::Connected) return NtStatus::DeviceNotReady;
        std::lock_guard<std::mutex> lock(mutex_);
        stats_.txPackets++;
        stats_.txBytes += frame.size();
        return NtStatus::Success;
    }

    void receivePacket(std::span<const uint8_t> frame) {
        if (frame.size() < ETH_HLEN) return;
        ReceiveCallback cb;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stats_.rxPackets++;
            stats_.rxBytes += frame.size();
            cb = rxCallback_;
        }
        if (cb) cb(frame);
    }

private:
    std::wstring name_;
    std::wstring friendlyName_;
    MacAddress mac_;
    pci::PciAddress pciAddr_;
    uint32_t mtu_{DEFAULT_MTU};
    uint64_t speedBps_{10'000'000'000ULL};
    MediaConnectState linkState_{MediaConnectState::Connected};
    uint32_t linkStateChangeCount_{0};
    mutable std::mutex mutex_;
    AdapterStatistics stats_{};
    ReceiveCallback rxCallback_;
};

// ============================================================================
// 10C. NetAdapterCx (Network Adapter WDF Class Extension) Definitions
// ============================================================================

struct NET_FRAGMENT {
    uint64_t OsReserved1{0};
    uint64_t OsReserved2{0};
    uint64_t PhysicalAddress{0};
    uint32_t Capacity{2048};
    uint32_t Offset{0};
    uint32_t ValidLength{0};
    bool     EndOfPacket{true};
};

struct NET_PACKET {
    uint32_t FragmentIndex{0};
    uint16_t FragmentCount{1};
    uint16_t LayoutFlags{0};
    uint32_t Index{0};
    void*    OsReserved{nullptr};
};

struct NET_RING_BUFFER {
    uint32_t ElementSize{0};
    uint32_t NumberOfElements{256};
    uint32_t ElementIndexMask{255};
    uint32_t BeginIndex{0};
    uint32_t NextIndex{0};
    uint32_t EndIndex{0};
    std::vector<NET_PACKET> packets;
    std::vector<NET_FRAGMENT> fragments;

    explicit NET_RING_BUFFER(uint32_t numElements = 256)
        : NumberOfElements(numElements), ElementIndexMask(numElements - 1) {
        packets.resize(numElements);
        fragments.resize(numElements);
        for (uint32_t i = 0; i < numElements; ++i) {
            packets[i].Index = i;
            packets[i].FragmentIndex = i;
            packets[i].FragmentCount = 1;
            fragments[i].Capacity = 2048;
        }
    }
};

class NetTxQueue {
    uint32_t queueId_{0};
    std::unique_ptr<NET_RING_BUFFER> ringBuffer_;
    std::atomic<uint64_t> packetsPosted_{0};
    std::atomic<uint64_t> packetsCompleted_{0};

public:
    explicit NetTxQueue(uint32_t queueId, uint32_t ringSize = 256)
        : queueId_(queueId), ringBuffer_(std::make_unique<NET_RING_BUFFER>(ringSize)) {}

    [[nodiscard]] uint32_t getQueueId() const noexcept { return queueId_; }
    [[nodiscard]] NET_RING_BUFFER* getRingBuffer() noexcept { return ringBuffer_.get(); }
    [[nodiscard]] const NET_RING_BUFFER* getRingBuffer() const noexcept { return ringBuffer_.get(); }
    [[nodiscard]] uint64_t getPacketsPosted() const noexcept { return packetsPosted_.load(); }
    [[nodiscard]] uint64_t getPacketsCompleted() const noexcept { return packetsCompleted_.load(); }

    void advanceRing(uint32_t count) {
        packetsPosted_ += count;
        packetsCompleted_ += count;
        ringBuffer_->BeginIndex = (ringBuffer_->BeginIndex + count) & ringBuffer_->ElementIndexMask;
        ringBuffer_->NextIndex = ringBuffer_->BeginIndex;
        ringBuffer_->EndIndex = (ringBuffer_->EndIndex + count) & ringBuffer_->ElementIndexMask;
    }
};

class NetRxQueue {
    uint32_t queueId_{0};
    std::unique_ptr<NET_RING_BUFFER> ringBuffer_;
    std::atomic<uint64_t> packetsReceived_{0};

public:
    explicit NetRxQueue(uint32_t queueId, uint32_t ringSize = 256)
        : queueId_(queueId), ringBuffer_(std::make_unique<NET_RING_BUFFER>(ringSize)) {}

    [[nodiscard]] uint32_t getQueueId() const noexcept { return queueId_; }
    [[nodiscard]] NET_RING_BUFFER* getRingBuffer() noexcept { return ringBuffer_.get(); }
    [[nodiscard]] const NET_RING_BUFFER* getRingBuffer() const noexcept { return ringBuffer_.get(); }
    [[nodiscard]] uint64_t getPacketsReceived() const noexcept { return packetsReceived_.load(); }

    void postReceivedPackets(uint32_t count) {
        packetsReceived_ += count;
        ringBuffer_->NextIndex = (ringBuffer_->NextIndex + count) & ringBuffer_->ElementIndexMask;
    }
};

class NetAdapterContext {
    uint32_t adapterId_{0};
    std::wstring name_;
    MacAddress mac_;
    uint32_t mtu_{DEFAULT_MTU};
    uint64_t speedBps_{2'500'000'000ULL}; // 2.5 Gbps default for NetAdapterCx
    MediaConnectState linkState_{MediaConnectState::Connected};
    std::vector<std::shared_ptr<NetTxQueue>> txQueues_;
    std::vector<std::shared_ptr<NetRxQueue>> rxQueues_;
    mutable std::mutex mutex_;

public:
    NetAdapterContext(uint32_t id, std::wstring_view name, MacAddress mac)
        : adapterId_(id), name_(name), mac_(mac) {}

    [[nodiscard]] uint32_t getId() const noexcept { return adapterId_; }
    [[nodiscard]] const std::wstring& getName() const noexcept { return name_; }
    [[nodiscard]] MacAddress getMacAddress() const noexcept { return mac_; }
    [[nodiscard]] uint32_t getMtu() const noexcept { return mtu_; }
    void setMtu(uint32_t mtu) noexcept { mtu_ = mtu; }
    [[nodiscard]] uint64_t getSpeedBps() const noexcept { return speedBps_; }
    void setSpeedBps(uint64_t spd) noexcept { speedBps_ = spd; }
    [[nodiscard]] MediaConnectState getLinkState() const noexcept { return linkState_; }
    void setLinkState(MediaConnectState st) noexcept { linkState_ = st; }

    uint32_t createTxQueue(uint32_t ringSize = 256) {
        std::lock_guard<std::mutex> lock(mutex_);
        uint32_t qId = static_cast<uint32_t>(txQueues_.size());
        txQueues_.push_back(std::make_shared<NetTxQueue>(qId, ringSize));
        return qId;
    }

    uint32_t createRxQueue(uint32_t ringSize = 256) {
        std::lock_guard<std::mutex> lock(mutex_);
        uint32_t qId = static_cast<uint32_t>(rxQueues_.size());
        rxQueues_.push_back(std::make_shared<NetRxQueue>(qId, ringSize));
        return qId;
    }

    std::shared_ptr<NetTxQueue> getTxQueue(uint32_t qId) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (qId < txQueues_.size()) return txQueues_[qId];
        return nullptr;
    }

    std::shared_ptr<NetRxQueue> getRxQueue(uint32_t qId) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (qId < rxQueues_.size()) return rxQueues_[qId];
        return nullptr;
    }

    [[nodiscard]] size_t getTxQueueCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return txQueues_.size();
    }

    [[nodiscard]] size_t getRxQueueCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return rxQueues_.size();
    }
};

struct NDIS_TELEMETRY {
    uint64_t totalAdaptersEnumerated{0};
    uint64_t totalPacketsTransmitted{0};
    uint64_t totalBytesTransmitted{0};
    uint64_t totalPacketsReceived{0};
    uint64_t totalBytesReceived{0};
    uint64_t totalLsoSegmentsCreated{0};
    uint64_t totalRscSegmentsCoalesced{0};
    uint64_t totalCsumTxOffloaded{0};
    uint64_t totalCsumRxValidated{0};
    uint64_t totalNblAllocations{0};
    uint64_t totalNblFreed{0};
    uint64_t totalLinkStateChanges{0};
    uint64_t totalHardwareResets{0};
    uint64_t totalNetAdapterQueuesCreated{0};
    uint64_t totalNetPacketsProcessed{0};
};

// ============================================================================
// ============================================================================
// 11. Sovereign TitanNDIS Subsystem (TitanNdisSubsystem)
// ============================================================================

/**
 * @brief Singleton Managing All NDIS Miniport and Protocol Drivers.
 */
class TitanNdisSubsystem {
public:
    static TitanNdisSubsystem& Instance() {
        static TitanNdisSubsystem instance;
        return instance;
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (initialized_) return;

        nblPool_ = std::make_unique<NetBufferListPool>(512);

        // 1. Probe & instantiate RazzleNet 10GbE PCIe Miniport Adapter (00:04.0)
        auto razzle = std::make_shared<RazzleNetAdapter>(
            L"\\Device\\RazzleNet0",
            L"RazzleNet 10-Gigabit Ethernet Adapter (Titan 10G-SR)",
            MacAddress::razzleNetDefault(),
            pci::PciAddress(0, 4, 0)
        );
        adapters_[L"razzlenet0"] = razzle;
        primaryAdapter_ = razzle;

        // 2. Instantiate Sovereign Virtual Ethernet Adapter (Loopback)
        auto vnet = std::make_shared<VirtualNetworkAdapter>(
            L"\\Device\\MicaNetVirtual",
            L"MicaNT Sovereign Virtual Ethernet Adapter",
            MacAddress::defaultMica(),
            DEFAULT_MTU,
            10'000'000'000ULL
        );
        adapters_[L"vnet0"] = vnet;

        // 3. Instantiate Intel E1000e Gigabit Ethernet Adapter (00:03.0)
        auto e1000 = std::make_shared<IntelE1000eAdapter>(
            L"\\Device\\E1000e0",
            L"Intel 82574L Gigabit Network Connection",
            MacAddress(0x00, 0x1B, 0x21, 0xAA, 0xBB, 0xCC),
            pci::PciAddress(0, 3, 0)
        );
        adapters_[L"e1000e0"] = e1000;

        // 4. Instantiate Red Hat VirtIO Ethernet Adapter (00:05.0)
        auto virtio = std::make_shared<VirtIoNetAdapter>(
            L"\\Device\\VirtIoNet0",
            L"Red Hat VirtIO Ethernet Adapter",
            MacAddress(0x52, 0x54, 0x00, 0x12, 0x34, 0x56),
            pci::PciAddress(0, 5, 0)
        );
        adapters_[L"virtionet0"] = virtio;

        // 5. Instantiate initial NetAdapterCx Context
        auto netAdp0 = std::make_shared<NetAdapterContext>(
            nextNetAdapterId_++,
            L"\\Device\\NetAdapter0",
            MacAddress(0x00, 0xE0, 0x4C, 0x11, 0x22, 0x33)
        );
        netAdp0->createTxQueue(256);
        netAdp0->createRxQueue(256);
        netAdapters_[L"netadapter0"] = netAdp0;

        initialized_ = true;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        adapters_.clear();
        netAdapters_.clear();
        primaryAdapter_.reset();
        nblPool_.reset();
        initialized_ = false;
    }

    [[nodiscard]] bool isInitialized() const noexcept {
        return initialized_;
    }

    [[nodiscard]] std::shared_ptr<INdisAdapter> getPrimaryAdapter() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return primaryAdapter_;
    }

    void setPrimaryAdapter(std::shared_ptr<INdisAdapter> adapter) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        primaryAdapter_ = adapter;
    }

    [[nodiscard]] std::shared_ptr<INdisAdapter> getAdapter(std::wstring_view name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = adapters_.find(std::wstring(name));
        if (it != adapters_.end()) return it->second;
        return nullptr;
    }

    [[nodiscard]] std::shared_ptr<INdisAdapter> getAdapterByIndex(size_t index) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (index >= adapters_.size()) return nullptr;
        auto it = adapters_.begin();
        std::advance(it, index);
        return it->second;
    }

    [[nodiscard]] size_t getAdapterCount() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return adapters_.size();
    }

    [[nodiscard]] std::vector<std::shared_ptr<INdisAdapter>> getAllAdapters() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::shared_ptr<INdisAdapter>> list;
        list.reserve(adapters_.size());
        for (const auto& [name, adp] : adapters_) {
            list.push_back(adp);
        }
        return list;
    }

    void registerAdapter(std::wstring_view name, std::shared_ptr<INdisAdapter> adapter) {
        std::lock_guard<std::mutex> lock(mutex_);
        adapters_[std::wstring(name)] = adapter;
        if (!primaryAdapter_) primaryAdapter_ = adapter;
    }

    // NetAdapterCx context management
    std::shared_ptr<NetAdapterContext> createNetAdapter(
        std::wstring_view name,
        MacAddress mac = MacAddress(0x00, 0xE0, 0x4C, 0x11, 0x22, 0x33)
    ) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto adp = std::make_shared<NetAdapterContext>(nextNetAdapterId_++, name, mac);
        netAdapters_[std::wstring(name)] = adp;
        return adp;
    }

    [[nodiscard]] std::shared_ptr<NetAdapterContext> getNetAdapter(std::wstring_view name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = netAdapters_.find(std::wstring(name));
        if (it != netAdapters_.end()) return it->second;
        return nullptr;
    }

    [[nodiscard]] std::shared_ptr<NetAdapterContext> getNetAdapterById(uint32_t id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [name, adp] : netAdapters_) {
            if (adp && adp->getId() == id) return adp;
        }
        return nullptr;
    }

    [[nodiscard]] size_t getNetAdapterCount() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return netAdapters_.size();
    }

    [[nodiscard]] std::vector<std::shared_ptr<NetAdapterContext>> getAllNetAdapters() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::shared_ptr<NetAdapterContext>> list;
        list.reserve(netAdapters_.size());
        for (const auto& [name, adp] : netAdapters_) {
            list.push_back(adp);
        }
        return list;
    }

    [[nodiscard]] NetBufferListPool* getNblPool() const noexcept {
        return nblPool_.get();
    }

    void recordLsoSegments(uint64_t count) noexcept { totalLsoSegments_ += count; }
    void recordRscSegments(uint64_t count) noexcept { totalRscSegments_ += count; }
    void recordCsumTx(uint64_t count) noexcept { totalCsumTx_ += count; }
    void recordCsumRx(uint64_t count) noexcept { totalCsumRx_ += count; }

    [[nodiscard]] NDIS_TELEMETRY getTelemetry() const {
        std::lock_guard<std::mutex> lock(mutex_);
        NDIS_TELEMETRY telem{};
        telem.totalAdaptersEnumerated = adapters_.size() + netAdapters_.size();
        for (const auto& [name, adp] : adapters_) {
            if (adp) {
                auto stats = adp->getStatistics();
                telem.totalPacketsTransmitted += stats.txPackets;
                telem.totalBytesTransmitted += stats.txBytes;
                telem.totalPacketsReceived += stats.rxPackets;
                telem.totalBytesReceived += stats.rxBytes;

                auto razzle = std::dynamic_pointer_cast<RazzleNetAdapter>(adp);
                if (razzle) {
                    telem.totalLinkStateChanges += razzle->getLinkStateChangeCount();
                    telem.totalHardwareResets += razzle->getResetCount();
                    telem.totalLsoSegmentsCreated += razzle->getStatistics().lsoPackets;
                    telem.totalCsumTxOffloaded += razzle->getStatistics().csumOffloadTx;
                    telem.totalCsumRxValidated += razzle->getStatistics().csumOffloadRx;
                }
                auto vnet = std::dynamic_pointer_cast<VirtualNetworkAdapter>(adp);
                if (vnet) {
                    telem.totalLinkStateChanges += vnet->getLinkStateChangeCount();
                }
                auto virtio = std::dynamic_pointer_cast<VirtIoNetAdapter>(adp);
                if (virtio) {
                    telem.totalLinkStateChanges += virtio->getLinkStateChangeCount();
                }
                auto e1000 = std::dynamic_pointer_cast<IntelE1000eAdapter>(adp);
                if (e1000) {
                    telem.totalLinkStateChanges += e1000->getLinkStateChangeCount();
                }
            }
        }
        telem.totalLsoSegmentsCreated += totalLsoSegments_.load();
        telem.totalRscSegmentsCoalesced += totalRscSegments_.load();
        telem.totalCsumTxOffloaded += totalCsumTx_.load();
        telem.totalCsumRxValidated += totalCsumRx_.load();
        if (nblPool_) {
            telem.totalNblAllocations = nblPool_->getTotalAllocations();
            telem.totalNblFreed = nblPool_->getTotalFreed();
        }
        for (const auto& [name, adp] : netAdapters_) {
            if (adp) {
                telem.totalNetAdapterQueuesCreated += (adp->getTxQueueCount() + adp->getRxQueueCount());
                for (size_t i = 0; i < adp->getTxQueueCount(); ++i) {
                    auto q = adp->getTxQueue(static_cast<uint32_t>(i));
                    if (q) telem.totalNetPacketsProcessed += q->getPacketsPosted();
                }
                for (size_t i = 0; i < adp->getRxQueueCount(); ++i) {
                    auto q = adp->getRxQueue(static_cast<uint32_t>(i));
                    if (q) telem.totalNetPacketsProcessed += q->getPacketsReceived();
                }
            }
        }
        return telem;
    }

    void resetTelemetry() noexcept {
        totalLsoSegments_ = 0;
        totalRscSegments_ = 0;
        totalCsumTx_ = 0;
        totalCsumRx_ = 0;
    }

    void resetForTesting() {
        shutdown();
        initialize();
        resetTelemetry();
    }

private:
    TitanNdisSubsystem() = default;
    ~TitanNdisSubsystem() = default;
    TitanNdisSubsystem(const TitanNdisSubsystem&) = delete;
    TitanNdisSubsystem& operator=(const TitanNdisSubsystem&) = delete;

    bool initialized_{false};
    mutable std::mutex mutex_;
    std::shared_ptr<INdisAdapter> primaryAdapter_;
    std::map<std::wstring, std::shared_ptr<INdisAdapter>> adapters_;
    std::map<std::wstring, std::shared_ptr<NetAdapterContext>> netAdapters_;
    uint32_t nextNetAdapterId_{0};
    std::unique_ptr<NetBufferListPool> nblPool_;
    std::atomic<uint64_t> totalLsoSegments_{0};
    std::atomic<uint64_t> totalRscSegments_{0};
    std::atomic<uint64_t> totalCsumTx_{0};
    std::atomic<uint64_t> totalCsumRx_{0};
};

// ============================================================================
// 12. Helper Utility Functions
// ============================================================================

/**
 * @brief Helper to wrap payload in an Ethernet II frame.
 */
inline std::vector<uint8_t> buildEthernetFrame(
    MacAddress destMac,
    MacAddress srcMac,
    uint16_t etherType,
    std::span<const uint8_t> payload
) {
    std::vector<uint8_t> frame(ETH_HLEN + payload.size());
    auto* hdr = reinterpret_cast<EthernetHeader*>(frame.data());
    hdr->destMac = destMac;
    hdr->srcMac = srcMac;
    hdr->setEtherType(etherType);
    if (!payload.empty()) {
        std::memcpy(frame.data() + ETH_HLEN, payload.data(), payload.size());
    }
    return frame;
}

// ============================================================================
// 13. NDIS & RazzleNet C ABI Export Thunks (ndis.sys / razzlenet.sys / netadaptercx.sys / e1000e.sys)
// ============================================================================

#define NDIS_EXPORT

extern "C" {

inline int32_t WINAPI NdisMRegisterMiniportDriver(
    void* /*driverObject*/,
    void* /*registryPath*/,
    void* /*miniportCharacteristics*/,
    void** driverHandle
) {
    if (driverHandle) *driverHandle = reinterpret_cast<void*>(0xDEADBEEF00000001ULL);
    return static_cast<int32_t>(NDIS_STATUS_SUCCESS);
}

inline int32_t WINAPI NdisMDeregisterMiniportDriver(void* /*driverHandle*/) {
    return static_cast<int32_t>(NDIS_STATUS_SUCCESS);
}

inline int32_t WINAPI NdisMSetMiniportAttributes(
    void* /*miniportAdapterHandle*/,
    void* /*miniportAttributes*/
) {
    return static_cast<int32_t>(NDIS_STATUS_SUCCESS);
}

inline void* WINAPI NdisAllocateNetBufferListPool(
    void* /*nblPoolHandle*/,
    void* /*parameters*/
) {
    auto& ndis = TitanNdisSubsystem::Instance();
    if (!ndis.isInitialized()) ndis.initialize();
    return ndis.getNblPool();
}

inline void WINAPI NdisFreeNetBufferListPool(void* /*poolHandle*/) {
    // Pool lifetime managed by TitanNdisSubsystem
}

inline void* WINAPI NdisAllocateNetBufferList(
    void* poolHandle,
    uint16_t /*contextSize*/,
    uint16_t /*contextBackFill*/
) {
    auto* pool = reinterpret_cast<NetBufferListPool*>(poolHandle);
    if (!pool) {
        auto& ndis = TitanNdisSubsystem::Instance();
        if (!ndis.isInitialized()) ndis.initialize();
        pool = ndis.getNblPool();
    }
    if (!pool) return nullptr;

    auto nbl = pool->allocate();
    // Return heap pointer to shared_ptr wrapper
    return new std::shared_ptr<NetBufferList>(nbl);
}

inline void WINAPI NdisFreeNetBufferList(void* nblHandle) {
    if (!nblHandle) return;
    auto* spNbl = reinterpret_cast<std::shared_ptr<NetBufferList>*>(nblHandle);
    auto& ndis = TitanNdisSubsystem::Instance();
    if (ndis.isInitialized() && ndis.getNblPool() && *spNbl) {
        ndis.getNblPool()->free(*spNbl);
    }
    delete spNbl;
}

inline int32_t WINAPI NdisMIndicateReceiveNetBufferLists(
    void* /*miniportAdapterHandle*/,
    void* nblHandle,
    uint32_t /*portNumber*/,
    uint32_t /*numberOfNetBufferLists*/,
    uint32_t /*receiveFlags*/
) {
    if (!nblHandle) return static_cast<int32_t>(NDIS_STATUS_INVALID_PARAMETER);
    auto* spNbl = reinterpret_cast<std::shared_ptr<NetBufferList>*>(nblHandle);
    if (!spNbl || !*spNbl) return static_cast<int32_t>(NDIS_STATUS_INVALID_PARAMETER);

    (*spNbl)->status = NDIS_STATUS_SUCCESS;
    return static_cast<int32_t>(NDIS_STATUS_SUCCESS);
}

inline int32_t WINAPI NdisMSendNetBufferListsComplete(
    void* /*miniportAdapterHandle*/,
    void* nblHandle,
    uint32_t /*sendCompleteFlags*/
) {
    if (!nblHandle) return static_cast<int32_t>(NDIS_STATUS_INVALID_PARAMETER);
    auto* spNbl = reinterpret_cast<std::shared_ptr<NetBufferList>*>(nblHandle);
    if (!spNbl || !*spNbl) return static_cast<int32_t>(NDIS_STATUS_INVALID_PARAMETER);

    (*spNbl)->status = NDIS_STATUS_SUCCESS;
    return static_cast<int32_t>(NDIS_STATUS_SUCCESS);
}

inline int32_t WINAPI NdisQueryAdapterInformation(
    uint32_t adapterIndex,
    uint32_t* outMtu,
    uint64_t* outSpeedBps,
    uint32_t* outLinkState
) {
    auto& ndis = TitanNdisSubsystem::Instance();
    if (!ndis.isInitialized()) ndis.initialize();

    auto adp = ndis.getAdapterByIndex(adapterIndex);
    if (!adp) return static_cast<int32_t>(NDIS_STATUS_DEVICE_FAILED);

    if (outMtu) *outMtu = adp->getMtu();
    if (outSpeedBps) *outSpeedBps = adp->getSpeedBps();
    if (outLinkState) *outLinkState = static_cast<uint32_t>(adp->getLinkState());

    return static_cast<int32_t>(NDIS_STATUS_SUCCESS);
}

inline int32_t WINAPI NdisMResetMiniport(void* /*miniportAdapterHandle*/) {
    auto& ndis = TitanNdisSubsystem::Instance();
    if (!ndis.isInitialized()) ndis.initialize();
    auto adp = ndis.getPrimaryAdapter();
    if (adp) {
        auto razzle = std::dynamic_pointer_cast<RazzleNetAdapter>(adp);
        if (razzle) {
            razzle->resetHardware();
        }
    }
    return static_cast<int32_t>(NDIS_STATUS_SUCCESS);
}

inline int32_t WINAPI RazzleNetInitializeMiniport(
    void* /*driverObject*/,
    void* /*registryPath*/
) {
    auto& ndis = TitanNdisSubsystem::Instance();
    if (!ndis.isInitialized()) ndis.initialize();
    return static_cast<int32_t>(NDIS_STATUS_SUCCESS);
}

inline int32_t WINAPI RazzleNetTransmitPacket(
    const uint8_t* frameData,
    uint32_t frameLength
) {
    if (!frameData || frameLength < ETH_HLEN) return static_cast<int32_t>(NDIS_STATUS_INVALID_PARAMETER);

    auto& ndis = TitanNdisSubsystem::Instance();
    if (!ndis.isInitialized()) ndis.initialize();

    auto adp = ndis.getPrimaryAdapter();
    if (!adp) return static_cast<int32_t>(NDIS_STATUS_DEVICE_FAILED);

    auto status = adp->sendPacket(std::span<const uint8_t>(frameData, frameLength));
    return (status == NtStatus::Success) ? static_cast<int32_t>(NDIS_STATUS_SUCCESS)
                                         : static_cast<int32_t>(NDIS_STATUS_FAILURE);
}

inline int32_t WINAPI NetAdapterCreate(
    void* /*adapterInit*/,
    void* /*objectAttributes*/,
    void** netAdapterHandle
) {
    auto& ndis = TitanNdisSubsystem::Instance();
    if (!ndis.isInitialized()) ndis.initialize();
    auto adp = ndis.createNetAdapter(L"NetAdapterWdf" + std::to_wstring(ndis.getNetAdapterCount()));
    if (netAdapterHandle && adp) {
        *netAdapterHandle = adp.get();
        return static_cast<int32_t>(NDIS_STATUS_SUCCESS);
    }
    return static_cast<int32_t>(NDIS_STATUS_FAILURE);
}

inline int32_t WINAPI NetAdapterStart(void* adapterHandle) {
    if (!adapterHandle) return static_cast<int32_t>(NDIS_STATUS_INVALID_PARAMETER);
    auto* adp = reinterpret_cast<NetAdapterContext*>(adapterHandle);
    adp->setLinkState(MediaConnectState::Connected);
    return static_cast<int32_t>(NDIS_STATUS_SUCCESS);
}

inline int32_t WINAPI NetAdapterStop(void* adapterHandle) {
    if (!adapterHandle) return static_cast<int32_t>(NDIS_STATUS_INVALID_PARAMETER);
    auto* adp = reinterpret_cast<NetAdapterContext*>(adapterHandle);
    adp->setLinkState(MediaConnectState::Disconnected);
    return static_cast<int32_t>(NDIS_STATUS_SUCCESS);
}

inline int32_t WINAPI NetTxQueueCreate(
    void* netAdapterHandle,
    void* /*queueConfig*/,
    void* /*objectAttributes*/,
    void** txQueueHandle
) {
    if (!netAdapterHandle || !txQueueHandle) return static_cast<int32_t>(NDIS_STATUS_INVALID_PARAMETER);
    auto* adp = reinterpret_cast<NetAdapterContext*>(netAdapterHandle);
    uint32_t qId = adp->createTxQueue();
    auto queue = adp->getTxQueue(qId);
    if (queue) {
        *txQueueHandle = queue.get();
        return static_cast<int32_t>(NDIS_STATUS_SUCCESS);
    }
    return static_cast<int32_t>(NDIS_STATUS_FAILURE);
}

inline int32_t WINAPI NetRxQueueCreate(
    void* netAdapterHandle,
    void* /*queueConfig*/,
    void* /*objectAttributes*/,
    void** rxQueueHandle
) {
    if (!netAdapterHandle || !rxQueueHandle) return static_cast<int32_t>(NDIS_STATUS_INVALID_PARAMETER);
    auto* adp = reinterpret_cast<NetAdapterContext*>(netAdapterHandle);
    uint32_t qId = adp->createRxQueue();
    auto queue = adp->getRxQueue(qId);
    if (queue) {
        *rxQueueHandle = queue.get();
        return static_cast<int32_t>(NDIS_STATUS_SUCCESS);
    }
    return static_cast<int32_t>(NDIS_STATUS_FAILURE);
}

inline void WINAPI NetTxQueueNotifyMoreCompletedPacketsAvailable(void* txQueueHandle) {
    if (!txQueueHandle) return;
    auto* q = reinterpret_cast<NetTxQueue*>(txQueueHandle);
    q->advanceRing(1);
}

inline void WINAPI NetRxQueueNotifyMoreReceivedPacketsAvailable(void* rxQueueHandle) {
    if (!rxQueueHandle) return;
    auto* q = reinterpret_cast<NetRxQueue*>(rxQueueHandle);
    q->postReceivedPackets(1);
}

inline int32_t WINAPI E1000eInitializeMiniport(
    void* /*driverObject*/,
    void* /*registryPath*/
) {
    auto& ndis = TitanNdisSubsystem::Instance();
    if (!ndis.isInitialized()) ndis.initialize();
    return static_cast<int32_t>(NDIS_STATUS_SUCCESS);
}

} // extern "C"

// C++ Telemetry helpers outside extern "C"
inline NDIS_TELEMETRY NdisQueryTelemetry() {
    auto& ndis = TitanNdisSubsystem::Instance();
    if (!ndis.isInitialized()) ndis.initialize();
    return ndis.getTelemetry();
}

inline void NdisResetTelemetryForTesting() {
    auto& ndis = TitanNdisSubsystem::Instance();
    ndis.resetTelemetry();
}

// ============================================================================
// 14. Subsystem Initialization & Registration
// ============================================================================

inline void InitializeNdisSubsystem() {
    // 1. Initialize Subsystem & Register Adapters
    TitanNdisSubsystem::Instance().initialize();

    // 2. Register Driver Exports in JanusLDR Dynamic Loader
    auto& ldr = ldr::DynamicLoader::get();
    ldr.registerExport("ndis.sys", "NdisMRegisterMiniportDriver", reinterpret_cast<void*>(NdisMRegisterMiniportDriver));
    ldr.registerExport("ndis.sys", "NdisMDeregisterMiniportDriver", reinterpret_cast<void*>(NdisMDeregisterMiniportDriver));
    ldr.registerExport("ndis.sys", "NdisMSetMiniportAttributes", reinterpret_cast<void*>(NdisMSetMiniportAttributes));
    ldr.registerExport("ndis.sys", "NdisAllocateNetBufferListPool", reinterpret_cast<void*>(NdisAllocateNetBufferListPool));
    ldr.registerExport("ndis.sys", "NdisFreeNetBufferListPool", reinterpret_cast<void*>(NdisFreeNetBufferListPool));
    ldr.registerExport("ndis.sys", "NdisAllocateNetBufferList", reinterpret_cast<void*>(NdisAllocateNetBufferList));
    ldr.registerExport("ndis.sys", "NdisFreeNetBufferList", reinterpret_cast<void*>(NdisFreeNetBufferList));
    ldr.registerExport("ndis.sys", "NdisMIndicateReceiveNetBufferLists", reinterpret_cast<void*>(NdisMIndicateReceiveNetBufferLists));
    ldr.registerExport("ndis.sys", "NdisMSendNetBufferListsComplete", reinterpret_cast<void*>(NdisMSendNetBufferListsComplete));
    ldr.registerExport("ndis.sys", "NdisQueryAdapterInformation", reinterpret_cast<void*>(NdisQueryAdapterInformation));
    ldr.registerExport("ndis.sys", "NdisMResetMiniport", reinterpret_cast<void*>(NdisMResetMiniport));

    ldr.registerExport("razzlenet.sys", "RazzleNetInitializeMiniport", reinterpret_cast<void*>(RazzleNetInitializeMiniport));
    ldr.registerExport("razzlenet.sys", "RazzleNetTransmitPacket", reinterpret_cast<void*>(RazzleNetTransmitPacket));

    ldr.registerExport("netadaptercx.sys", "NetAdapterCreate", reinterpret_cast<void*>(NetAdapterCreate));
    ldr.registerExport("netadaptercx.sys", "NetAdapterStart", reinterpret_cast<void*>(NetAdapterStart));
    ldr.registerExport("netadaptercx.sys", "NetAdapterStop", reinterpret_cast<void*>(NetAdapterStop));
    ldr.registerExport("netadaptercx.sys", "NetTxQueueCreate", reinterpret_cast<void*>(NetTxQueueCreate));
    ldr.registerExport("netadaptercx.sys", "NetRxQueueCreate", reinterpret_cast<void*>(NetRxQueueCreate));
    ldr.registerExport("netadaptercx.sys", "NetTxQueueNotifyMoreCompletedPacketsAvailable", reinterpret_cast<void*>(NetTxQueueNotifyMoreCompletedPacketsAvailable));
    ldr.registerExport("netadaptercx.sys", "NetRxQueueNotifyMoreReceivedPacketsAvailable", reinterpret_cast<void*>(NetRxQueueNotifyMoreReceivedPacketsAvailable));

    ldr.registerExport("e1000e.sys", "E1000eInitializeMiniport", reinterpret_cast<void*>(E1000eInitializeMiniport));

    // 3. Register Core Network Drivers in SCM
    auto ndisSvc = std::make_shared<scm::ServiceRecord>();
    ndisSvc->serviceName = L"ndis";
    ndisSvc->displayName = L"Network Driver Interface Specification (NDIS 6.88)";
    ndisSvc->serviceType = scm::SERVICE_KERNEL_DRIVER;
    ndisSvc->startType = scm::SERVICE_BOOT_START;
    ndisSvc->errorControl = scm::SERVICE_ERROR_CRITICAL;
    ndisSvc->binaryPath = L"C:\\Windows\\System32\\drivers\\ndis.sys";
    ndisSvc->status.dwCurrentState = scm::SERVICE_RUNNING;
    scm::ServiceControlManager::get().registerServiceRecord(ndisSvc);

    auto razzleSvc = std::make_shared<scm::ServiceRecord>();
    razzleSvc->serviceName = L"razzlenet";
    razzleSvc->displayName = L"RazzleNet 10GbE/100GbE PCIe Miniport Driver";
    razzleSvc->serviceType = scm::SERVICE_KERNEL_DRIVER;
    razzleSvc->startType = scm::SERVICE_SYSTEM_START;
    razzleSvc->errorControl = scm::SERVICE_ERROR_NORMAL;
    razzleSvc->binaryPath = L"C:\\Windows\\System32\\drivers\\razzlenet.sys";
    razzleSvc->status.dwCurrentState = scm::SERVICE_RUNNING;
    scm::ServiceControlManager::get().registerServiceRecord(razzleSvc);

    auto netadapterSvc = std::make_shared<scm::ServiceRecord>();
    netadapterSvc->serviceName = L"netadaptercx";
    netadapterSvc->displayName = L"Network Adapter WDF Class Extension (NetAdapterCx)";
    netadapterSvc->serviceType = scm::SERVICE_KERNEL_DRIVER;
    netadapterSvc->startType = scm::SERVICE_BOOT_START;
    netadapterSvc->errorControl = scm::SERVICE_ERROR_NORMAL;
    netadapterSvc->binaryPath = L"C:\\Windows\\System32\\drivers\\netadaptercx.sys";
    netadapterSvc->status.dwCurrentState = scm::SERVICE_RUNNING;
    scm::ServiceControlManager::get().registerServiceRecord(netadapterSvc);

    auto e1000Svc = std::make_shared<scm::ServiceRecord>();
    e1000Svc->serviceName = L"e1000e";
    e1000Svc->displayName = L"Intel PRO/1000 Gigabit PCIe Network Driver";
    e1000Svc->serviceType = scm::SERVICE_KERNEL_DRIVER;
    e1000Svc->startType = scm::SERVICE_DEMAND_START;
    e1000Svc->errorControl = scm::SERVICE_ERROR_NORMAL;
    e1000Svc->binaryPath = L"C:\\Windows\\System32\\drivers\\e1000e.sys";
    e1000Svc->status.dwCurrentState = scm::SERVICE_RUNNING;
    scm::ServiceControlManager::get().registerServiceRecord(e1000Svc);

    // 4. Register Version Database Information
    auto& verDb = version::VersionDatabase::Instance();
    verDb.RegisterModule("ndis.sys", "10.0.22621.1", "MicaNT NDIS 6.88 Subsystem Driver");
    verDb.RegisterModule("razzlenet.sys", "10.0.22621.1", "MicaNT RazzleNet 10GbE/100GbE PCIe Miniport Driver");
    verDb.RegisterModule("netadaptercx.sys", "10.0.22621.1", "MicaNT NetAdapter WDF Class Extension");
    verDb.RegisterModule("e1000e.sys", "10.0.22621.1", "MicaNT Intel Gigabit PCIe Miniport Driver");
}

} // namespace micant::ndis

namespace micant {
using ndis::TitanNdisSubsystem;
using ndis::INdisAdapter;
using ndis::VirtualNetworkAdapter;
using ndis::RazzleNetAdapter;
using ndis::IntelE1000eAdapter;
using ndis::VirtIoNetAdapter;
using ndis::NetAdapterContext;
using ndis::NetTxQueue;
using ndis::NetRxQueue;
using ndis::NET_RING_BUFFER;
using ndis::NET_PACKET;
using ndis::NET_FRAGMENT;
using ndis::NDIS_TELEMETRY;
using ndis::NdisQueryTelemetry;
using ndis::NdisResetTelemetryForTesting;
using ndis::InitializeNdisSubsystem;
} // namespace micant
