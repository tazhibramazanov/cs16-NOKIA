package cspc;

import cs.*;
import java.awt.*;
import java.awt.event.*;
import java.awt.image.*;
import java.io.*;
import java.util.*;
import javax.sound.sampled.*;
import javax.swing.*;

/** Windows/PC test build: same engine as the phone, 320x240 scaled window, keyboard+mouse or phone-style keys. */
public final class PCMain extends Sys implements KeyListener, MouseListener, MouseMotionListener, MouseWheelListener {
    int GW = 320, GH = 240;          // 320x240 horizontal, 240x320 vertical (setting «Экран», like on the phone)
    int[] screen = new int[GW * GH];
    int curOrient = 1;
    BufferedImage img, hiImg; int[] hiPix;
    Frame frame; Canvas canvas;
    Game game;
    volatile boolean running = true;
    boolean captured; Robot robot; Cursor blank;
    int curScale = -1; boolean fullscreen;
    final File cfgDir;

    public static void main(String[] a) {
        try { new PCMain().run(); } catch (Throwable t) {
            t.printStackTrace();
            JOptionPane.showMessageDialog(null, "Ошибка: " + t, "CS 1.6 E52 (PC)", JOptionPane.ERROR_MESSAGE);
            System.exit(1);
        }
    }

    PCMain() {
        String base = System.getenv("APPDATA");
        if (base == null) base = System.getProperty("user.home");
        cfgDir = new File(base, "CS16E52");
        cfgDir.mkdirs();
    }

    void run() throws Exception {
        Sys.s = this;
        makeImg();
        game = new Game(GW, GH, screen);
        game.pc = true;
        game.orient = 1;   // first start: horizontal (saved setting wins)
        frame = new Frame("Counter-Strike 1.6 — Nokia E52 Edition (PC тест)");
        canvas = new Canvas();
        canvas.setBackground(Color.black);
        frame.add(canvas);
        frame.addWindowListener(new WindowAdapter() { public void windowClosing(WindowEvent e) { quit(); } });
        canvas.addKeyListener(this); canvas.addMouseListener(this); canvas.addMouseMotionListener(this); canvas.addMouseWheelListener(this);
        canvas.setFocusTraversalKeysEnabled(false);
        frame.setFocusTraversalKeysEnabled(false);
        try { robot = new Robot(); } catch (Exception e) { }
        blank = Toolkit.getDefaultToolkit().createCustomCursor(new BufferedImage(16, 16, BufferedImage.TYPE_INT_ARGB), new Point(0, 0), "blank");
        try { frame.setIconImage(javax.imageio.ImageIO.read(open("/icon.png"))); } catch (Exception e) { }
        game.init();
        if (game.orient != curOrient) setOrient(game.orient);
        applyScale();
        frame.setVisible(true);
        canvas.createBufferStrategy(2);
        canvas.requestFocus();
        new Thread(new Runnable() { public void run() { preloadAll(); } }).start();
        long last = System.nanoTime();
        while (running) {
            long now = System.nanoTime();
            float dt = (now - last) / 1e9f; last = now;
            if (game.winScale != curScale) applyScale();
            synchronized (this) {
                if (game.orient != curOrient) { setOrient(game.orient); applyScale(); }
                game.tick(dt);
                game.draw();
            }
            if (captured && (game.state != 2 || game.wantsDigits() && !isOverlayMenu())) release();
            present();
            long target = 1000000000L / (game.fpsCap20 ? 20 : 60);
            // "как на E52" + Nokia FR: real frames keep the phone pace, generated frames cost about as much as on the phone
            if (game.nfr > 0 && game.lastGenerated) target = 10000000L;
            long spent = System.nanoTime() - now;
            if (spent < target) Thread.sleep((target - spent) / 1000000L);
        }
        System.exit(0);
    }

    boolean isOverlayMenu() { return false; }

    void makeImg() {
        DataBufferInt db = new DataBufferInt(screen, screen.length);
        img = new BufferedImage(new DirectColorModel(24, 0xFF0000, 0xFF00, 0xFF), Raster.createPackedRaster(db, GW, GH, GW, new int[]{0xFF0000, 0xFF00, 0xFF}, null), false, null);
        hiImg = null; hiPix = null;
    }

