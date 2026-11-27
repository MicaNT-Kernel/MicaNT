/**
 * @file sndrec_app.cpp
 * @brief Sovereign Dynamic Ring-3 Multimedia Audio Synthesizer & WAV Player (C:\Windows\System32\sndrec.exe).
 *
 * ZERO direct syscall instructions! Dynamically imports ONLY standard Win32 & Multimedia APIs
 * from KERNEL32.dll + WINMM.dll (waveOutGetNumDevs, waveOutOpen, waveOutPrepareHeader,
 * waveOutWrite, waveOutUnprepareHeader, waveOutClose, PlayToneMica, QueryAudioDeviceMica)
 * -> NTDLL.dll -> Ring 0 Intel ICH AC'97 (8086:2415) PCIe Bus-Master DMA!
 *
 * Usage:
 *   sndrec.exe --info                      (Inspect live AC'97 PCIe DMA hardware & counters)
 *   sndrec.exe --tone <freqHz> <durMs>     (Synthesize & play pure 48kHz stereo sine via winmm.dll)
 *   sndrec.exe <ntfs_wav_path>             (Load RIFF/WAVE from NTFS 3.1 & stream via waveOutWrite)
 */

#include <cstdint>
#include <cstddef>

extern "C" {

void* memset(void* dst, int c, size_t n) noexcept {
    auto* d = static_cast<unsigned char*>(dst);
    while (n--) *d++ = static_cast<unsigned char>(c);
    return dst;
}

#pragma pack(push, 1)
struct WaveFormatEx {
    uint16_t wFormatTag;
    uint16_t nChannels;
    uint32_t nSamplesPerSec;
    uint32_t nAvgBytesPerSec;
    uint16_t nBlockAlign;
    uint16_t wBitsPerSample;
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

// kernel32.dll
__declspec(dllimport) void*       GetStdHandle(uint32_t nStdHandle) noexcept;
__declspec(dllimport) void*       CreateFileW(const wchar_t* lpFileName, uint32_t dwAccess, uint32_t dwShare,
                                              void*, uint32_t dwDisp, uint32_t dwFlags, void*) noexcept;
__declspec(dllimport) int32_t     ReadFile(void* hFile, void* lpBuf, uint32_t nBytes, uint32_t* lpRead, void*) noexcept;
__declspec(dllimport) int32_t     WriteFile(void* hFile, const void* buf, uint32_t len, uint32_t* written, void*) noexcept;
__declspec(dllimport) int32_t     CloseHandle(void* hObject) noexcept;
__declspec(dllimport) void*       VirtualAlloc(void* lpAddr, size_t dwSize, uint32_t flType, uint32_t flProt) noexcept;
__declspec(dllimport) int32_t     VirtualFree(void* lpAddr, size_t dwSize, uint32_t flType) noexcept;
__declspec(dllimport) const char* GetCommandLineA() noexcept;
__declspec(dllimport) void        ExitProcess(uint32_t uExitCode) noexcept;

// winmm.dll
__declspec(dllimport) uint32_t waveOutGetNumDevs() noexcept;
__declspec(dllimport) uint32_t waveOutOpen(void** phwo, uint32_t uDev, const WaveFormatEx* pwfx, uint64_t cb, uint64_t inst, uint32_t flags) noexcept;
__declspec(dllimport) uint32_t waveOutPrepareHeader(void* hwo, WaveHdr* pwh, uint32_t cbwh) noexcept;
__declspec(dllimport) uint32_t waveOutWrite(void* hwo, WaveHdr* pwh, uint32_t cbwh) noexcept;
__declspec(dllimport) uint32_t waveOutUnprepareHeader(void* hwo, WaveHdr* pwh, uint32_t cbwh) noexcept;
__declspec(dllimport) uint32_t waveOutClose(void* hwo) noexcept;
__declspec(dllimport) int32_t  QueryAudioDeviceMica(void* outInfoBuf, uint32_t bufLen) noexcept;
__declspec(dllimport) int32_t  PlayToneMica(uint32_t freqHz, uint32_t durationMs) noexcept;

} // extern "C"

namespace {

static constexpr uint32_t STD_OUTPUT_HANDLE = 0xFFFFFFF5u;

static size_t StrLen(const char* s) noexcept { size_t n = 0; while (s[n]) ++n; return n; }

static bool StrEq(const char* a, const char* b) noexcept {
    size_t i = 0;
    while (a[i] && b[i]) { if (a[i] != b[i]) return false; ++i; }
    return a[i] == b[i];
}

static void AppendStr(char* dst, size_t cap, const char* src) noexcept {
    size_t d = StrLen(dst);
    for (size_t i = 0; src[i] && d + 1 < cap; ++i) dst[d++] = src[i];
    dst[d] = '\0';
}

static void AppendU64(char* dst, size_t cap, uint64_t v) noexcept {
    char tmp[24]; size_t n = 0;
    if (v == 0) tmp[n++] = '0';
    else { while (v > 0 && n < 22) { tmp[n++] = static_cast<char>('0' + (v % 10)); v /= 10; } }
    char rev[24];
    for (size_t i = 0; i < n; ++i) rev[i] = tmp[n - 1 - i];
    rev[n] = '\0';
    AppendStr(dst, cap, rev);
}

static void AppendHex16(char* dst, size_t cap, uint16_t v) noexcept {
    static const char hx[] = "0123456789ABCDEF";
    char s[5] = {hx[(v >> 12) & 0xF], hx[(v >> 8) & 0xF], hx[(v >> 4) & 0xF], hx[v & 0xF], '\0'};
    AppendStr(dst, cap, s);
}

static void PrintLine(void* hOut, const char* s) noexcept {
    char buf[196];
    size_t n = 0;
    while (s[n] && n + 3 < sizeof(buf)) { buf[n] = s[n]; ++n; }
    buf[n++] = '\r'; buf[n++] = '\n';
    uint32_t wr = 0;
    WriteFile(hOut, buf, static_cast<uint32_t>(n), &wr, nullptr);
}

static int Tokenize(const char* cmd, char argvBuf[8][96]) noexcept {
    int argc = 0; size_t i = 0;
    while (cmd[i] && argc < 8) {
        while (cmd[i] == ' ' || cmd[i] == '\t') ++i;
        if (!cmd[i]) break;
        size_t o = 0;
        while (cmd[i] && cmd[i] != ' ' && cmd[i] != '\t' && o + 1 < 96) argvBuf[argc][o++] = cmd[i++];
        argvBuf[argc][o] = '\0';
        argc++;
    }
    return argc;
}

static uint32_t ParseU32(const char* s, uint32_t fallback) noexcept {
    uint32_t v = 0;
    bool any = false;
    for (size_t i = 0; s[i] >= '0' && s[i] <= '9'; ++i) {
        v = v * 10u + static_cast<uint32_t>(s[i] - '0');
        any = true;
    }
    return any ? v : fallback;
}

static uint16_t RdU16(const uint8_t* p) noexcept {
    return static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
}

static uint32_t RdU32(const uint8_t* p) noexcept {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

static void PrintAudioInfo(void* hOut) noexcept {
    NtAudioDeviceInfo info{};
    uint32_t devs = waveOutGetNumDevs();
    if (!QueryAudioDeviceMica(&info, sizeof(info)) || !info.present) {
        PrintLine(hOut, "[winmm] No hardware AC'97 PCIe DMA audio controller detected.");
        ExitProcess(1);
    }

    PrintLine(hOut, "MicaNT Multimedia Audio Configuration (winmm.dll -> Intel AC'97 PCIe DMA)");
    char line[160]{};
    AppendStr(line, sizeof(line), "  Controller PCI . . : ");
    AppendU64(line, sizeof(line), info.pciBus); AppendStr(line, sizeof(line), ":");
    AppendU64(line, sizeof(line), info.pciDev); AppendStr(line, sizeof(line), ".");
    AppendU64(line, sizeof(line), info.pciFunc); AppendStr(line, sizeof(line), " [");
    AppendHex16(line, sizeof(line), info.vendorId); AppendStr(line, sizeof(line), ":");
    AppendHex16(line, sizeof(line), info.deviceId); AppendStr(line, sizeof(line), "] ");
    AppendStr(line, sizeof(line), info.controllerName);
    PrintLine(hOut, line);

    line[0] = '\0';
    AppendStr(line, sizeof(line), "  I/O Ports & Format : NAMBAR=0x");
    AppendHex16(line, sizeof(line), info.nambarPort);
    AppendStr(line, sizeof(line), " NABMBAR=0x");
    AppendHex16(line, sizeof(line), info.nabmbarPort);
    AppendStr(line, sizeof(line), " | ");
    AppendU64(line, sizeof(line), info.sampleRate);
    AppendStr(line, sizeof(line), "Hz ");
    AppendU64(line, sizeof(line), info.bitsPerSample);
    AppendStr(line, sizeof(line), "-bit Stereo (Devs=");
    AppendU64(line, sizeof(line), devs);
    AppendStr(line, sizeof(line), ")");
    PrintLine(hOut, line);

    line[0] = '\0';
    AppendStr(line, sizeof(line), "  Hardware DMA Stats : BDL Buffers=");
    AppendU64(line, sizeof(line), info.dmaBuffersCompleted);
    AppendStr(line, sizeof(line), " | Frames=");
    AppendU64(line, sizeof(line), info.totalFramesPlayed);
    AppendStr(line, sizeof(line), " | Bytes=");
    AppendU64(line, sizeof(line), info.totalBytesDma);
    PrintLine(hOut, line);
}

static int PlayNtfsWavFile(void* hOut, const char* asciiPath) noexcept {
    // Strip optional "C:" prefix so kernel NTFS path begins cleanly at '\'
    const char* p = asciiPath;
    if ((p[0] == 'C' || p[0] == 'c') && p[1] == ':') p += 2;

    wchar_t wpath[128]{};
    size_t wi = 0;
    while (p[wi] && wi + 1 < 128) {
        wpath[wi] = static_cast<wchar_t>(static_cast<unsigned char>(p[wi]));
        ++wi;
    }
    wpath[wi] = L'\0';

    void* hFile = CreateFileW(wpath, 0x80000000u /*GENERIC_READ*/, 1u, nullptr, 3u /*OPEN_EXISTING*/, 0x80u, nullptr);
    if (!hFile || hFile == reinterpret_cast<void*>(-1)) {
        char err[160]{};
        AppendStr(err, sizeof(err), "[sndrec] ERROR: Cannot open WAV on NTFS: ");
        AppendStr(err, sizeof(err), asciiPath);
        PrintLine(hOut, err);
        return 1;
    }

    constexpr uint32_t kMaxWav = 524288u;
    auto* wavBuf = static_cast<uint8_t*>(VirtualAlloc(nullptr, kMaxWav, 0x3000u, 0x04u));
    if (!wavBuf) {
        CloseHandle(hFile);
        PrintLine(hOut, "[sndrec] ERROR: VirtualAlloc failed.");
        return 1;
    }

    uint32_t totalRead = 0;
    while (totalRead < kMaxWav) {
        uint32_t chunk = 0;
        if (!ReadFile(hFile, wavBuf + totalRead, kMaxWav - totalRead, &chunk, nullptr) || chunk == 0) break;
        totalRead += chunk;
    }
    CloseHandle(hFile);

    if (totalRead < 44u ||
        wavBuf[0] != 'R' || wavBuf[1] != 'I' || wavBuf[2] != 'F' || wavBuf[3] != 'F' ||
        wavBuf[8] != 'W' || wavBuf[9] != 'A' || wavBuf[10] != 'V' || wavBuf[11] != 'E') {
        VirtualFree(wavBuf, 0, 0x8000u);
        PrintLine(hOut, "[sndrec] ERROR: Invalid RIFF/WAVE header.");
        return 1;
    }

    WaveFormatEx fmt{};
    uint32_t dataOff = 0, dataSize = 0;
    uint32_t off = 12u;
    while (off + 8u <= totalRead) {
        uint32_t csz = RdU32(wavBuf + off + 4u);
        if (wavBuf[off] == 'f' && wavBuf[off + 1] == 'm' && wavBuf[off + 2] == 't' && wavBuf[off + 3] == ' ' && csz >= 16u && off + 8u + 16u <= totalRead) {
            fmt.wFormatTag      = RdU16(wavBuf + off + 8u);
            fmt.nChannels       = RdU16(wavBuf + off + 10u);
            fmt.nSamplesPerSec  = RdU32(wavBuf + off + 12u);
            fmt.nAvgBytesPerSec = RdU32(wavBuf + off + 16u);
            fmt.nBlockAlign     = RdU16(wavBuf + off + 20u);
            fmt.wBitsPerSample  = RdU16(wavBuf + off + 22u);
        } else if (wavBuf[off] == 'd' && wavBuf[off + 1] == 'a' && wavBuf[off + 2] == 't' && wavBuf[off + 3] == 'a') {
            dataOff = off + 8u;
            dataSize = csz;
            if (dataOff + dataSize > totalRead) dataSize = totalRead - dataOff;
            break;
        }
        off += 8u + csz + (csz & 1u);
    }

    if (fmt.wFormatTag != 1u || dataOff == 0 || dataSize < 4u) {
        VirtualFree(wavBuf, 0, 0x8000u);
        PrintLine(hOut, "[sndrec] ERROR: Unsupported WAV format or missing data chunk.");
        return 1;
    }

    char infoLine[168]{};
    AppendStr(infoLine, sizeof(infoLine), "[winmm] Playing NTFS RIFF/WAVE '");
    AppendStr(infoLine, sizeof(infoLine), asciiPath);
    AppendStr(infoLine, sizeof(infoLine), "' (");
    AppendU64(infoLine, sizeof(infoLine), fmt.nSamplesPerSec);
    AppendStr(infoLine, sizeof(infoLine), "Hz ");
    AppendU64(infoLine, sizeof(infoLine), fmt.wBitsPerSample);
    AppendStr(infoLine, sizeof(infoLine), "-bit ");
    AppendU64(infoLine, sizeof(infoLine), fmt.nChannels);
    AppendStr(infoLine, sizeof(infoLine), "ch | ");
    AppendU64(infoLine, sizeof(infoLine), dataSize);
    AppendStr(infoLine, sizeof(infoLine), " B PCM)");
    PrintLine(hOut, infoLine);

    void* hwo = nullptr;
    if (waveOutOpen(&hwo, 0u, &fmt, 0, 0, 0) != 0) {
        VirtualFree(wavBuf, 0, 0x8000u);
        PrintLine(hOut, "[winmm] ERROR: waveOutOpen failed.");
        return 1;
    }

    WaveHdr hdr{};
    hdr.lpData = reinterpret_cast<char*>(wavBuf + dataOff);
    hdr.dwBufferLength = dataSize;
    waveOutPrepareHeader(hwo, &hdr, sizeof(hdr));
    uint32_t res = waveOutWrite(hwo, &hdr, sizeof(hdr));
    waveOutUnprepareHeader(hwo, &hdr, sizeof(hdr));
    waveOutClose(hwo);
    VirtualFree(wavBuf, 0, 0x8000u);

    if (res != 0) {
        PrintLine(hOut, "[winmm] ERROR: waveOutWrite DMA transfer failed.");
        return 1;
    }

    char done[160]{};
    AppendStr(done, sizeof(done), "[winmm] waveOutWrite COMPLETE: Streamed ");
    AppendU64(done, sizeof(done), dataSize / 4u);
    AppendStr(done, sizeof(done), " stereo PCM frames via Intel AC'97 DMA!");
    PrintLine(hOut, done);
    return 0;
}

} // namespace

extern "C" void WinMainEntry() noexcept {
    void* hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    char argv[8][96]{};
    int argc = Tokenize(GetCommandLineA(), argv);

    if (argc < 2 || StrEq(argv[1], "--info")) {
        PrintAudioInfo(hOut);
        ExitProcess(0);
    }

    if (StrEq(argv[1], "--tone")) {
        uint32_t freq = (argc >= 3) ? ParseU32(argv[2], 440u) : 440u;
        uint32_t dur  = (argc >= 4) ? ParseU32(argv[3], 400u) : 400u;
        char msg[160]{};
        AppendStr(msg, sizeof(msg), "[winmm] Synthesizing ");
        AppendU64(msg, sizeof(msg), freq);
        AppendStr(msg, sizeof(msg), " Hz sine tone (");
        AppendU64(msg, sizeof(msg), dur);
        AppendStr(msg, sizeof(msg), " ms @ 48kHz Stereo) -> waveOutWrite DMA...");
        PrintLine(hOut, msg);

        int32_t frames = PlayToneMica(freq, dur);
        if (frames <= 0) {
            PrintLine(hOut, "[winmm] ERROR: PlayToneMica failed.");
            ExitProcess(1);
        }
        msg[0] = '\0';
        AppendStr(msg, sizeof(msg), "[winmm] PlayToneMica COMPLETE: ");
        AppendU64(msg, sizeof(msg), static_cast<uint64_t>(frames));
        AppendStr(msg, sizeof(msg), " frames (");
        AppendU64(msg, sizeof(msg), static_cast<uint64_t>(frames) * 4u);
        AppendStr(msg, sizeof(msg), " B) played through Intel AC'97 DAC!");
        PrintLine(hOut, msg);
        ExitProcess(0);
    }

    int rc = PlayNtfsWavFile(hOut, argv[1]);
    ExitProcess(static_cast<uint32_t>(rc));
}
