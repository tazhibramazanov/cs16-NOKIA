#ifndef CS16_NETWORK_H
#define CS16_NETWORK_H

/*
 * Network.h -- сетевой слой на RSocketServ/RSocket.
 * Протокол тот же, что в src/core/cs/Net.java:
 *   u16 length (type + payload), u8 type, payload.
 * Дополнительно: UDP-поиск LAN-серверов и HTTP-запросы к MQTT-брокеру
 * комнат (MqttLink.java).
 */

#include "Engine.h"

#include <es_sock.h>
#include <in_sock.h>

const TInt KNetProtocol = 1;
const TInt KNetPort = 27016;
const TInt KNetMaxFrame = 8192;
const TInt KNetBufferSize = 16384;
const TInt KNetMaxInbox = 64;

enum TNetPacketType
    {
    EClientJoin = 1,
    EClientToHost = 2,
    EClientToAll = 3,
    EClientToOne = 4,
    EClientPing = 5,
    EServerWelcome = 10,
    EServerPeerJoin = 11,
    EServerPeerLeft = 12,
    EServerHost = 13,
    EServerRoom = 14,
    EServerPong = 15,
    EServerError = 16,
    EServerFromClient = 20,
    EServerFromHost = 21
    };

/* Пишущий буфер Net.Buf: без аллокаций, с фиксированным лимитом. */
class TNetBuffer
    {
public:
    TNetBuffer();
    void Reset();
    TNetBuffer& U8(TInt aValue);
    TNetBuffer& U16(TInt aValue);
    TNetBuffer& I32(TUint32 aValue);
    TNetBuffer& I16Float(TReal32 aValue);
    TNetBuffer& Raw(const TUint8* aData, TInt aLength);
    TNetBuffer& Str(const TDesC8& aText);
    TNetBuffer& Str(const TDesC& aText);
    TInt Length() const { return iLength; }
    const TUint8* Data() const { return iData; }

private:
    TUint8 iData[KNetBufferSize];
    TInt iLength;
    };

/* Читающий буфер Net.Rd. */
class TNetReader
    {
public:
    TNetReader(const TUint8* aData, TInt aLength);
    TBool More() const { return iPosition < iLength; }
    TInt U8();
    TInt U16();
    TInt I16();
    TUint32 I32();
    TReal32 Float16();
    TPtrC8 Str();
    TInt Position() const { return iPosition; }

private:
    const TUint8* iData;
    TInt iLength;
    TInt iPosition;
    };

class CNetConnection
    {
public:
    static CNetConnection* NewL(RSocketServ& aSocketServer);
    ~CNetConnection();

    void ConnectL(const TDesC8& aHost, TInt aPort);
    void AcceptL(TInt aPort);
    void Send(TInt aType, const TNetBuffer& aPayload);
    TBool Poll(TUint8* aFrame, TInt& aLength);
    void KeepAlive();
    void Close();

    TBool Connected() const { return iConnected; }
    TInt Error() const { return iError; }
    const TDesC& ErrorText() const { return iErrorText; }
    TInt SentBytes() const { return iSentBytes; }
    TInt ReceivedBytes() const { return iReceivedBytes; }
    TUint32 LastSendTime() const { return iLastSend; }
    TUint32 LastReceiveTime() const { return iLastReceive; }

private:
    CNetConnection(RSocketServ& aSocketServer);
    void ReadMoreL();
    void PumpL();

private:
    RSocketServ& iSocketServer;
    RSocket iSocket;
    RSocket iListener;
    TBool iConnected;
    TInt iError;
    HBufC* iErrorTextBuffer;
    TPtr iErrorText;
    TUint32 iLastSend;
    TUint32 iLastReceive;
    TInt iSentBytes;
    TInt iReceivedBytes;
    TUint8 iInput[KNetBufferSize + 4];
    TInt iInputLength;
    TUint8 iFrame[KNetMaxInbox][KNetBufferSize + 2];
    TInt iFrameLength[KNetMaxInbox];
    TInt iFrameRead;
    TInt iFrameWrite;
    };

/* UDP-широковещательный поиск серверов в LAN. */
class CLanFinder
    {
public:
    static CLanFinder* NewL(RSocketServ& aSocketServer);
    ~CLanFinder();
    void StartL(TInt aPort);
    void Stop();
    TBool Poll(TInetAddr& aAddress, TPtrC8& aPayload);

private:
    CLanFinder(RSocketServ& aSocketServer);
    RSocketServ& iSocketServer;
    RSocket iSocket;
    TInetAddr iSender;
    TUint8 iBuffer[512];
    };

/* Простой HTTP-клиент для списка комнат MqttLink. */
class CHttpClient
    {
public:
    static CHttpClient* NewL(RSocketServ& aSocketServer);
    ~CHttpClient();
    TInt Request(const TDesC8& aHost, TInt aPort, const TDesC8& aPath,
                 TPtrC8& aBody, TUint8* aBuffer, TInt aBufferSize);

private:
    CHttpClient(RSocketServ& aSocketServer);
    RSocketServ& iSocketServer;
    RSocket iSocket;
    TInetAddr iAddress;
    TUint8 iReceiveBuffer[4096];
    };

#endif
