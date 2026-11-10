#include "task_switcher.hpp"

namespace micant::explorer {
namespace {

struct EnumContext {
    TaskSwitchSnapshot* snapshot;
    HWND desktop;
    HWND taskbar;
    HWND startMenu;
    HWND foreground;
};

BOOL CALLBACK collectWindow(HWND hwnd, LPARAM parameter) {
    auto* context = reinterpret_cast<EnumContext*>(parameter);
    if (!context || !context->snapshot || hwnd == context->desktop ||
        hwnd == context->taskbar || hwnd == context->startMenu ||
        !IsWindowVisible(hwnd) || context->snapshot->count >= kMaxTaskSwitchEntries) {
        return TRUE;
    }

    wchar_t wideTitle[96]{};
    const int titleLength = GetWindowTextW(hwnd, wideTitle, static_cast<int>(sizeof(wideTitle) / sizeof(wideTitle[0])));
    if (titleLength <= 0) return TRUE;

    TaskSwitchEntry& entry = context->snapshot->entries[context->snapshot->count];
    entry.hwnd = hwnd;
    for (int i = 0; i < titleLength && i < static_cast<int>(sizeof(entry.title) - 1); ++i) {
        const wchar_t ch = wideTitle[i];
        entry.title[i] = ch >= 32 && ch <= 126 ? static_cast<char>(ch) : '?';
    }
    entry.title[sizeof(entry.title) - 1] = '\0';
    entry.minimized = IsIconic(hwnd) != FALSE;
    entry.active = hwnd == context->foreground && !entry.minimized;
    ++context->snapshot->count;
    return TRUE;
}

} // namespace

void BuildTaskSwitchSnapshot(TaskSwitchSnapshot* snapshot, HWND desktop,
                             HWND taskbar, HWND startMenu) noexcept {
    if (!snapshot) return;
    *snapshot = {};
    EnumContext context{snapshot, desktop, taskbar, startMenu, GetForegroundWindow()};
    EnumWindows(collectWindow, reinterpret_cast<LPARAM>(&context));
}

} // namespace micant::explorer
