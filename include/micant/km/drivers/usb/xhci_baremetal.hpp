// ============================================================================
// MicaNT: Bare-Metal Intel xHCI USB 3.0 Host Controller & HID Keyboard Engine
// (xhci_baremetal.hpp)
//
// Description:
//   Reusable xHCI transport implementation intended for a standalone OEM .sys
//   driver. The current endpoint setup remains a narrow legacy HID prototype;
//   it is not connected to the production USB/PnP/DMA lifecycle yet.
//
//   Features:
//   - USBLEGSUP BIOS-to-OS semaphore handoff
//   - Controller reset and 64-byte aligned DCBAA / Command Ring / Event Ring init
//   - Dynamic 32-byte / 64-byte Context Size support (HCCPARAMS1.CSZ)
//   - Accurate Input Control Context indexing: Slot=1, EP0(DCI 1)=2, EP1-IN(DCI 3)=4
//   - Root Hub Port 1 detection & reset (Full-Speed 12M)
//   - Slot enablement, device addressing, and Endpoint 1 (Interrupt IN) config
//   - Asynchronous interrupt transfer queueing and Event Ring completion polling
//   - Bounded raw HID report completion handoff for a canonical HID consumer
//
// Clean-Room ISO C++23. Zero proprietary Microsoft code.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include "micant/shared/ntdef.hpp"
#include "micant/shared/ntstatus.hpp"
#include "micant/km/hal/highhalf.hpp"
#include "micant/km/hal/amd64_bootstrap.hpp"
#include "micant/km/core/ke.hpp"

namespace micant::km::xhci_baremetal {

// --- Port I/O Primitives ---
static inline void outb(uint16_t port, uint8_t val) noexcept {
#if defined(__x86_64__)
    __asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
#endif
}

static inline uint8_t inb(uint16_t port) noexcept {
    uint8_t ret = 0;
#if defined(__x86_64__)
    __asm__ __volatile__("inb %1, %0" : "=a"(ret) : "Nd"(port));
#endif
    return ret;
}

static inline micant::hal::amd64::BootstrapClockStatus waitForMilliseconds(uint32_t ms) noexcept {
    using namespace micant::hal::amd64;
    BootstrapDeadline deadline{};
    BootstrapClockStatus status = makeBootstrapDeadline(ms, deadline);
    if (status != BootstrapClockStatus::Success) return status;
    for (;;) {
        bool expired = false;
        status = isBootstrapDeadlineExpired(deadline, expired);
        if (status != BootstrapClockStatus::Success || expired) return status;
        status = delayBootstrapMicroseconds(50);
        if (status != BootstrapClockStatus::Success) return status;
    }
}

static inline bool bootstrapWaitContextReady() noexcept {
    if (micant::ke::KeGetCurrentIrql() != micant::ke::PASSIVE_LEVEL) return false;
#if defined(__x86_64__)
    uintptr_t flags = 0;
    __asm__ __volatile__("pushfq; popq %0" : "=r"(flags) :: "memory");
    if ((flags & (1ULL << 9)) == 0) return false;
#else
    return false;
#endif
    uint64_t counter = 0, frequency = 0;
    return micant::hal::amd64::queryBootstrapClock(counter, frequency) ==
               micant::hal::amd64::BootstrapClockStatus::Success && frequency != 0;
}

template <typename Predicate>
static inline bool pollUntilDeadline(uint32_t timeoutMs, Predicate&& predicate,
                                    micant::hal::amd64::BootstrapClockStatus& status) noexcept {
    using namespace micant::hal::amd64;
    BootstrapDeadline deadline{};
    status = makeBootstrapDeadline(timeoutMs, deadline);
    if (status != BootstrapClockStatus::Success) return false;
    for (;;) {
        if (predicate()) return true;
        bool expired = false;
        status = isBootstrapDeadlineExpired(deadline, expired);
        if (status != BootstrapClockStatus::Success || expired) return false;
        status = delayBootstrapMicroseconds(50);
        if (status != BootstrapClockStatus::Success) return false;
    }
}

// --- MMIO Access Helpers ---
static inline uint8_t mmioRead8(uint64_t addr) noexcept {
    return *reinterpret_cast<volatile const uint8_t*>(addr);
}

static inline uint16_t mmioRead16(uint64_t addr) noexcept {
    return *reinterpret_cast<volatile const uint16_t*>(addr);
}

static inline uint32_t mmioRead32(uint64_t addr) noexcept {
    return *reinterpret_cast<volatile const uint32_t*>(addr);
}

static inline uint64_t mmioRead64(uint64_t addr) noexcept {
    return *reinterpret_cast<volatile const uint64_t*>(addr);
}

static inline void mmioWrite8(uint64_t addr, uint8_t val) noexcept {
    *reinterpret_cast<volatile uint8_t*>(addr) = val;
}

static inline void mmioWrite32(uint64_t addr, uint32_t val) noexcept {
    *reinterpret_cast<volatile uint32_t*>(addr) = val;
}

static inline void mmioWrite64(uint64_t addr, uint64_t val) noexcept {
    *reinterpret_cast<volatile uint64_t*>(addr) = val;
}

// ============================================================================
// xHCI Hardware Structures
// ============================================================================

struct alignas(16) XhciTrb {
    uint64_t param{0};
    uint32_t status{0};
    uint32_t control{0};
};

struct alignas(16) XhciErstEntry {
    uint64_t ringSegmentBaseAddress{0};
    uint32_t ringSegmentSize{0};
    uint32_t reserved{0};
};

// TRB Types
inline constexpr uint32_t TRB_NORMAL               = 1;
inline constexpr uint32_t TRB_SETUP_STAGE          = 2;
inline constexpr uint32_t TRB_DATA_STAGE           = 3;
inline constexpr uint32_t TRB_STATUS_STAGE         = 4;
inline constexpr uint32_t TRB_LINK                 = 6;
inline constexpr uint32_t TRB_ENABLE_SLOT          = 9;
inline constexpr uint32_t TRB_DISABLE_SLOT         = 10;
inline constexpr uint32_t TRB_ADDRESS_DEVICE       = 11;
inline constexpr uint32_t TRB_CONFIGURE_ENDPOINT   = 12;
inline constexpr uint32_t TRB_TRANSFER_EVENT       = 32;
inline constexpr uint32_t TRB_CMD_COMPLETION_EVENT = 33;
inline constexpr uint32_t TRB_PORT_STATUS_CHANGE   = 34;

// Serial Logging
class KernelLogSink {
public:
    static void print(const char* s) noexcept {
        if (!s) return;
        while (*s) {
            while ((inb(0x3F8 + 5) & 0x20) == 0) {}
            outb(0x3F8, static_cast<uint8_t>(*s++));
        }
    }
    static void printHex(uint64_t val) noexcept {
        const char hex[] = "0123456789ABCDEF";
        char buf[19];
        buf[0] = '0'; buf[1] = 'x';
        for (int i = 15; i >= 0; --i) {
            buf[2 + (15 - i)] = hex[(val >> (i * 4)) & 0xF];
        }
        buf[18] = '\0';
        print(buf);
    }
    static void printDec(uint32_t val) noexcept {
        char buf[12];
        int idx = 10;
        buf[11] = '\0';
        if (val == 0) { print("0"); return; }
        while (val > 0 && idx >= 0) {
            buf[idx--] = '0' + (val % 10);
            val /= 10;
        }
        print(&buf[idx + 1]);
    }
};

// ============================================================================
// Static Memory Structures (Zero Dynamic Allocations, 100% Volatile DMA Safe)
// ============================================================================

alignas(64) static volatile uint64_t s_dcbaa[64]{};
alignas(64) static volatile uint64_t s_scratchpadArray[128]{};
alignas(4096) static volatile uint8_t s_scratchpadPages[32][4096]{};
alignas(64) static volatile XhciTrb  s_cmdRing[32]{};
alignas(64) static volatile XhciTrb  s_evtRing[64]{};
alignas(64) static volatile XhciErstEntry s_erst[1]{};

// Up to 33 contexts (Input Control Context + Slot Context + 31 EP Contexts)
// At 64 bytes max each (16 dwords): 33 * 16 = 528 dwords.
alignas(64) static volatile uint32_t s_inputContext[1024]{};
alignas(64) static volatile uint32_t s_devContext[1024]{};

alignas(64) static volatile XhciTrb  s_ep0Ring[32]{};
alignas(64) static volatile XhciTrb  s_ep1Ring[32]{};
alignas(64) static volatile uint8_t  s_kbdReport[8]{};

// A completed controller transfer is transport data, not yet a keyboard or
// mouse input packet. The USB/HID stack must parse the device's real report
// descriptor before passing this payload to KbdHid/MouHid.
struct XhciHidReportCompletion {
    uint8_t slotId{0};
    uint8_t endpointId{0};
    uint8_t completionCode{0};
    uint8_t reserved{0};
    size_t bytesTransferred{0};
};

// ============================================================================
// Bare-Metal xHCI Engine
// ============================================================================

class XhciBareMetalEngine {
private:
    uint64_t m_mmioBase{0};
    uint64_t m_mmioPhysical{0};
    uint64_t m_mmioSize{0};
    uint64_t m_opBase{0};
    uint64_t m_rtBase{0};
    uint64_t m_dbBase{0};
    uint32_t m_slotId{0};
    uint8_t  m_pciBus{0};
    uint8_t  m_pciDevice{0};
    uint8_t  m_pciFunction{0};
    uint32_t m_targetPort{1};
    size_t   m_ctxDwords{8}; // 8 for 32-byte contexts, 16 for 64-byte contexts
    uint8_t  m_cmdEnqueue{0};
    uint8_t  m_cmdCycle{1};
    uint8_t  m_evtDequeue{0};
    uint8_t  m_evtCycle{1};
    uint8_t  m_ep0Enqueue{0};
    uint8_t  m_ep0Cycle{1};
    uint8_t  m_ep1Enqueue{0};
    uint8_t  m_ep1Cycle{1};
    uint32_t m_transferEventsCount{0};
    bool     m_initialized{false};
    bool     m_keyboardReady{false};
    bool     m_pciAccessFailed{false};
    char     m_statusStr[64]{"xHCI: Uninitialized"};

