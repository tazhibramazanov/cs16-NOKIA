package cs;

/** Bot AI: buys, navigates the original CS nav mesh, fights, plants/defuses. */
public final class Bot {
    final Game g; final Player p;
    int[] path; int pi; int goal = -1; float repathAt;
    int task; // 0 go to site, 1 hold, 2 hunt, 3 defuse, 4 guard bomb, 5 go to dropped bomb
    int site; float holdUntil;
    Player enemy; float reactAt, lastSeen, lastSeenX, lastSeenY, lastSeenZ; boolean seen;
    float aimYaw, aimPitch; float errY, errP; float nextPerceive;
    float stuckT, lastX, lastY, jumpUntil, duckUntil;
    float strafe = 1, strafeT; float burstEnd, pauseUntil;
    float lookYaw, lookPitch; float nextLook; boolean hasLook;
    float flashedUntil;
    boolean sniper; boolean bought;
    float heardX, heardY, heardZ, heardAt = -100;

    Bot(Game g, Player p) { this.g = g; this.p = p; sniper = (p.id % 5) == 3; }

    /** every armed bot plays like a beatable human, not a squad of pros (zombies keep their speed) */
    boolean zHuman() { return !p.zombie; }
    float react() { int d = g.difficulty; float r = d == 0 ? 0.65f : d == 1 ? 0.38f : d == 2 ? 0.22f : 0.14f; return zHuman() ? r * 2.4f + 0.3f : r; }
    float turnSpeed() { int d = g.difficulty; float t = d == 0 ? 220 : d == 1 ? 340 : d == 2 ? 520 : 720; return zHuman() ? t * 0.55f : t; }
    float aimErr() { int d = g.difficulty; float e = d == 0 ? 5f : d == 1 ? 3f : d == 2 ? 1.6f : 0.9f; return zHuman() ? e * 2.6f + 2 : e; }

    // radio-driven behaviour
    Player leader; float followUntil, holdHereUntil;

    void onRadio(Player from, int cmd) {
        if (cmd == 4 || cmd == 3 || cmd == 8 || cmd == 0 || cmd == 5 || cmd == 14 || g.mode == 2) return;   // no squads hiding together
        float dx = from.ox - p.ox, dy = from.oy - p.oy;
        boolean near = dx * dx + dy * dy < 1600f * 1600f;
        boolean reply = false;
        switch (cmd) {
            case 4: case 3: case 8: case 0: case 5: case 14: // follow / regroup / stick together / cover me / need help
                if (near || cmd == 5 || cmd == 14) { leader = from; followUntil = g.time + (cmd == 4 ? 35 : 20); holdHereUntil = 0; path = null; reply = true; }
                break;
            case 2: case 9: // hold position / get in position
                holdHereUntil = g.time + 25; leader = null; path = null; reply = true; break;
            case 6: case 10: case 1: // go / storm / take point
                leader = null; holdHereUntil = 0; path = null; task = p.team == 1 ? 0 : 2; repathAt = 0; reply = true; break;
            case 7: // fall back
                leader = null; holdHereUntil = 0; task = 2; path = null;
                { float[] sp = p.team == 1 ? g.spawnT : g.spawnCT; if (sp.length >= 4) go(g.nav.find(sp[0], sp[1], sp[2] - 36)); }
                reply = true; break;
            case 11: g.radioLater(p, 17, 0.6f + g.rf() * 2.5f); return;
            case 13: if (g.time - heardAt > 3) { heardX = from.ox + (float) Math.cos(Math.toRadians(from.yaw)) * 600; heardY = from.oy + (float) Math.sin(Math.toRadians(from.yaw)) * 600; heardZ = from.oz; heardAt = g.time; } break;
        }
        if (reply && from == g.me && g.rf() < 0.45f) g.radioLater(p, g.rf() < 0.85f ? 12 : 19, 0.5f + g.rf());
    }

