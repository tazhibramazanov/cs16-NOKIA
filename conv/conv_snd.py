import os as _os
GAME = _os.environ.get('CS16_GAME', '/mnt/user-data/uploads/ks16')   # folder with cstrike/ and valve/
HERE = _os.path.dirname(_os.path.dirname(_os.path.abspath(__file__))) + '/'

import wave, numpy as np, os, sys, glob, struct
SRC=GAME + '/cstrike/sound/'
def conv(src, dst, rate=11025, gain=1.0):
    w=wave.open(src,'rb'); ch,sw,fr,n=w.getnchannels(),w.getsampwidth(),w.getframerate(),w.getnframes()
    raw=w.readframes(n); w.close()
    if sw==1: a=(np.frombuffer(raw,np.uint8).astype(np.float32)-128)/128
    else: a=np.frombuffer(raw,'<i2').astype(np.float32)/32768
    a=a.reshape(-1,ch).mean(1)
    if fr!=rate:
        t=np.arange(0,len(a)*rate/fr)*fr/rate
        a=np.interp(t,np.arange(len(a)),a)
    a=np.clip(a*gain,-1,1)
    b=(a*127+128).astype(np.uint8).tobytes()
    o=wave.open(dst,'wb'); o.setnchannels(1); o.setsampwidth(1); o.setframerate(rate); o.writeframes(b); o.close()
    return len(b)
if __name__=='__main__':
    out=sys.argv[1]; os.makedirs(out,exist_ok=True); tot=0
    for f in sorted(glob.glob(SRC+'*/*.wav')):
        rel=f[len(SRC):]
        nm=rel.replace('/','_').replace('-','_').lower()
        try: tot+=conv(f, os.path.join(out,nm))
        except Exception as e: print('ERR',rel,e)
    # a few sounds live in Half-Life's own folder (valve/), not in cstrike/
    for rel in ('items/ammopickup2.wav', 'items/gunpickup2.wav', 'items/weapondrop1.wav'):
        nm = rel.replace('/', '_')
        f = GAME + '/valve/sound/' + rel
        if not os.path.exists(os.path.join(out, nm)) and os.path.exists(f):
            try: tot += conv(f, os.path.join(out, nm))
            except Exception as e: print('ERR', rel, e)
    print('total bytes',tot, len(glob.glob(out+'/*.wav')),'files')
