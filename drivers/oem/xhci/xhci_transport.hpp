#pragma once

#include "micant/shared/boot_driver.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>

// OEM-only xHCI register and ring transport. The USB bus owner supplies and
// consumes commands and events; this object never interprets HID reports.
namespace micant::oem::xhci {
struct Trb { uint64_t parameter; uint32_t status, control; };
struct ErstEntry { uint64_t address; uint32_t size, reserved; };
struct alignas(4096) DeviceDma {
    alignas(64) uint32_t inputContext[33 * 16];
    alignas(64) uint32_t deviceContext[32 * 16];
    alignas(64) Trb controlRing[64];
    alignas(64) uint8_t controlBuffer[4096];
    alignas(64) Trb interruptRing[64];
    alignas(64) uint8_t interruptBuffer[64];
    alignas(64) Trb bulkInRing[64];
    alignas(64) Trb bulkOutRing[64];
    alignas(4096) uint8_t bulkBuffer[4096];
};
struct alignas(4096) DmaState {
    alignas(64) uint64_t dcbaa[256];
    alignas(64) uint64_t scratchArray[32];
    alignas(64) Trb command[64];
    alignas(64) Trb event[64];
    alignas(64) ErstEntry erst[1];
    alignas(4096) uint8_t scratchPages[32][4096];
    DeviceDma devices[4];
};
static_assert(sizeof(Trb) == 16);

class Transport {
    const boot_driver::KernelServices* services_{};
    uint16_t bus_{}, device_{}, function_{}, originalCommand_{};
    uint64_t claim_{}, dmaToken_{}, dmaAddress_{};
    volatile uint8_t* registers_{};
    size_t registerLength_{};
    uint32_t operational_{}, runtime_{}, doorbells_{};
    uint32_t legacyOffset_{};
    DmaState* dma_{};
    bool commandEnabled_{}, running_{};
    bool acquiredOwnership_{};
    bool commandPending_{};
    uint64_t pendingCommandAddress_{};
    uint8_t commandIndex_{}, commandCycle_{1};
    uint8_t eventCycle_{1};
    uint16_t eventIndex_{};
    uint8_t maxPorts_{}, maxSlots_{};
    uint8_t portProtocol_[256]{}, portSlotType_[256]{};
    uint8_t contextWords_{8};
    struct SlotState {
        DeviceDma* memory{};
        uint64_t dmaAddress{};
        uint8_t controlIndex{}, controlCycle{1}, controlSlot{};
        bool controlPending{};
        uint64_t controlStatusAddress{};
        uint8_t interruptDci{}, interruptIndex{}, interruptCycle{1};
        uint16_t interruptLength{};
        bool interruptPending{};
        uint64_t interruptAddress{};
        uint8_t bulkInAddress{}, bulkOutAddress{}, bulkIndex[2]{}, bulkCycle[2]{1, 1};
        bool bulkFault{};
    } slots_[4]{};
    SlotState* active_{};
    Trb deferredEvents_[64]{};
    uint8_t deferredHead_{}, deferredCount_{};
    bool eventOverflow_{};
    bool deferEvent(const Trb& event) noexcept {
        if (deferredCount_ == 64) { eventOverflow_ = true; return false; }
        deferredEvents_[(deferredHead_ + deferredCount_) % 64] = event;
        ++deferredCount_;
        return true;
    }

