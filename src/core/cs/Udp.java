package cs;

import java.io.IOException;

/** A UDP socket supplied by the platform (LAN discovery). */
public abstract class Udp {
    /** sender address of the last receive() */
    public String from;
    public abstract void send(byte[] b, int len, String host, int port) throws IOException;
    /** blocks until a datagram arrives; returns its length */
    public abstract int receive(byte[] buf) throws IOException;
    public abstract void close();
}
