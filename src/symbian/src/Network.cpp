/*
 * Network.cpp -- сетевой слой нативного движка CS16_E52.
 *
 * Протокол полностью повторяет src/core/cs/Net.java:
 *   u16 big-endian длина (type + payload), u8 type, payload.
 * Дополнительно: UDP-маяки LanFinder/LanServer и plain TCP HTTP/1.0 GET
 * для списка комнат MQTT (MqttLink.java).
 *
 * Асинхронность без активного объекта: все приёмы делаются через
 * RSocket::Recv/RecvFrom с TRequestStatus, после выдачи запроса вызывается
 * User::After(0) (несколько раз), и если за это время данные не пришли,
 * запрос отменяется CancelRecv(). Так Poll() не блокирует UI-поток и при
 * этом не требует CActive/RTimer и аллокаций. TCP Connect/Accept остаются
 * синхронными (как в Java): ConnectL/AcceptL вызываются один раз при
 * установке соединения, а не в игровом кадре.
 */

#include "Network.h"

#include <e32std.h>
#include <es_sock.h>
#include <in_sock.h>


/* Возвращается вместо "нет данных сейчас" (recv отменён). */
const TInt KNetNoData = -1001;
const TInt KNetReadYields = 4;
const TInt KNetPollReads = 8;
const TInt KNetHttpAttempts = 200;
const TInt KNetHttpIdleAttempts = 40;
const TInt KNetHttpWaitTicks = 2;
const TInt KNetHttpWaitMs = 5;

/* Пустая строка до выделения iErrorTextBuffer в NewL. */
static TUint16 gEmptyErrorText[1] = { 0 };


static void SetNetError(TInt& aError, TPtr& aText, TInt aCode, const TDesC& aMessage)
    {
    aError = aCode;
    aText.Copy(aMessage);
    }

static void ClearNetError(TInt& aError, TPtr& aText)
    {
    aError = KErrNone;
    aText.SetLength(0);
    }

/*
 * Числовой адрес разбираем сами, hostname -- через CResolver.
 * GetByName асинхронный; здесь это допустимо, т.к. вызывается только из
 * ConnectL/Request, а не из Poll/Send.
 */
static TInt ResolveNetHost(RSocketServ& /*aServer*/, const TDesC8& aHost,
                           TInt aPort, TInetAddr& aAddress)
    {
    if (aHost.Length() == 0) return KErrArgument;
    TBuf<64> hostBuf;
    if (aHost.Length() > hostBuf.MaxLength()) return KErrArgument;
    hostBuf.Copy(aHost);
    aAddress.Init(KAfInet);
    TInt error = KErrNone;
    TRAP(error, aAddress.Input(hostBuf));
    if (error != KErrNone) return error;
    aAddress.SetPort(aPort);
    return KErrNone;
    }

/*
 * Один неблокирующий приём. >0 -- байты, 0 -- конец потока,
 * KNetNoData -- сейчас ничего нет, остальные отрицательные -- ошибка.
 */
static TInt NetRecvSome(RSocket& aSocket, TDes8& aBuffer)
    {
    TRequestStatus status;
    aSocket.Recv(aBuffer, 0, status);
    for (TInt i = 0; i < KNetReadYields && status == KRequestPending; i++)
        User::After(0);
    if (status == KRequestPending)
        {
        aSocket.CancelRecv();
        User::WaitForRequest(status);
        return KNetNoData;
        }
    return status.Int();
    }

static TInt NetRecvFromSome(RSocket& aSocket, TDes8& aBuffer, TInetAddr& aAddress)
    {
    TRequestStatus status;
    aSocket.RecvFrom(aBuffer, aAddress, 0, status);
    for (TInt i = 0; i < KNetReadYields && status == KRequestPending; i++)
        User::After(0);
    if (status == KRequestPending)
        {
        aSocket.CancelRecv();
        User::WaitForRequest(status);
        return KNetNoData;
        }
    return status.Int();
    }

/* HTTP не в игровом кадре: здесь можно подождать несколько миллисекунд. */
static TInt NetRecvHttp(RSocket& aSocket, TDes8& aBuffer)
    {
    TRequestStatus status;
    aSocket.Recv(aBuffer, 0, status);
    for (TInt i = 0; i < KNetHttpWaitTicks && status == KRequestPending; i++)
        User::After(KNetHttpWaitMs * 1000);
    if (status == KRequestPending)
        {
        aSocket.CancelRecv();
        User::WaitForRequest(status);
        return KNetNoData;
        }
    return status.Int();
    }