    uint32_t read(size_t offset) const noexcept {
        return *reinterpret_cast<volatile uint32_t*>(registers_ + offset);
    }
    void write(size_t offset, uint32_t value) noexcept {
        *reinterpret_cast<volatile uint32_t*>(registers_ + offset) = value;
    }
    void write64(size_t offset, uint64_t value) noexcept {
        write(offset, static_cast<uint32_t>(value));
        write(offset + 4, static_cast<uint32_t>(value >> 32));
    }
    bool within(uint64_t offset, uint64_t size) const noexcept {
        return offset <= registerLength_ && size <= registerLength_ - offset;
    }
    bool pciRead(uint16_t offset, uint32_t& value) const noexcept {
        return services_->PciReadConfig32(services_->Context, bus_, device_, function_, offset, &value) ==
            boot_driver::STATUS_SUCCESS;
    }
    bool pciWriteCommand(uint16_t value) noexcept {
        return services_->PciWriteConfig32(services_->Context, bus_, device_, function_, 4, value) ==
            boot_driver::STATUS_SUCCESS;
    }
    template<typename Predicate> bool wait(Predicate predicate, uint32_t timeoutMs) noexcept {
        uint64_t start{}, frequency{};
        if (services_->QueryMonotonicClock(services_->Context, &start, &frequency) != boot_driver::STATUS_SUCCESS ||
            !frequency || frequency / 1000 > UINT64_MAX / timeoutMs) return false;
        const uint64_t duration = (frequency / 1000) * timeoutMs;
        if (!duration) return false;
        for (unsigned n = 0; n < 100000; ++n) {
            if (predicate()) return true;
            uint64_t now{}, current{};
            if (services_->QueryMonotonicClock(services_->Context, &now, &current) != boot_driver::STATUS_SUCCESS ||
                current != frequency || now < start || now - start >= duration ||
                services_->DelayMicroseconds(services_->Context, 50) != boot_driver::STATUS_SUCCESS) return false;
        }
        return false;
    }
    bool waitTransactions() noexcept {
        uint32_t status{}, caps{};
        if (!pciRead(4, status) || !(status & (1u << 20)) || !pciRead(0x34, caps)) return false;
        uint8_t pointer = static_cast<uint8_t>(caps);
        for (unsigned i = 0; pointer && i < 48; ++i) {
            uint32_t cap{};
            if (pointer < 0x40 || pointer > 0xf4 || (pointer & 3) || !pciRead(pointer, cap)) return false;
            if ((cap & 0xff) == 0x10)
                return wait([&]() noexcept { uint32_t v{}; return pciRead(pointer + 8, v) && !(v & (1u << 21)); }, 100);
            pointer = static_cast<uint8_t>(cap >> 8);
        }
        return false;
    }

public:
    // Resource ownership remains with this object until stop succeeds. The
    // caller must stop all USB submissions before invoking stop.
    bool initialize(const boot_driver::KernelServices& s, uint16_t bus, uint16_t device,
                    uint16_t function, uint64_t claim, uint64_t physical, size_t length) noexcept {
        if (services_ || !claim || !physical || length < 0x1000 ||
            !s.MapDeviceMemory || !s.ReleaseDeviceMemory || !s.AllocateDmaBuffer ||
            !s.FreeDmaBuffer || !s.QueryMonotonicClock || !s.DelayMicroseconds ||
            !s.PciReadConfig32 || !s.PciWriteConfig32 || !s.ReleasePciDevice) return false;
        services_ = &s; bus_ = bus; device_ = device; function_ = function;
        claim_ = claim; registerLength_ = length;
        registers_ = static_cast<volatile uint8_t*>(s.MapDeviceMemory(s.Context, physical, length));
        if (!registers_) return false;
        void* memory{};
        if (s.AllocateDmaBuffer(s.Context, claim_, sizeof(DmaState), alignof(DmaState), UINT64_MAX,
                                &memory, &dmaAddress_, &dmaToken_) != boot_driver::STATUS_SUCCESS ||
            !memory || !dmaToken_ || !dmaAddress_ || dmaAddress_ > UINT64_MAX - sizeof(DmaState)) return false;
        dma_ = static_cast<DmaState*>(memory);
        uint32_t command{};
        if (!pciRead(4, command) || (command & 7u)) return false;
        originalCommand_ = static_cast<uint16_t>(command);
        if (!pciWriteCommand(originalCommand_ | 6u)) return false;
        commandEnabled_ = true;
        const uint8_t capLength = *registers_;
        if (capLength < 0x20 || (capLength & 3) || !within(capLength, 0x40)) return false;
        operational_ = capLength;
        const uint32_t hcs1 = read(4), hcs2 = read(8), hcc1 = read(0x10);
        contextWords_ = (hcc1 & (1u << 2)) ? 16 : 8;
        runtime_ = read(0x18) & ~uint32_t{31};
        doorbells_ = read(0x14) & ~uint32_t{3};
        if (!within(runtime_, 0x40) || !within(doorbells_, 4) || !(hcs1 & 0xff) ||
            !(read(operational_ + 8) & 1u)) return false;
        maxSlots_ = static_cast<uint8_t>(hcs1);
        maxPorts_ = static_cast<uint8_t>(hcs1 >> 24);
        if (!maxPorts_ || !within(uint64_t{operational_} + 0x400, uint64_t{maxPorts_} * 16)) return false;
        uint32_t capability = (hcc1 >> 16) * 4u;
        for (unsigned visited = 0; capability && visited < 64; ++visited) {
            if (!within(capability, 4)) return false;
            const uint32_t value = read(capability);
            if ((value & 0xff) == 1) {
                legacyOffset_ = capability;
                acquiredOwnership_ = (value & (1u << 24)) == 0;
                write(capability, value | (1u << 24));
                if (!wait([&]() noexcept { return !(read(capability) & (1u << 16)); }, 1000)) return false;
            } else if ((value & 0xff) == 2) {
                if (!within(capability, 16) || read(capability + 4) != 0x20425355u) return false;
                const uint8_t major = static_cast<uint8_t>(value >> 24);
                const uint32_t ports = read(capability + 8);
                const uint8_t first = static_cast<uint8_t>(ports), count = static_cast<uint8_t>(ports >> 8);
                const uint8_t speedCount = static_cast<uint8_t>(ports >> 28);
                const uint8_t slotType = static_cast<uint8_t>(read(capability + 12) & 31u);
                if ((major != 2 && major != 3) || !first || !count ||
                    unsigned{first} + count - 1 > maxPorts_ ||
                    !within(uint64_t{capability} + 16, uint64_t{speedCount} * 4)) return false;
                for (unsigned p = first; p < unsigned{first} + count; ++p) {
                    if (portProtocol_[p]) return false;
                    // Custom PSI encodings are not yet interpreted.
                    portProtocol_[p] = speedCount ? 0xff : major;
                    portSlotType_[p] = slotType;
                }
            }
            const uint32_t next = (value >> 8) & 0xff;
            if (!next) break;
            if (capability > UINT32_MAX - next * 4u) return false;
            capability += next * 4u;
            if (visited == 63) return false;
        }
        write(operational_, read(operational_) & ~1u);
        if (!wait([&]() noexcept { return (read(operational_ + 4) & 1u) != 0; }, 100)) return false;
        write(operational_, read(operational_) | 2u);
        if (!wait([&]() noexcept { return !(read(operational_) & 2u) && !(read(operational_ + 4) & (1u << 11)); }, 1000)) return false;
        const uint32_t scratch = (((hcs2 >> 21) & 31u) << 5) | ((hcs2 >> 27) & 31u);
        if (scratch > 32) return false;
        for (uint32_t i = 0; i < scratch; ++i)
            dma_->scratchArray[i] = dmaAddress_ + offsetof(DmaState, scratchPages) + uint64_t{i} * 4096;
        if (scratch) dma_->dcbaa[0] = dmaAddress_ + offsetof(DmaState, scratchArray);
        dma_->command[63] = {dmaAddress_ + offsetof(DmaState, command), 0, (6u << 10) | 3u};
        dma_->erst[0] = {dmaAddress_ + offsetof(DmaState, event), 64, 0};
        __atomic_thread_fence(__ATOMIC_RELEASE);
        write64(operational_ + 0x30, dmaAddress_ + offsetof(DmaState, dcbaa));
        write64(operational_ + 0x18, (dmaAddress_ + offsetof(DmaState, command)) | 1u);
        write(operational_ + 0x38, hcs1 & 0xff);
        write(runtime_ + 0x20, 0); // IMAN: no interrupt route installed
        write(runtime_ + 0x28, 1);
        write64(runtime_ + 0x30, dmaAddress_ + offsetof(DmaState, erst));
        write64(runtime_ + 0x38, dmaAddress_ + offsetof(DmaState, event));
        return true; // USB owner must attach before starting the controller.
    }

