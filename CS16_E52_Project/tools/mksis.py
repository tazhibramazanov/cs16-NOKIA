#!/usr/bin/env python3
"""Minimal Symbian OS 9 (S60 3rd edition) SIS/SISX builder + signer (Python 3).
Field layout follows the Symbian SIS file format (as implemented by makesis / ensymble)."""
import struct, zlib, hashlib, subprocess, time, sys, os

T = dict(SISString=1, SISArray=2, SISCompressed=3, SISVersion=4, SISVersionRange=5, SISDate=6, SISTime=7, SISDateTime=8,
         SISUid=9, SISLanguage=11, SISContents=12, SISController=13, SISInfo=14, SISSupportedLanguages=15,
         SISSupportedOptions=16, SISPrerequisites=17, SISDependency=18, SISProperties=19, SISProperty=20,
         SISCertificateChain=22, SISLogo=23, SISFileDescription=24, SISHash=25, SISIf=26, SISElseIf=27,
         SISInstallBlock=28, SISExpression=29, SISData=30, SISDataUnit=31, SISFileData=32, SISSupportedOption=33,
         SISControllerChecksum=34, SISDataChecksum=35, SISSignature=36, SISBlob=37, SISSignatureAlgorithm=38,
         SISSignatureCertificateChain=39, SISDataIndex=40, SISCapabilities=41)
EOpInstall, EOpRun, EOpText, EOpNull = 1, 2, 4, 8
RunInstall, RunUninstall, RunByMime, RunWaitEnd = 1 << 1, 1 << 2, 1 << 3, 1 << 4


def pad(n): return b'\0' * (((n + 3) & ~3) - n)


def hdr(t, n):
    if n < 0x80000000: return struct.pack('<LL', t, n)
    return struct.pack('<LLL', t, (n & 0x7fffffff) | 0x80000000, n >> 31)


def field(name, payload):
    return hdr(T[name], len(payload)) + payload + pad(len(payload))


def strip(f):
    t, n = struct.unpack_from('<LL', f, 0); h = 8
    if n & 0x80000000:
        n2, = struct.unpack_from('<L', f, 8); n = (n & 0x7fffffff) | (n2 << 31); h = 12
    return f[h:h + n]


def array(elemtype, items):
    body = struct.pack('<L', T[elemtype]) + b''.join(it[4:] for it in items)
    return field('SISArray', body)


def sstring(s): return field('SISString', s.encode('utf-16-le'))
def blob(b): return field('SISBlob', b)
def uid(u): return field('SISUid', struct.pack('<L', u))
def version(a, b, c): return field('SISVersion', struct.pack('<lll', a, b, c))


def compressed(data, algo):
    if algo == 2:  # auto
        c = zlib.compress(data, 9)
        if len(c) < len(data): algo, body = 1, c
        else: algo, body = 0, data
    elif algo == 1: body = zlib.compress(data, 9)
    else: body = data
    return hdr(T['SISCompressed'], 12 + len(body)) + struct.pack('<LQ', algo, len(data)) + body + pad(len(body)), len(body)


def crc16(b):
    v = 0
    for c in b:
        v ^= c << 8
        for _ in range(8):
            v <<= 1
            if v & 0x10000: v ^= 0x1021
            v &= 0xffff
    return v


def uidcrc(u1, u2, u3):
    s = struct.pack('<LLL', u1, u2, u3)
    return (crc16(s[1::2]) << 16) | crc16(s[0::2])


