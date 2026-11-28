#ifndef CS16_STUB_ES_SOCK_H
#define CS16_STUB_ES_SOCK_H

#include "e32std.h"
#include "e32base.h"
#include "in_sock.h"

const TUint KAfInet = 1;
const TUint KAfInet6 = 2;
const TUint KSockStream = 1;
const TUint KSockDatagram = 2;
const TUint KSockSeqPacket = 3;
const TUint KProtocolInetTcp = 6;
const TUint KProtocolInetUdp = 17;
const TUint KProtocolInetIcmp = 1;
const TUint KSOLSocket = 0x0000;
const TUint KSOLInetIp = 0x0002;
const TUint KSOBroadcast = 0x0006;
const TUint KSOReuseAddr = 0x0008;
const TUint KSONodeAddr = 0x0012;

class RSocketServ
    {
public:
    RSocketServ() : iHandle(0) {}
    TInt Connect() { iHandle = 1; return KErrNone; }
    void Close() { iHandle = 0; }
    TInt Handle() const { return iHandle; }
private:
    TInt iHandle;
    };

class RSocket
    {
public:
    RSocket() : iHandle(0) {}
    TInt Open(RSocketServ&, TUint, TUint, TUint) { iHandle = 1; return KErrNone; }
    TInt Open(RSocketServ&, TUint, TUint) { iHandle = 1; return KErrNone; }
    TInt Connect(const TInetAddr&) { return KErrNone; }
    TInt Bind(const TInetAddr&) { return KErrNone; }
    TInt Listen(TInt) { return KErrNone; }
    TInt Accept(RSocket&) { return KErrNone; }
    TInt Send(const TDesC8&, TUint) { return KErrNone; }
    TInt Recv(TDes8&, TUint, TRequestStatus&) { return KErrNone; }
    TInt RecvFrom(TDes8&, TInetAddr&, TUint, TRequestStatus&) { return KErrNone; }
    void CancelRecv() {}
    TInt SetOpt(TUint, TUint, TInt) { return KErrNone; }
    TInt GetOpt(TUint, TUint, TInt&) { return KErrNone; }
    TInt LocalName(TInetAddr&) { return KErrNone; }
    TInt RemoteName(TInetAddr&) { return KErrNone; }
    void Close() { iHandle = 0; }
    TInt Handle() const { return iHandle; }
private:
    TInt iHandle;
    };

struct TNameRecord
    {
    TInetAddr iAddr;
    TNameRecord* iNext;
    TInt iGenerationNumber;
    TNameRecord() : iNext(0), iGenerationNumber(0) {}
    };

class CResolver : public CBase
    {
public:
    static CResolver* NewL(RSocketServ& aServer) { (void)aServer; return new (ELeave) CResolver(); }
    static CResolver* NewLC(RSocketServ& aServer) { return NewL(aServer); }
    void GetByName(const TDesC8&, TNameRecord&, TRequestStatus&) {}
    void GetByAddress(const TInetAddr&, TNameRecord&, TRequestStatus&) {}
    void Cancel(TRequestStatus&) {}
private:
    CResolver() {}
    };

#endif
