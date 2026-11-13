#pragma once

#include <cstddef>
#include <cstdint>

// Generic hub descriptor parsing for the freestanding USB bus.
namespace micant::usb::baremetal {
struct HubInterface {
    uint8_t configuration{}, interfaceNumber{}, protocol{}, endpoint{}, interval{};
    uint16_t maxPacket{}, bytesPerInterval{};
    uint8_t maxBurst{};
    bool superspeedCompanion{};
};
[[nodiscard]] inline bool findUsb2Hub(const uint8_t* bytes, size_t length, HubInterface& output,
                                      bool superspeed = false) noexcept {
    if (!bytes || length < 9 || bytes[0] < 9 || bytes[1] != 2 || !bytes[5]) return false;
    const size_t total = bytes[2] | (size_t{bytes[3]} << 8);
    if (total < 9 || total > length) return false;
    HubInterface candidate{};
    bool active = false, expectCompanion = false;
    for (size_t offset = 0; offset < total;) {
        if (total - offset < 2) return false;
        const auto* p = bytes + offset;
        const size_t size = p[0];
        if (size < 2 || size > total - offset) return false;
        if (expectCompanion && p[1] != 0x30) expectCompanion = false;
        if (p[1] == 4) {
            if (active && candidate.endpoint && (!superspeed || candidate.superspeedCompanion)) { output = candidate; return true; }
            if (size < 9) return false;
            active = p[3] == 0 && p[5] == 9 && p[6] == 0 &&
                     (superspeed ? p[7] <= 3 : p[7] <= 1);
            candidate = {}; expectCompanion = false;
            if (active) { candidate.configuration = bytes[5]; candidate.interfaceNumber = p[2]; candidate.protocol = p[7]; }
        } else if (active && p[1] == 5) {
            if (size < 7) return false;
            const uint16_t packet = static_cast<uint16_t>(p[4] | (uint16_t{p[5]} << 8));
            if ((p[2] & 0x80u) && !(p[2] & 0x70u) && (p[2] & 15u) &&
                (p[3] & 3u) == 3 && packet && packet <= 64 && p[6]) {
                if (candidate.endpoint) return false;
                candidate.endpoint = p[2]; candidate.maxPacket = packet; candidate.interval = p[6];
                expectCompanion = true;
            }
        } else if (active && p[1] == 0x30 && expectCompanion && !candidate.superspeedCompanion) {
            if (size < 6 || p[2] > 15 || p[3]) return false;
            candidate.maxBurst = p[2];
            candidate.bytesPerInterval = static_cast<uint16_t>(p[4] | (uint16_t{p[5]} << 8));
            candidate.superspeedCompanion = true;
            expectCompanion = false;
        }
        offset += size;
    }
    if (active && candidate.endpoint && (!superspeed || candidate.superspeedCompanion)) { output = candidate; return true; }
    return false;
}

}
