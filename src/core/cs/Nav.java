package cs;

import java.io.*;

/** CS bot navigation mesh (from the original .nav files) + A* */
public final class Nav {
    public int n;
    public float[] x0, y0, x1, y1, zNW, zNE, zSE, zSW, cx, cy, cz;
    public int[][] conn; public byte[][] cdir;
    public float[][] spots;
    public int[] attr;

    public Nav(InputStream in0) throws IOException {
        Bin in = new Bin(in0);
        in.readInt();
        int np = in.readShort();
        for (int i = 0; i < np; i++) in.readUTF();
        n = in.readShort() & 0xFFFF;
        x0 = new float[n]; y0 = new float[n]; x1 = new float[n]; y1 = new float[n];
        zNW = new float[n]; zNE = new float[n]; zSE = new float[n]; zSW = new float[n];
        cx = new float[n]; cy = new float[n]; cz = new float[n];
        conn = new int[n][]; cdir = new byte[n][]; spots = new float[n][]; attr = new int[n];
        for (int i = 0; i < n; i++) {
            x0[i] = in.readFloat(); y0[i] = in.readFloat(); x1[i] = in.readFloat(); y1[i] = in.readFloat();
            zNW[i] = in.readFloat(); zNE[i] = in.readFloat(); zSE[i] = in.readFloat(); zSW[i] = in.readFloat();
            attr[i] = in.readUnsignedByte(); in.readShort();
            int c = in.readUnsignedByte();
            conn[i] = new int[c]; cdir[i] = new byte[c];
            for (int k = 0; k < c; k++) { cdir[i][k] = in.readByte(); conn[i][k] = in.readShort() & 0xFFFF; }
            int s = in.readUnsignedByte();
            spots[i] = new float[s * 3];
            for (int k = 0; k < s * 3; k++) spots[i][k] = in.readFloat();
            cx[i] = (x0[i] + x1[i]) * 0.5f; cy[i] = (y0[i] + y1[i]) * 0.5f;
            cz[i] = (zNW[i] + zNE[i] + zSE[i] + zSW[i]) * 0.25f;
        }
        in.close();
        g = new float[n]; f = new float[n]; from = new int[n]; stamp = new int[n]; closed = new int[n];
        heap = new int[n * 4];
    }

    public float zAt(int a, float x, float y) {
        float u = (x1[a] - x0[a]) > 0 ? (x - x0[a]) / (x1[a] - x0[a]) : 0, v = (y1[a] - y0[a]) > 0 ? (y - y0[a]) / (y1[a] - y0[a]) : 0;
        if (u < 0) u = 0; if (u > 1) u = 1; if (v < 0) v = 0; if (v > 1) v = 1;
        float north = zNW[a] + (zNE[a] - zNW[a]) * u, south = zSW[a] + (zSE[a] - zSW[a]) * u;
        return north + (south - north) * v;
    }

    /** area containing (x,y) closest in z (feet height) */
    public int find(float x, float y, float z) {
        int best = -1; float bd = 1e9f;
        for (int i = 0; i < n; i++) {
            if (x >= x0[i] - 1 && x <= x1[i] + 1 && y >= y0[i] - 1 && y <= y1[i] + 1) {
                float d = Math.abs(zAt(i, x, y) - z);
                if (d < bd && d < 80) { bd = d; best = i; }
            }
        }
        if (best >= 0) return best;
        for (int i = 0; i < n; i++) {
            float dx = Math.max(Math.max(x0[i] - x, x - x1[i]), 0), dy = Math.max(Math.max(y0[i] - y, y - y1[i]), 0);
            float d = dx * dx + dy * dy + (cz[i] - z) * (cz[i] - z) * 4;
            if (d < bd) { bd = d; best = i; }
        }
        return best;
    }

