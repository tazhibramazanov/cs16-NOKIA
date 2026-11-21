package cs;

import java.io.*;
import java.util.Hashtable;

/** 2D drawing on the frame buffer: fonts, HUD sprites (additive), boxes, indexed sprites. */
public final class Gfx {
    public int W, H; public int[] fb;
    public static final int HUDCOL = 0xFFA000; // classic CS orange-yellow
    // fonts
    public Font f1, f2;
    // HUD icons
    private final Hashtable hud = new Hashtable();
    private byte[][] hData; private int[] hW, hH;

    public static final class Font {
        public int h; char[] code; int[] w; byte[][] g;
        int find(char c) {
            int lo = 0, hi = code.length - 1;
            while (lo <= hi) { int m = (lo + hi) >> 1; if (code[m] == c) return m; if (code[m] < c) lo = m + 1; else hi = m - 1; }
            return -1;
        }
    }

    public Gfx(int w, int h, int[] fb) { W = w; H = h; this.fb = fb; }

    public void load() throws IOException {
        f1 = loadFont("/f1.fnt"); f2 = loadFont("/f2.fnt");
        Bin in = new Bin(Sys.s.open("/hud.bin"));
        in.readInt();
        int n = in.readShort();
        hData = new byte[n][]; hW = new int[n]; hH = new int[n];
        for (int i = 0; i < n; i++) {
            String nm = in.readUTF(); hW[i] = in.readShort(); hH[i] = in.readShort();
            hData[i] = new byte[hW[i] * hH[i]]; in.readFully(hData[i]);
            hud.put(nm, new Integer(i));
        }
        in.close();
    }

    private Font loadFont(String name) throws IOException {
        Bin in = new Bin(Sys.s.open(name));
        in.readInt();
        Font f = new Font();
        f.h = in.readShort(); int n = in.readShort();
        f.code = new char[n]; f.w = new int[n]; f.g = new byte[n][];
        // sort by code: written in order of ascii then cyrillic then extras -> insertion
        char[] c = new char[n]; int[] w = new int[n]; byte[][] g = new byte[n][];
        for (int i = 0; i < n; i++) {
            c[i] = (char) in.readShort(); w[i] = in.readUnsignedByte(); g[i] = new byte[w[i] * f.h]; in.readFully(g[i]);
        }
        in.close();
        int[] idx = new int[n]; for (int i = 0; i < n; i++) idx[i] = i;
        for (int i = 1; i < n; i++) { int v = idx[i], j = i - 1; while (j >= 0 && c[idx[j]] > c[v]) { idx[j + 1] = idx[j]; j--; } idx[j + 1] = v; }
        for (int i = 0; i < n; i++) { f.code[i] = c[idx[i]]; f.w[i] = w[idx[i]]; f.g[i] = g[idx[i]]; }
        return f;
    }

    public int hudId(String name) { Integer i = (Integer) hud.get(name); return i == null ? -1 : i.intValue(); }
    public int hudW(int id) { return id < 0 ? 0 : hW[id]; }
    public int hudH(int id) { return id < 0 ? 0 : hH[id]; }

    /** HUD sprite: alpha-blended with a 1px dark shadow for readability on a small screen */
    public void hud(int id, int x, int y, int col, int alpha) {
        if (id < 0) return;
        hudPass(id, x + 1, y + 1, 0, alpha * 3 / 4);
        hudPass(id, x, y, col, alpha);
    }

    private void hudPass(int id, int x, int y, int col, int alpha) {
        int w = hW[id], h = hH[id]; byte[] d = hData[id];
        int cr = (col >> 16) & 255, cg = (col >> 8) & 255, cb = col & 255;
        for (int j = 0; j < h; j++) {
            int yy = y + j; if (yy < 0 || yy >= H) continue;
            int o = yy * W;
            for (int i = 0; i < w; i++) {
                int xx = x + i; if (xx < 0 || xx >= W) continue;
                int a = (d[j * w + i] & 255) * 3 / 2; if (a > 255) a = 255;
                a = (a * alpha) >> 8;
                if (a == 0) continue;
                int p = fb[o + xx];
                int r = (p >> 16) & 255, g = (p >> 8) & 255, b = p & 255;
                r += ((cr - r) * a) >> 8; g += ((cg - g) * a) >> 8; b += ((cb - b) * a) >> 8;
                fb[o + xx] = (r << 16) | (g << 8) | b;
            }
        }
    }

