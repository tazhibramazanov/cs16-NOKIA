package cs;

/**
 * Push-to-talk voice (like a walkie-talkie): the clip is recorded while the key is held, then sent.
 * 8 kHz mono, IMA ADPCM (4 bits a sample, 4 KB a second). Every network part is coded on its own,
 * so a lost part does not spoil the rest.
 */
public final class Voice {
    public static final int RATE = 8000, MAX_SEC = 6, PART = 6000;   // samples in one network part (0.75 s, 3000 bytes)

    static final int[] STEP = {7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97,
            107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060,
            1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845,
            8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};
    static final int[] IDX = {-1, -1, -1, -1, 2, 4, 6, 8};

    /** samples s[off..off+n) -> (n+1)/2 bytes */
    public static byte[] encode(short[] s, int off, int n) {
        byte[] out = new byte[(n + 1) / 2];
        int pred = 0, idx = 0;
        for (int i = 0; i < n; i++) {
            int step = STEP[idx], diff = s[off + i] - pred, code = 0;
            if (diff < 0) { code = 8; diff = -diff; }
            int dq = step >> 3;
            if (diff >= step) { code |= 4; diff -= step; dq += step; }
            step >>= 1;
            if (diff >= step) { code |= 2; diff -= step; dq += step; }
            step >>= 1;
            if (diff >= step) { code |= 1; dq += step; }
            pred += (code & 8) != 0 ? -dq : dq;
            if (pred > 32767) pred = 32767; else if (pred < -32768) pred = -32768;
            idx += IDX[code & 7]; if (idx < 0) idx = 0; else if (idx > 88) idx = 88;
            if ((i & 1) == 0) out[i >> 1] = (byte) code; else out[i >> 1] |= (byte) (code << 4);
        }
        return out;
    }

    /** n samples from b[off..] into dst[at..] */
    public static void decode(byte[] b, int off, int n, short[] dst, int at) {
        int pred = 0, idx = 0;
        for (int i = 0; i < n; i++) {
            int code = (b[off + (i >> 1)] >> ((i & 1) * 4)) & 15;
            int step = STEP[idx], dq = step >> 3;
            if ((code & 4) != 0) dq += step;
            if ((code & 2) != 0) dq += step >> 1;
            if ((code & 1) != 0) dq += step >> 2;
            pred += (code & 8) != 0 ? -dq : dq;
            if (pred > 32767) pred = 32767; else if (pred < -32768) pred = -32768;
            idx += IDX[code & 7]; if (idx < 0) idx = 0; else if (idx > 88) idx = 88;
            dst[at + i] = (short) pred;
        }
    }

    /** any rate -> 8 kHz (linear) */
    public static short[] to8k(short[] s, int n, int rate) {
        if (rate == RATE) { if (n == s.length) return s; short[] o = new short[n]; System.arraycopy(s, 0, o, 0, n); return o; }
        int m = (int) ((long) n * RATE / rate);
        short[] o = new short[m];
        for (int i = 0; i < m; i++) {
            long p = (long) i * rate * 256 / RATE;
            int a = (int) (p >> 8), f = (int) (p & 255);
            int x = s[Math.min(a, n - 1)], y = s[Math.min(a + 1, n - 1)];
            o[i] = (short) (x + (((y - x) * f) >> 8));
        }
        return o;
    }

    /** phone microphones are quiet: remove the DC offset and bring the loudest part up to ~70% (at most x8) */
    public static void normalize(short[] s, int n) {
        if (n == 0) return;
        long sum = 0;
        for (int i = 0; i < n; i++) sum += s[i];
        int dc = (int) (sum / n), peak = 1;
        for (int i = 0; i < n; i++) { int v = s[i] - dc; if (v < 0) v = -v; if (v > peak) peak = v; }
        int gain = 23000 * 256 / peak;   // 8.8 fixed point
        if (gain > 8 * 256) gain = 8 * 256;
        for (int i = 0; i < n; i++) {
            int v = ((s[i] - dc) * gain) >> 8;
            s[i] = (short) (v > 32767 ? 32767 : v < -32768 ? -32768 : v);
        }
        // 15 ms fade in/out against clicks
        int f = Math.min(120, n / 2);
        for (int i = 0; i < f; i++) { s[i] = (short) (s[i] * i / f); s[n - 1 - i] = (short) (s[n - 1 - i] * i / f); }
    }

    /** a WAV file around 8 kHz mono 16-bit samples (the phone plays clips through MMAPI) */
    public static byte[] wav(short[] s, int n) {
        byte[] b = new byte[44 + n * 2];
        put(b, 0, 0x46464952); put(b, 4, 36 + n * 2); put(b, 8, 0x45564157); put(b, 12, 0x20746d66);
        put(b, 16, 16); b[20] = 1; b[22] = 1; put(b, 24, RATE); put(b, 28, RATE * 2); b[32] = 2; b[34] = 16;
        put(b, 36, 0x61746164); put(b, 40, n * 2);
        for (int i = 0; i < n; i++) { b[44 + i * 2] = (byte) s[i]; b[45 + i * 2] = (byte) (s[i] >> 8); }
        return b;
    }

    static void put(byte[] b, int o, int v) { b[o] = (byte) v; b[o + 1] = (byte) (v >> 8); b[o + 2] = (byte) (v >> 16); b[o + 3] = (byte) (v >> 24); }

    /** read a WAV (any PCM rate/bits/channels) into 8 kHz mono; null if it is not PCM WAV */
    public static short[] fromWav(byte[] b) {
        if (b == null || b.length < 44 || b[0] != 'R' || b[1] != 'I' || b[8] != 'W') return null;
        int p = 12, ch = 1, bits = 16, rate = RATE, fmt = 1;
        while (p + 8 <= b.length) {
            int len = (b[p + 4] & 255) | (b[p + 5] & 255) << 8 | (b[p + 6] & 255) << 16 | (b[p + 7] & 255) << 24;
            if (b[p] == 'f' && b[p + 1] == 'm' && b[p + 2] == 't') {
                fmt = (b[p + 8] & 255) | (b[p + 9] & 255) << 8;
                ch = (b[p + 10] & 255) | (b[p + 11] & 255) << 8;
                rate = (b[p + 12] & 255) | (b[p + 13] & 255) << 8 | (b[p + 14] & 255) << 16;
                bits = (b[p + 22] & 255) | (b[p + 23] & 255) << 8;
            } else if (b[p] == 'd' && b[p + 1] == 'a' && b[p + 2] == 't' && b[p + 3] == 'a') {
                if (fmt != 1 && fmt != 0xFFFE) return null;
                int avail = b.length - p - 8;
                if (len <= 0 || len > avail) len = avail;   // phones often leave the length at 0 while recording
                int bps = Math.max(1, bits / 8) * Math.max(1, ch), n = len / bps;
                short[] s = new short[n];
                for (int i = 0; i < n; i++) {
                    int o = p + 8 + i * bps;
                    s[i] = bits == 8 ? (short) (((b[o] & 255) - 128) << 8) : (short) ((b[o] & 255) | (b[o + 1] << 8));
                }
                return to8k(s, n, rate);
            }
            if (len < 0) return null;
            p += 8 + len + (len & 1);
        }
        return null;
    }
}
