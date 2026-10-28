#!/usr/bin/env python3
"""Send mapper keys to this fixture's private X display, without global input."""
import ctypes as c
import ctypes.util

def key(display, letter):
    x=c.CDLL(ctypes.util.find_library('X11'))
    U=c.c_ulong; P=c.c_void_p
    x.XOpenDisplay.argtypes=[c.c_char_p];x.XOpenDisplay.restype=P
    x.XDefaultRootWindow.argtypes=[P];x.XDefaultRootWindow.restype=U
    x.XQueryTree.argtypes=[P,U,c.POINTER(U),c.POINTER(U),c.POINTER(c.POINTER(U)),c.POINTER(c.c_uint)]
    x.XFetchName.argtypes=[P,U,c.POINTER(c.c_char_p)]
    x.XKeysymToKeycode.argtypes=[P,U];x.XKeysymToKeycode.restype=c.c_uint
    x.XStringToKeysym.argtypes=[c.c_char_p];x.XStringToKeysym.restype=U
    x.XSendEvent.argtypes=[P,U,c.c_int,c.c_long,P]
    x.XFlush.argtypes=[P];x.XCloseDisplay.argtypes=[P];x.XFree.argtypes=[P]
    d=x.XOpenDisplay(display.encode());assert d
    root=x.XDefaultRootWindow(d)
    def find(w):
        name=c.c_char_p()
        if x.XFetchName(d,w,c.byref(name)) and name.value:
            text=name.value;x.XFree(name)
            if b'DOSBox-X' in text: return w
        r=U();p=U();children=c.POINTER(U)();n=c.c_uint()
        if x.XQueryTree(d,w,c.byref(r),c.byref(p),c.byref(children),c.byref(n)):
            result=next((v for child in list(children[:n.value]) if (v:=find(child))),0)
            if children:x.XFree(children)
            return result
        return 0
    w=find(root);assert w,'emulator window absent'
    class Key(c.Structure):
        _fields_=[('type',c.c_int),('serial',U),('send_event',c.c_int),('display',P),('window',U),('root',U),('subwindow',U),('time',U),('x',c.c_int),('y',c.c_int),('x_root',c.c_int),('y_root',c.c_int),('state',c.c_uint),('keycode',c.c_uint),('same_screen',c.c_int)]
    class Event(c.Union): _fields_=[('key',Key),('pad',c.c_long*24)]
    for name,down in [('F12',True),(letter,True),(letter,False),('F12',False)]:
        e=Event();e.key=Key(2 if down else 3,0,1,d,w,root,0,0,1,1,1,1,0,x.XKeysymToKeycode(d,x.XStringToKeysym(name.encode())),1)
        assert x.XSendEvent(d,w,0,1 if down else 2,c.byref(e))
        x.XFlush(d)
    x.XCloseDisplay(d)
