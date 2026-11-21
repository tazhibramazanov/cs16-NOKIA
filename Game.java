package cs;

import java.io.*;
import java.util.Random;

public final class Game {
    public static Game g;
    public static final String VERSION = "1.1";
    // actions
    public static final int A_FWD = 1, A_BACK = 2, A_TURNL = 4, A_TURNR = 8, A_STRL = 16, A_STRR = 32, A_LOOKU = 64, A_LOOKD = 128,
            A_FIRE = 256, A_JUMP = 512, A_DUCK = 1024, A_RELOAD = 2048, A_USE = 4096, A_NEXTW = 8192, A_ALT = 16384, A_BUY = 32768,
            A_MENU = 65536, A_SCORE = 131072, A_UP = 262144, A_DOWN = 524288, A_SELECT = 1048576;

    public static final String[] MAPS = {"de_dust2", "de_dust", "de_train"};
    public static final String[] TNAMES = {"Террористы", "Спецназ"};
    static final String[] BOTNAMES = {"Albert", "Arnold", "Bill", "Calvin", "Christian", "Colin", "Cory", "Dave", "Eric", "Ernie", "Gary", "Hal",
            "Harvey", "Jim", "Joe", "Keith", "Kevin", "Lyle", "Moe", "Morris", "Neil", "Pete", "Rock", "Sam", "Scott", "Steve", "Ted", "Tom", "Wally", "Yahn"};

    public int W, H; // screen (the phone can switch between 240x320 and 320x240)
    public int[] screen;
    public Render r; public Gfx gx;
    public int quality = 0; // 0 auto (~20 fps), 1 high 320x240, 2 medium 240x180, 3 low 160x120
    int rLevel = 0; float autoAcc; int autoFrames; float autoCool; public float realDt;
    static final int[] LW = {320, 280, 240, 200, 160};
    Render[] rLevels = new Render[5];

    public Random rnd = new Random();
    public float time;
    public int keys;
    public boolean showFps = true; public int fps; private int fpsCount; private long fpsT;
    public int sens = 5, difficulty = 1, volume = 80, maxRounds = 15;
    public boolean soundOn = true, autoAim = true;

    // state
    static final int ST_MENU = 0, ST_LOAD = 1, ST_PLAY = 2, ST_END = 3;
    public int state = ST_MENU;
    public int menu = 0, menuSel = 0; // menu ids below
    static final int M_MAIN = 0, M_TEAM = 2, M_OPT = 3, M_PAUSE = 4, M_NONE = -1, M_HELP = 5;
    int pendingMap, pendingTeam;
    String loadMsg = "";

    // world
    public MapData m; public Move mv; public Nav nav; public String mapName; int mapIdx;
    Sprites sprT, sprCT;
    Sprites[] vws = new Sprites[Wp.N + 2];
    int[] vwsAge = new int[Wp.N + 2]; int vwsTick;
    int[] drawEnts = new int[0];
    float[] spawnT, spawnCT;
    int[] siteModel = new int[0]; String[] siteName = new String[0];
    int[] buyModel = new int[0]; int[] buyTeam = new int[0];
    int[] breakModel = new int[0];

    // players
    public Player[] pl = new Player[0]; public Player me; public int specIdx = -1;
    int round, scoreT, scoreCT, lossStreakT, lossStreakCT;
    float roundStart, freezeEnd, buyEnd, roundEndAt = -1, roundTime = 135;
    boolean roundOver;
    public boolean bombPlanted, bombDropped, bombExploded; public float bombX, bombY, bombZ, bombAt, bombBeep; public int bombSite = -1;
    String centerMsg; float centerMsgUntil; int centerCol;
    // killfeed
    String[] kfA = new String[5], kfB = new String[5]; int[] kfW = new int[5], kfColA = new int[5], kfColB = new int[5]; float[] kfT = new float[5]; boolean[] kfHS = new boolean[5];
    // grenades
    static final int MAXG = 16;
    float[] grX = new float[MAXG], grY = new float[MAXG], grZ = new float[MAXG], grVX = new float[MAXG], grVY = new float[MAXG], grVZ = new float[MAXG], grT = new float[MAXG];
    int[] grType = new int[MAXG], grOwner = new int[MAXG]; boolean[] grOn = new boolean[MAXG];
    // smoke clouds
    static final int MAXS = 6;
    float[] smX = new float[MAXS], smY = new float[MAXS], smZ = new float[MAXS], smT = new float[MAXS]; boolean[] smOn = new boolean[MAXS];
    // effects (impacts, flashes)
    static final int MAXFX = 48;
    float[] fxX = new float[MAXFX], fxY = new float[MAXFX], fxZ = new float[MAXFX], fxT = new float[MAXFX], fxLife = new float[MAXFX], fxSize = new float[MAXFX];
    int[] fxCol = new int[MAXFX]; int fxNext;
    // human state
    float turnHeld, hurtFlash, viewBob, muzzleUntil;
    int buyCat = -1; boolean buyOpen, scoreOpen; int buySel;
    float lightLevel = 32; int myLight = 32;
    float deadCamYaw;
    private final float[] tmp3 = new float[3];
    // ---- settings (like the CS options dialog)
    public int sensV = 5; public boolean invert; public int layout; // 0 classic, 1 alternative
    public int gammaIdx = 1, crossCol = 0, crossSize = 1; public boolean crossDyn = true, viewBobOn = true, radarOn = true;
    public boolean firstRunDone; public boolean pc; public int winScale = 3;
    // "create server" settings
    public int cMap = 0, cTeam = 3, cBots = 9, cRoundTime = 135, cBuyTime = 45, cFreeze = 3, cMoney = 800; public boolean cFF = false;
    /** "Новая игра" always opens with factory settings (nothing is remembered from the last match) */
    void resetCreate() { cMap = 0; cTeam = 3; cBots = 9; cRoundTime = 135; cBuyTime = 45; cFreeze = 3; cMoney = 800; cFF = false; maxRounds = 15; difficulty = 1; }
    float buyTimeS = 45, freezeS = 3; int startMoney = 800; boolean friendlyFire;
    // ---- multiplayer and game modes
    public MP mp; public int mode;   // 0 original (bomb), 1 deathmatch, 2 zombie
    public String mpServer = "", mpName = "", mpErr;
    int mpMode, mpMap;
    static final int M_MP = 16, M_SEARCH = 17, M_LAN = 18, M_LANG = 19, M_WORLD = 20;
    LanFinder lanFinder; LanFinder.Game[] lanList = new LanFinder.Game[0];
    MqttLink world; MqttLink.Room[] worldList = new MqttLink.Room[0]; int worldK;   // "Найти игру по всему миру": every room in the world

    void openWorld() { saveSettings(); mpErr = null; closeWorld(); world = MqttLink.browse(); menu = M_WORLD; menuSel = 0; worldK = 0; }
    void closeWorld() { if (world != null) world.close(); world = null; }

    /** a game in a list: "> host \t players \t mode, map, score" (drawn on two lines) */
    String roomLine(MqttLink.Room x) {
        String d = MP.MODEN[x.mode] + ", " + MAPS[x.map];
        if (x.score && x.mode == 0 && x.round > 0) d += ", T " + x.t + ":" + x.ct + " CT";
        if (x.score && x.mode == 2 && x.round > 0) d += ", раунд " + x.round;
        return "> " + x.name + "\t" + x.players + "/10\t" + d;
    }
    float infectAt; boolean infected;
    // zombie types (the host picks one at random for every new zombie)
    public static final String[] ZNAME = {"Бегун", "Громила", "Ходок"};
    public static final String[] ZDESC = {"быстрый, слабый удар", "медленный, сильный удар", "всё средне"};
    static final int[] ZSPD = {330, 225, 280}, ZDMG = {14, 55, 28}, ZHP = {1500, 4000, 2500};
    /** this game decides damage, rounds and bots (offline or the online host) */
    boolean auth() { return mp == null || mp.host; }
    /** online, mirroring the host */
    boolean client() { return mp != null && !mp.host; }
    String botName() {
        for (int t = 0; t < 40; t++) {
            String n = BOTNAMES[(rnd.nextInt() & 0xFFFF) % BOTNAMES.length]; boolean used = false;
            for (int i = 0; i < pl.length; i++) if (n.equals(pl[i].name)) used = true;
            if (!used) return n;
        }
        return "Bot" + (rnd.nextInt() & 99);
    }
    void centerAll(String s, float d, int c) { center(s, d, c); if (mp != null && mp.host) mp.evCenter(s, d, c); }
    void chatAll(String s, int c) { chat(s, c); if (mp != null && mp.host) mp.evChat(s, c); }
    void soundAll(String n, int vol) { sound(null, n, vol); if (mp != null && mp.host && me != null) mp.evSound(null, 0, 0, 0, n, vol, 0); }
    void net(int op, int a, int c) { if (client()) mp.act(op, a, c); }

    void openMP() {
        mpMode = 0; mpMap = 0; mpErr = null;     // mode and map are chosen every time
        if (mpName.length() == 0) mpName = "Игрок" + (1000 + (rnd.nextInt() & 0xFFFF) % 9000);
        menu = M_MP; menuSel = 4;
    }

    boolean mpCreate;
    void startSearch(boolean create) {
        mpCreate = create;
        saveSettings();
        mpErr = null;
        if (mpServer.trim().length() == 0) mp = new MP(this, MqttLink.open(), mpMode, mpMap, mpName, create);   // free public internet server
        else mp = new MP(this, mpServer, mpMode, mpMap, mpName, create);
        menu = M_SEARCH; menuSel = 0;
    }

    void editServer(final boolean thenSearch) {
        Sys.s.editText("Свой сервер адрес:порт (пусто = авто)", mpServer, 64, new TextCb() { public void done(String t) {
            if (t != null) { mpServer = t.trim(); saveSettings(); if (thenSearch && mpServer.length() > 0) startSearch(mpCreate); }
        } });
    }

    void closeFinder() { if (lanFinder != null) lanFinder.close(); lanFinder = null; }

    /** "Создать игру по LAN": the server runs inside this game, others find it over Wi-Fi */
    void startLan() {
        saveSettings(); mpErr = null; mpCreate = true;
        LanServer ls = new LanServer(mpMode, mpMap, mpName);
        try {
            Conn c = ls.start();
            mp = new MP(this, ls, c, mpMode, mpMap, mpName);
            menu = M_SEARCH; menuSel = 0;
        } catch (Throwable t) {
            ls.close();
            mpFailed("Не удалось открыть игру по LAN (порт " + LanServer.PORT + " занят или нет Wi-Fi)");
        }
    }

    void mpCancel() { boolean c = mpCreate; if (mp != null) mp.close(); mp = null; mode = 0; menu = M_MP; menuSel = c ? 5 : 4; }

    /** server put us into a room: load the map */
    public void mpStart(MP m) {
        resetCreate();
        mode = m.mode; cMap = m.map;
        startMatch(m.map, mode == 2 ? 2 : 1 + (rnd.nextInt() & 1));
    }

    public void mpFailed(String s) {
        mp = null; mode = 0; mpErr = s;
        if (state != ST_MENU) { state = ST_MENU; Sys.s.stopSounds(); }
        menu = M_SEARCH; menuSel = 0;
    }

    void mpLeave() { if (mp != null) mp.close(); mp = null; mode = 0; state = ST_MENU; menu = M_MAIN; menuSel = 0; Sys.s.stopSounds(); }

    public void mpBecameHost() { if (state == ST_PLAY && bombPlanted) bombBeep = time; }

    /** client: the host started a new round */
    void clientNewRound() {
        roundOver = false; roundEndAt = -1; bombExploded = false; bombSite = -1; infected = false;
        for (int i = 0; i < MAXG; i++) grOn[i] = false;
        for (int i = 0; i < MAXS; i++) smOn[i] = false;
        for (int i = 0; i < MAXD; i++) dOn[i] = false;
        if (m != null) for (int i = 0; i < m.numModels; i++) m.modelActive[i] = true;
        buyOpen = false;
    }

    // ---- modes
    void modeTick() {
        if (mode == 1) {
            if (roundOver) return;
            for (int i = 0; i < pl.length; i++) { Player p = pl[i]; if (!p.alive && time - p.deathTime > 3) respawn(p); }
        } else if (mode == 2 && infected && !roundOver) {   // dead zombies come back after 5 s
            for (int i = 0; i < pl.length; i++) { Player p = pl[i]; if (!p.alive && p.zombie && time - p.deathTime > 5) zombieRespawn(p); }
        } else if (mode == 2 && !infected && !roundOver && time >= infectAt) {
            infected = true;
            int n = 0; for (int i = 0; i < pl.length; i++) if (pl[i].alive) n++;
            int z = 1;
            for (int k = 0; k < z; k++) {
                int pick = (rnd.nextInt() & 0xFFFF) % Math.max(1, n - k), c = 0;
                for (int i = 0; i < pl.length; i++) if (pl[i].alive && !pl[i].zombie && c++ == pick) { infect(pl[i], null); break; }
            }
            centerAll("Заражение началось! Бегите от зомби", 3, 0x60FF60);
            soundAll("radio_takepoint", 80);
        }
    }

    void infect(Player p, Player by) {
        p.ztype = (rnd.nextInt() & 0xFFFF) % 3;
        p.zombie = true; p.team = 1; p.health = by == null ? ZHP[p.ztype] * 3 / 2 : ZHP[p.ztype]; p.armor = 0; p.helmet = false;
        p.primary = -1; p.secondary = -1; p.he = p.flash = p.smoke = 0; p.defuser = false; p.zoom = 0; p.reloading = false;
        p.planting = p.defusing = false; p.c4 = false;
        switchTo(p, Wp.KNIFE);
        if (p.ai != null) p.ai.newRound();
        sound(p, "player_die3", 80);
        if (p == me) { center("Вы заражены! Вы — " + ZNAME[p.ztype] + " (" + ZDESC[p.ztype] + ")", 4, 0x60FF60); buyOpen = false; }
        chatAll(p.name + " стал зомби: " + ZNAME[p.ztype], 0x80FF80);
        if (by != null) { by.kills++; killfeed(by, p, Wp.KNIFE, false); if (mp != null && mp.host) mp.evKill(by, p, Wp.KNIFE, false); }
    }

    void zombieRespawn(Player p) {
        int zt = p.ztype;
        p.team = 1; respawn(p);
        p.zombie = true; p.team = 1; p.ztype = zt; p.health = ZHP[zt]; p.armor = 0; p.helmet = false; p.money = 0;
        p.primary = -1; p.secondary = -1; p.he = p.flash = p.smoke = 0;
        switchTo(p, Wp.KNIFE);
        if (p.ai != null) p.ai.newRound();
        if (p == me) center("Вы снова в игре: " + ZNAME[zt], 2, 0x60FF60);
    }

    /** deathmatch: back into the fight 3 s after death */
    void respawn(Player p) {
        float[] sp = p.team == 1 ? spawnT : spawnCT; int n = sp.length / 4; if (n == 0) return;
        int k = (rnd.nextInt() & 0xFFFF) % n;
        p.resetForRound(); p.spawnSeq++;
        p.primary = -1; p.he = p.flash = p.smoke = 0; p.defuser = false; p.c4 = false; p.armor = 100; p.helmet = true; p.money = 16000;
        p.secondary = p.team == 1 ? Wp.GLOCK : Wp.USP; p.clip[p.secondary] = Wp.clip[p.secondary]; p.ammo[p.secondary] = Wp.reserve[p.secondary];
        p.ox = sp[k * 4]; p.oy = sp[k * 4 + 1]; p.oz = sp[k * 4 + 2] + 1; p.yaw = sp[k * 4 + 3]; p.pitch = 0;
        m.trace(1, p.ox, p.oy, p.oz, p.ox, p.oy, p.oz - 64);
        if (!m.rStartSolid) p.oz = m.rEz;
        mv.categorize(p);
        p.cur = p.secondary; p.drawEnd = time + 0.5f; setVm(p, "draw", Wp.drawT[p.cur]);
        if (p.ai != null) p.ai.newRound();
        if (p == me) { specIdx = -1; center("Снова в бою! Закупка: #", 1.5f, 0xFFFFFF); }
    }
    // dropped weapons
    static final int MAXD = 24;
    float[] dX = new float[MAXD], dY = new float[MAXD], dZ = new float[MAXD], dVX = new float[MAXD], dVY = new float[MAXD], dVZ = new float[MAXD], dYaw = new float[MAXD], dT = new float[MAXD];
    int[] dW = new int[MAXD], dClip = new int[MAXD], dAmmo = new int[MAXD], dBy = new int[MAXD]; boolean[] dOn = new boolean[MAXD], dSil = new boolean[MAXD], dRest = new boolean[MAXD];
    Sprites sprW;
    // radio
    public static final String[] RS = {"ct_coverme", "takepoint", "position", "regroup", "followme", "fireassis", "com_go", "fallback", "sticktog", "com_getinpos", "stormfront", "com_reportin",
            "ct_affirm", "ct_enemys", "ct_backup", "clear", "ct_inpos", "ct_reportingin", "blow", "negative", "enemydown"};
    public static final String[] RT = {"Прикройте меня!", "Ты на острие!", "Держите эту позицию!", "Перегруппировка!", "За мной!", "Под огнём, нужна помощь!",
            "Вперёд, вперёд, вперёд!", "Отступаем!", "Держаться вместе!", "Занять позиции, ждать сигнала", "Штурмуем!", "Доложить обстановку!",
            "Принято!", "Противник замечен!", "Нужна поддержка!", "Чисто!", "Я на позиции", "Докладываю", "Уходите, сейчас рванёт!", "Отказ", "Противник убит"};
    String[] chatS = new String[5]; int[] chatC = new int[5]; float[] chatT = new float[5];
    Player[] rqP = new Player[8]; int[] rqCmd = new int[8]; float[] rqT = new float[8];

    public Game(int w, int h, int[] screen) {
        W = w; H = h; this.screen = screen;
        g = this;
    }

    public void init() throws IOException {
        gx = new Gfx(W, H, screen);
        gx.load();
        bPhone.phoneDefaults(0); bPC.pcDefaults();
        if (pc) gfx = 3;
        loadSettings();
        applyGamma();
        setQuality(quality);
        if (!firstRunDone) { menu = M_FIRST1; menuSel = 0; }
        String[] snd = {"weapons_ak47_1", "weapons_m4a1_unsil_1", "weapons_glock18_2", "weapons_usp_unsil_1", "weapons_deagle_1", "player_pl_step1", "player_pl_step2",
                "player_bhit_flesh_1", "player_headshot1", "player_die1", "weapons_knife_slash1", "weapons_awp1", "weapons_clipout1", "weapons_clipin1", "radio_go", "radio_terwin", "radio_ctwin"};
        Sys.s.preloadSounds(snd);
    }

    // ---- CS 1.6-style graphics presets (independent of the old resolution setting)
    static final String[] GFXN = {"Минимальная", "Средняя", "Высокая", "Максимальная"};
    public int gfx = 1;
    Render rHi;
    public int[] hiBase; public boolean hiActive;
    /** PC + maximum: the 3D view is rendered at 640x480 (like the original's minimum OpenGL resolution) */
    /** "Максимальная": the 3D view is drawn at double resolution and smoothed down (PC and phone) */
    public boolean hiRes() { return gfx == 3; }
    public int[] hiFrame() { return rHi != null ? rHi.fb : null; }

    public void setGfx(int g) { gfx = g; setLevel(rLevel); }

    // ---- phone screen orientation: 0 vertical 240x320, 1 horizontal 320x240 (phone held sideways, keypad on the right)
    static final String[] ORIENTN = {"вертикально", "горизонтально"};
    public int orient;
    /** switch the frame size (called by the phone canvas when orient changes) */
    public void resize(int w, int h, int[] scr) {
        W = w; H = h; screen = scr;
        gx.W = w; gx.H = h; gx.fb = scr;
        rLevels = new Render[5]; rHi = null; hiBase = null; hiActive = false;
        genValid = false; genWorld = null; genVm = null; sxI = null;
        freeUi();
        setLevel(rLevel);
    }
    /** in horizontal mode the number pad is turned too: true while digits mean directions (playing, no menu) */
    public boolean turnDigits() { return turned && state == ST_PLAY && menu == M_NONE && !buyOpen; }
    /** set by the phone canvas: the picture is shown turned a quarter on the screen (the phone is held sideways) */
    public boolean turned;

    void applyGfx(Render rr) {
        rr.mipBias = gfx == 0 ? 1 : 0;
        rr.stepShift = gfx == 0 ? 5 : gfx == 1 ? 4 : 3;
        rr.bilinear = gfx == 3;
        rr.flatSky = gfx == 0;
        rr.mipScale = gfx >= 2 ? 0.7f : 1f;
    }

    public void setQuality(int q) {
        quality = q;
        setLevel(q == 0 ? rLevel : q == 1 ? 0 : q == 2 ? 2 : 4);
    }

    void setLevel(int l) {
        rLevel = l;
        for (int i = 1; i < 5; i++) if (i != l) rLevels[i] = null;
        if (rLevels[l] == null) rLevels[l] = l == 0 ? new Render(W, H, screen) : new Render(LW[l] * W / 320, LW[l] * H / 320, null);
        r = rLevels[l];
        if (l == 0 && hiRes()) { if (rHi == null) rHi = new Render(W * 2, H * 2, null); r = rHi; }
        else rHi = null;
        applyGfx(r);
        hiActive = false;
        if (m != null) r.setMap(m);
        xmap = new int[W]; for (int x = 0; x < W; x++) xmap[x] = x * r.W / W;
        ymap = new int[H]; for (int y = 0; y < H; y++) ymap[y] = y * r.H / H;
    }

    public void applyGamma() {
        float gm = 1 + gammaIdx * 0.15f;
        for (int i = 0; i < 256; i++) Render.gamma[i] = Math.min(255, (int) (255 * pow(i / 255f, 1 / gm) + 0.5f));
        if (m != null) r.setMap(m);
        if (sprT != null) sprT.invalidate(); if (sprCT != null) sprCT.invalidate(); if (sprW != null) sprW.invalidate();
        for (int i = 0; i < vws.length; i++) if (vws[i] != null) vws[i].invalidate();
        if (Mdl3.pal != null) Render.buildModelShade(Mdl3.pal);
    }
    private int[] xmap, ymap;

    // ======================================================================= settings
    void loadSettings() {
        byte[] b = Sys.s.load();
        if (b == null || b.length < 30 || b[0] < 2 || b[0] > 8) return;
        int k = 1;
        sens = b[k++]; sensV = b[k++]; invert = b[k++] != 0; layout = b[k++]; difficulty = b[k++]; volume = b[k++]; quality = b[k++];
        soundOn = b[k++] != 0; showFps = b[k++] != 0; maxRounds = b[k++]; autoAim = b[k++] != 0; gammaIdx = b[k++]; crossCol = b[k++]; crossSize = b[k++];
        crossDyn = b[k++] != 0; viewBobOn = b[k++] != 0; radarOn = b[k++] != 0; firstRunDone = b[k++] != 0; winScale = b[k++];
        cMap = b[k++]; cTeam = b[k++]; cBots = b[k++]; cRoundTime = (b[k++] & 255) * 15; cBuyTime = (b[k++] & 255) * 5; cFreeze = b[k++];
        cMoney = ((b[k++] & 255) << 8 | (b[k++] & 255)) * 100; cFF = b[k++] != 0; fpsCap20 = b[k++] != 0; pcMouse = b[k++] != 0;
        if (b[0] >= 3) { gfx = b[k++]; if (gfx < 0 || gfx > 3) gfx = 1; }
        if (b[0] >= 4) { nfr = b[k++]; if (nfr < 0 || nfr > 5) nfr = 0; }
        if (b[0] >= 5) { orient = b[k++]; if (orient < 0 || orient > 1) orient = pc ? 1 : 0; }
        if (pc && b[0] < 6) orient = 1;   // older PC saves had no screen choice: horizontal
        if (b[0] >= 7) { voiceOn = b[k++] != 0; voiceVol = b[k++]; if (voiceVol < 10 || voiceVol > 100) voiceVol = 100; }
        Lang.set(b[0] >= 8 ? b[k++] : Lang.EN);   // 1.1: English until the player picks Russian
        if (sens < 1 || sens > 10) sens = 5; if (sensV < 1 || sensV > 10) sensV = 5; if (difficulty < 0 || difficulty > 3) difficulty = 1;
        if (quality < 0 || quality > 3) quality = 0; if (maxRounds < 3 || maxRounds > 30) maxRounds = 15; if (gammaIdx < 0 || gammaIdx > 4) gammaIdx = 1;
        if (crossCol < 0 || crossCol > 4) crossCol = 0; if (crossSize < 0 || crossSize > 2) crossSize = 1; if (winScale < 1 || winScale > 4) winScale = 3;
        if (cMap < 0 || cMap > 2) cMap = 0; if (cTeam < 1 || cTeam > 3) cTeam = 3; if (cBots < 1 || cBots > 9) cBots = 9;
        if (cRoundTime < 60 || cRoundTime > 300) cRoundTime = 135; if (cBuyTime < 15 || cBuyTime > 90) cBuyTime = 45; if (cFreeze < 0 || cFreeze > 10) cFreeze = 3;
        if (cMoney < 800 || cMoney > 16000) cMoney = 800; if (layout < 0 || layout > 2) layout = 0;
        boolean got = false;
        int bn = b.length > k ? b[k] : 0;   // actions in the save (older versions: fewer)
        if (bn > 0 && bn <= Binds.N && bPhone.read(b, k + 1, bn)) got = bPC.read(b, k + 1 + bn * 8, bn);
        if (got && b[0] >= 5) {
            Net.Rd rd = new Net.Rd(b, k + 1 + bn * 16);
            if (rd.more()) mpServer = rd.str(); if (rd.more()) mpName = rd.str();
            if (mpServer.indexOf('.') < 0) mpServer = "";   // "1111" and other non-addresses -> automatic internet server
        }
        if (!got) { if (layout == 2) layout = 0; bPhone.phoneDefaults(layout); bPC.pcDefaults(); }
    }

    public void saveSettings() {
        byte[] b = new byte[]{8, (byte) sens, (byte) sensV, (byte) (invert ? 1 : 0), (byte) layout, (byte) difficulty, (byte) volume, (byte) quality,
                (byte) (soundOn ? 1 : 0), (byte) (showFps ? 1 : 0), (byte) maxRounds, (byte) (autoAim ? 1 : 0), (byte) gammaIdx, (byte) crossCol, (byte) crossSize,
                (byte) (crossDyn ? 1 : 0), (byte) (viewBobOn ? 1 : 0), (byte) (radarOn ? 1 : 0), (byte) (firstRunDone ? 1 : 0), (byte) winScale,
                (byte) cMap, (byte) cTeam, (byte) cBots, (byte) (cRoundTime / 15), (byte) (cBuyTime / 5), (byte) cFreeze,
                (byte) ((cMoney / 100) >> 8), (byte) (cMoney / 100), (byte) (cFF ? 1 : 0), (byte) (fpsCap20 ? 1 : 0), (byte) (pcMouse ? 1 : 0), (byte) gfx, (byte) nfr, (byte) orient, (byte) (voiceOn ? 1 : 0), (byte) voiceVol, (byte) Lang.lang};
        Net.Buf sb = new Net.Buf().str(mpServer).str(mpName);
        byte[] all = new byte[b.length + 1 + Binds.N * 16 + sb.n];
        System.arraycopy(b, 0, all, 0, b.length);
        System.arraycopy(sb.b, 0, all, b.length + 1 + Binds.N * 16, sb.n);
        all[b.length] = (byte) Binds.N;
        bPhone.write(all, b.length + 1);
        bPC.write(all, b.length + 1 + Binds.N * 8);
        Sys.s.save(all);
    }

