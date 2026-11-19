package cs;

import java.io.*;
import java.util.Vector;

/**
 * Multiplayer link to the matchmaking/relay server.
 * Frame: u16 length (of type+payload), u8 type, payload.
 */
public final class Net implements Runnable {
    public static final int PROTO = 1;
    // client -> server
    public static final int C_JOIN = 1, C_TOHOST = 2, C_TOALL = 3, C_TOONE = 4, C_PING = 5;
    // server -> client
    public static final int S_WELCOME = 10, S_PEERJOIN = 11, S_PEERLEFT = 12, S_HOST = 13, S_ROOM = 14, S_PONG = 15, S_ERROR = 16,
            S_FROMCLIENT = 20, S_FROMHOST = 21;

    final String host; final int port;
    Conn conn; DataOutputStream out;
    final Vector inbox = new Vector();
    public volatile boolean connected, closed; public volatile String error;
    long lastSend, lastRecv;
    public int sentBytes, recvBytes;

    public Net(String host, int port) { this.host = host; this.port = port; new Thread(this).start(); }
    /** an already open connection (LAN host talking to its own server) */
    public Net(Conn c) { this.host = null; this.port = 0; conn = c; new Thread(this).start(); }

    public void run() {
        try {
            if (conn == null) conn = Sys.s.connect(host, port);
            out = new DataOutputStream(conn.out);
            DataInputStream in = new DataInputStream(conn.in);
            connected = true; lastRecv = Sys.s.millis();
            while (!closed) {
                int len = in.readUnsignedShort();
                if (len < 1 || len > 8192) throw new IOException("bad frame");
                byte[] b = new byte[len];
                in.readFully(b);
                recvBytes += len + 2; lastRecv = Sys.s.millis();
                synchronized (inbox) { inbox.addElement(b); }
            }
        } catch (Throwable t) {
            if (!closed) error = connected ? "Связь с сервером потеряна" : "Не удалось подключиться к серверу";
        }
        connected = false;
        if (conn != null) conn.close();
    }

    /** next received frame (type byte first) or null */
    public byte[] poll() {
        synchronized (inbox) {
            if (inbox.size() == 0) return null;
            byte[] b = (byte[]) inbox.elementAt(0); inbox.removeElementAt(0); return b;
        }
    }

    public void send(int type, byte[] payload, int len) {
        if (!connected || out == null) return;
        try {
            synchronized (this) {
                out.writeShort(len + 1); out.writeByte(type);
                if (len > 0) out.write(payload, 0, len);
                out.flush();
            }
            sentBytes += len + 3; lastSend = Sys.s.millis();
        } catch (IOException e) { error = "Связь с сервером потеряна"; close(); }
    }

    public void send(int type, Buf b) { send(type, b.b, b.n); }

    public void keepAlive() { if (connected && Sys.s.millis() - lastSend > 4000) send(C_PING, null, 0); }

    public void close() { closed = true; connected = false; if (conn != null) conn.close(); }

    /** growable byte writer */
    public static final class Buf {
        public byte[] b = new byte[512]; public int n;
        public Buf reset() { n = 0; return this; }
        void need(int k) { if (n + k > b.length) { byte[] nb = new byte[Math.max(b.length * 2, n + k)]; System.arraycopy(b, 0, nb, 0, n); b = nb; } }
        public Buf u8(int v) { need(1); b[n++] = (byte) v; return this; }
        public Buf u16(int v) { need(2); b[n++] = (byte) (v >> 8); b[n++] = (byte) v; return this; }
        public Buf i16(float v) { int i = (int) (v < 0 ? v - 0.5f : v + 0.5f); if (i > 32767) i = 32767; if (i < -32768) i = -32768; return u16(i); }
        public Buf i32(int v) { u16(v >>> 16); return u16(v & 0xFFFF); }
        public Buf str(String s) {
            if (s == null) s = "";
            try { byte[] u = s.getBytes("UTF-8"); int l = Math.min(u.length, 255); u8(l); need(l); System.arraycopy(u, 0, b, n, l); n += l; } catch (Exception e) { u8(0); }
            return this;
        }
    }

    /** byte reader */
    public static final class Rd {
        public byte[] b; public int p;
        public Rd(byte[] b, int p) { this.b = b; this.p = p; }
        public boolean more() { return p < b.length; }
        public int u8() { return p < b.length ? b[p++] & 255 : 0; }
        public int u16() { return (u8() << 8) | u8(); }
        public int i16() { return (short) u16(); }
        public int i32() { return (u16() << 16) | u16(); }
        public String str() {
            int l = u8(); if (p + l > b.length) l = b.length - p;
            String s; try { s = new String(b, p, l, "UTF-8"); } catch (Exception e) { s = ""; } p += l; return s;
        }
    }
}