    bool pciRead32(uint16_t bus, uint16_t dev, uint16_t func,
                   uint16_t reg, uint32_t& value) noexcept {
        if (micant::hal::amd64::readPciConfig32(bus, dev, func, reg, value) ==
            micant::hal::amd64::PciConfigAccessResult::Success) return true;
        m_pciAccessFailed = true;
        return false;
    }

    bool pciWrite32(uint16_t bus, uint16_t dev, uint16_t func,
                    uint16_t reg, uint32_t value) noexcept {
        if (micant::hal::amd64::writePciConfig32(bus, dev, func, reg, value) ==
            micant::hal::amd64::PciConfigAccessResult::Success) return true;
        m_pciAccessFailed = true;
        return false;
    }

    [[nodiscard]] static uint64_t toPhys(const volatile void* ptr) noexcept {
        uint64_t va = reinterpret_cast<uint64_t>(const_cast<const void*>(ptr));
        return micant::hal::highhalf::toPhysical(va);
    }

    static size_t strLen(const char* s) noexcept {
        size_t l = 0;
        while (s && s[l]) ++l;
        return l;
    }

    static void copyStr(char* dst, const char* src, size_t maxLen) noexcept {
        if (!dst || maxLen == 0) return;
        size_t i = 0;
        while (src && src[i] && i + 1 < maxLen) {
            dst[i] = src[i];
            i++;
        }
        dst[i] = '\0';
    }

    void setStatus(const char* s) noexcept {
        size_t i = 0;
        while (s && s[i] && i < 63) {
            m_statusStr[i] = s[i];
            i++;
        }
        m_statusStr[i] = '\0';
    }

    bool waitForCmdComplete(uint32_t& outSlotId) noexcept {
        bool commandSucceeded = false;
        micant::hal::amd64::BootstrapClockStatus clockStatus{};
        const bool completed = pollUntilDeadline(1000, [&]() noexcept {
            volatile XhciTrb* trb = &s_evtRing[m_evtDequeue];
            uint32_t ctrl = trb->control;
            if ((ctrl & 1) == m_evtCycle) {
                uint32_t type = (ctrl >> 10) & 0x3F;
                uint32_t code = (trb->status >> 24) & 0xFF;
                uint32_t evtSlot = (ctrl >> 24) & 0xFF;

                // Advance Event Ring Dequeue Pointer
                m_evtDequeue = (m_evtDequeue + 1) % 64;
                if (m_evtDequeue == 0) m_evtCycle ^= 1;
                mmioWrite64(m_rtBase + 0x38, toPhys(&s_evtRing[m_evtDequeue]) | (1ULL << 3));

                if (type == TRB_CMD_COMPLETION_EVENT) {
                    outSlotId = evtSlot;
                    if (code != 1) {
                        KernelLogSink::print("[xHCI] Cmd Completion Code Error: ");
                        KernelLogSink::printDec(code);
                        KernelLogSink::print("\r\n");
                        char err[64];
                        copyStr(err, "xHCI: Cmd Code Err ", sizeof(err));
                        size_t el = strLen(err);
                        if (code >= 10) err[el++] = '0' + static_cast<char>(code / 10);
                        err[el++] = '0' + static_cast<char>(code % 10);
                        err[el] = '\0';
                        setStatus(err);
                        commandSucceeded = false;
                        return true;
                    }
                    commandSucceeded = true;
                    return true;
                }
            }
            return false;
        }, clockStatus);
        if (completed) return commandSucceeded;
        if (clockStatus != micant::hal::amd64::BootstrapClockStatus::Success) {
            setStatus("xHCI: HAL clock unavailable during command wait");
            return false;
        }
        uint32_t sts = mmioRead32(m_opBase + 0x04);
        KernelLogSink::print("[xHCI] Cmd timed out! USBSTS=0x");
        KernelLogSink::printHex(sts);
        KernelLogSink::print("\r\n");
        char tout[64];
        copyStr(tout, "xHCI: Cmd Timeout STS=0x", sizeof(tout));
        size_t tl = strLen(tout);
        const char hexC[] = "0123456789ABCDEF";
        tout[tl++] = hexC[(sts >> 12) & 0xF];
        tout[tl++] = hexC[(sts >> 8) & 0xF];
        tout[tl++] = hexC[(sts >> 4) & 0xF];
        tout[tl++] = hexC[sts & 0xF];
        tout[tl] = '\0';
        setStatus(tout);
        return false;
    }