    bool start() noexcept {
        if (!services_ || !dma_ || !commandEnabled_ || running_) return false;
        write(operational_, read(operational_) | 1u);
        if (!wait([&]() noexcept { return !(read(operational_ + 4) & 1u); }, 100)) return false;
        running_ = true;
        return true;
    }

    uint8_t portCount() const noexcept { return maxPorts_; }
    uint8_t portSlotType(uint8_t port) const noexcept {
        return port && port <= maxPorts_ ? portSlotType_[port] : 0;
    }
    bool selectDevice(uint8_t slot) noexcept {
        for (auto& candidate : slots_) if (candidate.controlSlot == slot && slot) {
            active_ = &candidate;
            return true;
        }
        return false;
    }
    bool forgetDevice(uint8_t slot) noexcept {
        for (auto& candidate : slots_) if (candidate.controlSlot == slot && slot) {
            if (candidate.controlPending || candidate.interruptPending) return false;
            dma_->dcbaa[slot] = 0;
            __atomic_thread_fence(__ATOMIC_RELEASE);
            if (active_ == &candidate) active_ = nullptr;
            candidate = {};
            return true;
        }
        // An Enable Slot command may have completed after disconnect, before
        // any per-slot DMA context was prepared. Disable Slot completion is
        // the caller's precondition for this no-context case.
        return slot && slot <= maxSlots_;
    }
    bool readPort(uint8_t port, uint32_t& status) const noexcept {
        if (!running_ || !port || port > maxPorts_) return false;
        status = read(operational_ + 0x400 + (port - 1) * 16);
        return true;
    }
    bool resetPort(uint8_t port, uint8_t& speed) noexcept {
        uint32_t status{};
        if (!readPort(port, status) || !(status & 1u)) return false;
        const uint32_t currentSpeed = (status >> 10) & 15u;
        const bool super = portProtocol_[port] == 3;
        if (portProtocol_[port] == 0xff || (super ? (currentSpeed < 4 || currentSpeed > 5)
                                                       : (currentSpeed < 1 || currentSpeed > 3))) return false;
        const uint32_t offset = operational_ + 0x400 + (port - 1) * 16;
        // PORTSC change bits and PED are write-one-to-clear: write only PP
        // and PR, preserving port power without acknowledging notifications.
        const uint32_t reset = super ? (1u << 31) : (1u << 4);
        write(offset, (status & (1u << 9)) | reset);
        if (!wait([&]() noexcept {
            const auto value = read(offset);
            return !(value & reset) && (value & 1u) && (value & 2u);
        }, 200)) return false;
        status = read(offset);
        speed = static_cast<uint8_t>((status >> 10) & 15u);
        return super ? speed >= 4 && speed <= 5 : speed >= 1 && speed <= 3;
    }
    bool setDeviceContext(uint8_t slot, uint64_t deviceContext) noexcept {
        if (!running_ || !dma_ || !slot || slot > maxSlots_ ||
            !deviceContext || (deviceContext & 63u)) return false;
        dma_->dcbaa[slot] = deviceContext;
        __atomic_thread_fence(__ATOMIC_RELEASE);
        return true;
    }

