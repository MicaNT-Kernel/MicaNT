#include "micant/km/win32k/freestanding.hpp"
#include <atomic>

namespace micant::win32k::freestanding {
namespace {

constexpr size_t kWindowCapacity = MICANT_WIN32K_WINDOW_CAPACITY;
constexpr size_t kClassCapacity = 32;
constexpr size_t kMessageCapacity = 256;
constexpr uint64_t kHandleTag = 0x4D4E570000000000ULL;
constexpr uint32_t kWmQuit = 0x0012;
constexpr uint32_t kWmNcCreate = 0x0081;
constexpr int32_t kSwShowMinimized = 2;
constexpr int32_t kSwMinimize = 6;
constexpr int32_t kGwlpWndProc = -4;
constexpr int32_t kGwlpUserData = -21;
std::atomic_flag g_stateLock = ATOMIC_FLAG_INIT;

class StateLockGuard {
public:
    StateLockGuard() noexcept {
        while (g_stateLock.test_and_set(std::memory_order_acquire)) {
#if defined(__x86_64__)
            __asm__ __volatile__("pause");
#elif defined(__aarch64__)
            __asm__ __volatile__("yield");
#endif
        }
    }

    ~StateLockGuard() noexcept {
        g_stateLock.clear(std::memory_order_release);
    }

