/**
 * @file winmm.cpp
 * @brief Sovereign Win32 Multimedia Audio DLL (C:\Windows\System32\winmm.dll) for MicaNT.
 *
 * Freestanding ISO C++23. Clean-Room design. ZERO direct `syscall` instructions!
 * Dynamically imports native NtAudio* gateways strictly from ntdll.dll via PE32+ IAT:
 *   App (sndrec.exe) -> winmm.dll (IAT) -> ntdll.dll (IAT) -> Ring 0 Intel AC'97 PCIe DMA!
 *
 * Exports authentic Win32 Waveform Audio APIs:
 *   waveOutGetNumDevs, waveOutOpen, waveOutPrepareHeader, waveOutWrite,
 *   waveOutUnprepareHeader, waveOutClose, QueryAudioDeviceMica, PlayToneMica.
 */

#include <cstdint>
#include <cstddef>

extern "C" {

__declspec(dllimport) uint64_t NtAudioQueryDevice(void* outInfoBuf, uint32_t bufLen) noexcept;
__declspec(dllimport) uint64_t NtAudioOpenStream(void** outStreamHandle, uint32_t sampleRate, uint32_t channels, uint32_t bitsPerSample) noexcept;
__declspec(dllimport) int32_t  NtAudioWritePcm(void* hStream, const void* pcmBuf, uint32_t byteLen) noexcept;
__declspec(dllimport) uint64_t NtAudioCloseStream(void* hStream) noexcept;
__declspec(dllimport) uint64_t NtAllocateVirtualMemory(void* proc, void** base, uint64_t zeroBits, size_t* size, uint32_t allocType, uint32_t protect) noexcept;
__declspec(dllimport) uint64_t NtFreeVirtualMemory(void* proc, void** base, size_t* size, uint32_t freeType) noexcept;

#pragma pack(push, 1)
struct WaveFormatEx {
    uint16_t wFormatTag;      // 1 = WAVE_FORMAT_PCM
    uint16_t nChannels;       // 2 = Stereo
    uint32_t nSamplesPerSec;  // 48000 Hz
    uint32_t nAvgBytesPerSec;
    uint16_t nBlockAlign;
    uint16_t wBitsPerSample;  // 16-bit
    uint16_t cbSize;
};

struct WaveHdr {
    char*    lpData;
    uint32_t dwBufferLength;
    uint32_t dwBytesRecorded;
    uint64_t dwUser;
    uint32_t dwFlags;
    uint32_t dwLoops;
    WaveHdr* lpNext;
    uint64_t reserved;
};

struct NtAudioDeviceInfo {
    uint32_t present;
    uint32_t sampleRate;
    uint16_t vendorId;
    uint16_t deviceId;
    uint8_t  pciBus;
    uint8_t  pciDev;
    uint8_t  pciFunc;
    uint8_t  channels;
    uint16_t bitsPerSample;
    uint16_t nambarPort;
    uint16_t nabmbarPort;
    uint16_t vraSupported;
    uint64_t dmaBuffersCompleted;
    uint64_t totalFramesPlayed;
    uint64_t totalBytesDma;
    char     controllerName[48];
};
#pragma pack(pop)

// Quarter-wave 65-entry fixed-point sine table (0..pi/2 scaled to 0..16384) for zero-FPU pure Ring-3 synthesis!
static const int16_t kQuarterSin[65] = {
        0,   402,   804,  1205,  1606,  2006,  2404,  2801,
     3196,  3590,  3981,  4370,  4756,  5139,  5520,  5897,
     6270,  6639,  7005,  7366,  7723,  8076,  8423,  8765,
     9102,  9434,  9760, 10080, 10394, 10702, 11003, 11297,
    11585, 11866, 12140, 12406, 12665, 12916, 13160, 13395,
    13623, 13842, 14053, 14256, 14449, 14635, 14811, 14978,
    15137, 15286, 15426, 15557, 15679, 15791, 15893, 15986,
    16069, 16143, 16207, 16261, 16305, 16340, 16364, 16379,
    16384
};

static int16_t fastSin256(uint32_t phase256) noexcept {
    uint32_t idx = phase256 & 255u;
    if (idx <= 64u)  return kQuarterSin[idx];
    if (idx <= 128u) return kQuarterSin[128u - idx];
    if (idx <= 192u) return static_cast<int16_t>(-kQuarterSin[idx - 128u]);
    return static_cast<int16_t>(-kQuarterSin[256u - idx]);
}

bool DllMain(void*, unsigned long, void*) noexcept {
    return true;
}

__declspec(dllexport) uint32_t waveOutGetNumDevs() noexcept {
    NtAudioDeviceInfo info{};
    if (NtAudioQueryDevice(&info, sizeof(info)) == 0 && info.present) return 1u;
    return 0u;
}

__declspec(dllexport) uint32_t waveOutOpen(
    void** phwo,
    uint32_t /*uDeviceID*/,
    const WaveFormatEx* pwfx,
    uint64_t /*dwCallback*/,
    uint64_t /*dwInstance*/,
    uint32_t /*fdwOpen*/
) noexcept {
    if (!phwo || !pwfx || pwfx->wFormatTag != 1u) return 11u; // MMSYSERR_INVALPARAM
    void* h = nullptr;
    uint64_t st = NtAudioOpenStream(&h, pwfx->nSamplesPerSec, pwfx->nChannels, pwfx->wBitsPerSample);
    if (st != 0 || !h) {
        *phwo = nullptr;
        return 1u; // MMSYSERR_ERROR
    }
    *phwo = h;
    return 0u; // MMSYSERR_NOERROR
}

__declspec(dllexport) uint32_t waveOutPrepareHeader(void* hwo, WaveHdr* pwh, uint32_t /*cbwh*/) noexcept {
    if (!hwo || !pwh || !pwh->lpData) return 11u;
    pwh->dwFlags |= 0x00000002u; // WHDR_PREPARED
    return 0u;
}

__declspec(dllexport) uint32_t waveOutWrite(void* hwo, WaveHdr* pwh, uint32_t /*cbwh*/) noexcept {
    if (!hwo || !pwh || !pwh->lpData || pwh->dwBufferLength < 4u) return 11u;
    int32_t written = NtAudioWritePcm(hwo, pwh->lpData, pwh->dwBufferLength);
    if (written <= 0) return 1u;
    pwh->dwFlags |= 0x00000001u; // WHDR_DONE
    return 0u;
}

__declspec(dllexport) uint32_t waveOutUnprepareHeader(void* hwo, WaveHdr* pwh, uint32_t /*cbwh*/) noexcept {
    if (!hwo || !pwh) return 11u;
    pwh->dwFlags &= ~0x00000002u;
    return 0u;
}

__declspec(dllexport) uint32_t waveOutClose(void* hwo) noexcept {
    if (!hwo) return 5u; // MMSYSERR_INVALHANDLE
    return (NtAudioCloseStream(hwo) == 0) ? 0u : 1u;
}

__declspec(dllexport) int32_t QueryAudioDeviceMica(void* outInfoBuf, uint32_t bufLen) noexcept {
    return (NtAudioQueryDevice(outInfoBuf, bufLen) == 0) ? 1 : 0;
}

// Synthesize a pure 48 kHz 16-bit Stereo sine tone in Ring 3 and stream via waveOutOpen/Write/Close!
__declspec(dllexport) int32_t PlayToneMica(uint32_t freqHz, uint32_t durationMs) noexcept {
    if (freqHz < 50u || freqHz > 16000u || durationMs == 0u) return 0;
    if (durationMs > 950u) durationMs = 950u;
    constexpr uint32_t kRate = 48000u;
    uint32_t frames = (kRate * durationMs) / 1000u;
    if (frames > 48000u) frames = 48000u;

    void* baseAddr = nullptr;
    size_t allocSz = static_cast<size_t>(frames) * 4u;
    if (NtAllocateVirtualMemory(reinterpret_cast<void*>(-1), &baseAddr, 0, &allocSz, 0x3000u, 0x04u) != 0 || !baseAddr) {
        return 0;
    }
    auto* pcmBuf = static_cast<int16_t*>(baseAddr);

    // 16.16 fixed-point phase accumulator across 256-entry sine cycle
    uint64_t phaseInc = (static_cast<uint64_t>(freqHz) * 256ULL * 65536ULL) / kRate;
    uint64_t phase = 0;
    for (uint32_t i = 0; i < frames; ++i) {
        int32_t s = fastSin256(static_cast<uint32_t>(phase >> 16));
        // Clean 10ms fade-in/fade-out envelope (zero clicks!)
        uint32_t env = 480u;
        if (i < 480u) env = i;
        else if (i + 480u > frames) env = frames - i;
        int16_t sample = static_cast<int16_t>((s * static_cast<int32_t>(env)) / 480);
        pcmBuf[i * 2u]     = sample; // Left
        pcmBuf[i * 2u + 1] = sample; // Right
        phase += phaseInc;
    }

    WaveFormatEx fmt{};
    fmt.wFormatTag = 1u;
    fmt.nChannels = 2u;
    fmt.nSamplesPerSec = kRate;
    fmt.wBitsPerSample = 16u;
    fmt.nBlockAlign = 4u;
    fmt.nAvgBytesPerSec = kRate * 4u;

    void* hwo = nullptr;
    if (waveOutOpen(&hwo, 0u, &fmt, 0, 0, 0) != 0) {
        size_t fsz = 0;
        NtFreeVirtualMemory(reinterpret_cast<void*>(-1), &baseAddr, &fsz, 0x8000u);
        return 0;
    }

    WaveHdr hdr{};
    hdr.lpData = reinterpret_cast<char*>(pcmBuf);
    hdr.dwBufferLength = frames * 4u;
    waveOutPrepareHeader(hwo, &hdr, sizeof(hdr));
    uint32_t wr = waveOutWrite(hwo, &hdr, sizeof(hdr));
    waveOutUnprepareHeader(hwo, &hdr, sizeof(hdr));
    waveOutClose(hwo);
    size_t fsz = 0;
    NtFreeVirtualMemory(reinterpret_cast<void*>(-1), &baseAddr, &fsz, 0x8000u);
    return (wr == 0u) ? static_cast<int32_t>(frames) : 0;
}

} // extern "C"
