#!/usr/bin/env python3
"""
import_assets.py — подготовка данных для нативного CS 1.6 E52.

Файлы, которые создают conv/*.py, копируются без изменений из res/ и build/
в папку данных пакета (на телефоне !:\\data\\cs16\\). Исходные файлы только
читаются; каталоги src/core, src/midp и src/pc не трогаются вообще.

Кроме того sky_*.png (полоса 128x640 из пяти граней 128x128 в порядке
rt/lf/bk/ft/up) превращается в sky_*.rgb565 — 5*128*128 16-битных значений
RGB565 в порядке little-endian, как их читает CMap::LoadSkyL().

Декодер PNG написан на чистом Python (zlib + struct): Pillow не нужен.
Поддерживаются цветовые типы 0 (серый), 2 (RGB), 3 (палитра), 4 (серый+alpha),
6 (RGBA), глубины 1/2/4/8/16, все five фильтров и чересстрочная развёртка Adam7.
"""
import argparse
import fnmatch
import os
import shutil
import stat
import struct
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..'))

DEFAULT_SOURCES = [os.path.join(ROOT, 'res'), os.path.join(ROOT, 'build')]
DEFAULT_DEST = os.path.join(ROOT, 'build', 'symbian', 'data', 'cs16')

ASSET_PATTERNS = (
    '*.cse', '*.nv', '*.m3', '*.a3', 'mdltex.bin', 'hud.bin',
    '*.fnt', '*.wav', 't.pls', 'ct.pls', '*.vws', 'ui_*.png', 'icon.png', 'sky_*.png',
)
SKY_WIDTH, SKY_HEIGHT = 128, 640
SKY_FACES = 5
PRUNE_DIRS = {'.git', '__pycache__', 'node_modules', 'shots'}


def say(text=''):
    print(text, flush=True)


# --------------------------------------------------------------------- PNG
PNG_SIGNATURE = b'\x89PNG\r\n\x1a\n'
CHANNELS = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}
DEPTHS = {0: (1, 2, 4, 8, 16), 2: (8, 16), 3: (1, 2, 4, 8), 4: (8, 16), 6: (8, 16)}
ADAM7 = ((0, 0, 8, 8), (4, 0, 8, 8), (0, 4, 4, 8), (2, 0, 4, 4),
         (0, 2, 2, 4), (1, 0, 2, 2), (0, 1, 1, 2))


def _png_chunks(data):
    if data[:8] != PNG_SIGNATURE:
        raise ValueError('это не PNG (нет сигнатуры)')
    pos = 8
    while pos + 12 <= len(data):
        length, = struct.unpack_from('>I', data, pos)
        kind = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + length]
        stored, = struct.unpack_from('>I', data, pos + 8 + length)
        if zlib.crc32(kind + body) & 0xffffffff != stored:
            raise ValueError('битый блок PNG %s (CRC не совпал)' % kind.decode('latin-1'))
        yield kind, body
        pos += length + 12
        if kind == b'IEND':
            return


def _paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    return b if pb <= pc else c


def _unfilter(line, previous, kind, stride):
    if kind == 0:
        return
    if kind not in (1, 2, 3, 4):
        raise ValueError('неизвестный фильтр PNG: %d' % kind)
    for i in range(len(line)):
        left = line[i - stride] if i >= stride else 0
        up = previous[i]
        upper_left = previous[i - stride] if i >= stride else 0
        if kind == 1:
            add = left
        elif kind == 2:
            add = up
        elif kind == 3:
            add = (left + up) >> 1
        else:
            add = _paeth(left, up, upper_left)
        line[i] = (line[i] + add) & 0xff


def _sample(line, index, depth):
    if depth == 16:
        return line[index * 2]                       # старший байт: 16 -> 8 бит
    if depth == 8:
        return line[index]
    shift = 8 - depth - ((index * depth) & 7)
    return (line[(index * depth) >> 3] >> shift) & ((1 << depth) - 1)


def _rgb(values, colour_type, palette, depth):
    samples = values if isinstance(values, (tuple, list)) else (values,)
    if colour_type == 3:
        index = samples[0]
        return palette[index] if index < len(palette) else (0, 0, 0)
    if colour_type in (0, 4):
        maximum = (1 << depth) - 1
        grey = samples[0] * 255 // maximum if maximum else 0
        return (grey, grey, grey)
    if colour_type in (2, 6):
        return (samples[0], samples[1], samples[2])
    raise ValueError('неподдерживаемый цветовой тип PNG: %d' % colour_type)


