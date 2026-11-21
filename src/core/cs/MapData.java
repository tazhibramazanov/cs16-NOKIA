package cs;

import java.io.*;

/** Converted GoldSrc BSP30 map (CSE2 format) + hull tracing. */
public final class MapData {
    public static final int CONTENTS_EMPTY = -1, CONTENTS_SOLID = -2, CONTENTS_WATER = -3, CONTENTS_SKY = -6;

    // palette / shading
    public int[] pal = new int[256];
    public float tintR, tintG, tintB;
    // textures
    public int numTex;
    public String[] texName;
    public int[] texFlags, texW, texH;
    public byte[][][] texMip; // [tex][mip]
    // planes
    public float[] plNx, plNy, plNz, plD; public byte[] plType;
    // verts
    public float[] vx, vy, vz;
    public short[] fvert;
    // faces
    public int numFaces;
    public short[] fPlane, fNumV, fTex; public byte[] fSide; public int[] fFirstV;
    public float[] fS, fT, fLS, fLT; // 4 floats each per face
    public int[] fLmOfs; public byte[] fLmW, fLmH;
    // nodes
    public int[] nPlane; public short[] nChild0, nChild1; public short[] nBox; public short[] nFirstFace, nNumFaces;
    public int[] nParent;
    // leaves
    public int numLeaves;
    public int[] lContents, lVisOfs; public short[] lBox, lFirstMark, lNumMarks; public int[] lParent;
    public short[] marks;
    public byte[] vis;
    // clipnodes
    public int[] cPlane; public short[] cChild0, cChild1;
    // hull 0 built from nodes
    public int[] h0Plane; public int[] h0Child0, h0Child1;
    // models
    public int numModels;
    public float[] mMins, mMaxs, mOrigin; public int[] mHead; public int[] mFirstFace, mNumFaces;
    // entities
    public String[][] entKeys, entVals;
    public byte[] light;
    public int[][] sky = new int[5][]; // rt lf bk ft up, 128x128 RGB
    public String skyName;
    private static int palRaw(byte[] p, int c) { return ((p[c * 3] & 255) << 16) | ((p[c * 3 + 1] & 255) << 8) | (p[c * 3 + 2] & 255); }

    public static int u16(short s) { return s & 0xFFFF; }

