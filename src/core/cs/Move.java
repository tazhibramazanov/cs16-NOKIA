package cs;

/** Player movement: port of the essential parts of GoldSrc pm_shared (CS 1.6 constants). */
public final class Move {
    public static final float GRAVITY = 800, FRICTION = 4, STOPSPEED = 75, ACCEL = 5, AIRACCEL = 10, STEP = 18;
    public static final float JUMP = 268.3283f;
    private final MapData m;
    public int[] ladders = new int[0];

    public Move(MapData md) { m = md; }

    private int hull(Player p) { return p.ducked ? 3 : 1; }

    public boolean testPos(Player p, float x, float y, float z, int hull) {
        m.trace(hull, x, y, z, x, y, z);
        return !m.rStartSolid;
    }

    /** returns landing speed if just landed (for fall damage), else 0 */
    public float move(Player p, float dt, float maxspeed) {
        float landVel = 0;
        boolean wasGround = p.onGround;
        float preVz = p.vz;
        // duck
        if (p.bDuck && !p.ducked) {
            if (p.onGround) { p.oz -= 18; p.ducked = true; }
            else { p.ducked = true; } // in air: tuck legs (origin stays, hull shrinks)
            if (!testPos(p, p.ox, p.oy, p.oz, 3)) { p.oz += p.onGround ? 18 : 0; p.ducked = false; }
        } else if (!p.bDuck && p.ducked) {
            float nz = p.onGround ? p.oz + 18 : p.oz;
            if (testPos(p, p.ox, p.oy, nz, 1)) { p.oz = nz; p.ducked = false; }
        }
        if (p.ducked && p.onGround) maxspeed *= 0.333f;

        // view vectors (flat)
        float cy = (float) Math.cos(Math.toRadians(p.yaw)), sy = (float) Math.sin(Math.toRadians(p.yaw));
        float fwx = cy, fwy = sy, rtx = sy, rty = -cy;

        // ladder
        p.onLadder = false;
        for (int i = 0; i < ladders.length; i++) {
            int md = ladders[i];
            if (p.ox + 17 > m.mMins[md * 3] && p.ox - 17 < m.mMaxs[md * 3] && p.oy + 17 > m.mMins[md * 3 + 1] && p.oy - 17 < m.mMaxs[md * 3 + 1]
                    && p.oz + 37 > m.mMins[md * 3 + 2] && p.oz - 37 < m.mMaxs[md * 3 + 2]) { p.onLadder = true; break; }
        }
        if (p.onLadder) {
            float cp = (float) Math.cos(Math.toRadians(p.pitch)), sp = (float) Math.sin(Math.toRadians(p.pitch));
            float f = p.fmove > 0 ? 200 : p.fmove < 0 ? -200 : 0;
            float s = p.smove > 0 ? 200 : p.smove < 0 ? -200 : 0;
            p.vx = cy * cp * f + rtx * s; p.vy = sy * cp * f + rty * s;
            p.vz = f != 0 ? (-sp * f >= 0 ? 1 : -1) * Math.abs(f) * (Math.abs(sp) > 0.15f ? 1 : 0) : 0;
            if (f != 0 && Math.abs(sp) <= 0.15f) p.vz = f > 0 ? 200 : -200;
            if (p.bJump && !p.jumpHeld) { p.vx = -cy * 270; p.vy = -sy * 270; p.vz = 100; p.onLadder = false; }
            p.jumpHeld = p.bJump;
            flyMove(p, dt);
            categorize(p);
            return 0;
        }

        // friction
        if (p.onGround) friction(p, dt);
        // wish
        float wx = fwx * p.fmove + rtx * p.smove, wy = fwy * p.fmove + rty * p.smove;
        float ws = (float) Math.sqrt(wx * wx + wy * wy);
        if (ws > 0.001f) { wx /= ws; wy /= ws; }
        if (ws > maxspeed) ws = maxspeed;
        if (p.onGround) {
            accelerate(p, wx, wy, ws, ACCEL, dt);
            p.vz = 0;
            // jump
            if (p.bJump && !p.jumpHeld) {
                p.onGround = false;
                p.vz = JUMP;
                if (p.jumpPenalty > 0) {
                    float r = (100 - p.jumpPenalty * 0.001f * 19) * 0.01f;
                    p.vx *= r; p.vy *= r;
                }
                p.jumpPenalty = 1315.789429f;
                Game.g.sound(p, "player_pl_jump1", 60);
            }
        }
        p.jumpHeld = p.bJump;
        if (p.onGround) {
            walkMove(p, dt);
        } else {
            airAccelerate(p, wx, wy, ws, dt);
            p.vz -= GRAVITY * dt * 0.5f;
            flyMove(p, dt);
            p.vz -= GRAVITY * dt * 0.5f;
        }
        if (p.jumpPenalty > 0) { p.jumpPenalty -= dt * 1000; if (p.jumpPenalty < 0) p.jumpPenalty = 0; }
        categorize(p);
        if (!wasGround && p.onGround) { landVel = -preVz; p.vz = 0; }
        return landVel;
    }