    /** «Экран»: 1 horizontal 320x240, 0 vertical 240x320 (the phone's picture) */
    void setOrient(int o) {
        curOrient = o;
        GW = o == 1 ? 320 : 240; GH = o == 1 ? 240 : 320;
        screen = new int[GW * GH];
        makeImg();
        game.resize(GW, GH, screen);
        curScale = -1;
    }

    void applyScale() {
        Rectangle scr = GraphicsEnvironment.getLocalGraphicsEnvironment().getMaximumWindowBounds();
        int fit = Math.max(1, Math.min((scr.width - 20) / GW, (scr.height - 60) / GH));
        if (game.winScale > fit) game.winScale = fit;
        curScale = game.winScale;
        if (fullscreen) return;
        canvas.setPreferredSize(new Dimension(GW * curScale, GH * curScale));
        frame.pack();
        frame.setLocationRelativeTo(null);
    }

    void present() {
        BufferStrategy bs = canvas.getBufferStrategy();
        if (bs == null) return;
        Graphics2D g = (Graphics2D) bs.getDrawGraphics();
        int w = canvas.getWidth(), h = canvas.getHeight();
        int s = Math.max(1, Math.min(w / GW, h / GH));
        float fs = Math.min(w / (float) GW, h / (float) GH);
        int dw = (int) (GW * fs), dh = (int) (GH * fs);
        g.setColor(Color.black); g.fillRect(0, 0, w, h);
        g.setRenderingHint(RenderingHints.KEY_INTERPOLATION, RenderingHints.VALUE_INTERPOLATION_NEAREST_NEIGHBOR);
        int[] hi = game.hiActive ? game.hiFrame() : null;
        if (hi != null && hi.length == GW * GH * 4) {
            // 640x480 3D view; pixels the HUD/menus touched come from the 320x240 frame
            if (hiImg == null) {
                hiPix = new int[GW * GH * 4];
                DataBufferInt db2 = new DataBufferInt(hiPix, hiPix.length);
                hiImg = new BufferedImage(new DirectColorModel(24, 0xFF0000, 0xFF00, 0xFF), Raster.createPackedRaster(db2, GW * 2, GH * 2, GW * 2, new int[]{0xFF0000, 0xFF00, 0xFF}, null), false, null);
            }
            int[] base = game.hiBase, out = hiPix;
            int W2 = GW * 2;
            for (int y = 0; y < GH * 2; y++) {
                int so = (y >> 1) * GW, o = y * W2;
                for (int x = 0; x < W2; x++) {
                    int i = so + (x >> 1), c = screen[i];
                    out[o + x] = c == base[i] ? hi[o + x] : c;
                }
            }
            g.setRenderingHint(RenderingHints.KEY_INTERPOLATION, RenderingHints.VALUE_INTERPOLATION_BILINEAR);
            g.drawImage(hiImg, (w - dw) / 2, (h - dh) / 2, dw, dh, null);
        } else g.drawImage(img, (w - dw) / 2, (h - dh) / 2, dw, dh, null);
        if (!captured && game.state == 2 && game.menu == -1 && game.pcMouse) {
            g.setColor(new Color(0, 0, 0, 160)); g.fillRect(0, h - 22, w, 22);
            g.setColor(Color.white); g.drawString("Щёлкните мышью в окне, чтобы управлять обзором (Esc — пауза)", 8, h - 7);
        }
        g.dispose();
        bs.show();
        Toolkit.getDefaultToolkit().sync();
    }

    // ------------------------------------------------------------------ input
    void capture() {
        if (!game.pcMouse || robot == null) return;
        captured = true; canvas.setCursor(blank); center();
    }
    void release() { captured = false; canvas.setCursor(Cursor.getDefaultCursor()); }
    void center() {
        Point p = canvas.getLocationOnScreen();
        Rectangle scr = frame.getGraphicsConfiguration().getBounds();
        int x0 = Math.max(p.x, scr.x), x1 = Math.min(p.x + canvas.getWidth(), scr.x + scr.width);
        int y0 = Math.max(p.y, scr.y), y1 = Math.min(p.y + canvas.getHeight(), scr.y + scr.height);
        warpX = (x0 + x1) / 2; warpY = (y0 + y1) / 2; warping = true;
        robot.mouseMove(warpX, warpY);
    }

