// ============================================================================
// MicaNT: Sovereign Bare-Metal Win32k Window Manager, DWM Compositor &
//         i8042 Hardware PS/2 Mouse Engine (win32k_baremetal.hpp)
//
// Description:
//   Pure -ffreestanding -nostdlib ISO C++23 bare-metal windowing subsystem
//   compiled directly into ntoskrnl.exe (Ring 0).
//
//   Implements genuine multi-process Windows NT desktop composition:
//     - Hardware i8042 PS/2 Auxiliary Mouse driver (ports 0x64/0x60, AUXB bit 5,
//       3-byte signed packet decoder, live hardware cursor sprite, hit-testing,
//       click-to-focus Z-order elevation, and live title-bar window dragging)
//     - Up to 6 simultaneous independent Ring-3 GUI windows (HWND >= 0x10001)
//       owned by separate isolated-CR3 processes, each backed by its own 4 MB
//       physical pixel surface (0x18800000 + slot * 4MB)
//     - Tear-free 1920x1080 32-bpp BGRA shadow backbuffer (0x18000000) with
//       deep-space Mica wallpaper gradient, translucent taskbar with live
//       window tabs & hardware telemetry tray, back-to-front Z-order alpha drop
//       shadows, active/inactive window borders & title bars, and single linear
//       scanout blit into physical UEFI GOP VRAM
//     - Non-blocking per-window Win32 MSG ring buffers (WM_PAINT, WM_CHAR,
//       WM_KEYDOWN, WM_LBUTTONDOWN, WM_LBUTTONUP, WM_MOUSEMOVE, WM_SETFOCUS,
//       WM_CLOSE) enabling true preemptive concurrency across isolated CR3 apps
//
// Clean-Room ISO C++23. Zero proprietary headers or third-party code.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include "micant/km/hal/bootvid.hpp"

namespace micant::km::win32k_baremetal {

// ============================================================================
// 1. Canonical Win32 Message & Request Structures (Shared ABI with ntdll/user32)
// ============================================================================

inline constexpr uint32_t WM_CREATE      = 0x0001u;
inline constexpr uint32_t WM_DESTROY     = 0x0002u;
inline constexpr uint32_t WM_SETFOCUS    = 0x0007u;
inline constexpr uint32_t WM_PAINT       = 0x000Fu;
inline constexpr uint32_t WM_CLOSE       = 0x0010u;
inline constexpr uint32_t WM_QUIT        = 0x0012u;
inline constexpr uint32_t WM_KEYDOWN     = 0x0100u;
inline constexpr uint32_t WM_CHAR        = 0x0102u;
inline constexpr uint32_t WM_MOUSEMOVE   = 0x0200u;
inline constexpr uint32_t WM_LBUTTONDOWN = 0x0201u;
inline constexpr uint32_t WM_LBUTTONUP   = 0x0202u;

inline constexpr uint32_t VK_ESCAPE      = 0x1Bu;
inline constexpr uint32_t VK_F2          = 0x71u;
inline constexpr uint32_t VK_F10         = 0x79u;

inline constexpr uint64_t DWM_HWND_BASE  = 0x10001ULL;

#pragma pack(push, 1)
struct DwmPoint {
    int32_t x;
    int32_t y;
};

struct DwmMsg {
    uint64_t hwnd;
    uint32_t message;
    uint64_t wParam;
    int64_t  lParam;
    uint32_t time;
    DwmPoint pt;
    uint32_t lPrivate;
};

struct NtCreateWindowReq {
    int32_t  x;
    int32_t  y;
    int32_t  width;
    int32_t  height;
    uint32_t style;
    uint32_t reserved;
    char     title[64];
};
#pragma pack(pop)

// Dedicated Identity-Mapped Physical RAM Arena above 384 MB PFN Pool:
//   0x18000000 .. 0x187FFFFF (8 MB): 1920x1080x4 Tear-Free DWM Shadow Backbuffer
//   0x18800000 .. 0x19FFFFFF (24 MB): 6 x 4 MB Per-Window Client Backing Surfaces
inline constexpr uint64_t DWM_SHADOW_PHYS_BASE  = 0x18000000ULL;
inline constexpr uint64_t DWM_SURFACE_PHYS_BASE = 0x18800000ULL;
inline constexpr uint64_t DWM_SURFACE_STRIDE    = 0x00400000ULL; // 4 MB per window
inline constexpr size_t   DWM_MAX_WINDOWS       = 6;
inline constexpr size_t   DWM_MSG_QUEUE_CAP     = 32;
inline constexpr int32_t  DWM_TITLEBAR_H        = 30;
inline constexpr int32_t  DWM_TASKBAR_H         = 44;

struct DwmWindowSlot {
    bool      inUse;
    bool      visible;
    bool      hasSurface;
    bool      minimized;
    uint64_t  hwnd;
    uint32_t  ownerPid;
    int32_t   x;
    int32_t   y;
    int32_t   clientW;
    int32_t   clientH;
    uint32_t  zOrder;
    char      title[64];
    uint32_t* pixels; // Points to DWM_SURFACE_PHYS_BASE + slot * 4MB
    uint32_t  surfaceW;
    uint32_t  surfaceH;
    DwmMsg    queue[DWM_MSG_QUEUE_CAP];
    uint32_t  qHead;
    uint32_t  qTail;
    uint32_t  qCount;
};

// ============================================================================
// 2. Freestanding Bare-Metal Win32k Compositor & i8042 PS/2 Mouse Engine
// ============================================================================

class BareMetalWin32kCompositor {
private:
    bool      m_initialized;
    bool      m_desktopActive;
    bool      m_dirty;
    bool      m_mouseReady;
    bool      m_shiftDown;
    bool      m_ctrlDown;
    bool      m_ps2Ext;

