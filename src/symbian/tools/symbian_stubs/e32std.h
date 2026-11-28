#ifndef CS16_STUB_E32STD_H
#define CS16_STUB_E32STD_H

#include "e32def.h"
#include <string.h>

template <class T> inline T Min(T aFirst, T aSecond)
    { return aFirst < aSecond ? aFirst : aSecond; }
template <class T> inline T Max(T aFirst, T aSecond)
    { return aFirst > aSecond ? aFirst : aSecond; }
template <class T> inline T Abs(T aValue)
    { return aValue < 0 ? -aValue : aValue; }

class TDesC8;
class TDes8;
class TPtrC8;
class TPtrC;
class HBufC;
class HBufC8;

/* ------------------------------------------------------------ narrow text */

class TDesC8
    {
public:
    TInt Length() const { return iLength; }
    const TUint8* Ptr() const { return iPtr; }
    TUint8 operator[](TInt aIndex) const { return iPtr[aIndex]; }
    TInt Compare(const TDesC8& aOther) const
        {
        TInt n = iLength < aOther.iLength ? iLength : aOther.iLength;
        for (TInt i = 0; i < n; i++)
            {
            if (iPtr[i] < aOther.iPtr[i]) return -1;
            if (iPtr[i] > aOther.iPtr[i]) return 1;
            }
        return iLength == aOther.iLength ? 0 : (iLength < aOther.iLength ? -1 : 1);
        }
    TBool operator==(const TDesC8& aOther) const { return Compare(aOther) == 0; }
    TBool operator!=(const TDesC8& aOther) const { return Compare(aOther) != 0; }
    TPtrC8 Left(TInt aLength) const;
    TInt Find(const TDesC8& aNeedle) const
        {
        if (aNeedle.iLength <= 0 || aNeedle.iLength > iLength) return -1;
        for (TInt i = 0; i <= iLength - aNeedle.iLength; i++)
            {
            TInt j = 0;
            while (j < aNeedle.iLength && iPtr[i + j] == aNeedle.iPtr[j]) j++;
            if (j == aNeedle.iLength) return i;
            }
        return -1;
        }
protected:
    TDesC8() : iPtr(0), iLength(0) {}
    TDesC8(const TUint8* aPtr, TInt aLength) : iPtr(aPtr), iLength(aLength) {}
    void SetData(const TUint8* aPtr, TInt aLength) { iPtr = aPtr; iLength = aLength; }
protected:
    const TUint8* iPtr;
    TInt iLength;
    };

class TPtrC8 : public TDesC8
    {
public:
    TPtrC8() : TDesC8((const TUint8*)"", 0) {}
    TPtrC8(const TUint8* aPtr, TInt aLength) : TDesC8(aPtr, aLength > 0 ? aLength : 0) {}
    TPtrC8(const TDesC8& aDes) : TDesC8(aDes.Ptr(), aDes.Length()) {}
    TPtrC8(const char* aPtr) : TDesC8((const TUint8*)aPtr, 0) {}
    void Set(const TUint8* aPtr, TInt aLength) { SetData(aPtr, aLength > 0 ? aLength : 0); }
    void Set(const TDesC8& aDes) { SetData(aDes.Ptr(), aDes.Length()); }
    TPtrC8& operator=(const TDesC8& aDes) { Set(aDes); return *this; }
    };

inline TPtrC8 TDesC8::Left(TInt aLength) const
    {
    if (aLength < 0) aLength = 0;
    if (aLength > iLength) aLength = iLength;
    return TPtrC8(iPtr, aLength);
    }

class TDes8 : public TDesC8
    {
public:
    void SetLength(TInt aLength) { iLength = aLength; }
    TInt MaxLength() const { return iMaxLength; }
    TUint8* Ptr() { return (TUint8*)TDesC8::Ptr(); }
    void Append(TUint8 aChar)
        {
        if (iLength < iMaxLength) ((TUint8*)TDesC8::Ptr())[iLength++] = aChar;
        }
    void Append(const TDesC8& aDes)
        {
        TInt n = aDes.Length();
        if (n > iMaxLength - iLength) n = iMaxLength - iLength;
        for (TInt i = 0; i < n; i++) Append(aDes[i]);
        }
    void Append(const char* aText)
        {
        while (*aText) Append((TUint8)*aText++);
        }
    void Copy(const TDesC8& aDes)
        {
        SetLength(0);
        Append(aDes);
        }
    void FillZ(TInt aCount = 0)
        {
        TInt n = aCount > 0 ? aCount : iLength;
        for (TInt i = 0; i < n; i++) ((TUint8*)TDesC8::Ptr())[i] = 0;
        }
    void Zero()
        {
        for (TInt i = 0; i < iMaxLength; i++) ((TUint8*)TDesC8::Ptr())[i] = 0;
        SetLength(0);
        }
protected:
    TDes8(TUint8* aPtr, TInt aLength, TInt aMaxLength)
        : TDesC8(aPtr, aLength), iMaxLength(aMaxLength) {}
    void SetPtr(TUint8* aPtr) { SetData(aPtr, TDesC8::Length()); }
private:
    TInt iMaxLength;
    };

