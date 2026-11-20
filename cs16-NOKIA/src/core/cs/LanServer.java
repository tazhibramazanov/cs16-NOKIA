package cs;

import java.io.*;
import java.util.Vector;

/**
 * The relay server running inside the game for "Создать игру по LAN": one room, the creator is the host.
 * Announces itself on the Wi-Fi network with UDP so others find it without typing an address.
 */
public final class LanServer implements Runnable {
    public static final int PORT = 27016, BEACON = 27017, MAX = 10;
    final int mode, map; final String name;
    Listener ls; Udp udp;
    final Vector clients = new Vector();
    int nextId = 1;
    volatile boolean closed;
    public String ip;

    final class Client implements Runnable {
        Conn c; DataOutputStream out; int id; String name = "?";
        Client(Conn c) { this.c = c; out = new DataOutputStream(c.out); }
        void send(int type, byte[] p, int off, int len) {
            try { synchronized (this) { out.writeShort(len + 1); out.writeByte(type); if (len > 0) out.write(p, off, len); out.flush(); } }
            catch (IOException e) { c.close(); }
        }
        public void run() {
            try {
                DataInputStream in = new DataInputStream(c.in);
                while (!closed) {
                    int len = in.readUnsignedShort();
                    if (len < 1 || len > 8192) break;
                    byte[] b = new byte[len]; in.readFully(b);
                    handle(this, b);
                }
            } catch (Throwable t) { }
            leave(this);
            c.close();
        }
    }

    public LanServer(int mode, int map, String name) { this.mode = mode; this.map = map; this.name = name; }

    /** opens the port; returns the in-game connection of the host (null + exception if the port is busy) */
    public Conn start() throws IOException {
        ls = Sys.s.listen(PORT);
        ip = ls.localIp;
        new Thread(this).start();
        new Thread(new Runnable() { public void run() { beacons(); } }).start();
        Pipe p = new Pipe();
        attach(p.b);
        return p.a;
    }

    public void run() {   // accept loop
        while (!closed) {
            try { Conn c = ls.accept(); if (closed) { c.close(); break; } attach(c); }
            catch (Throwable t) { if (closed) break; try { Thread.sleep(200); } catch (InterruptedException e) { } }
        }
    }

    void attach(Conn c) { Client cl = new Client(c); new Thread(cl).start(); }

    void beacons() {
        try { udp = Sys.s.udp(0); } catch (Throwable t) { return; }
        Net.Buf b = new Net.Buf();
        String bc = null;
        if (ip != null && ip.lastIndexOf('.') > 0) bc = ip.substring(0, ip.lastIndexOf('.')) + ".255";
        while (!closed) {
            int n; synchronized (this) { n = clients.size(); }
            b.reset().u8('C').u8('S').u8('1').u8('6').u8(Net.PROTO).u8(mode).u8(map).u8(n).str(name).u16(PORT);
            try { udp.send(b.b, b.n, "255.255.255.255", BEACON); } catch (Throwable t) { }
            if (bc != null) try { udp.send(b.b, b.n, bc, BEACON); } catch (Throwable t) { }
            try { Thread.sleep(1000); } catch (InterruptedException e) { }
        }
        udp.close();
    }

    synchronized Client host() { return clients.size() > 0 ? (Client) clients.elementAt(0) : null; }

    synchronized void handle(Client c, byte[] b) {
        int t = b[0] & 255;
        Client h = host();
        if (t == Net.C_PING) { c.send(Net.S_PONG, null, 0, 0); return; }
        if (t == Net.C_JOIN) {
            if (clients.contains(c)) return;
            if (clients.size() >= MAX) { err(c, "Комната заполнена"); return; }
            if (b.length < 2 || (b[1] & 255) != Net.PROTO) { err(c, "Нужна другая версия игры"); return; }
            Net.Rd r = new Net.Rd(b, 4); c.name = r.str(); if (c.name.length() == 0) c.name = "Игрок";
            c.id = nextId++;
            clients.addElement(c);
            h = host();
            Net.Buf w = new Net.Buf().u16(c.id).u8(h == c ? 1 : 0).u16(1).u8(clients.size()).u16(h.id);
            c.send(Net.S_WELCOME, w.b, 0, w.n);
            if (h != c) { Net.Buf j = new Net.Buf().u16(c.id).str(c.name); h.send(Net.S_PEERJOIN, j.b, 0, j.n); }
            roomInfo();
            return;
        }
        if (!clients.contains(c)) return;
        if (t == Net.C_TOHOST && h != c) {
            byte[] o = new byte[b.length + 1]; o[0] = (byte) (c.id >> 8); o[1] = (byte) c.id; System.arraycopy(b, 1, o, 2, b.length - 1);
            h.send(Net.S_FROMCLIENT, o, 0, o.length);
        } else if (t == Net.C_TOALL && h == c) {
            for (int i = 0; i < clients.size(); i++) { Client o = (Client) clients.elementAt(i); if (o != c) o.send(Net.S_FROMHOST, b, 1, b.length - 1); }
        } else if (t == Net.C_TOONE && h == c && b.length >= 3) {
            int id = ((b[1] & 255) << 8) | (b[2] & 255);
            for (int i = 0; i < clients.size(); i++) { Client o = (Client) clients.elementAt(i); if (o.id == id) o.send(Net.S_FROMHOST, b, 3, b.length - 3); }
        }
    }

    void err(Client c, String s) { Net.Buf e = new Net.Buf().str(s); c.send(Net.S_ERROR, e.b, 0, e.n); }

    void roomInfo() {
        Net.Buf w = new Net.Buf().u8(clients.size()).u16(clients.size());
        for (int i = 0; i < clients.size(); i++) ((Client) clients.elementAt(i)).send(Net.S_ROOM, w.b, 0, w.n);
    }

    synchronized void leave(Client c) {
        if (!clients.contains(c)) return;
        boolean wasHost = host() == c;
        clients.removeElement(c);
        if (clients.size() == 0) return;
        Client h = host();
        if (wasHost) { close(); return; }   // the host's device is the server: the game ends with it
        Net.Buf w = new Net.Buf().u16(c.id); h.send(Net.S_PEERLEFT, w.b, 0, w.n);
        roomInfo();
    }

    public void close() {
        closed = true;
        if (ls != null) ls.close();
        synchronized (this) { for (int i = 0; i < clients.size(); i++) ((Client) clients.elementAt(i)).c.close(); }
    }
}
