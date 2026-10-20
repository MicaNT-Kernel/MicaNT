import ctypes

user32 = ctypes.windll.user32

class RECT(ctypes.Structure):
    _fields_ = [('left', ctypes.c_long), ('top', ctypes.c_long), ('right', ctypes.c_long), ('bottom', ctypes.c_long)]

WNDENUMPROC = ctypes.WINFUNCTYPE(ctypes.c_bool, ctypes.c_void_p, ctypes.c_void_p)
def enum_cb(hwnd, lparam):
    if user32.IsWindowVisible(hwnd):
        buf = ctypes.create_unicode_buffer(256)
        user32.GetWindowTextW(hwnd, buf, 256)
        title = buf.value
        if title:
            r = RECT()
            user32.GetWindowRect(hwnd, ctypes.byref(r))
            print(f"HWND: {hwnd:#x}, Title: '{title}', Rect: ({r.left}, {r.top}, {r.right}, {r.bottom})")
    return True

user32.EnumWindows(WNDENUMPROC(enum_cb), 0)