class TPtr8 : public TDes8
    {
public:
    TPtr8() : TDes8((TUint8*)"", 0, 0) {}
    TPtr8(TUint8* aPtr, TInt aLength) : TDes8(aPtr, aLength, aLength) {}
    TPtr8(TUint8* aPtr, TInt aLength, TInt aMaxLength) : TDes8(aPtr, aLength, aMaxLength) {}
    void Set(TUint8* aPtr, TInt aLength) { SetPtr(aPtr); SetLength(aLength); }
    void Set(TUint8* aPtr, TInt aLength, TInt aMaxLength)
        {
        SetPtr(aPtr);
        SetLength(aLength);
        iMax = aMaxLength;
        }
    void SetLength(TInt aLength) { TDes8::SetLength(aLength); }
    TPtr8& operator=(const TDesC8& aDes) { Copy(aDes); return *this; }
private:
    TInt iMax;
    };

template <TInt N> class TBuf8 : public TDes8
    {
public:
    TBuf8() : TDes8(iBuffer, 0, N) {}
    TBuf8(const TDesC8& aDes) : TDes8(iBuffer, 0, N) { Copy(aDes); }
    void Zero()
        {
        for (TInt i = 0; i < N; i++) iBuffer[i] = 0;
        SetLength(0);
        }
private:
    TUint8 iBuffer[N];
    };

class HBufC8 : public TDesC8
    {
public:
    static HBufC8* NewL(TInt aLength) { return new (ELeave) HBufC8(aLength); }
    static HBufC8* NewLC(TInt aLength) { return NewL(aLength); }
    void SetLength(TInt aLength) { SetData(iBuffer, aLength); }
    TInt MaxLength() const { return iCapacity; }
    TPtr8 Des() { return TPtr8(iBuffer, TDesC8::Length(), iCapacity); }
    const TUint8* Ptr() const { return iBuffer; }
    HBufC8(TInt aCapacity) : TDesC8(0, 0), iCapacity(aCapacity > 0 ? aCapacity : 0)
        { iBuffer = new TUint8[iCapacity > 0 ? iCapacity : 1]; SetData(iBuffer, 0); }
private:
    TUint8* iBuffer;
    TInt iCapacity;
    };

template <TInt N> class TLitC8 : public TDesC8
    {
public:
    TLitC8(const char (&aText)[N]) : TDesC8((const TUint8*)iBuffer, N - 1)
        {
        for (TInt i = 0; i < N; i++) iBuffer[i] = (TUint8)aText[i];
        }
private:
    TUint8 iBuffer[N];
    };

#define _L8(aText) TLitC8<sizeof(aText)>(aText)

/* ------------------------------------------------------------ wide text */

class TDesC
    {
public:
    TInt Length() const { return iLength; }
    const TText* Ptr() const { return iPtr; }
    TText operator[](TInt aIndex) const { return iPtr[aIndex]; }
    TInt Compare(const TDesC& aOther) const
        {
        TInt n = iLength < aOther.iLength ? iLength : aOther.iLength;
        for (TInt i = 0; i < n; i++)
            {
            if (iPtr[i] < aOther.iPtr[i]) return -1;
            if (iPtr[i] > aOther.iPtr[i]) return 1;
            }
        return iLength == aOther.iLength ? 0 : (iLength < aOther.iLength ? -1 : 1);
        }
    TBool operator==(const TDesC& aOther) const { return Compare(aOther) == 0; }
    TBool operator!=(const TDesC& aOther) const { return Compare(aOther) != 0; }
    TPtrC Left(TInt aLength) const;
    HBufC* AllocL() const;
protected:
    TDesC() : iPtr(0), iLength(0) {}
    TDesC(const TText* aPtr, TInt aLength) : iPtr(aPtr), iLength(aLength) {}
    void SetData(const TText* aPtr, TInt aLength) { iPtr = aPtr; iLength = aLength; }
protected:
    const TText* iPtr;
    TInt iLength;
    };

class TPtrC : public TDesC
    {
public:
    TPtrC() : TDesC((const TText*)"", 0) {}
    TPtrC(const TText* aPtr) : TDesC(aPtr, TextLength(aPtr)) {}
    TPtrC(const TText* aPtr, TInt aLength) : TDesC(aPtr, aLength > 0 ? aLength : 0) {}
    TPtrC(const TDesC& aDes) : TDesC(aDes.Ptr(), aDes.Length()) {}
    void Set(const TText* aPtr, TInt aLength) { SetData(aPtr, aLength > 0 ? aLength : 0); }
    void Set(const TDesC& aDes) { SetData(aDes.Ptr(), aDes.Length()); }
    TPtrC& operator=(const TDesC& aDes) { Set(aDes); return *this; }
    static TInt TextLength(const TText* aPtr)
        {
        if (!aPtr) return 0;
        TInt length = 0;
        while (aPtr[length]) length++;
        return length;
        }
    };