    /** phone-emulation scheme: PC key -> phone key code */
    static int phoneCode(KeyEvent e) {
        int k = e.getKeyCode();
        if (k >= KeyEvent.VK_0 && k <= KeyEvent.VK_9) return '0' + (k - KeyEvent.VK_0);
        if (k >= KeyEvent.VK_NUMPAD0 && k <= KeyEvent.VK_NUMPAD9) return '0' + (k - KeyEvent.VK_NUMPAD0);
        switch (k) {
            case KeyEvent.VK_MULTIPLY: return '*';
            case KeyEvent.VK_DIVIDE: case KeyEvent.VK_NUMBER_SIGN: return '#';
            case KeyEvent.VK_UP: return Binds.K_UP;
            case KeyEvent.VK_DOWN: return Binds.K_DOWN;
            case KeyEvent.VK_LEFT: return Binds.K_LEFT;
            case KeyEvent.VK_RIGHT: return Binds.K_RIGHT;
            case KeyEvent.VK_ENTER: return Binds.K_FIRE;
            case KeyEvent.VK_F1: case KeyEvent.VK_ESCAPE: return Binds.K_LSOFT;
            case KeyEvent.VK_F2: case KeyEvent.VK_TAB: return Binds.K_RSOFT;
            case KeyEvent.VK_BACK_SPACE: return Binds.K_CLEAR;
        }
        return 0;
    }

    static int phoneNav(int c) {
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

    /** keyboard+mouse scheme: fixed menu navigation */
    static int pcNav(int k) {
        switch (k) {
            case KeyEvent.VK_UP: return Game.A_UP;
            case KeyEvent.VK_DOWN: return Game.A_DOWN;
            case KeyEvent.VK_LEFT: return Binds.A_LEFT;
            case KeyEvent.VK_RIGHT: return Binds.A_RIGHT;
            case KeyEvent.VK_ENTER: return Game.A_SELECT;
            case KeyEvent.VK_ESCAPE: case KeyEvent.VK_BACK_SPACE: return Binds.A_ESC;
            case KeyEvent.VK_PAGE_UP: return Binds.A_TABL;
            case KeyEvent.VK_PAGE_DOWN: case KeyEvent.VK_TAB: return Binds.A_TABR;
        }
        return 0;
    }

    int codeOf(KeyEvent e) { return game.pcMouse ? e.getKeyCode() : phoneCode(e); }

    int maskOf(int code) {
        if (code == 0) return 0;
        return game.bindMask(code) | (game.pcMouse ? pcNav(code) : phoneNav(code));
    }

    int digitOf(KeyEvent e) {
        int k = e.getKeyCode();
        if (k >= KeyEvent.VK_0 && k <= KeyEvent.VK_9) return k - KeyEvent.VK_0;
        if (k >= KeyEvent.VK_NUMPAD0 && k <= KeyEvent.VK_NUMPAD9) return k - KeyEvent.VK_NUMPAD0;
        return -1;
    }

    public String keyName(int c) {
        switch (c) {
            case Binds.M_L: return "ЛКМ";
            case Binds.M_R: return "ПКМ";
            case Binds.M_M: return "СКМ";
            case Binds.M_WUP: return "Колесо вверх";
            case Binds.M_WDN: return "Колесо вниз";
            case KeyEvent.VK_SPACE: return "Пробел";
            case KeyEvent.VK_CONTROL: return "Ctrl";
            case KeyEvent.VK_SHIFT: return "Shift";
            case KeyEvent.VK_ALT: return "Alt";
            case KeyEvent.VK_ESCAPE: return "Esc";
            case KeyEvent.VK_TAB: return "Tab";
            case KeyEvent.VK_ENTER: return "Enter";
            case KeyEvent.VK_UP: return "Стрелка вверх";
            case KeyEvent.VK_DOWN: return "Стрелка вниз";
            case KeyEvent.VK_LEFT: return "Стрелка влево";
            case KeyEvent.VK_RIGHT: return "Стрелка вправо";
        }
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) return String.valueOf((char) c);
        String t = KeyEvent.getKeyText(c);
        return t == null || t.length() == 0 || t.startsWith("Unknown") ? "Код " + c : t;
    }