    public MapData(InputStream in0) throws IOException {
        Bin in = new Bin(in0);
        byte[] magic = new byte[4]; in.readFully(magic);
        byte[] p = new byte[768]; in.readFully(p);
        for (int i = 0; i < 256; i++) pal[i] = ((p[i * 3] & 255) << 16) | ((p[i * 3 + 1] & 255) << 8) | (p[i * 3 + 2] & 255);
        tintR = in.readFloat(); tintG = in.readFloat(); tintB = in.readFloat();
        byte[] lut = new byte[32768]; in.readFully(lut);
        numTex = in.readInt();
        texName = new String[numTex]; texFlags = new int[numTex]; texW = new int[numTex]; texH = new int[numTex];
        texMip = new byte[numTex][][];
        float[] tsx = new float[numTex], tsy = new float[numTex];
        for (int i = 0; i < numTex; i++) {
            texName[i] = in.readUTF(); texFlags[i] = in.readShort(); texW[i] = in.readShort(); texH[i] = in.readShort();
            tsx[i] = in.readFloat(); tsy[i] = in.readFloat();
            byte[] b0 = new byte[in.readInt()]; in.readFully(b0);
            texMip[i] = new byte[3][];
            texMip[i][0] = b0;
            int w = texW[i], h = texH[i];
            boolean masked = (texFlags[i] & 2) != 0;
            for (int mlev = 1; mlev < 3; mlev++) {
                byte[] src = texMip[i][mlev - 1];
                int sw = Math.max(1, w >> (mlev - 1)), sh = Math.max(1, h >> (mlev - 1));
                if (sw < 2 || sh < 2 || (texFlags[i] & 1) != 0) { texMip[i][mlev] = src; continue; }
                int dw = sw >> 1, dh = sh >> 1;
                byte[] dst = new byte[dw * dh];
                for (int y = 0; y < dh; y++) for (int x = 0; x < dw; x++) {
                    int o = (y * 2) * sw + x * 2;
                    int c0 = src[o] & 255, c1 = src[o + 1] & 255, c2 = src[o + sw] & 255, c3 = src[o + sw + 1] & 255;
                    if (masked) {
                        int tr = (c0 == 255 ? 1 : 0) + (c1 == 255 ? 1 : 0) + (c2 == 255 ? 1 : 0) + (c3 == 255 ? 1 : 0);
                        if (tr >= 2) { dst[y * dw + x] = (byte) 255; continue; }
                        if (c0 == 255) c0 = c3 != 255 ? c3 : c1 != 255 ? c1 : c2;
                        if (c1 == 255) c1 = c0; if (c2 == 255) c2 = c0; if (c3 == 255) c3 = c0;
                    }
                    int p0 = palRaw(p, c0), p1 = palRaw(p, c1), p2 = palRaw(p, c2), p3 = palRaw(p, c3);
                    int r = ((p0 >> 16) & 255) + ((p1 >> 16) & 255) + ((p2 >> 16) & 255) + ((p3 >> 16) & 255);
                    int g = ((p0 >> 8) & 255) + ((p1 >> 8) & 255) + ((p2 >> 8) & 255) + ((p3 >> 8) & 255);
                    int bl = (p0 & 255) + (p1 & 255) + (p2 & 255) + (p3 & 255);
                    int ci = lut[((r >> 5) << 10) | ((g >> 5) << 5) | (bl >> 5)] & 255;
                    if (ci == 255) ci = 254;
                    dst[y * dw + x] = (byte) ci;
                }
                texMip[i][mlev] = dst;
            }
        }
        int nti = in.readInt();
        float[] tiv = new float[nti * 8]; short[] tiTex = new short[nti];
        for (int i = 0; i < nti; i++) { for (int k = 0; k < 8; k++) tiv[i * 8 + k] = in.readFloat(); tiTex[i] = in.readShort(); }
        int n = in.readInt();
        plNx = new float[n]; plNy = new float[n]; plNz = new float[n]; plD = new float[n]; plType = new byte[n];
        for (int i = 0; i < n; i++) { plNx[i] = in.readFloat(); plNy[i] = in.readFloat(); plNz[i] = in.readFloat(); plD[i] = in.readFloat(); plType[i] = in.readByte(); }
        n = in.readInt(); vx = new float[n]; vy = new float[n]; vz = new float[n];
        for (int i = 0; i < n; i++) { vx[i] = in.readFloat(); vy[i] = in.readFloat(); vz[i] = in.readFloat(); }
        n = in.readInt(); fvert = new short[n];
        for (int i = 0; i < n; i++) fvert[i] = in.readShort();
        n = numFaces = in.readInt();
        fPlane = new short[n]; fSide = new byte[n]; fFirstV = new int[n]; fNumV = new short[n]; fTex = new short[n];
        fS = new float[n * 4]; fT = new float[n * 4]; fLS = new float[n * 4]; fLT = new float[n * 4];
        fLmOfs = new int[n]; fLmW = new byte[n]; fLmH = new byte[n];
        for (int i = 0; i < n; i++) {
            fPlane[i] = in.readShort(); fSide[i] = in.readByte(); fFirstV[i] = in.readInt(); fNumV[i] = in.readShort(); fTex[i] = in.readShort();
            int ti = in.readShort();
            fLmOfs[i] = in.readInt(); fLmW[i] = in.readByte(); fLmH[i] = in.readByte();
            int bms = in.readShort(), bmt = in.readShort();
            int t = fTex[i];
            float sx = t >= 0 ? tsx[t] : 1, sy = t >= 0 ? tsy[t] : 1;
            for (int k = 0; k < 4; k++) {
                fS[i * 4 + k] = tiv[ti * 8 + k] * sx; fT[i * 4 + k] = tiv[ti * 8 + 4 + k] * sy;
                fLS[i * 4 + k] = tiv[ti * 8 + k] / 16f; fLT[i * 4 + k] = tiv[ti * 8 + 4 + k] / 16f;
            }
            fLS[i * 4 + 3] = (tiv[ti * 8 + 3] - bms * 16) / 16f;
            fLT[i * 4 + 3] = (tiv[ti * 8 + 7] - bmt * 16) / 16f;
        }
        n = in.readInt();
        nPlane = new int[n]; nChild0 = new short[n]; nChild1 = new short[n]; nBox = new short[n * 6]; nFirstFace = new short[n]; nNumFaces = new short[n];
        nParent = new int[n];
        for (int i = 0; i < n; i++) {
            nPlane[i] = in.readInt(); nChild0[i] = in.readShort(); nChild1[i] = in.readShort();
            for (int k = 0; k < 6; k++) nBox[i * 6 + k] = in.readShort();
            nFirstFace[i] = in.readShort(); nNumFaces[i] = in.readShort();
        }
        n = numLeaves = in.readInt();
        lContents = new int[n]; lVisOfs = new int[n]; lBox = new short[n * 6]; lFirstMark = new short[n]; lNumMarks = new short[n]; lParent = new int[n];
        for (int i = 0; i < n; i++) {
            lContents[i] = in.readInt(); lVisOfs[i] = in.readInt();
            for (int k = 0; k < 6; k++) lBox[i * 6 + k] = in.readShort();
            lFirstMark[i] = in.readShort(); lNumMarks[i] = in.readShort();
        }
        n = in.readInt(); marks = new short[n];
        for (int i = 0; i < n; i++) marks[i] = in.readShort();
        n = in.readInt(); vis = new byte[n]; in.readFully(vis);
        n = in.readInt(); cPlane = new int[n]; cChild0 = new short[n]; cChild1 = new short[n];
        for (int i = 0; i < n; i++) { cPlane[i] = in.readInt(); cChild0[i] = in.readShort(); cChild1[i] = in.readShort(); }
        n = numModels = in.readInt();
        mMins = new float[n * 3]; mMaxs = new float[n * 3]; mOrigin = new float[n * 3]; mHead = new int[n * 4]; mFirstFace = new int[n]; mNumFaces = new int[n];
        for (int i = 0; i < n; i++) {
            for (int k = 0; k < 3; k++) mMins[i * 3 + k] = in.readFloat();
            for (int k = 0; k < 3; k++) mMaxs[i * 3 + k] = in.readFloat();
            for (int k = 0; k < 3; k++) mOrigin[i * 3 + k] = in.readFloat();
            for (int k = 0; k < 4; k++) mHead[i * 4 + k] = in.readInt();
            in.readInt(); mFirstFace[i] = in.readInt(); mNumFaces[i] = in.readInt();
        }
        n = in.readInt(); entKeys = new String[n][]; entVals = new String[n][];
        for (int i = 0; i < n; i++) {
            int k = in.readShort(); entKeys[i] = new String[k]; entVals[i] = new String[k];
            for (int j = 0; j < k; j++) { entKeys[i][j] = in.readUTF(); entVals[i][j] = in.readUTF(); }
        }
        n = in.readInt(); light = new byte[n]; in.readFully(light);
        skyName = in.readUTF();
        int[] wh = new int[2];
        int[] strip = Sys.s.image("/sky_" + skyName + ".png", wh);
        for (int f = 0; f < 5; f++) {
            int[] sk = new int[128 * 128];
            if (strip != null && wh[0] == 128 && wh[1] >= 640) System.arraycopy(strip, f * 128 * 128, sk, 0, 128 * 128);
            else for (int i2 = 0; i2 < sk.length; i2++) sk[i2] = 0xFF7896C8;
            sky[f] = sk;
        }
        in.close();
        // parents
        for (int i = 0; i < nPlane.length; i++) { setParent(nChild0[i], i); setParent(nChild1[i], i); }
        nParent[0] = -1;
        // hull 0
        int nn = nPlane.length;
        h0Plane = nPlane; h0Child0 = new int[nn]; h0Child1 = new int[nn];
        for (int i = 0; i < nn; i++) {
            int c = nChild0[i]; h0Child0[i] = c >= 0 ? c : lContents[-c - 1];
            c = nChild1[i]; h0Child1[i] = c >= 0 ? c : lContents[-c - 1];
        }
    }

