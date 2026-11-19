import struct, glob, sys, zlib, numpy as np
from PIL import Image
def parse(d):
    magic=d[:4]; p=4; pal=d[p:p+768]; p+=768; hdr=b''
    if magic==b'PLS1': hdr=d[p:p+8]; p+=8; dirs=8
    else: dirs=1
    na,=struct.unpack_from('>H',d,p); p+=2; anims=[]
    for a in range(na):
        l,=struct.unpack_from('>H',d,p); p+=2; nm=d[p:p+l]; p+=l
        dur,nf=struct.unpack_from('>fH',d,p); p+=6; fr=[]
        for k in range(nf*dirs):
            x,y,w,h=struct.unpack_from('>hhhh',d,p); p+=8
            fr.append((x,y,w,h,np.frombuffer(d,np.uint8,w*h,p).copy())); p+=w*h
        anims.append((nm,dur,nf,fr))
    return magic,pal,hdr,anims
def build(magic,pal,hdr,anims):
    out=bytearray(magic)+pal+hdr+struct.pack('>H',len(anims))
    for nm,dur,nf,fr in anims:
        out+=struct.pack('>H',len(nm))+nm+struct.pack('>fH',dur,nf)
        for x,y,w,h,dd in fr: out+=struct.pack('>hhhh',x,y,w,h)+dd.tobytes()
    return bytes(out)
def requant(path,ncol):
    d=open(path,'rb').read()
    magic,pal,hdr,anims=parse(d)
    P=np.frombuffer(pal,np.uint8).reshape(256,3)
    cnt=np.zeros(256)
    for a in anims:
        for f in a[3]: cnt+=np.bincount(f[4],minlength=256)
    cnt[0]=0; idx=np.nonzero(cnt)[0]
    if len(idx)<=ncol: return
    reps=np.maximum(1,(cnt[idx]/cnt[idx].sum()*30000).astype(int))
    S=np.repeat(P[idx],reps,axis=0)
    side=int(np.ceil(np.sqrt(len(S)))); pad=np.zeros((side*side,3),np.uint8); pad[:len(S)]=S; pad[len(S):]=S[0]
    q=Image.fromarray(pad.reshape(side,side,3)).quantize(colors=ncol,method=Image.Quantize.MEDIANCUT,dither=Image.Dither.NONE)
    np_=np.array(q.getpalette()[:ncol*3],np.uint8).reshape(-1,3)
    dist=((P[:,None,:].astype(int)-np_[None,:,:].astype(int))**2).sum(-1)
    m=(dist.argmin(1)+1).astype(np.uint8); m[0]=0
    newpal=np.zeros((256,3),np.uint8); newpal[1:1+len(np_)]=np_
    anims=[(nm,dur,nf,[(x,y,w,h,m[dd]) for x,y,w,h,dd in fr]) for nm,dur,nf,fr in anims]
    out=build(magic,newpal.tobytes(),hdr,anims)
    open(path,'wb').write(out)
    return len(zlib.compress(d,9)),len(zlib.compress(out,9))
if __name__=='__main__':
    n=int(sys.argv[1]); a=b=0
    for f in sys.argv[2:]:
        r=requant(f,n)
        if r: a+=r[0]; b+=r[1]
    print(a,b)
