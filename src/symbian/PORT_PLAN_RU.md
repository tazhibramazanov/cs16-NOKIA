# План портирования cs16-NOKIA-main на Symbian C++ (Nokia E52)

## Этап 1. Проверка окружения (выполнено)

| Что проверялось | Результат |
|---|---|
| Переменные `EPOC32`, `SYMBIANSDK`, `S60SDK` | не заданы |
| Каталоги `C:\Symbian`, `C:\S60`, `C:\Nokia\...`, `C:\Qt` |不存在 / SDK не найден |
| Утилиты `bldmake`, `abld`, `makesis`, `signsis`, `devicescmd` | не найдены |
| Компиляторы `gcc`, `g++`, `clang++`, `cl` | не найдены |
| Python 3, Java | есть |

Вывод: на этой машине можно написать и проверить скрипты/данные, но **собрать ARM-бинарник нельзя**. Сборка `.sisx` выполняется на машине с S60 3rd FP2 SDK + GCCE; `tools/build_symbian.py` автоматически определит SDK и либо соберёт пакет, либо выдаст понятное сообщение и не сорвёт обычную сборку `.jar`/`.exe`.

## Этап 2. Расположение

```
cs16-NOKIA-main/
  src/core/...        НЕ ТРОГАЕТСЯ (эталон логики)
  src/midp/...        НЕ ТРОГАЕТСЯ
  src/pc/...          НЕ ТРОГАЕТСЯ
  src/symbian/        НОВОЕ
    group/            bld.inf, cs16e52.mmp
    inc/              Engine.h, Assets.h, Renderer.h, Game.h, Audio.h,
                      Network.h, App.h, Cs16.hrh
    src/              Engine.cpp, Assets.cpp, Renderer.cpp, Game.cpp,
                      Audio.cpp, Network.cpp, App.cpp
    data/             .rss, _reg.rss, .pkg
    tools/            import_assets.py, build_symbian.py
    ANALYSIS_RU.md, README_RU.md
```

## Этап 3. Соответствие классов

| Java (`src/core/cs`, `src/pc/cspc`) | Нативный модуль | Что сохраняется |
|---|---|---|
| `Render.java` | `Renderer.h/.cpp` | BSP front-to-back, span buffer MAXSEG=64, z-buffer, sky 5 граней, lightmap-градиенты, perspective resample, mip-уровни, все quality-knobs (`mipBias`, `stepShift`, `bilinear`, `flatSky`, `mipScale`) |
| `MapData.java` | `Assets.cpp` (`CMap`) | бинарный CSE3: палитра/tint/LUT/3 mip/texinfo/planes/verts/faces/nodes/leaves/vis/clipnodes/models/entities; `pointLeaf`, `trace`, `surfaceTex`, `lightAt` |
| `Mdl3.java`, `Anim3.java`, `Sprites.java` | `Assets.cpp` (`CModel`, `CAnimation`, `CSpriteSet`) | CSM2/CSA3/CST1/HUD1/FNT1 побайтно, `pose()` со смешиванием кватернионов |
| `Gfx.java`, `Lang.java` | `Renderer.cpp` + `Assets.cpp` | HUD, панели, кнопки, шрифты f1/f2 (ASCII+кириллица), локализация RU/EN |
| `Move.java`, `Player.java`, `Bot.java`, `Nav.java` | `Game.cpp` | физика (GRAVITY/FRICTION/ACCEL/STEP/JUMP), BSP-trace, навигация, боты |
| `Wp.java`, `Binds.java` | `Game.cpp` (таблицы的生成 из Java) | все 30 оружий, цены, урон, темп, разброс, прицел, radio-команды, раскладки клавиш E52 |
| `Game.java` (3127 строк) | `Game.cpp` | состояния/меню, настройки (byte v8), раунды, деньги, бомба, режими, боты, HUD, Auto resolution, Nokia FR, voice |
| `MP.java`, `MqttLink.java`, `LanFinder.java`, `LanServer.java`, `Net/Udp/Conn/Pipe` | `Network.cpp` (`RSocketServ`/`RSocket`) | тот же протокол и формат пакетов, LAN-поиск, TCP-сессии, MQTT-комнаты |
| `Voice.java`, `Mixer.java` (PC) | `Audio.cpp` | 8-канальный софт-микшер, WAV 11025/8/mono, PTT через `CMdaAudioInputStream`, вывод через `CMdaAudioOutputStream` |
| `Sys.java`, `MidpSys.java`, `GameCanvas.java`, `CSMidlet.java` | `App.cpp` | `CAknApplication`/`CAknDocument`/`CAknAppUi`/`CCoeControl`, DSA, `OfferKeyEventL`, `CActive`+`RTimer` |

## Этап 4. Оптимизации (без потери качества)

1. DSA → прямая запись в RGB565 (`EColor64K`), никаких ARGB32-буферов, `drawRGB`, поворотов и масштабирований MIDP.
2. Таблица `shade[64*256]` сразу в RGB565; текстуры — 8-бит индексы со степенными размерами и mip-сдвигами.
3. 16.16 fixed point в physics/span; LUT sin/cos и 1/z; `FX_Sqrt`.
4. Ни одного `new` в кадре: пулы игроков/эффектов/звуков/вершин, все буферы выделяются в `ConstructL`.
5. `CActive` + `RTimer::HighRes` + `User::NTickCount()` вместо `CPeriodic`; точный pacing и FPS-лимит Nokia FR.
6. `-O3 -march=armv6 -mtune=arm1136j-s -fno-exceptions -fno-rtti`, без `TRAP`/`CleanupStack` внутри рендера.

## Этап 5. Ресурсы

`tools/import_assets.py` читает уже сгенерированные `conv/`-скриптами файлы из `res/` и `build/` **только на чтение**, копирует их в `data\cs16\` и конвертирует `sky_des.png` в `sky_des.rgb565` (128×640) на этапе импорта. Форматы CSE3/CSM2/CSA3/CST1/HUD1/FNT1/WAV не меняются.

## Этап 6. Сборка

`group/cs16e52.mmp`, `group/bld.inf`, `inc/Cs16.hrh`, `data/cs16e52.rss`, `data/cs16e52_reg.rss`, `data/cs16e52.pkg`; `tools/build_symbian.py` вызывается из `build.py` после обычной сборки. При отсутствии SDK шаг пропускается с предупреждением.

## Порядок написания кода

Engine → Assets → Renderer → Game → Audio → Network → App → build files → scripts → README.
