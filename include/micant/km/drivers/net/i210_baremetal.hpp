// ============================================================================
// MicaNT: Bare-Metal Intel I210 Gigabit Ethernet Controller Driver
// (i210_baremetal.hpp)
//
// Description:
//   Clean-room bare-metal PCIe bus-master DMA network driver for the Intel
//   I210-AT Gigabit Network Connection (0x8086:0x1533 at PCIe 02:00.0).
//   Enables physical Ethernet packet TX and RX, ARP resolution, ICMP echo
//   (ping) replies, and raw remote executive connectivity on bare server silicon.
//
// Clean-Room ISO C++23. Zero proprietary Microsoft code.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <span>
#include <cstring>
#if defined(_MSC_VER)
#include <immintrin.h>
#endif
#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"

namespace micant::km::i210_baremetal {

// --- Port I/O Primitives ---
static inline void outl(uint16_t port, uint32_t val) noexcept {
#if defined(_MSC_VER)
    (void)port; (void)val;
#elif defined(__x86_64__) || defined(__i386__)
    __asm__ __volatile__("outl %0, %1" : : "a"(val), "Nd"(port));
#else
    (void)port; (void)val;
#endif
}

static inline uint32_t inl(uint16_t port) noexcept {
    uint32_t ret = 0;
#if defined(_MSC_VER)
    (void)port;
    return 0xFFFFFFFF;
#elif defined(__x86_64__) || defined(__i386__)
    __asm__ __volatile__("inl %1, %0" : "=a"(ret) : "Nd"(port));
#else
    (void)port;
#endif
    return ret;
}

// --- PCI Configuration Space Access ---
static inline uint32_t pciRead32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg) noexcept {
    uint32_t address = (1u << 31) | (static_cast<uint32_t>(bus) << 16) |
                       (static_cast<uint32_t>(dev) << 11) |
                       (static_cast<uint32_t>(func) << 8) | (reg & 0xFC);
    outl(0xCF8, address);
    return inl(0xCFC);
}

static inline void pciWrite32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t reg, uint32_t val) noexcept {
    uint32_t address = (1u << 31) | (static_cast<uint32_t>(bus) << 16) |
                       (static_cast<uint32_t>(dev) << 11) |
                       (static_cast<uint32_t>(func) << 8) | (reg & 0xFC);
    outl(0xCF8, address);
    outl(0xCFC, val);
}

// --- Intel I210 Register Offsets ---
inline constexpr uint32_t I210_REG_CTRL      = 0x00000; // Device Control
inline constexpr uint32_t I210_REG_STATUS    = 0x00008; // Device Status
inline constexpr uint32_t I210_REG_CTRL_EXT  = 0x00018; // Extended Device Control
inline constexpr uint32_t I210_REG_ICR       = 0x000C0; // Interrupt Cause Read
inline constexpr uint32_t I210_REG_ICS       = 0x000C8; // Interrupt Cause Set
inline constexpr uint32_t I210_REG_IMS       = 0x000D0; // Interrupt Mask Set
inline constexpr uint32_t I210_REG_IMC       = 0x000D8; // Interrupt Mask Clear
inline constexpr uint32_t I210_REG_RCTL      = 0x00100; // Receive Control
inline constexpr uint32_t I210_REG_TCTL      = 0x00400; // Transmit Control
inline constexpr uint32_t I210_REG_TIPG      = 0x00410; // Transmit Inter-Packet Gap
inline constexpr uint32_t I210_REG_RAL0      = 0x05400; // Receive Address 0 Low (MAC 0..3)
inline constexpr uint32_t I210_REG_RAH0      = 0x05404; // Receive Address 0 High (MAC 4..5 + AV)

// Queue 0 Descriptors (I210 offsets)
inline constexpr uint32_t I210_REG_RDBAL     = 0x0C000; // RX Desc Base Low (Queue 0)
inline constexpr uint32_t I210_REG_RDBAH     = 0x0C004; // RX Desc Base High
inline constexpr uint32_t I210_REG_RDLEN     = 0x0C008; // RX Desc Length
inline constexpr uint32_t I210_REG_SRRCTL0   = 0x0C00C; // Split & Replication Receive Control
inline constexpr uint32_t I210_REG_RDH       = 0x0C010; // RX Desc Head
inline constexpr uint32_t I210_REG_RDT       = 0x0C018; // RX Desc Tail
inline constexpr uint32_t I210_REG_RXDCTL    = 0x0C028; // RX Desc Control

inline constexpr uint32_t I210_REG_TDBAL     = 0x0E000; // TX Desc Base Low (Queue 0)
inline constexpr uint32_t I210_REG_TDBAH     = 0x0E004; // TX Desc Base High
inline constexpr uint32_t I210_REG_TDLEN     = 0x0E008; // TX Desc Length
inline constexpr uint32_t I210_REG_TDH       = 0x0E010; // TX Desc Head
inline constexpr uint32_t I210_REG_TDT       = 0x0E018; // TX Desc Tail
inline constexpr uint32_t I210_REG_TXDCTL    = 0x0E028; // TX Desc Control

// Legacy Aliases (82574/82575 fallback)
inline constexpr uint32_t I210_LEGACY_RDBAL   = 0x02800;
inline constexpr uint32_t I210_LEGACY_RDBAH   = 0x02804;
inline constexpr uint32_t I210_LEGACY_RDLEN   = 0x02808;
inline constexpr uint32_t I210_LEGACY_SRRCTL0 = 0x0280C;
inline constexpr uint32_t I210_LEGACY_RDH     = 0x02810;
inline constexpr uint32_t I210_LEGACY_RDT     = 0x02818;
inline constexpr uint32_t I210_LEGACY_TDBAL   = 0x03800;
inline constexpr uint32_t I210_LEGACY_TDBAH   = 0x03804;
inline constexpr uint32_t I210_LEGACY_TDLEN   = 0x03808;
inline constexpr uint32_t I210_LEGACY_TDH     = 0x03810;
inline constexpr uint32_t I210_LEGACY_TDT     = 0x03818;
inline constexpr uint32_t I210_LEGACY_TXDCTL  = 0x03828;
inline constexpr uint32_t I210_LEGACY_RXDCTL  = 0x02828;

