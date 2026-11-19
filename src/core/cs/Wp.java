package cs;

/** Weapon table (CS 1.6 values, simplified accuracy model). */
public final class Wp {
    public static final int KNIFE = 0, GLOCK = 1, USP = 2, P228 = 3, DEAGLE = 4, ELITE = 5, FIVESEVEN = 6,
            M3 = 7, XM1014 = 8, MAC10 = 9, TMP = 10, MP5 = 11, UMP45 = 12, P90 = 13,
            GALIL = 14, FAMAS = 15, AK47 = 16, M4A1 = 17, SG552 = 18, AUG = 19, SCOUT = 20, AWP = 21, G3SG1 = 22, SG550 = 23,
            M249 = 24, HE = 25, FLASH = 26, SMOKE = 27, C4 = 28, N = 29;
    public static final int S_PRIMARY = 0, S_SECONDARY = 1, S_KNIFE = 2, S_GRENADE = 3, S_C4 = 4;

    public static final String[] name = {"knife", "glock18", "usp", "p228", "deagle", "elite", "fiveseven", "m3", "xm1014", "mac10", "tmp", "mp5", "ump45", "p90",
            "galil", "famas", "ak47", "m4a1", "sg552", "aug", "scout", "awp", "g3sg1", "sg550", "m249", "hegrenade", "flashbang", "smokegrenade", "c4"};
    public static final String[] title = {"Нож", "Glock 18", "USP .45", "SIG P228", "Desert Eagle", "Dual Elites", "Five-SeveN", "Benelli M3", "Benelli XM1014",
            "Ingram MAC-10", "Steyr TMP", "MP5 Navy", "UMP 45", "FN P90", "IMI Galil", "FAMAS", "AK-47", "Colt M4A1", "SG 552", "Steyr AUG", "Schmidt Scout",
            "AWP", "G3/SG-1", "SG 550", "M249", "HE граната", "Флешка", "Дымовая граната", "C4"};
    public static final String[] kill = {"d_knife", "d_glock18", "d_usp", "d_p228", "d_deagle", "d_elite", "d_fiveseven", "d_m3", "d_xm1014", "d_mac10", "d_tmp",
            "d_mp5navy", "d_ump45", "d_p90", "d_galil", "d_famas", "d_ak47", "d_m4a1", "d_sg552", "d_aug", "d_scout", "d_awp", "d_g3sg1", "d_sg550", "d_m249",
            "d_grenade", "d_flashbang", "d_grenade", "d_skull"};
    public static final String[] snd = {"weapons_knife_slash1", "weapons_glock18_2", "weapons_usp_unsil_1", "weapons_p228_1", "weapons_deagle_1", "weapons_elite_fire",
            "weapons_fiveseven_1", "weapons_m3_1", "weapons_xm1014_1", "weapons_mac10_1", "weapons_tmp_1", "weapons_mp5_1", "weapons_ump45_1", "weapons_p90_1",
            "weapons_galil_1", "weapons_famas_1", "weapons_ak47_1", "weapons_m4a1_unsil_1", "weapons_sg552_1", "weapons_aug_1", "weapons_scout_fire_1",
            "weapons_awp1", "weapons_g3sg1_1", "weapons_sg550_1", "weapons_m249_1", "weapons_pinpull", "weapons_pinpull", "weapons_pinpull", "weapons_c4_plant"};
    public static final int[] price = {0, 400, 500, 600, 650, 800, 750, 1700, 3000, 1400, 1250, 1500, 1700, 2350, 2000, 2250, 2500, 3100, 3500, 3500, 2750, 4750, 5000, 4200, 5750, 300, 200, 300, 0};
    public static final int[] slot = {2, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 3, 4};
    /** 0 any, 1 T only, 2 CT only */
    public static final int[] team = {0, 0, 0, 0, 0, 1, 2, 0, 0, 1, 2, 0, 0, 0, 1, 2, 1, 2, 1, 2, 0, 0, 1, 2, 0, 0, 0, 0, 1};
    public static final int[] clip = {0, 20, 12, 13, 7, 30, 20, 8, 7, 30, 30, 30, 25, 50, 35, 25, 30, 30, 30, 30, 10, 10, 20, 30, 100, 1, 1, 1, 1};
    public static final int[] reserve = {0, 120, 100, 52, 35, 120, 100, 32, 32, 100, 120, 120, 100, 100, 90, 90, 90, 90, 90, 90, 90, 30, 90, 90, 200, 0, 0, 0, 0};
    public static final int[] ammoPrice = {0, 40, 50, 50, 80, 40, 50, 65, 65, 50, 40, 40, 50, 50, 60, 60, 80, 60, 60, 60, 80, 125, 80, 60, 60, 0, 0, 0, 0};
    public static final int[] dmg = {15, 25, 34, 32, 54, 36, 20, 20, 20, 29, 20, 26, 30, 21, 30, 30, 36, 32, 33, 32, 75, 115, 80, 70, 32, 0, 0, 0, 0};
    public static final float[] rangeMod = {1, .75f, .79f, .8f, .81f, .75f, .885f, .7f, .7f, .82f, .85f, .84f, .82f, .885f, .98f, .96f, .98f, .97f, .955f, .96f, .98f, .99f, .98f, .98f, .97f, 1, 1, 1, 1};
    public static final float[] cycle = {.4f, .15f, .15f, .2f, .3f, .12f, .15f, .88f, .25f, .07f, .07f, .075f, .105f, .066f, .0875f, .0825f, .0955f, .0875f, .0825f, .0825f, 1.25f, 1.45f, .25f, .25f, .1f, 1, 1, 1, 1};
    public static final float[] reloadT = {0, 2.2f, 2.7f, 2.7f, 2.2f, 4.5f, 2.7f, .45f, .45f, 3.15f, 2.12f, 2.63f, 3.5f, 3.4f, 2.45f, 3.3f, 2.45f, 3.05f, 3f, 3.3f, 2f, 2.5f, 3.5f, 3.35f, 4.7f, 0, 0, 0, 0};
    public static final boolean[] auto = {false, false, false, false, false, false, false, false, true, true, true, true, true, true, true, true, true, true, true, true, false, false, false, false, true, false, false, false, false};
    public static final float[] armorR = {1.7f, .5f, 1f, 1.25f, 1.5f, 1.05f, 1.5f, 1, 1, .95f, 1, 1, 1, 1.5f, 1.55f, 1.4f, 1.55f, 1.4f, 1.4f, 1.4f, 1.7f, 1.95f, 1.65f, 1.45f, 1.5f, 1, 1, 1, 1};
    public static final int[] speed = {250, 250, 250, 250, 250, 250, 250, 230, 240, 250, 250, 250, 250, 245, 240, 240, 221, 230, 235, 240, 260, 210, 210, 210, 220, 250, 250, 250, 250};
    public static final int[] pellets = {1, 1, 1, 1, 1, 1, 1, 9, 6, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    public static final float[] spread = {0, .025f, .02f, .02f, .025f, .03f, .02f, .0675f, .0725f, .03f, .025f, .02f, .025f, .025f, .022f, .02f, .02f, .015f, .018f, .018f, .1f, .12f, .05f, .05f, .03f, 0, 0, 0, 0};
    public static final float[] moveSpread = {0, .03f, .03f, .03f, .05f, .04f, .03f, .02f, .02f, .035f, .03f, .03f, .035f, .035f, .07f, .07f, .08f, .07f, .07f, .07f, .2f, .3f, .15f, .15f, .09f, 0, 0, 0, 0};
    public static final float[] recoilSpread = {0, .012f, .01f, .012f, .03f, .012f, .01f, 0, 0, .006f, .005f, .004f, .005f, .004f, .006f, .006f, .008f, .006f, .006f, .006f, 0, 0, .02f, .02f, .006f, 0, 0, 0, 0};
    public static final float[] punch = {0, 1.2f, 1.6f, 1.5f, 2.6f, 1.4f, 1.2f, 4f, 3f, .8f, .6f, .55f, .7f, .55f, .9f, .85f, 1.05f, .8f, .8f, .75f, 2.2f, 2.8f, 1.8f, 1.8f, .9f, 0, 0, 0, 0};
    /** zoom fov levels (0 = none) */
    public static final int[] zoom1 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 55, 55, 40, 40, 40, 40, 0, 0, 0, 0, 0};
    public static final int[] zoom2 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 15, 10, 15, 15, 0, 0, 0, 0, 0};
    public static final float[] drawT = {.75f, .75f, 1f, .75f, .75f, 1f, .75f, 1f, 1f, 1f, 1f, 1f, 1f, 1f, 1f, 1f, 1f, 1.2f, 1f, 1f, 1.2f, 1.3f, 1.2f, 1.2f, 1.2f, .7f, .7f, .7f, .8f};

    /** CS 1.6 bullet penetration power (units) and max number of walls */
    public static final int[] penPower = {0, 21, 15, 25, 30, 21, 30, 0, 0, 15, 21, 21, 15, 30, 35, 35, 39, 35, 35, 35, 39, 45, 39, 35, 35, 0, 0, 0, 0};
    public static final int[] penCount = {0, 1, 1, 1, 2, 1, 1, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 3, 2, 2, 2, 0, 0, 0, 0};
    /** material (from CS materials.txt): 0 concrete 1 metal 2 vent 3 dirt 4 slosh 5 tile 6 grate 7 wood 8 computer 9 glass */
    public static float matPower(int mat) { return mat == 1 ? .15f : mat == 0 || mat == 3 ? .25f : mat == 6 || mat == 2 ? .5f : mat == 5 ? .65f : mat == 8 ? .4f : 1f; }
    public static float matDamage(int mat) { return mat == 1 ? .2f : mat == 6 ? .4f : mat == 2 || mat == 8 ? .45f : mat == 5 ? .3f : mat == 7 ? .6f : .5f; }

    public static boolean isSniper(int w) { return w == SCOUT || w == AWP || w == G3SG1 || w == SG550; }
    public static boolean isShotgun(int w) { return w == M3 || w == XM1014; }
    public static boolean isGun(int w) { return w >= GLOCK && w <= M249; }
    public static boolean isGrenade(int w) { return w >= HE && w <= SMOKE; }

    // buy menu layout (CS order)
    public static final int[][] menu = {
            {GLOCK, USP, P228, DEAGLE, ELITE, FIVESEVEN},
            {M3, XM1014},
            {MAC10, TMP, MP5, UMP45, P90},
            {GALIL, FAMAS, AK47, M4A1, SG552, AUG, SCOUT, AWP, G3SG1, SG550},
            {M249}};
    public static final String[] menuTitle = {"Пистолеты", "Дробовики", "Пист.-пулемёты", "Винтовки", "Пулемёт", "Патроны осн. (6)", "Патроны пист. (7)", "Снаряжение"};
}
