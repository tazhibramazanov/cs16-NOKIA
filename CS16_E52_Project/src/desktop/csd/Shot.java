package csd;
import cs.*;
import java.io.*;
import java.awt.image.BufferedImage;
import javax.imageio.ImageIO;
public class Shot {
    public static void main(String[] a) throws Exception {
        Sys.s = new DeskSys("res"); MapData m = new MapData(new FileInputStream(a[0]));
        Render r = new Render(320, 240, null);
        r.setMap(m);
        float x=Float.parseFloat(a[1]), y=Float.parseFloat(a[2]), z=Float.parseFloat(a[3]), yaw=Float.parseFloat(a[4]), pitch=Float.parseFloat(a[5]);
        r.setCamera(x,y,z,yaw,pitch,90);
        r.renderWorld(); // warm
        long t0=System.nanoTime(); int N=50;
        for(int i=0;i<N;i++){ r.setCamera(x,y,z,yaw+i*0.01f,pitch,90); r.renderWorld(); }
        long t1=System.nanoTime();
        for (int e=0;e<m.numEnts();e++){ String md=m.ent(e,"model"); String cl=m.ent(e,"classname");
            if(md!=null&&md.startsWith("*")&&(cl.equals("func_wall")||cl.equals("func_illusionary")||cl.equals("func_breakable"))) r.renderBrushModel(Integer.parseInt(md.substring(1)),0,0,0,true);}
        r.renderWorldAlpha();
        System.out.println("ms/frame "+(t1-t0)/1e6/N+" faces "+r.statFaces+" spans "+r.statSpans+" pix "+r.statPix);
        BufferedImage im=new BufferedImage(320,240,BufferedImage.TYPE_INT_RGB);
        im.setRGB(0,0,320,240,r.fb,0,320);
        ImageIO.write(im,"png",new File(a[6]));
    }
}