    // ======================================================================= utils
    public static float atan2(float y, float x) {
        float ax = Math.abs(x), ay = Math.abs(y);
        if (ax < 1e-9f && ay < 1e-9f) return 0;
        float a = Math.min(ax, ay) / Math.max(ax, ay);
        float s = a * a;
        float r2 = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
        if (ay > ax) r2 = 1.57079637f - r2;
        if (x < 0) r2 = 3.14159274f - r2;
        if (y < 0) r2 = -r2;
        return r2 * 57.2957795f;
    }

    public static float angNorm(float a) { while (a > 180) a -= 360; while (a < -180) a += 360; return a; }
    public float rf() { return rnd.nextFloat(); }
    public float rs() { return rnd.nextFloat() * 2 - 1; }

    Player listener() { return me != null && me.alive ? me : (specIdx >= 0 && specIdx < pl.length ? pl[specIdx] : me); }

    public void sound(Player src, String name, int vol) {
        if (src != null && mp != null && mp.host && !name.startsWith("player_pl_step")) mp.evSound(src, src.ox, src.oy, src.oz, name, vol, 1800);
        if (!soundOn) return;
        Player l = listener();
        if (src != null && l != null && src != l) {
            float dx = src.ox - l.ox, dy = src.oy - l.oy, dz = src.oz - l.oz;
            float d = (float) Math.sqrt(dx * dx + dy * dy + dz * dz);
            vol = (int) (vol * (1 - d / 1800f));
            if (vol < 8) return;
        }
        Sys.s.sound(name, vol * volume / 100);
    }

    public void soundAt(float x, float y, float z, String name, int vol, float range) {
        if (mp != null && mp.host) mp.evSound(null, x, y, z, name, vol, range);
        if (!soundOn) return;
        if (range <= 0) { Sys.s.sound(name, vol * volume / 100); return; }   // global (host event)
        Player l = listener();
        if (l != null) {
            float dx = x - l.ox, dy = y - l.oy, dz = z - l.oz;
            float d = (float) Math.sqrt(dx * dx + dy * dy + dz * dz);
            vol = (int) (vol * (1 - d / range));
            if (vol < 8) return;
        }
        Sys.s.sound(name, vol * volume / 100);
    }

    // ======================================================================= voice chat (push-to-talk, like a walkie-talkie)
    public boolean voiceOn = true;
    public int voiceVol = 100;
    boolean voiceRec, voiceHeld;
    long voiceRecStart;
    final Object voiceLock = new Object();
    short[] voiceOut; String voiceErr;   // filled by the microphone callback (another thread)
    String talkName; long talkUntil;     // who is speaking now
    int voiceSeq;
    // incoming clips put together from their parts, one slot per player
    byte[][][] vParts; int[] vSeq, vGot, vCount; int[][] vLen;

    // ---- "Проверить микрофон": record 3 s and play it back to yourself
    int micTestState;        // 0 idle, 1 recording, 2 waiting for the clip, 3 playing back
    long micTestAt; String micTestMsg; long micTestMsgUntil;
    short[] micTestOut; String micTestErr;

    String micTestLabel() {
        long now = Sys.s.millis();
        switch (micTestState) {
            case 1: return "Говорите... " + Math.max(1, (int) (3999 - (now - micTestAt)) / 1000) + " с";
            case 2: return "Обработка записи...";
            case 3: return "Слушайте себя...";
        }
        if (micTestMsg != null && now < micTestMsgUntil) return micTestMsg;
        return "Проверить микрофон";
    }

    void micTest() {
        if (micTestState != 0) return;
        if (voiceRec) return;
        if (Sys.s.micStart()) { micTestState = 1; micTestAt = Sys.s.millis(); }
        else { micTestMsg = "Микрофон недоступен"; micTestMsgUntil = Sys.s.millis() + 5000; }
    }

    void micTestTick() {
        if (micTestState == 0) return;
        long now = Sys.s.millis();
        if (micTestState == 1 && now - micTestAt >= 3000) {
            micTestState = 2; micTestAt = now;
            Sys.s.micStop(new VoiceCb() {
                public void done(short[] pcm, String err) { synchronized (voiceLock) { micTestOut = pcm; micTestErr = pcm == null ? (err != null ? err : "пустая запись") : null; } }
            });
        }
        if (micTestState == 2) {
            short[] c; String e;
            synchronized (voiceLock) { c = micTestOut; e = micTestErr; micTestOut = null; micTestErr = null; }
            if (e != null) { micTestState = 0; micTestMsg = "Ошибка — " + e; micTestMsgUntil = now + 8000; }
            else if (c != null) {
                int peak = 0; for (int i = 0; i < c.length; i++) { int v = c[i] < 0 ? -c[i] : c[i]; if (v > peak) peak = v; }
                if (peak < 60) { micTestState = 0; micTestMsg = "Тишина — микрофон выключен или занят"; micTestMsgUntil = now + 8000; }
                else {
                    Voice.normalize(c, c.length);
                    Sys.s.playVoice(c, Math.max(30, voiceVol * volume / 100));
                    micTestState = 3; micTestAt = now + c.length * 1000L / Voice.RATE + 300;
                }
            } else if (now - micTestAt > 6000) { micTestState = 0; micTestMsg = "Запись не пришла"; micTestMsgUntil = now + 8000; }
        }
        if (micTestState == 3 && now >= micTestAt) { micTestState = 0; micTestMsg = "Готово. Слышали себя?"; micTestMsgUntil = now + 4000; }
    }

    void voiceTick() {
        micTestTick();
        boolean held = (keys & Binds.A_VOICE) != 0 && state == ST_PLAY && menu == M_NONE && !buyOpen;
        long now = Sys.s.millis();
        if (held && !voiceHeld && !voiceRec && micTestState == 0) {
            if (mp == null || mp.phase != 2) center("Голосовой чат — только в сетевой игре", 2, 0xFFE080);
            else if (!voiceOn) center("Голосовой чат выключен (Настройки → Звук)", 2, 0xFFE080);
            else if (Sys.s.micStart()) { voiceRec = true; voiceRecStart = now; }
            else center("Микрофон недоступен", 2, 0xFF8060);
        }
        if (voiceRec && (!held || now - voiceRecStart >= Voice.MAX_SEC * 1000)) {
            voiceRec = false;
            final boolean tooShort = now - voiceRecStart < 250;
            Sys.s.micStop(new VoiceCb() {
                public void done(short[] pcm, String err) {
                    synchronized (voiceLock) { if (pcm != null && !tooShort) voiceOut = pcm; else if (err != null) voiceErr = err; }
                }
            });
        }
        voiceHeld = held;
        short[] out; String err;
        synchronized (voiceLock) { out = voiceOut; err = voiceErr; voiceOut = null; voiceErr = null; }
        if (err != null) center("Микрофон: " + err, 3, 0xFF8060);
        if (out != null && mp != null && mp.phase == 2) {
            mp.sendVoice(out);
            if (mp.roomPlayers <= 1) center("Голос отправлен, но в комнате больше никого нет", 2, 0xFFE080);
        }
    }

    /** a part of a clip from player pid (sent by the host) */
    void voiceIn(int pid, int seq, int part, int parts, int n, byte[] b, int off) {
        if (pl == null || pid >= pl.length || pl[pid] == me || parts < 1 || parts > 16 || part >= parts) return;
        if (vParts == null || vParts.length != pl.length) {
            int k = pl.length; vParts = new byte[k][][]; vSeq = new int[k]; vGot = new int[k]; vCount = new int[k]; vLen = new int[k][];
            for (int i = 0; i < k; i++) vSeq[i] = -1;
        }
        if (vSeq[pid] != seq || vParts[pid] == null || vParts[pid].length != parts) {
            vSeq[pid] = seq; vParts[pid] = new byte[parts][]; vLen[pid] = new int[parts]; vGot[pid] = 0; vCount[pid] = parts;
        }
        if (vParts[pid][part] != null) return;
        int bytes = (n + 1) / 2;
        if (off + bytes > b.length) return;
        byte[] c = new byte[bytes]; System.arraycopy(b, off, c, 0, bytes);
        vParts[pid][part] = c; vLen[pid][part] = n;
        if (++vGot[pid] < vCount[pid]) return;
        int total = 0;
        for (int i = 0; i < parts; i++) total += vLen[pid][i];
        short[] pcm = new short[total];
        for (int i = 0, at = 0; i < parts; i++) { Voice.decode(vParts[pid][i], 0, vLen[pid][i], pcm, at); at += vLen[pid][i]; }
        vParts[pid] = null; vSeq[pid] = -1;
        if (!soundOn || !voiceOn) return;
        Sys.s.playVoice(pcm, voiceVol * volume / 100);
        talkName = pl[pid].name; talkUntil = Sys.s.millis() + total * 1000L / Voice.RATE + 300;
    }

    void drawVoice() {
        long now = Sys.s.millis();
        String s; int col;
        if (voiceRec) {
            s = "Говорите... " + (int) (Voice.MAX_SEC * 1000 - (now - voiceRecStart) + 999) / 1000 + " с";
            col = (now / 400 & 1) == 0 ? 0xFF4040 : 0xFFA0A0;
        } else if (talkName != null && now < talkUntil) { s = "Говорит: " + talkName; col = 0x80FF80; }
        else return;
        int w = gx.textW(gx.f1, s) + 10;
        // landscape: above the health/ammo bar; portrait: under the radar (chat and kill feed live elsewhere)
        int x = port() ? 4 : W / 2 - w / 2, y = port() ? 92 : H - 52;
        gx.fill(x, y, w, 15, 0, 160);
        gx.text(gx.f1, s, x + 5, y + 2, col);
    }

    void center(String s, float dur, int col) { centerMsg = s; centerMsgUntil = time + dur; centerCol = col; }

    // ======================================================================= input
    /** platform entry: a physical key may map to several actions (e.g. D-pad up = look up + menu up) */
    public void keyMask(int mask, boolean down) {
        if (mask == 0) return;
        boolean menuish = state != ST_PLAY || menu != M_NONE || buyOpen;
        if (menuish && down) {
            keys |= mask;
            int rep = (mask & A_UP) != 0 ? A_UP : (mask & A_DOWN) != 0 ? A_DOWN : (mask & A_SELECT) != 0 ? A_SELECT
                    : (mask & Binds.A_ESC) != 0 ? Binds.A_ESC : (mask & Binds.A_TABL) != 0 ? Binds.A_TABL : (mask & Binds.A_TABR) != 0 ? Binds.A_TABR : (mask & Binds.A_LEFT) != 0 ? Binds.A_LEFT : (mask & Binds.A_RIGHT) != 0 ? Binds.A_RIGHT : (mask & -mask);
            key(rep, true);
            return;
        }
        for (int bit = 1; bit != 0 && bit <= Binds.A_VOICE; bit <<= 1) if ((mask & bit) != 0) {
            key(bit, down);
            if (down && (menu != M_NONE || buyOpen || state != ST_PLAY)) break;   // a menu just opened: the other bits of this key are not for it
        }
    }

    public void key(int action, boolean down) {
        if (down) keys |= action; else keys &= ~action;
        if (!down) { if (action == A_TURNL || action == A_TURNR) turnHeld = 0; return; }
        if (state == ST_MENU || state == ST_END || (state == ST_PLAY && menu != M_NONE)) { menuKey(action); return; }
        if (state != ST_PLAY) return;
        if (buyOpen) {
            if (action == A_MENU || action == A_BUY || action == Binds.A_ESC) { if (buyCat >= 0) buyCat = -1; else buyOpen = false; }
            else if (action == A_UP || action == A_LOOKU) buySel = Math.max(0, buySel - 1);
            else if (action == A_DOWN || action == A_LOOKD) buySel++;
            else if (action == A_SELECT) digit(buySel == 8 ? 9 : buySel + 1);
            return;
        }
        if (action == A_MENU) { menu = M_PAUSE; menuSel = 0; return; }
        if (action == A_SCORE) { scoreOpen = !scoreOpen; return; }
        if (me == null) return;
        if (!me.alive) {
            if (action == A_FIRE || action == A_SELECT || action == A_NEXTW) nextSpec();
            return;
        }
        if (action == A_BUY) {
            if (canBuy(me)) { buyOpen = true; buyCat = -1; buySel = 0; }
            else { menu = M_GAMEMENU; menuSel = 0; }
        }
        else if (action == A_NEXTW) nextWeapon(me);
        else if (action == A_RELOAD) { startReload(me); net('L', -1, -1); }
        else if (action == A_ALT) altFire(me);
        else if (action == Binds.A_LASTW) switchTo(me, me.last);
        else if (action == Binds.A_DROP) { dropCurrent(me); net('D', -1, -1); }
        else if (action == Binds.A_RADIO) { menu = M_RADIO1; menuSel = 0; }
    }

    /** PC: mouse look */
    public void look(float dyaw, float dpitch) {
        if (state != ST_PLAY || menu != M_NONE || me == null || !me.alive || buyOpen) return;
        float zk = me.zoom == 0 ? 1 : me.zoom == 1 ? 0.45f : 0.2f;
        me.yaw = angNorm(me.yaw - dyaw * zk); me.pitch += (invert ? -dpitch : dpitch) * zk;
        if (me.pitch > 89) me.pitch = 89; if (me.pitch < -89) me.pitch = -89;
    }

    /** PC hotkeys */
    public void pcKey(char c) {
        if (state != ST_PLAY || me == null || !me.alive) return;
        if (menu != M_NONE && !isOverlay()) return;
        if (c >= '1' && c <= '5' && menu == M_NONE && !buyOpen) { selectSlot(me, c - '1'); return; }
        if (c == 'x') { menu = M_RADIO2; menuSel = 0; }
        else if (c == 'c') { menu = M_RADIO3; menuSel = 0; }
    }

    boolean isOverlay() { return menu == M_GAMEMENU || menu == M_RADIO1 || menu == M_RADIO2 || menu == M_RADIO3; }
    boolean isPausedMenu() { return state == ST_PLAY && menu != M_NONE && !isOverlay(); }

    /** true when number keys should be treated as menu digits rather than movement */
    public boolean wantsDigits() {
        return state == ST_MENU || state == ST_END || (state == ST_PLAY && (menu != M_NONE || buyOpen));
    }

    public void pauseFromSystem() {
        keys = 0;
        if (state == ST_PLAY && (menu == M_NONE || isOverlay())) { menu = M_PAUSE; menuSel = 0; }
    }

    public void digit(int d) {
        if (state != ST_PLAY || menu != M_NONE) {
            if (state == ST_END) { menuKey(A_SELECT); return; }
            if (menu == M_HELP) { menuKey(A_SELECT); return; }
            if (isOptMenu()) {
                menuKey(d == 2 ? A_UP : d == 8 ? A_DOWN : d == 4 ? A_TURNL : d == 6 ? A_TURNR : d == 5 ? A_SELECT : d == 0 ? A_MENU : 0);
                return;
            }
            String[] it = menuItems();
            if (d == 0) { menuKey(A_MENU); return; }
            if (d >= 1 && d <= it.length) { menuSel = d - 1; menuKey(A_SELECT); }
            else if (d == 5) menuKey(A_SELECT);
            return;
        }
        if (!buyOpen) return;
        if (buyCat < 0) {
            if (d >= 1 && d <= 5) { buyCat = d - 1; buySel = 0; }
            else if (d == 6) { buyAmmo(me, 0); net('B', 2, 0); }
            else if (d == 7) { buyAmmo(me, 1); net('B', 2, 1); }
            else if (d == 8) { buyCat = 7; buySel = 0; }
            else if (d == 9) { buyOpen = false; menu = M_GAMEMENU; menuSel = 0; }
            else if (d == 0) buyOpen = false;
        } else if (buyCat == 7) {
            if (d == 0) { buyCat = -1; return; }
            if (buyEquip(me, d - 1)) net('B', 1, d - 1);
        } else {
            if (d == 0) { buyCat = -1; return; }
            int[] list = Wp.menu[buyCat];
            if (d - 1 < list.length) { if (buyWeapon(me, list[d - 1])) { buyOpen = false; net('B', 0, list[d - 1]); } }
        }
    }

    // ======================================================================= menus
    static final int M_CREATE = 1, M_GFX = 6, M_CTRL = 7, M_SND = 8, M_FIRST1 = 9, M_FIRST2 = 10, M_GAMEMENU = 11, M_RADIO1 = 12, M_RADIO2 = 13, M_RADIO3 = 14;
    static final String[] CROSS = {"зелёный", "красный", "синий", "жёлтый", "голубой"};
    static final int[] CROSSC = {0x32FA32, 0xFA3232, 0x3232FA, 0xFAFA32, 0x32FAFA};
    static String onoff(boolean b) { return b ? "вкл" : "выкл"; }
    static String mmss(int s) { return s / 60 + ":" + (s % 60 < 10 ? "0" : "") + s % 60; }
    String resName(int l) { return (LW[l] * W / 320) + "x" + (LW[l] * H / 320); }
    /** portrait phone screen (240x320) */
    boolean port() { return H > W; }
    String qualName() { return quality == 0 ? "Авто (~20 FPS)" : quality == 1 ? "Максимум " + resName(0) : quality == 2 ? "Среднее " + resName(2) : "Низкое " + resName(4); }

