#pragma once

/**
 * @file dhcp.hpp
 * @brief Clean-Room RFC 2131 Dynamic Host Configuration Protocol (DHCP) Client.
 *
 * Implements standard RFC 2131 DHCP DORA (Discover, Offer, Request, Ack) handshake,
 * lease renewal/release state machine, TLV option parsing, and Windows NT SCM
 * dhcpcsvc service registration.
 *
 * References: RFC 2131, RFC 1497, 17 U.S.C. § 1201(f).
 */

#include <cstdint>
#include <cstring>
#include <array>
#include <vector>
#include <string>
#include <span>
#include <chrono>
#include <mutex>
#include "micant/shared/ntdef.hpp"
#include "micant/um/native/ldr.hpp"
#include "micant/um/servers/scm.hpp"
#include "micant/um/subsystems/winbase/kernel32.hpp"
#include "micant/um/subsystems/net/ws2_32.hpp"
#include "micant/shared/afd_socket_abi.hpp"

namespace micant::dhcp {

// RFC 2131 Opcodes
inline constexpr uint8_t BOOTREQUEST = 1;
inline constexpr uint8_t BOOTREPLY   = 2;

// RFC 2131 Hardware Types
inline constexpr uint8_t HTYPE_ETHERNET = 1;

// RFC 1497 Magic Cookie
inline constexpr uint32_t DHCP_MAGIC_COOKIE = 0x63825363; // 99.130.83.99

// RFC 2131 Option Codes
inline constexpr uint8_t DHCP_OPT_PAD                = 0;
inline constexpr uint8_t DHCP_OPT_SUBNET_MASK        = 1;
inline constexpr uint8_t DHCP_OPT_ROUTER             = 3;
inline constexpr uint8_t DHCP_OPT_DNS_SERVER         = 6;
inline constexpr uint8_t DHCP_OPT_HOST_NAME          = 12;
inline constexpr uint8_t DHCP_OPT_DOMAIN_NAME        = 15;
inline constexpr uint8_t DHCP_OPT_REQUESTED_IP       = 50;
inline constexpr uint8_t DHCP_OPT_LEASE_TIME         = 51;
inline constexpr uint8_t DHCP_OPT_MESSAGE_TYPE       = 53;
inline constexpr uint8_t DHCP_OPT_SERVER_ID          = 54;
inline constexpr uint8_t DHCP_OPT_PARAM_REQUEST_LIST = 55;
inline constexpr uint8_t DHCP_OPT_RENEWAL_TIME       = 58;
inline constexpr uint8_t DHCP_OPT_REBINDING_TIME     = 59;
inline constexpr uint8_t DHCP_OPT_END                = 255;

// RFC 2131 Message Types (Option 53)
#ifndef MICANT_DHCP_MSG_TYPES_DEFINED
#define MICANT_DHCP_MSG_TYPES_DEFINED
inline constexpr uint8_t DHCPDISCOVER = 1;
inline constexpr uint8_t DHCPOFFER    = 2;
inline constexpr uint8_t DHCPREQUEST  = 3;
inline constexpr uint8_t DHCPDECLINE  = 4;
inline constexpr uint8_t DHCPACK      = 5;
inline constexpr uint8_t DHCPNAK      = 6;
inline constexpr uint8_t DHCPRELEASE  = 7;
inline constexpr uint8_t DHCPINFORM   = 8;
#endif

struct DhcpIpv4Address {
    uint32_t addr{};
    constexpr DhcpIpv4Address() noexcept = default;
    constexpr explicit DhcpIpv4Address(uint32_t value) noexcept : addr(value) {}
    [[nodiscard]] constexpr bool isZero() const noexcept { return addr == 0; }
    [[nodiscard]] static constexpr DhcpIpv4Address any() noexcept { return {}; }
    friend constexpr bool operator==(DhcpIpv4Address, DhcpIpv4Address) noexcept = default;
};
struct DhcpMacAddress { uint8_t bytes[6]{}; };
[[nodiscard]] constexpr uint16_t dhcpHtons(uint16_t value) noexcept {
    return static_cast<uint16_t>((value << 8) | (value >> 8));
}
[[nodiscard]] constexpr uint32_t dhcpHtonl(uint32_t value) noexcept {
    return __builtin_bswap32(value);
}
[[nodiscard]] constexpr uint32_t dhcpNtohl(uint32_t value) noexcept {
    return dhcpHtonl(value);
}

enum class DhcpState {
    Init,
    Selecting,
    Requesting,
    Bound,
    Renewing,
    Rebinding
};

[[nodiscard]] inline const char* getDhcpStateName(DhcpState st) noexcept {
    switch (st) {
        case DhcpState::Init:       return "INIT";
        case DhcpState::Selecting:  return "SELECTING";
        case DhcpState::Requesting: return "REQUESTING";
        case DhcpState::Bound:      return "BOUND";
        case DhcpState::Renewing:   return "RENEWING";
        case DhcpState::Rebinding:  return "REBINDING";
        default:                    return "UNKNOWN";
    }
}

#pragma pack(push, 1)

/**
 * @brief 240-byte Fixed RFC 2131 DHCP Header (before Options).
 */
struct DhcpRawHeader {
    uint8_t  op{BOOTREQUEST};
    uint8_t  htype{HTYPE_ETHERNET};
    uint8_t  hlen{6};
    uint8_t  hops{0};
    uint32_t xid{0};
    uint16_t secs{0};
    uint16_t flags{0};
    uint32_t ciaddr{0};
    uint32_t yiaddr{0};
    uint32_t siaddr{0};
    uint32_t giaddr{0};
    uint8_t  chaddr[16]{0};
    char     sname[64]{0};
    char     file[128]{0};
    uint32_t magicCookie{dhcpHtonl(DHCP_MAGIC_COOKIE)};
};

#pragma pack(pop)

struct ParsedDhcpMessage {
    uint8_t op{0};
    uint32_t xid{0};
    DhcpMacAddress clientMac{};
    DhcpIpv4Address clientIp{};
    DhcpIpv4Address yourIp{};
    DhcpIpv4Address yiaddr{};
    DhcpIpv4Address requestedIp{};
    DhcpIpv4Address serverIp{};
    uint8_t messageType{0};
    DhcpIpv4Address subnetMask{};
    DhcpIpv4Address router{};
    std::vector<DhcpIpv4Address> dnsServers;
    DhcpIpv4Address dnsServer{};
    uint32_t leaseTimeSeconds{86400};
    DhcpIpv4Address serverIdentifier{};
};

/**
 * @brief Clean-Room Autonomous DHCP Client Engine (dhcpcsvc.dll).
 */
class DhcpClientEngine {
public:
    static DhcpClientEngine& get() {
        static DhcpClientEngine instance;
        return instance;
    }

