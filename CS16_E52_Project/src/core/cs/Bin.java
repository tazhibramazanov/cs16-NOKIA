package cs;

import java.io.*;

/** Whole-resource reader (CLDC has no BufferedInputStream; reading everything at once is also much faster on phones). */
public final class Bin {
    public byte[] b; public int p, n;

    public Bin(InputStream in) throws IOException {
        int av = 0; try { av = in.available(); } catch (Exception e) { }
        byte[] buf = new byte[av > 1024 ? av + 16 : 65536]; int len = 0;
        while (true) {
            if (len == buf.length) { byte[] nb = new byte[buf.length * 2]; System.arraycopy(buf, 0, nb, 0, len); buf = nb; }
            int r = in.read(buf, len, buf.length - len);
            if (r < 0) break;
            len += r;
        }
        in.close();
        b = buf; n = len;
    }

    public int readInt() { int v = ((b[p] & 255) << 24) | ((b[p + 1] & 255) << 16) | ((b[p + 2] & 255) << 8) | (b[p + 3] & 255); p += 4; return v; }
    public short readShort() { short v = (short) (((b[p] & 255) << 8) | (b[p + 1] & 255)); p += 2; return v; }
    public int readUnsignedShort() { int v = ((b[p] & 255) << 8) | (b[p + 1] & 255); p += 2; return v; }
    public byte readByte() { return b[p++]; }
    public int readUnsignedByte() { return b[p++] & 255; }
    public float readFloat() { return Float.intBitsToFloat(readInt()); }
    public void readFully(byte[] d) { System.arraycopy(b, p, d, 0, d.length); p += d.length; }
    public void skip(int k) { p += k; }

    public String readUTF() {
        int len = readUnsignedShort();
        StringBuffer sb = new StringBuffer(len);
        int end = p + len;
        while (p < end) {
            int c = b[p++] & 255;
            if (c < 0x80) sb.append((char) c);
            else if ((c & 0xE0) == 0xC0) { int c2 = b[p++] & 63; sb.append((char) (((c & 31) << 6) | c2)); }
            else { int c2 = b[p++] & 63, c3 = b[p++] & 63; sb.append((char) (((c & 15) << 12) | (c2 << 6) | c3)); }
        }
        return sb.toString();
    }

    public void close() { b = null; }
}
