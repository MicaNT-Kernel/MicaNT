#pragma once
// Canonical freestanding Ring-0 <-> Ring-3 Audio Device ABI (Intel AC'97)

#include <cstdint>

namespace micant::abi {

#pragma pack(push, 1)
struct NtAudioDeviceInfo {
    uint32_t online;
    uint16_t vendorId;
    uint16_t deviceId;
    uint8_t  pciBus;
    uint8_t  pciDev;
    uint8_t  pciFunc;
    uint8_t  channels;
    uint16_t nambar;
    uint16_t nabmbar;
    uint32_t sampleRate;
    uint32_t bitsPerSample;
    uint64_t framesPlayed;
    uint64_t bytesPlayed;
    uint32_t bdlBuffersSubmitted;
    uint32_t activeStreams;
    char     deviceName[48];
};
#pragma pack(pop)

static_assert(sizeof(NtAudioDeviceInfo) == 96, "NtAudioDeviceInfo ABI size mismatch");

} // namespace micant::abi
