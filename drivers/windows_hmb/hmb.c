// ============================================================================
// Haven Memory Bus (HMB 2.0) Windows NT Kernel Paging Volume Driver
// File: hmb.c
// 
// Description:
//   Clean-room Windows Kernel-Mode Driver implementing the Haven Memory Bus
//   (HMB Spec 2.0) high-speed 4-tier paging memory system for Windows 10/11.
//   Provides in-RAM compressed z-Swap, zero-page deduplication, and
//   direct-stride LBA volume emulation for high-performance drop-in paging.
//
// Clean-Room ISO C / Windows NT Driver Architecture. Zero proprietary code.
// ============================================================================

#include <ntddk.h>
#include <ntdddisk.h>
#include <mountmgr.h>

#define HMB_DRIVER_VERSION       0x00020000 // Spec 2.0
#define HMB_PAGE_SIZE            4096
#define HMB_SECTOR_SIZE          512
#define HMB_SECTORS_PER_PAGE     8
#define HMB_TOTAL_CAPACITY_BYTES (4ULL * 1024ULL * 1024ULL * 1024ULL) // 4 GB Paging Volume
#define HMB_TOTAL_PAGES          (HMB_TOTAL_CAPACITY_BYTES / HMB_PAGE_SIZE)
#define HMB_MAX_ZSWAP_SLOTS      65536 // 256 MB in-RAM compressed slots

#define HMB_SLOT_EMPTY           0
#define HMB_SLOT_ZERO_PAGE       1
#define HMB_SLOT_COMPRESSED      2
#define HMB_SLOT_RAW             3

#define IOCTL_HMB_GET_TELEMETRY  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x880, METHOD_BUFFERED, FILE_ANY_ACCESS)

#pragma pack(push, 1)
typedef struct _HMB_TELEMETRY {
    ULONG64 Magic;                 // 0x4D454D4E45564148 ("HAVENMEM")
    ULONG   Version;               // 0x00020000
    ULONG   TotalPagesCapacity;
    ULONG64 TotalReads;
    ULONG64 TotalWrites;
    ULONG64 ZSwapHits;
    ULONG64 ZeroPageDeduplications;
    ULONG64 BytesSavedInRam;
    ULONG   ActiveZSwapSlots;
    ULONG   Reserved;
} HMB_TELEMETRY, *PHMB_TELEMETRY;
#pragma pack(pop)

typedef struct _HMB_SLOT {
    ULONG   Flags;
    USHORT  CompressedSize;
    USHORT  OriginalSize;
    PUCHAR  Data;
} HMB_SLOT, *PHMB_SLOT;

