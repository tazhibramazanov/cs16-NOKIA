#ifndef CS16_STUB_E32MATH_H
#define CS16_STUB_E32MATH_H

#include "e32std.h"
#include <math.h>

class Math
    {
public:
    static TReal Sin(TReal aValue) { return ::sin(aValue); }
    static TReal Cos(TReal aValue) { return ::cos(aValue); }
    static TReal Tan(TReal aValue) { return ::tan(aValue); }
    static TReal ASin(TReal aValue) { return ::asin(aValue); }
    static TReal ACos(TReal aValue) { return ::acos(aValue); }
    static TReal Atan(TReal aValue) { return ::atan(aValue); }
    static TReal Atan2(TReal aY, TReal aX) { return ::atan2(aY, aX); }
    static TReal Sqrt(TReal aValue) { return ::sqrt(aValue); }
    static TReal Log(TReal aValue) { return ::log(aValue); }
    static TReal Exp(TReal aValue) { return ::exp(aValue); }
    static TReal Pow(TReal aBase, TReal aExponent) { return ::pow(aBase, aExponent); }
    static TReal Abs(TReal aValue) { return aValue < 0 ? -aValue : aValue; }
    static TInt FloatToBits(TInt32& aBits, TReal32 aValue)
        {
        union { TReal32 f; TInt32 i; } converter;
        converter.f = aValue;
        aBits = converter.i;
        return KErrNone;
        }
    static TInt BitsToFloat(TReal32& aValue, TInt32 aBits)
        {
        union { TReal32 f; TInt32 i; } converter;
        converter.i = aBits;
        aValue = converter.f;
        return KErrNone;
        }
    static TInt IntToBits(TReal32& aValue, TInt32 aBits) { return BitsToFloat(aValue, aBits); }
    };

#endif