    void resetForTest() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (configurationSocket_ != ws2_32::INVALID_SOCKET)
            (void)ws2_32::closesocket(configurationSocket_);
        configurationSocket_ = ws2_32::INVALID_SOCKET;
        state_ = DhcpState::Init;
        dhcpEnabled_ = false;
        currentXid_ = 0x12345678;
        assignedIp_ = DhcpIpv4Address::any();
        subnetMask_ = DhcpIpv4Address::any();
        gateway_ = DhcpIpv4Address::any();
        serverIdentifier_ = DhcpIpv4Address::any();
        leaseDurationSeconds_ = 0;
        leaseObtainedTime_ = 0;
        leaseExpiresTime_ = 0;
        hardwareAddress_ = {};
        ackDnsServer_ = {};
    }

    [[nodiscard]] bool isDhcpEnabled() const noexcept { return dhcpEnabled_; }

    [[nodiscard]] DhcpState getState() const noexcept { return state_; }
    [[nodiscard]] DhcpIpv4Address getAssignedIp() const noexcept { return assignedIp_; }
    [[nodiscard]] DhcpIpv4Address getSubnetMask() const noexcept { return subnetMask_; }
    [[nodiscard]] DhcpIpv4Address getGateway() const noexcept { return gateway_; }
    [[nodiscard]] DhcpIpv4Address getServerIdentifier() const noexcept { return serverIdentifier_; }
    [[nodiscard]] DhcpIpv4Address getDnsServer() const noexcept { return ackDnsServer_; }
    [[nodiscard]] uint32_t getLeaseDuration() const noexcept { return leaseDurationSeconds_; }
    [[nodiscard]] int64_t getLeaseObtainedTime() const noexcept { return leaseObtainedTime_; }
    [[nodiscard]] int64_t getLeaseExpiresTime() const noexcept { return leaseExpiresTime_; }

