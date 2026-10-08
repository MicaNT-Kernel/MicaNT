/**
 * @file vmbus.hpp
 * @brief Windows Virtual Machine Bus (VMBus / vmbus.sys) & Hyper-V Synthetic Driver Subsystem
 *
 * MicaNT Dave Cutler Clean-Room Architecture
 * Codename: TitanVMBus / AegisChannel
 * Specification Reference: Microsoft Hypervisor Top-Level Functional Specification (TLFS),
 *                          Windows Hyper-V VMBus Protocol Architecture (vmbus.sys),
 *                          Synthetic Storage (storvsc.sys), Synthetic Network (netvsc.sys),
 *                          Hyper-V Guest-to-Host Sockets (hv_sock / AF_HYPERV),
 *                          Dynamic Memory Ballooning (dmvsc.sys).
 */

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <array>

#include "ntstatus.hpp"
#include "scm.hpp"
#include "version.hpp"

namespace micant::vmbus {

// ----------------------------------------------------------------------------
// Hyper-V Synthetic Device Class GUIDs & Channel Types
// ----------------------------------------------------------------------------
enum class VmbusChannelType : uint32_t {
    Unknown       = 0,
    Storage       = 1, // Synthetic SCSI Storage (storvsc.sys) - {ba6163d9-04a1-4d29-b605-72e2ffb1dc7f}
    Network       = 2, // Synthetic Network Adapter (netvsc.sys) - {f8615163-1236-4328-a213-da5d77b69114}
    HvSocket      = 3, // Hyper-V VM Socket (hv_sock) - {e0762426-32d3-465d-98be-812301980860}
    DynamicMemory = 4, // Dynamic Memory Ballooning (dmvsc.sys) - {525074dc-8985-46e2-8057-a307dc18a502}
    Heartbeat     = 5, // Hyper-V Guest Heartbeat - {57164f39-9115-4e78-ab55-382f3bd5422d}
    KvpExchange   = 6, // Key-Value Pair Data Exchange - {a9a0f4e0-5623-40f7-b9e3-070b165b7004}
    Shutdown      = 7  // Hyper-V Guest Clean Shutdown - {0e0b6031-5213-4934-818b-38d90ced39db}
};

inline const char* VmbusChannelTypeToString(VmbusChannelType type) {
    switch (type) {
        case VmbusChannelType::Storage:       return "Synthetic SCSI Storage (storvsc.sys)";
        case VmbusChannelType::Network:       return "Synthetic Network Adapter (netvsc.sys)";
        case VmbusChannelType::HvSocket:      return "Hyper-V Sockets (hv_sock AF_HYPERV)";
        case VmbusChannelType::DynamicMemory: return "Dynamic Memory Ballooning (dmvsc.sys)";
        case VmbusChannelType::Heartbeat:     return "Hyper-V Guest Heartbeat";
        case VmbusChannelType::KvpExchange:   return "Hyper-V KVP Data Exchange";
        case VmbusChannelType::Shutdown:      return "Hyper-V Guest Clean Shutdown";
        default:                              return "Unknown / Generic Synthetic Device";
    }
}

inline const char* VmbusChannelTypeToGuid(VmbusChannelType type) {
    switch (type) {
        case VmbusChannelType::Storage:       return "{ba6163d9-04a1-4d29-b605-72e2ffb1dc7f}";
        case VmbusChannelType::Network:       return "{f8615163-1236-4328-a213-da5d77b69114}";
        case VmbusChannelType::HvSocket:      return "{e0762426-32d3-465d-98be-812301980860}";
        case VmbusChannelType::DynamicMemory: return "{525074dc-8985-46e2-8057-a307dc18a502}";
        case VmbusChannelType::Heartbeat:     return "{57164f39-9115-4e78-ab55-382f3bd5422d}";
        case VmbusChannelType::KvpExchange:   return "{a9a0f4e0-5623-40f7-b9e3-070b165b7004}";
        case VmbusChannelType::Shutdown:      return "{0e0b6031-5213-4934-818b-38d90ced39db}";
        default:                              return "{00000000-0000-0000-0000-000000000000}";
    }
}

enum class VmbusChannelState : uint32_t {
    Offered   = 0, // Offer received from host partition
    Opened    = 1, // Channel opened, ring buffers mapped
    Active    = 2, // Packets streaming actively
    Closed    = 3, // Channel closed by guest
    Rescinded = 4  // Channel offer revoked by host
};

inline const char* VmbusChannelStateToString(VmbusChannelState state) {
    switch (state) {
        case VmbusChannelState::Offered:   return "OFFERED (Awaiting Open)";
        case VmbusChannelState::Opened:    return "OPENED (Ring Buffers Mapped)";
        case VmbusChannelState::Active:    return "ACTIVE (Data Streaming)";
        case VmbusChannelState::Closed:    return "CLOSED";
        case VmbusChannelState::Rescinded: return "RESCINDED (Revoked by Host)";
        default:                           return "UNKNOWN";
    }
}

// ----------------------------------------------------------------------------
// GPADL (Guest Physical Address Descriptor List)
// ----------------------------------------------------------------------------
struct GpadlDescriptor {
    uint32_t gpadlId{0};
    uint32_t pageCount{0};
    std::vector<uint64_t> guestPfns;
    bool isMapped{false};
};

// ----------------------------------------------------------------------------
// VMBus Circular Ring Buffer Simulation
// ----------------------------------------------------------------------------
struct VmbusPacket {
    uint64_t transactionId{0};
    std::vector<uint8_t> payload;
};

class VmbusRingBuffer {
public:
    explicit VmbusRingBuffer(uint32_t capacityBytes = 65536)
        : m_capacity(capacityBytes) {}