    private final int[] digits = new int[10];
    private boolean digitsInit;

    /** HUD number using CS sprites; returns width */
    public int number(int v, int x, int y, int col, int alpha, int minDigits) {
        if (!digitsInit) { for (int i = 0; i < 10; i++) digits[i] = hudId("number_" + i); digitsInit = true; }
        String s = Integer.toString(v < 0 ? 0 : v);
        while (s.length() < minDigits) s = "0" + s;
        int dw = hW[digits[0]] + 2;
        for (int i = 0; i < s.length(); i++) hud(digits[s.charAt(i) - '0'], x + i * dw, y, col, alpha);
        return s.length() * dw;
    }

    public int numberW(int v, int minDigits) {
        if (!digitsInit) { for (int i = 0; i < 10; i++) digits[i] = hudId("number_" + i); digitsInit = true; }
        String s = Integer.toString(v < 0 ? 0 : v);
        int l = Math.max(s.length(), minDigits);
        return l * (hW[digits[0]] + 2);
    }

    public int textW(Font f, String s) {
        s = Lang.t(s);
        int w = 0;
        for (int i = 0; i < s.length(); i++) { int k = f.find(s.charAt(i)); w += k < 0 ? f.h / 3 : f.w[k]; }
        return w;
    }

    public void text(Font f, String s, int x, int y, int col) {
        s = Lang.t(s);
        int cr = (col >> 16) & 255, cg = (col >> 8) & 255, cb = col & 255;
        for (int i = 0; i < s.length(); i++) {
            int k = f.find(s.charAt(i));
            if (k < 0) { x += f.h / 3; continue; }
            int w = f.w[k]; byte[] g = f.g[k];
            for (int j = 0; j < f.h; j++) {
                int yy = y + j; if (yy < 0 || yy >= H) continue;
                int o = yy * W;
                for (int q = 0; q < w; q++) {
                    int xx = x + q; if (xx < 0 || xx >= W) continue;
                    int a = g[j * w + q] & 255;
                    if (a == 0) continue;
                    if (a > 250) { fb[o + xx] = col; continue; }
                    int p = fb[o + xx];
                    int r = (p >> 16) & 255, gg = (p >> 8) & 255, b = p & 255;
                    r += ((cr - r) * a) >> 8; gg += ((cg - gg) * a) >> 8; b += ((cb - b) * a) >> 8;
                    fb[o + xx] = (r << 16) | (gg << 8) | b;
                }
            }
            x += w;
        }
    }

    /** text with dark shadow */
    public void textS(Font f, String s, int x, int y, int col) { text(f, s, x + 1, y + 1, 0); text(f, s, x, y, col); }
    public void textC(Font f, String s, int cx, int y, int col) { textS(f, s, cx - textW(f, s) / 2, y, col); }

    public void fill(int x, int y, int w, int h, int col, int alpha) {
        if (x < 0) { w += x; x = 0; } if (y < 0) { h += y; y = 0; }
        if (x + w > W) w = W - x; if (y + h > H) h = H - y;
        if (w <= 0 || h <= 0) return;
        int cr = (col >> 16) & 255, cg = (col >> 8) & 255, cb = col & 255;
        for (int j = y; j < y + h; j++) {
            int o = j * W;
            for (int i = x; i < x + w; i++) {
                if (alpha >= 256) { fb[o + i] = col; continue; }
                int p = fb[o + i];
                int r = (p >> 16) & 255, g = (p >> 8) & 255, b = p & 255;
                r += ((cr - r) * alpha) >> 8; g += ((cg - g) * alpha) >> 8; b += ((cb - b) * alpha) >> 8;
                fb[o + i] = (r << 16) | (g << 8) | b;
            }
        }
    }

    public void rect(int x, int y, int w, int h, int col) {
        fill(x, y, w, 1, col, 256); fill(x, y + h - 1, w, 1, col, 256); fill(x, y, 1, h, col, 256); fill(x + w - 1, y, 1, h, col, 256);
    }

    // ---------------------------------------------------------------- VGUI2 / Steam "classic" look (TrackerScheme of CS 1.6)
    public static final int FRAME = 0x4C5844, BRIGHT = 0x889180, DARK = 0x282E22, FIELD = 0x3E4637, ORANGE = 0x8E8923,
            OFFWHITE = 0xD8D8D8, DULL = 0xB6B6B6, CHECK = 0xC4B550, TRACK = 0x1F1F1F;
    // VGUI1 in-game look (ClientScheme of CS 1.6)
    public static final int VG = 0xFFB000, VG_DIS = 0x7A5200;

