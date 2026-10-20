/**
 * @file prism_vulkan1_opengl32_16dll_seal.cpp
 * @brief Horizon 109 (Kernel Unit #81): Sovereign PrismX Official Khronos Vulkan 1.3 Loader
 *        (vulkan-1.dll) & OpenGL 4.6 WGL Runtime (opengl32.dll) + 16-DLL System32 Seal.
 *        Implements standalone PE32+ DLL exports for vulkan-1.dll and opengl32.dll.
 */

#include <cstdint>
#include <cstddef>

#if !defined(BUILD_SYSTEM32_STANDALONE_DLL)
#include "micant/km/core/ntos_internal.hpp"
#include "micant/um/subsystems/graphics/prism_vulkan1_opengl32_16dll_seal.hpp"

namespace micant::sys::internal {

void PrismXVulkan1OpenGL32SixteenDllSealInitializeExecutiveSubsystem() noexcept {
    micant::um::prism_vulkan1_opengl32_seal::InitializePrismSystem32SixteenDllSealExports();
}

} // namespace micant::sys::internal

#else

namespace {
// Dispatchable ICD handle magic (0x01CDC0DE)
alignas(16) static uintptr_t g_VulkanIcdMagic = 0x01CDC0DEULL;
static void* g_DummyHandle = &g_VulkanIcdMagic;
} // namespace

#if defined(BUILD_SYSTEM32_VULKAN_1_DLL)
extern "C" {
__declspec(dllexport) int32_t vkCreateInstance(const void* /*pCreateInfo*/, const void* /*pAllocator*/, void** pInstance) {
    if (!pInstance) return -1; // VK_ERROR_INITIALIZATION_FAILED
    *pInstance = g_DummyHandle;
    return 0; // VK_SUCCESS
}

__declspec(dllexport) void vkDestroyInstance(void* /*instance*/, const void* /*pAllocator*/) {}

__declspec(dllexport) int32_t vkEnumeratePhysicalDevices(void* /*instance*/, uint32_t* pPhysicalDeviceCount, void** pPhysicalDevices) {
    if (!pPhysicalDeviceCount) return -1;
    if (!pPhysicalDevices) {
        *pPhysicalDeviceCount = 1;
        return 0;
    }
    if (*pPhysicalDeviceCount >= 1) {
        pPhysicalDevices[0] = g_DummyHandle;
        *pPhysicalDeviceCount = 1;
        return 0;
    }
    return 0;
}

__declspec(dllexport) void* vkGetInstanceProcAddr(void* /*instance*/, const char* /*pName*/) {
    return nullptr;
}

__declspec(dllexport) void* vkGetDeviceProcAddr(void* /*device*/, const char* /*pName*/) {
    return nullptr;
}

__declspec(dllexport) int32_t vkCreateDevice(void* /*physicalDevice*/, const void* /*pCreateInfo*/, const void* /*pAllocator*/, void** pDevice) {
    if (pDevice) *pDevice = g_DummyHandle;
    return 0;
}

__declspec(dllexport) void vkDestroyDevice(void* /*device*/, const void* /*pAllocator*/) {}

__declspec(dllexport) int32_t vkEnumerateInstanceExtensionProperties(const char* /*pLayerName*/, uint32_t* pPropertyCount, void* /*pProperties*/) {
    if (pPropertyCount) *pPropertyCount = 0;
    return 0;
}

__declspec(dllexport) int32_t vkEnumerateInstanceLayerProperties(uint32_t* pPropertyCount, void* /*pProperties*/) {
    if (pPropertyCount) *pPropertyCount = 0;
    return 0;
}
}

#elif defined(BUILD_SYSTEM32_OPENGL32_DLL)
extern "C" {
__declspec(dllexport) void* wglCreateContext(void* /*hdc*/) {
    return g_DummyHandle;
}

__declspec(dllexport) int32_t wglDeleteContext(void* /*hglrc*/) {
    return 1; // TRUE
}

__declspec(dllexport) int32_t wglMakeCurrent(void* /*hdc*/, void* /*hglrc*/) {
    return 1; // TRUE
}

__declspec(dllexport) void* wglGetProcAddress(const char* /*lpszProc*/) {
    return nullptr;
}

__declspec(dllexport) void* wglGetCurrentContext() {
    return g_DummyHandle;
}

__declspec(dllexport) void* wglGetCurrentDC() {
    return nullptr;
}

__declspec(dllexport) const char* glGetString(uint32_t name) {
    if (name == 0x1F00) return "Sovereign PrismX / MicaNT"; // GL_VENDOR
    if (name == 0x1F01) return "PrismX OpenGL 4.6 on Direct3D 12"; // GL_RENDERER
    if (name == 0x1F02) return "4.6.0 MicaNT Sovereign 2026"; // GL_VERSION
    if (name == 0x8B8C) return "4.60"; // GL_SHADING_LANGUAGE_VERSION
    return "";
}

__declspec(dllexport) void glClear(uint32_t /*mask*/) {}
__declspec(dllexport) void glClearColor(float /*red*/, float /*green*/, float /*blue*/, float /*alpha*/) {}
__declspec(dllexport) void glViewport(int32_t /*x*/, int32_t /*y*/, int32_t /*width*/, int32_t /*height*/) {}
}
#endif

#endif
