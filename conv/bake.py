#!/usr/bin/env python3
import os as _os
GAME = _os.environ.get('CS16_GAME', '/mnt/user-data/uploads/ks16')   # folder with cstrike/ and valve/
HERE = _os.path.dirname(_os.path.dirname(_os.path.abspath(__file__))) + '/'

"""Bake original CS 1.6 studio models into indexed sprite sets for the E52 engine.
   - v_*.mdl  -> first-person weapon frames (perspective, 320x240 screen space)
   - player models (+ merged p_ weapon) -> 8-direction billboard frames (orthographic)
"""
import numpy as np, math, struct, sys, os
from PIL import Image
from mdl import MDL

MD = GAME + '/cstrike/models/'

def norm(v):
    return v / (np.linalg.norm(v, axis=-1, keepdims=True) + 1e-9)

def skin_tex(m, skin):
    if m.skinref is not None and skin < m.nskinref:
        return m.textures[m.skinref[skin]]
    return m.textures[min(skin, len(m.textures) - 1)]

def collect(m, M, bodysel=None, extra=None):
    """returns list of (tex, verts[n,3,3], norms[n,3,3], st[n,3,2])"""
    out = []
    for bi, (bpname, base, models) in enumerate(m.bodyparts):
        mi = 0 if bodysel is None else bodysel.get(bpname, 0)
        if mi >= len(models): continue
        md = models[mi]
        if len(md['verts']) == 0: continue
        V = np.einsum('nij,nj->ni', M[md['vinfo']][:, :, :3], md['verts']) + M[md['vinfo']][:, :, 3]
        N = norm(np.einsum('nij,nj->ni', M[md['ninfo']][:, :, :3], md['norms']))
        for skin, tris in md['meshes']:
            if not tris: continue
            t = np.array(tris)  # n,3,4
            out.append((skin_tex(m, skin), V[t[:, :, 0]], N[t[:, :, 1]], t[:, :, 2:4].astype(np.float64)))
    return out

def light_tris(N, lightdir, amb=0.62, shade=0.5, lam=1.5):
    lc = -(N @ lightdir)  # cos with light
    lc = np.clip(lc, -1, 1)
    lc = (lc + (lam - 1)) / lam
    il = amb + shade * np.clip(lc, 0, None)
    return il

def raster(W, H, batches, proj, lightdir, persp=True):
    col = np.zeros((H, W, 3), np.float32)
    alpha = np.zeros((H, W), np.float32)
    zbuf = np.full((H, W), np.inf, np.float32)
    for tex, V, N, ST in batches:
        pix, pal = tex['pix'], tex['pal'].astype(np.float32)
        tw, th = tex['w'], tex['h']
        masked = bool(tex['flags'] & 0x40)
        P, Z = proj(V.reshape(-1, 3))
        P = P.reshape(-1, 3, 2); Z = Z.reshape(-1, 3)
        L = light_tris(N.reshape(-1, 3), lightdir).reshape(-1, 3)
        for i in range(len(P)):
            z = Z[i]
            if np.any(z <= 0.5): continue
            p = P[i]
            x0 = max(int(np.floor(p[:, 0].min())), 0); x1 = min(int(np.ceil(p[:, 0].max())), W - 1)
            y0 = max(int(np.floor(p[:, 1].min())), 0); y1 = min(int(np.ceil(p[:, 1].max())), H - 1)
            if x0 > x1 or y0 > y1: continue
            xs = np.arange(x0, x1 + 1) + 0.5; ys = np.arange(y0, y1 + 1) + 0.5
            X, Y = np.meshgrid(xs, ys)
            (ax, ay), (bx, by), (cx, cy) = p
            area = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax)
            if abs(area) < 1e-9: continue
            w0 = ((bx - X) * (cy - Y) - (by - Y) * (cx - X)) / area
            w1 = ((cx - X) * (ay - Y) - (cy - Y) * (ax - X)) / area
            w2 = 1 - w0 - w1
            inside = (w0 >= -1e-6) & (w1 >= -1e-6) & (w2 >= -1e-6)
            if not inside.any(): continue
            if persp:
                iz = 1.0 / z
                q = w0 * iz[0] + w1 * iz[1] + w2 * iz[2]
                b0 = w0 * iz[0] / q; b1 = w1 * iz[1] / q; b2 = w2 * iz[2] / q
                depth = 1.0 / q
            else:
                b0, b1, b2 = w0, w1, w2
                depth = w0 * z[0] + w1 * z[1] + w2 * z[2]
            sub = zbuf[y0:y1 + 1, x0:x1 + 1]
            ok = inside & (depth < sub)
            if not ok.any(): continue
            st = ST[i]
            u = b0 * st[0, 0] + b1 * st[1, 0] + b2 * st[2, 0]
            v = b0 * st[0, 1] + b1 * st[1, 1] + b2 * st[2, 1]
            ui = np.clip(u.astype(np.int64), 0, tw - 1); vi = np.clip(v.astype(np.int64), 0, th - 1)
            idx = pix[vi, ui]
            if masked: ok &= idx != 255
            li = (b0 * L[i, 0] + b1 * L[i, 1] + b2 * L[i, 2])[..., None]
            c = pal[idx] * li
            cs = col[y0:y1 + 1, x0:x1 + 1]; als = alpha[y0:y1 + 1, x0:x1 + 1]
            cs[ok] = c[ok]; als[ok] = 1; sub[ok] = depth[ok]
    return np.clip(col, 0, 255), alpha

