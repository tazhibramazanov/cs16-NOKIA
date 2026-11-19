import os as _os
GAME = _os.environ.get('CS16_GAME', '/mnt/user-data/uploads/ks16')   # folder with cstrike/ and valve/
HERE = _os.path.dirname(_os.path.dirname(_os.path.abspath(__file__))) + '/'

"""Original CS 1.6 GameUI / VGUI assets -> compact PNGs for the 320x240 UI."""
import os, sys
from PIL import Image
sys.path.insert(0, os.path.dirname(__file__))
from conv_snd import conv

CS = GAME + '/cstrike/'
OUT = HERE + 'res/'


def save_q(im, path, colors=255):
    """palettised PNG (keeps 1-bit-ish alpha via a transparent index)"""
    if im.mode == 'RGBA':
        a = im.split()[3]
        rgb = Image.new('RGB', im.size, (0, 0, 0)); rgb.paste(im, mask=a)
        q = rgb.quantize(colors=colors - 1, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
        pal = q.getpalette()[:3 * (colors - 1)] + [255, 0, 255]
        px = q.load(); ap = a.load()
        for y in range(im.size[1]):
            for x in range(im.size[0]):
                if ap[x, y] < 128: px[x, y] = colors - 1
        q.putpalette(pal)
        q.save(path, optimize=True, transparency=colors - 1)
    else:
        q = im.convert('RGB').quantize(colors=colors, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.FLOYDSTEINBERG)
        q.save(path, optimize=True)
    return os.path.getsize(path)


tot = 0
# --- main menu background (1280x720 tiles), cropped to 4:3 around the right-hand figures like CS does at 640x480
bg = Image.new('RGB', (1280, 720))
for i in range(15):
    t = Image.open(CS + 'resource/background/bg_new-%d.tga' % i).convert('RGB')
    bg.paste(t, ((i % 5) * 256, (i // 5) * 256))
crop = bg.crop((1280 - 960, 0, 1280, 720))          # 960x720 = 4:3, keeps both soldiers
tot += save_q(crop.resize((320, 240), Image.LANCZOS), OUT + 'ui_bg.png', 128)
# portrait phone screen (240x320): 540x720 = 3:4 around both soldiers, darkened lower-left behind the menu text
from PIL import ImageDraw
pc = bg.crop((740, 0, 1280, 720)).resize((240, 320), Image.LANCZOS)
sh = Image.new('L', (240, 320), 0); d = ImageDraw.Draw(sh)
for y in range(320):
    a = max(0, min(150, (y - 170) * 2))
    d.line([(0, y), (239, y)], fill=a)
pc = Image.composite(Image.new('RGB', (240, 320), (8, 12, 20)), pc, sh)
tot += save_q(pc, OUT + 'ui_bg_p.png', 128)

# --- "Counter-Strike" logo
lg = Image.open(CS + 'resource/logo_game.tga').convert('RGBA')
lg = lg.crop(lg.getbbox())
lg = lg.resize((lg.width // 2, max(1, lg.height // 2)), Image.LANCZOS)
tot += save_q(lg, OUT + 'ui_logo.png', 32)

# --- buy-menu pictures (VGUI), half size
W = {'glock18': 'glock18', 'usp': 'usp45', 'p228': 'p228', 'deagle': 'deserteagle', 'elite': 'elites', 'fiveseven': 'fiveseven',
     'm3': 'm3', 'xm1014': 'xm1014', 'mac10': 'mac10', 'tmp': 'tmp', 'mp5': 'mp5', 'ump45': 'ump45', 'p90': 'p90',
     'galil': 'galil', 'famas': 'famas', 'ak47': 'ak47', 'm4a1': 'm4a1', 'sg552': 'sg552', 'aug': 'aug', 'scout': 'scout',
     'awp': 'awp', 'g3sg1': 'g3sg1', 'sg550': 'sg550', 'm249': 'm249', 'hegrenade': 'hegrenade', 'flashbang': 'flashbang',
     'smokegrenade': 'smokegrenade', 'kevlar': 'kevlar', 'kevlar_helmet': 'kevlar_helmet', 'defuser': 'defuser'}
for k, f in W.items():
    im = Image.open(CS + 'gfx/vgui/' + f + '.tga').convert('RGBA')
    im = im.crop(im.getbbox())
    s = min(120 / im.width, 56 / im.height, 0.5)
    im = im.resize((max(1, int(im.width * s)), max(1, int(im.height * s))), Image.LANCZOS)
    tot += save_q(im, OUT + 'ui_w_' + k + '.png', 64)

# --- team pictures for the team menu
for k, f in (('t', 't_random'), ('ct', 'ct_random')):
    im = Image.open(CS + 'gfx/vgui/' + f + '.tga').convert('RGBA')
    im = im.crop(im.getbbox())
    s = min(100 / im.height, 0.5)
    im = im.resize((int(im.width * s), int(im.height * s)), Image.LANCZOS)
    tot += save_q(im, OUT + 'ui_team_' + k + '.png', 64)

# --- menu sounds
for k in ('buttonrollover', 'buttonclick'):
    tot += conv(CS + 'resource/UI/' + k + '.wav', OUT + 'snd/ui_' + k + '.wav', 11025, 0.8)
print('ui assets bytes', tot)
