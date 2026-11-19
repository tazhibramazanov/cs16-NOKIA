package csd;
import cs.*;
public class Dbg {
    static String en(Bot b) { try { java.lang.reflect.Field f = Bot.class.getDeclaredField("enemy"); f.setAccessible(true); Player e = (Player) f.get(b);
        java.lang.reflect.Field s = Bot.class.getDeclaredField("seen"); s.setAccessible(true); return e == null ? "-" : e.name + " seen=" + s.get(b); } catch (Exception x) { return x.toString(); } }
    public static void main(String[] a) throws Exception {
        DeskSys sys = new DeskSys(a[0]); Sys.s = sys; Game.debug = true;
        int[] screen = new int[320 * 240];
        Game g = new Game(320, 240, screen); g.init();
        int map = Integer.parseInt(a[1]);
        java.lang.reflect.Method sm = Game.class.getDeclaredMethod("startMatch", new Class[]{int.class, int.class});
        sm.setAccessible(true); sm.invoke(g, new Object[]{new Integer(map), new Integer(2)});
        g.tick(0.05f); g.draw(); g.tick(0.05f);
        java.lang.reflect.Field sf = Game.class.getDeclaredField("siteModel"); sf.setAccessible(true);
        int[] sites = (int[]) sf.get(g);
        for (int i = 0; i < sites.length; i++) { int md = sites[i]; System.out.println("site " + i + " model " + md + " " + g.m.mMins[md*3] + "," + g.m.mMins[md*3+1] + "," + g.m.mMins[md*3+2] + " - " + g.m.mMaxs[md*3] + "," + g.m.mMaxs[md*3+1] + "," + g.m.mMaxs[md*3+2]); }
        java.lang.reflect.Field tf = Bot.class.getDeclaredField("task"); tf.setAccessible(true);
        java.lang.reflect.Field pf = Bot.class.getDeclaredField("path"); pf.setAccessible(true);
        java.lang.reflect.Field pif = Bot.class.getDeclaredField("pi"); pif.setAccessible(true);
        java.lang.reflect.Field stf = Bot.class.getDeclaredField("site"); stf.setAccessible(true);
        g.me.alive = false; g.me.health = 0;
        float dt = 0.05f;
        for (float t = 0; t < Float.parseFloat(a[2]); t += dt) {
            g.tick(dt);
            if (((int) (t / dt)) % 200 == 0) {
                for (int i = 0; i < g.pl.length; i++) { Player p = g.pl[i]; if (p.team != 1 || !p.alive || p.ai == null) continue;
                    int[] path = (int[]) pf.get(p.ai);
                    System.out.println("t=" + (int) t + " " + p.name + (p.c4 ? "[C4]" : "") + " task " + tf.get(p.ai) + " site " + stf.get(p.ai) + " path " + (path == null ? "null" : pif.get(p.ai) + "/" + path.length) + " pos " + (int) p.ox + "," + (int) p.oy + "," + (int) p.oz + " inSite " + g.inSite(p) + " cur " + Wp.name[p.cur] + " planting " + p.planting + " fm " + (int)p.fmove + " sm " + (int)p.smove + " v " + (int)p.vx + "," + (int)p.vy + " gnd " + p.onGround + " duck " + p.ducked + " yaw " + (int)p.yaw + " enemy " + (en(p.ai)) ); }
            }
        }
    }
}