def build(out, appuid, name, vendor, ver, files, key_pem, cert_pem, langs=(1,), targets=((0x101F7961, 'Series60ProductID'),)):
    """files: list of (data, target, mimetype, operation, options)"""
    t = time.gmtime()
    info = field('SISInfo', uid(appuid) + sstring(vendor) + array('SISString', [sstring(name)] * len(langs)) +
                 array('SISString', [sstring(vendor)] * len(langs)) + version(*ver) +
                 field('SISDateTime', field('SISDate', struct.pack('<HBB', t.tm_year, t.tm_mon - 1, t.tm_mday)) +
                       field('SISTime', struct.pack('<BBB', t.tm_hour, t.tm_min, t.tm_sec))) + struct.pack('<BB', 0, 0))
    opts = field('SISSupportedOptions', array('SISSupportedOption', []))
    lng = field('SISSupportedLanguages', array('SISLanguage', [field('SISLanguage', struct.pack('<L', l)) for l in langs]))
    deps = [field('SISDependency', uid(u) + field('SISVersionRange', version(0, 0, 0)) + array('SISString', [sstring(n)] * len(langs))) for u, n in targets]
    prereq = field('SISPrerequisites', array('SISDependency', deps) + array('SISDependency', []))
    props = field('SISProperties', array('SISProperty', []))
    fdescs, fdatas = [], []
    for i, (data, target, mime, op, oo) in enumerate(files):
        fd, clen = compressed(data, 2)
        fdatas.append(field('SISFileData', fd))
        h = field('SISHash', struct.pack('<L', 1) + blob(hashlib.sha1(data).digest()))
        fdescs.append(field('SISFileDescription', sstring(target) + sstring(mime) + h +
                            struct.pack('<LLQQL', op, oo, clen, len(data), i)))
    ib = field('SISInstallBlock', array('SISFileDescription', fdescs) + array('SISController', []) + array('SISIf', []))
    ctrl_body = info + opts + lng + prereq + props + ib
    # sign the controller contents (without header/padding), before signatures and data index
    sig = subprocess.run(['openssl', 'dgst', '-sha1', '-sign', key_pem], input=ctrl_body, capture_output=True, check=True).stdout
    der = subprocess.run(['openssl', 'x509', '-in', cert_pem, '-outform', 'DER'], capture_output=True, check=True).stdout
    sigchain = field('SISSignatureCertificateChain',
                     array('SISSignature', [field('SISSignature', field('SISSignatureAlgorithm', sstring('1.2.840.113549.1.1.5')) + blob(sig))]) +
                     field('SISCertificateChain', blob(der)))
    ctrl = field('SISController', ctrl_body + sigchain + field('SISDataIndex', struct.pack('<L', 0)))
    ctrlc, _ = compressed(ctrl, 1)
    data = field('SISData', array('SISDataUnit', [field('SISDataUnit', array('SISFileData', fdatas))]))
    contents = field('SISContents', field('SISControllerChecksum', struct.pack('<H', crc16(ctrlc))) +
                     field('SISDataChecksum', struct.pack('<H', crc16(data))) + ctrlc + data)
    u1, u2 = 0x10201A7A, 0
    blobout = struct.pack('<LLLL', u1, u2, appuid, uidcrc(u1, u2, appuid)) + contents
    open(out, 'wb').write(blobout)
    return len(blobout)


if __name__ == '__main__':
    pass

# ---------------------------------------------------------------- verification (independent parser)
NAMES = {v: k for k, v in T.items()}


def readf(b, p):
    t, n = struct.unpack_from('<LL', b, p); h = 8
    if n & 0x80000000:
        n2, = struct.unpack_from('<L', b, p + 8); n = (n & 0x7fffffff) | (n2 << 31); h = 12
    return t, b[p + h:p + h + n], p + h + n + (((n + 3) & ~3) - n)


def readarray(payload):
    et, = struct.unpack_from('<L', payload, 0); p = 4; out = []
    while p < len(payload):
        n, = struct.unpack_from('<L', payload, p); h = 4
        out.append(payload[p + h:p + h + n]); p += h + n + (((n + 3) & ~3) - n)
    return et, out


def fields(payload):
    p = 0; out = []
    while p < len(payload):
        t, body, p = readf(payload, p); out.append((NAMES.get(t, t), body))
    return out


