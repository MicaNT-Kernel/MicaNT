// ============================================================================
// MicaNT: GDI+ 2D Vector & Imaging Satellite Subsystem (satellite_gdiplus.hpp)
//
// Modern Clean-Room Forwarding Bridge in pure ISO C++23.
// All exports have graduated directly to Canonical Core MicaNT Subsystems:
//   - gdiplus.dll -> include/micant/gdiplus.hpp
// ============================================================================

#pragma once

#include "gdiplus.hpp"

namespace micant::satellite::gdiplus {

using namespace micant::gdiplus;

template <typename TLoader>
inline void registerGdiPlusExports(TLoader&) {
    // 0 exports defined in satellite header.
    // All 98 GDI+ exports have graduated directly to canonical core
    // include/micant/gdiplus.hpp (micant::gdiplus::InitializeGdiPlusExports).
}

inline void InitializeGdiPlusSatelliteExports() {
    // 0 exports defined in satellite header.
    // Core exports are registered via micant::gdiplus::InitializeGdiPlusExports().
}

} // namespace micant::satellite::gdiplus
