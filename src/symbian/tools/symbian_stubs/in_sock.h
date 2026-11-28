#ifndef CS16_STUB_IN_SOCK_H
#define CS16_STUB_IN_SOCK_H

#include "e32std.h"

const TUint32 KInetAddrAny = 0;
const TUint32 KInetAddrLoopback = 0x7F000001u;
const TInt KInetAddrMaxTextLength = 46;

class TInetAddr
    {
public:
    TInetAddr() : iPort(0), iAddress(0) {}
    TInt SetAddress(const TDesC8&) { return KErrNone; }
    TInt SetAddress(TUint32 aAddress) { iAddress = aAddress; return KErrNone; }
    void SetPort(TInt aPort) { iPort = aPort; }
    TInt Port() const { return iPort; }
    TUint32 Address() const { return iAddress; }
    TBool IsUnspecified() const { return iAddress == KInetAddrAny; }
    TInt Family() const { return 1; }
    TDes8& Output(TDes8& aDes) const { return aDes; }
private:
    TInt iPort;
    TUint32 iAddress;
    };

#endif