    void sendCommand(uint64_t param, uint32_t status, uint32_t trbType, uint32_t slotId = 0) noexcept {
        s_cmdRing[m_cmdEnqueue].param = param;
        s_cmdRing[m_cmdEnqueue].status = status;
        s_cmdRing[m_cmdEnqueue].control = (trbType << 10) | (slotId << 24) | m_cmdCycle;

        m_cmdEnqueue++;
        if (m_cmdEnqueue == 31) { // 32nd TRB is Link TRB
            s_cmdRing[31].param = toPhys(&s_cmdRing[0]);
            s_cmdRing[31].status = 0;
            s_cmdRing[31].control = (TRB_LINK << 10) | (1 << 1) | m_cmdCycle; // Toggle Cycle = bit 1
            m_cmdEnqueue = 0;
            m_cmdCycle ^= 1;
        }

#if defined(__x86_64__)
        __asm__ __volatile__("mfence" ::: "memory");
#endif

        // Ring Doorbell 0 (Host Controller Command Ring)
        mmioWrite32(m_dbBase + 0x00, 0);
    }

    bool sendControlNoData(uint8_t bmRequestType, uint8_t bRequest, uint16_t wValue, uint16_t wIndex) noexcept {
        uint64_t setupData = static_cast<uint64_t>(bmRequestType) |
                             (static_cast<uint64_t>(bRequest) << 8) |
                             (static_cast<uint64_t>(wValue) << 16) |
                             (static_cast<uint64_t>(wIndex) << 32);

        // 1. Setup Stage TRB (TRT = 0 No Data, IDT = 1 Immediate Data)
        s_ep0Ring[m_ep0Enqueue].param = setupData;
        s_ep0Ring[m_ep0Enqueue].status = 8; // Length 8 bytes
        s_ep0Ring[m_ep0Enqueue].control = (TRB_SETUP_STAGE << 10) | (1 << 6) | m_ep0Cycle;

        m_ep0Enqueue++;
        if (m_ep0Enqueue == 31) {
            s_ep0Ring[31].param = toPhys(&s_ep0Ring[0]);
            s_ep0Ring[31].status = 0;
            s_ep0Ring[31].control = (TRB_LINK << 10) | (1 << 1) | m_ep0Cycle;
            m_ep0Enqueue = 0;
            m_ep0Cycle ^= 1;
        }

        // 2. Status Stage TRB (DIR = 1 IN, IOC = 1)
        s_ep0Ring[m_ep0Enqueue].param = 0;
        s_ep0Ring[m_ep0Enqueue].status = 0;
        s_ep0Ring[m_ep0Enqueue].control = (TRB_STATUS_STAGE << 10) | (1 << 16) | (1 << 5) | m_ep0Cycle;

        m_ep0Enqueue++;
        if (m_ep0Enqueue == 31) {
            s_ep0Ring[31].param = toPhys(&s_ep0Ring[0]);
            s_ep0Ring[31].status = 0;
            s_ep0Ring[31].control = (TRB_LINK << 10) | (1 << 1) | m_ep0Cycle;
            m_ep0Enqueue = 0;
            m_ep0Cycle ^= 1;
        }

#if defined(__x86_64__)
        __asm__ __volatile__("mfence" ::: "memory");
#endif

        // Ring Doorbell for Slot ID, Target = 1 (Control Endpoint 0, DCI 1)
        mmioWrite32(m_dbBase + m_slotId * 4, 1);

        // Wait for Transfer Event on Primary Event Ring (ep == 1).
        bool transferSucceeded = false;
        micant::hal::amd64::BootstrapClockStatus clockStatus{};
        const bool completed = pollUntilDeadline(300, [&]() noexcept {
            volatile XhciTrb* trb = &s_evtRing[m_evtDequeue];
            uint32_t ctrl = trb->control;
            if ((ctrl & 1) == m_evtCycle) {
                uint32_t type = (ctrl >> 10) & 0x3F;
                uint32_t slot = (ctrl >> 24) & 0xFF;
                uint32_t ep   = (ctrl >> 16) & 0x1F;
                uint32_t comp = (trb->status >> 24) & 0xFF;

                m_evtDequeue = (m_evtDequeue + 1) % 64;
                if (m_evtDequeue == 0) m_evtCycle ^= 1;
                mmioWrite64(m_rtBase + 0x38, toPhys(&s_evtRing[m_evtDequeue]) | (1ULL << 3));

                if (type == TRB_TRANSFER_EVENT && slot == m_slotId && ep == 1) {
                    if (comp == 1 || comp == 13) {
                        transferSucceeded = true;
                        return true;
                    }
                    KernelLogSink::print("[xHCI] Control transfer comp code: ");
                    KernelLogSink::printDec(comp);
                    KernelLogSink::print("\r\n");
                    transferSucceeded = false;
                    return true;
                }
            }
            return false;
        }, clockStatus);
        if (completed) return transferSucceeded;
        if (clockStatus != micant::hal::amd64::BootstrapClockStatus::Success) {
            setStatus("xHCI: HAL clock unavailable during transfer wait");
            return false;
        }
        KernelLogSink::print("[xHCI] Control transfer timed out!\r\n");
        return false;
    }

    void queueKeyboardTransfer() noexcept {
        s_ep1Ring[m_ep1Enqueue].param = toPhys(&s_kbdReport[0]);
        s_ep1Ring[m_ep1Enqueue].status = 8; // Transfer length = 8 bytes
        s_ep1Ring[m_ep1Enqueue].control = (TRB_NORMAL << 10) | (1 << 5) | (1 << 2) | m_ep1Cycle; // IOC=1, ISP=1

        m_ep1Enqueue++;
        if (m_ep1Enqueue == 31) {
            s_ep1Ring[31].param = toPhys(&s_ep1Ring[0]);
            s_ep1Ring[31].status = 0;
            s_ep1Ring[31].control = (TRB_LINK << 10) | (1 << 1) | m_ep1Cycle;
            m_ep1Enqueue = 0;
            m_ep1Cycle ^= 1;
        }

#if defined(__x86_64__)
        __asm__ __volatile__("mfence" ::: "memory");
#endif

        // Ring Doorbell for Slot 1, Endpoint 1 IN (DCI 3)
        mmioWrite32(m_dbBase + m_slotId * 4, 3);
    }

