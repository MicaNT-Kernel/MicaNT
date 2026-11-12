#pragma once

#include "micant/km/executive/io_block_device.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

// USB Mass Storage BOT / SCSI class owner. The host transfer callback must
// submit real endpoint transfers and return actual lengths, never host data.
namespace micant::usb::baremetal {
struct BulkTransport {
    void* context{};
    NtStatus (*transfer)(void*, uint8_t endpoint, void* buffer, size_t bytes,
                         size_t& transferred, uint32_t timeoutMs) noexcept{};
    NtStatus (*resetRecovery)(void*, uint8_t interfaceNumber,
                              uint8_t bulkIn, uint8_t bulkOut) noexcept{};
    size_t maximumTransfer{};
    bool (*begin)(void*) noexcept{};
    void (*end)(void*) noexcept{};
};

class MassStorage {
    BulkTransport transport_{};
    uint8_t interface_{}, in_{}, out_{};
    uint32_t nextTag_{1}, blockSize_{};
    uint64_t blocks_{};
    std::atomic_flag operation_ = ATOMIC_FLAG_INIT;
    std::atomic<bool> connected_{false};
    std::atomic<uint32_t> backendReferences_{0};

    struct Guard {
        std::atomic_flag& flag;
        bool acquired;
        explicit Guard(std::atomic_flag& f) noexcept : flag(f), acquired(!f.test_and_set(std::memory_order_acquire)) {}
        ~Guard() { if (acquired) flag.clear(std::memory_order_release); }
    };
    static void le32(uint8_t* p, uint32_t v) noexcept {
        for (unsigned i = 0; i < 4; ++i) p[i] = static_cast<uint8_t>(v >> (8 * i));
    }
    static uint32_t getLe32(const uint8_t* p) noexcept {
        return uint32_t{p[0]} | (uint32_t{p[1]} << 8) | (uint32_t{p[2]} << 16) | (uint32_t{p[3]} << 24);
    }
    static uint32_t getBe32(const uint8_t* p) noexcept {
        return (uint32_t{p[0]} << 24) | (uint32_t{p[1]} << 16) | (uint32_t{p[2]} << 8) | p[3];
    }
    static void be32(uint8_t* p, uint32_t v) noexcept {
        for (unsigned i = 0; i < 4; ++i) p[i] = static_cast<uint8_t>(v >> (24 - i * 8));
    }
    static uint64_t getBe64(const uint8_t* p) noexcept {
        return (uint64_t{getBe32(p)} << 32) | getBe32(p + 4);
    }
    static void be64(uint8_t* p, uint64_t v) noexcept {
        be32(p, static_cast<uint32_t>(v >> 32)); be32(p + 4, static_cast<uint32_t>(v));
    }
    NtStatus recover() noexcept {
        return transport_.resetRecovery(transport_.context, interface_, in_, out_);
    }
    NtStatus command(const uint8_t* cdb, size_t cdbSize, void* data,
                     uint32_t bytes, bool dataIn) noexcept {
        if (!connected_.load(std::memory_order_acquire)) return NtStatus::DeviceNotReady;
        if (!cdb || !cdbSize || cdbSize > 16 || (bytes && !data) || !nextTag_)
            return NtStatus::InvalidParameter;
        if (!transport_.begin(transport_.context)) return NtStatus::DeviceBusy;
        struct EndTransaction {
            BulkTransport& transport;
            ~EndTransaction() { transport.end(transport.context); }
        } end{transport_};
        const uint32_t tag = nextTag_++;
        uint8_t cbw[31]{};
        le32(cbw, 0x43425355u); le32(cbw + 4, tag); le32(cbw + 8, bytes);
        cbw[12] = dataIn ? 0x80 : 0; cbw[14] = static_cast<uint8_t>(cdbSize);
        std::memcpy(cbw + 15, cdb, cdbSize);
        size_t count{};
        auto status = transport_.transfer(transport_.context, out_, cbw, sizeof(cbw), count, 2000);
        if (!connected_.load(std::memory_order_acquire)) return NtStatus::DeviceNotReady;
        if (status != NtStatus::Success || count != sizeof(cbw)) { (void)recover(); return NtStatus::DeviceNotReady; }
        size_t dataTransferred{};
        if (bytes) {
            status = transport_.transfer(transport_.context, dataIn ? in_ : out_, data, bytes, dataTransferred, 5000);
            if (!connected_.load(std::memory_order_acquire)) return NtStatus::DeviceNotReady;
            if (status != NtStatus::Success || dataTransferred > bytes) { (void)recover(); return NtStatus::DeviceNotReady; }
        }
        uint8_t csw[13]{}; count = 0;
        status = transport_.transfer(transport_.context, in_, csw, sizeof(csw), count, 2000);
        if (!connected_.load(std::memory_order_acquire)) return NtStatus::DeviceNotReady;
        const uint32_t residue = getLe32(csw + 8);
        if (status != NtStatus::Success || count != sizeof(csw) || getLe32(csw) != 0x53425355u ||
            getLe32(csw + 4) != tag || residue > bytes || dataTransferred != bytes - residue || csw[12] > 2) {
            (void)recover(); return NtStatus::DeviceNotReady;
        }
        if (csw[12] == 2) { (void)recover(); return NtStatus::DeviceNotReady; }
        if (csw[12] == 1 || residue) return NtStatus::DeviceDataError;
        return NtStatus::Success;
    }
    static NtStatus readThunk(void* context, uint64_t lba, uint32_t count,
                              void* output, uint32_t* read) noexcept {
        return static_cast<MassStorage*>(context)->read(lba, count, output, read);
    }
    static void releaseThunk(void* context) noexcept {
        auto& references = static_cast<MassStorage*>(context)->backendReferences_;
        if (!references.fetch_sub(1, std::memory_order_acq_rel)) __builtin_trap();
    }
public:
    NtStatus initialize(BulkTransport transport, uint8_t interfaceNumber,
                        uint8_t bulkIn, uint8_t bulkOut) noexcept {
        Guard guard(operation_);
        if (!guard.acquired) return NtStatus::DeviceBusy;
        if (connected_.load(std::memory_order_relaxed) || !transport.transfer || !transport.resetRecovery ||
            !transport.begin || !transport.end ||
            transport.maximumTransfer < 4096 ||
            !(bulkIn & 0x80) || (bulkOut & 0x80) || !(bulkIn & 15) || !(bulkOut & 15))
            return NtStatus::InvalidParameter;
        transport_ = transport; interface_ = interfaceNumber; in_ = bulkIn; out_ = bulkOut;
        connected_.store(true, std::memory_order_release);
        uint8_t inquiry[36]{};
        const uint8_t inquire[6]{0x12, 0, 0, 0, sizeof(inquiry), 0};
        auto status = command(inquire, sizeof(inquire), inquiry, sizeof(inquiry), true);
        if (status == NtStatus::Success && (inquiry[0] & 0x1f) != 0) status = NtStatus::NotSupported;
        const uint8_t ready[6]{};
        if (status == NtStatus::Success) {
            for (unsigned attempt = 0; attempt < 3; ++attempt) {
                status = command(ready, sizeof(ready), nullptr, 0, false);
                if (status != NtStatus::DeviceDataError) break;
                uint8_t sense[18]{};
                const uint8_t senseCdb[6]{0x03, 0, 0, 0, sizeof(sense), 0};
                if (command(senseCdb, sizeof(senseCdb), sense, sizeof(sense), true) != NtStatus::Success) break;
                // Retry only NOT READY / UNIT ATTENTION; other failures are
                // reported rather than masking a persistent medium error.
                const uint8_t key = sense[2] & 15u;
                if (key != 2 && key != 6) break;
            }
        }
        uint8_t capacity[8]{}; const uint8_t capacityCdb[10]{0x25};
        if (status == NtStatus::Success) status = command(capacityCdb, sizeof(capacityCdb), capacity, sizeof(capacity), true);
        if (status == NtStatus::Success) {
            uint64_t last = getBe32(capacity);
            uint32_t size = getBe32(capacity + 4);
            if (last == UINT32_MAX) {
                uint8_t capacity16[32]{}; uint8_t cdb16[16]{0x9e, 0x10};
                be32(cdb16 + 10, sizeof(capacity16));
                status = command(cdb16, sizeof(cdb16), capacity16, sizeof(capacity16), true);
                if (status == NtStatus::Success) {
                    last = getBe64(capacity16); size = getBe32(capacity16 + 8);
                }
            }
            if (status == NtStatus::Success && (last == UINT64_MAX || !size || size > 4096 ||
                (size & (size - 1)) || last + 1 > UINT64_MAX / size)) status = NtStatus::NotSupported;
            if (status == NtStatus::Success) {
                uint8_t sector[4096]{};
                uint8_t readCdb[10]{0x28}; readCdb[8] = 1;
                status = command(readCdb, sizeof(readCdb), sector, size, true);
                if (status == NtStatus::Success) { blocks_ = uint64_t{last} + 1; blockSize_ = size; }
            }
        }
        if (status != NtStatus::Success) { connected_.store(false, std::memory_order_release); blocks_ = 0; blockSize_ = 0; }
        return status;
    }
    NtStatus read(uint64_t lba, uint32_t count, void* output, uint32_t* done) noexcept {
        if (!done) return NtStatus::InvalidParameter;
        *done = 0;
        if (!output || !count) return NtStatus::InvalidParameter;
        Guard guard(operation_);
        if (!guard.acquired) return NtStatus::DeviceBusy;
        if (!connected_.load(std::memory_order_acquire) || !blocks_) return NtStatus::DeviceNotReady;
        if (lba >= blocks_ || count > blocks_ - lba ||
            count > UINT32_MAX / blockSize_) return NtStatus::InvalidParameter;
        auto* out = static_cast<uint8_t*>(output);
        while (*done < count) {
            if (!connected_.load(std::memory_order_acquire)) return NtStatus::DeviceNotReady;
            uint32_t chunk = count - *done;
            if (chunk > 65535) chunk = 65535;
            if (chunk > transport_.maximumTransfer / blockSize_)
                chunk = static_cast<uint32_t>(transport_.maximumTransfer / blockSize_);
            if (chunk > UINT32_MAX / blockSize_) chunk = UINT32_MAX / blockSize_;
            const uint64_t current = lba + *done;
            uint8_t cdb[16]{}; size_t cdbLength{};
            if (current <= UINT32_MAX && chunk - 1 <= UINT32_MAX - current) {
                cdb[0] = 0x28; be32(cdb + 2, static_cast<uint32_t>(current));
                cdb[7] = static_cast<uint8_t>(chunk >> 8); cdb[8] = static_cast<uint8_t>(chunk);
                cdbLength = 10;
            } else {
                cdb[0] = 0x88; be64(cdb + 2, current); be32(cdb + 10, chunk);
                cdbLength = 16;
            }
            const auto status = command(cdb, cdbLength, out + size_t{*done} * blockSize_, chunk * blockSize_, true);
            if (status != NtStatus::Success) return status;
            *done += chunk;
        }
        return NtStatus::Success;
    }
    // The bus owner keeps this object alive until the final BlockDeviceObject
    // reference is released. Detach prevents subsequent transport requests.
    void detach() noexcept { connected_.store(false, std::memory_order_release); }
    [[nodiscard]] io::BlockDeviceBackend blockBackend() noexcept {
        Guard guard(operation_);
        if (!guard.acquired || !connected_.load(std::memory_order_acquire) || !blocks_ || !blockSize_ ||
            backendReferences_.load(std::memory_order_relaxed) == UINT32_MAX) return {};
        backendReferences_.fetch_add(1, std::memory_order_relaxed);
        return {this, blocks_, blockSize_, readThunk, releaseThunk};
    }
    [[nodiscard]] bool canDestroy() const noexcept {
        return !connected_.load(std::memory_order_acquire) &&
            backendReferences_.load(std::memory_order_acquire) == 0 && !operation_.test(std::memory_order_acquire);
    }
};
}
