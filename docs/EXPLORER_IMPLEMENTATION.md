# MicaNT Explorer implementation

MicaNT implements its desktop shell in its own codebase against the windowing
ABI in `include/micant/shared/win32k_client.h`. ReactOS Explorer is used only as
a structural reference. No ReactOS source, assets, Wine code, Wine runtime, or
third-party import libraries are part of this implementation.

## Functional structure

The ReactOS Explorer layout separates startup, the desktop surface, the tray
window, task buttons, task switching, the Start menu, and notification-area
items. MicaNT follows those functional boundaries while using its existing
freestanding C++ and Win32k APIs:

| MicaNT block | Responsibility | State |
| --- | --- | --- |
| `tools/explorer_shell_app.cpp` | Shell startup, desktop/window state, event routing | Existing shell; still being split into focused blocks |
| `tools/explorer_shell/taskbar.hpp` and `.cpp` | Topmost taskbar window, painting from a snapshot of open shell tasks | Implemented; task tracking currently covers Explorer-owned windows |
| `tools/explorer_shell/start_menu.hpp` and `.cpp` | Start menu painting and explicit hit-test actions | Implemented; application launching/search still need shell services |
| `tools/explorer_shell/desktop.hpp` and `.cpp` | Desktop background, icons, and icon hit-testing | Implemented as a separate desktop window/surface |
| `tools/explorer_shell/notification_area.*` | Clock and notification icons | Pending |
| `tools/explorer_shell/task_switcher.*` | Window enumeration, activation, minimize/restore | Pending |

The new blocks must use MicaNT's `MicaWin32k*` interface and local drawing
primitives. When a required operation is missing from that interface, implement
the operation in MicaNT's Win32k/API layer before wiring the shell to it.