// Bitmasks
inline constexpr uint32_t CTRL_SLU            = (1u << 6);  // Set Link Up
inline constexpr uint32_t CTRL_RST            = (1u << 26); // Device Reset
inline constexpr uint32_t CTRL_EXT_DRV_LOAD   = (1u << 28); // Driver Loaded (Management NC-SI Pass-Through)
inline constexpr uint32_t STATUS_LU          = (1u << 1);  // Link Up status
inline constexpr uint32_t RCTL_EN            = (1u << 1);  // RX Enable
inline constexpr uint32_t RCTL_SBP           = (1u << 2);  // Store Bad Packets
inline constexpr uint32_t RCTL_UPE           = (1u << 3);  // Unicast Promiscuous
inline constexpr uint32_t RCTL_MPE           = (1u << 4);  // Multicast Promiscuous
inline constexpr uint32_t RCTL_BAM           = (1u << 15); // Broadcast Accept
inline constexpr uint32_t RCTL_BSIZE_2048    = (0u << 16); // 2048-byte RX buffer
inline constexpr uint32_t RCTL_SECRC         = (1u << 26); // Strip Ethernet CRC

inline constexpr uint32_t TCTL_EN            = (1u << 1);  // TX Enable
inline constexpr uint32_t TCTL_PSP           = (1u << 3);  // Pad Short Packets
inline constexpr uint32_t TCTL_CT            = (0x0Fu << 4);  // Collision Threshold
inline constexpr uint32_t TCTL_COLD          = (0x40u << 12); // Collision Distance

#pragma pack(push, 1)

// --- Legacy 16-byte Receive Descriptor ---
struct I210RxDesc {
    uint64_t buffer_addr; // Physical buffer address
    uint16_t length;      // Length of received packet
    uint16_t checksum;    // Packet checksum
    uint8_t  status;      // Bit 0: DD (Descriptor Done), Bit 1: EOP
    uint8_t  errors;      // Errors
    uint16_t special;     // Special
};

// --- Legacy 16-byte Transmit Descriptor ---
struct I210TxDesc {
    uint64_t buffer_addr; // Physical buffer address
    uint16_t length;      // Length of packet to transmit
    uint8_t  cso;         // Checksum offset
    uint8_t  cmd;         // Bit 0: EOP, Bit 1: IFCS, Bit 3: RS
    uint8_t  status;      // Bit 0: DD (Descriptor Done)
    uint8_t  css;         // Checksum start
    uint16_t special;     // Special
};

// --- Ethernet Frame Structures ---
struct EthernetHeader {
    uint8_t  dstMac[6];
    uint8_t  srcMac[6];
    uint16_t etherType; // Big-endian
};

struct ArpPacket {
    uint16_t hwType;      // 0x0001 (Ethernet)
    uint16_t protoType;   // 0x0800 (IPv4)
    uint8_t  hwSize;      // 6
    uint8_t  protoSize;   // 4
    uint16_t opcode;      // 1 = Request, 2 = Reply
    uint8_t  senderMac[6];
    uint32_t senderIp;
    uint8_t  targetMac[6];
    uint32_t targetIp;
};

struct Ipv4Header {
    uint8_t  verIhl;      // Version (4) + IHL (5)
    uint8_t  tos;         // Type of Service
    uint16_t totalLength; // Big-endian
    uint16_t id;
    uint16_t flagsFrag;
    uint8_t  ttl;
    uint8_t  protocol;    // 1 = ICMP, 6 = TCP, 17 = UDP
    uint16_t checksum;
    uint32_t srcIp;
    uint32_t dstIp;
};

struct IcmpHeader {
    uint8_t  type;        // 8 = Echo Request, 0 = Echo Reply
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t sequence;
};

struct TcpHeader {
    uint16_t srcPort;       // Big-endian
    uint16_t dstPort;       // Big-endian
    uint32_t seqNumber;     // Big-endian
    uint32_t ackNumber;     // Big-endian
    uint8_t  dataOffsetRes; // Data offset (high 4 bits in 32-bit dwords) + reserved
    uint8_t  flags;         // FIN=0x01, SYN=0x02, RST=0x04, PSH=0x08, ACK=0x10, URG=0x20
    uint16_t windowSize;    // Big-endian
    uint16_t checksum;      // Big-endian
    uint16_t urgentPointer; // Big-endian
};

struct TcpPseudoHeader {
    uint32_t srcIp;
    uint32_t dstIp;
    uint8_t  zero{0};
    uint8_t  protocol{6};
    uint16_t tcpLength{0};
};


#pragma pack(pop)

// --- Static DMA Ring Buffers ---
inline constexpr size_t NUM_RX_DESC = 64;
inline constexpr size_t NUM_TX_DESC = 64;
inline constexpr size_t RX_BUFFER_SIZE = 2048;

alignas(128) static I210RxDesc s_rxRing[NUM_RX_DESC];
alignas(128) static I210TxDesc s_txRing[NUM_TX_DESC];
alignas(4096) static uint8_t   s_rxBuffers[NUM_RX_DESC][RX_BUFFER_SIZE];
alignas(4096) static uint8_t   s_txBuffers[NUM_TX_DESC][RX_BUFFER_SIZE];


static inline uint16_t htons(uint16_t v) noexcept {
    return static_cast<uint16_t>((v << 8) | (v >> 8));
}
static inline uint16_t ntohs(uint16_t v) noexcept {
    return htons(v);
}
static inline uint32_t htonl(uint32_t v) noexcept {
    return ((v & 0x000000FF) << 24) |
           ((v & 0x0000FF00) << 8)  |
           ((v & 0x00FF0000) >> 8)  |
           ((v & 0xFF000000) >> 24);
}
static inline uint32_t ntohl(uint32_t v) noexcept {
    return htonl(v);
}

static inline uint16_t computeTcpChecksum(uint32_t srcIp, uint32_t dstIp, const void* tcpSegment, size_t tcpLen) noexcept {
    TcpPseudoHeader pseudo;
    pseudo.srcIp = srcIp;
    pseudo.dstIp = dstIp;
    pseudo.zero = 0;
    pseudo.protocol = 6;
    pseudo.tcpLength = htons(static_cast<uint16_t>(tcpLen));

    uint32_t sum = 0;
    const uint16_t* p = reinterpret_cast<const uint16_t*>(&pseudo);
    for (size_t i = 0; i < sizeof(pseudo) / 2; ++i) {
        sum += *p++;
    }

    const uint16_t* t = static_cast<const uint16_t*>(tcpSegment);
    size_t len = tcpLen;
    while (len > 1) {
        sum += *t++;
        len -= 2;
    }
    if (len == 1) {
        sum += static_cast<uint16_t>(*reinterpret_cast<const uint8_t*>(t));
    }

    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return static_cast<uint16_t>(~sum);
}

enum class TcpState : uint8_t {
    Closed = 0,
    Listen = 1,
    SynReceived = 2,
    Established = 3
};