    StateLockGuard(const StateLockGuard&) = delete;
    StateLockGuard& operator=(const StateLockGuard&) = delete;
};

struct WindowClass {
    uint16_t name[MICANT_WIN32K_TEXT_CAPACITY]{};
    uint16_t atom{};
    bool active{};
};

struct Window {
    uint16_t title[MICANT_WIN32K_TEXT_CAPACITY]{};
    uint64_t handle{};
    uint64_t parent{};
    uint64_t menu{};
    uint64_t windowProcedure{};
    intptr_t userData{};
    int32_t x{}, y{}, width{}, height{};
    uint32_t style{}, extendedStyle{};
    uint16_t classAtom{};
    bool active{}, visible{}, iconic{};
};

struct QueuedMessage {
    uint64_t hwnd{}, wParam{};
    int64_t lParam{};
    uint32_t message{}, time{};
    int32_t x{}, y{};
    bool active{};
};

WindowClass g_classes[kClassCapacity]{};
Window g_windows[kWindowCapacity]{};
QueuedMessage g_messages[kMessageCapacity]{};
uint16_t g_nextAtom{1};
uint32_t g_nextGeneration{1};
uint32_t g_messageTime{};
uint64_t g_foregroundWindow{};
int32_t g_quitCode{};
bool g_quitPending{};

bool equalText(const uint16_t* first, const uint16_t* second) noexcept {
    for (size_t index = 0; index < MICANT_WIN32K_TEXT_CAPACITY; ++index) {
        if (first[index] != second[index]) return false;
        if (first[index] == 0) return true;
    }
    return false;
}

bool terminatedText(const uint16_t* text) noexcept {
    for (size_t index = 0; index < MICANT_WIN32K_TEXT_CAPACITY; ++index) {
        if (text[index] == 0) return true;
    }
    return false;
}

void copyText(uint16_t* destination, const uint16_t* source) noexcept {
    for (size_t index = 0; index < MICANT_WIN32K_TEXT_CAPACITY; ++index) {
        destination[index] = source[index];
        if (source[index] == 0) break;
    }
}

WindowClass* findClass(const uint16_t* name) noexcept {
    for (auto& windowClass : g_classes) {
        if (windowClass.active && equalText(windowClass.name, name)) {
            return &windowClass;
        }
    }
    return nullptr;
}

Window* findWindow(uint64_t handle) noexcept {
    if (!handle) return nullptr;
    for (auto& window : g_windows) {
        if (window.active && window.handle == handle) return &window;
    }
    return nullptr;
}

uint64_t makeHandle(size_t index) noexcept {
    uint64_t generation = g_nextGeneration++;
    if (generation == 0 || generation > 0x00FFFFFFu) {
        g_nextGeneration = 2;
        generation = 1;
    }
    return kHandleTag | (generation << 8) | static_cast<uint64_t>(index + 1);
}

bool enqueue(const QueuedMessage& message) noexcept {
    for (auto& slot : g_messages) {
        if (!slot.active) {
            slot = message;
            slot.active = true;
            return true;
        }
    }
    return false;
}

bool messageMatches(const QueuedMessage& message,
                    const MicaWin32kRequest& request) noexcept {
    const uint64_t requestedWindow = request.args[0];
    const uint32_t minimum = static_cast<uint32_t>(request.args[1]);
    const uint32_t maximum = static_cast<uint32_t>(request.args[2]);
    return (!requestedWindow || requestedWindow == message.hwnd) &&
           (!minimum || message.message >= minimum) &&
           (!maximum || message.message <= maximum);
}

bool dequeue(MicaWin32kRequest& request) noexcept {
    for (auto& message : g_messages) {
        if (!message.active || !messageMatches(message, request)) continue;
        request.messageHwnd = message.hwnd;
        request.messageWParam = message.wParam;
        request.messageLParam = message.lParam;
        request.messageId = message.message;
        request.messageTime = message.time;
        request.messageX = message.x;
        request.messageY = message.y;
        if (request.args[3]) message.active = false;
        return true;
    }
    if (g_quitPending &&
        (!request.args[0] || request.args[0] == 0) &&
        (!request.args[1] || kWmQuit >= request.args[1]) &&
        (!request.args[2] || kWmQuit <= request.args[2])) {
        request.messageHwnd = 0;
        request.messageWParam = static_cast<uint64_t>(g_quitCode);
        request.messageLParam = 0;
        request.messageId = kWmQuit;
        request.messageTime = ++g_messageTime;
        if (request.args[3]) g_quitPending = false;
        return true;
    }
    return false;
}

NtStatus registerClass(MicaWin32kRequest& request) noexcept {
    if (!terminatedText(request.className) || request.className[0] == 0) {
        return NtStatus::InvalidParameter;
    }
    if (findClass(request.className)) return NtStatus::ObjectNameCollision;
    for (auto& windowClass : g_classes) {
        if (windowClass.active) continue;
        windowClass.active = true;
        windowClass.atom = g_nextAtom++;
        if (g_nextAtom == 0) g_nextAtom = 1;
        copyText(windowClass.name, request.className);
        request.result = windowClass.atom;
        return NtStatus::Success;
    }
    return NtStatus::InsufficientResources;
}

NtStatus createWindow(MicaWin32kRequest& request) noexcept {
    if (!terminatedText(request.className) || !terminatedText(request.title)) {
        return NtStatus::InvalidParameter;
    }
    const auto* windowClass = findClass(request.className);
    if (!windowClass) return NtStatus::ObjectNameNotFound;
    const int64_t width = static_cast<int64_t>(request.args[4]);
    const int64_t height = static_cast<int64_t>(request.args[5]);
    if (width <= 0 || height <= 0 || width > 16384 || height > 16384) {
        return NtStatus::InvalidParameter;
    }
    for (size_t index = 0; index < kWindowCapacity; ++index) {
        auto& window = g_windows[index];
        if (window.active) continue;
        window = {};
        window.active = true;
        window.handle = makeHandle(index);
        window.parent = request.args[6];
        window.menu = request.args[7];
        window.extendedStyle = static_cast<uint32_t>(request.args[0]);
        window.style = static_cast<uint32_t>(request.args[1]);
        window.x = static_cast<int32_t>(request.args[2]);
        window.y = static_cast<int32_t>(request.args[3]);
        window.width = static_cast<int32_t>(width);
        window.height = static_cast<int32_t>(height);
        window.classAtom = windowClass->atom;
        window.visible = (window.style & 0x10000000u) != 0;
        copyText(window.title, request.title);
        request.result = window.handle;
        if (window.visible) g_foregroundWindow = window.handle;
        return NtStatus::Success;
    }
    return NtStatus::InsufficientResources;
}

} // namespace

NtStatus dispatchRequest(MicaWin32kRequest& request) noexcept {
    StateLockGuard lock;
    request.result = 0;
    switch (request.operation) {
        case MICANT_WIN32K_OP_REGISTER_CLASS:
            return registerClass(request);
        case MICANT_WIN32K_OP_CREATE_WINDOW:
            return createWindow(request);
        case MICANT_WIN32K_OP_DESTROY_WINDOW: {
            auto* window = findWindow(request.args[0]);
            if (!window) return NtStatus::InvalidHandle;
            window->active = false;
            if (g_foregroundWindow == window->handle) g_foregroundWindow = 0;
            request.result = 1;
            return NtStatus::Success;
        }
        case MICANT_WIN32K_OP_SHOW_WINDOW: {
            auto* window = findWindow(request.args[0]);
            if (!window) return NtStatus::InvalidHandle;
            const int32_t command = static_cast<int32_t>(request.args[1]);
            const bool wasVisible = window->visible;
            window->visible = command != 0;
            window->iconic = command == kSwShowMinimized ||
                             command == kSwMinimize;
            if (window->visible) g_foregroundWindow = window->handle;
            request.result = wasVisible;
            return NtStatus::Success;
        }
        case MICANT_WIN32K_OP_SET_WINDOW_POS: {
            auto* window = findWindow(request.args[0]);
            if (!window) return NtStatus::InvalidHandle;
            const uint32_t flags = static_cast<uint32_t>(request.args[5]);
            if (!(flags & 0x0002u)) {
                window->x = static_cast<int32_t>(request.args[1]);
                window->y = static_cast<int32_t>(request.args[2]);
            }
            if (!(flags & 0x0001u)) {
                const int64_t width = static_cast<int64_t>(request.args[3]);
                const int64_t height = static_cast<int64_t>(request.args[4]);
                if (width <= 0 || height <= 0 || width > 16384 || height > 16384) {
                    return NtStatus::InvalidParameter;
                }
                window->width = static_cast<int32_t>(width);
                window->height = static_cast<int32_t>(height);
            }
            request.result = 1;
            return NtStatus::Success;
        }
        case MICANT_WIN32K_OP_POST_MESSAGE: {
            if (request.args[0] && !findWindow(request.args[0])) {
                return NtStatus::InvalidHandle;
            }
            const QueuedMessage message{
                .hwnd = request.args[0],
                .wParam = request.args[2],
                .lParam = static_cast<int64_t>(request.args[3]),
                .message = static_cast<uint32_t>(request.args[1]),
                .time = ++g_messageTime,
                .x = 0,
                .y = 0,
                .active = true,
            };
            if (!enqueue(message)) return NtStatus::InsufficientResources;
            request.result = 1;
            return NtStatus::Success;
        }
        case MICANT_WIN32K_OP_SEND_MESSAGE:
            if (request.args[0] && !findWindow(request.args[0])) {
                return NtStatus::InvalidHandle;
            }
            return NtStatus::Success;
        case MICANT_WIN32K_OP_POST_QUIT:
            g_quitCode = static_cast<int32_t>(request.args[0]);
            g_quitPending = true;
            return NtStatus::Success;
        case MICANT_WIN32K_OP_PEEK_MESSAGE:
        case MICANT_WIN32K_OP_GET_MESSAGE:
            if (dequeue(request)) {
                request.result = 1;
                return NtStatus::Success;
            }
            return request.operation == MICANT_WIN32K_OP_GET_MESSAGE
                ? NtStatus::Timeout : NtStatus::Success;
        case MICANT_WIN32K_OP_GET_CLIENT_RECT:
        case MICANT_WIN32K_OP_GET_WINDOW_RECT: {
            const auto* window = findWindow(request.args[0]);
            if (!window) return NtStatus::InvalidHandle;
            request.rect = request.operation == MICANT_WIN32K_OP_GET_CLIENT_RECT
                ? MicaWin32kRect{0, 0, window->width, window->height}
                : MicaWin32kRect{window->x, window->y,
                    window->x + window->width, window->y + window->height};
            request.result = 1;
            return NtStatus::Success;
        }
        case MICANT_WIN32K_OP_ENUM_WINDOWS: {
            for (const auto& window : g_windows) {
                if (!window.active || window.parent) continue;
                request.windowHandles[request.result++] = window.handle;
            }
            return NtStatus::Success;
        }
        case MICANT_WIN32K_OP_GET_WINDOW_TEXT: {
            const auto* window = findWindow(request.args[0]);
            if (!window) return NtStatus::InvalidHandle;
            copyText(request.text, window->title);
            while (request.result < MICANT_WIN32K_TEXT_CAPACITY &&
                   request.text[request.result] != 0) {
                ++request.result;
            }
            return NtStatus::Success;
        }
        case MICANT_WIN32K_OP_IS_WINDOW_VISIBLE: {
            const auto* window = findWindow(request.args[0]);
            if (!window) return NtStatus::InvalidHandle;
            request.result = window->visible;
            return NtStatus::Success;
        }
        case MICANT_WIN32K_OP_IS_ICONIC: {
            const auto* window = findWindow(request.args[0]);
            if (!window) return NtStatus::InvalidHandle;
            request.result = window->iconic;
            return NtStatus::Success;
        }
        case MICANT_WIN32K_OP_GET_FOREGROUND_WINDOW:
            request.result = findWindow(g_foregroundWindow)
                ? g_foregroundWindow : 0;
            return NtStatus::Success;
        case MICANT_WIN32K_OP_SET_WINDOW_LONG: {
            auto* window = findWindow(request.args[0]);
            if (!window) return NtStatus::InvalidHandle;
            const int32_t index = static_cast<int32_t>(request.args[1]);
            if (index == kGwlpUserData) {
                request.result = static_cast<uint64_t>(window->userData);
                window->userData = static_cast<intptr_t>(request.args[2]);
            } else if (index == kGwlpWndProc) {
                request.result = window->windowProcedure;
                window->windowProcedure = request.args[2];
            } else {
                return NtStatus::InvalidParameter;
            }
            return NtStatus::Success;
        }
        case MICANT_WIN32K_OP_GET_WINDOW_LONG: {
            const auto* window = findWindow(request.args[0]);
            if (!window) return NtStatus::InvalidHandle;
            const int32_t index = static_cast<int32_t>(request.args[1]);
            if (index == kGwlpUserData) {
                request.result = static_cast<uint64_t>(window->userData);
            } else if (index == kGwlpWndProc) {
                request.result = window->windowProcedure;
            } else {
                return NtStatus::InvalidParameter;
            }
            return NtStatus::Success;
        }
        case MICANT_WIN32K_OP_DEF_WINDOW_PROC:
            if (request.args[0] && !findWindow(request.args[0])) {
                return NtStatus::InvalidHandle;
            }
            request.result = static_cast<uint32_t>(request.args[1]) == kWmNcCreate;
            return NtStatus::Success;
        default:
            return NtStatus::NotSupported;
    }
}

} // namespace micant::win32k::freestanding