def verify(path, cert_pem):
    b = open(path, 'rb').read()
    u1, u2, u3, crc = struct.unpack_from('<LLLL', b, 0)
    assert u1 == 0x10201A7A and crc == uidcrc(u1, u2, u3), 'bad UID header'
    t, contents, end = readf(b, 16)
    assert NAMES[t] == 'SISContents' and end == len(b), 'bad SISContents'
    f = fields(contents)
    names = [x[0] for x in f]
    assert names == ['SISControllerChecksum', 'SISDataChecksum', 'SISCompressed', 'SISData'], names
    # checksums over whole fields
    p = 0; raw = []
    while p < len(contents):
        q = p; t2, body, p = readf(contents, p); raw.append(contents[q:p])
    assert struct.unpack('<H', f[0][1])[0] == crc16(raw[2]), 'controller checksum'
    assert struct.unpack('<H', f[1][1])[0] == crc16(raw[3]), 'data checksum'
    algo, ulen = struct.unpack_from('<LQ', f[2][1], 0)
    ctrl = zlib.decompress(f[2][1][12:]) if algo == 1 else f[2][1][12:]
    assert len(ctrl) == ulen
    t, cbody, _ = readf(ctrl, 0)
    assert NAMES[t] == 'SISController'
    cf = fields(cbody)
    cn = [x[0] for x in cf]
    assert cn == ['SISInfo', 'SISSupportedOptions', 'SISSupportedLanguages', 'SISPrerequisites', 'SISProperties', 'SISInstallBlock',
                  'SISSignatureCertificateChain', 'SISDataIndex'], cn
    # signature check: signed data = controller body up to the signature chain
    signed_len = 0; p = 0
    for i in range(6):
        _, _, p = readf(cbody, p)
    signed = cbody[:p]
    sc = fields(cf[6][1])
    et, sigs = readarray(sc[0][1])
    sg = fields(sigs[0])
    alg = fields(sg[0][1])[0][1].decode('utf-16-le')
    sigdata = sg[1][1]
    der = fields(sc[1][1])[0][1]
    open('/tmp/_c.der', 'wb').write(der); open('/tmp/_s.bin', 'wb').write(sigdata); open('/tmp/_d.bin', 'wb').write(signed)
    subprocess.run(['openssl', 'x509', '-inform', 'DER', '-in', '/tmp/_c.der', '-pubkey', '-noout', '-out', '/tmp/_p.pem'], check=True)
    v = subprocess.run(['openssl', 'dgst', '-sha1', '-verify', '/tmp/_p.pem', '-signature', '/tmp/_s.bin', '/tmp/_d.bin'], capture_output=True, text=True)
    assert 'Verified OK' in v.stdout, v.stdout + v.stderr
    # files
    ib = fields(cf[5][1])
    _, fdescs = readarray(ib[0][1])
    du_t, units = readarray(fields(f[3][1])[0][1])
    _, fdatas = readarray(fields(units[0])[0][1])
    ib_ = cf[0][1]; p = 0; parts = []
    for i in range(6):
        t_, body_, p = readf(ib_, p); parts.append(body_)
    _, nm = readarray(parts[2])
    res = {'uid': hex(u3), 'name': nm[0].decode('utf-16-le'), 'vendor': parts[1].decode('utf-16-le'), 'sigalg': alg, 'files': []}
    for fd in fdescs:
        ff = []; p = 0
        for i in range(3):
            t_, body_, p = readf(fd, p); ff.append((NAMES[t_], body_))
        target = ff[0][1].decode('utf-16-le'); mime = ff[1][1].decode('utf-16-le')
        h = ff[2][1]; halg, = struct.unpack_from('<L', h, 0); digest = fields(h[4:])[0][1]
        # integral tail after the 3 fields
        p = 0
        for i in range(3): _, _, p = readf(fd, p)
        op, oo, clen, ulen, idx = struct.unpack_from('<LLQQL', fd, p)
        fdat = fdatas[idx]
        _, comp, _ = readf(fdat, 0)
        a2, ul2 = struct.unpack_from('<LQ', comp, 0)
        data = zlib.decompress(comp[12:12 + clen]) if a2 == 1 else comp[12:12 + clen]
        assert len(data) == ulen == ul2, ('length mismatch', target, len(data), ulen, ul2, a2, clen, len(comp))
        assert hashlib.sha1(data).digest() == digest, 'hash mismatch ' + target
        res['files'].append((target, mime, op, oo, ulen, clen))
    return res