    private void setParent(int c, int parent) {
        if (c >= 0) nParent[c] = parent; else lParent[-c - 1] = parent;
    }

    public String ent(int e, String key) {
        String[] k = entKeys[e];
        for (int i = 0; i < k.length; i++) if (k[i].equals(key)) return entVals[e][i];
        return null;
    }

    public int numEnts() { return entKeys.length; }

    /** Leaf index containing point (world BSP). */
    public int pointLeaf(float x, float y, float z) {
        int n = 0;
        while (n >= 0) {
            int p = nPlane[n];
            float d = plNx[p] * x + plNy[p] * y + plNz[p] * z - plD[p];
            n = d >= 0 ? nChild0[n] : nChild1[n];
        }
        return -n - 1;
    }

    // ---------------- hull tracing ----------------
    public static final float DIST_EPSILON = 0.03125f;

    /** hull: 0 point, 1 stand, 2 large, 3 duck */
    public int hullPointContents(int hull, int num, float x, float y, float z) {
        if (hull == 0) {
            while (num >= 0) {
                int p = h0Plane[num];
                float d = plType[p] < 3 ? (plType[p] == 0 ? x : plType[p] == 1 ? y : z) - plD[p] : plNx[p] * x + plNy[p] * y + plNz[p] * z - plD[p];
                num = d < 0 ? h0Child1[num] : h0Child0[num];
            }
        } else {
            while (num >= 0) {
                int p = cPlane[num];
                float d = plType[p] < 3 ? (plType[p] == 0 ? x : plType[p] == 1 ? y : z) - plD[p] : plNx[p] * x + plNy[p] * y + plNz[p] * z - plD[p];
                num = d < 0 ? cChild1[num] : cChild0[num];
            }
        }
        return num;
    }