    // Prepare the input/device contexts and EP0 ring for a single USB 2
    // root-port device. The caller submits the returned Address Device TRB.
    bool prepareAddressDevice(uint8_t slot, uint8_t port, uint8_t speed, Trb& command,
                              uint8_t hubSlot = 0, uint8_t hubPort = 0, bool multiTt = false) noexcept {
        if (!running_ || !dma_ || !slot || slot > maxSlots_ || !port || port > maxPorts_ ||
            speed < 1 || speed > 5 || (hubSlot && (!hubPort || hubPort > 15 || speed > 3)) ||
            (!hubSlot && (hubPort || multiTt)) || !within(uint64_t{doorbells_} + slot * 4u, 4))
            return false;
        if (selectDevice(slot)) return false;
        active_ = nullptr;
        for (size_t i = 0; i < 4; ++i) if (!slots_[i].controlSlot) {
            slots_[i] = {};
            slots_[i].memory = &dma_->devices[i];
            slots_[i].dmaAddress = dmaAddress_ + offsetof(DmaState, devices) + i * sizeof(DeviceDma);
            active_ = &slots_[i];
            break;
        }
        if (!active_) return false;
        std::memset(active_->memory->inputContext, 0, sizeof(active_->memory->inputContext));
        std::memset(active_->memory->deviceContext, 0, sizeof(active_->memory->deviceContext));
        std::memset(active_->memory->controlRing, 0, sizeof(active_->memory->controlRing));
        active_->controlIndex = 0; active_->controlCycle = 1; active_->controlSlot = slot;
        const uint64_t epAddress = active_->dmaAddress + offsetof(DeviceDma, controlRing);
        active_->memory->controlRing[63] = {epAddress, 0, (6u << 10) | 3u};
        active_->memory->inputContext[1] = 3; // Add slot and endpoint zero.
        auto* slotContext = active_->memory->inputContext + contextWords_;
        slotContext[0] = (uint32_t{speed} << 20) | (1u << 27) | uint32_t{hubPort};
        slotContext[1] = uint32_t{port} << 16;
        if (hubSlot && speed != 3) {
            slotContext[0] |= multiTt ? 1u << 25 : 0;
            slotContext[2] = uint32_t{hubSlot} | (uint32_t{hubPort} << 8);
        }
        auto* endpoint = active_->memory->inputContext + contextWords_ * 2;
        const uint32_t packet = speed >= 4 ? 512u : speed == 3 ? 64u : 8u;
        endpoint[1] = (3u << 1) | (4u << 3) | (packet << 16);
        endpoint[2] = static_cast<uint32_t>(epAddress | 1u);
        endpoint[3] = static_cast<uint32_t>(epAddress >> 32);
        endpoint[4] = 8;
        if (!setDeviceContext(slot, active_->dmaAddress + offsetof(DeviceDma, deviceContext))) return false;
        command = {active_->dmaAddress + offsetof(DeviceDma, inputContext), 0,
                   (11u << 10) | (uint32_t{slot} << 24)};
        return true;
    }

    // Issue a standard EP0 setup, optionally receiving up to one page. The
    // buffer is owned by the transport until the matching transfer event.
    bool submitControl(uint64_t setupPacket, uint16_t length, bool input, uint64_t& statusAddress) noexcept {
        if (!running_ || !active_ || !active_->controlSlot || active_->controlPending || length > sizeof(active_->memory->controlBuffer) ||
            (setupPacket >> 48) != length || (length && (!input || !(setupPacket & 0x80u)))) return false;
        if (active_->controlIndex > 60) {
            auto& link = active_->memory->controlRing[active_->controlIndex];
            link.parameter = active_->dmaAddress + offsetof(DeviceDma, controlRing);
            link.status = 0;
            __atomic_thread_fence(__ATOMIC_RELEASE);
            link.control = (6u << 10) | 2u | active_->controlCycle;
            active_->controlIndex = 0;
            active_->controlCycle ^= 1;
        }
        auto push = [&](uint64_t parameter, uint32_t status, uint32_t control) noexcept {
            auto& trb = active_->memory->controlRing[active_->controlIndex];
            trb.parameter = parameter; trb.status = status;
            __atomic_thread_fence(__ATOMIC_RELEASE);
            trb.control = control | active_->controlCycle;
            const uint64_t address = active_->dmaAddress + offsetof(DeviceDma, controlRing) + active_->controlIndex * sizeof(Trb);
            if (++active_->controlIndex == 63) {
                auto& link = active_->memory->controlRing[63];
                link.parameter = active_->dmaAddress + offsetof(DeviceDma, controlRing); link.status = 0;
                __atomic_thread_fence(__ATOMIC_RELEASE);
                link.control = (6u << 10) | 2u | active_->controlCycle;
                active_->controlIndex = 0; active_->controlCycle ^= 1;
            }
            return address;
        };
        if (length) std::memset(active_->memory->controlBuffer, 0, length);
        (void)push(setupPacket, 8, (2u << 10) | (1u << 6) | (length ? (3u << 16) : 0));
        if (length) (void)push(active_->dmaAddress + offsetof(DeviceDma, controlBuffer), length,
                               (3u << 10) | (1u << 16));
        active_->controlStatusAddress = push(0, 0, (4u << 10) | (1u << 5) | (length ? 0 : (1u << 16)));
        statusAddress = active_->controlStatusAddress;
        active_->controlPending = true;
        __atomic_thread_fence(__ATOMIC_SEQ_CST);
        write(doorbells_ + active_->controlSlot * 4u, 1); // DCI 1: EP0.
        return true;
    }
    const uint8_t* controlBuffer() const noexcept { return dma_ && active_ ? active_->memory->controlBuffer : nullptr; }

