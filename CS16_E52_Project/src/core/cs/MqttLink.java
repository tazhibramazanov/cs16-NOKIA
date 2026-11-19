package cs;

import java.io.*;
import java.util.Vector;

/**
 * Worldwide play without our own server: free public MQTT brokers (plain TCP, 24/7, no sign-up) carry the
 * traffic. This class pretends to be our relay server: the game talks to it through a Pipe with the same
 * frames (JOIN, TOHOST, TOALL, ...), and it turns them into MQTT topics.
 *   ads      cs16e52/v1/lobby/<mode>/<map>/<room>   retained: players, host name (cleared by the host's will)
 *   to host  cs16e52/v1/r/<room>/h                  'J' id name | 'D' id data | 'L' id (client's will)
 *   to one   cs16e52/v1/r/<room>/c/<id>             'W' | 'D' data
 *   to all   cs16e52/v1/r/<room>/a                  data
 */
public final class MqttLink implements Runnable {
    public static final String[] BROKERS = {"broker.emqx.io", "broker.hivemq.com", "test.mosquitto.org"};
    public static int PORT = 1883;
    public static String[] brokers = BROKERS;
    static final String P = "cs16e52/v1/";

    final Conn game;                 // our side of the pipe to the game
    DataOutputStream gout;
    Conn mq; DataOutputStream mout; InputStream min;
    final Vector inbox = new Vector();   // MQTT publishes: Object[]{topic, payload}
    volatile boolean closed;
    String room; int myId; boolean host; int players = 1; String hostName = "";
    int mode, map; boolean create;
    long lastHost, lastAd, lastPing;
    final java.util.Random rnd = new java.util.Random();
    public String broker;

    /** returns the connection the game (Net) should use */
    public static Conn open() {
        Pipe p = new Pipe();
        new MqttLink(p.b);
        return p.a;
    }

    MqttLink(Conn game) { this.game = game; gout = new DataOutputStream(game.out); new Thread(this).start(); }

    // ------------------------------------------------------------------ to the game
    void toGame(int type, Net.Buf b) {
        try { synchronized (gout) { gout.writeShort(b.n + 1); gout.writeByte(type); gout.write(b.b, 0, b.n); gout.flush(); } } catch (IOException e) { closed = true; }
    }
    void toGame(int type, byte[] d, int off, int len) {
        try { synchronized (gout) { gout.writeShort(len + 1); gout.writeByte(type); gout.write(d, off, len); gout.flush(); } } catch (IOException e) { closed = true; }
    }
    void fail(String s) { toGame(Net.S_ERROR, new Net.Buf().str(s)); }

    // ------------------------------------------------------------------ main
    public void run() {
        try {
            DataInputStream gin = new DataInputStream(game.in);
            // the first frame from the game is JOIN
            int len = gin.readUnsignedShort(); byte[] f = new byte[len]; gin.readFully(f);
            if (f[0] != Net.C_JOIN) { fail("bad join"); return; }
            Net.Rd r = new Net.Rd(f, 1);
            r.u8(); mode = r.u8(); map = r.u8(); hostName = r.str(); create = r.more() && r.u8() != 0;
            myId = 2 + (rnd.nextInt() & 0x7FFF);
            if (!create && find()) { /* joined */ }
            else startHost();
            if (closed) return;
            // pump: game frames -> MQTT in a second thread, MQTT -> game here
            final DataInputStream gi = gin;
            new Thread(new Runnable() { public void run() { pumpGame(gi); } }).start();
            pumpMqtt();
        } catch (Throwable t) {
            if (!closed) fail("Не удалось подключиться к интернет-серверу");
        }
        close();
    }

