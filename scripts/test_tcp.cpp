#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>

static inline uint16_t htons(uint16_t v) noexcept {
    return static_cast<uint16_t>((v << 8) | (v >> 8));
}
static inline uint32_t htonl(uint32_t v) noexcept {
    return ((v & 0x000000FF) << 24) |
           ((v & 0x0000FF00) << 8)  |
           ((v & 0x00FF0000) >> 8)  |
           ((v & 0xFF000000) >> 24);
}

#pragma pack(push, 1)
struct TcpPseudoHeader {
    uint32_t srcIp;
    uint32_t dstIp;
    uint8_t  zero{0};
    uint8_t  protocol{6};
    uint16_t tcpLength{0};
};

struct TcpHeader {
    uint16_t srcPort;
    uint16_t dstPort;
    uint32_t seqNumber;
    uint32_t ackNumber;
    uint8_t  dataOffsetRes;
    uint8_t  flags;
    uint16_t windowSize;
    uint16_t checksum;
    uint16_t urgentPointer;
};
#pragma pack(pop)

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

int main() {
    uint8_t src[4] = {147, 135, 1, 229};
    uint8_t dst[4] = {40, 160, 53, 129};
    uint32_t srcIp, dstIp;
    std::memcpy(&srcIp, src, 4);
    std::memcpy(&dstIp, dst, 4);

    uint8_t buf[64]{};
    auto* tcp = reinterpret_cast<TcpHeader*>(buf);
    uint8_t* opts = buf + sizeof(TcpHeader);

    tcp->srcPort = htons(2323);
    tcp->dstPort = htons(54321);
    tcp->seqNumber = htonl(0x20261020);
    tcp->ackNumber = htonl(1001);
    tcp->dataOffsetRes = (24 / 4) << 4;
    tcp->flags = 0x12; // SYN | ACK
    tcp->windowSize = htons(8192);
    tcp->checksum = 0;
    tcp->urgentPointer = 0;

    opts[0] = 2; opts[1] = 4; opts[2] = 0x05; opts[3] = 0xB4;

    uint16_t ck = computeTcpChecksum(srcIp, dstIp, buf, 24);
    tcp->checksum = ck;

    std::printf("C++ ck: 0x%04X\n", ck);
    std::printf("In memory bytes at checksum offset: 0x%02X 0x%02X\n",
                buf[16], buf[17]);

    // Check if on wire bytes match Python's 0x73 0x15
    if (buf[16] == 0x73 && buf[17] == 0x15) {
        std::printf("MATCHES EXACT PYTHON BYTES!\n");
        return 0;
    } else {
        std::printf("MISMATCH! Expected 0x73 0x15, got 0x%02X 0x%02X\n", buf[16], buf[17]);
        return 1;
    }
}
