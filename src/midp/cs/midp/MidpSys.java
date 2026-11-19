package cs.midp;

import java.io.*;
import java.util.*;
import javax.microedition.media.*;
import javax.microedition.media.control.VolumeControl;
import javax.microedition.rms.*;
import cs.Sys;
import cs.Conn;
import cs.TextCb;
import cs.Listener;
import cs.Udp;
import javax.microedition.io.*;
import javax.microedition.lcdui.*;

/** MIDP platform: resources from the JAR, MMAPI sound on a background thread, RMS settings. */
public final class MidpSys extends Sys implements Runnable {
    private final CSMidlet mid;
    private final Hashtable players = new Hashtable();
    private final Vector lru = new Vector();
    private static final int MAX_PLAYERS = 14;
    private final String[] qName = new String[16];
    private final int[] qVol = new int[16];
    private int qHead, qTail;
    private volatile boolean alive = true;
    private final Thread thread;
    private final Hashtable lastPlay = new Hashtable();
    private boolean soundBroken;

    MidpSys(CSMidlet m) {
        mid = m;
        thread = new Thread(this);
        thread.start();
    }

    /** TCP socket (the phone asks once for permission to use the network) */
    public Conn connect(String host, int port) throws IOException {
        final SocketConnection sc = (SocketConnection) Connector.open("socket://" + host + ":" + port, Connector.READ_WRITE, true);
        try { sc.setSocketOption(SocketConnection.DELAY, 0); } catch (Exception e) { }
        Conn c = new Conn() { public void close() { try { in.close(); } catch (Exception e) { } try { out.close(); } catch (Exception e) { } try { sc.close(); } catch (Exception e) { } } };
        c.in = sc.openInputStream(); c.out = sc.openOutputStream();
        return c;
    }

    /** LAN server socket (S60 supports ServerSocketConnection) */
    public Listener listen(int port) throws IOException {
        final ServerSocketConnection ssc = (ServerSocketConnection) Connector.open("socket://:" + port);
        Listener l = new Listener() {
            public Conn accept() throws IOException {
                final SocketConnection sc = (SocketConnection) ssc.acceptAndOpen();
                try { sc.setSocketOption(SocketConnection.DELAY, 0); } catch (Exception e) { }
                Conn c = new Conn() { public void close() { try { in.close(); } catch (Exception e) { } try { out.close(); } catch (Exception e) { } try { sc.close(); } catch (Exception e) { } } };
                c.in = sc.openInputStream(); c.out = sc.openOutputStream();
                return c;
            }
            public void close() { try { ssc.close(); } catch (Exception e) { } }
        };
        try { l.localIp = ssc.getLocalAddress(); } catch (Exception e) { }
        return l;
    }

    /** UDP for finding LAN games */
    public Udp udp(int port) throws IOException {
        final UDPDatagramConnection dc = (UDPDatagramConnection) Connector.open(port > 0 ? "datagram://:" + port : "datagram://");
        return new Udp() {
            public void send(byte[] b, int len, String host, int p) throws IOException {
                Datagram dg = dc.newDatagram(b, len, "datagram://" + host + ":" + p);
                dc.send(dg);
            }
            public int receive(byte[] buf) throws IOException {
                Datagram dg = dc.newDatagram(buf, buf.length);
                dc.receive(dg);
                String a = dg.getAddress();   // datagram://1.2.3.4:5678
                if (a != null) { if (a.startsWith("datagram://")) a = a.substring(11); int c = a.lastIndexOf(':'); if (c > 0) a = a.substring(0, c); }
                from = a;
                return dg.getLength();
            }
            public void close() { try { dc.close(); } catch (Exception e) { } }
        };
    }

    /** native text input screen; back to the game afterwards */
    public void editText(String title, String init, int maxLen, final TextCb cb) {
        final Display d = Display.getDisplay(mid);
        final Displayable back = d.getCurrent();
        final TextBox tb = new TextBox(title, init != null && init.length() <= maxLen ? init : "", maxLen, TextField.ANY);
        final Command ok = new Command("OK", Command.OK, 1), cancel = new Command("Отмена", Command.BACK, 2);
        tb.addCommand(ok); tb.addCommand(cancel);
        tb.setCommandListener(new CommandListener() { public void commandAction(Command c, Displayable s) {
            d.setCurrent(back);
            cb.done(c == ok ? tb.getString() : null);
        } });
        d.setCurrent(tb);
    }