    /**
     * @brief Crafts an RFC 2131 DHCPDISCOVER packet.
     */
    std::vector<uint8_t> buildDiscover(uint32_t xid, DhcpMacAddress mac) {
        std::vector<uint8_t> packet(sizeof(DhcpRawHeader), 0);
        auto* hdr = reinterpret_cast<DhcpRawHeader*>(packet.data());
        hdr->op = BOOTREQUEST;
        hdr->htype = HTYPE_ETHERNET;
        hdr->hlen = 6;
        hdr->xid = dhcpHtonl(xid);
        hdr->flags = dhcpHtons(0x8000); // Broadcast
        std::memcpy(hdr->chaddr, mac.bytes, 6);
        hdr->magicCookie = dhcpHtonl(DHCP_MAGIC_COOKIE);

        // Options:
        // 53 (Message Type): 1 (DHCPDISCOVER)
        packet.push_back(DHCP_OPT_MESSAGE_TYPE);
        packet.push_back(1);
        packet.push_back(DHCPDISCOVER);

        // 55 (Param Request List): Subnet, Router, DNS, Domain
        packet.push_back(DHCP_OPT_PARAM_REQUEST_LIST);
        packet.push_back(4);
        packet.push_back(DHCP_OPT_SUBNET_MASK);
        packet.push_back(DHCP_OPT_ROUTER);
        packet.push_back(DHCP_OPT_DNS_SERVER);
        packet.push_back(DHCP_OPT_DOMAIN_NAME);

        // 255 (End)
        packet.push_back(DHCP_OPT_END);

        state_ = DhcpState::Selecting;
        currentXid_ = xid;
        return packet;
    }

    /**
     * @brief Crafts an RFC 2131 DHCPREQUEST packet.
     */
    std::vector<uint8_t> buildRequest(
        uint32_t xid,
        DhcpMacAddress mac,
        DhcpIpv4Address requestedIp,
        DhcpIpv4Address serverId
    ) {
        std::vector<uint8_t> packet(sizeof(DhcpRawHeader), 0);
        auto* hdr = reinterpret_cast<DhcpRawHeader*>(packet.data());
        hdr->op = BOOTREQUEST;
        hdr->htype = HTYPE_ETHERNET;
        hdr->hlen = 6;
        hdr->xid = dhcpHtonl(xid);
        hdr->flags = dhcpHtons(0x8000);
        std::memcpy(hdr->chaddr, mac.bytes, 6);
        hdr->magicCookie = dhcpHtonl(DHCP_MAGIC_COOKIE);

        // Options:
        // 53 (Message Type): 3 (DHCPREQUEST)
        packet.push_back(DHCP_OPT_MESSAGE_TYPE);
        packet.push_back(1);
        packet.push_back(DHCPREQUEST);

        // 50 (Requested IP)
        packet.push_back(DHCP_OPT_REQUESTED_IP);
        packet.push_back(4);
        const auto* ipBytes = reinterpret_cast<const uint8_t*>(&requestedIp.addr);
        packet.insert(packet.end(), ipBytes, ipBytes + 4);

        // 54 (Server Identifier)
        packet.push_back(DHCP_OPT_SERVER_ID);
        packet.push_back(4);
        const auto* srvBytes = reinterpret_cast<const uint8_t*>(&serverId.addr);
        packet.insert(packet.end(), srvBytes, srvBytes + 4);

        packet.push_back(DHCP_OPT_END);

        state_ = DhcpState::Requesting;
        return packet;
    }

