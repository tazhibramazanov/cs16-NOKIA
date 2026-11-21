package cs;

import java.io.*;

/** Platform abstraction (MIDP phone / desktop test harness). */
public abstract class Sys {
    public static Sys s;
    public abstract InputStream open(String name);
    /** play sound by name (without .wav), volume 0..100 */
    public abstract void sound(String name, int vol);
    public long millis() { return System.currentTimeMillis(); }
    public void stopSounds() {}
    public abstract void quit();
    public void save(byte[] b) {}
    public byte[] load() { return null; }
    public void preloadSounds(String[] names) {}
    /** human-readable key name for a platform key code, or null for the phone's built-in names */
    public String keyName(int code) { return null; }
    /** decode a PNG resource to ARGB; wh receives width/height */
    public abstract int[] image(String name, int[] wh);
    /** open a TCP connection (multiplayer); null-safe default: no network */
    public Conn connect(String host, int port) throws IOException { throw new IOException("нет сети"); }
    /** listen for TCP connections (LAN server) */
    public Listener listen(int port) throws IOException { throw new IOException("нет сети"); }
    /** UDP socket: port > 0 binds it for receiving (LAN discovery), 0 = sending only */
    public Udp udp(int port) throws IOException { throw new IOException("нет сети"); }
    /** ask the user for a line of text (server address, nickname); cb gets null on cancel */
    public void editText(String title, String init, int maxLen, TextCb cb) { cb.done(null); }

    // ---- push-to-talk voice chat
    /** start recording the microphone; false if there is none */
    public boolean micStart() { return false; }
    /** stop recording; the clip (8 kHz mono) arrives later through cb, maybe on another thread */
    public void micStop(VoiceCb cb) { cb.done(null, "нет микрофона"); }
    /** play a received clip (8 kHz mono), vol 0..100 */
    public void playVoice(short[] pcm, int vol) { }
}