/* Запись кадра в поток: u16 length, u8 type, payload. */
static TInt NetWriteFrame(RSocket& aSocket, TInt aType,
                          const TUint8* aData, TInt aLength)
    {
    if (aLength < 0 || aLength > KNetMaxFrame - 1) return KErrArgument;
    TInt frameLength = aLength + 1;
    TUint8 header[3];
    header[0] = (TUint8)(frameLength >> 8);
    header[1] = (TUint8)frameLength;
    header[2] = (TUint8)aType;
    TSockXfrLength sent;
    TRequestStatus status;
    aSocket.Send(TPtrC8(header, 3), 0, status, sent);
    User::WaitForRequest(status);
    TInt error = status.Int();
    if (error != KErrNone) return error;
    if (aLength > 0)
        {
        aSocket.Send(TPtrC8(aData, aLength), 0, status, sent);
        User::WaitForRequest(status);
        error = status.Int();
        }
    return error;
    }


// ---------------------------------------------------------------- TNetBuffer

TNetBuffer::TNetBuffer() : iLength(0)
    {
    }

void TNetBuffer::Reset()
    {
    iLength = 0;
    }

TNetBuffer& TNetBuffer::U8(TInt aValue)
    {
    if (iLength < KNetBufferSize)
        iData[iLength++] = (TUint8)aValue;
    return *this;
    }

TNetBuffer& TNetBuffer::U16(TInt aValue)
    {
    if (iLength <= KNetBufferSize - 2)
        {
        iData[iLength++] = (TUint8)((TUint)aValue >> 8);
        iData[iLength++] = (TUint8)aValue;
        }
    return *this;
    }

TNetBuffer& TNetBuffer::I32(TUint32 aValue)
    {
    U16((TInt)(aValue >> 16));
    U16((TInt)(aValue & 0xFFFFu));
    return *this;
    }

TNetBuffer& TNetBuffer::I16Float(TReal32 aValue)
    {
    /* Net.Buf.i16(): round half away from zero, then clamp to signed 16 bit. */
    TInt value = (TInt)(aValue < 0 ? aValue - 0.5f : aValue + 0.5f);
    if (value > 32767) value = 32767;
    if (value < -32768) value = -32768;
    return U16(value);
    }

TNetBuffer& TNetBuffer::Raw(const TUint8* aData, TInt aLength)
    {
    if (aData == 0 || aLength <= 0 || iLength >= KNetBufferSize) return *this;
    TInt count = aLength;
    if (count > KNetBufferSize - iLength) count = KNetBufferSize - iLength;
    Mem::Copy(iData + iLength, aData, count);
    iLength += count;
    return *this;
    }

TNetBuffer& TNetBuffer::Str(const TDesC8& aText)
    {
    if (iLength >= KNetBufferSize) return *this;
    TInt count = aText.Length();
    if (count > 255) count = 255;                  /* Java: u8 length prefix */
    if (count > KNetBufferSize - iLength - 1) count = KNetBufferSize - iLength - 1;
    iData[iLength++] = (TUint8)count;
    if (count > 0) Mem::Copy(iData + iLength, aText.Ptr(), count);
    iLength += count;
    return *this;
    }