    String[] menuItems() {
        switch (menu) {
            case M_MAIN: return new String[]{"Новая игра", "Мультиплеер", "Настройки", "Выход"};
            case M_MP: return new String[]{"Режим: " + MP.MODEN[mpMode], "Карта: " + MAPS[mpMap], "Ник: " + mpName,
                    "Сервер: " + (mpServer.length() == 0 ? "авто" : mpServer),
                    "> Найти игру по всему миру", "> Создать свою игру",
                    "> Создать игру по LAN (Wi-Fi)", "> Найти игру по LAN (Wi-Fi)", "> Одиночная игра (с ботами)", "Отмена"};
            case M_LAN: {
                lanList = lanFinder != null ? lanFinder.list() : new LanFinder.Game[0];
                String[] a = new String[Math.max(1, lanList.length) + 1];
                if (lanList.length == 0) a[0] = lanFinder != null && lanFinder.error != null ? lanFinder.error : "Ищем игры в вашей Wi-Fi сети...";
                for (int i = 0; i < lanList.length; i++) { LanFinder.Game x = lanList[i]; a[i] = "> " + x.name + "\t" + x.players + "/10\t" + MP.MODEN[x.mode % 3] + ", " + MAPS[x.map % 3]; }
                a[a.length - 1] = "Отмена";
                return a;
            }
            case M_WORLD: {
                worldList = world != null ? world.rooms() : new MqttLink.Room[0];
                int n = worldList.length;
                String[] a = new String[(n == 0 ? 1 : n) + 3];
                if (n == 0) a[0] = world != null && world.error != null ? world.error
                        : world != null && world.searching() ? "Ищем игры по всему миру..." : "Пока никто не играет — создайте свою игру";
                for (int i = 0; i < n; i++) a[i] = roomLine(worldList[i]);
                int k = n == 0 ? 1 : n;
                a[k] = "> Быстрая игра (подобрать автоматически)"; a[k + 1] = "> Создать свою игру"; a[k + 2] = "Отмена";
                if (worldK > 0 && k != worldK && menuSel >= worldK) menuSel += k - worldK;   // keep the cursor on the same button when rooms come and go
                worldK = k;
                if (n == 0 && menuSel == 0) menuSel = 1;   // the status line is not a button
                if (menuSel >= a.length) menuSel = a.length - 1;
                if (menuSel < 0) menuSel = 0;
                return a;
            }
            case M_SEARCH: return new String[]{mp == null ? "Назад" : "Отмена"};
            case M_PAUSE: return new String[]{"Вернуться к игре", "Сменить команду", "Настройки", "Отключиться", "Выход"};
            case M_BINDS: {
                String[] a = new String[Binds.N + 2];
                for (int i = 0; i < Binds.N; i++) a[i] = Binds.NAME[i] + "\t" + keysOf(i);
                a[Binds.N] = "Сбросить"; a[Binds.N + 1] = "OK";
                return a;
            }
            case M_CREATE: {
                int tot = cBots + 1, mine = (tot + 1) / 2;
                return new String[]{"Карта: " + MAPS[cMap], "Команда: " + (cTeam == 1 ? "Террористы" : cTeam == 2 ? "Спецназ" : "Автовыбор"),
                        "Сложность ботов: " + diffName(), "Ботов: " + cBots + " (" + mine + " на " + (tot - mine) + ")", "Раундов в матче: " + maxRounds,
                        "Время раунда: " + mmss(cRoundTime), "Время закупки: " + cBuyTime + " с", "Фризтайм: " + cFreeze + " с",
                        "Стартовые деньги: $" + cMoney, "Огонь по своим: " + onoff(cFF), "Начать", "Отмена"};
            }
            case M_TEAM: return new String[]{"Террористы", "Спецназ", "Автовыбор", "Отмена"};
            case M_GFX: {
                String[] a = {"Графика: " + GFXN[gfx], "Разрешение: " + (nfr > 0 ? "DLSS " + r.W + "x" + r.H : qualName()), "Nokia FR и DLSS: " + nfrName(), "Яркость: " + (gammaIdx + 1) + "/5", "Покачивание оружия: " + onoff(viewBobOn),
                        "Цвет прицела: " + CROSS[crossCol], "Размер прицела: " + (crossSize == 0 ? "малый" : crossSize == 1 ? "средний" : "большой"),
                        "Динамический прицел: " + onoff(crossDyn), "Радар: " + onoff(radarOn), "Показывать FPS: " + onoff(showFps), "OK"};
                if (pc) a = new String[]{a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], "Экран: " + ORIENTN[orient], "Размер окна: x" + winScale,
                        "Лимит FPS: " + (fpsCap20 ? "20 (как на E52)" : "60"), "OK"};
                else a = new String[]{a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], a[9], "Экран: " + ORIENTN[orient],
                        "Лимит FPS: " + (fpsCap20 ? "20 (меньше греется)" : "без лимита"), "OK"};
                return a;
            }
            case M_CTRL: return new String[]{
                    pc ? "Схема: " + (pcMouse ? "клавиатура + мышь" : "как на телефоне, " + layoutName()) : "Раскладка: " + layoutName(),
                    "Чувствительность: " + sens, "Вверх/вниз: " + sensV, (pc && pcMouse ? "Инверсия мыши: " : "Инверсия взгляда: ") + onoff(invert),
                    "Помощь прицеливания: " + onoff(autoAim), "OK"};
            case M_LANG: return new String[]{(Lang.lang == Lang.EN ? "> " : "   ") + "English", (Lang.lang == Lang.RU ? "> " : "   ") + "Русский", "OK"};
            case M_SND: return new String[]{"Звук: " + onoff(soundOn), "Громкость: " + volume + "%", "Голосовой чат: " + onoff(voiceOn), "Громкость голоса: " + voiceVol + "%", micTestLabel(), "OK"};
            case M_FIRST1: return pc ? new String[]{"Клавиатура + мышь (как в CS)", "Как на телефоне (цифры и стрелки)"}
                    : new String[]{"Классика: 2468 ходьба", "Альтернатива: джойстик ходьба"};
            case M_FIRST2: return new String[]{"Авто (держит ~20 FPS)", "Максимум " + resName(0), "Среднее " + resName(2), "Низкое " + resName(4)};
            case M_HELP: return new String[]{"Назад"};
            case M_GAMEMENU: return new String[]{"Радио: команды", "Радио: группе", "Радио: ответы", "Выбросить оружие", "Глушитель вкл/выкл", "Закупка", "Закрыть"};
            case M_RADIO1: return radioItems(0, 6);
            case M_RADIO2: return radioItems(6, 12);
            case M_RADIO3: return radioItems(12, 21);
        }
        return new String[0];
    }

    String[] radioItems(int a, int b) {
        String[] r2 = new String[b - a];
        for (int i = a; i < b; i++) r2[i - a] = RT[i];
        return r2;
    }

    String menuTitle() {
        switch (menu) {
            case M_CREATE: return "Новая игра";
            case M_MP: return "Мультиплеер";
            case M_SEARCH: return "Поиск игры";
            case M_LAN: return "Игры по LAN (Wi-Fi)";
            case M_WORLD: return "Игры по всему миру";
            case M_TEAM: return "Выберите команду";
            case M_GFX: case M_CTRL: case M_BINDS: case M_SND: case M_LANG: return "Настройки";
            case M_FIRST1: return "Первый запуск — управление";
            case M_FIRST2: return "Первый запуск — разрешение";
            case M_GAMEMENU: return "Меню";
            case M_RADIO1: return "Радио: команды";
            case M_RADIO2: return "Радио: группе";
            case M_RADIO3: return "Радио: ответы";
        }
        return "";
    }

    String diffName() { return difficulty == 0 ? "лёгкие" : difficulty == 1 ? "обычные" : difficulty == 2 ? "сложные" : "эксперт"; }

    int optReturn = M_MAIN;

    /** change option value; returns true if the item is an option */
    boolean changeOpt(int dir) {
        switch (menu) {
            case M_MP:
                if (menuSel == 0) { mpMode = (mpMode + 3 + dir) % 3; return true; }
                if (menuSel == 1) { mpMap = (mpMap + 3 + dir) % 3; return true; }
                return false;
            case M_CREATE:
                switch (menuSel) {
                    case 0: cMap = (cMap + 3 + dir) % 3; return true;
                    case 1: cTeam = (cTeam + 2 + dir) % 3 + 1; return true;
                    case 2: difficulty = (difficulty + 4 + dir) % 4; return true;
                    case 3: cBots += dir; if (cBots > 9) cBots = 1; if (cBots < 1) cBots = 9; return true;
                    case 4: maxRounds += dir * 3; if (maxRounds > 30) maxRounds = 3; if (maxRounds < 3) maxRounds = 30; return true;
                    case 5: cRoundTime += dir * 15; if (cRoundTime > 300) cRoundTime = 60; if (cRoundTime < 60) cRoundTime = 300; return true;
                    case 6: cBuyTime += dir * 15; if (cBuyTime > 90) cBuyTime = 15; if (cBuyTime < 15) cBuyTime = 90; return true;
                    case 7: cFreeze += dir; if (cFreeze > 10) cFreeze = 0; if (cFreeze < 0) cFreeze = 10; return true;
                    case 8: cMoney += dir * (cMoney >= 4000 ? 1000 : 400); if (cMoney > 16000) cMoney = 800; if (cMoney < 800) cMoney = 16000; if (cMoney > 4000 && cMoney % 1000 != 0) cMoney = (cMoney / 1000) * 1000; return true;
                    case 9: cFF = !cFF; return true;
                }
                return false;
            case M_GFX:
                switch (menuSel) {
                    case 0: setGfx((gfx + 4 + dir) % 4); return true;
                    case 1: if (nfr > 0) return true; setQuality((quality + 4 + dir) % 4); return true;
                    case 2: nfr = (nfr + 6 + dir) % 6; genValid = false; if (nfr == 0) setQuality(quality); return true;
                    case 3: gammaIdx = (gammaIdx + 5 + dir) % 5; applyGamma(); return true;
                    case 4: viewBobOn = !viewBobOn; return true;
                    case 5: crossCol = (crossCol + 5 + dir) % 5; return true;
                    case 6: crossSize = (crossSize + 3 + dir) % 3; return true;
                    case 7: crossDyn = !crossDyn; return true;
                    case 8: radarOn = !radarOn; return true;
                    case 9: showFps = !showFps; return true;
                    case 10: orient = 1 - orient; return true;
                    case 11: if (pc) { winScale = (winScale + 3 + dir) % 4 + 1; return true; } fpsCap20 = !fpsCap20; return true;
                    case 12: if (pc) { fpsCap20 = !fpsCap20; return true; } return false;
                }
                return false;
            case M_CTRL:
                switch (menuSel) {
                    case 0:
                        if (!pc) { layout = layout == 0 ? 1 : 0; bPhone.phoneDefaults(layout); return true; }
                        // PC: клавиатура + мышь -> как на телефоне (классическая) -> (альтернативная) -> ...
                        int st = pcMouse ? 0 : layout == 1 ? 2 : 1;
                        st = (st + 3 + dir) % 3;
                        if (st == 0) pcMouse = true;
                        else { pcMouse = false; layout = st - 1; bPhone.phoneDefaults(layout); }
                        return true;
                    case 1: sens += dir; if (sens > 10) sens = 1; if (sens < 1) sens = 10; return true;
                    case 2: sensV += dir; if (sensV > 10) sensV = 1; if (sensV < 1) sensV = 10; return true;
                    case 3: invert = !invert; return true;
                    case 4: autoAim = !autoAim; return true;
                }
                return false;
            case M_BINDS:
                if (menuSel < Binds.N && binds().k2[menuSel] != 0) { binds().clear2(menuSel); if (!(pc && pcMouse)) layout = 2; return true; }
                return menuSel < Binds.N;
            case M_SND:
                switch (menuSel) {
                    case 0: soundOn = !soundOn; if (!soundOn) Sys.s.stopSounds(); return true;
                    case 1: volume += dir * 10; if (volume > 100) volume = 10; if (volume < 10) volume = 100; return true;
                    case 2: voiceOn = !voiceOn; return true;
                    case 3: voiceVol += dir * 10; if (voiceVol > 100) voiceVol = 10; if (voiceVol < 10) voiceVol = 100; return true;
                }
                return false;
        }
        return false;
    }

    void menuKey(int a) {
        if (state == ST_END) { if (a == A_SELECT || a == A_FIRE || a == A_MENU || a == Binds.A_ESC) { uiClick(); if (mp != null) mpLeave(); else nextMapAfterEnd(); } return; }
        String[] it = menuItems();
        boolean left = a == A_TURNL || a == A_STRL || a == Binds.A_LEFT, right = a == A_TURNR || a == A_STRR || a == Binds.A_RIGHT;
        boolean up = a == A_UP || a == A_LOOKU || a == A_FWD, down = a == A_DOWN || a == A_LOOKD || a == A_BACK;
        // ---- tabbed options dialog: Видео / Управление / Клавиши / Звук
        int tab = optTab();
        if (tab >= 0 && bindCapture < 0) {
            if (a == Binds.A_TABL || a == Binds.A_TABR) { switchTab(tab + (a == Binds.A_TABL ? -1 : 1)); return; }
            if (tabFocus) {
                if (left || right) { switchTab(tab + (left ? -1 : 1)); tabFocus = true; return; }
                if (down || a == A_SELECT || a == A_FIRE) { tabFocus = false; menuSel = 0; uiRoll(); return; }
                if (up) return;
            } else if (up && menuSel == 0) { tabFocus = true; uiRoll(); return; }
        }
        boolean skip0 = menu == M_WORLD && worldList.length == 0;   // the "searching..." line is not a button
        if (up) { menuSel = (menuSel + it.length - 1) % it.length; if (skip0 && menuSel == 0) menuSel = it.length - 1; uiRoll(); return; }
        if (down) { menuSel = (menuSel + 1) % it.length; if (skip0 && menuSel == 0) menuSel = 1; uiRoll(); return; }
        if (a == Binds.A_RADIO && (menu == M_RADIO1 || menu == M_RADIO2)) { menu++; menuSel = 0; return; }
        if (a == Binds.A_RADIO && menu == M_RADIO3) { menu = M_NONE; return; }
        if (a == Binds.A_ESC) a = A_MENU;
        if (a == A_MENU || a == A_SCORE && isOverlay()) {
            switch (menu) {
                case M_PAUSE: menu = M_NONE; break;
                case M_TEAM: menu = state == ST_PLAY ? M_PAUSE : M_MAIN; menuSel = state == ST_PLAY ? 1 : 0; break;
                case M_GFX: case M_CTRL: case M_SND: case M_BINDS: case M_LANG: closeOptions(); break;
                case M_FIRST1: case M_FIRST2: break;
                case M_GAMEMENU: menu = M_NONE; break;
                case M_RADIO1: case M_RADIO2: case M_RADIO3: menu = M_NONE; break;
                case M_CREATE: saveSettings(); menu = M_MAIN; menuSel = 0; break;
                case M_MP: menu = M_MAIN; menuSel = 1; break;
                case M_LAN: closeFinder(); menu = M_MP; menuSel = 7; break;
                case M_WORLD: closeWorld(); menu = M_MP; menuSel = 4; break;
                case M_SEARCH: mpCancel(); break;
                default: if (state == ST_PLAY) menu = M_NONE; else { menu = M_MAIN; menuSel = 0; }
            }
            uiClick();
            return;
        }
        if ((left || right) && changeOpt(left ? -1 : 1)) { uiRoll(); return; }
        if (a != A_SELECT && a != A_FIRE) return;
        uiClick();
        if (menu != M_CREATE && menu != M_BINDS || menu == M_CREATE && menuSel < 10) if (changeOpt(1)) return;
        switch (menu) {
            case M_FIRST1:
                if (pc) pcMouse = menuSel == 0; else { layout = menuSel; bPhone.phoneDefaults(layout); }
                menu = M_FIRST2; menuSel = 0; break;
            case M_FIRST2:
                setQuality(menuSel); firstRunDone = true; saveSettings(); menu = M_MAIN; menuSel = 0; break;
            case M_MAIN:
                if (menuSel == 0) { resetCreate(); menu = M_CREATE; menuSel = 10; }
                else if (menuSel == 1) openMP();
                else if (menuSel == 2) openOptions(M_MAIN);
                else { saveSettings(); Sys.s.quit(); }
                break;
            case M_MP:
                if (menuSel == 2) Sys.s.editText("Ваш ник", mpName, 20, new TextCb() { public void done(String t) { if (t != null && t.trim().length() > 0) { mpName = t.trim(); saveSettings(); } } });
                else if (menuSel == 3) editServer(false);
                else if (menuSel == 4) { if (mpServer.trim().length() == 0) openWorld(); else startSearch(false); }   // own server: as before
                else if (menuSel == 5) startSearch(true);
                else if (menuSel == 6) startLan();
                else if (menuSel == 7) { lanFinder = new LanFinder(); menu = M_LAN; menuSel = 0; }
                else if (menuSel == 8) { resetCreate(); mode = mpMode; cMap = mpMap; startMatch(mpMap, mode == 2 ? 2 : 1 + (rnd.nextInt() & 1)); }
                else if (menuSel == 9) { menu = M_MAIN; menuSel = 1; }
                break;
            case M_LAN:
                if (menuSel < lanList.length) {
                    LanFinder.Game x = lanList[menuSel];
                    closeFinder();
                    mpMode = x.mode % 3; mpMap = x.map % 3; mpErr = null; mpCreate = false;
                    mp = new MP(this, x.ip + ":" + x.port, mpMode, mpMap, mpName, false); mp.lanGame = true;
                    menu = M_SEARCH; menuSel = 0;
                } else if (menuSel == lanList.length || lanList.length == 0 && menuSel == 1) { closeFinder(); menu = M_MP; menuSel = 7; }
                break;
            case M_WORLD: {
                int n = worldList.length, k = n == 0 ? 1 : n;
                if (menuSel < n) {
                    MqttLink.Room x = worldList[menuSel];
                    closeWorld();
                    mpMode = x.mode; mpMap = x.map; mpErr = null; mpCreate = false; saveSettings();
                    mp = new MP(this, MqttLink.open(x.id), mpMode, mpMap, mpName, false);
                    menu = M_SEARCH; menuSel = 0;
                } else if (menuSel == k) { closeWorld(); startSearch(false); }
                else if (menuSel == k + 1) { closeWorld(); startSearch(true); }
                else if (menuSel == k + 2) { closeWorld(); menu = M_MP; menuSel = 4; }
                break;
            }
            case M_SEARCH:
                mpCancel();
                break;
            case M_CREATE:
                if (menuSel == 11) { saveSettings(); menu = M_MAIN; menuSel = 0; break; }
                if (menuSel == 10) { saveSettings(); mode = 0; startMatch(cMap, cTeam == 3 ? 1 + (rnd.nextInt() & 1) : cTeam); }
                break;
            case M_TEAM:
                if (menuSel == 3) { menu = M_PAUSE; menuSel = 1; break; }
                switchTeam(menuSel == 2 ? 1 + (rnd.nextInt() & 1) : menuSel + 1); menu = M_NONE; break;
            case M_LANG:
                if (menuSel < 2) { Lang.set(menuSel == 0 ? Lang.EN : Lang.RU); saveSettings(); uiClick(); }
                else closeOptions();
                break;
            case M_GFX: case M_CTRL: case M_SND:
                if (menu == M_SND && menuSel == 4) { micTest(); break; }
                if (menuSel == it.length - 1) closeOptions();
                break;
            case M_BINDS:
                if (menuSel < Binds.N) { bindCapture = menuSel; bindUntil = System.currentTimeMillis() + 6000; }
                else if (menuSel == Binds.N) {
                    if (pc && pcMouse) bPC.pcDefaults(); else { if (layout == 2) layout = 0; bPhone.phoneDefaults(layout); }
                    saveSettings();
                } else closeOptions();
                break;
            case M_HELP: menu = helpReturn; helpReturn = M_MAIN; menuSel = 0; break;
            case M_PAUSE:
                if (menuSel == 0) menu = M_NONE;
                else if (menuSel == 1) { if (mp != null) { menu = M_NONE; center("В мультиплеере команды распределяет сервер", 2, 0xFFFFFF); } else { menu = M_TEAM; menuSel = me != null && me.team == 2 ? 1 : 0; } }
                else if (menuSel == 2) openOptions(M_PAUSE);
                else if (menuSel == 3) { if (mp != null) mpLeave(); else { state = ST_MENU; menu = M_MAIN; menuSel = 0; Sys.s.stopSounds(); } }
                else { saveSettings(); Sys.s.quit(); }
                break;
            case M_GAMEMENU:
                menu = M_NONE;
                if (me == null || !me.alive) break;
                if (menuSel <= 2) { menu = M_RADIO1 + menuSel; menuSel = 0; }
                else if (menuSel == 3) { dropCurrent(me); net('D', -1, -1); }
                else if (menuSel == 4) { if (me.cur == Wp.M4A1 || me.cur == Wp.USP) altFire(me); else center("Глушитель есть только у M4A1 и USP", 1.5f, 0xFFFFFF); }
                else if (menuSel == 5) { if (canBuy(me)) { buyOpen = true; buyCat = -1; buySel = 0; } else center("Здесь нельзя покупать", 1.5f, 0xFFFFFF); }
                break;
            case M_RADIO1: case M_RADIO2: case M_RADIO3: {
                int base = menu == M_RADIO1 ? 0 : menu == M_RADIO2 ? 6 : 12;
                menu = M_NONE;
                if (me != null) radio(me, base + menuSel);
                break;
            }
        }
    }

    // ---- options dialog helpers
    static final int[] OPT_TABS = {6, 7, 15, 8, 19};   // M_GFX, M_CTRL, M_BINDS, M_SND, M_LANG
    static final String[] OPT_TAB_NAMES = {"Видео", "Управление", "Клавиши", "Звук", "Язык"};
    static final String[] OPT_TAB_SHORT = {"Видео", "Упр.", "Клав.", "Звук", "Язык"};
    public boolean tabFocus; int optReturnSel;
    int optTab() { for (int i = 0; i < OPT_TABS.length; i++) if (OPT_TABS[i] == menu) return i; return -1; }
    void switchTab(int t) { int n = OPT_TABS.length; t = (t + n) % n; menu = OPT_TABS[t]; menuSel = Lang.lang == Lang.RU && menu == M_LANG ? 1 : 0; uiRoll(); }
    void openOptions(int ret) { optReturn = ret; optReturnSel = menuSel; menu = M_GFX; menuSel = 0; tabFocus = false; }
    void closeOptions() { saveSettings(); menu = optReturn; menuSel = optReturnSel; tabFocus = false; bindCapture = -1; }
    void uiRoll() { if (soundOn) Sys.s.sound("ui_buttonrollover", volume * 6 / 10); }
    void uiClick() { if (soundOn) Sys.s.sound("ui_buttonclick", volume * 7 / 10); }
    int helpReturn = M_MAIN;
    public boolean pcMouse = true, fpsCap20;
    // ---- key bindings
    public final Binds bPhone = new Binds(), bPC = new Binds();
    public int bindCapture = -1; long bindUntil; int bindsReturn = M_CTRL;
    static final int M_BINDS = 15;
    String layoutName() { return layout == 0 ? "классическая" : layout == 1 ? "альтернативная" : "своя"; }
    public Binds binds() { return pc && pcMouse ? bPC : bPhone; }
    public int bindMask(int code) { return binds().mask(code); }
    public String keyName(int c) {
        if (c == 0) return "—";
        if (pc && pcMouse) { String n = Sys.s.keyName(c); if (n != null) return n; }
        return Binds.phoneName(c);
    }
    String keysOf(int i) {
        Binds b = binds();
        return b.k1[i] == 0 ? "—" : b.k2[i] == 0 ? keyName(b.k1[i]) : keyName(b.k1[i]) + ", " + keyName(b.k2[i]);
    }
    /** platform calls this first for every key press; returns true when the key was consumed (binding mode) */
    public boolean rawKey(int code) {
        if (bindCapture < 0) return false;
        if (System.currentTimeMillis() > bindUntil) { bindCapture = -1; return false; }
        if (pc && pcMouse && code == 27) { bindCapture = -1; return true; }   // Esc cancels on PC
        binds().bind(bindCapture, code);
        if (!(pc && pcMouse)) layout = 2;
        bindCapture = -1; keys = 0;
        saveSettings();
        return true;
    }
    boolean isOptMenu() { return menu == M_CREATE || menu == M_GFX || menu == M_CTRL || menu == M_SND || menu == M_BINDS || menu == M_LANG; }

    // ======================================================================= loading
    void startMatch(int mapI, int team) {
        mapIdx = mapI; pendingTeam = team;
        state = ST_LOAD; loadMsg = "Загрузка " + MAPS[mapI] + "...";
        uiPic = null; uiPicName = null;
        menu = M_NONE;
        loadStage = 0;
    }
    int loadStage;

    void doLoad() throws IOException {
        mapName = MAPS[mapIdx];
        m = null; nav = null; r.m = null;
        for (int i = 0; i < vws.length; i++) vws[i] = null;
        System.gc();
        m = new MapData(Sys.s.open("/" + mapName + ".cse"));
        nav = new Nav(Sys.s.open("/" + mapName + ".nv"));
        load3D();
        if (!use3D()) loadSprites();
        r.setMap(m);
        roundTime = cRoundTime; buyTimeS = cBuyTime; freezeS = cFreeze; startMoney = cMoney; friendlyFire = cFF;
        if (mode == 1) { roundTime = 600; freezeS = 2; buyTimeS = 600; startMoney = 16000; maxRounds = 1; friendlyFire = false; }
        if (mode == 2) { roundTime = 300; freezeS = 2; buyTimeS = 45; startMoney = 4000; maxRounds = 10; friendlyFire = false; }
        mv = new Move(m);
        setupEntities();
        setupPlayers(pendingTeam);
        round = 0; scoreT = scoreCT = 0; lossStreakT = lossStreakCT = 0;
        time = 0;
        newRound(true);
        state = ST_PLAY; menu = M_NONE;
        if (mp != null) { mp.onLoaded(); if (mp.host) mp.hostLoaded(); }
    }

    void setupEntities() {
        int n = m.numEnts();
        float[] t = new float[n * 4], c = new float[n * 4]; int nt = 0, nc = 0;
        int[] de = new int[n], sm = new int[n], st = new int[n], bz = new int[n], br = new int[n], ld = new int[n];
        int nde = 0, nsm = 0, nst = 0, nbz = 0, nbr = 0, nld = 0;
        for (int e = 0; e < n; e++) {
            String cl = m.ent(e, "classname"); if (cl == null) continue;
            String org = m.ent(e, "origin"), mdl = m.ent(e, "model");
            if (cl.equals("info_player_deathmatch") || cl.equals("info_player_start")) {
                float[] o = vec(org); String an = m.ent(e, "angles");
                float yaw = an != null ? vec(an)[1] : 0;
                float[] dst = cl.equals("info_player_deathmatch") ? t : c;
                int k = cl.equals("info_player_deathmatch") ? nt++ : nc++;
                dst[k * 4] = o[0]; dst[k * 4 + 1] = o[1]; dst[k * 4 + 2] = o[2]; dst[k * 4 + 3] = yaw;
            }
            int mi = mdl != null && mdl.startsWith("*") ? Integer.parseInt(mdl.substring(1)) : -1;
            if (mi < 0) continue;
            if (cl.equals("func_wall") || cl.equals("func_illusionary") || cl.equals("func_breakable") || cl.equals("func_door") || cl.equals("func_wall_toggle")) de[nde++] = mi;
            if (cl.equals("func_wall") || cl.equals("func_breakable") || cl.equals("func_door") || cl.equals("func_wall_toggle")) sm[nsm++] = mi;
            if (cl.equals("func_breakable")) br[nbr++] = mi;
            if (cl.equals("func_bomb_target")) st[nst++] = mi;
            if (cl.equals("func_buyzone")) bz[nbz++] = mi;
            if (cl.equals("func_ladder")) ld[nld++] = mi;
        }
        spawnT = new float[nt * 4]; System.arraycopy(t, 0, spawnT, 0, nt * 4);
        spawnCT = new float[nc * 4]; System.arraycopy(c, 0, spawnCT, 0, nc * 4);
        drawEnts = sub(de, nde); m.solidModels = sub(sm, nsm); siteModel = sub(st, nst); buyModel = sub(bz, nbz); breakModel = sub(br, nbr);
        mv.ladders = sub(ld, nld);
        m.modelActive = new boolean[m.numModels];
        for (int i = 0; i < m.numModels; i++) m.modelActive[i] = true;
        // site names: sort by x descending -> A first (dust2: A is east)
        siteName = new String[siteModel.length];
        for (int i = 0; i < siteModel.length; i++) {
            int rank = 0;
            for (int j = 0; j < siteModel.length; j++) if (j != i && mcx(siteModel[j]) > mcx(siteModel[i])) rank++;
            siteName[i] = rank == 0 ? "A" : "B";
        }
        buyTeam = new int[buyModel.length];
        for (int i = 0; i < buyModel.length; i++) {
            int bm = buyModel[i];
            buyTeam[i] = 0;
            for (int k = 0; k < spawnT.length / 4; k++) if (inModel(bm, spawnT[k * 4], spawnT[k * 4 + 1], spawnT[k * 4 + 2], 40)) buyTeam[i] = 1;
            for (int k = 0; k < spawnCT.length / 4; k++) if (inModel(bm, spawnCT[k * 4], spawnCT[k * 4 + 1], spawnCT[k * 4 + 2], 40)) buyTeam[i] = 2;
        }
    }

    float mcx(int md) { return (m.mMins[md * 3] + m.mMaxs[md * 3]) * 0.5f; }
    float mcy(int md) { return (m.mMins[md * 3 + 1] + m.mMaxs[md * 3 + 1]) * 0.5f; }
    float mcz(int md) { return (m.mMins[md * 3 + 2] + m.mMaxs[md * 3 + 2]) * 0.5f; }

    boolean inModel(int md, float x, float y, float z, float pad) {
        return x > m.mMins[md * 3] - pad && x < m.mMaxs[md * 3] + pad && y > m.mMins[md * 3 + 1] - pad && y < m.mMaxs[md * 3 + 1] + pad
                && z > m.mMins[md * 3 + 2] - pad - 36 && z < m.mMaxs[md * 3 + 2] + pad + 36;
    }

    static int[] sub(int[] a, int n) { int[] r2 = new int[n]; System.arraycopy(a, 0, r2, 0, n); return r2; }

    static float[] vec(String s) {
        float[] v = new float[3];
        if (s == null) return v;
        int k = 0, st = 0;
        s = s.trim() + " ";
        for (int i = 0; i < s.length() && k < 3; i++) {
            if (s.charAt(i) == ' ') { if (i > st) { try { v[k++] = Float.parseFloat(s.substring(st, i)); } catch (Exception e) { k++; } } st = i + 1; }
        }
        return v;
    }

    void setupPlayers(int humanTeam) {
        int tot = cBots + 1, mine = (tot + 1) / 2;
        pl = new Player[tot];
        int[] names = new int[BOTNAMES.length]; for (int i = 0; i < names.length; i++) names[i] = i;
        for (int i = names.length - 1; i > 0; i--) { int j = rnd.nextInt(i + 1); if (j < 0) j = -j; int tt = names[i]; names[i] = names[j]; names[j] = tt; }
        int ni = 0;
        for (int i = 0; i < tot; i++) {
            Player p = new Player(); p.id = i;
            if (i == 0) { p.name = "Игрок"; p.team = humanTeam; me = p; }
            else {
                p.isBot = true; p.name = BOTNAMES[names[ni++ % names.length]];
                p.team = i < mine ? humanTeam : 3 - humanTeam;
                p.ai = new Bot(this, p);
            }
            p.money = startMoney; p.secondary = p.team == 1 ? Wp.GLOCK : Wp.USP;
            p.clip[p.secondary] = Wp.clip[p.secondary]; p.ammo[p.secondary] = p.team == 1 ? 40 : 24;
            p.cur = p.secondary;
            pl[i] = p;
        }
    }

    void switchTeam(int t) {
        if (me.team == t) return;
        // swap with a bot of the other team
        for (int i = 1; i < pl.length; i++) if (pl[i].team == t) { pl[i].team = me.team; break; }
        me.team = t;
        me.alive = false; me.health = 0;
        center("Вы перейдёте в команду " + TNAMES[t - 1] + " в следующем раунде", 3, 0xFFFFFF);
    }

    // ======================================================================= rounds
    void newRound(boolean first) {
        round++;
        roundOver = false; roundEndAt = -1;
        bombPlanted = false; bombDropped = false; bombExploded = false; bombSite = -1;
        for (int i = 0; i < MAXG; i++) grOn[i] = false;
        for (int i = 0; i < MAXS; i++) smOn[i] = false;
        for (int i = 0; i < m.numModels; i++) m.modelActive[i] = true;
        roundStart = time; freezeEnd = time + freezeS; buyEnd = time + freezeS + buyTimeS;
        for (int i = 0; i < MAXD; i++) dOn[i] = false;
        for (int i = 0; i < chatT.length; i++) chatT[i] = -100;
        int ti = 0, ci = 0;
        int nT = spawnT.length / 4, nC = spawnCT.length / 4;
        int offT = rnd.nextInt() & 0xFFFF, offC = rnd.nextInt() & 0xFFFF;
        if (mode == 2) {   // zombie: everyone starts human, the infection comes after a few seconds
            infected = false; infectAt = time + 14;   // one random player turns after 14 s
            for (int i = 0; i < pl.length; i++) {
                Player p = pl[i];
                if (p.zombie) { p.zombie = false; p.alive = false; }
                p.team = 2;
            }
        }
        for (int i = 0; i < pl.length; i++) {
            Player p = pl[i];
            p.spawnSeq++;
            if (mode == 1) { p.money = 16000; p.armor = 100; p.helmet = true; }
            if (mode == 2) p.money = 16000;   // zombie: every human can buy a rifle each round
            if (!p.alive && !first) {
                // died last round: reset loadout to default pistol
                p.primary = -1; p.he = p.flash = p.smoke = 0; p.armor = 0; p.helmet = false; p.defuser = false;
                p.secondary = p.team == 1 ? Wp.GLOCK : Wp.USP; p.silUSP = false; p.silM4 = false;
                p.clip[p.secondary] = Wp.clip[p.secondary]; p.ammo[p.secondary] = p.team == 1 ? 40 : 24;
            }
            if (first) { p.primary = -1; p.he = p.flash = p.smoke = 0; p.armor = 0; p.helmet = false; p.defuser = false; }
            if (p.team == 1 && p.secondary == Wp.USP && first) p.secondary = Wp.GLOCK;
            p.c4 = false;
            p.resetForRound();
            float[] sp; int k;
            if (p.team == 1) { sp = spawnT; k = ((ti++) + offT) % nT; } else { sp = spawnCT; k = ((ci++) + offC) % nC; }
            p.ox = sp[k * 4]; p.oy = sp[k * 4 + 1]; p.oz = sp[k * 4 + 2] + 1; p.yaw = sp[k * 4 + 3]; p.pitch = 0;
            // drop to floor
            m.trace(1, p.ox, p.oy, p.oz, p.ox, p.oy, p.oz - 64);
            if (!m.rStartSolid) p.oz = m.rEz;
            mv.categorize(p);
            p.cur = p.primary >= 0 ? p.primary : p.secondary >= 0 ? p.secondary : Wp.KNIFE;
            p.drawEnd = time + 0.5f; setVm(p, "draw", Wp.drawT[p.cur]);
            if (p.ai != null) p.ai.newRound();
        }
        // give C4 to a random terrorist (bot preferred if human is T? random)
        int nTerr = 0; for (int i = 0; i < pl.length; i++) if (pl[i].team == 1) nTerr++;
        if (nTerr > 0 && mode == 0) {
            int pick = (rnd.nextInt() & 0xFFFF) % nTerr;
            for (int i = 0; i < pl.length; i++) if (pl[i].team == 1 && pick-- == 0) { pl[i].c4 = true; break; }
        }
        specIdx = -1; buyOpen = false;
        centerAll(mode == 1 ? "Дезматч: 10 минут, возрождение через 3 с" : mode == 2 ? "Раунд " + round + ": через 14 с один станет зомби. Продержитесь 5 минут!" : "Раунд " + round, 3, 0xFFFFFF);
        if (!first) soundAll("radio_moveout", 70);
        // a teammate calls something at round start
        for (int i = 1; i < pl.length; i++) if (pl[i].team == (me != null ? me.team : 0) && (rnd.nextInt() & 3) == 0) { radioLater(pl[i], (rnd.nextInt() & 1) == 0 ? 6 : 8, freezeS + 0.5f); break; }
    }

    public static boolean debug;
    static void log(String s) { if (debug) System.out.println(s); }

    void endRound(int winner, String msg, String snd, int reason) {
        if (roundOver) return;
        log("t=" + (int) time + " round " + round + " end: " + msg);
        roundOver = true; roundEndAt = time + 5;
        if (winner == 1) scoreT++; else scoreCT++;
        centerAll(msg, 5, winner == 1 ? 0xFF6040 : 0x60A0FF);
        soundAll(snd, 90);
        // money
        int winBonus = reason == 1 ? 3500 : 3250; // bomb event
        for (int i = 0; i < pl.length; i++) {
            Player p = pl[i];
            if (p.team == winner) addMoney(p, winBonus);
            else {
                int streak = winner == 1 ? lossStreakCT : lossStreakT;
                addMoney(p, 1400 + Math.min(streak, 4) * 500);
            }
        }
        if (winner == 1) { lossStreakCT++; lossStreakT = 0; } else { lossStreakT++; lossStreakCT = 0; }
        if (winner == 1 && reason != 1 && false) { }
    }

    void addMoney(Player p, int v) { p.money += v; if (p.money > 16000) p.money = 16000; if (p.money < 0) p.money = 0; }

    void checkRoundEnd() {
        if (roundOver) return;
        int aT = 0, aC = 0, nT = 0, nC = 0;
        for (int i = 0; i < pl.length; i++) { if (pl[i].team == 1) { nT++; if (pl[i].alive) aT++; } else { nC++; if (pl[i].alive) aC++; } }
        if (mode == 1) {
            if (time > freezeEnd + roundTime) {
                int w = scoreT > scoreCT ? 1 : scoreCT > scoreT ? 2 : 0;
                roundOver = true; roundEndAt = time + 5;
                centerAll(w == 0 ? "Ничья!" : (w == 1 ? "Террористы" : "Спецназ") + " победили в дезматче!", 5, 0xFFE080);
                soundAll(w == 1 ? "radio_terwin" : "radio_ctwin", 90);
            }
            return;
        }
        if (mode == 2) {
            if (!infected) return;
            if (aC == 0) { endRound(1, "Зомби победили!", "radio_terwin", 0); return; }
            if (time > freezeEnd + roundTime) endRound(2, "Люди продержались 5 минут и победили!", "radio_ctwin", 0);
            return;
        }
        if (bombExploded) { endRound(1, "Цель уничтожена! Террористы победили", "radio_terwin", 1); return; }
        if (aC == 0 && nC > 0) { endRound(1, "Террористы победили!", "radio_terwin", 0); return; }
        if (aT == 0 && nT > 0 && !bombPlanted) { endRound(2, "Спецназ победил!", "radio_ctwin", 0); return; }
        if (!bombPlanted && time > freezeEnd + roundTime) { endRound(2, "Время вышло. Спецназ победил!", "radio_ctwin", 0); }
    }

    // ======================================================================= main tick
    public void tick(float dt) {
        realDt = dt;
        voiceTick();
        if (mp != null) { MqttLink.adRound = round; MqttLink.adT = scoreT; MqttLink.adCT = scoreCT; }   // shown in the worldwide room list
        if (quality == 0 && nfr == 0 && state == ST_PLAY && menu == M_NONE) {
            autoAcc += dt; autoFrames++;
            if (autoAcc >= 1.5f) {
                float f = autoFrames / autoAcc; autoAcc = 0; autoFrames = 0;
                if (time > autoCool) {
                    if (f < 18 && rLevel < 4) { setLevel(rLevel + 1); autoCool = time + 2.5f; }
                    else if (f > 28 && rLevel > 0) { setLevel(rLevel - 1); autoCool = time + 5; }
                }
            }
        }
        if (dt > 0.1f) dt = 0.1f;
        if (mp != null) { mp.update(dt); if (mp == null) return; }
        long now = Sys.s.millis();
        fpsCount++;
        if (now - fpsT >= 1000) { fps = fpsCount; fpsCount = 0; fpsT = now; }
        if (state == ST_LOAD) {
            if (loadStage++ < 1) return; // let the loading screen draw first
            try { doLoad(); } catch (Throwable e) { e.printStackTrace(); loadMsg = "Ошибка загрузки: " + e; state = ST_MENU; menu = M_MAIN; }
            return;
        }
        if (state == ST_MENU) { time += dt; return; }
        if (state == ST_END) { time += dt; return; }
        if (isPausedMenu() && mp == null) return; // paused (online the match goes on)
        time += dt;
        // human input
        humanInput(dt);
        // bots
        for (int i = 0; i < pl.length; i++) if (pl[i].ai != null && pl[i].alive && (auth() || pl[i] == me)) pl[i].ai.think(dt);
        // players
        boolean cl = client();
        for (int i = 0; i < pl.length; i++) if (!cl || pl[i] == me) updatePlayer(pl[i], dt);
        updateGrenades(dt);
        if (!cl) { updateDrops(dt); updateBomb(dt); modeTick(); }
        for (int i = 0; i < rqP.length; i++) if (rqP[i] != null && time >= rqT[i]) { Player q = rqP[i]; rqP[i] = null; radio(q, rqCmd[i]); }
        if (!roundOver && !cl) checkRoundEnd();
        if (roundOver && time > roundEndAt && !cl) {
            if (mode == 1 || scoreT >= (maxRounds / 2 + 1) || scoreCT >= (maxRounds / 2 + 1) || round >= maxRounds) {
                if (mp != null) mp.sendSnapshot();   // tell the others the match is over
                state = ST_END; Sys.s.stopSounds();
                if (mp != null) mp.sendSnapshot();
            }
            else newRound(false);
        }
        if (hurtFlash > 0) hurtFlash -= dt;
        if (me != null && !me.alive && (specIdx < 0 || !pl[specIdx].alive)) nextSpec();
    }

    void nextMapAfterEnd() {
        state = ST_MENU; menu = M_MAIN; menuSel = 0;
        startMatch((mapIdx + 1) % MAPS.length, me != null ? me.team : 2);
    }

    void nextSpec() {
        for (int k = 1; k <= pl.length; k++) {
            int i = ((specIdx < 0 ? 0 : specIdx) + k) % pl.length;
            if (pl[i].alive && (pl[i].team == me.team || !anyAlive(me.team))) { specIdx = i; return; }
        }
        specIdx = -1;
    }

    boolean anyAlive(int team) { for (int i = 0; i < pl.length; i++) if (pl[i].team == team && pl[i].alive) return true; return false; }

    void humanInput(float dt) {
        Player p = me;
        if (p != null && p.ai != null) return;   // test autopilot
        if (p == null || !p.alive) {
            if (specIdx < 0) deadCamYaw += dt * 10;
            return;
        }
        int k = keys;
        if (buyOpen) k &= ~(A_FIRE | A_TURNL | A_TURNR | A_LOOKU | A_LOOKD);
        float spd = 400;
        p.fmove = ((k & A_FWD) != 0 ? spd : 0) - ((k & A_BACK) != 0 ? spd : 0);
        p.smove = ((k & A_STRR) != 0 ? spd : 0) - ((k & A_STRL) != 0 ? spd : 0);
        if ((k & (A_TURNL | A_TURNR)) != 0) turnHeld += dt; else turnHeld = 0;
        float zoomK = p.zoom == 0 ? 1 : p.zoom == 1 ? 0.45f : 0.2f;
        float tsp = (70 + 22 * sens) * (0.45f + Math.min(1, turnHeld * 2.5f) * 0.9f) * zoomK;
        if ((k & A_TURNL) != 0) p.yaw += tsp * dt;
        if ((k & A_TURNR) != 0) p.yaw -= tsp * dt;
        float psp = (55 + 12 * sensV) * zoomK * (invert ? -1 : 1);
        if ((k & A_LOOKU) != 0) p.pitch -= psp * dt;
        if ((k & A_LOOKD) != 0) p.pitch += psp * dt;
        if (p.pitch > 89) p.pitch = 89; if (p.pitch < -89) p.pitch = -89;
        p.yaw = angNorm(p.yaw);
        p.bJump = (k & A_JUMP) != 0; p.bDuck = (k & A_DUCK) != 0;
        p.bFire = (k & (A_FIRE | A_SELECT)) != 0; p.bUse = (k & A_USE) != 0;
        if (time < freezeEnd) { p.fmove = p.smove = 0; p.bJump = false; }
        // light aim assist: nudge toward enemy close to crosshair
        if (autoAim && p.bFire) aimAssist(p, dt);
    }

    void aimAssist(Player p, float dt) {
        p.fwd(tmp3);
        Player best = null; float bestA = 4.5f;
        for (int i = 0; i < pl.length; i++) {
            Player q = pl[i];
            if (q == p || !q.alive || q.team == p.team) continue;
            float dx = q.ox - p.ox, dy = q.oy - p.oy, dz = q.oz + (q.ducked ? 4 : 10) - p.eyeZ();
            float d = (float) Math.sqrt(dx * dx + dy * dy + dz * dz);
            if (d < 1) continue;
            float yaw = atan2(dy, dx), pit = -atan2(dz, (float) Math.sqrt(dx * dx + dy * dy));
            float a = Math.abs(angNorm(yaw - p.yaw)) + Math.abs(pit - p.pitch);
            if (a < bestA && visible(p, q)) { bestA = a; best = q; }
        }
        if (best != null) {
            float dx = best.ox - p.ox, dy = best.oy - p.oy, dz = best.oz + (best.ducked ? 4 : 10) - p.eyeZ();
            float yaw = atan2(dy, dx), pit = -atan2(dz, (float) Math.sqrt(dx * dx + dy * dy));
            float k2 = Math.min(1, dt * 6);
            p.yaw += angNorm(yaw - p.yaw) * k2; p.pitch += (pit - p.pitch) * k2;
        }
    }

    // ======================================================================= player update
    float maxSpeed(Player p) {
        float s = Wp.speed[p.cur];
        if (p.zoom > 0 && Wp.isSniper(p.cur)) s = p.cur == Wp.AWP ? 150 : 200;
        if (p.planting || p.defusing) s = 0;
        if (p.zombie) s = ZSPD[p.ztype];
        return s;
    }

    void updatePlayer(Player p, float dt) {
        if (!p.alive) { p.animTime += dt; return; }
        if (time < freezeEnd) { p.fmove = p.smove = 0; p.bJump = false; }
        if (!p.remote) {   // online players move on their own phone/PC
        // physics substeps
        float left = dt;
        float land = 0;
        while (left > 0.0001f) {
            float st = left > 0.034f ? 0.034f : left;
            float l2 = mv.move(p, st, maxSpeed(p));
            if (l2 > land) land = l2;
            left -= st;
        }
        if (land > 580) {
            int dmgFall = (int) ((land - 580) * (100f / (1024 - 580)));
            if (dmgFall > 0) { damage(p, null, dmgFall, Wp.N, false, 0); sound(p, "player_pl_fallpain1", 80); }
        }
        // fell out of world
        if (p.oz < -8000) damage(p, null, 1000, Wp.N, false, 0);
        }
        // footsteps
        float sp = p.speedFactor();
        if (p.onGround && sp > 150 && !p.ducked) {
            p.stepTime -= dt;
            if (p.stepTime <= 0) { p.stepTime = 0.34f; sound(p, (rnd.nextInt() & 1) == 0 ? "player_pl_step1" : "player_pl_step2", p == me ? 25 : 55); }
        }
        if (sp > 20) p.animTime += dt * sp / 220f; else p.animTime = 0;
        // punch decay
        float dec = dt * (10 + Math.abs(p.punchP) * 4);
        if (p.punchP > 0) { p.punchP -= dec; if (p.punchP < 0) p.punchP = 0; } else if (p.punchP < 0) { p.punchP += dec; if (p.punchP > 0) p.punchP = 0; }
        dec = dt * (10 + Math.abs(p.punchY) * 4);
        if (p.punchY > 0) { p.punchY -= dec; if (p.punchY < 0) p.punchY = 0; } else if (p.punchY < 0) { p.punchY += dec; if (p.punchY > 0) p.punchY = 0; }
        p.recoil -= dt * 0.25f; if (p.recoil < 0) p.recoil = 0;
        if (!p.bFire && time > p.nextAttack) p.shotsFired = 0;
        weapons(p, dt);
        bombActions(p, dt);
        p.firePrev = p.bFire; p.fire2Prev = p.bFire2; p.usePrev = p.bUse;
    }

    // ======================================================================= weapons
    int vmSetFor(Player p) {
        if (p.cur == Wp.M4A1 && p.silM4) return Wp.N;
        if (p.cur == Wp.USP && p.silUSP) return Wp.N + 1;
        return p.cur;
    }

    void setVm(Player p, String anim, float len) {
        p.vmSet = anim.equals("detach") ? (p.cur == Wp.M4A1 ? Wp.N : Wp.N + 1) : vmSetFor(p);
        p.vmName = anim;
        if (p != me && !(specIdx >= 0 && pl[specIdx] == p)) { p.vmLen = len; p.vmTime = time; return; }
        Sprites s = vm(p.vmSet);
        p.vmAnim = s == null ? 0 : Math.max(0, s.find(anim));
        p.vmTime = time; p.vmLen = len;
    }

    Sprites vm(int w) {
        if (vws[w] == null) {
            try {
                int loaded = 0, oldest = -1;
                for (int i = 0; i < vws.length; i++) if (vws[i] != null) { loaded++; if (oldest < 0 || vwsAge[i] < vwsAge[oldest]) oldest = i; }
                if (loaded >= 3 && oldest >= 0) vws[oldest] = null;
                String nm = w == Wp.N ? "m4a1s" : w == Wp.N + 1 ? "usps" : Wp.name[w];
                vws[w] = Sprites.load("/v_" + nm + ".vws", false);
            } catch (Throwable e) { return null; }
        }
        vwsAge[w] = ++vwsTick;
        return vws[w];
    }

    void switchTo(Player p, int w) {
        if (!p.hasWeapon(w) || p.cur == w) return;
        if (p.planting) p.planting = false;
        p.last = p.cur; p.cur = w; p.reloading = false; p.zoom = 0; p.shotsFired = 0;
        p.drawEnd = time + Wp.drawT[w]; p.nextAttack = Math.max(p.nextAttack, time + Wp.drawT[w] * 0.8f);
        setVm(p, "draw", Wp.drawT[w]);
        if (p == me) { if (w == Wp.KNIFE) sound(p, "weapons_knife_deploy1", 50); else if (Wp.isGun(w)) sound(p, "weapons_slideback1", 40); }
    }

    public void nextWeapon(Player p) {
        int[] order = {p.primary, p.secondary, Wp.KNIFE, p.he > 0 ? Wp.HE : -1, p.flash > 0 ? Wp.FLASH : -1, p.smoke > 0 ? Wp.SMOKE : -1, p.c4 ? Wp.C4 : -1};
        int idx = 0;
        for (int i = 0; i < order.length; i++) if (order[i] == p.cur) idx = i;
        for (int k = 1; k <= order.length; k++) { int w = order[(idx + k) % order.length]; if (w >= 0) { switchTo(p, w); return; } }
    }

    public void selectSlot(Player p, int s) {
        if (s == 0 && p.primary >= 0) switchTo(p, p.primary);
        else if (s == 1 && p.secondary >= 0) switchTo(p, p.secondary);
        else if (s == 2) switchTo(p, Wp.KNIFE);
        else if (s == 3) { if (p.he > 0) switchTo(p, Wp.HE); else if (p.flash > 0) switchTo(p, Wp.FLASH); else if (p.smoke > 0) switchTo(p, Wp.SMOKE); }
        else if (s == 4 && p.c4) switchTo(p, Wp.C4);
    }

    public void startReload(Player p) {
        int w = p.cur;
        if (!Wp.isGun(w) || p.reloading || p.clip[w] >= Wp.clip[w] || p.ammo[w] <= 0 || time < p.drawEnd) return;
        p.reloading = true; p.zoom = 0;
        if (Wp.isShotgun(w)) { p.reloadEnd = time + 0.5f; p.shellNext = time + 0.5f; setVm(p, "rstart", 0.5f); }
        else { p.reloadEnd = time + Wp.reloadT[w]; setVm(p, "reload", Wp.reloadT[w]); sound(p, "weapons_clipout1", p == me ? 60 : 40); }
    }

    void altFire(Player p) {
        int w = p.cur;
        if ((w == Wp.M4A1 || w == Wp.USP) && !p.reloading && time >= p.nextAttack && time >= p.drawEnd) {
            boolean on = w == Wp.M4A1 ? !p.silM4 : !p.silUSP;
            float len = w == Wp.M4A1 ? 2f : 3.1f;
            if (on) { if (w == Wp.M4A1) p.silM4 = true; else p.silUSP = true; setVm(p, "attach", len); }
            else { setVm(p, "detach", len); if (w == Wp.M4A1) p.silM4 = false; else p.silUSP = false; }
            p.nextAttack = time + len; p.drawEnd = time + len * 0.9f;
            sound(p, "weapons_" + (w == Wp.M4A1 ? "m4a1" : "usp") + (on ? "_silencer_on" : "_silencer_off"), 60);
            return;
        }
        if (Wp.zoom1[w] > 0 && !p.reloading) {
            int max = Wp.zoom2[w] > 0 ? 2 : 1;
            p.zoom = (p.zoom + 1) % (max + 1);
            sound(p, "weapons_zoom", 40);
        } else if (w == Wp.KNIFE && time >= p.nextAttack) {
            knife(p, true);
        }
    }

    void weapons(Player p, float dt) {
        int w = p.cur;
        // reload progress
        if (p.reloading) {
            if (Wp.isShotgun(w)) {
                if (time >= p.shellNext) {
                    if (p.clip[w] < Wp.clip[w] && p.ammo[w] > 0 && !(p.bFire && p.clip[w] > 0)) {
                        p.clip[w]++; p.ammo[w]--; p.shellNext = time + Wp.reloadT[w]; setVm(p, "insert", Wp.reloadT[w]);
                        sound(p, "weapons_m3_insertshell", p == me ? 60 : 35);
                    } else { p.reloading = false; setVm(p, "rafter", 0.4f); p.nextAttack = time + 0.3f; if (p == me) sound(p, "weapons_m3_pump", 60); }
                }
            } else if (time >= p.reloadEnd) {
                int need = Wp.clip[w] - p.clip[w], take = Math.min(need, p.ammo[w]);
                p.clip[w] += take; p.ammo[w] -= take; p.reloading = false; setVm(p, "idle", 1);
                sound(p, "weapons_clipin1", p == me ? 60 : 35);
            }
            if (p.reloading && !(Wp.isShotgun(w) && p.bFire && p.clip[w] > 0)) return;
            if (p.reloading) p.reloading = false;
        }
        if (p.planting) return;
        if (time < p.drawEnd) return;
        if (w == Wp.C4) return; // handled by bombActions
        if (p.bFire && time >= p.nextAttack) {
            if (w == Wp.KNIFE) { knife(p, false); return; }
            if (Wp.isGrenade(w)) { if (!p.firePrev || true) throwGrenade(p); return; }
            if (!Wp.auto[w] && p.firePrev && p != me && false) return;
            if (!Wp.auto[w] && p.firePrev) return;
            if (p.clip[w] <= 0) {
                if (!p.firePrev) sound(p, "weapons_dryfire_rifle", 40);
                p.nextAttack = time + 0.2f;
                if (p.ammo[w] > 0) startReload(p);
                return;
            }
            shoot(p);
        }
        // auto reload when empty
        if (!p.bFire && Wp.isGun(w) && p.clip[w] == 0 && p.ammo[w] > 0 && !p.reloading && time >= p.nextAttack) startReload(p);
    }

    void shoot(Player p) {
        int w = p.cur;
        p.clip[w]--;
        p.nextAttack = time + Wp.cycle[w];
        p.shotsFired++;
        // spread
        float sp = p.speedFactor();
        float spread = Wp.spread[w];
        if (Wp.isSniper(w)) { if (p.zoom > 0) spread = w == Wp.AWP ? 0.001f : w == Wp.SCOUT ? 0.007f : 0.012f; }
        spread += Wp.moveSpread[w] * Math.min(1, sp / 220f) * (sp > 40 ? 1 : 0);
        if (!p.onGround) spread += 0.3f;
        if (p.ducked) spread *= 0.75f;
        spread += p.recoil;
        p.recoil += Wp.recoilSpread[w]; if (p.recoil > 0.09f) p.recoil = 0.09f;
        float yaw = p.yaw + p.punchY * 2, pitch = p.pitch + p.punchP * 2;
        float cy = (float) Math.cos(Math.toRadians(yaw)), sy = (float) Math.sin(Math.toRadians(yaw));
        float cp = (float) Math.cos(Math.toRadians(pitch)), spp = (float) Math.sin(Math.toRadians(pitch));
        float fx = cp * cy, fy = cp * sy, fz = -spp;
        float rx = sy, ry = -cy, ux = spp * cy, uy = spp * sy, uz = cp;
        float ex = p.ox, ey = p.oy, ez = p.eyeZ();
        int pel = Wp.pellets[w];
        boolean sil = (w == Wp.M4A1 && p.silM4) || (w == Wp.USP && p.silUSP);
        int dmgBase = Wp.dmg[w];
        if (sil) { dmgBase = w == Wp.M4A1 ? 33 : 30; spread *= 0.85f; }
        for (int k = 0; k < pel; k++) {
            float a = rs() * spread, b = rs() * spread;
            if (pel > 1) { a = rs() * Wp.spread[w]; b = rs() * Wp.spread[w] * 0.6f; }
            float dx = fx + rx * a + ux * b, dy = fy + ry * a + uy * b, dz = fz + uz * b;
            float l = (float) Math.sqrt(dx * dx + dy * dy + dz * dz); dx /= l; dy /= l; dz /= l;
            bullet(p, w, ex, ey, ez, dx, dy, dz, pel > 1 ? 3000 : 8192, dmgBase);
        }
        // recoil (view punch)
        float pk = Wp.punch[w];
        if (pel == 1) {
            float add = pk * (0.6f + Math.min(p.shotsFired, 8) * 0.12f);
            p.punchP -= add; if (p.punchP < -6) p.punchP = -6;
            p.punchY += rs() * pk * 0.35f * Math.min(1, p.shotsFired / 4f);
        } else { p.punchP -= pk; }
        if (Wp.isSniper(w) && (w == Wp.AWP || w == Wp.SCOUT)) { p.zoom = 0; }
        String s = sil ? (w == Wp.M4A1 ? "weapons_m4a1_1" : "weapons_usp1") : Wp.snd[w];
        sound(p, s, sil ? (p == me ? 60 : 45) : (p == me ? 90 : 100));
        setVm(p, "shoot", Math.min(0.5f, Math.max(Wp.cycle[w], 0.25f)));
        if (p == me) muzzleUntil = time + 0.05f;
        p.lastHurt = p.lastHurt; // no-op
        // bots hear the shot
        for (int i = 0; i < pl.length; i++) if (pl[i].ai != null && pl[i].alive && pl[i].team != p.team) {
            if (sil) { float ddx = pl[i].ox - p.ox, ddy = pl[i].oy - p.oy; if (ddx * ddx + ddy * ddy > 500 * 500) continue; }
            pl[i].ai.heard(p);
        }
        p.lastShot = time; p.lastShotSil = sil;
    }

    /** hitscan */
    /** hitscan with CS 1.6-style wall penetration (material from materials.txt) */
    void bullet(Player p, int w, float ex, float ey, float ez, float dx, float dy, float dz, float range, int baseDmg) {
        float dmgMul = 1, travelled = 0;
        int pens = Wp.penCount[w]; float power = Wp.penPower[w];
        float sx = ex, sy = ey, sz = ez;
        for (int iter = 0; iter < 4; iter++) {
            float left = range - travelled;
            if (left <= 1) return;
            m.trace(0, sx, sy, sz, sx + dx * left, sy + dy * left, sz + dz * left);
            boolean wall = m.rFrac < 1 && !m.rStartSolid;
            float wallD = m.rStartSolid ? 0 : m.rFrac * left;
            float wx = m.rEx, wy = m.rEy, wz = m.rEz; int hitModel = m.rModel;
            Player hit = null; float hitD = wallD; int group = 0; float hz = 0;
            for (int i = 0; i < pl.length; i++) {
                Player q = pl[i];
                if (q == p || !q.alive) continue;
                int gr = rayHit(q, sx, sy, sz, dx, dy, dz, hitD);
                if (gr >= 0) { hit = q; hitD = rayT; group = gr; hz = sz + dz * rayT; }
            }
            if (hit != null) {
                float d = baseDmg * dmgMul * pow(Wp.rangeMod[w], (travelled + hitD) / 500f);
                if (Wp.isShotgun(w) && travelled + hitD > 600) d *= 0.6f;
                float mul = group == 1 ? 4 : group == 3 ? 1.25f : group == 4 ? 0.75f : 1;
                damage(hit, p, (int) (d * mul), w, group == 1, group);
                fx(sx + dx * hitD, sy + dy * hitD, hz, 0xA00000, 0.25f, 4);
                return;
            }
            if (!wall) return;
            if (iter == 0 || dmgMul > 0.3f) {
                fx(wx, wy, wz, 0x303030, 4f, 1.5f);
                fx(wx - dx * 2, wy - dy * 2, wz - dz * 2, 0xC8B898, 0.35f, 5);
                if ((rnd.nextInt() & 7) == 0) soundAt(wx, wy, wz, "weapons_ric_conc_1", 50, 900);
            }
            if (pens <= 0 || power <= 0) return;
            int tex = m.surfaceTex(hitModel, wx - dx * 4, wy - dy * 4, wz - dz * 4, wx + dx * 4, wy + dy * 4, wz + dz * 4);
            int mat = tex >= 0 ? (m.texFlags[tex] >> 4) & 15 : 0;
            if (tex >= 0 && (m.texFlags[tex] & 1) != 0) return; // sky
            float pw = power * Wp.matPower(mat);
            float nx = wx + dx * pw, ny = wy + dy * pw, nz = wz + dz * pw;
            int head = hitModel > 0 ? m.mHead[hitModel * 4] : m.mHead[0];
            if (m.hullPointContents(0, head, nx, ny, nz) == MapData.CONTENTS_SOLID) return;
            if (hitModel > 0 && m.hullPointContents(0, m.mHead[0], nx, ny, nz) == MapData.CONTENTS_SOLID) return;
            dmgMul *= Wp.matDamage(mat); pens--; statPen++;
            travelled += wallD + pw;
            sx = nx; sy = ny; sz = nz;
            fx(nx, ny, nz, 0xC8B898, 0.3f, 4);
        }
    }

    static float pow(float b, float e) { return (float) exp(e * ln(b)); }
    static double ln(double x) { // CLDC has no Math.log
        if (x <= 0) return -1e9; int k = 0; while (x > 2) { x /= 2.718281828; k++; } while (x < 0.5) { x *= 2.718281828; k--; }
        double y = (x - 1) / (x + 1), y2 = y * y, s = 0, t = y; for (int i = 1; i < 30; i += 2) { s += t / i; t *= y2; } return 2 * s + k;
    }
    static double exp(double x) { double s = 1, t = 1; for (int i = 1; i < 30; i++) { t *= x / i; s += t; if (Math.abs(t) < 1e-12) break; } return s; }

    float rayT;

    /** returns hit group (1 head, 2 chest, 3 stomach, 4 legs) or -1 */
    int rayHit(Player q, float ex, float ey, float ez, float dx, float dy, float dz, float maxD) {
        float hzc = q.ducked ? q.oz + 10 : q.oz + 26;
        // head sphere
        float hr = 6.5f;
        float ox = ex - q.ox, oy = ey - q.oy, oz = ez - hzc;
        float b = ox * dx + oy * dy + oz * dz, c = ox * ox + oy * oy + oz * oz - hr * hr;
        float disc = b * b - c;
        float best = maxD; int grp = -1;
        if (disc >= 0) { float t = -b - (float) Math.sqrt(disc); if (t > 0 && t < best) { best = t; grp = 1; } }
        // body cylinder (vertical)
        float top = q.ducked ? q.oz + 4 : q.oz + 20, bot = q.oz - (q.ducked ? 18 : 36);
        float br = 13;
        float a2 = dx * dx + dy * dy;
        if (a2 > 1e-6f) {
            float b2 = ox * dx + oy * dy, c2 = ox * ox + oy * oy - br * br;
            float disc2 = b2 * b2 - a2 * c2;
            if (disc2 >= 0) {
                float t = (-b2 - (float) Math.sqrt(disc2)) / a2;
                if (t < 0) t = 0;
                float z = ez + dz * t;
                if (t < best && z >= bot && z <= top) {
                    best = t; float rel = (z - bot) / (top - bot);
                    grp = rel > 0.62f ? 2 : rel > 0.42f ? 3 : 4;
                }
            }
        }
        rayT = best;
        return grp;
    }

    void knife(Player p, boolean stab) {
        p.nextAttack = time + (stab ? 1.1f : 0.45f);
        setVm(p, stab ? "shoot2" : "shoot", stab ? 1f : 0.45f);
        p.fwd(tmp3);
        float ex = p.ox, ey = p.oy, ez = p.eyeZ();
        float range = stab ? 32 : 48;
        Player hit = null; float hd = range + 16;
        for (int i = 0; i < pl.length; i++) {
            Player q = pl[i]; if (q == p || !q.alive) continue;
            int gr = rayHit(q, ex, ey, ez, tmp3[0], tmp3[1], tmp3[2], range + 16);
            if (gr >= 0 && rayT < hd) { hd = rayT; hit = q; }
        }
        if (hit != null) {
            int d = stab ? 65 : 20;
            float back = angNorm(hit.yaw - p.yaw);
            if (Math.abs(back) < 50) d *= 3;
            damage(hit, p, d, Wp.KNIFE, false, 2);
            sound(p, "weapons_knife_hit1", 80);
        } else {
            m.trace(0, ex, ey, ez, ex + tmp3[0] * range, ey + tmp3[1] * range, ez + tmp3[2] * range);
            sound(p, m.rFrac < 1 ? "weapons_knife_hitwall1" : "weapons_knife_slash1", 60);
        }
    }

    public void damage(Player v, Player att, int d, int weapon, boolean hs, int group) {
        if (!v.alive || client()) return;
        if (mode == 2 && att != null && att != v) {
            if (att.zombie && !v.zombie) {   // a zombie claw: damage by type, the last hit infects
                if (weapon != Wp.KNIFE) return;
                int zd = ZDMG[att.ztype];
                if (v.armor > 0) { int ab = Math.min(v.armor, zd / 2); v.armor -= ab; zd -= ab; }
                if (v.health - zd <= 0) { infect(v, att); checkRoundEnd(); return; }
                v.health -= zd; v.lastAttacker = att.id;
                if (v == me) { hurtFlash = 0.25f; me.punchP -= 2; }
                sound(v, "player_bhit_flesh_1", 80);
                return;
            }
            if (att.zombie == v.zombie) return;
        }
        if (att != null && att.team == v.team && att != v) {
            if (!friendlyFire) return;
            d = d * 35 / 100; // CS-like reduced team damage
        }
        if (v.armor > 0 && weapon < Wp.N && group != 4 && (group != 1 || v.helmet)) {
            float ratio = Wp.armorR[weapon] * 0.5f;
            float nd = d * ratio, al = (d - nd) * 0.5f;
            if (al > v.armor) { nd = d - v.armor * 2; al = v.armor; }
            v.armor -= (int) al; d = (int) nd;
            if (v.armor < 0) v.armor = 0;
        }
        if (d < 1) d = 1;
        v.health -= d;
        v.lastHurt = time;
        if (att != null) {
            v.lastAttacker = att.id;
            v.lastHurtYaw = atan2(att.oy - v.oy, att.ox - v.ox);
            if (v.ai != null) v.ai.hurtBy(att);
        }
        // aim punch
        v.punchP -= hs ? 4 : 1.5f;
        if (v == me) hurtFlash = 0.25f;
        sound(v, hs ? (v.helmet ? "player_bhit_helmet_1" : "player_headshot1") : (v.armor > 0 ? "player_bhit_kevlar_1" : "player_bhit_flesh_1"), 70);
        if (v.health <= 0) kill(v, att, weapon, hs);
    }

    void kill(Player v, Player att, int weapon, boolean hs) {
        if (client()) return;
        if (mode == 1 && att != null && att != v && att.team != v.team) { if (att.team == 1) scoreT++; else scoreCT++; }
        v.alive = false; v.health = 0; v.deaths++; v.deathTime = time; v.animTime = 0;
        v.deathAnim = hs ? 1 : 0; v.planting = v.defusing = false;
        sound(v, (rnd.nextInt() & 1) == 0 ? "player_die1" : "player_die3", 80);
        if (v.c4) { v.c4 = false; bombDropped = true; bombX = v.ox; bombY = v.oy; bombZ = groundZ(v.ox, v.oy, v.oz); }
        int dw = v.primary >= 0 ? v.primary : v.secondary;
        if (dw >= 0 && (v.clip[dw] > 0 || v.ammo[dw] > 0)) spawnDrop(v, dw, 120);
        if (att != null && att != v) {
            if (att.team != v.team) { att.kills++; addMoney(att, 300); } else { att.kills--; addMoney(att, -3300); }
            if (att == me && att.team != v.team) sound(null, "radio_enemydown", 45);
            else if (att.isBot && att.team != v.team && (rnd.nextInt() & 3) == 0) radioLater(att, 20, 0.6f);
        }
        killfeed(att, v, weapon, hs);
        if (mp != null && mp.host) mp.evKill(att, v, weapon, hs);
        if (v == me) { specIdx = -1; buyOpen = false; me.zoom = 0; }
    }

    void killfeed(Player a, Player v, int w, boolean hs) {
        for (int i = kfA.length - 1; i > 0; i--) { kfA[i] = kfA[i - 1]; kfB[i] = kfB[i - 1]; kfW[i] = kfW[i - 1]; kfT[i] = kfT[i - 1]; kfHS[i] = kfHS[i - 1]; kfColA[i] = kfColA[i - 1]; kfColB[i] = kfColB[i - 1]; }
        kfA[0] = a == null || a == v ? "" : a.name; kfB[0] = v.name; kfW[0] = gx.hudId(w < Wp.N ? Wp.kill[w] : "d_skull"); kfT[0] = time; kfHS[0] = hs;
        kfColA[0] = a == null ? 0xFFFFFF : teamCol(a.team); kfColB[0] = teamCol(v.team);
    }

    static int teamCol(int t) { return t == 1 ? 0xFF4040 : 0x99CCFF; }

    // ======================================================================= buying
    // ======================================================================= dropped weapons
    float groundZ(float x, float y, float z) {
        m.trace(0, x, y, z, x, y, z - 512);
        return m.rEz;
    }

    void spawnDrop(Player p, int w, float speed) {
        if (client()) return;
        int k = -1; float oldest = 1e9f;
        for (int i = 0; i < MAXD; i++) { if (!dOn[i]) { k = i; break; } if (dT[i] < oldest) { oldest = dT[i]; k = i; } }
        float cy = (float) Math.cos(Math.toRadians(p.yaw)), sy = (float) Math.sin(Math.toRadians(p.yaw));
        dOn[k] = true; dW[k] = w; dClip[k] = p.clip[w]; dAmmo[k] = p.ammo[w]; dBy[k] = p.id; dT[k] = time; dRest[k] = false;
        dSil[k] = (w == Wp.M4A1 && p.silM4) || (w == Wp.USP && p.silUSP);
        dX[k] = p.ox + cy * 12; dY[k] = p.oy + sy * 12; dZ[k] = p.eyeZ() - 10;
        dVX[k] = cy * speed + p.vx; dVY[k] = sy * speed + p.vy; dVZ[k] = speed * 0.6f; dYaw[k] = p.yaw + rs() * 40;
    }

    public void dropCurrent(Player p) {
        int w = p.cur;
        if (w == Wp.C4 && p.c4) {
            p.c4 = false; bombDropped = true; bombX = p.ox + (float) Math.cos(Math.toRadians(p.yaw)) * 30; bombY = p.oy + (float) Math.sin(Math.toRadians(p.yaw)) * 30;
            bombZ = groundZ(bombX, bombY, p.oz); bombDropBy = p.id; bombDropT = time;
        } else if (Wp.isGun(w)) {
            spawnDrop(p, w, 250);
            if (w == p.primary) p.primary = -1; else if (w == p.secondary) p.secondary = -1;
            sound(p, "items_weapondrop1", 50);
        } else { if (p == me) center("Это нельзя выбросить", 1.2f, 0xFFFFFF); return; }
        p.cur = Wp.KNIFE; p.reloading = false; p.zoom = 0;
        int nw = p.primary >= 0 ? p.primary : p.secondary >= 0 ? p.secondary : Wp.KNIFE;
        p.cur = -1; switchTo(p, nw);
    }
    int bombDropBy = -1; float bombDropT;
    public int statPickups, statPen, statRadio;

    void updateDrops(float dt) {
        for (int i = 0; i < MAXD; i++) {
            if (!dOn[i]) continue;
            if (!dRest[i]) {
                dVZ[i] -= 800 * dt;
                float nx = dX[i] + dVX[i] * dt, ny = dY[i] + dVY[i] * dt, nz = dZ[i] + dVZ[i] * dt;
                m.trace(0, dX[i], dY[i], dZ[i], nx, ny, nz);
                if (m.rFrac < 1) {
                    dX[i] = m.rEx + m.rNx; dY[i] = m.rEy + m.rNy; dZ[i] = m.rEz + m.rNz;
                    if (m.rNz > 0.7f) { dRest[i] = true; dZ[i] = m.rEz + 1; }
                    else { float d = dVX[i] * m.rNx + dVY[i] * m.rNy + dVZ[i] * m.rNz; dVX[i] = (dVX[i] - 2 * d * m.rNx) * 0.3f; dVY[i] = (dVY[i] - 2 * d * m.rNy) * 0.3f; dVZ[i] = (dVZ[i] - 2 * d * m.rNz) * 0.3f; }
                } else { dX[i] = nx; dY[i] = ny; dZ[i] = nz; }
                if (time - dT[i] > 4) dRest[i] = true;
            }
            // pick up
            for (int k = 0; k < pl.length; k++) {
                Player p = pl[k];
                if (!p.alive || p.zombie || (dBy[i] == p.id && time - dT[i] < 1.5f)) continue;
                float ddx = p.ox - dX[i], ddy = p.oy - dY[i], ddz = (p.oz - (p.ducked ? 18 : 36)) - dZ[i];
                if (ddx * ddx + ddy * ddy > 36 * 36 || ddz > 40 || ddz < -40) continue;
                int w = dW[i];
                boolean prim = Wp.slot[w] == Wp.S_PRIMARY;
                if ((prim && p.primary >= 0) || (!prim && p.secondary >= 0)) continue;
                if (prim) p.primary = w; else p.secondary = w;
                p.clip[w] = dClip[i]; p.ammo[w] = dAmmo[i];
                if (w == Wp.M4A1) p.silM4 = dSil[i]; if (w == Wp.USP) p.silUSP = dSil[i];
                dOn[i] = false; statPickups++;
                sound(p, "items_gunpickup2", 60);
                if (p == me) center("Подобрано: " + Wp.title[w], 1.5f, 0xFFFFFF);
                if (p.isBot || p.cur == Wp.KNIFE || (prim && p.cur == p.secondary)) switchTo(p, w);
                break;
            }
        }
    }

    // ======================================================================= radio
    public void radio(Player p, int cmd) {
        if (client() && !mp.applying) { if (p == me && p != null && p.alive) mp.act('r', cmd, -1); return; }
        if (p == null || !p.alive || cmd < 0 || cmd >= RS.length) return;
        if (time - p.lastRadio < 1.2f) return;
        p.lastRadio = time; statRadio++;
        if (me != null && p.team == me.team) {
            sound(null, "radio_" + RS[cmd], 85);
            chat(p.name + " (РАДИО): " + RT[cmd], teamCol(p.team));
        }
        for (int i = 0; i < pl.length; i++) if (pl[i].ai != null && pl[i] != p && pl[i].team == p.team && pl[i].alive) pl[i].ai.onRadio(p, cmd);
        if (mp != null && mp.host) mp.evRadio(p, cmd);
    }

    public void radioLater(Player p, int cmd, float delay) {
        for (int i = 0; i < rqP.length; i++) if (rqP[i] == null) { rqP[i] = p; rqCmd[i] = cmd; rqT[i] = time + delay; return; }
    }

    void chat(String s, int col) {
        for (int i = chatS.length - 1; i > 0; i--) { chatS[i] = chatS[i - 1]; chatC[i] = chatC[i - 1]; chatT[i] = chatT[i - 1]; }
        chatS[0] = s; chatC[0] = col; chatT[0] = time;
    }

    public boolean canBuy(Player p) {
        if (p.zombie || !p.alive) return false;
        if (mode == 1) return true;   // deathmatch: buy anywhere, any time
        if (time > buyEnd) return false;
        for (int i = 0; i < buyModel.length; i++)
            if ((buyTeam[i] == 0 || buyTeam[i] == p.team) && inModel(buyModel[i], p.ox, p.oy, p.oz, 8)) return true;
        if (buyModel.length == 0) {
            float[] sp = p.team == 1 ? spawnT : spawnCT;
            for (int k = 0; k < sp.length / 4; k++) { float dx = sp[k * 4] - p.ox, dy = sp[k * 4 + 1] - p.oy; if (dx * dx + dy * dy < 400 * 400) return true; }
        }
        return false;
    }

    public boolean buyWeapon(Player p, int w) {
        if (Wp.team[w] != 0 && Wp.team[w] != p.team) { if (p == me) center("Недоступно для вашей команды", 1.5f, 0xFFFFFF); return false; }
        if (p.money < Wp.price[w]) { if (p == me) center("Недостаточно денег", 1.5f, 0xFFFFFF); return false; }
        if (Wp.slot[w] == Wp.S_PRIMARY && p.primary >= 0 && p.primary != w) spawnDrop(p, p.primary, 60);
        if (Wp.slot[w] == Wp.S_SECONDARY && p.secondary >= 0 && p.secondary != w) spawnDrop(p, p.secondary, 60);
        if (w == Wp.M4A1) p.silM4 = false; if (w == Wp.USP) p.silUSP = false;
        if (Wp.slot[w] == Wp.S_PRIMARY) { if (p.primary == w) return false; p.primary = w; }
        else if (Wp.slot[w] == Wp.S_SECONDARY) { if (p.secondary == w) return false; p.secondary = w; }
        else return false;
        p.money -= Wp.price[w];
        p.clip[w] = Wp.clip[w]; p.ammo[w] = 0;
        // buy full ammo automatically when affordable (like autobuy) - one clip's worth
        int extra = Wp.reserve[w];
        int perClip = Wp.ammoPrice[w];
        int clips = extra / Math.max(1, Wp.clip[w]);
        for (int c = 0; c < clips && p.money >= perClip && p.ammo[w] < extra; c++) { p.money -= perClip; p.ammo[w] = Math.min(extra, p.ammo[w] + Wp.clip[w]); }
        if (p == me) { sound(p, "items_kevlar", 30); }
        switchTo(p, w);
        return true;
    }

    void buyAmmo(Player p, int slot) {
        int w = slot == 0 ? p.primary : p.secondary;
        if (w < 0) return;
        while (p.ammo[w] < Wp.reserve[w] && p.money >= Wp.ammoPrice[w]) { p.money -= Wp.ammoPrice[w]; p.ammo[w] = Math.min(Wp.reserve[w], p.ammo[w] + Wp.clip[w]); }
        if (p == me) sound(p, "items_kevlar", 30);
    }

    /** 0 kevlar, 1 kevlar+helmet, 2 flash, 3 HE, 4 smoke, 5 defuse kit */
    public boolean buyEquip(Player p, int e) {
        int price = e == 0 ? 650 : e == 1 ? (p.armor >= 100 ? 350 : 1000) : e == 2 ? 200 : e == 3 ? 300 : e == 4 ? 300 : e == 5 ? 200 : 99999;
        if (p.money < price) { if (p == me) center("Недостаточно денег", 1.5f, 0xFFFFFF); return false; }
        switch (e) {
            case 0: if (p.armor >= 100) return false; p.armor = 100; break;
            case 1: if (p.armor >= 100 && p.helmet) return false; p.armor = 100; p.helmet = true; break;
            case 2: if (p.flash >= 2) return false; p.flash++; break;
            case 3: if (p.he >= 1) return false; p.he++; break;
            case 4: if (p.smoke >= 1) return false; p.smoke++; break;
            case 5: if (p.team != 2 || p.defuser) return false; p.defuser = true; break;
            default: return false;
        }
        p.money -= price;
        if (p == me) sound(p, e < 2 ? "items_kevlar" : "weapons_pinpull", 40);
        return true;
    }

    // ======================================================================= grenades
    void throwGrenade(Player p) {
        int w = p.cur;
        int gi = -1; for (int i = 0; i < MAXG; i++) if (!grOn[i]) { gi = i; break; }
        if (gi < 0) return;
        p.fwd(tmp3);
        float pit = p.pitch - 10;
        float cp = (float) Math.cos(Math.toRadians(pit)), sp = (float) Math.sin(Math.toRadians(pit));
        float cy = (float) Math.cos(Math.toRadians(p.yaw)), sy = (float) Math.sin(Math.toRadians(p.yaw));
        float v = 750;
        grOn[gi] = true; grType[gi] = w; grOwner[gi] = p.id; grT[gi] = time + (w == Wp.SMOKE ? 1.5f : 1.6f);
        grX[gi] = p.ox + cp * cy * 16; grY[gi] = p.oy + cp * sy * 16; grZ[gi] = p.eyeZ() - 4;
        grVX[gi] = cp * cy * v + p.vx; grVY[gi] = cp * sy * v + p.vy; grVZ[gi] = -sp * v + p.vz;
        if (w == Wp.HE) p.he--; else if (w == Wp.FLASH) p.flash--; else p.smoke--;
        p.nextAttack = time + 1;
        setVm(p, "shoot", 0.5f);
        sound(p, "weapons_pinpull", 50);
        if (p.team == (me != null ? me.team : 0)) sound(null, "radio_ct_fireinhole", 40);
        if (!p.hasWeapon(w)) { p.drawEnd = time + 0.5f; int nw = p.primary >= 0 ? p.primary : p.secondary >= 0 ? p.secondary : Wp.KNIFE; p.last = Wp.KNIFE; p.cur = nw; setVm(p, "draw", 0.8f); }
    }

    void updateGrenades(float dt) {
        for (int i = 0; i < MAXG; i++) {
            if (!grOn[i]) continue;
            grVZ[i] -= 800 * 0.4f * dt;
            float nx = grX[i] + grVX[i] * dt, ny = grY[i] + grVY[i] * dt, nz = grZ[i] + grVZ[i] * dt;
            m.trace(0, grX[i], grY[i], grZ[i], nx, ny, nz);
            if (m.rFrac < 1) {
                float d = grVX[i] * m.rNx + grVY[i] * m.rNy + grVZ[i] * m.rNz;
                grVX[i] = (grVX[i] - 2 * d * m.rNx) * 0.45f; grVY[i] = (grVY[i] - 2 * d * m.rNy) * 0.45f; grVZ[i] = (grVZ[i] - 2 * d * m.rNz) * 0.45f;
                grX[i] = m.rEx + m.rNx; grY[i] = m.rEy + m.rNy; grZ[i] = m.rEz + m.rNz;
                if (Math.abs(d) > 80) soundAt(grX[i], grY[i], grZ[i], "weapons_he_bounce_1", 50, 1200);
            } else { grX[i] = nx; grY[i] = ny; grZ[i] = nz; }
            if (time >= grT[i]) { explode(i); grOn[i] = false; }
        }
        for (int i = 0; i < MAXS; i++) if (smOn[i] && time > smT[i]) smOn[i] = false;
    }

    void explode(int i) {
        int t = grType[i];
        float x = grX[i], y = grY[i], z = grZ[i];
        Player owner = pl[grOwner[i]];
        if (t == Wp.HE) {
            soundAt(x, y, z, "weapons_hegrenade_1", 100, 3000);
            for (int k = 0; k < 6; k++) fx(x + rs() * 40, y + rs() * 40, z + rf() * 50, 0xFFA030, 0.5f, 30);
            for (int k = 0; k < pl.length; k++) {
                Player q = pl[k]; if (!q.alive) continue;
                float dx = q.ox - x, dy = q.oy - y, dz = q.oz - z, d = (float) Math.sqrt(dx * dx + dy * dy + dz * dz);
                if (d > 350) continue;
                m.trace(0, x, y, z + 8, q.ox, q.oy, q.oz); if (m.rFrac < 1) continue;
                int dmg = (int) (100 * (1 - d / 350));
                if (dmg > 0) damage(q, owner, dmg, Wp.HE, false, 2);
            }
        } else if (t == Wp.FLASH) {
            soundAt(x, y, z, "weapons_flashbang_1", 100, 2500);
            fx(x, y, z, 0xFFFFFF, 0.2f, 40);
            for (int k = 0; k < pl.length; k++) {
                Player q = pl[k]; if (!q.alive) continue;
                float dx = x - q.ox, dy = y - q.oy, dz = z - q.eyeZ(), d = (float) Math.sqrt(dx * dx + dy * dy + dz * dz);
                if (d > 1500) continue;
                m.trace(0, q.ox, q.oy, q.eyeZ(), x, y, z); if (m.rFrac < 0.99f) continue;
                q.fwd(tmp3);
                float dot = (dx * tmp3[0] + dy * tmp3[1] + dz * tmp3[2]) / Math.max(1, d);
                float dur = (dot > 0.3f ? 4.5f : dot > -0.3f ? 2.5f : 1f) * (1 - d / 1800);
                if (dur > 0.3f) { q.flashStart = time; q.flashUntil = time + dur; if (q.ai != null) q.ai.flashed(dur); }
            }
        } else {
            soundAt(x, y, z, "weapons_sg_explode", 80, 1500);
            for (int k = 0; k < MAXS; k++) if (!smOn[k]) { smOn[k] = true; smX[k] = x; smY[k] = y; smZ[k] = z + 40; smT[k] = time + 18; break; }
        }
    }

    /** true if the segment passes through a smoke cloud */
    public boolean smokeBlocks(float x1, float y1, float z1, float x2, float y2, float z2) {
        for (int i = 0; i < MAXS; i++) {
            if (!smOn[i]) continue;
            float age = time - (smT[i] - 18);
            float rad = Math.min(1, age / 2f) * 150;
            if (smT[i] - time < 3) rad *= (smT[i] - time) / 3f;
            float dx = x2 - x1, dy = y2 - y1, dz = z2 - z1;
            float l2 = dx * dx + dy * dy + dz * dz; if (l2 < 1) continue;
            float t = ((smX[i] - x1) * dx + (smY[i] - y1) * dy + (smZ[i] - z1) * dz) / l2;
            if (t < 0) t = 0; if (t > 1) t = 1;
            float px = x1 + dx * t - smX[i], py = y1 + dy * t - smY[i], pz = z1 + dz * t - smZ[i];
            if (px * px + py * py + pz * pz < rad * rad) return true;
        }
        return false;
    }

    /** can p see q (world trace + smoke). Checks head and chest. */
    public boolean visible(Player p, Player q) {
        float ex = p.ox, ey = p.oy, ez = p.eyeZ();
        float tz = q.ducked ? q.oz + 8 : q.oz + 22;
        m.trace(0, ex, ey, ez, q.ox, q.oy, tz);
        boolean ok = m.rFrac >= 1;
        if (!ok) { m.trace(0, ex, ey, ez, q.ox, q.oy, q.oz); ok = m.rFrac >= 1; }
        if (!ok) return false;
        return !smokeBlocks(ex, ey, ez, q.ox, q.oy, tz);
    }

    void fx(float x, float y, float z, int col, float life, float size) {
        int i = fxNext; fxNext = (fxNext + 1) % MAXFX;
        fxX[i] = x; fxY[i] = y; fxZ[i] = z; fxCol[i] = col; fxT[i] = time; fxLife[i] = life; fxSize[i] = size;
    }

    // ======================================================================= bomb
    public boolean inSite(Player p) {
        for (int i = 0; i < siteModel.length; i++) if (inModel(siteModel[i], p.ox, p.oy, p.oz, 0)) return true;
        return false;
    }

    int siteAt(float x, float y, float z) {
        for (int i = 0; i < siteModel.length; i++) if (inModel(siteModel[i], x, y, z, 64)) return i;
        int best = 0; float bd = 1e12f;
        for (int i = 0; i < siteModel.length; i++) { float dx = mcx(siteModel[i]) - x, dy = mcy(siteModel[i]) - y; if (dx * dx + dy * dy < bd) { bd = dx * dx + dy * dy; best = i; } }
        return best;
    }

    void bombActions(Player p, float dt) {
        if (client() || mode != 0) return;
        // pick up dropped bomb
        if (bombDropped && p.team == 1 && !bombPlanted) {
            float dx = p.ox - bombX, dy = p.oy - bombY, dz = p.oz - 30 - bombZ;
            if (dx * dx + dy * dy < 40 * 40 && Math.abs(dz) < 60) { bombDropped = false; p.c4 = true; if (p == me) center("Вы подобрали бомбу", 2, 0xFFFFFF); }
        }
        // planting
        if (p.cur == Wp.C4 && p.c4) {
            boolean can = inSite(p) && p.onGround && !roundOver && time > freezeEnd;
            if (p.bFire && can) {
                if (!p.planting) { p.planting = true; p.plantProgress = 0; setVm(p, "shoot", 3); sound(p, "weapons_c4_plant", 40); }
                p.plantProgress += dt;
                if (p.plantProgress >= 3) plant(p);
            } else {
                if (p.bFire && !p.firePrev && p == me && !can) center("Бомбу можно заложить только на точке", 2, 0xFFFFFF);
                if (p.planting) { p.planting = false; setVm(p, "idle", 1); }
            }
        }
        // defusing
        if (bombPlanted && p.team == 2 && !roundOver) {
            float dx = p.ox - bombX, dy = p.oy - bombY, dz = (p.oz - 36) - bombZ;
            boolean near = dx * dx + dy * dy < 56 * 56 && Math.abs(dz) < 64;
            if (near && p.bUse) {
                if (!p.defusing) { p.defusing = true; p.defuseProgress = 0; sound(p, "weapons_c4_disarm", 60); if (p == me) center(p.defuser ? "Обезвреживание (набор)..." : "Обезвреживание...", 2, 0xFFFFFF); }
                p.defuseProgress += dt;
                if (p.defuseProgress >= (p.defuser ? 5 : 10)) {
                    bombPlanted = false;
                    sound(null, "weapons_c4_disarmed", 90);
                    addMoney(p, 250);
                    endRound(2, "Бомба обезврежена! Спецназ победил", "radio_bombdef", 1);
                }
            } else if (p.defusing) p.defusing = false;
        }
    }

    void plant(Player p) {
        p.planting = false; p.c4 = false;
        bombPlanted = true; bombX = p.ox; bombY = p.oy; bombZ = p.oz - (p.ducked ? 18 : 36);
        bombAt = time + 40; bombBeep = time;
        bombSite = siteAt(p.ox, p.oy, p.oz);
        log("t=" + (int) time + " bomb planted by " + p.name + " site " + bombSite);
        soundAll("radio_bombpl", 90);
        centerAll("Бомба заложена!", 3, 0xFF6040);
        for (int i = 0; i < pl.length; i++) if (pl[i].team == 1) addMoney(pl[i], pl[i] == p ? 300 + 800 / 5 : 800 / 5);
        int nw = p.primary >= 0 ? p.primary : p.secondary >= 0 ? p.secondary : Wp.KNIFE;
        p.cur = nw; p.drawEnd = time + 0.6f; setVm(p, "draw", 0.8f);
        for (int i = 0; i < pl.length; i++) if (pl[i].ai != null) pl[i].ai.bombPlanted();
    }

    void updateBomb(float dt) {
        if (!bombPlanted || bombExploded) return;
        float leftT = bombAt - time;
        float interval = leftT > 20 ? 1.4f : leftT > 10 ? 0.9f : leftT > 5 ? 0.45f : 0.2f;
        if (time >= bombBeep) { bombBeep = time + interval; soundAt(bombX, bombY, bombZ, "weapons_c4_beep1", 70, 2500); }
        if (time >= bombAt) {
            bombExploded = true; bombPlanted = false;
            soundAt(bombX, bombY, bombZ, "weapons_c4_explode1", 100, 6000);
            for (int k = 0; k < 10; k++) fx(bombX + rs() * 150, bombY + rs() * 150, bombZ + rf() * 200, 0xFFA030, 1.2f, 90);
            for (int k = 0; k < pl.length; k++) {
                Player q = pl[k]; if (!q.alive) continue;
                float dx = q.ox - bombX, dy = q.oy - bombY, dz = q.oz - bombZ, d = (float) Math.sqrt(dx * dx + dy * dy + dz * dz);
                if (d < 1000) damage(q, null, (int) (500 * (1 - d / 1000) * (1 - d / 1000) * 1.2f + 0.5f), Wp.C4, false, 2);
            }
            for (int k = 0; k < breakModel.length; k++) {
                int md = breakModel[k]; float dx = mcx(md) - bombX, dy = mcy(md) - bombY;
                if (dx * dx + dy * dy < 900 * 900) m.modelActive[md] = false;
            }
            checkRoundEnd();
        }
    }

    // ======================================================================= drawing
    public void draw() {
        hiActive = false;
        if (state == ST_LOAD) { drawLoading(); return; }
        if (state == ST_MENU || state == ST_END) { drawMenuScreen(); return; }
        Player view = me;
        boolean spec = me == null || !me.alive;
        if (spec && specIdx >= 0) view = pl[specIdx];
        float vx = view.ox, vy = view.oy, vz = view.eyeZ();
        float yaw = view.yaw + view.punchY, pitch = view.pitch + view.punchP;
        float fov = 90;
        if (view.zoom > 0 && Wp.zoom1[view.cur] > 0) fov = view.zoom == 1 ? Wp.zoom1[view.cur] : Wp.zoom2[view.cur];
        if (spec && specIdx < 0) { yaw = deadCamYaw + me.yaw; pitch = 15; vz = me.oz + 30; }
        // view bob
        float sp = view.speedFactor();
        if (view.onGround && sp > 30) viewBob += sp * 0.018f * 0.033f * 30 * (1f / Math.max(10, fps > 0 ? fps : 20));
        float bob = viewBobOn ? (float) Math.sin(time * 9) * Math.min(1, sp / 250f) * 1.2f : 0;
        // ---- Nokia FR: between real frames show generated ones (last frame re-projected to the current view angles)
        long nowMs = System.currentTimeMillis();
        if (nfr > 0) {
            if (lastDrawMs > 0) {
                float iv = Math.min(500, nowMs - lastDrawMs);
                if (lastGenerated) genIvMs += (iv - genIvMs) * 0.2f; else realIvMs += (Math.max(iv, realFloorMs) - realIvMs) * 0.25f;
            }
            lastDrawMs = nowMs;
            float gdx = vx - genX, gdy = vy - genY, gdz = vz - genZ;
            if (genLeft > 0 && genValid && fov == genFov && r.W == genW && r.H == genH && gdx * gdx + gdy * gdy + gdz * gdz < 48 * 48) {
                genLeft--; lastGenerated = true; genFrames++;
                drawGenerated(view, spec, yaw, pitch);
                return;
            }
        } else { genValid = false; lastDrawMs = 0; }
        lastGenerated = false; realFrames++;
        r.setCamera(vx, vy, vz + (view.onGround ? bob * 0.5f : 0), yaw, pitch, fov);
        r.renderWorld();
        for (int i = 0; i < drawEnts.length; i++) if (m.modelActive[drawEnts[i]]) r.renderBrushModel(drawEnts[i], 0, 0, 0, true);
        r.renderWorldAlpha();
        drawPlayers(view);
        drawWorldFx();
        if (nfr > 0) { saveGenWorld(yaw, pitch, fov); genX = vx; genY = vy; genZ = vz; }
        // upscale if needed
        boolean vm3 = (!spec || specIdx >= 0) && drawViewModel3D(view, bob);
        if (nfr > 0) { saveGenVm(vm3); planGenerated(); }
        if (r.fb != screen) upscale();
        if (!spec || specIdx >= 0) { if (vm3) drawMuzzle(view); else drawViewModel(view, bob); }
        drawHud(view, spec);
    }

    // ======================================================================= Nokia FR (frame generation) + DLSS (smart upscaling)
    /** 0 = off, else target displayed FPS index into NFR_T */
    public int nfr;
    public static final int[] NFR_T = {0, 10, 15, 20, 25, 30};
    /** PC "как на E52" mode: real frames are paced like the phone, generated frames fill the gaps */
    public float realFloorMs;
    public boolean lastGenerated;
    long lastDrawMs; float realIvMs = 50, genIvMs = 8;
    int genLeft, genW, genH; boolean genValid, genHasVm; float genYaw, genPitch, genFov, genX, genY, genZ;
    int[] genWorld, genVm;
    int realFrames, genFrames; public int realFps, genFps; long nfrStatT; float nfrCool;

    String nfrName() { return nfr == 0 ? "выкл" : NFR_T[nfr] + " FPS"; }

    void saveGenWorld(float yaw, float pitch, float fov) {
        int n = r.W * r.H;
        if (genWorld == null || genWorld.length != n) { genWorld = new int[n]; genVm = new int[n]; }
        System.arraycopy(r.fb, 0, genWorld, 0, n);
        genYaw = yaw; genPitch = pitch; genFov = fov; genW = r.W; genH = r.H; genValid = true;
    }

    /** the first-person weapon is drawn with a fresh z-buffer, so z != 0 marks its pixels */
    void saveGenVm(boolean vm3) {
        genHasVm = vm3;
        if (!vm3) return;
        int[] F = r.fb, Z = r.zb, V = genVm;
        for (int i = V.length - 1; i >= 0; i--) V[i] = Z[i] != 0 ? F[i] : -1;
    }

    /** how many generated frames to put after this real frame, and DLSS resolution control */
    void planGenerated() {
        float target = 1000f / NFR_T[nfr];
        float real = Math.max(realIvMs, realFloorMs), gen = Math.max(2, genIvMs);
        int k = 0;
        if (real > target * 1.05f && gen < target) k = (int) Math.ceil((real - target) / (target - gen));
        if (k > 3) k = 3;
        genLeft = k;
        long now = System.currentTimeMillis();
        if (nfrStatT == 0) nfrStatT = now;
        if (now - nfrStatT >= 1000) {
            float sec = (now - nfrStatT) / 1000f;
            realFps = (int) (realFrames / sec + 0.5f); genFps = (int) (genFrames / sec + 0.5f);
            realFrames = 0; genFrames = 0; nfrStatT = now;
            // DLSS: keep the real (rendered) frame rate around half of the target, upscale the rest
            int T = NFR_T[nfr];
            if (time > nfrCool && state == ST_PLAY && menu == M_NONE) {
                if ((realFps < T * 0.5f || realFps + genFps < T * 0.9f) && rLevel < 4) { setLevel(rLevel + 1); nfrCool = time + 2; genValid = false; }
                else if (realFps > T * 0.8f && rLevel > 0) { setLevel(rLevel - 1); nfrCool = time + 4; genValid = false; }
            }
        }
    }

    /** re-project the last real frame to the current view angles (rotation-only "time warp") */
    void drawGenerated(Player view, boolean spec, float yaw, float pitch) {
        int w = genW, h = genH;
        float dyaw = angNorm(yaw - genYaw), dp = pitch - genPitch;
        if (dyaw > 40) dyaw = 40; if (dyaw < -40) dyaw = -40; if (dp > 40) dp = 40; if (dp < -40) dp = -40;
        int dx = (int) ((float) Math.tan(Math.toRadians(dyaw)) * r.xscale);
        int dy = (int) (-(float) Math.tan(Math.toRadians(dp)) * r.yscale);
        int[] src = genWorld, dst = r.fb;
        for (int y = 0; y < h; y++) {
            int sy = y - dy; if (sy < 0) sy = 0; else if (sy >= h) sy = h - 1;
            int so = sy * w, o = y * w;
            int x0 = dx > 0 ? dx : 0, x1 = dx < 0 ? w + dx : w;
            if (x1 > x0) System.arraycopy(src, so + x0 - dx, dst, o + x0, x1 - x0);
            if (x0 > 0) { int c = src[so]; for (int x = 0; x < x0 && x < w; x++) dst[o + x] = c; }
            if (x1 < w) { int c = src[so + w - 1]; for (int x = Math.max(0, x1); x < w; x++) dst[o + x] = c; }
        }
        if (genHasVm) { int[] V = genVm; for (int i = V.length - 1; i >= 0; i--) if (V[i] != -1) dst[i] = V[i]; }
        if (r.fb != screen) upscale();
        if (!spec || specIdx >= 0) drawMuzzle(view);
        drawHud(view, spec);
    }

    /** "DLSS": bilinear reconstruction of the low-resolution frame instead of blocky pixels */
    void upscaleSmooth(int[] src, int rw, int rh) {
        int[] dst = screen;
        if (sxI == null || sxI.length != W || upW != rw || upH != rh) {
            upW = rw; upH = rh;
            sxI = new int[W]; sxF = new int[W]; syI = new int[H]; syF = new int[H];
            for (int x = 0; x < W; x++) { int f = (int) (((x + 0.5f) * rw / W - 0.5f) * 256); if (f < 0) f = 0; if (f > (rw - 1) * 256) f = (rw - 1) * 256; sxI[x] = f >> 8; sxF[x] = f & 255; }
            for (int y = 0; y < H; y++) { int f = (int) (((y + 0.5f) * rh / H - 0.5f) * 256); if (f < 0) f = 0; if (f > (rh - 1) * 256) f = (rh - 1) * 256; syI[y] = f >> 8; syF[y] = f & 255; }
        }
        for (int y = 0; y < H; y++) {
            int r0 = syI[y] * rw, r1 = syI[y] < rh - 1 ? r0 + rw : r0, fv = syF[y], iv = 256 - fv, o = y * W;
            for (int x = 0; x < W; x++) {
                int i0 = sxI[x], i1 = i0 < rw - 1 ? i0 + 1 : i0, fu = sxF[x], iu = 256 - fu;
                int a = src[r0 + i0], b = src[r0 + i1], c = src[r1 + i0], d = src[r1 + i1];
                int rbA = (((a & 0xFF00FF) * iu + (b & 0xFF00FF) * fu) >>> 8) & 0xFF00FF, gA = (((a & 0xFF00) * iu + (b & 0xFF00) * fu) >>> 8) & 0xFF00;
                int rbB = (((c & 0xFF00FF) * iu + (d & 0xFF00FF) * fu) >>> 8) & 0xFF00FF, gB = (((c & 0xFF00) * iu + (d & 0xFF00) * fu) >>> 8) & 0xFF00;
                dst[o + x] = ((((rbA * iv + rbB * fv) >>> 8) & 0xFF00FF) | (((gA * iv + gB * fv) >>> 8) & 0xFF00));
            }
        }
    }
    int[] sxI, sxF, syI, syF; int upW, upH;

    void upscale() {
        int[] src = r.fb, dst = screen; int rw = r.W, rh = r.H;
        if (rw == W * 2 && rh == H * 2) {   // hi-res 3D: 2x2 box filter into the 320x240 frame, keep a copy to detect HUD pixels
            for (int y = 0; y < H; y++) {
                int s0 = y * 2 * rw, s1 = s0 + rw, o = y * W;
                for (int x = 0; x < W; x++, s0 += 2, s1 += 2) {
                    int a = src[s0], b = src[s0 + 1], c = src[s1], d = src[s1 + 1];
                    int rb = ((a & 0xFF00FF) + (b & 0xFF00FF) + (c & 0xFF00FF) + (d & 0xFF00FF)) >>> 2 & 0xFF00FF;
                    int g = ((a & 0xFF00) + (b & 0xFF00) + (c & 0xFF00) + (d & 0xFF00)) >>> 2 & 0xFF00;
                    dst[o + x] = rb | g;
                }
            }
            if (pc) {   // the PC window shows the sharp 640x480 picture under the HUD
                if (hiBase == null) hiBase = new int[W * H];
                System.arraycopy(dst, 0, hiBase, 0, W * H);
                hiActive = true;
            }
            return;
        }
        if (nfr > 0) { upscaleSmooth(src, rw, rh); return; }
        if (rw * 2 == W && rh * 2 == H) {
            for (int y = 0; y < rh; y++) {
                int so = y * rw, o = y * 2 * W;
                for (int x = 0; x < rw; x++) { int c = src[so + x]; dst[o++] = c; dst[o++] = c; }
                System.arraycopy(dst, y * 2 * W, dst, y * 2 * W + W, W);
            }
            return;
        }
        int[] xm = xmap, ym = ymap;
        int lastY = -1;
        for (int y = 0; y < H; y++) {
            int sy = ym[y], o = y * W;
            if (sy == lastY) { System.arraycopy(dst, o - W, dst, o, W); continue; }
            lastY = sy;
            int so = sy * rw;
            for (int x = 0; x < W; x++) dst[o + x] = src[so + xm[x]];
        }
    }

    // ======================================================================= real 3D models
    Mdl3 mT, mCT, mC4, mBack; Anim3 aT, aCT;
    java.util.Hashtable mdlCache = new java.util.Hashtable();
    java.util.Hashtable mergeCache = new java.util.Hashtable();
    float[] bQ = new float[64 * 4], bP = new float[64 * 3], bW = new float[64 * 12], bW2 = new float[64 * 12], bE = new float[12];
    int[][][] seqAim = new int[2][][], seqShoot = new int[2][][];
    int[][] seqGait = new int[2][], seqDeath = new int[2][];
    static final String[] ACLS = {"knife", "onehanded", "onehanded", "onehanded", "onehanded", "dualpistols", "onehanded", "shotgun", "shotgun", "onehanded",
            "onehanded", "mp5", "carbine", "carbine", "ak47", "carbine", "ak47", "rifle", "rifle", "carbine", "rifle", "rifle", "rifle", "rifle", "m249",
            "grenade", "grenade", "grenade", "c4"};
    static final String[] GAITS = {"idle1", "walk", "run", "crouch_idle", "crouchrun", "jump"};
    static final String[] DEATHS = {"death1", "death2", "death3", "gutshot", "back", "head", "crouch_die"};

    /** 3D players/items for every preset except "minimal" (which keeps the light sprites) */
    public boolean use3D() { return gfx > 0 && mT != null; }

    void load3D() {
        if (mT != null) return;
        try {
            Mdl3.loadTextures();
            Render.buildModelShade(Mdl3.pal);
            mT = Mdl3.load("pl_t"); mCT = Mdl3.load("pl_ct"); aT = Anim3.load("pl_t"); aCT = Anim3.load("pl_ct");
            mC4 = Mdl3.load("w_c4"); mBack = Mdl3.load("w_backpack");
            if (aT == null || aCT == null || mCT == null) { mT = null; return; }
            for (int t = 0; t < 2; t++) {
                Anim3 a = t == 0 ? aT : aCT;
                seqGait[t] = new int[GAITS.length]; for (int i = 0; i < GAITS.length; i++) seqGait[t][i] = a.find(GAITS[i]);
                seqDeath[t] = new int[DEATHS.length]; for (int i = 0; i < DEATHS.length; i++) seqDeath[t][i] = a.find(DEATHS[i]);
                seqAim[t] = new int[ACLS.length][2]; seqShoot[t] = new int[ACLS.length][2];
                for (int w = 0; w < ACLS.length; w++) for (int c = 0; c < 2; c++) {
                    String pre = c == 0 ? "ref_" : "crouch_";
                    seqAim[t][w][c] = a.find(pre + "aim_" + ACLS[w]); seqShoot[t][w][c] = a.find(pre + "shoot_" + ACLS[w]);
                    if (seqAim[t][w][c] < 0) seqAim[t][w][c] = a.find(pre + "aim_ak47");
                }
            }
        } catch (Throwable e) { mT = null; }
    }

    void loadSprites() {
        try {
            if (sprT == null) sprT = Sprites.load("/t.pls", true);
            if (sprCT == null) sprCT = Sprites.load("/ct.pls", true);
        } catch (IOException e) { }
    }

    Mdl3 mdl(String name) {
        Object o = mdlCache.get(name);
        if (o == null) { Mdl3 md = Mdl3.load(name); mdlCache.put(name, md != null ? (Object) md : (Object) "x"); return md; }
        return o instanceof Mdl3 ? (Mdl3) o : null;
    }

    void drawPlayer3D(Player p) {
        int t = p.team == 1 ? 0 : 1;
        Mdl3 md = t == 0 ? mT : mCT; Anim3 an = t == 0 ? aT : aCT;
        if (!p.alive) {
            int k = p.deathAnim == 1 ? 5 : p.ducked ? 6 : p.id % 5;
            int s = seqDeath[t][k]; if (s < 0) s = seqDeath[t][0];
            an.pose(s, (time - p.deathTime) * an.fps[s], false, 0, bQ, bP);
        } else {
            float sp = p.speedFactor();
            int g;
            if (!p.onGround) g = 5; else if (p.ducked) g = sp > 20 ? 4 : 3; else g = sp > 150 ? 2 : sp > 20 ? 1 : 0;
            int gs = seqGait[t][g];
            float gf;
            if (g == 2) gf = p.animTime * 1.4f * an.nfr[gs];
            else if (g == 1) gf = p.animTime * 1.8f * an.nfr[gs];
            else if (g == 4) gf = p.animTime * 2.5f * an.nfr[gs];
            else if (g == 5) gf = an.nfr[gs] * 0.45f;
            else gf = time * an.fps[gs];
            an.pose(gs, gf, g != 5, 0, bQ, bP);
            int w = p.cur < ACLS.length && p.cur >= 0 ? p.cur : 0, c = p.ducked ? 1 : 0;
            int us = seqAim[t][w][c]; float uf = 0;
            int ss = seqShoot[t][w][c];
            if (p.planting && ss >= 0) { us = ss; uf = Math.min(1, p.plantProgress / 3f) * (an.nfr[ss] - 1); }
            else if (ss >= 0 && time - p.lastShot < an.duration(ss)) { us = ss; uf = (time - p.lastShot) * an.fps[ss]; }
            float blend = an.nbl[us] > 1 ? (p.pitch + 45) / 45f : 0;
            an.pose(us, uf, false, blend, bQ, bP);
        }
        Mdl3.entity(p.ox, p.oy, p.oz, p.yaw, bE);
        md.build(bQ, bP, bE, bW);
        int lev = m.lightAt(p.ox, p.oy, p.oz - 20);
        r.drawMesh(md.parts[0][0], bW, lev);
        if (p.c4 && p.alive && md.parts.length > 1 && md.parts[1].length > 1) r.drawMesh(md.parts[1][1], bW, lev);
        if (p.alive && p.cur >= 0 && p.cur < Wp.name.length) {
            Mdl3 pw = mdl("p_" + Wp.name[p.cur]);
            if (pw != null) {
                String key = t + Wp.name[p.cur];
                int[] map = (int[]) mergeCache.get(key);
                if (map == null) { map = pw.mergeMap(md); mergeCache.put(key, map); }
                pw.buildMerged(map, bW, bE, bW2);
                r.drawMesh(pw.parts[0][0], bW2, lev);
            }
        }
    }

    Anim3 vAnim(String name) {
        Object o = mdlCache.get(name + ".a3");
        if (o == null) { Anim3 a = Anim3.load(name); mdlCache.put(name + ".a3", a != null ? (Object) a : (Object) "x"); return a; }
        return o instanceof Anim3 ? (Anim3) o : null;
    }

    String vmSetName(int set) { return set == Wp.N ? "m4a1s" : set == Wp.N + 1 ? "usps" : Wp.name[set]; }
    String lastVmMesh;

    /** first-person weapon as a real 3D model (smoothly interpolated original animations) */
    boolean drawViewModel3D(Player p, float bob) {
        if (mT == null) load3D();
        if (mT == null || !p.alive) return false;
        if (p.zoom > 0 && Wp.isSniper(p.cur)) return false;
        float t0 = time - p.vmTime;
        if (t0 >= p.vmLen && p.vmSet != vmSetFor(p)) { p.vmSet = vmSetFor(p); p.vmName = "idle"; }
        int set = p.vmSet;
        String sn = vmSetName(set);
        String meshName = set == Wp.N ? "v_m4a1" : set == Wp.N + 1 ? "v_usp" : "v_" + sn;
        if (lastVmMesh != null && !lastVmMesh.equals(meshName)) {   // keep memory low on the phone: one weapon at a time
            mdlCache.remove(lastVmMesh);
            mdlCache.remove("v_" + vmSetName(Wp.N) + ".a3"); mdlCache.remove("v_" + vmSetName(Wp.N + 1) + ".a3");
            for (int i = 0; i < Wp.N; i++) if (!("v_" + Wp.name[i]).equals(meshName)) mdlCache.remove("v_" + Wp.name[i] + ".a3");
        }
        lastVmMesh = meshName;
        Mdl3 md = mdl(meshName); Anim3 an = vAnim("v_" + sn);
        if (md == null || an == null) return false;
        String nm = p.vmName == null ? "idle" : p.vmName;
        int s = an.find(nm);
        float t = time - p.vmTime;
        boolean shoot = nm.startsWith("shoot");
        if (s < 0 || (!nm.equals("idle") && t >= (shoot ? an.duration(s) : p.vmLen))) { s = an.find("idle"); nm = "idle"; }
        if (s < 0) return false;
        float fr; boolean loop = false;
        if (nm.equals("idle")) { fr = time * an.fps[s]; loop = true; }
        else if (shoot) fr = t * an.fps[s];
        else fr = p.vmLen > 0 ? t / p.vmLen * (an.nfr[s] - 1) : an.nfr[s] - 1;
        an.pose(s, fr, loop, 0, bQ, bP);
        // light
        int target = m.lightAt(p.ox, p.oy, p.oz);
        lightLevel += (target - lightLevel) * 0.2f;
        int lev = (int) (lightLevel * 0.8f + 8); if (lev > 63) lev = 63;
        // the model is attached to the eye; v_ models are left-handed: model +Y goes to the RIGHT (mirror)
        float sp = viewBobOn ? p.speedFactor() : 0;
        float bx = (float) Math.sin(time * 4.5f) * Math.min(1, sp / 250f) * 0.6f;
        float bz = -Math.abs((float) Math.sin(time * 4.5f)) * Math.min(1, sp / 250f) * 0.6f - p.punchP * 0.08f;
        float ox = r.ex + r.rx * bx + r.ux * bz, oy = r.ey + r.ry * bx + r.uy * bz, oz = r.ez + r.rz * bx + r.uz * bz;
        bE[0] = r.fx; bE[1] = r.rx; bE[2] = r.ux; bE[3] = ox;
        bE[4] = r.fy; bE[5] = r.ry; bE[6] = r.uy; bE[7] = oy;
        bE[8] = r.fz; bE[9] = r.rz; bE[10] = r.uz; bE[11] = oz;
        md.build(bQ, bP, bE, bW);
        r.clearZ();
        for (int pi = 0; pi < md.parts.length; pi++) if (md.parts[pi].length > 0) r.drawMesh(md.parts[pi][0], bW, lev, true, false);
        return true;
    }

    boolean drawStatic3D(String name, float x, float y, float z, float yaw) {
        Mdl3 md = name.equals("w_c4") ? mC4 : name.equals("w_backpack") ? mBack : mdl(name);
        if (md == null) return false;
        Mdl3.entity(x, y, z, yaw, bE);
        md.buildDefault(bE, bW2);
        int lev = m.lightAt(x, y, z + 8);
        for (int pi = 0; pi < md.parts.length; pi++) if (md.parts[pi].length > 0) r.drawMesh(md.parts[pi][0], bW2, lev);
        return true;
    }

    void drawPlayers(Player view) {
        boolean d3 = use3D();
        if (!d3) loadSprites();
        for (int i = 0; i < pl.length; i++) {
            Player p = pl[i];
            if (p == view) continue;
            if (!p.alive && time - p.deathTime > 15) continue;
            if (d3) {
                float ddx = p.ox - r.ex, ddy = p.oy - r.ey;
                if (ddx * ddx + ddy * ddy < 20 * 20) continue;   // a body inside the camera only produces stretched triangles
                drawPlayer3D(p);
                if (p.alive && time - p.lastShot >= 0 && time - p.lastShot < 0.06f && Wp.isGun(p.cur) && !p.lastShotSil) {
                    float cy = (float) Math.cos(Math.toRadians(p.yaw)), sy = (float) Math.sin(Math.toRadians(p.yaw));
                    blob(p.ox + cy * 30, p.oy + sy * 30, p.oz + (p.ducked ? 2 : 14), 4, 0xFFD070, 200);
                }
                continue;
            }
            Sprites s = p.team == 1 ? sprT : sprCT;
            int an; int fr;
            if (!p.alive) { an = s.find(p.deathAnim == 1 ? "die2" : "die"); int nf = s.nfr[an]; fr = (int) ((time - p.deathTime) / 0.14f); if (fr >= nf) fr = nf - 1; }
            else {
                float sp = p.speedFactor();
                if (p.ducked) { if (sp > 20) { an = s.find("crouchrun"); fr = (int) (p.animTime * 3) % s.nfr[an]; } else { an = s.find("crouch"); fr = 0; } }
                else if (sp > 20) { an = s.find("run"); fr = (int) (p.animTime * 6.5f) % s.nfr[an]; }
                else { an = s.find("stand"); fr = 0; }
            }
            // direction
            float toCam = atan2(r.ey - p.oy, r.ex - p.ox);
            float rel = angNorm(toCam - p.yaw);
            int d = ((int) Math.floor((rel + 22.5f) / 45f) % 8 + 8) % 8;
            int idx = d * s.nfr[an] + fr;
            int lev = m.lightAt(p.ox, p.oy, p.oz - 20);
            int[] pal = s.lit(Math.min(63, lev + 4));
            int fw = s.fw[an][idx], fh = s.fh[an][idx];
            r.drawSprite(s.data[an][idx], fw, fh, pal, p.ox, p.oy, p.oz, s.ax - s.fx[an][idx], s.ay - s.fy[an][idx], s.ppu);
            // muzzle flash on others
            if (p.alive && time - p.lastShot >= 0 && time - p.lastShot < 0.06f && Wp.isGun(p.cur) && !p.lastShotSil) {
                float cy = (float) Math.cos(Math.toRadians(p.yaw)), sy = (float) Math.sin(Math.toRadians(p.yaw));
                blob(p.ox + cy * 30, p.oy + sy * 30, p.oz + (p.ducked ? 2 : 14), 4, 0xFFD070, 200);
            }
        }
        // bomb
        if (bombPlanted || bombDropped) {
            drawItem(bombPlanted ? "w_c4" : "w_backpack", bombX, bombY, bombZ + 1, 30);
            if (bombPlanted && ((int) (time * 4) & 1) == 0) blob(bombX, bombY, bombZ + 6, 1.5f, 0xFF2020, 256);
        }
        // dropped weapons
        for (int i = 0; i < MAXD; i++) if (dOn[i]) drawItem("w_" + (dW[i] == Wp.MP5 ? "mp5" : Wp.name[dW[i]]), dX[i], dY[i], dZ[i], dYaw[i]);
        // grenades in flight
        for (int i = 0; i < MAXG; i++) if (grOn[i]) {
            String gn = grType[i] == Wp.HE ? "w_hegrenade" : grType[i] == Wp.FLASH ? "w_flashbang" : "w_smokegrenade";
            if (!use3D() || !drawStatic3D(gn, grX[i], grY[i], grZ[i] - 3, (time * 540 + i * 77) % 360))
                blob(grX[i], grY[i], grZ[i], 2.5f, grType[i] == Wp.HE ? 0x405030 : grType[i] == Wp.FLASH ? 0xB0B0B0 : 0x506080, 256);
        }
    }

    /** solid / translucent round blob billboard (z tested) */
    void drawItem(String name, float x, float y, float z, float yaw) {
        if (mT == null) load3D();
        if (drawStatic3D(name, x, y, z, yaw)) return;
        if (sprW == null) return;
        int an = sprW.find(name); if (an < 0) return;
        float toCam = atan2(r.ey - y, r.ex - x);
        int d = ((int) Math.floor((angNorm(toCam - yaw) + 22.5f) / 45f) % 8 + 8) % 8;
        int lev = m.lightAt(x, y, z + 8);
        r.drawSprite(sprW.data[an][d], sprW.fw[an][d], sprW.fh[an][d], sprW.lit(Math.min(63, lev + 4)), x, y, z, sprW.ax - sprW.fx[an][d], sprW.ay - sprW.fy[an][d], sprW.ppu);
    }

    void blob(float x, float y, float z, float rad, int col, int alpha) {
        if (!r.project(x, y, z)) return;
        boolean smoke = col == 0xB4B4B4;
        if (r.projZ < (smoke ? 4 : 24)) return;   // right in front of the lens: would cover the screen
        float s = rad * r.xscale / r.projZ;
        float cap = col == 0xFFA030 ? r.W / 4f : r.W / 10f;   // explosions may be bigger
        if (!smoke && s > cap) s = cap;   // effects never grow into giant discs
        int zf = Render.zOf(r.projZ);
        int x0 = (int) (r.projX - s), x1 = (int) (r.projX + s), y0 = (int) (r.projY - s), y1 = (int) (r.projY + s);
        if (x1 < 0 || y1 < 0 || x0 >= r.W || y0 >= r.H) return;
        if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 >= r.W) x1 = r.W - 1; if (y1 >= r.H) y1 = r.H - 1;
        float s2 = s * s;
        int cr = (col >> 16) & 255, cg = (col >> 8) & 255, cb = col & 255;
        int[] fb = r.fb, zb = r.zb; int RW = r.W;
        for (int yy = y0; yy <= y1; yy++) {
            float dy = yy + 0.5f - r.projY;
            for (int xx = x0; xx <= x1; xx++) {
                float dx = xx + 0.5f - r.projX;
                float dd = dx * dx + dy * dy;
                if (dd > s2) continue;
                int p = yy * RW + xx;
                if (zb[p] > zf) continue;
                if (alpha >= 256) { fb[p] = col; continue; }
                int a = (int) (alpha * (1 - dd / s2));
                int c = fb[p];
                int rr = (c >> 16) & 255, gg = (c >> 8) & 255, bb = c & 255;
                rr += ((cr - rr) * a) >> 8; gg += ((cg - gg) * a) >> 8; bb += ((cb - bb) * a) >> 8;
                fb[p] = (rr << 16) | (gg << 8) | bb;
            }
        }
    }

    void drawWorldFx() {
        for (int i = 0; i < MAXFX && gfx > 0; i++) {
            float age = time - fxT[i];
            if (fxLife[i] <= 0 || age > fxLife[i] || age < 0) continue;
            float k = 1 - age / fxLife[i];
            int col = fxCol[i];
            if (col == 0x303030) blob(fxX[i], fxY[i], fxZ[i], fxSize[i], col, 256);
            else blob(fxX[i], fxY[i], fxZ[i] + (col == 0xC8B898 ? age * 30 : 0), fxSize[i] * (col == 0xFFA030 ? 1.4f - k * 0.4f : 1), col, (int) (200 * k) + 30);
        }
        for (int i = 0; i < MAXS; i++) {
            if (!smOn[i]) continue;
            float age = time - (smT[i] - 18);
            float rad = Math.min(1, age / 2f) * 130;
            float fade = smT[i] - time < 3 ? (smT[i] - time) / 3f : 1;
            for (int k = 0; k < 5; k++) {
                float ox = (k == 1 ? 60 : k == 2 ? -60 : 0) * Math.min(1, age / 2f), oy = (k == 3 ? 60 : k == 4 ? -60 : 0) * Math.min(1, age / 2f);
                blob(smX[i] + ox, smY[i] + oy, smZ[i] - (k > 0 ? 20 : 0), rad * (k == 0 ? 1 : 0.8f), 0xB4B4B4, (int) (250 * fade));
            }
        }
    }

    void drawViewModel(Player p, float bob) {
        if (p.zoom > 0 && Wp.isSniper(p.cur)) { drawScope(); return; }
        if (!p.alive) return;
        float t0 = time - p.vmTime;
        if (t0 >= p.vmLen && p.vmSet != vmSetFor(p)) { p.vmSet = vmSetFor(p); Sprites s2 = vm(p.vmSet); p.vmAnim = s2 == null ? 0 : Math.max(0, s2.find("idle")); }
        Sprites s = vm(p.vmSet);
        if (s == null) return;
        int an = p.vmAnim;
        if (an < 0 || an >= s.anim.length) an = 0;
        float t = time - p.vmTime;
        int nf = s.nfr[an];
        int f;
        if (t >= p.vmLen || p.vmLen <= 0) {
            if (!s.anim[an].equals("idle")) { int id = s.find("idle"); if (id >= 0) { an = id; p.vmAnim = id; nf = s.nfr[an]; } }
            f = nf - 1; if (s.anim[an].equals("idle")) f = 0;
        } else f = (int) (t / p.vmLen * nf);
        if (f >= nf) f = nf - 1; if (f < 0) f = 0;
        // light
        int target = m.lightAt(p.ox, p.oy, p.oz);
        lightLevel += (target - lightLevel) * 0.2f;
        int lev = (int) (lightLevel * 0.85f + 10); if (lev > 63) lev = 63;
        int[] pal = s.lit(lev);
        float sp = viewBobOn ? p.speedFactor() : 0;
        int bx = (int) ((float) Math.sin(time * 4.5f) * Math.min(1, sp / 250f) * 3);
        int by = (int) (Math.abs((float) Math.sin(time * 4.5f)) * Math.min(1, sp / 250f) * 3) + (int) (-p.punchP * 0.6f);
        gx.blit(s.data[an][f], s.fw[an][f], s.fh[an][f], s.fx[an][f] + bx, s.fy[an][f] + by + 2, pal);
        drawMuzzle(p);
    }

    void drawMuzzle(Player p) {
        if (time < muzzleUntil && Wp.isGun(p.cur) && p == me) {
            // muzzle flash glow near screen centre-right
            int cx = W / 2 + 46, cy = H / 2 + 34;
            for (int k = 0; k < 3; k++) gx.fill(cx - 6 + k * 2, cy - 4 + k, 12 - k * 4, 8 - k * 2, k == 2 ? 0xFFFFE0 : 0xFFC040, 160 + k * 40);
        }
    }

    void drawScope() {
        int cx = W / 2, cy = H / 2, rad = H / 2 - 4;
        for (int y = 0; y < H; y++) {
            int dy = y - cy, o = y * W;
            int half = dy * dy < rad * rad ? (int) Math.sqrt(rad * rad - dy * dy) : -1;
            for (int x = 0; x < W; x++) {
                if (half < 0 || x < cx - half || x > cx + half) screen[o + x] = 0;
            }
        }
        gx.fill(0, cy, W, 1, 0, 256); gx.fill(cx, 0, 1, H, 0, 256);
    }

    void drawHud(Player v, boolean spec) {
        int col = Gfx.HUDCOL;
        if (hurtFlash > 0 && !spec) gx.tint(0xC00000, (int) (hurtFlash * 300));
        if (v.flashUntil > time) {
            float left = v.flashUntil - time;
            gx.tint(0xFFFFFF, left > 1.5f ? 256 : (int) (left / 1.5f * 256));
        }
        // crosshair
        if (!spec && !(v.zoom > 0 && Wp.isSniper(v.cur)) && Wp.isGun(v.cur) && !Wp.isSniper(v.cur)) {
            float spd = v.speedFactor();
            int gap = 2 + crossSize + (crossDyn ? (int) (Wp.spread[v.cur] * 60 + v.recoil * 200 + Math.min(1, spd / 220f) * Wp.moveSpread[v.cur] * 100 + (v.onGround ? 0 : 10)) : 1);
            int cx = W / 2, cy = H / 2, len = 3 + crossSize * 2, cc = CROSSC[crossCol];
            gx.fill(cx - gap - len, cy, len, 1, cc, 256); gx.fill(cx + gap + 1, cy, len, 1, cc, 256);
            gx.fill(cx, cy - gap - len, 1, len, cc, 256); gx.fill(cx, cy + gap + 1, 1, len, cc, 256);
        } else if (!spec && (v.cur == Wp.KNIFE || Wp.isGrenade(v.cur) || v.cur == Wp.C4 || Wp.isSniper(v.cur)) && v.zoom == 0) {
            gx.fill(W / 2, H / 2, 1, 1, CROSSC[crossCol], 256);
        }
        int y = H - 16;
        int a = 200;
        if (!spec) {
            // health
            int hc = v.health <= 25 ? 0xFF2020 : col;
            gx.hud(gx.hudId("cross"), 4, y - 1, hc, a); gx.number(v.health, 18, y, hc, a, 1);
            // armor
            gx.hud(gx.hudId(v.helmet ? "suithelmet_full" : "suit_full"), 62, y - 1, col, v.armor > 0 ? a : 90); gx.number(v.armor, 76, y, col, a, 1);
            // ammo
            if (Wp.isGun(v.cur)) {
                int w2 = gx.numberW(v.ammo[v.cur], 1);
                gx.number(v.ammo[v.cur], W - 4 - w2, y, col, a, 1);
                gx.hud(gx.hudId("divider"), W - 10 - w2, y - 4, col, a);
                int w1 = gx.numberW(v.clip[v.cur], 1);
                gx.number(v.clip[v.cur], W - 16 - w2 - w1, y, col, a, 1);
            } else if (Wp.isGrenade(v.cur)) {
                int c = v.cur == Wp.HE ? v.he : v.cur == Wp.FLASH ? v.flash : v.smoke;
                gx.number(c, W - 16, y, col, a, 1);
            }
            // money
            int mw = gx.numberW(v.money, 1);
            gx.hud(gx.hudId("dollar"), W - 14 - mw, y - 22, col, a); gx.number(v.money, W - 4 - mw, y - 22, col, a, 1);
            // icons
            if (canBuy(v)) gx.hud(gx.hudId("buyzone"), 2, H / 2 - 20, 0x00C000, 220);
            if (v.c4) gx.hud(gx.hudId("c4"), 2, H / 2 + 2, inSite(v) && ((int) (time * 2) & 1) == 0 ? 0xFF2020 : 0x00C000, 220);
            if (v.defuser) gx.hud(gx.hudId("defuser"), 2, H / 2 + 2, 0x00C000, 220);
        }
        // timer
        float tl;
        if (bombPlanted) tl = -1;
        else if (time < freezeEnd) tl = freezeEnd - time;
        else tl = Math.max(0, freezeEnd + roundTime - time);
        if (tl >= 0) {
            int mm = (int) tl / 60, ss = (int) tl % 60;
            int tx = W / 2 - 26, ty = port() ? y - 22 : y;
            gx.hud(gx.hudId("stopwatch"), tx - 14, ty - 1, col, a);
            int wmm = gx.number(mm, tx, ty, col, a, 1);
            gx.fill(tx + wmm + 1, ty + 3, 2, 2, 0xFFA000, 200); gx.fill(tx + wmm + 1, ty + 8, 2, 2, 0xFFA000, 200);
            gx.number(ss, tx + wmm + 5, ty, col, a, 2);
        } else {
            boolean bl = ((int) (time * 4) & 1) == 0;
            gx.hud(gx.hudId("c4"), W / 2 - 8, (port() ? y - 22 : y) - 4, bl ? 0xFF2020 : 0x800000, 230);
        }
        if (mode == 2 && v.zombie) gx.tint(0x30FF30, 40);
        // score line
        String sc = mode == 2 ? "Зомби " + scoreT + " : " + scoreCT + " Люди" : "T " + scoreT + " : " + scoreCT + " CT";
        gx.textC(gx.f1, sc, W / 2, 2, 0xE0E0E0);
        // radar
        if (radarOn) drawRadar(v);
        // radio / chat lines
        for (int i = 0; i < chatS.length; i++) {
            if (chatS[i] == null || time - chatT[i] > 5) continue;
            gx.textS(gx.f1, chatS[i], 4, H - 56 - i * 13, chatC[i]);
        }
        // killfeed
        int ky = port() ? 14 : 2;
        for (int i = 0; i < kfA.length; i++) {
            if (kfB[i] == null || time - kfT[i] > 6) continue;
            int wi = kfW[i];
            int wb = gx.textW(gx.f1, kfB[i]), wa = gx.textW(gx.f1, kfA[i]), wic = gx.hudW(wi), whs = kfHS[i] ? gx.hudW(gx.hudId("d_headshot")) + 2 : 0;
            int x = W - 4 - wb - wic - whs - wa - 6;
            gx.fill(x - 2, ky, W - x, 13, 0, 110);
            gx.textS(gx.f1, kfA[i], x, ky, kfColA[i]); x += wa + 3;
            gx.hud(wi, x, ky + 2, 0xFF4040, 255); x += wic + 1;
            if (kfHS[i]) { gx.hud(gx.hudId("d_headshot"), x, ky + 2, 0xFF4040, 255); x += whs; }
            gx.textS(gx.f1, kfB[i], x + 2, ky, kfColB[i]);
            ky += 14;
        }
        // planting/defusing bar
        if (!spec && (v.planting || v.defusing)) {
            float pr = v.planting ? v.plantProgress / 3 : v.defuseProgress / (v.defuser ? 5 : 10);
            gx.fill(W / 2 - 60, H - 52, 120, 8, 0, 160); gx.fill(W / 2 - 59, H - 51, (int) (118 * Math.min(1, pr)), 6, 0xFFA000, 220);
        }
        if (spec) {
            String s = specIdx >= 0 ? "Наблюдение: " + pl[specIdx].name + " (5 — следующий)" : "Вы мертвы";
            gx.fill(0, H - 18, W, 18, 0, 150);
            gx.textC(gx.f1, s, W / 2, H - 15, 0xFFFFFF);
            if (specIdx >= 0) { Player q = pl[specIdx]; gx.textS(gx.f1, "HP " + q.health, 4, H - 32, 0xFFA000); }
        }
        if (centerMsg != null && time < centerMsgUntil) gx.textC(gx.f2, centerMsg, W / 2, H / 3 - 10, centerCol);
        drawVoice();
        if (time < freezeEnd && !roundOver && !spec) gx.textC(gx.f1, "Закупка: # (решётка)", W / 2, H / 3 + 12, 0xD0D0D0);
        if (showFps) gx.textS(gx.f1, nfr > 0 ? fps + " fps (+" + genFps + " Nokia FR)" : fps + " fps" + (quality == 0 ? " (" + r.W + "x" + r.H + ")" : ""), 4, 80, 0x80FF80);
        if (buyOpen) drawBuyMenu();
        if (scoreOpen) drawScoreboard();
        if (menu != M_NONE) drawMenuOverlay();
    }

    void drawRadar(Player v) {
        int R = 30, cx = 4 + R, cy = 16 + R;
        for (int yy = -R; yy <= R; yy++) {
            int hw = (int) Math.sqrt(R * R - yy * yy);
            gx.fill(cx - hw, cy + yy, hw * 2 + 1, 1, 0x103010, 150);
        }
        float cyw = (float) Math.cos(Math.toRadians(v.yaw)), syw = (float) Math.sin(Math.toRadians(v.yaw));
        float scale = R / 1600f;
        for (int i = 0; i < pl.length; i++) {
            Player q = pl[i];
            if (!q.alive || q == v) continue;
            if (q.team != v.team && !(time - q.nextAttack > -1.2f && Wp.isGun(q.cur) && time - q.nextAttack < 1)) continue; // enemies only when shooting
            dot(q.ox, q.oy, v, cyw, syw, scale, cx, cy, R, q.team != v.team ? 0xFF3030 : 0x4080FF, 3);
        }
        if ((bombPlanted || bombDropped) && v.team == 1) dot(bombX, bombY, v, cyw, syw, scale, cx, cy, R, ((int) (time * 4) & 1) == 0 ? 0xFF2020 : 0xFFFFFF, 3);
        if (bombPlanted && v.team == 2) dot(bombX, bombY, v, cyw, syw, scale, cx, cy, R, ((int) (time * 4) & 1) == 0 ? 0xFF2020 : 0xFFFFFF, 3);
        gx.fill(cx, cy, 2, 2, 0xFFFFFF, 256);
    }

    void dot(float x, float y, Player v, float cyw, float syw, float sc, int cx, int cy, int R, int col, int sz) {
        float dx = x - v.ox, dy = y - v.oy;
        float fx2 = dx * cyw + dy * syw, rx = dx * syw - dy * cyw;
        int px = (int) (rx * sc), py = (int) (-fx2 * sc);
        if (px * px + py * py > (R - 2) * (R - 2)) { float l = (float) Math.sqrt(px * px + py * py); px = (int) (px / l * (R - 2)); py = (int) (py / l * (R - 2)); }
        gx.fill(cx + px - 1, cy + py - 1, sz, sz, col, 256);
    }









    // ======================================================================= CS 1.6-style interface
    int[] uiBg, uiLogo, uiTeamT, uiTeamCT; int[] uiLogoWH = new int[2], uiTeamTWH = new int[2], uiTeamCTWH = new int[2];
    int[] uiPic; int[] uiPicWH = new int[2]; String uiPicName;
    boolean uiLoaded;
    static final String[] EQUIP_PIC = {"kevlar", "kevlar_helmet", "flashbang", "hegrenade", "smokegrenade", "defuser"};

    void loadUi() {
        if (uiLoaded) return;
        uiLoaded = true;
        int[] wh = new int[2];
        uiBg = Sys.s.image(port() ? "/ui_bg_p.png" : "/ui_bg.png", wh);
        if (uiBg != null && (wh[0] != W || wh[1] != H)) uiBg = null;
        uiLogo = Sys.s.image("/ui_logo.png", uiLogoWH);
    }

    void freeUi() { uiBg = null; uiLogo = null; uiLoaded = false; uiTeamT = uiTeamCT = null; uiPic = null; uiPicName = null; }

    int[] pic(String name) {
        if (name == null) return null;
        if (!name.equals(uiPicName)) { uiPicName = name; uiPic = Sys.s.image("/ui_w_" + name + ".png", uiPicWH); }
        return uiPic;
    }

    void drawBg() {
        loadUi();
        if (uiBg != null) System.arraycopy(uiBg, 0, screen, 0, W * H);
        else for (int y = 0; y < H; y++) { int c = 0x101828 + ((y * 24 / H) << 8); for (int x = 0; x < W; x++) screen[y * W + x] = c; }
    }

    /** the main GameUI menu: logo + plain text items on the left (Main.Menu.X/Y of TrackerScheme) */
    void drawMainMenu(String[] it, boolean active) {
        int n = it.length, top = Math.max(118, H - 16 - n * 15);
        if (port() && state == ST_PLAY) top -= 44;   // keep the pause menu above the HUD
        loadUi();
        if (uiLogo != null) gx.blitARGB(uiLogo, uiLogoWH[0], uiLogoWH[1], 26, top - 24);
        else gx.textS(gx.f2, "COUNTER-STRIKE", 26, top - 28, 0xFFFFFF);
        for (int i = 0; i < n; i++) {
            boolean armed = active && i == menuSel;
            gx.textS(gx.f1, it[i], 26, top + i * 15, armed ? 0xFFFFFF : Gfx.DULL);
        }
        gx.text(gx.f1, "Nokia E52 Edition v" + VERSION, W - 6 - gx.textW(gx.f1, "Nokia E52 Edition v" + VERSION), H - 13, 0x9A9A9A);
    }

    boolean isDialog() {
        return menu == M_CREATE || menu == M_MP || menu == M_LAN || menu == M_WORLD || menu == M_FIRST1 || menu == M_FIRST2 || optTab() >= 0;
    }

    static boolean isButton(String s) { return s.equals("OK") || s.equals("Начать") || s.equals("Отмена") || s.equals("Сбросить") || s.equals("Найти игру") || s.equals("Назад"); }

    /** value 0..1000 for slider rows, -1 otherwise */
    int sliderOf(int idx) {
        if (menu == M_GFX && idx == 3) return gammaIdx * 250;
        if (menu == M_CTRL && idx == 1) return (sens - 1) * 1000 / 9;
        if (menu == M_CTRL && idx == 2) return (sensV - 1) * 1000 / 9;
        if (menu == M_SND && idx == 1) return (volume - 10) * 1000 / 90;
        return -1;
    }

    /** Steam-styled dialog (Options with tabs, New game, first run) */
    void drawDialog() {
        String[] it = menuItems();
        int tab = optTab();
        boolean list = menu == M_FIRST1 || menu == M_FIRST2;
        int nb = 0; while (nb < it.length && isButton(it[it.length - 1 - nb])) nb++;
        int nr = it.length - nb;
        int x = 4, y = 4, w = W - 8, h = H - 8;
        boolean port = port(), two = port && !list;   // portrait: label line + control line
        String[] tabN = port ? OPT_TAB_SHORT : OPT_TAB_NAMES;
        if (list) { w = Math.min(290, W - 8); h = 40 + nr * 16; x = (W - w) / 2; y = (H - h) / 2; }
        gx.frame(x, y, w, h, menuTitle());
        int px = x + 6, py = y + 17, pw = w - 12, ph = h - 17 - (nb > 0 ? (port ? 36 : 24) : 6);
        if (tab >= 0) {
            // five tabs: narrow the padding if they do not fit, then slide the row so the active tab is always whole
            int nt = OPT_TABS.length, pad = 14, sum = 0;
            for (int i = 0; i < nt; i++) sum += gx.textW(gx.f1, tabN[i]) + pad + 1;
            if (sum > pw) { pad = 7; sum = 0; for (int i = 0; i < nt; i++) sum += gx.textW(gx.f1, tabN[i]) + pad + 1; }
            int upTo = 0; for (int i = 0; i <= tab; i++) upTo += gx.textW(gx.f1, tabN[i]) + pad + 1;
            int shift = sum > pw ? Math.max(0, Math.min(sum - pw, upTo - pw + 8)) : 0;
            int tx = px - shift;
            for (int i = 0; i < nt; i++) {
                int tw = gx.textW(gx.f1, tabN[i]) + pad;
                if (tx < px - 1 || tx + tw > px + pw + 1) { tx += tw + 1; continue; }   // only whole tabs
                boolean act = i == tab;
                gx.fill(tx, py + (act ? 0 : 2), tw, act ? 15 : 13, act ? Gfx.FRAME : Gfx.FIELD, 256);
                gx.fill(tx, py + (act ? 0 : 2), tw, 1, Gfx.BRIGHT, 256); gx.fill(tx, py + (act ? 0 : 2), 1, act ? 15 : 13, Gfx.BRIGHT, 256);
                gx.fill(tx + tw - 1, py + (act ? 0 : 2), 1, act ? 15 : 13, Gfx.DARK, 256);
                gx.text(gx.f1, tabN[i], tx + pad / 2, py + (act ? 2 : 3), act ? (tabFocus ? 0xFFFFFF : Gfx.CHECK) : Gfx.OFFWHITE);
                if (act && tabFocus) { gx.rect(tx + 2, py + 2, tw - 4, 12, 0); }
                tx += tw + 1;
            }
            py += 14; ph -= 14;
            gx.fill(px, py, pw, 1, Gfx.BRIGHT, 256);           // panel top edge, broken under the active tab
            int ax = px - shift; for (int i = 0; i < tab; i++) ax += gx.textW(gx.f1, tabN[i]) + pad + 1;
            gx.fill(ax + 1, py, gx.textW(gx.f1, tabN[tab]) + pad - 2, 1, Gfx.FRAME, 256);
            gx.fill(px, py, 1, ph, Gfx.BRIGHT, 256); gx.fill(px + pw - 1, py, 1, ph, Gfx.DARK, 256); gx.fill(px, py + ph - 1, pw, 1, Gfx.DARK, 256);
        } else if (list) {
            gx.fill(px, py + 2, pw, ph - 2, Gfx.FIELD, 256); gx.bevel(px, py + 2, pw, ph - 2, false);
        }
        boolean focusRows = !tabFocus || tab < 0;
        // ---- rows
        boolean games = menu == M_WORLD || menu == M_LAN;   // game lists: two lines a row
        int rowH = list ? 16 : games ? 26 : two ? (menu == M_BINDS ? 24 : 28) : 14;
        int top = py + (list ? 5 : 4);
        if (menu == M_BINDS) top += 13;
        int maxRows = (py + ph - 3 - top) / rowH;
        int vis = Math.min(nr, maxRows), first = 0;
        if (nr > vis) { int sel = Math.min(menuSel, nr - 1); first = sel - vis / 2; if (first < 0) first = 0; if (first > nr - vis) first = nr - vis; }
        if (menu == M_BINDS) {   // list header like the "Клавиатура" page
            int hx = px + 4, hw = pw - 8;
            if (two) {
                gx.fill(hx, top - 13, hw, 12, Gfx.FRAME, 256); gx.bevel(hx, top - 13, hw, 12, true);
                gx.text(gx.f1, "Действие → клавиша", hx + 4, top - 12, Gfx.OFFWHITE);
            } else {
            gx.fill(hx, top - 13, hw, 12, Gfx.FRAME, 256); gx.bevel(hx, top - 13, hw * 55 / 100, 12, true); gx.bevel(hx + hw * 55 / 100, top - 13, hw - hw * 55 / 100, 12, true);
            gx.text(gx.f1, "Действие", hx + 4, top - 12, Gfx.OFFWHITE); gx.text(gx.f1, "Клавиша", hx + hw * 55 / 100 + 4, top - 12, Gfx.OFFWHITE);
            }
            gx.fill(hx, top - 1, hw, vis * rowH + 2, Gfx.FIELD, 256);
        }
        int cx = px + pw * 55 / 100, cw = pw - (cx - px) - 8;
        if (two) { cx = px + 18; cw = pw - 18 - (nr > vis ? 18 : 8); }
        for (int r0 = 0; r0 < vis; r0++) {
            int i = first + r0, ry = top + r0 * rowH;
            boolean sel = focusRows && i == menuSel;
            String t = it[i];
            if (list || menu == M_BINDS) {
                if (sel) gx.fill(px + 4, ry, pw - 8 - (two && nr > vis ? 12 : 0), rowH - 1, Gfx.ORANGE, 256);
                int tab2 = t.indexOf('\t');
                if (tab2 < 0) gx.text(gx.f1, t, px + 10, ry + (rowH - 11) / 2, sel ? 0xFFFFFF : Gfx.OFFWHITE);
                else if (two) {
                    gx.text(gx.f1, t.substring(0, tab2), px + 8, ry + 1, sel ? 0xFFFFFF : Gfx.OFFWHITE);
                    gx.text(gx.f1, t.substring(tab2 + 1), px + 22, ry + 12, sel ? 0xFFFFFF : Gfx.CHECK);
                } else {
                    gx.text(gx.f1, t.substring(0, tab2), px + 8, ry + 1, sel ? 0xFFFFFF : Gfx.OFFWHITE);
                    gx.text(gx.f1, t.substring(tab2 + 1), px + 4 + (pw - 8) * 55 / 100 + 4, ry + 1, sel ? 0xFFFFFF : Gfx.OFFWHITE);
                }
                continue;
            }
            if (games) {
                int t1 = t.indexOf('\t'), t2 = t1 < 0 ? -1 : t.indexOf('\t', t1 + 1);
                int bw = pw - 6 - (nr > vis ? 12 : 0);
                if (sel) gx.fill(px + 3, ry, bw, rowH - 2, Gfx.ORANGE, 256);
                if (t2 < 0) { gx.text(gx.f1, t, px + 8, ry + (rowH - 13) / 2, sel ? 0xFFFFFF : t.startsWith(">") ? Gfx.VG : Gfx.OFFWHITE); continue; }   // a button, or the status line (plain)
                String cnt = t.substring(t1 + 1, t2);
                gx.text(gx.f1, t.substring(0, t1), px + 8, ry + 2, sel ? 0xFFFFFF : Gfx.VG);
                gx.text(gx.f1, cnt, px + 3 + bw - 6 - gx.textW(gx.f1, cnt), ry + 2, sel ? 0xFFFFFF : Gfx.OFFWHITE);
                gx.text(gx.f1, t.substring(t2 + 1), px + 20, ry + 13, sel ? 0xFFFFFF : Gfx.DULL);
                continue;
            }
            int c = t.indexOf(": ");
            String label = c < 0 ? t : t.substring(0, c), val = c < 0 ? "" : t.substring(c + 2);
            if (c < 0) {   // an action row (no control)
                if (sel) gx.fill(px + 3, ry - 1, pw - 6 - (nr > vis ? 12 : 0), 13, Gfx.ORANGE, 256);
                gx.text(gx.f1, label, px + 8, ry + 1, sel ? 0xFFFFFF : Gfx.VG);
                continue;
            }
            if (sel) gx.fill(px + 3, ry - 1, two ? (val.length() > 0 ? 6 + gx.textW(gx.f1, label) + 6 : pw - 6) : cx - px - 6, two && val.length() == 0 ? 14 : rowH - 1, Gfx.ORANGE, 256);
            if (sel && two && val.length() > 0) gx.fill(px + 3, ry - 1, 3, rowH - 1, Gfx.ORANGE, 256);
            gx.text(gx.f1, label, px + 8, ry + 1, sel ? 0xFFFFFF : Gfx.OFFWHITE);
            if (two) ry += 13;
            int sv = sliderOf(i);
            if (val.equals("вкл") || val.equals("выкл")) {
                gx.checkbox(cx, ry + 1, val.equals("вкл"));
                gx.text(gx.f1, val.equals("вкл") ? "Включено" : "Выключено", cx + 14, ry + 1, sel ? 0xFFFFFF : Gfx.DULL);
            } else if (sv >= 0) {
                gx.slider(cx, ry + 1, cw - 34, sv, sel);
                gx.text(gx.f1, val, cx + cw - 28, ry + 1, sel ? 0xFFFFFF : Gfx.OFFWHITE);
            } else gx.combo(cx, ry, cw, val, sel);
        }
        if (nr > vis) {   // scrollbar
            int sx = px + pw - 12, sy = top, sh = vis * rowH;
            if (menu == M_BINDS) { sx = px + pw - 15; }
            gx.fill(sx, sy, 8, sh, 0x262626, 256);
            int kh = Math.max(10, sh * vis / nr), ky = sy + (sh - kh) * first / Math.max(1, nr - vis);
            gx.fill(sx, ky, 8, kh, 0x6E6E6E, 256); gx.bevel(sx, ky, 8, kh, true);
        }
        // ---- buttons
        if (nb > 0) {
            int bx = x + w - 8, by = y + h - 19;
            for (int b = nb - 1; b >= 0; b--) {
                int i = nr + b;
                int bw = Math.max(56, gx.textW(gx.f1, it[i]) + 16);
                bx -= bw;
                gx.button(bx, by, bw, 14, it[i], focusRows && menuSel == i);
                bx -= 6;
            }
            String hint = pc && pcMouse ? (menu == M_BINDS ? "Enter, A/D" : "Стрелки, Enter, Esc")
                    : (menu == M_BINDS ? "5 назначить" : tab >= 0 ? "2/8 4/6 5, */# вкладки" : "2/8 выбор, 4/6 изменить");
            gx.text(gx.f1, hint, x + 8, port ? by - 13 : by + 2, 0x9AA090);
        }
        // ---- "press a key" box
        if (menu == M_BINDS && bindCapture >= 0) {
            long left = bindUntil - System.currentTimeMillis();
            if (left <= 0) bindCapture = -1;
            else {
                int bw = 230, bh = 64, bx = (W - bw) / 2, by = (H - bh) / 2;
                gx.frame(bx, by, bw, bh, "Назначить клавишу");
                gx.textC(gx.f1, Binds.NAME[bindCapture], W / 2, by + 20, Gfx.CHECK);
                gx.textC(gx.f1, "Нажмите нужную клавишу", W / 2, by + 34, 0xFFFFFF);
                gx.textC(gx.f1, pc && pcMouse ? "Esc — отмена" : "Отмена: подождите " + (left / 1000 + 1) + " с", W / 2, by + 48, Gfx.DULL);
            }
        }
    }

    /** show_menu-style text menu (radio commands) on the left side, like the original */
    void drawTextMenu() {
        String[] it = menuItems();
        int hh = 18 + it.length * 13 + 18;
        int y = Math.max(94, H / 2 - hh / 2);
        if (y + hh > H - 30) y = Math.max(20, H - 30 - hh);
        gx.textS(gx.f1, menuTitle(), 10, y, 0xFFFFFF);
        y += 18;
        for (int i = 0; i < it.length; i++) {
            String s = (i < 9 ? (i + 1) + ". " : "") + it[i];
            gx.textS(gx.f1, s, 10, y + i * 13, i == menuSel ? 0xFFE040 : 0xFFFFFF);
        }
        gx.textS(gx.f1, "0. Выход", 10, y + it.length * 13 + 5, 0xFFFFFF);
    }

    /** VGUI1 (orange) panel like the in-game menus of CS 1.6 */
    void vguiPanel(int x, int y, int w, int h, String title) {
        gx.fill(x, y, w, h, 0, 200);
        gx.textS(gx.f2, title, x + 10, y + 6, Gfx.VG);
        gx.fill(x + 10, y + 26, w - 20, 1, Gfx.VG, 256);
    }

    void vguiButton(int x, int y, int w, String s, boolean sel, boolean enabled) {
        int col = enabled ? Gfx.VG : Gfx.VG_DIS;
        if (sel) gx.fill(x, y, w, 15, Gfx.VG, 100);
        gx.rect(x, y, w, 15, col);
        gx.text(gx.f1, s, x + 6, y + 2, sel && enabled ? 0xFFFFFF : col);
    }

    void drawTeamMenu() {
        if (port()) { drawTeamMenuP(); return; }
        int x = 8, y = 18, w = W - 16, h = 186;
        vguiPanel(x, y, w, h, "Выберите команду");
        String[] it = {"1  Террористы", "2  Спецназ", "3  Автовыбор", "0  Отмена"};
        for (int i = 0; i < 4; i++) vguiButton(x + 10, y + 36 + i * 20 + (i == 3 ? 20 : 0), 120, it[i], i == menuSel, true);
        int[] img = null; int[] wh = null;
        if (menuSel == 0) { if (uiTeamT == null) uiTeamT = Sys.s.image("/ui_team_t.png", uiTeamTWH); img = uiTeamT; wh = uiTeamTWH; }
        if (menuSel == 1) { if (uiTeamCT == null) uiTeamCT = Sys.s.image("/ui_team_ct.png", uiTeamCTWH); img = uiTeamCT; wh = uiTeamCTWH; }
        int rx = x + 150, rw = w - 160;
        if (img != null) gx.blitARGB(img, wh[0], wh[1], rx + (rw - wh[0]) / 2, y + 34);
        String d = menuSel == 0 ? "Взорвать точку A или B" : menuSel == 1 ? "Защитить точки A и B"
                : menuSel == 2 ? "Случайная команда" : "";
        gx.text(gx.f1, d, rx + (rw - gx.textW(gx.f1, d)) / 2, y + h - 22, Gfx.VG);
    }

    /** portrait: buttons on top, team picture and description below */
    void drawTeamMenuP() {
        int x = 6, y = 14, w = W - 12, h = H - 30;
        vguiPanel(x, y, w, h, "Выберите команду");
        String[] it = {"1  Террористы", "2  Спецназ", "3  Автовыбор", "0  Отмена"};
        for (int i = 0; i < 4; i++) vguiButton(x + 10, y + 36 + i * 20 + (i == 3 ? 12 : 0), w - 20, it[i], i == menuSel, true);
        int[] img = null; int[] wh = null;
        if (menuSel == 0) { if (uiTeamT == null) uiTeamT = Sys.s.image("/ui_team_t.png", uiTeamTWH); img = uiTeamT; wh = uiTeamTWH; }
        if (menuSel == 1) { if (uiTeamCT == null) uiTeamCT = Sys.s.image("/ui_team_ct.png", uiTeamCTWH); img = uiTeamCT; wh = uiTeamCTWH; }
        int iy = y + 36 + 3 * 20 + 12 + 24;
        if (img != null) gx.blitARGB(img, wh[0], wh[1], x + (w - wh[0]) / 2, iy);
        String d = menuSel == 0 ? "Взорвать точку A или B" : menuSel == 1 ? "Защитить точки A и B"
                : menuSel == 2 ? "Случайная команда" : "";
        gx.textC(gx.f1, d, W / 2, y + h - 22, Gfx.VG);
    }

    /** portrait buy menu: list across the width, picture and info under it */
    void drawBuyMenuP() {
        int x = 4, y = 8, w = W - 8, h = H - 16;
        String title; String[] items; boolean[] en;
        int pick = -1; String picName = null;
        if (buyCat < 0) {
            title = "Купить предмет";
            items = new String[9]; en = new boolean[9];
            for (int i = 0; i < 8; i++) { items[i] = (i + 1) + "  " + Wp.menuTitle[i]; en[i] = true; }
            items[8] = "9  Радио / меню"; en[8] = true;
        } else if (buyCat == 7) {
            title = "Снаряжение";
            String[] e = {"Бронежилет", "Бронежилет + шлем", "Световая граната", "Осколочная граната", "Дымовая граната", "Набор сапёра"};
            int[] pr = {650, 1000, 200, 300, 300, 200};
            items = new String[6]; en = new boolean[6];
            for (int i = 0; i < 6; i++) { items[i] = (i + 1) + "  " + e[i]; en[i] = pr[i] <= me.money && (i != 5 || me.team == 2); }
            if (buySel < 6) { picName = EQUIP_PIC[buySel]; pick = pr[buySel]; }
        } else {
            title = Wp.menuTitle[buyCat];
            int[] list = Wp.menu[buyCat];
            items = new String[list.length]; en = new boolean[list.length];
            for (int i = 0; i < list.length; i++) {
                int wpn = list[i];
                items[i] = (i + 1) + "  " + Wp.title[wpn];
                en[i] = !(Wp.team[wpn] != 0 && Wp.team[wpn] != me.team) && Wp.price[wpn] <= me.money;
            }
            if (buySel < list.length) { int wpn = list[buySel]; picName = Wp.name[wpn].equals("usp") ? "usp" : Wp.name[wpn]; pick = wpn; }
        }
        if (buySel >= items.length) buySel = items.length - 1;
        vguiPanel(x, y, w, h, title);
        for (int i = 0; i < items.length; i++) vguiButton(x + 10, y + 33 + i * 16, w - 20, items[i], i == buySel, en[i]);
        int by = y + h - 20;
        vguiButton(x + 10, by, 96, "0  " + (buyCat < 0 ? "Отмена" : "Назад"), false, true);
        String mny = "У вас: $" + me.money;
        gx.text(gx.f1, mny, x + w - 10 - gx.textW(gx.f1, mny), by + 2, 0x80FF80);
        int iy = y + 33 + items.length * 16 + 4;
        if (picName != null) {
            int[] p = pic(picName);
            if (p != null) gx.blitARGB(p, uiPicWH[0], uiPicWH[1], x + (w - uiPicWH[0]) / 2, iy + (56 - uiPicWH[1]) / 2);
            int ty = iy + 58, lx = x + 10, rx2 = x + w - 10;
            if (buyCat == 7) {
                gx.textC(gx.f1, "Цена: $" + pick, W / 2, ty, Gfx.VG);
            } else if (pick >= 0) {
                String pr = "$" + Wp.price[pick];
                gx.text(gx.f1, Wp.title[pick], lx, ty, 0xFFFFFF);
                gx.text(gx.f1, pr, rx2 - gx.textW(gx.f1, pr), ty, Gfx.VG);
                gx.text(gx.f1, "Магазин: " + Wp.clip[pick], lx, ty + 13, Gfx.VG);
                if (Wp.team[pick] != 0) { String tm = "только " + (Wp.team[pick] == 1 ? "Т" : "CT"); gx.text(gx.f1, tm, rx2 - gx.textW(gx.f1, tm), ty + 13, Gfx.VG); }
            }
        } else if (buyCat < 0) {
            gx.text(gx.f1, "Выберите категорию", x + 10, iy + 4, Gfx.VG);
            gx.text(gx.f1, "6, 7 — купить патроны", x + 10, iy + 18, Gfx.VG_DIS + 0x202000);
        }
    }

    void drawBuyMenu() {
        if (port()) { drawBuyMenuP(); return; }
        int x = 6, y = 12, w = W - 12, h = 206;
        String title; String[] items; boolean[] en;
        int pick = -1; String picName = null;
        if (buyCat < 0) {
            title = "Купить предмет";
            items = new String[9]; en = new boolean[9];
            for (int i = 0; i < 8; i++) { items[i] = (i + 1) + "  " + Wp.menuTitle[i]; en[i] = true; }
            items[8] = "9  Радио / меню"; en[8] = true;
        } else if (buyCat == 7) {
            title = "Снаряжение";
            String[] e = {"Бронежилет", "Бронежилет + шлем", "Световая граната", "Осколочная граната", "Дымовая граната", "Набор сапёра"};
            int[] pr = {650, 1000, 200, 300, 300, 200};
            items = new String[6]; en = new boolean[6];
            for (int i = 0; i < 6; i++) { items[i] = (i + 1) + "  " + e[i]; en[i] = pr[i] <= me.money && (i != 5 || me.team == 2); }
            if (buySel < 6) { picName = EQUIP_PIC[buySel]; pick = pr[buySel]; }
        } else {
            title = Wp.menuTitle[buyCat];
            int[] list = Wp.menu[buyCat];
            items = new String[list.length]; en = new boolean[list.length];
            for (int i = 0; i < list.length; i++) {
                int wpn = list[i];
                items[i] = (i + 1) + "  " + Wp.title[wpn];
                en[i] = !(Wp.team[wpn] != 0 && Wp.team[wpn] != me.team) && Wp.price[wpn] <= me.money;
            }
            if (buySel < list.length) { int wpn = list[buySel]; picName = Wp.name[wpn].equals("usp") ? "usp" : Wp.name[wpn]; pick = wpn; }
        }
        if (buySel >= items.length) buySel = items.length - 1;
        vguiPanel(x, y, w, h, title);
        for (int i = 0; i < items.length; i++) vguiButton(x + 10, y + 33 + i * 16, 136, items[i], i == buySel, en[i]);
        vguiButton(x + 10, y + h - 20, 136, "0  " + (buyCat < 0 ? "Отмена" : "Назад"), false, true);
        // right side: picture + info like the VGUI buy menu
        int rx = x + 156, rw = w - 166;
        if (picName != null) {
            int[] p = pic(picName);
            if (p != null) gx.blitARGB(p, uiPicWH[0], uiPicWH[1], rx + (rw - uiPicWH[0]) / 2, y + 36 + (56 - uiPicWH[1]) / 2);
            int ty = y + 98;
            if (buyCat == 7) {
                gx.text(gx.f1, "Цена: $" + pick, rx, ty, Gfx.VG);
            } else if (pick >= 0) {
                gx.text(gx.f1, Wp.title[pick], rx, ty, 0xFFFFFF);
                gx.text(gx.f1, "Цена: $" + Wp.price[pick], rx, ty + 13, Gfx.VG);
                gx.text(gx.f1, "Магазин: " + Wp.clip[pick], rx, ty + 26, Gfx.VG);
                if (Wp.team[pick] != 0) gx.text(gx.f1, "Только: " + (Wp.team[pick] == 1 ? "Террористы" : "Спецназ"), rx, ty + 39, Gfx.VG);
            }
        } else if (buyCat < 0) {
            gx.text(gx.f1, "Выберите категорию", rx, y + 40, Gfx.VG);
            gx.text(gx.f1, "6, 7 — купить патроны", rx, y + 56, Gfx.VG_DIS + 0x202000);
        }
        gx.text(gx.f1, "У вас: $" + me.money, rx, y + h - 18, 0x80FF80);
    }

    void drawScoreboard() {
        int x = 10, y = 12, w = W - 20, h = H - 30;
        gx.fill(x, y, w, h, 0, 175);
        gx.rect(x, y, w, h, Gfx.VG_DIS);
        gx.textS(gx.f1, (port() ? "" : "Counter-Strike 1.6 · ") + mapName + " · раунд " + round + "/" + maxRounds, x + 6, y + 4, Gfx.VG);
        int cS = x + w - 118, cD = x + w - 78, cL = x + w - 36;
        gx.text(gx.f1, "Счёт", cS, y + 18, Gfx.VG); gx.text(gx.f1, "Смерти", cD - 6, y + 18, Gfx.VG); gx.text(gx.f1, "Пинг", cL, y + 18, Gfx.VG);
        int ry = y + 32;
        for (int t = 1; t <= 2; t++) {
            int tc = t == 1 ? 0xFF5050 : 0x99CCFF;
            int cnt = 0, alive = 0; for (int i = 0; i < pl.length; i++) if (pl[i].team == t) { cnt++; if (pl[i].alive) alive++; }
            gx.text(gx.f1, (t == 2 && port() && Lang.lang == Lang.EN ? "CT" : TNAMES[t - 1]) + "   " + alive + "/" + cnt, x + 6, ry, tc);   // "Counter-Terrorists" does not fit the narrow table
            gx.text(gx.f1, "" + (t == 1 ? scoreT : scoreCT), cS + 6, ry, tc);
            gx.fill(x + 6, ry + 12, w - 12, 1, tc, 160);
            ry += 15;
            for (int i = 0; i < pl.length; i++) {
                Player p = pl[i]; if (p.team != t) continue;
                int c = p.alive ? tc : (t == 1 ? 0x904040 : 0x5A7896);
                if (p == me) gx.fill(x + 4, ry - 1, w - 8, 12, 0xFFFFFF, 30);
                String nm = p.name + (p.c4 && me.team == 1 ? (port() ? " (C4)" : "  [бомба]") : "") + (!p.alive && !port() ? "  убит" : "");
                gx.text(gx.f1, nm, x + 12, ry, c);
                gx.text(gx.f1, "" + p.kills, cS + 6, ry, c);
                gx.text(gx.f1, "" + p.deaths, cD + 6, ry, c);
                gx.text(gx.f1, p.isBot ? "BOT" : p == me ? "Вы" : "—", cL, ry, c);
                ry += 11;
            }
            ry += 6;
        }
    }

    boolean mpLanErr() { return mpErr != null && mpErr.indexOf("LAN") >= 0; }

    void drawSearch() {
        int w = Math.min(280, W - 8), h = 132, x = (W - w) / 2, y = (H - h) / 2;
        gx.frame(x, y, w, h, "Мультиплеер");
        gx.text(gx.f1, MP.MODEN[mpMode] + " · " + MAPS[mpMap], x + 10, y + 20, Gfx.CHECK);
        String st; int col = Gfx.OFFWHITE;
        if (mp == null) { st = mpErr != null ? mpErr : "Отменено"; col = 0xFF8060; }
        else if (!mp.net.connected) st = mp.lanGame ? "Подключение к игре по LAN..." : "Подключение к серверу...";
        else if (mp.viaMqtt && mp.phase == 0) st = mp.create ? "Создаём вашу игру в интернете..." : "Ищем игроков по всему миру (до 10 с)...";
        else if (mp.phase == 0) st = mp.lan != null ? "Создаём игру по LAN..." : mp.lanGame ? "Подключаемся к игре по LAN..." : mp.create ? "Создаём вашу игру на сервере..." : "Поиск игроков по всему миру...";
        else st = "Игра найдена! Загрузка карты...";
        java.util.Vector v = new java.util.Vector(); wrap(st, w - 20, v);
        for (int i = 0; i < v.size() && i < 2; i++) gx.text(gx.f1, (String) v.elementAt(i), x + 10, y + 36 + i * 13, col);
        if (mp != null) {
            int segs = (int) ((System.currentTimeMillis() / 120) % 18) + 1, bw = w - 20;
            gx.fill(x + 10, y + 66, bw, 10, Gfx.FIELD, 256); gx.bevel(x + 10, y + 66, bw, 10, false);
            for (int i = 0; i < segs && 2 + i * 11 + 9 < bw; i++) gx.fill(x + 12 + i * 11, y + 68, 9, 6, Gfx.ORANGE, 256);
            if (mp.online > 0) gx.text(gx.f1, "Онлайн: " + mp.online + ", в комнате: " + mp.roomPlayers, x + 10, y + 82, Gfx.DULL);
        } else if (!mpLanErr()) gx.text(gx.f1, "Сервер: " + (mpServer.length() == 0 ? "авто (бесплатный)" : mpServer), x + 10, y + 66, Gfx.DULL);
        String bt = mp == null ? "Назад" : "Отмена";
        gx.button(x + w - 8 - 70, y + h - 19, 70, 14, bt, true);
    }

    void drawLoading() {
        drawBg();
        int w = 230, h = 58, x = (W - w) / 2, y = (H - h) / 2 + 30;
        gx.frame(x, y, w, h, "Загрузка");
        gx.text(gx.f1, loadMsg, x + 10, y + 18, Gfx.OFFWHITE);
        gx.fill(x + 10, y + 34, w - 20, 12, Gfx.FIELD, 256); gx.bevel(x + 10, y + 34, w - 20, 12, false);
        int segs = (int) ((System.currentTimeMillis() / 120) % 18) + 1;
        for (int i = 0; i < segs; i++) gx.fill(x + 12 + i * 11, y + 36, 9, 8, Gfx.ORANGE, 256);
    }

    void drawMenuScreen() {
        drawBg();
        if (state == ST_END) {
            int win = scoreT > scoreCT ? 1 : scoreCT > scoreT ? 2 : 0;
            drawScoreboard();
            String s = win == 0 ? "Ничья!" : (win == 1 ? "Террористы" : "Спецназ") + " выиграли матч!";
            gx.fill(0, H - 17, W, 17, 0, 200);
            gx.textC(gx.f1, s + "   5 — карта " + MAPS[(mapIdx + 1) % MAPS.length], W / 2, H - 14, Gfx.VG);
            return;
        }
        if (menu == M_SEARCH) { drawMainMenu(menuItemsOf(M_MAIN), false); gx.fill(0, 0, W, H, 0, 120); drawSearch(); return; }
        boolean dlg = isDialog();
        if (menu == M_TEAM) { drawMainMenu(menuItemsOf(M_MAIN), false); gx.fill(0, 0, W, H, 0, 156); drawTeamMenu(); return; }
        drawMainMenu(dlg ? menuItemsOf(M_MAIN) : menuItems(), !dlg);
        if (dlg) { gx.fill(0, 0, W, H, 0, 110); drawDialog(); }
    }

    String[] menuItemsOf(int m2) { int s = menu; menu = m2; String[] r2 = menuItems(); menu = s; return r2; }

    void drawMenuOverlay() {
        if (isOverlay()) { drawTextMenu(); return; }
        if (menu == M_TEAM) { gx.tint(0, 60); drawTeamMenu(); return; }
        if (menu == M_PAUSE) { gx.tint(0, 90); drawMainMenu(menuItems(), true); return; }
        if (isDialog()) { gx.tint(0, 110); drawMainMenu(menuItemsOf(M_PAUSE), false); drawDialog(); return; }
        if (menu == M_HELP) { drawHelp(); return; }
    }

    void drawScoreboardSmall() {
        int y = 92;
        for (int t = 1; t <= 2; t++) {
            int x = t == 1 ? 20 : W / 2 + 10;
            gx.textS(gx.f1, TNAMES[t - 1], x, y, teamCol(t));
            int row = 0;
            for (int i = 0; i < pl.length; i++) if (pl[i].team == t) {
                gx.textS(gx.f1, pl[i].name + "  " + pl[i].kills + "/" + pl[i].deaths, x, y + 14 + row * 13, pl[i] == me ? 0xFFFF80 : 0xE0E0E0); row++;
            }
        }
    }







    void drawHelp() {
        String[] l;
        if (pc && pcMouse) l = new String[]{"W/S/A/D — ходьба, мышь — обзор, ЛКМ — огонь", "ПКМ — прицел / глушитель / удар ножом", "Пробел — прыжок, Ctrl — присесть, R — перезарядка",
                "E — обезвредить бомбу, G — выбросить оружие", "1-5 — оружие, Q — прошлое, колесо — следующее", "B — закупка, Z/X/C — радио, Tab — счёт",
                "Esc — пауза/настройки, F11 — окно/полный экран", "Бомба: 5 → C4, на точке A/B держите ЛКМ 3 с"};
        else if (layout == 1) l = new String[]{"Джойстик ^v — вперёд/назад, <> — поворот", "2/8 — взгляд вверх/вниз, 4/6 — шаг вбок", "5 или центр — огонь, 1 — прыжок, 3 — присесть",
                "7 — перезарядка, 9 — сменить оружие, 0 — обезвредить", "* — прицел / глушитель / удар ножом", "# — закупка (в зоне) или меню: радио, выбросить оружие",
                "Левая софт — пауза/настройки, правая — счёт", "Бомба: 9 → C4, на точке держите 5 три секунды"};
        else l = new String[]{"2/8 — вперёд/назад, 4/6 — шаг влево/вправо", "Джойстик — поворот и взгляд вверх/вниз", "5 или центр — огонь, 1 — прыжок, 3 — присесть",
                "7 — перезарядка, 9 — сменить оружие, 0 — обезвредить", "* — прицел / глушитель / удар ножом", "# — закупка (в зоне) или меню: радио, выбросить оружие",
                "Левая софт — пауза/настройки, правая — счёт", "Бомба: 9 → C4, на точке держите 5 три секунды"};
        java.util.Vector v = new java.util.Vector();
        for (int i = 0; i < l.length; i++) wrap(l[i], W - 26, v);
        int n = v.size(), lh = port() ? 13 : 15, top = port() ? 24 : 48;
        gx.fill(6, top, W - 12, n * lh + 28, 0, 180);
        gx.rect(6, top, W - 12, n * lh + 28, 0xFFA000);
        for (int i = 0; i < n; i++) gx.textS(gx.f1, (String) v.elementAt(i), 12, top + 6 + i * lh, 0xE0E0E0);
        gx.textC(gx.f1, pc ? "Enter — назад" : "5 — назад", W / 2, top + 10 + n * lh, 0xFFA000);
    }

    /** word-wrap s into lines no wider than maxW (continuation lines indented) */
    void wrap(String s, int maxW, java.util.Vector out) {
        s = Lang.t(s);
        String pre = "";
        while (s.length() > 0) {
            if (gx.textW(gx.f1, pre + s) <= maxW) { out.addElement(pre + s); return; }
            int cut = -1;
            for (int i = 1; i < s.length(); i++) if (s.charAt(i) == ' ' && gx.textW(gx.f1, pre + s.substring(0, i)) <= maxW) cut = i;
            if (cut < 0) { out.addElement(pre + s); return; }
            out.addElement(pre + s.substring(0, cut));
            s = s.substring(cut + 1); pre = "   ";
        }
    }
}
