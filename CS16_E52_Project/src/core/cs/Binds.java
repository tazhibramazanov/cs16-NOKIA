package cs;

/** User-editable key bindings: every action has up to two keys (like "bind" in CS). */
public final class Binds {
    // extra actions (beyond Game.A_*), only reachable through bindings
    public static final int A_LASTW = 1 << 21, A_DROP = 1 << 22, A_RADIO = 1 << 23;
    // navigation-only bits added by the platform for fixed keys (joystick / arrows / Enter / Esc)
    public static final int A_LEFT = 1 << 24, A_RIGHT = 1 << 25, A_ESC = 1 << 26, A_TABL = 1 << 27, A_TABR = 1 << 28;

    public static final String[] NAME = {"Вперёд", "Назад", "Шаг влево", "Шаг вправо", "Поворот влево", "Поворот вправо",
            "Взгляд вверх", "Взгляд вниз", "Огонь", "Прыжок", "Присесть", "Перезарядка", "Обезвредить/исп.",
            "След. оружие", "Прошлое оружие", "Прицел/глушитель", "Выбросить оружие", "Закупка / меню #", "Радиокоманды",
            "Пауза/настройки", "Счёт"};
    public static final int[] ACT = {Game.A_FWD, Game.A_BACK, Game.A_STRL, Game.A_STRR, Game.A_TURNL, Game.A_TURNR,
            Game.A_LOOKU, Game.A_LOOKD, Game.A_FIRE, Game.A_JUMP, Game.A_DUCK, Game.A_RELOAD, Game.A_USE,
            Game.A_NEXTW, A_LASTW, Game.A_ALT, A_DROP, Game.A_BUY, A_RADIO, Game.A_MENU, Game.A_SCORE};
    public static final int N = ACT.length;

    // phone key codes (MIDP canonical): digits '0'..'9', '*', '#', joystick -1..-5, soft keys -6/-7, C = -8
    public static final int K_UP = -1, K_DOWN = -2, K_LEFT = -3, K_RIGHT = -4, K_FIRE = -5, K_LSOFT = -6, K_RSOFT = -7, K_CLEAR = -8;
    // PC: mouse pseudo-codes
    public static final int M_L = 0x10001, M_R = 0x10002, M_M = 0x10003, M_WUP = 0x10004, M_WDN = 0x10005;

    public final int[] k1 = new int[N], k2 = new int[N]; // 0 = no key

    public int mask(int code) {
        if (code == 0) return 0;
        int m = 0;
        for (int i = 0; i < N; i++) if (k1[i] == code || k2[i] == code) m |= ACT[i];
        return m;
    }

    public boolean has(int code) { return mask(code) != 0; }

    /** assign a key to action idx; the key is taken away from any other action (which then gets a spare key back) */
    public void bind(int idx, int code) {
        if (code == 0) return;
        if (k1[idx] == code) return;
        if (k2[idx] == code) { k2[idx] = k1[idx]; k1[idx] = code; return; }
        int spare = k2[idx];
        k2[idx] = k1[idx];
        k1[idx] = code;
        for (int j = 0; j < N; j++) {
            if (j == idx) continue;
            if (k2[j] == code) k2[j] = 0;
            if (k1[j] == code) {
                k1[j] = k2[j]; k2[j] = 0;
                if (k1[j] == 0) {           // it lost its only key: give it a spare one so nothing becomes unreachable
                    if (spare == 0) { spare = k2[idx]; k2[idx] = 0; }
                    k1[j] = spare; spare = 0;
                }
            }
        }
    }

    public void clear2(int idx) { k2[idx] = 0; }

    void set(int idx, int a, int b) { k1[idx] = a; k2[idx] = b; }

    /** layout 0 = classic (2468 walk, joystick look), 1 = alternative (joystick walk) */
    public void phoneDefaults(int layout) {
        for (int i = 0; i < N; i++) { k1[i] = 0; k2[i] = 0; }
        if (layout == 1) {
            set(0, K_UP, 0); set(1, K_DOWN, 0); set(4, K_LEFT, 0); set(5, K_RIGHT, 0); set(6, '2', 0); set(7, '8', 0);
        } else {
            set(0, '2', 0); set(1, '8', 0); set(4, K_LEFT, 0); set(5, K_RIGHT, 0); set(6, K_UP, 0); set(7, K_DOWN, 0);
        }
        set(2, '4', 0); set(3, '6', 0); set(8, '5', K_FIRE); set(9, '1', 0); set(10, '3', 0); set(11, '7', 0); set(12, '0', 0);
        set(13, '9', 0); set(15, '*', 0); set(17, '#', 0); set(19, K_LSOFT, K_CLEAR); set(20, K_RSOFT, 0);
    }

    /** AWT virtual key codes (kept numeric so the core stays CLDC-only) */
    public void pcDefaults() {
        for (int i = 0; i < N; i++) { k1[i] = 0; k2[i] = 0; }
        set(0, 'W', 0); set(1, 'S', 0); set(2, 'A', 0); set(3, 'D', 0); set(4, 37, 0); set(5, 39, 0); set(6, 38, 0); set(7, 40, 0);
        set(8, M_L, 0); set(9, ' ', 0); set(10, 17, 0); set(11, 'R', 0); set(12, 'E', 0); set(13, M_WDN, M_WUP); set(14, 'Q', 0);
        set(15, M_R, 0); set(16, 'G', 0); set(17, 'B', 0); set(18, 'Z', 0); set(19, 27, 0); set(20, 9, 0);
    }

    public void write(byte[] b, int o) {
        for (int i = 0; i < N; i++) { put(b, o + i * 8, k1[i]); put(b, o + i * 8 + 4, k2[i]); }
    }

    public boolean read(byte[] b, int o) {
        if (b.length < o + N * 8) return false;
        for (int i = 0; i < N; i++) { k1[i] = get(b, o + i * 8); k2[i] = get(b, o + i * 8 + 4); }
        for (int i = 0; i < N; i++) if (k1[i] == 0 && k2[i] != 0) { k1[i] = k2[i]; k2[i] = 0; }
        return true;
    }

    static void put(byte[] b, int o, int v) { b[o] = (byte) (v >> 24); b[o + 1] = (byte) (v >> 16); b[o + 2] = (byte) (v >> 8); b[o + 3] = (byte) v; }
    static int get(byte[] b, int o) { return (b[o] & 255) << 24 | (b[o + 1] & 255) << 16 | (b[o + 2] & 255) << 8 | (b[o + 3] & 255); }

    public static String phoneName(int c) {
        if (c >= '0' && c <= '9') return String.valueOf((char) c);
        switch (c) {
            case 0: return "—";
            case '*': return "*";
            case '#': return "#";
            case K_UP: return "Джойст. вверх";
            case K_DOWN: return "Джойст. вниз";
            case K_LEFT: return "Джойст. влево";
            case K_RIGHT: return "Джойст. вправо";
            case K_FIRE: return "Центр джойст.";
            case K_LSOFT: return "Лев. софт";
            case K_RSOFT: return "Прав. софт";
            case K_CLEAR: return "C";
            case -10: return "Вызов";
            case -50: return "Карандаш";
        }
        return "Клавиша " + c;
    }
}