    public void keyPressed(KeyEvent e) {
        synchronized (this) {
            if (e.getKeyCode() == KeyEvent.VK_F11 || (e.getKeyCode() == KeyEvent.VK_ENTER && e.isAltDown())) { toggleFullscreen(); return; }
            if (e.getKeyCode() == KeyEvent.VK_ESCAPE && captured) release();
            int code = codeOf(e);
            if (code != 0 && game.rawKey(code)) return;
            int d = digitOf(e);
            if (d >= 0 && game.wantsDigits()) { game.digit(d); return; }
            if (game.pcMouse && !game.wantsDigits() && !game.binds().has(code)) {
                char c = Character.toLowerCase(e.getKeyChar());
                if ((c >= '1' && c <= '5') || c == 'x' || c == 'c') { game.pcKey(c); return; }
            }
            int a = maskOf(code);
            if (a != 0) game.keyMask(a, true);
        }
    }

    public void keyReleased(KeyEvent e) {
        synchronized (this) { int a = maskOf(codeOf(e)); if (a != 0) game.keyMask(a, false); }
    }

    public void keyTyped(KeyEvent e) { }

    void toggleFullscreen() {
        fullscreen = !fullscreen;
        frame.dispose();
        frame.setUndecorated(fullscreen);
        if (fullscreen) frame.setExtendedState(Frame.MAXIMIZED_BOTH);
        else { frame.setExtendedState(Frame.NORMAL); curScale = -1; }
        frame.setVisible(true);
        if (!fullscreen) applyScale();
        canvas.createBufferStrategy(2);
        canvas.requestFocus();
    }

    static int mouseCode(MouseEvent e) {
        if (SwingUtilities.isLeftMouseButton(e)) return Binds.M_L;
        if (SwingUtilities.isRightMouseButton(e)) return Binds.M_R;
        if (SwingUtilities.isMiddleMouseButton(e)) return Binds.M_M;
        return 0;
    }

    public void mousePressed(MouseEvent e) {
        synchronized (this) {
            canvas.requestFocus();
            if (!game.pcMouse) return;
            int code = mouseCode(e);
            if (game.bindCapture >= 0 && code != 0 && game.rawKey(code)) return;
            if (!captured && game.state == 2 && game.menu == -1 && !game.wantsDigits()) { capture(); return; }
            if (game.wantsDigits()) { if (code == Binds.M_L) game.keyMask(Game.A_SELECT, true); return; }
            int a = game.bindMask(code);
            if (a != 0) game.keyMask(a, true);
        }
    }

    public void mouseReleased(MouseEvent e) {
        synchronized (this) {
            int code = mouseCode(e);
            int a = game.bindMask(code) | (code == Binds.M_L ? Game.A_SELECT : 0);
            if (a != 0) game.keyMask(a, false);
        }
    }

    int lastMX = -1, lastMY = -1; boolean warping;

    public void mouseMoved(MouseEvent e) {
        if (!captured) { lastMX = -1; return; }
        Point sp = e.getLocationOnScreen();
        if (warping) { if (Math.abs(sp.x - warpX) <= 2 && Math.abs(sp.y - warpY) <= 2) { warping = false; lastMX = sp.x; lastMY = sp.y; } return; }
        if (lastMX < 0) { lastMX = sp.x; lastMY = sp.y; return; }
        int dx = sp.x - lastMX, dy = sp.y - lastMY;
        lastMX = sp.x; lastMY = sp.y;
        if (dx != 0 || dy != 0) synchronized (this) {
            float k = 0.022f * (game.sens * 2.2f);
            game.look(dx * k, dy * k * game.sensV / Math.max(1, game.sens));
        }
        Rectangle scr = frame.getGraphicsConfiguration().getBounds();
        Point cp = canvas.getLocationOnScreen();
        int l = Math.max(cp.x, scr.x) + 40, r = Math.min(cp.x + canvas.getWidth(), scr.x + scr.width) - 40;
        int t = Math.max(cp.y, scr.y) + 40, b = Math.min(cp.y + canvas.getHeight(), scr.y + scr.height) - 40;
        if (sp.x < l || sp.x > r || sp.y < t || sp.y > b) center();
    }
    int warpX, warpY;

