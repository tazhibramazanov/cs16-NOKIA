package cs.midp;

import javax.microedition.lcdui.*;
import cs.*;

/** Full-screen canvas: runs the game loop and maps the Nokia E52 keypad to game actions. */
public final class GameCanvas extends Canvas implements Runnable {
    static final int GW = 240, GH = 320;   // portrait: the E52 screen is 240x320
    private final CSMidlet mid;
    private int[] screen = new int[GW * GH];   // the game's frame: 240x320, or 320x240 in horizontal mode
    private int[] rotBuf;                       // horizontal mode: the frame turned onto the 240x320 screen
    private volatile int[] shown;               // what paint() draws (always 240x320)
    private int orient;                         // orientation the buffers are set up for
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
            game = new Game(GW, GH, screen);
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
                if (game.orient != orient) setOrient(game.orient);
                game.tick(dt);
                if (paused) { try { Thread.sleep(30); } catch (InterruptedException e) { } continue; }   // hidden: keep the match going, skip drawing
                game.draw();
                if (orient == 1) { rotate(); shown = rotBuf; } else shown = screen;
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

    /** horizontal: phone turned left (keypad on the right) - game top is the right edge of the screen */
    private void setOrient(int o) {
        orient = o;
        if (o == 1) { screen = new int[GH * GW]; if (rotBuf == null) rotBuf = new int[GW * GH]; game.resize(GH, GW, screen); }
        else { screen = new int[GW * GH]; rotBuf = null; game.resize(GW, GH, screen); }
        downAs.clear();
    }

    private void rotate() {
        int[] in = screen, out = rotBuf;
        int w = GH, h = GW;   // game frame 320x240
        for (int y = 0; y < h; y++) {
            int i = y * w, o = GW - 1 - y;
            for (int x = 0; x < w; x++, o += GW) out[o] = in[i++];
        }
    }

    protected void paint(Graphics g) {
        int w = getWidth(), h = getHeight();
        ox = (w - GW) / 2; oy = (h - GH) / 2;
        if (ox != 0 || oy != 0) { g.setColor(0); g.fillRect(0, 0, w, h); }
        int sx = ox < 0 ? -ox : 0, sy = oy < 0 ? -oy : 0;
        int dw = GW - sx, dh = GH - sy;
        if (dw > w) dw = w;
        if (dh > h) dh = h;
        int[] buf = shown;
        if (buf != null && dw > 0 && dh > 0)
            g.drawRGB(buf, sy * GW + sx, GW, ox + sx, oy + sy, dw, dh, false);
        if (error != null) {
            g.setColor(0xFF4040);
            g.setFont(Font.getFont(Font.FACE_SYSTEM, Font.STYLE_PLAIN, Font.SIZE_SMALL));
            g.drawString(error, 2, h - 2, Graphics.LEFT | Graphics.BOTTOM);
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
        if (orient == 1) { c = turn(c, game.turnDigits()); downAs.put(new Integer(k), new Integer(c)); }
        if (game.rawKey(c)) return;
        if (k >= KEY_NUM0 && k <= KEY_NUM9 && game.wantsDigits()) { game.digit(k - KEY_NUM0); return; }
        game.keyMask(game.bindMask(c) | nav(c), true);
    }

    protected void keyReleased(int k) {
        if (game == null) return;
        int c = canon(k);
        if (orient == 1) { Object o = downAs.remove(new Integer(k)); c = o != null ? ((Integer) o).intValue() : turn(c, false); }
        game.keyMask(game.bindMask(c) | nav(c), false);
    }

    protected void keyRepeated(int k) { }

    void quit() { running = false; mid.exit(); }
}
