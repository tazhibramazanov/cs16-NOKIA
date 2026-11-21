#!/usr/bin/env python3
"""Build the single-file PC exe: launcher stub + xz payload (jre + CS16_PC.jar) + trailer.
usage: pack_exe.py <stub.exe> <dir with jre/ and CS16_PC.jar> <out.exe>"""
import lzma, os, struct, sys, zlib

SKIP_DIRS = {'include'}
SKIP_FILES = {'jvm.lib', 'jcmd.exe', 'jfr.exe', 'jinfo.exe', 'jmap.exe', 'jps.exe', 'jrunscript.exe', 'jstack.exe',
              'jstat.exe', 'jwebserver.exe', 'keytool.exe', 'kinit.exe', 'klist.exe', 'ktab.exe', 'rmiregistry.exe'}

def entries(root):
    out = []
    for d, ds, fs in os.walk(os.path.join(root, 'jre')):
        ds[:] = sorted(x for x in ds if x not in SKIP_DIRS)
        for f in sorted(fs):
            if f in SKIP_FILES: continue
            p = os.path.join(d, f)
            out.append((os.path.relpath(p, root).replace(os.sep, '/'), p))
    out.append(('CS16_PC.jar', os.path.join(root, 'CS16_PC.jar')))
    return out

def blob(items):
    b = bytearray()
    for name, p in items:
        nb = name.encode('utf-8'); data = open(p, 'rb').read()
        b += struct.pack('<H', len(nb)) + nb + struct.pack('<I', len(data)) + data
    return bytes(b)

def main():
    stub, root, out = sys.argv[1:4]
    es = entries(root)
    pe = [e for e in es if e[0].lower().endswith(('.dll', '.exe'))]
    rest = [e for e in es if e not in pe]
    raw1 = blob(pe)
    raw2 = blob(rest) + struct.pack('<H', 0)
    lz = {"id": lzma.FILTER_LZMA2, "preset": 9 | lzma.PRESET_EXTREME, "dict_size": 64 << 20}
    c1 = lzma.compress(raw1, format=lzma.FORMAT_XZ, check=lzma.CHECK_CRC32, filters=[{"id": lzma.FILTER_X86}, lz])
    c2 = lzma.compress(raw2, format=lzma.FORMAT_XZ, check=lzma.CHECK_CRC32, filters=[lz])
    s = open(stub, 'rb').read()
    pid = zlib.crc32(c1 + c2) & 0xffffffff
    off1 = len(s); off2 = off1 + len(c1)
    # struct layout in C: magic[8], off[2], len[2], raw, id, pad
    tr = b'CS16PAYL' + struct.pack('<QQQQQII', off1, off2, len(c1), len(c2), len(raw1) + len(raw2), pid, 0)
    with open(out, 'wb') as f:
        f.write(s); f.write(c1); f.write(c2); f.write(tr)
    print(f'{len(es)} files, raw {len(raw1)+len(raw2)}, xz {len(c1)}+{len(c2)}, exe {os.path.getsize(out)}, id {pid:08x}')

main()
