package cs;

/**
 * Software renderer: BSP front-to-back + per-row span coverage buffer (zero overdraw),
 * perspective-correct texturing every 16 px, bilinear lightmaps, skybox, z-buffer for
 * brush entities / sprites.
 */
public final class Render {
    public final int W, H;
    public final int[] fb, zb;
    public MapData m;

    // camera
    public float ex, ey, ez, yaw, pitch;
    public float fx, fy, fz, rx, ry, rz, ux, uy, uz;
    public float xscale, yscale, cx, cy, fov = 90;

    public final int[] shade = new int[64 * 256];
    public static final int[] gamma = new int[256];
    static { for (int i = 0; i < 256; i++) gamma[i] = i; }
    private int[][] skyG = new int[5][];
    private int[] texShift, texMaskS, texMaskT, texTsh;

    // span coverage buffer
    private static final int MAXSEG = 64;
    private final short[] segA, segB;
    private final byte[] segN;
    private int covered;

    // per-frame bookkeeping
    private int frame = 1, visFrame = 0, viewLeaf = -2;
    private int[] leafVis, nodeVis, faceFrame, vertFrame;
    private float[] tvx, tvy, tvz;
    // clip buffers
    private final float[] cax = new float[64], cay = new float[64], caz = new float[64];
    private final float[] cbx = new float[64], cby = new float[64], cbz = new float[64];
    private final float[] psx = new float[64], psy = new float[64];
    private final float[] rowMin, rowMax;
    // sky spans
    private int nSky; private short[] skyY = new short[2048], skyX0 = new short[2048], skyX1 = new short[2048];
    // alpha faces queued from world
    private int nAlpha; private int[] alphaFaces = new int[512];
    public int statFaces, statSpans, statPix;
    public boolean fullbright;
    public int mipBias = 0;
    /** quality knobs (CS 1.6-style presets): perspective/light step = 1<<stepShift px, bilinear texture filtering, flat sky, mip sharpness */
    public int stepShift = 4;
    public boolean bilinear, flatSky;
    public float mipScale = 1f;
    private int skyFlat;

    public Render(int w, int h, int[] fbuf) {
        W = w; H = h;
        fb = fbuf != null ? fbuf : new int[w * h]; zb = new int[w * h];
        segA = new short[h * MAXSEG]; segB = new short[h * MAXSEG]; segN = new byte[h];
        rowMin = new float[h]; rowMax = new float[h]; rowFrame = new int[h];
    }

    public void setMap(MapData md) {
        m = md;
        leafVis = new int[m.numLeaves];
        nodeVis = new int[m.nPlane.length];
        faceFrame = new int[m.numFaces];
        vertFrame = new int[m.vx.length];
        tvx = new float[m.vx.length]; tvy = new float[m.vx.length]; tvz = new float[m.vx.length];
        viewLeaf = -2; visFrame = 0; frame = 1;
        int nt = m.numTex;
        texShift = new int[nt * 3]; texMaskS = new int[nt * 3]; texMaskT = new int[nt * 3]; texTsh = new int[nt * 3];
        for (int t = 0; t < nt; t++) for (int mip = 0; mip < 3; mip++) {
            int w = Math.max(1, m.texW[t] >> mip), h = Math.max(1, m.texH[t] >> mip);
            if (mip >= m.texMip[t].length) { w = m.texW[t]; h = m.texH[t]; }
            int sh = 0; while ((1 << sh) < w) sh++;
            texShift[t * 3 + mip] = sh; texMaskS[t * 3 + mip] = w - 1; texMaskT[t * 3 + mip] = (h - 1) << sh; texTsh[t * 3 + mip] = 16 - sh;
        }
        buildSky();
        for (int l = 0; l < 64; l++) for (int c = 0; c < 256; c++) {
            int p = m.pal[c];
            float k = l / 32f;
            int r = (int) (((p >> 16) & 255) * k * (0.25f + 0.75f * m.tintR));
            int g = (int) (((p >> 8) & 255) * k * (0.25f + 0.75f * m.tintG));
            int b = (int) ((p & 255) * k * (0.25f + 0.75f * m.tintB));
            if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
            shade[l * 256 + c] = (gamma[r] << 16) | (gamma[g] << 8) | gamma[b];
        }
    }

    private void buildSky() {
        for (int f = 0; f < 5; f++) {
            int[] src = m.sky[f]; int[] d = new int[src.length];
            for (int i = 0; i < src.length; i++) { int c = src[i]; d[i] = (gamma[(c >> 16) & 255] << 16) | (gamma[(c >> 8) & 255] << 8) | gamma[c & 255]; }
            skyG[f] = d;
        }
        long r = 0, g = 0, b = 0; int n = 0;
        for (int f = 0; f < 4; f++) { int[] d = skyG[f]; for (int i = 32 * 128; i < 96 * 128; i += 7) { int c = d[i]; r += (c >> 16) & 255; g += (c >> 8) & 255; b += c & 255; n++; } }
        skyFlat = n == 0 ? 0x8090A0 : (int) (r / n) << 16 | (int) (g / n) << 8 | (int) (b / n);
    }

    public void setCamera(float x, float y, float z, float yawDeg, float pitchDeg, float fovDeg) {
        ex = x; ey = y; ez = z; yaw = yawDeg; pitch = pitchDeg; fov = fovDeg;
        float cyw = (float) Math.cos(Math.toRadians(yawDeg)), syw = (float) Math.sin(Math.toRadians(yawDeg));
        float cp = (float) Math.cos(Math.toRadians(pitchDeg)), sp = (float) Math.sin(Math.toRadians(pitchDeg));
        fx = cp * cyw; fy = cp * syw; fz = -sp;
        rx = syw; ry = -cyw; rz = 0;
        ux = sp * cyw; uy = sp * syw; uz = cp;
        cx = W * 0.5f; cy = H * 0.5f;
        xscale = cx / (float) Math.tan(Math.toRadians(fovDeg * 0.5f));
        yscale = xscale;
    }

    // ---------------------------------------------------------------- world
    public void renderWorld() {
        frame++;
        covered = 0; statFaces = statSpans = statPix = 0; nSky = 0; nAlpha = 0;
        for (int i = 0; i < H; i++) segN[i] = 0;
        int leaf = m.pointLeaf(ex, ey, ez);
        if (leaf != viewLeaf) { viewLeaf = leaf; markLeaves(leaf); }
        walk(0, 15);
        if (covered < W * H) fillUncovered();
        if (nSky > 0) drawSky();
    }

