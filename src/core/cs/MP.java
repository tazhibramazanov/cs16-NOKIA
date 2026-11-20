package cs;

/**
 * Online match glue. The first player of a server room is the host: his Game runs bots, rounds and damage
 * and sends snapshots; the others move their own player locally, send it to the host and mirror the rest.
 */
public final class MP {
    public static final String[] MODEN = {"Оригинал", "Дезматч", "Зомби"};
    public static final int PORT = 27016;
    final Game g;
    public final Net net;
    public boolean host;
    public int myId, roomPlayers, online;
    public final int mode, map;
    final String name;
    public int phase;          // 0 connecting, 1 welcome received (loading), 2 playing, 3 failed
    public String err;
    long searchStart;
    float sendAcc, snapAcc;
    boolean rosterDirty, applying, rosterOk;
    byte[] pendingRoster;
    final Net.Buf b = new Net.Buf(), e = new Net.Buf();
    int lastRound = -1;
    float plantShown;

    public final boolean create;   // "Создать свою игру": always a new room
    public LanServer lan;          // LAN host: the server runs inside this game
    public boolean lanGame;
    /** worldwide through a public MQTT broker (MqttLink pretends to be our server) */
    public boolean viaMqtt;
    public MP(Game g, Conn c, int mode, int map, String name, boolean create) {
        this.g = g; this.mode = mode; this.map = map; this.name = name; this.create = create; viaMqtt = true;
        net = new Net(c);
        searchStart = Sys.s.millis();
    }
    /** LAN host: talk to our own in-game server */
    public MP(Game g, LanServer lan, Conn c, int mode, int map, String name) {
        this.g = g; this.mode = mode; this.map = map; this.name = name; this.create = true; this.lan = lan; lanGame = true;
        net = new Net(c);
        searchStart = Sys.s.millis();
    }
    public MP(Game g, String server, int mode, int map, String name, boolean create) {
        this.g = g; this.mode = mode; this.map = map; this.name = name; this.create = create;
        String h = server.trim(); int port = PORT;
        int c = h.lastIndexOf(':');
        if (c > 0) { try { port = Integer.parseInt(h.substring(c + 1).trim()); } catch (Exception ex) { } h = h.substring(0, c).trim(); }
        net = new Net(h, port);
        searchStart = Sys.s.millis();
    }

    public void close() { net.close(); if (lan != null) lan.close(); }

    // ------------------------------------------------------------------ per tick
    public void update(float dt) {
        if (phase == 3) return;
        if (net.error != null) { fail(net.error); return; }
        if (phase == 0 && net.connected && !joinSent) {
            joinSent = true;
            b.reset().u8(Net.PROTO).u8(mode).u8(map).str(name).u8(create ? 1 : 0);
            net.send(Net.C_JOIN, b);
        }
        if (phase == 0 && !net.connected && Sys.s.millis() - searchStart > 20000) { fail("Сервер не отвечает"); return; }
        byte[] m;
        int guard = 0;
        while ((m = net.poll()) != null && guard++ < 200) handle(m);
        net.keepAlive();
        if (phase != 2 || g.state != Game.ST_PLAY) return;
        if (host) {
            if (rosterDirty) { sendRoster(-1); rosterDirty = false; }
            snapAcc += dt;
            if (snapAcc >= 0.1f && roomPlayers > 1) { snapAcc = 0; sendSnapshot(); }
        } else {
            sendAcc += dt;
            if (sendAcc >= 1 / 15f) { sendAcc = 0; sendState(); }
            interp(dt);
        }
    }
    boolean joinSent;

    void fail(String s) {
        err = s; phase = 3; close();
        g.mpFailed(s);
    }

    /** the map finished loading: host announces itself, client asks for the roster */
    public void onLoaded() {
        phase = 2;
        if (host) { g.me.netId = myId; g.me.name = name; rosterDirty = true; }
        else {
            for (int i = 0; i < g.pl.length; i++) { g.pl[i].ai = null; g.pl[i].isBot = true; }
            if (pendingRoster != null) applyRoster(new Net.Rd(pendingRoster, 2));   // [0] frame type, [1] 'I'
            toHost(b.reset().u8('Q'));
        }
    }

