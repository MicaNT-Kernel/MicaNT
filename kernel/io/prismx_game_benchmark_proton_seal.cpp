/**
 * @file prismx_game_benchmark_proton_seal.cpp
 * @brief Horizon 91 Kernel Compilation Unit (#63):
 *        PrismX Game Engine Benchmark & Wine/Proton DX12/VulkanOn12 Zero-Overhead Seal.
 */

#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prismx_game_benchmark_proton_seal.hpp"

namespace micant::sys::internal {

void PrismXGameBenchmarkProtonSealInitializeExecutiveSubsystem() noexcept {
    micant::um::prismx_game_benchmark::InitializePrismXGameBenchmarkExports();
}

} // namespace micant::sys::internal
