#!/usr/bin/env python3
"""
gen_data.py — regenerates LangData.h and WeaponData.h from the untouched Java
core sources. Guarantees that the native port uses exactly the same texts and
weapon values as src/core/cs/Lang.java and src/core/cs/Wp.java.

Usage:
    python src/symbian/tools/gen_data.py            # writes inc/*.h
    python src/symbian/tools/gen_data.py --check    # only compares, no writes
"""
import argparse
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SYMBIAN = os.path.abspath(os.path.join(HERE, ".."))
ROOT = os.path.abspath(os.path.join(SYMBIAN, "..", ".."))
INC = os.path.join(SYMBIAN, "inc")

WEAPON_FIELDS = (
    ("price", "KWpPrice", "TInt"),
    ("slot", "KWpSlot", "TInt"),
    ("team", "KWpTeam", "TInt"),
    ("clip", "KWpClip", "TInt"),
    ("reserve", "KWpReserve", "TInt"),
    ("ammoPrice", "KWpAmmoPrice", "TInt"),
    ("dmg", "KWpDmg", "TInt"),
    ("rangeMod", "KWpRangeMod", "TReal32"),
    ("cycle", "KWpCycle", "TReal32"),
    ("reloadT", "KWpReloadT", "TReal32"),
    ("auto", "KWpAuto", "TBool"),
    ("armorR", "KWpArmorR", "TReal32"),
    ("speed", "KWpSpeed", "TInt"),
    ("pellets", "KWpPellets", "TInt"),
    ("spread", "KWpSpread", "TReal32"),
    ("moveSpread", "KWpMoveSpread", "TReal32"),
    ("recoilSpread", "KWpRecoilSpread", "TReal32"),
    ("punch", "KWpPunch", "TReal32"),
    ("zoom1", "KWpZoom1", "TInt"),
    ("zoom2", "KWpZoom2", "TInt"),
    ("drawT", "KWpDrawT", "TReal32"),
    ("penPower", "KWpPenPower", "TInt"),
    ("penCount", "KWpPenCount", "TInt"),
)


def read(path):
    with open(path, encoding="utf-8") as handle:
        return handle.read()


def java_string_array(source, name):
    marker = "String[] %s = {" % name
    start = source.find(marker)
    if start < 0:
        raise RuntimeError("no array %s in Java source" % name)
    start = source.find("{", start) + 1
    end = source.find("};", start)
    if end < 0:
        raise RuntimeError("unterminated array %s" % name)
    values = []
    for line in source[start:end].splitlines():
        line = line.strip()
        if not line:
            continue
        match = re.match(r'^"(.*)"(?:\s*,)?$', line)
        if not match:
            raise RuntimeError("unexpected line in array %s: %s" % (name, line))
        text = match.group(1)
        if "\\" in text:
            raise RuntimeError("escape sequences need manual review: %s" % text)
        values.append(text)
    return values


def cpp_wide_literal(text):
    # Symbian's TText is 16-bit; the explicit cast keeps Zig/Clang stubs happy.
    return '(const TText*)L"%s"' % text.replace('"', '\\"')


def generate_lang():
    source = read(os.path.join(ROOT, "src", "core", "cs", "Lang.java"))
    russian = java_string_array(source, "RU_")
    english = java_string_array(source, "EN_")
    if len(russian) != len(english):
        raise RuntimeError("RU_/EN_ arrays have different lengths")
    lines = [
        "#ifndef CS16_LANG_DATA_H",
        "#define CS16_LANG_DATA_H",
        "",
        "/* Generated from src/core/cs/Lang.java -- do not edit by hand. */",
        "",
        "static const TInt KLangCount = %d;" % len(russian),
        "",
        "static const TText* const KRussianText[] = {",
    ]
    lines += ["    %s," % cpp_wide_literal(value) for value in russian]
    lines += [
        "};",
        "",
        "static const TText* const KEnglishText[] = {",
    ]
    lines += ["    %s," % cpp_wide_literal(value) for value in english]
    lines += ["};", "", "#endif", ""]
    return "\n".join(lines)


def java_array(source, name):
    marker = "[] %s = {" % name
    start = source.find(marker)
    if start < 0:
        raise RuntimeError("no array %s in Java source" % name)
    start = source.find("{", start) + 1
    depth = 1
    position = start
    while position < len(source) and depth:
        if source[position] == "{":
            depth += 1
        elif source[position] == "}":
            depth -= 1
        position += 1
    body = source[start:position - 1]
    body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
    body = re.sub(r"//[^\r\n]*", "", body)
    return [item.strip() for item in body.split(",") if item.strip()]


def cpp_number(item, kind):
    if kind == "TBool":
        return {"false": "EFalse", "true": "ETrue"}[item]
    if kind == "TReal32":
        if not item.endswith("f"):
            return item                     # Java int literal, valid for TReal32 too
        value = item[:-1]
        if "." not in value:
            value += ".0"                   # 1f is invalid C++; emit 1.0f
        return value + "f"
    return item


def generate_weapons():
    source = read(os.path.join(ROOT, "src", "core", "cs", "Wp.java"))
    arrays = {java: java_array(source, java) for java, _, _ in WEAPON_FIELDS}
    counts = {len(values) for values in arrays.values()}
    if len(counts) != 1:
        raise RuntimeError("weapon arrays have different lengths: %s" % sorted(counts))
    count = counts.pop()
    lines = [
        "#ifndef CS16_WEAPON_DATA_H",
        "#define CS16_WEAPON_DATA_H",
        "",
        "/* Generated from src/core/cs/Wp.java -- weapon values are identical. */",
        "",
        "const TInt KWeaponCount = %d;" % count,
        "",
    ]
    for java, native, kind in WEAPON_FIELDS:
        values = ", ".join(cpp_number(item, kind) for item in arrays[java])
        lines.append("static const %s %s[%d] = { %s };" % (kind, native, count, values))
    lines += ["", "#endif", ""]
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="compare with generated content only")
    args = parser.parse_args()
    outputs = (("LangData.h", generate_lang()), ("WeaponData.h", generate_weapons()))
    failed = False
    for name, content in outputs:
        path = os.path.join(INC, name)
        current = read(path) if os.path.isfile(path) else None
        if args.check:
            if current == content:
                print("OK   %s" % name)
            else:
                print("DIFF %s (regenerate with gen_data.py)" % name)
                failed = True
        elif current != content:
            with open(path, "w", encoding="utf-8", newline="\n") as handle:
                handle.write(content)
            print("WROTE %s" % path)
        else:
            print("OK   %s (unchanged)" % name)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