    // trace state (single-threaded)
    public boolean tAllSolid, tStartSolid, tInOpen, tInWater;
    public float tFrac, tEx, tEy, tEz, tNx, tNy, tNz, tPd;
    private int tHull, tHead;

    private boolean rhc(int num, float p1f, float p2f, float p1x, float p1y, float p1z, float p2x, float p2y, float p2z) {
        if (num < 0) {
            if (num != CONTENTS_SOLID) {
                tAllSolid = false;
                if (num == CONTENTS_EMPTY) tInOpen = true; else tInWater = true;
            } else tStartSolid = true;
            return true;
        }
        int p, c0, c1;
        if (tHull == 0) { p = h0Plane[num]; c0 = h0Child0[num]; c1 = h0Child1[num]; }
        else { p = cPlane[num]; c0 = cChild0[num]; c1 = cChild1[num]; }
        float t1, t2;
        int ty = plType[p];
        if (ty < 3) {
            if (ty == 0) { t1 = p1x - plD[p]; t2 = p2x - plD[p]; }
            else if (ty == 1) { t1 = p1y - plD[p]; t2 = p2y - plD[p]; }
            else { t1 = p1z - plD[p]; t2 = p2z - plD[p]; }
        } else {
            t1 = plNx[p] * p1x + plNy[p] * p1y + plNz[p] * p1z - plD[p];
            t2 = plNx[p] * p2x + plNy[p] * p2y + plNz[p] * p2z - plD[p];
        }
        if (t1 >= 0 && t2 >= 0) return rhc(c0, p1f, p2f, p1x, p1y, p1z, p2x, p2y, p2z);
        if (t1 < 0 && t2 < 0) return rhc(c1, p1f, p2f, p1x, p1y, p1z, p2x, p2y, p2z);
        float frac = t1 < 0 ? (t1 + DIST_EPSILON) / (t1 - t2) : (t1 - DIST_EPSILON) / (t1 - t2);
        if (frac < 0) frac = 0; if (frac > 1) frac = 1;
        float midf = p1f + (p2f - p1f) * frac;
        float mx = p1x + frac * (p2x - p1x), my = p1y + frac * (p2y - p1y), mz = p1z + frac * (p2z - p1z);
        int side = t1 < 0 ? 1 : 0;
        int cs = side == 0 ? c0 : c1, co = side == 0 ? c1 : c0;
        if (!rhc(cs, p1f, midf, p1x, p1y, p1z, mx, my, mz)) return false;
        if (hullPointContents(tHull, co, mx, my, mz) != CONTENTS_SOLID)
            return rhc(co, midf, p2f, mx, my, mz, p2x, p2y, p2z);
        if (tAllSolid) return false;
        if (side == 0) { tNx = plNx[p]; tNy = plNy[p]; tNz = plNz[p]; tPd = plD[p]; }
        else { tNx = -plNx[p]; tNy = -plNy[p]; tNz = -plNz[p]; tPd = -plD[p]; }
        while (hullPointContents(tHull, tHead, mx, my, mz) == CONTENTS_SOLID) {
            frac -= 0.1f;
            if (frac < 0) { tFrac = midf; tEx = mx; tEy = my; tEz = mz; return false; }
            midf = p1f + (p2f - p1f) * frac;
            mx = p1x + frac * (p2x - p1x); my = p1y + frac * (p2y - p1y); mz = p1z + frac * (p2z - p1z);
        }
        tFrac = midf; tEx = mx; tEy = my; tEz = mz;
        return false;
    }

