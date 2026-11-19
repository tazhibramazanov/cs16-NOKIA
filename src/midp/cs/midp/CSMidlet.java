package cs.midp;

import javax.microedition.midlet.*;
import javax.microedition.lcdui.*;

public final class CSMidlet extends MIDlet {
    private GameCanvas canvas;

    protected void startApp() {
        if (canvas == null) {
            canvas = new GameCanvas(this);
            Display.getDisplay(this).setCurrent(canvas);
            canvas.start();
        } else {
            canvas.resume();
            Display.getDisplay(this).setCurrent(canvas);
        }
    }

    protected void pauseApp() { if (canvas != null) canvas.pause(); }

    protected void destroyApp(boolean u) { if (canvas != null) canvas.stop(); }

    void exit() { destroyApp(true); notifyDestroyed(); }
}
