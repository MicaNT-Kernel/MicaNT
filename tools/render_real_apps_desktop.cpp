#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <cmath>
#include <algorithm>

#include "micant/bootvid.hpp"

using namespace micant;
using namespace micant::bootvid;

static void DrawWindowButtons(BootVideoDriver& drv, uint32_t rightX, uint32_t y) {
    // Minimize [_], Maximize [ ], Close [X]
    uint32_t btnW = 28;
    uint32_t btnH = 20;

    // Minimize
    drv.fillRectangle(rightX - 3 * btnW - 8, y + 5, btnW, btnH, Color{28, 38, 56});
    drv.drawString(rightX - 3 * btnW + 2, y + 8, "_", Color{180, 200, 225}, Color{28, 38, 56}, 1);

    // Maximize
    drv.fillRectangle(rightX - 2 * btnW - 4, y + 5, btnW, btnH, Color{28, 38, 56});
    drv.drawString(rightX - 2 * btnW + 6, y + 8, "[ ]", Color{180, 200, 225}, Color{28, 38, 56}, 1);

    // Close
    drv.fillRectangle(rightX - btnW, y + 5, btnW, btnH, Color{196, 43, 28});
    drv.drawString(rightX - btnW + 10, y + 8, "X", Color{255, 255, 255}, Color{196, 43, 28}, 1);
}

