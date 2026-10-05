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
// Freestanding runtime allocator for bare-metal UEFI execution
alignas(16) static uint8_t s_uefiHeap[1024 * 1024]; // 1MB heap
static size_t s_uefiHeapOffset = 0;

void* operator new(size_t size) {
    size = (size + 15) & ~static_cast<size_t>(15);
    if (s_uefiHeapOffset + size > sizeof(s_uefiHeap)) return nullptr;
    void* ptr = &s_uefiHeap[s_uefiHeapOffset];
    s_uefiHeapOffset += size;
    return ptr;
}

void* operator new[](size_t size) {
    return operator new(size);
}

void operator delete(void*) noexcept {}
void operator delete[](void*) noexcept {}
void operator delete(void*, size_t) noexcept {}
void operator delete[](void*, size_t) noexcept {}

extern "C" void* malloc(size_t size) { return operator new(size); }
extern "C" void free(void*) {}

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
        if (systemTable->conOut) {
            systemTable->conOut->outputString(systemTable->conOut, L"[MicaNT Boot] Located UEFI GOP Linear Framebuffer successfully.\r\n");
        }
        engine.displaySplash("Initializing MicaNT Core Subsystems...", 0.25f);
    } else {
        if (systemTable->conOut) {
            systemTable->conOut->outputString(systemTable->conOut, L"[MicaNT Boot] Operating in serial/console mode (no GOP).\r\n");
        }
    }

    // 3. Query physical memory map
    alignas(16) static uint8_t s_memoryMapBuffer[65536];
    uint64_t memoryMapSize = sizeof(s_memoryMapBuffer);
    uint64_t mapKey = 0;
    uint64_t descriptorSize = sizeof(micant::uefi::EfiMemoryDescriptor);
    uint32_t descriptorVersion = 1;

    micant::uefi::EfiStatus mapStatus = systemTable->bootServices->getMemoryMap(
        &memoryMapSize,
        reinterpret_cast<micant::uefi::EfiMemoryDescriptor*>(s_memoryMapBuffer),
        &mapKey,
        &descriptorSize,
        &descriptorVersion
    );

    if (mapStatus == micant::uefi::EFI_SUCCESS) {
        size_t descCount = descriptorSize > 0 ? (memoryMapSize / descriptorSize) : 0;
        std::span<const micant::uefi::EfiMemoryDescriptor> mapSpan(
            reinterpret_cast<const micant::uefi::EfiMemoryDescriptor*>(s_memoryMapBuffer),
            descCount
        );

        if (systemTable->conOut) {
            systemTable->conOut->outputString(systemTable->conOut, L"[MicaNT Boot] Ingested physical memory map from UEFI firmware.\r\n");
        }

        // 4. Ingest memory map and build LOADER_PARAMETER_BLOCK
        if (hasGop) {
            engine.displaySplash("Building Loader Parameter Block (LPB)...", 0.65f);
        }
        engine.buildLoaderParameterBlock(mapSpan);
    }

    // 5. Exit Boot Services & Transfer Execution
    if (hasGop) {
        engine.displaySplash("Transferring Execution to KiSystemStartup...", 1.0f);
    }

    if (systemTable->conOut) {
        systemTable->conOut->outputString(systemTable->conOut, L"[MicaNT Boot] Exiting UEFI Boot Services and entering Long Mode...\r\n");
    }

    micant::uefi::EfiStatus exitStatus = engine.exitBootServices(mapKey);
    if (exitStatus != micant::uefi::EFI_SUCCESS) {
        // Retry once in case memory map key changed
        memoryMapSize = sizeof(s_memoryMapBuffer);
        systemTable->bootServices->getMemoryMap(
            &memoryMapSize,
            reinterpret_cast<micant::uefi::EfiMemoryDescriptor*>(s_memoryMapBuffer),
            &mapKey,
            &descriptorSize,
            &descriptorVersion
        );
        exitStatus = engine.exitBootServices(mapKey);
        if (exitStatus != micant::uefi::EFI_SUCCESS) {
            return exitStatus;
        }
    }

    // At this stage, firmware runtime terminates and we are in pure 64-bit Long Mode.
    // In real hardware / KiSystemStartup:
    // KiSystemStartup(&engine.getLpb());
    while (true) {
#if defined(__x86_64__)
        __asm__ __volatile__("hlt");
#endif
    }

    return micant::uefi::EFI_SUCCESS;
}
