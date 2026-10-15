/**
 * @file ring3_desktop_shell.cpp
 * @brief Horizon 69: Ring 3 Interactive Session winlogon.exe, Native C++23 explorer.exe & Zero-Copy dwm.exe Unit (#41)
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/servers/ring3_winlogon_explorer_dwm.hpp"
#include <cstdio>

namespace micant::um::ring3_shell {

void Ring3ShellInitializeExecutiveSubsystem() noexcept {
    auto& eng = Ring3WinlogonExplorerDwmEngine::instance();
    eng.initialize();

    const auto wlRep  = eng.bootstrapInteractiveWinlogonSession();
    const auto expRep = eng.launchZeroWebView2ExplorerShell();
    const auto dwmRep = eng.verifyZeroCopyDwmAndInputRouter();

    std::printf("[MicaNT Ring 3 Shell] winlogon.exe (PID %u, %s, %u us lsaiso Auth) | explorer.exe (PID %u, %u KB WS, %u WebView2, %u us $MFT) | dwm.exe (PID %u, %u Planes, %u Copies, %uHz Cursor)\n",
                wlRep.WinlogonPid,
                wlRep.AuthenticatedUser,
                wlRep.LsaIsoAuthLatencyUs,
                expRep.ExplorerPid,
                expRep.WorkingSetKb,
                expRep.WebView2ProcessCount,
                expRep.MftInstantSearchLatencyUs,
                dwmRep.DwmPid,
                dwmRep.DirectFlipHardwarePlanes,
                dwmRep.IntermediateDwmCopies,
                dwmRep.AsyncCursorHz);
}

} // namespace micant::um::ring3_shell

namespace micant::sys::internal {

void Ring3ShellInitializeExecutiveSubsystem() noexcept {
    micant::um::ring3_shell::Ring3ShellInitializeExecutiveSubsystem();
}

} // namespace micant::sys::internal
