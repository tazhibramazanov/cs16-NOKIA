package cs;

public final class Player {
    public int id;
    public String name;
    public boolean isBot;
    public int team; // 1 = T, 2 = CT
    public boolean alive;
    public float ox, oy, oz, vx, vy, vz;
    public float yaw, pitch;
    public boolean onGround, ducked, onLadder, jumpHeld;
    public float jumpPenalty;
    public float punchP, punchY;
    public int health, armor; public boolean helmet, defuser;
    public int money = 800, kills, deaths;
    public int cur = Wp.KNIFE, last = Wp.KNIFE;
    public int primary = -1, secondary = -1, he, flash, smoke; public boolean c4;
    public final int[] clip = new int[Wp.N], ammo = new int[Wp.N];
    public float nextAttack, reloadEnd, drawEnd, shellNext; public boolean reloading; public int shotsFired; public float recoil;
    public int zoom; // 0,1,2
    // input (filled by human input or bot)
    public float fmove, smove; public boolean bJump, bDuck, bFire, bFire2, bUse;
    public boolean firePrev, fire2Prev, usePrev;
    // view model animation
    public int vmAnim; public float vmTime; public float vmLen;
    // sprite animation
    public float animTime, deathTime; public int deathAnim;
    public float lastHurt; public float lastHurtYaw;
    public float stepTime; public float flashUntil, flashStart;
    public float plantProgress, defuseProgress; public boolean planting, defusing;
    public Bot ai;
    public boolean silM4, silUSP; public int vmSet; public String vmName = "idle"; public float lastRadio = -10, lastShot = -10; public boolean lastShotSil;
    public int lastAttacker = -1;
    // multiplayer / modes
    public int netId, spawnSeq, ztype; public boolean remote, zombie; public float tx, ty, tz, tyaw;
    public float speedFactor() { return (float) Math.sqrt(vx * vx + vy * vy); }

    public void fwd(float[] out) {
        float cy = (float) Math.cos(Math.toRadians(yaw)), sy = (float) Math.sin(Math.toRadians(yaw));
        float cp = (float) Math.cos(Math.toRadians(pitch)), sp = (float) Math.sin(Math.toRadians(pitch));
        out[0] = cp * cy; out[1] = cp * sy; out[2] = -sp;
    }

    public float eyeZ() { return oz + (ducked ? 12 : 17); }

    public boolean hasWeapon(int w) {
        if (w == Wp.KNIFE) return true;
        if (w == primary || w == secondary) return true;
        if (w == Wp.HE) return he > 0;
        if (w == Wp.FLASH) return flash > 0;
        if (w == Wp.SMOKE) return smoke > 0;
        if (w == Wp.C4) return c4;
        return false;
    }

    public void resetForRound() {
        alive = true; health = 100; vx = vy = vz = 0; punchP = punchY = 0; ducked = false; onLadder = false;
        reloading = false; zoom = 0; shotsFired = 0; recoil = 0; nextAttack = 0; drawEnd = 0;
        planting = defusing = false; plantProgress = defuseProgress = 0; flashUntil = 0; jumpPenalty = 0;
        fmove = smove = 0; bJump = bDuck = bFire = bFire2 = bUse = false;
        lastAttacker = -1;
    }
}
