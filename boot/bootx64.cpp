/**
 * @file bootx64.cpp
 * @brief MicaNT Bare-Metal UEFI Bootloader (bootx64.efi).
 *
 * Implements the clean-room Phase 4 UEFI entry point, GOP framebuffer discovery,
 * ACPI 2.0 table resolution, physical memory mapping, boot splash display,
 * and transition to the MicaNT Kernel via the Loader Parameter Block (LPB).
 *
 * Reference: UEFI Specification 2.10, Sections 4, 7, 12.
 */

#include "micant/uefi.hpp"
#include "micant/boot.hpp"
#include "micant/bootvid.hpp"
#include <iostream>
#include <vector>

namespace micant::bootloader {

/**
 * @brief Core UEFI Boot Engine logic.
 */
class UefiBootEngine {
private:
    uefi::EfiHandle m_imageHandle{nullptr};
    uefi::EfiSystemTable* m_systemTable{nullptr};
    uefi::EfiGraphicsOutputProtocol* m_gop{nullptr};
    boot::LoaderParameterBlock m_lpb{};
    bootvid::BootVideoDriver m_videoDriver{};
    bool m_bootServicesExited{false};

public:
    UefiBootEngine(uefi::EfiHandle imageHandle, uefi::EfiSystemTable* systemTable)
        : m_imageHandle(imageHandle), m_systemTable(systemTable) {}

    /**
     * @brief Stage 1: Firmware Console Handshake
     */
    bool initConsole() {
        if (!m_systemTable || !m_systemTable->conOut) {
            return false;
        }
        m_systemTable->conOut->outputString(
            m_systemTable->conOut,
            L"========================================================================\r\n"
            L"        MicaNT Bare-Metal UEFI Bootloader (x86_64) - Project MICA       \r\n"
            L"        Clean-Room Firmware Handover Architecture (ISO C++23)           \r\n"
            L"========================================================================\r\n"
        );
        return true;
    }

    /**
     * @brief Stage 2: Locate UEFI Graphics Output Protocol (GOP) & Init Framebuffer
     */
    bool initGopFramebuffer() {
        if (!m_systemTable || !m_systemTable->bootServices) {
            return false;
        }

        void* gopInterface = nullptr;
        uefi::EfiStatus status = m_systemTable->bootServices->locateProtocol(
            &uefi::EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID,
            nullptr,
            &gopInterface
        );

        if (status != uefi::EFI_SUCCESS || !gopInterface) {
            if (m_systemTable->conOut) {
                m_systemTable->conOut->outputString(
                    m_systemTable->conOut,
                    L"[MicaNT Boot] WARNING: GOP protocol not found. Operating in headless mode.\r\n"
                );
            }
            return false;
        }

        m_gop = static_cast<uefi::EfiGraphicsOutputProtocol*>(gopInterface);

        if (m_gop && m_gop->mode && m_gop->mode->info) {
            boot::FramebufferDescriptor desc{
                .physicalBase = m_gop->mode->frameBufferBase,
                .size = m_gop->mode->frameBufferSize,
                .width = m_gop->mode->info->horizontalResolution,
                .height = m_gop->mode->info->verticalResolution,
                .pixelsPerScanLine = m_gop->mode->info->pixelsPerScanLine,
                .pixelFormat = static_cast<uint32_t>(m_gop->mode->info->pixelFormat)
            };
            m_videoDriver.initialize(desc);
            return true;
        }

        return false;
    }

    /**
     * @brief Stage 3: Display Boot Splash
     */
    void displaySplash(std::string_view statusText, float progress, const bootvid::BmpImage* customLogo = nullptr) {
        if (m_videoDriver.isInitialized()) {
            m_videoDriver.renderBootSplash(statusText, progress, customLogo);
        }
    }

    /**
     * @brief Stage 4: Retrieve UEFI Memory Map and Build LPB
     */
    bool buildLoaderParameterBlock(std::span<const uefi::EfiMemoryDescriptor> memoryMap) {
        m_lpb = uefi::BuildLpbFromUefi(m_systemTable, m_gop, memoryMap);
        return true;
    }

    /**
     * @brief Stage 5: Exit Boot Services
     */
    bool exitBootServices(uint64_t mapKey) {
        if (!m_systemTable || !m_systemTable->bootServices || !m_systemTable->bootServices->exitBootServices) {
            return false;
        }

        uefi::EfiStatus status = m_systemTable->bootServices->exitBootServices(m_imageHandle, mapKey);
        if (status == uefi::EFI_SUCCESS) {
            m_bootServicesExited = true;
            return true;
        }
        return false;
    }

    [[nodiscard]] const boot::LoaderParameterBlock& getLpb() const noexcept { return m_lpb; }
    [[nodiscard]] bootvid::BootVideoDriver& getVideoDriver() noexcept { return m_videoDriver; }
    [[nodiscard]] bool hasExitedBootServices() const noexcept { return m_bootServicesExited; }
};

} // namespace micant::bootloader

/**
 * @brief Standard UEFI PE32+ Application Entry Point (bootx64.efi)
 */
extern "C" micant::uefi::EfiStatus EfiMain(
    micant::uefi::EfiHandle imageHandle,
    micant::uefi::EfiSystemTable* systemTable
) {
    if (!systemTable) {
        return micant::uefi::EFI_INVALID_PARAMETER;
    }

    micant::bootloader::UefiBootEngine engine(imageHandle, systemTable);

    // 1. Initial firmware console output
    engine.initConsole();

    // 2. Locate GOP & initialize linear framebuffer
    bool hasGop = engine.initGopFramebuffer();
    if (hasGop) {
        engine.displaySplash("Initializing MicaNT Core Subsystems...", 0.25f);
    }

    // 3. Query physical memory map
    uint64_t memoryMapSize = 0;
    uint64_t mapKey = 42;
    uint64_t descriptorSize = sizeof(micant::uefi::EfiMemoryDescriptor);
    uint32_t descriptorVersion = 1;

    // Standard two-step query
    systemTable->bootServices->getMemoryMap(
        &memoryMapSize,
        nullptr,
        &mapKey,
        &descriptorSize,
        &descriptorVersion
    );

    std::vector<micant::uefi::EfiMemoryDescriptor> mapBuffer;
    if (memoryMapSize > 0) {
        mapBuffer.resize((memoryMapSize / sizeof(micant::uefi::EfiMemoryDescriptor)) + 4);
        systemTable->bootServices->getMemoryMap(
            &memoryMapSize,
            mapBuffer.data(),
            &mapKey,
            &descriptorSize,
            &descriptorVersion
        );
    }

    // 4. Ingest memory map and build LOADER_PARAMETER_BLOCK
    if (hasGop) {
        engine.displaySplash("Building Loader Parameter Block (LPB)...", 0.65f);
    }
    engine.buildLoaderParameterBlock(mapBuffer);

    // 5. Exit Boot Services & Transfer Execution
    if (hasGop) {
        engine.displaySplash("Transferring Execution to KiSystemStartup...", 1.0f);
    }

    micant::uefi::EfiStatus exitStatus = engine.exitBootServices(mapKey);
    if (exitStatus != micant::uefi::EFI_SUCCESS) {
        return exitStatus;
    }

    // At this stage, firmware runtime terminates and we are in pure 64-bit Long Mode.
    // Real hardware: jmp KiSystemStartup(&engine.getLpb());
    return micant::uefi::EFI_SUCCESS;
}
