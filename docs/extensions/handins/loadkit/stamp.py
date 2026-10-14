import sys, time, os
path, out, t0ns, pid = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
def alive():
    import ctypes
    h = ctypes.windll.kernel32.OpenProcess(0x1000, 0, pid)
    if not h: return False
    code = ctypes.c_ulong(); ctypes.windll.kernel32.GetExitCodeProcess(h, ctypes.byref(code)); ctypes.windll.kernel32.CloseHandle(h)
    return code.value == 259
while not os.path.exists(path):
    time.sleep(0.002)
buf = b''; pos = 0; dead_since = None
with open(out, 'w', encoding='utf-8', errors='replace') as o, open(path, 'rb') as f:
    while True:
        chunk = f.read()
        now = (time.time_ns() - t0ns) // 1000000
        if chunk:
            buf += chunk
            *lines, buf = buf.split(b'\n')
            for l in lines: o.write(f"{now} {l.decode('utf-8','replace').rstrip()}\n")
        else:
            if not alive():
                if dead_since is None: dead_since = time.time()
                elif time.time() - dead_since > 0.5: break
            time.sleep(0.003)
