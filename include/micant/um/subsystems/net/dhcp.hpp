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
#include <vector>
#include <string>
#include <span>
#include <chrono>
#include <mutex>
#include "micant/shared/ntdef.hpp"
#include "micant/um/native/ldr.hpp"
#include "micant/um/servers/scm.hpp"
#include "micant/um/subsystems/winbase/kernel32.hpp"
#include "micant/km/drivers/net/tcpip.hpp"

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
    uint32_t magicCookie{tcpip::htonl(DHCP_MAGIC_COOKIE)};
};

#pragma pack(pop)

struct ParsedDhcpMessage {
    uint8_t op{0};
    uint32_t xid{0};
    ndis::MacAddress clientMac{};
    tcpip::Ipv4Address clientIp{};
    tcpip::Ipv4Address yourIp{};
    tcpip::Ipv4Address yiaddr{};
    tcpip::Ipv4Address requestedIp{};
    tcpip::Ipv4Address serverIp{};
    uint8_t messageType{0};
    tcpip::Ipv4Address subnetMask{};
    tcpip::Ipv4Address router{};
    std::vector<tcpip::Ipv4Address> dnsServers;
    tcpip::Ipv4Address dnsServer{};
    uint32_t leaseTimeSeconds{86400};
    tcpip::Ipv4Address serverIdentifier{};
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
        state_ = DhcpState::Init;
        dhcpEnabled_ = false;
        currentXid_ = 0x12345678;
        assignedIp_ = tcpip::Ipv4Address::any();
        subnetMask_ = tcpip::Ipv4Address::any();
        gateway_ = tcpip::Ipv4Address::any();
        serverIdentifier_ = tcpip::Ipv4Address::any();
        leaseDurationSeconds_ = 0;
        leaseObtainedTime_ = 0;
        leaseExpiresTime_ = 0;
    }

    [[nodiscard]] bool isDhcpEnabled() const noexcept { return dhcpEnabled_; }
    void setDhcpEnabled(bool enabled) noexcept { dhcpEnabled_ = enabled; }

    [[nodiscard]] DhcpState getState() const noexcept { return state_; }
    [[nodiscard]] tcpip::Ipv4Address getAssignedIp() const noexcept { return assignedIp_; }
    [[nodiscard]] tcpip::Ipv4Address getSubnetMask() const noexcept { return subnetMask_; }
    [[nodiscard]] tcpip::Ipv4Address getGateway() const noexcept { return gateway_; }
    [[nodiscard]] tcpip::Ipv4Address getServerIdentifier() const noexcept { return serverIdentifier_; }
    [[nodiscard]] uint32_t getLeaseDuration() const noexcept { return leaseDurationSeconds_; }
    [[nodiscard]] int64_t getLeaseObtainedTime() const noexcept { return leaseObtainedTime_; }
    [[nodiscard]] int64_t getLeaseExpiresTime() const noexcept { return leaseExpiresTime_; }

    /**
     * @brief Crafts an RFC 2131 DHCPDISCOVER packet.
     */
    std::vector<uint8_t> buildDiscover(uint32_t xid, ndis::MacAddress mac) {
        std::vector<uint8_t> packet(sizeof(DhcpRawHeader), 0);
        auto* hdr = reinterpret_cast<DhcpRawHeader*>(packet.data());
        hdr->op = BOOTREQUEST;
        hdr->htype = HTYPE_ETHERNET;
        hdr->hlen = 6;
        hdr->xid = tcpip::htonl(xid);
        hdr->flags = tcpip::htons(0x8000); // Broadcast
        std::memcpy(hdr->chaddr, mac.bytes, 6);
        hdr->magicCookie = tcpip::htonl(DHCP_MAGIC_COOKIE);

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
        ndis::MacAddress mac,
        tcpip::Ipv4Address requestedIp,
        tcpip::Ipv4Address serverId
    ) {
        std::vector<uint8_t> packet(sizeof(DhcpRawHeader), 0);
        auto* hdr = reinterpret_cast<DhcpRawHeader*>(packet.data());
        hdr->op = BOOTREQUEST;
        hdr->htype = HTYPE_ETHERNET;
        hdr->hlen = 6;
        hdr->xid = tcpip::htonl(xid);
        hdr->flags = tcpip::htons(0x8000);
        std::memcpy(hdr->chaddr, mac.bytes, 6);
        hdr->magicCookie = tcpip::htonl(DHCP_MAGIC_COOKIE);

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
        ndis::MacAddress mac,
        tcpip::Ipv4Address clientIp,
        tcpip::Ipv4Address serverId
    ) {
        std::vector<uint8_t> packet(sizeof(DhcpRawHeader), 0);
        auto* hdr = reinterpret_cast<DhcpRawHeader*>(packet.data());
        hdr->op = BOOTREQUEST;
        hdr->htype = HTYPE_ETHERNET;
        hdr->hlen = 6;
        hdr->xid = tcpip::htonl(xid);
        hdr->ciaddr = clientIp.addr;
        std::memcpy(hdr->chaddr, mac.bytes, 6);
        hdr->magicCookie = tcpip::htonl(DHCP_MAGIC_COOKIE);

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

        state_ = DhcpState::Init;
        return packet;
    }

    /**
     * @brief Parses an RFC 2131 DHCP packet.
     */
    static bool parseMessage(std::span<const uint8_t> data, ParsedDhcpMessage& out) {
        if (data.size() < sizeof(DhcpRawHeader)) return false;
        const auto* hdr = reinterpret_cast<const DhcpRawHeader*>(data.data());
        if (tcpip::ntohl(hdr->magicCookie) != DHCP_MAGIC_COOKIE) return false;

        out.op = hdr->op;
        out.xid = tcpip::ntohl(hdr->xid);
        std::memcpy(out.clientMac.bytes, hdr->chaddr, 6);
        out.clientIp = tcpip::Ipv4Address(hdr->ciaddr);
        out.yourIp = tcpip::Ipv4Address(hdr->yiaddr);
        out.yiaddr = out.yourIp;
        out.serverIp = tcpip::Ipv4Address(hdr->siaddr);

        size_t offset = sizeof(DhcpRawHeader);
        while (offset < data.size()) {
            uint8_t opt = data[offset++];
            if (opt == DHCP_OPT_PAD) continue;
            if (opt == DHCP_OPT_END) break;
            if (offset >= data.size()) break;
            uint8_t len = data[offset++];
            if (offset + len > data.size()) break;

            const uint8_t* val = data.data() + offset;
            if (opt == DHCP_OPT_REQUESTED_IP && len >= 4) {
                uint32_t req = 0;
                std::memcpy(&req, val, 4);
                out.requestedIp = tcpip::Ipv4Address(req);
            } else if (opt == DHCP_OPT_MESSAGE_TYPE && len >= 1) {
                out.messageType = val[0];
            } else if (opt == DHCP_OPT_SUBNET_MASK && len >= 4) {
                uint32_t mask = 0;
                std::memcpy(&mask, val, 4);
                out.subnetMask = tcpip::Ipv4Address(mask);
            } else if (opt == DHCP_OPT_ROUTER && len >= 4) {
                uint32_t gw = 0;
                std::memcpy(&gw, val, 4);
                out.router = tcpip::Ipv4Address(gw);
            } else if (opt == DHCP_OPT_SERVER_ID && len >= 4) {
                uint32_t srv = 0;
                std::memcpy(&srv, val, 4);
                out.serverIdentifier = tcpip::Ipv4Address(srv);
            } else if (opt == DHCP_OPT_LEASE_TIME && len >= 4) {
                uint32_t lt = 0;
                std::memcpy(&lt, val, 4);
                out.leaseTimeSeconds = tcpip::ntohl(lt);
            } else if (opt == DHCP_OPT_DNS_SERVER) {
                for (size_t i = 0; i + 4 <= len; i += 4) {
                    uint32_t dns = 0;
                    std::memcpy(&dns, val + i, 4);
                    out.dnsServers.push_back(tcpip::Ipv4Address(dns));
                }
            }
            offset += len;
        }

        return true;
    }

    /**
     * @brief Processes a DHCPOFFER and generates DHCPREQUEST.
     */
    bool handleOffer(const ParsedDhcpMessage& offer, std::vector<uint8_t>& outRequestPacket) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (offer.messageType != DHCPOFFER || offer.xid != currentXid_) return false;

        assignedIp_ = (!offer.yourIp.isZero()) ? offer.yourIp : offer.yiaddr;
        subnetMask_ = offer.subnetMask;
        gateway_ = offer.router;
        serverIdentifier_ = offer.serverIdentifier;
        leaseDurationSeconds_ = offer.leaseTimeSeconds;

        auto myMac = tcpip::NetworkStack::get().getMacAddress();
        outRequestPacket = buildRequest(currentXid_, myMac, assignedIp_, serverIdentifier_);
        return true;
    }

    /**
     * @brief Processes a DHCPACK and commits the network configuration.
     */
    bool handleAck(const ParsedDhcpMessage& ack) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (ack.messageType != DHCPACK || ack.xid != currentXid_) return false;

        assignedIp_ = (!ack.yourIp.isZero()) ? ack.yourIp : ack.yiaddr;
        if (!ack.subnetMask.isZero()) subnetMask_ = ack.subnetMask;
        if (!ack.router.isZero()) gateway_ = ack.router;
        if (!ack.serverIdentifier.isZero()) serverIdentifier_ = ack.serverIdentifier;
        if (ack.leaseTimeSeconds > 0) leaseDurationSeconds_ = ack.leaseTimeSeconds;

        auto now = std::chrono::system_clock::now();
        leaseObtainedTime_ = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        leaseExpiresTime_ = leaseObtainedTime_ + leaseDurationSeconds_;

        state_ = DhcpState::Bound;
        dhcpEnabled_ = true;

        // Commit configuration to NetworkStack
        auto& net = tcpip::NetworkStack::get();
        net.setLocalIp(assignedIp_, subnetMask_, gateway_);
        if (!ack.dnsServers.empty()) {
            net.setDnsServer(ack.dnsServers.front());
        }

        return true;
    }

    /**
     * @brief Simulates an autonomous complete DORA cycle for test harness or loopback.
     */
    bool performSimulatedDora(
        tcpip::Ipv4Address offerIp,
        tcpip::Ipv4Address subnet,
        tcpip::Ipv4Address router,
        tcpip::Ipv4Address dnsServer,
        uint32_t leaseSeconds = 86400
    ) {
        auto mac = tcpip::NetworkStack::get().getMacAddress();
        uint32_t xid = 0x54455354; // 'TEST'

        // 1. Discover
        auto discoverPkt = buildDiscover(xid, mac);
        if (discoverPkt.empty()) return false;

        // 2. Synthesize Offer
        ParsedDhcpMessage offer{};
        offer.op = BOOTREPLY;
        offer.xid = xid;
        offer.clientMac = mac;
        offer.yourIp = offerIp;
        offer.subnetMask = subnet;
        offer.router = router;
        offer.serverIdentifier = router;
        offer.leaseTimeSeconds = leaseSeconds;
        offer.messageType = DHCPOFFER;
        offer.dnsServers.push_back(dnsServer);

        std::vector<uint8_t> reqPkt;
        if (!handleOffer(offer, reqPkt)) return false;

        // 3. Synthesize Ack
        ParsedDhcpMessage ack = offer;
        ack.messageType = DHCPACK;

        return handleAck(ack);
    }

    /**
     * @brief Releases the currently assigned DHCP lease.
     */
    void releaseLease() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ == DhcpState::Bound) {
            auto mac = tcpip::NetworkStack::get().getMacAddress();
            (void)buildRelease(currentXid_, mac, assignedIp_, serverIdentifier_);
        }
        state_ = DhcpState::Init;
        dhcpEnabled_ = false;
        assignedIp_ = tcpip::Ipv4Address::any();
        subnetMask_ = tcpip::Ipv4Address::any();
        gateway_ = tcpip::Ipv4Address::any();
        auto& net = tcpip::NetworkStack::get();
        net.setLocalIp(tcpip::Ipv4Address::any(), tcpip::Ipv4Address::any(), tcpip::Ipv4Address::any());
    }

private:
    DhcpClientEngine() = default;

    std::mutex mutex_;
    DhcpState state_{DhcpState::Init};
    bool dhcpEnabled_{false};
    uint32_t currentXid_{0x12345678};
    tcpip::Ipv4Address assignedIp_{};
    tcpip::Ipv4Address subnetMask_{};
    tcpip::Ipv4Address gateway_{};
    tcpip::Ipv4Address serverIdentifier_{};
    uint32_t leaseDurationSeconds_{86400};
    int64_t leaseObtainedTime_{0};
    int64_t leaseExpiresTime_{0};
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