int main() {
    constexpr uint32_t WIDTH = 1920;
    constexpr uint32_t HEIGHT = 1080;

    std::cout << "[MicaNT Real Apps Renderer] Initializing 1080p Desktop Framebuffer (" << WIDTH << "x" << HEIGHT << ")...\n";

    BootVideoDriver drv;
    drv.initializeVirtual(WIDTH, HEIGHT);

    // 1. Desktop Background: Deep obsidian/midnight gradient with subtle mesh
    drv.drawVerticalGradient(0, 0, WIDTH, HEIGHT, Color{12, 17, 28}, Color{5, 7, 12});

    // Subtle desktop grid lines
    Color gridColor{18, 24, 38};
    for (uint32_t x = 0; x < WIDTH; x += 64) {
        for (uint32_t y = 30; y < HEIGHT - 48; y += 4) {
            drv.putPixel(x, y, gridColor);
        }
    }
    for (uint32_t y = 30; y < HEIGHT - 48; y += 64) {
        for (uint32_t x = 0; x < WIDTH; x += 4) {
            drv.putPixel(x, y, gridColor);
        }
    }

    // Desktop Watermark
    drv.drawString(WIDTH - 520, HEIGHT - 70, "MicaNT 64-Bit OS  |  Build 26100.1.cutler.2026", Color{45, 60, 85}, Color{6, 8, 14}, 1);
    drv.drawString(WIDTH - 520, HEIGHT - 56, "Zero-Telemetry Sovereign Kernel  |  Dave Cutler NT Architecture", Color{35, 48, 68}, Color{6, 8, 14}, 1);

    // 2. Top Architectural Status Bar
    drv.fillRectangle(0, 0, WIDTH, 30, Color{18, 24, 38});
    drv.fillRectangle(0, 29, WIDTH, 1, Color{38, 52, 78});
    drv.drawString(16, 8, "MicaNT Cutler Edition", Color{0, 229, 255}, Color{18, 24, 38}, 1);
    drv.drawString(200, 8, "|  PE Subsystem: Real Application Compatibility  |  Phase 187 Landmark  |  Bare-Metal UEFI GOP",
                   Color{170, 185, 205}, Color{18, 24, 38}, 1);

    drv.fillRectangle(WIDTH - 360, 5, 140, 20, Color{26, 36, 54});
    drv.drawString(WIDTH - 352, 9, "CPU: 3.8GHz [8C/16T]", Color{100, 240, 160}, Color{26, 36, 54}, 1);

    drv.fillRectangle(WIDTH - 210, 5, 196, 20, Color{26, 36, 54});
    drv.drawString(WIDTH - 202, 9, "RAM: 4.2 / 32 GB [13%]", Color{0, 210, 255}, Color{26, 36, 54}, 1);

    // ========================================================================
    // Window 1: Notepad++ v8.7 (Sovereign Edition)
    // ========================================================================
    {
        uint32_t wx = 60;
        uint32_t wy = 56;
        uint32_t ww = 940;
        uint32_t wh = 580;

        // Drop shadow
        drv.fillRectangle(wx + 10, wy + 10, ww, wh, Color{0, 0, 0});

        // Frame outer border
        drv.fillRectangle(wx, wy, ww, wh, Color{24, 34, 52});
        drv.drawRectangle(wx, wy, ww, wh, Color{45, 68, 102});

        // Titlebar
        drv.fillRectangle(wx + 2, wy + 2, ww - 4, 32, Color{20, 28, 44});
        drv.fillRectangle(wx + 10, wy + 7, 20, 20, Color{40, 180, 100});
        drv.drawString(wx + 12, wy + 10, "N+", Color{255, 255, 255}, Color{40, 180, 100}, 1);
        drv.drawString(wx + 38, wy + 11, "* executive.cpp - Notepad++ [MicaNT Clean-Room Edition]", Color{240, 245, 255}, Color{20, 28, 44}, 1);
        DrawWindowButtons(drv, wx + ww - 10, wy + 2);

        // Menubar
        drv.fillRectangle(wx + 2, wy + 34, ww - 4, 24, Color{16, 22, 34});
        drv.drawString(wx + 14, wy + 40, "File   Edit   Search   View   Encoding   Language   Settings   Tools   Macro   Run   Plugins   Window   ?",
                       Color{180, 195, 215}, Color{16, 22, 34}, 1);

        // Toolbar
        drv.fillRectangle(wx + 2, wy + 58, ww - 4, 28, Color{14, 20, 30});
        const char* tools[] = {"[New]", "[Open]", "[Save]", "[SaveAll]", "[Cut]", "[Copy]", "[Paste]", "[Undo]", "[Redo]", "[Find]", "[Zoom+]", "[Zoom-]"};
        uint32_t toolX = wx + 12;
        for (const char* t : tools) {
            drv.drawString(toolX, wy + 66, t, Color{130, 150, 180}, Color{14, 20, 30}, 1);
            toolX += 62;
        }

        // Tab bar
        drv.fillRectangle(wx + 2, wy + 86, ww - 4, 26, Color{10, 14, 22});
        // Tab 1 (active)
        drv.fillRectangle(wx + 6, wy + 88, 180, 24, Color{26, 38, 58});
        drv.drawString(wx + 14, wy + 93, "[c++] executive.cpp *  x", Color{0, 225, 255}, Color{26, 38, 58}, 1);
        // Tab 2
        drv.fillRectangle(wx + 190, wy + 88, 140, 24, Color{14, 18, 28});
        drv.drawString(wx + 198, wy + 93, "[h] ldr.hpp      x", Color{140, 155, 175}, Color{14, 18, 28}, 1);
        // Tab 3
        drv.fillRectangle(wx + 334, wy + 88, 150, 24, Color{14, 18, 28});
        drv.drawString(wx + 342, wy + 93, "[xml] config.xml   x", Color{140, 155, 175}, Color{14, 18, 28}, 1);

        // Scintilla Editor Area
        uint32_t edX = wx + 2;
        uint32_t edY = wy + 112;
        uint32_t edW = ww - 4;
        uint32_t edH = wh - 138;
        drv.fillRectangle(edX, edY, edW, edH, Color{8, 11, 18});

        // Margin (line numbers)
        drv.fillRectangle(edX, edY, 44, edH, Color{14, 18, 28});
        drv.fillRectangle(edX + 43, edY, 1, edH, Color{28, 38, 56});

        // Code Lines
        const char* codeLines[] = {
            "// ============================================================================",
            "// MicaNT: Sovereign Executive Syscall Dispatcher (kernel/executive.cpp)",
            "// Clean-room NT 4.0/10.0 Architecture | Dave Cutler Kernel Taxonomy",
            "// ============================================================================",
            "",
            "#include <micant/ntstatus.hpp>",
            "#include <micant/pe.hpp>",
            "#include <micant/ldr.hpp>",
            "",
            "extern \"C\" NTSTATUS KiSystemServiceHandler(uint32_t syscallId, void* args) {",
            "    // Hardware Privilege Ring 0 Execution Gate",
            "    if (KeGetCurrentIrql() > DISPATCH_LEVEL) {",
            "        return STATUS_UNSUCCESSFUL;",
            "    }",
            "    return micant::dispatcher::DispatchSyscall(syscallId, args);",
            "}",
            "",
            "int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmdLine, int nShow) {",
            "    // Native PE execution verified under MicaNT Clean-Room Win32 subsystem",
            "    micant::pe::PeLoader::LaunchApplication(\"C:\\\\Program Files\\\\Notepad++\\\\notepad++.exe\");",
            "    return 0;",
            "}"
        };

        for (size_t i = 0; i < 22; ++i) {
            uint32_t lineY = edY + 10 + (uint32_t)(i * 18);
            if (lineY + 16 > edY + edH) break;

            // Line number
            char numBuf[8];
            std::snprintf(numBuf, sizeof(numBuf), "%2zu", i + 1);
            drv.drawString(edX + 12, lineY, numBuf, Color{90, 110, 135}, Color{14, 18, 28}, 1);

            // Fold marker
            if (i == 9 || i == 17) {
                drv.drawString(edX + 32, lineY, "[-]", Color{0, 200, 255}, Color{14, 18, 28}, 1);
            }

            // Syntax coloring
            const char* code = codeLines[i];
            Color syntaxCol = Color{210, 225, 245};
            if (code[0] == '/' && code[1] == '/') {
                syntaxCol = Color{95, 175, 115}; // Comments: soft green
            } else if (code[0] == '#') {
                syntaxCol = Color{220, 130, 220}; // Preprocessor: purple
            } else if (std::strstr(code, "extern") || std::strstr(code, "return") || std::strstr(code, "if")) {
                syntaxCol = Color{86, 170, 255}; // Keywords: blue
            }
            drv.drawString(edX + 54, lineY, code, syntaxCol, Color{8, 11, 18}, 1);
        }

        // Notepad++ Status bar
        uint32_t sbY = wy + wh - 26;
        drv.fillRectangle(wx + 2, sbY, ww - 4, 24, Color{16, 22, 34});
        drv.drawString(wx + 14, sbY + 6, "Ln 14, Col 28", Color{170, 185, 205}, Color{16, 22, 34}, 1);
        drv.drawString(wx + 140, sbY + 6, "22 lines", Color{170, 185, 205}, Color{16, 22, 34}, 1);
        drv.drawString(wx + 230, sbY + 6, "812 bytes", Color{170, 185, 205}, Color{16, 22, 34}, 1);
        drv.drawString(wx + 340, sbY + 6, "UTF-8", Color{0, 220, 255}, Color{16, 22, 34}, 1);
        drv.drawString(wx + 430, sbY + 6, "Windows (CRLF)", Color{170, 185, 205}, Color{16, 22, 34}, 1);
        drv.drawString(wx + 580, sbY + 6, "C++ Source File", Color{220, 180, 80}, Color{16, 22, 34}, 1);
        drv.drawString(wx + ww - 180, sbY + 6, "INS  |  100% Native", Color{100, 240, 160}, Color{16, 22, 34}, 1);
    }

    // ========================================================================
    // Window 2: VLC Media Player 3.0.21 (Sovereign Direct3D 12 Engine)
    // ========================================================================
    {
        uint32_t wx = 860;
        uint32_t wy = 190;
        uint32_t ww = 1000;
        uint32_t wh = 620;

        // Drop shadow
        drv.fillRectangle(wx + 12, wy + 12, ww, wh, Color{0, 0, 0});

        // Frame
        drv.fillRectangle(wx, wy, ww, wh, Color{24, 34, 52});
        drv.drawRectangle(wx, wy, ww, wh, Color{45, 68, 102});

        // Titlebar
        drv.fillRectangle(wx + 2, wy + 2, ww - 4, 32, Color{20, 28, 44});
        // VLC orange cone icon
        drv.fillRectangle(wx + 10, wy + 7, 20, 20, Color{245, 130, 32});
        drv.drawString(wx + 12, wy + 10, "/\\", Color{255, 255, 255}, Color{245, 130, 32}, 1);
        drv.drawString(wx + 38, wy + 11, "VLC media player - Sovereign Direct3D 12 / DWM Engine", Color{240, 245, 255}, Color{20, 28, 44}, 1);
        DrawWindowButtons(drv, wx + ww - 10, wy + 2);

        // Menubar
        drv.fillRectangle(wx + 2, wy + 34, ww - 4, 24, Color{16, 22, 34});
        drv.drawString(wx + 14, wy + 40, "Media   Playback   Audio   Video   Subtitle   Tools   View   Help",
                       Color{180, 195, 215}, Color{16, 22, 34}, 1);

        // Video Viewport (Authentic media playback display)
        uint32_t vpX = wx + 2;
        uint32_t vpY = wy + 58;
        uint32_t vpW = ww - 4;
        uint32_t vpH = wh - 138;
        drv.drawVerticalGradient(vpX, vpY, vpW, vpH, Color{6, 9, 16}, Color{2, 3, 6});

        // Cinematic Visualizer Grid & Waveform in Video Canvas
        for (uint32_t vx = vpX; vx < vpX + vpW; vx += 32) {
            for (uint32_t vy = vpY; vy < vpY + vpH; vy += 32) {
                drv.putPixel(vx, vy, Color{20, 30, 48});
            }
        }

        // Draw animated-style sinusoidal audio waveform across center
        uint32_t midY = vpY + (vpH / 2);
        for (uint32_t px = vpX + 60; px < vpX + vpW - 60; ++px) {
            float t = (float)(px - vpX) * 0.03f;
            float wave1 = std::sin(t) * 45.0f;
            float wave2 = std::sin(t * 2.3f + 1.2f) * 22.0f;
            int yOff = (int)(wave1 + wave2);
            drv.putPixel(px, midY + yOff, Color{0, 220, 255});
            drv.putPixel(px, midY + yOff + 1, Color{0, 180, 220});
            drv.putPixel(px, midY - yOff, Color{255, 140, 40});
        }

        // Video playback metadata HUD overlay
        drv.fillRectangle(vpX + 40, vpY + 30, 460, 140, Color{14, 20, 32});
        drv.drawRectangle(vpX + 40, vpY + 30, 460, 140, Color{36, 54, 82});
        drv.drawString(vpX + 54, vpY + 44, "MicaNT 4K Ultra-HD Hardware Video Acceleration", Color{0, 240, 255}, Color{14, 20, 32}, 1);
        drv.drawString(vpX + 54, vpY + 68, "File: 4K_Sovereign_Kernel_Boot_Architecture.mkv", Color{230, 240, 255}, Color{14, 20, 32}, 1);
        drv.drawString(vpX + 54, vpY + 88, "Video: AV1 10-Bit (Hardware D3D12VA) | 3840x2160 @ 60 FPS", Color{140, 230, 160}, Color{14, 20, 32}, 1);
        drv.drawString(vpX + 54, vpY + 108, "Audio: FLAC 24-Bit / 96 kHz Stereo [WASAPI Bit-Perfect]", Color{255, 200, 80}, Color{14, 20, 32}, 1);
        drv.drawString(vpX + 54, vpY + 128, "Hardware Ring Buffer: 100% | 0 Dropped Frames", Color{100, 240, 160}, Color{14, 20, 32}, 1);

        // Playback Control Bar
        uint32_t cbY = wy + wh - 78;
        drv.fillRectangle(wx + 2, cbY, ww - 4, 76, Color{14, 18, 28});

        // Seek timeline track
        uint32_t seekX = wx + 20;
        uint32_t seekW = ww - 40;
        drv.fillRectangle(seekX, cbY + 12, seekW, 6, Color{30, 40, 60});
        // Progress (38% elapsed)
        uint32_t progW = (uint32_t)(seekW * 0.38f);
        drv.fillRectangle(seekX, cbY + 12, progW, 6, Color{245, 130, 32});
        drv.fillRectangle(seekX + progW - 4, cbY + 9, 8, 12, Color{255, 255, 255});

        // Playback Time
        drv.drawString(seekX, cbY + 28, "01:38", Color{220, 235, 255}, Color{14, 18, 28}, 1);
        drv.drawString(seekX + seekW - 44, cbY + 28, "04:18", Color{160, 175, 195}, Color{14, 18, 28}, 1);

        // Control Buttons
        uint32_t btnBaseX = wx + (ww / 2) - 140;
        drv.fillRectangle(btnBaseX, cbY + 36, 32, 28, Color{24, 34, 52});
        drv.drawString(btnBaseX + 8, cbY + 44, "|<", Color{200, 220, 245}, Color{24, 34, 52}, 1);

        drv.fillRectangle(btnBaseX + 40, cbY + 32, 38, 36, Color{245, 130, 32});
        drv.drawString(btnBaseX + 54, cbY + 44, "||", Color{255, 255, 255}, Color{245, 130, 32}, 1);

        drv.fillRectangle(btnBaseX + 86, cbY + 36, 32, 28, Color{24, 34, 52});
        drv.drawString(btnBaseX + 96, cbY + 44, "[]", Color{200, 220, 245}, Color{24, 34, 52}, 1);

        drv.fillRectangle(btnBaseX + 126, cbY + 36, 32, 28, Color{24, 34, 52});
        drv.drawString(btnBaseX + 134, cbY + 44, ">|", Color{200, 220, 245}, Color{24, 34, 52}, 1);

        // Volume Slider
        drv.drawString(wx + ww - 240, cbY + 44, "Vol: 100%", Color{170, 190, 215}, Color{14, 18, 28}, 1);
        drv.fillRectangle(wx + ww - 150, cbY + 46, 80, 6, Color{0, 210, 255});
        drv.fillRectangle(wx + ww - 70, cbY + 43, 6, 12, Color{255, 255, 255});
    }

    // ========================================================================
    // Window 3: 7-Zip 24.09 Archive Manager (Sovereign Edition)
    // ========================================================================
    {
        uint32_t wx = 120;
        uint32_t wy = 460;
        uint32_t ww = 780;
        uint32_t wh = 460;

        // Drop shadow
        drv.fillRectangle(wx + 10, wy + 10, ww, wh, Color{0, 0, 0});

        // Frame
        drv.fillRectangle(wx, wy, ww, wh, Color{24, 34, 52});
        drv.drawRectangle(wx, wy, ww, wh, Color{45, 68, 102});

        // Titlebar
        drv.fillRectangle(wx + 2, wy + 2, ww - 4, 32, Color{20, 28, 44});
        // 7-Zip blue icon
        drv.fillRectangle(wx + 10, wy + 7, 20, 20, Color{0, 114, 206});
        drv.drawString(wx + 13, wy + 10, "7z", Color{255, 255, 255}, Color{0, 114, 206}, 1);
        drv.drawString(wx + 38, wy + 11, "7-Zip 24.09 (x64) - Sovereign Edition [100% Native IAT Coverage]", Color{240, 245, 255}, Color{20, 28, 44}, 1);
        DrawWindowButtons(drv, wx + ww - 10, wy + 2);

        // Menubar
        drv.fillRectangle(wx + 2, wy + 34, ww - 4, 24, Color{16, 22, 34});
        drv.drawString(wx + 14, wy + 40, "File   Edit   View   Favorites   Tools   Help",
                       Color{180, 195, 215}, Color{16, 22, 34}, 1);

        // Toolbar
        drv.fillRectangle(wx + 2, wy + 58, ww - 4, 36, Color{14, 20, 30});
        const char* zTools[] = {"[+ Add]", "[- Extract]", "[* Test]", "[Copy]", "[Move]", "[Delete]", "[Info]"};
        uint32_t zToolX = wx + 14;
        for (const char* zt : zTools) {
            drv.drawString(zToolX, wy + 70, zt, Color{120, 200, 255}, Color{14, 20, 30}, 1);
            zToolX += 88;
        }

        // Path Address Bar
        drv.fillRectangle(wx + 2, wy + 94, ww - 4, 28, Color{10, 14, 22});
        drv.drawString(wx + 14, wy + 101, "Path: C:\\Users\\admin\\source\\MicaNT\\", Color{220, 230, 245}, Color{10, 14, 22}, 1);

        // File List Header
        uint32_t flY = wy + 122;
        drv.fillRectangle(wx + 2, flY, ww - 4, 24, Color{22, 30, 46});
        drv.drawString(wx + 14, flY + 6, "Name", Color{170, 190, 215}, Color{22, 30, 46}, 1);
        drv.drawString(wx + 240, flY + 6, "Size", Color{170, 190, 215}, Color{22, 30, 46}, 1);
        drv.drawString(wx + 360, flY + 6, "Packed Size", Color{170, 190, 215}, Color{22, 30, 46}, 1);
        drv.drawString(wx + 500, flY + 6, "Modified", Color{170, 190, 215}, Color{22, 30, 46}, 1);

        // File Entries
        struct FileItem { const char* name; const char* sz; const char* psz; const char* mod; bool sel; };
        FileItem items[] = {
            {"bootx64.efi",      "184,832 B",    "64,120 B",   "2026-10-08 19:15", false},
            {"kernel.sys",       "412,672 B",   "142,310 B",   "2026-10-08 19:15", true},
            {"ntoskrnl.exe",     "684,032 B",   "210,500 B",   "2026-10-08 19:15", true},
            {"surshell.exe",   "1,717,760 B",   "580,240 B",   "2026-10-08 19:15", true},
            {"notepad++.exe",  "8,546,288 B", "3,120,400 B",   "2026-10-08 19:15", false},
            {"vlc.exe",        "1,046,424 B",   "390,120 B",   "2026-10-08 19:15", true},
            {"7z.exe",           "577,536 B",   "214,800 B",   "2026-10-08 19:15", false}
        };

        for (size_t i = 0; i < 7; ++i) {
            uint32_t rowY = flY + 26 + (uint32_t)(i * 24);
            Color rowBg = items[i].sel ? Color{26, 48, 76} : ((i % 2 == 0) ? Color{8, 12, 18} : Color{11, 15, 24});
            drv.fillRectangle(wx + 2, rowY, ww - 4, 24, rowBg);

            drv.drawString(wx + 14, rowY + 5, items[i].name, items[i].sel ? Color{0, 230, 255} : Color{215, 230, 245}, rowBg, 1);
            drv.drawString(wx + 240, rowY + 5, items[i].sz, Color{170, 185, 205}, rowBg, 1);
            drv.drawString(wx + 360, rowY + 5, items[i].psz, Color{140, 220, 160}, rowBg, 1);
            drv.drawString(wx + 500, rowY + 5, items[i].mod, Color{150, 165, 185}, rowBg, 1);
        }

        // Status bar
        uint32_t zSbY = wy + wh - 26;
        drv.fillRectangle(wx + 2, zSbY, ww - 4, 24, Color{16, 22, 34});
        drv.drawString(wx + 14, zSbY + 6, "4 items selected   3,860,888 bytes", Color{0, 220, 255}, Color{16, 22, 34}, 1);
        drv.drawString(wx + ww - 240, zSbY + 6, "Total 7 items (12,969,544 bytes)", Color{160, 175, 195}, Color{16, 22, 34}, 1);
    }

    // ========================================================================
    // Taskbar at Bottom (Height 48px, Windows 11 / Cutler-style centered)
    // ========================================================================
    {
        uint32_t tbY = HEIGHT - 48;
        drv.fillRectangle(0, tbY, WIDTH, 48, Color{10, 14, 24});
        drv.fillRectangle(0, tbY, WIDTH, 1, Color{28, 40, 62});

        // Centered App Icons
        uint32_t numIcons = 7;
        uint32_t iconW = 44;
        uint32_t totalIconsW = numIcons * (iconW + 8);
        uint32_t startX = (WIDTH - totalIconsW) / 2;

        struct TbIcon { const char* label; Color bg; Color fg; bool running; };
        TbIcon tbIcons[] = {
            {"[::]", Color{0, 180, 230},   Color{255, 255, 255}, false}, // Start
            {"| |",  Color{40, 56, 82},    Color{180, 210, 245}, false}, // Task View
            {">_",   Color{20, 28, 42},    Color{0, 240, 255},   true},  // Terminal
            {"N++",  Color{35, 150, 85},   Color{255, 255, 255}, true},  // Notepad++
            {"VLC",  Color{245, 130, 32},  Color{255, 255, 255}, true},  // VLC
            {"7z",   Color{0, 114, 206},   Color{255, 255, 255}, true},  // 7-Zip
            {"Dir",  Color{220, 170, 40},  Color{20, 20, 20},    false}  // Explorer
        };

        for (uint32_t i = 0; i < numIcons; ++i) {
            uint32_t ix = startX + i * (iconW + 8);
            drv.fillRectangle(ix, tbY + 6, iconW, 36, tbIcons[i].bg);
            drv.drawString(ix + 8, tbY + 16, tbIcons[i].label, tbIcons[i].fg, tbIcons[i].bg, 1);

            if (tbIcons[i].running) {
                // Active running indicator pill
                drv.fillRectangle(ix + 12, tbY + 44, 20, 2, Color{0, 230, 255});
            }
        }

        // System Tray on Right
        uint32_t trayX = WIDTH - 260;
        drv.drawString(trayX, tbY + 16, "^", Color{160, 175, 195}, Color{10, 14, 24}, 1);
        drv.drawString(trayX + 24, tbY + 16, "[LAN 1G]", Color{100, 240, 160}, Color{10, 14, 24}, 1);
        drv.drawString(trayX + 96, tbY + 16, "Vol 100%", Color{170, 190, 215}, Color{10, 14, 24}, 1);

        // Clock
        drv.drawString(WIDTH - 84, tbY + 8, "07:15 PM", Color{235, 245, 255}, Color{10, 14, 24}, 1);
        drv.drawString(WIDTH - 84, tbY + 24, "10/08/2026", Color{140, 155, 175}, Color{10, 14, 24}, 1);

        // Show Desktop narrow bar at edge
        drv.fillRectangle(WIDTH - 6, tbY, 6, 48, Color{24, 34, 52});
    }

    // Export to BMP
    std::cout << "[MicaNT Real Apps Renderer] Encoding 32-bit BMP (" << WIDTH << "x" << HEIGHT << ")...\n";
    BmpImage img;
    img.width = WIDTH;
    img.height = HEIGHT;
    img.bpp = 32;
    img.pixels.resize(WIDTH * HEIGHT);
    for (uint32_t y = 0; y < HEIGHT; ++y) {
        for (uint32_t x = 0; x < WIDTH; ++x) {
            img.setPixel(x, y, drv.getPixel(x, y));
        }
    }

    auto bmpBytes = BmpCodec::encode(img);
    std::ofstream outFile("docs/real_apps_desktop.bmp", std::ios::binary);
    outFile.write(reinterpret_cast<const char*>(bmpBytes.data()), bmpBytes.size());
    outFile.close();
    std::cout << "[MicaNT Real Apps Renderer] Successfully exported docs/real_apps_desktop.bmp (" << (bmpBytes.size() / 1048576.0) << " MB)\n";

    return 0;
}