    // ------------------------------------------------------------------ incoming
    void handle(byte[] m) {
        Net.Rd r = new Net.Rd(m, 1);
        switch (m[0]) {
            case Net.S_WELCOME:
                myId = r.u16(); host = r.u8() != 0; r.u16(); roomPlayers = r.u8();
                phase = 1;
                g.mpStart(this);
                break;
            case Net.S_ROOM: roomPlayers = r.u8(); online = r.u16(); break;
            case Net.S_ERROR: fail(r.str()); break;
            case Net.S_HOST: becomeHost(); break;
            case Net.S_PEERJOIN: { int id = r.u16(); String nm = r.str(); peerJoin(id, nm); break; }
            case Net.S_PEERLEFT: peerLeft(r.u16()); break;
            case Net.S_FROMCLIENT: { int id = r.u16(); if (host && phase == 2) fromClient(id, r); break; }
            case Net.S_FROMHOST: if (!host) fromHost(m, r); break;
        }
    }

    // ------------------------------------------------------------------ host side
    Player byNet(int id) { if (g.pl == null) return null; for (int i = 0; i < g.pl.length; i++) if (g.pl[i].netId == id && g.pl[i].remote) return g.pl[i]; return null; }

    void peerJoin(int id, String nm) {
        if (phase != 2 || g.pl == null) { pendingJoins.addElement(new Object[]{new Integer(id), nm}); return; }
        // take the place of a bot in the team with fewer people
        int[] humans = new int[3];
        for (int i = 0; i < g.pl.length; i++) if (!g.pl[i].isBot) humans[g.pl[i].team]++;
        int want = mode == 2 ? 0 : humans[1] <= humans[2] ? 1 : 2;
        Player pick = null;
        for (int i = 0; i < g.pl.length; i++) { Player p = g.pl[i]; if (p.isBot && (want == 0 || p.team == want)) { pick = p; break; } }
        if (pick == null) for (int i = 0; i < g.pl.length; i++) if (g.pl[i].isBot) { pick = g.pl[i]; break; }
        if (pick == null) return;
        pick.isBot = false; pick.ai = null; pick.remote = true; pick.netId = id; pick.name = nm; pick.kills = pick.deaths = 0;
        pick.fmove = pick.smove = 0; pick.bFire = pick.bFire2 = pick.bUse = false;
        pick.spawnSeq++;
        g.chat(nm + " зашёл в игру", 0xFFE080);
        rosterDirty = true;
    }
    final java.util.Vector pendingJoins = new java.util.Vector();

    void peerLeft(int id) {
        Player p = byNet(id);
        if (p == null) return;
        g.chat(p.name + " вышел из игры", 0xFFE080);
        botify(p);
        rosterDirty = true;
    }

    void botify(Player p) {
        p.remote = false; p.netId = 0; p.isBot = true; p.name = g.botName(); p.ai = new Bot(g, p);
        p.bFire = p.bFire2 = p.bUse = false;
    }

    void becomeHost() {
        host = true;
        if (g.pl != null) {
            for (int i = 0; i < g.pl.length; i++) {
                Player p = g.pl[i];
                if (p == g.me) continue;
                if (p.netId == 0) { p.isBot = true; p.remote = false; p.ai = new Bot(g, p); if (p.alive) p.ai.newRound(); }
                else p.remote = true;
            }
        }
        g.mpBecameHost();
        g.chat("Вы теперь ведущий игры", 0xFFE080);
        rosterDirty = true;
    }