    /** portal point between a and b (on shared edge, clamped toward (tx,ty)) */
    public float px, py, pz;
    public void portal(int a, int b, float tx, float ty) {
        float lo, hi;
        int d = -1;
        for (int k = 0; k < conn[a].length; k++) if (conn[a][k] == b) { d = cdir[a][k]; break; }
        if (d == 0 || d == 2) { // north/south: shared edge along x
            lo = Math.max(x0[a], x0[b]); hi = Math.min(x1[a], x1[b]);
            if (lo > hi) { float m2 = (lo + hi) * 0.5f; lo = hi = m2; }
            float m = tx; float pad = Math.min(16, (hi - lo) * 0.5f);
            if (m < lo + pad) m = lo + pad; if (m > hi - pad) m = hi - pad;
            px = m; py = d == 0 ? y0[a] : y1[a];
        } else if (d == 1 || d == 3) {
            lo = Math.max(y0[a], y0[b]); hi = Math.min(y1[a], y1[b]);
            if (lo > hi) { float m2 = (lo + hi) * 0.5f; lo = hi = m2; }
            float m = ty; float pad = Math.min(16, (hi - lo) * 0.5f);
            if (m < lo + pad) m = lo + pad; if (m > hi - pad) m = hi - pad;
            py = m; px = d == 1 ? x1[a] : x0[a];
        } else { px = cx[b]; py = cy[b]; }
        pz = zAt(b, px, py);
    }

    // A*
    private float[] g, f; private int[] from, stamp, closed; private int st = 0;
    private int[] heap; private int hn;

    private void push(int a) {
        int i = hn++;
        if (hn > heap.length) { int[] h2 = new int[heap.length * 2]; System.arraycopy(heap, 0, h2, 0, heap.length); heap = h2; }
        heap[i] = a;
        while (i > 0) { int pa = (i - 1) >> 1; if (f[heap[pa]] <= f[heap[i]]) break; int t = heap[pa]; heap[pa] = heap[i]; heap[i] = t; i = pa; }
    }

    private int pop() {
        int r = heap[0]; heap[0] = heap[--hn]; int i = 0;
        while (true) {
            int l = i * 2 + 1, rr = l + 1, s = i;
            if (l < hn && f[heap[l]] < f[heap[s]]) s = l;
            if (rr < hn && f[heap[rr]] < f[heap[s]]) s = rr;
            if (s == i) break;
            int t = heap[s]; heap[s] = heap[i]; heap[i] = t; i = s;
        }
        return r;
    }

    /** returns path of area indices from start to goal (inclusive) or null. avoid: optional cost bias array */
    public int[] path(int s, int goal, float[] bias) {
        if (s < 0 || goal < 0) return null;
        st++; hn = 0;
        g[s] = 0; f[s] = dist(s, goal); from[s] = -1; stamp[s] = st; push(s);
        int iter = 0;
        while (hn > 0 && iter++ < 4000) {
            int a = pop();
            if (closed[a] == st) continue;
            closed[a] = st;
            if (a == goal) {
                int len = 0; for (int k = a; k >= 0; k = from[k]) len++;
                int[] p = new int[len];
                for (int k = a; k >= 0; k = from[k]) p[--len] = k;
                return p;
            }
            int[] c = conn[a];
            for (int k = 0; k < c.length; k++) {
                int b = c[k];
                if (closed[b] == st) continue;
                float climb = cz[b] - cz[a];
                if (climb > 60) continue; // too high to climb (jump-up links are rare)
                float cost = g[a] + dist(a, b) + (climb > 20 ? 60 : 0) + (bias != null ? bias[b] : 0);
                if (stamp[b] != st || cost < g[b]) {
                    stamp[b] = st; g[b] = cost; from[b] = a; f[b] = cost + dist(b, goal); push(b);
                }
            }
        }
        return null;
    }

    private float dist(int a, int b) {
        float dx = cx[a] - cx[b], dy = cy[a] - cy[b], dz = cz[a] - cz[b];
        return (float) Math.sqrt(dx * dx + dy * dy + dz * dz);
    }
}
