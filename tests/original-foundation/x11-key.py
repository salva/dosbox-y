#!/usr/bin/env python3
"""Send a key chord to a named non-secret X11 emulator window (no global grab)."""
import ctypes as C
import sys
import time
x = C.CDLL('libX11.so.6')
x.XOpenDisplay.restype = C.c_void_p
x.XOpenDisplay.argtypes = [C.c_char_p]
x.XDefaultRootWindow.argtypes = [C.c_void_p]
x.XDefaultRootWindow.restype = C.c_ulong
x.XQueryTree.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(C.c_ulong), C.POINTER(C.c_ulong), C.POINTER(C.POINTER(C.c_ulong)), C.POINTER(C.c_uint)]
x.XFetchName.argtypes = [C.c_void_p, C.c_ulong, C.POINTER(C.c_char_p)]
x.XKeysymToKeycode.argtypes = [C.c_void_p, C.c_ulong]
x.XStringToKeysym.argtypes = [C.c_char_p]
x.XStringToKeysym.restype = C.c_ulong
x.XSendEvent.argtypes = [C.c_void_p, C.c_ulong, C.c_int, C.c_long, C.c_void_p]
x.XFlush.argtypes = [C.c_void_p]
x.XCloseDisplay.argtypes = [C.c_void_p]
x.XFree.argtypes = [C.c_void_p]
class Key(C.Structure):
    _fields_ = [('type', C.c_int), ('serial', C.c_ulong), ('send_event', C.c_int), ('display', C.c_void_p), ('window', C.c_ulong), ('root', C.c_ulong), ('subwindow', C.c_ulong), ('time', C.c_ulong), ('x', C.c_int), ('y', C.c_int), ('x_root', C.c_int), ('y_root', C.c_int), ('state', C.c_uint), ('keycode', C.c_uint), ('same_screen', C.c_int)]
d = x.XOpenDisplay(None)
if not d:
    raise SystemExit('no X display')
root = x.XDefaultRootWindow(d)
def windows(w):
    name = C.c_char_p()
    if x.XFetchName(d, w, C.byref(name)) and name.value:
        text = name.value.decode(errors='replace')
        x.XFree(name)
        if sys.argv[1] in text:
            yield w, text
    r, p, children, n = C.c_ulong(), C.c_ulong(), C.POINTER(C.c_ulong)(), C.c_uint()
    if x.XQueryTree(d, w, C.byref(r), C.byref(p), C.byref(children), C.byref(n)):
        for i in range(n.value):
            yield from windows(children[i])
        if children:
            x.XFree(children)
found = list(windows(root))
if len(found) != 1:
    raise SystemExit(f'expected one matching window: {found}')
w, title = found[0]
print(w, title)
if sys.argv[2:] == ['WM_DELETE_WINDOW']:
    class Client(C.Structure):
        _fields_ = [('type', C.c_int), ('serial', C.c_ulong), ('send_event', C.c_int), ('display', C.c_void_p), ('window', C.c_ulong), ('message_type', C.c_ulong), ('format', C.c_int), ('data', C.c_long * 5)]
    x.XInternAtom.argtypes = [C.c_void_p, C.c_char_p, C.c_int]
    x.XInternAtom.restype = C.c_ulong
    ev = Client(type=33, display=d, window=w, message_type=x.XInternAtom(d, b'WM_PROTOCOLS', 0), format=32)
    ev.data[0] = x.XInternAtom(d, b'WM_DELETE_WINDOW', 0)
    buf = C.create_string_buffer(24 * C.sizeof(C.c_long))
    C.memmove(buf, C.byref(ev), C.sizeof(ev))
    if not x.XSendEvent(d, w, 0, 0, buf):
        raise SystemExit('WM_DELETE failed')
    x.XFlush(d)
    x.XCloseDisplay(d)
    raise SystemExit(0)
keys = [x.XKeysymToKeycode(d, x.XStringToKeysym(k.encode())) for k in sys.argv[2:]]
if not all(keys):
    raise SystemExit('unknown key')
for typ, seq in [(2, keys), (3, list(reversed(keys)))]:
    for code in seq:
        ev = Key(type=typ, display=d, window=w, root=root, keycode=code, same_screen=1)
        # XEvent is a 24-long union, larger than XKeyEvent.
        buf = C.create_string_buffer(24 * C.sizeof(C.c_long))
        C.memmove(buf, C.byref(ev), C.sizeof(ev))
        if not x.XSendEvent(d, w, 1, 1 << (typ - 2), buf):
            raise SystemExit('XSendEvent failed')
        x.XFlush(d)
        time.sleep(.12)
x.XCloseDisplay(d)
