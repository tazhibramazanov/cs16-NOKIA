#!/bin/bash
# Single-file PC exe: launcher (zig cc) + packed jre + CS16_PC.jar.
# usage: ./build_exe.sh <dir with jre/ and CS16_PC.jar> <out.exe>
set -e
cd "$(dirname "$0")/launcher"
python3 -m ziglang cc -target x86_64-windows-gnu -O2 -municode -Wl,--subsystem,windows -DXZ_DEC_X86 -Ixz \
  sfx.c xz/xz_crc32.c xz/xz_dec_stream.c xz/xz_dec_lzma2.c xz/xz_dec_bcj.c launcher.rc \
  -o /tmp/cs16_stub.exe -lshell32 -luser32 -lgdi32
cd ..
python3 tools/pack_exe.py /tmp/cs16_stub.exe "$1" "$2"