    private void markLeaves(int leaf) {
        visFrame++;
        int ofs = m.lVisOfs[leaf];
        int nl = m.numLeaves;
        if (ofs < 0 || m.vis.length == 0 || leaf == 0) {
            for (int i = 0; i < m.numLeaves; i++) markLeaf(i);
            return;
        }
        byte[] v = m.vis;
        int l = 1;
        while (l < nl) {
            int b = v[ofs++] & 255;
            if (b == 0) { l += 8 * (v[ofs++] & 255); continue; }
            for (int bit = 0; bit < 8 && l < nl; bit++, l++) if ((b & (1 << bit)) != 0) markLeaf(l);
        }
        markLeaf(leaf);
    }

    private void markLeaf(int l) {
        if (leafVis[l] == visFrame) return;
        leafVis[l] = visFrame;
        int n = m.lParent[l];
        while (n >= 0 && nodeVis[n] != visFrame) { nodeVis[n] = visFrame; n = m.nParent[n]; }
    }

    /** clip bits: 1 left 2 right 4 top 8 bottom */
    private int cullBox(short[] box, int o, int clip) {
        // planes as view-space conditions; test the corner most inside
        for (int i = 0; i < 4; i++) {
            if ((clip & (1 << i)) == 0) continue;
            float nx, ny, nz;
            if (i == 0) { nx = rx * xscale + fx * cx; ny = ry * xscale + fy * cx; nz = rz * xscale + fz * cx; }
            else if (i == 1) { nx = -rx * xscale + fx * (W - cx); ny = -ry * xscale + fy * (W - cx); nz = -rz * xscale + fz * (W - cx); }
            else if (i == 2) { nx = -ux * yscale + fx * cy; ny = -uy * yscale + fy * cy; nz = -uz * yscale + fz * cy; }
            else { nx = ux * yscale + fx * (H - cy); ny = uy * yscale + fy * (H - cy); nz = uz * yscale + fz * (H - cy); }
            float pxv = nx >= 0 ? box[o + 3] : box[o], pyv = ny >= 0 ? box[o + 4] : box[o + 1], pzv = nz >= 0 ? box[o + 5] : box[o + 2];
            float d = nx * (pxv - ex) + ny * (pyv - ey) + nz * (pzv - ez);
            if (d < 0) return -1;
            float qx = nx >= 0 ? box[o] : box[o + 3], qy = ny >= 0 ? box[o + 1] : box[o + 4], qz = nz >= 0 ? box[o + 2] : box[o + 5];
            if (nx * (qx - ex) + ny * (qy - ey) + nz * (qz - ez) >= 0) clip &= ~(1 << i);
        }
        return clip;
    }

    private void walk(int node, int clip) {
        if (covered >= W * H) return;
        if (node < 0) {
            int l = -node - 1;
            if (leafVis[l] != visFrame) return;
            if (clip != 0 && cullBox(m.lBox, l * 6, clip) < 0) return;
            int f0 = m.lFirstMark[l] & 0xFFFF, nf = m.lNumMarks[l] & 0xFFFF;
            for (int i = 0; i < nf; i++) faceFrame[m.marks[f0 + i] & 0xFFFF] = frame;
            return;
        }
        if (nodeVis[node] != visFrame) return;
        if (clip != 0) { clip = cullBox(m.nBox, node * 6, clip); if (clip < 0) return; }
        int p = m.nPlane[node];
        float d = m.plNx[p] * ex + m.plNy[p] * ey + m.plNz[p] * ez - m.plD[p];
        int front = d >= 0 ? m.nChild0[node] : m.nChild1[node];
        int back = d >= 0 ? m.nChild1[node] : m.nChild0[node];
        walk(front, clip);
        int ff = m.nFirstFace[node] & 0xFFFF, nf = m.nNumFaces[node] & 0xFFFF;
        for (int i = ff; i < ff + nf; i++) {
            if (faceFrame[i] != frame) continue;
            faceFrame[i] = 0;
            boolean faceFront = m.fSide[i] == 0;
            if ((d >= 0) != faceFront) continue; // backface
            int t = m.fTex[i];
            if (t < 0) continue;
            int fl = m.texFlags[t];
            if ((fl & 6) != 0) { if (nAlpha < alphaFaces.length) alphaFaces[nAlpha++] = i; continue; }
            drawFace(i, 0, 0, 0, (fl & 1) != 0 ? 1 : 0);
            if (covered >= W * H) return;
        }
        walk(back, clip);
    }

    /** draw queued alpha (fence/water) faces from the world */
    public void renderWorldAlpha() {
        for (int i = 0; i < nAlpha; i++) drawFace(alphaFaces[i], 0, 0, 0, 2);
    }

    /** Draw a brush entity (submodel) with z-test. */
    public void renderBrushModel(int model, float ox, float oy, float oz, boolean masked) {
        int ff = m.mFirstFace[model], nf = m.mNumFaces[model];
        // bbox frustum check
        float mnx = m.mMins[model * 3] + ox, mny = m.mMins[model * 3 + 1] + oy, mnz = m.mMins[model * 3 + 2] + oz;
        float mxx = m.mMaxs[model * 3] + ox, mxy = m.mMaxs[model * 3 + 1] + oy, mxz = m.mMaxs[model * 3 + 2] + oz;
        float ccx = (mnx + mxx) * 0.5f - ex, ccy = (mny + mxy) * 0.5f - ey, ccz = (mnz + mxz) * 0.5f - ez;
        float rad = (mxx - mnx + mxy - mny + mxz - mnz) * 0.5f;
        float vz = ccx * fx + ccy * fy + ccz * fz;
        if (vz < -rad) return;
        float vx = ccx * rx + ccy * ry + ccz * rz;
        if (Math.abs(vx) - rad > (vz + rad) * cx / xscale + 1) return;
        for (int i = ff; i < ff + nf; i++) {
            int t = m.fTex[i]; if (t < 0) continue;
            int p = m.fPlane[i] & 0xFFFF;
            float d = m.plNx[p] * (ex - ox) + m.plNy[p] * (ey - oy) + m.plNz[p] * (ez - oz) - m.plD[p];
            if ((d >= 0) != (m.fSide[i] == 0)) continue;
            drawFace(i, ox, oy, oz, 2);
        }
    }

    // ---------------------------------------------------------------- face rasterization
    private static final float NEAR = 1f;

