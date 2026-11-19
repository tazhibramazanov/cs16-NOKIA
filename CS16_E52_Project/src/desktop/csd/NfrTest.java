package csd;
import cs.*;
import java.io.*;
import java.awt.image.BufferedImage;
import javax.imageio.ImageIO;
/** emulate a slow phone: real frames cost realMs, generated frames genMs; report displayed fps for each Nokia FR target */
public class NfrTest {
    public static void main(String[] a) throws Exception {
        DeskSys sys = new DeskSys("res"); Sys.s = sys;
        int[] screen = new int[320 * 240];
        Game g = new Game(320, 240, screen);
        g.init();
        int realMs = Integer.parseInt(a[0]), genMs = Integer.parseInt(a[1]);
        java.lang.reflect.Method sm = Game.class.getDeclaredMethod("startMatch", new Class[]{int.class, int.class});
        sm.setAccessible(true); sm.invoke(g, new Object[]{new Integer(0), new Integer(2)});
        g.tick(0.05f); g.draw(); g.tick(0.05f);
        java.lang.reflect.Constructor c = Bot.class.getDeclaredConstructor(new Class[]{Game.class, Player.class}); c.setAccessible(true);
        g.me.ai = (Bot) c.newInstance(new Object[]{g, g.me});
        java.lang.reflect.Method nr = Bot.class.getDeclaredMethod("newRound", new Class[0]); nr.setAccessible(true); nr.invoke(g.me.ai, new Object[0]);
        java.lang.reflect.Field fq = Game.class.getDeclaredField("nfr"); 
        for (int t = 0; t <= 5; t++) {
            g.nfr = t; g.setQuality(1);
            long end = System.currentTimeMillis() + 7000, last = System.currentTimeMillis();
            int frames = 0, gen = 0; long t0 = 0;
            boolean saved = false;
            while (System.currentTimeMillis() < end) {
                long now = System.currentTimeMillis();
                float dt = (now - last) / 1000f; last = now;
                g.me.yaw += 40 * dt;   // keep turning, like a player
                g.tick(dt); g.draw();
                long spent = System.currentTimeMillis() - now;
                long want = g.lastGenerated ? genMs : realMs;
                if (spent < want) Thread.sleep(want - spent);
                if (System.currentTimeMillis() > end - 3000) { if (t0 == 0) t0 = System.currentTimeMillis(); frames++; if (g.lastGenerated) gen++; }
                if (t == 5 && g.lastGenerated && !saved && System.currentTimeMillis() > end - 2000) {
                    BufferedImage im = new BufferedImage(320, 240, BufferedImage.TYPE_INT_RGB); im.setRGB(0, 0, 320, 240, screen, 0, 320);
                    ImageIO.write(im, "png", new File("/tmp/nfr_gen.png")); saved = true;
                }
            }
            float sec = (System.currentTimeMillis() - t0) / 1000f;
            System.out.println("target " + Game.NFR_T[t] + ": shown " + Math.round(frames / sec) + " fps (real " + Math.round((frames - gen) / sec) + ", generated " + Math.round(gen / sec) + ")  render " + g.r.W + "x" + g.r.H);
        }
    }
}
