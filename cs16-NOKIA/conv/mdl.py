import struct, numpy as np, math
class MDL:
    def __init__(s, path):
        d = s.d = open(path,'rb').read()
        assert d[:4]==b'IDST', path
        h = struct.unpack_from('<27i', d, 136)
        (s.flags, nb, bi, nbc, bci, nhb, hbi, nseq, seqi, nsg, sgi, ntex, texi, texdi, nskinref, nskinfam, skini, nbp, bpi, natt, atti) = h[:21]
        s.bones=[]
        for i in range(nb):
            o=bi+i*112
            name=d[o:o+32].split(b'\0')[0].decode('latin1')
            parent,=struct.unpack_from('<i',d,o+32)
            val=struct.unpack_from('<6f',d,o+64); sc=struct.unpack_from('<6f',d,o+88)
            s.bones.append((name,parent,val,sc))
        s.seqs=[]
        for i in range(nseq):
            o=seqi+i*176
            lab=d[o:o+32].split(b'\0')[0].decode('latin1')
            fps,flags,act,aw,nev,evi,nfr,npiv,pivi,mtype,mbone=struct.unpack_from('<f10i',d,o+32)
            lin=struct.unpack_from('<3f',d,o+76)
            nbl,animi=struct.unpack_from('<2i',d,o+120)
            sg,=struct.unpack_from('<i',d,o+156)
            s.seqs.append(dict(name=lab,fps=fps,frames=nfr,mtype=mtype,mbone=mbone,nbl=nbl,anim=animi,sg=sg,lin=lin,events=(nev,evi)))
        s.textures=[]
        for i in range(ntex):
            o=texi+i*80
            name=d[o:o+64].split(b'\0')[0].decode('latin1')
            fl,w,h,idx=struct.unpack_from('<4i',d,o+64)
            pix=np.frombuffer(d,np.uint8,w*h,idx).reshape(h,w)
            pal=np.frombuffer(d,np.uint8,768,idx+w*h).reshape(256,3)
            s.textures.append(dict(name=name,flags=fl,w=w,h=h,pix=pix,pal=pal))
        s.skinref=np.frombuffer(d,'<i2',nskinref*max(1,nskinfam),skini) if nskinref else None
        s.nskinref=nskinref
        s.bodyparts=[]
        for i in range(nbp):
            o=bpi+i*76
            name=d[o:o+64].split(b'\0')[0].decode('latin1')
            nm,base,mi=struct.unpack_from('<3i',d,o+64)
            models=[]
            for j in range(nm):
                mo=mi+j*112
                mname=d[mo:mo+64].split(b'\0')[0].decode('latin1')
                typ,rad,nmesh,meshi,nv,vii,vi,nn,nii,ni=struct.unpack_from('<if8i',d,mo+64)
                vinfo=np.frombuffer(d,np.uint8,nv,vii)
                verts=np.frombuffer(d,'<f4',nv*3,vi).reshape(-1,3)
                ninfo=np.frombuffer(d,np.uint8,nn,nii)
                norms=np.frombuffer(d,'<f4',nn*3,ni).reshape(-1,3)
                meshes=[]
                for k in range(nmesh):
                    ntri,trii,skin,nnorm,normi=struct.unpack_from('<5i',d,meshi+k*20)
                    tris=[]
                    p=trii
                    while True:
                        c,=struct.unpack_from('<h',d,p); p+=2
                        if c==0: break
                        fan=c<0; c=abs(c)
                        vs=[struct.unpack_from('<4h',d,p+q*8) for q in range(c)]; p+=c*8
                        for q in range(2,c):
                            if fan: t=(vs[0],vs[q-1],vs[q])
                            else: t=(vs[q-2],vs[q-1],vs[q]) if q%2==0 else (vs[q-1],vs[q-2],vs[q])
                            tris.append(t)
                    meshes.append((skin,tris))
                models.append(dict(name=mname,vinfo=vinfo,verts=verts,ninfo=ninfo,norms=norms,meshes=meshes))
            s.bodyparts.append((name,base,models))
        s.attach=[]
        for i in range(natt):
            o=atti+i*88
            bone,=struct.unpack_from('<i',d,o+32+4); org=struct.unpack_from('<3f',d,o+40)
            s.attach.append((bone,org))
    def seq(s,name):
        for i,q in enumerate(s.seqs):
            if q['name'].lower()==name.lower(): return i
        return None
    def anim_value(s, ao, bone, comp, frame):
        val=s.bones[bone][2][comp]; sc=s.bones[bone][3][comp]
        off,=struct.unpack_from('<H',s.d,ao+comp*2)
        if off==0: return val
        p=ao+off
        k=frame
        d=s.d
        while True:
            valid,total=d[p],d[p+1]
            if total<=k:
                k-=total; p+=(valid+1)*2
                if total==0: break
            else: break
        if valid>k: v,=struct.unpack_from('<h',d,p+(k+1)*2)
        else: v,=struct.unpack_from('<h',d,p+valid*2)
        return val+v*sc
    def pose(s, seqi, frame, blend=0):
        q=s.seqs[seqi]
        nb=len(s.bones)
        base=q['anim']+blend*nb*12
        frame=min(int(frame), max(0,q['frames']-1))
        pos=np.zeros((nb,3)); rot=np.zeros((nb,3))
        for b in range(nb):
            ao=base+b*12
            for c in range(3): pos[b,c]=s.anim_value(ao,b,c,frame)
            for c in range(3): rot[b,c]=s.anim_value(ao,b,c+3,frame)
        mb=q['mbone']
        if q['mtype'] & 1: pos[mb,0]=s.bones[mb][2][0]
        if q['mtype'] & 2: pos[mb,1]=s.bones[mb][2][1]
        if q['mtype'] & 4: pos[mb,2]=s.bones[mb][2][2]
        return pos,rot
    def matrices(s,pos,rot):
        nb=len(s.bones); M=np.zeros((nb,3,4))
        for b in range(nb):
            r=quat_mat(angle_quat(rot[b]))
            m=np.zeros((3,4)); m[:,:3]=r; m[:,3]=pos[b]
            par=s.bones[b][1]
            if par<0: M[b]=m
            else:
                P=M[par]; M[b][:,:3]=P[:,:3]@m[:,:3]; M[b][:,3]=P[:,:3]@m[:,3]+P[:,3]
        return M
def angle_quat(a):
    sy,cy=math.sin(a[2]*.5),math.cos(a[2]*.5)
    sp,cp=math.sin(a[1]*.5),math.cos(a[1]*.5)
    sr,cr=math.sin(a[0]*.5),math.cos(a[0]*.5)
    return (sr*cp*cy-cr*sp*sy, cr*sp*cy+sr*cp*sy, cr*cp*sy-sr*sp*cy, cr*cp*cy+sr*sp*sy)
def quat_mat(q):
    x,y,z,w=q
    return np.array([[1-2*y*y-2*z*z, 2*x*y-2*w*z, 2*x*z+2*w*y],
                     [2*x*y+2*w*z, 1-2*x*x-2*z*z, 2*y*z-2*w*x],
                     [2*x*z-2*w*y, 2*y*z+2*w*x, 1-2*x*x-2*y*y]])