    void fromClient(int id, Net.Rd r) {
        int op = r.u8();
        if (op == 'Q') { Player p = byNet(id); if (p == null) flushJoins(); sendRoster(id); return; }
        Player p = byNet(id);
        if (p == null) return;
        switch (op) {
            case 'S': {
                if (!p.alive) return;
                p.ox = r.i16(); p.oy = r.i16(); p.oz = r.i16() / 4f;
                p.vx = r.i16(); p.vy = r.i16(); p.vz = r.i16();
                p.yaw = r.u16() * 360f / 65536f; p.pitch = (byte) r.u8();
                int f = r.u8();
                p.bFire = (f & 1) != 0; p.bFire2 = (f & 2) != 0; p.bUse = (f & 4) != 0; p.bDuck = (f & 8) != 0;
                p.ducked = (f & 16) != 0; p.onGround = (f & 32) != 0; p.silM4 = (f & 64) != 0; p.silUSP = (f & 128) != 0;
                int cur = r.u8(); p.zoom = r.u8();
                if (cur != p.cur && cur < Wp.N && p.hasWeapon(cur)) g.switchTo(p, cur);
                break;
            }
            case 'B': {
                int kind = r.u8(), v = r.u8();
                if (!g.canBuy(p)) return;
                if (kind == 0 && v < Wp.N) g.buyWeapon(p, v); else if (kind == 1) g.buyEquip(p, v); else if (kind == 2) g.buyAmmo(p, v);
                break;
            }
            case 'L': g.startReload(p); break;
            case 'D': g.dropCurrent(p); break;
            case 'r': g.radio(p, r.u8()); break;
            case 'V': {   // a part of a voice clip: pass it on to everyone with the speaker's slot, and play it here
                int seq = r.u8(), part = r.u8(), parts = r.u8(), n = r.u16();
                if (roomPlayers > 1) {
                    Net.Buf v = new Net.Buf(); v.u8('V').u8(p.id).u8(seq).u8(part).u8(parts).u16(n).raw(r.b, r.p, Math.min((n + 1) / 2, r.b.length - r.p));
                    toAll(v);
                }
                g.voiceIn(p.id, seq, part, parts, n, r.b, r.p);
                break;
            }
        }
    }

    void flushJoins() {
        while (pendingJoins.size() > 0) {
            Object[] o = (Object[]) pendingJoins.elementAt(0); pendingJoins.removeElementAt(0);
            peerJoin(((Integer) o[0]).intValue(), (String) o[1]);
        }
    }

    public void hostLoaded() {
        flushJoins();
        if (lan != null) g.chat("Игра по LAN создана" + (lan.ip != null ? ", адрес " + lan.ip : "") + ". Ждём игроков из Wi-Fi", 0xFFE080);
    }

    void sendRoster(int to) {
        Player[] pl = g.pl;
        b.reset().u8('I').u8(g.mode).u16((int) g.roundTime).u8(g.maxRounds).u8(pl.length);
        for (int i = 0; i < pl.length; i++) b.u16(pl[i].netId).u8(pl[i].team).u8(pl[i].isBot ? 1 : 0).str(pl[i].name);
        if (to < 0) toAll(b); else toOne(to, b);
    }

    void sendSnapshot() {
        float tl = g.time < g.freezeEnd ? g.freezeEnd - g.time : Math.max(0, g.freezeEnd + g.roundTime - g.time);
        int fl = (g.time < g.freezeEnd ? 1 : 0) | (g.roundOver ? 2 : 0) | (g.bombPlanted ? 4 : 0) | (g.bombDropped ? 8 : 0) | (g.state == Game.ST_END ? 16 : 0);
        b.reset().u8('P').u8(g.round).u8(g.scoreT).u8(g.scoreCT).u16((int) (tl * 10)).u8(fl)
                .u8((int) Math.max(0, Math.min(255, g.buyEnd - g.time)))
                .i16(g.bombX).i16(g.bombY).i16(g.bombZ).u8((int) Math.max(0, Math.min(255, g.bombAt - g.time)));
        Player[] pl = g.pl;
        b.u8(pl.length);
        for (int i = 0; i < pl.length; i++) {
            Player p = pl[i];
            int f = (p.alive ? 1 : 0) | (p.ducked ? 2 : 0) | (g.time - p.lastShot < 0.12f ? 4 : 0) | (p.planting ? 8 : 0) | (p.defusing ? 16 : 0)
                    | (p.c4 ? 32 : 0) | (p.zombie ? 64 : 0) | (p.helmet ? 128 : 0);
            b.u8(f).u8(p.team).i16(p.ox).i16(p.oy).i16(p.oz * 4).u16((int) (p.yaw * 65536 / 360) & 0xFFFF).u8((int) p.pitch)
                    .u8(p.cur).u16(Math.max(0, Math.min(65535, p.health))).u8(p.armor).u16(p.money)
                    .u8(p.primary & 255).u8(p.secondary & 255).u8(p.he | (p.flash << 2) | (p.smoke << 4) | (p.defuser ? 64 : 0))
                    .u8(p.kills & 255).u8(p.deaths & 255).u8(p.spawnSeq & 255)
                    .u8((int) Math.max(0, Math.min(255, (p.flashUntil - g.time) * 10))).u8(p.ztype);
        }
        toAll(b);
    }