    bool findController() noexcept {
        m_pciAccessFailed = false;
        for (uint16_t bus = 0; bus <= UINT8_MAX; ++bus) {
            for (uint8_t device = 0; device < 32; ++device) {
                uint32_t id0{};
                if (!pciRead32(bus, device, 0, 0x00, id0)) return false;
                if (static_cast<uint16_t>(id0) == UINT16_MAX) continue;
                uint32_t headerReg{};
                if (!pciRead32(bus, device, 0, 0x0C, headerReg)) return false;
                const uint8_t headerType = static_cast<uint8_t>(headerReg >> 16);
                const uint8_t functions = (headerType & 0x80) ? 8 : 1;
                for (uint8_t function = 0; function < functions; ++function) {
                    uint32_t id = id0;
                    if (function != 0 && !pciRead32(bus, device, function, 0x00, id))
                        return false;
                    if (static_cast<uint16_t>(id) == UINT16_MAX) continue;
                    uint32_t classReg{};
                    if (!pciRead32(bus, device, function, 0x08, classReg)) return false;
                    const uint8_t baseClass = static_cast<uint8_t>(classReg >> 24);
                    const uint8_t subClass = static_cast<uint8_t>(classReg >> 16);
                    const uint8_t progIf = static_cast<uint8_t>(classReg >> 8);
                    if (baseClass != 0x0C || subClass != 0x03 || progIf != 0x30) continue;
                    m_pciBus = static_cast<uint8_t>(bus);
                    m_pciDevice = device;
                    m_pciFunction = function;
                    return true;
                }
            }
        }
        return false;
    }

    bool mapControllerRegisters() noexcept {
        uint32_t barLow{};
        if (!pciRead32(m_pciBus, m_pciDevice, m_pciFunction, 0x10, barLow)) return false;
        if ((barLow & 1U) || ((barLow >> 1) & 3U) == 1U ||
            ((barLow >> 1) & 3U) == 3U || (barLow & (1U << 3))) return false;

        const bool is64Bit = ((barLow >> 1) & 3U) == 2U;
        uint32_t barHigh{};
        if (is64Bit && !pciRead32(m_pciBus, m_pciDevice, m_pciFunction, 0x14, barHigh))
            return false;
        const uint64_t physical = (static_cast<uint64_t>(barHigh) << 32) |
                                  static_cast<uint64_t>(barLow & ~0x0FU);
        if (!physical) return false;

        // Probe BAR aperture size with memory decode disabled, then restore
        // the exact command and BAR values before mapping any register.
        uint32_t command{};
        if (!pciRead32(m_pciBus, m_pciDevice, m_pciFunction, 0x04, command)) return false;
        if (!pciWrite32(m_pciBus, m_pciDevice, m_pciFunction, 0x04,
                        (command & 0xFFFFU) & ~0x0002U)) return false;

        bool probeOk = pciWrite32(m_pciBus, m_pciDevice, m_pciFunction, 0x10, UINT32_MAX);
        if (probeOk && is64Bit)
            probeOk = pciWrite32(m_pciBus, m_pciDevice, m_pciFunction, 0x14, UINT32_MAX);
        uint32_t maskLow{}, maskHigh{};
        if (probeOk) probeOk = pciRead32(m_pciBus, m_pciDevice, m_pciFunction, 0x10, maskLow);
        if (probeOk && is64Bit)
            probeOk = pciRead32(m_pciBus, m_pciDevice, m_pciFunction, 0x14, maskHigh);

        // Always restore the original BAR contents and command register after
        // a probe has started, including every HAL access-error path.
        bool restored = true;
        if (is64Bit)
            restored = pciWrite32(m_pciBus, m_pciDevice, m_pciFunction, 0x14, barHigh) && restored;
        restored = pciWrite32(m_pciBus, m_pciDevice, m_pciFunction, 0x10, barLow) && restored;
        restored = pciWrite32(m_pciBus, m_pciDevice, m_pciFunction, 0x04,
                              command & 0xFFFFU) && restored;
        if (!probeOk || !restored) return false;

        const uint64_t mask = is64Bit
            ? (static_cast<uint64_t>(maskHigh) << 32) |
                  static_cast<uint64_t>(maskLow & ~0x0FU)
            : static_cast<uint64_t>(maskLow & ~0x0FU);
        if (!mask) return false;
        const uint64_t size = is64Bit
            ? (~mask) + 1
            : static_cast<uint32_t>(~static_cast<uint32_t>(mask)) + 1U;
        if (size < 0x1000 || size > 0x100000 || (size & (size - 1)) != 0 ||
            physical > UINT64_MAX - size) return false;

        // Keep one mapping across failed initialization retries. Bootstrap MM
        // maps it uncached and returns null if the physical range is invalid.
        if (m_mmioBase && m_mmioPhysical == physical && m_mmioSize == size) return true;
        void* mapping = micant::hal::amd64::mapBootstrapDeviceMemory(
            physical, static_cast<size_t>(size));
        if (!mapping) return false;
        const uint64_t virtualBase = reinterpret_cast<uintptr_t>(mapping);
        if (virtualBase > UINT64_MAX - size) {
            (void)micant::hal::amd64::releaseBootstrapDeviceMemory(
                mapping, static_cast<size_t>(size));
            return false;
        }
        m_mmioPhysical = physical;
        m_mmioSize = size;
        m_mmioBase = virtualBase;
        return true;
    }

public:
    static XhciBareMetalEngine& get() noexcept {
        static XhciBareMetalEngine instance;
        return instance;
    }

    [[nodiscard]] const char* getStatus() const noexcept {
        return m_statusStr;
    }

