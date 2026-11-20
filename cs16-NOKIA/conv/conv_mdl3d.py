import os as _os
GAME = _os.environ.get('CS16_GAME', '/mnt/user-data/uploads/ks16')   # folder with cstrike/ and valve/
HERE = _os.path.dirname(_os.path.dirname(_os.path.abspath(__file__))) + '/'

"""GoldSrc .mdl -> compact real-3D formats for the Java engine.

mdltex.bin  'CST1'  shared 255-colour palette + all model skins (downscaled)
<name>.m3   'CSM2'  bones (parent, name, default local pos+quat), body parts / submodels, bone-space verts+normals, textured tris
<team>.a3   'CSA2'  player animation set: gait / aim / shoot (3 pitch blends) / death sequences, quantised quaternions
"""
import os, sys, struct, math, hashlib
import numpy as np
from PIL import Image
sys.path.insert(0, os.path.dirname(__file__))
from mdl import MDL, angle_quat

MD = GAME + '/cstrike/models/'
OUT = HERE + 'res/'
VSCALE = 16.0

tex_list = []          # (key, w, h, rgb array)
tex_index = {}


def be(fmt, *a): return struct.pack('>' + fmt, *a)


def utf(s):
    b = s.encode('utf-8'); return be('H', len(b)) + b