    bool prepareInterruptEndpoint(uint8_t endpointAddress, uint16_t packetSize,
                                  uint8_t interval, uint8_t speed, Trb& command,
                                  uint8_t maxBurst = 0, uint16_t bytesPerInterval = 0) noexcept {
        const uint8_t endpoint = endpointAddress & 15u;
        if (!running_ || !active_ || !active_->controlSlot || !(endpointAddress & 0x80) || (endpointAddress & 0x70) ||
            !endpoint || !packetSize || packetSize > sizeof(active_->memory->interruptBuffer) || !interval ||
            active_->interruptDci || speed < 1 || speed > 5 || maxBurst > 15 ||
            (speed < 4 && (maxBurst || bytesPerInterval)) ||
            (speed >= 4 && (!bytesPerInterval || bytesPerInterval > packetSize || maxBurst))) return false;
        const uint8_t dci = static_cast<uint8_t>(endpoint * 2 + 1);
        uint8_t exponent{};
        if (speed >= 3) {
            if (interval > 16) return false;
            exponent = static_cast<uint8_t>(interval - 1);
        } else {
            if ((speed == 2 && packetSize > 8) || packetSize > 64) return false;
            unsigned period = 1;
            while (period < interval) { period <<= 1; ++exponent; }
            exponent = static_cast<uint8_t>(exponent + 3);
        }
        std::memset(active_->memory->inputContext, 0, sizeof(active_->memory->inputContext));
        std::memset(active_->memory->interruptRing, 0, sizeof(active_->memory->interruptRing));
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        auto* slot = active_->memory->inputContext + contextWords_;
        for (size_t i = 0; i < contextWords_; ++i) slot[i] = active_->memory->deviceContext[i];
        slot[0] = (slot[0] & ~(31u << 27)) | (uint32_t{dci} << 27);
        active_->memory->inputContext[1] = 1u | (1u << dci);
        auto* ep = active_->memory->inputContext + (dci + 1) * contextWords_;
        const uint64_t ring = active_->dmaAddress + offsetof(DeviceDma, interruptRing);
        ep[0] = uint32_t{exponent} << 16;
        ep[1] = (3u << 1) | (7u << 3) | (uint32_t{maxBurst} << 8) | (uint32_t{packetSize} << 16);
        ep[2] = static_cast<uint32_t>(ring | 1u); ep[3] = static_cast<uint32_t>(ring >> 32);
        ep[4] = packetSize | (uint32_t{speed >= 4 ? bytesPerInterval : packetSize} << 16);
        active_->memory->interruptRing[63] = {ring, 0, (6u << 10) | 3u};
        active_->interruptDci = dci; active_->interruptLength = packetSize;
        active_->interruptIndex = 0; active_->interruptCycle = 1;
        __atomic_thread_fence(__ATOMIC_RELEASE);
        command = {active_->dmaAddress + offsetof(DeviceDma, inputContext), 0,
                   (12u << 10) | (uint32_t{active_->controlSlot} << 24)};
        return true;
    }
    bool prepareHubEndpoint(uint8_t endpoint, uint16_t packet, uint8_t interval, uint8_t speed,
                            uint8_t ports, uint8_t thinkTime, bool multiTt, Trb& command) noexcept {
        if (!ports || ports > 15 || thinkTime > 3 || speed != 3) return false;
        if (!prepareInterruptEndpoint(endpoint, packet, interval, speed, command)) return false;
        auto* slot = active_->memory->inputContext + contextWords_;
        slot[0] |= 1u << 26; // Hub
        if (multiTt) slot[0] |= 1u << 25;
        slot[1] = (slot[1] & 0x00ffffffu) | (uint32_t{ports} << 24);
        slot[2] = (slot[2] & ~(3u << 16)) | (uint32_t{thinkTime} << 16);
        __atomic_thread_fence(__ATOMIC_RELEASE);
        return true;
    }
    bool submitInterrupt(uint64_t& transferAddress) noexcept {
        if (!running_ || !active_ || !active_->interruptDci || active_->interruptPending) return false;
        std::memset(active_->memory->interruptBuffer, 0, sizeof(active_->memory->interruptBuffer));
        auto& trb = active_->memory->interruptRing[active_->interruptIndex];
        trb.parameter = active_->dmaAddress + offsetof(DeviceDma, interruptBuffer);
        trb.status = active_->interruptLength;
        __atomic_thread_fence(__ATOMIC_RELEASE);
        trb.control = (1u << 10) | (1u << 5) | (1u << 2) | active_->interruptCycle;
        active_->interruptAddress = active_->dmaAddress + offsetof(DeviceDma, interruptRing) + active_->interruptIndex * sizeof(Trb);
        transferAddress = active_->interruptAddress;
        active_->interruptPending = true;
        if (++active_->interruptIndex == 63) {
            auto& link = active_->memory->interruptRing[63];
            link.parameter = active_->dmaAddress + offsetof(DeviceDma, interruptRing); link.status = 0;
            __atomic_thread_fence(__ATOMIC_RELEASE);
            link.control = (6u << 10) | 2u | active_->interruptCycle;
            active_->interruptIndex = 0; active_->interruptCycle ^= 1;
        }
        __atomic_thread_fence(__ATOMIC_SEQ_CST);
        write(doorbells_ + active_->controlSlot * 4u, active_->interruptDci);
        return true;
    }
    const uint8_t* interruptBuffer() const noexcept { return dma_ && active_ ? active_->memory->interruptBuffer : nullptr; }

