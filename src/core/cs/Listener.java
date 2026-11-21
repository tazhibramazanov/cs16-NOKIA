package cs;

import java.io.IOException;

/** A listening TCP socket supplied by the platform (LAN server). */
public abstract class Listener {
    public abstract Conn accept() throws IOException;
    public abstract void close();
    /** this device's address on the local network, or null */
    public String localIp;
}
