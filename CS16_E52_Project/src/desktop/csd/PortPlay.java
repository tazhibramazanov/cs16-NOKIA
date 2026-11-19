package cs;
import csd.DeskSys;
import java.io.*;
import java.awt.image.BufferedImage;
import javax.imageio.ImageIO;
/** in-game portrait render at different presets: PortPlay <res> <out> */
public class PortPlay {
    public static void main(String[] a) throws Exception {
        Sys.s = new DeskSys(a[0]); new File(a[1]).mkdirs();
        int W = 240, H = 320; int[] scr = new int[W * H];
        Game g = new Game(W, H, scr); g.init(); g.firstRunDone = true; g.menu = -1;
        g.startMatch(0, 2);
        for (int i = 0; i < 60; i++) { g.tick(0.05f); g.draw(); }
        String[] nm = {"gfx0_q1", "gfx1_q1", "gfx2_q1", "gfx1_q2", "gfx1_q3", "gfx1_auto", "gfx2_nfr20", "gfx3_max"};
        int[][] cf = {{0, 1, 0}, {1, 1, 0}, {2, 1, 0}, {1, 2, 0}, {1, 3, 0}, {1, 0, 0}, {2, 1, 3}, {3, 1, 0}};
        for (int k = 0; k < nm.length; k++) {
            g.nfr = cf[k][2]; g.setGfx(cf[k][0]); g.setQuality(cf[k][1]);
            long t0 = System.nanoTime(); int n = 0;
            for (int i = 0; i < 20; i++) { g.tick(0.05f); g.draw(); n++; }
            double ms = (System.nanoTime() - t0) / 1e6 / n;
            BufferedImage im = new BufferedImage(W, H, BufferedImage.TYPE_INT_RGB);
            im.setRGB(0, 0, W, H, scr, 0, W);
            ImageIO.write(im, "png", new File(a[1], nm[k] + ".png"));
            System.out.println(nm[k] + " " + ms + " ms  r=" + g.r.W + "x" + g.r.H);
        }
        // runtime switch to horizontal and back
        g.nfr = 0; g.setGfx(1); g.setQuality(1);
        int[] land = new int[320 * 240]; g.orient = 1; g.resize(320, 240, land);
        for (int i = 0; i < 10; i++) { g.tick(0.05f); g.draw(); }
        g.menu = 6; g.menuSel = 10; g.draw();
        BufferedImage im = new BufferedImage(320, 240, BufferedImage.TYPE_INT_RGB); im.setRGB(0, 0, 320, 240, land, 0, 320);
        ImageIO.write(im, "png", new File(a[1], "z_land_opts.png"));
        g.menu = -1; g.buyOpen = true; g.draw(); g.buyOpen = false;
        im.setRGB(0, 0, 320, 240, land, 0, 320); ImageIO.write(im, "png", new File(a[1], "z_land_buy.png"));
        g.orient = 0; g.resize(240, 320, scr); g.menu = 6; g.menuSel = 10; g.draw();
        im = new BufferedImage(W, H, BufferedImage.TYPE_INT_RGB); im.setRGB(0, 0, W, H, scr, 0, W);
        ImageIO.write(im, "png", new File(a[1], "z_back_port_opts.png"));
        g.nfr = 3; g.setGfx(2); g.menu = -1; g.showFps = true;
        for (int i = 0; i < 60; i++) { g.tick(0.05f); g.draw(); }
        im.setRGB(0, 0, W, H, scr, 0, W); ImageIO.write(im, "png", new File(a[1], "z_nfr_fps.png"));
    }
}
