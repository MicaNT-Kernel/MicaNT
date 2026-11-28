#pragma once
// Canonical freestanding Ring-0 <-> Ring-3 Network Adapter / DNS / ICMP ABI

#include <cstdint>

namespace micant::abi {

#pragma pack(push, 1)
struct NetAdapterInfo {
    uint32_t present;
    uint32_t linkUp;
    uint16_t vendorId;
    uint16_t deviceId;
    uint8_t  pciBus;
    uint8_t  pciDev;
    uint8_t  pciFunc;
    uint8_t  reserved0;
    uint8_t  mac[6];
    uint8_t  gatewayMac[6];
    uint32_t ipv4Addr;
    uint32_t subnetMask;
    uint32_t gatewayIp;
    uint32_t dnsIp;
    uint64_t txPackets;
    uint64_t rxPackets;
    uint64_t arpResolved;
    uint64_t icmpReplies;
    uint64_t tcpHandshakes;
    uint32_t dhcpBound;
    uint32_t dhcpServerIp;
    uint32_t dhcpLeaseSeconds;
    uint32_t dhcpRenewCount;
    uint64_t udpPackets;
    uint64_t dnsResolved;
};

struct NetDnsResult {
    uint32_t resolvedIpNet;
    uint32_t ttlSeconds;
    uint32_t rttUs;
    uint32_t answerCount;
    uint32_t serverIpNet;
    uint16_t serverPort;
    uint16_t reserved;
};
#pragma pack(pop)

static_assert(sizeof(NetAdapterInfo) == 120, "NetAdapterInfo ABI size mismatch");
static_assert(sizeof(NetDnsResult) == 24, "NetDnsResult ABI size mismatch");

} // namespace micant::abi
