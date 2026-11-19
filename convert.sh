#!/bin/bash
# Converts the original Counter-Strike 1.6 files into the phone formats (res/).
# usage: CS16_GAME=/path/to/folder/with/cstrike/and/valve ./convert.sh
set -e
cd "$(dirname "$0")"
: "${CS16_GAME:?set CS16_GAME to the folder that contains cstrike/ and valve/}"
export CS16_GAME
mkdir -p res/snd
python3 conv/conv_map.py res de_dust2 de_dust de_train
python3 conv/conv_nav.py res de_dust2 de_dust de_train
python3 conv/conv_snd.py res/snd
python3 conv/conv_hud.py res
python3 conv/conv_mdl3d.py
python3 conv/conv_ui.py
python3 conv/bake.py res pl:terror:ak47:t pl:gign:m4a1:ct
python3 conv/bake.py res world
echo "res/ готово"