    void newRound() {
        leader = null; followUntil = 0; holdHereUntil = 0; spotted = false; nextItemCheck = 0;
        path = null; enemy = null; task = 0; bought = false; holdUntil = 0; hasLook = false; flashedUntil = 0; heardAt = -100;
        site = g.siteModel.length > 0 ? (g.rnd.nextInt() & 0xFFFF) % g.siteModel.length : 0;
        if (p.team == 2 && g.siteModel.length > 1) site = (p.id % 2);
        if (p.team == 1) { // terrorists mostly go together
            int teamSite = (g.round * 7 + 3) % Math.max(1, g.siteModel.length);
            if ((g.rnd.nextInt() & 3) != 0) site = teamSite;
        }
        nextPerceive = g.time + g.rf() * 0.3f;
    }

    void buy() {
        bought = true;
        Player q = p;
        int m = q.money;
        if (q.team == 1) {
            if (sniper && m >= 5750) g.buyWeapon(q, Wp.AWP);
            else if (m >= 3500) g.buyWeapon(q, Wp.AK47);
            else if (m >= 2700 && q.primary < 0) g.buyWeapon(q, Wp.AK47);
            else if (m >= 2000 && q.primary < 0 && g.round > 1) g.buyWeapon(q, Wp.GALIL);
            else if (m >= 1400 && q.primary < 0 && g.round > 2 && (g.rnd.nextInt() & 1) == 0) g.buyWeapon(q, Wp.MAC10);
        } else {
            if (sniper && m >= 5750) g.buyWeapon(q, Wp.AWP);
            else if (m >= 4100) g.buyWeapon(q, Wp.M4A1);
            else if (m >= 3100 && q.primary < 0) g.buyWeapon(q, Wp.M4A1);
            else if (m >= 2250 && q.primary < 0 && g.round > 1) g.buyWeapon(q, Wp.FAMAS);
            else if (m >= 1500 && q.primary < 0 && g.round > 2 && (g.rnd.nextInt() & 1) == 0) g.buyWeapon(q, Wp.MP5);
        }
        if (q.primary < 0 && q.money >= 650 && g.round == 1 && (g.rnd.nextInt() & 1) == 0 && q.secondary != Wp.DEAGLE) { if (q.money >= 650) g.buyWeapon(q, Wp.DEAGLE); }
        if (q.money >= 1000) g.buyEquip(q, 1); else if (q.money >= 650) g.buyEquip(q, 0);
        if (q.team == 2 && q.money >= 200 && q.primary >= 0) g.buyEquip(q, 5);
        if (q.money >= 300 && (g.rnd.nextInt() & 1) == 0) g.buyEquip(q, 3);
        if (q.money >= 200 && (g.rnd.nextInt() & 3) == 0) g.buyEquip(q, 2);
        if (q.money >= 300 && (g.rnd.nextInt() & 3) == 0) g.buyEquip(q, 4);
        if (q.primary == Wp.M4A1 && (q.id & 1) == 0) q.silM4 = true;
        if (q.primary >= 0) g.switchTo(q, q.primary);
        else if (q.secondary >= 0) g.switchTo(q, q.secondary);
    }

    void heard(Player src) {
        float dx = src.ox - p.ox, dy = src.oy - p.oy;
        if (dx * dx + dy * dy < 1600 * 1600) { heardX = src.ox; heardY = src.oy; heardZ = src.oz; heardAt = g.time; }
    }

    void hurtBy(Player a) {
        if (a.team == p.team) return;
        heardX = a.ox; heardY = a.oy; heardZ = a.oz; heardAt = g.time;
        if (enemy == null) { lookYaw = Game.atan2(a.oy - p.oy, a.ox - p.ox); hasLook = true; nextLook = g.time + 1.5f; }
    }

    void flashed(float d) { flashedUntil = g.time + d * 0.8f; }

    void bombPlanted() {
        path = null;
        task = p.team == 2 ? 3 : 4;
    }

