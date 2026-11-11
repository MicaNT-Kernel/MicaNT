#ifndef MICANT_WIN32K_EXPORTS
#define MICANT_WIN32K_EXPORTS
#endif

#include "micant/shared/user_graphics.hpp"
#include "micant/shared/user_syscall.hpp"
#include "micant/shared/win32k_client.h"

#include <cstdint>

#if defined(BUILD_SYSTEM32_STANDALONE_DLL)
extern "C" bool DllMain(void*, unsigned long, void*) noexcept { return true; }
#endif

namespace {

bool request(uint32_t operation, MicaWin32kRequest& value) noexcept {
    value.size = sizeof(value);
    value.version = MICANT_WIN32K_REQUEST_VERSION;
    value.operation = operation;
    return micant::sys::invokeUserService(
        0x86, reinterpret_cast<uint64_t>(&value)) == 0;
}

bool copyText(uint16_t* destination, const wchar_t* source) noexcept {
    if (!source) {
        destination[0] = 0;
        return true;
    }
    for (size_t index = 0; index < MICANT_WIN32K_TEXT_CAPACITY; ++index) {
        const auto character = static_cast<uint16_t>(source[index]);
        destination[index] = character;
        if (!character) return true;
    }
    destination[MICANT_WIN32K_TEXT_CAPACITY - 1] = 0;
    return false;
}

intptr_t callWindowProcedure(void* hwnd, uint32_t message,
                             uintptr_t wParam, intptr_t lParam) noexcept {
    MicaWin32kRequest value{};
    value.args[0] = reinterpret_cast<uint64_t>(hwnd);
    value.args[1] = static_cast<uint64_t>(-4);
    if (!request(MICANT_WIN32K_OP_GET_WINDOW_LONG, value)) return 0;
    using WindowProcedure = intptr_t (*)(void*, uint32_t, uintptr_t, intptr_t);
    auto procedure = reinterpret_cast<WindowProcedure>(value.result);
    return procedure ? procedure(hwnd, message, wParam, lParam)
                     : MicaWin32kDefWindowProcW(hwnd, message, wParam, lParam);
}

void copyMessage(const MicaWin32kRequest& source,
                 MicaWin32kMessage* destination) noexcept {
    destination->hwnd = reinterpret_cast<void*>(source.messageHwnd);
    destination->message = source.messageId;
    destination->wParam = static_cast<uintptr_t>(source.messageWParam);
    destination->lParam = static_cast<intptr_t>(source.messageLParam);
    destination->time = source.messageTime;
    destination->pt = {source.messageX, source.messageY};
}

}

