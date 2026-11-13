#include "micant/km/executive/ob.hpp"
#include <cassert>
#include <atomic>
#include <thread>

namespace {
using namespace micant;
constexpr uint32_t kRead = 0x1;
constexpr uint32_t kWrite = 0x2;
int g_securityDescriptor{};
bool g_deny{};
bool g_failOpen{};

NtStatus check(void* descriptor, uint32_t desired, uint32_t* granted) noexcept {
    if (descriptor != &g_securityDescriptor || !granted) return NtStatus::InvalidParameter;
    if (g_deny || (desired & ~(kRead | ob::MAXIMUM_ALLOWED))) return NtStatus::AccessDenied;
    *granted = (desired & ob::MAXIMUM_ALLOWED) ? kRead : desired;
    return NtStatus::Success;
}

NtStatus onOpen(ob::ObjectHeader*, uint32_t) noexcept {
    return g_failOpen ? NtStatus::Unsuccessful : NtStatus::Success;
}
}

int main() {
    ob::ObjectType type{};
    type.validAccessMask = kRead | kWrite;
    type.genericMapping = {kRead, kWrite, kRead, kRead | kWrite};
    type.openProcedure = onOpen;
    ob::ObjectHeader named{};
    named.type = &type;
    named.attributes = OBJ_PERMANENT;
    named.securityDescriptor = &g_securityDescriptor;
    auto& ns = ob::ObjectNamespace::get();
    ns.initializeRoot();
    auto directory = ns.getDirectory(L"\\KernelObjects");
    assert(directory && directory->insertObject(L"HostOpenFixture", &named) == NtStatus::Success);

    ob::ObSetAccessCheckProcedure(check);
    ob::HandleTable table;
    const UnicodeString name(L"\\KernelObjects\\HostOpenFixture");
    ObjectAttributes attributes{};
    attributes.objectName = &name;
    attributes.attributes = OBJ_CASE_INSENSITIVE | OBJ_INHERIT;
    Handle handle{};
    assert(ob::ObOpenObjectByName(&attributes, &type, ob::UserMode, ob::GENERIC_READ,
                                  table, handle) == NtStatus::Success);
    assert(handle && table.getEntry(handle)->grantedAccess == kRead);
    assert(table.getEntry(handle)->handleAttributes == OBJ_INHERIT);
    assert(table.closeHandle(handle) == NtStatus::Success);
    assert(ob::ObOpenObjectByName(&attributes, &type, ob::UserMode, ob::MAXIMUM_ALLOWED,
                                  table, handle) == NtStatus::Success);
    assert(table.getEntry(handle)->grantedAccess == kRead);
    assert(table.closeHandle(handle) == NtStatus::Success);
    assert(ob::ObOpenObjectByName(&attributes, &type, ob::UserMode,
                                  ob::MAXIMUM_ALLOWED | kWrite, table, handle) == NtStatus::AccessDenied);
    assert(!handle && !table.getActiveHandleCount());

    attributes.attributes = OBJ_KERNEL_HANDLE;
    handle = 0xdead;
    assert(ob::ObOpenObjectByName(&attributes, &type, ob::UserMode, kRead,
                                  table, handle) == NtStatus::InvalidParameter && !handle);
    attributes.attributes = 0;
    attributes.rootDirectory = 4;
    assert(ob::ObOpenObjectByName(&attributes, &type, ob::KernelMode, kRead,
                                  table, handle) == NtStatus::NotSupported && !handle);
    attributes.rootDirectory = 0;
    g_deny = true;
    assert(ob::ObOpenObjectByName(&attributes, &type, ob::UserMode, kRead,
                                  table, handle) == NtStatus::AccessDenied && !handle);
    g_deny = false;
    named.securityDescriptor = nullptr;
    assert(ob::ObOpenObjectByName(&attributes, &type, ob::UserMode, kRead,
                                  table, handle) == NtStatus::AccessDenied && !handle);
    named.securityDescriptor = &g_securityDescriptor;
    ob::ObSetAccessCheckProcedure(nullptr);
    assert(ob::ObOpenObjectByName(&attributes, &type, ob::UserMode, kRead,
                                  table, handle) == NtStatus::AccessDenied && !handle);
    attributes.attributes = OBJ_FORCE_ACCESS_CHECK;
    assert(ob::ObOpenObjectByName(&attributes, &type, ob::KernelMode, kRead,
                                  table, handle) == NtStatus::NotSupported && !handle);
    ob::ObSetAccessCheckProcedure(check);
    attributes.attributes = 0;
    g_failOpen = true;
    assert(ob::ObOpenObjectByName(&attributes, &type, ob::UserMode, kRead,
                                  table, handle) == NtStatus::Unsuccessful && !handle);
    assert(table.getActiveHandleCount() == 0 && named.handleCount == 0);
    assert(ob::ObOpenObjectByPointer(&named, OBJ_PROTECT_CLOSE, kRead, &type,
                                     table, handle) == NtStatus::Unsuccessful);
    assert(!handle && !table.getActiveHandleCount() && !named.handleCount);
    g_failOpen = false;

    ob::ObjectHeader inserted{};
    inserted.type = &type;
    inserted.objectName = L"\\KernelObjects\\HostInsertFixture";
    handle = 0xdead;
    assert(ob::ObInsertObject(&inserted, nullptr, kRead, &table, &handle) == NtStatus::Success);
    assert(handle && inserted.handleCount == 1 &&
           ns.lookupObject(L"\\KernelObjects\\HostInsertFixture") == &inserted);
    assert(table.closeHandle(handle) == NtStatus::Success);
    assert(!ns.lookupObject(L"\\KernelObjects\\HostInsertFixture") && !inserted.parentDirectory);
    g_failOpen = true;
    handle = 0xdead;
    assert(ob::ObInsertObject(&inserted, nullptr, kRead, &table, &handle) == NtStatus::Unsuccessful);
    assert(!handle && !inserted.handleCount && !table.getActiveHandleCount() &&
           !ns.lookupObject(L"\\KernelObjects\\HostInsertFixture") && !inserted.parentDirectory);
    g_failOpen = false;

    ob::ObjectHeader protectedInsert{};
    protectedInsert.type = &type;
    ob::HandleTable protectedTable;
    assert(protectedTable.createHandle(&protectedInsert, handle, kRead, OBJ_PROTECT_CLOSE) == NtStatus::Success);
    assert(handle && protectedTable.getEntry(handle)->handleAttributes == OBJ_PROTECT_CLOSE);
    assert(ob::ObCloseHandle(handle, protectedTable) == NtStatus::HandleNotClosable);
    protectedTable.closeAllHandles();
    assert(!protectedTable.getActiveHandleCount() && !protectedInsert.handleCount);

    ob::ObjectHeader raced{};
    raced.type = &type;
    raced.attributes = OBJ_PERMANENT;
    assert(directory->insertObject(L"HostOpenRace", &raced) == NtStatus::Success);
    const UnicodeString racedName(L"\\KernelObjects\\HostOpenRace");
    ObjectAttributes racedAttributes{};
    racedAttributes.objectName = &racedName;
    std::atomic<bool> startRace{false};
    std::atomic<bool> raceFailed{false};
    std::array<std::thread, 4> openers;
    for (auto& opener : openers) {
        opener = std::thread([&] {
            while (!startRace.load(std::memory_order_acquire)) std::this_thread::yield();
            for (size_t i = 0; i < 500; ++i) {
                Handle opened{};
                const auto status = ob::ObOpenObjectByName(&racedAttributes, &type, ob::KernelMode,
                    kRead, table, opened);
                if (status == NtStatus::Success) {
                    if (table.closeHandle(opened) != NtStatus::Success) {
                        raceFailed.store(true, std::memory_order_relaxed);
                    }
                } else if (status != NtStatus::ObjectNameNotFound) {
                    raceFailed.store(true, std::memory_order_relaxed);
                }
            }
        });
    }
    startRace.store(true, std::memory_order_release);
    std::this_thread::yield();
    if (!directory->removeObject(L"HostOpenRace")) raceFailed.store(true, std::memory_order_relaxed);
    for (auto& opener : openers) opener.join();
    assert(!raceFailed.load(std::memory_order_relaxed));
    assert(raced.pointerCount.load(std::memory_order_relaxed) == 1 && raced.handleCount.load() == 0);

    ob::ObSetAccessCheckProcedure(nullptr);
    assert(directory->removeObject(L"HostOpenFixture"));
}