    int goalForSite(int s) {
        if (g.siteModel.length == 0) return g.nav.find(0, 0, 0);
        int md = g.siteModel[s];
        float x = g.mcx(md) + g.rs() * (g.m.mMaxs[md * 3] - g.m.mMins[md * 3]) * 0.3f;
        float y = g.mcy(md) + g.rs() * (g.m.mMaxs[md * 3 + 1] - g.m.mMins[md * 3 + 1]) * 0.3f;
        return g.nav.find(x, y, g.m.mMins[md * 3 + 2]);
    }

    int myArea() { return g.nav.find(p.ox, p.oy, p.oz - (p.ducked ? 18 : 36)); }

    boolean go(int area) {
        int a = myArea();
        path = g.nav.path(a, area, null);
        pi = 0; goal = area;
        repathAt = g.time + 6;
        return path != null;
    }

    void think(float dt) {
        Nav nav = g.nav;
        if (g.time < g.freezeEnd) {
            if (!bought && g.time > g.roundStart + 0.5f + (p.id % 5) * 0.2f) buy();
            p.fmove = p.smove = 0; p.bFire = false;
            return;
        }
        if (!bought) buy();
        p.bFire = false; p.bFire2 = false; p.bUse = false; p.bJump = false;
        boolean blind = g.time < flashedUntil;
        // ---------------- perception
        if (g.time >= nextPerceive) {
            nextPerceive = g.time + 0.12f + g.rf() * 0.08f;
            Player best = null; float bd = 1e9f;
            p.fwd(fw);
            for (int i = 0; i < g.pl.length; i++) {
                Player q = g.pl[i];
                if (!q.alive || q.team == p.team) continue;
                if (q == ignore && g.time < ignoreUntil) continue;
                float dx = q.ox - p.ox, dy = q.oy - p.oy, dz = q.oz - p.oz;
                float d = dx * dx + dy * dy + dz * dz;
                if (d > 3500f * 3500f) continue;
                float dl = (float) Math.sqrt(d);
                float dot = (dx * fw[0] + dy * fw[1] + dz * fw[2]) / Math.max(1, dl);
                boolean recentlyHurt = q.id == p.lastAttacker && g.time - p.lastHurt < 1.5f;
                if (dot < 0.25f && !recentlyHurt && dl > 250) continue;
                if (blind) continue;
                if (!g.visible(p, q)) continue;
                if (d < bd) { bd = d; best = q; }
            }
            if (best != null) {
                if (enemy != best) { engageStart = g.time; if (!spotted && g.rf() < 0.35f) { spotted = true; g.radioLater(p, 13, 0.3f); } }
                if (enemy != best || !seen) {
                    float rt = react();
                    if (enemy == best && g.time - lastSeen < 1) rt *= 0.3f;
                    reactAt = g.time + rt * (0.8f + g.rf() * 0.5f);
                    errY = g.rs() * aimErr() * 3; errP = g.rs() * aimErr() * 2;
                }
                enemy = best; seen = true; lastSeen = g.time; lastSeenX = best.ox; lastSeenY = best.oy; lastSeenZ = best.oz;
            } else {
                seen = false;
                if (enemy != null && (!enemy.alive || g.time - lastSeen > 3)) enemy = null;
            }
        }
        if (enemy != null && !enemy.alive) { enemy = null; seen = false; }
        // ---------------- tasks / navigation goal
        if (g.bombPlanted && p.team == 2) task = 3;
        if (g.bombPlanted && p.team == 1 && task != 4) { task = 4; path = null; }
        if (g.bombDropped && p.team == 1 && !g.bombPlanted && task != 5) { task = 5; path = null; }
        if (!g.bombDropped && task == 5) { task = 0; path = null; }
        // reload when safe
        int w = p.cur;
        if (!seen && Wp.isGun(w) && p.clip[w] < Wp.clip[w] * 0.4f && p.ammo[w] > 0 && !p.reloading) g.startReload(p);
        // weapon choice
        if (!p.planting && p.cur != Wp.C4) {
            if (p.primary >= 0 && (p.clip[p.primary] > 0 || p.ammo[p.primary] > 0) && p.cur != p.primary && !p.reloading) g.switchTo(p, p.primary);
            else if ((p.primary < 0 || (p.clip[p.primary] == 0 && p.ammo[p.primary] == 0)) && p.secondary >= 0 && p.cur != p.secondary && (p.clip[p.secondary] > 0 || p.ammo[p.secondary] > 0)) g.switchTo(p, p.secondary);
        }
        float moveYaw = p.yaw; boolean moving = false;
        // bomb actions override movement
        boolean planting = p.c4 && g.inSite(p) && !g.bombPlanted && !seen;
        boolean defusing = false;
        if (task == 3 && g.bombPlanted && !seen) {
            float dx = g.bombX - p.ox, dy = g.bombY - p.oy;
            defusing = dx * dx + dy * dy < 48 * 48;
        }
        if (seen && enemy != null && g.time - engageStart > 7 && g.time - p.lastHurt > 4 && enemy.lastAttacker != p.id) {
            ignore = enemy; ignoreUntil = g.time + 4; enemy = null; seen = false;
        }
        boolean fighting = enemy != null && (seen || g.time - lastSeen < 1.2f);
        // ---------------- navigation (always computed, used unless fighting dictates otherwise)
        if (!planting && !defusing) {
            float tx = 0, ty = 0;
            boolean arrived = false;
            if (path == null || pi >= path.length) {
                arrived = path != null;
                path = null;
                if (g.time > repathAt - 5.5f || arrived) chooseGoal(arrived);
            } else if (leader != null && g.time < followUntil && g.time > repathAt || g.mode != 0 && g.time > repathAt) chooseGoal(false);
            if (path != null && pi < path.length) {
                int cur = path[pi];
                if (pi + 1 < path.length) {
                    int nx = path[pi + 1];
                    nav.portal(cur, nx, p.ox, p.oy);
                    tx = nav.px; ty = nav.py;
                    float dx = tx - p.ox, dy = ty - p.oy;
                    if (dx * dx + dy * dy < 22 * 22 || inArea(nx)) {
                        pi++;
                        if (nav.cz[nx] - (p.oz - (p.ducked ? 18 : 36)) > 22 && p.onGround) { p.bJump = true; }
                        if ((nav.attr[nx] & 1) != 0) duckUntil = g.time + 1.2f;
                        if ((nav.attr[nx] & 2) != 0 && p.onGround) { p.bJump = true; }
                    }
                } else {
                    tx = nav.cx[cur]; ty = nav.cy[cur];
                    float dx = tx - p.ox, dy = ty - p.oy;
                    if (dx * dx + dy * dy < 24 * 24) pi++;
                }
                moveYaw = Game.atan2(ty - p.oy, tx - p.ox);
                moving = true;
            }
        }
        // ---------------- aiming & fighting
        float wantYaw, wantPitch;
        boolean wantMove = moving;
        if (fighting) {
            float tx = seen ? enemy.ox : lastSeenX, ty = seen ? enemy.oy : lastSeenY;
            float tzBase = seen ? enemy.oz : lastSeenZ;
            boolean head = g.difficulty >= 2 || (g.difficulty == 1 && (p.id & 1) == 0);
            float tz = tzBase + (enemy.ducked ? (head ? 9 : 2) : (head ? 25 : 12));
            if (seen) { tx += enemy.vx * 0.05f; ty += enemy.vy * 0.05f; }
            float dx = tx - p.ox, dy = ty - p.oy, dz = tz - p.eyeZ();
            float hd = (float) Math.sqrt(dx * dx + dy * dy);
            wantYaw = Game.atan2(dy, dx) + errY; wantPitch = -Game.atan2(dz, hd) + errP;
            errY *= (1 - dt * 2.2f); errP *= (1 - dt * 2.2f);
            float dist = (float) Math.sqrt(dx * dx + dy * dy + dz * dz);
            int cw = p.cur;
            float range = W_range(cw);
            float ae = Math.abs(Game.angNorm(wantYaw - p.yaw)) + Math.abs(wantPitch - p.pitch);
            float tol = Math.max(1.2f, 40f / Math.max(50, dist) * 57.3f * 0.35f);
            if (seen && g.time >= reactAt && ae < tol && !blind && dist < range * 1.3f) {
                if (Wp.isSniper(cw) && p.zoom == 0 && dist > 300) { g.altFire(p); }
                else if (Wp.auto[cw]) {
                    if (dist > 900) {
                        if (g.time > pauseUntil) { p.bFire = true; if (burstEnd == 0) burstEnd = g.time + 0.25f; if (g.time > burstEnd) { pauseUntil = g.time + 0.35f; burstEnd = 0; } }
                    } else if (dist > 450) {
                        if (g.time > pauseUntil) { p.bFire = true; if (burstEnd == 0) burstEnd = g.time + 0.5f; if (g.time > burstEnd) { pauseUntil = g.time + 0.25f; burstEnd = 0; } }
                    } else p.bFire = true;
                } else p.bFire = !p.firePrev;
            }
            if (cw == Wp.KNIFE && dist < 70) p.bFire = true;
            if (seen) {
                boolean stand = (Wp.isSniper(cw) || (dist > 600 && dist < range)) && !(cw == Wp.KNIFE);
                if (stand) { wantMove = false; p.fmove = 0; p.smove = 0; }
                else if (dist < 600 || cw == Wp.KNIFE) {
                    strafeT -= dt;
                    if (strafeT <= 0) { strafeT = 0.4f + g.rf() * 0.7f; strafe = g.rf() < 0.5f ? -1 : 1; if (g.rf() < 0.25f) strafe = 0; }
                    p.fmove = dist < 150 ? -150 : 0;
                    p.smove = strafe * 250;
                    if (cw == Wp.KNIFE) { p.fmove = 400; p.smove = 0; }
                    if (p.bFire && Wp.auto[cw] && dist > 300) p.smove = 0;
                    wantMove = false;
                }
                // else: too far for this weapon -> keep walking the path while aiming
                if (dist > 900 && p.bFire && g.difficulty >= 1 && !Wp.isSniper(cw) && stand) p.bDuck = true; else if (g.time > duckUntil) p.bDuck = false;
            } else {
                if (!moving || hd < 600) { moveYaw = Game.atan2(lastSeenY - p.oy, lastSeenX - p.ox); wantMove = true; }
                p.bDuck = false;
            }
        } else {
            if (p.zoom > 0) p.zoom = 0;
            p.bDuck = g.time < duckUntil;
            if (g.time - heardAt < 2.5f) { wantYaw = Game.atan2(heardY - p.oy, heardX - p.ox); wantPitch = 0; }
            else if (hasLook && g.time < nextLook) { wantYaw = lookYaw; wantPitch = lookPitch; }
            else { wantYaw = moving ? moveYaw : p.yaw; wantPitch = 0; hasLook = false; }
            if (!moving && task == 1 && g.time > nextLook) { lookYaw = holdYaw + g.rs() * 50; lookPitch = g.rs() * 6; nextLook = g.time + 1.5f + g.rf() * 2.5f; hasLook = true; }
        }
        if (wantMove) {
            float rel = (float) Math.toRadians(Game.angNorm(moveYaw - p.yaw));
            p.fmove = (float) Math.cos(rel) * 400; p.smove = -(float) Math.sin(rel) * 400;
            // stuck detection
            float mdx = p.ox - lastX, mdy = p.oy - lastY;
            if (mdx * mdx + mdy * mdy < 12 * dt * dt * 400) {
                stuckT += dt;
                if (stuckT > 0.7f && p.onGround && g.time > nextUnstuck) { p.bJump = true; nextUnstuck = g.time + 0.8f; strafe = -strafe; }
                if (stuckT > 1.0f) p.smove += strafe * 300;
                if (stuckT > 2.2f) { path = null; stuckT = 0; repathAt = g.time; }
            } else stuckT = Math.max(0, stuckT - dt);
        } else if (!fighting) { p.fmove = 0; p.smove = 0; stuckT = 0; }
        lastX = p.ox; lastY = p.oy;
        if (planting) {
            if (p.cur != Wp.C4) g.switchTo(p, Wp.C4);
            p.bFire = true; p.fmove = p.smove = 0; p.bDuck = true; p.bJump = false;
        }
        if (defusing) { p.bUse = true; p.fmove = p.smove = 0; p.bDuck = true; p.bJump = false; wantPitch = 50; }
        // turn toward wanted
        float ts = turnSpeed() * dt;
        float dyaw = Game.angNorm(wantYaw - p.yaw);
        if (dyaw > ts) dyaw = ts; if (dyaw < -ts) dyaw = -ts;
        p.yaw = Game.angNorm(p.yaw + dyaw);
        float dp = wantPitch - p.pitch;
        if (dp > ts) dp = ts; if (dp < -ts) dp = -ts;
        p.pitch += dp;
        if (p.pitch > 89) p.pitch = 89; if (p.pitch < -89) p.pitch = -89;
        if (blind) { p.bFire = g.rf() < 0.1f && enemy != null; p.yaw += g.rs() * 90 * dt; }
    }