    bool write(const uint8_t* data, uint32_t len, uint64_t transId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!data || len == 0 || (m_usedBytes + len + sizeof(VmbusPacket)) > m_capacity) {
            return false;
        }

        VmbusPacket pkt;
        pkt.transactionId = transId;
        pkt.payload.assign(data, data + len);
        m_queue.push_back(std::move(pkt));
        m_usedBytes += len;
        m_totalPacketsWritten++;
        m_totalBytesWritten += len;
        return true;
    }

    bool read(uint8_t* out, uint32_t maxLen, uint32_t& outLen, uint64_t& outTransId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_queue.empty() || !out) {
            return false;
        }

        const auto& pkt = m_queue.front();
        if (pkt.payload.size() > maxLen) {
            return false;
        }

        std::copy(pkt.payload.begin(), pkt.payload.end(), out);
        outLen = static_cast<uint32_t>(pkt.payload.size());
        outTransId = pkt.transactionId;

        m_usedBytes -= outLen;
        m_queue.erase(m_queue.begin());
        m_totalPacketsRead++;
        m_totalBytesRead += outLen;
        return true;
    }

    uint32_t getQueuedCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return static_cast<uint32_t>(m_queue.size());
    }

    uint64_t getTotalPacketsWritten() const { return m_totalPacketsWritten; }
    uint64_t getTotalBytesWritten() const { return m_totalBytesWritten; }
    uint64_t getTotalPacketsRead() const { return m_totalPacketsRead; }
    uint64_t getTotalBytesRead() const { return m_totalBytesRead; }

    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.clear();
        m_usedBytes = 0;
    }

private:
    uint32_t m_capacity{65536};
    uint32_t m_usedBytes{0};
    std::vector<VmbusPacket> m_queue;
    uint64_t m_totalPacketsWritten{0};
    uint64_t m_totalBytesWritten{0};
    uint64_t m_totalPacketsRead{0};
    uint64_t m_totalBytesRead{0};
    mutable std::mutex m_mutex;
};

// ----------------------------------------------------------------------------
// Synthetic SCSI Storage Driver Context (storvsc.sys)
// ----------------------------------------------------------------------------
class SyntheticStorageDevice {
public:
    SyntheticStorageDevice(uint32_t channelId, std::wstring lunName, uint64_t capacityBytes)
        : m_channelId(channelId), m_lunName(std::move(lunName)), m_capacityBytes(capacityBytes) {}

