#!/usr/bin/env python3
"""Static sanity check for the native port (runs without a Symbian SDK).

Checks include-file resolution, balanced braces, missing method definitions,
and forbidden placeholder markers. It does not replace a real compiler.
"""

import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
INC = os.path.join(ROOT, "inc")
SRC = os.path.join(ROOT, "src")


def read(path):
    with open(path, "rb") as handle:
        data = handle.read()
    try:
        return data.decode("utf-8")
    except UnicodeDecodeError:
        return data.decode("cp1251", errors="replace")


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"//[^\r\n]*", "", text)
    return text


def brace_balance(text):
    depth = 0
    minimum = 0
    for char in text:
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            minimum = min(minimum, depth)
    return depth, minimum


def main():
    errors = []
    headers = sorted(os.listdir(INC)) if os.path.isdir(INC) else []
    sources = sorted(os.listdir(SRC)) if os.path.isdir(SRC) else []
    all_text = {}

    for name in headers + sources:
        if not name.endswith((".h", ".cpp")):
            continue
        path = os.path.join(INC if name.endswith(".h") else SRC, name)
        raw = read(path)
        all_text[name] = strip_comments(raw)

        depth, minimum = brace_balance(all_text[name])
        if depth != 0:
            errors.append("%s: unbalanced braces (%d)" % (name, depth))
        if minimum < 0:
            errors.append("%s: closing brace without opening" % name)

        for match in re.finditer(r'#include\s+"([^"]+)"', raw):
            included = match.group(1)
            if not os.path.exists(os.path.join(INC, included)):
                errors.append("%s: missing local include %s" % (name, included))

        for marker in ("TODO", "FIXME", "NOT IMPLEMENTED", "заглушка", "stub"):
            if marker.lower() in raw.lower():
                errors.append("%s: placeholder marker %r" % (name, marker))

    definitions = set()
    for name, text in all_text.items():
        if not name.endswith(".cpp"):
            continue
        for match in re.finditer(
            r"(?:^|\n)\s*(?:[A-Za-z_][\w:<>\*&\s]*?\s+)?(~?[A-Za-z_]\w*::)?"
            r"(~?[A-Za-z_]\w*)\s*\(([^;{}]*)\)\s*(?:const\s*)?\{",
            text,
        ):
            owner, method = match.group(1), match.group(2)
            definitions.add(((owner or "").rstrip(":"), method))

    for header in headers:
        if not header.endswith(".h") or header in ("LangData.h", "WeaponData.h"):
            continue
        text = all_text[header]
        class_blocks = re.finditer(r"class\s+(C[A-Za-z_]\w*)\s*(?::[^{]+)?\{(.*?)\n\s*\};", text, re.S)
        for block in class_blocks:
            class_name, body = block.group(1), block.group(2)
            for match in re.finditer(r"(?:^|\n)\s*(?:virtual\s+|static\s+)?[\w:<>\*&\s]+?\s+(~?[A-Za-z_]\w*)\s*\(([^;{}]*)\)\s*(?:const\s*)?(?:=0\s*)?;", body):
                method = match.group(1)
                if method in ("operator", "if", "while", "for", "switch"):
                    continue
                if (class_name, method) not in definitions and ("", method) not in definitions:
                    errors.append("%s::%s declared but not defined" % (class_name, method))

    if errors:
        print("Static check FAILED (%d problems):" % len(errors))
        for problem in errors:
            print("  - " + problem)
        return 1
    print("Static check OK: %d files, %d definitions" % (len(all_text), len(definitions)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
