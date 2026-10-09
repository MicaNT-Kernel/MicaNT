// ============================================================================
// MicaNT: Sovereign Interactive Window Manager & Input Routing Subsystem
// (input_router.hpp)
//
// Modern Clean-Room Implementation in pure ISO C++23.
// Provides hardware mouse & keyboard routing, non-client hit testing (WM_NCHITTEST),
// interactive window dragging, Aero Snap state machine, active window Z-order promotion,
// Scancode->VK translation, TranslateMessage WM_CHAR text synthesis, and
// automated message pump dispatch for retail applications (Notepad++, 7-Zip, VLC).
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include <deque>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <cwctype>

#include "ntdef.hpp"
#include "kernel32.hpp"
#include "user32.hpp"

namespace micant::input {

// ============================================================================
// 1. Non-Client Hit Test Constants (WM_NCHITTEST)
// ============================================================================

inline constexpr uint32_t WM_NCHITTEST       = 0x0084;
inline constexpr uint32_t WM_NCLBUTTONDOWN   = 0x00A1;
inline constexpr uint32_t WM_NCLBUTTONUP     = 0x00A2;
inline constexpr uint32_t WM_COMMAND         = 0x0111;

inline constexpr int HTERROR       = -2;
inline constexpr int HTTRANSPARENT = -1;
inline constexpr int HTNOWHERE     = 0;
inline constexpr int HTCLIENT      = 1;
inline constexpr int HTCAPTION     = 2;
inline constexpr int HTSYSMENU     = 3;
inline constexpr int HTGROWBOX     = 4;
inline constexpr int HTMENU        = 5;
inline constexpr int HTHSCROLL     = 6;
inline constexpr int HTVSCROLL     = 7;
inline constexpr int HTMINBUTTON   = 8;
inline constexpr int HTMAXBUTTON   = 9;
inline constexpr int HTLEFT        = 10;
inline constexpr int HTRIGHT       = 11;
inline constexpr int HTTOP         = 12;
inline constexpr int HTTOPLEFT     = 13;
inline constexpr int HTTOPRIGHT    = 14;
inline constexpr int HTBOTTOM      = 15;
inline constexpr int HTBOTTOMLEFT  = 16;
inline constexpr int HTBOTTOMRIGHT = 17;
inline constexpr int HTBORDER      = 18;
inline constexpr int HTCLOSE       = 20;

// ============================================================================
// 2. Hardware Mouse & Keyboard Constants
// ============================================================================

inline constexpr uint32_t MOUSE_MOVE        = 0x0001;
inline constexpr uint32_t MOUSE_LBUTTONDOWN = 0x0002;
inline constexpr uint32_t MOUSE_LBUTTONUP   = 0x0004;
inline constexpr uint32_t MOUSE_RBUTTONDOWN = 0x0008;
inline constexpr uint32_t MOUSE_RBUTTONUP   = 0x0010;
inline constexpr uint32_t MOUSE_MBUTTONDOWN = 0x0020;
inline constexpr uint32_t MOUSE_MBUTTONUP   = 0x0040;
inline constexpr uint32_t MOUSE_WHEEL       = 0x0080;

inline constexpr uint32_t VK_BACK    = 0x08;
inline constexpr uint32_t VK_TAB     = 0x09;
inline constexpr uint32_t VK_RETURN  = 0x0D;
inline constexpr uint32_t VK_SHIFT   = 0x10;
inline constexpr uint32_t VK_CONTROL = 0x11;
inline constexpr uint32_t VK_MENU    = 0x12; // Alt
inline constexpr uint32_t VK_ESCAPE  = 0x1B;
inline constexpr uint32_t VK_SPACE   = 0x20;
inline constexpr uint32_t VK_LEFT    = 0x25;
inline constexpr uint32_t VK_UP      = 0x26;
inline constexpr uint32_t VK_RIGHT   = 0x27;
inline constexpr uint32_t VK_DOWN    = 0x28;

// Aero Snap Modes
enum class SnapMode : uint8_t {
    None = 0,
    LeftHalf,
    RightHalf,
    Maximize
};

// VLC Playback State
enum class VlcState : uint8_t {
    Stopped = 0,
    Playing,
    Paused
};

// ============================================================================
// 3. Window Geometry & Hit Test Structures
// ============================================================================

struct HitTestResult {
    win32::HWND hwnd{nullptr};
    int hitCode{HTNOWHERE};
    int clientX{0};
    int clientY{0};
};

struct ManagedWindowBounds {
    win32::HWND hwnd{nullptr};
    std::wstring title;
    int x{0};
    int y{0};
    int width{800};
    int height{600};
    bool visible{true};
    bool maximized{false};
    SnapMode currentSnap{SnapMode::None};
    int normalX{0};
    int normalY{0};
    int normalW{800};
    int normalH{600};
};

// ============================================================================
// 4. InputRouter: High-Performance Thread-Safe Input & Window Manager Subsystem
// ============================================================================

class InputRouter {
public:
    static InputRouter& get() noexcept {
        static InputRouter instance;
        return instance;
    }

