#!/usr/bin/env python3
"""
build_symbian.py — сборка нативного CS 1.6 E52 для S60 3rd Edition FP2 (GCCE).

1. Вызывает import_assets.py (только чтение из res/ и build/).
2. Ищет SDK по переменным EPOCROOT / EPOC32 / SYMBIANSDK / S60SDK и по
   стандартным путям установки S60 3rd FP2.
3. bldmake bldfiles -> abld build gcce urel -> makesis -> signsis.

Если SDK или ассетов нет, скрипт печатает понятное сообщение и завершается
с кодом 0: обычная сборка .jar / .exe в build.py продолжается как обычно.
"""
import argparse
import base64
import glob
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SYMBIAN = os.path.abspath(os.path.join(HERE, '..'))
ROOT = os.path.abspath(os.path.join(SYMBIAN, '..', '..'))
# Кириллица в пути ломает cpp.exe/makmake в SDK FP2 — используем junction при наличии.
for _asci_root in (r'C:\CS16_NOKIA_SRC',):
    try:
        if os.path.isdir(_asci_root):
            _cand = os.path.join(_asci_root, 'src', 'symbian')
            if os.path.isdir(_cand):
                SYMBIAN = os.path.abspath(_cand)
                ROOT = os.path.abspath(_asci_root)
                break
    except Exception:
        pass
GROUP = os.path.join(SYMBIAN, 'group')
DATA = os.path.join(SYMBIAN, 'data')
DEFAULT_OUT = os.path.join(ROOT, 'build', 'symbian')
DEFAULT_ASSETS = os.path.join(DEFAULT_OUT, 'data', 'cs16')
DEFAULT_PKG = os.path.join(DATA, 'cs16e52.pkg')
DEFAULT_CERT = os.path.join(ROOT, 'cert', 'cs16e52_cert.pem')
DEFAULT_KEY = os.path.join(ROOT, 'cert', 'cs16e52_key.pem')
TARGET = 'cs16e52'
UID3 = '0x2003C516'
PACKAGE = 'CS16_E52.sisx'

sys.path.insert(0, HERE)
import import_assets  # noqa: E402  (свой скрипт лежит рядом)


def say(text=''):
    print(text, flush=True)


# --------------------------------------------------------------------- SDK
def _normalise_sdk_path(value):
    if not value:
        return None
    path = os.path.abspath(os.path.expanduser(os.path.expandvars(value.strip().strip('"'))))
    parts = path.split(os.sep)
    lowered = [part.lower() for part in parts]
    if 'epoc32' in lowered:
        path = os.sep.join(parts[:lowered.index('epoc32')]) or os.sep
    if not os.path.isdir(path):
        return None
    if not any(entry.lower() == 'epoc32' and os.path.isdir(os.path.join(path, entry))
               for entry in os.listdir(path)):
        return None
    return os.path.normpath(path)


def find_sdk(explicit=None):
    candidates = [explicit] if explicit else []
    for variable in ('EPOCROOT', 'EPOC32', 'SYMBIANSDK', 'S60SDK'):
        candidates.append(os.environ.get(variable))
    candidates += [
        r'C:\Symbian\9.2\S60_3rd_FP2',
        r'C:\Symbian\9.2\S60_3rd_FP2_SDK',
        r'C:\Symbian\9.3\S60_3rd_FP2',
        r'C:\S60\devices\S60_3rd_FP2_SDK_v1.1',
        r'C:\S60\devices\S60_3rd_FP2_SDK',
        r'C:\Nokia\Devices\S60_3rd_FP2_SDK',
        r'C:\Program Files (x86)\Nokia\S60_3rd_FP2_SDK',
        r'C:\Program Files\Nokia\S60_3rd_FP2_SDK',
    ]
    for pattern in (r'C:\Symbian\*\S60*FP2*', r'C:\S60\devices\*FP2*',
                    r'C:\Nokia\Devices\*FP2*', r'C:\Program Files (x86)\Nokia\*FP2*'):
        candidates.extend(glob.glob(pattern))
    seen = set()
    for candidate in candidates:
        sdk = _normalise_sdk_path(candidate)
        if sdk and sdk.lower() not in seen:
            seen.add(sdk.lower())
            return sdk
    return None


def epoc_dir(sdk):
    for entry in os.listdir(sdk):
        if entry.lower() == 'epoc32':
            return os.path.join(sdk, entry)
    return None


