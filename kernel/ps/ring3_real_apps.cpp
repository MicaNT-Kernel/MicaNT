/**
 * @file ring3_real_apps.cpp
 * @brief Horizon 70: Ring 3 Real-World Application Triad Roll-In Unit (#42)
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/winuser/ring3_real_apps_triad.hpp"
#include <cstdio>

namespace micant::um::ring3_apps {

void Ring3AppsInitializeExecutiveSubsystem() noexcept {
    auto& eng = Ring3RealAppsTriadEngine::instance();
    eng.initialize();

    const auto nppRep = eng.verifyNotepadPlusPlusOnFourRings();
    const auto vlcRep = eng.verifyVlcMediaPlayerOnFourRings();
    const auto szRep  = eng.verifySevenZipAndTaskMgrOnFourRings();

    std::printf("[MicaNT Ring 3 Apps] %s (PID %u, %u us CI, %u us Font) | vlc.exe (PID %u, %u us WaveRT, %u us DirectFlip, %u Drops) | 7zFM.exe (PID %u, %u Iter, %u us $MFT) | 4-Ring Seal=%s\n",
                nppRep.ExeName,
                nppRep.ProcessId,
                nppRep.CiHashVerifyLatencyUs,
                nppRep.FontdrvGlyphRasterLatencyUs,
                vlcRep.ProcessId,
                vlcRep.WaveRtDmaPeriodUs,
                vlcRep.DirectFlipScanoutLatencyUs,
                vlcRep.AudioUnderrunCount,
                szRep.SevenZipPid,
                szRep.Lzma2BenchmarkIterations,
                szRep.MftDirectQueryLatencyUs,
                szRep.AllFourRingsSealedAndActive ? "YES" : "NO");
}

} // namespace micant::um::ring3_apps

namespace micant::sys::internal {

void Ring3AppsInitializeExecutiveSubsystem() noexcept {
    micant::um::ring3_apps::Ring3AppsInitializeExecutiveSubsystem();
}

} // namespace micant::sys::internal
