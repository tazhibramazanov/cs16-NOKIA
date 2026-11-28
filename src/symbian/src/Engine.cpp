/*
 * Engine.cpp
 */
#include "Engine.h"
#include <e32math.h>
#include <e32std.h>
#include <e32cmn.h>

static TFixed gSinTable[KAngleTableSize];
static TFixed gReciprocal[KReciprocalSize];
static TBool gInitialized = EFalse;
static RLibrary gGlesLib;
static TBool gGlesTried = EFalse;
static TBool gGlesLoaded = EFalse;

TBool CsGpuAvailable()
    {
    if (!gGlesTried)
        {
        gGlesTried = ETrue;
        gGlesLoaded = EFalse;
        TFileName name;
        name.Copy(_L("libgles_cm"));
        if (gGlesLib.Load(name) != KErrNone)
            {
            name.Copy(_L("libGLES_CM"));
            if (gGlesLib.Load(name) != KErrNone) { gGlesLib.Close(); return EFalse; }
            }
        /* Library present: mark available for render-mode selection.
         * Full EGL/GLES context setup is not wired into DSA pipeline here. */
        gGlesLoaded = ETrue;
        }
    return gGlesLoaded;
    }

void EngineInitialize()
    {
    if (gInitialized)
        {
        gGlesTried = EFalse;
        gGlesLoaded = EFalse;
        return;
        }

    const TReal kStep = 6.28318530717958647692 / KAngleTableSize;
    for (TInt i = 0; i < KAngleTableSize; i++)
        {
        TReal sine = 0;
        Math::Sin(sine, kStep * i);
        gSinTable[i] = FxFromReal((TReal32)sine);
        }

    for (TInt i = 0; i < KReciprocalSize; i++)
        {
        TReal32 x = 1.0f + (TReal32)i / KReciprocalSize;
        gReciprocal[i] = FxFromReal(1.0f / x);
        }
    gInitialized = ETrue;
    gGlesTried = EFalse;
    gGlesLoaded = EFalse;
    }

TFixed FxSin(TReal32 aDegrees)
    {
    TInt index = (TInt)(aDegrees * (KAngleTableSize / 360.0f));
    index &= KAngleTableSize - 1;
    return gSinTable[index];
    }

TFixed FxCos(TReal32 aDegrees)
    {
    TInt index = (TInt)((aDegrees + 90.0f) * (KAngleTableSize / 360.0f));
    index &= KAngleTableSize - 1;
    return gSinTable[index];
    }

TReal32 FloatSin(TReal32 aDegrees)
    {
    TInt index = (TInt)(aDegrees * (KAngleTableSize / 360.0f));
    index &= KAngleTableSize - 1;
    return FxToReal(gSinTable[index]);
    }

TReal32 FloatCos(TReal32 aDegrees)
    {
    TInt index = (TInt)((aDegrees + 90.0f) * (KAngleTableSize / 360.0f));
    index &= KAngleTableSize - 1;
    return FxToReal(gSinTable[index]);
    }

TReal32 FloatTan(TReal32 aDegrees)
    {
    TInt index = (TInt)(aDegrees * (KAngleTableSize / 360.0f));
    index &= KAngleTableSize - 1;
    TInt cosIndex = (index + (KAngleTableSize >> 2)) & (KAngleTableSize - 1);
    TFixed cs = gSinTable[cosIndex];
    if (cs == 0) return 0;
    return FxToReal((TFixed)(((TInt64)gSinTable[index] << KFixedShift) / cs));
    }

TReal32 FloatSqrt(TReal32 aValue)
    {
    if (aValue <= 0) return 0;
    union { TReal32 f; TUint32 u; } converter;
    converter.f = aValue;
    converter.u = 0x5F3759DFu - (converter.u >> 1);
    TReal32 x = converter.f;
    x = x * (1.5f - 0.5f * aValue * x * x);
    x = x * (1.5f - 0.5f * aValue * x * x);
    return aValue * x;
    }

TReal32 FloatAtan2(TReal32 aY, TReal32 aX)
    {
    TReal angle = 0;
    if (Math::ATan(angle, (TReal)aY, (TReal)aX) != KErrNone) return 0;
    return (TReal32)(angle * (180.0 / 3.14159265358979323846));
    }

