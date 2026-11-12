#pragma once

#include <cstddef>
#include <cstdint>

// Descriptor parsing shared by the canonical freestanding USB bus and HID
// class owner. This consumes actual transfer bytes and has no hardware access.
namespace micant::usb::baremetal {
struct HidInterface {
    uint8_t configuration{}, interfaceNumber{}, alternate{}, protocol{};
    uint8_t endpoint{}, interval{};
    uint16_t maxPacket{}, reportDescriptorLength{};
    uint8_t maxBurst{};
    uint16_t bytesPerInterval{};
    bool superspeedCompanion{};
};
struct StorageInterface {
    uint8_t configuration{}, interfaceNumber{}, alternate{}, bulkIn{}, bulkOut{};
    uint16_t inPacket{}, outPacket{};
    uint8_t inBurst{}, outBurst{};
    bool inCompanion{}, outCompanion{};
};

struct HubInterface {
    uint8_t configuration{}, interfaceNumber{}, protocol{}, endpoint{}, interval{};
    uint16_t maxPacket{};
};
[[nodiscard]] inline bool findUsb2Hub(const uint8_t* bytes, size_t length, HubInterface& output) noexcept {
    if (!bytes || length < 9 || bytes[0] < 9 || bytes[1] != 2 || !bytes[5]) return false;
    const size_t total = bytes[2] | (size_t{bytes[3]} << 8);
    if (total < 9 || total > length) return false;
    HubInterface candidate{};
    bool active = false;
    for (size_t offset = 0; offset < total;) {
        if (total - offset < 2) return false;
        const auto* p = bytes + offset;
        const size_t size = p[0];
        if (size < 2 || size > total - offset) return false;
        if (p[1] == 4) {
            if (active && candidate.endpoint) { output = candidate; return true; }
            if (size < 9) return false;
            active = p[3] == 0 && p[5] == 9 && p[6] == 0 && (p[7] == 1 || p[7] == 2);
            candidate = {};
            if (active) { candidate.configuration = bytes[5]; candidate.interfaceNumber = p[2]; candidate.protocol = p[7]; }
        } else if (active && p[1] == 5) {
            if (size < 7) return false;
            const uint16_t packet = static_cast<uint16_t>(p[4] | (uint16_t{p[5]} << 8));
            if ((p[2] & 0x80u) && !(p[2] & 0x70u) && (p[2] & 15u) &&
                (p[3] & 3u) == 3 && packet && packet <= 64 && p[6]) {
                if (candidate.endpoint) return false;
                candidate.endpoint = p[2]; candidate.maxPacket = packet; candidate.interval = p[6];
            }
        }
        offset += size;
    }
    if (active && candidate.endpoint) { output = candidate; return true; }
    return false;
}

[[nodiscard]] inline bool findBulkStorage(const uint8_t* bytes, size_t length,
                                          StorageInterface& output) noexcept {
    if (!bytes || length < 9 || bytes[0] < 9 || bytes[1] != 2) return false;
    const size_t total = bytes[2] | (size_t{bytes[3]} << 8);
    if (total < 9 || total > length || !bytes[5]) return false;
    StorageInterface candidate{}, selected{};
    bool active = false, found = false;
    uint8_t lastBulk{};
    const auto finish = [&]() noexcept {
        if (active && candidate.bulkIn && candidate.bulkOut && !found) { selected = candidate; found = true; }
    };
    for (size_t offset = 0; offset < total;) {
        if (total - offset < 2) return false;
        const auto* p = bytes + offset;
        const size_t size = p[0];
        if (size < 2 || size > total - offset) return false;
        if (p[1] == 4) {
            finish();
            if (size < 9) return false;
            active = p[5] == 8 && p[6] == 6 && p[7] == 0x50;
            candidate = {};
            lastBulk = 0;
            if (active) { candidate.configuration = bytes[5]; candidate.interfaceNumber = p[2]; candidate.alternate = p[3]; }
        } else if (active && p[1] == 5) {
            if (size < 7) return false;
            const uint16_t packet = static_cast<uint16_t>(p[4] | (uint16_t{p[5]} << 8));
            if ((p[3] & 3u) == 2u && (p[2] & 15u) && !(p[2] & 0x70u) &&
                (packet & 0x7ffu) && !(packet & 0xf800u)) {
                if (p[2] & 0x80u) {
                    if (candidate.bulkIn) return false;
                    candidate.bulkIn = p[2]; candidate.inPacket = packet & 0x7ffu;
                    lastBulk = 1;
                } else {
                    if (candidate.bulkOut) return false;
                    candidate.bulkOut = p[2]; candidate.outPacket = packet & 0x7ffu;
                    lastBulk = 2;
                }
            } else lastBulk = 0;
        } else if (active && p[1] == 0x30 && lastBulk) {
            if (size < 6 || p[2] > 15 || p[3] || p[4] || p[5]) return false;
            if (lastBulk == 1) { candidate.inBurst = p[2]; candidate.inCompanion = true; }
            else { candidate.outBurst = p[2]; candidate.outCompanion = true; }
            lastBulk = 0;
        } else if (p[1] != 0x30) {
            lastBulk = 0;
        }
        offset += size;
    }
    finish();
    if (found) output = selected;
    return found;
}

[[nodiscard]] inline bool findBootHid(const uint8_t* bytes, size_t length,
                                      HidInterface& output) noexcept {
    if (!bytes || length < 9 || bytes[0] < 9 || bytes[1] != 2) return false;
    const size_t total = static_cast<size_t>(bytes[2]) | (static_cast<size_t>(bytes[3]) << 8);
    if (total < 9 || total > length || !bytes[5]) return false;
    HidInterface candidate{}, selected{};
    bool active = false, haveHid = false, haveEndpoint = false, found = false, lastEndpoint = false;
    const auto finish = [&]() noexcept {
        if (!active || !haveHid || !haveEndpoint) return false;
        if (!found) { selected = candidate; found = true; }
        return true;
    };
    for (size_t offset = 0; offset < total;) {
        if (total - offset < 2) return false;
        const size_t size = bytes[offset];
        const uint8_t type = bytes[offset + 1];
        if (size < 2 || size > total - offset) return false;
        const auto* p = bytes + offset;
        if (type == 4) {
            (void)finish();
            if (size < 9) return false;
            active = p[5] == 3 && p[6] == 1 && (p[7] == 1 || p[7] == 2);
            haveHid = haveEndpoint = false;
            lastEndpoint = false;
            candidate = {};
            if (active) {
                candidate.configuration = bytes[5];
                candidate.interfaceNumber = p[2]; candidate.alternate = p[3]; candidate.protocol = p[7];
            }
        } else if (active && type == 0x21) {
            if (size < 6 || p[5] == 0 || size < 6u + static_cast<size_t>(p[5]) * 3u) return false;
            for (size_t i = 0; i < p[5]; ++i) if (p[6 + i * 3] == 0x22) {
                if (haveHid) return false;
                candidate.reportDescriptorLength = static_cast<uint16_t>(p[7 + i * 3] | (uint16_t{p[8 + i * 3]} << 8));
                haveHid = candidate.reportDescriptorLength != 0;
            }
        } else if (active && type == 5) {
            if (size < 7) return false;
            lastEndpoint = false;
            const uint16_t packet = static_cast<uint16_t>(p[4] | (uint16_t{p[5]} << 8));
            if ((p[2] & 0x80u) && !(p[2] & 0x70u) && (p[3] & 3u) == 3u && (p[2] & 15u) &&
                (packet & 0x7ffu) && !(packet & 0xe000u) && p[6]) {
                if (haveEndpoint) return false;
                candidate.endpoint = p[2]; candidate.interval = p[6];
                candidate.maxPacket = packet & 0x7ffu;
                haveEndpoint = true;
                lastEndpoint = true;
            }
        } else if (active && type == 0x30 && lastEndpoint) {
            if (size < 6 || p[2] > 15 || p[3]) return false;
            candidate.maxBurst = p[2];
            candidate.bytesPerInterval = static_cast<uint16_t>(p[4] | (uint16_t{p[5]} << 8));
            candidate.superspeedCompanion = true;
            lastEndpoint = false;
        } else if (type != 0x30) {
            lastEndpoint = false;
        }
        offset += size;
    }
    (void)finish();
    if (found) output = selected;
    return found;
}

// Validates bounds of the full report descriptor, including long items. The
// HID class retains the raw bytes and performs usage/report interpretation.
[[nodiscard]] inline bool validateHidReportDescriptor(const uint8_t* bytes, size_t length) noexcept {
    if (!bytes || !length) return false;
    unsigned collections = 0;
    bool sawInput = false;
    for (size_t offset = 0; offset < length;) {
        const uint8_t prefix = bytes[offset++];
        if (prefix == 0xfe) {
            if (length - offset < 2) return false;
            const size_t size = bytes[offset]; offset += 2;
            if (size > length - offset) return false;
            offset += size;
            continue;
        }
        const size_t encoded = prefix & 3u;
        const size_t size = encoded == 3 ? 4 : encoded;
        if (size > length - offset) return false;
        const uint8_t type = (prefix >> 2) & 3u, tag = prefix >> 4;
        if (type == 0 && tag == 10) { if (++collections > 32) return false; }
        if (type == 0 && tag == 12) { if (!collections) return false; --collections; }
        if (type == 0 && tag == 8) sawInput = true;
        offset += size;
    }
    return !collections && sawInput;
}
}