    /** mode 0 = world (s-buffer), 1 = sky (s-buffer, record), 2 = z-tested (entities/alpha) */
    private void drawFace(int f, float ox, float oy, float oz, int mode) {
        int nv = m.fNumV[f], fv0 = m.fFirstV[f];
        if (nv > 60) return;
        // transform
        float zmin = 1e9f;
        int n = 0;
        for (int i = 0; i < nv; i++) {
            int v = m.fvert[fv0 + i] & 0xFFFF;
            float x, y, z;
            if (ox == 0 && oy == 0 && oz == 0) {
                if (vertFrame[v] != frame) {
                    float dx = m.vx[v] - ex, dy = m.vy[v] - ey, dz = m.vz[v] - ez;
                    tvx[v] = dx * rx + dy * ry + dz * rz; tvy[v] = dx * ux + dy * uy + dz * uz; tvz[v] = dx * fx + dy * fy + dz * fz;
                    vertFrame[v] = frame;
                }
                x = tvx[v]; y = tvy[v]; z = tvz[v];
            } else {
                float dx = m.vx[v] + ox - ex, dy = m.vy[v] + oy - ey, dz = m.vz[v] + oz - ez;
                x = dx * rx + dy * ry + dz * rz; y = dx * ux + dy * uy + dz * uz; z = dx * fx + dy * fy + dz * fz;
            }
            cax[n] = x; cay[n] = y; caz[n] = z; n++;
        }
        // clip against near + 4 side planes
        n = clipPlane(n, 0); if (n < 3) return;
        n = clipPlane(n, 1); if (n < 3) return;
        n = clipPlane(n, 2); if (n < 3) return;
        n = clipPlane(n, 3); if (n < 3) return;
        n = clipPlane(n, 4); if (n < 3) return;
        // project
        int ymin = H, ymax = -1;
        for (int i = 0; i < n; i++) {
            float iz = 1f / caz[i];
            psx[i] = cx + cax[i] * xscale * iz;
            psy[i] = cy - cay[i] * yscale * iz;
            if (caz[i] < zmin) zmin = caz[i];
        }
        rowStamp++;
        int rs = rowStamp;
        for (int i = 0; i < n; i++) {
            int j = i + 1 == n ? 0 : i + 1;
            float ya = psy[i], yb = psy[j], xa = psx[i], xb = psx[j];
            if (ya == yb) continue;
            if (ya > yb) { float tt = ya; ya = yb; yb = tt; tt = xa; xa = xb; xb = tt; }
            int iy0 = (int) Math.ceil(ya - 0.5f), iy1 = (int) Math.ceil(yb - 0.5f) - 1;
            if (iy0 < 0) iy0 = 0; if (iy1 > H - 1) iy1 = H - 1;
            if (iy0 > iy1) continue;
            float slope = (xb - xa) / (yb - ya);
            float x = xa + (iy0 + 0.5f - ya) * slope;
            if (iy0 < ymin) ymin = iy0;
            if (iy1 > ymax) ymax = iy1;
            for (int y = iy0; y <= iy1; y++, x += slope) {
                if (rowFrame[y] != rs) { rowFrame[y] = rs; rowMin[y] = x; rowMax[y] = x; }
                else { if (x < rowMin[y]) rowMin[y] = x; if (x > rowMax[y]) rowMax[y] = x; }
            }
        }
        if (ymax < ymin) return;
        statFaces++;
        if (mode == 1) {
            for (int y = ymin; y <= ymax; y++) {
                int xl = (int) Math.ceil(rowMin[y] - 0.5f), xr = (int) Math.ceil(rowMax[y] - 0.5f);
                if (xl < 0) xl = 0; if (xr > W) xr = W;
                if (xl < xr) spanInsert(y, xl, xr, true);
            }
            return;
        }
        setupGradients(f, ox, oy, oz, zmin);
        for (int y = ymin; y <= ymax; y++) {
            int xl = (int) Math.ceil(rowMin[y] - 0.5f), xr = (int) Math.ceil(rowMax[y] - 0.5f);
            if (xl < 0) xl = 0; if (xr > W) xr = W;
            if (xl >= xr) continue;
            if (mode == 0) spanInsert(y, xl, xr, false);
            else drawSpanZ(y, xl, xr);
        }
    }
    private int rowStamp = 0;
    private final int[] rowFrame;

    private int clipPlane(int n, int plane) {
        // returns new count, result stays in ca*
        float[] ix = cax, iy = cay, iz = caz, ox = cbx, oy = cby, oz = cbz;
        int out = 0;
        boolean anyOut = false;
        for (int i = 0; i < n; i++) if (pd(plane, ix[i], iy[i], iz[i]) < 0) { anyOut = true; break; }
        if (!anyOut) return n;
        for (int i = 0; i < n; i++) {
            int j = i + 1 == n ? 0 : i + 1;
            float da = pd(plane, ix[i], iy[i], iz[i]), db = pd(plane, ix[j], iy[j], iz[j]);
            if (da >= 0) { ox[out] = ix[i]; oy[out] = iy[i]; oz[out] = iz[i]; out++; }
            if ((da >= 0) != (db >= 0)) {
                float t = da / (da - db);
                ox[out] = ix[i] + (ix[j] - ix[i]) * t; oy[out] = iy[i] + (iy[j] - iy[i]) * t; oz[out] = iz[i] + (iz[j] - iz[i]) * t; out++;
            }
            if (out >= 62) break;
        }
        System.arraycopy(ox, 0, ix, 0, out); System.arraycopy(oy, 0, iy, 0, out); System.arraycopy(oz, 0, iz, 0, out);
        return out;
    }

    private float pd(int plane, float x, float y, float z) {
        switch (plane) {
            case 0: return z - NEAR;
            case 1: return x * xscale + z * cx + 0.01f;
            case 2: return -x * xscale + z * (W - cx) + 0.01f;
            case 3: return -y * yscale + z * cy + 0.01f;
            default: return y * yscale + z * (H - cy) + 0.01f;
        }
    }

    // gradients for current face
    private float gZx, gZy, gZ0, gSx, gSy, gS0, gTx, gTy, gT0, gLx, gLy, gL0, gMx, gMy, gM0;
    private byte[] gTex; private int gTsh, gTmask, gSmask; private int gLm, gLw, gLh; private boolean gMasked; private int gLevelConst;