def decode_png(path):
    """Возвращает (width, height, rows), rows[y][x] = (r, g, b)."""
    with open(path, 'rb') as handle:
        data = handle.read()
    width = height = depth = colour = interlace = None
    palette = []
    idat = []
    for kind, body in _png_chunks(data):
        if kind == b'IHDR':
            width, height, depth, colour, compression, filtering, interlace = struct.unpack('>IIBBBBB', body)
            if compression != 0 or filtering != 0:
                raise ValueError('поддерживаются только стандартные сжатие и фильтрация PNG')
            if colour not in CHANNELS or depth not in DEPTHS.get(colour, ()):
                raise ValueError('неподдерживаемое сочетание тип=%d глубина=%d' % (colour, depth))
            if interlace not in (0, 1):
                raise ValueError('неподдерживаемый тип чередования: %d' % interlace)
        elif kind == b'PLTE':
            palette = [(body[i], body[i + 1], body[i + 2]) for i in range(0, len(body) - 2, 3)]
        elif kind == b'IDAT':
            idat.append(body)
        elif kind == b'IEND':
            break
    if width is None or not idat:
        raise ValueError('в PNG нет IHDR или IDAT')
    try:
        raw = zlib.decompress(b''.join(idat))
    except zlib.error as exc:
        raise ValueError('не удалось распаковать IDAT: %s' % exc)

    channels = CHANNELS[colour]
    pixels = [[(0, 0, 0)] * width for _ in range(height)]
    passes = ADAM7 if interlace else ((0, 0, 1, 1),)
    offset = 0
    for x0, y0, step_x, step_y in passes:
        pw = (width - x0 + step_x - 1) // step_x
        ph = (height - y0 + step_y - 1) // step_y
        if pw <= 0 or ph <= 0:
            continue
        row_bytes = (pw * channels * depth + 7) // 8
        stride = max(1, (channels * depth + 7) // 8)
        previous = bytearray(row_bytes)
        for py in range(ph):
            if offset >= len(raw):
                raise ValueError('IDAT оборван раньше конца изображения')
            kind = raw[offset]
            offset += 1
            line = bytearray(raw[offset:offset + row_bytes])
            if len(line) != row_bytes:
                raise ValueError('IDAT оборван в середине строки')
            offset += row_bytes
            _unfilter(line, previous, kind, stride)
            effective_depth = depth if depth < 8 else 8   # 16-битные выборки уже приведены к 8 битам
            for px in range(pw):
                samples = tuple(_sample(line, px * channels + c, depth) for c in range(channels))
                pixels[y0 + py * step_y][x0 + px * step_x] = _rgb(samples, colour, palette, effective_depth)
            previous = line
    return width, height, pixels


def sky_to_rgb565(path):
    """Полоса 128x640 -> bytes из 81920 little-endian TUint16."""
    width, height, pixels = decode_png(path)
    if width != SKY_WIDTH or height != SKY_HEIGHT:
        raise ValueError('нужна полоса %dx%d, а файл %dx%d' % (SKY_WIDTH, SKY_HEIGHT, width, height))
    values = []
    for y in range(height):
        row = pixels[y]
        for x in range(width):
            red, green, blue = row[x]
            values.append(((red & 0xf8) << 8) | ((green & 0xfc) << 3) | ((blue & 0xf8) >> 3))
    return struct.pack('<%dH' % len(values), *values)


# ------------------------------------------------------------------ копирование
def collect_files(sources):
    found = {}
    for source in sources:
        source = os.path.abspath(os.path.expanduser(source))
        if os.path.isfile(source):
            candidates = [(source, os.path.basename(source))]
        elif os.path.isdir(source):
            candidates = []
            for base, dirs, files in os.walk(source):
                dirs[:] = sorted(d for d in dirs if d not in PRUNE_DIRS)
                for name in sorted(files):
                    candidates.append((os.path.join(base, name), os.path.relpath(os.path.join(base, name), source)))
        else:
            say('  источник не найден: ' + source)
            continue
        for full, relative in candidates:
            name = os.path.basename(full)
            if not any(fnmatch.fnmatch(name.lower(), pattern) for pattern in ASSET_PATTERNS):
                continue
            found.setdefault(name, full)          # первый источник имеет приоритет
    return found


def write_readonly(data_or_source, destination):
    os.makedirs(os.path.dirname(destination), exist_ok=True)
    if os.path.exists(destination):
        os.chmod(destination, stat.S_IREAD | stat.S_IWRITE)
    if isinstance(data_or_source, bytes):
        with open(destination, 'wb') as handle:
            handle.write(data_or_source)
    else:
        shutil.copyfile(data_or_source, destination)   # байты не меняются
    os.chmod(destination, stat.S_IREAD)


def import_assets(sources=None, destination=None):
    """Копирует ассеты в destination (по умолчанию — в пакет build/symbian)."""
    sources = list(sources or DEFAULT_SOURCES)
    destination = os.path.abspath(destination or DEFAULT_DEST)
    found = collect_files(sources)
    if not found:
        say('[import_assets] Ассеты не найдены: сначала запустите build.py '
            '(он вызовет conv/ и создаст .cse/.nv/.m3/.a3/.wav в res/).')
        say('[import_assets] Источники: ' + ', '.join(sources))
        say('[import_assets] Нативный пакет собран не будет; обычная сборка продолжается.')
        return 0

    copied = 0
    skies = 0
    for name in sorted(found):
        source = found[name]
        write_readonly(source, os.path.join(destination, name))
        copied += 1
        if name.startswith('sky_') and name.lower().endswith('.png'):
            try:
                raw = sky_to_rgb565(source)
                target = os.path.join(destination, name[:-4] + '.rgb565')
                write_readonly(raw, target)
                skies += 1
                say('  sky: %s -> %s (%d байт)' % (name, os.path.basename(target), len(raw)))
            except Exception as exc:
                say('  ВНИМАНИЕ: не удалось конвертировать %s: %s' % (name, exc))
    say('[import_assets] Скопировано %d файл(ов) в %s' % (copied, destination))
    say('[import_assets] Небо: %d файл(ов) RGB565 (128x640, 5 граней)' % skies)
    return copied


def main(argv=None):
    parser = argparse.ArgumentParser(description='Подготовка данных для нативного CS 1.6 E52')
    parser.add_argument('--source', action='append', metavar='ПУТЬ',
                        help='папка с результатами conv/ (можно указать несколько раз)')
    parser.add_argument('--dest', metavar='ПУТЬ', default=DEFAULT_DEST,
                        help='папка данных пакета (по умолчанию %s)' % DEFAULT_DEST)
    args = parser.parse_args(argv)
    try:
        import_assets(args.source, args.dest)
    except Exception as exc:
        say('[import_assets] ОШИБКА: %s' % exc)
        return 0                  # не срываем обычную сборку .jar/.exe
    return 0


if __name__ == '__main__':
    sys.exit(main())