    uint32_t  m_screenW;
    uint32_t  m_screenH;
    uint32_t  m_screenPitch;
    uint32_t  m_pixelFormat;
    uint32_t* m_gopFb;
    uint32_t* m_backBuf;

    // Live hardware i8042 PS/2 Mouse state
    uint8_t   m_mousePkt[3];
    uint8_t   m_mousePktIdx;
    int32_t   m_mouseX;
    int32_t   m_mouseY;
    bool      m_leftDown;
    bool      m_rightDown;
    uint64_t  m_mousePacketCount;
    uint64_t  m_clickCount;
    uint64_t  m_dragMoveCount;
    uint64_t  m_compositeCount;
    uint64_t  m_tickCounter;

    // Window Z-order, focus, and live title-bar drag state
    DwmWindowSlot m_windows[DWM_MAX_WINDOWS];
    uint64_t  m_nextHwnd;
    uint32_t  m_nextZOrder;
    uint64_t  m_focusedHwnd;
    uint64_t  m_draggingHwnd;
    int32_t   m_dragOffX;
    int32_t   m_dragOffY;

    // Live tray status string updated by kernel
    char      m_trayText[96];

    // Optional UART callback for concise hardware telemetry
    using LogFn = void (*)(const char*);
    LogFn     m_logFn;

    static inline uint8_t inb(uint16_t port) noexcept {
        uint8_t ret = 0;
#if defined(__x86_64__)
        __asm__ __volatile__("inb %1, %0" : "=a"(ret) : "Nd"(port));
#endif
        return ret;
    }

    static inline void outb(uint16_t port, uint8_t val) noexcept {
#if defined(__x86_64__)
        __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
#endif
    }

    static bool waitKbcInputClear() noexcept {
        for (int i = 0; i < 100000; ++i) {
            if ((inb(0x64) & 0x02u) == 0) return true;
        }
        return false;
    }

    static bool waitKbcOutputFull() noexcept {
        for (int i = 0; i < 100000; ++i) {
            if ((inb(0x64) & 0x01u) != 0) return true;
        }
        return false;
    }

    static void mouseWrite(uint8_t cmd) noexcept {
        waitKbcInputClear();
        outb(0x64, 0xD4);
        waitKbcInputClear();
        outb(0x60, cmd);
    }

    static uint8_t mouseRead() noexcept {
        if (waitKbcOutputFull()) return inb(0x60);
        return 0;
    }

