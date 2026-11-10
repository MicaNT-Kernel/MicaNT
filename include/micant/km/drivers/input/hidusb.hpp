#pragma once

#include <cstdint>
#include <cstring>

#include "micant/km/drivers/usb/xhci.hpp"
#include "micant/km/executive/io.hpp"

namespace micant::km::hid {

// USB transport implementation used by hidclass.sys for HID interrupt and
// class-specific control transfers. Collection parsing and report queues stay
// owned by hidclass; URB construction and submission belong to hidusb.sys.
class HidUsbMiniport {
public:
    static NtStatus submitInterrupt(uint8_t slotId, void* buffer, uint32_t length) {
        if (!buffer || !length) return NtStatus::InvalidParameter;
        xhci::URB* urb = xhci::USBD_UrbAllocate();
        if (!urb) return NtStatus::InsufficientResources;
        urb->UrbHeader.Function = xhci::URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER;
        urb->UrbBulkOrInterruptTransfer.TransferBuffer = buffer;
        urb->UrbBulkOrInterruptTransfer.TransferBufferLength = length;
        urb->UrbBulkOrInterruptTransfer.TransferFlags =
            xhci::USBD_TRANSFER_DIRECTION_IN | xhci::USBD_SHORT_TRANSFER_OK;
        NtStatus status = xhci::XhciSubmitUrb(urb, slotId);
        xhci::USBD_UrbFree(urb);
        return status;
    }

    static NtStatus submitControl(uint8_t slotId, uint8_t request, uint8_t reportType,
        uint8_t reportId, void* buffer, uint16_t length) {
        if (!buffer && length) return NtStatus::InvalidParameter;
        xhci::URB* urb = xhci::USBD_UrbAllocate();
        if (!urb) return NtStatus::InsufficientResources;
        urb->UrbHeader.Function = xhci::URB_FUNCTION_CONTROL_TRANSFER_EX;
        auto* transfer = &urb->UrbControlTransferEx;
        transfer->TransferBuffer = buffer;
        transfer->TransferBufferLength = length;
        const bool input = request == 0x01 || request == 0x02 || request == 0x03;
        transfer->TransferFlags = input ? xhci::USBD_TRANSFER_DIRECTION_IN : xhci::USBD_TRANSFER_DIRECTION_OUT;
        usb::USB_DEFAULT_PIPE_SETUP_PACKET setup{};
        setup.bmRequestType = usb::RequestType::Class | usb::RequestType::RecipientInterface |
            (input ? usb::RequestType::DirectionIn : usb::RequestType::DirectionOut);
        setup.bRequest = request;
        setup.wValue = static_cast<uint16_t>((static_cast<uint16_t>(reportType) << 8) | reportId);
        setup.wIndex = 0;
        setup.wLength = length;
        std::memcpy(transfer->SetupPacket, &setup, sizeof(setup));
        NtStatus status = xhci::XhciSubmitUrb(urb, slotId);
        xhci::USBD_UrbFree(urb);
        return status;
    }
};

inline void InitializeHidUsbDriver(io::DriverObject& driver) {
    driver.driverName = L"\\Driver\\HidUsb";
}

} // namespace micant::km::hid
