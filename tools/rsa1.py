"""Pure-Python RSA for signing .sisx: key generation, PEM read/write, PKCS#1 v1.5 SHA-1 signatures and a
self-signed X.509 certificate. No openssl or other libraries needed (works the same on Windows)."""
import base64, hashlib, os, random, re

# ------------------------------------------------------------------ DER
def _len(n):
    if n < 128: return bytes([n])
    b = n.to_bytes((n.bit_length() + 7) // 8, 'big')
    return bytes([0x80 | len(b)]) + b

def tlv(tag, body): return bytes([tag]) + _len(len(body)) + body
def seq(*items): return tlv(0x30, b''.join(items))
def set_(*items): return tlv(0x31, b''.join(items))
def integer(v):
    b = v.to_bytes(max(1, (v.bit_length() + 8) // 8), 'big')
    return tlv(0x02, b)
def oid(s):
    p = [int(x) for x in s.split('.')]
    out = bytes([40 * p[0] + p[1]])
    for v in p[2:]:
        enc = [v & 0x7F]; v >>= 7
        while v: enc.append(0x80 | (v & 0x7F)); v >>= 7
        out += bytes(reversed(enc))
    return tlv(0x06, out)
NULL = b'\x05\x00'
def utf8(s): return tlv(0x0C, s.encode('utf-8'))
def printable(s): return tlv(0x13, s.encode('ascii'))
def gentime(s): return tlv(0x18, s.encode('ascii'))   # YYYYMMDDHHMMSSZ
def bitstr(b): return tlv(0x03, b'\x00' + b)

def parse(d, p=0):
    """-> (tag, body, next)"""
    tag = d[p]; n = d[p + 1]; p += 2
    if n & 0x80:
        k = n & 0x7F; n = int.from_bytes(d[p:p + k], 'big'); p += k
    return tag, d[p:p + n], p + n

def children(body):
    out, p = [], 0
    while p < len(body):
        t, b, p = parse(body, p); out.append((t, b))
    return out

# ------------------------------------------------------------------ keys
def _is_prime(n):
    if n < 2: return False
    for q in (2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37):
        if n % q == 0: return n == q
    d, s = n - 1, 0
    while d % 2 == 0: d //= 2; s += 1
    rnd = random.SystemRandom()
    for _ in range(40):
        a = rnd.randrange(2, n - 2); x = pow(a, d, n)
        if x in (1, n - 1): continue
        for _ in range(s - 1):
            x = pow(x, 2, n)
            if x == n - 1: break
        else: return False
    return True

def _prime(bits):
    while True:
        c = int.from_bytes(os.urandom(bits // 8), 'big') | (3 << (bits - 2)) | 1
        if _is_prime(c): return c

def generate(bits=1024, e=65537):
    while True:
        p, q = _prime(bits // 2), _prime(bits // 2)
        phi = (p - 1) * (q - 1)
        if p != q and phi % e: break
    d = pow(e, -1, phi)
    return dict(n=p * q, e=e, d=d, p=p, q=q)

def key_pem(k):
    body = seq(integer(0), integer(k['n']), integer(k['e']), integer(k['d']), integer(k['p']), integer(k['q']),
               integer(k['d'] % (k['p'] - 1)), integer(k['d'] % (k['q'] - 1)), integer(pow(k['q'], -1, k['p'])))
    return _pem('RSA PRIVATE KEY', body)

def _pem(label, der):
    b = base64.b64encode(der).decode()
    return '-----BEGIN %s-----\n%s\n-----END %s-----\n' % (label, '\n'.join(b[i:i + 64] for i in range(0, len(b), 64)), label)

def _unpem(text):
    m = re.search(r'-----BEGIN ([A-Z ]+)-----(.*?)-----END', text, re.S)
    return m.group(1), base64.b64decode(''.join(m.group(2).split()))

def load_key(path):
    label, der = _unpem(open(path).read())
    _, body, _ = parse(der)
    f = children(body)
    if label == 'PRIVATE KEY':   # PKCS#8 wrapper
        _, body, _ = parse(f[2][1]); f = children(body)
    v = [int.from_bytes(b, 'big') for t, b in f]
    return dict(n=v[1], e=v[2], d=v[3], p=v[4], q=v[5])

def cert_der(path):
    label, der = _unpem(open(path).read())
    return der

# ------------------------------------------------------------------ signatures
SHA1_PREFIX = bytes.fromhex('3021300906052b0e03021a05000414')

def sign(k, data):
    nlen = (k['n'].bit_length() + 7) // 8
    t = SHA1_PREFIX + hashlib.sha1(data).digest()
    em = b'\x00\x01' + b'\xff' * (nlen - len(t) - 3) + b'\x00' + t
    return pow(int.from_bytes(em, 'big'), k['d'], k['n']).to_bytes(nlen, 'big')

def cert_pubkey(der):
    """(n, e) from an X.509 certificate"""
    _, cert, _ = parse(der)
    tbs = children(children(cert)[0][1] if False else parse(cert)[1])
    i = 1 if tbs[0][0] == 0xA0 else 0
    spki = children(tbs[i + 5][1])
    bits = spki[1][1][1:]
    _, rsa, _ = parse(bits)
    n, e = [int.from_bytes(b, 'big') for t, b in children(rsa)]
    return n, e

def verify(der, data, sig):
    n, e = cert_pubkey(der)
    nlen = (n.bit_length() + 7) // 8
    em = pow(int.from_bytes(sig, 'big'), e, n).to_bytes(nlen, 'big')
    t = SHA1_PREFIX + hashlib.sha1(data).digest()
    return em == b'\x00\x01' + b'\xff' * (nlen - len(t) - 3) + b'\x00' + t

# ------------------------------------------------------------------ self-signed certificate
def make_cert(k, cn='CS 1.6 E52 Self-Signed', org='CS16 E52 Edition', country='RU'):
    alg = seq(oid('1.2.840.113549.1.1.5'), NULL)   # sha1WithRSAEncryption
    name = seq(set_(seq(oid('2.5.4.6'), printable(country))),
               set_(seq(oid('2.5.4.10'), utf8(org))),
               set_(seq(oid('2.5.4.3'), utf8(cn))))
    pub = seq(seq(oid('1.2.840.113549.1.1.1'), NULL), bitstr(seq(integer(k['n']), integer(k['e']))))
    tbs = seq(tlv(0xA0, integer(2)), integer(int.from_bytes(os.urandom(8), 'big') >> 1), alg, name,
              seq(tlv(0x17, b'090101000000Z'), gentime('20491231000000Z')), name, pub)
    der = seq(tbs, alg, bitstr(sign(k, tbs)))
    return _pem('CERTIFICATE', der)

def ensure(key_path, cert_path):
    """create a key + certificate pair if there is none yet"""
    if os.path.exists(key_path) and os.path.exists(cert_path): return False
    os.makedirs(os.path.dirname(key_path) or '.', exist_ok=True)
    k = generate(1024)
    open(key_path, 'w').write(key_pem(k))
    open(cert_path, 'w').write(make_cert(k))
    return True
