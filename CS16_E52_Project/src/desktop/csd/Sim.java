package csd;
import cs.*;
import java.io.*;
import java.awt.image.BufferedImage;
import javax.imageio.ImageIO;
/** Headless simulation: runs a match with bots (human driven by a bot too) and saves screenshots. */
public class Sim {
    public static void main(String[] a) throws Exception {
        DeskSys sys = new DeskSys(a[0]); Sys.s = sys; Game.debug = true;
        int[] screen = new int[320 * 240];
        Game g = new Game(320, 240, screen);
        g.init();
        int map = Integer.parseInt(a[1]); float secs = Float.parseFloat(a[2]); String out = a[3];
        int q = a.length > 4 ? Integer.parseInt(a[4]) : 0; g.setQuality(q);
        g.difficulty = 1;
        java.lang.reflect.Method sm = Game.class.getDeclaredMethod("startMatch", new Class[]{int.class, int.class});
        sm.setAccessible(true); sm.invoke(g, new Object[]{new Integer(map), new Integer(2)});
        g.tick(0.05f); g.draw(); g.tick(0.05f); // load
        // human autopilot
        java.lang.reflect.Constructor c = Bot.class.getDeclaredConstructor(new Class[]{Game.class, Player.class});
        c.setAccessible(true);
        g.me.ai = (Bot) c.newInstance(new Object[]{g, g.me});
        java.lang.reflect.Method nr = Bot.class.getDeclaredMethod("newRound", new Class[0]); nr.setAccessible(true); nr.invoke(g.me.ai, new Object[0]);
        float dt = 1f / 20; int shots = 0; long drawNs = 0; int draws = 0;
        for (float t = 0; t < secs; t += dt) {
            g.tick(dt);
            if (((int) (t / dt)) % 4 == 0) { long t0 = System.nanoTime(); g.draw(); drawNs += System.nanoTime() - t0; draws++; }
            if (((int) (t / dt)) % (int) (8 / dt) == 0 && shots < 12) {
                g.draw();
                BufferedImage im = new BufferedImage(320, 240, BufferedImage.TYPE_INT_RGB);
                im.setRGB(0, 0, 320, 240, screen, 0, 320);
                ImageIO.write(im, "png", new File(out + "_" + (shots++) + ".png"));
            }
        }
        System.out.println("avg draw ms " + (drawNs / 1e6 / Math.max(1, draws)) + " sounds " + sys.soundCount);
        Player[] pl = g.pl;
        for (int i = 0; i < pl.length; i++) System.out.println(pl[i].name + " team " + pl[i].team + " k " + pl[i].kills + " d " + pl[i].deaths + " $" + pl[i].money + " alive " + pl[i].alive + " pos " + (int) pl[i].ox + "," + (int) pl[i].oy + "," + (int) pl[i].oz + " w " + Wp.name[pl[i].cur]);
        java.lang.reflect.Field f = Game.class.getDeclaredField("round"); f.setAccessible(true);
        java.lang.reflect.Field st = Game.class.getDeclaredField("scoreT"); st.setAccessible(true);
        java.lang.reflect.Field sc = Game.class.getDeclaredField("scoreCT"); sc.setAccessible(true);
        System.out.println("pickups " + g.statPickups + " penetrations " + g.statPen + " radio " + g.statRadio); System.out.println("round " + f.get(g) + " score T " + st.get(g) + " CT " + sc.get(g));
    }
}