    bool prepareBulkEndpoints(uint8_t input, uint16_t inputPacket, uint8_t output,
                              uint16_t outputPacket, uint8_t speed, Trb& command,
                              uint8_t inputBurst = 0, uint8_t outputBurst = 0) noexcept {
        const uint8_t inNumber = input & 15u, outNumber = output & 15u;
        if (!running_ || !active_ || !active_->controlSlot || active_->bulkInAddress || !(input & 0x80) || (input & 0x70) ||
            (output & 0xf0) || !inNumber || !outNumber || !inputPacket || !outputPacket ||
            (speed != 1 && speed != 3 && speed != 4 && speed != 5) ||
            inputBurst > 15 || outputBurst > 15 || (speed < 4 && (inputBurst || outputBurst)) ||
            inputPacket > (speed >= 4 ? 1024 : speed == 3 ? 512 : 64) ||
            outputPacket > (speed >= 4 ? 1024 : speed == 3 ? 512 : 64)) return false;
        const uint8_t inDci = static_cast<uint8_t>(inNumber * 2 + 1), outDci = static_cast<uint8_t>(outNumber * 2);
        std::memset(active_->memory->inputContext, 0, sizeof(active_->memory->inputContext));
        std::memset(active_->memory->bulkInRing, 0, sizeof(active_->memory->bulkInRing));
        std::memset(active_->memory->bulkOutRing, 0, sizeof(active_->memory->bulkOutRing));
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        auto* slot = active_->memory->inputContext + contextWords_;
        for (size_t i = 0; i < contextWords_; ++i) slot[i] = active_->memory->deviceContext[i];
        const uint8_t lastDci = inDci > outDci ? inDci : outDci;
        slot[0] = (slot[0] & ~(31u << 27)) | (uint32_t{lastDci} << 27);
        active_->memory->inputContext[1] = 1u | (1u << inDci) | (1u << outDci);
        auto configure = [&](uint8_t dci, uint32_t type, uint16_t packet, uint8_t burst, uint64_t ring) noexcept {
            auto* ep = active_->memory->inputContext + (dci + 1) * contextWords_;
            ep[1] = (3u << 1) | (type << 3) | (uint32_t{burst} << 8) | (uint32_t{packet} << 16);
            ep[2] = static_cast<uint32_t>(ring | 1u); ep[3] = static_cast<uint32_t>(ring >> 32);
            ep[4] = packet;
        };
        const uint64_t inRing = active_->dmaAddress + offsetof(DeviceDma, bulkInRing);
        const uint64_t outRing = active_->dmaAddress + offsetof(DeviceDma, bulkOutRing);
        configure(inDci, 6, inputPacket, inputBurst, inRing); configure(outDci, 2, outputPacket, outputBurst, outRing);
        active_->memory->bulkInRing[63] = {inRing, 0, (6u << 10) | 3u};
        active_->memory->bulkOutRing[63] = {outRing, 0, (6u << 10) | 3u};
        active_->bulkInAddress = input; active_->bulkOutAddress = output; active_->bulkIndex[0] = active_->bulkIndex[1] = 0;
        active_->bulkCycle[0] = active_->bulkCycle[1] = 1;
        __atomic_thread_fence(__ATOMIC_RELEASE);
        command = {active_->dmaAddress + offsetof(DeviceDma, inputContext), 0,
                   (12u << 10) | (uint32_t{active_->controlSlot} << 24)};
        return true;
    }
    bool bulkTransfer(uint8_t endpoint, void* buffer, size_t length, size_t& actual,
                      uint32_t timeoutMs) noexcept {
        actual = 0;
        if (!running_ || !active_ || active_->bulkFault || !buffer || !length || length > sizeof(active_->memory->bulkBuffer) ||
            !timeoutMs || (endpoint != active_->bulkInAddress && endpoint != active_->bulkOutAddress)) return false;
        const bool input = endpoint == active_->bulkInAddress;
        const unsigned q = input ? 0 : 1;
        const uint8_t dci = static_cast<uint8_t>((endpoint & 15u) * 2 + (input ? 1 : 0));
        auto* ring = input ? active_->memory->bulkInRing : active_->memory->bulkOutRing;
        const size_t ringOffset = input ? offsetof(DeviceDma, bulkInRing) : offsetof(DeviceDma, bulkOutRing);
        if (!input) std::memcpy(active_->memory->bulkBuffer, buffer, length);
        auto& trb = ring[active_->bulkIndex[q]];
        trb.parameter = active_->dmaAddress + offsetof(DeviceDma, bulkBuffer);
        trb.status = static_cast<uint32_t>(length);
        __atomic_thread_fence(__ATOMIC_RELEASE);
        trb.control = (1u << 10) | (1u << 5) | (1u << 2) | active_->bulkCycle[q];
        const uint64_t transfer = active_->dmaAddress + ringOffset + active_->bulkIndex[q] * sizeof(Trb);
        if (++active_->bulkIndex[q] == 63) {
            ring[63].parameter = active_->dmaAddress + ringOffset; ring[63].status = 0;
            __atomic_thread_fence(__ATOMIC_RELEASE);
            ring[63].control = (6u << 10) | 2u | active_->bulkCycle[q];
            active_->bulkIndex[q] = 0; active_->bulkCycle[q] ^= 1;
        }
        __atomic_thread_fence(__ATOMIC_SEQ_CST);
        write(doorbells_ + active_->controlSlot * 4u, dci);
        Trb event{}; bool complete = false, valid = false;
        const bool arrived = wait([&]() noexcept {
            while (pollHardwareEvent(event)) {
                if (((event.control >> 10) & 63u) != 32 ||
                    (event.parameter & ~uint64_t{15}) != transfer ||
                    (event.control >> 24) != active_->controlSlot || ((event.control >> 16) & 31u) != dci) {
                    if (!deferEvent(event)) { complete = true; return true; }
                    continue;
                }
                const uint32_t code = event.status >> 24, residual = event.status & 0xffffffu;
                complete = true;
                if ((code == 1 || code == 13) && residual <= length) {
                    actual = length - residual; valid = true;
                }
                return true;
            }
            return false;
        }, timeoutMs);
        if (!arrived || !complete || !valid) { active_->bulkFault = true; return false; }
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        if (input) std::memcpy(buffer, active_->memory->bulkBuffer, actual);
        return true;
    }
    bool bulkResetRecovery(uint8_t interfaceNumber, uint8_t input, uint8_t output) noexcept {
        if (!running_ || !active_ || input != active_->bulkInAddress || output != active_->bulkOutAddress) return false;
        active_->bulkFault = true;
        auto controlNoData = [&](uint8_t type, uint8_t request, uint16_t value, uint16_t index) noexcept {
            uint64_t address{};
            const uint64_t setup = uint64_t{type} | (uint64_t{request} << 8) |
                (uint64_t{value} << 16) | (uint64_t{index} << 32);
            if (!submitControl(setup, 0, false, address)) return false;
            Trb event{}; bool good = false;
            if (!wait([&]() noexcept {
                while (pollHardwareEvent(event)) {
                    if (((event.control >> 10) & 63u) == 32 &&
                        (event.parameter & ~uint64_t{15}) == address) {
                        good = (event.status >> 24) == 1; return true;
                    }
                    if (!deferEvent(event)) return true;
                }
                return false;
            }, 2000)) return false;
            return good;
        };
        auto commandAndWait = [&](Trb request) noexcept {
            uint64_t address{};
            if (!submitCommand(request, address)) return false;
            Trb event{}; bool good = false;
            if (!wait([&]() noexcept {
                while (pollHardwareEvent(event)) {
                    if (((event.control >> 10) & 63u) == 33 &&
                        (event.parameter & ~uint64_t{15}) == address) {
                        good = (event.status >> 24) == 1; return true;
                    }
                    if (!deferEvent(event)) return true;
                }
                return false;
            }, 2000)) return false;
            return good;
        };
        if (!controlNoData(0x21, 0xff, 0, interfaceNumber)) return false;
        const uint8_t endpoints[2]{input, output};
        for (unsigned q = 0; q < 2; ++q) {
            const uint8_t dci = static_cast<uint8_t>((endpoints[q] & 15u) * 2 + (q == 0 ? 1 : 0));
            const uint64_t ring = active_->dmaAddress + (q == 0 ? offsetof(DeviceDma, bulkInRing)
                                                        : offsetof(DeviceDma, bulkOutRing));
            if (!commandAndWait({0, 0, (14u << 10) | (uint32_t{dci} << 16) | (uint32_t{active_->controlSlot} << 24)}) ||
                !commandAndWait({(ring + active_->bulkIndex[q] * sizeof(Trb)) | active_->bulkCycle[q], 0,
                                 (16u << 10) | (uint32_t{dci} << 16) | (uint32_t{active_->controlSlot} << 24)}) ||
                !controlNoData(0x02, 1, 0, endpoints[q])) return false;
        }
        active_->bulkFault = false;
        return true;
    }

