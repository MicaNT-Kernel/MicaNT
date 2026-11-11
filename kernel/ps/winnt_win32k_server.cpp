#ifndef MICANT_WIN32K_EXPORTS
#define MICANT_WIN32K_EXPORTS
#endif
#include "micant/shared/win32k_client.h"
#include "micant/shared/user_graphics.hpp"
#include "micant/shared/user_syscall.hpp"
#include "micant/km/win32k/win32k.hpp"

using namespace micant::win32k;

#if defined(BUILD_SYSTEM32_STANDALONE_DLL)
extern "C" bool DllMain(void*, unsigned long, void*) noexcept { return true; }
#endif

// The user-mode Win32k service owns its own dispatcher instance; the kernel's
// standard syscall table is initialized by ntoskrnl and must not be linked here.
namespace micant::sys {
void SyscallDispatcher::initializeStandardTable() {}
}

namespace {
int32_t PresentComposedDesktop(TitanWin32kSubsystem& server) {
    auto base = server.composeDesktopFrame(false);
    auto heldLayers = server.collectDesktopCompositionLayers();
    if (base && !base->pixels.empty()) {
        std::vector<micant::boot_driver::CompositionLayer> layers;
        layers.reserve(heldLayers.size());
        for (const auto& layer : heldLayers) layers.push_back(layer.descriptor);
        micant::um::UserPresentCompositionParams64 composition{};
        composition.baseSurfaceBufferVa = reinterpret_cast<uint64_t>(base->pixels.data());
        composition.layersVa = reinterpret_cast<uint64_t>(layers.data());
        composition.width = base->width;
        composition.height = base->height;
        composition.pitch = base->stride / sizeof(uint32_t);
        composition.layerCount = static_cast<uint32_t>(layers.size());
        const uint64_t compositionStatus = micant::sys::invokeUserService(
            0x85, reinterpret_cast<uint64_t>(&composition));
        if (compositionStatus == 0) return 1;
    }
    auto bitmap = server.composeDesktopFrame();
    if (!bitmap || bitmap->pixels.empty() || bitmap->width == 0 || bitmap->height == 0) return 0;
    micant::um::UserPresentSurfaceParams64 params{};
    params.surfaceBufferVa = reinterpret_cast<uint64_t>(bitmap->pixels.data());
    params.width = bitmap->width;
    params.height = bitmap->height;
    params.pitch = bitmap->width;
    const uint64_t status = micant::sys::invokeUserService(
        0x81, reinterpret_cast<uint64_t>(&params));
    return status == 0 ? 1 : 0;
}
}