    /**
     * @brief Crafts an RFC 2131 DHCPRELEASE packet.
     */
    std::vector<uint8_t> buildRelease(
        uint32_t xid,
        DhcpMacAddress mac,
        DhcpIpv4Address clientIp,
        DhcpIpv4Address serverId
    ) {
        std::vector<uint8_t> packet(sizeof(DhcpRawHeader), 0);
        auto* hdr = reinterpret_cast<DhcpRawHeader*>(packet.data());
        hdr->op = BOOTREQUEST;
        hdr->htype = HTYPE_ETHERNET;
        hdr->hlen = 6;
        hdr->xid = dhcpHtonl(xid);
        hdr->ciaddr = clientIp.addr;
        std::memcpy(hdr->chaddr, mac.bytes, 6);
        hdr->magicCookie = dhcpHtonl(DHCP_MAGIC_COOKIE);

        // 53 (Message Type): 7 (DHCPRELEASE)
        packet.push_back(DHCP_OPT_MESSAGE_TYPE);
        packet.push_back(1);
        packet.push_back(DHCPRELEASE);

        // 54 (Server Identifier)
        packet.push_back(DHCP_OPT_SERVER_ID);
        packet.push_back(4);
        const auto* srvBytes = reinterpret_cast<const uint8_t*>(&serverId.addr);
        packet.insert(packet.end(), srvBytes, srvBytes + 4);

        packet.push_back(DHCP_OPT_END);

        return packet;
    }