using ShellCommandCallback = void(*)(const char* cmd, char* reply, size_t maxReply);

/**
 * @brief Sovereign Bare-Metal Intel I210 Gigabit Ethernet Controller Driver.
 */
class BareMetalI210Driver {
private:
    uint32_t m_mmioPhys{0};
    volatile uint32_t* m_mmio{nullptr};
    bool     m_initialized{false};
    bool     m_linkUp{false};
    uint8_t  m_mac[6]{0xA4, 0xBF, 0x01, 0x1E, 0xA3, 0xBE}; // Server Supermicro default MAC
    uint8_t  m_gatewayMac[6]{0x00, 0x00, 0x0C, 0x9F, 0xF0, 0x01}; // OVH Gateway Cisco HSRP MAC
    uint32_t m_ip{0xE5018793}; // 147.135.1.229 in host byte order (0x938701E5 network byte order)
    uint32_t m_rxTail{0};
    uint32_t m_txTail{0};
    uint32_t m_heartbeatCount{0};
    uint64_t m_rxPackets{0};
    uint64_t m_txPackets{0};
    uint64_t m_icmpRepliesSent{0};
    uint64_t m_arpRepliesSent{0};
    struct TcpSocket {
        TcpState state{TcpState::Listen};
        uint32_t remoteIp{0};
        uint16_t remotePort{0};
        uint8_t  remoteMac[6]{};
        uint32_t localSeq{0x20261020};
        uint32_t remoteSeq{0};
        char     rxLine[256]{};
        size_t   rxLineLen{0};
    };
    TcpSocket m_sockShell{};
    TcpSocket m_sockHttp{};

    ShellCommandCallback m_shellCb{nullptr};
    uint64_t m_tcpBytesSent{0};
    uint64_t m_tcpPacketsRecv{0};
    uint32_t m_lastIcmpSrcIp{0};
    uint32_t m_lastPingReplySrcIp{0};
    uint64_t m_icmpEchoRepliesRecv{0};
    uint32_t m_synTimeoutShell{0};
    uint32_t m_synTimeoutHttp{0};
    uint16_t m_shellActivePort{2323};
    uint32_t m_lastTcpSrcIp{0};
    uint16_t m_lastTcpDstPort{0};


    inline void writeReg(uint32_t offset, uint32_t val) noexcept {
        m_mmio[offset / 4] = val;
    }

    inline uint32_t readReg(uint32_t offset) const noexcept {
        return m_mmio[offset / 4];
    }

public:
    static BareMetalI210Driver& get() noexcept {
        static BareMetalI210Driver instance;
        return instance;
    }

    [[nodiscard]] bool isInitialized() const noexcept { return m_initialized; }
    [[nodiscard]] bool isLinkUp() const noexcept { return m_linkUp; }
    [[nodiscard]] const uint8_t* getMac() const noexcept { return m_mac; }
    [[nodiscard]] uint64_t getRxPackets() const noexcept { return m_rxPackets; }
    [[nodiscard]] uint64_t getTxPackets() const noexcept { return m_txPackets; }
    [[nodiscard]] uint64_t getIcmpReplies() const noexcept { return m_icmpRepliesSent; }
    [[nodiscard]] uint32_t getRDH() const noexcept { return m_initialized ? readReg(I210_REG_RDH) : 0; }
    [[nodiscard]] uint32_t getRDT() const noexcept { return m_initialized ? readReg(I210_REG_RDT) : 0; }
    [[nodiscard]] uint32_t getRXDCTL() const noexcept { return m_initialized ? readReg(I210_REG_RXDCTL) : 0; }
    [[nodiscard]] uint32_t getSRRCTL() const noexcept { return m_initialized ? readReg(I210_REG_SRRCTL0) : 0; }
    [[nodiscard]] uint8_t  getDesc0Status() const noexcept { return s_rxRing[0].status; }
    [[nodiscard]] uint32_t getRxTail() const noexcept { return m_rxTail; }

    void registerShellCallback(ShellCommandCallback cb) noexcept {
        m_shellCb = cb;
    }

    [[nodiscard]] bool isTcpConnected() const noexcept {
        return m_sockShell.state == TcpState::Established || m_sockHttp.state == TcpState::Established;
    }

    [[nodiscard]] uint64_t getTcpBytesSent() const noexcept { return m_tcpBytesSent; }
    [[nodiscard]] uint64_t getTcpPacketsRecv() const noexcept { return m_tcpPacketsRecv; }
    [[nodiscard]] uint32_t getLastIcmpSrcIp() const noexcept { return m_lastIcmpSrcIp; }
    [[nodiscard]] uint32_t getLastPingReplySrcIp() const noexcept { return m_lastPingReplySrcIp; }
    [[nodiscard]] uint64_t getIcmpEchoReplies() const noexcept { return m_icmpEchoRepliesRecv; }
    [[nodiscard]] uint16_t getShellActivePort() const noexcept { return m_shellActivePort; }
    [[nodiscard]] uint32_t getLastTcpSrcIp() const noexcept { return m_lastTcpSrcIp; }
    [[nodiscard]] uint16_t getLastTcpDstPort() const noexcept { return m_lastTcpDstPort; }

    [[nodiscard]] bool isLocalSubnet(uint32_t ip) const noexcept {
        return (ip & 0x00FFFFFF) == (0xE5018793 & 0x00FFFFFF);
    }