    private void friction(Player p, float dt) {
        float sp = (float) Math.sqrt(p.vx * p.vx + p.vy * p.vy);
        if (sp < 0.1f) { p.vx = p.vy = 0; return; }
        float control = sp < STOPSPEED ? STOPSPEED : sp;
        float drop = control * FRICTION * dt;
        float ns = sp - drop; if (ns < 0) ns = 0;
        ns /= sp;
        p.vx *= ns; p.vy *= ns;
    }

    private void accelerate(Player p, float wx, float wy, float ws, float accel, float dt) {
        float cur = p.vx * wx + p.vy * wy;
        float add = ws - cur;
        if (add <= 0) return;
        float as = accel * dt * ws;
        if (as > add) as = add;
        p.vx += as * wx; p.vy += as * wy;
    }

    private void airAccelerate(Player p, float wx, float wy, float ws, float dt) {
        float wsp = ws > 30 ? 30 : ws;
        float cur = p.vx * wx + p.vy * wy;
        float add = wsp - cur;
        if (add <= 0) return;
        float as = AIRACCEL * ws * dt;
        if (as > add) as = add;
        p.vx += as * wx; p.vy += as * wy;
    }

    public void categorize(Player p) {
        if (p.vz > 180) { p.onGround = false; return; }
        int h = hull(p);
        m.trace(h, p.ox, p.oy, p.oz, p.ox, p.oy, p.oz - 2);
        if (m.rFrac < 1 && m.rNz >= 0.7f && !m.rAllSolid) {
            p.onGround = true;
            if (!m.rStartSolid) { p.ox = m.rEx; p.oy = m.rEy; p.oz = m.rEz; }
        } else p.onGround = false;
    }

