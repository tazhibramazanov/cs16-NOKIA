package csd;
import cs.*;
import java.io.*;
import java.awt.image.BufferedImage;
import javax.imageio.ImageIO;
public class DirT {
    public static void main(String[] a) throws Exception {
        DeskSys sys = new DeskSys("res"); Sys.s = sys;
        int[] screen = new int[320 * 240];
        Game g = new Game(320, 240, screen); g.init();
        java.lang.reflect.Method sm = Game.class.getDeclaredMethod("startMatch", new Class[]{int.class, int.class});
        sm.setAccessible(true); sm.invoke(g, new Object[]{new Integer(0), new Integer(2)});
        g.tick(0.05f); g.draw(); g.tick(0.05f);
        BufferedImage out = new BufferedImage(320 * 4, 240, BufferedImage.TYPE_INT_RGB);
        Player me = g.me; Player b = null;
        for (int i = 0; i < g.pl.length; i++) if (g.pl[i].team == 1) { b = g.pl[i]; break; }
        for (int i = 1; i < g.pl.length; i++) if (g.pl[i] != b) { g.pl[i].alive = false; g.pl[i].deathTime = -100; }
        // T spawn area
        me.ox = -640; me.oy = -800; me.oz = 210; me.yaw = 90; me.pitch = 0;
        int[] yaws = {270, 0, 90, 180};
        for (int k = 0; k < 4; k++) {
            b.ox = -640; b.oy = -640; b.oz = 210; b.yaw = yaws[k]; b.vx = b.vy = 0; b.alive = true;
            g.draw();
            out.setRGB(k * 320, 0, 320, 240, screen, 0, 320);
        }
        ImageIO.write(out, "png", new File("shots/dir.png"));
    }
}