def to_rgba(col, alpha, W, H, ss):
    img = np.concatenate([col, alpha[..., None] * 255], -1).astype(np.uint8)
    im = Image.fromarray(img, 'RGBA')
    # premultiplied downsample
    pm = col * alpha[..., None]
    pmi = Image.fromarray(np.clip(pm, 0, 255).astype(np.uint8)).resize((W, H), Image.BOX)
    ai = Image.fromarray((alpha * 255).astype(np.uint8)).resize((W, H), Image.BOX)
    a = np.array(ai).astype(np.float32) / 255
    c = np.array(pmi).astype(np.float32) / np.maximum(a[..., None], 1e-3)
    return np.clip(c, 0, 255).astype(np.uint8), a

# ------------------------------------------------------------------ viewmodels
VW, VH, SS = 320, 240, 3

def vm_proj(V):
    x = V[:, 0]; y = V[:, 1]; z = V[:, 2]
    W = VW * SS; H = VH * SS
    pr = W / 2  # fov 90
    sx = W / 2 + y / x * pr  # CS 1.6 v_ models are left-handed, mirrored for cl_righthand 1
    sy = H / 2 - z / x * pr
    return np.stack([sx, sy], -1), x

def frames_of(m, seqname, count, maxframes=None):
    si = m.seq(seqname)
    if si is None: return None, []
    nf = m.seqs[si]['frames']
    if maxframes: nf = min(nf, maxframes)
    if count <= 1: return si, [0]
    return si, [int(round(i * (nf - 1) / (count - 1))) for i in range(count)]