    /**
     * @brief Parses an RFC 2131 DHCP packet.
     */
    static bool parseMessage(std::span<const uint8_t> data, ParsedDhcpMessage& out) {
        if (data.size() < sizeof(DhcpRawHeader)) return false;
        DhcpRawHeader header{};
        std::memcpy(&header, data.data(), sizeof(header));
        const auto* hdr = &header;
        if (dhcpNtohl(hdr->magicCookie) != DHCP_MAGIC_COOKIE) return false;
        if ((hdr->op != BOOTREQUEST && hdr->op != BOOTREPLY) ||
            hdr->htype != HTYPE_ETHERNET || hdr->hlen != 6) return false;

        ParsedDhcpMessage parsed{};
        parsed.op = hdr->op;
        parsed.xid = dhcpNtohl(hdr->xid);
        std::memcpy(parsed.clientMac.bytes, hdr->chaddr, 6);
        parsed.clientIp = DhcpIpv4Address(hdr->ciaddr);
        parsed.yourIp = DhcpIpv4Address(hdr->yiaddr);
        parsed.yiaddr = parsed.yourIp;
        parsed.serverIp = DhcpIpv4Address(hdr->siaddr);

        size_t offset = sizeof(DhcpRawHeader);
        bool ended = false;
        bool seenType = false;
        while (offset < data.size()) {
            uint8_t opt = data[offset++];
            if (opt == DHCP_OPT_PAD) continue;
            if (opt == DHCP_OPT_END) { ended = true; break; }
            if (offset >= data.size()) return false;
            uint8_t len = data[offset++];
            if (len > data.size() - offset) return false;

            if ((opt == DHCP_OPT_MESSAGE_TYPE && (len != 1 || seenType)) ||
                ((opt == DHCP_OPT_REQUESTED_IP || opt == DHCP_OPT_SUBNET_MASK ||
                  opt == DHCP_OPT_SERVER_ID || opt == DHCP_OPT_LEASE_TIME) && len != 4) ||
                ((opt == DHCP_OPT_ROUTER || opt == DHCP_OPT_DNS_SERVER) &&
                 (len == 0 || len % 4 != 0))) return false;

            const uint8_t* val = data.data() + offset;
            if (opt == DHCP_OPT_REQUESTED_IP && len >= 4) {
                uint32_t req = 0;
                std::memcpy(&req, val, 4);
                parsed.requestedIp = DhcpIpv4Address(req);
            } else if (opt == DHCP_OPT_MESSAGE_TYPE && len >= 1) {
                seenType = true;
                parsed.messageType = val[0];
            } else if (opt == DHCP_OPT_SUBNET_MASK && len >= 4) {
                uint32_t mask = 0;
                std::memcpy(&mask, val, 4);
                parsed.subnetMask = DhcpIpv4Address(mask);
            } else if (opt == DHCP_OPT_ROUTER && len >= 4) {
                uint32_t gw = 0;
                std::memcpy(&gw, val, 4);
                parsed.router = DhcpIpv4Address(gw);
            } else if (opt == DHCP_OPT_SERVER_ID && len >= 4) {
                uint32_t srv = 0;
                std::memcpy(&srv, val, 4);
                parsed.serverIdentifier = DhcpIpv4Address(srv);
            } else if (opt == DHCP_OPT_LEASE_TIME && len >= 4) {
                uint32_t lt = 0;
                std::memcpy(&lt, val, 4);
                parsed.leaseTimeSeconds = dhcpNtohl(lt);
            } else if (opt == DHCP_OPT_DNS_SERVER) {
                for (size_t i = 0; i + 4 <= len; i += 4) {
                    uint32_t dns = 0;
                    std::memcpy(&dns, val + i, 4);
                    parsed.dnsServers.push_back(DhcpIpv4Address(dns));
                }
            }
            offset += len;
        }

        if (!ended || !seenType) return false;
        out = std::move(parsed);
        return true;
    }

    /**
     * @brief Processes a DHCPOFFER and generates DHCPREQUEST.
     */
    bool handleOffer(const ParsedDhcpMessage& offer, std::vector<uint8_t>& outRequestPacket) {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto myMac = hardwareAddress_;
        if (state_ != DhcpState::Selecting || offer.op != BOOTREPLY ||
            offer.messageType != DHCPOFFER || offer.xid != currentXid_ ||
            std::memcmp(offer.clientMac.bytes, myMac.bytes, 6) != 0 ||
            (offer.yourIp.isZero() && offer.yiaddr.isZero()) ||
            offer.serverIdentifier.isZero()) return false;

        assignedIp_ = (!offer.yourIp.isZero()) ? offer.yourIp : offer.yiaddr;
        subnetMask_ = offer.subnetMask;
        gateway_ = offer.router;
        serverIdentifier_ = offer.serverIdentifier;
        leaseDurationSeconds_ = offer.leaseTimeSeconds;

        outRequestPacket = buildRequest(currentXid_, myMac, assignedIp_, serverIdentifier_);
        return true;
    }

