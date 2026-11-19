#!/bin/bash
set -e
export JAVA_TOOL_OPTIONS=
cd /home/claude/cs
T=/home/claude/tools
CP=$T/cldcapi11-2.0.4.jar:$T/midpapi20-2.0.4.jar
VER=${VER:-0.1.0}
rm -rf build/mcls build/jar && mkdir -p build/mcls build/jar
java -jar $T/ecj-4.6.1.jar -source 1.3 -target 1.3 -nowarn -encoding UTF-8 -bootclasspath $CP -d build/mcls src/core/cs/*.java src/midp/cs/midp/*.java
(cd build/mcls && jar cf ../in.jar .)
cat > build/pg.pro <<P
-injars /home/claude/cs/build/in.jar
-outjars /home/claude/cs/build/out.jar
-libraryjars $CP
-microedition
-target 1.3
-dontoptimize
-overloadaggressively
-repackageclasses ''
-allowaccessmodification
-keep public class cs.midp.CSMidlet
P
java -cp $T/proguard-base-7.4.2.jar:$T/proguard-core-9.1.1.jar:$T/log4j-api-2.19.0.jar:$T/log4j-core-2.19.0.jar:$T/kotlin-stdlib-1.9.0.jar proguard.ProGuard @build/pg.pro > build/pg.log 2>&1 || { tail -30 build/pg.log; exit 1; }
cd build/jar && unzip -q ../out.jar && rm -rf META-INF
cp ../../res/*.cse ../../res/*.nv ../../res/t.pls ../../res/ct.pls ../../res/*.m3 ../../res/*.a3 ../../res/mdltex.bin ../../res/*.fnt ../../res/hud.bin ../../res/icon.png ../../res/sky_*.png ../../res/ui_*.png .
cp ../../res/snd/*.wav .
cat > ../MANIFEST.MF <<M
MIDlet-1: CS 1.6,/icon.png,cs.midp.CSMidlet
MIDlet-Name: CS16E52
MIDlet-Vendor: E52 Edition
MIDlet-Version: $VER
MIDlet-Icon: /icon.png
MicroEdition-Configuration: CLDC-1.1
MicroEdition-Profile: MIDP-2.0
Nokia-MIDlet-Category: Game
M
cd .. && rm -f CS16_E52.jar && (cd jar && zip -q -9 -r ../CS16_E52.jar .) && (mkdir -p mf/META-INF && cp MANIFEST.MF mf/META-INF/MANIFEST.MF && cd mf && zip -q ../CS16_E52.jar META-INF/MANIFEST.MF)
SIZE=$(stat -c %s CS16_E52.jar)
cat > CS16_E52.jad <<J
MIDlet-1: CS 1.6,/icon.png,cs.midp.CSMidlet
MIDlet-Name: CS16E52
MIDlet-Vendor: E52 Edition
MIDlet-Version: $VER
MIDlet-Jar-URL: CS16_E52.jar
MIDlet-Jar-Size: $SIZE
MicroEdition-Configuration: CLDC-1.1
MicroEdition-Profile: MIDP-2.0
Nokia-MIDlet-Category: Game
J
echo "built CS16_E52.jar $SIZE bytes"
