#ifndef CS16_STUB_E32DEF_H
#define CS16_STUB_E32DEF_H

/* Minimal Symbian E32 base definitions for syntax checking only. */

#include <stddef.h>

typedef int TInt;
typedef unsigned int TUint;
typedef signed char TInt8;
typedef unsigned char TUint8;
typedef short TInt16;
typedef unsigned short TUint16;
typedef int TInt32;
typedef unsigned int TUint32;
typedef long long TInt64;
typedef unsigned long long TUint64;
typedef float TReal32;
typedef double TReal;
typedef double TReal64;
typedef int TBool;
/* Windows wchar_t is 16-bit, matching Symbian's 16-bit TText. */
typedef unsigned short TText;
typedef void TAny;

const TBool ETrue = 1;
const TBool EFalse = 0;

const TInt KErrNone = 0;
const TInt KErrGeneral = -1;
const TInt KErrCancel = -3;
const TInt KErrNoMemory = -4;
const TInt KErrNotSupported = -5;
const TInt KErrArgument = -6;
const TInt KErrTooBig = -7;
const TInt KErrOverflow = -8;
const TInt KErrUnderflow = -9;
const TInt KErrDisconnected = -11;
const TInt KErrInUse = -14;
const TInt KErrCorrupt = -20;
const TInt KErrAccessDenied = -21;
const TInt KErrAlreadyExists = -22;
const TInt KErrPathNotFound = -23;
const TInt KErrNotFound = -24;
const TInt KErrClosed = -26;
const TInt KErrTimedOut = -27;
const TInt KErrAbort = -28;
const TInt KErrNotReady = -31;
const TInt KErrEof = -40;
const TInt KErrCompletion = -41;
const TInt KErrServerTerminated = -45;
const TInt KErrCouldNotConnect = -48;
const TInt KErrDiskFull = -53;

#define GLDEF_C
#define IMPORT_C
#define EXPORT_C
#define LOCAL_C static

struct TLeave {};
extern const TLeave ELeave;

inline void* operator new(size_t aSize, const TLeave&) { return ::operator new(aSize); }
inline void* operator new[](size_t aSize, const TLeave&) { return ::operator new[](aSize); }
inline void operator delete(void* aPtr, const TLeave&) { ::operator delete(aPtr); }
inline void operator delete[](void* aPtr, const TLeave&) { ::operator delete[](aPtr); }

struct TUid
    {
    TUint32 iUid;
    TUid() : iUid(0) {}
    TUid(TUint32 aUid) : iUid(aUid) {}
    static TUid Uid(TInt aUid) { return TUid((TUint32)aUid); }
    };

class TChar
    {
public:
    TChar() : iCode(0) {}
    TChar(TUint8 aCode) : iCode((TUint16)aCode) {}
    TChar(TUint16 aCode) : iCode(aCode) {}
    TChar(TInt aCode) : iCode((TUint16)aCode) {}
    TChar(char aCode) : iCode((TUint16)(TUint8)aCode) {}
    TUint16 Code() const { return iCode; }
private:
    TUint16 iCode;
    };

/* Leave traps: syntax only, no exception model is needed for -fsyntax-only. */
#define TRAP(aError, aStatement) { (void)(aError); aStatement; }
#define TRAPD(aError, aStatement) TInt aError = KErrNone; { aStatement; }

#define CleanupClosePushL(aObject) ((void)0)

#endif
