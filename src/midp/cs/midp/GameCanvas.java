package cs.midp;

import javax.microedition.lcdui.*;
import cs.*;

/** Full-screen canvas: runs the game loop and maps the Nokia E52 keypad to game actions. */
public final class GameCanvas extends Canvas implements Runnable {
    static final int GW = 240, GH = 320;   // portrait: the E52 screen is 240x320
    private final CSMidlet mid;
    private int[] screen = new int[GW * GH];   // the game's frame: 240x320, or 320x240 in horizontal mode
    private int[] rotBuf;                       // the frame turned a quarter (when it does not match the screen's shape)
    private volatile int[] shown;               // what paint() draws
    private volatile int shW, shH;              // its size
    private int orient = -1;                    // orientation the buffers are set up for
    private boolean land;                       // the phone's screen is wider than tall (E71/E72/E63, E6, N8 sideways...)
    private boolean turned;                     // frame is turned a quarter on the screen: arrows and digits are turned too
    private volatile int pendingOrient = -1;    // screen shape changed (phone rotated / slider opened)
    private int[] scaled, xmap;                 // the frame stretched to fill bigger (or shrunk for smaller) screens
    private int scW, scH, scBW;
    private final java.util.Hashtable downAs = new java.util.Hashtable();   // physical key -> code used at press
    private Game game;
    private MidpSys sys;
    private volatile boolean running, paused;
    private Thread thread;
    private String error;
    private int ox, oy;

    GameCanvas(CSMidlet m) {
        mid = m;
        setFullScreenMode(true);
    }

    void start() {
        running = true;
        thread = new Thread(this);
        thread.start();
    }

    void stop() { running = false; if (sys != null) sys.shutdown(); }
    void pause() { paused = true; if (game != null) game.pauseFromSystem(); if (sys != null) sys.stopSounds(); }
    void resume() { paused = false; }

    protected void hideNotify() { pause(); }
    protected void showNotify() { resume(); }

    public void run() {
        try {
            sys = new MidpSys(mid);
            Sys.s = sys;
            land = getWidth() > getHeight();
            game = new Game(GW, GH, screen);
            if (land) game.orient = 1;   // first start on a landscape phone: landscape picture (saved setting wins)
            game.init();
        } catch (Throwable t) {
            error = "Ошибка запуска: " + t;
            repaint();
            return;
        }
        long last = System.currentTimeMillis();
        while (running) {
            long now = System.currentTimeMillis();
            float dt = (now - last) / 1000f;
            last = now;
            if (paused && (game == null || game.mp == null)) { try { Thread.sleep(100); } catch (InterruptedException e) { } continue; }   // online the match goes on for the others
            try {
                if (pendingOrient >= 0) { game.orient = pendingOrient; pendingOrient = -1; }
                if (game.orient != orient) setOrient(game.orient);
                game.tick(dt);
                if (paused) { try { Thread.sleep(30); } catch (InterruptedException e) { } continue; }   // hidden: keep the match going, skip drawing
                game.draw();
                int fw = orient == 1 ? GH : GW, fh = orient == 1 ? GW : GH;
                int[] f = screen;
                if (turned) { rotate(screen, fw, fh, rotBuf); f = rotBuf; int t = fw; fw = fh; fh = t; }
                int[] sc = fit(f, fw, fh);
                if (sc != null) { shown = sc; shW = scW; shH = scH; } else { shown = f; shW = fw; shH = fh; }
            } catch (OutOfMemoryError e) {
                error = "Недостаточно памяти";
            } catch (Throwable t) {
                error = "Ошибка: " + t;
                t.printStackTrace();
            }
            repaint();
            serviceRepaints();
            long spent = System.currentTimeMillis() - now;
            int frameMs = game != null && game.fpsCap20 ? 50 : 15;   // "Лимит FPS: 20" saves battery and heat
            if (spent < frameMs) { try { Thread.sleep(frameMs - spent); } catch (InterruptedException e) { } }
            else Thread.yield();
        }
    }

    /** the picture's shape differs from the screen's (E52 held sideways, or a landscape phone held upright):
     *  the phone is turned left (keypad on the right) - game top is the right edge of the screen */
    private void setOrient(int o) {
        orient = o;
        if (o == 1) { screen = new int[GH * GW]; game.resize(GH, GW, screen); }
        else { screen = new int[GW * GH]; game.resize(GW, GH, screen); }
        turned = (o == 1) != land;
        game.turned = turned;
        rotBuf = turned ? new int[GW * GH] : null;
        downAs.clear();
    }

    /** fw x fh frame -> fh x fw, turned a quarter clockwise */
    private static void rotate(int[] in, int fw, int fh, int[] out) {
        for (int y = 0; y < fh; y++) {
            int i = y * fw, o = fh - 1 - y;
            for (int x = 0; x < fw; x++, o += fh) out[o] = in[i++];
        }
    }