    /**
     * @brief Processes a DHCPACK and commits the network configuration.
     */
    bool handleAck(const ParsedDhcpMessage& ack) {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto myMac = hardwareAddress_;
        const auto acknowledgedIp = ack.yourIp.isZero() ? ack.yiaddr : ack.yourIp;
        if (state_ != DhcpState::Requesting || ack.op != BOOTREPLY ||
            ack.messageType != DHCPACK || ack.xid != currentXid_ ||
            std::memcmp(ack.clientMac.bytes, myMac.bytes, 6) != 0 ||
            ack.serverIdentifier.isZero() || ack.serverIdentifier != serverIdentifier_ ||
            !ack.leaseTimeSeconds || acknowledgedIp.isZero() ||
            acknowledgedIp != assignedIp_) return false;

        if (configurationSocket_ == ws2_32::INVALID_SOCKET) return false;
        shared::afd::NetworkConfigurationV1 configuration{};
        if (ws2_32::queryNetworkConfiguration(configurationSocket_, configuration) != NtStatus::Success)
            return false;
        const auto nextMask = ack.subnetMask.isZero() ? subnetMask_ : ack.subnetMask;
        const auto nextGateway = ack.router.isZero() ? gateway_ : ack.router;
        const auto nextDns = ack.dnsServers.empty()
            ? DhcpIpv4Address(configuration.DnsServerNetworkOrder) : ack.dnsServers.front();
        configuration.Flags = shared::afd::NetworkConfigurationSet;
        configuration.Ipv4AddressNetworkOrder = acknowledgedIp.addr;
        configuration.SubnetMaskNetworkOrder = nextMask.addr;
        configuration.GatewayNetworkOrder = nextGateway.addr;
        configuration.DnsServerNetworkOrder = nextDns.addr;
        if (ws2_32::setNetworkConfiguration(configurationSocket_, configuration) != NtStatus::Success)
            return false;

        assignedIp_ = acknowledgedIp;
        subnetMask_ = nextMask;
        gateway_ = nextGateway;
        serverIdentifier_ = ack.serverIdentifier;
        leaseDurationSeconds_ = ack.leaseTimeSeconds;
        ackDnsServer_ = nextDns;
        const auto now = std::chrono::system_clock::now();
        leaseObtainedTime_ = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        leaseExpiresTime_ = leaseObtainedTime_ + leaseDurationSeconds_;

        state_ = DhcpState::Bound;
        dhcpEnabled_ = true;

        return true;
    }

