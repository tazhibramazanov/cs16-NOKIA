#!/usr/bin/env python3
import os as _os
GAME = _os.environ.get('CS16_GAME', '/mnt/user-data/uploads/ks16')   # folder with cstrike/ and valve/
HERE = _os.path.dirname(_os.path.dirname(_os.path.abspath(__file__))) + '/'

# CS 1.6 BSP v30 -> CSE map format for the E52 engine (big-endian, read by DataInputStream)
import struct, re, sys, os, math
import numpy as np
from PIL import Image

ROOT = GAME + ''
WADS = {
    'halflife.wad': ROOT + '/valve/halflife.wad', 'decals.wad': ROOT + '/valve/decals.wad',
    'liquids.wad': ROOT + '/valve/liquids.wad', 'cs_dust.wad': ROOT + '/cstrike/cs_dust.wad',
    'cstrike.wad': ROOT + '/cstrike/cstrike.wad', 'cs_bdog.wad': ROOT + '/cstrike/cs_bdog.wad',
}
SKYDIR = ROOT + '/cstrike/gfx/env/'
NODRAW = {'aaatrigger', 'clip', 'origin', 'null', 'bevel', 'hint', 'skip', 'noclip', 'trigger'}

def lumps(d):
    return [struct.unpack_from('<ii', d, 4 + i * 8) for i in range(15)]

class WadCache:
    def __init__(self):
        self.idx = {}
    def load(self, path):
        d = open(path, 'rb').read()
        assert d[:4] == b'WAD3'
        n, ofs = struct.unpack_from('<ii', d, 4)
        for i in range(n):
            fp, ds, sz, typ, comp = struct.unpack_from('<iiibb', d, ofs + i * 32)
            name = d[ofs + i * 32 + 16:ofs + i * 32 + 32].split(b'\0')[0].decode('latin1').lower()
            if typ == 0x43 and name not in self.idx:
                self.idx[name] = (d, fp)
    def get(self, name):
        return self.idx.get(name.lower())

