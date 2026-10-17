import ctypes
from ctypes import wintypes

user32 = ctypes.windll.user32
WNDENUMPROC = ctypes.WINFUNCTYPE(ctypes.c_bool, wintypes.HWND, wintypes.LPARAM)

def enum_cb(hwnd, extra):
    if user32.IsWindowVisible(hwnd):
        length = user32.GetWindowTextLengthW(hwnd)
        if length > 0:
            buff = ctypes.create_unicode_buffer(length + 1)
            user32.GetWindowTextW(hwnd, buff, length + 1)
            title = buff.value
            if any(k in title.lower() for k in ["jviewer", "51.81", "kvm", "ipmi", "supermicro"]):
                print(f"MATCH: {hwnd}: {title}")
                user32.ShowWindow(hwnd, 9)
                user32.SetForegroundWindow(hwnd)
    return True

user32.EnumWindows(WNDENUMPROC(enum_cb), 0)
