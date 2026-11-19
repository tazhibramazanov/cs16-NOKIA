package cs;
import csd.DeskSys;
import java.io.*;
import java.awt.image.BufferedImage;
import javax.imageio.ImageIO;
/** MPTest <res> <out> <name> <mode> <map> <secs> [W H] : joins the local server, autopilot plays */
public class MPTest {
    public static void main(String[] a) throws Exception {
        DeskSys sys = new DeskSys(a[0]); Sys.s = sys;
        String out = a[1], name = a[2]; int mode = Integer.parseInt(a[3]), map = Integer.parseInt(a[4]); float secs = Float.parseFloat(a[5]);
        int W = a.length > 6 ? Integer.parseInt(a[6]) : 320, H = a.length > 7 ? Integer.parseInt(a[7]) : 240;
        int[] scr = new int[W * H];
        Game g = new Game(W, H, scr); g.init(); g.firstRunDone = true; g.menu = 0;
        g.mpServer = "127.0.0.1:27016";
        if (System.getProperty("mqtt") != null) { MqttLink.brokers = new String[]{System.getProperty("mqtt")}; g.mpServer = ""; } String keep = g.mpServer; g.openMP(); g.mpServer = keep; g.mpName = name; g.mpMode = mode; g.mpMap = map;
        String role = a.length > 8 ? a[8] : "";
        if (role.equals("lanhost")) g.startLan();
        else if (role.equals("lanjoin")) {
            g.lanFinder = new LanFinder();
            LanFinder.Game[] l = new LanFinder.Game[0];
            for (int i = 0; i < 100 && l.length == 0; i++) { Thread.sleep(100); l = g.lanFinder.list(); }
            System.out.println(name + " LAN games found: " + l.length + (l.length > 0 ? " -> " + l[0].ip + ":" + l[0].port + " " + l[0].name + " mode " + l[0].mode + " map " + l[0].map : ""));
            if (l.length == 0) System.exit(1);
            g.closeFinder(); g.mpMode = l[0].mode; g.mpMap = l[0].map;
            g.mp = new MP(g, l[0].ip + ":" + l[0].port, l[0].mode, l[0].map, name, false); g.mp.lanGame = true; g.menu = Game.M_SEARCH;
        } else g.startSearch(role.equals("create"));
        float dt = 0.05f; int shot = 0; boolean pilot = false;
        for (float t = 0; t < secs; t += dt) {
            if (g.state == Game.ST_PLAY && g.me != null && g.mp != null && g.mp.phase == 2 && !pilot && g.mp.rosterOk | g.mp.host) {
                pilot = true;
                g.me.ai = new Bot(g, g.me); g.me.ai.newRound();
                System.out.println(name + " in game, host=" + g.mp.host + " me=" + g.me.name + " id=" + g.me.id + " team=" + g.me.team);
            }
            if (pilot && g.me.ai == null) { g.me.ai = new Bot(g, g.me); g.me.ai.newRound(); }

            if (role.equals("lanhost") && (int) (t / dt) == (int) (25 / dt)) { g.menu = Game.M_PAUSE; g.menuSel = 0; System.out.println(name + " opened PAUSE menu"); }
            g.tick(dt);
            if (g.state == Game.ST_PLAY) g.draw();
            int ti = (int) (t / dt);
            if (pilot && ti % 100 == 0) {
                StringBuilder sb = new StringBuilder(name + " t=" + (int) t + " r=" + g.round + " " + g.scoreT + ":" + g.scoreCT + " |");
                for (int i = 0; i < g.pl.length; i++) { Player p = g.pl[i]; sb.append(" " + p.name + (p.isBot ? "" : "*") + "[" + p.team + (p.alive ? "" : "x") + " " + (int) p.ox + "," + (int) p.oy + " k" + p.kills + " hp" + p.health + "]"); }
                System.out.println(sb);
                g.draw();
                BufferedImage im = new BufferedImage(W, H, BufferedImage.TYPE_INT_RGB); im.setRGB(0, 0, W, H, scr, 0, W);
                ImageIO.write(im, "png", new File(out + "_" + name + "_" + (shot++) + ".png"));
            }
            if (g.state == Game.ST_MENU && g.menu == Game.M_SEARCH && g.mp == null) { System.out.println(name + " FAILED: " + g.mpErr); break; }
            Thread.sleep(50);
        }
        System.out.println(name + " done. net sent " + (g.mp != null ? g.mp.net.sentBytes : -1) + " recv " + (g.mp != null ? g.mp.net.recvBytes : -1));
        System.exit(0);
    }
}
