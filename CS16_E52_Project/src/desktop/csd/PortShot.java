package cs;
import csd.DeskSys;
import java.io.*;
import java.lang.reflect.*;
import java.awt.image.BufferedImage;
import javax.imageio.ImageIO;
/** Renders every screen of the game at a given size (portrait test): PortShot <resdir> <outdir> [w h] */
public class PortShot {
    static Game g; static int W, H; static int[] scr; static String out;
    static void set(String f, Object v) throws Exception { Field x = Game.class.getDeclaredField(f); x.setAccessible(true); x.set(g, v); }
    static Object get(String f) throws Exception { Field x = Game.class.getDeclaredField(f); x.setAccessible(true); return x.get(g); }
    static void call(String m, Class[] t, Object[] a) throws Exception { Method x = Game.class.getDeclaredMethod(m, t); x.setAccessible(true); x.invoke(g, a); }
    static void shot(String name) throws Exception {
        g.draw();
        BufferedImage im = new BufferedImage(W, H, BufferedImage.TYPE_INT_RGB);
        im.setRGB(0, 0, W, H, scr, 0, W);
        ImageIO.write(im, "png", new File(out, name + ".png"));
    }
    public static void main(String[] a) throws Exception {
        DeskSys sys = new DeskSys(a[0]); Sys.s = sys;
        out = a[1]; new File(out).mkdirs();
        W = a.length > 2 ? Integer.parseInt(a[2]) : 240; H = a.length > 3 ? Integer.parseInt(a[3]) : 320;
        scr = new int[W * H];
        g = new Game(W, H, scr);
        g.init();
        g.firstRunDone = true;
        g.menu = 0; g.menuSel = 0; shot("01_main");
        call("openOptions", new Class[]{int.class}, new Object[]{new Integer(0)});
        int[] tabs = {6, 7, 15, 8};
        for (int i = 0; i < 4; i++) { g.menu = tabs[i]; g.menuSel = 0; shot("02_opt" + i); }
        g.menu = 15; g.menuSel = 3; set("bindCapture", new Integer(3)); set("bindUntil", new Long(System.currentTimeMillis() + 5000)); shot("02_opt2_capture"); set("bindCapture", new Integer(-1));
        g.menu = 1; g.menuSel = 0; shot("03_create");
        g.openMP(); shot("03b_mp");
        g.menu = 17; set("mpErr", "Не удалось подключиться к серверу"); shot("03c_search_err");
        g.mpServer = "127.0.0.1:1"; g.startSearch(false); for (int i = 0; i < 3; i++) { g.tick(0.05f); } shot("03d_search");
        Thread.sleep(1500); g.tick(0.05f); shot("03e_search_fail");
        g.mpServer = ""; g.menu = 0;
        g.menu = 9; g.menuSel = 0; shot("04_first1");
        g.menu = 10; g.menuSel = 0; shot("04_first2");
        g.menu = 5; shot("05_help");
        g.menu = 2; g.menuSel = 0; shot("06_team_t"); g.menuSel = 1; shot("06_team_ct");
        g.menu = -1;
        call("startMatch", new Class[]{int.class, int.class}, new Object[]{new Integer(0), new Integer(2)});
        for (int i = 0; i < 6; i++) { g.tick(0.05f); g.draw(); }
        g.state = 1; set("loadMsg", "Загрузка карты de_dust2..."); shot("07_loading"); g.state = 2;
        for (int i = 0; i < 40; i++) g.tick(0.05f);
        g.menu = -1; shot("08_play");
        g.buyOpen = true; g.buyCat = -1; g.buySel = 0; shot("09_buy_root");
        g.buyCat = 3; g.buySel = 1; shot("09_buy_rifles");
        g.buyCat = 7; g.buySel = 1; shot("09_buy_equip");
        g.buyOpen = false;
        g.scoreOpen = true; shot("10_score"); g.scoreOpen = false;
        g.menu = 4; g.menuSel = 0; shot("11_pause");
        g.menu = 12; g.menuSel = 0; shot("12_radio");
        g.menu = 5; g.menuSel = 0; shot("12b_help_ingame");
        g.menu = 7; g.menuSel = 1; shot("12c_opt_ingame");
        g.menu = -1;
        for (int i = 0; i < 200; i++) g.tick(0.05f);
        shot("13_play_later");
        g.state = 3; shot("14_end");
        System.out.println("ok " + W + "x" + H);
    }
}
