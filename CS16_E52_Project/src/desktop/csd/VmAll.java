package cs;
import csd.DeskSys;
import java.io.*;
import java.awt.image.BufferedImage;
import javax.imageio.ImageIO;
/** every weapon in hands, in-game path: VmAll <res> <out.png> */
public class VmAll {
    public static void main(String[] a) throws Exception {
        Sys.s = new DeskSys(a[0]);
        int W = 320, H = 240; int[] scr = new int[W * H];
        Game g = new Game(W, H, scr); g.init(); g.firstRunDone = true; g.resetCreate(); g.cBots = 0; g.mode = 0;
        g.startMatch(0, 2);
        while (g.state != Game.ST_PLAY) { g.tick(0.05f); g.draw(); }
        g.menu = -1; g.freezeEnd = 0; g.me.yaw = 90; g.me.pitch = 10;
        int n = Wp.N, cols = 6, rows = (n + cols - 1) / cols;
        BufferedImage sheet = new BufferedImage(cols * 160, rows * 130, BufferedImage.TYPE_INT_RGB);
        java.awt.Graphics2D gr = sheet.createGraphics();
        for (int w = 0; w < n; w++) {
            Player p = g.me;
            if (Wp.slot[w] == Wp.S_PRIMARY) p.primary = w; else if (Wp.slot[w] == Wp.S_SECONDARY) p.secondary = w;
            if (w == Wp.HE) p.he = 1; if (w == Wp.FLASH) p.flash = 1; if (w == Wp.SMOKE) p.smoke = 1; if (w == Wp.C4) p.c4 = true;
            if (!p.hasWeapon(w)) continue;
            p.clip[w] = Wp.clip[w];
            g.switchTo(p, w);
            for (int i = 0; i < 30; i++) { g.tick(0.05f); }
            g.draw();
            BufferedImage im = new BufferedImage(W, H, BufferedImage.TYPE_INT_RGB); im.setRGB(0, 0, W, H, scr, 0, W);
            ImageIO.write(im, "png", new File(a[1].replace(".png", "_" + w + ".png")));
            int x = (w % cols) * 160, y = (w / cols) * 130;
            gr.drawImage(im.getScaledInstance(160, 120, java.awt.Image.SCALE_SMOOTH), x, y + 10, null);
            gr.setColor(java.awt.Color.YELLOW); gr.drawString(w + " " + Wp.name[w], x + 2, y + 10);
        }
        ImageIO.write(sheet, "png", new File(a[1]));
    }
}