    private final float[] fw = new float[3];
    float engageStart, nextUnstuck, ignoreUntil; Player ignore;

    static float W_range(int w) {
        if (Wp.isSniper(w)) return 5000;
        if (w >= Wp.GALIL && w <= Wp.AUG || w == Wp.M249) return 2600;
        if (Wp.isShotgun(w)) return 700;
        if (w >= Wp.MAC10 && w <= Wp.P90) return 1500;
        if (w == Wp.DEAGLE) return 1600;
        if (w == Wp.KNIFE) return 80;
        return 1100;
    }
    float holdYaw;

    boolean inArea(int a) {
        Nav n = g.nav;
        return p.ox >= n.x0[a] + 6 && p.ox <= n.x1[a] - 6 && p.oy >= n.y0[a] + 6 && p.oy <= n.y1[a] - 6 && Math.abs(n.zAt(a, p.ox, p.oy) - (p.oz - (p.ducked ? 18 : 36))) < 40;
    }

    void moveTowards(float x, float y) {
        float yw = Game.atan2(y - p.oy, x - p.ox);
        float rel = (float) Math.toRadians(Game.angNorm(yw - p.yaw));
        p.fmove = (float) Math.cos(rel) * 400; p.smove = -(float) Math.sin(rel) * 400;
    }

