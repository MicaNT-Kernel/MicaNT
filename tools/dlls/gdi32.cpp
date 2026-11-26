/**
 * @file gdi32.cpp
 * @brief Sovereign Graphics Device Interface DLL (gdi32.dll) for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. Zero proprietary headers.
 * ZERO direct syscall instructions: chains exclusively through ntdll.dll PE32+ IAT
 * (NtAllocateVirtualMemory, NtFreeVirtualMemory, NtGdiGetVideoInfo, NtGdiPresentSurface).
 *
 * Implements real Ring-3 32-bpp BGRA Device Contexts, software rasterization
 * (FillSolidRect, DrawRectBorder, SetPixel, Bresenham MoveToEx/LineTo, clean-room 8x8
 * bitmap TextOutA/TextOutScaledA), and hardware VRAM presentation via BitBlt -> SSN 0x81.
 */

#include <cstdint>
#include <cstddef>

extern "C" {

__declspec(dllimport) uint64_t NtAllocateVirtualMemory(
    void* processHandle, void** baseAddress, uint64_t zeroBits, size_t* regionSize,
    uint32_t allocationType, uint32_t protect
) noexcept;
__declspec(dllimport) uint64_t NtFreeVirtualMemory(
    void* processHandle, void** baseAddress, size_t* regionSize, uint32_t freeType
) noexcept;
__declspec(dllimport) uint64_t NtGdiGetVideoInfo(void* videoInfo) noexcept;
__declspec(dllimport) uint64_t NtGdiPresentSurface(const void* params) noexcept;

#pragma pack(push, 1)
struct UserPresentSurfaceParams64 {
    uint64_t surfaceBufferVa;
    uint32_t dstX;
    uint32_t dstY;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t reserved;
};
#pragma pack(pop)

struct GdiDcEntry {
    bool      inUse;
    uint64_t  handle;
    uint32_t* pixels;
    size_t    allocBytes;
    uint32_t  width;
    uint32_t  height;
    int32_t   dstX;
    int32_t   dstY;
    int32_t   curX;
    int32_t   curY;
    uint32_t  penColor;
    uint32_t  textColor;
    uint32_t  bkColor;
};

static constexpr size_t MAX_GDI_DCS = 8;
static GdiDcEntry s_dcs[MAX_GDI_DCS];
static uint64_t s_gdiHandleCounter = 0x3000;

bool DllMain(void*, unsigned long, void*) noexcept {
    return true;
}

static GdiDcEntry* lookupDc(void* hdc) noexcept {
    uint64_t h = reinterpret_cast<uint64_t>(hdc);
    for (size_t i = 0; i < MAX_GDI_DCS; ++i) {
        if (s_dcs[i].inUse && s_dcs[i].handle == h) return &s_dcs[i];
    }
    return nullptr;
}

// Convert Win32 COLORREF (0x00BBGGRR) or already-BGRA (alpha != 0) into 32-bpp BGRA
static inline uint32_t toBgra32(uint32_t color) noexcept {
    if ((color & 0xFF000000u) != 0) return color; // Already 0xAARRGGBB BGRA
    uint32_t r = color & 0xFFu;
    uint32_t g = (color >> 8) & 0xFFu;
    uint32_t b = (color >> 16) & 0xFFu;
    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

// Clean-Room 8x8 ASCII Bitmap Font (ASCII 32..126, 1 bit/pixel MSB left)
static void getGlyph8x8(char ch, uint8_t outRows[8]) noexcept {
    for (int i = 0; i < 8; ++i) outRows[i] = 0;
    if (ch >= 'a' && ch <= 'z') {
        // Render lowercase cleanly with distinct forms or compact uppercase clarity
    }
    switch (ch) {
        case ' ': break;
        case '!': { const uint8_t g[8]={0x18,0x18,0x18,0x18,0x18,0x00,0x18,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case '"': { const uint8_t g[8]={0x66,0x66,0x24,0x00,0x00,0x00,0x00,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case '#': { const uint8_t g[8]={0x24,0x7E,0x24,0x24,0x7E,0x24,0x00,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case '$': { const uint8_t g[8]={0x18,0x3E,0x60,0x3C,0x06,0x7C,0x18,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case '%': { const uint8_t g[8]={0x62,0x64,0x08,0x10,0x26,0x46,0x00,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case '&': { const uint8_t g[8]={0x38,0x44,0x38,0x54,0x48,0x34,0x00,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case '\'':{ const uint8_t g[8]={0x18,0x18,0x10,0x00,0x00,0x00,0x00,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case '(': { const uint8_t g[8]={0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case ')': { const uint8_t g[8]={0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case '*': { const uint8_t g[8]={0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case '+': { const uint8_t g[8]={0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case ',': { const uint8_t g[8]={0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case '-': { const uint8_t g[8]={0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case '.': { const uint8_t g[8]={0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case '/': { const uint8_t g[8]={0x02,0x04,0x08,0x10,0x20,0x40,0x00,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case '0': { const uint8_t g[8]={0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case '1': { const uint8_t g[8]={0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case '2': { const uint8_t g[8]={0x3C,0x66,0x06,0x1C,0x30,0x60,0x7E,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case '3': { const uint8_t g[8]={0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case '4': { const uint8_t g[8]={0x0C,0x1C,0x3C,0x6C,0x7E,0x0C,0x0C,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case '5': { const uint8_t g[8]={0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case '6': { const uint8_t g[8]={0x3C,0x60,0x7C,0x66,0x66,0x66,0x3C,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case '7': { const uint8_t g[8]={0x7E,0x06,0x0C,0x18,0x30,0x30,0x30,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case '8': { const uint8_t g[8]={0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case '9': { const uint8_t g[8]={0x3C,0x66,0x66,0x3E,0x06,0x0C,0x38,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case ':': { const uint8_t g[8]={0x00,0x18,0x18,0x00,0x18,0x18,0x00,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case ';': { const uint8_t g[8]={0x00,0x18,0x18,0x00,0x18,0x18,0x30,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case '<': { const uint8_t g[8]={0x0C,0x18,0x30,0x60,0x30,0x18,0x0C,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case '=': { const uint8_t g[8]={0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case '>': { const uint8_t g[8]={0x60,0x30,0x18,0x0C,0x18,0x30,0x60,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case '?': { const uint8_t g[8]={0x3C,0x66,0x06,0x1C,0x18,0x00,0x18,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case '@': { const uint8_t g[8]={0x3C,0x66,0x6E,0x6A,0x6E,0x60,0x3C,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'A': { const uint8_t g[8]={0x18,0x3C,0x66,0x66,0x7E,0x66,0x66,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'B': { const uint8_t g[8]={0x7C,0x66,0x66,0x7C,0x66,0x66,0x7C,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'C': { const uint8_t g[8]={0x3C,0x66,0x60,0x60,0x60,0x66,0x3C,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'D': { const uint8_t g[8]={0x78,0x6C,0x66,0x66,0x66,0x6C,0x78,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'E': { const uint8_t g[8]={0x7E,0x60,0x60,0x7C,0x60,0x60,0x7E,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'F': { const uint8_t g[8]={0x7E,0x60,0x60,0x7C,0x60,0x60,0x60,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'G': { const uint8_t g[8]={0x3C,0x66,0x60,0x6E,0x66,0x66,0x3E,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'H': { const uint8_t g[8]={0x66,0x66,0x66,0x7E,0x66,0x66,0x66,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'I': { const uint8_t g[8]={0x3C,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'J': { const uint8_t g[8]={0x1E,0x0C,0x0C,0x0C,0x6C,0x6C,0x38,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'K': { const uint8_t g[8]={0x66,0x6C,0x78,0x70,0x78,0x6C,0x66,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'L': { const uint8_t g[8]={0x60,0x60,0x60,0x60,0x60,0x60,0x7E,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'M': { const uint8_t g[8]={0x63,0x77,0x7F,0x6B,0x63,0x63,0x63,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'N': { const uint8_t g[8]={0x66,0x76,0x7E,0x7E,0x6E,0x66,0x66,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'O': { const uint8_t g[8]={0x3C,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'P': { const uint8_t g[8]={0x7C,0x66,0x66,0x7C,0x60,0x60,0x60,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'Q': { const uint8_t g[8]={0x3C,0x66,0x66,0x66,0x6A,0x6C,0x36,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'R': { const uint8_t g[8]={0x7C,0x66,0x66,0x7C,0x6C,0x66,0x66,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'S': { const uint8_t g[8]={0x3C,0x66,0x60,0x3C,0x06,0x66,0x3C,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'T': { const uint8_t g[8]={0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'U': { const uint8_t g[8]={0x66,0x66,0x66,0x66,0x66,0x66,0x3C,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'V': { const uint8_t g[8]={0x66,0x66,0x66,0x66,0x66,0x3C,0x18,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'W': { const uint8_t g[8]={0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'X': { const uint8_t g[8]={0x66,0x66,0x3C,0x18,0x3C,0x66,0x66,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'Y': { const uint8_t g[8]={0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'Z': { const uint8_t g[8]={0x7E,0x06,0x0C,0x18,0x30,0x60,0x7E,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case '[': { const uint8_t g[8]={0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case '\\':{ const uint8_t g[8]={0x40,0x20,0x10,0x08,0x04,0x02,0x00,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case ']': { const uint8_t g[8]={0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case '^': { const uint8_t g[8]={0x18,0x3C,0x66,0x00,0x00,0x00,0x00,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case '_': { const uint8_t g[8]={0x00,0x00,0x00,0x00,0x00,0x00,0x7E,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case '|': { const uint8_t g[8]={0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'a': { const uint8_t g[8]={0x00,0x00,0x3C,0x06,0x3E,0x66,0x3E,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'b': { const uint8_t g[8]={0x60,0x60,0x7C,0x66,0x66,0x66,0x7C,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'c': { const uint8_t g[8]={0x00,0x00,0x3C,0x60,0x60,0x60,0x3C,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'd': { const uint8_t g[8]={0x06,0x06,0x3E,0x66,0x66,0x66,0x3E,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'e': { const uint8_t g[8]={0x00,0x00,0x3C,0x66,0x7E,0x60,0x3C,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'f': { const uint8_t g[8]={0x1C,0x30,0x7C,0x30,0x30,0x30,0x30,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'g': { const uint8_t g[8]={0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x3C}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'h': { const uint8_t g[8]={0x60,0x60,0x7C,0x66,0x66,0x66,0x66,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'i': { const uint8_t g[8]={0x18,0x00,0x38,0x18,0x18,0x18,0x3C,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'j': { const uint8_t g[8]={0x0C,0x00,0x1C,0x0C,0x0C,0x6C,0x38,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'k': { const uint8_t g[8]={0x60,0x60,0x66,0x6C,0x78,0x6C,0x66,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'l': { const uint8_t g[8]={0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'm': { const uint8_t g[8]={0x00,0x00,0x66,0x7F,0x6B,0x6B,0x63,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'n': { const uint8_t g[8]={0x00,0x00,0x7C,0x66,0x66,0x66,0x66,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'o': { const uint8_t g[8]={0x00,0x00,0x3C,0x66,0x66,0x66,0x3C,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'p': { const uint8_t g[8]={0x00,0x00,0x7C,0x66,0x66,0x7C,0x60,0x60}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'q': { const uint8_t g[8]={0x00,0x00,0x3E,0x66,0x66,0x3E,0x06,0x06}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'r': { const uint8_t g[8]={0x00,0x00,0x7C,0x66,0x60,0x60,0x60,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 's': { const uint8_t g[8]={0x00,0x00,0x3E,0x60,0x3C,0x06,0x7C,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 't': { const uint8_t g[8]={0x30,0x30,0x7C,0x30,0x30,0x36,0x1C,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'u': { const uint8_t g[8]={0x00,0x00,0x66,0x66,0x66,0x66,0x3E,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'v': { const uint8_t g[8]={0x00,0x00,0x66,0x66,0x66,0x3C,0x18,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'w': { const uint8_t g[8]={0x00,0x00,0x63,0x6B,0x6B,0x7F,0x36,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'x': { const uint8_t g[8]={0x00,0x00,0x66,0x3C,0x18,0x3C,0x66,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'y': { const uint8_t g[8]={0x00,0x00,0x66,0x66,0x66,0x3E,0x06,0x3C}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        case 'z': { const uint8_t g[8]={0x00,0x00,0x7E,0x0C,0x18,0x30,0x7E,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
        default:  { const uint8_t g[8]={0x3C,0x66,0x06,0x1C,0x18,0x00,0x18,0x00}; for(int i=0;i<8;++i) outRows[i]=g[i]; break; }
    }
}

__declspec(dllexport) void* GdiCreateSurfaceDC(int x, int y, int width, int height) noexcept {
    if (width <= 0 || height <= 0 || width > 1920 || height > 1080) return nullptr;
    for (size_t i = 0; i < MAX_GDI_DCS; ++i) {
        if (!s_dcs[i].inUse) {
            size_t bytes = static_cast<size_t>(width) * static_cast<size_t>(height) * sizeof(uint32_t);
            void* base = nullptr;
            uint64_t st = NtAllocateVirtualMemory(reinterpret_cast<void*>(-1), &base, 0, &bytes, 0x3000u, 0x04u);
            if (static_cast<int32_t>(static_cast<uint32_t>(st)) < 0 || !base) {
                return nullptr;
            }
            s_dcs[i].inUse = true;
            s_dcs[i].handle = ++s_gdiHandleCounter;
            s_dcs[i].pixels = static_cast<uint32_t*>(base);
            s_dcs[i].allocBytes = bytes;
            s_dcs[i].width = static_cast<uint32_t>(width);
            s_dcs[i].height = static_cast<uint32_t>(height);
            s_dcs[i].dstX = x;
            s_dcs[i].dstY = y;
            s_dcs[i].curX = 0;
            s_dcs[i].curY = 0;
            s_dcs[i].penColor = 0xFF00E5FFu;
            s_dcs[i].textColor = 0xFFF0F5FFu;
            s_dcs[i].bkColor = 0xFF101522u;
            return reinterpret_cast<void*>(s_dcs[i].handle);
        }
    }
    return nullptr;
}

__declspec(dllexport) void* CreateCompatibleDC(void* /*hdc*/) noexcept {
    return reinterpret_cast<void*>(++s_gdiHandleCounter);
}

__declspec(dllexport) bool DeleteDC(void* hdc) noexcept {
    if (auto* dc = lookupDc(hdc)) {
        if (dc->pixels) {
            void* base = dc->pixels;
            size_t sz = 0;
            NtFreeVirtualMemory(reinterpret_cast<void*>(-1), &base, &sz, 0x8000u);
        }
        dc->inUse = false;
        dc->pixels = nullptr;
        return true;
    }
    return true;
}

__declspec(dllexport) void* CreateCompatibleBitmap(void* /*hdc*/, int /*cx*/, int /*cy*/) noexcept {
    return reinterpret_cast<void*>(++s_gdiHandleCounter);
}

__declspec(dllexport) void* CreateSolidBrush(uint32_t color) noexcept {
    return reinterpret_cast<void*>(0x40000000ULL | static_cast<uint64_t>(toBgra32(color)));
}

__declspec(dllexport) void* SelectObject(void* /*hdc*/, void* hgdiobj) noexcept {
    return hgdiobj;
}

__declspec(dllexport) bool DeleteObject(void* /*ho*/) noexcept {
    return true;
}

__declspec(dllexport) void* GetStockObject(int /*i*/) noexcept {
    return reinterpret_cast<void*>(0x10);
}

__declspec(dllexport) uint32_t SetDCPenColor(void* hdc, uint32_t color) noexcept {
    if (auto* dc = lookupDc(hdc)) {
        uint32_t prev = dc->penColor;
        dc->penColor = toBgra32(color);
        return prev;
    }
    return 0;
}

__declspec(dllexport) uint32_t SetTextColor(void* hdc, uint32_t color) noexcept {
    if (auto* dc = lookupDc(hdc)) {
        uint32_t prev = dc->textColor;
        dc->textColor = toBgra32(color);
        return prev;
    }
    return 0;
}

__declspec(dllexport) uint32_t SetBkColor(void* hdc, uint32_t color) noexcept {
    if (auto* dc = lookupDc(hdc)) {
        uint32_t prev = dc->bkColor;
        dc->bkColor = toBgra32(color);
        return prev;
    }
    return 0;
}

__declspec(dllexport) uint32_t SetPixel(void* hdc, int x, int y, uint32_t color) noexcept {
    auto* dc = lookupDc(hdc);
    if (!dc || !dc->pixels || x < 0 || y < 0) return 0;
    uint32_t ux = static_cast<uint32_t>(x);
    uint32_t uy = static_cast<uint32_t>(y);
    if (ux >= dc->width || uy >= dc->height) return 0;
    uint32_t px = toBgra32(color);
    dc->pixels[uy * dc->width + ux] = px;
    return px;
}

__declspec(dllexport) bool GdiFillSolidRect(void* hdc, int x, int y, int w, int h, uint32_t color) noexcept {
    auto* dc = lookupDc(hdc);
    if (!dc || !dc->pixels || w <= 0 || h <= 0) return false;
    int x0 = (x < 0) ? 0 : x;
    int y0 = (y < 0) ? 0 : y;
    int x1 = x + w;
    int y1 = y + h;
    if (x1 > static_cast<int>(dc->width)) x1 = static_cast<int>(dc->width);
    if (y1 > static_cast<int>(dc->height)) y1 = static_cast<int>(dc->height);
    if (x0 >= x1 || y0 >= y1) return false;

    uint32_t px = toBgra32(color);
    for (int cy = y0; cy < y1; ++cy) {
        uint32_t* row = dc->pixels + static_cast<size_t>(cy) * dc->width;
        for (int cx = x0; cx < x1; ++cx) {
            row[cx] = px;
        }
    }
    return true;
}

__declspec(dllexport) bool GdiDrawRectBorder(void* hdc, int x, int y, int w, int h, uint32_t color) noexcept {
    if (w <= 0 || h <= 0) return false;
    GdiFillSolidRect(hdc, x, y, w, 1, color);
    GdiFillSolidRect(hdc, x, y + h - 1, w, 1, color);
    GdiFillSolidRect(hdc, x, y, 1, h, color);
    GdiFillSolidRect(hdc, x + w - 1, y, 1, h, color);
    return true;
}

__declspec(dllexport) bool MoveToEx(void* hdc, int x, int y, void* /*lpPrev*/) noexcept {
    auto* dc = lookupDc(hdc);
    if (!dc) return false;
    dc->curX = x;
    dc->curY = y;
    return true;
}

__declspec(dllexport) bool LineTo(void* hdc, int x1, int y1) noexcept {
    auto* dc = lookupDc(hdc);
    if (!dc || !dc->pixels) return false;
    int x0 = dc->curX;
    int y0 = dc->curY;
    int dx = (x1 >= x0) ? (x1 - x0) : (x0 - x1);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = (y1 >= y0) ? -(y1 - y0) : -(y0 - y1);
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;
    uint32_t px = dc->penColor;

    while (true) {
        if (x0 >= 0 && y0 >= 0 && static_cast<uint32_t>(x0) < dc->width && static_cast<uint32_t>(y0) < dc->height) {
            dc->pixels[static_cast<size_t>(y0) * dc->width + static_cast<size_t>(x0)] = px;
        }
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
    dc->curX = x1;
    dc->curY = y1;
    return true;
}

__declspec(dllexport) bool GdiTextOutScaledA(void* hdc, int x, int y, const char* str, int len, int scale) noexcept {
    auto* dc = lookupDc(hdc);
    if (!dc || !dc->pixels || !str) return false;
    if (scale < 1) scale = 1;
    if (scale > 4) scale = 4;

    int curX = x;
    uint32_t fg = dc->textColor;
    for (int idx = 0; str[idx] && (len < 0 || idx < len); ++idx) {
        uint8_t rows[8];
        getGlyph8x8(str[idx], rows);
        for (int r = 0; r < 8; ++r) {
            uint8_t bits = rows[r];
            for (int c = 0; c < 8; ++c) {
                if (bits & (0x80u >> c)) {
                    for (int sy = 0; sy < scale; ++sy) {
                        int py = y + r * scale + sy;
                        if (py < 0 || static_cast<uint32_t>(py) >= dc->height) continue;
                        uint32_t* dstRow = dc->pixels + static_cast<size_t>(py) * dc->width;
                        for (int sx = 0; sx < scale; ++sx) {
                            int px = curX + c * scale + sx;
                            if (px >= 0 && static_cast<uint32_t>(px) < dc->width) {
                                dstRow[px] = fg;
                            }
                        }
                    }
                }
            }
        }
        curX += 8 * scale;
    }
    return true;
}

__declspec(dllexport) bool TextOutA(void* hdc, int x, int y, const char* lpString, int c) noexcept {
    return GdiTextOutScaledA(hdc, x, y, lpString, c, 1);
}

__declspec(dllexport) bool TextOutW(void* hdc, int x, int y, const wchar_t* lpString, int c) noexcept {
    if (!lpString) return false;
    char buf[160];
    int n = 0;
    while (lpString[n] && (c < 0 || n < c) && n < 159) {
        buf[n] = (lpString[n] < 128) ? static_cast<char>(lpString[n]) : '?';
        ++n;
    }
    buf[n] = '\0';
    return GdiTextOutScaledA(hdc, x, y, buf, n, 1);
}

// Present Ring-3 DC surface directly to physical UEFI GOP VRAM via ntdll!NtGdiPresentSurface (SSN 0x81)
__declspec(dllexport) bool BitBlt(
    void* /*hdcDest*/,
    int xDest,
    int yDest,
    int w,
    int h,
    void* hdcSrc,
    int /*xSrc*/,
    int /*ySrc*/,
    uint32_t /*rop*/
) noexcept {
    auto* srcDc = lookupDc(hdcSrc);
    if (!srcDc || !srcDc->pixels) return false;
    uint32_t bw = (w > 0 && static_cast<uint32_t>(w) <= srcDc->width) ? static_cast<uint32_t>(w) : srcDc->width;
    uint32_t bh = (h > 0 && static_cast<uint32_t>(h) <= srcDc->height) ? static_cast<uint32_t>(h) : srcDc->height;

    alignas(8) UserPresentSurfaceParams64 params;
    params.surfaceBufferVa = reinterpret_cast<uint64_t>(srcDc->pixels);
    params.dstX = static_cast<uint32_t>(xDest >= 0 ? xDest : srcDc->dstX);
    params.dstY = static_cast<uint32_t>(yDest >= 0 ? yDest : srcDc->dstY);
    params.width = bw;
    params.height = bh;
    params.pitch = srcDc->width;
    params.reserved = 0;

    return NtGdiPresentSurface(&params) == 0;
}

} // extern "C"
