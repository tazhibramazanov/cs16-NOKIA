package csd;
import cs.*;
import java.io.*;
public class PhysT {
    public static void main(String[] a) throws Exception {
        DeskSys sys = new DeskSys("res"); Sys.s = sys;
        int[] screen = new int[320*240]; Game g = new Game(320,240,screen); g.init();
        MapData m = new MapData(new FileInputStream("res/de_dust2.cse"));
        m.modelActive = new boolean[m.numModels]; for (int i=0;i<m.numModels;i++) m.modelActive[i]=true;
        java.lang.reflect.Field f = Game.class.getDeclaredField("m"); f.setAccessible(true); f.set(g, m);
        Move mv = new Move(m);
        java.util.Vector v = new java.util.Vector();
        for (int e=0;e<m.numEnts();e++){ String cl=m.ent(e,"classname"), md=m.ent(e,"model"); if (md!=null&&md.startsWith("*")&&(cl.equals("func_wall")||cl.equals("func_breakable"))) v.addElement(new Integer(Integer.parseInt(md.substring(1)))); }
        m.solidModels = new int[v.size()]; for (int i=0;i<v.size();i++) m.solidModels[i]=((Integer)v.elementAt(i)).intValue();
        Player p = new Player();
        p.ox=Float.parseFloat(a[0]); p.oy=Float.parseFloat(a[1]); p.oz=Float.parseFloat(a[2]); p.bDuck = a.length>3;
        for (int i=0;i<40;i++){
            mv.move(p, 0.05f, 250);
            m.trace(p.ducked?3:1, p.ox,p.oy,p.oz,p.ox,p.oy,p.oz-2);
            System.out.println(i+" pos "+p.ox+","+p.oy+","+p.oz+" v "+p.vx+","+p.vy+","+p.vz+" gnd "+p.onGround+" duck "+p.ducked+" | tr frac "+m.rFrac+" n "+m.rNx+","+m.rNy+","+m.rNz+" ss "+m.rStartSolid+" as "+m.rAllSolid);
        }
    }
}
