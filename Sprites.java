package cs;

import java.io.*;

/** Baked sprite sets: first-person weapon (VWS1) and player billboards (PLS1). */
public final class Sprites {
    public int[] pal = new int[256];
    public int[] litPal = new int[256];
    private int litLevel = -1;
    public String[] anim; public float[] dur; public int[] nfr;
    // frames: [anim][dir*nfr + f]
    public short[][] fx, fy, fw, fh; public byte[][][] data;
    public int dirs;
    public float ppu; public int ax, ay;

    public static Sprites load(String name, boolean player) throws IOException {
        Bin in = new Bin(Sys.s.open(name));
        Sprites s = new Sprites();
        in.readInt();
        byte[] p = new byte[768]; in.readFully(p);
        for (int i = 0; i < 256; i++) s.pal[i] = ((p[i * 3] & 255) << 16) | ((p[i * 3 + 1] & 255) << 8) | (p[i * 3 + 2] & 255);
        s.dirs = 1;
        if (player) { s.ppu = in.readFloat(); s.ax = in.readShort(); s.ay = in.readShort(); s.dirs = 8; }
        int na = in.readShort();
        s.anim = new String[na]; s.dur = new float[na]; s.nfr = new int[na];
        s.fx = new short[na][]; s.fy = new short[na][]; s.fw = new short[na][]; s.fh = new short[na][]; s.data = new byte[na][][];
        for (int a = 0; a < na; a++) {
            s.anim[a] = in.readUTF(); s.dur[a] = in.readFloat(); int nf = in.readShort(); s.nfr[a] = nf;
            int tot = nf * s.dirs;
            s.fx[a] = new short[tot]; s.fy[a] = new short[tot]; s.fw[a] = new short[tot]; s.fh[a] = new short[tot]; s.data[a] = new byte[tot][];
            for (int k = 0; k < tot; k++) {
                s.fx[a][k] = in.readShort(); s.fy[a][k] = in.readShort(); s.fw[a][k] = in.readShort(); s.fh[a][k] = in.readShort();
                byte[] d = new byte[s.fw[a][k] * s.fh[a][k]]; in.readFully(d); s.data[a][k] = d;
            }
        }
        in.close();
        return s;
    }

    public void invalidate() { litLevel = -1; }

    public int find(String a) { for (int i = 0; i < anim.length; i++) if (anim[i].equals(a)) return i; return -1; }

    /** palette scaled by light level (0..63, 32 = neutral) */
    public int[] lit(int level) {
        if (level == litLevel) return litPal;
        litLevel = level;
        int k = level * 8; // /256 => level/32
        for (int i = 1; i < 256; i++) {
            int c = pal[i];
            int r = (((c >> 16) & 255) * k) >> 8, g = (((c >> 8) & 255) * k) >> 8, b = ((c & 255) * k) >> 8;
            if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
            litPal[i] = (Render.gamma[r] << 16) | (Render.gamma[g] << 8) | Render.gamma[b];
        }
        return litPal;
    }
}