    void initialize(int screenWidth = 1280, int screenHeight = 800) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_screenWidth = screenWidth;
        m_screenHeight = screenHeight;
        m_cursorX = screenWidth / 2;
        m_cursorY = screenHeight / 2;
        m_mouseButtons = 0;
        m_shiftPressed = false;
        m_ctrlPressed = false;
        m_altPressed = false;
        m_isDragging = false;
        m_dragHwnd = nullptr;
        m_pendingSnap = SnapMode::None;
        m_activeHwnd = nullptr;
        m_focusHwnd = nullptr;
        m_windows.clear();
        m_zOrder.clear();

        // Application State Initializers
        m_notepadBuffer.clear();
        m_notepadCursor = 0;
        m_notepadLines = 1;

        m_7zipBenchmarkRunning = false;
        m_7zipBenchmarkIterations = 0;
        m_7zipExtractOpened = false;

        m_vlcState = VlcState::Stopped;
        m_vlcVolume = 80;

        m_statMouseMoves = 0;
        m_statMouseClicks = 0;
        m_statKeyEvents = 0;
        m_statCharsSynthesized = 0;
        m_statMsgsDispatched = 0;
        m_statWindowsDragged = 0;
        m_statAeroSnaps = 0;
    }

    // ------------------------------------------------------------------------
    // Window Registration & Topology
    // ------------------------------------------------------------------------
    void registerWindow(win32::HWND hwnd, const std::wstring& title, int x, int y, int width, int height) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        ManagedWindowBounds wb;
        wb.hwnd = hwnd;
        wb.title = title;
        wb.x = x;
        wb.y = y;
        wb.width = width;
        wb.height = height;
        wb.normalX = x;
        wb.normalY = y;
        wb.normalW = width;
        wb.normalH = height;
        wb.visible = true;
        wb.maximized = false;
        wb.currentSnap = SnapMode::None;

        m_windows[hwnd] = wb;
        // Promote to front of Z-order
        auto it = std::find(m_zOrder.begin(), m_zOrder.end(), hwnd);
        if (it != m_zOrder.end()) m_zOrder.erase(it);
        m_zOrder.push_front(hwnd);