TNetBuffer& TNetBuffer::Str(const TDesC& aText)
    {
    /* Кодируем UTF-8 прямо в буфер, чтобы не выделять HBufC8. */
    if (iLength >= KNetBufferSize) return *this;
    TUint8* target = iData + iLength + 1;
    TInt limit = KNetBufferSize - iLength - 1;
    if (limit > 255) limit = 255;
    TInt written = 0;
    TInt position = 0;

    while (position < aText.Length() && written < limit)
        {
        TUint code = (TUint)aText[position++];
        if (code >= 0xD800 && code <= 0xDBFF && position < aText.Length())
            {
            TUint low = (TUint)aText[position];
            if (low >= 0xDC00 && low <= 0xDFFF)
                {
                position++;
                TUint scalar = 0x10000u + ((code - 0xD800) << 10) + (low - 0xDC00);
                if (written + 4 > limit) break;
                target[written++] = (TUint8)(0xF0u | (scalar >> 18));
                target[written++] = (TUint8)(0x80u | ((scalar >> 12) & 0x3Fu));
                target[written++] = (TUint8)(0x80u | ((scalar >> 6) & 0x3Fu));
                target[written++] = (TUint8)(0x80u | (scalar & 0x3Fu));
                continue;
                }
            }
        if (code < 0x80u)
            {
            if (written + 1 > limit) break;
            target[written++] = (TUint8)code;
            }
        else if (code < 0x800u)
            {
            if (written + 2 > limit) break;
            target[written++] = (TUint8)(0xC0u | (code >> 6));
            target[written++] = (TUint8)(0x80u | (code & 0x3Fu));
            }
        else
            {
            if (written + 3 > limit) break;
            target[written++] = (TUint8)(0xE0u | (code >> 12));
            target[written++] = (TUint8)(0x80u | ((code >> 6) & 0x3Fu));
            target[written++] = (TUint8)(0x80u | (code & 0x3Fu));
            }
        }

    iData[iLength] = (TUint8)written;
    iLength += 1 + written;
    return *this;
    }


// ---------------------------------------------------------------- TNetReader

TNetReader::TNetReader(const TUint8* aData, TInt aLength)
    : iData(aData), iLength(aLength < 0 ? 0 : aLength), iPosition(0)
    {
    }

TInt TNetReader::U8()
    {
    if (iData == 0 || iPosition >= iLength) return 0;
    return (TInt)iData[iPosition++];
    }

TInt TNetReader::U16()
    {
    TInt high = U8();
    TInt low = U8();
    return (high << 8) | low;
    }

TInt TNetReader::I16()
    {
    TInt value = U16();
    return (value & 0x8000) != 0 ? value - 0x10000 : value;
    }

TUint32 TNetReader::I32()
    {
    TInt high = U16();
    TInt low = U16();
    return ((TUint32)high << 16) | (TUint32)low;
    }

TReal32 TNetReader::Float16()
    {
    /* Net.Buf.i16(float) -- это округлённое целое, а не масштабированное. */
    return (TReal32)I16();
    }

TPtrC8 TNetReader::Str()
    {
    TInt count = U8();
    if (iData == 0 || iPosition >= iLength) return TPtrC8((const TUint8*)"", 0);
    if (count > iLength - iPosition) count = iLength - iPosition;
    TPtrC8 result(iData + iPosition, count);
    iPosition += count;
    return result;
    }


// ------------------------------------------------------------ CNetConnection

CNetConnection::CNetConnection(RSocketServ& aSocketServer)
    : iSocketServer(aSocketServer), iConnected(EFalse), iError(KErrNone),
      iErrorTextBuffer(NULL), iErrorText(gEmptyErrorText, 0),
      iLastSend(0), iLastReceive(0), iSentBytes(0), iReceivedBytes(0),
      iInputLength(0), iFrameRead(0), iFrameWrite(0)
    {
    Mem::FillZ(iInput, sizeof(iInput));
    Mem::FillZ(iFrame, sizeof(iFrame));
    Mem::FillZ(iFrameLength, sizeof(iFrameLength));
    }

CNetConnection* CNetConnection::NewL(RSocketServ& aSocketServer)
    {
    CNetConnection* self = new (ELeave) CNetConnection(aSocketServer);
    CleanupStack::PushL(self);
    self->iErrorTextBuffer = HBufC::NewL(192);
    {
    TPtr p = self->iErrorTextBuffer->Des();
    self->iErrorText.Set(p);
    }
    CleanupStack::Pop(self);
    return self;
    }

CNetConnection::~CNetConnection()
    {
    Close();
    delete iErrorTextBuffer;
    }

void CNetConnection::ConnectL(const TDesC8& aHost, TInt aPort)
    {
    Close();
    ClearNetError(iError, iErrorText);

    TInetAddr address;
    TInt error = ResolveNetHost(iSocketServer, aHost, aPort, address);
    if (error != KErrNone)
        {
        SetNetError(iError, iErrorText, error, _L("Не удалось определить адрес сервера"));
        User::Leave(error);
        }

    error = iSocket.Open(iSocketServer, KAfInet, KSockStream, KProtocolInetTcp);
    if (error != KErrNone)
        {
        SetNetError(iError, iErrorText, error, _L("Не удалось открыть сокет"));
        User::Leave(error);
        }

    {
    TRequestStatus status;
    iSocket.Connect(address, status);
    User::WaitForRequest(status);
    error = status.Int();
    }
    if (error != KErrNone)
        {
        SetNetError(iError, iErrorText, error, _L("Не удалось подключиться к серверу"));
        iSocket.Close();
        User::Leave(error);
        }

    iConnected = ETrue;
    iLastReceive = User::NTickCount();
    iLastSend = iLastReceive;
    }