    // host -> everyone events
    public void evKill(Player a, Player v, int w, boolean hs) { if (!active()) return; toAll(e.reset().u8('K').u8(a == null ? 255 : a.id).u8(v.id).u8(w).u8(hs ? 1 : 0)); }
    public void evCenter(String s, float dur, int col) { if (!active()) return; toAll(e.reset().u8('C').str(s).u8((int) (dur * 10)).i32(col)); }
    public void evSound(Player src, float x, float y, float z, String nm, int vol, float range) {
        if (!active()) return;
        toAll(e.reset().u8('N').u8(src == null ? 255 : src.id).i16(x).i16(y).i16(z).u8(vol).u16((int) range).str(nm));
    }
    public void evRadio(Player p, int cmd) { if (!active()) return; toAll(e.reset().u8('R').u8(p.id).u8(cmd)); }
    public void evChat(String s, int col) { if (!active()) return; toAll(e.reset().u8('T').str(s).i32(col)); }
    boolean active() { return host && phase == 2 && roomPlayers > 1 && !applying; }

    // ------------------------------------------------------------------ client side
    public void sendState() {
        Player p = g.me;
        if (p == null || !p.alive) return;
        int f = (p.bFire ? 1 : 0) | (p.bFire2 ? 2 : 0) | (p.bUse ? 4 : 0) | (p.bDuck ? 8 : 0) | (p.ducked ? 16 : 0) | (p.onGround ? 32 : 0)
                | (p.silM4 ? 64 : 0) | (p.silUSP ? 128 : 0);
        toHost(b.reset().u8('S').i16(p.ox).i16(p.oy).i16(p.oz * 4).i16(p.vx).i16(p.vy).i16(p.vz)
                .u16((int) (p.yaw * 65536 / 360) & 0xFFFF).u8((int) p.pitch & 255).u8(f).u8(p.cur).u8(p.zoom));
    }
    public void act(int op, int a, int c) {
        b.reset().u8(op); if (a >= 0) b.u8(a); if (c >= 0) b.u8(c);
        toHost(b);
    }

    void fromHost(byte[] m, Net.Rd r) {
        int op = r.u8();
        if (op == 'I') { if (g.state != Game.ST_PLAY || phase != 2) { pendingRoster = m; return; } applyRoster(r); return; }
        if (phase != 2 || g.state != Game.ST_PLAY || g.pl == null || !rosterOk) return;
        applying = true;
        try {
            switch (op) {
                case 'P': applySnapshot(r); break;
                case 'K': {
                    int a = r.u8(), v = r.u8(), w = r.u8(); boolean hs = r.u8() != 0;
                    if (v < g.pl.length) g.killfeed(a < g.pl.length ? g.pl[a] : null, g.pl[v], w, hs);
                    break;
                }
                case 'C': { String s = r.str(); float d = r.u8() / 10f; int col = r.i32(); g.center(s, d, col); break; }
                case 'N': {
                    int src = r.u8(); float x = r.i16(), y = r.i16(), z = r.i16(); int vol = r.u8(), range = r.u16(); String nm = r.str();
                    if (src < g.pl.length && g.pl[src] == g.me && (nm.startsWith("weapons_") || nm.startsWith("player_pl_step") || nm.startsWith("items_"))) break;
                    g.soundAt(x, y, z, nm, vol, range);
                    break;
                }
                case 'R': { int p = r.u8(), cmd = r.u8(); if (p < g.pl.length) { g.pl[p].lastRadio = -10; g.radio(g.pl[p], cmd); } break; }
                case 'T': { String s = r.str(); g.chat(s, r.i32()); break; }
                case 'V': { int pid = r.u8(), seq = r.u8(), part = r.u8(), parts = r.u8(), n = r.u16(); g.voiceIn(pid, seq, part, parts, n, r.b, r.p); break; }
            }
        } finally { applying = false; }
    }