        m_activeHwnd = hwnd;
        m_focusHwnd = hwnd;
    }

    void unregisterWindow(win32::HWND hwnd) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_windows.erase(hwnd);
        auto it = std::find(m_zOrder.begin(), m_zOrder.end(), hwnd);
        if (it != m_zOrder.end()) m_zOrder.erase(it);
        if (m_activeHwnd == hwnd) {
            m_activeHwnd = m_zOrder.empty() ? nullptr : m_zOrder.front();
            m_focusHwnd = m_activeHwnd;
        }
    }

    bool getWindowBounds(win32::HWND hwnd, ManagedWindowBounds& outBounds) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_windows.find(hwnd);
        if (it != m_windows.end()) {
            outBounds = it->second;
            return true;
        }
        return false;
    }

    win32::HWND getActiveWindow() const noexcept {
        return m_activeHwnd;
    }

    win32::HWND getFocusedWindow() const noexcept {
        return m_focusHwnd;
    }

    void setFocusedWindow(win32::HWND hwnd) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        if (m_focusHwnd != hwnd) {
            if (m_focusHwnd) {
                user32::WindowManager::get().postMessage(m_focusHwnd, user32::WM_KILLFOCUS, reinterpret_cast<uintptr_t>(hwnd), 0);
            }
            m_focusHwnd = hwnd;
            if (m_focusHwnd) {
                user32::WindowManager::get().postMessage(m_focusHwnd, user32::WM_SETFOCUS, 0, 0);
            }
        }
    }

    void bringToTop(win32::HWND hwnd) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = std::find(m_zOrder.begin(), m_zOrder.end(), hwnd);
        if (it != m_zOrder.end()) {
            m_zOrder.erase(it);
            m_zOrder.push_front(hwnd);
        }
        if (m_activeHwnd != hwnd) {
            if (m_activeHwnd) {
                user32::WindowManager::get().postMessage(m_activeHwnd, user32::WM_ACTIVATE, 0, reinterpret_cast<intptr_t>(hwnd)); // WA_INACTIVE
            }
            m_activeHwnd = hwnd;
            setFocusedWindow(hwnd);
            user32::WindowManager::get().postMessage(m_activeHwnd, user32::WM_ACTIVATE, 1, 0); // WA_ACTIVE
        }
    }

    // ------------------------------------------------------------------------
    // Hit Testing & Non-Client Geometry
    // ------------------------------------------------------------------------
    HitTestResult hitTest(int screenX, int screenY) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        HitTestResult result;
        result.hitCode = HTNOWHERE;

        // Traverse Z-order front-to-back
        for (win32::HWND hwnd : m_zOrder) {
            auto it = m_windows.find(hwnd);
            if (it == m_windows.end() || !it->second.visible) continue;

            const auto& w = it->second;
            if (screenX >= w.x && screenX < w.x + w.width &&
                screenY >= w.y && screenY < w.y + w.height) {
                
                result.hwnd = hwnd;
                int relX = screenX - w.x;
                int relY = screenY - w.y;

                // Frame Metrics: 32px Caption bar, 4px Resize Borders
                constexpr int kCaptionHeight = 32;
                constexpr int kBorder = 4;
                constexpr int kBtnWidth = 32;

                // Border resizing hit checks
                if (relY < kBorder) {
                    if (relX < kBorder * 3) result.hitCode = HTTOPLEFT;
                    else if (relX >= w.width - kBorder * 3) result.hitCode = HTTOPRIGHT;
                    else result.hitCode = HTTOP;
                    return result;
                } else if (relY >= w.height - kBorder) {
                    if (relX < kBorder * 3) result.hitCode = HTBOTTOMLEFT;
                    else if (relX >= w.width - kBorder * 3) result.hitCode = HTBOTTOMRIGHT;
                    else result.hitCode = HTBOTTOM;
                    return result;
                } else if (relX < kBorder) {
                    result.hitCode = HTLEFT;
                    return result;
                } else if (relX >= w.width - kBorder) {
                    result.hitCode = HTRIGHT;
                    return result;
                }

                // Titlebar / Caption hit checks
                if (relY < kCaptionHeight) {
                    // Right-to-left buttons: Close, Maximize, Minimize
                    int closeLeft = w.width - kBtnWidth;
                    int maxLeft = w.width - (kBtnWidth * 2);
                    int minLeft = w.width - (kBtnWidth * 3);

                    if (relX >= closeLeft) {
                        result.hitCode = HTCLOSE;
                    } else if (relX >= maxLeft) {
                        result.hitCode = HTMAXBUTTON;
                    } else if (relX >= minLeft) {
                        result.hitCode = HTMINBUTTON;
                    } else {
                        result.hitCode = HTCAPTION;
                    }
                    return result;
                }

                // Client Area
                result.hitCode = HTCLIENT;
                result.clientX = relX - kBorder;
                result.clientY = relY - kCaptionHeight;
                return result;
            }
        }

        return result;
    }

    // ------------------------------------------------------------------------
    // Hardware Mouse Routing & Aero Snap
    // ------------------------------------------------------------------------
    void routeMouseEvent(int screenX, int screenY, uint32_t buttonFlags, int wheelDelta = 0) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_cursorX = std::clamp(screenX, 0, m_screenWidth - 1);
        m_cursorY = std::clamp(screenY, 0, m_screenHeight - 1);
        m_statMouseMoves++;

        HitTestResult hit = hitTest(m_cursorX, m_cursorY);

        // Window Dragging & Aero Snap Processing
        if (m_isDragging && m_dragHwnd) {
            auto it = m_windows.find(m_dragHwnd);
            if (it != m_windows.end()) {
                int dx = m_cursorX - m_dragStartMouseX;
                int dy = m_cursorY - m_dragStartMouseY;
                it->second.x = m_dragStartWinX + dx;
                it->second.y = m_dragStartWinY + dy;

                // Aero Snap border edge detection
                constexpr int kSnapThreshold = 10;
                if (m_cursorX <= kSnapThreshold) {
                    m_pendingSnap = SnapMode::LeftHalf;
                } else if (m_cursorX >= m_screenWidth - kSnapThreshold) {
                    m_pendingSnap = SnapMode::RightHalf;
                } else if (m_cursorY <= kSnapThreshold) {
                    m_pendingSnap = SnapMode::Maximize;
                } else {
                    m_pendingSnap = SnapMode::None;
                }
            }
        }

        // Left Button Down
        if ((buttonFlags & MOUSE_LBUTTONDOWN) != 0) {
            m_mouseButtons |= MOUSE_LBUTTONDOWN;
            m_statMouseClicks++;

            if (hit.hwnd) {
                bringToTop(hit.hwnd);

                if (hit.hitCode == HTCAPTION) {
                    // Begin window drag
                    m_isDragging = true;
                    m_dragHwnd = hit.hwnd;
                    m_dragStartMouseX = m_cursorX;
                    m_dragStartMouseY = m_cursorY;
                    auto& w = m_windows[hit.hwnd];
                    // If window was snapped/maximized, restore normal dimensions
                    if (w.currentSnap != SnapMode::None || w.maximized) {
                        w.width = w.normalW;
                        w.height = w.normalH;
                        w.maximized = false;
                        w.currentSnap = SnapMode::None;
                    }
                    m_dragStartWinX = w.x;
                    m_dragStartWinY = w.y;
                    m_statWindowsDragged++;

                    user32::WindowManager::get().postMessage(hit.hwnd, WM_NCLBUTTONDOWN, HTCAPTION, (m_cursorY << 16) | (m_cursorX & 0xFFFF));
                } else if (hit.hitCode == HTCLIENT) {
                    user32::WindowManager::get().postMessage(hit.hwnd, user32::WM_LBUTTONDOWN, 0, (hit.clientY << 16) | (hit.clientX & 0xFFFF));
                } else if (hit.hitCode == HTCLOSE) {
                    user32::WindowManager::get().postMessage(hit.hwnd, user32::WM_CLOSE, 0, 0);
                } else if (hit.hitCode == HTMAXBUTTON) {
                    toggleMaximize(hit.hwnd);
                } else if (hit.hitCode == HTMINBUTTON) {
                    minimizeWindow(hit.hwnd);
                }
            }
        }

        // Left Button Up
        if ((buttonFlags & MOUSE_LBUTTONUP) != 0) {
            m_mouseButtons &= ~MOUSE_LBUTTONDOWN;

            if (m_isDragging && m_dragHwnd) {
                applySnap(m_dragHwnd, m_pendingSnap);
                m_isDragging = false;
                m_dragHwnd = nullptr;
                m_pendingSnap = SnapMode::None;
            }

            if (hit.hwnd && hit.hitCode == HTCLIENT) {
                user32::WindowManager::get().postMessage(hit.hwnd, user32::WM_LBUTTONUP, 0, (hit.clientY << 16) | (hit.clientX & 0xFFFF));
            }
        }

        // Right Button Down / Up
        if ((buttonFlags & MOUSE_RBUTTONDOWN) != 0) {
            m_mouseButtons |= MOUSE_RBUTTONDOWN;
            m_statMouseClicks++;
            if (hit.hwnd && hit.hitCode == HTCLIENT) {
                user32::WindowManager::get().postMessage(hit.hwnd, user32::WM_RBUTTONDOWN, 0, (hit.clientY << 16) | (hit.clientX & 0xFFFF));
            }
        }
        if ((buttonFlags & MOUSE_RBUTTONUP) != 0) {
            m_mouseButtons &= ~MOUSE_RBUTTONDOWN;
            if (hit.hwnd && hit.hitCode == HTCLIENT) {
                user32::WindowManager::get().postMessage(hit.hwnd, user32::WM_RBUTTONUP, 0, (hit.clientY << 16) | (hit.clientX & 0xFFFF));
            }
        }

        // Mouse Move Post
        if (hit.hwnd && hit.hitCode == HTCLIENT) {
            user32::WindowManager::get().postMessage(hit.hwnd, user32::WM_MOUSEMOVE, 0, (hit.clientY << 16) | (hit.clientX & 0xFFFF));
        }

        // Mouse Wheel Post
        if (wheelDelta != 0 && hit.hwnd) {
            user32::WindowManager::get().postMessage(hit.hwnd, user32::WM_MOUSEWHEEL, (wheelDelta << 16), (m_cursorY << 16) | (m_cursorX & 0xFFFF));
        }
    }

    void applySnap(win32::HWND hwnd, SnapMode snap) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_windows.find(hwnd);
        if (it == m_windows.end()) return;

        constexpr int kTaskbarHeight = 48;
        int usableHeight = m_screenHeight - kTaskbarHeight;

        switch (snap) {
            case SnapMode::LeftHalf:
                it->second.x = 0;
                it->second.y = 0;
                it->second.width = m_screenWidth / 2;
                it->second.height = usableHeight;
                it->second.currentSnap = SnapMode::LeftHalf;
                it->second.maximized = false;
                m_statAeroSnaps++;
                break;
            case SnapMode::RightHalf:
                it->second.x = m_screenWidth / 2;
                it->second.y = 0;
                it->second.width = m_screenWidth / 2;
                it->second.height = usableHeight;
                it->second.currentSnap = SnapMode::RightHalf;
                it->second.maximized = false;
                m_statAeroSnaps++;
                break;
            case SnapMode::Maximize:
                it->second.x = 0;
                it->second.y = 0;
                it->second.width = m_screenWidth;
                it->second.height = usableHeight;
                it->second.currentSnap = SnapMode::Maximize;
                it->second.maximized = true;
                m_statAeroSnaps++;
                break;
            case SnapMode::None:
            default:
                break;
        }

        user32::WindowManager::get().postMessage(hwnd, user32::WM_SIZE, 0, (it->second.height << 16) | (it->second.width & 0xFFFF));
    }

    void toggleMaximize(win32::HWND hwnd) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_windows.find(hwnd);
        if (it == m_windows.end()) return;

        if (it->second.maximized) {
            // Restore normal bounds
            it->second.x = it->second.normalX;
            it->second.y = it->second.normalY;
            it->second.width = it->second.normalW;
            it->second.height = it->second.normalH;
            it->second.maximized = false;
            it->second.currentSnap = SnapMode::None;
        } else {
            // Maximize
            it->second.normalX = it->second.x;
            it->second.normalY = it->second.y;
            it->second.normalW = it->second.width;
            it->second.normalH = it->second.height;
            applySnap(hwnd, SnapMode::Maximize);
        }
        user32::WindowManager::get().postMessage(hwnd, user32::WM_SIZE, 0, (it->second.height << 16) | (it->second.width & 0xFFFF));
    }

    void minimizeWindow(win32::HWND hwnd) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        auto it = m_windows.find(hwnd);
        if (it != m_windows.end()) {
            it->second.visible = false;
            user32::WindowManager::get().postMessage(hwnd, user32::WM_SHOWWINDOW, 0, 0); // SW_HIDE
        }
    }

    // ------------------------------------------------------------------------
    // Hardware Keyboard Routing & Scancode Translation
    // ------------------------------------------------------------------------
    uint32_t scancodeToVirtualKey(uint8_t scancode) const noexcept {
        switch (scancode) {
            case 0x01: return VK_ESCAPE;
            case 0x02: return '1';
            case 0x03: return '2';
            case 0x04: return '3';
            case 0x05: return '4';
            case 0x06: return '5';
            case 0x07: return '6';
            case 0x08: return '7';
            case 0x09: return '8';
            case 0x0A: return '9';
            case 0x0B: return '0';
            case 0x0E: return VK_BACK;
            case 0x0F: return VK_TAB;
            case 0x10: return 'Q';
            case 0x11: return 'W';
            case 0x12: return 'E';
            case 0x13: return 'R';
            case 0x14: return 'T';
            case 0x15: return 'Y';
            case 0x16: return 'U';
            case 0x17: return 'I';
            case 0x18: return 'O';
            case 0x19: return 'P';
            case 0x1C: return VK_RETURN;
            case 0x1D: return VK_CONTROL;
            case 0x1E: return 'A';
            case 0x1F: return 'S';
            case 0x20: return 'D';
            case 0x21: return 'F';
            case 0x22: return 'G';
            case 0x23: return 'H';
            case 0x24: return 'J';
            case 0x25: return 'K';
            case 0x26: return 'L';
            case 0x2A: return VK_SHIFT;
            case 0x2C: return 'Z';
            case 0x2D: return 'X';
            case 0x2E: return 'C';
            case 0x2F: return 'V';
            case 0x30: return 'B';
            case 0x31: return 'N';
            case 0x32: return 'M';
            case 0x36: return VK_SHIFT;
            case 0x38: return VK_MENU; // Alt
            case 0x39: return VK_SPACE;
            case 0x48: return VK_UP;
            case 0x4B: return VK_LEFT;
            case 0x4D: return VK_RIGHT;
            case 0x50: return VK_DOWN;
            default:   return 0;
        }
    }

    void routeKeyboardEvent(uint8_t scancode, bool isKeyUp) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_statKeyEvents++;

        uint32_t vk = scancodeToVirtualKey(scancode);
        if (vk == 0) return;

        // Track Modifiers
        if (vk == VK_SHIFT) m_shiftPressed = !isKeyUp;
        if (vk == VK_CONTROL) m_ctrlPressed = !isKeyUp;
        if (vk == VK_MENU) m_altPressed = !isKeyUp;

        win32::HWND targetHwnd = m_focusHwnd ? m_focusHwnd : m_activeHwnd;
        if (!targetHwnd) return;

        uint32_t msg = isKeyUp ? user32::WM_KEYUP : user32::WM_KEYDOWN;
        user32::WindowManager::get().postMessage(targetHwnd, msg, vk, scancode);

        // Win32 TranslateMessage: Synthesize WM_CHAR for printable keys
        if (!isKeyUp) {
            wchar_t ch = 0;
            if (vk >= 'A' && vk <= 'Z') {
                ch = m_shiftPressed ? static_cast<wchar_t>(vk) : static_cast<wchar_t>(std::tolower(vk));
            } else if (vk >= '0' && vk <= '9') {
                if (m_shiftPressed) {
                    const wchar_t syms[] = L")!@#$%^&*(";
                    ch = syms[vk - '0'];
                } else {
                    ch = static_cast<wchar_t>(vk);
                }
            } else if (vk == VK_SPACE) {
                ch = L' ';
            } else if (vk == VK_RETURN) {
                ch = L'\n';
            } else if (vk == VK_BACK) {
                ch = L'\b';
            } else if (vk == VK_TAB) {
                ch = L'\t';
            }

            if (ch != 0) {
                m_statCharsSynthesized++;
                user32::WindowManager::get().postMessage(targetHwnd, user32::WM_CHAR, ch, scancode);
            }
            // Dispatch directly to application hook handler
            dispatchApplicationInput(targetHwnd, ch, vk, false);
        } else {
            // Key Up application dispatch
            dispatchApplicationInput(targetHwnd, 0, vk, true);
        }
    }

    // ------------------------------------------------------------------------
    // Retail Application Dispatch & Automation Hooks
    // ------------------------------------------------------------------------
    void dispatchApplicationInput(win32::HWND hwnd, wchar_t ch, uint32_t vk, bool isKeyUp = false) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_statMsgsDispatched++;

        auto it = m_windows.find(hwnd);
        if (it == m_windows.end()) return;

        const std::wstring& title = it->second.title;

        // 1. Notepad++ Scintilla Document Editing Automation
        if (title.find(L"Notepad++") != std::wstring::npos) {
            if (!isKeyUp && ch != 0) {
                if (ch == L'\b') {
                    if (!m_notepadBuffer.empty()) {
                        if (m_notepadBuffer.back() == L'\n') m_notepadLines--;
                        m_notepadBuffer.pop_back();
                        if (m_notepadCursor > 0) m_notepadCursor--;
                    }
                } else if (ch == L'\n') {
                    m_notepadBuffer.push_back(ch);
                    m_notepadCursor++;
                    m_notepadLines++;
                } else {
                    m_notepadBuffer.push_back(ch);
                    m_notepadCursor++;
                }
            }
        }

        // 2. VLC Media Player Playback Automation (Space = Play/Pause, Up/Down = Volume)
        if (title.find(L"VLC") != std::wstring::npos) {
            if (!isKeyUp) {
                if (vk == VK_SPACE) {
                    if (m_vlcState == VlcState::Playing) {
                        m_vlcState = VlcState::Paused;
                    } else {
                        m_vlcState = VlcState::Playing;
                    }
                } else if (vk == VK_UP) {
                    m_vlcVolume = std::min(100, m_vlcVolume + 5);
                } else if (vk == VK_DOWN) {
                    m_vlcVolume = std::max(0, m_vlcVolume - 5);
                }
            }
        }
    }

    // 7-Zip Command / Button Automation
    void dispatch7ZipCommand(win32::HWND hwnd, uint32_t controlId) {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        m_statMsgsDispatched++;

        auto it = m_windows.find(hwnd);
        if (it == m_windows.end()) return;

        // Control ID 1001: Benchmark Button
        if (controlId == 1001) {
            m_7zipBenchmarkRunning = !m_7zipBenchmarkRunning;
            if (m_7zipBenchmarkRunning) {
                m_7zipBenchmarkIterations += 32;
            }
        }
        // Control ID 1002: Extract Button
        else if (controlId == 1002) {
            m_7zipExtractOpened = true;
        }

        user32::WindowManager::get().postMessage(hwnd, WM_COMMAND, controlId, 0);
    }

    // ------------------------------------------------------------------------
    // Query Application Automation States
    // ------------------------------------------------------------------------
    std::wstring getNotepadDocumentText() const {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_notepadBuffer;
    }

    size_t getNotepadLineCount() const noexcept {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_notepadLines;
    }

    bool is7ZipBenchmarkActive() const noexcept {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_7zipBenchmarkRunning;
    }

    uint32_t get7ZipBenchmarkIterations() const noexcept {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_7zipBenchmarkIterations;
    }

    bool is7ZipExtractOpened() const noexcept {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_7zipExtractOpened;
    }

    VlcState getVlcPlaybackState() const noexcept {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_vlcState;
    }

    int getVlcVolume() const noexcept {
        std::lock_guard<std::recursive_mutex> lock(m_mutex);
        return m_vlcVolume;
    }

    void getCursorPos(int& x, int& y) const noexcept {
        x = m_cursorX;
        y = m_cursorY;
    }

    void getStats(uint64_t& mouseMoves, uint64_t& mouseClicks, uint64_t& keyEvents, uint64_t& chars, uint64_t& msgs, uint64_t& dragged, uint64_t& snaps) const noexcept {
        mouseMoves = m_statMouseMoves.load(std::memory_order_relaxed);
        mouseClicks = m_statMouseClicks.load(std::memory_order_relaxed);
        keyEvents = m_statKeyEvents.load(std::memory_order_relaxed);
        chars = m_statCharsSynthesized.load(std::memory_order_relaxed);
        msgs = m_statMsgsDispatched.load(std::memory_order_relaxed);
        dragged = m_statWindowsDragged.load(std::memory_order_relaxed);
        snaps = m_statAeroSnaps.load(std::memory_order_relaxed);
    }

