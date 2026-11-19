package cs;
import csd.DeskSys;
/** offline zombie round: ZTest <res> */
public class ZTest {
    public static void main(String[] a) throws Exception {
        Sys.s = new DeskSys(a[0]);
        int[] scr = new int[240 * 320];
        Game g = new Game(240, 320, scr); g.init(); g.firstRunDone = true;
        g.resetCreate(); g.mode = a.length > 1 ? Integer.parseInt(a[1]) : 2; g.startMatch(0, 2);
        while (g.state != Game.ST_PLAY) { g.tick(0.05f); g.draw(); }
        g.me.ai = new Bot(g, g.me); g.me.ai.newRound();
        for (int i = 0; i < 20 * (a.length > 2 ? Integer.parseInt(a[2]) : 600); i++) {
            g.tick(0.05f);
            if (i % 400 == 0) {
                int z = 0, h = 0, zd = 0; StringBuilder t = new StringBuilder();
                for (int k = 0; k < g.pl.length; k++) { Player p = g.pl[k]; if (p.zombie) { z++; if (!p.alive) zd++; t.append(Game.ZNAME[p.ztype].charAt(0)); } else if (p.alive) h++; }
                System.out.println("t=" + i / 20 + "s round " + g.round + " score Z" + g.scoreT + ":H" + g.scoreCT + " humans " + h + " zombies " + z + " (dead " + zd + ") types " + t);
            }
        }
    }
}