    void applyRoster(Net.Rd r) {
        int md = r.u8(); g.mode = md; g.roundTime = r.u16(); g.maxRounds = r.u8();
        int n = r.u8();
        Player[] old = g.pl;
        Player[] pl = new Player[n];
        for (int i = 0; i < n; i++) {
            Player p = old != null && i < old.length ? old[i] : new Player();
            p.id = i; p.netId = r.u16(); p.team = r.u8(); p.isBot = r.u8() != 0; p.name = r.str();
            p.ai = null; p.remote = false;
            pl[i] = p;
            if (p.netId == myId && myId != 0) { g.me = p; p.isBot = false; }
        }
        g.pl = pl;
        pendingRoster = null; rosterOk = true;
    }

    void applySnapshot(Net.Rd r) {
        int round = r.u8(); g.scoreT = r.u8(); g.scoreCT = r.u8();
        float tl = r.u16() / 10f; int fl = r.u8(); float buyLeft = r.u8();
        float bx = r.i16(), by = r.i16(), bz = r.i16(); int bombLeft = r.u8();
        if (round != lastRound) { lastRound = round; g.clientNewRound(); }
        g.round = round;
        if ((fl & 1) != 0) g.freezeEnd = g.time + tl; else g.freezeEnd = g.time + tl - g.roundTime;
        g.buyEnd = g.time + buyLeft;
        boolean wasOver = g.roundOver;
        g.roundOver = (fl & 2) != 0; if (g.roundOver && !wasOver) g.roundEndAt = g.time + 5;
        g.bombPlanted = (fl & 4) != 0; g.bombDropped = (fl & 8) != 0;
        if (g.bombPlanted || g.bombDropped) { g.bombX = bx; g.bombY = by; g.bombZ = bz; g.bombAt = g.time + bombLeft; }
        if ((fl & 16) != 0 && g.state == Game.ST_PLAY) { g.state = Game.ST_END; Sys.s.stopSounds(); }
        int n = r.u8();
        for (int i = 0; i < n && i < g.pl.length; i++) {
            Player p = g.pl[i];
            int f = r.u8(); int team = r.u8();
            float x = r.i16(), y = r.i16(), z = r.i16() / 4f, yaw = r.u16() * 360f / 65536f; int pitch = (byte) r.u8();
            int cur = r.u8(), hp = r.u16(), armor = r.u8(), money = r.u16(), prim = r.u8(), sec = r.u8(), gr = r.u8(), kills = r.u8(), deaths = r.u8(), seq = r.u8(), flash = r.u8(), zt = r.u8();
            boolean alive = (f & 1) != 0;
            boolean wasZ = p.zombie;
            p.team = team; p.kills = (byte) kills; p.ztype = zt < 3 ? zt : 0; p.deaths = deaths; p.c4 = (f & 32) != 0; p.zombie = (f & 64) != 0;
            p.planting = (f & 8) != 0; p.defusing = (f & 16) != 0; p.helmet = (f & 128) != 0;
            if (p == g.me && p.zombie && !wasZ) { g.center("Вы заражены! Вы — " + Game.ZNAME[p.ztype] + " (" + Game.ZDESC[p.ztype] + ")", 4, 0x60FF60); g.buyOpen = false; }
            if (p == g.me) { applyMe(p, f, alive, x, y, z, yaw, cur, hp, armor, money, prim, sec, gr, seq, flash); continue; }
            p.health = hp; p.money = money; p.armor = armor;
            p.primary = prim == 255 ? -1 : prim; p.secondary = sec == 255 ? -1 : sec;
            if (cur < Wp.N) p.cur = cur;
            if ((f & 4) != 0) p.lastShot = g.time;
            if (p.alive && !alive) { p.alive = false; p.deathTime = g.time; p.animTime = 0; p.deathAnim = 0; }
            boolean snap = !p.alive && alive || seq != (p.spawnSeq & 255);
            p.alive = alive; p.spawnSeq = seq; p.ducked = (f & 2) != 0;
            p.tx = x; p.ty = y; p.tz = z; p.tyaw = yaw; p.pitch = pitch;
            float dx = x - p.ox, dy = y - p.oy;
            if (snap || dx * dx + dy * dy > 300 * 300) { p.ox = x; p.oy = y; p.oz = z; p.yaw = yaw; }
        }
    }