    private void setupGradients(int f, float ox, float oy, float oz, float zmin) {
        int p = m.fPlane[f] & 0xFFFF;
        float nx = m.plNx[p], ny = m.plNy[p], nz = m.plNz[p];
        float dist = m.plD[p] + nx * ox + ny * oy + nz * oz - (nx * ex + ny * ey + nz * ez);
        if (dist > -0.01f && dist < 0.01f) dist = dist < 0 ? -0.01f : 0.01f;
        float idist = 1f / dist;
        float a = (nx * rx + ny * ry + nz * rz) * idist / xscale;
        float b = -(nx * ux + ny * uy + nz * uz) * idist / yscale;
        float c = (nx * fx + ny * fy + nz * fz) * idist;
        gZx = a; gZy = b; gZ0 = c - a * cx - b * cy;
        int t = m.fTex[f];
        // mip selection
        float[] S = m.fS; int o = f * 4;
        float slen = (float) Math.sqrt(S[o] * S[o] + S[o + 1] * S[o + 1] + S[o + 2] * S[o + 2]);
        float tpp = slen * zmin / xscale * mipScale;
        int mip = tpp < 1.6f ? 0 : tpp < 3.2f ? 1 : 2;
        mip += mipBias; if (mip > 2) mip = 2; if (mip < 0) mip = 0;
        if (mip >= m.texMip[t].length) mip = m.texMip[t].length - 1;
        float ms = 1f / (1 << mip);
        gTex = m.texMip[t][mip];
        int ti = t * 3 + mip;
        gTsh = texTsh[ti]; gTmask = texMaskT[ti]; gSmask = texMaskS[ti];
        gMasked = (m.texFlags[t] & 2) != 0;
        // s
        grad(m.fS, o, ms, ox, oy, oz); gSx = qx; gSy = qy; gS0 = q0;
        grad(m.fT, o, ms, ox, oy, oz); gTx = qx; gTy = qy; gT0 = q0;
        gLm = m.fLmOfs[f];
        if (gLm >= 0 && !fullbright) {
            gLw = m.fLmW[f] & 255; gLh = m.fLmH[f] & 255;
            grad(m.fLS, o, 1, ox, oy, oz); gLx = qx; gLy = qy; gL0 = q0;
            grad(m.fLT, o, 1, ox, oy, oz); gMx = qx; gMy = qy; gM0 = q0;
        } else { gLm = -1; gLevelConst = fullbright ? 32 : 40; }
    }

    private float qx, qy, q0;

    private void grad(float[] V, int o, float sc, float ox, float oy, float oz) {
        float vx = V[o] * sc, vy = V[o + 1] * sc, vz = V[o + 2] * sc;
        float k = (V[o + 3] * sc) + vx * (ex - ox) + vy * (ey - oy) + vz * (ez - oz);
        float sa = (vx * rx + vy * ry + vz * rz) / xscale;
        float sb = -(vx * ux + vy * uy + vz * uz) / yscale;
        float s0 = (vx * fx + vy * fy + vz * fz) - sa * cx - sb * cy;
        qx = sa + k * gZx; qy = sb + k * gZy; q0 = s0 + k * gZ0;
    }

    private int lightSample(float ls, float lt) {
        float mw = gLw - 1, mh = gLh - 1;
        if (ls < 0) ls = 0; else if (ls > mw) ls = mw;
        if (lt < 0) lt = 0; else if (lt > mh) lt = mh;
        int i0 = (int) ls, j0 = (int) lt;
        int fs = (int) ((ls - i0) * 256), ft = (int) ((lt - j0) * 256);
        int i1 = i0 < gLw - 1 ? i0 + 1 : i0, j1 = j0 < gLh - 1 ? j0 + 1 : j0;
        byte[] L = m.light;
        int b = gLm;
        int a00 = L[b + j0 * gLw + i0] & 255, a10 = L[b + j0 * gLw + i1] & 255;
        int a01 = L[b + j1 * gLw + i0] & 255, a11 = L[b + j1 * gLw + i1] & 255;
        int top = (a00 << 8) + (a10 - a00) * fs, bot = (a01 << 8) + (a11 - a01) * fs;
        return ((top << 8) + (bot - top) * ft); // level << 16
    }

    /** world span through coverage buffer */
    private void spanInsert(int y, int x0, int x1, boolean sky) {
        int base = y * MAXSEG;
        int n = segN[y];
        short[] A = segA, B = segB;
        int i = 0;
        while (i < n && B[base + i] <= x0) i++;
        int j = i;
        int cur = x0;
        while (cur < x1) {
            if (i < n && A[base + i] <= cur) { if (B[base + i] > cur) cur = B[base + i]; i++; continue; }
            int e = i < n ? A[base + i] : x1;
            if (e > x1) e = x1;
            if (sky) addSky(y, cur, e); else drawSpan(y, cur, e);
            covered += e - cur;
            cur = e;
        }
        // merge [x0,x1)
        int a = x0, b = x1;
        while (j > 0 && B[base + j - 1] >= a) j--;
        int k = j;
        while (k < n && A[base + k] <= b) { if (A[base + k] < a) a = A[base + k]; if (B[base + k] > b) b = B[base + k]; k++; }
        int removed = k - j;
        if (removed == 0) {
            if (n >= MAXSEG) {
                // overflow: extend the closest neighbour (gap pixels stay as they are)
                if (j >= n) B[base + n - 1] = (short) b;
                else if (j == 0) A[base] = (short) a;
                else if (a - B[base + j - 1] < A[base + j] - b) B[base + j - 1] = (short) b;
                else A[base + j] = (short) a;
                return;
            }
            System.arraycopy(A, base + j, A, base + j + 1, n - j);
            System.arraycopy(B, base + j, B, base + j + 1, n - j);
            A[base + j] = (short) a; B[base + j] = (short) b; segN[y] = (byte) (n + 1);
        } else {
            A[base + j] = (short) a; B[base + j] = (short) b;
            if (removed > 1) {
                System.arraycopy(A, base + k, A, base + j + 1, n - k);
                System.arraycopy(B, base + k, B, base + j + 1, n - k);
                segN[y] = (byte) (n - removed + 1);
            }
        }
    }

    private void addSky(int y, int x0, int x1) {
        int o = y * W;
        for (int x = x0; x < x1; x++) zb[o + x] = 0;
        if (nSky >= skyY.length) {
            short[] a = new short[nSky * 2]; System.arraycopy(skyY, 0, a, 0, nSky); skyY = a;
            a = new short[nSky * 2]; System.arraycopy(skyX0, 0, a, 0, nSky); skyX0 = a;
            a = new short[nSky * 2]; System.arraycopy(skyX1, 0, a, 0, nSky); skyX1 = a;
        }
        skyY[nSky] = (short) y; skyX0[nSky] = (short) x0; skyX1[nSky] = (short) x1; nSky++;
    }

    private void fillUncovered() {
        // any pixels not covered (leaks/outside) -> sky
        for (int y = 0; y < H; y++) {
            int base = y * MAXSEG, n = segN[y], cur = 0;
            for (int i = 0; i <= n; i++) {
                int e = i < n ? segA[base + i] : W;
                if (e > cur) addSky(y, cur, e);
                if (i < n) cur = segB[base + i];
            }
        }
    }

