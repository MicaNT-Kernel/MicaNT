#pragma once

/**
 * @file libvlc.hpp
 * @brief Clean-room ABI bridge for VideoLAN libvlc.dll.
 *
 * Implements the core VLC media player initialization, playback control,
 * and interface registration exports for clean-room execution.
 */

#include <cstdint>
#include <string_view>
#include "ldr.hpp"

namespace micant::libvlc {

inline void* libvlc_new(int argc, const char* const* argv) noexcept {
    if (win32::g_TraceApi) {
        std::cout << "[*] [TRACE] libvlc_new(argc=" << argc << ")\n";
    }
    return reinterpret_cast<void*>(0x1000);
}

inline void libvlc_release(void* /*p_instance*/) noexcept {
    if (win32::g_TraceApi) std::cout << "[*] [TRACE] libvlc_release\n";
}

inline void libvlc_set_user_agent(void* /*p_instance*/, const char* /*name*/, const char* /*http*/) noexcept {
    if (win32::g_TraceApi) std::cout << "[*] [TRACE] libvlc_set_user_agent\n";
}

inline void libvlc_set_app_id(void* /*p_instance*/, const char* /*id*/, const char* /*version*/, const char* /*icon*/) noexcept {
    if (win32::g_TraceApi) std::cout << "[*] [TRACE] libvlc_set_app_id\n";
}

inline int libvlc_add_intf(void* /*p_instance*/, const char* name) noexcept {
    if (win32::g_TraceApi) {
        std::cout << "[*] [TRACE] libvlc_add_intf: " << (name ? name : "null") << "\n";
    }
    return 0;
}

inline void libvlc_playlist_play(void* /*p_instance*/) noexcept {
    if (win32::g_TraceApi) std::cout << "[*] [TRACE] libvlc_playlist_play\n";
}

inline void libvlc_wait(void* /*p_instance*/) noexcept {
    if (win32::g_TraceApi) std::cout << "[*] [TRACE] libvlc_wait\n";
}

inline void InitializeLibVlcExports() {
    auto& ldr = ldr::DynamicLoader::get();
    ldr.registerExport("libvlc.dll", "libvlc_new", reinterpret_cast<void*>(libvlc_new));
    ldr.registerExport("libvlc.dll", "libvlc_release", reinterpret_cast<void*>(libvlc_release));
    ldr.registerExport("libvlc.dll", "libvlc_set_user_agent", reinterpret_cast<void*>(libvlc_set_user_agent));
    ldr.registerExport("libvlc.dll", "libvlc_set_app_id", reinterpret_cast<void*>(libvlc_set_app_id));
    ldr.registerExport("libvlc.dll", "libvlc_add_intf", reinterpret_cast<void*>(libvlc_add_intf));
    ldr.registerExport("libvlc.dll", "libvlc_playlist_play", reinterpret_cast<void*>(libvlc_playlist_play));
    ldr.registerExport("libvlc.dll", "libvlc_wait", reinterpret_cast<void*>(libvlc_wait));
}

} // namespace micant::libvlc