    // Performs the actual Ring 3 DHCP DORA exchange over the canonical AFD
    // socket boundary. Each response is accepted only from UDP/67 and must
    // match the transaction ID, client hardware address and server ID.
    bool acquireLease(uint32_t retryCount = 3) noexcept {
        if (!retryCount || retryCount > 5) return false;
        ws2_32::WSADATA data{};
        if (ws2_32::WSAStartup(0x0202, &data) != 0) return false;
        const auto socket = ws2_32::socket(ws2_32::AF_INET, ws2_32::SOCK_DGRAM, ws2_32::IPPROTO_UDP);
        if (socket == ws2_32::INVALID_SOCKET) return false;
        configurationSocket_ = socket;
        auto closeOnExit = [this]() noexcept {
            if (configurationSocket_ != ws2_32::INVALID_SOCKET) {
                (void)ws2_32::closesocket(configurationSocket_);
                configurationSocket_ = ws2_32::INVALID_SOCKET;
            }
        };
        try {
        uint32_t timeout = 1000;
        if (ws2_32::setsockopt(socket, ws2_32::SOL_SOCKET, ws2_32::SO_RCVTIMEO,
                               reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == ws2_32::SOCKET_ERROR) {
            closeOnExit(); return false;
        }
        ws2_32::sockaddr_in local{};
        local.sin_family = ws2_32::AF_INET;
        local.sin_port = ws2_32::htons(68);
        local.sin_addr.S_un.S_addr = ws2_32::htonl(0);
        if (ws2_32::bind(socket, reinterpret_cast<const ws2_32::sockaddr*>(&local), sizeof(local)) == ws2_32::SOCKET_ERROR) {
            closeOnExit(); return false;
        }
        shared::afd::NetworkConfigurationV1 current{};
        if (ws2_32::queryNetworkConfiguration(socket, current) != NtStatus::Success) {
            closeOnExit(); return false;
        }
        std::memcpy(hardwareAddress_.bytes, current.MacAddress, sizeof(hardwareAddress_.bytes));
        uint8_t macOr = 0;
        for (uint8_t byte : hardwareAddress_.bytes) macOr |= byte;
        if (!macOr || (hardwareAddress_.bytes[0] & 1)) { closeOnExit(); return false; }

        ws2_32::sockaddr_in server{};
        server.sin_family = ws2_32::AF_INET;
        server.sin_port = ws2_32::htons(67);
        server.sin_addr.S_un.S_addr = ws2_32::htonl(0xFFFFFFFFu);
        std::array<uint8_t, 1500> receiveBuffer{};
        for (uint32_t attempt = 0; attempt < retryCount; ++attempt) {
            const uint32_t xid = static_cast<uint32_t>(win32::GetTickCount64()) ^
                (static_cast<uint32_t>(hardwareAddress_.bytes[2]) << 24) ^
                (static_cast<uint32_t>(hardwareAddress_.bytes[4]) << 8) ^ attempt;
            auto discover = buildDiscover(xid, hardwareAddress_);
            if (ws2_32::sendto(socket, reinterpret_cast<const char*>(discover.data()),
                    static_cast<int>(discover.size()), 0,
                    reinterpret_cast<const ws2_32::sockaddr*>(&server), sizeof(server)) !=
                static_cast<int>(discover.size())) continue;

            ws2_32::sockaddr_in source{};
            int sourceLength = sizeof(source);
            int received = ws2_32::recvfrom(socket, reinterpret_cast<char*>(receiveBuffer.data()),
                receiveBuffer.size(), 0, reinterpret_cast<ws2_32::sockaddr*>(&source), &sourceLength);
            if (received <= 0 || sourceLength < static_cast<int>(sizeof(source)) ||
                source.sin_family != ws2_32::AF_INET || ws2_32::ntohs(source.sin_port) != 67 ||
                !source.sin_addr.S_un.S_addr) continue;
            ParsedDhcpMessage offer{};
            if (!parseMessage(std::span<const uint8_t>(receiveBuffer.data(), static_cast<size_t>(received)), offer) ||
                offer.xid != xid || offer.messageType != DHCPOFFER || offer.op != BOOTREPLY) continue;
            std::vector<uint8_t> request;
            if (!handleOffer(offer, request) || request.empty()) continue;
            if (ws2_32::sendto(socket, reinterpret_cast<const char*>(request.data()),
                    static_cast<int>(request.size()), 0,
                    reinterpret_cast<const ws2_32::sockaddr*>(&server), sizeof(server)) !=
                static_cast<int>(request.size())) continue;

            source = {};
            sourceLength = sizeof(source);
            received = ws2_32::recvfrom(socket, reinterpret_cast<char*>(receiveBuffer.data()),
                receiveBuffer.size(), 0, reinterpret_cast<ws2_32::sockaddr*>(&source), &sourceLength);
            if (received <= 0 || sourceLength < static_cast<int>(sizeof(source)) ||
                source.sin_family != ws2_32::AF_INET || ws2_32::ntohs(source.sin_port) != 67 ||
                !source.sin_addr.S_un.S_addr) continue;
            ParsedDhcpMessage ack{};
            if (!parseMessage(std::span<const uint8_t>(receiveBuffer.data(), static_cast<size_t>(received)), ack) ||
                ack.xid != xid || ack.messageType != DHCPACK || ack.op != BOOTREPLY ||
                ack.serverIdentifier != serverIdentifier_) continue;
            const bool committed = handleAck(ack);
            closeOnExit();
            return committed;
        }
        closeOnExit();
        return false;
        } catch (...) {
            closeOnExit();
            return false;
        }
    }

    /**
     * @brief Releases the currently assigned DHCP lease.
     */
    bool releaseLease() {
        std::lock_guard<std::mutex> lock(mutex_);
        auto socket = configurationSocket_;
        if (socket == ws2_32::INVALID_SOCKET && state_ == DhcpState::Bound) {
            socket = ws2_32::socket(ws2_32::AF_INET, ws2_32::SOCK_DGRAM, ws2_32::IPPROTO_UDP);
        }
        if (state_ == DhcpState::Bound && socket == ws2_32::INVALID_SOCKET) return false;
        if (socket != ws2_32::INVALID_SOCKET && state_ == DhcpState::Bound) {
            const auto packet = buildRelease(currentXid_, hardwareAddress_, assignedIp_, serverIdentifier_);
            ws2_32::sockaddr_in server{};
            server.sin_family = ws2_32::AF_INET;
            server.sin_port = ws2_32::htons(67);
            server.sin_addr.S_un.S_addr = serverIdentifier_.addr;
            (void)ws2_32::sendto(socket, reinterpret_cast<const char*>(packet.data()),
                static_cast<int>(packet.size()), 0,
                reinterpret_cast<const ws2_32::sockaddr*>(&server), sizeof(server));
            shared::afd::NetworkConfigurationV1 configuration{};
            if (ws2_32::queryNetworkConfiguration(socket, configuration) != NtStatus::Success) {
                (void)ws2_32::closesocket(socket);
                return false;
            }
            configuration.Flags = shared::afd::NetworkConfigurationSet;
            configuration.Ipv4AddressNetworkOrder = 0;
            configuration.SubnetMaskNetworkOrder = 0;
            configuration.GatewayNetworkOrder = 0;
            configuration.DnsServerNetworkOrder = 0;
            if (ws2_32::setNetworkConfiguration(socket, configuration) != NtStatus::Success) {
                (void)ws2_32::closesocket(socket);
                return false;
            }
            (void)ws2_32::closesocket(socket);
        }
        state_ = DhcpState::Init;
        dhcpEnabled_ = false;
        configurationSocket_ = ws2_32::INVALID_SOCKET;
        assignedIp_ = DhcpIpv4Address::any();
        subnetMask_ = DhcpIpv4Address::any();
        gateway_ = DhcpIpv4Address::any();
        serverIdentifier_ = DhcpIpv4Address::any();
        ackDnsServer_ = DhcpIpv4Address::any();
        return true;
    }

private:
    DhcpClientEngine() = default;

    std::mutex mutex_;
    DhcpState state_{DhcpState::Init};
    bool dhcpEnabled_{false};
    uint32_t currentXid_{0x12345678};
    DhcpIpv4Address assignedIp_{};
    DhcpIpv4Address subnetMask_{};
    DhcpIpv4Address gateway_{};
    DhcpIpv4Address serverIdentifier_{};
    uint32_t leaseDurationSeconds_{86400};
    int64_t leaseObtainedTime_{0};
    int64_t leaseExpiresTime_{0};
    DhcpMacAddress hardwareAddress_{};
    DhcpIpv4Address ackDnsServer_{};
    ws2_32::SOCKET configurationSocket_{ws2_32::INVALID_SOCKET};
};

/**
 * @brief SCM Service Registration for dhcpcsvc.
 */
inline void InitializeDhcpClientSubsystem() {
    auto& scm = scm::ServiceControlManager::get();
    auto dhcpSvc = std::make_shared<scm::ServiceRecord>();
    dhcpSvc->serviceName = L"dhcpcsvc";
    dhcpSvc->displayName = L"DHCP Client";
    dhcpSvc->serviceType = scm::SERVICE_WIN32_SHARE_PROCESS;
    dhcpSvc->startType = scm::SERVICE_AUTO_START;
    dhcpSvc->errorControl = scm::SERVICE_ERROR_NORMAL;
    dhcpSvc->binaryPath = L"C:\\Windows\\System32\\svchost.exe -k LocalServiceNetworkRestricted";
    dhcpSvc->status.dwCurrentState = scm::SERVICE_RUNNING;
    scm.registerServiceRecord(dhcpSvc);

    auto& verDb = version::VersionDatabase::Instance();
    verDb.RegisterModule("dhcpcsvc.dll", "10.0.26100.5900", "MicaNT DHCP Client Service");
}

} // namespace micant::dhcp