    bool sendTcpPacket(TcpSocket& sock, uint8_t flags, const void* payload = nullptr, uint16_t payloadLen = 0, uint16_t localPort = 2323) noexcept {
        alignas(16) uint8_t buf[1514];
        size_t totalLen = sizeof(EthernetHeader) + sizeof(Ipv4Header) + sizeof(TcpHeader) + payloadLen;
        if (totalLen > sizeof(buf)) return false;

        auto* eth = reinterpret_cast<EthernetHeader*>(buf);
        auto* ip  = reinterpret_cast<Ipv4Header*>(buf + sizeof(EthernetHeader));
        auto* tcp = reinterpret_cast<TcpHeader*>(buf + sizeof(EthernetHeader) + sizeof(Ipv4Header));
        uint8_t* pData = buf + sizeof(EthernetHeader) + sizeof(Ipv4Header) + sizeof(TcpHeader);

        if (isLocalSubnet(sock.remoteIp)) {
            if (sock.remoteMac[0] != 0 || sock.remoteMac[1] != 0) {
                std::memcpy(eth->dstMac, sock.remoteMac, 6);
            } else {
                std::memcpy(eth->dstMac, m_gatewayMac, 6);
            }
        } else {
            std::memcpy(eth->dstMac, m_gatewayMac, 6);
        }
        std::memcpy(eth->srcMac, m_mac, 6);
        eth->etherType = 0x0008; // 0x0800 in big-endian

        uint16_t tcpHdrLen = sizeof(TcpHeader);
        if (flags == 0x12) {
            tcpHdrLen += 4; // Add MSS 1460 option
        }

        uint16_t effPayloadLen = (flags == 0x12) ? 0 : payloadLen;
        totalLen = sizeof(EthernetHeader) + sizeof(Ipv4Header) + tcpHdrLen + effPayloadLen;

        ip->verIhl = 0x45;
        ip->tos = 0;
        ip->totalLength = htons(static_cast<uint16_t>(sizeof(Ipv4Header) + tcpHdrLen + effPayloadLen));
        ip->id = htons(static_cast<uint16_t>(m_txPackets & 0xFFFF));
        ip->flagsFrag = htons(0x4000); // DF
        ip->ttl = 64;
        ip->protocol = 6; // TCP
        ip->checksum = 0;
        ip->srcIp = 0xE5018793; // 147.135.1.229
        ip->dstIp = sock.remoteIp;
        ip->checksum = checksum16(ip, sizeof(Ipv4Header));

        tcp->srcPort = htons(localPort);
        tcp->dstPort = sock.remotePort;
        tcp->seqNumber = htonl(sock.localSeq);
        tcp->ackNumber = htonl(sock.remoteSeq);
        tcp->dataOffsetRes = (tcpHdrLen / 4) << 4;
        tcp->flags = flags;
        tcp->windowSize = htons(8192);
        tcp->checksum = 0;
        tcp->urgentPointer = 0;

        if (flags == 0x12) {
            pData[0] = 2; pData[1] = 4; pData[2] = 0x05; pData[3] = 0xB4; // MSS 1460
        } else if (payload && payloadLen > 0) {
            std::memcpy(pData, payload, payloadLen);
        }

        tcp->checksum = computeTcpChecksum(ip->srcIp, ip->dstIp, tcp, tcpHdrLen + effPayloadLen);

        if (totalLen < 60) {
            std::memset(buf + totalLen, 0, 60 - totalLen);
            totalLen = 60;
        }

        sendPacket(buf, static_cast<uint16_t>(totalLen));
        m_tcpBytesSent += payloadLen;
        return true;
    }

    void sendTcpData(const char* str, uint16_t localPort = 2323) noexcept {
        if (!str) return;
        TcpSocket& sock = (localPort == 80) ? m_sockHttp : m_sockShell;
        if (sock.state != TcpState::Established) return;
        size_t len = 0;
        while (str[len]) len++;
        if (len == 0) return;

        size_t sent = 0;
        while (sent < len) {
            size_t chunk = (len - sent > 1024) ? 1024 : (len - sent);
            sendTcpPacket(sock, 0x18 /* PSH | ACK */, str + sent, static_cast<uint16_t>(chunk), localPort);
            sock.localSeq += static_cast<uint32_t>(chunk);
            sent += chunk;
        }
    }


    bool sendPing(uint32_t targetIp) noexcept {
        if (!m_initialized) return false;
        alignas(16) uint8_t buf[sizeof(EthernetHeader) + sizeof(Ipv4Header) + sizeof(IcmpHeader) + 32];
        auto* eth  = reinterpret_cast<EthernetHeader*>(buf);
        auto* ip   = reinterpret_cast<Ipv4Header*>(buf + sizeof(EthernetHeader));
        auto* icmp = reinterpret_cast<IcmpHeader*>(buf + sizeof(EthernetHeader) + sizeof(Ipv4Header));
        char* data = reinterpret_cast<char*>(buf + sizeof(EthernetHeader) + sizeof(Ipv4Header) + sizeof(IcmpHeader));

        std::memcpy(eth->dstMac, m_gatewayMac, 6);
        std::memcpy(eth->srcMac, m_mac, 6);
        eth->etherType = 0x0008; // 0x0800 in big-endian

        uint16_t totalLen = sizeof(buf);
        ip->verIhl = 0x45;
        ip->tos = 0;
        ip->totalLength = htons(static_cast<uint16_t>(sizeof(Ipv4Header) + sizeof(IcmpHeader) + 32));
        ip->id = htons(static_cast<uint16_t>(m_txPackets & 0xFFFF));
        ip->flagsFrag = 0;
        ip->ttl = 64;
        ip->protocol = 1; // ICMP
        ip->checksum = 0;
        ip->srcIp = 0xE5018793; // 147.135.1.229
        ip->dstIp = targetIp;
        ip->checksum = checksum16(ip, sizeof(Ipv4Header));

        static uint16_t s_pingSeq = 1;
        icmp->type = 8; // Echo Request
        icmp->code = 0;
        icmp->checksum = 0;
        icmp->id = htons(0x4D49); // "MI"
        icmp->sequence = htons(s_pingSeq++);

        const char pingPayload[] = "MicaNT Sovereign BareMetal Silicon";
        std::memcpy(data, pingPayload, 32);

        icmp->checksum = checksum16(icmp, sizeof(IcmpHeader) + 32);

        sendPacket(buf, totalLen);
        return true;
    }

