import os as _os
GAME = _os.environ.get('CS16_GAME', '/mnt/user-data/uploads/ks16')   # folder with cstrike/ and valve/
HERE = _os.path.dirname(_os.path.dirname(_os.path.abspath(__file__))) + '/'

import sys; sys.path.insert(0,HERE + 'tools')
import mksis
jar=open('build/CS16_E52.jar','rb').read(); jad=open('build/CS16_E52.jad','rb').read()
d='c:\\data\\cs16e52\\'
n=mksis.build('build/CS16_E52.sisx',0xA000E52C,'CS 1.6 E52 Setup','E52 Edition',(1,0,0),
  [(jar,d+'CS16_E52.jar','',mksis.EOpInstall,0),
   (jad,d+'CS16_E52.jad','text/vnd.sun.j2me.app-descriptor',mksis.EOpRun,mksis.RunInstall|mksis.RunByMime)],
  'cert/cs16e52_key.pem','cert/cs16e52_cert.pem',
  targets=((0x101F7961,'Series60ProductID'),))
print('sisx bytes',n)
r=mksis.verify('build/CS16_E52.sisx','cert/cs16e52_cert.pem'); print(r)
