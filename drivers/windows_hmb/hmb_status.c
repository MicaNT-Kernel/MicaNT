// ============================================================================
// Haven Memory Bus (HMB 2.0) - Windows Telemetry Monitor CLI
// File: hmb_status.c
// ============================================================================

#include <windows.h>
#include <stdio.h>

#define IOCTL_HMB_GET_TELEMETRY  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x880, METHOD_BUFFERED, FILE_ANY_ACCESS)

#pragma pack(push, 1)
typedef struct _HMB_TELEMETRY {
    unsigned __int64 Magic;                 // 0x4D454D4E45564148 ("HAVENMEM")
    unsigned long   Version;               // 0x00020000
    unsigned long   TotalPagesCapacity;
    unsigned __int64 TotalReads;
    unsigned __int64 TotalWrites;
    unsigned __int64 ZSwapHits;
    unsigned __int64 ZeroPageDeduplications;
    unsigned __int64 BytesSavedInRam;
    unsigned long   ActiveZSwapSlots;
    unsigned long   Reserved;
} HMB_TELEMETRY, *PHMB_TELEMETRY;
#pragma pack(pop)

int main(void) {
    HANDLE hDev = CreateFileW(
        L"\\\\.\\HmbControl",
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    // Fallback: If \\.\HmbControl wasn't found due to Session 0 namespace isolation,
    // open the NT kernel device object directly via \\.\GLOBALROOT\Device\HmbControl
    if (hDev == INVALID_HANDLE_VALUE) {
        hDev = CreateFileW(
            L"\\\\.\\GLOBALROOT\\Device\\HmbControl",
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );
    }

    if (hDev == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        printf("[HMB] Error: Unable to open \\\\.\\HmbControl (Error %lu).\n", err);
        if (err == ERROR_FILE_NOT_FOUND) {
            printf("[HMB] Reason: Kernel device or symbolic link not found.\n");
            printf("[HMB] Ensure the 'hmb' kernel driver service is installed and started.\n");
        } else if (err == ERROR_ACCESS_DENIED) {
            printf("[HMB] Reason: Access Denied. Please run this command as Administrator!\n");
        }
        return 1;
    }

    HMB_TELEMETRY telem = { 0 };
    DWORD bytesReturned = 0;
    BOOL ok = DeviceIoControl(
        hDev,
        IOCTL_HMB_GET_TELEMETRY,
        NULL, 0,
        &telem, sizeof(telem),
        &bytesReturned,
        NULL
    );

    CloseHandle(hDev);

    if (!ok || bytesReturned < sizeof(telem)) {
        printf("[HMB] Failed to query HMB telemetry (Error %lu).\n", GetLastError());
        return 1;
    }

    printf("=======================================================\n");
    printf("  Haven Memory Bus (HMB 2.0) - Windows Telemetry\n");
    printf("=======================================================\n");
    printf("  Architecture:        Spec 2.0 Direct-Stride 4-Tier Memory\n");
    printf("  Paging Volume:       P:\\ (HMB Virtual Volume)\n");
    printf("  Paging Capacity:     %lu MB (%lu Pages)\n", 
           (telem.TotalPagesCapacity * 4096) / (1024 * 1024), 
           telem.TotalPagesCapacity);
    printf("  Total Page Writes:   %llu\n", telem.TotalWrites);
    printf("  Total Page Reads:    %llu\n", telem.TotalReads);
    printf("  z-Swap RAM Hits:     %llu (Sub-microsecond, 0 Disk I/O)\n", telem.ZSwapHits);
    printf("  Zero-Page Dedup:     %llu Pages\n", telem.ZeroPageDeduplications);
    printf("  RAM Memory Saved:    %llu KB (%llu MB)\n", 
           telem.BytesSavedInRam / 1024, 
           telem.BytesSavedInRam / (1024 * 1024));
    printf("  Active z-Swap Slots: %lu / 65536 in RAM\n", telem.ActiveZSwapSlots);
    printf("=======================================================\n");
    return 0;
}