    uint32_t getChannelId() const { return m_channelId; }
    const std::wstring& getLunName() const { return m_lunName; }
    uint64_t getCapacityBytes() const { return m_capacityBytes; }

    bool scsiRead(uint64_t lba, uint32_t sectorCount, uint8_t* outBuf) {
        if (!outBuf || sectorCount == 0) return false;
        uint64_t byteOffset = lba * 512;
        if (byteOffset + (sectorCount * 512) > m_capacityBytes) return false;

        // Populate deterministic sector payload
        for (uint32_t i = 0; i < sectorCount * 512; ++i) {
            outBuf[i] = static_cast<uint8_t>((lba + i) & 0xFF);
        }
        m_readOps++;
        m_readBytes += (sectorCount * 512);
        return true;
    }

    bool scsiWrite(uint64_t lba, uint32_t sectorCount, const uint8_t* inBuf) {
        if (!inBuf || sectorCount == 0) return false;
        uint64_t byteOffset = lba * 512;
        if (byteOffset + (sectorCount * 512) > m_capacityBytes) return false;

        m_writeOps++;
        m_writeBytes += (sectorCount * 512);
        return true;
    }

    uint64_t getReadOps() const { return m_readOps; }
    uint64_t getWriteOps() const { return m_writeOps; }
    uint64_t getReadBytes() const { return m_readBytes; }
    uint64_t getWriteBytes() const { return m_writeBytes; }

private:
    uint32_t m_channelId{1};
    std::wstring m_lunName;
    uint64_t m_capacityBytes{512ULL * 1024ULL * 1024ULL * 1024ULL}; // 512 GB Virtual SCSI LUN
    std::atomic<uint64_t> m_readOps{0};
    std::atomic<uint64_t> m_writeOps{0};
    std::atomic<uint64_t> m_readBytes{0};
    std::atomic<uint64_t> m_writeBytes{0};
};

// ----------------------------------------------------------------------------
// Synthetic Network Adapter Driver Context (netvsc.sys)
// ----------------------------------------------------------------------------
class SyntheticNetworkAdapter {
public:
    SyntheticNetworkAdapter(uint32_t channelId, std::string macAddress, uint32_t linkSpeedGbps)
        : m_channelId(channelId), m_mac(std::move(macAddress)), m_linkSpeedGbps(linkSpeedGbps) {}

    uint32_t getChannelId() const { return m_channelId; }
    const std::string& getMacAddress() const { return m_mac; }
    uint32_t getLinkSpeedGbps() const { return m_linkSpeedGbps; }
    uint32_t getMtu() const { return m_mtu; }
    void setMtu(uint32_t mtu) { m_mtu = mtu; }

    bool transmitPacket(const uint8_t* pktData, uint32_t length) {
        if (!pktData || length == 0 || length > m_mtu + 14) return false;
        m_txPackets++;
        m_txBytes += length;
        return true;
    }

    bool receivePacket(uint32_t length) {
        if (length == 0 || length > m_mtu + 14) return false;
        m_rxPackets++;
        m_rxBytes += length;
        return true;
    }

    uint64_t getTxPackets() const { return m_txPackets; }
    uint64_t getRxPackets() const { return m_rxPackets; }
    uint64_t getTxBytes() const { return m_txBytes; }
    uint64_t getRxBytes() const { return m_rxBytes; }

private:
    uint32_t m_channelId{2};
    std::string m_mac{"00:15:5D:01:A0:42"};
    uint32_t m_linkSpeedGbps{100}; // 100 Gbps Virtual Synthetic Pipe
    uint32_t m_mtu{1500};
    std::atomic<uint64_t> m_txPackets{0};
    std::atomic<uint64_t> m_rxPackets{0};
    std::atomic<uint64_t> m_txBytes{0};
    std::atomic<uint64_t> m_rxBytes{0};
};