def miptex_rgb(d, o):
    nm, w, h = struct.unpack_from('<16sII', d, o)
    offs = struct.unpack_from('<4I', d, o + 24)
    pix = np.frombuffer(d, np.uint8, w * h, o + offs[0]).reshape(h, w)
    po = o + offs[3] + (w // 8) * (h // 8) + 2
    pal = np.frombuffer(d, np.uint8, 768, po).reshape(256, 3)
    return w, h, pix, pal

def pow2(n, cap=64):
    p = 1
    while p * 2 <= n: p *= 2
    if n - p > p * 2 - n: p *= 2  # nearest
    return max(4, min(cap, p))

def parse_ents(s):
    out = []
    for e in re.findall(r'\{([^}]*)\}', s):
        out.append(dict(re.findall(r'"([^"]*)" "([^"]*)"', e)))
    return out

def light_level(lum):
    # GoldSrc-like brightening (lightgamma) -> level 0..63, 32 = texture as is
    x = lum / 255.0
    v = (x ** 0.55) * 1.32
    return int(max(0, min(63, round(v * 32))))

LLUT = [light_level(i) for i in range(256)]

MATCODES = {'C': 0, 'M': 1, 'V': 2, 'D': 3, 'S': 4, 'T': 5, 'G': 6, 'W': 7, 'P': 8, 'Y': 9, 'N': 10, 'X': 11}
MATS = {}
for line in open(ROOT + '/cstrike/sound/materials.txt', encoding='latin1'):
    t = line.split()
    if len(t) >= 2 and not t[0].startswith('//') and t[0] in MATCODES:
        MATS[t[1][:12].lower()] = MATCODES[t[0]]

def material(name):
    n = name
    if n[:1] in '-+': n = n[2:]
    if n[:1] in '{!~ ': n = n[1:]
    return MATS.get(n[:12].lower(), 0)

def convert(mapname, outpath, log=print):
    d = open(f'{ROOT}/cstrike/maps/{mapname}.bsp', 'rb').read()
    L = lumps(d)
    def lump(i): return L[i][0], L[i][1]
    ents = parse_ents(d[L[0][0]:L[0][0] + L[0][1]].decode('latin1'))
    world = ents[0]
    wc = WadCache()
    for w in world.get('wad', '').split(';'):
        b = w.replace('\\', '/').split('/')[-1].lower()
        if b in WADS: wc.load(WADS[b])
    # planes
    o, l = lump(1); n = l // 20
    planes = [struct.unpack_from('<4fi', d, o + i * 20) for i in range(n)]
    o, l = lump(3); verts = np.frombuffer(d, '<f4', l // 4, o).reshape(-1, 3).astype(np.float64)
    o, l = lump(12); edges = np.frombuffer(d, '<u2', l // 2, o).reshape(-1, 2)
    o, l = lump(13); surfedges = np.frombuffer(d, '<i4', l // 4, o)
    o, l = lump(6); texinfo = [(np.array(struct.unpack_from('<8f', d, o + i * 40), np.float64).reshape(2, 4),) + struct.unpack_from('<ii', d, o + i * 40 + 32) for i in range(l // 40)]
    o, l = lump(7); faces = [struct.unpack_from('<HhihhBBBBi', d, o + i * 20) for i in range(l // 20)]
    o, l = lump(5); nodes = [struct.unpack_from('<ihh6hHH', d, o + i * 24) for i in range(l // 24)]
    o, l = lump(10); leaves = [struct.unpack_from('<ii6hHH4B', d, o + i * 28) for i in range(l // 28)]
    o, l = lump(11); marks = np.frombuffer(d, '<u2', l // 2, o)
    o, l = lump(9); clip = [struct.unpack_from('<ihh', d, o + i * 8) for i in range(l // 8)]
    o, l = lump(14); models = [struct.unpack_from('<9f4i3i', d, o + i * 64) for i in range(l // 64)]
    vo, vl = lump(4); visdata = d[vo:vo + vl]
    lo, ll = lump(8); light = np.frombuffer(d, np.uint8, ll, lo)
    # textures
    to, tl = lump(2); ntex = struct.unpack_from('<i', d, to)[0]
    toffs = struct.unpack_from('<%di' % ntex, d, to + 4)
    texsrc = []
    for i in range(ntex):
        mo = to + toffs[i]
        nm, w, h, o0 = struct.unpack_from('<16sIII', d, mo)
        nm = nm.split(b'\0')[0].decode('latin1')
        if o0 > 0: texsrc.append((nm,) + miptex_rgb(d, mo))
        else:
            r = wc.get(nm)
            if r is None:
                log('MISSING texture', nm); texsrc.append((nm, w, h, None, None))
            else: texsrc.append((nm,) + miptex_rgb(r[0], r[1]))
    # which textures are drawn
    used = set()
    brush_models_drawn = set([0])
    ent_model = {}
    for e in ents:
        m = e.get('model', '')
        if m.startswith('*'):
            ent_model[int(m[1:])] = e
            if e.get('classname') not in ('func_ladder', 'func_buyzone', 'func_bomb_target', 'trigger_multiple', 'trigger_camera', 'func_hostage_rescue'):
                brush_models_drawn.add(int(m[1:]))
    drawn_faces = set()
    for mi in brush_models_drawn:
        m = models[mi]; ff, nf = m[14], m[15]
        for f in range(ff, ff + nf): drawn_faces.add(f)
    for f in drawn_faces:
        used.add(texinfo[faces[f][4]][1])
    # build texture list (remap)
    texmap = {}; texlist = []
    for ti in sorted(used):
        nm = texsrc[ti][0]; low = nm.lower()
        if low in NODRAW: continue
        texmap[ti] = len(texlist); texlist.append(ti)
    # global palette from all used textures
    imgs = {}
    for ti in texlist:
        nm, w, h, pix, pal = texsrc[ti]
        if pix is None:
            rgb = np.full((16, 16, 3), 128, np.uint8); w = h = 16; mask = np.zeros((16, 16), bool)
        else:
            rgb = pal[pix]; mask = (pix == 255) if nm.startswith('{') else np.zeros(pix.shape, bool)
        imgs[ti] = (rgb, mask, w, h)
    sky = None
    sample = []
    for ti in texlist:
        rgb, mask, w, h = imgs[ti]
        if texsrc[ti][0].lower() == 'sky': continue
        px = rgb[~mask].reshape(-1, 3)
        if len(px) > 6000: px = px[np.random.RandomState(1).choice(len(px), 6000, replace=False)]
        sample.append(px)
    sample = np.concatenate(sample)
    side = int(math.ceil(math.sqrt(len(sample))))
    pad = np.zeros((side * side, 3), np.uint8); pad[:len(sample)] = sample; pad[len(sample):] = sample[0]
    qimg = Image.fromarray(pad.reshape(side, side, 3)).quantize(colors=255, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    gpal = np.array(qimg.getpalette()[:255 * 3], np.uint8).reshape(255, 3)
    palimg = Image.new('P', (1, 1)); palimg.putpalette(list(gpal.flatten()) + [0, 0, 0] * 1)
    def quant(rgbarr, mask):
        im = Image.fromarray(rgbarr).quantize(palette=palimg, dither=Image.Dither.NONE)
        a = np.array(im, np.uint8)
        a[a == 255] = 254
        a[mask] = 255
        return a
    texout = []  # (name, flags, w, h, [mip bytes])
    texscale = {}
    for ti in texlist:
        nm = texsrc[ti][0]; low = nm.lower()
        rgb, mask, w, h = imgs[ti]
        flags = 0
        if low == 'sky': flags |= 1
        if nm.startswith('{'): flags |= 2
        if nm.startswith('!') or low.startswith('water'): flags |= 4
        nw, nh = pow2(w), pow2(h)
        texscale[ti] = (nw / w, nh / h)
        mips = []
        if flags & 1:
            texout.append((nm, flags, 8, 8, [bytes(64)])); continue
        im = Image.fromarray(rgb)
        mk = Image.fromarray((mask * 255).astype(np.uint8))
        r = np.array(im.resize((nw, nh), Image.LANCZOS if (nw < w or nh < h) else Image.BICUBIC))
        mm = np.array(mk.resize((nw, nh), Image.BOX)) > 127 if flags & 2 else np.zeros((nh, nw), bool)
        flags |= material(nm) << 4
        texout.append((nm, flags, nw, nh, [quant(r, mm).tobytes()]))
    log(mapname, 'textures', len(texout), 'texels', sum(len(t[4][0]) for t in texout))
    # faces
    fverts = []; facerecs = []
    lightbytes = bytearray()
    lumsum = np.zeros(3); lumcnt = 0
    nf = len(faces)
    lo_sorted = sorted(set(f[9] for f in faces if f[9] >= 0))
    gap = {}
    for a, b in zip(lo_sorted, lo_sorted[1:] + [len(light)]):
        gap[a] = (b - a) // 3
    for fi, f in enumerate(faces):
        pn, side, fe, ne, tinf, s0, s1, s2, s3, lofs = f
        vi = []
        for k in range(ne):
            se = surfedges[fe + k]
            vi.append(int(edges[se][0]) if se >= 0 else int(edges[-se][1]))
        tvec, mip, tflags = texinfo[tinf]
        draw = fi in drawn_faces and mip in texmap
        tex = texmap.get(mip, -1) if draw else -1
        # lightmap extents
        v32 = verts[vi].astype(np.float32); t32 = tvec.astype(np.float32)
        st = (v32 @ t32[:, :3].T + t32[:, 3]).astype(np.float64)
        mins = st.min(0); maxs = st.max(0)
        bmins = np.floor(mins / 16); bmaxs = np.ceil(maxs / 16)
        lw = int(bmaxs[0] - bmins[0]) + 1; lh = int(bmaxs[1] - bmins[1]) + 1
        gs = gap.get(lofs)
        if lofs >= 0 and gs is not None and gs != lw * lh:
            fixed = False
            for dw in (0, -1, 1):
                for dh in (0, -1, 1):
                    if not fixed and (lw + dw) * (lh + dh) == gs:
                        for ax, dd in ((0, dw), (1, dh)):
                            if dd == 0: continue
                            fmin = abs(mins[ax] / 16 - round(mins[ax] / 16)) < 0.02
                            if dd < 0:
                                if fmin: bmins[ax] += 1
                                else: bmaxs[ax] -= 1
                            else:
                                if fmin: bmins[ax] -= 1
                                else: bmaxs[ax] += 1
                        lw += dw; lh += dh; fixed = True
            if not fixed: log('lightmap size unresolved face', fi, lw, lh, gs)
        lmoff = -1
        if draw and lofs >= 0 and not (tflags & 1) and lw <= 64 and lh <= 64:
            raw = light[lofs:lofs + lw * lh * 3].reshape(-1, 3).astype(np.float64)
            if len(raw) == lw * lh:
                lum = raw[:, 0] * 0.299 + raw[:, 1] * 0.587 + raw[:, 2] * 0.114
                lumsum += raw.sum(0); lumcnt += len(raw)
                lmoff = len(lightbytes)
                lightbytes += bytes(LLUT[int(x)] for x in np.clip(lum, 0, 255))
        sx, sy = texscale.get(mip, (1, 1))
        svec = tvec[0] * sx; tvec2 = tvec[1] * sy
        lsv = tvec[0] / 16.0; lsv = lsv.copy(); lsv[3] = (tvec[0][3] - bmins[0] * 16) / 16.0
        ltv = tvec[1] / 16.0; ltv = ltv.copy(); ltv[3] = (tvec[1][3] - bmins[1] * 16) / 16.0
        facerecs.append((pn, 1 if side else 0, len(fverts), len(vi), tex, tinf, lmoff, lw, lh, int(bmins[0]), int(bmins[1])))
        fverts.extend(vi)
    tint = lumsum / max(1, lumcnt); tint = tint / tint.max()
    log('tint', tint, 'lightbytes', len(lightbytes))
    # skybox
    skyname = world.get('skyname', 'des').lower()
    skyfiles = {f.lower(): f for f in os.listdir(SKYDIR)}
    skyfaces = []
    for suf in ['rt', 'lf', 'bk', 'ft', 'up']:
        fn = skyfiles.get(skyname + suf + '.tga')
        if fn: skyfaces.append(Image.open(SKYDIR + fn).convert('RGB').resize((128, 128), Image.LANCZOS))
        else: skyfaces.append(Image.new('RGB', (128, 128), (120, 150, 200)))
    # write
    out = bytearray()
    def w8(x): out.extend(struct.pack('>B', x))
    def w16(x): out.extend(struct.pack('>h', x))
    def w32(x): out.extend(struct.pack('>i', x))
    def wf(x): out.extend(struct.pack('>f', x))
    def wstr(s):
        b = s.encode('utf-8'); out.extend(struct.pack('>H', len(b))); out.extend(b)
    out += b'CSE3'
    for c in gpal: out += bytes(c)
    out += bytes([0, 0, 0])
    for c in tint: wf(float(c))
    # inverse palette LUT (5:5:5 -> index)
    g = np.stack(np.meshgrid(np.arange(32), np.arange(32), np.arange(32), indexing='ij'), -1).reshape(-1, 3) * 8 + 4
    lut = np.zeros(32768, np.uint8)
    pf = gpal.astype(np.int32)
    for i0 in range(0, 32768, 4096):
        dd = ((g[i0:i0 + 4096, None, :] - pf[None, :, :]) ** 2).sum(-1)
        lut[i0:i0 + 4096] = dd.argmin(1)
    out.extend(lut.tobytes())
    w32(len(texout))
    for ti_, (nm, flags, tw, th, mips) in zip(texlist, texout):
        wstr(nm); w16(flags); w16(tw); w16(th)
        sx, sy = texscale.get(ti_, (1, 1)); wf(float(sx)); wf(float(sy))
        w32(len(mips[0])); out.extend(mips[0])
    w32(len(texinfo))
    for tv, mip, tfl in texinfo:
        for k in range(8): wf(float(tv.flatten()[k]))
        w16(texmap.get(mip, -1))
    w32(len(planes))
    for p in planes: wf(p[0]); wf(p[1]); wf(p[2]); wf(p[3]); w8(p[4])
    w32(len(verts))
    for v in verts: wf(v[0]); wf(v[1]); wf(v[2])
    w32(len(fverts))
    for v in fverts: w16(v - 65536 if v >= 32768 else v)
    w32(len(facerecs))
    for (pn, side, fv, nv, tex, tinf, lmoff, lw, lh, bms, bmt) in facerecs:
        w16(pn - 65536 if pn >= 32768 else pn); w8(side); w32(fv); w16(nv); w16(tex); w16(tinf)
        w32(lmoff); w8(lw); w8(lh); w16(bms); w16(bmt)
    w32(len(nodes))
    for nd in nodes:
        w32(nd[0]); w16(nd[1]); w16(nd[2])
        for k in range(6): w16(nd[3 + k])
        w16(nd[9] if nd[9] < 32768 else nd[9] - 65536); w16(nd[10] if nd[10] < 32768 else nd[10] - 65536)
    w32(len(leaves))
    for lf in leaves:
        w32(lf[0]); w32(lf[1])
        for k in range(6): w16(lf[2 + k])
        w16(lf[8] - 65536 if lf[8] >= 32768 else lf[8]); w16(lf[9] - 65536 if lf[9] >= 32768 else lf[9])
    w32(len(marks))
    for m in marks: w16(int(m) - 65536 if m >= 32768 else int(m))
    w32(len(visdata)); out.extend(visdata)
    w32(len(clip))
    for c in clip: w32(c[0]); w16(c[1]); w16(c[2])
    w32(len(models))
    for m in models:
        for k in range(9): wf(m[k])
        for k in range(4): w32(m[9 + k])
        w32(m[13]); w32(m[14]); w32(m[15])
    keep = {'classname', 'origin', 'angles', 'angle', 'model', 'rendermode', 'renderamt', 'spawnflags', 'targetname', 'skyname', 'health', 'material', 'pitch', '_light', 'message', 'target'}
    w32(len(ents))
    for e in ents:
        kv = [(k, v) for k, v in e.items() if k in keep]
        w16(len(kv))
        for k, v in kv: wstr(k); wstr(v)
    w32(len(lightbytes)); out.extend(lightbytes)
    wstr(skyname)
    strip = Image.new('RGB', (128, 640))
    for k, im in enumerate(skyfaces): strip.paste(im, (0, k * 128))
    strip.save(os.path.join(os.path.dirname(outpath), 'sky_' + skyname + '.png'), optimize=True)
    open(outpath, 'wb').write(out)
    log(mapname, '->', outpath, len(out), 'bytes')
    return gpal

if __name__ == '__main__':
    for m in sys.argv[2:]:
        convert(m, os.path.join(sys.argv[1], m + '.cse'))