def find_tool(executable, sdk, override=None):
    names = [executable + '.exe', executable + '.bat', executable + '.cmd', executable]
    choices = []
    if override:
        choices.append(override)
    else:
        epoc = epoc_dir(sdk)
        for folder in ([os.path.join(epoc, 'tools')] if epoc else []) + [os.path.join(sdk, 'tools')]:
            for name in names:
                choices.append(os.path.join(folder, name))
        from_path = shutil.which(executable)
        if from_path:
            choices.append(from_path)
    for choice in choices:
        if os.path.isfile(choice):
            return choice
    return None


# ------------------------------------------------------------------ пакет
def make_resolved_pkg(template, destination, assets_dir):
    """Заменяет относительный путь к данным абсолютным и убирает комментарии."""
    with open(template, encoding='utf-8-sig', errors='replace') as handle:
        lines = handle.read().splitlines()
    kept = []
    for line in lines:
        stripped = line.strip()
        if not stripped or stripped.startswith(';') or stripped.startswith('/*') or stripped.startswith('*'):
            continue
        try:
            stripped.encode('ascii')
        except UnicodeEncodeError:
            continue                      # makesis спокойнее с ASCII-пакетом
        kept.append(stripped)
    text = '\n'.join(kept) + '\n'

    relative = os.path.relpath(DEFAULT_ASSETS, DATA).replace('/', '\\')
    old = '"%s\\-"' % relative
    new = '"%s\\-"' % os.path.abspath(assets_dir).replace('/', '\\')
    if old not in text:
        raise RuntimeError('в %s не найдена строка подключения папки данных' % os.path.basename(template))
    text = text.replace(old, new)
    resolved = os.path.join(destination, 'CS16_E52_generated.pkg')
    with open(resolved, 'w', encoding='utf-8', errors='replace', newline='\n') as handle:
        handle.write(text)
    return resolved


def prepare_certificate(cert_path, key_path, destination):
    cert_path = os.path.abspath(cert_path)
    key_path = os.path.abspath(key_path)
    for label, path in (('сертификат', cert_path), ('ключ', key_path)):
        if not os.path.isfile(path):
            raise RuntimeError('нет %s: %s (обычно его создаёт build.py -> build_sis.py)' % (label, path))
    with open(cert_path, 'rb') as handle:
        raw = handle.read()
    if raw.startswith(b'\x30'):             # уже DER
        return cert_path, key_path
    match = re.search(br'-----BEGIN CERTIFICATE-----(.*?)-----END CERTIFICATE-----', raw, re.S)
    if not match:
        raise RuntimeError('не удалось прочитать PEM-сертификат: ' + cert_path)
    der = base64.b64decode(b''.join(match.group(1).split()))
    if not der.startswith(b'\x30'):
        raise RuntimeError('сертификат не в формате X.509 DER: ' + cert_path)
    der_path = os.path.join(destination, 'cs16e52_cert.der')
    with open(der_path, 'wb') as handle:
        handle.write(der)
    return der_path, key_path


def execute(command, cwd, env):
    say('  > ' + ' '.join(os.path.basename(str(part)) if index == 0 else str(part)
                          for index, part in enumerate(command)))
    try:
        result = subprocess.run(command, cwd=cwd, env=env)
    except OSError as exc:
        raise RuntimeError('не удалось запустить %s: %s' % (os.path.basename(str(command[0])), exc))
    if result.returncode != 0:
        raise RuntimeError('%s завершился с кодом %d' % (os.path.basename(str(command[0])), result.returncode))


# -------------------------------------------------------------------- main
def parse_args(argv):
    parser = argparse.ArgumentParser(description='Нативная сборка CS 1.6 E52 (S60 3rd FP2, GCCE)')
    parser.add_argument('--sdk', metavar='ПУТЬ', help='корень S60 3rd FP2 SDK')
    parser.add_argument('--output', metavar='ПУТЬ', default=DEFAULT_OUT,
                        help='куда положить CS16_E52.sisx (по умолчанию %s)' % DEFAULT_OUT)
    parser.add_argument('--assets-dir', metavar='ПУТЬ', default=DEFAULT_ASSETS,
                        help='папка данных пакета; на телефоне !:\\data\\cs16')
    parser.add_argument('--source', action='append', metavar='ПУТЬ',
                        help='источник ассетов для import_assets.py (можно несколько раз)')
    parser.add_argument('--pkg', metavar='ПУТЬ', default=DEFAULT_PKG, help='шаблон PKG-файла')
    parser.add_argument('--cert', metavar='ПУТЬ', default=DEFAULT_CERT, help='PEM/DER сертификат')
    parser.add_argument('--key', metavar='ПУТЬ', default=DEFAULT_KEY, help='PEM ключ подписи')
    parser.add_argument('--bldmake', metavar='ПУТЬ', help='полный путь к bldmake')
    parser.add_argument('--abld', metavar='ПУТЬ', help='полный путь к abld')
    parser.add_argument('--makesis', metavar='ПУТЬ', help='полный путь к makesis')
    parser.add_argument('--signsis', metavar='ПУТЬ', help='полный путь к signsis')
    parser.add_argument('--abld-args', nargs='+', default=['build', 'gcce', 'urel'],
                        help='аргументы abld (по умолчанию build gcce urel)')
    return parser.parse_args(argv)


