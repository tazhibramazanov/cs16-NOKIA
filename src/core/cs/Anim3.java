package cs;

import java.io.*;

/** Player animation set (.a3): quantised quaternions per bone/frame, up to 3 pitch blends per sequence. */
public final class Anim3 {
    public int nb, nseq;
    public String[] name;
    public float[] fps;
    public int[] nfr, nbl;
    int[][] bones;          // bone index per used bone
    boolean[][] posAnim, rotAnim;
    short[][] constRot;
    short[][] constPos;     // 3 per used bone (only meaningful when !posAnim)
    int[][] off;            // offset of each used bone inside a frame record
    int[] stride;           // shorts per frame record
    short[][] data;         // [blend][frame][record]

    public static Anim3 load(String file) {
        try {
            InputStream is = Sys.s.open("/" + file + ".a3");
            if (is == null) return null;
            DataInputStream in = new DataInputStream(is);
            if (in.readInt() != 0x43534133) return null;
            Anim3 a = new Anim3();
            a.nb = in.readShort();
            int ns = a.nseq = in.readShort();
            a.name = new String[ns]; a.fps = new float[ns]; a.nfr = new int[ns]; a.nbl = new int[ns];
            a.bones = new int[ns][]; a.posAnim = new boolean[ns][]; a.rotAnim = new boolean[ns][]; a.constRot = new short[ns][]; a.constPos = new short[ns][]; a.off = new int[ns][]; a.stride = new int[ns]; a.data = new short[ns][];
            for (int s = 0; s < ns; s++) {
                a.name[s] = in.readUTF(); a.fps[s] = in.readFloat(); a.nfr[s] = in.readShort(); a.nbl[s] = in.readByte();
                int nu = in.readShort();
                int[] bi = new int[nu]; boolean[] pa = new boolean[nu], ra = new boolean[nu]; short[] cp = new short[nu * 3], cr = new short[nu * 3]; int[] of = new int[nu];
                int st = 0;
                for (int k = 0; k < nu; k++) {
                    bi[k] = in.readByte(); int fl = in.readByte(); pa[k] = (fl & 1) != 0; ra[k] = (fl & 2) != 0;
                    if (!pa[k]) { cp[k * 3] = in.readShort(); cp[k * 3 + 1] = in.readShort(); cp[k * 3 + 2] = in.readShort(); }
                    if (!ra[k]) { cr[k * 3] = in.readShort(); cr[k * 3 + 1] = in.readShort(); cr[k * 3 + 2] = in.readShort(); }
                    of[k] = st; st += (ra[k] ? 3 : 0) + (pa[k] ? 3 : 0);
                }
                short[] d = new short[a.nbl[s] * a.nfr[s] * st];
                for (int i = 0; i < d.length; i++) d[i] = in.readShort();
                a.bones[s] = bi; a.posAnim[s] = pa; a.rotAnim[s] = ra; a.constRot[s] = cr; a.constPos[s] = cp; a.off[s] = of; a.stride[s] = st; a.data[s] = d;
            }
            in.close();
            return a;
        } catch (IOException e) { return null; }
    }

    public int find(String n) { for (int i = 0; i < nseq; i++) if (name[i].equals(n)) return i; return -1; }

    public float duration(int s) { return s < 0 ? 0 : nfr[s] / fps[s]; }