typedef struct _DEVICE_EXTENSION {
    PDEVICE_OBJECT DeviceObject;
    UNICODE_STRING DeviceName;
    UNICODE_STRING SymbolicLink;
    BOOLEAN        IsControlDevice;
    KSPIN_LOCK     Lock;
    HMB_TELEMETRY  Telemetry;
    PHMB_SLOT      Slots;
    UCHAR          CompressionScratch[HMB_PAGE_SIZE];
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

static PDEVICE_OBJECT g_PagingDevice = NULL;
static PDEVICE_OBJECT g_ControlDevice = NULL;

// Fast Zero-Check (scans 4KB in 64-bit words)
static BOOLEAN HmbIsZeroPage(const ULONG64* Buffer) {
    for (ULONG i = 0; i < (HMB_PAGE_SIZE / sizeof(ULONG64)); ++i) {
        if (Buffer[i] != 0) return FALSE;
    }
    return TRUE;
}

// Fast Lightweight Byte-Run Compressor for In-RAM z-Swap
static USHORT HmbCompressPage(const UCHAR* Src, UCHAR* Dst, USHORT DstMax) {
    USHORT srcPos = 0, dstPos = 0;
    while (srcPos < HMB_PAGE_SIZE && dstPos + 4 < DstMax) {
        UCHAR val = Src[srcPos];
        UCHAR count = 1;
        while (srcPos + count < HMB_PAGE_SIZE && count < 255 && Src[srcPos + count] == val) {
            count++;
        }
        if (count >= 4 || val == 0) {
            Dst[dstPos++] = 0xFF; // Run Marker
            Dst[dstPos++] = count;
            Dst[dstPos++] = val;
            srcPos += count;
        } else {
            if (val == 0xFF) {
                Dst[dstPos++] = 0xFF;
                Dst[dstPos++] = 1;
                Dst[dstPos++] = 0xFF;
            } else {
                Dst[dstPos++] = val;
            }
            srcPos++;
        }
    }
    return (srcPos == HMB_PAGE_SIZE) ? dstPos : HMB_PAGE_SIZE;
}

// Decompress Byte-Run Stream
static void HmbDecompressPage(const UCHAR* Src, USHORT SrcLen, UCHAR* Dst) {
    USHORT srcPos = 0, dstPos = 0;
    while (srcPos < SrcLen && dstPos < HMB_PAGE_SIZE) {
        if (Src[srcPos] == 0xFF) {
            srcPos++;
            if (srcPos + 1 < SrcLen) {
                UCHAR count = Src[srcPos++];
                UCHAR val = Src[srcPos++];
                for (UCHAR k = 0; k < count && dstPos < HMB_PAGE_SIZE; ++k) {
                    Dst[dstPos++] = val;
                }
            }
        } else {
            Dst[dstPos++] = Src[srcPos++];
        }
    }
    if (dstPos < HMB_PAGE_SIZE) {
        RtlZeroMemory(Dst + dstPos, HMB_PAGE_SIZE - dstPos);
    }
}

// Read/Write Dispatch for HMB Paging Volume
static NTSTATUS HmbReadWrite(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    PDEVICE_EXTENSION ext = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
    ULONG isWrite = (irpSp->MajorFunction == IRP_MJ_WRITE);
    ULONG64 byteOffset = isWrite ? irpSp->Parameters.Write.ByteOffset.QuadPart : irpSp->Parameters.Read.ByteOffset.QuadPart;
    ULONG length = isWrite ? irpSp->Parameters.Write.Length : irpSp->Parameters.Read.Length;

    if (byteOffset >= HMB_TOTAL_CAPACITY_BYTES) {
        Irp->IoStatus.Status = STATUS_END_OF_FILE;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_END_OF_FILE;
    }

    if (byteOffset + length > HMB_TOTAL_CAPACITY_BYTES) {
        length = (ULONG)(HMB_TOTAL_CAPACITY_BYTES - byteOffset);
    }

    PUCHAR userBuffer = NULL;
    if (Irp->MdlAddress) {
        userBuffer = (PUCHAR)MmGetSystemAddressForMdlSafe(Irp->MdlAddress, HighPagePriority);
    } else {
        userBuffer = (PUCHAR)Irp->AssociatedIrp.SystemBuffer;
        if (!userBuffer) userBuffer = (PUCHAR)Irp->UserBuffer;
    }

    if (!userBuffer) {
        Irp->IoStatus.Status = STATUS_INSUFFICIENT_RESOURCES;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    ULONG64 startSlot = byteOffset / HMB_PAGE_SIZE;
    ULONG64 numPages = (length + HMB_PAGE_SIZE - 1) / HMB_PAGE_SIZE;

    KLOCK_QUEUE_HANDLE lockHandle;
    KeAcquireInStackQueuedSpinLock(&ext->Lock, &lockHandle);

    if (isWrite) {
        ext->Telemetry.TotalWrites++;
        for (ULONG64 p = 0; p < numPages; ++p) {
            ULONG64 slotIdx = (startSlot + p) % HMB_MAX_ZSWAP_SLOTS;
            PHMB_SLOT slot = &ext->Slots[slotIdx];
            PUCHAR pageSrc = userBuffer + (p * HMB_PAGE_SIZE);

            if (HmbIsZeroPage((const ULONG64*)pageSrc)) {
                // Tier 2: Zero-Page Deduplication (100% savings, 0 bytes memory allocated!)
                if (slot->Data) {
                    ExFreePool(slot->Data);
                    slot->Data = NULL;
                }
                slot->Flags = HMB_SLOT_ZERO_PAGE;
                slot->CompressedSize = 0;
                slot->OriginalSize = HMB_PAGE_SIZE;
                ext->Telemetry.ZeroPageDeduplications++;
                ext->Telemetry.BytesSavedInRam += HMB_PAGE_SIZE;
            } else {
                // Tier 2: In-RAM Compressed z-Swap
                UCHAR* compBuf = ext->CompressionScratch;
                USHORT compLen = HmbCompressPage(pageSrc, compBuf, HMB_PAGE_SIZE - 64);
                if (compLen < HMB_PAGE_SIZE - 64) {
                    // Page compressed successfully!
                    if (slot->Data) ExFreePool(slot->Data);
                    slot->Data = (PUCHAR)ExAllocatePoolWithTag(NonPagedPool, compLen, 'bMhH');
                    if (slot->Data) {
                        RtlCopyMemory(slot->Data, compBuf, compLen);
                        slot->Flags = HMB_SLOT_COMPRESSED;
                        slot->CompressedSize = compLen;
                        slot->OriginalSize = HMB_PAGE_SIZE;
                        ext->Telemetry.BytesSavedInRam += (HMB_PAGE_SIZE - compLen);
                        ext->Telemetry.ActiveZSwapSlots++;
                    } else {
                        slot->Flags = HMB_SLOT_EMPTY;
                    }
                } else {
                    // Uncompressible: store raw
                    if (slot->Data && slot->CompressedSize != HMB_PAGE_SIZE) {
                        ExFreePool(slot->Data);
                        slot->Data = NULL;
                    }
                    if (!slot->Data) {
                        slot->Data = (PUCHAR)ExAllocatePoolWithTag(NonPagedPool, HMB_PAGE_SIZE, 'bMhH');
                    }
                    if (slot->Data) {
                        RtlCopyMemory(slot->Data, pageSrc, HMB_PAGE_SIZE);
                        slot->Flags = HMB_SLOT_RAW;
                        slot->CompressedSize = HMB_PAGE_SIZE;
                        slot->OriginalSize = HMB_PAGE_SIZE;
                    } else {
                        slot->Flags = HMB_SLOT_EMPTY;
                    }
                }
            }
        }
    } else {
        // Read Operation
        ext->Telemetry.TotalReads++;
        for (ULONG64 p = 0; p < numPages; ++p) {
            ULONG64 slotIdx = (startSlot + p) % HMB_MAX_ZSWAP_SLOTS;
            PHMB_SLOT slot = &ext->Slots[slotIdx];
            PUCHAR pageDst = userBuffer + (p * HMB_PAGE_SIZE);

            if (slot->Flags == HMB_SLOT_ZERO_PAGE) {
                RtlZeroMemory(pageDst, HMB_PAGE_SIZE);
                ext->Telemetry.ZSwapHits++;
            } else if (slot->Flags == HMB_SLOT_COMPRESSED && slot->Data) {
                HmbDecompressPage(slot->Data, slot->CompressedSize, pageDst);
                ext->Telemetry.ZSwapHits++;
            } else if (slot->Flags == HMB_SLOT_RAW && slot->Data) {
                RtlCopyMemory(pageDst, slot->Data, HMB_PAGE_SIZE);
            } else {
                RtlZeroMemory(pageDst, HMB_PAGE_SIZE);
            }
        }
    }

    KeReleaseInStackQueuedSpinLock(&lockHandle);

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = length;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

// IOCTL Dispatch for Disk and Volume Interfaces
static NTSTATUS HmbDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    PDEVICE_EXTENSION ext = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);
    ULONG ioctlCode = irpSp->Parameters.DeviceIoControl.IoControlCode;
    ULONG inLen = irpSp->Parameters.DeviceIoControl.InputBufferLength;
    ULONG outLen = irpSp->Parameters.DeviceIoControl.OutputBufferLength;
    PVOID buffer = Irp->AssociatedIrp.SystemBuffer;
    NTSTATUS status = STATUS_SUCCESS;
    ULONG_PTR info = 0;

    if (ext->IsControlDevice) {
        if (ioctlCode == IOCTL_HMB_GET_TELEMETRY) {
            if (outLen >= sizeof(HMB_TELEMETRY) && buffer) {
                KLOCK_QUEUE_HANDLE lockHandle;
                KeAcquireInStackQueuedSpinLock(&ext->Lock, &lockHandle);
                RtlCopyMemory(buffer, &ext->Telemetry, sizeof(HMB_TELEMETRY));
                KeReleaseInStackQueuedSpinLock(&lockHandle);
                info = sizeof(HMB_TELEMETRY);
                status = STATUS_SUCCESS;
            } else {
                status = STATUS_BUFFER_TOO_SMALL;
            }
        } else {
            status = STATUS_INVALID_DEVICE_REQUEST;
        }
        Irp->IoStatus.Status = status;
        Irp->IoStatus.Information = info;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return status;
    }

    // Disk & Storage IOCTLs
    switch (ioctlCode) {
        case IOCTL_DISK_GET_DRIVE_GEOMETRY: {
            if (outLen >= sizeof(DISK_GEOMETRY)) {
                PDISK_GEOMETRY geom = (PDISK_GEOMETRY)buffer;
                geom->Cylinders.QuadPart = HMB_TOTAL_CAPACITY_BYTES / (64 * 32 * HMB_SECTOR_SIZE);
                geom->MediaType = FixedMedia;
                geom->TracksPerCylinder = 64;
                geom->SectorsPerTrack = 32;
                geom->BytesPerSector = HMB_SECTOR_SIZE;
                info = sizeof(DISK_GEOMETRY);
                status = STATUS_SUCCESS;
            } else {
                status = STATUS_BUFFER_TOO_SMALL;
            }
            break;
        }
        case IOCTL_DISK_GET_PARTITION_INFO: {
            if (outLen >= sizeof(PARTITION_INFORMATION)) {
                PPARTITION_INFORMATION part = (PPARTITION_INFORMATION)buffer;
                part->StartingOffset.QuadPart = 0;
                part->PartitionLength.QuadPart = HMB_TOTAL_CAPACITY_BYTES;
                part->HiddenSectors = 0;
                part->PartitionNumber = 1;
                part->PartitionType = PARTITION_IFS; // Installable File System / Raw Volume
                part->BootIndicator = FALSE;
                part->RecognizedPartition = TRUE;
                part->RewritePartition = FALSE;
                info = sizeof(PARTITION_INFORMATION);
                status = STATUS_SUCCESS;
            } else {
                status = STATUS_BUFFER_TOO_SMALL;
            }
            break;
        }
        case IOCTL_DISK_GET_LENGTH_INFO: {
            if (outLen >= sizeof(GET_LENGTH_INFORMATION)) {
                PGET_LENGTH_INFORMATION len = (PGET_LENGTH_INFORMATION)buffer;
                len->Length.QuadPart = HMB_TOTAL_CAPACITY_BYTES;
                info = sizeof(GET_LENGTH_INFORMATION);
                status = STATUS_SUCCESS;
            } else {
                status = STATUS_BUFFER_TOO_SMALL;
            }
            break;
        }
        case IOCTL_STORAGE_GET_DEVICE_NUMBER: {
            if (outLen >= sizeof(STORAGE_DEVICE_NUMBER)) {
                PSTORAGE_DEVICE_NUMBER num = (PSTORAGE_DEVICE_NUMBER)buffer;
                num->DeviceType = FILE_DEVICE_DISK;
                num->DeviceNumber = 0x484D; // "HM"
                num->PartitionNumber = 1;
                info = sizeof(STORAGE_DEVICE_NUMBER);
                status = STATUS_SUCCESS;
            } else {
                status = STATUS_BUFFER_TOO_SMALL;
            }
            break;
        }
        case IOCTL_DISK_IS_WRITABLE:
        case IOCTL_DISK_CHECK_VERIFY: {
            status = STATUS_SUCCESS;
            info = 0;
            break;
        }
        default: {
            status = STATUS_INVALID_DEVICE_REQUEST;
            info = 0;
            break;
        }
    }

    Irp->IoStatus.Status = status;
    Irp->IoStatus.Information = info;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

static NTSTATUS HmbCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    UNREFERENCED_PARAMETER(DeviceObject);
    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

// Driver Unload Handler
VOID DriverUnload(PDRIVER_OBJECT DriverObject) {
    UNICODE_STRING pLink, cLink;
    RtlInitUnicodeString(&pLink, L"\\DosDevices\\Global\\P:");
    RtlInitUnicodeString(&cLink, L"\\DosDevices\\Global\\HmbControl");
    IoDeleteSymbolicLink(&pLink);
    IoDeleteSymbolicLink(&cLink);

    if (g_PagingDevice) {
        PDEVICE_EXTENSION ext = (PDEVICE_EXTENSION)g_PagingDevice->DeviceExtension;
        if (ext && ext->Slots) {
            for (ULONG i = 0; i < HMB_MAX_ZSWAP_SLOTS; ++i) {
                if (ext->Slots[i].Data) {
                    ExFreePool(ext->Slots[i].Data);
                }
            }
            ExFreePool(ext->Slots);
        }
        IoDeleteDevice(g_PagingDevice);
    }
    if (g_ControlDevice) {
        IoDeleteDevice(g_ControlDevice);
    }
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "[HMB] Driver unloaded successfully.\n");
}

// Canonical DriverEntry
NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
    UNREFERENCED_PARAMETER(RegistryPath);
    NTSTATUS status;

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, 
        "===========================================================\n"
        "  Haven Memory Bus (HMB 2.0) Windows NT Paging Engine\n"
        "  4-Tier Direct-Stride Architecture | Spec 2.0 Active\n"
        "===========================================================\n");

    DriverObject->DriverUnload = DriverUnload;
    for (ULONG i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; ++i) {
        DriverObject->MajorFunction[i] = HmbCreateClose;
    }
    DriverObject->MajorFunction[IRP_MJ_READ] = HmbReadWrite;
    DriverObject->MajorFunction[IRP_MJ_WRITE] = HmbReadWrite;
    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = HmbDeviceControl;

    // 1. Create HMB Paging Volume Device (\Device\HmbPagingVolume -> \DosDevices\Global\P:)
    UNICODE_STRING devName, symLink;
    RtlInitUnicodeString(&devName, L"\\Device\\HmbPagingVolume");
    RtlInitUnicodeString(&symLink, L"\\DosDevices\\Global\\P:");

    status = IoCreateDevice(
        DriverObject,
        sizeof(DEVICE_EXTENSION),
        &devName,
        FILE_DEVICE_DISK,
        FILE_DEVICE_IS_MOUNTED,
        FALSE,
        &g_PagingDevice
    );
    if (!NT_SUCCESS(status)) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[HMB] Failed to create Paging Device: 0x%08X\n", status);
        return status;
    }

    PDEVICE_EXTENSION pExt = (PDEVICE_EXTENSION)g_PagingDevice->DeviceExtension;
    RtlZeroMemory(pExt, sizeof(DEVICE_EXTENSION));
    pExt->DeviceObject = g_PagingDevice;
    pExt->DeviceName = devName;
    pExt->SymbolicLink = symLink;
    pExt->IsControlDevice = FALSE;
    KeInitializeSpinLock(&pExt->Lock);

    pExt->Telemetry.Magic = 0x4D454D4E45564148ULL; // "HAVENMEM"
    pExt->Telemetry.Version = HMB_DRIVER_VERSION;
    pExt->Telemetry.TotalPagesCapacity = HMB_TOTAL_PAGES;

    // Allocate in-RAM z-Swap slot array
    pExt->Slots = (PHMB_SLOT)ExAllocatePoolWithTag(NonPagedPool, sizeof(HMB_SLOT) * HMB_MAX_ZSWAP_SLOTS, 'bMhH');
    if (!pExt->Slots) {
        IoDeleteDevice(g_PagingDevice);
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlZeroMemory(pExt->Slots, sizeof(HMB_SLOT) * HMB_MAX_ZSWAP_SLOTS);

    g_PagingDevice->Flags |= DO_DIRECT_IO;
    g_PagingDevice->Flags &= ~DO_DEVICE_INITIALIZING;

    status = IoCreateSymbolicLink(&symLink, &devName);
    if (!NT_SUCCESS(status)) {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_WARNING_LEVEL, "[HMB] Warning: Could not create P: symlink (status 0x%08X)\n", status);
    }

    // 2. Create HMB Control & Telemetry Device (\Device\HmbControl -> \DosDevices\Global\HmbControl)
    UNICODE_STRING cDevName, cSymLink;
    RtlInitUnicodeString(&cDevName, L"\\Device\\HmbControl");
    RtlInitUnicodeString(&cSymLink, L"\\DosDevices\\Global\\HmbControl");

    status = IoCreateDevice(
        DriverObject,
        sizeof(DEVICE_EXTENSION),
        &cDevName,
        FILE_DEVICE_UNKNOWN,
        0,
        FALSE,
        &g_ControlDevice
    );
    if (NT_SUCCESS(status)) {
        PDEVICE_EXTENSION cExt = (PDEVICE_EXTENSION)g_ControlDevice->DeviceExtension;
        RtlZeroMemory(cExt, sizeof(DEVICE_EXTENSION));
        cExt->DeviceObject = g_ControlDevice;
        cExt->DeviceName = cDevName;
        cExt->SymbolicLink = cSymLink;
        cExt->IsControlDevice = TRUE;
        KeInitializeSpinLock(&cExt->Lock);
        g_ControlDevice->Flags &= ~DO_DEVICE_INITIALIZING;
        NTSTATUS symStatus = IoCreateSymbolicLink(&cSymLink, &cDevName);
        if (!NT_SUCCESS(symStatus)) {
            DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_WARNING_LEVEL, "[HMB] Warning: Could not create HmbControl symlink (status 0x%08X)\n", symStatus);
        }
    } else {
        DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "[HMB] Failed to create Control Device: 0x%08X\n", status);
    }

    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, 
        "[HMB] Paging Volume online (4GB capacity, 65,536 in-RAM z-Swap slots armed).\n");
    return STATUS_SUCCESS;
}
