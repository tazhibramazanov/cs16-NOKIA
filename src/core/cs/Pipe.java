package cs;

import java.io.*;

/** In-memory connection pair: the LAN host's own game talks to its own server without a socket. */
public final class Pipe {
    static final class Q {
        byte[] b = new byte[16384]; int r, w, n; boolean closed;
        synchronized void put(int v) throws IOException {
            if (closed) throw new IOException("closed");
            if (n == b.length) { byte[] nb = new byte[b.length * 2]; for (int i = 0; i < n; i++) nb[i] = b[(r + i) % b.length]; b = nb; r = 0; w = n; }
            b[w] = (byte) v; w = (w + 1) % b.length; n++; notifyAll();
        }
        synchronized int get() throws IOException {
            while (n == 0) { if (closed) return -1; try { wait(); } catch (InterruptedException e) { throw new IOException("interrupted"); } }
            int v = b[r] & 255; r = (r + 1) % b.length; n--; return v;
        }
        synchronized void close() { closed = true; notifyAll(); }
    }
    static Conn end(final Q qin, final Q qout) {
        Conn c = new Conn() { public void close() { qin.close(); qout.close(); } };
        c.in = new InputStream() { public int read() throws IOException { return qin.get(); } };
        c.out = new OutputStream() { public void write(int v) throws IOException { qout.put(v); } };
        return c;
    }
    public final Conn a, b;
    public Pipe() { Q x = new Q(), y = new Q(); a = end(x, y); b = end(y, x); }
}
