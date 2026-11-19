package csd;
import cs.*;
import java.io.*;
import java.awt.image.BufferedImage;
import javax.imageio.ImageIO;
/** world + 3D players test: map out.png cull(0/1) */
public class Shot3 {
    public static void main(String[] a) throws Exception {
        Sys.s = new DeskSys("res"); MapData m = new MapData(new FileInputStream(a[0]));
        Render r = new Render(640, 480, null);
        r.setMap(m);
        Mdl3.loadTextures(); Render.buildModelShade(Mdl3.pal);
        Mdl3 mt = Mdl3.load("pl_t"), mc = Mdl3.load("pl_ct"), wp = Mdl3.load("p_ak47"), c4 = Mdl3.load("w_c4");
        Anim3 at = Anim3.load("pl_t"), ac = Anim3.load("pl_ct");
        // CT spawn on dust2-ish: find a spawn
        float sx = 0, sy = 0, sz = 0;
        for (int e = 0; e < m.numEnts(); e++) if ("info_player_start".equals(m.ent(e, "classname"))) { String[] o = m.ent(e, "origin").split(" "); sx = Float.parseFloat(o[0]); sy = Float.parseFloat(o[1]); sz = Float.parseFloat(o[2]); break; }
        r.setCamera(sx - 150, sy, sz + 28, 0, 5, 90);
        r.renderWorld();
        float[] Q = new float[mt.nb * 4], P = new float[mt.nb * 3], B = new float[mt.nb * 12], E = new float[12], WB = new float[64 * 12];
        String[][] poses = {{"idle1", "ref_aim_ak47"}, {"run", "ref_aim_ak47"}, {"crouchrun", "crouch_aim_ak47"}, {null, "death1"}};
        for (int i = 0; i < 4; i++) {
            Mdl3 md = i % 2 == 0 ? mt : mc; Anim3 an = i % 2 == 0 ? at : ac;
            if (poses[i][0] != null) { an.pose(an.find(poses[i][0]), 5, true, 0, Q, P); an.pose(an.find(poses[i][1]), 0, false, 1, Q, P); }
            else an.pose(an.find(poses[i][1]), 30, false, 0, Q, P);
            Mdl3.entity(sx, sy - 90 + i * 60, sz, 180 + i * 30, E);
            md.build(Q, P, E, B);
            r.drawMesh(md.parts[0][0], B, 32);
            if (i == 0 && md.parts.length > 1) r.drawMesh(md.parts[1][md.parts[1].length - 1], B, 32);
            if (poses[i][0] != null) { int[] map = wp.mergeMap(md); wp.buildMerged(map, B, E, WB); r.drawMesh(wp.parts[0][0], WB, 32); }
        }
        Mdl3.entity(sx - 60, sy + 30, sz - 36, 30, E); c4.buildDefault(E, WB); r.drawMesh(c4.parts[0][0], WB, 32);
        System.out.println("tris " + r.statTris + " parts " + mt.parts.length + " " + mt.partName[0] + "," + (mt.parts.length > 1 ? mt.partName[1] + " subs " + mt.parts[1].length : ""));
        BufferedImage im = new BufferedImage(r.W, r.H, BufferedImage.TYPE_INT_RGB);
        im.setRGB(0, 0, r.W, r.H, r.fb, 0, r.W);
        ImageIO.write(im, "png", new File(a[1]));
    }
}