    bool initialize() noexcept {
        if (m_initialized) return true;

        // Fail before PCI/MMIO access or controller state changes if calibrated
        // BSP timing or the legal initialization context is unavailable.
        if (!bootstrapWaitContextReady()) {
            setStatus("xHCI: Calibrated BSP clock/context unavailable");
            return false;
        }

        // This legacy engine still has no canonical HAL PCI resource claim or
        // device-safe DMA allocator. Do not touch PCI configuration, reset the
        // controller, or start bus mastering until those owners are connected.
        setStatus("xHCI: HAL/Io resource and DMA services unavailable");
        return false;

        KernelLogSink::print("\r\n[xHCI] Scanning PCI hierarchy for xHCI controllers...\r\n");
        if (!findController()) {
            if (m_pciAccessFailed) {
                setStatus("xHCI: HAL PCI config read failed");
            } else {
                KernelLogSink::print("[xHCI] ERROR: No PCI class 0C/03/30 controller found.\r\n");
                setStatus("xHCI: No Controller Found");
            }
            return false;
        }

        if (!mapControllerRegisters()) {
            setStatus(m_pciAccessFailed ? "xHCI: HAL PCI config access failed" :
                                          "xHCI: Invalid MMIO BAR");
            return false;
        }
        const uint64_t bar0 = m_mmioPhysical;

        KernelLogSink::print("[xHCI] MMIO Base: Phys=0x");
        KernelLogSink::printHex(bar0);
        KernelLogSink::print(" HighVA=0x");
        KernelLogSink::printHex(m_mmioBase);
        KernelLogSink::print("\r\n");

        uint8_t capLength = mmioRead8(m_mmioBase + 0x00);
        uint16_t hciVersion = mmioRead16(m_mmioBase + 0x02);
        uint32_t hcsparams1 = mmioRead32(m_mmioBase + 0x04);
        uint32_t hccparams1 = mmioRead32(m_mmioBase + 0x10);
        uint32_t dboff = mmioRead32(m_mmioBase + 0x14);
        uint32_t rtsoff = mmioRead32(m_mmioBase + 0x18);

        uint32_t maxPorts = (hcsparams1 >> 24) & 0xFF;
        if (capLength < 0x20 || maxPorts == 0 || maxPorts > 64 ||
            capLength >= m_mmioSize || dboff >= m_mmioSize || rtsoff >= m_mmioSize ||
            static_cast<uint64_t>(capLength) + 0x400 + static_cast<uint64_t>(maxPorts) * 0x10 >
                m_mmioSize || static_cast<uint64_t>(dboff & ~0x03U) + 0x10 > m_mmioSize ||
            static_cast<uint64_t>(rtsoff & ~0x1FU) + 0x40 > m_mmioSize) {
            setStatus("xHCI: Invalid capability registers");
            return false;
        }

        // Context Size: Bit 2 of HCCPARAMS1 (0 = 32-byte contexts, 1 = 64-byte contexts)
        m_ctxDwords = (hccparams1 & (1U << 2)) ? 16 : 8;

        m_opBase = m_mmioBase + capLength;
        m_dbBase = m_mmioBase + (dboff & ~0x03ULL);
        m_rtBase = m_mmioBase + (rtsoff & ~0x1FULL);

        KernelLogSink::print("[xHCI] Version: ");
        KernelLogSink::printHex(hciVersion);
        KernelLogSink::print(" | CtxSize=");
        KernelLogSink::printDec(static_cast<uint32_t>(m_ctxDwords * 4));
        KernelLogSink::print(" bytes | OpBase=0x");
        KernelLogSink::printHex(m_opBase);
        KernelLogSink::print(" | DbBase=0x");
        KernelLogSink::printHex(m_dbBase);
        KernelLogSink::print("\r\n");

        // 2. BIOS / OS Ownership Handoff via USBLEGSUP
        uint32_t xecp = (hccparams1 >> 16) << 2;
        if (xecp != 0 && static_cast<uint64_t>(xecp) + 4 > m_mmioSize) {
            setStatus("xHCI: Invalid extended capability offset");
            return false;
        }
        if (xecp != 0) {
            uint64_t capOffset = xecp;
            bool ownershipCapabilityFound = false;
            bool ownershipReleased = false;
            bool capabilitiesValid = false;
            for (int step = 0; step < 16; ++step) {
                if (capOffset > m_mmioSize - 4 || m_mmioBase > UINT64_MAX - capOffset) break;
                const uint64_t currCap = m_mmioBase + capOffset;
                uint32_t capVal = mmioRead32(currCap);
                uint8_t capId = capVal & 0xFF;
                uint8_t nextPtr = (capVal >> 8) & 0xFF;
                if (capId == 1) { // USB Legacy Support
                    ownershipCapabilityFound = true;
                    KernelLogSink::print("[xHCI] Claiming OS Ownership in USBLEGSUP (Offset 0x");
                    KernelLogSink::printHex(currCap - m_mmioBase);
                    KernelLogSink::print(")...\r\n");
                    mmioWrite32(currCap, capVal | (1 << 24)); // Set OS Owned Semaphore
                    micant::hal::amd64::BootstrapClockStatus clockStatus{};
                    ownershipReleased = pollUntilDeadline(50, [&]() noexcept {
                        return (mmioRead32(currCap) & (1 << 16)) == 0;
                    }, clockStatus);
                    if (!ownershipReleased && clockStatus != micant::hal::amd64::BootstrapClockStatus::Success) {
                        setStatus("xHCI: HAL clock unavailable during ownership handoff");
                        return false;
                    }
                    capabilitiesValid = true;
                    break;
                }
                if (nextPtr == 0) {
                    capabilitiesValid = true;
                    break;
                }
                const uint64_t advance = static_cast<uint64_t>(nextPtr) << 2;
                if (advance > m_mmioSize - capOffset) break;
                capOffset += advance;
            }
            if (!capabilitiesValid || (ownershipCapabilityFound && !ownershipReleased)) {
                if (!capabilitiesValid) {
                    KernelLogSink::print("[xHCI] Invalid extended capability chain; controller left untouched.\r\n");
                    setStatus("xHCI: Invalid extended capabilities");
                } else {
                    KernelLogSink::print("[xHCI] Firmware ownership handoff timed out; controller left untouched.\r\n");
                    setStatus("xHCI: Firmware ownership timeout");
                }
                return false;
            }
            if (ownershipCapabilityFound)
                KernelLogSink::print("[xHCI] USBLEGSUP handoff complete.\r\n");
        }

        // 3. Controller Stop & Reset
        uint32_t usbCmd = mmioRead32(m_opBase + 0x00);
        if ((usbCmd & 1) != 0) {
            mmioWrite32(m_opBase + 0x00, usbCmd & ~1U); // Stop
            micant::hal::amd64::BootstrapClockStatus clockStatus{};
            const bool halted = pollUntilDeadline(40, [&]() noexcept {
                return (mmioRead32(m_opBase + 0x04) & 1) != 0;
            }, clockStatus);
            if (!halted && clockStatus != micant::hal::amd64::BootstrapClockStatus::Success) {
                setStatus("xHCI: HAL clock unavailable during stop");
                return false;
            }
            if (!halted) {
                setStatus("xHCI: Stop timeout");
                return false;
            }
        }

        mmioWrite32(m_opBase + 0x00, 2); // Issue HCRST
        micant::hal::amd64::BootstrapClockStatus clockStatus{};
        const bool resetComplete = pollUntilDeadline(100, [&]() noexcept {
            const uint32_t cmdNow = mmioRead32(m_opBase + 0x00);
            const uint32_t stsNow = mmioRead32(m_opBase + 0x04);
            return (cmdNow & 2) == 0 && (stsNow & (1 << 11)) == 0;
        }, clockStatus);
        if (!resetComplete && clockStatus != micant::hal::amd64::BootstrapClockStatus::Success) {
            setStatus("xHCI: HAL clock unavailable during reset");
            return false;
        }
        if (!resetComplete) {
            setStatus("xHCI: Reset timeout");
            return false;
        }
        KernelLogSink::print("[xHCI] Host controller reset complete.\r\n");

        // HAL/Io owns command-register transitions after reset. This legacy
        // body is unreachable until resource and DMA services are integrated.

        // 4. Configure Controller (Scratchpad, DCBAA, Command Ring, Event Ring)
        uint32_t hcsparams2 = mmioRead32(m_mmioBase + 0x08);
        uint32_t maxScratchpad = (((hcsparams2 >> 21) & 0x1F) << 5) | ((hcsparams2 >> 27) & 0x1F);
        KernelLogSink::print("[xHCI] Scratchpad Buffers: ");
        KernelLogSink::printDec(maxScratchpad);
        KernelLogSink::print("\r\n");

        for (size_t i = 0; i < 64; ++i) s_dcbaa[i] = 0;

        if (maxScratchpad > 0) {
            uint32_t toAlloc = (maxScratchpad > 32) ? 32 : maxScratchpad;
            for (uint32_t i = 0; i < toAlloc; ++i) {
                s_scratchpadArray[i] = toPhys(&s_scratchpadPages[i][0]);
            }
            s_dcbaa[0] = toPhys(&s_scratchpadArray[0]);
        }

        mmioWrite64(m_opBase + 0x30, toPhys(&s_dcbaa[0]));

        uint32_t maxSlots = hcsparams1 & 0xFF;
        uint32_t slotsToEnable = (maxSlots < 4) ? maxSlots : 4;
        if (slotsToEnable == 0) slotsToEnable = 1;
        mmioWrite32(m_opBase + 0x38, slotsToEnable); // CONFIG: MaxSlotsEn

        for (size_t i = 0; i < 32; ++i) {
            s_cmdRing[i].param = 0;
            s_cmdRing[i].status = 0;
            s_cmdRing[i].control = 0;
        }
        s_cmdRing[31].param = toPhys(&s_cmdRing[0]);
        s_cmdRing[31].control = (TRB_LINK << 10) | (1 << 1); // Link TRB
        m_cmdEnqueue = 0;
        m_cmdCycle = 1;
        mmioWrite64(m_opBase + 0x18, toPhys(&s_cmdRing[0]) | 1); // CRCR with RCS=1

        for (size_t i = 0; i < 64; ++i) {
            s_evtRing[i].param = 0;
            s_evtRing[i].status = 0;
            s_evtRing[i].control = 0;
        }
        m_evtDequeue = 0;
        m_evtCycle = 1;

        s_erst[0].ringSegmentBaseAddress = toPhys(&s_evtRing[0]);
        s_erst[0].ringSegmentSize = 64;
        s_erst[0].reserved = 0;

        mmioWrite32(m_rtBase + 0x28, 1); // ERSTSZ = 1
        mmioWrite64(m_rtBase + 0x30, toPhys(&s_erst[0])); // ERSTBA
        mmioWrite64(m_rtBase + 0x38, toPhys(&s_evtRing[0]) | (1ULL << 3)); // ERDP with EHB=1
        mmioWrite32(m_rtBase + 0x20, 3); // IMAN: IE=1, IP=1

        // Start Controller
        mmioWrite32(m_opBase + 0x00, 1); // USBCMD: Run/Stop = 1
        micant::hal::amd64::BootstrapClockStatus clockStatus{};
        const bool controllerStarted = pollUntilDeadline(50, [&]() noexcept {
            return (mmioRead32(m_opBase + 0x04) & 1) == 0; // HCH == 0
        }, clockStatus);
        if (!controllerStarted && clockStatus != micant::hal::amd64::BootstrapClockStatus::Success) {
            setStatus("xHCI: HAL clock unavailable during start");
            return false;
        }
        if (!controllerStarted) {
            setStatus("xHCI: Start timeout");
            return false;
        }
        KernelLogSink::print("[xHCI] Controller started (USBCMD=1, USBSTS=0).\r\n");

        // 5. Enumerate and Power-On Root Hub Ports
        KernelLogSink::print("[xHCI] Powering on ");
        KernelLogSink::printDec(maxPorts);
        KernelLogSink::print(" root ports...\r\n");

        for (uint32_t p = 1; p <= maxPorts; ++p) {
            uint64_t pAddr = m_opBase + 0x400 + (static_cast<uint64_t>(p - 1) * 0x10);
            uint32_t sc = mmioRead32(pAddr);
            // Write 1 to Port Power (PP, bit 9) if not already set, preserving other control bits
            if ((sc & (1U << 9)) == 0) {
                mmioWrite32(pAddr, (sc & ~0x00FE1E00U) | (1U << 9));
            }
        }

        // Wait 200ms for power stabilization and device pull-up debounce
        if (waitForMilliseconds(200) != micant::hal::amd64::BootstrapClockStatus::Success) {
            setStatus("xHCI: HAL clock unavailable during port power-up");
            return false;
        }

        uint32_t targetPort = 0;
        uint64_t targetPortAddr = 0;
        uint32_t targetPortSC = 0;

        // Scan ports with one calibrated absolute 2-second deadline.
        const bool foundPort = pollUntilDeadline(2000, [&]() noexcept {
            for (uint32_t p = 1; p <= maxPorts; ++p) {
                uint64_t pAddr = m_opBase + 0x400 + (static_cast<uint64_t>(p - 1) * 0x10);
                uint32_t sc = mmioRead32(pAddr);
                if ((sc & 1) != 0) { // CCS == 1 (Device Connected)
                    targetPort = p;
                    targetPortAddr = pAddr;
                    targetPortSC = sc;
                    KernelLogSink::print("[xHCI] Found connected USB device on Port ");
                    KernelLogSink::printDec(p);
                    KernelLogSink::print(" (PORTSC=0x");
                    KernelLogSink::printHex(sc);
                    KernelLogSink::print(")\r\n");
                    break;
                }
            }
            return targetPort != 0;
        }, clockStatus);
        if (!foundPort && clockStatus != micant::hal::amd64::BootstrapClockStatus::Success) {
            setStatus("xHCI: HAL clock unavailable during port debounce");
            return false;
        }

        if (targetPort == 0) {
            KernelLogSink::print("[xHCI] Warning: No connected USB device found on any root port!\r\n");
            setStatus("xHCI: No Devices Connected");
            return false;
        }

        // Require the selected connection to survive the debounce interval
        // before issuing the port reset.
        if (waitForMilliseconds(50) != micant::hal::amd64::BootstrapClockStatus::Success) {
            setStatus("xHCI: HAL clock unavailable during port debounce");
            return false;
        }
        targetPortSC = mmioRead32(targetPortAddr);
        if ((targetPortSC & 1U) == 0) {
            setStatus("xHCI: Port disconnected during debounce");
            return false;
        }

        m_targetPort = targetPort;

        // Reset Target Port (Port Reset bit 4, Port Power bit 9)
        mmioWrite32(targetPortAddr, (targetPortSC & ~0x00FE1E00U) | (1 << 4) | (1 << 9));
        const bool portResetComplete = pollUntilDeadline(150, [&]() noexcept {
            uint32_t sc = mmioRead32(targetPortAddr);
            if ((sc & (1 << 4)) == 0 && (sc & (1 << 1)) != 0) {
                return true;
            }
            return false;
        }, clockStatus);
        if (!portResetComplete && clockStatus != micant::hal::amd64::BootstrapClockStatus::Success) {
            setStatus("xHCI: HAL clock unavailable during port reset");
            return false;
        }

        uint32_t finalPortSC = mmioRead32(targetPortAddr);
        if (!portResetComplete) {
            setStatus("xHCI: Port reset timeout");
            return false;
        }
        uint8_t speed = static_cast<uint8_t>((finalPortSC >> 10) & 0x0F);
        KernelLogSink::print("[xHCI] Port ");
        KernelLogSink::printDec(targetPort);
        KernelLogSink::print(" Reset Complete. Enabled=1, Speed=");
        KernelLogSink::printDec(speed);
        KernelLogSink::print(" (1=Full-Speed 12M)\r\n");

        // 6. Enable Slot
        KernelLogSink::print("[xHCI] Sending Enable Slot Command...\r\n");
        sendCommand(0, 0, TRB_ENABLE_SLOT);
        uint32_t slotId = 0;
        if (!waitForCmdComplete(slotId) || slotId == 0) {
            KernelLogSink::print("[xHCI] ERROR: Enable Slot failed!\r\n");
            setStatus("xHCI: Enable Slot Failed");
            return false;
        }
        m_slotId = slotId;
        KernelLogSink::print("[xHCI] Slot ");
        KernelLogSink::printDec(m_slotId);
        KernelLogSink::print(" assigned by xHCI.\r\n");

        // 7. Address Device (Slot 1)
        for (size_t i = 0; i < sizeof(s_inputContext) / 4; ++i) s_inputContext[i] = 0;
        for (size_t i = 0; i < sizeof(s_devContext) / 4; ++i) s_devContext[i] = 0;

        s_dcbaa[m_slotId] = toPhys(&s_devContext[0]);

        // Input Control Context (index 0): Add Slot Context (bit 0) + EP0 Context (bit 1)
        s_inputContext[0 * m_ctxDwords + 1] = (1U << 0) | (1U << 1);

        // Slot Context (index 1):
        // dword 0: speed (bits 20..23), context entries = 1 (bits 27..31)
        s_inputContext[1 * m_ctxDwords + 0] = (static_cast<uint32_t>(speed) << 20) | (1U << 27);
        // dword 1: root hub port number (bits 16..23)
        s_inputContext[1 * m_ctxDwords + 1] = (targetPort << 16);

        // Endpoint 0 Context (index 2 = DCI 1):
        for (size_t i = 0; i < 32; ++i) {
            s_ep0Ring[i].param = 0;
            s_ep0Ring[i].status = 0;
            s_ep0Ring[i].control = 0;
        }
        s_ep0Ring[31].param = toPhys(&s_ep0Ring[0]);
        s_ep0Ring[31].control = (TRB_LINK << 10) | (1 << 1);

        // EP0: epType = 4 (Control), maxPacketSize = 64 (or 8), errorCount = 3
        s_inputContext[2 * m_ctxDwords + 1] = (3U << 1) | (4U << 3) | (64U << 16);
        // dword 2/3: TR Dequeue Pointer
        s_inputContext[2 * m_ctxDwords + 2] = static_cast<uint32_t>(toPhys(&s_ep0Ring[0]) | 1);
        s_inputContext[2 * m_ctxDwords + 3] = static_cast<uint32_t>(toPhys(&s_ep0Ring[0]) >> 32);
        s_inputContext[2 * m_ctxDwords + 4] = 8; // Average TRB Length

        KernelLogSink::print("[xHCI] Addressing Device on Slot ");
        KernelLogSink::printDec(m_slotId);
        KernelLogSink::print("...\r\n");

        sendCommand(toPhys(&s_inputContext[0]), 0, TRB_ADDRESS_DEVICE, m_slotId);
        uint32_t dummySlot = 0;
        if (!waitForCmdComplete(dummySlot)) {
            KernelLogSink::print("[xHCI] ERROR: Address Device failed!\r\n");
            setStatus("xHCI: Address Dev Failed");
            return false;
        }
        KernelLogSink::print("[xHCI] Device successfully addressed!\r\n");

        // 8. Configure Endpoint 1 (Keyboard Interrupt IN, DCI 3)
        for (size_t i = 0; i < sizeof(s_inputContext) / 4; ++i) s_inputContext[i] = 0;

        // Input Control Context (index 0): Add Slot Context (bit 0) + EP1-IN Context (bit 3)
        s_inputContext[0 * m_ctxDwords + 1] = (1U << 0) | (1U << 3);

        // Slot Context (index 1): contextEntries = 3 (since highest active DCI is 3)
        s_inputContext[1 * m_ctxDwords + 0] = (static_cast<uint32_t>(speed) << 20) | (3U << 27);
        s_inputContext[1 * m_ctxDwords + 1] = (targetPort << 16);

        // Endpoint 1 IN Context (index 4 = DCI 3):
        // Note: Input Context index for DCI k is (k + 1) * m_ctxDwords!
        for (size_t i = 0; i < 32; ++i) {
            s_ep1Ring[i].param = 0;
            s_ep1Ring[i].status = 0;
            s_ep1Ring[i].control = 0;
        }
        s_ep1Ring[31].param = toPhys(&s_ep1Ring[0]);
        s_ep1Ring[31].control = (TRB_LINK << 10) | (1 << 1);
        m_ep1Enqueue = 0;
        m_ep1Cycle = 1;

        // epType = 7 (Interrupt IN), interval = 7 (16ms for Full-Speed), maxPacketSize = 8, errorCount = 3
        s_inputContext[4 * m_ctxDwords + 0] = (7U << 16); // Interval = 7
        s_inputContext[4 * m_ctxDwords + 1] = (3U << 1) | (7U << 3) | (8U << 16);
        s_inputContext[4 * m_ctxDwords + 2] = static_cast<uint32_t>(toPhys(&s_ep1Ring[0]) | 1);
        s_inputContext[4 * m_ctxDwords + 3] = static_cast<uint32_t>(toPhys(&s_ep1Ring[0]) >> 32);
        s_inputContext[4 * m_ctxDwords + 4] = 8 | (8U << 16); // Average TRB Length=8, Max ESIT Payload=8

        KernelLogSink::print("[xHCI] Configuring Endpoint 1 (Keyboard Interrupt IN, DCI 3, Index 4)...\r\n");
        sendCommand(toPhys(&s_inputContext[0]), 0, TRB_CONFIGURE_ENDPOINT, m_slotId);
        if (!waitForCmdComplete(dummySlot)) {
            KernelLogSink::print("[xHCI] ERROR: Configure Endpoint failed!\r\n");
            setStatus("xHCI: Cfg Ep Failed");
            return false;
        }
        KernelLogSink::print("[xHCI] Endpoint 1 (DCI 3) successfully configured!\r\n");

        // 8b. Configure USB Device via Control Transfers on EP0
        m_ep0Enqueue = 0;
        m_ep0Cycle = 1;

        KernelLogSink::print("[xHCI] Sending USB SET_CONFIGURATION(1) to device...\r\n");
        if (sendControlNoData(0x00, 0x09, 1, 0)) {
            KernelLogSink::print("[xHCI] USB SET_CONFIGURATION(1) succeeded!\r\n");
        } else {
            KernelLogSink::print("[xHCI] Note: SET_CONFIGURATION returned non-zero/timed out, continuing...\r\n");
        }

        // Set Boot Protocol (Protocol 0)
        KernelLogSink::print("[xHCI] Sending USB SET_PROTOCOL(0) (Boot Protocol)...\r\n");
        sendControlNoData(0x21, 0x0B, 0, 0);

        // Set Idle Rate (0 = report only on change)
        KernelLogSink::print("[xHCI] Sending USB SET_IDLE(0, 0)...\r\n");
        sendControlNoData(0x21, 0x0A, 0, 0);

        // 9. Initial Queue of Keyboard Transfer TRB
        for (size_t i = 0; i < 8; ++i) s_kbdReport[i] = 0;
        queueKeyboardTransfer();

        m_initialized = true;
        m_keyboardReady = true;
        char statusBuf[64];
        statusBuf[0] = 'U'; statusBuf[1] = 'S'; statusBuf[2] = 'B'; statusBuf[3] = ':';
        statusBuf[4] = ' '; statusBuf[5] = 'A'; statusBuf[6] = 'M'; statusBuf[7] = 'I';
        statusBuf[8] = ' '; statusBuf[9] = 'K'; statusBuf[10] = 'b'; statusBuf[11] = 'd';
        statusBuf[12] = ' '; statusBuf[13] = 'R'; statusBuf[14] = 'e'; statusBuf[15] = 'a';
        statusBuf[16] = 'd'; statusBuf[17] = 'y'; statusBuf[18] = ' '; statusBuf[19] = '(';
        statusBuf[20] = 'P'; statusBuf[21] = 'o'; statusBuf[22] = 'r'; statusBuf[23] = 't';
        statusBuf[24] = ' ';
        size_t sl = 25;
        if (m_targetPort >= 10) statusBuf[sl++] = '0' + static_cast<char>(m_targetPort / 10);
        statusBuf[sl++] = '0' + static_cast<char>(m_targetPort % 10);
        statusBuf[sl++] = ')';
        statusBuf[sl] = '\0';
        setStatus(statusBuf);

        KernelLogSink::print("[xHCI] *** BARE-METAL USB KEYBOARD PIPELINE ACTIVE & LISTENING ***\r\n");
        return true;
    }