    bool initialize() noexcept {
        // 1. Discover Intel I210 on PCIe Bus 02, Dev 00, Func 00
        uint32_t vendorDev = pciRead32(2, 0, 0, 0x00);
        uint16_t vendorId = vendorDev & 0xFFFF;
        uint16_t deviceId = (vendorDev >> 16) & 0xFFFF;

        if (vendorId != 0x8086 || deviceId != 0x1533) {
            // Also probe Bus 03 (LAN 2 fallback)
            vendorDev = pciRead32(3, 0, 0, 0x00);
            vendorId = vendorDev & 0xFFFF;
            deviceId = (vendorDev >> 16) & 0xFFFF;
            if (vendorId != 0x8086 || deviceId != 0x1533) {
                return false;
            }
        }

        uint8_t targetBus = (vendorDev == pciRead32(2, 0, 0, 0x00)) ? 2 : 3;

        // 2. Read BAR0 (MMIO Base)
        uint32_t bar0 = pciRead32(targetBus, 0, 0, 0x10);
        m_mmioPhys = bar0 & ~0xFULL;
        if (m_mmioPhys == 0) {
            return false;
        }

        // 3. Enable Bus Master, Memory Space, and I/O Space in PCI Command
        uint32_t pciCmd = pciRead32(targetBus, 0, 0, 0x04);
        pciWrite32(targetBus, 0, 0, 0x04, pciCmd | 0x07);

        // 4. Map MMIO pointer
        m_mmio = reinterpret_cast<volatile uint32_t*>(static_cast<uintptr_t>(m_mmioPhys));

        // 5. Disable interrupts
        writeReg(I210_REG_IMC, 0xFFFFFFFF);
        (void)readReg(I210_REG_ICR);

        // 6. Read Hardware MAC Address from RAL0/RAH0
        uint32_t ral0 = readReg(I210_REG_RAL0);
        uint32_t rah0 = readReg(I210_REG_RAH0);
        if (rah0 & (1u << 31)) { // Address Valid bit
            m_mac[0] = static_cast<uint8_t>(ral0 & 0xFF);
            m_mac[1] = static_cast<uint8_t>((ral0 >> 8) & 0xFF);
            m_mac[2] = static_cast<uint8_t>((ral0 >> 16) & 0xFF);
            m_mac[3] = static_cast<uint8_t>((ral0 >> 24) & 0xFF);
            m_mac[4] = static_cast<uint8_t>(rah0 & 0xFF);
            m_mac[5] = static_cast<uint8_t>((rah0 >> 8) & 0xFF);
        } else {
            // Program known hardware MAC
            uint32_t wRal = static_cast<uint32_t>(m_mac[0]) |
                            (static_cast<uint32_t>(m_mac[1]) << 8) |
                            (static_cast<uint32_t>(m_mac[2]) << 16) |
                            (static_cast<uint32_t>(m_mac[3]) << 24);
            uint32_t wRah = static_cast<uint32_t>(m_mac[4]) |
                            (static_cast<uint32_t>(m_mac[5]) << 8) |
                            (1u << 31);
            writeReg(I210_REG_RAL0, wRal);
            writeReg(I210_REG_RAH0, wRah);
        }

        // 6b. Inform Management Controller / NC-SI that Host Driver is Loaded
        uint32_t ctrlExt = readReg(I210_REG_CTRL_EXT);
        writeReg(I210_REG_CTRL_EXT, ctrlExt | CTRL_EXT_DRV_LOAD);

        // 6c. Clear Multicast Table Array (MTA)
        for (uint32_t m = 0; m < 128; ++m) {
            writeReg(0x05200 + m * 4, 0);
        }

        // 7. Assert Set Link Up (SLU) in Device Control
        uint32_t ctrl = readReg(I210_REG_CTRL);
        writeReg(I210_REG_CTRL, ctrl | CTRL_SLU);

        // 8. Initialize Transmit Descriptors & Ring
        std::memset(s_txRing, 0, sizeof(s_txRing));
        uint64_t txRingPhys = reinterpret_cast<uint64_t>(s_txRing);
        writeReg(I210_REG_TDBAL, static_cast<uint32_t>(txRingPhys & 0xFFFFFFFF));
        writeReg(I210_REG_TDBAH, static_cast<uint32_t>((txRingPhys >> 32) & 0xFFFFFFFF));
        writeReg(I210_REG_TDLEN, sizeof(s_txRing));
        writeReg(I210_LEGACY_TDBAL, static_cast<uint32_t>(txRingPhys & 0xFFFFFFFF));
        writeReg(I210_LEGACY_TDBAH, static_cast<uint32_t>((txRingPhys >> 32) & 0xFFFFFFFF));
        writeReg(I210_LEGACY_TDLEN, sizeof(s_txRing));

        // Enable TX Queue and poll until hardware confirms
        writeReg(I210_REG_TXDCTL, (1u << 25));
        writeReg(I210_LEGACY_TXDCTL, (1u << 25));
        for (int p = 0; p < 1000; ++p) {
            if ((readReg(I210_REG_TXDCTL) | readReg(I210_LEGACY_TXDCTL)) & (1u << 25)) break;
        }
        writeReg(I210_REG_TDH, 0);
        writeReg(I210_REG_TDT, 0);
        writeReg(I210_LEGACY_TDH, 0);
        writeReg(I210_LEGACY_TDT, 0);

        writeReg(I210_REG_TIPG, 10 | (10 << 10) | (10 << 20));
        writeReg(I210_REG_TCTL, TCTL_EN | TCTL_PSP | TCTL_CT | TCTL_COLD);

        // 9. Initialize Receive Descriptors & Ring
        std::memset(s_rxRing, 0, sizeof(s_rxRing));
        for (size_t i = 0; i < NUM_RX_DESC; ++i) {
            uint64_t bufPhys = reinterpret_cast<uint64_t>(&s_rxBuffers[i][0]);
            s_rxRing[i].buffer_addr = bufPhys;
            s_rxRing[i].status = 0;
        }

        uint64_t rxRingPhys = reinterpret_cast<uint64_t>(s_rxRing);
        writeReg(I210_REG_RDBAL, static_cast<uint32_t>(rxRingPhys & 0xFFFFFFFF));
        writeReg(I210_REG_RDBAH, static_cast<uint32_t>((rxRingPhys >> 32) & 0xFFFFFFFF));
        writeReg(I210_REG_RDLEN, sizeof(s_rxRing));
        writeReg(I210_LEGACY_RDBAL, static_cast<uint32_t>(rxRingPhys & 0xFFFFFFFF));
        writeReg(I210_LEGACY_RDBAH, static_cast<uint32_t>((rxRingPhys >> 32) & 0xFFFFFFFF));
        writeReg(I210_LEGACY_RDLEN, sizeof(s_rxRing));

        // Configure SRRCTL (2 KB buffer, drop enable on queue full, DESCTYPE=000b Legacy)
        writeReg(I210_REG_SRRCTL0, 2 | (1u << 31));
        writeReg(I210_LEGACY_SRRCTL0, 2 | (1u << 31));

        // Enable RX Queue and poll until hardware confirms (Section 4.6.4)
        writeReg(I210_REG_RXDCTL, (1u << 25));
        writeReg(I210_LEGACY_RXDCTL, (1u << 25));
        for (int p = 0; p < 1000; ++p) {
            if ((readReg(I210_REG_RXDCTL) | readReg(I210_LEGACY_RXDCTL)) & (1u << 25)) break;
        }

        // Set Head & Tail AFTER Queue is Enabled!
        writeReg(I210_REG_RDH, 0);
        writeReg(I210_REG_RDT, static_cast<uint32_t>(NUM_RX_DESC - 1));
        writeReg(I210_LEGACY_RDH, 0);
        writeReg(I210_LEGACY_RDT, static_cast<uint32_t>(NUM_RX_DESC - 1));

        writeReg(I210_REG_RCTL, RCTL_EN | RCTL_UPE | RCTL_MPE | RCTL_BAM | RCTL_BSIZE_2048 | RCTL_SECRC);

        // 10. Check Link Status
        uint32_t status = readReg(I210_REG_STATUS);
        m_linkUp = (status & STATUS_LU) != 0;
        m_initialized = true;

        if (m_linkUp) {
            sendGratuitousArp();
            probeGateway();
        }

        return true;
    }