    private void drawSpan(int y, int x0, int x1) {
        statSpans++; statPix += x1 - x0;
        if (bilinear) { drawSpanBL(y, x0, x1); return; }
        float fy = y + 0.5f, fx0 = x0 + 0.5f;
        float zi = gZ0 + gZx * fx0 + gZy * fy;
        float sz = gS0 + gSx * fx0 + gSy * fy;
        float tz = gT0 + gTx * fx0 + gTy * fy;
        float lz = 0, mz = 0;
        boolean lit = gLm >= 0;
        if (lit) { lz = gL0 + gLx * fx0 + gLy * fy; mz = gM0 + gMx * fx0 + gMy * fy; }
        int[] F = fb, Z = zb, SH = shade;
        byte[] T = gTex;
        int tsh = gTsh, tmask = gTmask, smask = gSmask;
        int p = y * W + x0;
        int left = x1 - x0;
        float z = 1f / zi;
        int s = (int) (sz * z * 65536f), t = (int) (tz * z * 65536f);
        int l = lit ? lightSample(lz * z, mz * z) : gLevelConst << 16;
        int zf = (int) (zi * 16777216f);
        int dzf = (int) (gZx * 16777216f);
        int STEP = 1 << stepShift, SS = stepShift;
        while (left > 0) {
            int cnt = left > STEP ? STEP : left;
            zi += gZx * cnt; sz += gSx * cnt; tz += gTx * cnt;
            float z2 = 1f / zi;
            int s2 = (int) (sz * z2 * 65536f), t2 = (int) (tz * z2 * 65536f);
            int l2;
            if (lit) { lz += gLx * cnt; mz += gMx * cnt; l2 = lightSample(lz * z2, mz * z2); } else l2 = l;
            int ds, dt, dl;
            if (cnt == STEP) { ds = (s2 - s) >> SS; dt = (t2 - t) >> SS; dl = (l2 - l) >> SS; }
            else { ds = (s2 - s) / cnt; dt = (t2 - t) / cnt; dl = (l2 - l) / cnt; }
            for (int k = cnt; k > 0; k--) {
                F[p] = SH[((l >> 8) & 0x3F00) | (T[((t >> tsh) & tmask) | ((s >> 16) & smask)] & 255)];
                Z[p] = zf;
                p++; s += ds; t += dt; l += dl; zf += dzf;
            }
            s = s2; t = t2; l = l2;
            left -= cnt;
        }
    }

    /** bilinear-filtered world span: like the OpenGL renderer of the original game (GL_LINEAR) */
    private void drawSpanBL(int y, int x0, int x1) {
        float fy = y + 0.5f, fx0 = x0 + 0.5f;
        float zi = gZ0 + gZx * fx0 + gZy * fy;
        float sz = gS0 + gSx * fx0 + gSy * fy;
        float tz = gT0 + gTx * fx0 + gTy * fy;
        float lz = 0, mz = 0;
        boolean lit = gLm >= 0;
        if (lit) { lz = gL0 + gLx * fx0 + gLy * fy; mz = gM0 + gMx * fx0 + gMy * fy; }
        int[] F = fb, Z = zb, SH = shade;
        byte[] T = gTex;
        int tsh = gTsh, tmask = gTmask, smask = gSmask;
        int p = y * W + x0;
        int left = x1 - x0;
        float z = 1f / zi;
        int s = (int) (sz * z * 65536f) - 32768, t = (int) (tz * z * 65536f) - 32768;
        int l = lit ? lightSample(lz * z, mz * z) : gLevelConst << 16;
        int zf = (int) (zi * 16777216f);
        int dzf = (int) (gZx * 16777216f);
        int STEP = 1 << stepShift, SS = stepShift;
        while (left > 0) {
            int cnt = left > STEP ? STEP : left;
            zi += gZx * cnt; sz += gSx * cnt; tz += gTx * cnt;
            float z2 = 1f / zi;
            int s2 = (int) (sz * z2 * 65536f) - 32768, t2 = (int) (tz * z2 * 65536f) - 32768;
            int l2;
            if (lit) { lz += gLx * cnt; mz += gMx * cnt; l2 = lightSample(lz * z2, mz * z2); } else l2 = l;
            int ds, dt, dl;
            if (cnt == STEP) { ds = (s2 - s) >> SS; dt = (t2 - t) >> SS; dl = (l2 - l) >> SS; }
            else { ds = (s2 - s) / cnt; dt = (t2 - t) / cnt; dl = (l2 - l) / cnt; }
            for (int k = cnt; k > 0; k--) {
                int lv = (l >> 8) & 0x3F00;
                int sx = (s >> 16) & smask, sx1 = (sx + 1) & smask;
                int r0 = (t >> tsh) & tmask, r1 = ((t + 65536) >> tsh) & tmask;
                int fu = (s >> 8) & 255, fv = (t >> 8) & 255, iu = 256 - fu, iv = 256 - fv;
                int c00 = SH[lv | (T[r0 | sx] & 255)], c10 = SH[lv | (T[r0 | sx1] & 255)];
                int c01 = SH[lv | (T[r1 | sx] & 255)], c11 = SH[lv | (T[r1 | sx1] & 255)];
                int rbA = (((c00 & 0xFF00FF) * iu + (c10 & 0xFF00FF) * fu) >>> 8) & 0xFF00FF;
                int gA = (((c00 & 0xFF00) * iu + (c10 & 0xFF00) * fu) >>> 8) & 0xFF00;
                int rbB = (((c01 & 0xFF00FF) * iu + (c11 & 0xFF00FF) * fu) >>> 8) & 0xFF00FF;
                int gB = (((c01 & 0xFF00) * iu + (c11 & 0xFF00) * fu) >>> 8) & 0xFF00;
                F[p] = ((((rbA * iv + rbB * fv) >>> 8) & 0xFF00FF) | (((gA * iv + gB * fv) >>> 8) & 0xFF00));
                Z[p] = zf;
                p++; s += ds; t += dt; l += dl; zf += dzf;
            }
            s = s2; t = t2; l = l2;
            left -= cnt;
        }
    }

