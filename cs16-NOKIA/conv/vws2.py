import struct, zlib, sys, glob, os, numpy as np
def read(path):
    d=open(path,'rb').read(); p=4
    pal=d[p:p+768]; p+=768
    na,=struct.unpack_from('>H',d,p); p+=2
    anims=[]
    for a in range(na):
        l,=struct.unpack_from('>H',d,p); p+=2; nm=d[p:p+l].decode(); p+=l
        dur,nf=struct.unpack_from('>fH',d,p); p+=6
        fr=[]
        for k in range(nf):
            x,y,w,h=struct.unpack_from('>hhhh',d,p); p+=8
            data=np.frombuffer(d,np.uint8,w*h,p).reshape(h,w); p+=w*h
            fr.append((x,y,w,h,data))
        anims.append((nm,dur,fr))
    return pal,anims
def subsample(fr, keep):
    if len(fr)<=keep: return fr
    idx=[round(i*(len(fr)-1)/(keep-1)) for i in range(keep)]
    return [fr[i] for i in idx]
LIMIT={'reload':6,'draw':3,'shoot':4,'shoot2':3,'insert':3,'pullpin':3,'attach':5,'detach':5,'rafter':2,'rstart':2}
def write(path, pal, anims):
    out=bytearray(b'VWS2')+pal+struct.pack('>H',len(anims))
    for nm,dur,fr in anims:
        fr=subsample(fr, LIMIT.get(nm,99))
        b=nm.encode(); out+=struct.pack('>H',len(b))+b+struct.pack('>fH',dur,len(fr))
        prev=None
        for (x,y,w,h,data) in fr:
            out+=struct.pack('>hhhh',x,y,w,h)
            if prev is None:
                out+=b'\0'+data.tobytes()
            else:
                px,py,pw,ph,pd=prev
                canvas=np.zeros((240,320),np.uint8)
                canvas[py:py+ph,px:px+pw]=pd
                ref=canvas[y:y+h,x:x+w]
                out+=b'\1'+(data^ref).tobytes()
            prev=(x,y,w,h,data)
    open(path,'wb').write(out)
    return out
if __name__=='__main__':
    tot0=tot1=0
    for f in sorted(glob.glob(sys.argv[1]+'/v_*.vws')):
        d0=open(f,'rb').read()
        if d0[:4]!=b'VWS1': continue
        pal,an=read(f)
        out=write(f, pal, an)
        a=len(zlib.compress(d0,9)); b=len(zlib.compress(bytes(out),9)); tot0+=a; tot1+=b
    print('compressed before',tot0,'after',tot1)