    // One outstanding command keeps event-to-command ownership explicit until
    // the USB bus supplies a complete multi-command cancellation protocol.
    bool submitCommand(const Trb& command, uint64_t& commandAddress) noexcept {
        if (!running_ || !dma_ || commandPending_) return false;
        const uint32_t type = (command.control >> 10) & 63u;
        if (type < 9 || type > 23) return false;
        auto& target = dma_->command[commandIndex_];
        target.parameter = command.parameter;
        target.status = command.status;
        __atomic_thread_fence(__ATOMIC_RELEASE);
        target.control = (command.control & ~1u) | commandCycle_;
        commandAddress = dmaAddress_ + offsetof(DmaState, command) + commandIndex_ * sizeof(Trb);
        pendingCommandAddress_ = commandAddress;
        commandPending_ = true;
        if (++commandIndex_ == 63) {
            auto& link = dma_->command[63];
            link.parameter = dmaAddress_ + offsetof(DmaState, command);
            link.status = 0;
            __atomic_thread_fence(__ATOMIC_RELEASE);
            link.control = (6u << 10) | 2u | commandCycle_;
            commandIndex_ = 0;
            commandCycle_ ^= 1;
        }
        __atomic_thread_fence(__ATOMIC_SEQ_CST);
        write(doorbells_, 0);
        return true;
    }

