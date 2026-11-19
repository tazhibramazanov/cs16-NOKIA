package csd;
import cs.*;
import java.io.*;
import java.awt.image.BufferedImage;
import javax.imageio.ImageIO;
/** viewmodel test: name seq frame out.png */
public class VmShot {
    public static void main(String[] a) throws Exception {
        Sys.s = new DeskSys("res");
        Mdl3.loadTextures(); Render.buildModelShade(Mdl3.pal);
        Mdl3 md = Mdl3.load("v_" + a[0]); Anim3 an = Anim3.load("v_" + a[0]);
        BufferedImage im = new BufferedImage(640, 240, BufferedImage.TYPE_INT_RGB);
        for (int k = 0; k < 2; k++) {
            Render r = new Render(320, 240, null);
            r.setCamera(0, 0, 0, 0, 0, 90);
            java.util.Arrays.fill(r.fb, 0x506070);
            float[] Q = new float[64 * 4], P = new float[64 * 3], B = new float[64 * 12];
            an.pose(an.find(a[1]), Float.parseFloat(a[2]), false, 0, Q, P);
            float sg = k == 0 ? 1 : -1; float[] E = {r.fx, r.rx * sg, r.ux, 0, r.fy, r.ry * sg, r.uy, 0, r.fz, r.rz * sg, r.uz, 0};
            md.build(Q, P, E, B);
            for (int pi = 0; pi < md.parts.length; pi++) r.drawMesh(md.parts[pi][0], B, 34, k == 0);
            im.setRGB(k * 320, 0, 320, 240, r.fb, 0, 320);
        }
        ImageIO.write(im, "png", new File(a[3]));
    }
}
