import sys

# adt2text.pas の help_data を C++ のソース (CP437 のバイト列を \xNN で書く) にする。
# SDL 版 (GO32V2 でない)・Windows (UNIX でない) の分岐を選ぶ。
# 使い方: mkhelp.py adt2text.pas 出力.cpp
src, dst = sys.argv[1:3]
data = open(src, 'rb').read()
lines = data.split(b'\n')

at2ver = None
for l in lines:
    if b'at2ver  =' in l:
        at2ver = l.split(b"'")[1]
        break

start = next(i for i, l in enumerate(lines) if l.strip().startswith(b'help_data: array'))
# 前処理: 選ばない分岐を除く
body = []
stack = []   # True = 出力する
for l in lines[start + 1:]:
    s = l.strip()
    if s.startswith(b'{$IFDEF') or s.startswith(b'{$IFNDEF'):
        name = s.split()[1].rstrip(b'}')
        defined = False   # GO32V2 / UNIX / BETA はどれも定義しない
        keep = defined if s.startswith(b'{$IFDEF') else not defined
        stack.append(keep)
        continue
    if s.startswith(b'{$ELSE'):
        stack[-1] = not stack[-1]
        continue
    if s.startswith(b'{$ENDIF'):
        stack.pop()
        continue
    if all(stack):
        body.append(l)
    if s.endswith(b');') and not stack:
        break

text = b'\n'.join(body)
# 要素を読む
items = []
cur = b''
i = 0
in_item = False
while i < len(text):
    c = text[i:i + 1]
    if c == b"'":
        j = i + 1
        lit = b''
        while True:
            if text[j:j + 1] == b"'":
                if text[j + 1:j + 2] == b"'":
                    lit += b"'"
                    j += 2
                    continue
                break
            lit += text[j:j + 1]
            j += 1
        cur += lit
        in_item = True
        i = j + 1
    elif c == b'#':
        j = i + 1
        while text[j:j + 1].isdigit():
            j += 1
        cur += bytes([int(text[i + 1:j])])
        in_item = True
        i = j
    elif text.startswith(b'at2ver', i):
        cur += at2ver
        i += 6
    elif c == b',' or c == b')':
        if in_item:
            items.append(cur)
        cur = b''
        in_item = False
        i += 1
        if c == b')':
            break
    else:
        i += 1

def lit(b):
    out = '"'
    prev_hex = False
    for ch in b:
        if 0x20 <= ch < 0x7f and ch not in (0x22, 0x5c):
            c = chr(ch)
            if prev_hex and c in '0123456789abcdefABCDEF':
                out += '" "'
            out += c
            prev_hex = False
        else:
            out += '\\x%02X' % ch
            prev_hex = True
    return out + '"'

with open(dst, 'w', newline='\r\n', encoding='utf-8') as f:
    f.write('// 本家のヘルプの本文 (adt2text.pas の help_data。SDL 版・Windows の分岐)。CP437 のバイト列。\n')
    f.write('// src/AdLibTrackerModoki/tools/mkhelp.py で adt2text.pas から作る。直接編集しない。\n\n')
    f.write('#include "HelpData.h"\n\nnamespace at2\n{\n\n')
    f.write('const char* const help_data[HELP_LINES] = {\n')
    for it in items:
        f.write('    ' + lit(it) + ',\n')
    f.write('};\n\n} // namespace at2\n')
print(len(items), 'lines')