void CNetConnection::AcceptL(TInt aPort)
    {
    Close();
    ClearNetError(iError, iErrorText);

    TInt error = iListener.Open(iSocketServer, KAfInet, KSockStream, KProtocolInetTcp);
    if (error != KErrNone)
        {
        SetNetError(iError, iErrorText, error, _L("Не удалось открыть сокет"));
        User::Leave(error);
        }
    TInt one = 1;
    iListener.SetOpt(KSOLSocket, KSoReuseAddr, one);

    TInetAddr address;
    address.Init(KAfInet);
    address.SetPort(aPort);
    error = iListener.Bind(address);
    if (error == KErrNone) error = iListener.Listen(4);
    if (error != KErrNone)
        {
        SetNetError(iError, iErrorText, error, _L("Порт занят или недоступен"));
        iListener.Close();
        User::Leave(error);
        }

    error = iSocket.Open(iSocketServer, KAfInet, KSockStream, KProtocolInetTcp);
    if (error != KErrNone)
        {
        SetNetError(iError, iErrorText, error, _L("Не удалось открыть сокет"));
        iListener.Close();
        User::Leave(error);
        }

    /* Ждём клиента (аналог Listener.accept() в LanServer.java). */
    {
    TRequestStatus status;
    iListener.Accept(iSocket, status);
    User::WaitForRequest(status);
    error = status.Int();
    }
    if (error != KErrNone)
        {
        SetNetError(iError, iErrorText, error, _L("Не удалось принять подключение"));
        iSocket.Close();
        iListener.Close();
        User::Leave(error);
        }

    iListener.Close();
    iConnected = ETrue;
    iLastReceive = User::NTickCount();
    iLastSend = iLastReceive;
    }

void CNetConnection::Send(TInt aType, const TNetBuffer& aPayload)
    {
    if (!iConnected || iSocket.SubSessionHandle() == 0) return;

    TInt payloadLength = aPayload.Length();
    if (payloadLength < 0 || payloadLength > KNetMaxFrame - 1)
        {
        SetNetError(iError, iErrorText, KErrArgument, _L("Слишком большой пакет"));
        Close();
        return;
        }

    TInt error = NetWriteFrame(iSocket, aType, aPayload.Data(), payloadLength);
    if (error != KErrNone)
        {
        SetNetError(iError, iErrorText, error, _L("Связь с сервером потеряна"));
        Close();
        return;
        }

    iSentBytes += payloadLength + 3;
    iLastSend = User::NTickCount();
    }

void CNetConnection::ReadMoreL()
    {
    /* Не оставляет: используется из Poll/Send-пути без исключений. */
    if (!iConnected || iSocket.SubSessionHandle() == 0) return;
    if (iInputLength >= (TInt)sizeof(iInput)) return;

    TPtr8 destination(iInput + iInputLength, 0,
                      (TInt)sizeof(iInput) - iInputLength);
    TInt received = NetRecvSome(iSocket, destination);
    if (received > 0)
        iInputLength += received;
    else if (received == 0)
        {
        SetNetError(iError, iErrorText, KErrDisconnected, _L("Связь с сервером потеряна"));
        Close();
        }
    else if (received != KNetNoData)
        {
        SetNetError(iError, iErrorText, received, _L("Связь с сервером потеряна"));
        Close();
        }
    }

