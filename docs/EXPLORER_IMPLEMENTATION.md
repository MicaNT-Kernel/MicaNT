# MicaNT Explorer implementation

MicaNT implements its desktop shell in its own codebase and links against the
standard `user32.dll` and `gdi32.dll` API names. Those MicaNT DLLs route the
documented Win32 calls through the private window manager backend. ReactOS
Explorer is used only as a structural reference. No ReactOS source, assets,
Wine code, Wine runtime, or external runtime libraries are part of this
implementation.

## Functional structure

The ReactOS Explorer layout separates startup, the desktop surface, the tray
window, task buttons, task switching, the Start menu, and notification-area
items. MicaNT follows those functional boundaries while using its existing
freestanding C++ and Win32k APIs:

| MicaNT block | Responsibility | State |
| --- | --- | --- |
| `tools/explorer_shell_app.cpp` | Shell startup, desktop/window state, event routing | Uses user32/gdi32 imports; owns independent desktop, taskbar, menu, and window surfaces |
| `tools/explorer_shell/taskbar.hpp` and `.cpp` | Topmost taskbar window, painting from a snapshot of open shell tasks | Tracks Explorer-owned windows; activation, minimize/restore, and resize state feed task buttons |
| `tools/explorer_shell/start_menu.hpp` and `.cpp` | Start menu painting and explicit hit-test actions | Implemented; application launching/search still need shell services |
| `tools/explorer_shell/desktop.hpp` and `.cpp` | Desktop background, icons, and icon hit-testing | Implemented as a separate desktop window/surface |
| `tools/explorer_shell/notification_area.hpp` and `.cpp` | Clock/network/sound tray rendering and hit testing | Clock and network open their shell views; volume is currently an indicator because there is no audio endpoint API |
| `tools/explorer_shell/task_switcher.*` | Window enumeration, activation, minimize/restore | OS-wide enumeration is pending; the taskbar currently tracks windows owned by Explorer |

The executable is built by `scripts/stage_micant.py` as `build/explorer.exe` and
staged into `C:\MicaNT\System32`. From the current MicaNT command shell it can
be started with `run explorer.exe`. It is not a logon auto-start shell yet:
the current ring-3 boot path runs one user image synchronously and does not yet
provide the process/session startup path needed to keep winlogon and Explorer
running as separate processes.
