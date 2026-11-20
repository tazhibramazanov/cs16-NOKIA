#!/usr/bin/env python3
"""
CS 1.6 — Nokia E52 Edition: сборка одной командой.

    python build.py                    — найдёт CS 1.6 сам (Steam, рядом с проектом или поиском по всем дискам)
    python build.py "D:\\Games\\CS 1.6"  — или укажите папку, где лежат cstrike и valve

Всё, чего не хватает (numpy, Pillow, Java, компилятор, ProGuard, библиотеки телефона), скачивается само.
Готовые файлы появятся на рабочем столе в папке «КС 1.6 НОКИЯ»: CS16_E52.sisx, CS16_E52.jar (+ .jad), CS16_E52_PC.exe.
"""
import glob, os, shutil, subprocess, sys, time, urllib.request, zipfile

ROOT = os.path.dirname(os.path.abspath(__file__))
CACHE = os.path.join(ROOT, '_cache')
LIB = os.path.join(CACHE, 'lib')
BUILD = os.path.join(ROOT, 'build')
OUT_NAME = 'КС 1.6 НОКИЯ'
RES = os.path.join(ROOT, 'res')
WIN = os.name == 'nt'
EXE = '.exe' if WIN else ''
VERSION = '1.1.0'
MAPS = ['de_dust2', 'de_dust', 'de_train']

MAVENS = ['https://repo1.maven.org/maven2/',                        # Maven Central and its mirrors, tried in order
          'https://maven-central.storage-download.googleapis.com/maven2/',
          'https://repo.maven.apache.org/maven2/']
JARS = {
    'ecj-4.6.1.jar': 'org/eclipse/jdt/core/compiler/ecj/4.6.1/ecj-4.6.1.jar',
    'cldcapi11-2.0.4.jar': 'org/microemu/cldcapi11/2.0.4/cldcapi11-2.0.4.jar',
    'midpapi20-2.0.4.jar': 'org/microemu/midpapi20/2.0.4/midpapi20-2.0.4.jar',
    'microemu-jsr-135-2.0.4.jar': 'org/microemu/microemu-jsr-135/2.0.4/microemu-jsr-135-2.0.4.jar',   # only RecordControl (voice chat) is taken from it
    'proguard-base-7.4.2.jar': 'com/guardsquare/proguard-base/7.4.2/proguard-base-7.4.2.jar',
    'proguard-core-9.1.1.jar': 'com/guardsquare/proguard-core/9.1.1/proguard-core-9.1.1.jar',
    'log4j-api-2.19.0.jar': 'org/apache/logging/log4j/log4j-api/2.19.0/log4j-api-2.19.0.jar',
    'log4j-core-2.19.0.jar': 'org/apache/logging/log4j/log4j-core/2.19.0/log4j-core-2.19.0.jar',
    'kotlin-stdlib-1.9.0.jar': 'org/jetbrains/kotlin/kotlin-stdlib/1.9.0/kotlin-stdlib-1.9.0.jar',
}
JDK_URLS = [   # tried in order; any JDK 17 with javac and jlink will do
    ('Eclipse Temurin', 'https://api.adoptium.net/v3/binary/latest/17/ga/windows/x64/jdk/hotspot/normal/eclipse'),
    ('Microsoft OpenJDK', 'https://aka.ms/download-jdk/microsoft-jdk-17-windows-x64.zip'),
    ('Amazon Corretto', 'https://corretto.aws/downloads/latest/amazon-corretto-17-x64-windows-jdk.zip'),
]
UA = 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0 Safari/537.36'


def say(s=''): print(s, flush=True)


def fail(s):
    say(); say('ОШИБКА: ' + s); say()
    if WIN: input('Нажмите Enter, чтобы закрыть...')
    sys.exit(1)


def step(n, s): say(); say('[%d/7] %s' % (n, s))