    /** z-tested span (brush entities, masked textures) */
    private void drawSpanZ(int y, int x0, int x1) {
        float fy = y + 0.5f, fx0 = x0 + 0.5f;
        float zi = gZ0 + gZx * fx0 + gZy * fy;
        float sz = gS0 + gSx * fx0 + gSy * fy;
        float tz = gT0 + gTx * fx0 + gTy * fy;
        float lz = 0, mz = 0;
        boolean lit = gLm >= 0;
        if (lit) { lz = gL0 + gLx * fx0 + gLy * fy; mz = gM0 + gMx * fx0 + gMy * fy; }
        int[] F = fb, Z = zb, SH = shade;
        byte[] T = gTex;
        int tsh = gTsh, tmask = gTmask, smask = gSmask;
        boolean masked = gMasked;
        int p = y * W + x0;
        int left = x1 - x0;
        float z = 1f / zi;
        int s = (int) (sz * z * 65536f), t = (int) (tz * z * 65536f);
        int l = lit ? lightSample(lz * z, mz * z) : gLevelConst << 16;
        int zf = (int) (zi * 16777216f);
        int dzf = (int) (gZx * 16777216f);
        int STEP = 1 << stepShift;
        while (left > 0) {
            int cnt = left > STEP ? STEP : left;
            zi += gZx * cnt; sz += gSx * cnt; tz += gTx * cnt;
            float z2 = 1f / zi;
            int s2 = (int) (sz * z2 * 65536f), t2 = (int) (tz * z2 * 65536f);
            int l2;
            if (lit) { lz += gLx * cnt; mz += gMx * cnt; l2 = lightSample(lz * z2, mz * z2); } else l2 = l;
            int ds = (s2 - s) / cnt, dt = (t2 - t) / cnt, dl = (l2 - l) / cnt;
            for (int k = cnt; k > 0; k--) {
                if (zf > Z[p]) {
                    int c = T[((t >> tsh) & tmask) | ((s >> 16) & smask)] & 255;
                    if (!masked || c != 255) { F[p] = SH[((l >> 8) & 0x3F00) | c]; Z[p] = zf; }
                }
                p++; s += ds; t += dt; l += dl; zf += dzf;
            }
            s = s2; t = t2; l = l2;
            left -= cnt;
        }
    }

    // ---------------------------------------------------------------- sky
    private void drawSky() {
        if (flatSky) {
            for (int i = 0; i < nSky; i++) { int p = skyY[i] * W, x1 = skyX1[i]; for (int x = skyX0[i]; x < x1; x++) fb[p + x] = skyFlat; }
            return;
        }
        int[][] SK = skyG;
        for (int i = 0; i < nSky; i++) {
            int y = skyY[i], x0 = skyX0[i], x1 = skyX1[i];
            float vy = (cy - (y + 0.5f)) / yscale;
            // direction = f + r*vx + u*vy
            float bx = fx + ux * vy, by = fy + uy * vy, bz = fz + uz * vy;
            float sx = rx / xscale, sy = ry / xscale, sz = rz / xscale;
            int p = y * W + x0;
            for (int x = x0; x < x1; x++) {
                float vx = (x + 0.5f - cx);
                float dx = bx + sx * vx, dy = by + sy * vx, dz = bz + sz * vx;
                float ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy, az = dz < 0 ? -dz : dz;
                int face; float s, t;
                if (az >= ax && az >= ay) {
                    if (dz > 0) { face = 4; float iz = 1f / az; s = -dy * iz; t = -dx * iz; }
                    else { fb[p++] = SK[3][127 * 128 + 64]; continue; }
                } else if (ax >= ay) {
                    float ia = 1f / ax;
                    if (dx > 0) { face = 0; s = -dy * ia; t = dz * ia; } else { face = 1; s = dy * ia; t = dz * ia; }
                } else {
                    float ia = 1f / ay;
                    if (dy > 0) { face = 2; s = dx * ia; t = dz * ia; } else { face = 3; s = -dx * ia; t = dz * ia; }
                }
                int ix = (int) ((s + 1) * 63.99f), iy = (int) ((1 - t) * 63.99f);
                if (ix < 0) ix = 0; if (ix > 127) ix = 127; if (iy < 0) iy = 0; if (iy > 127) iy = 127;
                fb[p++] = SK[face][(iy << 7) | ix];
            }
        }
    }

    // ---------------------------------------------------------------- sprites
    /**
     * Draw an indexed sprite billboard. pix: palette indices (0 = transparent), pal: ARGB palette.
     * (wx,wy,wz) anchor world position, (ax,ay) anchor in sprite pixels, ppu: sprite pixels per world unit.
     */
    public void drawSprite(byte[] pix, int sw, int sh, int[] pal, float wx, float wy, float wz, float ax, float ay, float ppu) {
        float dx = wx - ex, dy = wy - ey, dz = wz - ez;
        float z = dx * fx + dy * fy + dz * fz;
        if (z < 22) return;
        float x = dx * rx + dy * ry + dz * rz, y = dx * ux + dy * uy + dz * uz;
        float iz = 1f / z;
        float sxp = cx + x * xscale * iz, syp = cy - y * yscale * iz;
        float k = xscale * iz / ppu; // screen px per sprite px
        float left = sxp - ax * k, top = syp - ay * k;
        int x0 = (int) Math.ceil(left - 0.5f), y0 = (int) Math.ceil(top - 0.5f);
        int x1 = (int) Math.ceil(left + sw * k - 0.5f), y1 = (int) Math.ceil(top + sh * k - 0.5f);
        if (x1 <= 0 || y1 <= 0 || x0 >= W || y0 >= H) return;
        int step = (int) (65536f / k);
        int u0 = (int) (((x0 + 0.5f) - left) / k * 65536f), v = (int) (((y0 + 0.5f) - top) / k * 65536f);
        if (x0 < 0) { u0 += -x0 * step; x0 = 0; }
        if (y0 < 0) { v += -y0 * step; y0 = 0; }
        if (x1 > W) x1 = W; if (y1 > H) y1 = H;
        int zf = (int) (iz * 16777216f);
        for (int yy = y0; yy < y1; yy++, v += step) {
            int sy2 = v >> 16; if (sy2 >= sh) break;
            int row = sy2 * sw;
            int p = yy * W + x0;
            int u = u0;
            for (int xx = x0; xx < x1; xx++, p++, u += step) {
                int sx2 = u >> 16; if (sx2 >= sw) break;
                int c = pix[row + sx2] & 255;
                if (c != 0 && zf > zb[p]) { fb[p] = pal[c]; }
            }
        }
    }

    /** project world point to screen; returns false if behind. result in projX, projY, projZ */
    public float projX, projY, projZ;
    public boolean project(float wx, float wy, float wz) {
        float dx = wx - ex, dy = wy - ey, dz = wz - ez;
        float z = dx * fx + dy * fy + dz * fz;
        if (z < 1) return false;
        float x = dx * rx + dy * ry + dz * rz, y = dx * ux + dy * uy + dz * uz;
        projX = cx + x * xscale / z; projY = cy - y * yscale / z; projZ = z;
        return true;
    }