    public void mouseDragged(MouseEvent e) { mouseMoved(e); }
    public void mouseWheelMoved(MouseWheelEvent e) {
        synchronized (this) {
            if (!game.pcMouse) return;
            int code = e.getWheelRotation() > 0 ? Binds.M_WDN : Binds.M_WUP;
            if (game.bindCapture >= 0 && game.rawKey(code)) return;
            if (game.wantsDigits()) { game.keyMask(code == Binds.M_WDN ? Game.A_DOWN : Game.A_UP, true); return; }
            int a = game.bindMask(code);
            if (a != 0) { game.keyMask(a, true); game.keyMask(a, false); }
        }
    }
    public void mouseClicked(MouseEvent e) { }
    public void mouseEntered(MouseEvent e) { }
    public void mouseExited(MouseEvent e) { }

    // ------------------------------------------------------------------ Sys
    public InputStream open(String name) {
        InputStream in = PCMain.class.getResourceAsStream(name.startsWith("/") ? name : "/" + name);
        if (in == null) throw new RuntimeException("missing " + name);
        return new BufferedInputStream(in, 65536);
    }

    public int[] image(String name, int[] wh) {
        try {
            BufferedImage im = javax.imageio.ImageIO.read(open(name));
            wh[0] = im.getWidth(); wh[1] = im.getHeight();
            return im.getRGB(0, 0, wh[0], wh[1], null, 0, wh[0]);
        } catch (Exception e) { return null; }
    }

    final Hashtable clips = new Hashtable();
    final Hashtable wavs = new Hashtable();

    byte[] wav(String name) {
        byte[] b = (byte[]) wavs.get(name);
        if (b != null) return b;
        try {
            InputStream in = open("/" + name + ".wav");
            ByteArrayOutputStream bo = new ByteArrayOutputStream();
            byte[] buf = new byte[8192]; int r;
            while ((r = in.read(buf)) > 0) bo.write(buf, 0, r);
            in.close();
            b = bo.toByteArray();
        } catch (Exception e) { b = new byte[0]; }
        wavs.put(name, b);
        return b;
    }

    final java.util.HashSet clipErrLogged = new java.util.HashSet();
    Mixer decoder;   // only used to read .wav files for the fallback Clips

    /** fallback when the mixer has no device: Windows often keeps short 8-bit 11 kHz Clips silent,
     *  so each sound becomes 16-bit 44.1 kHz stereo with a little silence after it */
    Clip newClip(String name) {
        try {
            if (decoder == null) { final PCMain self = this; decoder = new Mixer(new Mixer.Loader() { public InputStream open(String n) { return self.open(n); } }, new File(cfgDir, "sound_log.txt")); }
            short[] d = decoder.load(name);
            if (d == null || d.length == 0) return null;
            int rate = ((Integer) decoder.rates.get(name)).intValue();
            int n = (int) ((long) d.length * 44100 / rate), pad = 44100 / 10;
            byte[] out = new byte[(n + pad) * 4];
            for (int i = 0; i < n; i++) {
                long p = (long) i * rate * 65536 / 44100;
                int idx = (int) (p >>> 16), f = (int) (p & 0xFFFF);
                int a = d[Math.min(idx, d.length - 1)], b = d[Math.min(idx + 1, d.length - 1)];
                int v = a + (((b - a) * f) >> 16);
                out[i * 4] = out[i * 4 + 2] = (byte) v; out[i * 4 + 1] = out[i * 4 + 3] = (byte) (v >> 8);
            }
            Clip c = AudioSystem.getClip();
            c.open(new AudioFormat(44100, 16, 2, true, false), out, 0, out.length);
            return c;
        } catch (Throwable e) { if (clipErrLogged.add(name)) logSound("clip " + name + ": " + e); return null; }
    }