def download(url, dst, label, must=True):
    """must=False: return False instead of stopping the build (another source will be tried)"""
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    tmp = dst + '.part'
    for attempt in range(3):
        try:
            req = urllib.request.Request(url, headers={'User-Agent': UA})   # some sites refuse Python's default name (HTTP 403)
            with urllib.request.urlopen(req, timeout=60) as r, open(tmp, 'wb') as f:
                total = int(r.headers.get('Content-Length') or 0); got = 0; last = 0
                while True:
                    b = r.read(1 << 16)
                    if not b: break
                    f.write(b); got += len(b)
                    if time.time() - last > 0.5 and total:
                        last = time.time()
                        print('\r  %s: %d%% (%d из %d МБ)   ' % (label, got * 100 // total, got >> 20, total >> 20), end='', flush=True)
            print('\r  %s: готово%s' % (label, ' ' * 30), flush=True)
            os.replace(tmp, dst)
            return True
        except Exception as e:
            say('\n  не получилось скачать (%s)%s' % (e, ', ещё попытка...' if attempt < 2 else ''))
            if attempt < 2: time.sleep(10 if '429' in str(e) else 2)   # 429 = "too many requests": wait longer
    try: os.remove(tmp)
    except OSError: pass
    if not must: return False
    fail('не удалось скачать ' + url + '\nПроверьте интернет и запустите сборку ещё раз.')


def run(cmd, cwd=ROOT, env=None, quiet=False):
    r = subprocess.run(cmd, cwd=cwd, env=env, stdout=subprocess.PIPE if quiet else None, stderr=subprocess.STDOUT if quiet else None)
    if r.returncode != 0:
        if quiet and r.stdout: say(r.stdout.decode('utf-8', 'replace')[-3000:])
        fail('команда не выполнилась: ' + ' '.join(os.path.basename(str(c)) if i == 0 else str(c) for i, c in enumerate(cmd[:4])) + ' ...')


# ------------------------------------------------------------------ 1. Python libraries
def python_libs():
    step(1, 'Библиотеки Python (numpy, Pillow)')
    missing = []
    try: import numpy  # noqa
    except ImportError: missing.append('numpy')
    try: import PIL  # noqa
    except ImportError: missing.append('pillow')
    if not missing: say('  уже есть'); return
    say('  устанавливаю: ' + ', '.join(missing))
    run([sys.executable, '-m', 'pip', 'install', '--user', '--disable-pip-version-check'] + missing)


# ------------------------------------------------------------------ 2. Java (JDK with javac and jlink)
def jdk_ok(home):
    b = os.path.join(home, 'bin')
    return all(os.path.exists(os.path.join(b, t + EXE)) for t in ('java', 'javac', 'jlink'))


def find_jdk():
    step(2, 'Java (JDK)')
    cands = sorted(glob.glob(os.path.join(CACHE, 'jdk*')))
    if os.environ.get('JAVA_HOME'): cands.append(os.environ['JAVA_HOME'])
    jc = shutil.which('javac')
    if jc: cands.append(os.path.dirname(os.path.dirname(os.path.realpath(jc))))
    for h in cands:
        if jdk_ok(h):
            try:
                v = subprocess.run([os.path.join(h, 'bin', 'javac' + EXE), '-version'], stdout=subprocess.PIPE, stderr=subprocess.STDOUT).stdout.decode()
                import re
                m = re.search(r'javac (\d+)', v)
                if m and int(m.group(1)) >= 11: say('  найдена: ' + m.group(0)); return h
            except Exception: pass
    if not WIN: fail('нужен JDK 11 или новее (javac и jlink). Установите его, например: sudo apt install openjdk-17-jdk')
    say('  Java не найдена — скачиваю JDK 17 (около 190 МБ, один раз)')
    z = os.path.join(CACHE, 'jdk17.zip')
    for name, url in JDK_URLS:
        if not os.path.exists(z):
            say('  источник: ' + name)
            if not download(url, z, 'JDK 17', must=False): continue
        say('  распаковываю...')
        try:
            with zipfile.ZipFile(z) as zf:
                top = zf.namelist()[0].split('/')[0]
                zf.extractall(CACHE)
            h = os.path.join(CACHE, top)
        except Exception:
            h = None
        try: os.remove(z)
        except OSError: pass
        if h and jdk_ok(h): return h
        say('  архив повреждён, пробую другой источник...')
    fail('не удалось скачать Java ни с одного сайта.\n'
         'Можно поставить её вручную: https://adoptium.net (Temurin 17, файл .msi) — и запустить сборку ещё раз.')


def tool(jdk, name): return os.path.join(jdk, 'bin', name + EXE)


# ------------------------------------------------------------------ 3. build tools (jars)
def build_tools():
    step(3, 'Компилятор для телефона и ProGuard')
    for name, path in JARS.items():
        dst = os.path.join(LIB, name)
        if os.path.exists(dst): continue
        for i, m in enumerate(MAVENS):
            if download(m + path, dst, name, must=False): break
            if i + 1 < len(MAVENS): say('  пробую другое зеркало...')
        else:
            fail('не удалось скачать ' + name + '\nПроверьте интернет и запустите сборку ещё раз (через пару минут).')
    rec = os.path.join(LIB, 'mmapi-record.jar')   # the full-MMAPI recording interface, for compiling only (the phone has its own)
    if not os.path.exists(rec):
        with zipfile.ZipFile(os.path.join(LIB, 'microemu-jsr-135-2.0.4.jar')) as zi, zipfile.ZipFile(rec, 'w') as zo:
            n = 'javax/microedition/media/control/RecordControl.class'
            zo.writestr(n, zi.read(n))
    say('  всё на месте')


# ------------------------------------------------------------------ 4. the game files
def is_game(d):
    return d and os.path.isdir(os.path.join(d, 'cstrike', 'maps')) and os.path.isdir(os.path.join(d, 'valve')) \
        and all(os.path.exists(os.path.join(d, 'cstrike', 'maps', m + '.bsp')) for m in MAPS)


def steam_dirs():
    out = []
    if WIN:
        try:
            import winreg
            for hive, key in ((winreg.HKEY_CURRENT_USER, r'Software\Valve\Steam'), (winreg.HKEY_LOCAL_MACHINE, r'SOFTWARE\WOW6432Node\Valve\Steam')):
                try:
                    with winreg.OpenKey(hive, key) as k:
                        for v in ('SteamPath', 'InstallPath'):
                            try: out.append(winreg.QueryValueEx(k, v)[0])
                            except OSError: pass
                except OSError: pass
        except ImportError: pass
        out += [r'C:\Program Files (x86)\Steam', r'C:\Program Files\Steam', r'D:\Steam', r'D:\SteamLibrary', r'E:\SteamLibrary']
    else:
        out += [os.path.expanduser('~/.steam/steam'), os.path.expanduser('~/.local/share/Steam')]
    libs = []
    for s in out:
        libs.append(s)
        vdf = os.path.join(s, 'steamapps', 'libraryfolders.vdf')
        if os.path.exists(vdf):
            for line in open(vdf, encoding='utf-8', errors='ignore'):
                if '"path"' in line: libs.append(line.split('"')[3].replace('\\\\', '\\'))
    return [os.path.join(l, 'steamapps', 'common', 'Half-Life') for l in libs]


SKIP = {'windows', '$recycle.bin', 'system volume information', 'recovery', 'perflogs', 'winsxs',
        'node_modules', '.git', '__pycache__', '$windows.~bt', '$windows.~ws', 'msocache', 'config.msi',
        'temp', 'tmp', 'packages', 'microsoft', 'windowsapps', 'nvidia corporation', 'intel', 'amd'}
if os.name != 'nt':
    SKIP |= {'proc', 'sys', 'dev', 'run', 'snap', 'usr', 'lib', 'lib64', 'bin', 'sbin', 'etc', 'var', 'boot'}


def roots():
    """Где искать: сначала папка пользователя (рабочий стол, загрузки...), потом все диски целиком."""
    home = os.path.expanduser('~')
    out = [os.path.join(home, n) for n in ('Desktop', 'Рабочий стол', 'OneDrive', 'Downloads', 'Documents', 'Games')]
    out.append(home)
    if WIN:
        import string
        out += ['%s:\\' % c for c in string.ascii_uppercase if os.path.exists('%s:\\' % c)]
    else:
        out += ['/home', '/mnt', '/media', '/opt', '/']
    return [r for r in out if os.path.isdir(r)]


def scan_disks():
    """Обходит диски и ищет папку, в которой лежат cstrike (с нужными картами) и valve."""
    skip_own = {os.path.normcase(os.path.abspath(p)) for p in (CACHE, BUILD, RES)}
    seen, t0, last = set(), time.time(), 0.0
    for top in roots():
        stack = [(top, 0)]
        while stack:
            d, depth = stack.pop()
            key = os.path.normcase(os.path.abspath(d))
            if key in seen or key in skip_own: continue
            seen.add(key)
            now = time.time()
            if now - last > 1.5:
                last = now
                print('\r  ищу (%d с): %-60s' % (now - t0, d[-60:]), end='', flush=True)
            try:
                with os.scandir(d) as it: ents = list(it)
            except OSError:
                continue
            names = {e.name.lower() for e in ents}
            if 'cstrike' in names and 'valve' in names and is_game(d):
                print('\r' + ' ' * 80 + '\r', end='')
                return d
            if depth >= 9: continue
            for e in ents:
                try:
                    if e.is_symlink() or not e.is_dir(follow_symlinks=False): continue
                    if hasattr(os.path, 'isjunction') and os.path.isjunction(e.path): continue
                except OSError:
                    continue
                n = e.name.lower()
                if n in SKIP or n.startswith('$') or (n.startswith('.') and n not in ('.steam', '.local', 'share')):
                    continue
                stack.append((e.path, depth + 1))
    print('\r' + ' ' * 80 + '\r', end='')
    return None


def find_game(arg):
    step(4, 'Папка с Counter-Strike 1.6')
    saved = os.path.join(CACHE, 'game_path.txt')
    cands = [arg, os.environ.get('CS16_GAME')]
    if os.path.exists(saved): cands.append(open(saved, encoding='utf-8').read().strip())
    cands += [ROOT, os.path.dirname(ROOT)]
    cands += [os.path.join(ROOT, n) for n in ('cs16', 'CS 1.6', 'Counter-Strike 1.6', 'Half-Life', 'game')]
    cands += steam_dirs()
    game = next((c for c in cands if c and is_game(c)), None)
    if not game:
        say('  В Steam и рядом с проектом не нашёл — ищу по всем дискам (обычно 1–5 минут)...')
        game = scan_disks()
    if not game:
        say('  Не нашёл CS 1.6 на компьютере.')
        say('  Нужна папка, внутри которой лежат папки cstrike и valve')
        say('  (в Steam: ...\\steamapps\\common\\Half-Life).')
        while True:
            d = input('  Вставьте путь к ней и нажмите Enter: ').strip().strip('"')
            if is_game(d): game = d; break
            say('  В этой папке нет cstrike\\maps\\de_dust2.bsp, de_dust.bsp, de_train.bsp и папки valve. Попробуйте ещё раз.')
    say('  найдена: ' + game)
    os.makedirs(CACHE, exist_ok=True)
    with open(saved, 'w', encoding='utf-8') as f: f.write(game)
    return game


def desktop():
    """Настоящий рабочий стол (в том числе если он перенесён в OneDrive)."""
    if WIN:
        try:
            import ctypes
            buf = ctypes.create_unicode_buffer(260)
            if ctypes.windll.shell32.SHGetFolderPathW(None, 0x10, None, 0, buf) == 0 and os.path.isdir(buf.value):
                return buf.value
        except Exception:
            pass
    for n in ('Desktop', 'Рабочий стол'):
        d = os.path.join(os.path.expanduser('~'), n)
        if os.path.isdir(d): return d
    return ROOT


# ------------------------------------------------------------------ 5. convert
def convert(game):
    step(5, 'Перевожу карты, модели, звуки и интерфейс в формат телефона (несколько минут)')
    mark = os.path.join(RES, '.converted')
    env = dict(os.environ, CS16_GAME=game, PYTHONIOENCODING='utf-8')
    snd_job = ('звуки', ['conv/conv_snd.py', 'res/snd'])
    if os.path.exists(mark) and open(mark, encoding='utf-8').read() == game:
        if sound_count() < 20 or not os.path.exists(os.path.join(RES, 'snd', 'ui_buttonclick.wav')):   # older builds lost sounds: redo just them
            say('  звуков игры не хватает — перевожу звуки заново...')
            run([sys.executable] + snd_job[1], env=env, quiet=True)
            check_sounds()
            return
        say('  уже сделано раньше (удалите папку res, кроме icon.png и *.fnt, чтобы переконвертировать)'); return
    os.makedirs(os.path.join(RES, 'snd'), exist_ok=True)
    jobs = [('карты', ['conv/conv_map.py', 'res'] + MAPS),
            ('навигация ботов', ['conv/conv_nav.py', 'res'] + MAPS),
            snd_job,
            ('HUD и шрифты', ['conv/conv_hud.py', 'res']),
            ('3D-модели', ['conv/conv_mdl3d.py']),
            ('меню', ['conv/conv_ui.py']),
            ('игроки для минимальной графики', ['conv/bake.py', 'res', 'pl:terror:ak47:t', 'pl:gign:m4a1:ct'])]
    for i, (label, args) in enumerate(jobs):
        say('  %d/%d %s...' % (i + 1, len(jobs), label))
        run([sys.executable] + args, env=env, quiet=True)
    check_sounds()
    open(mark, 'w', encoding='utf-8').write(game)


def sound_count():
    d = os.path.join(RES, 'snd')
    return len(glob.glob(os.path.join(d, 'weapons_*.wav'))) + len(glob.glob(os.path.join(d, 'player_*.wav'))) if os.path.isdir(d) else 0


def check_sounds():
    n = sound_count()
    say('  звуков стрельбы и игроков: %d' % n)
    if n < 20: fail('звуки игры не перевелись (найдено %d). Проверьте, что в папке игры есть cstrike\\sound\\weapons' % n)


# ------------------------------------------------------------------ 6. build
def zipdir(zf, d, skip=()):
    for base, _, files in os.walk(d):
        for f in sorted(files):
            p = os.path.join(base, f); a = os.path.relpath(p, d).replace(os.sep, '/')
            if a.split('/')[0] in skip: continue
            zf.write(p, a)


def resources():
    pats = ['*.cse', '*.nv', 't.pls', 'ct.pls', '*.m3', '*.a3', 'mdltex.bin', '*.fnt', 'hud.bin', 'icon.png', 'sky_*.png', 'ui_*.png', 'snd/*.wav']
    out = []
    for p in pats:
        g = sorted(glob.glob(os.path.join(RES, p)))
        if not g: fail('нет файлов res/' + p + ' — конвертация не завершилась')
        out += g
    return out


PG_JARS = ('proguard-base-7.4.2.jar', 'proguard-core-9.1.1.jar', 'log4j-api-2.19.0.jar', 'log4j-core-2.19.0.jar', 'kotlin-stdlib-1.9.0.jar')


def build_phone(jdk):
    java = tool(jdk, 'java')
    cp = os.pathsep.join(os.path.join(LIB, j) for j in ('cldcapi11-2.0.4.jar', 'midpapi20-2.0.4.jar', 'mmapi-record.jar'))
    mcls = os.path.join(BUILD, 'mcls'); shutil.rmtree(mcls, ignore_errors=True); os.makedirs(mcls)
    src = glob.glob(os.path.join(ROOT, 'src', 'core', 'cs', '*.java')) + glob.glob(os.path.join(ROOT, 'src', 'midp', 'cs', 'midp', '*.java'))
    say('  компиляция для телефона...')
    run([java, '-jar', os.path.join(LIB, 'ecj-4.6.1.jar'), '-source', '1.3', '-target', '1.3', '-nowarn', '-encoding', 'UTF-8',
         '-bootclasspath', cp, '-d', mcls] + src, quiet=True)
    inj, outj = os.path.join(BUILD, 'in.jar'), os.path.join(BUILD, 'out.jar')
    with zipfile.ZipFile(inj, 'w') as zf: zipdir(zf, mcls)
    # ProGuard misreads folders with spaces, brackets or Russian letters ("Новая папка (2)"):
    # it runs inside the build folder and sees only short relative names
    pgd = os.path.join(BUILD, 'pg'); shutil.rmtree(pgd, ignore_errors=True); os.makedirs(pgd)
    for j in PG_JARS + ('cldcapi11-2.0.4.jar', 'midpapi20-2.0.4.jar', 'mmapi-record.jar'):
        shutil.copy(os.path.join(LIB, j), os.path.join(pgd, j))
    open(os.path.join(BUILD, 'pg.pro'), 'w', encoding='ascii').write('\n'.join([
        '-injars in.jar', '-outjars out.jar',
        '-libraryjars pg/cldcapi11-2.0.4.jar' + os.pathsep + 'pg/midpapi20-2.0.4.jar' + os.pathsep + 'pg/mmapi-record.jar',
        '-microedition', '-target 1.3', '-dontoptimize', '-overloadaggressively', "-repackageclasses ''",
        '-allowaccessmodification', '-keep public class cs.midp.CSMidlet', '']))
    say('  ProGuard (сжатие кода)...')
    if os.path.exists(outj): os.remove(outj)
    run([java, '-cp', os.pathsep.join('pg/' + j for j in PG_JARS), 'proguard.ProGuard', '@pg.pro'], cwd=BUILD, quiet=True)
    shutil.rmtree(pgd, ignore_errors=True)
    man = ('Manifest-Version: 1.0\nMIDlet-1: CS 1.6,/icon.png,cs.midp.CSMidlet\nMIDlet-Name: CS16E52\nMIDlet-Vendor: E52 Edition\n'
           'MIDlet-Version: %s\nMIDlet-Icon: /icon.png\nMicroEdition-Configuration: CLDC-1.1\nMicroEdition-Profile: MIDP-2.0\n'
           'Nokia-MIDlet-Category: Game\n' % VERSION)
    jar = os.path.join(BUILD, 'CS16_E52.jar')
    with zipfile.ZipFile(jar, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
        zf.writestr('META-INF/MANIFEST.MF', man.replace('\n', '\r\n'))
        with zipfile.ZipFile(outj) as zi:
            names = [n for n in zi.namelist() if not n.startswith('META-INF/')]
            dirs = sorted(set(n[:i + 1] for n in names for i in range(len(n)) if n[i] == '/'))
            for d in dirs:
                if d not in names: zf.writestr(d, b'')   # folder entries, as old phone loaders expect
            for n in names: zf.writestr(n, zi.read(n))
        for f in resources(): zf.write(f, os.path.basename(f))
    size = os.path.getsize(jar)
    open(os.path.join(BUILD, 'CS16_E52.jad'), 'w', encoding='utf-8').write(
        'MIDlet-1: CS 1.6,/icon.png,cs.midp.CSMidlet\nMIDlet-Name: CS16E52\nMIDlet-Vendor: E52 Edition\nMIDlet-Version: %s\n'
        'MIDlet-Jar-URL: CS16_E52.jar\nMIDlet-Jar-Size: %d\nMicroEdition-Configuration: CLDC-1.1\nMicroEdition-Profile: MIDP-2.0\n'
        'Nokia-MIDlet-Category: Game\n' % (VERSION, size))
    say('  CS16_E52.jar: %.1f МБ' % (size / 1048576))


def build_sis():
    sys.path.insert(0, os.path.join(ROOT, 'tools'))
    import rsa1
    if rsa1.ensure(os.path.join(ROOT, 'cert', 'cs16e52_key.pem'), os.path.join(ROOT, 'cert', 'cs16e52_cert.pem')):
        say('  создан ваш личный ключ подписи: cert/cs16e52_key.pem (никому его не давайте)')
    say('  подписанный установщик .sisx...')
    run([sys.executable, os.path.join('tools', 'build_sis.py')], quiet=True)


def build_pc(jdk):
    say('  компиляция для ПК...')
    pcc = os.path.join(BUILD, 'pccls'); shutil.rmtree(pcc, ignore_errors=True); os.makedirs(pcc)
    src = glob.glob(os.path.join(ROOT, 'src', 'core', 'cs', '*.java')) + glob.glob(os.path.join(ROOT, 'src', 'pc', 'cspc', '*.java'))
    run([tool(jdk, 'javac'), '-nowarn', '--release', '11', '-encoding', 'UTF-8', '-d', pcc] + src, quiet=True)
    jar = os.path.join(BUILD, 'CS16_PC.jar')
    with zipfile.ZipFile(jar, 'w', zipfile.ZIP_DEFLATED) as zf:
        zf.writestr('META-INF/MANIFEST.MF', 'Manifest-Version: 1.0\r\nMain-Class: cspc.PCMain\r\n\r\n')
        zipdir(zf, pcc)
        for f in resources(): zf.write(f, os.path.basename(f))


# sounds the game must have: menu clicks, shots, steps, hits, radio
KEY_SOUNDS = ('ui_buttonclick', 'ui_buttonrollover', 'weapons_ak47_1', 'weapons_m4a1_unsil_1', 'weapons_glock18_2', 'weapons_usp_unsil_1',
              'weapons_deagle_1', 'weapons_awp1', 'weapons_knife_slash1', 'weapons_clipin1', 'weapons_c4_plant', 'player_pl_step1',
              'player_bhit_flesh_1', 'player_die1', 'radio_go', 'radio_terwin', 'radio_ctwin')


def check_jar_sounds(jar):
    """open the finished game and make sure the key sounds are inside and are not silence"""
    import wave, io
    bad = []
    with zipfile.ZipFile(jar) as zf:
        names = set(zf.namelist())
        for s in KEY_SOUNDS:
            n = s + '.wav'
            if n not in names: bad.append(s + ' (нет файла)'); continue
            try:
                w = wave.open(io.BytesIO(zf.read(n))); fr = w.readframes(w.getnframes()); sw = w.getsampwidth(); w.close()
                vals = fr if sw == 1 else fr[1::2]
                if not vals or max(vals) - min(vals) < 8: bad.append(s + ' (тишина)')
            except Exception as e:
                bad.append(s + ' (битый: %s)' % e)
    if bad: fail('в %s испорчены звуки: %s' % (os.path.basename(jar), ', '.join(bad)))
    say('  звуки в %s проверены: %d из %d в порядке' % (os.path.basename(jar), len(KEY_SOUNDS), len(KEY_SOUNDS)))


def build_exe(jdk):
    if not WIN:
        say('  .exe собирается только на Windows — пропускаю (CS16_PC.jar запускается командой java -jar)'); return False
    stub = os.path.join(ROOT, 'launcher', 'stub.exe')
    if not os.path.exists(stub): say('  нет launcher/stub.exe — пропускаю .exe'); return False
    jre = os.path.join(CACHE, 'jre')
    if not os.path.isdir(jre):
        say('  собираю маленькую Java для игры...')
        run([tool(jdk, 'jlink'), '--add-modules', 'java.base,java.desktop', '--strip-debug', '--no-header-files', '--no-man-pages',
             '--compress=2', '--output', jre], quiet=True)
    pcdir = os.path.join(BUILD, 'pcdir'); shutil.rmtree(pcdir, ignore_errors=True); os.makedirs(pcdir)
    shutil.copytree(jre, os.path.join(pcdir, 'jre'))
    shutil.copy(os.path.join(BUILD, 'CS16_PC.jar'), pcdir)
    say('  упаковываю CS16_E52_PC.exe (сжатие займёт минуту-две)...')
    run([sys.executable, os.path.join('tools', 'pack_exe.py'), stub, pcdir, os.path.join(BUILD, 'CS16_E52_PC.exe')], quiet=True)
    return True


def main():
    say('CS 1.6 — Nokia E52 Edition %s: сборка' % VERSION)
    arg = sys.argv[1] if len(sys.argv) > 1 else None
    python_libs()
    jdk = find_jdk()
    build_tools()
    game = find_game(arg)
    convert(game)
    step(6, 'Сборка')
    os.makedirs(BUILD, exist_ok=True)
    build_phone(jdk)
    check_jar_sounds(os.path.join(BUILD, 'CS16_E52.jar'))
    build_sis()
    build_pc(jdk)
    check_jar_sounds(os.path.join(BUILD, 'CS16_PC.jar'))
    exe = build_exe(jdk)
    step(7, 'Готово')
    OUT = os.path.join(desktop(), OUT_NAME)
    os.makedirs(OUT, exist_ok=True)
    files = ['CS16_E52.sisx', 'CS16_E52.jar', 'CS16_E52.jad'] + (['CS16_E52_PC.exe'] if exe else ['CS16_PC.jar'])
    for f in files: shutil.copy(os.path.join(BUILD, f), OUT)
    say()
    say('  ' + '=' * 60)
    say('  ВСЁ ПОЛУЧИЛОСЬ! Готовые файлы лежат в папке:')
    say()
    say('      ' + OUT)
    say()
    say('  (на рабочем столе, папка «%s»)' % OUT_NAME if os.path.dirname(OUT) != ROOT else '  (в папке проекта, папка «%s»)' % OUT_NAME)
    say('  ' + '=' * 60)
    for f in files: say('    %-18s %6.1f МБ' % (f, os.path.getsize(os.path.join(OUT, f)) / 1048576))
    say()
    say('  На телефон: скопируйте CS16_E52.sisx на Nokia E52 и откройте его.')
    if exe: say('  На ПК: запустите CS16_E52_PC.exe.')
    say()
    if WIN:
        try: os.startfile(OUT)
        except OSError: pass
    if WIN: input('Нажмите Enter, чтобы закрыть...')


if __name__ == '__main__':
    try: main()
    except KeyboardInterrupt: fail('остановлено')