extern "C" {
void* MicaWin32kCreateWindowExW(uint32_t ex, const wchar_t* cls,
    const wchar_t* title, uint32_t style, int32_t x, int32_t y, int32_t w, int32_t h,
    void* parent, void* menu, void* instance, void* param) {
    auto& server = TitanWin32kSubsystem::Instance();
    server.initialize();
    void* hwnd=server.createWindowEx(ex, cls, title, style, x, y, w, h, parent, menu, instance, param);
    if (!hwnd || !server.getWindowLongPtr(hwnd,-4)) return hwnd;
    MicaWin32kCreateStruct create{param,instance,menu,parent,h,w,y,x,static_cast<int32_t>(style),title,cls,ex};
    MSG msg{hwnd,0x0081,1,reinterpret_cast<intptr_t>(&create),0,{0,0}}; // WM_NCCREATE
    if (!server.dispatchMessage(msg)) { server.destroyWindow(hwnd); return nullptr; }
    msg.message=0x0001; msg.wParam=0; // WM_CREATE
    if (server.dispatchMessage(msg) == -1) { server.destroyWindow(hwnd); return nullptr; }
    return hwnd;
}
int32_t MicaWin32kDestroyWindow(void* hwnd) {
    const int32_t result = NtUserDestroyWindow(hwnd);
    if (result) PresentComposedDesktop(TitanWin32kSubsystem::Instance());
    return result;
}
int32_t MicaWin32kShowWindow(void* hwnd, int32_t cmd) {
    const int32_t result = NtUserShowWindow(hwnd, cmd);
    if (hwnd) PresentComposedDesktop(TitanWin32kSubsystem::Instance());
    return result;
}
int32_t MicaWin32kSetWindowPos(void* hwnd, void* after,
    int32_t x, int32_t y, int32_t w, int32_t h, uint32_t flags) {
    const int32_t result = NtUserSetWindowPos(hwnd, after, x, y, w, h, flags);
    if (result) PresentComposedDesktop(TitanWin32kSubsystem::Instance());
    return result;
}
int32_t MicaWin32kPostMessageW(void* hwnd, uint32_t message, uintptr_t wParam, intptr_t lParam) {
    return NtUserPostMessage(hwnd, message, wParam, lParam);
}
intptr_t MicaWin32kSendMessageW(void* hwnd, uint32_t message, uintptr_t wParam, intptr_t lParam) {
    return NtUserSendMessage(hwnd, message, wParam, lParam);
}
void MicaWin32kPostQuitMessage(int32_t exitCode) {
    NtUserPostQuitMessage(exitCode);
}
uint16_t MicaWin32kRegisterClassW(const wchar_t* className, void* proc) {
    return NtUserRegisterClassW(className,reinterpret_cast<WNDPROC>(proc));
}
intptr_t MicaWin32kDispatchMessageW(const MicaWin32kMessage* input) {
    if (!input) return 0;
    MSG msg{input->hwnd,input->message,input->wParam,input->lParam,input->time,{input->pt.x,input->pt.y}};
    return NtUserDispatchMessage(&msg);
}
intptr_t MicaWin32kSetWindowLongPtrW(void* hwnd, int32_t index, intptr_t value) {
    return NtUserSetWindowLongPtr(hwnd,index,value);
}
intptr_t MicaWin32kGetWindowLongPtrW(void* hwnd, int32_t index) {
    return NtUserGetWindowLongPtr(hwnd,index);
}
intptr_t MicaWin32kDefWindowProcW(void* hwnd, uint32_t message, uintptr_t wParam, intptr_t lParam) {
    const intptr_t result = NtUserDefWindowProc(hwnd,message,wParam,lParam);
    if (message == 0x0010) PresentComposedDesktop(TitanWin32kSubsystem::Instance());
    return result;
}
void* MicaWin32kGetDC(void* hwnd) { return NtUserGetDC(hwnd); }
int32_t MicaWin32kReleaseDC(void* hwnd, void* dc) { return NtUserReleaseDC(hwnd,dc); }
void* MicaWin32kCreateCompatibleDC(void* dc) { return NtGdiCreateCompatibleDC(dc); }
void* MicaWin32kCreateCompatibleBitmap(void* dc, int32_t w, int32_t h) { return NtGdiCreateCompatibleBitmap(dc,w,h); }
void* MicaWin32kSelectObject(void* dc, void* object) { return NtGdiSelectBitmap(dc,object); }
int32_t MicaWin32kBitBlt(void* dst, int32_t x, int32_t y, int32_t w, int32_t h,
    void* src, int32_t sx, int32_t sy, uint32_t rop) { return NtGdiBitBlt(dst,x,y,w,h,src,sx,sy,rop); }
int32_t MicaWin32kPresentDC(void* hdc) {
    if (!hdc) return 0;
    return PresentComposedDesktop(TitanWin32kSubsystem::Instance());
}
int32_t MicaWin32kDeleteObject(void* object) { return NtGdiDeleteObjectApp(object); }
void* MicaWin32kGetStockObject(int32_t index) { return NtGdiGetStockObject(index); }
void* MicaWin32kCreateDibSection(void* dc, int32_t width, int32_t height, void** bits) {
    if (bits) *bits=nullptr;
    if (width<=0 || height<=0 || width>16384 || height>16384) return nullptr;
    auto bitmap=NtGdiCreateCompatibleBitmap(dc,width,height);
    auto surface=TitanWin32kSubsystem::Instance().getGdi().getBitmap(bitmap);
    if (!surface) return nullptr;
    if (bits) *bits=surface->pixels.data();
    return bitmap;
}
int32_t MicaWin32kPeekMessageW(MicaWin32kMessage* out, void* hwnd,
    uint32_t min, uint32_t max, uint32_t remove) {
    if (!out) return 0;
    MSG msg{};
    if (!NtUserPeekMessage(&msg, hwnd, min, max, remove)) return 0;
    out->hwnd=msg.hwnd; out->message=msg.message; out->wParam=msg.wParam;
    out->lParam=msg.lParam; out->time=msg.time; out->pt={msg.pt.x,msg.pt.y};
    return 1;
}
int32_t MicaWin32kGetMessageW(MicaWin32kMessage* out, void* hwnd,
    uint32_t min, uint32_t max) {
    if (!out) return -1;
    MSG msg{};
    const auto result=NtUserGetMessage(&msg, hwnd, min, max);
    out->hwnd=msg.hwnd; out->message=msg.message; out->wParam=msg.wParam;
    out->lParam=msg.lParam; out->time=msg.time; out->pt={msg.pt.x,msg.pt.y};
    return result;
}
static int32_t CopyRect(void* hwnd, MicaWin32kRect* out, bool client) {
    if (!hwnd || !out) return 0;
    auto wnd=TitanWin32kSubsystem::Instance().getWindow(hwnd);
    if (!wnd) return 0;
    const auto& r=client ? wnd->rcClient : wnd->rcWindow;
    *out={r.left,r.top,r.right,r.bottom};
    return 1;
}
int32_t MicaWin32kGetClientRect(void* hwnd, MicaWin32kRect* out) {
    return CopyRect(hwnd,out,true);
}
int32_t MicaWin32kGetWindowRect(void* hwnd, MicaWin32kRect* out) {
    return CopyRect(hwnd,out,false);
}
int32_t MicaWin32kEnumWindows(void* callback, intptr_t parameter) {
    if (!callback) return 0;
    using EnumProc = int32_t (*)(void*, intptr_t);
    auto proc = reinterpret_cast<EnumProc>(callback);
    const auto windows = TitanWin32kSubsystem::Instance().enumerateTopLevelWindows();
    for (void* hwnd : windows) {
        if (!proc(hwnd, parameter)) return 0;
    }
    return 1;
}
int32_t MicaWin32kGetWindowTextW(void* hwnd, wchar_t* output, int32_t capacity) {
    return TitanWin32kSubsystem::Instance().getWindowText(hwnd, output, capacity);
}
int32_t MicaWin32kIsWindowVisible(void* hwnd) {
    return TitanWin32kSubsystem::Instance().isWindowVisible(hwnd);
}
int32_t MicaWin32kIsIconic(void* hwnd) {
    return TitanWin32kSubsystem::Instance().isIconic(hwnd);
}
void* MicaWin32kGetForegroundWindow() {
    return TitanWin32kSubsystem::Instance().getForegroundWindow();
}
}