void CNetConnection::PumpL()
    {
    /* Не оставляет: ошибки превращаются в iError/iErrorText + Close(). */
    if (!iConnected) return;

    for (TInt pass = 0; pass < KNetPollReads && iConnected; pass++)
        {
        TInt consumed = 0;
        while (iInputLength - consumed >= 2)
            {
            TInt frameLength = ((TInt)iInput[consumed] << 8) | iInput[consumed + 1];
            if (frameLength < 1 || frameLength > KNetMaxFrame)
                {
                SetNetError(iError, iErrorText, KErrCorrupt, _L("Неверный кадр"));
                Close();
                return;
                }
            if (iInputLength - consumed - 2 < frameLength) break;

            TInt slot = iFrameWrite;
            TInt next = (slot + 1) % KNetMaxInbox;
            if (next == iFrameRead)                 /* кольцо переполнено */
                iFrameRead = (iFrameRead + 1) % KNetMaxInbox;

            Mem::Copy(iFrame[slot], iInput + consumed + 2, frameLength);
            iFrameLength[slot] = frameLength;
            iFrameWrite = next;
            iReceivedBytes += frameLength + 2;
            iLastReceive = User::NTickCount();
            consumed += 2 + frameLength;
            }

        if (consumed > 0)
            {
            TInt remaining = iInputLength - consumed;
            if (remaining > 0)
                Mem::Copy(iInput, iInput + consumed, remaining);
            iInputLength = remaining;
            }

        TInt before = iInputLength;
        ReadMoreL();
        if (!iConnected || iInputLength == before) return;
        }
    }

TBool CNetConnection::Poll(TUint8* aFrame, TInt& aLength)
    {
    PumpL();                                       /* никогда не оставляет */
    if (iFrameRead == iFrameWrite) { aLength = 0; return EFalse; }
    if (aFrame == 0) { aLength = 0; return EFalse; }

    TInt frameLength = iFrameLength[iFrameRead];
    TInt capacity = aLength > 0 ? aLength : KNetBufferSize + 2;
    TInt copyLength = frameLength < capacity ? frameLength : capacity;
    Mem::Copy(aFrame, iFrame[iFrameRead], copyLength);
    iFrameRead = (iFrameRead + 1) % KNetMaxInbox;
    aLength = frameLength;                         /* полная длина кадра */
    return ETrue;
    }

void CNetConnection::KeepAlive()
    {
    if (!iConnected || iSocket.SubSessionHandle() == 0) return;
    TUint32 now = User::NTickCount();
    if ((TUint32)(now - iLastSend) <= 4000) return;

    TInt error = NetWriteFrame(iSocket, EClientPing, (const TUint8*)0, 0);
    if (error != KErrNone)
        {
        SetNetError(iError, iErrorText, error, _L("Связь с сервером потеряна"));
        Close();
        return;
        }
    iSentBytes += 3;
    iLastSend = now;
    }

void CNetConnection::Close()
    {
    iConnected = EFalse;
    iSocket.Close();
    iListener.Close();
    iInputLength = 0;
    iFrameRead = 0;
    iFrameWrite = 0;
    Mem::FillZ(iFrameLength, sizeof(iFrameLength));
    }


// ---------------------------------------------------------------- CLanFinder

CLanFinder::CLanFinder(RSocketServ& aSocketServer)
    : iSocketServer(aSocketServer)
    {
    Mem::FillZ(iBuffer, sizeof(iBuffer));
    }

CLanFinder* CLanFinder::NewL(RSocketServ& aSocketServer)
    {
    return new (ELeave) CLanFinder(aSocketServer);
    }

CLanFinder::~CLanFinder()
    {
    Stop();
    }

void CLanFinder::StartL(TInt aPort)
    {
    Stop();
    TInt error = iSocket.Open(iSocketServer, KAfInet, KSockDatagram, KProtocolInetUdp);
    User::LeaveIfError(error);

    TInetAddr address;
    address.Init(KAfInet);
    address.SetAddress(KInetAddrAny);
    address.SetPort(aPort);
    error = iSocket.Bind(address);
    if (error != KErrNone)
        {
        iSocket.Close();
        User::Leave(error);
        }
    }

void CLanFinder::Stop()
    {
    iSocket.Close();
    }

TBool CLanFinder::Poll(TInetAddr& aAddress, TPtrC8& aPayload)
    {
    /* Без аллокаций: используется только iBuffer и iSender. */
    if (iSocket.SubSessionHandle() == 0) return EFalse;

    for (TInt attempt = 0; attempt < 4; attempt++)
        {
        TPtr8 packet(iBuffer, 0, (TInt)sizeof(iBuffer));
        TInt received = NetRecvFromSome(iSocket, packet, iSender);
        if (received <= 0) return EFalse;

        /* Фильтр LanFinder.java: "CS16" + Net.PROTO, минимум 9 байт. */
        if (received < 9 || iBuffer[0] != 'C' || iBuffer[1] != 'S' ||
            iBuffer[2] != '1' || iBuffer[3] != '6' ||
            iBuffer[4] != KNetProtocol)
            continue;

        aAddress = iSender;
        aPayload.Set(iBuffer, received);
        return ETrue;
        }
    return EFalse;
    }


