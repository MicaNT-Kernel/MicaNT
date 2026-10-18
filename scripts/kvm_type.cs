using System;
using System.Runtime.InteropServices;
using System.Threading;
using System.Text;

class Program {
    [DllImport("user32.dll")]
    static extern IntPtr OpenInputDesktop(uint dwFlags, bool fInherit, uint dwDesiredAccess);

    [DllImport("user32.dll")]
    static extern bool SetThreadDesktop(IntPtr hDesktop);

    [DllImport("user32.dll")]
    static extern bool EnumWindows(EnumWindowsProc lpEnumFunc, IntPtr lParam);
    delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);

    [DllImport("user32.dll")]
    static extern int GetWindowText(IntPtr hWnd, StringBuilder lpString, int nMaxCount);

    [DllImport("user32.dll")]
    static extern bool IsWindowVisible(IntPtr hWnd);

    [DllImport("user32.dll")]
    static extern bool SetForegroundWindow(IntPtr hWnd);

    [DllImport("user32.dll")]
    static extern bool ShowWindow(IntPtr hWnd, int nCmdShow);

    [DllImport("user32.dll")]
    static extern short VkKeyScan(char ch);

    [DllImport("user32.dll")]
    static extern uint MapVirtualKey(uint uCode, uint uMapType);

    [DllImport("user32.dll")]
    static extern void keybd_event(byte bVk, byte bScan, uint dwFlags, int dwExtraInfo);

    [DllImport("user32.dll")]
    static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);

    [DllImport("user32.dll")]
    static extern bool SetCursorPos(int X, int Y);

    [DllImport("user32.dll")]
    static extern void mouse_event(uint dwFlags, uint dx, uint dy, uint dwData, int dwExtraInfo);

    [StructLayout(LayoutKind.Sequential)]
    struct RECT {
        public int Left, Top, Right, Bottom;
    }

    const uint MOUSEEVENTF_LEFTDOWN = 0x0002;
    const uint MOUSEEVENTF_LEFTUP   = 0x0004;
    const uint KEYEVENTF_KEYUP      = 0x0002;

    static void Main(string[] args) {
        if (args.Length == 0) {
            Console.WriteLine("Usage: kvm_type.exe <string_to_type>");
            return;
        }

        string text = string.Join(" ", args);

        try {
            IntPtr hInp = OpenInputDesktop(0, false, 0x01FF);
            if (hInp != IntPtr.Zero) {
                SetThreadDesktop(hInp);
            }
        } catch {}

        IntPtr jviewerHwnd = IntPtr.Zero;
        EnumWindows((hWnd, lParam) => {
            if (IsWindowVisible(hWnd)) {
                StringBuilder sb = new StringBuilder(256);
                GetWindowText(hWnd, sb, 256);
                string title = sb.ToString();
                if (title.IndexOf("JViewer", StringComparison.OrdinalIgnoreCase) >= 0) {
                    jviewerHwnd = hWnd;
                    return false;
                }
            }
            return true;
        }, IntPtr.Zero);

        if (jviewerHwnd == IntPtr.Zero) {
            Console.WriteLine("Error: JViewer window not found!");
            return;
        }

        bool doMaximize = false;
        bool doScroll = false;
        if (text.Contains("--maximize")) {
            doMaximize = true;
            text = text.Replace("--maximize", "").Trim();
        }
        if (text.Contains("--scroll")) {
            doScroll = true;
            text = text.Replace("--scroll", "").Trim();
        }

        ShowWindow(jviewerHwnd, doMaximize ? 3 : 9); // 3 = SW_MAXIMIZE
        SetForegroundWindow(jviewerHwnd);
        Thread.Sleep(200);

        RECT r;
        GetWindowRect(jviewerHwnd, out r);
        int clickX = r.Left + 500;
        int clickY = r.Top + 400;
        SetCursorPos(clickX, clickY);
        mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
        Thread.Sleep(30);
        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
        Thread.Sleep(150);

        if (doScroll) {
            // Scroll down in console area
            for (int i = 0; i < 5; ++i) {
                mouse_event(0x0800, 0, 0, unchecked((uint)-120), 0); // MOUSEEVENTF_WHEEL -120
                Thread.Sleep(50);
            }
        }

        if (string.IsNullOrEmpty(text)) {
            Console.WriteLine("Window manipulated successfully.");
            return;
        }

        // Replace literal \r, \n, \enter with actual control chars
        text = text.Replace("\\r\\n", "\r").Replace("\\n", "\r").Replace("\\r", "\r");
        if (!text.EndsWith("\r") && !text.EndsWith("\n")) {
            text += "\r";
        }

        Console.WriteLine("Typing: " + text.TrimEnd());

        foreach (char c in text) {
            if (c == '\r' || c == '\n') {
                byte vk = 0x0D; // VK_RETURN
                byte scan = (byte)MapVirtualKey(vk, 0);
                keybd_event(vk, scan, 0, 0);
                Thread.Sleep(50);
                keybd_event(vk, scan, KEYEVENTF_KEYUP, 0);
                Thread.Sleep(100);
                continue;
            }

            if (c == 27) { // ESC
                byte vk = 0x1B;
                byte scan = 0x01;
                keybd_event(vk, scan, 0, 0);
                Thread.Sleep(40);
                keybd_event(vk, scan, KEYEVENTF_KEYUP, 0);
                Thread.Sleep(80);
                continue;
            }

            short vkVal = VkKeyScan(c);
            if (vkVal == -1) continue;

            byte virtualKey = (byte)(vkVal & 0xFF);
            byte shiftState = (byte)((vkVal >> 8) & 0xFF);
            byte scanCode = (byte)MapVirtualKey(virtualKey, 0);

            if ((shiftState & 1) != 0) {
                keybd_event(0x10, (byte)MapVirtualKey(0x10, 0), 0, 0);
                Thread.Sleep(20);
            }

            keybd_event(virtualKey, scanCode, 0, 0);
            Thread.Sleep(35);
            keybd_event(virtualKey, scanCode, KEYEVENTF_KEYUP, 0);
            Thread.Sleep(45);

            if ((shiftState & 1) != 0) {
                keybd_event(0x10, (byte)MapVirtualKey(0x10, 0), KEYEVENTF_KEYUP, 0);
                Thread.Sleep(20);
            }
        }

        Console.WriteLine("Done typing!");
    }
}
