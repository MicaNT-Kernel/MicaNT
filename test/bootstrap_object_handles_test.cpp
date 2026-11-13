#include "micant/km/executive/ob.hpp"
#include <cassert>
#include <cstdio>

int main() {
    using namespace micant;
    ob::HandleTable table;
    ob::ObjectType type;
    ob::ObjectHeader object;
    object.type = &type;
    const uint32_t access = ob::SYNCHRONIZE | 1;
    Handle first{}, second{};
    assert(table.getActiveHandleCount() == 0);
    assert(table.createHandle(&object, first, access) == NtStatus::Success);
    assert(first != 0 && table.getActiveHandleCount() == 1);
    uint32_t granted{};
    assert(table.lookup(first, &granted) == &object && granted == access);
    assert(object.pointerCount == 2 && object.handleCount == 1);
    assert(object.handleInfo.handleCount == 1 && type.totalNumberOfHandles == 1);
    Handle denied{};
    assert(table.duplicateHandle(first, table, denied, 2) == NtStatus::AccessDenied);
    assert(!denied && table.getActiveHandleCount() == 1);
    ob::ObjectHeader* reference{};
    assert(ob::ObReferenceObjectByHandle(first, table, access, &type, &reference) == NtStatus::Success);
    assert(reference == &object && object.pointerCount == 3);
    ob::ObDereferenceObject(reference);

    assert(table.duplicateHandle(first, table, second, 0, true, 2) == NtStatus::Success);
    assert(first != second && table.getActiveHandleCount() == 2);
    assert(table.lookup(second, &granted) == &object && granted == access);
    const auto* entry = table.getEntry(second);
    assert(entry && entry->handleAttributes == OBJ_INHERIT);
    assert(object.pointerCount == 3 && object.handleCount == 2);
    assert(table.closeHandle(first) == NtStatus::Success);
    assert(!table.lookup(first) && table.lookup(second) == &object);
    assert(table.closeHandle(first) == NtStatus::InvalidHandle);
    assert(object.pointerCount == 2 && object.handleCount == 1);
    assert(table.closeHandle(second) == NtStatus::Success);
    assert(table.getActiveHandleCount() == 0);
    assert(object.pointerCount == 1 && object.handleCount == 0);
    assert(object.handleInfo.handleCount == 0 && type.totalNumberOfHandles == 0);
    ob::HandleTable parent, child;
    Handle inherited{}, privateHandle{};
    assert(parent.createHandle(&object, inherited, access) == NtStatus::Success);
    assert(parent.setHandleAttributes(inherited, OBJ_INHERIT) == NtStatus::Success);
    assert(parent.createHandle(&object, privateHandle, access) == NtStatus::Success);
    assert(child.inheritHandlesFrom(parent) == NtStatus::Success);
    assert(child.lookup(inherited, &granted) == &object && granted == access);
    assert(child.lookup(privateHandle) == nullptr && object.handleCount == 3);
    assert(child.inheritHandlesFrom(parent) == NtStatus::InvalidParameter);
    assert(parent.closeHandle(inherited) == NtStatus::Success);
    assert(parent.closeHandle(privateHandle) == NtStatus::Success);
    assert(child.lookup(inherited) == &object && child.closeHandle(inherited) == NtStatus::Success);
    assert(object.pointerCount == 1 && object.handleCount == 0);
    std::puts("Canonical object handle tests passed");
}
