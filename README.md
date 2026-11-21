# CS 1.6 — Nokia E52 Edition

A custom engine that runs **Counter-Strike 1.6** on button **Nokia Symbian smartphones** (built and tested on a Nokia E52) and on Windows PCs.

**English** · [Русская версия](README_RU.md)

## Which Nokia phones it runs on

The game adapts to the screen by itself: it stretches the picture to fill the display and switches to landscape on wide screens.

| Phones | Works? |
|---|---|
| **Symbian S60 3rd Edition** with keypads: E52, E55, E71, E72, E63, E66, E75, E5, N95, N82, N78, N79, N86, 6120, 6220, 6710, 5730, E51 and others | ✅ yes (`.sisx` or `.jar`) |
| **Symbian S60 5th Edition and Symbian^3/Anna/Belle** with a keyboard: N97, E6, E7, C6 and others | ✅ yes, with the keyboard |
| Touch-only phones: 5800, N8, C7, 500, 603, 700, 808 | ⚠️ starts, but there are no touch controls yet |
| Button **Series 40 / Asha** phones (6300, C2, C3, 2700, 5310, etc.) | ❌ no — not enough memory, the game is 5 MB |
| Basic phones (105, 106, 110, 3310) | ❌ no — they can't run Java |

The more powerful the phone, the smoother the game. On weak models set graphics to "Minimum" and resolution to "Auto" in the settings.

> This repository contains only the engine code, the converters and the server. The game files (maps, models, sounds, textures) are **not** included — they belong to Valve. To build the game you need your own copy of Counter-Strike 1.6.

## How to build (2 steps)

1. **Download the project:** green **Code → Download ZIP** button, then unzip it.
2. **Run `СОБРАТЬ.bat`** ("BUILD") and wait 2–5 minutes.

The builder does everything by itself:
- finds CS 1.6: in Steam, next to the project, or by searching all drives (the path is remembered); if it can't find it, it asks;
- downloads everything that is missing: Python libraries, Java, the compiler;
- converts maps, models and sounds into the phone format;
- builds the game and creates your personal signing key.

The finished files appear **on your desktop in the folder "КС 1.6 НОКИЯ"** — at the end the builder prints its full path and opens it:

| File | What it is |
|---|---|
| `CS16_E52.sisx` | installer for Nokia — copy it to the phone and open it |
| `CS16_E52.jar` | alternative file for the phone |
| `CS16_E52_PC.exe` | PC version — a single file with Java inside |

**You need the CS 1.6 folder with the `cstrike` and `valve` subfolders.** In Steam it is `...\steamapps\common\Half-Life`. It can be anywhere — the builder finds it by itself. You can also give the path manually: drag the folder onto `СОБРАТЬ.bat`.

If Python is not installed, `СОБРАТЬ.bat` tries to install it. If that fails, install Python 3 from [python.org](https://www.python.org/downloads/), tick **"Add python.exe to PATH"**, and run `СОБРАТЬ.bat` again.

On Linux and macOS: `python3 build.py /path/to/cs16` (JDK 11+ required; the `.exe` is not built there).

## Installing on a Nokia E52

Copy `CS16_E52.sisx` to the phone (USB, Bluetooth or a memory card) and open it. The game needs about 6 MB; a memory card is not required.

## Features

- Software 3D renderer (BSP, z-buffer, lighting), maps **de_dust2**, **de_dust**, **de_train**
- Real 3D player and first-person weapon models with animations
- Bots with map navigation and difficulty levels
- Modes: **Original** (bomb, rounds, buy menu), **Deathmatch**, **Zombie** (three zombie types: Runner, Brute, Walker)
- Multiplayer:
  - worldwide — without your own server (through free public MQTT brokers) or through your own server;
  - **worldwide game list**, just like LAN: host name, number of players (e.g. `3/10`), mode, map and score (`T 2:1 CT`) or the round number in Zombie. Pick any game from the list, press "Quick game", or create your own — it instantly shows up in everyone's list;
  - LAN (Wi-Fi) — no address to type;
  - empty slots are filled with bots; a joining player takes a bot's place;
  - players with English and Russian games can play together;
  - walkie-talkie voice chat: hold the key and talk (up to 6 seconds), release — the others hear you. On Nokia it's the green call key, on PC — `K`.
- **Two languages: English and Russian** (Settings → Language). The game starts in English
- Menus in the style of the original CS 1.6, graphics: Minimum / Medium / High / Maximum
- **Nokia FR and DLSS** — frame generation and dynamic resolution for weak phones
- Portrait (240×320) and landscape screens on the phone, every key can be remapped
- PC version: keyboard and mouse, a single `.exe` with built-in Java

## Phone controls (default)

| Key | Action |
|---|---|
| 2 / 8 / 4 / 6 | move |
| joystick | look |
| 5 / joystick center | fire |
| 1 | jump |
| 3 | crouch |
| 7 | reload |
| 0 | defuse / use |
| 9 | next weapon |
| * | scope / silencer |
| # | buy |

All keys can be remapped: Settings → Keys.

## Multiplayer

By default ("Server: auto") the game finds opponents by itself through free public MQTT brokers — nothing to set up. "Find a game worldwide" shows the list of all open games. You
