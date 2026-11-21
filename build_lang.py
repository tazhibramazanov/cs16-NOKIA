"""ru_en.py -> src/core/cs/Lang.java (run after editing the dictionary)"""
import os, sys
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
from ru_en import D
def j(s): return '"' + s.replace('\\', '\\\\').replace('"', '\\"') + '"'
D = dict(D)
for k, v in list(D.items()):          # settings rows are drawn as "label" + value: the label alone must translate too
    if ': ' in k and ': ' in v:
        lk, lv = k.split(': ')[0].strip(), v.split(': ')[0].strip()
        if lk and lk not in D: D[lk] = lv
keys = sorted(D, key=lambda k: (-len(k), k))          # longest first: whole phrases before their words
body = ''.join('        %s,\n' % j(k) for k in keys)
eng = ''.join('        %s,\n' % j(D[k]) for k in keys)
src = '''package cs;

import java.util.Hashtable;
import java.util.Vector;

/**
 * Game language. The texts in the code are Russian; in English every line is translated on its way to the screen
 * (Gfx.text/textW), so even lines glued together at run time ("Наблюдение: " + name + " (5 — следующий)") come out whole.
 * Generated from lang/ru_en.py by lang/build_lang.py.
 */
public final class Lang {
    public static final int EN = 0, RU = 1;
    public static int lang = EN;

    static final String[] RU_ = {
%s    };
    static final String[] EN_ = {
%s    };

    static Hashtable exact;
    static final Hashtable cache = new Hashtable();
    /** tests: lines that still have Russian letters after translation */
    public static Vector missing;

    public static String t(String s) {
        if (lang == RU || s == null || !cyr(s)) return s;
        Object c = cache.get(s);
        if (c != null) return (String) c;
        String r = translate(s);
        if (cache.size() > 600) cache.clear();
        cache.put(s, r);
        if (missing != null && cyr(r) && !missing.contains(s)) missing.addElement(s);
        return r;
    }

    static boolean cyr(String s) {
        for (int i = s.length() - 1; i >= 0; i--) { char ch = s.charAt(i); if (ch >= 0x400 && ch <= 0x4FF) return true; }
        return false;
    }

    static boolean letter(char ch) { return ch >= 0x400 && ch <= 0x4FF; }

    static String translate(String s) {
        if (exact == null) { Hashtable h = new Hashtable(); for (int i = 0; i < RU_.length; i++) h.put(RU_[i], EN_[i]); exact = h; }
        Object e = exact.get(s);
        if (e != null) return (String) e;
        for (int k = 0; k < RU_.length && cyr(s); k++) {
            String key = RU_[k];
            int p = s.indexOf(key);
            while (p >= 0) {
                int end = p + key.length();
                boolean okL = p == 0 || !letter(key.charAt(0)) || !letter(s.charAt(p - 1));
                boolean okR = end >= s.length() || !letter(key.charAt(key.length() - 1)) || !letter(s.charAt(end));
                if (okL && okR) { s = s.substring(0, p) + EN_[k] + s.substring(end); p = s.indexOf(key, p + EN_[k].length()); }
                else p = s.indexOf(key, p + 1);
            }
        }
        return s;
    }

    public static void set(int l) { lang = l == RU ? RU : EN; cache.clear(); }
}
''' % (body, eng)
open(os.path.join(HERE, '..', 'src', 'core', 'cs', 'Lang.java'), 'w', encoding='utf-8').write(src)
print('Lang.java:', len(keys), 'phrases')
