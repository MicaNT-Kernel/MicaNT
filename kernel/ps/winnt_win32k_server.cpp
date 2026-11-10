#ifndef MICANT_WIN32K_EXPORTS
#define MICANT_WIN32K_EXPORTS
#endif
#include "micant/shared/win32k_client.h"
#include "micant/km/win32k/win32k.hpp"

using namespace micant::win32k;

// The user-mode Win32k service owns its own dispatcher instance; the kernel's
// standard syscall table is initialized by ntoskrnl and must not be linked here.
namespace micant::sys {
void SyscallDispatcher::initializeStandardTable() {}
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
    return NtUserDestroyWindow(hwnd);
}
int32_t MicaWin32kShowWindow(void* hwnd, int32_t cmd) {
    return NtUserShowWindow(hwnd, cmd);
}
int32_t MicaWin32kSetWindowPos(void* hwnd, void* after,
    int32_t x, int32_t y, int32_t w, int32_t h, uint32_t flags) {
    return NtUserSetWindowPos(hwnd, after, x, y, w, h, flags);
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
    return NtUserDefWindowProc(hwnd,message,wParam,lParam);
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
}