inline TPtrC TDesC::Left(TInt aLength) const
    {
    if (aLength < 0) aLength = 0;
    if (aLength > iLength) aLength = iLength;
    return TPtrC(iPtr, aLength);
    }

class TDes : public TDesC
    {
public:
    void SetLength(TInt aLength) { iLength = aLength; }
    TInt MaxLength() const { return iMaxLength; }
    TText* Ptr() { return (TText*)TDesC::Ptr(); }
    void Append(TChar aChar)
        {
        if (iLength < iMaxLength) ((TText*)TDesC::Ptr())[iLength++] = aChar.Code();
        }
    void Append(const TDesC& aDes)
        {
        TInt n = aDes.Length();
        if (n > iMaxLength - iLength) n = iMaxLength - iLength;
        for (TInt i = 0; i < n; i++) Append((TText)aDes[i]);
        }
    void Append(TText aChar) { Append(TChar(aChar)); }
    void AppendNum(TInt aValue)
        {
        TText buffer[24];
        TInt length = 0;
        TUint magnitude = aValue < 0 ? (TUint)(-(TInt64)aValue) : (TUint)aValue;
        if (magnitude == 0) buffer[length++] = (TText)'0';
        while (magnitude > 0 && length < 23)
            {
            buffer[length++] = (TText)('0' + (magnitude % 10));
            magnitude /= 10;
            }
        if (aValue < 0) buffer[length++] = (TText)'-';
        while (length > 0) Append(buffer[--length]);
        }
    void Zero()
        {
        for (TInt i = 0; i < iMaxLength; i++) ((TText*)TDesC::Ptr())[i] = 0;
        SetLength(0);
        }
    void Copy(const TDesC& aDes)
        {
        SetLength(0);
        Append(aDes);
        }
    void FillZ(TInt aCount = 0)
        {
        TInt n = aCount > 0 ? aCount : iLength;
        for (TInt i = 0; i < n; i++) ((TText*)TDesC::Ptr())[i] = 0;
        }
protected:
    TDes(TText* aPtr, TInt aLength, TInt aMaxLength)
        : TDesC(aPtr, aLength), iMaxLength(aMaxLength) {}
    void SetPtr(TText* aPtr) { SetData(aPtr, TDesC::Length()); }
private:
    TInt iMaxLength;
    };

class TPtr : public TDes
    {
public:
    TPtr() : TDes((TText*)"", 0, 0) {}
    TPtr(TText* aPtr, TInt aMaxLength) : TDes(aPtr, 0, aMaxLength) {}
    TPtr(TText* aPtr, TInt aLength, TInt aMaxLength) : TDes(aPtr, aLength, aMaxLength) {}
    void Set(TText* aPtr, TInt aLength) { SetPtr(aPtr); SetLength(aLength); }
    void Set(TText* aPtr, TInt aLength, TInt aMaxLength)
        {
        SetPtr(aPtr);
        SetLength(aLength);
        iMax = aMaxLength;
        }
    TPtr& operator=(const TDesC& aDes) { Copy(aDes); return *this; }
private:
    TInt iMax;
    };

template <TInt N> class TBuf : public TDes
    {
public:
    TBuf() : TDes(iBuffer, 0, N) {}
    TBuf(const TDesC& aDes) : TDes(iBuffer, 0, N) { Copy(aDes); }
    void Zero() { for (TInt i = 0; i < N; i++) iBuffer[i] = 0; SetLength(0); }
private:
    TText iBuffer[N];
    };

class HBufC : public TDesC
    {
public:
    static HBufC* NewL(TInt aLength) { return new (ELeave) HBufC(aLength); }
    static HBufC* NewLC(TInt aLength) { return NewL(aLength); }
    void SetLength(TInt aLength) { SetData(iBuffer, aLength); }
    TInt MaxLength() const { return iCapacity; }
    TPtr Des() { return TPtr(iBuffer, TDesC::Length(), iCapacity); }
    const TText* Ptr() const { return iBuffer; }
    HBufC* AllocL() const
        {
        HBufC* copy = new (ELeave) HBufC(TDesC::Length());
        for (TInt i = 0; i < TDesC::Length(); i++) copy->iBuffer[i] = TDesC::Ptr()[i];
        copy->SetData(copy->iBuffer, TDesC::Length());
        return copy;
        }
    HBufC(TInt aCapacity) : TDesC(0, 0), iCapacity(aCapacity > 0 ? aCapacity : 0)
        { iBuffer = new TText[iCapacity > 0 ? iCapacity : 1]; SetData(iBuffer, 0); }
private:
    TText* iBuffer;
    TInt iCapacity;
    };