    /** look for an open room with the same mode and map; true if we joined one */
    boolean find() throws IOException {
        if (!connect(null, null, false)) { fail("Нет связи с интернет-сервером"); closed = true; return false; }
        subscribe(P + "lobby/" + mode + "/" + map + "/+");
        Vector rooms = new Vector();   // Object[]{room, players, liveFlag}
        long end = Sys.s.millis() + 4000;
        while (Sys.s.millis() < end && !closed) {
            Object[] m = next(200);
            if (m == null) continue;
            String t = (String) m[0]; byte[] pl = (byte[]) m[1]; boolean retained = ((Boolean) m[2]).booleanValue();
            if (!t.startsWith(P + "lobby/") || pl.length < 1) continue;
            String rm = t.substring(t.lastIndexOf('/') + 1);
            int n = pl[0] & 255;
            Object[] e = null;
            for (int i = 0; i < rooms.size(); i++) if (((Object[]) rooms.elementAt(i))[0].equals(rm)) e = (Object[]) rooms.elementAt(i);
            if (e == null) { e = new Object[]{rm, new Integer(n), Boolean.FALSE}; rooms.addElement(e); }
            e[1] = new Integer(n); if (!retained) e[2] = Boolean.TRUE;
        }
        disconnect();
        // live rooms first, the fullest first
        while (true) {
            Object[] best = null;
            for (int i = 0; i < rooms.size(); i++) {
                Object[] e = (Object[]) rooms.elementAt(i);
                int n = ((Integer) e[1]).intValue();
                if (!((Boolean) e[2]).booleanValue() || n >= 10) continue;
                if (best == null || n > ((Integer) best[1]).intValue()) best = e;
            }
            if (best == null) return false;
            rooms.removeElement(best);
            if (tryJoin((String) best[0])) return true;
        }
    }

    boolean tryJoin(String rm) throws IOException {
        room = rm; host = false;
        Net.Buf will = new Net.Buf().u8('L').u16(myId);
        if (!connect(P + "r/" + rm + "/h", copy(will), false)) return false;
        subscribe(P + "r/" + rm + "/c/" + myId);
        subscribe(P + "r/" + rm + "/a");
        publish(P + "r/" + rm + "/h", copy(new Net.Buf().u8('J').u16(myId).str(hostName)), false);
        long end = Sys.s.millis() + 6000;
        while (Sys.s.millis() < end && !closed) {
            Object[] m = next(200);
            if (m == null) continue;
            byte[] pl = (byte[]) m[1];
            if (((String) m[0]).endsWith("/c/" + myId) && pl.length > 0 && pl[0] == 'W') {
                lastHost = Sys.s.millis();
                toGame(Net.S_WELCOME, new Net.Buf().u16(myId).u8(0).u16(1).u8(pl.length > 1 ? pl[1] & 255 : 2).u16(1));
                return true;
            }
        }
        disconnect();
        return false;
    }

    void startHost() throws IOException {
        host = true; myId = 1;
        room = Integer.toString(rnd.nextInt() & 0x7FFFFFFF, 36);
        if (!connect(P + "lobby/" + mode + "/" + map + "/" + room, new byte[0], true)) { fail("Нет связи с интернет-сервером"); closed = true; return; }
        subscribe(P + "r/" + room + "/h");
        players = 1;
        advertise();
        toGame(Net.S_WELCOME, new Net.Buf().u16(myId).u8(1).u16(1).u8(1).u16(myId));
        toGame(Net.S_ROOM, new Net.Buf().u8(players).u16(players));
    }

    void advertise() throws IOException {
        publish(P + "lobby/" + mode + "/" + map + "/" + room, copy(new Net.Buf().u8(players).str(hostName)), true);
        lastAd = Sys.s.millis();
    }

    /** game -> MQTT */
    void pumpGame(DataInputStream gin) {
        try {
            while (!closed) {
                int len = gin.readUnsignedShort(); byte[] f = new byte[len]; gin.readFully(f);
                int t = f[0] & 255;
                if (t == Net.C_TOHOST && !host) {
                    byte[] o = new byte[len + 2]; o[0] = 'D'; o[1] = (byte) (myId >> 8); o[2] = (byte) myId; System.arraycopy(f, 1, o, 3, len - 1);
                    publish(P + "r/" + room + "/h", o, false);
                } else if (t == Net.C_TOALL && host) {
                    byte[] o = new byte[len - 1]; System.arraycopy(f, 1, o, 0, len - 1);
                    publish(P + "r/" + room + "/a", o, false);
                } else if (t == Net.C_TOONE && host && len >= 3) {
                    int id = ((f[1] & 255) << 8) | (f[2] & 255);
                    byte[] o = new byte[len - 2]; o[0] = 'D'; System.arraycopy(f, 3, o, 1, len - 3);
                    publish(P + "r/" + room + "/c/" + id, o, false);
                } else if (t == Net.C_PING) toGame(Net.S_PONG, new Net.Buf());
            }
        } catch (Throwable e) { }
        closed = true;
        close();
    }