    inline uint32_t packColor(uint8_t r, uint8_t g, uint8_t b) const noexcept {
        if (m_pixelFormat == 0) {
            return 0xFF000000u | (static_cast<uint32_t>(b) << 16) | (static_cast<uint32_t>(g) << 8) | r;
        }
        return 0xFF000000u | (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | b;
    }

    inline uint32_t bgraToNative(uint32_t bgra) const noexcept {
        if (m_pixelFormat != 0) return bgra | 0xFF000000u;
        uint32_t r = (bgra >> 16) & 0xFFu;
        uint32_t g = (bgra >> 8) & 0xFFu;
        uint32_t b = bgra & 0xFFu;
        return 0xFF000000u | (b << 16) | (g << 8) | r;
    }

    void fillRectBack(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t px) noexcept {
        if (!m_backBuf || w <= 0 || h <= 0) return;
        int32_t x0 = (x < 0) ? 0 : x;
        int32_t y0 = (y < 0) ? 0 : y;
        int32_t x1 = x + w;
        int32_t y1 = y + h;
        if (x1 > static_cast<int32_t>(m_screenW)) x1 = static_cast<int32_t>(m_screenW);
        if (y1 > static_cast<int32_t>(m_screenH)) y1 = static_cast<int32_t>(m_screenH);
        if (x0 >= x1 || y0 >= y1) return;
        for (int32_t cy = y0; cy < y1; ++cy) {
            uint32_t* row = m_backBuf + static_cast<size_t>(cy) * m_screenW;
            for (int32_t cx = x0; cx < x1; ++cx) row[cx] = px;
        }
    }

    // Fast 50% darken shadow blend on backbuffer (zero floats, pure bit-shift!)
    void darkenRectBack(int32_t x, int32_t y, int32_t w, int32_t h) noexcept {
        if (!m_backBuf || w <= 0 || h <= 0) return;
        int32_t x0 = (x < 0) ? 0 : x;
        int32_t y0 = (y < 0) ? 0 : y;
        int32_t x1 = x + w;
        int32_t y1 = y + h;
        if (x1 > static_cast<int32_t>(m_screenW)) x1 = static_cast<int32_t>(m_screenW);
        if (y1 > static_cast<int32_t>(m_screenH)) y1 = static_cast<int32_t>(m_screenH);
        if (x0 >= x1 || y0 >= y1) return;
        for (int32_t cy = y0; cy < y1; ++cy) {
            uint32_t* row = m_backBuf + static_cast<size_t>(cy) * m_screenW;
            for (int32_t cx = x0; cx < x1; ++cx) {
                row[cx] = 0xFF000000u | ((row[cx] >> 1) & 0x007F7F7Fu);
            }
        }
    }

    void drawBorderBack(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t px) noexcept {
        fillRectBack(x, y, w, 1, px);
        fillRectBack(x, y + h - 1, w, 1, px);
        fillRectBack(x, y, 1, h, px);
        fillRectBack(x + w - 1, y, 1, h, px);
    }

    void drawCharBack(int32_t x, int32_t y, char c, uint32_t fg) noexcept {
        if (!m_backBuf) return;
        if (c < 32 || c > 126) c = '?';
        const uint8_t* glyph = micant::bootvid::font::GLYPH_DATA[c - 32];
        for (int32_t r = 0; r < 8; ++r) {
            int32_t py = y + r;
            if (py < 0 || py >= static_cast<int32_t>(m_screenH)) continue;
            uint8_t bits = glyph[r];
            uint32_t* row = m_backBuf + static_cast<size_t>(py) * m_screenW;
            for (int32_t col = 0; col < 8; ++col) {
                if (bits & (0x80u >> col)) {
                    int32_t px = x + col;
                    if (px >= 0 && px < static_cast<int32_t>(m_screenW)) row[px] = fg;
                }
            }
        }
    }

    void drawStrBack(int32_t x, int32_t y, const char* s, uint32_t fg, int32_t maxChars = 128) noexcept {
        if (!s) return;
        for (int32_t i = 0; s[i] && i < maxChars; ++i) {
            drawCharBack(x + i * 8, y, s[i], fg);
        }
    }

    void drawCursorBack(int32_t mx, int32_t my) noexcept {
        // Authentic crisp 14x20 Windows-style arrow cursor sprite:
        // 'B' = black outline, 'W' = pure white fill, '.' = transparent
        static const char* const kArrow[20] = {
            "B.............",
            "BB............",
            "BWB...........",
            "BWWB..........",
            "BWWWB.........",
            "BWWWWB........",
            "BWWWWWB.......",
            "BWWWWWWB......",
            "BWWWWWWWB.....",
            "BWWWWWWWWB....",
            "BWWWWWWWWWB...",
            "BWWWWWWWWWWB..",
            "BWWWWWWBBBBBB.",
            "BWWWBWWB......",
            "BWWB.BWWB.....",
            "BWB..BWWB.....",
            "BB....BWWB....",
            "B.....BWWB....",
            ".......BB.....",
            ".............."
        };
        uint32_t cBlack = packColor(8, 12, 18);
        uint32_t cWhite = packColor(255, 255, 255);
        for (int32_t r = 0; r < 20; ++r) {
            int32_t py = my + r;
            if (py < 0 || py >= static_cast<int32_t>(m_screenH)) continue;
            uint32_t* row = m_backBuf + static_cast<size_t>(py) * m_screenW;
            const char* line = kArrow[r];
            for (int32_t c = 0; c < 14; ++c) {
                int32_t px = mx + c;
                if (px < 0 || px >= static_cast<int32_t>(m_screenW)) continue;
                if (line[c] == 'B') row[px] = cBlack;
                else if (line[c] == 'W') row[px] = cWhite;
            }
        }
    }

    DwmWindowSlot* findWindow(uint64_t hwnd) noexcept {
        for (size_t i = 0; i < DWM_MAX_WINDOWS; ++i) {
            if (m_windows[i].inUse && m_windows[i].hwnd == hwnd) return &m_windows[i];
        }
        return nullptr;
    }

    void pushMsg(DwmWindowSlot& w, uint32_t message, uint64_t wParam, int64_t lParam) noexcept {
        if (w.qCount >= DWM_MSG_QUEUE_CAP) {
            // Drop oldest mouse move if full; otherwise drop oldest
            w.qHead = (w.qHead + 1) % DWM_MSG_QUEUE_CAP;
            w.qCount--;
        }
        DwmMsg& m = w.queue[w.qTail];
        m.hwnd = w.hwnd;
        m.message = message;
        m.wParam = wParam;
        m.lParam = lParam;
        m.time = static_cast<uint32_t>(++m_tickCounter);
        m.pt.x = m_mouseX;
        m.pt.y = m_mouseY;
        m.lPrivate = 0;
        w.qTail = (w.qTail + 1) % DWM_MSG_QUEUE_CAP;
        w.qCount++;
    }

    void bringToFront(DwmWindowSlot& w) noexcept {
        w.zOrder = ++m_nextZOrder;
        if (m_focusedHwnd != w.hwnd) {
            m_focusedHwnd = w.hwnd;
            pushMsg(w, WM_SETFOCUS, 0, 0);
            pushMsg(w, WM_PAINT, 0, 0);
        }
        m_dirty = true;
    }

    DwmWindowSlot* hitTestWindow(int32_t px, int32_t py) noexcept {
        DwmWindowSlot* best = nullptr;
        for (size_t i = 0; i < DWM_MAX_WINDOWS; ++i) {
            auto& w = m_windows[i];
            if (!w.inUse || !w.visible || w.minimized) continue;
            int32_t totalH = DWM_TITLEBAR_H + w.clientH + 2;
            int32_t totalW = w.clientW + 2;
            if (px >= w.x && px < w.x + totalW && py >= w.y && py < w.y + totalH) {
                if (!best || w.zOrder > best->zOrder) best = &w;
            }
        }
        return best;
    }

    void handleMousePacket(uint8_t b0, uint8_t b1, uint8_t b2) noexcept {
        m_mousePacketCount++;
        bool left  = (b0 & 0x01u) != 0;
        bool right = (b0 & 0x02u) != 0;
        int32_t dx = static_cast<int32_t>(b1) - ((b0 & 0x10u) ? 256 : 0);
        int32_t dy = static_cast<int32_t>(b2) - ((b0 & 0x20u) ? 256 : 0);

        int32_t oldX = m_mouseX;
        int32_t oldY = m_mouseY;
        m_mouseX += dx;
        m_mouseY -= dy; // PS/2 Y+ is upward; screen Y+ is downward
        if (m_mouseX < 0) m_mouseX = 0;
        if (m_mouseY < 0) m_mouseY = 0;
        if (m_mouseX >= static_cast<int32_t>(m_screenW)) m_mouseX = static_cast<int32_t>(m_screenW) - 1;
        if (m_mouseY >= static_cast<int32_t>(m_screenH)) m_mouseY = static_cast<int32_t>(m_screenH) - 1;

        if (m_mouseX != oldX || m_mouseY != oldY) {
            m_dirty = true;
        }

        // Left button press transition (0 -> 1)
        if (left && !m_leftDown) {
            m_clickCount++;
            int32_t tbY = static_cast<int32_t>(m_screenH) - DWM_TASKBAR_H;
            if (m_mouseY >= tbY) {
                // Taskbar click -> focus matching window tab!
                int32_t tabX = 148;
                for (size_t i = 0; i < DWM_MAX_WINDOWS; ++i) {
                    if (!m_windows[i].inUse || !m_windows[i].visible) continue;
                    if (m_mouseX >= tabX && m_mouseX < tabX + 210) {
                        bringToFront(m_windows[i]);
                        break;
                    }
                    tabX += 218;
                }
            } else if (auto* hit = hitTestWindow(m_mouseX, m_mouseY)) {
                int32_t totalW = hit->clientW + 2;
                int32_t relX = m_mouseX - hit->x;
                int32_t relY = m_mouseY - hit->y;
                if (relY < DWM_TITLEBAR_H && relX >= totalW - 34) {
                    // Close button [X]
                    pushMsg(*hit, WM_CLOSE, 0, 0);
                } else if (relY < DWM_TITLEBAR_H) {
                    // Title bar -> elevate Z-order & start live drag!
                    bringToFront(*hit);
                    m_draggingHwnd = hit->hwnd;
                    m_dragOffX = m_mouseX - hit->x;
                    m_dragOffY = m_mouseY - hit->y;
                    if (m_logFn) {
                        m_logFn("[Win32k] PS/2 Mouse Titlebar Click -> Elevated Z-Order & Started Drag!\r\n");
                    }
                } else {
                    // Client area -> elevate Z-order & deliver WM_LBUTTONDOWN
                    bringToFront(*hit);
                    int32_t cx = m_mouseX - (hit->x + 1);
                    int32_t cy = m_mouseY - (hit->y + DWM_TITLEBAR_H);
                    int64_t lp = (static_cast<int64_t>(cy & 0xFFFF) << 16) | (cx & 0xFFFF);
                    pushMsg(*hit, WM_LBUTTONDOWN, 1, lp);
                }
            }
        }
        // Active title-bar drag while holding left button
        else if (left && m_draggingHwnd != 0) {
            if (auto* dw = findWindow(m_draggingHwnd)) {
                int32_t nx = m_mouseX - m_dragOffX;
                int32_t ny = m_mouseY - m_dragOffY;
                if (nx < 0) nx = 0;
                if (ny < 0) ny = 0;
                if (nx > static_cast<int32_t>(m_screenW) - 160) nx = static_cast<int32_t>(m_screenW) - 160;
                if (ny > static_cast<int32_t>(m_screenH) - DWM_TASKBAR_H - 40) ny = static_cast<int32_t>(m_screenH) - DWM_TASKBAR_H - 40;
                if (nx != dw->x || ny != dw->y) {
                    dw->x = nx;
                    dw->y = ny;
                    m_dragMoveCount++;
                    m_dirty = true;
                }
            }
        }
        // Left button release transition (1 -> 0)
        else if (!left && m_leftDown) {
            if (m_draggingHwnd != 0) {
                if (m_logFn) {
                    m_logFn("[Win32k] PS/2 Mouse Drag Complete -> Window Repositioned on Desktop!\r\n");
                }
                m_draggingHwnd = 0;
                m_dirty = true;
            } else if (auto* hit = hitTestWindow(m_mouseX, m_mouseY)) {
                int32_t cx = m_mouseX - (hit->x + 1);
                int32_t cy = m_mouseY - (hit->y + DWM_TITLEBAR_H);
                int64_t lp = (static_cast<int64_t>(cy & 0xFFFF) << 16) | (cx & 0xFFFF);
                pushMsg(*hit, WM_LBUTTONUP, 0, lp);
            }
        }

        m_leftDown = left;
        m_rightDown = right;
    }

    void handleKeyboardScancode(uint8_t sc) noexcept {
        if (sc == 0xE0u) { m_ps2Ext = true; return; }
        if (m_ps2Ext) { m_ps2Ext = false; return; }

        if (sc == 0x2Au || sc == 0x36u) { m_shiftDown = true; return; }
        if (sc == 0xAAu || sc == 0xB6u) { m_shiftDown = false; return; }
        if (sc == 0x1Du) { m_ctrlDown = true; return; }
        if (sc == 0x9Du) { m_ctrlDown = false; return; }
        if (sc & 0x80u) return; // Key release

        // F10 -> Close all desktop windows cleanly (return to shell)
        if (sc == 0x44u) {
            broadcastCloseAll();
            return;
        }

        auto* target = findWindow(m_focusedHwnd);
        if (!target) {
            for (size_t i = 0; i < DWM_MAX_WINDOWS; ++i) {
                if (m_windows[i].inUse && m_windows[i].visible) { target = &m_windows[i]; break; }
            }
        }
        if (!target) return;

        // F2 -> Save shortcut (WM_KEYDOWN VK_F2)
        if (sc == 0x3Cu) {
            pushMsg(*target, WM_KEYDOWN, VK_F2, 1);
            return;
        }
        // ESC -> WM_KEYDOWN VK_ESCAPE
        if (sc == 0x01u) {
            pushMsg(*target, WM_KEYDOWN, VK_ESCAPE, 1);
            return;
        }

        static const char unshifted[128] = {
            0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
            '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\r',
            0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
            0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*',
            0, ' '
        };
        static const char shifted[128] = {
            0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
            '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\r',
            0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
            0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*',
            0, ' '
        };
        if (sc < 128) {
            char ch = m_shiftDown ? shifted[sc] : unshifted[sc];
            if (m_ctrlDown && (ch == 's' || ch == 'S')) {
                pushMsg(*target, WM_KEYDOWN, VK_F2, 1); // Ctrl+S == Save!
                return;
            }
            if (ch != 0) {
                pushMsg(*target, WM_CHAR, static_cast<uint64_t>(static_cast<uint8_t>(ch)), 1);
            }
        }
    }

    static void appendDec(char* dst, size_t& pos, size_t cap, uint64_t v) noexcept {
        char tmp[24];
        size_t n = 0;
        if (v == 0) tmp[n++] = '0';
        else { while (v > 0 && n < 22) { tmp[n++] = static_cast<char>('0' + (v % 10)); v /= 10; } }
        while (n > 0 && pos + 1 < cap) dst[pos++] = tmp[--n];
        dst[pos] = '\0';
    }

    static void appendStr(char* dst, size_t& pos, size_t cap, const char* s) noexcept {
        while (s && *s && pos + 1 < cap) dst[pos++] = *s++;
        dst[pos] = '\0';
    }

public:
    static BareMetalWin32kCompositor& get() noexcept {
        static BareMetalWin32kCompositor instance;
        return instance;
    }

    void initialize(micant::bootvid::BootVideoDriver* video, LogFn logFn = nullptr) noexcept {
        std::memset(this, 0, sizeof(*this));
        m_logFn = logFn;
        if (!video || !video->isInitialized()) return;

        m_screenW = video->getWidth();
        m_screenH = video->getHeight();
        m_screenPitch = video->getPitch();
        m_pixelFormat = video->getPixelFormat();
        m_gopFb = video->getRawBuffer();
        // Clamp compositor dimensions to the 8 MiB DWM_SHADOW_PHYS_BASE arena so >1080p modes never overflow into surfaces/AC'97 DMA!
        constexpr uint64_t DWM_SHADOW_BYTES = DWM_SURFACE_PHYS_BASE - DWM_SHADOW_PHYS_BASE; // 8 MiB
        if (m_screenW > 1920) m_screenW = 1920;
        if (m_screenW > 0 && static_cast<uint64_t>(m_screenW) * static_cast<uint64_t>(m_screenH) * 4ULL > DWM_SHADOW_BYTES) {
            m_screenH = static_cast<uint32_t>(DWM_SHADOW_BYTES / (static_cast<uint64_t>(m_screenW) * 4ULL));
        }
        m_backBuf = reinterpret_cast<uint32_t*>(DWM_SHADOW_PHYS_BASE);

        m_mouseX = static_cast<int32_t>(m_screenW / 2);
        m_mouseY = static_cast<int32_t>(m_screenH / 2);
        m_nextHwnd = DWM_HWND_BASE;
        m_nextZOrder = 1;

        size_t tp = 0;
        appendStr(m_trayText, tp, sizeof(m_trayText), "e1000 10.0.2.15 | 4C SMP | NTFS 3.1");

        for (size_t i = 0; i < DWM_MAX_WINDOWS; ++i) {
            m_windows[i].pixels = reinterpret_cast<uint32_t*>(DWM_SURFACE_PHYS_BASE + i * DWM_SURFACE_STRIDE);
        }

        initPs2Mouse();
        m_initialized = true;
    }

    void initPs2Mouse() noexcept {
        // Flush any stale bytes
        for (int i = 0; i < 32 && (inb(0x64) & 0x01u); ++i) (void)inb(0x60);

        // 1. Enable Auxiliary PS/2 Mouse device on i8042 controller
        waitKbcInputClear();
        outb(0x64, 0xA8);

        // 2. Read Compaq status byte, enable Aux clock (clear bit 5), disable IRQ12 (keep polled cleanly!)
        waitKbcInputClear();
        outb(0x64, 0x20);
        uint8_t status = waitKbcOutputFull() ? inb(0x60) : 0;
        status = static_cast<uint8_t>((status | 0x00u) & ~0x22u); // Clear bit 5 (aux clock disable) & bit 1 (no spurious IRQ12)
        waitKbcInputClear();
        outb(0x64, 0x60);
        waitKbcInputClear();
        outb(0x60, status);

        // 3. Set Defaults (0xF6) & Enable Packet Streaming (0xF4)
        mouseWrite(0xF6);
        (void)mouseRead(); // ACK 0xFA
        mouseWrite(0xF4);
        uint8_t ack = mouseRead();
        m_mouseReady = (ack == 0xFAu);

        // Flush post-init bytes
        for (int i = 0; i < 16 && (inb(0x64) & 0x01u); ++i) (void)inb(0x60);
    }

    void setTrayStatus(const char* text) noexcept {
        if (!text) return;
        size_t p = 0;
        appendStr(m_trayText, p, sizeof(m_trayText), text);
        m_dirty = true;
    }

    [[nodiscard]] bool isInitialized() const noexcept { return m_initialized; }
    [[nodiscard]] bool isMouseReady() const noexcept { return m_mouseReady; }
    [[nodiscard]] bool isDesktopActive() const noexcept { return m_desktopActive; }
    [[nodiscard]] int32_t getMouseX() const noexcept { return m_mouseX; }
    [[nodiscard]] int32_t getMouseY() const noexcept { return m_mouseY; }
    [[nodiscard]] uint64_t getFocusedHwnd() const noexcept { return m_focusedHwnd; }
    [[nodiscard]] uint64_t getMousePacketCount() const noexcept { return m_mousePacketCount; }
    [[nodiscard]] uint64_t getClickCount() const noexcept { return m_clickCount; }
    [[nodiscard]] uint64_t getDragMoveCount() const noexcept { return m_dragMoveCount; }
    [[nodiscard]] uint64_t getCompositeCount() const noexcept { return m_compositeCount; }

    [[nodiscard]] uint32_t getActiveWindowCount() const noexcept {
        uint32_t c = 0;
        for (size_t i = 0; i < DWM_MAX_WINDOWS; ++i) {
            if (m_windows[i].inUse && m_windows[i].visible) c++;
        }
        return c;
    }

    uint64_t createWindow(uint32_t ownerPid, const NtCreateWindowReq& req) noexcept {
        if (!m_initialized) return 0;
        for (size_t i = 0; i < DWM_MAX_WINDOWS; ++i) {
            if (!m_windows[i].inUse) {
                auto& w = m_windows[i];
                uint32_t* surfPtr = w.pixels;
                std::memset(&w, 0, sizeof(w));
                w.pixels = surfPtr;
                w.inUse = true;
                w.visible = true;
                w.hwnd = m_nextHwnd++;
                w.ownerPid = ownerPid;
                w.x = req.x;
                w.y = req.y;
                w.clientW = (req.width > 120 && req.width <= 1024) ? req.width : 640;
                w.clientH = (req.height > 80 && req.height <= 900) ? req.height : 420;
                w.zOrder = ++m_nextZOrder;
                size_t tp = 0;
                appendStr(w.title, tp, sizeof(w.title), req.title[0] ? req.title : "MicaNT Window");

                m_focusedHwnd = w.hwnd;
                m_desktopActive = true;
                m_dirty = true;
                pushMsg(w, WM_PAINT, 0, 0);
                return w.hwnd;
            }
        }
        return 0;
    }

    bool destroyWindow(uint64_t hwnd, uint32_t callerPid = 0) noexcept {
        auto* w = findWindow(hwnd);
        if (!w) return false;
        if (callerPid != 0 && w->ownerPid != callerPid) return false; // Enforce cross-process HWND isolation!
        uint32_t* surfPtr = w->pixels;
        std::memset(w, 0, sizeof(*w));
        w->pixels = surfPtr;

        // Pick next highest Z-order window for focus
        DwmWindowSlot* top = nullptr;
        for (size_t i = 0; i < DWM_MAX_WINDOWS; ++i) {
            if (m_windows[i].inUse && m_windows[i].visible) {
                if (!top || m_windows[i].zOrder > top->zOrder) top = &m_windows[i];
            }
        }
        m_focusedHwnd = top ? top->hwnd : 0;
        if (m_draggingHwnd == hwnd) m_draggingHwnd = 0;
        m_desktopActive = (top != nullptr);
        m_dirty = true;
        if (m_desktopActive) compositeToGop();
        return true;
    }

    void destroyWindowsForPid(uint32_t pid) noexcept {
        for (size_t i = 0; i < DWM_MAX_WINDOWS; ++i) {
            if (m_windows[i].inUse && m_windows[i].ownerPid == pid) {
                destroyWindow(m_windows[i].hwnd);
            }
        }
    }

    void broadcastCloseAll() noexcept {
        for (size_t i = 0; i < DWM_MAX_WINDOWS; ++i) {
            if (m_windows[i].inUse) {
                pushMsg(m_windows[i], WM_CLOSE, 0, 0);
            }
        }
        if (m_logFn) {
            m_logFn("[Win32k] Desktop F10/CloseAll -> Enqueued WM_CLOSE to all Ring-3 GUI windows.\r\n");
        }
    }

    // Called in caller's CR3 during NtGdiPresentSurface (0x81) when params->reserved == hwnd
    bool updateWindowSurface(uint64_t hwnd, const uint32_t* userPixels, uint32_t w, uint32_t h, uint32_t srcPitch, uint32_t callerPid = 0) noexcept {
        auto* win = findWindow(hwnd);
        if (!win || !userPixels || !win->pixels || w == 0 || h == 0) return false;
        if (callerPid != 0 && win->ownerPid != callerPid) return false; // Enforce cross-process HWND ownership!
        uint32_t cw = (w <= 1024) ? w : 1024;
        uint32_t ch = (h <= 900)  ? h : 900;
        uint32_t sp = (srcPitch >= cw) ? srcPitch : cw;

        for (uint32_t row = 0; row < ch; ++row) {
            std::memcpy(win->pixels + static_cast<size_t>(row) * cw,
                        userPixels + static_cast<size_t>(row) * sp,
                        cw * sizeof(uint32_t));
        }
        win->surfaceW = cw;
        win->surfaceH = ch;
        win->clientW = static_cast<int32_t>(cw);
        win->clientH = static_cast<int32_t>(ch);
        win->hasSurface = true;
        m_dirty = true;
        compositeToGop();
        return true;
    }

    // Drain ALL pending hardware PS/2 Mouse & Keyboard bytes (never stalls!)
    void pumpHardwareInput() noexcept {
        for (int iter = 0; iter < 64; ++iter) {
            uint8_t st = inb(0x64);
            if ((st & 0x01u) == 0) break;
            uint8_t data = inb(0x60);
            if ((st & 0x20u) != 0) {
                // Auxiliary PS/2 Mouse packet byte
                if (m_mousePktIdx == 0 && (data & 0x08u) == 0) {
                    continue; // Wait for valid sync bit 3
                }
                m_mousePkt[m_mousePktIdx++] = data;
                if (m_mousePktIdx >= 3) {
                    m_mousePktIdx = 0;
                    handleMousePacket(m_mousePkt[0], m_mousePkt[1], m_mousePkt[2]);
                }
            } else {
                // Primary PS/2 Keyboard scancode
                handleKeyboardScancode(data);
            }
        }
        if (m_desktopActive && m_dirty) {
            compositeToGop();
        }
    }

    // Non-blocking message dequeue for NtUserPeekMessage (0x85)
    bool peekMessage(uint64_t hwnd, uint32_t ownerPid, DwmMsg& outMsg, bool removeMsg) noexcept {
        pumpHardwareInput();

        DwmWindowSlot* win = (hwnd != 0) ? findWindow(hwnd) : nullptr;
        if (win && ownerPid != 0 && win->ownerPid != ownerPid) {
            return false; // Prevent cross-process keystroke/message sniffing!
        }
        if (!win) {
            for (size_t i = 0; i < DWM_MAX_WINDOWS; ++i) {
                if (m_windows[i].inUse && m_windows[i].ownerPid == ownerPid) {
                    win = &m_windows[i];
                    break;
                }
            }
        }
        if (!win || win->qCount == 0) return false;

        outMsg = win->queue[win->qHead];
        if (removeMsg) {
            win->qHead = (win->qHead + 1) % DWM_MSG_QUEUE_CAP;
            win->qCount--;
        }
        return true;
    }

    // Full tear-free 1920x1080 DWM composition pass into shadow buffer -> single scanout blit to UEFI GOP
    void compositeToGop() noexcept {
        if (!m_initialized || !m_backBuf || !m_gopFb || !m_desktopActive) return;

        // 1. Deep-Space Mica Carbon-Glass Desktop Wallpaper Gradient (zero floats!)
        uint32_t deskH = (m_screenH > static_cast<uint32_t>(DWM_TASKBAR_H)) ? (m_screenH - DWM_TASKBAR_H) : m_screenH;
        for (uint32_t y = 0; y < deskH; ++y) {
            uint8_t r = static_cast<uint8_t>(10 + (y * 12) / deskH);
            uint8_t g = static_cast<uint8_t>(16 + (y * 22) / deskH);
            uint8_t b = static_cast<uint8_t>(30 + (y * 36) / deskH);
            uint32_t rowPx = packColor(r, g, b);
            uint32_t* dstRow = m_backBuf + static_cast<size_t>(y) * m_screenW;
            for (uint32_t x = 0; x < m_screenW; ++x) dstRow[x] = rowPx;
        }

        // Subtle Mica Prism desktop watermark top-left
        drawStrBack(22, 16, "MicaNT 10.0 Sovereign Bare-Metal Desktop  |  Preemptive Multi-CR3 DWM Compositor + i8042 PS/2 Mouse",
                    packColor(115, 168, 215));
        drawStrBack(22, 30, "Click any window titlebar to focus/drag  |  F2/Ctrl+S saves Notepad to NTFS 3.1  |  F10 exits desktop",
                    packColor(78, 118, 160));

        // 2. Sort visible windows back-to-front by Z-Order
        size_t order[DWM_MAX_WINDOWS];
        size_t count = 0;
        for (size_t i = 0; i < DWM_MAX_WINDOWS; ++i) {
            if (m_windows[i].inUse && m_windows[i].visible && !m_windows[i].minimized) {
                order[count++] = i;
            }
        }
        for (size_t a = 0; a < count; ++a) {
            for (size_t b = a + 1; b < count; ++b) {
                if (m_windows[order[b]].zOrder < m_windows[order[a]].zOrder) {
                    size_t tmp = order[a];
                    order[a] = order[b];
                    order[b] = tmp;
                }
            }
        }

        // 3. Composite each window in strict back-to-front Z-Order!
        for (size_t k = 0; k < count; ++k) {
            const auto& w = m_windows[order[k]];
            bool isFocused = (w.hwnd == m_focusedHwnd);
            int32_t totalW = w.clientW + 2;
            int32_t totalH = DWM_TITLEBAR_H + w.clientH + 2;

            // Translucent Drop Shadow (+8px X, +10px Y)
            darkenRectBack(w.x + 8, w.y + 10, totalW, totalH);

            // Title Bar Chrome
            uint32_t borderCol = isFocused ? packColor(0, 229, 255) : packColor(52, 74, 108);
            uint32_t titleBg   = isFocused ? packColor(20, 38, 66)  : packColor(18, 25, 40);
            uint32_t titleFg   = isFocused ? packColor(238, 248, 255) : packColor(145, 168, 198);

            fillRectBack(w.x, w.y, totalW, DWM_TITLEBAR_H, titleBg);
            fillRectBack(w.x + 1, w.y + DWM_TITLEBAR_H - 1, totalW - 2, 1, borderCol);

            // Small Accent Window Emblem + Title + Owner PID
            fillRectBack(w.x + 10, w.y + 9, 12, 12, isFocused ? packColor(0, 229, 255) : packColor(90, 120, 160));
            fillRectBack(w.x + 13, w.y + 12, 6, 6, titleBg);

            char caption[96];
            size_t cp = 0;
            appendStr(caption, cp, sizeof(caption), w.title);
            appendStr(caption, cp, sizeof(caption), "  [PID ");
            appendDec(caption, cp, sizeof(caption), w.ownerPid);
            appendStr(caption, cp, sizeof(caption), isFocused ? " | ACTIVE]" : "]");
            drawStrBack(w.x + 30, w.y + 11, caption, titleFg, (totalW - 120) / 8);

            // Window Caption Buttons ([_] [O] [X])
            int32_t bx = w.x + totalW - 96;
            fillRectBack(bx,      w.y + 5, 26, 20, packColor(30, 46, 74));
            fillRectBack(bx + 30, w.y + 5, 26, 20, packColor(30, 46, 74));
            fillRectBack(bx + 60, w.y + 5, 30, 20, isFocused ? packColor(196, 43, 60) : packColor(110, 42, 54));
            drawStrBack(bx + 9,  w.y + 11, "_", packColor(220, 235, 255));
            drawStrBack(bx + 39, w.y + 11, "O", packColor(220, 235, 255));
            drawStrBack(bx + 71, w.y + 11, "X", packColor(255, 245, 245));

            // Blit Ring-3 Process Private Client Surface into Window Rect
            int32_t cx0 = w.x + 1;
            int32_t cy0 = w.y + DWM_TITLEBAR_H;
            if (w.hasSurface && w.pixels && w.surfaceW > 0 && w.surfaceH > 0) {
                uint32_t drawW = (w.surfaceW < static_cast<uint32_t>(w.clientW)) ? w.surfaceW : static_cast<uint32_t>(w.clientW);
                uint32_t drawH = (w.surfaceH < static_cast<uint32_t>(w.clientH)) ? w.surfaceH : static_cast<uint32_t>(w.clientH);
                for (uint32_t row = 0; row < drawH; ++row) {
                    int32_t dy = cy0 + static_cast<int32_t>(row);
                    if (dy < 0 || dy >= static_cast<int32_t>(deskH)) continue;
                    const uint32_t* srcRow = w.pixels + static_cast<size_t>(row) * w.surfaceW;
                    uint32_t* dstRow = m_backBuf + static_cast<size_t>(dy) * m_screenW;
                    int32_t xStart = (cx0 < 0) ? -cx0 : 0;
                    int32_t xEnd = static_cast<int32_t>(drawW);
                    if (cx0 + xEnd > static_cast<int32_t>(m_screenW)) xEnd = static_cast<int32_t>(m_screenW) - cx0;
                    if (m_pixelFormat != 0) {
                        if (xEnd > xStart) {
                            std::memcpy(dstRow + cx0 + xStart, srcRow + xStart, static_cast<size_t>(xEnd - xStart) * sizeof(uint32_t));
                        }
                    } else {
                        for (int32_t sx = xStart; sx < xEnd; ++sx) {
                            dstRow[cx0 + sx] = bgraToNative(srcRow[sx]);
                        }
                    }
                }
            } else {
                fillRectBack(cx0, cy0, w.clientW, w.clientH, packColor(15, 21, 34));
            }

            // Crisp Outer Window Frame Border
            drawBorderBack(w.x, w.y, totalW, totalH, borderCol);
            if (isFocused) {
                drawBorderBack(w.x - 1, w.y - 1, totalW + 2, totalH + 2, packColor(0, 128, 192));
            }
        }

        // 4. MicaNT Fluent Taskbar at Bottom (44px)
        int32_t tbY = static_cast<int32_t>(m_screenH) - DWM_TASKBAR_H;
        fillRectBack(0, tbY, static_cast<int32_t>(m_screenW), DWM_TASKBAR_H, packColor(14, 21, 34));
        fillRectBack(0, tbY, static_cast<int32_t>(m_screenW), 1, packColor(0, 185, 235));

        // Start Pill
        fillRectBack(12, tbY + 7, 122, 30, packColor(22, 42, 72));
        drawBorderBack(12, tbY + 7, 122, 30, packColor(0, 229, 255));
        fillRectBack(22, tbY + 14, 7, 7, packColor(0, 229, 255));
        fillRectBack(31, tbY + 14, 7, 7, packColor(100, 240, 160));
        fillRectBack(22, tbY + 23, 7, 7, packColor(255, 215, 0));
        fillRectBack(31, tbY + 23, 7, 7, packColor(176, 96, 255));
        drawStrBack(46, tbY + 18, "MicaNT", packColor(238, 248, 255));

        // Live Running Window Taskbar Tabs
        int32_t tabX = 148;
        for (size_t i = 0; i < DWM_MAX_WINDOWS; ++i) {
            const auto& w = m_windows[i];
            if (!w.inUse || !w.visible) continue;
            bool act = (w.hwnd == m_focusedHwnd);
            fillRectBack(tabX, tbY + 7, 210, 30, act ? packColor(26, 52, 88) : packColor(20, 30, 48));
            drawBorderBack(tabX, tbY + 7, 210, 30, act ? packColor(0, 229, 255) : packColor(45, 65, 98));
            if (act) {
                fillRectBack(tabX + 20, tbY + 34, 170, 3, packColor(0, 229, 255));
            }
            drawStrBack(tabX + 10, tbY + 18, w.title, act ? packColor(245, 250, 255) : packColor(155, 178, 208), 23);
            tabX += 218;
        }

        // Right System Tray Pill (Live Hardware Telemetry + Mouse Coords)
        char trayBuf[128];
        size_t tp = 0;
        appendStr(trayBuf, tp, sizeof(trayBuf), m_trayText);
        appendStr(trayBuf, tp, sizeof(trayBuf), " | PS/2 (");
        appendDec(trayBuf, tp, sizeof(trayBuf), static_cast<uint64_t>(m_mouseX));
        appendStr(trayBuf, tp, sizeof(trayBuf), ",");
        appendDec(trayBuf, tp, sizeof(trayBuf), static_cast<uint64_t>(m_mouseY));
        appendStr(trayBuf, tp, sizeof(trayBuf), ")");
        int32_t trayW = static_cast<int32_t>(tp * 8 + 24);
        int32_t trayX = static_cast<int32_t>(m_screenW) - trayW - 12;
        fillRectBack(trayX, tbY + 7, trayW, 30, packColor(18, 30, 50));
        drawBorderBack(trayX, tbY + 7, trayW, 30, packColor(46, 76, 118));
        drawStrBack(trayX + 12, tbY + 18, trayBuf, packColor(110, 240, 175));

        // 5. Hardware PS/2 Arrow Mouse Cursor on Top of Everything!
        drawCursorBack(m_mouseX, m_mouseY);

        // 6. Single Tear-Free Linear Scanout Blit -> Physical UEFI GOP VRAM
        if (m_screenPitch == m_screenW) {
            std::memcpy(m_gopFb, m_backBuf, static_cast<size_t>(m_screenW) * m_screenH * sizeof(uint32_t));
        } else {
            for (uint32_t y = 0; y < m_screenH; ++y) {
                std::memcpy(m_gopFb + static_cast<size_t>(y) * m_screenPitch,
                            m_backBuf + static_cast<size_t>(y) * m_screenW,
                            m_screenW * sizeof(uint32_t));
            }
        }

        m_dirty = false;
        m_compositeCount++;
    }
};

} // namespace micant::km::win32k_baremetal