def main(argv=None):
    args = parse_args(argv)
    output = os.path.abspath(os.path.expanduser(args.output))
    assets = os.path.abspath(os.path.expanduser(args.assets_dir))
    os.makedirs(output, exist_ok=True)
    stale = os.path.join(output, PACKAGE)
    if os.path.isfile(stale):                 # не показывать старый пакет как свежий
        os.remove(stale)

    say('[symbian] Шаг 1: импорт ресурсов в ' + assets)
    try:
        count = import_assets.import_assets(args.source, assets)
    except Exception as exc:
        say('[symbian] ОШИБКА импорта ресурсов: %s' % exc)
        say('[symbian] Нативный пакет не собран; обычная сборка продолжается.')
        return 0
    if not count:
        return 0

    sdk = find_sdk(args.sdk)
    if not sdk:
        say('[symbian] S60 3rd Edition FP2 SDK не найден — нативный .sisx не собран.')
        say('[symbian] Установите S60 3rd FP2 SDK + GCCE и задайте EPOCROOT (или')
        say('[symbian] укажите путь: python build_symbian.py --sdk "C:\\Symbian\\9.2\\S60_3rd_FP2").')
        say('[symbian] Обычная сборка .jar / .exe продолжается. Подробности: README_RU.md.')
        return 0
    say('[symbian] SDK: ' + sdk)

    tools = {}
    for name in ('bldmake', 'abld', 'makesis', 'signsis'):
        tools[name] = find_tool(name, sdk, getattr(args, name))
        if not tools[name]:
            say('[symbian] Не найден %s из S60 SDK. Укажите его полный путь через --%s.'
                % (name, name))
            say('[symbian] Нативный пакет не собран; обычная сборка продолжается.')
            return 0

    try:
        certificate, key = prepare_certificate(args.cert, args.key, output)
        resolved_pkg = make_resolved_pkg(args.pkg, output, assets)

        epoc = epoc_dir(sdk)
        env = dict(os.environ)
        env['EPOCROOT'] = sdk + os.sep          # инструменты Symbian требуют завершающий слеш
        env['EPOC32'] = epoc
        env['PATH'] = os.path.join(epoc, 'tools') + os.pathsep + env.get('PATH', '')
        env['PYTHONIOENCODING'] = 'utf-8'

        say('[symbian] Шаг 2: bldmake + abld (GCCE, urel)')
        execute([tools['bldmake'], 'bldfiles'], GROUP, env)
        execute([tools['abld']] + list(args.abld_args), GROUP, env)

        built_exe = os.path.join(epoc, 'release', 'gcce', 'urel', TARGET + '.exe')
        if not os.path.isfile(built_exe):
            raise RuntimeError('abld не создал ' + built_exe)

        say('[symbian] Шаг 3: makesis + signsis')
        sis = os.path.join(output, 'CS16_E52.sis')
        sisx = os.path.join(output, PACKAGE)
        for path in (sis, sisx):
            if os.path.exists(path):
                os.remove(path)
        execute([tools['makesis'], resolved_pkg, sis], output, env)
        if not os.path.isfile(sis):
            raise RuntimeError('makesis не создал ' + sis)
        execute([tools['signsis'], sis, sisx, certificate, key], output, env)
        if not os.path.isfile(sisx):
            raise RuntimeError('signsis не создал ' + sisx)
        say('[symbian] Готово: %s (%.1f МБ)'
            % (sisx, os.path.getsize(sisx) / 1048576))
        say('[symbian] Установите пакет на телефон: скопируйте его на карту/по Bluetooth и откройте.')
    except Exception as exc:
        say('[symbian] ОШИБКА: %s' % exc)
        say('[symbian] Нативный пакет не собран; обычная сборка .jar / .exe не затронута.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