    /** Trace through one model's hull (model 0 = world). Results in t* fields. */
    public void traceModel(int model, int hull, float x1, float y1, float z1, float x2, float y2, float z2) {
        tAllSolid = true; tStartSolid = false; tInOpen = false; tInWater = false;
        tFrac = 1; tEx = x2; tEy = y2; tEz = z2; tNx = tNy = tNz = 0; tPd = 0;
        tHull = hull; tHead = mHead[model * 4 + hull];
        rhc(tHead, 0, 1, x1, y1, z1, x2, y2, z2);
        if (tAllSolid) tStartSolid = true;
        if (tStartSolid) tFrac = 0;
        if (tFrac == 1) { tEx = x2; tEy = y2; tEz = z2; }
    }

    // solid brush entities for collision
    public int[] solidModels = new int[0];
    public boolean[] modelActive;

    public float rFrac, rEx, rEy, rEz, rNx, rNy, rNz; public boolean rStartSolid, rAllSolid; public int rModel;

    /** Trace world + solid brush entities; result in r* fields. */
    public void trace(int hull, float x1, float y1, float z1, float x2, float y2, float z2) {
        traceModel(0, hull, x1, y1, z1, x2, y2, z2);
        rFrac = tFrac; rEx = tEx; rEy = tEy; rEz = tEz; rNx = tNx; rNy = tNy; rNz = tNz; rStartSolid = tStartSolid; rAllSolid = tAllSolid; rModel = 0;
        for (int i = 0; i < solidModels.length; i++) {
            int m = solidModels[i];
            if (!modelActive[m]) continue;
            // quick bbox reject (expanded by hull size)
            float e = hull == 0 ? 0 : 40;
            float mnx = Math.min(x1, x2) - e, mxx = Math.max(x1, x2) + e;
            float mny = Math.min(y1, y2) - e, mxy = Math.max(y1, y2) + e;
            float mnz = Math.min(z1, z2) - e, mxz = Math.max(z1, z2) + e;
            if (mxx < mMins[m * 3] || mnx > mMaxs[m * 3] || mxy < mMins[m * 3 + 1] || mny > mMaxs[m * 3 + 1] || mxz < mMins[m * 3 + 2] || mnz > mMaxs[m * 3 + 2]) continue;
            traceModel(m, hull, x1, y1, z1, x2, y2, z2);
            if (tStartSolid) rStartSolid = true;
            if (tFrac < rFrac) {
                rFrac = tFrac; rEx = tEx; rEy = tEy; rEz = tEz; rNx = tNx; rNy = tNy; rNz = tNz; rAllSolid = tAllSolid; rModel = m;
            }
        }
    }

    /** texture of the surface hit by a segment (world + given submodel) or -1 */
    public int surfaceTex(int model, float x1, float y1, float z1, float x2, float y2, float z2) {
        if (model > 0) {
            int ff = mFirstFace[model], nf = mNumFaces[model];
            float best = 1e9f; int bt = -1;
            for (int i = ff; i < ff + nf; i++) {
                int p = fPlane[i] & 0xFFFF;
                float d1 = plNx[p] * x1 + plNy[p] * y1 + plNz[p] * z1 - plD[p], d2 = plNx[p] * x2 + plNy[p] * y2 + plNz[p] * z2 - plD[p];
                if ((d1 >= 0) == (d2 >= 0)) continue;
                float f = d1 / (d1 - d2);
                if (f < best && inFace(i, x1 + (x2 - x1) * f, y1 + (y2 - y1) * f, z1 + (z2 - z1) * f)) { best = f; bt = fTex[i]; }
            }
            return bt;
        }
        sfTex = -1;
        surfRec(0, x1, y1, z1, x2, y2, z2);
        return sfTex;
    }
    private int sfTex;
    private boolean surfRec(int node, float x1, float y1, float z1, float x2, float y2, float z2) {
        if (node < 0) return false;
        int p = nPlane[node];
        float d1 = plNx[p] * x1 + plNy[p] * y1 + plNz[p] * z1 - plD[p];
        float d2 = plNx[p] * x2 + plNy[p] * y2 + plNz[p] * z2 - plD[p];
        int side = d1 < 0 ? 1 : 0;
        int cf = side == 0 ? nChild0[node] : nChild1[node], cb = side == 0 ? nChild1[node] : nChild0[node];
        if ((d2 < 0 ? 1 : 0) == side) return surfRec(cf, x1, y1, z1, x2, y2, z2);
        float f = d1 / (d1 - d2);
        float mx = x1 + (x2 - x1) * f, my = y1 + (y2 - y1) * f, mz = z1 + (z2 - z1) * f;
        if (surfRec(cf, x1, y1, z1, mx, my, mz)) return true;
        int ff = nFirstFace[node] & 0xFFFF, nf = nNumFaces[node] & 0xFFFF;
        for (int i = ff; i < ff + nf; i++) if (fTex[i] >= 0 && inFace(i, mx, my, mz)) { sfTex = fTex[i]; return true; }
        return surfRec(cb, mx, my, mz, x2, y2, z2);
    }

