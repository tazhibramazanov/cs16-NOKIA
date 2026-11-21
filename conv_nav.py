import os as _os
GAME = _os.environ.get('CS16_GAME', '/mnt/user-data/uploads/ks16')   # folder with cstrike/ and valve/
HERE = _os.path.dirname(_os.path.dirname(_os.path.abspath(__file__))) + '/'

import struct, sys
def load(path):
    d=open(path,'rb').read(); p=0
    def u32():
        nonlocal p; v=struct.unpack_from('<I',d,p)[0]; p+=4; return v
    def u16():
        nonlocal p; v=struct.unpack_from('<H',d,p)[0]; p+=2; return v
    def u8():
        nonlocal p; v=d[p]; p+=1; return v
    def f32(n):
        nonlocal p; v=struct.unpack_from('<%df'%n,d,p); p+=4*n; return v
    assert u32()==0xFEEDFACE
    ver=u32()
    if ver>=4: u32()
    places=[]
    if ver>=5:
        for i in range(u16()):
            l=u16(); places.append(d[p:p+l].split(b'\0')[0].decode('latin1')); p+=l
    areas=[]
    for i in range(u32()):
        aid=u32(); attr=u8(); lo=f32(3); hi=f32(3); neZ,swZ=f32(2)
        conns=[]
        for dd in range(4):
            conns.append([u32() for _ in range(u32())])
        spots=[]
        for s in range(u8()):
            if ver==1: spots.append(f32(3))
            else: sid=u32(); pos=f32(3); fl=u8(); spots.append(pos)
        for a in range(u8()):
            u32();u32();u8();u32();u8()
        n=u32()
        for e in range(n):
            u32();u8();u32();u8()
            for s in range(u8()): u32();u8()
        place=u16() if ver>=5 else 0
        areas.append(dict(id=aid,attr=attr,lo=lo,hi=hi,neZ=neZ,swZ=swZ,conns=conns,spots=spots,place=place))
    return ver,places,areas,p,len(d)
def write(areas,places,out):
    idx={a['id']:i for i,a in enumerate(areas)}
    b=bytearray(b'NAV1')
    b+=struct.pack('>H',len(places))
    for pl in places:
        e=pl.encode(); b+=struct.pack('>H',len(e))+e
    b+=struct.pack('>H',len(areas))
    for a in areas:
        lo,hi=a['lo'],a['hi']
        b+=struct.pack('>7f',lo[0],lo[1],hi[0],hi[1],lo[2],a['neZ'],hi[2])+struct.pack('>f',a['swZ'])
        b+=struct.pack('>BH',a['attr'],a['place'])
        cs=[(dd,idx[c]) for dd in range(4) for c in a['conns'][dd] if c in idx]
        b+=struct.pack('>B',len(cs))
        for dd,c in cs: b+=struct.pack('>BH',dd,c)
        b+=struct.pack('>B',len(a['spots']))
        for s in a['spots']: b+=struct.pack('>3f',*s)
    open(out,'wb').write(b); return len(b)
if __name__=='__main__':
    for m in sys.argv[2:]:
        ver,places,areas,p,n=load(GAME + f'/cstrike/maps/{m}.nav')
        print(m,'ver',ver,'areas',len(areas),'places',places[:30],'consumed',p,'of',n)
        print(write(areas,places,f'{sys.argv[1]}/{m}.nv'))