// ----------------------------------------------------------------------------
// Hyper-V Sockets Context (hv_sock / AF_HYPERV)
// ----------------------------------------------------------------------------
class HvSocketEndpoint {
public:
    HvSocketEndpoint(uint32_t socketId, std::wstring serviceGuid)
        : m_socketId(socketId), m_serviceGuid(std::move(serviceGuid)) {}

    uint32_t getSocketId() const { return m_socketId; }
    const std::wstring& getServiceGuid() const { return m_serviceGuid; }
    bool isConnected() const { return m_connected; }
    void setConnected(bool conn) { m_connected = conn; }

    bool sendData(const uint8_t* data, uint32_t len) {
        if (!m_connected || !data || len == 0) return false;
        m_bytesSent += len;
        return true;
    }

    uint64_t getBytesSent() const { return m_bytesSent; }

private:
    uint32_t m_socketId{1};
    std::wstring m_serviceGuid;
    bool m_connected{true};
    std::atomic<uint64_t> m_bytesSent{0};
};

// ----------------------------------------------------------------------------
// VMBus Channel Object
// ----------------------------------------------------------------------------
class VmbusChannel {
public:
    VmbusChannel(uint32_t channelId, VmbusChannelType type, std::wstring friendlyName)
        : m_channelId(channelId), m_type(type), m_name(std::move(friendlyName)),
          m_inRing(65536), m_outRing(65536) {}

    uint32_t getId() const { return m_channelId; }
    VmbusChannelType getType() const { return m_type; }
    const std::wstring& getName() const { return m_name; }
    VmbusChannelState getState() const { return m_state; }

    void setState(VmbusChannelState state) { m_state = state; }

    VmbusRingBuffer& getInRing() { return m_inRing; }
    VmbusRingBuffer& getOutRing() { return m_outRing; }

    uint32_t getGpadlId() const { return m_gpadlId; }
    void setGpadlId(uint32_t gpadlId) { m_gpadlId = gpadlId; }

private:
    uint32_t m_channelId{1};
    VmbusChannelType m_type{VmbusChannelType::Storage};
    std::wstring m_name;
    VmbusChannelState m_state{VmbusChannelState::Offered};
    uint32_t m_gpadlId{0};
    VmbusRingBuffer m_inRing;
    VmbusRingBuffer m_outRing;
};

// ----------------------------------------------------------------------------
// Core VMBus Subsystem Singleton (TitanVMBus / AegisChannel)
// ----------------------------------------------------------------------------
class VmbusSubsystem {
public:
    static VmbusSubsystem& get() {
        static VmbusSubsystem instance;
        return instance;
    }

    bool initialize() {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_initialized) return true;

        m_channels.clear();
        m_gpadls.clear();
        m_sockets.clear();

        // Register default synthetic channel 1: SCSI Storage (storvsc.sys)
        auto ch1 = std::make_shared<VmbusChannel>(1, VmbusChannelType::Storage, L"Hyper-V Synthetic SCSI Controller");
        ch1->setState(VmbusChannelState::Active);
        ch1->setGpadlId(1001);
        m_channels[1] = ch1;
        m_storageDev = std::make_shared<SyntheticStorageDevice>(1, L"Hyper-V Virtual NVMe Disk 0", 512ULL * 1024ULL * 1024ULL * 1024ULL);

        // Register default synthetic channel 2: Network Adapter (netvsc.sys)
        auto ch2 = std::make_shared<VmbusChannel>(2, VmbusChannelType::Network, L"Hyper-V Synthetic Network Adapter");
        ch2->setState(VmbusChannelState::Active);
        ch2->setGpadlId(1002);
        m_channels[2] = ch2;
        m_netAdapter = std::make_shared<SyntheticNetworkAdapter>(2, "00:15:5D:01:A0:42", 100);