    /** point (on the face plane) inside the convex face polygon */
    public boolean inFace(int f, float x, float y, float z) {
        int p = fPlane[f] & 0xFFFF;
        float nx = plNx[p], ny = plNy[p], nz = plNz[p];
        int nv = fNumV[f], fv = fFirstV[f];
        float sgn = 0;
        for (int i = 0; i < nv; i++) {
            int a = fvert[fv + i] & 0xFFFF, b = fvert[fv + (i + 1) % nv] & 0xFFFF;
            float ex = vx[b] - vx[a], ey = vy[b] - vy[a], ez = vz[b] - vz[a];
            float px = x - vx[a], py = y - vy[a], pz = z - vz[a];
            float cx = ey * pz - ez * py, cy = ez * px - ex * pz, cz = ex * py - ey * px;
            float d = cx * nx + cy * ny + cz * nz;
            if (d > 0.5f) { if (sgn < 0) return false; sgn = 1; }
            else if (d < -0.5f) { if (sgn > 0) return false; sgn = -1; }
        }
        return true;
    }

    /** Light level (0..63) at a point: trace down to find the floor face and sample its lightmap. */
    public int lightAt(float x, float y, float z) {
        lpBest = -1;
        lightRec(0, x, y, z, x, y, z - 2048);
        return lpBest < 0 ? 32 : lpBest;
    }
    private int lpBest;
    private boolean lightRec(int node, float x1, float y1, float z1, float x2, float y2, float z2) {
        if (node < 0) return false;
        int p = nPlane[node];
        float d1 = plNx[p] * x1 + plNy[p] * y1 + plNz[p] * z1 - plD[p];
        float d2 = plNx[p] * x2 + plNy[p] * y2 + plNz[p] * z2 - plD[p];
        int side = d1 < 0 ? 1 : 0;
        int cf = side == 0 ? nChild0[node] : nChild1[node], cb = side == 0 ? nChild1[node] : nChild0[node];
        if ((d2 < 0 ? 1 : 0) == side) return lightRec(cf, x1, y1, z1, x2, y2, z2);
        float f = d1 / (d1 - d2);
        float mx = x1 + (x2 - x1) * f, my = y1 + (y2 - y1) * f, mz = z1 + (z2 - z1) * f;
        if (lightRec(cf, x1, y1, z1, mx, my, mz)) return true;
        // check faces on this node
        int ff = nFirstFace[node] & 0xFFFF, nf = nNumFaces[node] & 0xFFFF;
        for (int i = ff; i < ff + nf; i++) {
            int lo = fLmOfs[i]; if (lo < 0) continue;
            float ls = fLS[i * 4] * mx + fLS[i * 4 + 1] * my + fLS[i * 4 + 2] * mz + fLS[i * 4 + 3];
            float lt = fLT[i * 4] * mx + fLT[i * 4 + 1] * my + fLT[i * 4 + 2] * mz + fLT[i * 4 + 3];
            int w = fLmW[i] & 255, h = fLmH[i] & 255;
            if (ls < -0.01f || lt < -0.01f || ls > w - 0.99f || lt > h - 0.99f) continue;
            int si = (int) (ls + 0.5f), ti = (int) (lt + 0.5f);
            if (si >= w) si = w - 1; if (ti >= h) ti = h - 1; if (si < 0) si = 0; if (ti < 0) ti = 0;
            lpBest = light[lo + ti * w + si] & 255;
            return true;
        }
        return lightRec(cb, mx, my, mz, x2, y2, z2);
    }
}