    void applyMe(Player p, int f, boolean alive, float x, float y, float z, float yaw, int cur, int hp, int armor, int money, int prim, int sec, int gr, int seq, int flash) {
        boolean respawn = alive && (!p.alive || seq != (p.spawnSeq & 255));
        p.spawnSeq = seq;
        if (respawn) {
            p.resetForRound();
            p.ox = x; p.oy = y; p.oz = z; p.yaw = yaw; p.pitch = 0;
            g.mv.categorize(p);
            g.specIdx = -1;
        } else if (p.alive && !alive) {
            p.alive = false; p.health = 0; p.deathTime = g.time; p.animTime = 0; g.specIdx = -1; g.buyOpen = false; p.zoom = 0;
        }
        if (!alive) return;
        if (hp < p.health) { g.hurtFlash = 0.25f; p.punchP -= 1.5f; }
        p.health = hp; p.armor = armor; p.money = money; p.defuser = (gr & 64) != 0;
        p.he = gr & 3; p.flash = (gr >> 2) & 3; p.smoke = (gr >> 4) & 3;
        int np = prim == 255 ? -1 : prim, ns = sec == 255 ? -1 : sec;
        if (np != p.primary) { p.primary = np; if (np >= 0) { p.clip[np] = Wp.clip[np]; p.ammo[np] = Wp.reserve[np]; } }
        if (ns != p.secondary) { p.secondary = ns; if (ns >= 0) { p.clip[ns] = Wp.clip[ns]; p.ammo[ns] = Wp.reserve[ns]; } }
        if (respawn || !p.hasWeapon(p.cur)) { int c = cur < Wp.N && p.hasWeapon(cur) ? cur : p.primary >= 0 ? p.primary : p.secondary >= 0 ? p.secondary : Wp.KNIFE; g.switchTo(p, c); }
        if (p.planting) { plantShown += 0.1f; p.plantProgress = plantShown; } else plantShown = 0;
        if (p.defusing) p.defuseProgress += 0.1f; else p.defuseProgress = 0;
        float fu = g.time + flash / 10f; if (flash > 0 && fu > p.flashUntil) { p.flashUntil = fu; p.flashStart = g.time; }
    }

    /** smooth the other players between snapshots */
    void interp(float dt) {
        float k = Math.min(1, dt * 12);
        for (int i = 0; i < g.pl.length; i++) {
            Player p = g.pl[i];
            if (p == g.me || !p.alive) { if (!p.alive) p.animTime += dt; continue; }
            float ox = p.ox, oy = p.oy;
            p.ox += (p.tx - p.ox) * k; p.oy += (p.ty - p.oy) * k; p.oz += (p.tz - p.oz) * k;
            p.yaw += g.angNorm(p.tyaw - p.yaw) * k;
            if (dt > 0) { p.vx = (p.ox - ox) / dt; p.vy = (p.oy - oy) / dt; }
            float sp = p.speedFactor();
            if (sp > 20) p.animTime += dt * sp / 220f; else p.animTime = 0;
            p.onGround = true;
        }
    }

    /** send a recorded clip in parts of Voice.PART samples (3 KB each) */
    int voiceSeq;
    public void sendVoice(short[] pcm) {
        if (phase != 2 || g.me == null) return;
        int n = Math.min(pcm.length, Voice.MAX_SEC * Voice.RATE);
        if (n < Voice.RATE / 5) return;
        Voice.normalize(pcm, n);
        int parts = (n + Voice.PART - 1) / Voice.PART, seq = ++voiceSeq & 255;
        for (int i = 0; i < parts; i++) {
            int off = i * Voice.PART, cnt = Math.min(Voice.PART, n - off);
            byte[] enc = Voice.encode(pcm, off, cnt);
            Net.Buf v = new Net.Buf();
            if (host) { if (roomPlayers > 1) toAll(v.u8('V').u8(g.me.id).u8(seq).u8(i).u8(parts).u16(cnt).raw(enc, 0, enc.length)); }
            else toHost(v.u8('V').u8(seq).u8(i).u8(parts).u16(cnt).raw(enc, 0, enc.length));
        }
    }

    // ------------------------------------------------------------------ send helpers
    void toHost(Net.Buf x) { net.send(Net.C_TOHOST, x); }
    void toAll(Net.Buf x) { net.send(Net.C_TOALL, x); }
    void toOne(int id, Net.Buf x) {
        Net.Buf y = new Net.Buf(); y.u16(id); y.need(x.n); System.arraycopy(x.b, 0, y.b, y.n, x.n); y.n += x.n;
        net.send(Net.C_TOONE, y);
    }
}