    void sendGratuitousArp() noexcept {
        alignas(16) uint8_t buf[sizeof(EthernetHeader) + sizeof(ArpPacket)];
        auto* eth = reinterpret_cast<EthernetHeader*>(buf);
        auto* arp = reinterpret_cast<ArpPacket*>(buf + sizeof(EthernetHeader));

        std::memset(eth->dstMac, 0xFF, 6);
        std::memcpy(eth->srcMac, m_mac, 6);
        eth->etherType = 0x0608; // 0x0806 big-endian

        arp->hwType = 0x0100;    // 1 (Ethernet)
        arp->protoType = 0x0008; // 0x0800 (IPv4)
        arp->hwSize = 6;
        arp->protoSize = 4;
        arp->opcode = 0x0100;    // 1 (Request)
        std::memcpy(arp->senderMac, m_mac, 6);
        arp->senderIp = 0xE5018793;  // 147.135.1.229
        std::memset(arp->targetMac, 0xFF, 6);
        arp->targetIp = 0xE5018793;  // 147.135.1.229

        sendPacket(buf, sizeof(buf));
    }

    void probeGateway() noexcept {
        alignas(16) uint8_t buf[sizeof(EthernetHeader) + sizeof(ArpPacket)];
        auto* eth = reinterpret_cast<EthernetHeader*>(buf);
        auto* arp = reinterpret_cast<ArpPacket*>(buf + sizeof(EthernetHeader));

        std::memset(eth->dstMac, 0xFF, 6);
        std::memcpy(eth->srcMac, m_mac, 6);
        eth->etherType = 0x0608;

        arp->hwType = 0x0100;
        arp->protoType = 0x0008;
        arp->hwSize = 6;
        arp->protoSize = 4;
        arp->opcode = 0x0100; // Request
        std::memcpy(arp->senderMac, m_mac, 6);
        arp->senderIp = 0xE5018793;  // 147.135.1.229
        std::memset(arp->targetMac, 0x00, 6);
        arp->targetIp = 0xFE018793;  // 147.135.1.254 (Gateway)

        sendPacket(buf, sizeof(buf));
    }

    void heartbeat() noexcept {
        if (!m_initialized || !m_linkUp) return;
        m_heartbeatCount++;
        // Maintain continuous ARP keepalive with Cisco HSRP Gateway
        sendGratuitousArp();
        probeGateway();

        // Expire half-open SYN states if remote client did not complete 3-way handshake within ~5 seconds
        if (m_sockShell.state == TcpState::SynReceived) {
            if (++m_synTimeoutShell > 5) {
                m_sockShell.state = TcpState::Listen;
                m_sockShell.rxLineLen = 0;
                m_synTimeoutShell = 0;
            }
        } else {
            m_synTimeoutShell = 0;
        }

        if (m_sockHttp.state == TcpState::SynReceived) {
            if (++m_synTimeoutHttp > 5) {
                m_sockHttp.state = TcpState::Listen;
                m_synTimeoutHttp = 0;
            }
        } else {
            m_synTimeoutHttp = 0;
        }
    }

    /**
     * @brief Transmit an Ethernet frame via bus-master DMA.
     */
    bool sendPacket(const void* data, uint16_t length) noexcept {
        if (!m_initialized || !data || length == 0 || length > RX_BUFFER_SIZE) {
            return false;
        }

        uint16_t txLen = (length < 60) ? 60 : length; // Guarantee minimum IEEE 802.3 frame size (prevents datacenter switch drops)

        uint32_t cur = m_txTail;
        uint32_t next = (cur + 1) % NUM_TX_DESC;

        if (txLen > length) {
            std::memset(s_txBuffers[cur], 0, txLen);
        }
        std::memcpy(s_txBuffers[cur], data, length);

        s_txRing[cur].buffer_addr = reinterpret_cast<uint64_t>(s_txBuffers[cur]);
        s_txRing[cur].length = txLen;
        s_txRing[cur].cmd = (1 << 0) | (1 << 1) | (1 << 3); // EOP | IFCS | RS
        s_txRing[cur].status = 0;
        s_txRing[cur].cso = 0;
        s_txRing[cur].css = 0;
        s_txRing[cur].special = 0;

        m_txTail = next;
        writeReg(I210_REG_TDT, next);
        writeReg(I210_LEGACY_TDT, next);
        m_txPackets++;

        return true;
    }

    /**
     * @brief Poll for incoming Ethernet packets and dispatch live ARP/ICMP handlers.
     */
    void pollRx() noexcept {
        if (!m_initialized) return;

        while ((s_rxRing[m_rxTail].status & 1) != 0) {
#if defined(_MSC_VER)
            _mm_lfence();
#elif defined(__x86_64__)
            __asm__ __volatile__("lfence" ::: "memory");
#endif
            volatile I210RxDesc* desc = &s_rxRing[m_rxTail];
            uint16_t pktLen = desc->length;
            if (pktLen >= 14 && pktLen <= RX_BUFFER_SIZE) {
                m_rxPackets++;
                const uint8_t* pkt = s_rxBuffers[m_rxTail];
                handlePacket(pkt, pktLen);
            }

            // Re-arm descriptor
            desc->status = 0;
#if defined(_MSC_VER)
            _mm_mfence();
#elif defined(__x86_64__)
            __asm__ __volatile__("mfence" ::: "memory");
#endif
            uint32_t prevTail = m_rxTail;
            m_rxTail = (m_rxTail + 1) % NUM_RX_DESC;
            writeReg(I210_REG_RDT, prevTail);
            writeReg(I210_LEGACY_RDT, prevTail);
        }
    }

private:
    static inline uint16_t checksum16(const void* data, size_t len) noexcept {
        const uint16_t* ptr = static_cast<const uint16_t*>(data);
        uint32_t sum = 0;
        while (len > 1) {
            sum += *ptr++;
            len -= 2;
        }
        if (len == 1) {
            sum += *reinterpret_cast<const uint8_t*>(ptr);
        }
        while (sum >> 16) {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }
        return static_cast<uint16_t>(~sum);
    }