    void logSound(String s) {
        try { FileWriter w = new FileWriter(new File(cfgDir, "sound_log.txt"), true); w.write(new java.util.Date() + "  " + s + "\r\n"); w.close(); } catch (Exception e) { }
    }

    void preloadAll() {
        try {
            String[] list = {"weapons_ak47_1", "weapons_m4a1_unsil_1", "weapons_m4a1_1", "weapons_usp1", "weapons_usp_unsil_1", "weapons_glock18_2", "weapons_deagle_1",
                    "player_pl_step1", "player_pl_step2", "player_bhit_flesh_1", "weapons_awp1", "weapons_knife_slash1"};
            for (int i = 0; i < list.length; i++) getClips(list[i]);
        } catch (Throwable t) { }
    }

    Vector getClips(String name) {
        synchronized (clips) {
            Vector v = (Vector) clips.get(name);
            if (v == null) { v = new Vector(); clips.put(name, v); }
            return v;
        }
    }

    final Vector queue = new Vector();
    Thread soundThread;

    Mixer mixer; long mixerTriedAt = -1; boolean mixerLogged;

    public void preloadSounds(String[] names) {
        Mixer m = mixer();
        if (m != null) for (int i = 0; i < names.length; i++) m.load(names[i]);
    }

    Mixer mixer() {
        long now = System.currentTimeMillis();
        if (mixer == null && (mixerTriedAt < 0 || now - mixerTriedAt > 3000)) {   // device busy or not ready yet: try again every 3 s
            mixerTriedAt = now;
            final PCMain self = this;
            Mixer m = new Mixer(new Mixer.Loader() { public InputStream open(String n) { return self.open(n); } }, mixerLogged ? null : new File(cfgDir, "sound_log.txt"));   // details only on the first try
            if (m.start()) { mixer = m; logSound("mixer started"); }
            else if (!mixerLogged) { mixerLogged = true; logSound("mixer failed (" + m.error + "), using clips for now"); }
        }
        return mixer;
    }

    public void sound(String name, int vol) {
        if (vol <= 0) return;
        Mixer mx = mixer();
        if (mx != null && !mx.alive) { logSound("mixer stopped, reopening"); mixer = null; mixerTriedAt = -1; mx = mixer(); }   // e.g. headphones unplugged
        if (mx != null) { mx.play(name, Math.min(100, vol)); return; }
        synchronized (queue) { if (queue.size() < 32) { queue.addElement(new Object[]{name, new Integer(vol)}); queue.notify(); } }
        if (soundThread == null) {
            soundThread = new Thread(new Runnable() { public void run() { soundLoop(); } });
            soundThread.setDaemon(true); soundThread.start();
        }
    }

    void soundLoop() {
        while (running) {
            Object[] it;
            synchronized (queue) {
                while (queue.isEmpty()) { try { queue.wait(); } catch (InterruptedException e) { } }
                it = (Object[]) queue.elementAt(0); queue.removeElementAt(0);
            }
            String name = (String) it[0]; int vol = ((Integer) it[1]).intValue();
            Vector v = getClips(name);
            Clip c = null;
            for (int i = 0; i < v.size(); i++) { Clip x = (Clip) v.elementAt(i); if (!x.isRunning()) { c = x; break; } }
            if (c == null) {
                if (v.size() < 4) { c = newClip(name); if (c != null) v.addElement(c); }
                else { c = (Clip) v.elementAt(0); v.removeElementAt(0); v.addElement(c); c.stop(); }
            }
            if (c == null) continue;
            try {
                FloatControl g = (FloatControl) c.getControl(FloatControl.Type.MASTER_GAIN);
                float db = (float) (20 * Math.log10(Math.max(0.01, vol / 100.0)));
                g.setValue(Math.max(g.getMinimum(), Math.min(g.getMaximum(), db)));
            } catch (Exception e) { }
            c.setFramePosition(0);
            c.start();
        }
    }

    public void stopSounds() {
        if (mixer != null) mixer.stopAll();
        synchronized (clips) {
            Enumeration e = clips.elements();
            while (e.hasMoreElements()) { Vector v = (Vector) e.nextElement(); for (int i = 0; i < v.size(); i++) ((Clip) v.elementAt(i)).stop(); }
        }
    }

