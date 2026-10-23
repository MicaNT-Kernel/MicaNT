import ctypes, time
from ctypes import wintypes
user32 = ctypes.windll.user32

user32.OpenInputDesktop.restype = wintypes.HANDLE
hDesk = user32.OpenInputDesktop(0, False, 0x01FF)
if hDesk:
    user32.SetThreadDesktop(hDesk)

class RECT(ctypes.Structure):
    _fields_ = [('left', ctypes.c_long), ('top', ctypes.c_long), ('right', ctypes.c_long), ('bottom', ctypes.c_long)]

def click_cad():
    found_h = None
    def enum_cb(h, l):
        nonlocal found_h
        buf = ctypes.create_unicode_buffer(256)
        user32.GetWindowTextW(h, buf, 256)
        if 'JViewer' in buf.value:
            found_h = h
            return False
        return True

    WNDENUMPROC = ctypes.WINFUNCTYPE(ctypes.c_bool, wintypes.HWND, wintypes.LPARAM)
    user32.EnumWindows(WNDENUMPROC(enum_cb), 0)
    if not found_h:
        print("JViewer not found!")
        return

    user32.ShowWindow(found_h, 9)
    user32.SetForegroundWindow(found_h)
    time.sleep(0.2)

    wr = RECT()
    user32.GetWindowRect(found_h, ctypes.byref(wr))
    screen_x = wr.left + 940
    screen_y = wr.top + 42

    print(f"JViewer window at ({wr.left}, {wr.top}). Clicking CAD button at absolute screen ({screen_x}, {screen_y})...")
    user32.SetCursorPos(screen_x, screen_y)
    time.sleep(0.1)
    MOUSEEVENTF_LEFTDOWN = 0x0002
    MOUSEEVENTF_LEFTUP = 0x0004
    user32.mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0)
    time.sleep(0.08)
    user32.mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0)
    print("Click dispatched!")

click_cad()
