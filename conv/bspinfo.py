import os as _os
GAME = _os.environ.get('CS16_GAME', '/mnt/user-data/uploads/ks16')   # folder with cstrike/ and valve/
HERE = _os.path.dirname(_os.path.dirname(_os.path.abspath(__file__))) + '/'

import struct,sys,re
for m in ['de_dust2','de_dust','de_train']:
    d=open(GAME + f'/cstrike/maps/{m}.bsp','rb').read()
    ver=struct.unpack_from('<i',d,0)[0]
    L=[struct.unpack_from('<ii',d,4+i*8) for i in range(15)]
    names='ENT PLANES TEX VERTS VIS NODES TEXINFO FACES LIGHT CLIP LEAVES MARKS EDGES SURFEDGES MODELS'.split()
    print(m,ver,{n:l[1] for n,l in zip(names,L)})
    ent=d[L[0][0]:L[0][0]+L[0][1]].decode('latin1')
    cls={}
    for c in re.findall(r'"classname" "([^"]+)"',ent): cls[c]=cls.get(c,0)+1
    print(cls)
    print(re.findall(r'"wad" "([^"]+)"',ent))
    to,tl=L[2]; n=struct.unpack_from('<i',d,to)[0]; offs=struct.unpack_from('<%di'%n,d,to+4)
    tn=[]
    for o in offs:
        nm,w,h,o0=struct.unpack_from('<16sII I',d,to+o)
        tn.append((nm.split(b'\0')[0].decode(),w,h,o0>0))
    print(len(tn),'textures; embedded:',sum(1 for t in tn if t[3]), 'texels', sum(t[1]*t[2] for t in tn))
    print(' '.join(t[0] for t in tn))