    void handlePacket(const uint8_t* pkt, uint16_t len) noexcept {
        if (len < sizeof(EthernetHeader)) return;
        const auto* eth = reinterpret_cast<const EthernetHeader*>(pkt);
        uint16_t etherType = (static_cast<uint16_t>(pkt[12]) << 8) | pkt[13];

        if (etherType == 0x0806 && len >= sizeof(EthernetHeader) + sizeof(ArpPacket)) {
            // --- ARP (RFC 826) ---
            const auto* arp = reinterpret_cast<const ArpPacket*>(pkt + sizeof(EthernetHeader));
            uint16_t op = (static_cast<uint16_t>(pkt[sizeof(EthernetHeader) + 6]) << 8) | pkt[sizeof(EthernetHeader) + 7];
            if (op == 1) { // ARP Request
                // Check if target IP matches our IP (147.135.1.229)
                uint32_t myIp = 0xE5018793; // 147.135.1.229
                if (std::memcmp(&arp->targetIp, &myIp, 4) == 0) {
                    alignas(16) uint8_t replyBuf[sizeof(EthernetHeader) + sizeof(ArpPacket)];
                    auto* rEth = reinterpret_cast<EthernetHeader*>(replyBuf);
                    auto* rArp = reinterpret_cast<ArpPacket*>(replyBuf + sizeof(EthernetHeader));

                    std::memcpy(rEth->dstMac, eth->srcMac, 6);
                    std::memcpy(rEth->srcMac, m_mac, 6);
                    rEth->etherType = eth->etherType;

                    rArp->hwType = arp->hwType;
                    rArp->protoType = arp->protoType;
                    rArp->hwSize = 6;
                    rArp->protoSize = 4;
                    rArp->opcode = 0x0200; // Opcode 2 (Reply) in big-endian
                    std::memcpy(rArp->senderMac, m_mac, 6);
                    rArp->senderIp = myIp;
                    std::memcpy(rArp->targetMac, arp->senderMac, 6);
                    rArp->targetIp = arp->senderIp;

                    sendPacket(replyBuf, sizeof(replyBuf));
                    m_arpRepliesSent++;
                }
            } else if (op == 2) { // ARP Reply
                // Note: OVH Datacenter switch fabric requires the static Cisco HSRP Virtual MAC
                // (00:00:0c:9f:f0:01). Never overwrite m_gatewayMac with router physical interface MACs.
            }
        } else if (etherType == 0x0800 && len >= sizeof(EthernetHeader) + sizeof(Ipv4Header)) {
            // --- IPv4 (RFC 791) ---
            const uint8_t* ipPtr = pkt + sizeof(EthernetHeader);
            const auto* ip = reinterpret_cast<const Ipv4Header*>(ipPtr);
            uint8_t ihl = (ip->verIhl & 0x0F) * 4;
            if (ihl < 20 || len < sizeof(EthernetHeader) + ihl) return;

            if (ip->protocol == 1) { // ICMP (RFC 792)
                const uint8_t* icmpPtr = ipPtr + ihl;
                uint16_t icmpLen = len - (sizeof(EthernetHeader) + ihl);
                if (icmpLen >= sizeof(IcmpHeader)) {
                    const auto* icmp = reinterpret_cast<const IcmpHeader*>(icmpPtr);
                    if (icmp->type == 8) { // Echo Request (Ping)
                        m_lastIcmpSrcIp = ip->srcIp;
                        alignas(16) uint8_t replyBuf[1514];
                        if (len <= sizeof(replyBuf)) {
                            std::memcpy(replyBuf, pkt, len);
                            auto* rEth = reinterpret_cast<EthernetHeader*>(replyBuf);
                            auto* rIp = reinterpret_cast<Ipv4Header*>(replyBuf + sizeof(EthernetHeader));
                            auto* rIcmp = reinterpret_cast<IcmpHeader*>(replyBuf + sizeof(EthernetHeader) + ihl);

                            // Route reply: local subnet directly to srcMac; WAN traffic to Cisco HSRP Gateway MAC
                            if (isLocalSubnet(ip->srcIp)) {
                                std::memcpy(rEth->dstMac, eth->srcMac, 6);
                            } else {
                                std::memcpy(rEth->dstMac, m_gatewayMac, 6);
                            }
                            std::memcpy(rEth->srcMac, m_mac, 6);

                            // Swap IPs
                            rIp->dstIp = ip->srcIp;
                            rIp->srcIp = ip->dstIp;
                            rIp->ttl = 64;
                            rIp->checksum = 0;
                            rIp->checksum = checksum16(rIp, ihl);

                            // Echo Reply
                            rIcmp->type = 0; // Echo Reply
                            rIcmp->checksum = 0;
                            rIcmp->checksum = checksum16(rIcmp, icmpLen);

                            uint16_t txLen = len;
                            if (txLen < 60) {
                                std::memset(replyBuf + txLen, 0, 60 - txLen);
                                txLen = 60;
                            }

                            sendPacket(replyBuf, txLen);
                            m_icmpRepliesSent++;
                        }
                    } else if (icmp->type == 0) { // Echo Reply
                        m_icmpEchoRepliesRecv++;
                        m_lastPingReplySrcIp = ip->srcIp;
                    }
                }
            } else if (ip->protocol == 6) { // TCP (RFC 793) Port 2323/22/23 Remote Shell & Port 80 HTTP
                const uint8_t* tcpPtr = ipPtr + ihl;
                uint16_t tcpSegLen = len - (sizeof(EthernetHeader) + ihl);
                if (tcpSegLen < sizeof(TcpHeader)) return;
                const auto* tcp = reinterpret_cast<const TcpHeader*>(tcpPtr);

                uint16_t dstPort = ntohs(tcp->dstPort);
                if (dstPort != 2323 && dstPort != 22 && dstPort != 23 && dstPort != 80) return; // Support Port 2323, 22, 23 & 80

                m_lastTcpSrcIp = ip->srcIp;
                m_lastTcpDstPort = dstPort;
                m_tcpPacketsRecv++;
                TcpSocket& sock = (dstPort == 80) ? m_sockHttp : m_sockShell;
                if (dstPort != 80) {
                    m_shellActivePort = dstPort;
                }

                uint8_t flags = tcp->flags;
                uint32_t seq = ntohl(tcp->seqNumber);
                uint8_t headerLen = ((tcp->dataOffsetRes >> 4) & 0x0F) * 4;
                if (headerLen < sizeof(TcpHeader) || tcpSegLen < headerLen) return;
                uint16_t payloadLen = tcpSegLen - headerLen;
                const uint8_t* payload = tcpPtr + headerLen;

                // Handle RST
                if (flags & 0x04) {
                    if (sock.remoteIp == ip->srcIp && sock.remotePort == tcp->srcPort) {
                        sock.state = TcpState::Listen;
                        sock.rxLineLen = 0;
                    }
                    return;
                }

                // Handle SYN (New Connection)
                if (flags & 0x02) {
                    sock.remoteIp = ip->srcIp;
                    sock.remotePort = tcp->srcPort;
                    if (isLocalSubnet(ip->srcIp)) {
                        std::memcpy(sock.remoteMac, eth->srcMac, 6);
                    } else {
                        std::memcpy(sock.remoteMac, m_gatewayMac, 6);
                    }
                    sock.remoteSeq = seq + 1;
                    sock.localSeq = (dstPort == 80) ? 0x80801020 : 0x20261020;
                    sock.state = TcpState::SynReceived;

                    sendTcpPacket(sock, 0x12 /* SYN | ACK */, nullptr, 0, dstPort);
                    sock.localSeq++;
                    return;
                }

                // For existing sessions, guarantee packet matches the active connection
                if (sock.state != TcpState::Listen) {
                    if (sock.remoteIp != ip->srcIp || sock.remotePort != tcp->srcPort) {
                        return; // Ignore scanner packets from other IPs
                    }
                }

                // Handle Handshake ACK
                if (sock.state == TcpState::SynReceived && (flags & 0x10)) {
                    sock.state = TcpState::Established;
                    if (dstPort == 2323 || dstPort == 22 || dstPort == 23) {
                        const char* banner =
                            "\r\n=======================================================\r\n"
                            "  MicaNT Cutler Edition - Sovereign Kernel Executive\r\n"
                            "  Intel Xeon E3-1270 v6 @ 3.80GHz | 32 GB ECC RAM\r\n"
                            "  AHCI SATA 2.0 TB HGST Ultrastar (C: 128G, D: 1.7T)\r\n"
                            "  Intel I210 Gigabit Ethernet (8086:1533 at PCIe 02:00.0)\r\n"
                            "  Standard NT I/O Subsystem (Dave Cutler Packet IRP Model)\r\n"
                            "=======================================================\r\n"
                            "Connected to Sovereign Remote Kernel Shell on bare-metal silicon.\r\n"
                            "Commands: 'dir', 'drives', 'c:', 'd:', 'type <file>', 'write <file> <text>',\r\n"
                            "          'copy <src> <dst>', 'del <file>', 'net', 'io', 'smp', 'uptime', 'reboot'\r\n\r\n"
                            "micant:\\> ";
                        sendTcpData(banner, dstPort);
                        return;
                    }
                }

                // Handle Established HTTP
                if (dstPort == 80 && sock.state == TcpState::Established && payloadLen > 0) {
                    sock.remoteSeq = seq + payloadLen;
                    sendTcpPacket(sock, 0x10 /* ACK */, nullptr, 0, 80);

                    const char* httpRes =
                        "HTTP/1.1 200 OK\r\n"
                        "Content-Type: text/html\r\n"
                        "Connection: close\r\n\r\n"
                        "<!DOCTYPE html><html>"
                        "<head><title>MicaNT Cutler Edition - Sovereign Server</title>"
                        "<style>body{background:#0a0e17;color:#00d4ff;font-family:monospace;padding:30px;}"
                        "h1{color:#ffd700;}hr{border-color:#334466;}</style></head>"
                        "<body><h1>MicaNT Cutler Edition</h1>"
                        "<h3>Sovereign Bare-Metal NT Kernel Executive</h3>"
                        "<p><b>Processor:</b> Intel Xeon E3-1270 v6 @ 3.80GHz (4C/8T)</p>"
                        "<p><b>Memory:</b> 32,768 MB ECC DDR4-2400</p>"
                        "<p><b>Storage:</b> AHCI SATA Port 1: 2.0 TB HGST Ultrastar</p>"
                        "<ul><li>Volume C: 128 GB FAT32 (OS System Root)</li>"
                        "<li>Volume D: 1.7 TB FAT32 (High-Capacity Storage)</li></ul>"
                        "<p><b>Network:</b> Intel I210 Gigabit Ethernet (8086:1533 at PCIe 02:00.0)</p>"
                        "<p><b>Executive Status:</b> Ring 0 Executive | User Mode Ring 3 Active | LAPIC 100Hz</p>"
                        "<hr><p>Serving live from bare-metal silicon in OVH Beauharnois Datacenter.</p>"
                        "</body></html>\r\n";
                    sendTcpData(httpRes, 80);
                    sendTcpPacket(sock, 0x11 /* FIN | ACK */, nullptr, 0, 80);
                    sock.state = TcpState::Listen;
                    return;
                }

                // Handle Established Telnet/SSH Remote Shell
                if ((dstPort == 2323 || dstPort == 22 || dstPort == 23) && sock.state == TcpState::Established) {
                    if (flags & 0x01) { // FIN
                        sock.remoteSeq = seq + 1;
                        sendTcpPacket(sock, 0x11 /* FIN | ACK */, nullptr, 0, dstPort);
                        sock.state = TcpState::Listen;
                        sock.rxLineLen = 0;
                        return;
                    }

                    if (payloadLen > 0) {
                        sock.remoteSeq = seq + payloadLen;
                        sendTcpPacket(sock, 0x10 /* ACK */, nullptr, 0, dstPort);

                        for (uint16_t i = 0; i < payloadLen; ++i) {
                            char c = static_cast<char>(payload[i]);
                            if (static_cast<uint8_t>(c) == 0xFF) {
                                if (i + 2 < payloadLen) i += 2;
                                continue;
                            }
                            if (c == '\r' || c == '\n') {
                                if (sock.rxLineLen > 0) {
                                    sock.rxLine[sock.rxLineLen] = '\0';
                                    sendTcpData("\r\n", dstPort);
                                    if (m_shellCb) {
                                        char reply[512]{};
                                        m_shellCb(sock.rxLine, reply, sizeof(reply));
                                        sendTcpData(reply, dstPort);
                                    }
                                    sock.rxLineLen = 0;
                                }
                                sendTcpData("\r\nmicant:\\> ", dstPort);
                            } else if (c == '\b' || c == 0x7F) {
                                if (sock.rxLineLen > 0) {
                                    sock.rxLineLen--;
                                    sendTcpData("\b \b", dstPort);
                                }
                            } else if (sock.rxLineLen + 1 < sizeof(sock.rxLine) && c >= 32 && c <= 126) {
                                sock.rxLine[sock.rxLineLen++] = c;
                                char echoBuf[2] = {c, '\0'};
                                sendTcpData(echoBuf, dstPort);
                            }
                        }
                    }
                }
            }
        }
    }
};

} // namespace micant::km::i210_baremetal