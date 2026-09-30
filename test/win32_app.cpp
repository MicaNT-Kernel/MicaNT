/**
 * @file win32_app.cpp
 * @brief Standalone Win32 Userland Test Application for MicaNT.
 *
 * Exercises standard Win32 APIs from kernel32.hpp on top of MicaNT's
 * clean-room ntdll and CSRSS Client-Server Runtime Subsystem.
 */

#include <iostream>
#include <cassert>
#include "micant/kernel32.hpp"

using namespace micant::win32;

int main() {
    std::cout << "========================================================================\n";
    std::cout << "                 MicaNT Native Win32 Application Harness                \n";
    std::cout << "========================================================================\n\n";

    // 1. Initialize Win32 Environment & Console
    std::cout << "[Win32 App] Initializing Win32 Console via AllocConsole()...\n";
    BOOL allocOk = AllocConsole();
    assert(allocOk && "AllocConsole must succeed");

    BOOL titleOk = SetConsoleTitleW(L"MicaNT Win32 Terminal");
    assert(titleOk && "SetConsoleTitleW must succeed");

    wchar_t titleBuf[64]{};
    DWORD titleLen = GetConsoleTitleW(titleBuf, 64);
    assert(titleLen > 0 && "GetConsoleTitleW must return non-zero title length");
    std::wcout << L"[Win32 App] Console Window Title: " << titleBuf << L"\n";

    // 2. Process & Thread Identity
    DWORD pid = GetCurrentProcessId();
    DWORD tid = GetCurrentThreadId();
    std::cout << "[Win32 App] Process ID: " << pid << ", Thread ID: " << tid << "\n";
    assert(pid > 0 && tid > 0 && "Process and Thread IDs must be valid");

    // 3. Win32 Console Output via WriteConsoleW
    std::cout << "[Win32 App] Writing to stdout via WriteConsoleW()...\n";
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    assert(hStdout != INVALID_HANDLE_VALUE && "GetStdHandle must return valid output handle");

    const wchar_t* msg = L"Hello from MicaNT Win32 Subsystem!\r\n";
    DWORD charsWritten = 0;
    BOOL writeOk = WriteConsoleW(hStdout, msg, static_cast<DWORD>(wcslen(msg)), &charsWritten, nullptr);
    assert(writeOk && "WriteConsoleW must succeed");
    std::cout << "[Win32 App] Successfully wrote " << charsWritten << " characters to console.\n";

    // 4. Win32 Default Heap Allocation
    std::cout << "[Win32 App] Testing Win32 Heap APIs (HeapAlloc, HeapSize, HeapFree)...\n";
    HANDLE hHeap = GetProcessHeap();
    assert(hHeap != nullptr && "GetProcessHeap must return non-null handle");

    LPVOID pBuf = HeapAlloc(hHeap, 0x00000008 /* HEAP_ZERO_MEMORY */, 256);
    assert(pBuf != nullptr && "HeapAlloc must succeed");
    SIZE_T blockSize = HeapSize(hHeap, 0, pBuf);
    assert(blockSize >= 256 && "HeapSize must report at least 256 bytes");
    std::cout << "[Win32 App] HeapAlloc 256 bytes at 0x" << pBuf << " (Reported Size: " << blockSize << ")\n";

    LPVOID pRealloc = HeapReAlloc(hHeap, 0, pBuf, 512);
    assert(pRealloc != nullptr && "HeapReAlloc must succeed");
    std::cout << "[Win32 App] HeapReAlloc expanded to 512 bytes at 0x" << pRealloc << "\n";

    BOOL freeOk = HeapFree(hHeap, 0, pRealloc);
    assert(freeOk && "HeapFree must succeed");

    // 5. Win32 Virtual Memory (VirtualAlloc / VirtualFree)
    std::cout << "[Win32 App] Testing VirtualAlloc & VirtualFree...\n";
    LPVOID pVirtual = VirtualAlloc(nullptr, 64 * 1024, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    assert(pVirtual != nullptr && "VirtualAlloc 64KB must succeed");
    std::cout << "[Win32 App] VirtualAlloc mapped 64KB page range at 0x" << pVirtual << "\n";

    BOOL virtFreeOk = VirtualFree(pVirtual, 0, MEM_RELEASE);
    assert(virtFreeOk && "VirtualFree must succeed");

    // 6. Win32 Synchronization (CreateEventW / SetEvent / WaitForSingleObject)
    std::cout << "[Win32 App] Testing Win32 Event Synchronization...\n";
    HANDLE hEvent = CreateEventW(nullptr, FALSE, FALSE, L"MicaWin32SyncEvent");
    assert(hEvent != nullptr && "CreateEventW must succeed");

    DWORD wait1 = WaitForSingleObject(hEvent, 0); // Polling unsignaled event
    assert(wait1 == WAIT_TIMEOUT && "WaitForSingleObject on unsignaled event must return WAIT_TIMEOUT");

    SetEvent(hEvent);
    DWORD wait2 = WaitForSingleObject(hEvent, 100);
    assert(wait2 == WAIT_OBJECT_0 && "WaitForSingleObject on signaled event must return WAIT_OBJECT_0");
    CloseHandle(hEvent);
    std::cout << "[Win32 App] Event synchronization verified successfully.\n";

    // 7. System Ticks and Sleep
    uint64_t ticks = GetTickCount64();
    std::cout << "[Win32 App] GetTickCount64(): " << ticks << " ms\n";
    Sleep(10);
    std::cout << "[Win32 App] Sleep(10ms) completed.\n";

    // 8. Clean Exit via ExitProcess
    std::cout << "\n[Win32 App] All Win32 Subsystem checks PASSED! Calling ExitProcess(0)...\n";
    ExitProcess(0);
}
