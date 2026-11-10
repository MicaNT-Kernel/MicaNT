#pragma once

#include <windows.h>
#include <cstddef>

namespace micant::explorer {

constexpr size_t kMaxTaskSwitchEntries = 8;

struct TaskSwitchEntry {
    HWND hwnd{};
    char title[96]{};
    bool minimized{};
    bool active{};
};

struct TaskSwitchSnapshot {
    TaskSwitchEntry entries[kMaxTaskSwitchEntries]{};
    size_t count{};
};

// Snapshot visible top-level application windows via the documented USER32 API.
void BuildTaskSwitchSnapshot(TaskSwitchSnapshot* snapshot, HWND desktop,
                             HWND taskbar, HWND startMenu) noexcept;

} // namespace micant::explorer