        // Register default synthetic channel 3: Dynamic Memory (dmvsc.sys)
        auto ch3 = std::make_shared<VmbusChannel>(3, VmbusChannelType::DynamicMemory, L"Hyper-V Dynamic Memory Balloon Driver");
        ch3->setState(VmbusChannelState::Opened);
        ch3->setGpadlId(1003);
        m_channels[3] = ch3;

        // Register default synthetic channel 4: Hyper-V Sockets (hv_sock)
        auto ch4 = std::make_shared<VmbusChannel>(4, VmbusChannelType::HvSocket, L"Hyper-V Guest Socket Interconnect");
        ch4->setState(VmbusChannelState::Active);
        ch4->setGpadlId(1004);
        m_channels[4] = ch4;
        m_sockets[1] = std::make_shared<HvSocketEndpoint>(1, L"{e0762426-32d3-465d-98be-812301980860}");

        // Pre-seed GPADLs
        GpadlDescriptor g1{1001, 16, {0x1000, 0x1001}, true};
        GpadlDescriptor g2{1002, 32, {0x2000, 0x2001}, true};
        m_gpadls[1001] = g1;
        m_gpadls[1002] = g2;

        m_initialized = true;
        return true;
    }

    bool isInitialized() const { return m_initialized; }

    std::shared_ptr<VmbusChannel> getChannel(uint32_t channelId) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_channels.find(channelId);
        if (it != m_channels.end()) return it->second;
        return nullptr;
    }

    std::vector<std::shared_ptr<VmbusChannel>> getAllChannels() const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        std::vector<std::shared_ptr<VmbusChannel>> list;
        for (const auto& p : m_channels) list.push_back(p.second);
        return list;
    }

    uint32_t getChannelCount() const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return static_cast<uint32_t>(m_channels.size());
    }

    uint32_t offerChannel(VmbusChannelType type, std::wstring friendlyName) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        uint32_t newId = m_nextChannelId++;
        auto ch = std::make_shared<VmbusChannel>(newId, type, std::move(friendlyName));
        ch->setState(VmbusChannelState::Offered);
        m_channels[newId] = ch;
        return newId;
    }

    bool rescindChannel(uint32_t channelId) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_channels.find(channelId);
        if (it != m_channels.end()) {
            it->second->setState(VmbusChannelState::Rescinded);
            return true;
        }
        return false;
    }

    bool openChannel(uint32_t channelId, uint32_t gpadlId) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_channels.find(channelId);
        if (it != m_channels.end()) {
            it->second->setGpadlId(gpadlId);
            it->second->setState(VmbusChannelState::Active);
            return true;
        }
        return false;
    }

    bool closeChannel(uint32_t channelId) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_channels.find(channelId);
        if (it != m_channels.end()) {
            it->second->setState(VmbusChannelState::Closed);
            return true;
        }
        return false;
    }

    // GPADL Management
    uint32_t registerGpadl(uint32_t pageCount, const std::vector<uint64_t>& pfns) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        uint32_t gId = m_nextGpadlId++;
        GpadlDescriptor desc{gId, pageCount, pfns, true};
        m_gpadls[gId] = desc;
        return gId;
    }

    bool getGpadl(uint32_t gpadlId, GpadlDescriptor& desc) const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_gpadls.find(gpadlId);
        if (it != m_gpadls.end()) {
            desc = it->second;
            return true;
        }
        return false;
    }

    // Synthetic Device Accessors
    std::shared_ptr<SyntheticStorageDevice> getStorageDevice() { return m_storageDev; }
    std::shared_ptr<SyntheticNetworkAdapter> getNetworkAdapter() { return m_netAdapter; }

    // Hyper-V Sockets Management
    uint32_t createHvSocket(std::wstring serviceGuid) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        uint32_t sockId = m_nextSocketId++;
        auto sock = std::make_shared<HvSocketEndpoint>(sockId, std::move(serviceGuid));
        m_sockets[sockId] = sock;
        return sockId;
    }

    std::shared_ptr<HvSocketEndpoint> getHvSocket(uint32_t sockId) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_sockets.find(sockId);
        if (it != m_sockets.end()) return it->second;
        return nullptr;
    }

    // Dynamic Memory Ballooning Simulation
    uint32_t getBalloonedPages() const { return m_balloonedPages; }
    void requestMemoryBalloon(uint32_t pages) { m_balloonedPages += pages; }
    void releaseMemoryBalloon(uint32_t pages) {
        if (pages >= m_balloonedPages) m_balloonedPages = 0;
        else m_balloonedPages -= pages;
    }

    void reset() {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_channels.clear();
        m_gpadls.clear();
        m_sockets.clear();
        m_balloonedPages = 0;
        m_initialized = false;
        initialize();
    }