    /** MQTT -> game */
    void pumpMqtt() throws IOException {
        while (!closed) {
            Object[] m = next(500);
            long now = Sys.s.millis();
            if (now - lastPing > 20000) { lastPing = now; synchronized (this) { mout.write(0xC0); mout.write(0); mout.flush(); } }
            if (host && now - lastAd > 3000) advertise();
            if (!host && now - lastHost > 12000) { fail("Ведущий игры вышел или пропала связь"); closed = true; return; }
            if (m == null) continue;
            String t = (String) m[0]; byte[] pl = (byte[]) m[1];
            if (host) {
                if (!t.endsWith("/h") || pl.length < 3) continue;
                int op = pl[0], id = ((pl[1] & 255) << 8) | (pl[2] & 255);
                if (op == 'J') {
                    if (players >= 10) continue;
                    players++;
                    Net.Rd r = new Net.Rd(pl, 3); String nm = r.str();
                    publish(P + "r/" + room + "/c/" + id, new byte[]{'W', (byte) players}, false);
                    toGame(Net.S_PEERJOIN, new Net.Buf().u16(id).str(nm));
                    toGame(Net.S_ROOM, new Net.Buf().u8(players).u16(players));
                    advertise();
                } else if (op == 'D') {
                    byte[] o = new byte[pl.length - 1]; o[0] = (byte) (id >> 8); o[1] = (byte) id; System.arraycopy(pl, 3, o, 2, pl.length - 3);
                    toGame(Net.S_FROMCLIENT, o, 0, o.length);
                } else if (op == 'L') {
                    if (players > 1) players--;
                    toGame(Net.S_PEERLEFT, new Net.Buf().u16(id));
                    toGame(Net.S_ROOM, new Net.Buf().u8(players).u16(players));
                    advertise();
                }
            } else {
                lastHost = now;
                if (t.endsWith("/a")) toGame(Net.S_FROMHOST, pl, 0, pl.length);
                else if (pl.length > 0 && pl[0] == 'D') toGame(Net.S_FROMHOST, pl, 1, pl.length - 1);
            }
        }
    }

    // ------------------------------------------------------------------ MQTT 3.1.1 (QoS 0)
    boolean connect(String willTopic, byte[] willMsg, boolean willRetain) {
        for (int i = 0; i < brokers.length && !closed; i++) {
            if (broker != null && !brokers[i].equals(broker)) continue;   // once one works, stay on it
            try {
                mq = Sys.s.connect(brokers[i], PORT);
                mout = new DataOutputStream(mq.out); min = mq.in;
                Net.Buf v = new Net.Buf();
                v.u16(4); v.u8('M').u8('Q').u8('T').u8('T'); v.u8(4);
                v.u8(0x02 | (willTopic != null ? 0x04 | (willRetain ? 0x20 : 0) : 0)); v.u16(60);
                ustr(v, "cs16e52_" + Integer.toString(rnd.nextInt() & 0x7FFFFFFF, 36));
                if (willTopic != null) { ustr(v, willTopic); v.u16(willMsg.length); for (int k = 0; k < willMsg.length; k++) v.u8(willMsg[k]); }
                packet(0x10, v.b, v.n);
                int h = min.read(); int rl = remLen(); byte[] ack = new byte[rl]; readFully(ack);
                if (h != 0x20 || rl < 2 || ack[1] != 0) throw new IOException("refused");
                broker = brokers[i];
                lastPing = Sys.s.millis();
                inbox.removeAllElements();
                final InputStream in = min; final Conn me = mq;
                new Thread(new Runnable() { public void run() { reader(in, me); } }).start();
                return true;
            } catch (Throwable t) { if (mq != null) mq.close(); mq = null; }
        }
        return false;
    }