    /** other Nokia screens: stretch (or shrink) the picture to fill the screen, keeping its shape.
     *  null = the screen is about the frame's size, draw it as it is */
    private int[] fit(int[] f, int fw, int fh) {
        int w = getWidth(), h = getHeight();
        if (w <= 0 || h <= 0) return null;
        int k = Math.min(w * 256 / fw, h * 256 / fh);   // scale, 256 = 1:1
        if (k >= 236 && k <= 300) return null;
        int dw = fw * k / 256, dh = fh * k / 256;
        if (dw < 16 || dh < 16) return null;
        if (scaled == null || dw != scW || dh != scH || fw != scBW) {
            scaled = null;
            scaled = new int[dw * dh];
            xmap = new int[dw];
            for (int x = 0; x < dw; x++) xmap[x] = x * fw / dw;
            scW = dw; scH = dh; scBW = fw;
        }
        int[] out = scaled, xm = xmap;
        int prev = -1;
        for (int y = 0, o = 0; y < dh; y++, o += dw) {
            int sy = y * fh / dh;
            if (sy == prev) { System.arraycopy(out, o - dw, out, o, dw); continue; }
            prev = sy;
            int b = sy * fw;
            for (int x = 0; x < dw; x++) out[o + x] = f[b + xm[x]];
        }
        return out;
    }

    /** phone rotated (5800/N97/N8) or keyboard slid open (E7/N97): keep the picture the screen's shape */
    protected void sizeChanged(int w, int h) {
        boolean l = w > h;
        if (game != null && l != land) { land = l; pendingOrient = l ? 1 : 0; }
        else land = l;
        orient = -1;   // rebuild the buffers for the new shape
    }

    protected void paint(Graphics g) {
        int w = getWidth(), h = getHeight();
        int[] buf = shown;
        int bw = shW, bh = shH;
        if (buf == null || bw <= 0) { g.setColor(0); g.fillRect(0, 0, w, h); bw = 0; bh = 0; }
        ox = (w - bw) / 2; oy = (h - bh) / 2;
        if (ox != 0 || oy != 0) { g.setColor(0); g.fillRect(0, 0, w, h); }
        int sx = ox < 0 ? -ox : 0, sy = oy < 0 ? -oy : 0;
        int dw = bw - sx, dh = bh - sy;
        if (dw > w) dw = w;
        if (dh > h) dh = h;
        if (buf != null && dw > 0 && dh > 0 && buf.length >= bw * bh)
            g.drawRGB(buf, sy * bw + sx, bw, ox + sx, oy + sy, dw, dh, false);
        if (error != null) {
            g.setColor(0xFF4040);
            g.setFont(Font.getFont(Font.FACE_SYSTEM, Font.STYLE_PLAIN, Font.SIZE_SMALL));
            g.drawString(cs.Lang.t(error), 2, h - 2, Graphics.LEFT | Graphics.BOTTOM);
        }
    }

    // ------------------------------------------------------------------ keys
    /** normalise a device key code: digits, * and # stay; joystick/soft keys become -1..-8 (Nokia codes) */
    private int canon(int k) {
        if ((k >= KEY_NUM0 && k <= KEY_NUM9) || k == KEY_STAR || k == KEY_POUND) return k;
        if (k >= -8 && k <= -1) return k;
        try {
            switch (getGameAction(k)) {
                case UP: return Binds.K_UP;
                case DOWN: return Binds.K_DOWN;
                case LEFT: return Binds.K_LEFT;
                case RIGHT: return Binds.K_RIGHT;
                case FIRE: return Binds.K_FIRE;
            }
        } catch (Exception e) { }
        return k;
    }

    /** fixed menu navigation that works whatever the user has bound */
    private static int nav(int c) {
        switch (c) {
            case Binds.K_UP: return Game.A_UP;
            case Binds.K_DOWN: return Game.A_DOWN;
            case Binds.K_LEFT: return Binds.A_LEFT;
            case Binds.K_RIGHT: return Binds.A_RIGHT;
            case Binds.K_FIRE: return Game.A_SELECT;
            case Binds.K_LSOFT: case Binds.K_CLEAR: return Binds.A_ESC;
            case '*': return Binds.A_TABL;
            case '#': return Binds.A_TABR;
        }
        return 0;
    }

    /** horizontal mode: what the key is in the turned picture */
    private int turn(int c, boolean digits) {
        switch (c) {
            case Binds.K_UP: return Binds.K_LEFT;
            case Binds.K_RIGHT: return Binds.K_UP;
            case Binds.K_DOWN: return Binds.K_RIGHT;
            case Binds.K_LEFT: return Binds.K_DOWN;
        }
        if (digits && c >= KEY_NUM1 && c <= KEY_NUM9) return "741852963".charAt(c - KEY_NUM1);   // 1->7 2->4 3->1 4->8 6->2 7->9 8->6 9->3
        return c;
    }

    protected void keyPressed(int k) {
        if (game == null) return;
        int c = canon(k);
        if (turned) { c = turn(c, game.turnDigits()); downAs.put(new Integer(k), new Integer(c)); }
        if (game.rawKey(c)) return;
        if (k >= KEY_NUM0 && k <= KEY_NUM9 && game.wantsDigits()) { game.digit(k - KEY_NUM0); return; }
        game.keyMask(game.bindMask(c) | nav(c), true);
    }

    protected void keyReleased(int k) {
        if (game == null) return;
        int c = canon(k);
        if (turned) { Object o = downAs.remove(new Integer(k)); c = o != null ? ((Integer) o).intValue() : turn(c, false); }
        game.keyMask(game.bindMask(c) | nav(c), false);
    }

    protected void keyRepeated(int k) { }

    void quit() { running = false; mid.exit(); }
}