private:
    VmbusSubsystem() = default;

    bool m_initialized{false};
    std::map<uint32_t, std::shared_ptr<VmbusChannel>> m_channels;
    std::map<uint32_t, GpadlDescriptor> m_gpadls;
    std::map<uint32_t, std::shared_ptr<HvSocketEndpoint>> m_sockets;
    std::shared_ptr<SyntheticStorageDevice> m_storageDev;
    std::shared_ptr<SyntheticNetworkAdapter> m_netAdapter;
    uint32_t m_nextChannelId{5};
    uint32_t m_nextGpadlId{1005};
    uint32_t m_nextSocketId{2};
    std::atomic<uint32_t> m_balloonedPages{0};
    mutable std::recursive_mutex m_mutex;
};

// ----------------------------------------------------------------------------
// Win32 & NT Clean-Room Dynamic C ABI Parity Exports (vmbus.sys, hv_sock.dll)
// ----------------------------------------------------------------------------
extern "C" {

inline NTSTATUS VmbusInitializeSubsystem() {
    if (VmbusSubsystem::get().initialize()) {
        return micant::STATUS_SUCCESS;
    }
    return micant::STATUS_UNSUCCESSFUL;
}

inline NTSTATUS VmbusChannelEnumerate(uint32_t* pCount, uint32_t* pChannelIds) {
    if (!pCount) return micant::STATUS_INVALID_PARAMETER;
    auto& sys = VmbusSubsystem::get();
    sys.initialize();

    auto allChs = sys.getAllChannels();
    if (pChannelIds) {
        uint32_t copyCount = std::min(*pCount, static_cast<uint32_t>(allChs.size()));
        for (uint32_t i = 0; i < copyCount; ++i) {
            pChannelIds[i] = allChs[i]->getId();
        }
        *pCount = copyCount;
    } else {
        *pCount = static_cast<uint32_t>(allChs.size());
    }
    return micant::STATUS_SUCCESS;
}

inline NTSTATUS VmbusChannelOpen(uint32_t channelId, uint32_t gpadlId, uint32_t* pStatus) {
    if (!pStatus) return micant::STATUS_INVALID_PARAMETER;
    auto& sys = VmbusSubsystem::get();
    sys.initialize();

    auto ch = sys.getChannel(channelId);
    if (!ch) return micant::STATUS_NOT_FOUND;

    if (sys.openChannel(channelId, gpadlId)) {
        *pStatus = 1;
        return micant::STATUS_SUCCESS;
    }
    return micant::STATUS_UNSUCCESSFUL;
}

inline NTSTATUS VmbusChannelClose(uint32_t channelId) {
    auto& sys = VmbusSubsystem::get();
    sys.initialize();

    auto ch = sys.getChannel(channelId);
    if (!ch) return micant::STATUS_NOT_FOUND;

    bool ok = sys.closeChannel(channelId);
    return ok ? micant::STATUS_SUCCESS : micant::STATUS_UNSUCCESSFUL;
}

inline NTSTATUS VmbusChannelSendPacket(uint32_t channelId, const uint8_t* pData, uint32_t length, uint64_t transactionId) {
    if (!pData || length == 0) return micant::STATUS_INVALID_PARAMETER;
    auto& sys = VmbusSubsystem::get();
    sys.initialize();

    auto ch = sys.getChannel(channelId);
    if (!ch) return micant::STATUS_NOT_FOUND;

    if (ch->getState() != VmbusChannelState::Active && ch->getState() != VmbusChannelState::Opened) {
        return micant::STATUS_INVALID_DEVICE_STATE;
    }

    bool written = ch->getOutRing().write(pData, length, transactionId);
    return written ? micant::STATUS_SUCCESS : micant::STATUS_BUFFER_TOO_SMALL;
}

inline NTSTATUS VmbusChannelReceivePacket(uint32_t channelId, uint8_t* pBuffer, uint32_t bufferSize, uint32_t* pBytesRead, uint64_t* pTransactionId) {
    if (!pBuffer || !pBytesRead || !pTransactionId || bufferSize == 0) return micant::STATUS_INVALID_PARAMETER;
    auto& sys = VmbusSubsystem::get();
    sys.initialize();

    auto ch = sys.getChannel(channelId);
    if (!ch) return micant::STATUS_NOT_FOUND;

    uint32_t outLen = 0;
    uint64_t transId = 0;
    if (ch->getInRing().read(pBuffer, bufferSize, outLen, transId)) {
        *pBytesRead = outLen;
        *pTransactionId = transId;
        return micant::STATUS_SUCCESS;
    }
    return micant::STATUS_NO_MORE_ENTRIES;
}

inline NTSTATUS HvSocketCreate(const wchar_t* serviceGuid, uint32_t* pSocketId) {
    if (!serviceGuid || !pSocketId) return micant::STATUS_INVALID_PARAMETER;
    auto& sys = VmbusSubsystem::get();
    sys.initialize();

    uint32_t sId = sys.createHvSocket(serviceGuid);
    *pSocketId = sId;
    return micant::STATUS_SUCCESS;
}

inline NTSTATUS HvSocketSend(uint32_t socketId, const uint8_t* pData, uint32_t length) {
    if (!pData || length == 0) return micant::STATUS_INVALID_PARAMETER;
    auto& sys = VmbusSubsystem::get();
    sys.initialize();

    auto sock = sys.getHvSocket(socketId);
    if (!sock) return micant::STATUS_NOT_FOUND;

    bool sent = sock->sendData(pData, length);
    return sent ? micant::STATUS_SUCCESS : micant::STATUS_UNSUCCESSFUL;
}

} // extern "C"

