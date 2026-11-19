package cspc;

import javax.sound.sampled.*;
import java.io.*;
import java.util.*;

/**
 * Software sound mixer for the PC build: all game sounds are mixed into ONE SourceDataLine
 * (44.1 kHz, 16-bit, stereo). This avoids javax.sound Clips, which on many Windows machines
 * stay silent for short 8-bit sounds or run out of DirectSound buffers.
 */
final class Mixer implements Runnable {
    interface Loader { InputStream open(String name); }

    static final int RATE = 44100, VOICES = 32, BLOCK = 512;
    final Loader loader;
    final File logFile;
    final Hashtable cache = new Hashtable();       // name -> short[] (mono, original rate) ; rate kept in rates
    final Hashtable rates = new Hashtable();
    final short[][] vData = new short[VOICES][];
    final int[] vPos = new int[VOICES], vStep = new int[VOICES], vVol = new int[VOICES];
    final String[] vName = new String[VOICES];
    SourceDataLine line;
    volatile boolean alive;
    String error;

    Mixer(Loader l, File log) { loader = l; logFile = log; }

    void log(String s) {
        try { FileWriter w = new FileWriter(logFile, true); w.write(new Date() + "  " + s + "\r\n"); w.close(); } catch (Exception e) { }
    }

    /** open the output line; false if no audio device could be opened */
    boolean start() {
        AudioFormat[] fmts = {
                new AudioFormat(RATE, 16, 2, true, false), new AudioFormat(48000, 16, 2, true, false), new AudioFormat(22050, 16, 2, true, false),
                new AudioFormat(RATE, 16, 1, true, false), new AudioFormat(22050, 8, 1, false, false)};
        javax.sound.sampled.Mixer.Info[] mi = new javax.sound.sampled.Mixer.Info[0];
        try { mi = AudioSystem.getMixerInfo(); } catch (Throwable e) { }
        for (int dev = -1; dev < mi.length; dev++)
        for (int i = 0; i < fmts.length; i++) {
            try {
                SourceDataLine l = dev < 0 ? AudioSystem.getSourceDataLine(fmts[i]) : AudioSystem.getSourceDataLine(fmts[i], mi[dev]);
                l.open(fmts[i], (int) fmts[i].getSampleRate() / 10 * fmts[i].getFrameSize());   // ~100 ms buffer
                l.start();
                line = l; fmt = fmts[i];
                alive = true;
                Thread t = new Thread(this, "mixer"); t.setDaemon(true); t.setPriority(Thread.MAX_PRIORITY - 1); t.start();
                log("audio OK: " + fmts[i] + (dev < 0 ? " (default device)" : " on " + mi[dev].getName()));
                return true;
            } catch (Throwable e) { error = e.toString(); if (dev < 0) log("format " + fmts[i] + " failed: " + e); }
        }
        return false;
    }

    AudioFormat fmt;

    short[] load(String name) {
        Object o = cache.get(name);
        if (o != null) return o instanceof short[] ? (short[]) o : null;
        short[] d = null; int rate = 11025;
        try {
            InputStream in = loader.open("/" + name + ".wav");
            if (in != null) {
                ByteArrayOutputStream bo = new ByteArrayOutputStream();
                byte[] buf = new byte[8192]; int r;
                while ((r = in.read(buf)) > 0) bo.write(buf, 0, r);
                in.close();
                byte[] b = bo.toByteArray();
                // RIFF parse
                int p = 12, ch = 1, bits = 8;
                while (p + 8 <= b.length) {
                    String id = new String(b, p, 4, "ISO-8859-1");
                    int len = (b[p + 4] & 255) | (b[p + 5] & 255) << 8 | (b[p + 6] & 255) << 16 | (b[p + 7] & 255) << 24;
                    if (id.equals("fmt ")) {
                        ch = (b[p + 10] & 255) | (b[p + 11] & 255) << 8;
                        rate = (b[p + 12] & 255) | (b[p + 13] & 255) << 8 | (b[p + 14] & 255) << 16;
                        bits = (b[p + 22] & 255) | (b[p + 23] & 255) << 8;
                    } else if (id.equals("data")) {
                        int n = Math.min(len, b.length - p - 8);
                        int bps = bits / 8 * ch, cnt = n / Math.max(1, bps);
                        d = new short[cnt];
                        for (int i = 0; i < cnt; i++) {
                            int o2 = p + 8 + i * bps;
                            d[i] = bits == 8 ? (short) (((b[o2] & 255) - 128) << 8) : (short) ((b[o2] & 255) | (b[o2 + 1] << 8));
                        }
                        break;
                    }
                    p += 8 + len + (len & 1);
                }
            }
        } catch (Exception e) { log("load " + name + ": " + e); }
        cache.put(name, d != null ? (Object) d : (Object) "x");
        if (d != null) rates.put(name, new Integer(rate));
        return d;
    }

    void play(String name, int vol) {
        short[] d = load(name);
        if (d == null || !alive) return;
        int rate = ((Integer) rates.get(name)).intValue();
        synchronized (this) {
            int slot = -1, oldest = -1, op = -1;
            for (int i = 0; i < VOICES; i++) {
                if (vData[i] == null) { slot = i; break; }
                if (vPos[i] > op) { op = vPos[i]; oldest = i; }
            }
            if (slot < 0) slot = oldest;
            vData[slot] = d; vPos[slot] = 0; vName[slot] = name;
            vStep[slot] = (int) ((long) rate * 65536 / (long) fmt.getSampleRate());
            vVol[slot] = vol * 190 / 100;
        }
    }

    void stopAll() { synchronized (this) { for (int i = 0; i < VOICES; i++) vData[i] = null; } }

    int[] mix = new int[BLOCK];

    /** mix one block of BLOCK frames into out (format fmt); returns bytes written */
    int mixBlock(byte[] out) {
        int ch = fmt.getChannels(), bits = fmt.getSampleSizeInBits();
        for (int i = 0; i < BLOCK; i++) mix[i] = 0;
        synchronized (this) {
            for (int v = 0; v < VOICES; v++) {
                short[] d = vData[v]; if (d == null) continue;
                int pos = vPos[v], st = vStep[v], vol = vVol[v], n = d.length;
                boolean done = false;
                for (int i = 0; i < BLOCK; i++) {
                    int idx = pos >>> 16;
                    if (idx >= n) { done = true; break; }
                    int a = d[idx], b = idx + 1 < n ? d[idx + 1] : a, f = pos & 0xFFFF;
                    mix[i] += ((a + (((b - a) * f) >> 16)) * vol) >> 8;
                    pos += st;
                }
                vPos[v] = pos;
                if (done) vData[v] = null;
            }
        }
        int o = 0;
        for (int i = 0; i < BLOCK; i++) {
            int s = mix[i];
            if (s > 32767) s = 32767; else if (s < -32768) s = -32768;
            if (bits == 16) { for (int c = 0; c < ch; c++) { out[o++] = (byte) s; out[o++] = (byte) (s >> 8); } }
            else out[o++] = (byte) ((s >> 8) + 128);
        }
        return o;
    }

    public void run() {
        byte[] out = new byte[BLOCK * fmt.getChannels() * (fmt.getSampleSizeInBits() / 8)];
        while (alive) {
            int o = mixBlock(out);
            try { line.write(out, 0, o); } catch (Throwable e) { log("write: " + e); alive = false; }
        }
    }
}
