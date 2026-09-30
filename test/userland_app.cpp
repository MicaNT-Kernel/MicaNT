/**
 * MicaNT Standalone Ring 3 Userland Test Application
 * Built as a native 64-bit Windows PE executable.
 */

#include <windows.h>
#include <iostream>

int main() {
    const char* message = "[Ring 3 Userland] Hello from MicaNT Userland Subsystem!\n";
    DWORD bytesWritten = 0;
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    WriteFile(hStdout, message, static_cast<DWORD>(strlen(message)), &bytesWritten, NULL);

    // Test virtual memory allocation in user space
    void* ptr = VirtualAlloc(NULL, 64 * 1024, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (ptr) {
        std::cout << "[Ring 3 Userland] VirtualAlloc succeeded at: " << ptr << "\n";
        strcpy_s(static_cast<char*>(ptr), 64, "MicaNT Memory Mapped String");
        std::cout << "[Ring 3 Userland] Read back: " << static_cast<char*>(ptr) << "\n";
        VirtualFree(ptr, 0, MEM_RELEASE);
    }

    std::cout << "[Ring 3 Userland] Exiting with status 42.\n";
    return 42;
}
