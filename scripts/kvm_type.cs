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

    [DllImport("user32.dll")]
    static extern bool PostMessage(IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam);

    [DllImport("user32.dll")]
    static extern bool EnumChildWindows(IntPtr hWndParent, EnumWindowsProc lpEnumFunc, IntPtr lParam);

    const uint MOUSEEVENTF_LEFTDOWN = 0x0002;
    const uint MOUSEEVENTF_LEFTUP   = 0x0004;
    const uint KEYEVENTF_KEYUP      = 0x0002;
    const uint WM_KEYDOWN           = 0x0100;
    const uint WM_KEYUP             = 0x0101;

    static void SendCleanKey(byte vk, byte scan) {
        keybd_event(vk, scan, 0, 0);
        Thread.Sleep(50);
        keybd_event(vk, scan, KEYEVENTF_KEYUP, 0);
    }

    static void PostKeyToTree(IntPtr topHwnd, byte vk, byte scan) {
        IntPtr lDown = (IntPtr)((scan << 16) | 1);
        IntPtr lUp   = (IntPtr)((scan << 16) | 0xC0000001);
        keybd_event(vk, scan, 0, 0);
        PostMessage(topHwnd, WM_KEYDOWN, (IntPtr)vk, lDown);
        EnumChildWindows(topHwnd, (child, lp) => {
            PostMessage(child, WM_KEYDOWN, (IntPtr)vk, lDown);
            return true;
        }, IntPtr.Zero);
        Thread.Sleep(30);
        keybd_event(vk, scan, KEYEVENTF_KEYUP, 0);
        PostMessage(topHwnd, WM_KEYUP, (IntPtr)vk, lUp);
        EnumChildWindows(topHwnd, (child, lp) => {
            PostMessage(child, WM_KEYUP, (IntPtr)vk, lUp);
            return true;
        }, IntPtr.Zero);
    }

    static void FocusCanvas(IntPtr jviewerHwnd) {
        ShowWindow(jviewerHwnd, 9);
        SetForegroundWindow(jviewerHwnd);
        Thread.Sleep(100);
        RECT r;
        GetWindowRect(jviewerHwnd, out r);
        SetCursorPos(r.Left + 500, r.Top + 400);
        mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
        Thread.Sleep(30);
        mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
        Thread.Sleep(100);
    }

    [DllImport("user32.dll")]
    static extern bool SetProcessDPIAware();

    static void Main(string[] args) {
        SetProcessDPIAware();
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
        bool doTab = false;
        bool doEsc = false;
        bool doR = false;
        bool doRefresh = false;
        bool doEnter = false;
        bool doF6 = false;
        bool doF2 = false;
        bool doDown = false;
        bool doUp = false;
        if (text.Contains("--maximize")) {
            doMaximize = true;
            text = text.Replace("--maximize", "").Trim();
        }
        if (text.Contains("--scroll")) {
            doScroll = true;
            text = text.Replace("--scroll", "").Trim();
        }
        if (text.Contains("--tab")) {
            doTab = true;
            text = text.Replace("--tab", "").Trim();
        }
        if (text.Contains("--esc")) {
            doEsc = true;
            text = text.Replace("--esc", "").Trim();
        }
        if (text.Contains("--r")) {
            doR = true;
            text = text.Replace("--r", "").Trim();
        }
        if (text.Contains("--enter")) {
            doEnter = true;
            text = text.Replace("--enter", "").Trim();
        }
        if (text.Contains("--f6")) {
            doF6 = true;
            text = text.Replace("--f6", "").Trim();
        }
        if (text.Contains("--f2")) {
            doF2 = true;
            text = text.Replace("--f2", "").Trim();
        }
        if (text.Contains("--down")) {
            doDown = true;
            text = text.Replace("--down", "").Trim();
        }
        if (text.Contains("--up")) {
            doUp = true;
            text = text.Replace("--up", "").Trim();
        }
        if (text.Contains("--video-refresh")) {
            doRefresh = true;
            text = text.Replace("--video-refresh", "").Trim();
        }

        if (text.Contains("--spam-f6")) {
            ShowWindow(jviewerHwnd, 9);
            SetForegroundWindow(jviewerHwnd);
            Thread.Sleep(100);
            RECT r;
            GetWindowRect(jviewerHwnd, out r);
            SetCursorPos(r.Left + 500, r.Top + 400);
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
            Thread.Sleep(30);
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            Thread.Sleep(100);
            byte vk = 0x75; // VK_F6
            byte scan = 0x40;
            for (int i = 0; i < 60; i++) {
                PostKeyToTree(jviewerHwnd, vk, scan);
                Thread.Sleep(120);
            }
            Console.WriteLine("Spammed F6 key 60 times to JViewer.");
            return;
        }

        if (text.Contains("--spam-f2")) {
            ShowWindow(jviewerHwnd, 9);
            SetForegroundWindow(jviewerHwnd);
            Thread.Sleep(100);
            RECT r;
            GetWindowRect(jviewerHwnd, out r);
            SetCursorPos(r.Left + 500, r.Top + 400);
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
            Thread.Sleep(30);
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            Thread.Sleep(100);
            byte vk = 0x71; // VK_F2
            byte scan = 0x3C;
            for (int i = 0; i < 60; i++) {
                PostKeyToTree(jviewerHwnd, vk, scan);
                Thread.Sleep(120);
            }
            Console.WriteLine("Spammed F2 key 60 times to JViewer.");
            return;
        }

        if (text.Contains("--spam-2")) {
            ShowWindow(jviewerHwnd, 9);
            SetForegroundWindow(jviewerHwnd);
            Thread.Sleep(100);
            RECT r;
            GetWindowRect(jviewerHwnd, out r);
            SetCursorPos(r.Left + 500, r.Top + 400);
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
            Thread.Sleep(30);
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            Thread.Sleep(100);
            byte vk = 0x32; // '2'
            byte scan = 0x03;
            for (int i = 0; i < 60; i++) {
                PostKeyToTree(jviewerHwnd, vk, scan);
                Thread.Sleep(100);
            }
            Console.WriteLine("Spammed '2' key 60 times to JViewer.");
            return;
        }

        if (text.Contains("--step-down-2")) {
            ShowWindow(jviewerHwnd, 9);
            SetForegroundWindow(jviewerHwnd);
            Thread.Sleep(100);
            RECT r;
            GetWindowRect(jviewerHwnd, out r);
            SetCursorPos(r.Left + 500, r.Top + 400);
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
            Thread.Sleep(30);
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            Thread.Sleep(150);
            byte downVk = 0x28; // VK_DOWN
            byte downScan = (byte)MapVirtualKey(downVk, 0);
            PostKeyToTree(jviewerHwnd, downVk, downScan);
            Thread.Sleep(300);
            PostKeyToTree(jviewerHwnd, downVk, downScan);
            Console.WriteLine("Stepped DOWN 2 times.");
            return;
        }

        if (text.Contains("--select-ubuntu")) {
            ShowWindow(jviewerHwnd, 9);
            SetForegroundWindow(jviewerHwnd);
            Thread.Sleep(100);
            RECT r;
            GetWindowRect(jviewerHwnd, out r);
            SetCursorPos(r.Left + 500, r.Top + 400);
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
            Thread.Sleep(30);
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            Thread.Sleep(150);
            byte downVk = 0x28; // VK_DOWN
            byte downScan = (byte)MapVirtualKey(downVk, 0);
            byte enterVk = 0x0D; // VK_RETURN
            byte enterScan = 0x1C;
            for (int i = 0; i < 3; i++) {
                PostKeyToTree(jviewerHwnd, downVk, downScan);
                Thread.Sleep(250);
            }
            Thread.Sleep(300);
            PostKeyToTree(jviewerHwnd, enterVk, enterScan);
            Console.WriteLine("Selected item 4 (Ubuntu HGST drive) and pressed ENTER.");
            return;
        }

        ShowWindow(jviewerHwnd, doMaximize ? 3 : 9); // 3 = SW_MAXIMIZE
        SetForegroundWindow(jviewerHwnd);
        Thread.Sleep(200);

        if (doRefresh) {
            RECT wr;
            GetWindowRect(jviewerHwnd, out wr);
            SetCursorPos(wr.Left + 25, wr.Top + 55);
            Thread.Sleep(100);
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
            Thread.Sleep(50);
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            Thread.Sleep(300);
            // Down arrow twice, then Enter
            keybd_event(0x28, 0, 0, 0); // VK_DOWN
            Thread.Sleep(50);
            keybd_event(0x28, 0, KEYEVENTF_KEYUP, 0);
            Thread.Sleep(100);
            keybd_event(0x28, 0, 0, 0); // VK_DOWN
            Thread.Sleep(50);
            keybd_event(0x28, 0, KEYEVENTF_KEYUP, 0);
            Thread.Sleep(100);
            keybd_event(0x0D, 0, 0, 0); // VK_RETURN
            Thread.Sleep(50);
            keybd_event(0x0D, 0, KEYEVENTF_KEYUP, 0);
            Console.WriteLine("Sent Video Menu -> Refresh");
            return;
        }

        if (text.Contains("--cad")) {
            ShowWindow(jviewerHwnd, 9);
            SetForegroundWindow(jviewerHwnd);
            Thread.Sleep(200);
            RECT wr;
            GetWindowRect(jviewerHwnd, out wr);
            int cadX = wr.Right - 110;
            int cadY = wr.Top + 52;
            SetCursorPos(cadX, cadY);
            Thread.Sleep(100);
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
            Thread.Sleep(80);
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            Console.WriteLine("Clicked Ctrl Alt Del button at " + cadX + ", " + cadY);
            return;
        }

        if (text.Contains("--menu-cad")) {
            ShowWindow(jviewerHwnd, 9);
            SetForegroundWindow(jviewerHwnd);
            Thread.Sleep(150);
            RECT wr;
            GetWindowRect(jviewerHwnd, out wr);
            Console.WriteLine("JViewer Rect: Left=" + wr.Left + " Top=" + wr.Top + " Right=" + wr.Right + " Bottom=" + wr.Bottom);
            SetCursorPos(wr.Left + 80, wr.Top + 52);
            Thread.Sleep(100);
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
            Thread.Sleep(50);
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            Console.WriteLine("Opened Keyboard menu");
            Thread.Sleep(250);
            SetCursorPos(wr.Left + 115, wr.Top + 375);
            Thread.Sleep(100);
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
            Thread.Sleep(50);
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            Console.WriteLine("Clicked Ctrl+Alt+Del in menu");
            return;
        }

        if (text.Contains("--unhold-ctrl")) {
            ShowWindow(jviewerHwnd, 9);
            SetForegroundWindow(jviewerHwnd);
            Thread.Sleep(150);
            RECT wr;
            GetWindowRect(jviewerHwnd, out wr);
            SetCursorPos(wr.Left + 80, wr.Top + 52);
            Thread.Sleep(80);
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
            Thread.Sleep(50);
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            Thread.Sleep(250);
            SetCursorPos(wr.Left + 100, wr.Top + 150);
            Thread.Sleep(80);
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
            Thread.Sleep(50);
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            Thread.Sleep(250);
            SetCursorPos(wr.Left + 80, wr.Top + 52);
            Thread.Sleep(80);
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
            Thread.Sleep(50);
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            Thread.Sleep(250);
            SetCursorPos(wr.Left + 100, wr.Top + 205);
            Thread.Sleep(80);
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
            Thread.Sleep(50);
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            Thread.Sleep(250);
            SetCursorPos(wr.Left + 500, wr.Top + 400);
            Thread.Sleep(80);
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
            Thread.Sleep(50);
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            Console.WriteLine("Unhold Ctrl completed!");
            return;
        }

        if (text.Contains("--soft-kbd")) {
            ShowWindow(jviewerHwnd, 9);
            SetForegroundWindow(jviewerHwnd);
            Thread.Sleep(150);
            RECT wr;
            GetWindowRect(jviewerHwnd, out wr);
            SetCursorPos(wr.Left + 80, wr.Top + 52);
            Thread.Sleep(80);
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
            Thread.Sleep(50);
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            Thread.Sleep(250);
            SetCursorPos(wr.Left + 100, wr.Top + 120);
            Thread.Sleep(80);
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
            Thread.Sleep(50);
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            Console.WriteLine("Opened Soft Keyboard!");
            return;
        }

        if (text.Contains("--menu-keyboard")) {
            RECT wr;
            GetWindowRect(jviewerHwnd, out wr);
            SetCursorPos(wr.Left + 80, wr.Top + 52);
            Thread.Sleep(100);
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
            Thread.Sleep(50);
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            Console.WriteLine("Clicked Keyboard menu");
            return;
        }

        if (text.Contains("--menu-device")) {
            RECT wr;
            GetWindowRect(jviewerHwnd, out wr);
            SetCursorPos(wr.Left + 245, wr.Top + 52);
            Thread.Sleep(100);
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
            Thread.Sleep(50);
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            Console.WriteLine("Clicked Device menu");
            return;
        }

        if (text.Contains("--menu-options")) {
            RECT wr;
            GetWindowRect(jviewerHwnd, out wr);
            SetCursorPos(wr.Left + 185, wr.Top + 52);
            Thread.Sleep(100);
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
            Thread.Sleep(50);
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            Console.WriteLine("Clicked Options menu");
            return;
        }

        if (text.Contains("--device")) {
            RECT wr;
            GetWindowRect(jviewerHwnd, out wr);
            SetCursorPos(wr.Left + 245, wr.Top + 52);
            Thread.Sleep(100);
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
            Thread.Sleep(50);
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            Console.WriteLine("Clicked Device menu");
            return;
        }

        if (text.Contains("--click")) {
            RECT r;
            GetWindowRect(jviewerHwnd, out r);
            int clickX = r.Left + 500;
            int clickY = r.Top + 400;
            SetCursorPos(clickX, clickY);
            mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
            Thread.Sleep(30);
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            Thread.Sleep(150);
            text = text.Replace("--click", "").Trim();
        }

        if (doScroll) {
            // Scroll down in console area
            for (int i = 0; i < 5; ++i) {
                mouse_event(0x0800, 0, 0, unchecked((uint)-120), 0); // MOUSEEVENTF_WHEEL -120
                Thread.Sleep(50);
            }
        }

        if (doTab) {
            FocusCanvas(jviewerHwnd);
            byte vk = 0x09; // VK_TAB
            byte scan = 0x0F;
            SendCleanKey(vk, scan);
            Thread.Sleep(100);
            Console.WriteLine("Sent TAB key to JViewer via SendCleanKey.");
            if (string.IsNullOrEmpty(text)) return;
        }

        if (doEsc) {
            FocusCanvas(jviewerHwnd);
            byte vk = 0x1B; // VK_ESCAPE
            byte scan = 0x01;
            SendCleanKey(vk, scan);
            Thread.Sleep(100);
            Console.WriteLine("Sent ESC key to JViewer via SendCleanKey.");
            if (string.IsNullOrEmpty(text)) return;
        }

        if (doR) {
            FocusCanvas(jviewerHwnd);
            byte vk = 0x52; // 'R'
            byte scan = 0x13;
            SendCleanKey(vk, scan);
            Thread.Sleep(100);
            Console.WriteLine("Sent 'R' key to JViewer via SendCleanKey.");
            if (string.IsNullOrEmpty(text)) return;
        }

        if (doEnter) {
            FocusCanvas(jviewerHwnd);
            byte vk = 0x0D; // VK_RETURN
            byte scan = 0x1C;
            PostKeyToTree(jviewerHwnd, vk, scan);
            Console.WriteLine("Sent ENTER key to JViewer via PostKeyToTree.");
            if (string.IsNullOrEmpty(text)) return;
        }

        if (doF6) {
            FocusCanvas(jviewerHwnd);
            byte vk = 0x75; // VK_F6
            byte scan = 0x40;
            PostKeyToTree(jviewerHwnd, vk, scan);
            Thread.Sleep(100);
            Console.WriteLine("Sent F6 key to JViewer via PostKeyToTree.");
            if (string.IsNullOrEmpty(text)) return;
        }

        if (doF2) {
            FocusCanvas(jviewerHwnd);
            byte vk = 0x71; // VK_F2
            byte scan = 0x3C;
            PostKeyToTree(jviewerHwnd, vk, scan);
            Thread.Sleep(100);
            Console.WriteLine("Sent F2 key to JViewer via PostKeyToTree.");
            if (string.IsNullOrEmpty(text)) return;
        }

        if (doDown) {
            FocusCanvas(jviewerHwnd);
            byte vk = 0x28; // VK_DOWN
            byte scan = (byte)MapVirtualKey(vk, 0);
            PostKeyToTree(jviewerHwnd, vk, scan);
            Thread.Sleep(100);
            Console.WriteLine("Sent DOWN key to JViewer via PostKeyToTree.");
            if (string.IsNullOrEmpty(text)) return;
        }

        if (doUp) {
            FocusCanvas(jviewerHwnd);
            byte vk = 0x26; // VK_UP
            byte scan = (byte)MapVirtualKey(vk, 0);
            PostKeyToTree(jviewerHwnd, vk, scan);
            Thread.Sleep(100);
            Console.WriteLine("Sent UP key to JViewer via PostKeyToTree.");
            if (string.IsNullOrEmpty(text)) return;
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

        FocusCanvas(jviewerHwnd);
        Console.WriteLine("Typing: " + text.TrimEnd());

        foreach (char c in text) {
            if (c == '\r' || c == '\n') {
                byte vk = 0x0D; // VK_RETURN
                byte scan = 0x1C;
                PostKeyToTree(jviewerHwnd, vk, scan);
                Thread.Sleep(80);
                continue;
            }

            if (c == 27) { // ESC
                byte vk = 0x1B;
                byte scan = 0x01;
                SendCleanKey(vk, scan);
                Thread.Sleep(80);
                continue;
            }

            if (c == '\t') { // TAB
                byte vk = 0x09;
                byte scan = 0x0F;
                SendCleanKey(vk, scan);
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

            PostKeyToTree(jviewerHwnd, virtualKey, scanCode);
            Thread.Sleep(50);

            if ((shiftState & 1) != 0) {
                keybd_event(0x10, (byte)MapVirtualKey(0x10, 0), KEYEVENTF_KEYUP, 0);
                Thread.Sleep(20);
            }
        }

        Console.WriteLine("Done typing!");
    }
}
