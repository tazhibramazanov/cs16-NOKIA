package cs;

import java.io.*;

/** Real 3D studio model (converted from GoldSrc .mdl): skeleton, body parts, bone-space vertices/normals, textured triangles. */
public final class Mdl3 {
    public int nb;
    public byte[] parent;
    public String[] bname;
    public float[] defP, defQ;            // default local pose (seq 0 frame 0)
    public String[] partName;
    public Sub[][] parts;

    public static final class Sub {
        public int nv, nn;
        public byte[] vbone, nbone;
        public float[] v, n;               // bone space
        public short[] mtex;               // per mesh: global texture id
        public short[][] tri;              // per mesh: 12 shorts per triangle (v, n, s, t) x3
    }

    // ---- shared skins (mdltex.bin)
    public static int[] pal;              // 255 colours
    public static byte[][] tex;
    public static int[] texW, texH;

    public static void loadTextures() throws IOException {
        if (tex != null) return;
        DataInputStream in = new DataInputStream(Sys.s.open("/mdltex.bin"));
        if (in.readInt() != 0x43535431) throw new IOException("mdltex");
        int n = in.readShort();
        pal = new int[256];
        for (int i = 0; i < 256; i++) pal[i] = (in.readUnsignedByte() << 16) | (in.readUnsignedByte() << 8) | in.readUnsignedByte();
        tex = new byte[n][]; texW = new int[n]; texH = new int[n];
        for (int i = 0; i < n; i++) {
            texW[i] = in.readShort(); texH[i] = in.readShort();
            tex[i] = new byte[texW[i] * texH[i]];
            in.readFully(tex[i]);
        }
        in.close();
    }

    public static Mdl3 load(String name) {
        try {
            InputStream is = Sys.s.open("/" + name + ".m3");
            if (is == null) return null;
            DataInputStream in = new DataInputStream(is);
            if (in.readInt() != 0x43534D32) return null;
            Mdl3 m = new Mdl3();
            int nb = m.nb = in.readShort();
            m.parent = new byte[nb]; m.bname = new String[nb]; m.defP = new float[nb * 3]; m.defQ = new float[nb * 4];
            for (int b = 0; b < nb; b++) {
                m.parent[b] = in.readByte(); m.bname[b] = in.readUTF();
                for (int k = 0; k < 3; k++) m.defP[b * 3 + k] = in.readFloat();
                for (int k = 0; k < 4; k++) m.defQ[b * 4 + k] = in.readFloat();
            }
            int np = in.readShort();
            m.partName = new String[np]; m.parts = new Sub[np][];
            for (int p = 0; p < np; p++) {
                m.partName[p] = in.readUTF();
                int ns = in.readByte();
                m.parts[p] = new Sub[ns];
                for (int s = 0; s < ns; s++) {
                    Sub u = new Sub();
                    u.nv = in.readShort(); u.vbone = new byte[u.nv]; u.v = new float[u.nv * 3];
                    for (int i = 0; i < u.nv; i++) { u.vbone[i] = in.readByte(); for (int k = 0; k < 3; k++) u.v[i * 3 + k] = in.readShort() / 16f; }
                    u.nn = in.readShort(); u.nbone = new byte[u.nn]; u.n = new float[u.nn * 3];
                    for (int i = 0; i < u.nn; i++) { u.nbone[i] = in.readByte(); for (int k = 0; k < 3; k++) u.n[i * 3 + k] = in.readByte() / 127f; }
                    int nm = in.readShort();
                    u.mtex = new short[nm]; u.tri = new short[nm][];
                    for (int k = 0; k < nm; k++) {
                        u.mtex[k] = in.readShort();
                        int nt = in.readShort();
                        short[] t = new short[nt * 12];
                        for (int i = 0; i < t.length; i++) t[i] = in.readShort();
                        u.tri[k] = t;
                    }
                    m.parts[p][s] = u;
                }
            }
            in.close();
            return m;
        } catch (IOException e) { return null; }
    }

    public int bone(String nm) { for (int i = 0; i < nb; i++) if (bname[i].equals(nm)) return i; return -1; }

