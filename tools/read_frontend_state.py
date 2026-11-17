"""Bounded, read-only evidence from the supported MW PC frontend and car viewer."""
import argparse
import ctypes as c
from ctypes import wintypes as w
import json
import struct
from pathlib import Path

p = argparse.ArgumentParser()
p.add_argument('pid', type=int)
p.add_argument('output', type=Path)
args = p.parse_args()
k = c.WinDLL('kernel32', use_last_error=True)
k.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]
k.OpenProcess.restype = w.HANDLE
k.ReadProcessMemory.argtypes = [w.HANDLE, c.c_void_p, c.c_void_p, c.c_size_t, c.POINTER(c.c_size_t)]
k.CloseHandle.argtypes = [w.HANDLE]
h = k.OpenProcess(0x1010, False, args.pid)
if not h:
    raise c.WinError(c.get_last_error())

def read(address, size):
    if not 0x10000 <= address < 0x80000000 or not 0 < size <= 65536:
        raise ValueError('Outside bounded game range')
    buf, count = c.create_string_buffer(size), c.c_size_t()
    if not k.ReadProcessMemory(h, address, buf, size, c.byref(count)) or count.value != size:
        raise OSError(f'Unreadable game range {address:08X}')
    return buf.raw

def word(address):
    return struct.unpack('<I', read(address, 4))[0]

def text(address):
    return read(address, 128).split(b'\0')[0].decode('ascii', errors='replace') if address else ''

def chain(address, maximum):
    seen = set()
    for _ in range(maximum):
        if not address or address in seen:
            return
        seen.add(address)
        yield address
        address = word(address + 4)

def objects(head, depth=0):
    if depth > 12:
        return []
    result = []
    for address in chain(head, 512):
        data = struct.unpack('<23I', read(address, 0x5C))
        item = dict(address=hex(address), vtable=hex(data[0]), name=text(data[5]),
                    hash=hex(data[4]), type=data[6], flags=hex(data[7]))
        if data[11] and 0x44 <= data[12] <= 65536:
            geometry = struct.unpack('<17f', read(data[11], 0x44))
            item['position'] = geometry[7:10]
            item['size'] = geometry[14:17]
        if data[6] == 2:
            raw = read(address + 0x60, 12)
            label, string, capacity = struct.unpack('<3I', raw)
            item.update(label=hex(label), capacity=capacity)
            if string and 0 < capacity <= 2048:
                item['text'] = read(string, min(capacity * 2, 4096)).decode('utf-16-le', errors='replace').split('\0')[0]
        if data[6] == 5:
            item['children'] = objects(word(address + 0x64), depth + 1)
        result.append(item)
    return result

try:
    engine = word(word(0x91CADC))
    result = dict(pid=args.pid, gameFlow=word(0x925E90), engine=hex(engine), packages=[],
                  hooks={hex(a): read(a, 16).hex(' ') for a in (0x7A48F0, 0x7B9F23, 0x7A2310)},
                  viewerRide=read(0x9BA088, 0x310).hex(),
                  garageLoader=dict(address='0x9ba3a0', loadingRide=read(0x9BA3A0, 0x310).hex(),
                                    currentRide=read(0x9BA6B0, 0x310).hex(),
                                    flags=read(0x9BA9C0, 16).hex()))
    seen = set()
    for offset in (0xE0, 0xF0):
        for package in chain(word(engine + offset), 64):
            if package in seen:
                continue
            seen.add(package)
            result['packages'].append(dict(address=hex(package), name=text(word(package + 12)),
                                           objects=objects(word(package + 0x4C))))
    count = min(word(0x92CD24), 64)
    result['vehicles'] = [dict(address=hex(word(0x92CD28 + i * 4)),
                              words=read(word(0x92CD28 + i * 4), 0x100).hex()) for i in range(count)]
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps(dict(output=str(args.output), gameFlow=result['gameFlow'],
                          packages=[x['name'] for x in result['packages']], vehicles=count)))
finally:
    k.CloseHandle(h)
