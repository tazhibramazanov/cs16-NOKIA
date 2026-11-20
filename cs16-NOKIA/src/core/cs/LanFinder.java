package cs;

import java.util.Vector;

/** Listens for LAN game announcements (UDP broadcasts from LanServer). */
public final class LanFinder implements Runnable {
    public static final class Game { public String ip, name; public int mode, map, players, port; public long seen; }
    public final Vector games = new Vector();
    Udp udp; volatile boolean closed; public volatile String error;

    public LanFinder() { new Thread(this).start(); }

    public void run() {
        try { udp = Sys.s.udp(LanServer.BEACON); } catch (Throwable t) { error = "Wi-Fi недоступен"; return; }
        byte[] buf = new byte[300];
        while (!closed) {
            try {
                int n = udp.receive(buf);
                if (n < 9 || buf[0] != 'C' || buf[1] != 'S' || buf[2] != '1' || buf[3] != '6' || (buf[4] & 255) != Net.PROTO) continue;
                byte[] b = new byte[n]; System.arraycopy(buf, 0, b, 0, n);
                Net.Rd r = new Net.Rd(b, 5);
                int mode = r.u8(), map = r.u8(), pl = r.u8(); String nm = r.str(); int port = r.u16();
                String ip = udp.from;
                if (ip == null) continue;
                synchronized (games) {
                    Game g = null;
                    for (int i = 0; i < games.size(); i++) { Game x = (Game) games.elementAt(i); if (x.ip.equals(ip) && x.port == port) g = x; }
                    if (g == null) { g = new Game(); g.ip = ip; g.port = port; games.addElement(g); }
                    g.mode = mode; g.map = map; g.players = pl; g.name = nm; g.seen = Sys.s.millis();
                }
            } catch (Throwable t) { if (!closed) { try { Thread.sleep(300); } catch (InterruptedException e) { } } }
        }
    }

    /** games seen in the last 4 s */
    public Game[] list() {
        synchronized (games) {
            long now = Sys.s.millis();
            for (int i = games.size() - 1; i >= 0; i--) if (now - ((Game) games.elementAt(i)).seen > 4000) games.removeElementAt(i);
            Game[] a = new Game[games.size()]; games.copyInto(a); return a;
        }
    }

    public void close() { closed = true; if (udp != null) udp.close(); }
}