    // ---------------------------------------------------------------- matrices
    /** local quaternion+position -> 3x4 matrix into out[o..o+12] */
    public static void qmat(float x, float y, float z, float w, float px, float py, float pz, float[] out, int o) {
        out[o] = 1 - 2 * y * y - 2 * z * z; out[o + 1] = 2 * x * y - 2 * w * z; out[o + 2] = 2 * x * z + 2 * w * y; out[o + 3] = px;
        out[o + 4] = 2 * x * y + 2 * w * z; out[o + 5] = 1 - 2 * x * x - 2 * z * z; out[o + 6] = 2 * y * z - 2 * w * x; out[o + 7] = py;
        out[o + 8] = 2 * x * z - 2 * w * y; out[o + 9] = 2 * y * z + 2 * w * x; out[o + 10] = 1 - 2 * x * x - 2 * y * y; out[o + 11] = pz;
    }

    /** c = a * b (3x4 affine) */
    public static void mul(float[] a, int ao, float[] b, int bo, float[] c, int co) {
        for (int r = 0; r < 3; r++) {
            float a0 = a[ao + r * 4], a1 = a[ao + r * 4 + 1], a2 = a[ao + r * 4 + 2], a3 = a[ao + r * 4 + 3];
            c[co + r * 4] = a0 * b[bo] + a1 * b[bo + 4] + a2 * b[bo + 8];
            c[co + r * 4 + 1] = a0 * b[bo + 1] + a1 * b[bo + 5] + a2 * b[bo + 9];
            c[co + r * 4 + 2] = a0 * b[bo + 2] + a1 * b[bo + 6] + a2 * b[bo + 10];
            c[co + r * 4 + 3] = a0 * b[bo + 3] + a1 * b[bo + 7] + a2 * b[bo + 11] + a3;
        }
    }

    /** entity matrix: yaw about Z then translate */
    public static void entity(float x, float y, float z, float yawDeg, float[] out) {
        float c = (float) Math.cos(Math.toRadians(yawDeg)), s = (float) Math.sin(Math.toRadians(yawDeg));
        out[0] = c; out[1] = -s; out[2] = 0; out[3] = x;
        out[4] = s; out[5] = c; out[6] = 0; out[7] = y;
        out[8] = 0; out[9] = 0; out[10] = 1; out[11] = z;
    }

    private final float[] tmp = new float[12];

    /** world bone matrices from local quaternions/positions (nb*4, nb*3) and an entity matrix */
    public void build(float[] Q, float[] P, float[] ent, float[] out) {
        for (int b = 0; b < nb; b++) {
            qmat(Q[b * 4], Q[b * 4 + 1], Q[b * 4 + 2], Q[b * 4 + 3], P[b * 3], P[b * 3 + 1], P[b * 3 + 2], tmp, 0);
            int pr = parent[b];
            if (pr < 0) mul(ent, 0, tmp, 0, out, b * 12); else mul(out, pr * 12, tmp, 0, out, b * 12);
        }
    }

    /** static pose (default local transforms) */
    public void buildDefault(float[] ent, float[] out) { build(defQ, defP, ent, out); }

    /** bone-merge an attached model (p_ weapon) onto a parent skeleton by bone name */
    public int[] mergeMap(Mdl3 parentModel) {
        int[] map = new int[nb];
        for (int b = 0; b < nb; b++) map[b] = parentModel.bone(bname[b]);
        return map;
    }

    public void buildMerged(int[] map, float[] parentWorld, float[] ent, float[] out) {
        for (int b = 0; b < nb; b++) {
            if (map[b] >= 0) { System.arraycopy(parentWorld, map[b] * 12, out, b * 12, 12); continue; }
            qmat(defQ[b * 4], defQ[b * 4 + 1], defQ[b * 4 + 2], defQ[b * 4 + 3], defP[b * 3], defP[b * 3 + 1], defP[b * 3 + 2], tmp, 0);
            int pr = parent[b];
            if (pr < 0) mul(ent, 0, tmp, 0, out, b * 12); else mul(out, pr * 12, tmp, 0, out, b * 12);
        }
    }
}