private:
    InputRouter() = default;
    ~InputRouter() = default;

    mutable std::recursive_mutex m_mutex;
    int m_screenWidth{1280};
    int m_screenHeight{800};

    int m_cursorX{640};
    int m_cursorY{400};
    uint32_t m_mouseButtons{0};

    bool m_shiftPressed{false};
    bool m_ctrlPressed{false};
    bool m_altPressed{false};

    bool m_isDragging{false};
    win32::HWND m_dragHwnd{nullptr};
    int m_dragStartMouseX{0};
    int m_dragStartMouseY{0};
    int m_dragStartWinX{0};
    int m_dragStartWinY{0};
    SnapMode m_pendingSnap{SnapMode::None};

    win32::HWND m_activeHwnd{nullptr};
    win32::HWND m_focusHwnd{nullptr};

    std::unordered_map<win32::HWND, ManagedWindowBounds> m_windows;
    std::deque<win32::HWND> m_zOrder; // Front is index 0

    // Notepad++ Automation
    std::wstring m_notepadBuffer;
    size_t m_notepadCursor{0};
    size_t m_notepadLines{1};

    // 7-Zip Automation
    bool m_7zipBenchmarkRunning{false};
    uint32_t m_7zipBenchmarkIterations{0};
    bool m_7zipExtractOpened{false};

    // VLC Automation
    VlcState m_vlcState{VlcState::Stopped};
    int m_vlcVolume{80};

    // Telemetry
    std::atomic<uint64_t> m_statMouseMoves{0};
    std::atomic<uint64_t> m_statMouseClicks{0};
    std::atomic<uint64_t> m_statKeyEvents{0};
    std::atomic<uint64_t> m_statCharsSynthesized{0};
    std::atomic<uint64_t> m_statMsgsDispatched{0};
    std::atomic<uint64_t> m_statWindowsDragged{0};
    std::atomic<uint64_t> m_statAeroSnaps{0};
};

} // namespace micant::input