def add_tex(t, maxdim):
    w, h = t['w'], t['h']
    rgb = t['pal'][t['pix']]                     # h,w,3
    key = hashlib.md5(rgb.tobytes()).hexdigest()
    sc = 1
    while max(w, h) // sc > maxdim: sc *= 2
    if key in tex_index: return tex_index[key]
    nw, nh = max(1, w // sc), max(1, h // sc)
    im = Image.fromarray(rgb.astype(np.uint8)).resize((nw, nh), Image.BOX if sc > 1 else Image.NEAREST)
    tex_index[key] = (len(tex_list), nw / w, nh / h)
    tex_list.append((key, nw, nh, np.asarray(im)))
    return tex_index[key]


def default_local(m):
    """local pos + quaternion of every bone in sequence 0 frame 0"""
    pos, rot = m.pose(0, 0)
    out = []
    for b in range(len(m.bones)):
        q = angle_quat(rot[b])
        out.append((pos[b], q))
    return out


def write_mesh(m, outname, maxdim, parts=None):
    out = bytearray(b'CSM2')
    dl = default_local(m)
    out += be('h', len(m.bones))
    for b, (nm, par, val, sc) in enumerate(m.bones):
        p, q = dl[b]
        out += be('b', par) + utf(nm) + be('7f', *[float(x) for x in p], *[float(x) for x in q])
    bps = m.bodyparts if parts is None else [bp for bp in m.bodyparts if bp[0] in parts]
    out += be('h', len(bps))
    ntri = 0
    for name, base, models in bps:
        out += utf(name) + be('b', len(models))
        for mo in models:
            V = mo['verts']; N = mo['norms']
            out += be('h', len(V))
            for i in range(len(V)):
                out += be('b', int(mo['vinfo'][i])) + be('3h', *[int(round(float(x) * VSCALE)) for x in V[i]])
            out += be('h', len(N))
            for i in range(len(N)):
                n = N[i] / max(1e-6, np.linalg.norm(N[i]))
                out += be('b', int(mo['ninfo'][i])) + be('3b', *[int(round(float(x) * 127)) for x in n])
            meshes = mo['meshes']
            out += be('h', len(meshes))
            for skin, tris in meshes:
                ti = int(m.skinref[skin]) if m.skinref is not None else skin
                t = m.textures[ti]
                gid, fx, fy = add_tex(t, maxdim)
                out += be('hh', gid, len(tris))
                ntri += len(tris)
                for tri in tris:
                    for (vi, ni, s, tt) in tri:
                        out += be('4h', vi, ni, int(s * fx), int(tt * fy))
    open(OUT + outname + '.m3', 'wb').write(out)
    return len(out), ntri


# ------------------------------------------------------------------ animations
def legs_mask(m):
    names = [b[0] for b in m.bones]
    mask = []; copy = True
    for b in m.bones:
        if b[0] == 'Bip01 Spine': copy = False
        elif b[1] >= 0 and names[b[1]] == 'Bip01 Pelvis': copy = True
        mask.append(copy)
    return mask


CLASSES = ['knife', 'onehanded', 'dualpistols', 'shotgun', 'mp5', 'carbine', 'ak47', 'rifle', 'm249', 'grenade', 'c4']


def seq_list(m):
    L = []   # (name, src seq, frames list, blends list, bone set)
    legs = legs_mask(m)
    up = [not x for x in legs]
    allb = [True] * len(m.bones)
    def sub(name, step=None, maxf=None):
        q = m.seqs[m.seq(name)]
        n = q['frames']
        if maxf and n > maxf: step = max(1, math.ceil(n / maxf))
        step = step or 1
        return list(range(0, n, step)), q['fps'] / step
    for g, mx in (('idle1', 16), ('crouch_idle', 12), ('walk', 17), ('run', 19), ('crouchrun', 16), ('jump', 20)):
        fr, fps = sub(g, maxf=mx); L.append((g, g, fr, fps, [0], legs))
    for c in CLASSES:
        for pre in ('ref', 'crouch'):
            a = '%s_aim_%s' % (pre, c); s = '%s_shoot_%s' % (pre, c)
            if m.seq(a) is not None: L.append((a, a, [0], 1.0, [2, 4, 6], up))
            if m.seq(s) is not None:
                fr, fps = sub(s, maxf=12 if c == 'c4' else 6); L.append((s, s, fr, fps, [2, 4, 6] if m.seqs[m.seq(s)]['nbl'] > 1 else [0], up))
    for d in ('death1', 'death2', 'death3', 'head', 'gutshot', 'back', 'forward', 'crouch_die'):
        fr, fps = sub(d, maxf=16); L.append((d, d, fr, fps, [0], allb))
    return L


def write_anim_list(m, outname, sl):
    out = bytearray(b'CSA3')
    nb = len(m.bones)
    out += be('h', nb) + be('h', len(sl))
    nfr = 0
    for name, src, frames, fps, blends, mask in sl:
        si = m.seq(src)
        poses = {(b, f): m.pose(si, f, b if b < m.seqs[si]['nbl'] else 0) for b in blends for f in frames}
        bones = [i for i in range(nb) if mask[i]]
        P = np.array([[poses[(b, f)][0][i] for i in bones] for b in blends for f in frames])
        Qs = []
        for b in blends:
            for f in frames:
                row = []
                for i in bones:
                    q = np.array(angle_quat(poses[(b, f)][1][i]))
                    if q[3] < 0: q = -q
                    row.append(q)
                Qs.append(row)
        Qs = np.array(Qs)   # n, nbones, 4
        n = len(Qs)
        panim = (P.std(0).max(1) > 0.05) if n > 1 else np.zeros(len(bones), bool)
        ranim = (np.abs(Qs - Qs[0:1]).max((0, 2)) > 0.002) if n > 1 else np.zeros(len(bones), bool)
        out += utf(name) + be('fhbh', float(fps), len(frames), len(blends), len(bones))
        for k, i in enumerate(bones):
            out += be('bb', i, (1 if panim[k] else 0) | (2 if ranim[k] else 0))
            if not panim[k]: out += be('3h', *[int(round(float(x) * VSCALE)) for x in P[0][k]])
            if not ranim[k]: out += be('3h', *[int(round(float(x) * 32767)) for x in Qs[0][k][:3]])
        for j in range(n):
            for k, i in enumerate(bones):
                if ranim[k]: out += be('3h', *[int(round(float(x) * 32767)) for x in Qs[j][k][:3]])
                if panim[k]: out += be('3h', *[int(round(float(x) * VSCALE)) for x in P[j][k]])
            nfr += 1
    open(OUT + outname + '.a3', 'wb').write(out)
    return len(out), len(sl), nfr


def write_anim(m, outname):
    return write_anim_list(m, outname, seq_list(m))


# first-person weapons: our animation names -> sequences of the original v_ models
VMSEQ = {
    'ak47': [('idle', 'idle1'), ('shoot', 'shoot1'), ('reload', 'reload'), ('draw', 'draw')],
    'aug': [('idle', 'idle'), ('shoot', 'shoot1'), ('reload', 'reload'), ('draw', 'draw')],
    'awp': [('idle', 'idle1'), ('shoot', 'shoot1'), ('reload', 'reload'), ('draw', 'draw')],
    'c4': [('idle', 'idle1'), ('shoot', 'pressbutton'), ('draw', 'draw')],
    'deagle': [('idle', 'idle1'), ('shoot', 'shoot1'), ('reload', 'reload'), ('draw', 'draw')],
    'elite': [('idle', 'idle'), ('shoot', 'shoot_right1'), ('shoot2', 'shoot_left1'), ('reload', 'reload'), ('draw', 'draw')],
    'famas': [('idle', 'idle'), ('shoot', 'shoot1'), ('reload', 'reload'), ('draw', 'draw')],
    'fiveseven': [('idle', 'idle1'), ('shoot', 'shoot1'), ('reload', 'reload'), ('draw', 'draw')],
    'flashbang': [('idle', 'idle'), ('pullpin', 'pullpin'), ('shoot', 'throw'), ('draw', 'deploy')],
    'g3sg1': [('idle', 'idle1'), ('shoot', 'shoot'), ('reload', 'reload'), ('draw', 'draw')],
    'galil': [('idle', 'idle'), ('shoot', 'shoot1'), ('reload', 'reload'), ('draw', 'draw')],
    'glock18': [('idle', 'idle1'), ('shoot', 'shoot'), ('reload', 'reload'), ('draw', 'draw')],
    'hegrenade': [('idle', 'idle'), ('pullpin', 'pullpin'), ('shoot', 'throw'), ('draw', 'deploy')],
    'knife': [('idle', 'idle'), ('shoot', 'midslash1'), ('shoot2', 'stab'), ('draw', 'draw')],
    'm249': [('idle', 'idle1'), ('shoot', 'shoot1'), ('reload', 'reload'), ('draw', 'draw')],
    'm3': [('idle', 'idle'), ('shoot', 'shoot1'), ('rstart', 'start_reload'), ('insert', 'insert'), ('rafter', 'after_reload'), ('draw', 'draw')],
    'm4a1': [('idle', 'idle_unsil'), ('shoot', 'shoot1_unsil'), ('reload', 'reload_unsil'), ('draw', 'draw_unsil')],
    'm4a1s': [('idle', 'idle'), ('shoot', 'shoot1'), ('reload', 'reload'), ('draw', 'draw'), ('attach', 'add_silencer'), ('detach', 'detach_silencer')],
    'mac10': [('idle', 'idle1'), ('shoot', 'shoot1'), ('reload', 'reload'), ('draw', 'draw')],
    'mp5': [('idle', 'idle1'), ('shoot', 'shoot1'), ('reload', 'reload'), ('draw', 'draw')],
    'p228': [('idle', 'idle1'), ('shoot', 'shoot1'), ('reload', 'reload'), ('draw', 'draw')],
    'p90': [('idle', 'idle'), ('shoot', 'shoot1'), ('reload', 'reload'), ('draw', 'draw')],
    'scout': [('idle', 'idle1'), ('shoot', 'shoot'), ('reload', 'reload'), ('draw', 'draw')],
    'sg550': [('idle', 'idle1'), ('shoot', 'shoot'), ('reload', 'reload'), ('draw', 'draw')],
    'sg552': [('idle', 'idle'), ('shoot', 'shoot1'), ('reload', 'reload'), ('draw', 'draw')],
    'smokegrenade': [('idle', 'idle'), ('pullpin', 'pullpin'), ('shoot', 'throw'), ('draw', 'deploy')],
    'tmp': [('idle', 'idle1'), ('shoot', 'shoot'), ('reload', 'reload'), ('draw', 'draw')],
    'ump45': [('idle', 'idle1'), ('shoot', 'shoot1'), ('reload', 'reload'), ('draw', 'draw')],
    'usp': [('idle', 'idle_unsil'), ('shoot', 'shoot1_unsil'), ('reload', 'reload_unsil'), ('draw', 'draw_unsil')],
    'usps': [('idle', 'idle'), ('shoot', 'shoot1'), ('reload', 'reload'), ('draw', 'draw'), ('attach', 'add_silencer'), ('detach', 'detach_silencer')],
    'xm1014': [('idle', 'idle'), ('shoot', 'shoot1'), ('rstart', 'start_reload'), ('insert', 'insert'), ('rafter', 'after_reload'), ('draw', 'draw')],
}


def write_vm(nm):
    src = nm[:-1] if nm in ('m4a1s', 'usps') else nm
    m = MDL(MD + 'v_' + src + '.mdl')
    allb = [True] * len(m.bones)
    sl = []
    for an, sq in VMSEQ[nm]:
        si = m.seq(sq)
        if si is None: print('missing', nm, sq); continue
        q = m.seqs[si]; n = q['frames']
        step = 1 if n <= 48 else int(math.ceil(n / 48.0))
        sl.append((an, sq, list(range(0, n, step)), q['fps'] / step, [0], allb))
    a = write_anim_list(m, 'v_' + nm, sl)
    msz = (0, 0)
    if nm == src: msz = write_mesh(m, 'v_' + nm, 128)
    return a[0] + msz[0], msz[1]


def write_textures():
    # one shared palette for every model skin (index 255 unused)
    allpx = np.concatenate([t[3].reshape(-1, 3) for t in tex_list], 0)
    sample = allpx[::max(1, len(allpx) // 200000)]
    pim = Image.fromarray(sample.reshape(1, -1, 3).astype(np.uint8))
    q = pim.quantize(colors=255, method=Image.Quantize.MEDIANCUT)
    pal = np.array(q.getpalette()[:255 * 3], np.uint8).reshape(-1, 3)
    palimg = Image.new('P', (1, 1)); palimg.putpalette(list(pal.reshape(-1)) + [0, 0, 0])
    out = bytearray(b'CST1') + be('h', len(tex_list))
    out += bytes(pal.reshape(-1)) + b'\0\0\0'
    for key, w, h, rgb in tex_list:
        im = Image.fromarray(rgb.astype(np.uint8)).quantize(palette=palimg, dither=Image.Dither.NONE)
        idx = np.asarray(im, np.uint8)
        out += be('hh', w, h) + idx.tobytes()
    open(OUT + 'mdltex.bin', 'wb').write(out)
    return len(out)


if __name__ == '__main__':
    tot = 0
    for team, mdl in (('t', 'terror'), ('ct', 'gign')):
        m = MDL(MD + 'player/%s/%s.mdl' % (mdl, mdl))
        n, nt = write_mesh(m, 'pl_' + team, 256); tot += n
        a, ns, nf = write_anim(m, 'pl_' + team); tot += a
        print(team, 'mesh', n, 'tris', nt, 'anim', a, 'seqs', ns, 'frames', nf)
    P = "ak47 aug awp c4 deagle elite famas fiveseven flashbang g3sg1 galil glock18 hegrenade knife m249 m3 m4a1 mac10 mp5 p228 p90 scout sg550 sg552 smokegrenade tmp ump45 usp xm1014".split()
    for nm in P:
        n, nt = write_mesh(MDL(MD + 'p_' + nm + '.mdl'), 'p_' + nm, 128); tot += n
    W = P + ['backpack']
    for nm in W:
        n, nt = write_mesh(MDL(MD + 'w_' + nm + '.mdl'), 'w_' + nm, 128); tot += n
    for nm in VMSEQ:
        n, nt = write_vm(nm); tot += n
    tt = write_textures(); tot += tt
    print('textures', len(tex_list), tt, 'total bytes', tot)
