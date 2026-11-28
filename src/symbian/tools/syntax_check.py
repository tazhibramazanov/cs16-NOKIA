#!/usr/bin/env python3
"""C++ syntax check for the Symbian port without a Symbian SDK.

Runs ``python -m ziglang c++ -fsyntax-only`` over every ``src/*.cpp`` with
``inc`` and ``tools/symbian_stubs`` as include directories. Only stub SDK
headers are used; the game sources are never modified.

Usage:
    python src/symbian/tools/syntax_check.py [name ...]

Optional arguments filter source files by substring, e.g.::

    python src/symbian/tools/syntax_check.py Engine Assets Renderer Network Audio
"""

import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
INC = os.path.join(ROOT, "inc")
SRC = os.path.join(ROOT, "src")
STUBS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "symbian_stubs")

DIAGNOSTIC = re.compile(r"^[^\s].*?:\d+:\d+: (?:fatal )?(error|warning):")
# Zig 0.17.1 sometimes fails to cache a successful -fsyntax-only object on
# Windows. It is unrelated to the source being checked.
CACHE_NOISE = re.compile(r"failed to rename .*?-(\w+)\.obj.*FileNotFound")
MAX_DIAGNOSTICS = 40


def check(path, environment):
    command = [
        sys.executable, "-m", "ziglang", "c++", "-fsyntax-only",
        "-m32",  # Symbian EABI has 32-bit pointers; host Python is 64-bit.
        "-I", INC, "-I", STUBS, path,
    ]
    result = subprocess.run(command, cwd=ROOT, env=environment,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, errors="replace")
    lines = result.stdout.splitlines()
    diagnostics = [line for line in lines
                   if DIAGNOSTIC.match(line) and not CACHE_NOISE.search(line)]
    return result.returncode, diagnostics, lines


def main(argv):
    # A fresh cache prevents stale diagnostics when the stubs are edited while
    # other agents are still writing the game sources.
    cache_dir = tempfile.mkdtemp(prefix="cs16-zigcache-")
    environment = os.environ.copy()
    environment["ZIG_LOCAL_CACHE_DIR"] = cache_dir
    environment["ZIG_GLOBAL_CACHE_DIR"] = cache_dir
    try:
        names = [name.lower() for name in argv]
        sources = sorted(name for name in os.listdir(SRC) if name.endswith(".cpp"))
        if names:
            sources = [name for name in sources
                       if any(part in name.lower() for part in names)]
        if not sources:
            print("No C++ sources to check in %s" % SRC)
            return 2

        failed = []
        for name in sources:
            path = os.path.join(SRC, name)
            code, diagnostics, lines = check(path, environment)
            if not diagnostics:
                print("OK   %s" % name)
                continue
            failed.append(name)
            print("FAIL %s (%d diagnostic%s)" %
                  (name, len(diagnostics), "" if len(diagnostics) == 1 else "s"))
            shown = diagnostics or lines
            for line in shown[:MAX_DIAGNOSTICS]:
                print("     " + line)
            if len(shown) > MAX_DIAGNOSTICS:
                print("     ... %d more" % (len(shown) - MAX_DIAGNOSTICS))

        print("\nSyntax check: %d/%d files OK" %
              (len(sources) - len(failed), len(sources)))
        return 1 if failed else 0
    finally:
        shutil.rmtree(cache_dir, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
