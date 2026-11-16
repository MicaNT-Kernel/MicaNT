/**
 * @file prismx_win32_msgloop_latency_wddm32.cpp
 * @brief Horizon 313 (Kernel Unit #89): Sovereign MicaNT Native Win32 Subsystem,
 *        PE32+ IAT Dynamic Import Router & SurShell 540Hz Compositor Event Routing
 *        - PE32+ Import Address Table (IAT) Dynamic Import Router & Symbol Resolver
 *        - Native Win32 Thread Message Queue (GetMessage, PeekMessage, DispatchMessage)
 *        - Sub-Millisecond 8000Hz Raw Input Queuing & Microsecond Motion Coalescing
 *        - SurShell 540Hz Compositor Event Routing & Waitable SwapChain Synchronization (1.85ms)
 *        - Seamless Zero-Latency Flip Presentation Handover into WDDM 3.2 DirectFlip Pipeline.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_win32_msgloop_latency_wddm32.hpp"

namespace micant::um::prismx_win32 {

bool VerifyHorizon313Win32MsgLoopLatencySubsystem() noexcept {
    // ------------------------------------------------------------------------
    // Pillar 1: PE32+ IAT Dynamic Import Routing & Symbol Snapping
    // ------------------------------------------------------------------------
    PeIatDynamicRouter router;

    // Register official System32 DLL exports into kernel/user symbol directory
    router.RegisterExport("user32.dll", "PeekMessageW",      0x00007FF8B0011000ULL);
    router.RegisterExport("user32.dll", "GetMessageW",       0x00007FF8B0011080ULL);
    router.RegisterExport("user32.dll", "DispatchMessageW",  0x00007FF8B0011100ULL);
    router.RegisterExport("user32.dll", "PostMessageW",      0x00007FF8B0011180ULL);
    router.RegisterExport("user32.dll", "TranslateMessage",  0x00007FF8B0011200ULL);
    router.RegisterExport("gdi32.dll",  "SwapBuffers",       0x00007FF8B0022000ULL);
    router.RegisterExport("d3d12.dll",  "D3D12CreateDevice", 0x00007FF8B0033000ULL);

    PeDllImportModule user32Module{};
    user32Module.DllName = "user32.dll";
    user32Module.Imports = {
        { "PeekMessageW",     0, 101, false },
        { "GetMessageW",      0, 102, false },
        { "DispatchMessageW", 0, 103, false },
        { "PostMessageW",     0, 104, false },
        { "TranslateMessage", 0, 105, false }
    };

    NTSTATUS status = router.BindImportAddressTable(user32Module);
    if (!NT_SUCCESS(status)) return false;

    // Verify all 5 thunks bound to correct 64-bit virtual addresses
    if (user32Module.Imports[0].BoundVirtualAddress != 0x00007FF8B0011000ULL || !user32Module.Imports[0].Bound) return false;
    if (user32Module.Imports[1].BoundVirtualAddress != 0x00007FF8B0011080ULL || !user32Module.Imports[1].Bound) return false;
    if (user32Module.Imports[2].BoundVirtualAddress != 0x00007FF8B0011100ULL || !user32Module.Imports[2].Bound) return false;
    if (user32Module.Imports[3].BoundVirtualAddress != 0x00007FF8B0011180ULL || !user32Module.Imports[3].Bound) return false;
    if (user32Module.Imports[4].BoundVirtualAddress != 0x00007FF8B0011200ULL || !user32Module.Imports[4].Bound) return false;

    // Test rejection of non-existent symbol
    PeDllImportModule invalidModule{};
    invalidModule.DllName = "user32.dll";
    invalidModule.Imports = { { "NonExistentFunction", 0, 999, false } };
    if (NT_SUCCESS(router.BindImportAddressTable(invalidModule))) return false;

    // ------------------------------------------------------------------------
    // Pillar 2: Win32 Thread Message Queue Posting & FIFO Dispatch
    // ------------------------------------------------------------------------
    Win32ThreadMessageQueue queue;
    void* targetHwnd = reinterpret_cast<void*>(0x00000000000C0300ULL);

    // Post messages representing window startup and initial input
    queue.PostMessage(targetHwnd, winnt_user32_gdi32_imm32_abi::WM_CREATE_OFFICIAL, 0, 0, 1000);
    queue.PostMessage(targetHwnd, winnt_user32_gdi32_imm32_abi::WM_SIZE_OFFICIAL, 0, (2160 << 16) | 3840, 1050);
    queue.PostMessage(targetHwnd, winnt_user32_gdi32_imm32_abi::WM_PAINT_OFFICIAL, 0, 0, 1100);
    queue.PostMessage(targetHwnd, winnt_user32_gdi32_imm32_abi::WM_KEYDOWN_OFFICIAL, 0x41 /* VK_A */, 0, 1150);

    if (queue.GetPendingCount() != 4u) return false;

    // Test PeekMessage with PM_NOREMOVE_OFFICIAL: verify non-destructive queue inspection
    winnt_user32_gdi32_imm32_abi::WinNtMsg peekMsg{};
    bool peeked = queue.PeekMessage(peekMsg, targetHwnd, 0, 0, winnt_user32_gdi32_imm32_abi::PM_NOREMOVE_OFFICIAL);
    if (!peeked || peekMsg.message != winnt_user32_gdi32_imm32_abi::WM_CREATE_OFFICIAL) return false;
    if (queue.GetPendingCount() != 4u) return false;

    // Test GetMessage: verify in-order FIFO dequeuing
    winnt_user32_gdi32_imm32_abi::WinNtMsg gotMsg{};
    bool got = queue.GetMessage(gotMsg, targetHwnd);
    if (!got || gotMsg.message != winnt_user32_gdi32_imm32_abi::WM_CREATE_OFFICIAL) return false;
    if (queue.GetPendingCount() != 3u) return false;

    // ------------------------------------------------------------------------
    // Pillar 3: Sub-Millisecond 8000Hz Raw Input Queuing & Microsecond Coalescing
    // ------------------------------------------------------------------------
    SurShell540HzEventRouter router540;

    // Simulate 16 raw mouse reports arriving within a single 1.85ms 540Hz frame (8000Hz rate)
    for (int i = 0; i < 16; ++i) {
        RawInputMouseReport report{};
        report.DeltaX = 2;
        report.DeltaY = -1;
        report.ButtonFlags = (i == 15) ? 1 : 0;
        report.TimestampQpc = 50000 + i * 12;
        router540.PushRawMouseReport(report);
    }

    auto coalesced = router540.CoalesceRawInput();
    if (coalesced.DeltaX != 32 || coalesced.DeltaY != -16) return false;
    if (coalesced.ButtonFlags != 1u) return false;

    // ------------------------------------------------------------------------
    // Pillar 4: SurShell 540Hz Compositor Event Routing & Frame Latency Synchronization
    // ------------------------------------------------------------------------
    bool inputDispatched = false;
    int32_t recDeltaX = 0;
    int32_t recDeltaY = 0;
    bool paintDispatched = false;
    bool keydownDispatched = false;

    router540.RegisterHandler(winnt_user32_gdi32_imm32_abi::WM_INPUT_OFFICIAL,
        [&](void*, uint32_t, uintptr_t, intptr_t l) -> intptr_t {
            inputDispatched = true;
            recDeltaX = static_cast<int16_t>(l & 0xFFFF);
            recDeltaY = static_cast<int16_t>((l >> 16) & 0xFFFF);
            return 0;
        });

    router540.RegisterHandler(winnt_user32_gdi32_imm32_abi::WM_PAINT_OFFICIAL,
        [&](void*, uint32_t, uintptr_t, intptr_t) -> intptr_t {
            paintDispatched = true;
            return 0;
        });

    router540.RegisterHandler(winnt_user32_gdi32_imm32_abi::WM_KEYDOWN_OFFICIAL,
        [&](void*, uint32_t, uintptr_t w, intptr_t) -> intptr_t {
            if (w == 0x41) keydownDispatched = true;
            return 0;
        });

    // Push second batch of 16 raw input reports and execute 540Hz compositor tick
    for (int i = 0; i < 16; ++i) {
        RawInputMouseReport report{};
        report.DeltaX = 1;
        report.DeltaY = 3;
        report.TimestampQpc = 60000 + i * 12;
        router540.PushRawMouseReport(report);
    }

    bool tickOk = router540.ExecuteCompositorTick(queue, targetHwnd);
    if (!tickOk) return false;
    if (!inputDispatched || recDeltaX != 16 || recDeltaY != 48) return false;
    if (!paintDispatched || !keydownDispatched) return false;

    // ------------------------------------------------------------------------
    // Pillar 5: Telemetry Validation
    // ------------------------------------------------------------------------
    const auto& tel = router540.GetTelemetry();
    if (tel.CompositorFramesRendered540Hz != 1u) return false;
    if (tel.RawInputReportsReceived != 32u) return false;
    if (tel.RawInputReportsCoalesced != 32u) return false;
    if (!tel.SurShell540HzPacingValid) return false;
    if (!tel.Wddm32ZeroLatencyFlipValid) return false;
    if (tel.FrameBudgetMicroseconds < 1851.0f || tel.FrameBudgetMicroseconds > 1852.0f) return false;
    if (tel.InputToPhotonLatencyMicroseconds > 1000.0f) return false;

    return true;
}

} // namespace micant::um::prismx_win32
