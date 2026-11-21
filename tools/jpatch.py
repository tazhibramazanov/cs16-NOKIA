"""Helpers to replace/insert whole Java methods by signature (brace matching)."""
import sys
def find_method(src, sig):
    i = src.find(sig)
    if i < 0: raise ValueError('signature not found: ' + sig)
    # start of line
    ls = src.rfind('\n', 0, i) + 1
    # include preceding javadoc comment lines directly above
    j = src.find('{', i)
    depth = 0; k = j
    while True:
        c = src[k]
        if src.startswith('//', k):
            k = src.find('\n', k); continue
        if src.startswith('/*', k):
            k = src.find('*/', k) + 2; continue
        if c == '{': depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0: break
        elif c == '"':
            k += 1
            while src[k] != '"':
                if src[k] == '\\': k += 1
                k += 1
        elif c == "'":
            k += 1
            while src[k] != "'":
                if src[k] == '\\': k += 1
                k += 1
        k += 1
    end = src.find('\n', k) + 1
    return ls, end
def replace_method(src, sig, new):
    a, b = find_method(src, sig)
    if not new.endswith('\n'): new += '\n'
    return src[:a] + new + src[b:]
def insert_before(src, sig, new):
    a, b = find_method(src, sig)
    if not new.endswith('\n'): new += '\n'
    return src[:a] + new + src[a:]