extern "C" {

void* MicaWin32kCreateWindowExW(uint32_t exStyle, const wchar_t* className,
    const wchar_t* title, uint32_t style, int32_t x, int32_t y, int32_t width,
    int32_t height, void* parent, void* menu, void* instance, void* parameter) {
    MicaWin32kRequest value{};
    if (!copyText(value.className, className) ||
        !copyText(value.title, title)) return nullptr;
    value.args[0] = exStyle;
    value.args[1] = style;
    value.args[2] = static_cast<uint64_t>(static_cast<int64_t>(x));
    value.args[3] = static_cast<uint64_t>(static_cast<int64_t>(y));
    value.args[4] = static_cast<uint64_t>(static_cast<int64_t>(width));
    value.args[5] = static_cast<uint64_t>(static_cast<int64_t>(height));
    value.args[6] = reinterpret_cast<uint64_t>(parent);
    value.args[7] = reinterpret_cast<uint64_t>(menu);
    if (!request(MICANT_WIN32K_OP_CREATE_WINDOW, value) || !value.result) {
        return nullptr;
    }
    void* hwnd = reinterpret_cast<void*>(value.result);
    MicaWin32kCreateStruct create{parameter, instance, menu, parent, height,
        width, y, x, static_cast<int32_t>(style), title, className, exStyle};
    if (callWindowProcedure(hwnd, 0x0081, 1,
                            reinterpret_cast<intptr_t>(&create)) == 0 ||
        callWindowProcedure(hwnd, 0x0001, 0,
                            reinterpret_cast<intptr_t>(&create)) == -1) {
        MicaWin32kDestroyWindow(hwnd);
        return nullptr;
    }
    return hwnd;
}

int32_t MicaWin32kDestroyWindow(void* hwnd) {
    MicaWin32kRequest value{};
    value.args[0] = reinterpret_cast<uint64_t>(hwnd);
    return request(MICANT_WIN32K_OP_DESTROY_WINDOW, value) && value.result;
}

int32_t MicaWin32kShowWindow(void* hwnd, int32_t command) {
    MicaWin32kRequest value{};
    value.args[0] = reinterpret_cast<uint64_t>(hwnd);
    value.args[1] = static_cast<uint64_t>(static_cast<int64_t>(command));
    return request(MICANT_WIN32K_OP_SHOW_WINDOW, value)
        ? static_cast<int32_t>(value.result) : 0;
}

int32_t MicaWin32kSetWindowPos(void* hwnd, void* after, int32_t x,
    int32_t y, int32_t width, int32_t height, uint32_t flags) {
    (void)after;
    MicaWin32kRequest value{};
    value.args[0] = reinterpret_cast<uint64_t>(hwnd);
    value.args[1] = static_cast<uint64_t>(static_cast<int64_t>(x));
    value.args[2] = static_cast<uint64_t>(static_cast<int64_t>(y));
    value.args[3] = static_cast<uint64_t>(static_cast<int64_t>(width));
    value.args[4] = static_cast<uint64_t>(static_cast<int64_t>(height));
    value.args[5] = flags;
    return request(MICANT_WIN32K_OP_SET_WINDOW_POS, value) && value.result;
}

int32_t MicaWin32kPostMessageW(void* hwnd, uint32_t message,
                               uintptr_t wParam, intptr_t lParam) {
    MicaWin32kRequest value{};
    value.args[0] = reinterpret_cast<uint64_t>(hwnd);
    value.args[1] = message;
    value.args[2] = wParam;
    value.args[3] = static_cast<uint64_t>(lParam);
    return request(MICANT_WIN32K_OP_POST_MESSAGE, value) && value.result;
}

intptr_t MicaWin32kSendMessageW(void* hwnd, uint32_t message,
                                uintptr_t wParam, intptr_t lParam) {
    if (message == 0x0010) {
        MicaWin32kPostMessageW(hwnd, 0x0010, wParam, lParam);
        return 1;
    }
    return callWindowProcedure(hwnd, message, wParam, lParam);
}

void MicaWin32kPostQuitMessage(int32_t exitCode) {
    MicaWin32kRequest value{};
    value.args[0] = static_cast<uint64_t>(static_cast<int64_t>(exitCode));
    request(MICANT_WIN32K_OP_POST_QUIT, value);
}

uint16_t MicaWin32kRegisterClassW(const wchar_t* className, void* procedure) {
    MicaWin32kRequest value{};
    if (!copyText(value.className, className)) return 0;
    value.args[0] = reinterpret_cast<uint64_t>(procedure);
    return request(MICANT_WIN32K_OP_REGISTER_CLASS, value)
        ? static_cast<uint16_t>(value.result) : 0;
}

intptr_t MicaWin32kDispatchMessageW(const MicaWin32kMessage* message) {
    if (!message) return 0;
    return callWindowProcedure(message->hwnd, message->message,
                               message->wParam, message->lParam);
}

intptr_t MicaWin32kSetWindowLongPtrW(void* hwnd, int32_t index,
                                     intptr_t newValue) {
    MicaWin32kRequest value{};
    value.args[0] = reinterpret_cast<uint64_t>(hwnd);
    value.args[1] = static_cast<uint64_t>(static_cast<int64_t>(index));
    value.args[2] = static_cast<uint64_t>(newValue);
    return request(MICANT_WIN32K_OP_SET_WINDOW_LONG, value)
        ? static_cast<intptr_t>(value.result) : 0;
}

intptr_t MicaWin32kGetWindowLongPtrW(void* hwnd, int32_t index) {
    MicaWin32kRequest value{};
    value.args[0] = reinterpret_cast<uint64_t>(hwnd);
    value.args[1] = static_cast<uint64_t>(static_cast<int64_t>(index));
    return request(MICANT_WIN32K_OP_GET_WINDOW_LONG, value)
        ? static_cast<intptr_t>(value.result) : 0;
}

intptr_t MicaWin32kDefWindowProcW(void* hwnd, uint32_t message,
                                  uintptr_t wParam, intptr_t lParam) {
    MicaWin32kRequest value{};
    value.args[0] = reinterpret_cast<uint64_t>(hwnd);
    value.args[1] = message;
    value.args[2] = wParam;
    value.args[3] = static_cast<uint64_t>(lParam);
    return request(MICANT_WIN32K_OP_DEF_WINDOW_PROC, value)
        ? static_cast<intptr_t>(value.result) : 0;
}

int32_t MicaWin32kPeekMessageW(MicaWin32kMessage* output, void* hwnd,
    uint32_t minimum, uint32_t maximum, uint32_t remove) {
    if (!output) return 0;
    MicaWin32kRequest value{};
    value.args[0] = reinterpret_cast<uint64_t>(hwnd);
    value.args[1] = minimum;
    value.args[2] = maximum;
    value.args[3] = remove;
    if (!request(MICANT_WIN32K_OP_PEEK_MESSAGE, value) || !value.result) return 0;
    copyMessage(value, output);
    return 1;
}

int32_t MicaWin32kGetMessageW(MicaWin32kMessage* output, void* hwnd,
    uint32_t minimum, uint32_t maximum) {
    if (!output) return -1;
    MicaWin32kRequest value{};
    value.args[0] = reinterpret_cast<uint64_t>(hwnd);
    value.args[1] = minimum;
    value.args[2] = maximum;
    if (!request(MICANT_WIN32K_OP_GET_MESSAGE, value)) return -1;
    if (!value.result) return 0;
    copyMessage(value, output);
    return value.messageId == 0x0012 ? 0 : 1;
}

int32_t MicaWin32kGetClientRect(void* hwnd, MicaWin32kRect* output) {
    if (!output) return 0;
    MicaWin32kRequest value{};
    value.args[0] = reinterpret_cast<uint64_t>(hwnd);
    if (!request(MICANT_WIN32K_OP_GET_CLIENT_RECT, value) || !value.result) return 0;
    *output = value.rect;
    return 1;
}

int32_t MicaWin32kGetWindowRect(void* hwnd, MicaWin32kRect* output) {
    if (!output) return 0;
    MicaWin32kRequest value{};
    value.args[0] = reinterpret_cast<uint64_t>(hwnd);
    if (!request(MICANT_WIN32K_OP_GET_WINDOW_RECT, value) || !value.result) return 0;
    *output = value.rect;
    return 1;
}

int32_t MicaWin32kEnumWindows(void* callback, intptr_t parameter) {
    if (!callback) return 0;
    MicaWin32kRequest value{};
    if (!request(MICANT_WIN32K_OP_ENUM_WINDOWS, value)) return 0;
    using Callback = int32_t (*)(void*, intptr_t);
    auto enumerate = reinterpret_cast<Callback>(callback);
    for (uint64_t index = 0; index < value.result; ++index) {
        if (!enumerate(reinterpret_cast<void*>(value.windowHandles[index]),
                       parameter)) return 0;
    }
    return 1;
}

int32_t MicaWin32kGetWindowTextW(void* hwnd, wchar_t* output,
                                 int32_t capacity) {
    if (!output || capacity <= 0) return 0;
    MicaWin32kRequest value{};
    value.args[0] = reinterpret_cast<uint64_t>(hwnd);
    if (!request(MICANT_WIN32K_OP_GET_WINDOW_TEXT, value)) return 0;
    const uint64_t count = value.result < static_cast<uint64_t>(capacity - 1)
        ? value.result : static_cast<uint64_t>(capacity - 1);
    for (uint64_t index = 0; index < count; ++index) output[index] = value.text[index];
    output[count] = 0;
    return static_cast<int32_t>(count);
}

int32_t MicaWin32kIsWindowVisible(void* hwnd) {
    MicaWin32kRequest value{};
    value.args[0] = reinterpret_cast<uint64_t>(hwnd);
    return request(MICANT_WIN32K_OP_IS_WINDOW_VISIBLE, value)
        ? static_cast<int32_t>(value.result) : 0;
}

int32_t MicaWin32kIsIconic(void* hwnd) {
    MicaWin32kRequest value{};
    value.args[0] = reinterpret_cast<uint64_t>(hwnd);
    return request(MICANT_WIN32K_OP_IS_ICONIC, value)
        ? static_cast<int32_t>(value.result) : 0;
}

void* MicaWin32kGetForegroundWindow() {
    MicaWin32kRequest value{};
    return request(MICANT_WIN32K_OP_GET_FOREGROUND_WINDOW, value)
        ? reinterpret_cast<void*>(value.result) : nullptr;
}

void* MicaWin32kGetDC(void*) { return nullptr; }
int32_t MicaWin32kReleaseDC(void*, void*) { return 0; }
void* MicaWin32kCreateCompatibleDC(void*) { return nullptr; }
void* MicaWin32kCreateCompatibleBitmap(void*, int32_t, int32_t) { return nullptr; }
void* MicaWin32kSelectObject(void*, void*) { return nullptr; }
int32_t MicaWin32kBitBlt(void*, int32_t, int32_t, int32_t, int32_t,
                         void*, int32_t, int32_t, uint32_t) { return 0; }
int32_t MicaWin32kPresentDC(void*) { return 0; }
int32_t MicaWin32kDeleteObject(void*) { return 0; }
void* MicaWin32kGetStockObject(int32_t) { return nullptr; }
void* MicaWin32kCreateDibSection(void*, int32_t, int32_t, void** bits) {
    if (bits) *bits = nullptr;
    return nullptr;
}

}
