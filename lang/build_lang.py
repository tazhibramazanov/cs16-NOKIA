#!/usr/bin/env python3
"""Generate Lang.java and LangData.h from lang/translations_data.py (6 languages)."""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

from translations_data import RU, EN, FR, DE, JA, VI

assert len(RU) == len(EN) == len(FR) == len(DE) == len(JA) == len(VI), "length mismatch: %d,%d,%d,%d,%d,%d" % (len(RU),len(EN),len(FR),len(DE),len(JA),len(VI))


def jescape(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'


# --- Generate Lang.java ---
lines = []
lines.append('package cs;')
lines.append('')
lines.append('import java.util.Hashtable;')
lines.append('import java.util.Vector;')
lines.append('')
lines.append('/**')
lines.append(' * Game language. Generated from lang/translations_data.py by lang/build_lang.py.')
lines.append(' */')
lines.append('public final class Lang {')
lines.append('    public static final int EN = 0, RU = 1, FR = 2, DE = 3, JA = 4, VI = 5;')
lines.append('    public static int lang = EN;')
lines.append('')

for name, arr in [("RU", RU), ("EN", EN), ("FR", FR), ("DE", DE), ("JA", JA), ("VI", VI)]:
    lines.append(f'    static final String[] {name}_ = {{')
    for s in arr:
        lines.append(f'        {jescape(s)},')
    lines.append('    };')
    lines.append('')

lines.append('    static Hashtable exact;')
lines.append('    static final Hashtable cache = new Hashtable();')
lines.append('    /** tests: lines that still have Russian letters after translation */')
lines.append('    public static Vector missing;')
lines.append('')
lines.append('    public static String t(String s) {')
lines.append('        if (lang == RU || s == null || !cyr(s)) return s;')
lines.append('        Object c = cache.get(s);')
lines.append('        if (c != null) return (String) c;')
lines.append('        String r = translate(s);')
lines.append('        if (cache.size() > 600) cache.clear();')
lines.append('        cache.put(s, r);')
lines.append('        if (missing != null && cyr(r) && !missing.contains(s)) missing.addElement(s);')
lines.append('        return r;')
lines.append('    }')
lines.append('')
lines.append('    static boolean cyr(String s) {')
lines.append('        for (int i = s.length() - 1; i >= 0; i--) { char ch = s.charAt(i); if (ch >= 0x400 && ch <= 0x4FF) return true; }')
lines.append('        return false;')
lines.append('    }')
lines.append('')
lines.append('    static boolean letter(char ch) { return ch >= 0x400 && ch <= 0x4FF; }')
lines.append('')
lines.append('    static String translate(String s) {')
lines.append('        if (exact == null) {')
lines.append('            Hashtable h = new Hashtable();')
lines.append('            for (int i = 0; i < RU_.length; i++) {')
lines.append('                if (lang == EN) h.put(RU_[i], EN_[i]);')
lines.append('                else if (lang == FR) h.put(RU_[i], FR_[i]);')
lines.append('                else if (lang == DE) h.put(RU_[i], DE_[i]);')
lines.append('                else if (lang == JA) h.put(RU_[i], JA_[i]);')
lines.append('                else if (lang == VI) h.put(RU_[i], VI_[i]);')
lines.append('                else h.put(RU_[i], EN_[i]);')
lines.append('            }')
lines.append('            exact = h;')
lines.append('        }')
lines.append('        Object e = exact.get(s);')
lines.append('        if (e != null) return (String) e;')
lines.append('        String[] repl = EN_;')
lines.append('        if (lang == FR) repl = FR_;')
lines.append('        else if (lang == DE) repl = DE_;')
lines.append('        else if (lang == JA) repl = JA_;')
lines.append('        else if (lang == VI) repl = VI_;')
lines.append('        else repl = EN_;')
lines.append('        for (int k = 0; k < RU_.length && cyr(s); k++) {')
lines.append('            String key = RU_[k];')
lines.append('            int p = s.indexOf(key);')
lines.append('            while (p >= 0) {')
lines.append('                int end = p + key.length();')
lines.append('                boolean okL = p == 0 || !letter(key.charAt(0)) || !letter(s.charAt(p - 1));')
lines.append('                boolean okR = end >= s.length() || !letter(key.charAt(key.length() - 1)) || !letter(s.charAt(end));')
lines.append('                if (okL && okR) { s = s.substring(0, p) + repl[k] + s.substring(end); p = s.indexOf(key, p + repl[k].length()); }')
lines.append('                else p = s.indexOf(key, p + 1);')
lines.append('            }')
lines.append('        }')
lines.append('        return s;')
lines.append('    }')
lines.append('')
lines.append('    public static void set(int l) {')
lines.append('        if (l == RU) lang = RU;')
lines.append('        else if (l == FR) lang = FR;')
lines.append('        else if (l == DE) lang = DE;')
lines.append('        else if (l == JA) lang = JA;')
lines.append('        else if (l == VI) lang = VI;')
lines.append('        else lang = EN;')
lines.append('        exact = null;')
lines.append('        cache.clear();')
lines.append('    }')
lines.append('}')

java_path = os.path.join(HERE, '..', 'src', 'core', 'cs', 'Lang.java')
with open(java_path, 'w', encoding='utf-8', newline='\n') as f:
    f.write('\n'.join(lines) + '\n')
print('Lang.java written:', len(RU))

# --- Generate LangData.h ---
h = []
h.append('/* Generated from lang/translations_data.py by lang/build_lang.py */')
h.append('#ifndef LANGDATA_H')
h.append('#define LANGDATA_H')
h.append('#include <e32std.h>')
h.append('')
arrs = [("KRussianText", RU), ("KEnglishText", EN), ("KFrenchText", FR), ("KGermanText", DE), ("KJapaneseText", JA), ("KVietnameseText", VI)]
for aname, arr in arrs:
    h.append(f'static const TText* const {aname}[] = {{')
    for s in arr:
        esc = s.replace('\\', '\\\\').replace('"', '\\"')
        h.append(f'    (const TText*)L"{esc}",')
    h.append('};')
    h.append('')
h.append('#endif // LANGDATA_H')
h_path = os.path.join(HERE, '..', 'src', 'symbian', 'inc', 'LangData.h')
with open(h_path, 'w', encoding='utf-8', newline='\n') as f:
    f.write('\n'.join(h) + '\n')
print('LangData.h written')