# weapon -> list of (anim, seqname, count, maxframes)
VM = {
    'ak47': [('idle', 'idle1', 1), ('shoot', 'shoot1', 4, 10), ('reload', 'reload', 9), ('draw', 'draw', 4)],
    'aug': [('idle', 'idle', 1), ('shoot', 'shoot1', 4, 12), ('reload', 'reload', 9), ('draw', 'draw', 4)],
    'awp': [('idle', 'idle1', 1), ('shoot', 'shoot1', 7), ('reload', 'reload', 8), ('draw', 'draw', 4)],
    'c4': [('idle', 'idle1', 1), ('shoot', 'pressbutton', 8), ('draw', 'draw', 3)],
    'deagle': [('idle', 'idle1', 1), ('shoot', 'shoot1', 4, 14), ('reload', 'reload', 7), ('draw', 'draw', 4)],
    'elite': [('idle', 'idle', 1), ('shoot', 'shoot_right1', 4, 14), ('reload', 'reload', 9), ('draw', 'draw', 4)],
    'famas': [('idle', 'idle', 1), ('shoot', 'shoot1', 4, 12), ('reload', 'reload', 8), ('draw', 'draw', 4)],
    'fiveseven': [('idle', 'idle1', 1), ('shoot', 'shoot1', 4, 14), ('reload', 'reload', 7), ('draw', 'draw', 4)],
    'flashbang': [('idle', 'idle', 1), ('pullpin', 'pullpin', 4), ('shoot', 'throw', 4), ('draw', 'deploy', 3)],
    'g3sg1': [('idle', 'idle1', 1), ('shoot', 'shoot', 4), ('reload', 'reload', 9), ('draw', 'draw', 4)],
    'galil': [('idle', 'idle', 1), ('shoot', 'shoot1', 4, 12), ('reload', 'reload', 8), ('draw', 'draw', 4)],
    'glock18': [('idle', 'idle1', 1), ('shoot', 'shoot', 4, 14), ('reload', 'reload', 7), ('draw', 'draw', 4)],
    'hegrenade': [('idle', 'idle', 1), ('pullpin', 'pullpin', 4), ('shoot', 'throw', 4), ('draw', 'deploy', 3)],
    'knife': [('idle', 'idle', 1), ('shoot', 'midslash1', 6, 40), ('shoot2', 'stab', 5), ('draw', 'draw', 4)],
    'm249': [('idle', 'idle1', 1), ('shoot', 'shoot1', 4), ('reload', 'reload', 10), ('draw', 'draw', 4)],
    'm3': [('idle', 'idle', 1), ('shoot', 'shoot1', 7), ('rstart', 'start_reload', 2), ('insert', 'insert', 4), ('rafter', 'after_reload', 3), ('draw', 'draw', 4)],
    'm4a1': [('idle', 'idle_unsil', 1), ('shoot', 'shoot1_unsil', 4, 12), ('reload', 'reload_unsil', 9), ('draw', 'draw_unsil', 4)],
    'mac10': [('idle', 'idle1', 1), ('shoot', 'shoot1', 4, 12), ('reload', 'reload', 8), ('draw', 'draw', 4)],
    'mp5': [('idle', 'idle1', 1), ('shoot', 'shoot1', 4, 12), ('reload', 'reload', 8), ('draw', 'draw', 4)],
    'p228': [('idle', 'idle1', 1), ('shoot', 'shoot1', 4, 14), ('reload', 'reload', 7), ('draw', 'draw', 4)],
    'p90': [('idle', 'idle', 1), ('shoot', 'shoot1', 4), ('reload', 'reload', 9), ('draw', 'draw', 4)],
    'scout': [('idle', 'idle1', 1), ('shoot', 'shoot', 7), ('reload', 'reload', 7), ('draw', 'draw', 4)],
    'sg550': [('idle', 'idle1', 1), ('shoot', 'shoot', 4, 16), ('reload', 'reload', 9), ('draw', 'draw', 4)],
    'sg552': [('idle', 'idle', 1), ('shoot', 'shoot1', 4, 12), ('reload', 'reload', 9), ('draw', 'draw', 4)],
    'smokegrenade': [('idle', 'idle', 1), ('pullpin', 'pullpin', 4), ('shoot', 'throw', 4), ('draw', 'deploy', 3)],
    'tmp': [('idle', 'idle1', 1), ('shoot', 'shoot', 4), ('reload', 'reload', 7), ('draw', 'draw', 3)],
    'ump45': [('idle', 'idle1', 1), ('shoot', 'shoot1', 4), ('reload', 'reload', 9), ('draw', 'draw', 4)],
    'usp': [('idle', 'idle_unsil', 1), ('shoot', 'shoot1_unsil', 4, 14), ('reload', 'reload_unsil', 8), ('draw', 'draw_unsil', 4)],
    'xm1014': [('idle', 'idle', 1), ('shoot', 'shoot1', 5), ('rstart', 'start_reload', 2), ('insert', 'insert', 4), ('rafter', 'after_reload', 2), ('draw', 'draw', 4)],
}