    /**
     * write local rotations/positions of the bones this sequence drives.
     * frame: fractional frame; loop: wrap or clamp; blend: 0..nbl-1 (fractional)
     */
    public void pose(int s, float frame, boolean loop, float blend, float[] Q, float[] P) {
        if (s < 0) return;
        int n = nfr[s];
        int f0, f1; float tf;
        if (loop) {
            frame = frame % n; if (frame < 0) frame += n;
            f0 = (int) frame; f1 = f0 + 1 >= n ? 0 : f0 + 1; tf = frame - f0;
        } else {
            if (frame < 0) frame = 0; if (frame > n - 1) frame = n - 1;
            f0 = (int) frame; f1 = f0 + 1 > n - 1 ? n - 1 : f0 + 1; tf = frame - f0;
        }
        int nbs = nbl[s];
        if (blend < 0) blend = 0; if (blend > nbs - 1) blend = nbs - 1;
        int b0 = (int) blend, b1 = b0 + 1 > nbs - 1 ? nbs - 1 : b0 + 1; float tb = blend - b0;
        int st = stride[s];
        short[] d = data[s];
        int r00 = (b0 * n + f0) * st, r01 = (b0 * n + f1) * st, r10 = (b1 * n + f0) * st, r11 = (b1 * n + f1) * st;
        float w00 = (1 - tf) * (1 - tb), w01 = tf * (1 - tb), w10 = (1 - tf) * tb, w11 = tf * tb;
        int[] bi = bones[s]; int[] of = off[s]; boolean[] pa = posAnim[s], ra = rotAnim[s]; short[] cp = constPos[s], cr = constRot[s];
        final float QS = 1f / 32767f;
        for (int k = 0; k < bi.length; k++) {
            int b = bi[k], o = of[k];
            if (!ra[k]) {
                float x = cr[k * 3] * QS, y = cr[k * 3 + 1] * QS, z = cr[k * 3 + 2] * QS;
                Q[b * 4] = x; Q[b * 4 + 1] = y; Q[b * 4 + 2] = z; Q[b * 4 + 3] = sq(x, y, z);
            } else {
                float qx = 0, qy = 0, qz = 0, qw = 0;
                float ax = d[r00 + o] * QS, ay = d[r00 + o + 1] * QS, az = d[r00 + o + 2] * QS, aw = sq(ax, ay, az);
                qx += ax * w00; qy += ay * w00; qz += az * w00; qw += aw * w00;
                if (w01 > 0) { float x = d[r01 + o] * QS, y = d[r01 + o + 1] * QS, z = d[r01 + o + 2] * QS, w = sq(x, y, z); float sg = ax * x + ay * y + az * z + aw * w < 0 ? -w01 : w01; qx += x * sg; qy += y * sg; qz += z * sg; qw += w * sg; }
                if (w10 > 0) { float x = d[r10 + o] * QS, y = d[r10 + o + 1] * QS, z = d[r10 + o + 2] * QS, w = sq(x, y, z); float sg = ax * x + ay * y + az * z + aw * w < 0 ? -w10 : w10; qx += x * sg; qy += y * sg; qz += z * sg; qw += w * sg; }
                if (w11 > 0) { float x = d[r11 + o] * QS, y = d[r11 + o + 1] * QS, z = d[r11 + o + 2] * QS, w = sq(x, y, z); float sg = ax * x + ay * y + az * z + aw * w < 0 ? -w11 : w11; qx += x * sg; qy += y * sg; qz += z * sg; qw += w * sg; }
                float l = (float) Math.sqrt(qx * qx + qy * qy + qz * qz + qw * qw);
                if (l < 1e-6f) l = 1;
                l = 1 / l;
                Q[b * 4] = qx * l; Q[b * 4 + 1] = qy * l; Q[b * 4 + 2] = qz * l; Q[b * 4 + 3] = qw * l;
                o += 3;
            }
            if (pa[k]) {
                P[b * 3] = (d[r00 + o] * w00 + d[r01 + o] * w01 + d[r10 + o] * w10 + d[r11 + o] * w11) / 16f;
                P[b * 3 + 1] = (d[r00 + o + 1] * w00 + d[r01 + o + 1] * w01 + d[r10 + o + 1] * w10 + d[r11 + o + 1] * w11) / 16f;
                P[b * 3 + 2] = (d[r00 + o + 2] * w00 + d[r01 + o + 2] * w01 + d[r10 + o + 2] * w10 + d[r11 + o + 2] * w11) / 16f;
            } else { P[b * 3] = cp[k * 3] / 16f; P[b * 3 + 1] = cp[k * 3 + 1] / 16f; P[b * 3 + 2] = cp[k * 3 + 2] / 16f; }
        }
    }

    private static float sq(float x, float y, float z) { float t = 1 - x * x - y * y - z * z; return t > 0 ? (float) Math.sqrt(t) : 0; }
}
