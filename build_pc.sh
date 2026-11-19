#!/bin/bash
set -e
export JAVA_TOOL_OPTIONS=
cd "$(dirname "$0")"
rm -rf build/pccls build/pcjar && mkdir -p build/pccls build/pcjar
javac -nowarn --release 11 -encoding UTF-8 -d build/pccls src/core/cs/*.java src/pc/cspc/*.java
cd build/pcjar && cp -r ../pccls/* . && cp ../jar/*.cse ../jar/*.nv ../jar/*.pls ../jar/*.m3 ../jar/*.a3 ../jar/mdltex.bin ../jar/*.fnt ../jar/hud.bin ../jar/icon.png ../jar/sky_*.png ../jar/ui_*.png ../jar/*.wav .
printf "Main-Class: cspc.PCMain\n" > ../pcman.txt
rm -f ../CS16_PC.jar && jar cfm ../CS16_PC.jar ../pcman.txt .
ls -la ../CS16_PC.jar
