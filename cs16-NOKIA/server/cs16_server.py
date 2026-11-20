#!/usr/bin/env python3
"""CS 1.6 E52 Edition 1.0 - multiplayer server (matchmaking + relay).

Players (Nokia E52 and PC) connect over plain TCP. Players who pick the same
mode and map share a room (up to 10 people); empty slots are bots.
The first player in a room is the "host": his game runs the bots, rounds and
damage. The server only forwards messages; if the host leaves, the next
player becomes host.

Run:  python3 cs16_server.py [port]      (default port 27016, or $PORT)
No dependencies besides Python 3.7+.
"""
import asyncio, os, struct, sys, time

PROTO = 1
MAX_HUMANS = 10
IDLE_TIMEOUT = 30
MODES = ('original', 'deathmatch', 'zombie')

C_JOIN, C_TOHOST, C_TOALL, C_TOONE, C_PING = 1, 2, 3, 4, 5
S_WELCOME, S_PEERJOIN, S_PEERLEFT, S_HOST, S_ROOM, S_PONG, S_ERROR, S_FROMCLIENT, S_FROMHOST = 10, 11, 12, 13, 14, 15, 16, 20, 21


def log(*a):
    print(time.strftime('%Y-%m-%d %H:%M:%S'), *a, flush=True)


def ustr(s):
    b = s.encode('utf-8')[:255]
    return bytes([len(b)]) + b


class Client:
    def __init__(self, w):
        self.w = w
        self.net_id = 0
        self.name = '?'
        self.room = None
        self.joined_at = time.time()
        self.last = time.time()

    def send(self, typ, payload=b''):
        if self.w.is_closing():
            return
        self.w.write(struct.pack('>HB', len(payload) + 1, typ) + payload)


class Room:
    def __init__(self, rid, mode, mp):
        self.id, self.mode, self.map = rid, mode, mp
        self.clients = []   # join order; clients[0] is the host
        self.host = None


class Server:
    def __init__(self):
        self.rooms = {}
        self.next_room = 1
        self.next_id = 1
        self.online = 0

    def new_id(self):
        self.next_id = self.next_id % 65000 + 1
        return self.next_id

    def room_info(self, room):
        p = struct.pack('>BH', len(room.clients), self.online)
        for c in room.clients:
            c.send(S_ROOM, p)

    def join(self, c, mode, mp, name, create=False):
        best = None
        for r in ([] if create else self.rooms.values()):
            if r.mode == mode and r.map == mp and len(r.clients) < MAX_HUMANS:
                if best is None or len(r.clients) > len(best.clients):
                    best = r
        if best is None:
            best = Room(self.next_room, mode, mp)
            self.next_room = self.next_room % 65000 + 1
            self.rooms[best.id] = best
        c.net_id, c.name, c.room = self.new_id(), name[:24] or 'Игрок', best
        best.clients.append(c)
        if best.host is None:
            best.host = c
        c.send(S_WELCOME, struct.pack('>HBHBH', c.net_id, 1 if best.host is c else 0, best.id, len(best.clients), best.host.net_id))
        if best.host is not c:
            best.host.send(S_PEERJOIN, struct.pack('>H', c.net_id) + ustr(c.name))
        log('join', c.net_id, repr(c.name), 'room', best.id, MODES[mode] if mode < 3 else mode, 'map', mp,
            'players', len(best.clients), 'host' if best.host is c else '')
        self.room_info(best)

    def leave(self, c):
        r = c.room
        if r is None:
            return
        c.room = None
        if c in r.clients:
            r.clients.remove(c)
        log('leave', c.net_id, repr(c.name), 'room', r.id, 'left', len(r.clients))
        if not r.clients:
            del self.rooms[r.id]
            return
        if r.host is c:
            r.host = r.clients[0]
            r.host.send(S_HOST)
            log('room', r.id, 'new host', r.host.net_id)
        r.host.send(S_PEERLEFT, struct.pack('>H', c.net_id))
        self.room_info(r)

    def handle(self, c, typ, p):
        r = c.room
        if typ == C_PING:
            c.send(S_PONG)
        elif typ == C_JOIN:
            if r is not None or len(p) < 3:
                return
            if p[0] != PROTO:
                c.send(S_ERROR, ustr('Нужна другая версия игры'))
                return
            nl = p[3] if len(p) > 3 else 0
            name = p[4:4 + nl].decode('utf-8', 'replace')
            flags = p[4 + nl] if len(p) > 4 + nl else 0
            self.join(c, p[1], p[2], name, create=bool(flags & 1))
        elif r is None:
            return
        elif typ == C_TOHOST:
            if r.host is not c:
                r.host.send(S_FROMCLIENT, struct.pack('>H', c.net_id) + p)
        elif typ == C_TOALL:
            if r.host is c:
                for o in r.clients:
                    if o is not c:
                        o.send(S_FROMHOST, p)
        elif typ == C_TOONE:
            if r.host is c and len(p) >= 2:
                tid = struct.unpack('>H', p[:2])[0]
                for o in r.clients:
                    if o.net_id == tid:
                        o.send(S_FROMHOST, p[2:])

    async def conn(self, reader, writer):
        c = Client(writer)
        self.online += 1
        peer = writer.get_extra_info('peername')
        try:
            sock = writer.get_extra_info('socket')
            if sock is not None:
                import socket
                sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        except Exception:
            pass
        try:
            while True:
                hdr = await asyncio.wait_for(reader.readexactly(2), IDLE_TIMEOUT)
                n = struct.unpack('>H', hdr)[0]
                if n < 1 or n > 8192:
                    break
                body = await asyncio.wait_for(reader.readexactly(n), IDLE_TIMEOUT)
                c.last = time.time()
                self.handle(c, body[0], body[1:])
                if writer.transport.get_write_buffer_size() > 512 * 1024:
                    log('slow client dropped', c.net_id, peer)
                    break
        except (asyncio.IncompleteReadError, asyncio.TimeoutError, ConnectionError, OSError):
            pass
        finally:
            self.online -= 1
            self.leave(c)
            try:
                writer.close()
            except Exception:
                pass


async def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else int(os.environ.get('PORT', 27016))
    host = os.environ.get('HOST', '0.0.0.0')
    srv = Server()
    s = await asyncio.start_server(srv.conn, host, port)
    log('CS 1.6 E52 server 1.0 listening on', host, port)
    async with s:
        await s.serve_forever()


if __name__ == '__main__':
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