    private void walkMove(Player p, float dt) {
        float sp = p.vx * p.vx + p.vy * p.vy;
        if (sp < 1) { p.vx = p.vy = 0; return; }
        int h = hull(p);
        float dx = p.ox + p.vx * dt, dy = p.oy + p.vy * dt, dz = p.oz;
        m.trace(h, p.ox, p.oy, p.oz, dx, dy, dz);
        if (m.rFrac == 1) { p.ox = m.rEx; p.oy = m.rEy; p.oz = m.rEz; return; }
        float ox = p.ox, oy = p.oy, oz = p.oz, ovx = p.vx, ovy = p.vy, ovz = p.vz;
        // slide on ground
        flyMove(p, dt);
        float downx = p.ox, downy = p.oy, downz = p.oz, dvx = p.vx, dvy = p.vy, dvz = p.vz;
        // try step up
        p.ox = ox; p.oy = oy; p.oz = oz; p.vx = ovx; p.vy = ovy; p.vz = ovz;
        m.trace(h, p.ox, p.oy, p.oz, p.ox, p.oy, p.oz + STEP);
        if (!m.rStartSolid && !m.rAllSolid) { p.ox = m.rEx; p.oy = m.rEy; p.oz = m.rEz; }
        flyMove(p, dt);
        m.trace(h, p.ox, p.oy, p.oz, p.ox, p.oy, p.oz - STEP);
        if (m.rFrac < 1 && m.rNz < 0.7f) {
            p.ox = downx; p.oy = downy; p.oz = downz; p.vx = dvx; p.vy = dvy; p.vz = dvz; return;
        }
        if (!m.rStartSolid && !m.rAllSolid) { p.ox = m.rEx; p.oy = m.rEy; p.oz = m.rEz; }
        float ddist = (downx - ox) * (downx - ox) + (downy - oy) * (downy - oy);
        float udist = (p.ox - ox) * (p.ox - ox) + (p.oy - oy) * (p.oy - oy);
        if (ddist > udist) { p.ox = downx; p.oy = downy; p.oz = downz; p.vx = dvx; p.vy = dvy; p.vz = dvz; }
        else p.vz = dvz;
    }

    private final float[] pnx = new float[5], pny = new float[5], pnz = new float[5];

    private void flyMove(Player p, float dt) {
        int h = hull(p);
        int np = 0;
        float ovx = p.vx, ovy = p.vy, ovz = p.vz;
        float left = dt;
        for (int bump = 0; bump < 4; bump++) {
            if (p.vx == 0 && p.vy == 0 && p.vz == 0) break;
            float ex = p.ox + p.vx * left, ey = p.oy + p.vy * left, ez = p.oz + p.vz * left;
            m.trace(h, p.ox, p.oy, p.oz, ex, ey, ez);
            if (m.rAllSolid) { p.vx = p.vy = p.vz = 0; return; }
            if (m.rFrac > 0) { p.ox = m.rEx; p.oy = m.rEy; p.oz = m.rEz; np = 0; ovx = p.vx; ovy = p.vy; ovz = p.vz; }
            if (m.rFrac == 1) break;
            left -= left * m.rFrac;
            if (np >= 5) { p.vx = p.vy = p.vz = 0; break; }
            pnx[np] = m.rNx; pny[np] = m.rNy; pnz[np] = m.rNz; np++;
            // clip original velocity to all planes
            int i;
            for (i = 0; i < np; i++) {
                float b = ovx * pnx[i] + ovy * pny[i] + ovz * pnz[i];
                float ob = (pnz[i] > 0.7f) ? 1f : 1f;
                p.vx = ovx - pnx[i] * b * ob; p.vy = ovy - pny[i] * b * ob; p.vz = ovz - pnz[i] * b * ob;
                if (Math.abs(p.vx) < 0.01f) p.vx = 0; if (Math.abs(p.vy) < 0.01f) p.vy = 0; if (Math.abs(p.vz) < 0.01f) p.vz = 0;
                int j;
                for (j = 0; j < np; j++) if (j != i && p.vx * pnx[j] + p.vy * pny[j] + p.vz * pnz[j] < 0) break;
                if (j == np) break;
            }
            if (i == np) {
                if (np != 2) { p.vx = p.vy = p.vz = 0; break; }
                // crease
                float cx = pny[0] * pnz[1] - pnz[0] * pny[1], cyy = pnz[0] * pnx[1] - pnx[0] * pnz[1], cz = pnx[0] * pny[1] - pny[0] * pnx[1];
                float d = cx * p.vx + cyy * p.vy + cz * p.vz;
                p.vx = cx * d; p.vy = cyy * d; p.vz = cz * d;
            }
            if (p.vx * ovx + p.vy * ovy + p.vz * ovz <= 0) { p.vx = p.vy = p.vz = 0; break; }
        }
    }
}
