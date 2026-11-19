package csd;
import cs.*;
import java.io.*;
import java.awt.image.BufferedImage;
import javax.imageio.ImageIO;
/** render one view at a CS 1.6-style preset: map x y z yaw pitch out.png preset(0..3) scale(1|2) */
public class Shot2 {
    public static void main(String[] a) throws Exception {
        Sys.s = new DeskSys("res"); MapData m = new MapData(new FileInputStream(a[0]));
        int g = Integer.parseInt(a[7]), sc = Integer.parseInt(a[8]);
        Render r = new Render(320 * sc, 240 * sc, null);
        r.mipBias = g == 0 ? 1 : 0; r.stepShift = g == 0 ? 5 : g == 1 ? 4 : 3; r.bilinear = g == 3; r.flatSky = g == 0; r.mipScale = g >= 2 ? 0.7f : 1f;
        r.setMap(m);
        float x=Float.parseFloat(a[1]), y=Float.parseFloat(a[2]), z=Float.parseFloat(a[3]), yaw=Float.parseFloat(a[4]), pitch=Float.parseFloat(a[5]);
        r.setCamera(x,y,z,yaw,pitch,90); r.renderWorld();
        long t0=System.nanoTime(); int N=40;
        for(int i=0;i<N;i++){ r.setCamera(x,y,z,yaw,pitch,90); r.renderWorld(); }
        long t1=System.nanoTime();
        r.renderWorldAlpha();
        System.out.println("preset "+g+" x"+sc+" ms/frame "+(t1-t0)/1e6/N);
        BufferedImage im=new BufferedImage(r.W,r.H,BufferedImage.TYPE_INT_RGB);
        im.setRGB(0,0,r.W,r.H,r.fb,0,r.W);
        ImageIO.write(im,"png",new File(a[6]));
    }
}
