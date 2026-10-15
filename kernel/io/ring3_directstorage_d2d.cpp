/**
 * @file ring3_directstorage_d2d.cpp
 * @brief Horizon 72: Sovereign Ring 3 DirectStorage 1.2 GDeflate GPU Decompression,
 *        Direct2D/DirectWrite & Vulkan/DirectML Tunneling Through Ring 2 fontdrvhost/WUDFHost
 *        & Ring 0 NVMe BypassIO Compilation Unit (base/ntos/io/ring3_directstorage_d2d_vulkan_tunnel).
 */

#include <iostream>
#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/ring3_directstorage_d2d_vulkan_tunnel.hpp"

namespace micant::sys::internal {

void Ring3DirectStorageD2DInitializeExecutiveSubsystem() noexcept {
    auto& mgr = micant::um::dstorage_tunnel::SovereignDirectStorageD2DVulkanTunnelManager::get();
    mgr.initializeAndVerifyDirectStorageTunnel();
    const auto& t = mgr.telemetry();
    std::cout << "[MicaNT Ring 3 DirectStorage] Archive=\"" << t.assetArchivePath
              << "\" (" << (t.compressedBytesQueued / 1024) << " KB -> "
              << (t.gpuDecompressedBytes / 1024) << " KB GDeflate VRAM) | NVMe BypassIO="
              << t.nvmeBypassIoThroughputMBps << " MB/s (" << t.cpuDecompressionCopies
              << " CPU copies) | R2 FontRaster=" << t.ring2FontRasterLatencyUs
              << " us (" << t.ring2FontGlyphBatchCount << " glyphs)\n";
}

} // namespace micant::sys::internal