    public InputStream open(String name) {
        InputStream in = getClass().getResourceAsStream(name);
        if (in == null) in = getClass().getResourceAsStream(name.startsWith("/") ? name : "/" + name);
        if (in == null) throw new RuntimeException("missing " + name);
        return in;
    }

    public void sound(String name, int vol) {
        if (soundBroken || vol <= 0) return;
        synchronized (qName) {
            int next = (qTail + 1) & 15;
            if (next == qHead) return; // queue full: drop
            qName[qTail] = name; qVol[qTail] = vol; qTail = next;
            qName.notify();
        }
    }

    public void preloadSounds(String[] names) {
        for (int i = 0; i < names.length; i++) sound(names[i], 0);
    }

    public void stopSounds() {
        synchronized (qName) { qHead = qTail; }
        Enumeration e = players.elements();
        while (e.hasMoreElements()) {
            try { ((Player) e.nextElement()).stop(); } catch (Throwable t) { }
        }
    }

    void shutdown() {
        alive = false;
        synchronized (qName) { qName.notify(); }
        Enumeration e = players.elements();
        while (e.hasMoreElements()) { try { ((Player) e.nextElement()).close(); } catch (Throwable t) { } }
        players.clear();
    }

    public void run() {
        while (alive) {
            String n; int v;
            synchronized (qName) {
                while (alive && qHead == qTail) { try { qName.wait(); } catch (InterruptedException e) { } }
                if (!alive) return;
                n = qName[qHead]; v = qVol[qHead]; qHead = (qHead + 1) & 15;
            }
            play(n, v);
        }
    }

    private Player get(String name) throws Exception {
        Player p = (Player) players.get(name);
        if (p != null) { lru.removeElement(name); lru.addElement(name); return p; }
        if (players.size() >= MAX_PLAYERS && lru.size() > 0) {
            String old = (String) lru.elementAt(0); lru.removeElementAt(0);
            Player op = (Player) players.remove(old);
            try { op.close(); } catch (Throwable t) { }
        }
        InputStream in = open("/" + name + ".wav");
        p = Manager.createPlayer(in, "audio/x-wav");
        p.realize();
        p.prefetch();
        players.put(name, p); lru.addElement(name);
        return p;
    }

    private void play(String name, int vol) {
        try {
            Player p = get(name);
            if (vol <= 0) return; // preload only
            Long lt = (Long) lastPlay.get(name);
            long now = System.currentTimeMillis();
            if (lt != null && now - lt.longValue() < 60) return;
            lastPlay.put(name, new Long(now));
            if (p.getState() == Player.STARTED) { try { p.stop(); } catch (Throwable t) { } }
            try { p.setMediaTime(0); } catch (Throwable t) { }
            VolumeControl vc = (VolumeControl) p.getControl("VolumeControl");
            if (vc != null) vc.setLevel(vol > 100 ? 100 : vol);
            p.start();
        } catch (SecurityException e) {
            soundBroken = true;
        } catch (Throwable t) {
            // ignore single failures
        }
    }

    public int[] image(String name, int[] wh) {
        try {
            javax.microedition.lcdui.Image im = javax.microedition.lcdui.Image.createImage(open(name));
            wh[0] = im.getWidth(); wh[1] = im.getHeight();
            int[] px = new int[wh[0] * wh[1]];
            im.getRGB(px, 0, wh[0], 0, 0, wh[0], wh[1]);
            return px;
        } catch (Throwable t) { return null; }
    }

    public void quit() {
        shutdown();
        mid.exit();
    }

    public void save(byte[] b) {
        try {
            RecordStore rs = RecordStore.openRecordStore("cs16e52", true);
            if (rs.getNumRecords() == 0) rs.addRecord(b, 0, b.length); else rs.setRecord(1, b, 0, b.length);
            rs.closeRecordStore();
        } catch (Throwable t) { }
    }

    public byte[] load() {
        try {
            RecordStore rs = RecordStore.openRecordStore("cs16e52", true);
            byte[] b = rs.getNumRecords() > 0 ? rs.getRecord(1) : null;
            rs.closeRecordStore();
            return b;
        } catch (Throwable t) { return null; }
    }
}