    void reader(InputStream in, Conn c) {
        try {
            while (!closed && c == mq) {
                int h = in.read(); if (h < 0) break;
                int rl = 0, mul = 1, d;
                do { d = in.read(); if (d < 0) throw new IOException(); rl += (d & 127) * mul; mul *= 128; } while ((d & 128) != 0);
                byte[] b = new byte[rl]; int got = 0; while (got < rl) { int k = in.read(b, got, rl - got); if (k < 0) throw new IOException(); got += k; }
                if ((h & 0xF0) == 0x30) {
                    int tl = ((b[0] & 255) << 8) | (b[1] & 255);
                    String t = new String(b, 2, tl, "UTF-8");
                    int off = 2 + tl + (((h >> 1) & 3) > 0 ? 2 : 0);
                    byte[] p = new byte[rl - off]; System.arraycopy(b, off, p, 0, p.length);
                    synchronized (inbox) { inbox.addElement(new Object[]{t, p, (h & 1) != 0 ? Boolean.TRUE : Boolean.FALSE}); inbox.notifyAll(); }
                }
            }
        } catch (Throwable t) { }
        if (c == mq && !closed) { synchronized (inbox) { inbox.addElement(new Object[]{"", null, Boolean.FALSE}); inbox.notifyAll(); } }
    }

    Object[] next(long waitMs) throws IOException {
        synchronized (inbox) {
            if (inbox.size() == 0) { try { inbox.wait(waitMs); } catch (InterruptedException e) { } }
            if (inbox.size() == 0) return null;
            Object[] m = (Object[]) inbox.elementAt(0); inbox.removeElementAt(0);
            if (m[1] == null) throw new IOException("broker connection lost");
            return m;
        }
    }

    void subscribe(String topic) throws IOException {
        Net.Buf v = new Net.Buf(); v.u16(1); ustr(v, topic); v.u8(0);
        packet(0x82, v.b, v.n);
    }

    void publish(String topic, byte[] data, boolean retain) throws IOException {
        Net.Buf v = new Net.Buf(); ustr(v, topic);
        byte[] all = new byte[v.n + data.length]; System.arraycopy(v.b, 0, all, 0, v.n); System.arraycopy(data, 0, all, v.n, data.length);
        packet(0x30 | (retain ? 1 : 0), all, all.length);
    }

    void disconnect() {
        Conn c = mq; mq = null;
        if (c == null) return;
        try { synchronized (this) { mout.write(0xE0); mout.write(0); mout.flush(); } } catch (Throwable t) { }
        c.close();
    }

    synchronized void packet(int type, byte[] b, int n) throws IOException {
        if (mout == null) throw new IOException("no broker");
        mout.write(type);
        int x = n; do { int d = x % 128; x /= 128; if (x > 0) d |= 128; mout.write(d); } while (x > 0);
        mout.write(b, 0, n); mout.flush();
    }

    static void ustr(Net.Buf v, String s) {
        try { byte[] u = s.getBytes("UTF-8"); v.u16(u.length); for (int i = 0; i < u.length; i++) v.u8(u[i]); } catch (Exception e) { v.u16(0); }
    }
    static byte[] copy(Net.Buf b) { byte[] o = new byte[b.n]; System.arraycopy(b.b, 0, o, 0, b.n); return o; }
    int remLen() throws IOException { int rl = 0, mul = 1, d; do { d = min.read(); if (d < 0) throw new IOException(); rl += (d & 127) * mul; mul *= 128; } while ((d & 128) != 0); return rl; }
    void readFully(byte[] b) throws IOException { int got = 0; while (got < b.length) { int k = min.read(b, got, b.length - got); if (k < 0) throw new IOException(); got += k; } }

    void close() {
        closed = true;
        if (host && mq != null) try { publish(P + "lobby/" + mode + "/" + map + "/" + room, new byte[0], true); } catch (Throwable t) { }   // remove our ad
        if (!host && mq != null && room != null) try { publish(P + "r/" + room + "/h", new byte[]{'L', (byte) (myId >> 8), (byte) myId}, false); } catch (Throwable t) { }
        disconnect();
        game.close();
    }
}
