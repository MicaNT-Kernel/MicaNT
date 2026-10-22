/**
 * @file comdlg32.cpp
 * @brief Sovereign Common Dialogs Subsystem DLL (comdlg32.dll) for MicaNT System32.
 *
 * Freestanding ISO C++23. Clean-Room design. Zero proprietary headers.
 */

#include <cstdint>

extern "C" {

bool DllMain(void*, unsigned long, void*) noexcept { return true; }

__declspec(dllexport) bool GetOpenFileNameW(void*) noexcept { return true; }
__declspec(dllexport) bool GetSaveFileNameW(void*) noexcept { return true; }
__declspec(dllexport) bool ChooseColorW(void*) noexcept { return true; }

} // extern "C"
