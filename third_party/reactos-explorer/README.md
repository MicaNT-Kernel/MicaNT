# ReactOS Explorer source import

This directory contains the complete `base/shell/explorer` source tree from ReactOS commit `8fe3cee05606aecd9442674e40038649f048d10d` (upstream: https://github.com/reactos/reactos/tree/8fe3cee05606aecd9442674e40038649f048d10d/base/shell/explorer).

The imported files retain their upstream copyright and license notices. Upstream Explorer files are LGPL-2.1-or-later; `COPYING.LESSER` is the corresponding license text. MicaNT remains MIT-licensed; the imported component is kept as a separately identified third-party source component.

## MicaNT port status

The upstream application is not a drop-in build for MicaNT. Its ReactOS build target depends on ReactOS ATL/Wine support libraries and the `setupapi`, `comctl32`, `browseui`, `shdocvw`, `uxtheme`, and full shell32 API surfaces. MicaNT currently builds its own `explorer.exe` against `win32k.dll`; its shell32 DLL and several imported APIs are still stubs. Keep the existing shell as the bootable fallback while the compatibility layer and build target are ported.