    /** 3D border: raised = lit top/left */
    public void bevel(int x, int y, int w, int h, boolean raised) {
        int a = raised ? BRIGHT : DARK, b = raised ? DARK : BRIGHT;
        fill(x, y, w, 1, a, 256); fill(x, y, 1, h, a, 256);
        fill(x, y + h - 1, w, 1, b, 256); fill(x + w - 1, y, 1, h, b, 256);
    }

    /** a Steam-style window: olive frame, raised border, white title */
    public void frame(int x, int y, int w, int h, String title) {
        fill(x, y, w, h, FRAME, 256);
        bevel(x, y, w, h, true);
        if (title != null) {
            text(f1, title, x + 8, y + 3, 0xFFFFFF);
            // close "x" like the GameUI frame buttons
            int cx = x + w - 12, cy = y + 4;
            for (int i = 0; i < 6; i++) { fill(cx + i, cy + i, 1, 1, DULL, 256); fill(cx + 5 - i, cy + i, 1, 1, DULL, 256); }
        }
    }

    public void button(int x, int y, int w, int h, String label, boolean focus) {
        fill(x, y, w, h, focus ? ORANGE : FRAME, 256);
        bevel(x, y, w, h, true);
        if (focus) rect(x - 1, y - 1, w + 2, h + 2, 0);
        text(f1, label, x + (w - textW(f1, label)) / 2, y + (h - f1.h) / 2, 0xFFFFFF);
    }

    public void checkbox(int x, int y, boolean on) {
        fill(x, y, 9, 9, FIELD, 256);
        bevel(x, y, 9, 9, false);
        if (on) for (int i = 0; i < 5; i++) {   // check mark
            int px = x + 2 + i, py = i < 2 ? y + 4 + i : y + 6 - (i - 2) * 1;
            fill(px, py, 1, 2, CHECK, 256);
        }
    }

    public void combo(int x, int y, int w, String value, boolean focus) {
        fill(x, y, w, 11, FIELD, 256);
        bevel(x, y, w, 11, false);
        text(f1, value, x + 3, y + (11 - f1.h) / 2, focus ? 0xFFFFFF : OFFWHITE);
        int bx = x + w - 10;
        fill(bx, y + 1, 9, 9, FRAME, 256); bevel(bx, y + 1, 9, 9, true);
        for (int i = 0; i < 3; i++) fill(bx + 2 + i, y + 4 + i, 5 - i * 2, 1, focus ? 0xFFFFFF : DULL, 256);
    }

    /** horizontal slider, v in 0..1000 */
    public void slider(int x, int y, int w, int v, boolean focus) {
        fill(x, y + 4, w, 2, TRACK, 256);
        for (int i = 0; i <= 4; i++) fill(x + i * (w - 1) / 4, y + 8, 1, 2, DULL, 256);
        int nx = x + (w - 5) * v / 1000;
        fill(nx, y, 5, 10, focus ? ORANGE : FRAME, 256); bevel(nx, y, 5, 10, true);
    }

    /** ARGB image (alpha < 128 = transparent) */
    public void blitARGB(int[] px, int w, int h, int x, int y) {
        if (px == null) return;
        for (int j = 0; j < h; j++) {
            int yy = y + j; if (yy < 0 || yy >= H) continue;
            int o = yy * W, s = j * w;
            for (int i = 0; i < w; i++) {
                int xx = x + i; if (xx < 0 || xx >= W) continue;
                int c = px[s + i];
                if ((c >>> 24) >= 128) fb[o + xx] = c & 0xFFFFFF;
            }
        }
    }

    /** blit indexed image (0 = transparent) with palette */
    public void blit(byte[] d, int w, int h, int x, int y, int[] pal) {
        for (int j = 0; j < h; j++) {
            int yy = y + j; if (yy < 0 || yy >= H) continue;
            int o = yy * W, s = j * w;
            int i0 = 0, i1 = w;
            if (x < 0) i0 = -x; if (x + w > W) i1 = W - x;
            for (int i = i0; i < i1; i++) {
                int c = d[s + i] & 255;
                if (c != 0) fb[o + x + i] = pal[c];
            }
        }
    }

    /** whole-screen tint (flashbang, damage) */
    public void tint(int col, int alpha) { fill(0, 0, W, H, col, alpha); }
}