    // ---------------------------------------------------------------- real 3D studio models
    public static final int[] mshade = new int[64 * 256];
    public static void buildModelShade(int[] pal) {
        for (int l = 0; l < 64; l++) {
            float k = l / 32f;
            for (int c = 0; c < 256; c++) {
                int p = pal[c];
                int r = (int) (((p >> 16) & 255) * k), g = (int) (((p >> 8) & 255) * k), b = (int) ((p & 255) * k);
                if (r > 255) r = 255; if (g > 255) g = 255; if (b > 255) b = 255;
                mshade[l * 256 + c] = (gamma[r] << 16) | (gamma[g] << 8) | gamma[b];
            }
        }
    }

    private float[] mx = new float[512], my = new float[512], mz = new float[512];
    private int[] mlev = new int[512];
    public int statTris;
    /** light comes from above, slightly from the side (like GoldSrc's default lightvec) */
    private static final float LX = 0.30f, LY = 0.20f, LZ = -0.93f;

    /** draw one submodel with world bone matrices M (12 floats per bone); base light level 0..63 */
    public void drawMesh(Mdl3.Sub u, float[] M, int baseLev) { drawMesh(u, M, baseLev, false); }

    public void drawMesh(Mdl3.Sub u, float[] M, int baseLev, boolean mirror) { drawMesh(u, M, baseLev, mirror, false); }

    /** byNormals: decide front/back from the vertex normals (models whose parts are wound inconsistently, e.g. weapon viewmodels) */
    public void drawMesh(Mdl3.Sub u, float[] M, int baseLev, boolean mirror, boolean byNormals) {
        int nv = u.nv, nn = u.nn;
        if (mwx.length < nv) { mwx = new float[nv]; mwy = new float[nv]; mwz = new float[nv]; }
        if (mnx.length < nn) { mnx = new float[nn]; mny = new float[nn]; mnz = new float[nn]; }
        if (mx.length < nv) { mx = new float[nv]; my = new float[nv]; mz = new float[nv]; }
        if (mcx.length < nv) { mcx = new float[nv]; mcy = new float[nv]; }
        if (mlev.length < nn) mlev = new int[nn];
        float[] V = u.v; byte[] VB = u.vbone;
        int inFront = 0;
        for (int i = 0; i < nv; i++) {
            int o = VB[i] * 12;
            float vx = V[i * 3], vy = V[i * 3 + 1], vz = V[i * 3 + 2];
            float wx = M[o] * vx + M[o + 1] * vy + M[o + 2] * vz + M[o + 3] - ex;
            float wy = M[o + 4] * vx + M[o + 5] * vy + M[o + 6] * vz + M[o + 7] - ey;
            float wz = M[o + 8] * vx + M[o + 9] * vy + M[o + 10] * vz + M[o + 11] - ez;
            float z = wx * fx + wy * fy + wz * fz;
            mz[i] = z; mwx[i] = wx; mwy[i] = wy; mwz[i] = wz;
            float qx = wx * rx + wy * ry + wz * rz, qy = wx * ux + wy * uy + wz * uz;
            mcx[i] = qx; mcy[i] = qy;
            if (z > MNEAR) {
                inFront++;
                float iz = 1f / z;
                mx[i] = cx + qx * xscale * iz;
                my[i] = cy - qy * yscale * iz;
            }
        }
        if (inFront == 0) return;
        float[] N = u.n; byte[] NB = u.nbone;
        for (int i = 0; i < nn; i++) {
            int o = NB[i] * 12;
            float nx = N[i * 3], ny = N[i * 3 + 1], nz = N[i * 3 + 2];
            float wx = M[o] * nx + M[o + 1] * ny + M[o + 2] * nz, wy = M[o + 4] * nx + M[o + 5] * ny + M[o + 6] * nz, wz = M[o + 8] * nx + M[o + 9] * ny + M[o + 10] * nz;
            mnx[i] = wx; mny[i] = wy; mnz[i] = wz;
            float d = -(wx * LX + wy * LY + wz * LZ);
            float k = 0.62f + (d > 0 ? d * 0.55f : d * 0.12f);
            int l = (int) (baseLev * k) + 3;
            mlev[i] = l < 0 ? 0 : l > 63 ? 63 : l;
        }
        for (int mi = 0; mi < u.tri.length; mi++) {
            int tid = u.mtex[mi];
            byte[] T = Mdl3.tex[tid]; int tw = Mdl3.texW[tid], th = Mdl3.texH[tid];
            short[] t = u.tri[mi];
            for (int k = 0; k < t.length; k += 12) {
                int a = t[k], b = t[k + 4], c = t[k + 8];
                boolean flip = false;
                if (byNormals) {   // facing from the normals: sum of n·(vertex - eye) < 0 means it faces us
                    int na = t[k + 1], nb = t[k + 5], nc = t[k + 9];
                    float f = mnx[na] * mwx[a] + mny[na] * mwy[a] + mnz[na] * mwz[a]
                            + mnx[nb] * mwx[b] + mny[nb] * mwy[b] + mnz[nb] * mwz[b]
                            + mnx[nc] * mwx[c] + mny[nc] * mwy[c] + mnz[nc] * mwz[c];
                    if (f >= 0) continue;
                }
                if (mz[a] <= MNEAR || mz[b] <= MNEAR || mz[c] <= MNEAR) {
                    if (mz[a] <= MNEAR && mz[b] <= MNEAR && mz[c] <= MNEAR) continue;
                    clipTri(t, k, T, tw, th, mirror, byNormals);   // cut at the near plane instead of dropping the whole triangle
                    continue;
                }
                float ax = mx[a], ay = my[a], bx = mx[b], by = my[b], qx = mx[c], qy = my[c];
                float area = (bx - ax) * (qy - ay) - (qx - ax) * (by - ay);
                if (!byNormals && (mirror ? area >= 0 : area <= 0)) continue;
                statTris++;
                tri(ax, ay, 1f / mz[a], t[k + 2], t[k + 3], mlev[t[k + 1]],
                    bx, by, 1f / mz[b], t[k + 6], t[k + 7], mlev[t[k + 5]],
                    qx, qy, 1f / mz[c], t[k + 10], t[k + 11], mlev[t[k + 9]], T, tw, th);
            }
        }
    }

    static final float MNEAR = 2f;   // model near plane
    float[] mcx = new float[0], mcy = new float[0];
    final float[] cp = new float[6 * 5], cq = new float[6 * 5];