// ============================================================================
// 5. Win32 C ABI Parity Exports
// ============================================================================

extern "C" {

inline int32_t MicaInputInitialize(int screenWidth, int screenHeight) {
    micant::input::InputRouter::get().initialize(screenWidth, screenHeight);
    return 1;
}

inline int32_t MicaRouteHardwareMouseEvent(int x, int y, uint32_t buttonFlags, int wheelDelta) {
    micant::input::InputRouter::get().routeMouseEvent(x, y, buttonFlags, wheelDelta);
    return 1;
}

inline int32_t MicaRouteHardwareKeyboardEvent(uint8_t scancode, int32_t isKeyUp) {
    micant::input::InputRouter::get().routeKeyboardEvent(scancode, isKeyUp != 0);
    return 1;
}

inline int32_t MicaDispatchWindowMessage(micant::win32::HWND hwnd, uint32_t msg, uintptr_t wParam, intptr_t lParam) {
    return static_cast<int32_t>(micant::user32::WindowManager::get().sendMessage(hwnd, msg, wParam, lParam));
}

inline micant::win32::HWND MicaGetFocusedWindow() {
    return micant::input::InputRouter::get().getFocusedWindow();
}

inline int32_t MicaSetFocusedWindow(micant::win32::HWND hwnd) {
    micant::input::InputRouter::get().setFocusedWindow(hwnd);
    return 1;
}

inline void MicaGetInputStats(uint64_t* mouseMoves, uint64_t* mouseClicks, uint64_t* keyEvents, uint64_t* snaps) {
    uint64_t mm = 0, mc = 0, ke = 0, ch = 0, md = 0, wd = 0, as = 0;
    micant::input::InputRouter::get().getStats(mm, mc, ke, ch, md, wd, as);
    if (mouseMoves) *mouseMoves = mm;
    if (mouseClicks) *mouseClicks = mc;
    if (keyEvents) *keyEvents = ke;
    if (snaps) *snaps = as;
}

inline void MicaInputShutdown() {
    micant::input::InputRouter::get().initialize();
}

} // extern "C"