    [[nodiscard]] bool isKeyboardReady() const noexcept {
        return m_keyboardReady;
    }

    [[nodiscard]] uint32_t getTransferCount() const noexcept {
        return m_transferEventsCount;
    }

    // Non-blocking keyboard transport contract for the canonical USB/HID consumer.
    // This deliberately does not translate boot-report bytes into key events:
    // the caller must parse the real keyboard HID descriptor and invoke
    // HIDClass/KbdHid. This prototype does not configure a mouse endpoint.
    // STATUS_PENDING means no matching transfer is complete.
    [[nodiscard]] micant::NtStatus tryDequeueKeyboardReport(
        uint8_t* report, size_t reportCapacity, XhciHidReportCompletion& completion) noexcept {
        completion = {};
        if (!report || reportCapacity == 0)
            return micant::NtStatus::InvalidParameter;
        if (!m_initialized || !m_keyboardReady)
            return micant::NtStatus::DeviceNotReady;
        if (reportCapacity < sizeof(s_kbdReport))
            return micant::NtStatus::BufferTooSmall;

        // Consume unrelated controller events without claiming them as input.
        for (size_t scanned = 0; scanned < 64; ++scanned) {
            volatile XhciTrb* trb = &s_evtRing[m_evtDequeue];
            const uint32_t control = trb->control;
            if ((control & 1) != m_evtCycle) return micant::NtStatus::Pending;

            const uint32_t type = (control >> 10) & 0x3F;
            const uint32_t slot = (control >> 24) & 0xFF;
            const uint32_t endpoint = (control >> 16) & 0x1F;
            const uint32_t status = trb->status;
            const uint32_t completionCode = (status >> 24) & 0xFF;
            const uint32_t residual = status & 0x00FFFFFF;

            m_evtDequeue = (m_evtDequeue + 1) % 64;
            if (m_evtDequeue == 0) m_evtCycle ^= 1;
            mmioWrite64(m_rtBase + 0x38, toPhys(&s_evtRing[m_evtDequeue]) | (1ULL << 3));

            if (type != TRB_TRANSFER_EVENT || slot != m_slotId || endpoint != 3)
                continue;

            ++m_transferEventsCount;
            completion.slotId = static_cast<uint8_t>(slot);
            completion.endpointId = static_cast<uint8_t>(endpoint);
            completion.completionCode = static_cast<uint8_t>(completionCode);

            // A transfer event reports residual bytes, not bytes completed.
            // Success and Short Packet are the only report-bearing completions.
            if ((completionCode != 1 && completionCode != 13) || residual > sizeof(s_kbdReport))
                return micant::NtStatus::Unsuccessful;
            const size_t transferred = sizeof(s_kbdReport) - residual;
            if (transferred == 0) return micant::NtStatus::Unsuccessful;

#if defined(__x86_64__)
            __asm__ __volatile__("mfence" ::: "memory");
#endif
            for (size_t i = 0; i < transferred; ++i)
                report[i] = s_kbdReport[i];
            completion.bytesTransferred = transferred;

            // Re-arm only a report-bearing completion. Error/stall paths need
            // endpoint recovery by the eventual USBD owner before resubmission.
            queueKeyboardTransfer();
            return micant::NtStatus::Success;
        }
        return micant::NtStatus::Pending;
    }
};

} // namespace micant::km::xhci_baremetal
