#include "micant/km/executive/ob.hpp"
#include <array>
#include <atomic>
#include <thread>

int main() {
    using namespace micant;
    ob::HandleTable table;
    ob::ObjectType type{};
    ob::ObjectHeader object{};
    object.type = &type;
    constexpr uint32_t access = ob::SYNCHRONIZE | 1;
    constexpr size_t workerCount = 8;
    constexpr size_t iterations = 2000;
    std::atomic<bool> failed{false};
    std::array<std::thread, workerCount> workers;
    for (auto& worker : workers) {
        worker = std::thread([&] {
            for (size_t i = 0; i < iterations; ++i) {
                Handle handle{};
                if (table.createHandle(&object, handle, access) != NtStatus::Success) {
                    failed.store(true, std::memory_order_relaxed);
                    return;
                }
                ob::ObjectHeader* reference{};
                if (ob::ObReferenceObjectByHandle(handle, table, access, &type, &reference) != NtStatus::Success) {
                    failed.store(true, std::memory_order_relaxed);
                    (void)table.closeHandle(handle);
                    return;
                }
                ob::ObDereferenceObject(reference);
                if (table.closeHandle(handle) != NtStatus::Success) {
                    failed.store(true, std::memory_order_relaxed);
                    return;
                }
            }
        });
    }
    for (auto& worker : workers) worker.join();
    if (failed.load(std::memory_order_relaxed) || table.getActiveHandleCount() != 0 ||
        object.handleCount != 0 || object.pointerCount != 1 || type.totalNumberOfHandles != 0)
        return 1;

    Handle sharedHandle{};
    if (table.createHandle(&object, sharedHandle, access) != NtStatus::Success) return 2;
    std::atomic<bool> beginRace{false};
    std::array<std::thread, workerCount - 1> readers;
    for (auto& reader : readers) {
        reader = std::thread([&] {
            while (!beginRace.load(std::memory_order_acquire)) std::this_thread::yield();
            for (size_t i = 0; i < iterations; ++i) {
                ob::ObjectHeader* reference{};
                const NtStatus status = ob::ObReferenceObjectByHandle(
                    sharedHandle, table, access, &type, &reference);
                if (status == NtStatus::Success) {
                    ob::ObDereferenceObject(reference);
                } else if (status != NtStatus::InvalidHandle) {
                    failed.store(true, std::memory_order_relaxed);
                    return;
                }
            }
        });
    }
    beginRace.store(true, std::memory_order_release);
    std::this_thread::yield();
    if (table.closeHandle(sharedHandle) != NtStatus::Success) failed.store(true, std::memory_order_relaxed);
    for (auto& reader : readers) reader.join();
    return failed.load(std::memory_order_relaxed) || table.getActiveHandleCount() != 0 ||
           object.handleCount != 0 || object.pointerCount != 1 || type.totalNumberOfHandles != 0;
}