    boolean spotted; float nextItemCheck;

    boolean wanderGoal() {
        Nav nav = g.nav;
        for (int k = 0; k < 6 && nav.n > 0; k++) {
            int a = (int) (g.rf() * nav.n) % nav.n;
            if (go(a)) { repathAt = g.time + 10 + g.rf() * 12; return true; }
        }
        return false;
    }

    void chooseGoal(boolean arrived) {
        Nav nav = g.nav;
        int target = -1;
        if (g.mode == 2) {   // zombie mode
            leader = null; holdHereUntil = 0;
            if (p.zombie) {   // chase the nearest human
                Player t = null; float bd = 1e12f;
                for (int i = 0; i < g.pl.length; i++) {
                    Player q = g.pl[i]; if (!q.alive || q.zombie) continue;
                    float dx = q.ox - p.ox, dy = q.oy - p.oy, d = dx * dx + dy * dy;
                    if (d < bd) { bd = d; t = q; }
                }
                if (t != null && go(nav.find(t.ox, t.oy, t.oz - 36))) { repathAt = g.time + 1.5f + g.rf(); return; }
            } else {          // humans wander around the map instead of camping in one corner
                if (wanderGoal()) return;
            }
        }
        if (g.mode == 1) {   // deathmatch: roam the map, no bomb-site camping
            leader = null; holdHereUntil = 0;
            if (wanderGoal()) return;
        }
        if (leader != null && leader.alive && g.time < followUntil && task != 3 && !(p.c4 && !g.bombPlanted && false)) {
            float dx = leader.ox - p.ox, dy = leader.oy - p.oy;
            if (dx * dx + dy * dy < 140 * 140) { path = null; repathAt = g.time + 1; holdYaw = leader.yaw; return; }
            if (go(nav.find(leader.ox, leader.oy, leader.oz - 36))) { repathAt = g.time + 2; return; }
        }
        if (g.time < holdHereUntil && task != 3) { path = null; repathAt = g.time + 2; holdYaw = p.yaw; task = 1; holdUntil = holdHereUntil; return; }
        if (p.primary < 0 && g.time > nextItemCheck) {
            nextItemCheck = g.time + 2;
            int best = -1; float bd = 900 * 900;
            for (int i = 0; i < Game.MAXD; i++) if (g.dOn[i] && Wp.slot[g.dW[i]] == Wp.S_PRIMARY) {
                float dx = g.dX[i] - p.ox, dy = g.dY[i] - p.oy; float d = dx * dx + dy * dy;
                if (d < bd) { bd = d; best = i; }
            }
            if (best >= 0 && go(nav.find(g.dX[best], g.dY[best], g.dZ[best]))) { repathAt = g.time + 3; return; }
        }
        switch (task) {
            case 0:
                if (arrived && (p.team == 2 || !p.c4)) { task = 1; holdUntil = g.time + 5 + g.rf() * 8; }
                target = goalForSite(site);
                if (task == 1) target = holdSpot();
                break;
            case 1:
                if (g.time > holdUntil) {
                    task = 2;   // stop camping: go looking for the enemy
                } else if (!arrived) target = holdSpot();
                else { path = null; repathAt = g.time + 3; holdYaw = p.yaw + 180; return; }
                if (task == 2) target = huntTarget();
                break;
            case 2: target = huntTarget(); break;
            case 3: target = nav.find(g.bombX, g.bombY, g.bombZ); break;
            case 4: {
                if (arrived) { holdYaw = Game.atan2(p.oy - g.bombY, p.ox - g.bombX); path = null; repathAt = g.time + 8; return; }
                float a = g.rf() * 6.283f, d = 150 + g.rf() * 350;
                target = nav.find(g.bombX + (float) Math.cos(a) * d, g.bombY + (float) Math.sin(a) * d, g.bombZ);
                break;
            }
            case 5: target = nav.find(g.bombX, g.bombY, g.bombZ); break;
        }
        if (target < 0) target = (g.rnd.nextInt() & 0xFFFF) % nav.n;
        if (!go(target)) { path = null; repathAt = g.time + 1; }
    }

    int holdSpot() {
        // a hiding spot near the site
        Nav nav = g.nav;
        int md = g.siteModel.length > 0 ? g.siteModel[site] : -1;
        float cx = md >= 0 ? g.mcx(md) : p.ox, cy = md >= 0 ? g.mcy(md) : p.oy;
        for (int tries = 0; tries < 30; tries++) {
            int a = (g.rnd.nextInt() & 0xFFFF) % nav.n;
            float dx = nav.cx[a] - cx, dy = nav.cy[a] - cy;
            if (dx * dx + dy * dy < 700 * 700 && nav.spots[a].length > 0) { holdYaw = Game.atan2(dy, dx); return a; }
        }
        return goalForSite(site);
    }

    int huntTarget() {
        // go toward the enemy spawn / last heard position
        if (g.time - heardAt < 6) return g.nav.find(heardX, heardY, heardZ - 36);
        float[] sp = p.team == 1 ? g.spawnCT : g.spawnT;
        if (sp.length >= 4) { int k = ((g.rnd.nextInt() & 0xFFFF) % (sp.length / 4)) * 4; return g.nav.find(sp[k], sp[k + 1], sp[k + 2] - 36); }
        return -1;
    }
}