TFixed FxReciprocal(TFixed aValue)
    {
    if (aValue == 0) return KFixedOne;
    TBool negative = aValue < 0;
    TUint32 magnitude = (TUint32)(negative ? -aValue : aValue);
    TInt exponent = 0;
    while (magnitude >= 0x20000u) { magnitude >>= 1; exponent++; }
    while (magnitude < 0x10000u) { magnitude <<= 1; exponent--; }
    TInt index = (TInt)((magnitude - 0x10000u) >> (KFixedShift - KReciprocalBits));
    if (index >= KReciprocalSize) index = KReciprocalSize - 1;
    TFixed result = gReciprocal[index] >> exponent;
    return negative ? -result : result;
    }

TFixed FxDiv(TFixed aNumerator, TFixed aDenominator)
    {
    if (aDenominator == 0) return 0;
    return FxMul(aNumerator, FxReciprocal(aDenominator));
    }

TFixed FxSqrt(TFixed aValue)
    {
    if (aValue <= 0) return 0;
    TFixed x = aValue;
    for (TInt i = 0; i < 6; i++)
        {
        TFixed next = (TFixed)(((TInt64)x + ((TInt64)aValue << KFixedShift) / x) >> 1);
        if (next == x) break;
        x = next;
        }
    return x;
    }

TUint32 HashString(const TDesC8& aText)
    {
    TUint32 hash = 2166136261u;
    for (TInt i = 0; i < aText.Length(); i++)
        {
        hash ^= aText[i];
        hash *= 16777619u;
        }
    return hash;
    }

TUint32 HashString(const TDesC& aText)
    {
    TUint32 hash = 2166136261u;
    for (TInt i = 0; i < aText.Length(); i++)
        {
        hash ^= (TUint32)aText[i];
        hash *= 16777619u;
        }
    return hash;
    }

HBufC8* ReadWholeFileL(RFs& aFileServer, const TDesC& aFileName)
    {
    RFile file;
    User::LeaveIfError(file.Open(aFileServer, aFileName, EFileRead | EFileShareAny));
    CleanupClosePushL(file);
    TInt size = 0;
    User::LeaveIfError(file.Size(size));
    HBufC8* buffer = HBufC8::NewLC(size);
    TPtr8 pointer = buffer->Des();
    User::LeaveIfError(file.Read(pointer));
    CleanupStack::Pop(buffer);
    CleanupStack::PopAndDestroy(&file);
    return buffer;
    }

TBinaryReader::TBinaryReader(const TUint8* aData, TInt aLength)
    : iStart(aData), iPointer(aData), iEnd(aData + (aLength > 0 ? aLength : 0)), iValid(ETrue)
    {
    }

TBool TBinaryReader::Ensure(TInt aCount)
    {
    if (iPointer + aCount > iEnd)
        {
        iValid = EFalse;
        return EFalse;
        }
    return ETrue;
    }

TUint8 TBinaryReader::ReadByte()
    {
    if (!Ensure(1)) return 0;
    return *iPointer++;
    }

TUint16 TBinaryReader::ReadUnsignedShort()
    {
    if (!Ensure(2)) return 0;
    TUint16 value = (TUint16)(((TUint16)iPointer[0] << 8) | iPointer[1]);
    iPointer += 2;
    return value;
    }

TInt16 TBinaryReader::ReadSignedShort()
    {
    return (TInt16)ReadUnsignedShort();
    }

TInt32 TBinaryReader::ReadInteger()
    {
    if (!Ensure(4)) return 0;
    TInt32 value = ((TInt32)iPointer[0] << 24) | ((TInt32)iPointer[1] << 16) |
                   ((TInt32)iPointer[2] << 8) | iPointer[3];
    iPointer += 4;
    return value;
    }

TReal32 TBinaryReader::ReadReal()
    {
    union { TUint32 bits; TReal32 value; } converter;
    converter.bits = (TUint32)ReadInteger();
    return converter.value;
    }

void TBinaryReader::ReadBytes(TUint8* aDestination, TInt aCount)
    {
    if (!Ensure(aCount)) return;
    Mem::Copy(aDestination, iPointer, aCount);
    iPointer += aCount;
    }

TPtrC8 TBinaryReader::ReadString8()
    {
    TInt length = ReadUnsignedShort();
    if (!Ensure(length)) return TPtrC8();
    TPtrC8 result(iPointer, length);
    iPointer += length;
    return result;
    }

void TBinaryReader::Skip(TInt aCount)
    {
    if (!Ensure(aCount)) return;
    iPointer += aCount;
    }
