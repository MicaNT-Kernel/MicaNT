#pragma once

/**
 * @file ndis.hpp
 * @brief Clean-Room Network Driver Interface Specification (NDIS 6.x) Subsystem.
 *
 * Implements standard Windows NT NDIS driver abstractions:
 * - Ethernet II framing (IEEE 802.3 / DIX Ethernet)
 * - MAC address handling and byte-order utilities
 * - NetBuffer & NetBufferList (NBL) packet representation
 * - INdisAdapter / INdisMiniport abstract network interface card contract
 * - VirtualNetworkAdapter (high-performance in-memory loopback and virtual switch)
 *
 * References: Microsoft Learn NDIS 6.0+ Miniport Driver Architecture & IEEE 802.3.
 */

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <mutex>
#include <span>
#include <cstring>
#include <array>
#include <functional>
#include <iomanip>
#include <sstream>
#include "ntdef.hpp"
#include "ntstatus.hpp"

namespace micant::ndis {

// Standard Ethernet Constants
inline constexpr uint16_t ETHERTYPE_IPV4 = 0x0800;
inline constexpr uint16_t ETHERTYPE_ARP  = 0x0806;
inline constexpr uint16_t ETHERTYPE_VLAN = 0x8100;
inline constexpr uint16_t ETHERTYPE_IPV6 = 0x86DD;

inline constexpr size_t ETH_ALEN         = 6;
inline constexpr size_t ETH_HLEN         = 14;
inline constexpr size_t ETH_MIN_LEN      = 60;
inline constexpr size_t ETH_MAX_LEN      = 1514;
inline constexpr size_t DEFAULT_MTU      = 1500;

// NDIS Packet Filter Types
inline constexpr uint32_t NDIS_PACKET_TYPE_DIRECTED    = 0x00000001;
inline constexpr uint32_t NDIS_PACKET_TYPE_MULTICAST   = 0x00000002;
inline constexpr uint32_t NDIS_PACKET_TYPE_ALL_MULTICAST = 0x00000004;
inline constexpr uint32_t NDIS_PACKET_TYPE_BROADCAST   = 0x00000008;
inline constexpr uint32_t NDIS_PACKET_TYPE_PROMISCUOUS = 0x00000020;

// Media Connect States
enum class MediaConnectState : uint32_t {
    Unknown      = 0,
    Connected    = 1,
    Disconnected = 2
};

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

#pragma pack(pop)

/**
 * @brief NDIS 6.x NetBufferList Packet Container.
 */
struct NetBuffer {
    std::vector<uint8_t> data;
    uint32_t dataOffset{0};
    uint32_t dataLength{0};

    NetBuffer() = default;
    explicit NetBuffer(std::span<const uint8_t> payload)
        : data(payload.begin(), payload.end()), dataOffset(0), dataLength(static_cast<uint32_t>(payload.size())) {}
};

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
    mutable std::mutex mutex_;
    AdapterStatistics stats_{};
    ReceiveCallback rxCallback_;
    std::shared_ptr<VirtualNetworkAdapter> peerAdapter_;
};

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

} // namespace micant::ndis