    /** triangle crossing the near plane: Sutherland-Hodgman against z = MNEAR, then a fan of up to 2 triangles */
    float[] mwx = new float[0], mwy = new float[0], mwz = new float[0], mnx = new float[0], mny = new float[0], mnz = new float[0];
    void clipTri(short[] t, int k, byte[] T, int tw, int th, boolean mirror, boolean byNormals) {
        int n = 0;
        for (int j = 0; j < 3; j++) {
            int v = t[k + j * 4];
            cp[n * 6] = mcx[v]; cp[n * 6 + 1] = mcy[v]; cp[n * 6 + 2] = mz[v];
            cp[n * 6 + 3] = t[k + j * 4 + 2]; cp[n * 6 + 4] = t[k + j * 4 + 3]; cp[n * 6 + 5] = mlev[t[k + j * 4 + 1]];
            n++;
        }
        int m = 0;
        for (int j = 0; j < n; j++) {
            int a = j * 6, b = ((j + 1) % n) * 6;
            boolean ina = cp[a + 2] > MNEAR, inb = cp[b + 2] > MNEAR;
            if (ina) { for (int q = 0; q < 6; q++) cq[m * 6 + q] = cp[a + q]; m++; }
            if (ina != inb) {
                float f = (MNEAR + 0.01f - cp[a + 2]) / (cp[b + 2] - cp[a + 2]);
                for (int q = 0; q < 6; q++) cq[m * 6 + q] = cp[a + q] + (cp[b + q] - cp[a + q]) * f;
                m++;
            }
        }
        if (m < 3) return;
        for (int j = 0; j < m; j++) {   // project
            float iz = 1f / cq[j * 6 + 2];
            float px = cx + cq[j * 6] * xscale * iz, py = cy - cq[j * 6 + 1] * yscale * iz;
            cq[j * 6] = px; cq[j * 6 + 1] = py; cq[j * 6 + 2] = iz;
        }
        for (int j = 1; j + 1 < m; j++) {
            int a = 0, b = j * 6, c = (j + 1) * 6;
            float area = (cq[b] - cq[a]) * (cq[c + 1] - cq[a + 1]) - (cq[c] - cq[a]) * (cq[b + 1] - cq[a + 1]);
            if (!byNormals && (mirror ? area >= 0 : area <= 0)) continue;
            tri(cq[a], cq[a + 1], cq[a + 2], cq[a + 3], cq[a + 4], cq[a + 5],
                cq[b], cq[b + 1], cq[b + 2], cq[b + 3], cq[b + 4], cq[b + 5],
                cq[c], cq[c + 1], cq[c + 2], cq[c + 3], cq[c + 4], cq[c + 5], T, tw, th);
        }
    }

    /** z-buffered, affine-textured, Gouraud-lit triangle */
    private void tri(float x0, float y0, float z0, float u0, float v0, float l0,
                     float x1, float y1, float z1, float u1, float v1, float l1,
                     float x2, float y2, float z2, float u2, float v2, float l2, byte[] T, int tw, int th) {
        float dx1 = x1 - x0, dy1 = y1 - y0, dx2 = x2 - x0, dy2 = y2 - y0;
        float den = dx1 * dy2 - dx2 * dy1;
        if (den > -0.01f && den < 0.01f) return;
        float id = 1f / den;
        float dzx = ((z1 - z0) * dy2 - (z2 - z0) * dy1) * id, dzy = ((z2 - z0) * dx1 - (z1 - z0) * dx2) * id;
        float dux = ((u1 - u0) * dy2 - (u2 - u0) * dy1) * id, duy = ((u2 - u0) * dx1 - (u1 - u0) * dx2) * id;
        float dvx = ((v1 - v0) * dy2 - (v2 - v0) * dy1) * id, dvy = ((v2 - v0) * dx1 - (v1 - v0) * dx2) * id;
        float dlx = ((l1 - l0) * dy2 - (l2 - l0) * dy1) * id, dly = ((l2 - l0) * dx1 - (l1 - l0) * dx2) * id;
        // sort by y: A top, B middle, C bottom
        float ax = x0, ay = y0, bx = x1, by = y1, cx2 = x2, cy2 = y2, t;
        if (ay > by) { t = ax; ax = bx; bx = t; t = ay; ay = by; by = t; }
        if (by > cy2) { t = bx; bx = cx2; cx2 = t; t = by; by = cy2; cy2 = t; }
        if (ay > by) { t = ax; ax = bx; bx = t; t = ay; ay = by; by = t; }
        int ys = (int) Math.ceil(ay - 0.5f), ye = (int) Math.ceil(cy2 - 0.5f);
        if (ys < 0) ys = 0; if (ye > H) ye = H;
        if (ys >= ye) return;
        float sAC = (cx2 - ax) / (cy2 - ay);
        float sAB = by - ay > 0.0001f ? (bx - ax) / (by - ay) : 0, sBC = cy2 - by > 0.0001f ? (cx2 - bx) / (cy2 - by) : 0;
        int[] F = fb, Z = zb, MS = mshade;
        int dzf = (int) (dzx * 16777216f), duf = (int) (dux * 65536f), dvf = (int) (dvx * 65536f), dlf = (int) (dlx * 65536f);
        int twm = tw - 1, thm = th - 1;
        for (int y = ys; y < ye; y++) {
            float py = y + 0.5f;
            float xl = ax + (py - ay) * sAC;
            float xr = py < by ? ax + (py - ay) * sAB : bx + (py - by) * sBC;
            if (xl > xr) { t = xl; xl = xr; xr = t; }
            int xs = (int) Math.ceil(xl - 0.5f), xe = (int) Math.ceil(xr - 0.5f);
            if (xs < 0) xs = 0; if (xe > W) xe = W;
            if (xs >= xe) continue;
            float px = xs + 0.5f - x0, qy = py - y0;
            int zf = (int) ((z0 + dzx * px + dzy * qy) * 16777216f);
            int uf = (int) ((u0 + dux * px + duy * qy) * 65536f);
            int vf = (int) ((v0 + dvx * px + dvy * qy) * 65536f);
            int lf = (int) ((l0 + dlx * px + dly * qy) * 65536f);
            int p = y * W + xs, pe = y * W + xe;
            for (; p < pe; p++) {
                if (zf > Z[p]) {
                    int uu = uf >> 16, vv = vf >> 16, lv = lf >> 16;
                    if (uu < 0) uu = 0; else if (uu > twm) uu = twm;
                    if (vv < 0) vv = 0; else if (vv > thm) vv = thm;
                    if (lv < 0) lv = 0; else if (lv > 63) lv = 63;
                    F[p] = MS[(lv << 8) | (T[vv * tw + uu] & 255)];
                    Z[p] = zf;
                }
                zf += dzf; uf += duf; vf += dvf; lf += dlf;
            }
        }
    }

    public void clearZ() { int[] Z = zb; for (int i = Z.length - 1; i >= 0; i--) Z[i] = 0; }

    /** z value for a given view depth */
    public static int zOf(float z) { return (int) (16777216f / z); }

}