    bool pollHardwareEvent(Trb& result) noexcept {
        if (!running_ || !dma_) return false;
        volatile Trb& event = dma_->event[eventIndex_];
        const uint32_t control = event.control;
        if ((control & 1u) != eventCycle_) return false;
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        result = {event.parameter, event.status, control};
        if (((control >> 10) & 63u) == 33 && commandPending_ &&
            (result.parameter & ~uint64_t{15}) == pendingCommandAddress_) {
            commandPending_ = false;
            pendingCommandAddress_ = 0;
        }
        SlotState* eventSlot{};
        if (((control >> 10) & 63u) == 32)
            for (auto& slot : slots_) if (slot.controlSlot && slot.controlSlot == ((control >> 24) & 0xffu)) {
                eventSlot = &slot; break;
            }
        if (eventSlot && eventSlot->controlPending &&
            (result.parameter & ~uint64_t{15}) == eventSlot->controlStatusAddress &&
            ((control >> 16) & 31u) == 1) {
            eventSlot->controlPending = false;
            eventSlot->controlStatusAddress = 0;
        }
        if (eventSlot && eventSlot->interruptPending &&
            (result.parameter & ~uint64_t{15}) == eventSlot->interruptAddress &&
            ((control >> 16) & 31u) == eventSlot->interruptDci) {
            eventSlot->interruptPending = false;
            eventSlot->interruptAddress = 0;
        }
        if (++eventIndex_ == 64) { eventIndex_ = 0; eventCycle_ ^= 1; }
        __atomic_thread_fence(__ATOMIC_RELEASE);
        write64(runtime_ + 0x38, (dmaAddress_ + offsetof(DmaState, event) + eventIndex_ * sizeof(Trb)) | 8u);
        return true;
    }

    bool pollEvent(Trb& result) noexcept {
        if (deferredCount_) {
            result = deferredEvents_[deferredHead_];
            deferredHead_ = static_cast<uint8_t>((deferredHead_ + 1) % 64);
            --deferredCount_;
            return true;
        }
        return pollHardwareEvent(result);
    }
    bool healthy() const noexcept { return running_ && !eventOverflow_; }

    bool stop() noexcept {
        if (!services_) return true;
        if (commandEnabled_) {
            if (running_ || operational_) {
                write(operational_, read(operational_) & ~1u);
                if (!wait([&]() noexcept { return (read(operational_ + 4) & 1u) != 0; }, 100)) return false;
                running_ = false;
            }
            if (acquiredOwnership_) {
                write(legacyOffset_, read(legacyOffset_) & ~(1u << 24));
                acquiredOwnership_ = false;
            }
            if (!pciWriteCommand(originalCommand_ & ~uint16_t{7}) || !waitTransactions()) return false;
            commandEnabled_ = false;
        }
        if (dmaToken_) {
            if (services_->FreeDmaBuffer(services_->Context, dmaToken_) != boot_driver::STATUS_SUCCESS) return false;
            dmaToken_ = 0; dma_ = nullptr;
        }
        if (registers_) {
            if (!services_->ReleaseDeviceMemory(services_->Context, const_cast<uint8_t*>(registers_), registerLength_)) return false;
            registers_ = nullptr;
        }
        if (claim_ && services_->ReleasePciDevice(services_->Context, claim_) != boot_driver::STATUS_SUCCESS) return false;
        claim_ = 0; services_ = nullptr; dmaAddress_ = 0;
        operational_ = runtime_ = doorbells_ = legacyOffset_ = 0;
        maxPorts_ = maxSlots_ = 0;
        std::memset(portProtocol_, 0, sizeof(portProtocol_));
        std::memset(portSlotType_, 0, sizeof(portSlotType_));
        for (auto& slot : slots_) slot = {};
        active_ = nullptr;
        commandPending_ = false;
        commandIndex_ = 0;
        commandCycle_ = eventCycle_ = 1;
        eventIndex_ = 0;
        deferredHead_ = deferredCount_ = 0;
        eventOverflow_ = false;
        return true;
    }
};
}
