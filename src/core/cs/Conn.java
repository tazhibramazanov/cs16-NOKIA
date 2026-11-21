package cs;

import java.io.*;

/** An open TCP connection supplied by the platform. */
public abstract class Conn {
    public InputStream in;
    public OutputStream out;
    public abstract void close();
}