// ---------------------------------------------------------------- CHttpClient

CHttpClient::CHttpClient(RSocketServ& aSocketServer)
    : iSocketServer(aSocketServer)
    {
    Mem::FillZ(iReceiveBuffer, sizeof(iReceiveBuffer));
    }

CHttpClient* CHttpClient::NewL(RSocketServ& aSocketServer)
    {
    return new (ELeave) CHttpClient(aSocketServer);
    }

CHttpClient::~CHttpClient()
    {
    iSocket.Close();
    }

TInt CHttpClient::Request(const TDesC8& aHost, TInt aPort, const TDesC8& aPath,
                          TPtrC8& aBody, TUint8* aBuffer, TInt aBufferSize)
    {
    aBody.Set((const TUint8*)"", 0);
    if (aBuffer == 0 || aBufferSize < 32) return KErrArgument;

    TUint8 requestHeader[1024];
    /* "GET " (4) + " HTTP/1.0\r\nHost: " (17) + "\r\nConnection: close\r\n\r\n" (23). */
    TInt needed = 44 + aPath.Length() + aHost.Length();
    if (needed > (TInt)sizeof(requestHeader)) return KErrTooBig;

    TInetAddr address;
    TInt error = ResolveNetHost(iSocketServer, aHost, aPort, address);
    if (error != KErrNone) return error;

    iSocket.Close();
    error = iSocket.Open(iSocketServer, KAfInet, KSockStream, KProtocolInetTcp);
    if (error != KErrNone) return error;
    {
    TSockAddr& sa = address;
    TRequestStatus status;
    iSocket.Connect(sa, status);
    User::WaitForRequest(status);
    error = status.Int();
    }
    if (error != KErrNone)
        {
        iSocket.Close();
        return error;
        }

    TPtr8 request(requestHeader, 0, (TInt)sizeof(requestHeader));
    request.Append(_L8("GET "));
    request.Append(aPath);
    request.Append(_L8(" HTTP/1.0\r\nHost: "));
    request.Append(aHost);
    request.Append(_L8("\r\nConnection: close\r\n\r\n"));
    if (request.Length() != needed)
        {
        iSocket.Close();
        return KErrArgument;
        }

    {
    TRequestStatus status;
    TSockXfrLength sent;
    iSocket.Send(request, 0, status, sent);
    User::WaitForRequest(status);
    error = status.Int();
    }
    if (error != KErrNone)
        {
        iSocket.Close();
        return error;
        }

    TInt offset = 0;
    TInt bodyStart = -1;
    TInt idle = 0;
    for (TInt attempt = 0; attempt < KNetHttpAttempts && offset < aBufferSize; attempt++)
        {
        TInt chunkSize = sizeof(iReceiveBuffer);
        if (chunkSize > aBufferSize - offset) chunkSize = aBufferSize - offset;
        TPtr8 chunk(iReceiveBuffer, 0, (TInt)chunkSize);
        TInt received = NetRecvHttp(iSocket, chunk);
        if (received == KNetNoData)
            {
            idle++;
            if (idle >= KNetHttpIdleAttempts) break;
            continue;
            }
        if (received <= 0) break;                  /* конец ответа или ошибка */
        idle = 0;

        Mem::Copy(aBuffer + offset, iReceiveBuffer, received);
        TInt searchFrom = offset - received - 3;
        if (searchFrom < 0) searchFrom = 0;
        offset += received;

        if (bodyStart < 0)
            {
            for (TInt i = searchFrom; i <= offset - 4; i++)
                {
                if (aBuffer[i] == '\r' && aBuffer[i + 1] == '\n' &&
                    aBuffer[i + 2] == '\r' && aBuffer[i + 3] == '\n')
                    {
                    bodyStart = i + 4;
                    break;
                    }
                }
            }
        }

    iSocket.Close();
    if (bodyStart < 0) return KErrTimedOut;
    aBody.Set(aBuffer + bodyStart, offset - bodyStart);
    return KErrNone;
    }