    public Listener listen(int port) throws IOException {
        final java.net.ServerSocket ss = new java.net.ServerSocket();
        ss.setReuseAddress(true); ss.bind(new java.net.InetSocketAddress(port));
        Listener l = new Listener() {
            public Conn accept() throws IOException {
                final java.net.Socket so = ss.accept(); so.setTcpNoDelay(true);
                Conn c = new Conn() { public void close() { try { so.close(); } catch (IOException e) { } } };
                c.in = new BufferedInputStream(so.getInputStream()); c.out = new BufferedOutputStream(so.getOutputStream());
                return c;
            }
            public void close() { try { ss.close(); } catch (IOException e) { } }
        };
        l.localIp = lanIp();
        return l;
    }

    public Udp udp(int port) throws IOException {
        final java.net.DatagramSocket ds = new java.net.DatagramSocket(null);
        ds.setReuseAddress(true); ds.setBroadcast(true);
        ds.bind(new java.net.InetSocketAddress(port));
        return new Udp() {
            public void send(byte[] b, int len, String host, int p) throws IOException {
                ds.send(new java.net.DatagramPacket(b, len, java.net.InetAddress.getByName(host), p));
            }
            public int receive(byte[] buf) throws IOException {
                java.net.DatagramPacket pk = new java.net.DatagramPacket(buf, buf.length);
                ds.receive(pk); from = pk.getAddress().getHostAddress(); return pk.getLength();
            }
            public void close() { ds.close(); }
        };
    }

    /** this PC's address on the home network (192.168.x.x / 10.x.x.x) */
    static String lanIp() {
        try {
            java.util.Enumeration<java.net.NetworkInterface> e = java.net.NetworkInterface.getNetworkInterfaces();
            String any = null;
            while (e.hasMoreElements()) {
                java.net.NetworkInterface ni = e.nextElement();
                if (!ni.isUp() || ni.isLoopback() || ni.isVirtual()) continue;
                java.util.Enumeration<java.net.InetAddress> a = ni.getInetAddresses();
                while (a.hasMoreElements()) {
                    java.net.InetAddress ad = a.nextElement();
                    if (ad instanceof java.net.Inet4Address && ad.isSiteLocalAddress()) {
                        String s = ad.getHostAddress();
                        if (s.startsWith("192.168.")) return s;
                        if (any == null) any = s;
                    }
                }
            }
            return any;
        } catch (Exception ex) { return null; }
    }

    public Conn connect(String host, int port) throws IOException {
        final java.net.Socket so = new java.net.Socket();
        so.connect(new java.net.InetSocketAddress(host, port), 10000); so.setTcpNoDelay(true);
        Conn c = new Conn() { public void close() { try { so.close(); } catch (IOException e) { } } };
        c.in = new BufferedInputStream(so.getInputStream()); c.out = new BufferedOutputStream(so.getOutputStream());
        return c;
    }

    public void editText(final String title, final String init, final int maxLen, final TextCb cb) {
        SwingUtilities.invokeLater(new Runnable() { public void run() {
            Object r = JOptionPane.showInputDialog(frame, title, "CS 1.6 E52", JOptionPane.PLAIN_MESSAGE, null, null, init);
            String t = r == null ? null : r.toString();
            if (t != null && t.length() > maxLen) t = t.substring(0, maxLen);
            final String res = t;
            synchronized (game) { cb.done(res); }
            canvas.requestFocus();
        } });
    }

    public void quit() { running = false; game.saveSettings(); System.exit(0); }

    public void save(byte[] b) {
        try { FileOutputStream o = new FileOutputStream(new File(cfgDir, "settings.bin")); o.write(b); o.close(); } catch (Exception e) { }
    }

    public byte[] load() {
        try {
            File f = new File(cfgDir, "settings.bin");
            if (!f.exists()) return null;
            byte[] b = new byte[(int) f.length()];
            DataInputStream in = new DataInputStream(new FileInputStream(f)); in.readFully(b); in.close();
            return b;
        } catch (Exception e) { return null; }
    }
}
