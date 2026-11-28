#ifndef CS16_ENGINE_H
#define CS16_ENGINE_H

/*
 * Engine.h -- общие примитивы нативного движка CS16_E52.
 *
 * 16.16 fixed point, LUT sin/cos и 1/z, быстрый sqrt, детерминированный
 * генератор случайных чисел и big-endian ридер для форматов conv/.
 * Всё inline или статично, чтобы не было вызовов в пиксельном цикле.
 */

#include <e32std.h>
#include <e32math.h>
#include <e32def.h>
#include <f32file.h>

typedef TInt32 TFixed;

const TFixed KFixedOne = 65536;
const TInt KFixedShift = 16;
const TInt KAngleTableSize = 4096;   // 0.0879 градуса на запись
const TInt KReciprocalBits = 16;
const TInt KReciprocalSize = 4096;

inline TFixed FxFromReal(TReal32 aValue)
    { return (TFixed)(aValue * 65536.0f); }

inline TFixed FxFromInt(TInt aValue)
    { return (TFixed)(aValue << KFixedShift); }

inline TInt FxToInt(TFixed aValue)
    { return aValue >> KFixedShift; }

inline TReal32 FxToReal(TFixed aValue)
    { return aValue * (1.0f / 65536.0f); }

inline TFixed FxMul(TFixed aLeft, TFixed aRight)
    { return (TFixed)(((TInt64)aLeft * (TInt64)aRight) >> KFixedShift); }

inline TFixed FxDivInt(TFixed aValue, TInt aDivisor)
    { return aDivisor == 0 ? aValue : aValue / aDivisor; }

inline TFixed FxAbs(TFixed aValue)
    { return aValue < 0 ? -aValue : aValue; }

TFixed FxDiv(TFixed aNumerator, TFixed aDenominator);
TFixed FxReciprocal(TFixed aValue);
TFixed FxSqrt(TFixed aValue);

inline TInt FastFloor(TReal32 aValue)
    { return (TInt)aValue - (aValue < 0 && aValue != (TReal32)(TInt)aValue ? 1 : 0); }

inline TReal32 FastSqrt(TReal32 aValue)
    {
    if (aValue <= 0) return 0;
    union { TReal32 value; TUint32 bits; } converter;
    converter.value = aValue;
    converter.bits = 0x1FBCF800u + (converter.bits >> 1);
    TReal32 x = converter.value;
    x = 0.5f * (x + aValue / x);
    x = 0.5f * (x + aValue / x);
    return x;
    }

inline TReal32 FastReciprocal(TReal32 aValue)
    {
    union { TReal32 value; TUint32 bits; } converter;
    converter.value = aValue;
    converter.bits = 0x7EF311C2u - converter.bits;
    TReal32 x = converter.value;
    x = x * (2.0f - aValue * x);
    x = x * (2.0f - aValue * x);
    return x;
    }

void EngineInitialize();
TBool CsGpuAvailable();

/* Реальные функции объявлены здесь, таблицы лежат в Engine.cpp. */
TFixed FxSin(TReal32 aDegrees);
TFixed FxCos(TReal32 aDegrees);
TReal32 FloatSin(TReal32 aDegrees);
TReal32 FloatCos(TReal32 aDegrees);
TReal32 FloatTan(TReal32 aDegrees);
TReal32 FloatSqrt(TReal32 aValue);
TReal32 FloatAtan2(TReal32 aY, TReal32 aX);

inline TReal32 NormalizeAngle(TReal32 aAngle)
    {
    while (aAngle > 180.0f) aAngle -= 360.0f;
    while (aAngle < -180.0f) aAngle += 360.0f;
    return aAngle;
    }

inline TReal32 NormalizeAngle360(TReal32 aAngle)
    {
    while (aAngle >= 360.0f) aAngle -= 360.0f;
    while (aAngle < 0.0f) aAngle += 360.0f;
    return aAngle;
    }

inline TReal32 AngleDelta(TReal32 aFrom, TReal32 aTo)
    { return NormalizeAngle(aTo - aFrom); }

template <class T> inline T Clamp(T aValue, T aLow, T aHigh)
    { return aValue < aLow ? aLow : (aValue > aHigh ? aHigh : aValue); }

inline TUint32 PackRgb565(TInt aRed, TInt aGreen, TInt aBlue)
    {
    return (TUint32)((aRed >> 3) << 11) | (TUint32)((aGreen >> 2) << 5) | (TUint32)(aBlue >> 3);
    }

inline TUint32 BlendRgb565(TUint32 aFirst, TUint32 aSecond, TInt aWeight)
    {
    TInt r = (((aFirst >> 11) & 31) * (256 - aWeight) + ((aSecond >> 11) & 31) * aWeight) >> 8;
    TInt g = (((aFirst >> 5) & 63) * (256 - aWeight) + ((aSecond >> 5) & 63) * aWeight) >> 8;
    TInt b = ((aFirst & 31) * (256 - aWeight) + (aSecond & 31) * aWeight) >> 8;
    return (TUint32)((r << 11) | (g << 5) | b);
    }

class TRandom
    {
public:
    TRandom() : iState(0x9E3779B9u) {}

    void Seed(TUint32 aSeed)
        { iState = aSeed ? aSeed : 0x9E3779B9u; }

    TUint32 Next()
        {
        iState ^= iState << 13;
        iState ^= iState >> 17;
        iState ^= iState << 5;
        return iState;
        }

    TReal32 NextReal()
        { return (TReal32)(Next() >> 8) * (1.0f / 16777216.0f); }

    TReal32 NextSigned()
        { return NextReal() * 2.0f - 1.0f; }

    TInt NextInt(TInt aRange)
        { return aRange > 0 ? (TInt)(Next() % (TUint32)aRange) : 0; }

private:
    TUint32 iState;
    };

TUint32 HashString(const TDesC8& aText);
TUint32 HashString(const TDesC& aText);

/* Читает весь файл; вызывающий обязан удалить результат. */
HBufC8* ReadWholeFileL(RFs& aFileServer, const TDesC& aFileName);

/*
 * Последовательный big-endian ридер. Все форматы conv/ созданы Java
 * DataOutputStream, поэтому порядок байтов именно такой. Ридер не бросает
 * исключений: после конца буфера он помечает себя невалидным и отдаёт нули.
 */
class TBinaryReader
    {
public:
    TBinaryReader(const TUint8* aData, TInt aLength);

    TUint8 ReadByte();
    TUint16 ReadUnsignedShort();
    TInt16 ReadSignedShort();
    TInt32 ReadInteger();
    TReal32 ReadReal();
    void ReadBytes(TUint8* aDestination, TInt aCount);
    TPtrC8 ReadString8();        /* DataInputStream.readUTF, ASCII */
    TBool IsValid() const { return iValid; }
    TInt Position() const { return (TInt)(iPointer - iStart); }
    TInt Remaining() const { return (TInt)(iEnd - iPointer); }
    const TUint8* Pointer() const { return iPointer; }
    void Skip(TInt aCount);

private:
    TBool Ensure(TInt aCount);

private:
    const TUint8* iStart;
    const TUint8* iPointer;
    const TUint8* iEnd;
    TBool iValid;
    };

#endif
