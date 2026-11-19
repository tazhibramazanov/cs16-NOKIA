package csd;
import cs.*;
import java.io.*;
public class DeskSys extends Sys {
    String root; public boolean mute = true;
    public DeskSys(String root) { this.root = root; }
    public Listener listen(int port) throws IOException {
        final java.net.ServerSocket ss = new java.net.ServerSocket();
        ss.setReuseAddress(true); ss.bind(new java.net.InetSocketAddress(port));
        Listener l = new Listener() {
            public Conn accept() throws IOException {
                final java.net.Socket so = ss.accept(); so.setTcpNoDelay(true);
                Conn c = new Conn() { public void close() { try { so.close(); } catch (IOException e) { } } };
                c.in = new BufferedInputStream(so.getInputStream()); c.out = new BufferedOutputStream(so.getOutputStream());
                return c;
            }
            public void close() { try { ss.close(); } catch (IOException e) { } }
        };
        l.localIp = lanIp();
        return l;
    }

    public Udp udp(int port) throws IOException {
        final java.net.DatagramSocket ds = new java.net.DatagramSocket(null);
        ds.setReuseAddress(true); ds.setBroadcast(true);
        ds.bind(new java.net.InetSocketAddress(port));
        return new Udp() {
            public void send(byte[] b, int len, String host, int p) throws IOException {
                ds.send(new java.net.DatagramPacket(b, len, java.net.InetAddress.getByName(host), p));
            }
            public int receive(byte[] buf) throws IOException {
                java.net.DatagramPacket pk = new java.net.DatagramPacket(buf, buf.length);
                ds.receive(pk); from = pk.getAddress().getHostAddress(); return pk.getLength();
            }
            public void close() { ds.close(); }
        };
    }

    /** this PC's address on the home network (192.168.x.x / 10.x.x.x) */
    static String lanIp() {
        try {
            java.util.Enumeration<java.net.NetworkInterface> e = java.net.NetworkInterface.getNetworkInterfaces();
            String any = null;
            while (e.hasMoreElements()) {
                java.net.NetworkInterface ni = e.nextElement();
                if (!ni.isUp() || ni.isLoopback() || ni.isVirtual()) continue;
                java.util.Enumeration<java.net.InetAddress> a = ni.getInetAddresses();
                while (a.hasMoreElements()) {
                    java.net.InetAddress ad = a.nextElement();
                    if (ad instanceof java.net.Inet4Address && ad.isSiteLocalAddress()) {
                        String s = ad.getHostAddress();
                        if (s.startsWith("192.168.")) return s;
                        if (any == null) any = s;
                    }
                }
            }
            return any;
        } catch (Exception ex) { return null; }
    }

    public Conn connect(String host, int port) throws IOException {
        final java.net.Socket so = new java.net.Socket();
        so.connect(new java.net.InetSocketAddress(host, port), 10000); so.setTcpNoDelay(true);
        Conn c = new Conn() { public void close() { try { so.close(); } catch (IOException e) { } } };
        c.in = new BufferedInputStream(so.getInputStream()); c.out = new BufferedOutputStream(so.getOutputStream());
        return c;
    }
    public InputStream open(String name) {
        try {
            String n = name.startsWith("/") ? name.substring(1) : name;
            File f = new File(root, n);
            if (!f.exists()) f = new File(root + "/snd", n);
            return new BufferedInputStream(new FileInputStream(f), 65536);
        } catch (IOException e) { throw new RuntimeException(e.toString()); }
    }
    public int[] image(String name, int[] wh) {
        try {
            java.awt.image.BufferedImage im = javax.imageio.ImageIO.read(open(name));
            wh[0] = im.getWidth(); wh[1] = im.getHeight();
            return im.getRGB(0, 0, wh[0], wh[1], null, 0, wh[0]);
        } catch (Exception e) { return null; }
    }
    public int soundCount;
    public void sound(String name, int vol) { soundCount++; }
    public void quit() { System.exit(0); }
    byte[] saved;
    public void save(byte[] b) { saved = b; }
    public byte[] load() { return saved; }
}
