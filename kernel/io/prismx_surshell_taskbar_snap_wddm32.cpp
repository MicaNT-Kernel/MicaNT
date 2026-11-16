/**
 * @file prismx_surshell_taskbar_snap_wddm32.cpp
 * @brief Horizon 317 (Kernel Unit #93): Sovereign MicaNT SurShell Desktop Compositor,
 *        Taskbar Docking & Magnetic Window Snap Layouts Governor
 *        - Hardware-Accelerated SurShell Taskbar Docking & Active Process Taskbar Items
 *        - System Notification Tray with High-DPI Icon Dispatch & Badge Overlays
 *        - Magnetic Window Snap Layouts Governor (50/50, 70/30, 25x4 Quadrant Grid)
 *        - Multi-Monitor Virtual Desktop Switching with Animated Transition Matrices
 *        - Sub-Millisecond Alt-Tab Fast Switcher & Zero-Latency Window Snapping Architecture.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_surshell_taskbar_snap_wddm32.hpp"

namespace micant::um::prismx_surshell {

bool VerifyHorizon317SurShellTaskbarSnapSubsystem() noexcept {
    // ------------------------------------------------------------------------
    // Pillar 1: Usable Work Area Geometry & Taskbar Positioning
    // ------------------------------------------------------------------------
    MicaNtSurShellConfig cfg{};
    cfg.DisplayWidth = 3840;
    cfg.DisplayHeight = 2160;
    cfg.TaskbarHeight = 48;
    cfg.Position = MicaNtTaskbarPosition::Bottom;
    cfg.SnapThresholdPixels = 32;

    MicaNtSurShellEngine engine(cfg);

    auto wa = engine.GetUsableWorkArea();
    if (wa.left != 0 || wa.top != 0) return false;
    if (wa.right != 3840 || wa.bottom != 2112) return false;
    if (wa.Width() != 3840 || wa.Height() != 2112) return false;

    // ------------------------------------------------------------------------
    // Pillar 2: Magnetic Window Snap Layout Rect Calculations
    // ------------------------------------------------------------------------
    auto leftHalf = engine.CalculateSnapRect(MicaNtSnapZoneType::LeftHalf);
    if (leftHalf.left != 0 || leftHalf.right != 1920) return false;
    if (leftHalf.top != 0 || leftHalf.bottom != 2112) return false;

    auto rightHalf = engine.CalculateSnapRect(MicaNtSnapZoneType::RightHalf);
    if (rightHalf.left != 1920 || rightHalf.right != 3840) return false;

    auto tlQuad = engine.CalculateSnapRect(MicaNtSnapZoneType::TopLeftQuadrant);
    if (tlQuad.left != 0 || tlQuad.right != 1920 || tlQuad.top != 0 || tlQuad.bottom != 1056) return false;

    auto brQuad = engine.CalculateSnapRect(MicaNtSnapZoneType::BottomRightQuadrant);
    if (brQuad.left != 1920 || brQuad.right != 3840 || brQuad.top != 1056 || brQuad.bottom != 2112) return false;

    auto p70 = engine.CalculateSnapRect(MicaNtSnapZoneType::PrimaryLeft70);
    if (p70.Width() != 2688) return false;

    auto s30 = engine.CalculateSnapRect(MicaNtSnapZoneType::SecondaryRight30);
    if (s30.left != 2688 || s30.right != 3840) return false;

    auto maxRect = engine.CalculateSnapRect(MicaNtSnapZoneType::Maximize);
    if (maxRect.Width() != 3840 || maxRect.Height() != 2112) return false;

    // ------------------------------------------------------------------------
    // Pillar 3: Magnetic Edge & Corner Cursor Snap Targeting
    // ------------------------------------------------------------------------
    if (engine.EvaluateSnapTarget(10, 10) != MicaNtSnapZoneType::TopLeftQuadrant) return false;
    if (engine.EvaluateSnapTarget(3835, 10) != MicaNtSnapZoneType::TopRightQuadrant) return false;
    if (engine.EvaluateSnapTarget(10, 2105) != MicaNtSnapZoneType::BottomLeftQuadrant) return false;
    if (engine.EvaluateSnapTarget(3835, 2105) != MicaNtSnapZoneType::BottomRightQuadrant) return false;
    if (engine.EvaluateSnapTarget(1920, 10) != MicaNtSnapZoneType::Maximize) return false;
    if (engine.EvaluateSnapTarget(10, 1000) != MicaNtSnapZoneType::LeftHalf) return false;
    if (engine.EvaluateSnapTarget(3835, 1000) != MicaNtSnapZoneType::RightHalf) return false;
    if (engine.EvaluateSnapTarget(1000, 1000) != MicaNtSnapZoneType::None) return false;

    // ------------------------------------------------------------------------
    // Pillar 4: Taskbar Docking & System Notification Tray
    // ------------------------------------------------------------------------
    void* hwnd1 = reinterpret_cast<void*>(0x20001);
    void* hwnd2 = reinterpret_cast<void*>(0x20002);

    MicaNtTaskbarItemDesc tItem1{ 101, hwnd1, "Terminal", true, false, { 100, 2112, 148, 2160 } };
    MicaNtTaskbarItemDesc tItem2{ 102, hwnd2, "Game Client", false, false, { 152, 2112, 200, 2160 } };
    engine.AddTaskbarItem(tItem1);
    engine.AddTaskbarItem(tItem2);
    if (engine.GetTaskbarItems().size() != 2) return false;

    engine.SetTaskbarActive(hwnd2);
    if (engine.GetTaskbarItems()[0].IsActive) return false;
    if (!engine.GetTaskbarItems()[1].IsActive) return false;

    MicaNtSystemTrayItemDesc tray1{ 1, "Audio 7.1.4 Output", false, { 3700, 2112, 3724, 2160 } };
    MicaNtSystemTrayItemDesc tray2{ 2, "2.5GbE Network", true, { 3730, 2112, 3754, 2160 } };
    engine.AddSystemTrayItem(tray1);
    engine.AddSystemTrayItem(tray2);
    if (engine.GetTrayItems().size() != 2) return false;

    if (!engine.RemoveTaskbarItem(hwnd1)) return false;
    if (engine.GetTaskbarItems().size() != 1) return false;

    // ------------------------------------------------------------------------
    // Pillar 5: Multi-Monitor Virtual Desktops & Telemetry
    // ------------------------------------------------------------------------
    if (engine.GetVirtualDesktops().size() != 1) return false;
    if (!engine.GetVirtualDesktops()[0].IsActive) return false;

    uint32_t d2 = engine.CreateVirtualDesktop("Gaming & Low Latency");
    if (d2 != 2) return false;
    if (engine.GetVirtualDesktops().size() != 2) return false;

    bool switched = engine.SwitchVirtualDesktop(d2);
    if (!switched) return false;
    if (engine.GetVirtualDesktops()[0].IsActive) return false;
    if (!engine.GetVirtualDesktops()[1].IsActive) return false;

    const auto& tel = engine.GetTelemetry();
    if (tel.TotalSnapEvents < 7u) return false;
    if (tel.VirtualDesktopSwitches != 1u) return false;
    if (tel.TaskbarRenderPasses < 4u) return false;
    if (tel.SystemTrayEvents != 2u) return false;
    if (tel.AverageTaskSwitchLatencyMicroseconds >= 350.0f) return false;
    if (!tel.ZeroLatencyAltTabVerified) return false;
    if (!tel.MagneticSnapFeedbackVerified) return false;

    return true;
}

} // namespace micant::um::prismx_surshell