inline HBufC* TDesC::AllocL() const
    {
    HBufC* copy = new (ELeave) HBufC(iLength);
    TPtr pointer = copy->Des();
    TText* destination = pointer.Ptr();
    for (TInt i = 0; i < iLength; i++) destination[i] = iPtr[i];
    pointer.SetLength(iLength);
    copy->SetLength(iLength);
    return copy;
    }

template <TInt N> class TLitC : public TDesC
    {
public:
    TLitC(const char (&aText)[N]) : TDesC((const TText*)iBuffer, N - 1)
        {
        for (TInt i = 0; i < N; i++) iBuffer[i] = (TUint8)aText[i];
        }
private:
    TText iBuffer[N];
    };

#define _L(aText) TLitC<sizeof(aText)>(aText)
#define _LIT(aName, aText) static const TLitC<sizeof(aText)> aName(aText)

class TFileName : public TBuf<256>
    {
public:
    TFileName() : TBuf<256>() {}
    TFileName(const TDesC& aDes) : TBuf<256>(aDes) {}
    };

/* ------------------------------------------------------------ files / misc */

class RFs
    {
public:
    TInt Connect() { return KErrNone; }
    void Close() {}
    };

class RFile
    {
public:
    TInt Open(RFs&, const TDesC&, TUint) { return KErrNone; }
    TInt Replace(RFs&, const TDesC&, TUint) { return KErrNone; }
    TInt Create(RFs&, const TDesC&, TUint) { return KErrNone; }
    TInt Read(TDes8&) { return KErrNone; }
    TInt Read(TDes8&, TInt) { return KErrNone; }
    TInt Write(const TDesC8&) { return KErrNone; }
    TInt Write(const TDesC8&, TInt) { return KErrNone; }
    TInt Write(TUint8, TInt) { return KErrNone; }
    TInt Size(TInt& aSize) { aSize = 0; return KErrNone; }
    void Close() {}
    };

class RProcess
    {
public:
    TInt FileName(TDes&) { return KErrNone; }
    };

class TParsePtrC
    {
public:
    TParsePtrC(const TDesC&) {}
    const TDesC& Path() const { static TLitC<1> empty(""); return empty; }
    const TDesC& Drive() const { static TLitC<1> empty(""); return empty; }
    const TDesC& Name() const { static TLitC<1> empty(""); return empty; }
    const TDesC& DriveAndPath() const { static TLitC<1> empty(""); return empty; }
    const TDesC& NameAndExt() const { static TLitC<1> empty(""); return empty; }
    };

const TUint EFileRead = 0x00000001;
const TUint EFileWrite = 0x00000002;
const TUint EFileShareAny = 0x00000040;
const TUint EFileShareExclusive = 0x00000010;

class TRequestStatus
    {
public:
    TRequestStatus() : iStatus(KErrNone) {}
    TInt Int() const { return iStatus; }
    TBool operator==(TInt aValue) const { return iStatus == aValue; }
    TBool operator!=(TInt aValue) const { return iStatus != aValue; }
private:
    TInt iStatus;
    };

const TInt KRequestPending = -1;

class User
    {
public:
    static void Leave(TInt) {}
    static TInt LeaveIfError(TInt aError) { return aError; }
    static TInt LeaveNoMemory() { return KErrNoMemory; }
    static void After(TInt) {}
    static void WaitForRequest(TRequestStatus&) {}
    static TUint32 NTickCount() { return 0; }
    static void Panic(const TDesC&, TInt) {}
    };

class Mem
    {
public:
    static void Copy(void*, const void*, TInt) {}
    static void FillZ(void*, TInt) {}
    static void Fill(void*, TInt, TUint8) {}
    static TInt Compare(const void*, TInt, const void*, TInt) { return 0; }
    };

typedef void (*TCleanupOperation)(TAny*);

class TCleanupItem
    {
public:
    TCleanupItem(TCleanupOperation aOperation, TAny* aPtr)
        : iOperation(aOperation), iPtr(aPtr) {}
    TCleanupOperation iOperation;
    TAny* iPtr;
    };

class CleanupStack
    {
public:
    static void PushL(TAny*) {}
    static void PushL(const TCleanupItem&) {}
    static void Pop(TInt aCount = 1) { (void)aCount; }
    static void Pop(TAny*) {}
    static void PopAndDestroy(TInt aCount = 1) { (void)aCount; }
    static void PopAndDestroy(TInt aCount, TAny*) { (void)aCount; }
    static void PopAndDestroy(TAny*) {}
    };

#endif