// ----------------------------------------------------------------------------
// SCM & Version Registration Helper
// ----------------------------------------------------------------------------
inline void RegisterVmbusSubsystem() {
    // 1. Version Database Registration
    auto& vdb = micant::version::VersionDatabase::Instance();
    vdb.RegisterModule("vmbus.sys", "10.0.26100.1", "Hyper-V Virtual Machine Bus Root Driver (TitanVMBus)");
    vdb.RegisterModule("storvsc.sys", "10.0.26100.1", "Hyper-V Synthetic Storage Driver (AegisSCSI)");
    vdb.RegisterModule("netvsc.sys", "10.0.26100.1", "Hyper-V Synthetic Network Driver (AegisNet)");
    vdb.RegisterModule("hv_sock.dll", "10.0.26100.1", "Hyper-V Sockets Provider (AF_HYPERV)");

    // 2. SCM Registration
    auto& scm = micant::scm::ServiceControlManager::get();
    auto rec = std::make_shared<micant::scm::ServiceRecord>();
    rec->serviceName = L"VmBusService";
    rec->displayName = L"Hyper-V Virtual Machine Bus Service";
    rec->serviceType = micant::scm::SERVICE_WIN32_SHARE_PROCESS;
    rec->startType = micant::scm::SERVICE_AUTO_START;
    rec->binaryPath = L"C:\\Windows\\System32\\svchost.exe -k LocalSystemNetworkRestricted";
    rec->status.dwCurrentState = micant::scm::SERVICE_RUNNING;
    scm.registerServiceRecord(rec);

    // Initialize core subsystem singleton
    VmbusSubsystem::get().initialize();
}

} // namespace micant::vmbus
