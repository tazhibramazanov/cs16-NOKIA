import os as _os
GAME = _os.environ.get('CS16_GAME', '/mnt/user-data/uploads/ks16')   # folder with cstrike/ and valve/
HERE = _os.path.dirname(_os.path.dirname(_os.path.abspath(__file__))) + '/'

import struct, numpy as np, sys, re
from PIL import Image, ImageFont, ImageDraw
SP=GAME + '/cstrike/sprites/'
def spr(path):
    d=open(path,'rb').read()
    assert d[:4]==b'IDSP'
    ver,typ,fmt,rad,mw,mh,nf,beam,sync=struct.unpack_from('<iiifiiifi',d,4)
    p=40; npal,=struct.unpack_from('<h',d,p); p+=2
    pal=np.frombuffer(d,np.uint8,npal*3,p).reshape(-1,3); p+=npal*3
    frames=[]
    for i in range(nf):
        grp,ox,oy,w,h=struct.unpack_from('<5i',d,p); p+=20
        pix=np.frombuffer(d,np.uint8,w*h,p).reshape(h,w); p+=w*h
        frames.append(pal[pix])
    return frames, fmt
def hudlist():
    els={}
    for line in open(SP+'hud.txt'):
        t=line.split()
        if len(t)==7 and t[1]=='640': els[t[0]]=(t[2],int(t[3]),int(t[4]),int(t[5]),int(t[6]))
    return els
NEED=['number_%d'%i for i in range(10)]+['cross','dollar','suit_full','suit_empty','suithelmet_full','stopwatch','c4','defuser','buyzone','divider','minus','plus','d_headshot','d_skull','d_grenade']+\
 ['d_'+w for w in 'knife ak47 awp deagle famas fiveseven flashbang g3sg1 galil glock18 m249 m3 m4a1 mp5navy p228 p90 scout sg550 sg552 ump45 usp tmp xm1014 aug mac10 elite'.split()]
def main(out):
    els=hudlist(); cache={}
    b=bytearray(b'HUD1'); recs=[]
    for n in NEED:
        sp,x,y,w,h=els[n]
        if sp not in cache: cache[sp]=spr(SP+sp+'.spr')[0][0]
        img=cache[sp][y:y+h,x:x+w].astype(np.float32).max(-1)
        im=Image.fromarray(img.astype(np.uint8)).resize((max(1,round(w/2)),max(1,round(h/2))),Image.LANCZOS)
        a=np.array(im)
        recs.append((n,a))
    b+=struct.pack('>H',len(recs))
    for n,a in recs:
        e=n.encode(); b+=struct.pack('>H',len(e))+e+struct.pack('>HH',a.shape[1],a.shape[0])+a.tobytes()
    open(out+'/hud.bin','wb').write(b)
    print('hud',len(b))
    # fonts
    chars=[chr(c) for c in range(32,127)]+[chr(c) for c in range(0x410,0x450)]+['Ё','ё','№','—','«','»','•','×']
    for name,path,size in [('f1','/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf',10),('f2','/usr/share/fonts/truetype/dejavu/DejaVuSansCondensed-Bold.ttf',15)]:
        f=ImageFont.truetype(path,size)
        asc,desc=f.getmetrics(); H=asc+desc
        fb=bytearray(b'FNT1')+struct.pack('>HH',H,len(chars))
        for ch in chars:
            w=max(1,int(round(f.getlength(ch))))
            im=Image.new('L',(w+2,H)); ImageDraw.Draw(im).text((0,0),ch,font=f,fill=255)
            a=np.array(im)
            fb+=struct.pack('>HB',ord(ch),w+1)+a[:,:w+1].tobytes()
        open(out+'/%s.fnt'%name,'wb').write(fb); print(name,H,len(fb))
    # preview
    pv=Image.new('L',(300,60)); x=0
    for n,a in recs[:20]:
        pv.paste(Image.fromarray(a),(x,0)); x+=a.shape[1]+2
    x=0
    for n,a in recs[20:]:
        if x+a.shape[1]>300: break
        pv.paste(Image.fromarray(a),(x,30)); x+=a.shape[1]+2
    pv.resize((900,180),Image.NEAREST).save('../shots/hud.png')
main(sys.argv[1])