def quantize_set(frames_rgba):
    """frames: list of (rgb uint8 HxWx3, alpha float HxW). returns palette(256x3), list of index arrays (0=transparent)"""
    samples = []
    for rgb, a in frames_rgba:
        m = a > 0.5
        if m.any(): samples.append(rgb[m])
    S = np.concatenate(samples) if samples else np.zeros((1, 3), np.uint8)
    if len(S) > 200000: S = S[np.random.RandomState(0).choice(len(S), 200000, replace=False)]
    side = int(math.ceil(math.sqrt(len(S))))
    pad = np.zeros((side * side, 3), np.uint8); pad[:len(S)] = S; pad[len(S):] = S[0]
    q = Image.fromarray(pad.reshape(side, side, 3)).quantize(colors=255, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    pal = np.array(q.getpalette()[:255 * 3], np.uint8).reshape(-1, 3)
    pal = np.vstack([pal, np.zeros((255 - len(pal), 3), np.uint8)]) if len(pal) < 255 else pal
    pimg = Image.new('P', (1, 1)); pimg.putpalette(list(pal.flatten()) + [0, 0, 0])
    out = []
    for rgb, a in frames_rgba:
        idx = np.array(Image.fromarray(rgb).quantize(palette=pimg, dither=Image.Dither.NONE), np.uint8)
        idx = np.minimum(idx, 254) + 1
        idx[a <= 0.5] = 0
        out.append(idx)
    full = np.zeros((256, 3), np.uint8); full[1:] = pal
    return full, out

def crop(idx):
    ys, xs = np.nonzero(idx)
    if len(xs) == 0: return 0, 0, 1, 1, np.zeros((1, 1), np.uint8)
    x0, x1, y0, y1 = xs.min(), xs.max() + 1, ys.min(), ys.max() + 1
    return int(x0), int(y0), int(x1 - x0), int(y1 - y0), idx[y0:y1, x0:x1]

def bake_vm(name, outdir):
    m = MDL(MD + 'v_' + name + '.mdl')
    ld = norm(np.array([0.3, -0.25, -1.0]))
    anims = []
    allframes = []
    for spec in VM[name]:
        an, sq, cnt = spec[0], spec[1], spec[2]
        mx = spec[3] if len(spec) > 3 else None
        si, fl = frames_of(m, sq, cnt, mx)
        if si is None:
            print('missing seq', name, sq); continue
        q = m.seqs[si]
        nf = min(q['frames'], mx) if mx else q['frames']
        dur = max(1, nf - 1) / q['fps']
        recs = []
        for f in fl:
            pos, rot = m.pose(si, f)
            M = m.matrices(pos, rot)
            col, al = raster(VW * SS, VH * SS, collect(m, M), vm_proj, ld, True)
            rgb, a = to_rgba(col, al, VW, VH, SS)
            recs.append(len(allframes)); allframes.append((rgb, a))
        anims.append((an, dur, recs))
    pal, idxs = quantize_set(allframes)
    out = bytearray(b'VWS1')
    out += pal.tobytes()
    out += struct.pack('>H', len(anims))
    for an, dur, recs in anims:
        b = an.encode(); out += struct.pack('>H', len(b)) + b
        out += struct.pack('>fH', dur, len(recs))
        for r in recs:
            x, y, w, h, data = crop(idxs[r])
            out += struct.pack('>hhhh', x, y, w, h) + data.tobytes()
    open(os.path.join(outdir, 'v_' + name + '.vws'), 'wb').write(out)
    # preview
    pv = Image.new('RGB', (VW, VH), (60, 60, 70))
    r = anims[0][2][0]
    rgb = pal[idxs[r]]; mk = idxs[r] > 0
    bg = np.array(pv); bg[mk] = rgb[mk]
    Image.fromarray(bg).save(os.path.join(outdir, '..', 'shots', 'vm_' + name + '.png'))
    return len(out)

# ------------------------------------------------------------------ players
PPU = 1.6   # sprite pixels per world unit
PS = 3      # supersample

def player_pose(m, upper, legs, uf, lf, blend=4):
    su = m.seq(upper); sl = m.seq(legs)
    pu, ru = m.pose(su, uf, min(blend, m.seqs[su]['nbl'] - 1))
    pl, rl = m.pose(sl, lf, 0)
    names = [b[0] for b in m.bones]
    pos = pu.copy(); rot = ru.copy()
    copy = True
    for i, b in enumerate(m.bones):
        if b[0] == 'Bip01 Spine': copy = False
        elif b[1] >= 0 and names[b[1]] == 'Bip01 Pelvis': copy = True
        if copy: pos[i] = pl[i]; rot[i] = rl[i]
    # remove forward motion of root for gait
    return m.matrices(pos, rot)

def merged_weapon(pm, M_player, pnames):
    nb = len(pm.bones)
    Mw = np.zeros((nb, 3, 4))
    for b in range(nb):
        nm, par, val, sc = pm.bones[b]
        if nm in pnames:
            Mw[b] = M_player[pnames.index(nm)]
        else:
            from mdl import quat_mat, angle_quat
            r = quat_mat(angle_quat(val[3:6])); loc = np.zeros((3, 4)); loc[:, :3] = r; loc[:, 3] = val[:3]
            P = Mw[par]; Mw[b][:, :3] = P[:, :3] @ loc[:, :3]; Mw[b][:, 3] = P[:, :3] @ loc[:, 3] + P[:, 3]
    return Mw

def bake_player(model, pweapon, outname, outdir):
    m = MDL(MD + 'player/%s/%s.mdl' % (model, model))
    pm = MDL(MD + 'p_' + pweapon + '.mdl')
    pnames = [b[0] for b in m.bones]
    # (anim, upper, legs, count, upper_frame_fn)
    specs = [
        ('stand', 'ref_aim_ak47', 'idle1', 1),
        ('run', 'ref_aim_ak47', 'run', 8),
        ('crouch', 'crouch_aim_ak47', 'crouch_idle', 1),
        ('crouchrun', 'crouch_aim_ak47', 'crouchrun', 4),
        ('die', None, 'death1', 5),
        ('die2', None, 'head', 5),
    ]
    W = int(110 * PPU) * PS; H = int(110 * PPU) * PS
    allframes = []; anims = []
    for an, up, lg, cnt in specs:
        sl = m.seq(lg); nf = m.seqs[sl]['frames']
        fl = [0] if cnt == 1 else ([int(i * nf / cnt) for i in range(cnt)] if an in ('run', 'crouchrun') else [int(round(i * (nf - 1) / (cnt - 1))) for i in range(cnt)])
        dirs = []
        for d in range(8):
            ang = -math.radians(d * 45)
            ca, sa = math.cos(ang), math.sin(ang)
            def proj(V, ca=ca, sa=sa):
                x = V[:, 0] * ca - V[:, 1] * sa; y = V[:, 0] * sa + V[:, 1] * ca; z = V[:, 2]
                sx = W / 2 + y * PPU * PS
                sy = H * 0.5 - z * PPU * PS
                return np.stack([sx, sy], -1), 1000 - x
            recs = []
            for f in fl:
                if up is None:
                    pos, rot = m.pose(sl, f); M = m.matrices(pos, rot)
                else:
                    M = player_pose(m, up, lg, 0, f)
                batches = collect(m, M, {'backpack': 0})
                Mw = merged_weapon(pm, M, pnames)
                batches += collect(pm, Mw)
                # light from above-front of the model (rotated with view so shading is consistent)
                ld = norm(np.array([-0.4 * ca, 0.4 * sa, -1.0]))
                col, al = raster(W, H, batches, proj, ld, False)
                rgb, a = to_rgba(col, al, W // PS, H // PS, PS)
                recs.append(len(allframes)); allframes.append((rgb, a))
            dirs.append(recs)
        q = m.seqs[sl]
        anims.append((an, max(1, q['frames'] - 1) / q['fps'], dirs))
    pal, idxs = quantize_set(allframes)
    out = bytearray(b'PLS1')
    out += pal.tobytes()
    out += struct.pack('>fhh', PPU, W // PS // 2, H // PS // 2)  # anchor = model origin
    out += struct.pack('>H', len(anims))
    for an, dur, dirs in anims:
        b = an.encode(); out += struct.pack('>H', len(b)) + b
        out += struct.pack('>fH', dur, len(dirs[0]))
        for d in range(8):
            for r in dirs[d]:
                x, y, w, h, data = crop(idxs[r])
                out += struct.pack('>hhhh', x, y, w, h) + data.tobytes()
    open(os.path.join(outdir, outname + '.pls'), 'wb').write(out)
    # preview sheet: stand 8 dirs + run dir0 frames
    sheet = Image.new('RGB', ((W // PS) * 8, (H // PS) * 2), (90, 90, 100))
    for d in range(8):
        r = anims[0][2][d][0]
        im = pal[idxs[r]]; mk = idxs[r] > 0; bg = np.full(im.shape, 90, np.uint8); bg[mk] = im[mk]
        sheet.paste(Image.fromarray(bg), (d * (W // PS), 0))
    for k, r in enumerate(anims[1][2][1][:4] + anims[4][2][0][:4]):
        im = pal[idxs[r]]; mk = idxs[r] > 0; bg = np.full(im.shape, 90, np.uint8); bg[mk] = im[mk]
        sheet.paste(Image.fromarray(bg), (k * (W // PS), H // PS))
    sheet.save(os.path.join(outdir, '..', 'shots', 'pl_' + outname + '.png'))
    return len(out)

if __name__ == '__main__' and sys.argv[2] not in ('world', 'sil'):
    outdir = sys.argv[1]
    what = sys.argv[2:]
    for w in what:
        if w.startswith('pl:'):
            _, mdl, wep, nm = w.split(':')
            print(nm, bake_player(mdl, wep, nm, outdir), flush=True)
        else:
            print(w, bake_vm(w, outdir), flush=True)

# ------------------------------------------------------------------ world weapons (dropped)
WNAMES = "ak47 aug awp c4 deagle elite famas fiveseven flashbang g3sg1 galil glock18 hegrenade knife m249 m3 m4a1 mac10 mp5 p228 p90 scout sg550 sg552 smokegrenade tmp ump45 usp xm1014 backpack".split()
WPPU = 2.0

def bake_world(outdir):
    W = H = int(64 * WPPU) * PS
    allframes = []; anims = []
    elev = math.radians(35)
    for nm in WNAMES:
        m = MDL(MD + 'w_' + nm + '.mdl')
        si = 0
        pos, rot = m.pose(si, 0); M = m.matrices(pos, rot)
        dirs = []
        for d in range(8):
            ang = -math.radians(d * 45)
            ca, sa = math.cos(ang), math.sin(ang)
            ce, se = math.cos(elev), math.sin(elev)
            def proj(V, ca=ca, sa=sa):
                x = V[:, 0] * ca - V[:, 1] * sa; y = V[:, 0] * sa + V[:, 1] * ca; z = V[:, 2]
                # camera looks along -x, tilted down by elev
                depth = -x * ce + z * se
                up = z * ce + x * se
                sx = W / 2 + y * WPPU * PS
                sy = H * 0.5 - up * WPPU * PS
                return np.stack([sx, sy], -1), 1000 - depth
            ld = norm(np.array([-0.3 * ca, 0.3 * sa, -1.0]))
            col, al = raster(W, H, collect(m, M), proj, ld, False)
            rgb, a = to_rgba(col, al, W // PS, H // PS, PS)
            dirs.append([len(allframes)]); allframes.append((rgb, a))
        anims.append(('w_' + nm, 1.0, dirs))
    pal, idxs = quantize_set(allframes)
    out = bytearray(b'PLS1') + pal.tobytes()
    out += struct.pack('>fhh', WPPU, W // PS // 2, H // PS // 2)
    out += struct.pack('>H', len(anims))
    for an, dur, dirs in anims:
        b = an.encode(); out += struct.pack('>H', len(b)) + b + struct.pack('>fH', dur, 1)
        for d in range(8):
            x, y, w, h, data = crop(idxs[dirs[d][0]])
            out += struct.pack('>hhhh', x, y, w, h) + data.tobytes()
    open(os.path.join(outdir, 'w.pls'), 'wb').write(out)
    sheet = Image.new('RGB', ((W // PS) * 8, (H // PS) * 3), (90, 90, 100))
    for k, an in enumerate([0, 1, 3]):
        for d in range(8):
            r = anims[an][2][d][0]; im = pal[idxs[r]]; mk = idxs[r] > 0; bg = np.full(im.shape, 90, np.uint8); bg[mk] = im[mk]
            sheet.paste(Image.fromarray(bg), (d * (W // PS), k * (H // PS)))
    sheet.save(os.path.join(outdir, '..', 'shots', 'w_sheet.png'))
    return len(out)

VM['m4a1s'] = [('idle', 'idle', 1), ('shoot', 'shoot1', 4, 12), ('reload', 'reload', 9), ('draw', 'draw', 4), ('attach', 'add_silencer', 6), ('detach', 'detach_silencer', 6)]
VM['usps'] = [('idle', 'idle', 1), ('shoot', 'shoot1', 4, 14), ('reload', 'reload', 8), ('draw', 'draw', 4), ('attach', 'add_silencer', 7), ('detach', 'detach_silencer', 7)]
_orig_bake_vm = bake_vm
def bake_vm2(name, outdir):
    global MD
    if name in ('m4a1s', 'usps'):
        import shutil
        src = MD + 'v_' + name[:-1] + '.mdl'; dst = '/tmp/v_' + name + '.mdl'
        shutil.copy(src, dst)
        old = MD; MD = '/tmp/'
        try: return _orig_bake_vm(name, outdir)
        finally: MD = old
    return _orig_bake_vm(name, outdir)

if __name__ == '__main__' and len(sys.argv) > 2 and sys.argv[2] in ('world', 'sil'):
    if sys.argv[2] == 'world': print('w', bake_world(sys.argv[1]))
    else:
        for n in ('m4a1s', 'usps'): print(n, bake_vm2(n, sys.argv[1]))
