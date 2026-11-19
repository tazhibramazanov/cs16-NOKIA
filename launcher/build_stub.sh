#!/bin/sh
# пересобрать launcher/stub.exe (нужен Zig: pip install ziglang)
cd "$(dirname "$0")"
python3 -m ziglang cc -target x86_64-windows-gnu -O2 -municode -Wl,--subsystem,windows -DXZ_DEC_X86 -Ixz \
  sfx.c xz/xz_crc32.c xz/xz_dec_stream.c xz/xz_dec_lzma2.c xz/xz_dec_bcj.c launcher.rc \
  -o stub.exe -lshell32 -luser32 -lgdi32
