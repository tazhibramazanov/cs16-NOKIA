package cs;

/** the microphone clip is ready: 8 kHz mono 16-bit samples, or null with a reason */
public interface VoiceCb {
    void done(short[] pcm, String err);
}
