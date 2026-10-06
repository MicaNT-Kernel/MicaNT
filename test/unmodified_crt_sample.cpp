#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @file unmodified_crt_sample.cpp
 * @brief Standalone, unmodified 64-bit Windows console application using MSVCRT.
 *
 * Compiles strictly against standard Microsoft headers (<stdio.h>, <stdlib.h>, <windows.h>)
 * without any MicaNT headers or shims to test clean-room MSVCRT dynamic import binding,
 * heap allocation, string manipulation, and formatted I/O on MicaNT.
 */
extern "C" void __main() {}

int main() {
    printf("MicaNT: Standard C Runtime (msvcrt.dll) executing successfully!\n");

    char* buffer = (char*)malloc(128);
    if (!buffer) {
        return 1;
    }

    strcpy(buffer, "Clean-Room MSVCRT Heap & Formatted I/O Interoperability Verified.");
    printf("Buffer Content: %s\n", buffer);
    printf("Buffer Length: %zu characters\n", strlen(buffer));

    free(buffer);
    ExitProcess(0);
    return 0;
}
