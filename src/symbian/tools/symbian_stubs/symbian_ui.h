#ifndef CS16_STUB_SYMBIAN_UI_H
#define CS16_STUB_SYMBIAN_UI_H

/*
 * Syntax-only stubs for the Avkon/ConEnv graphics classes used by App.cpp.
 * These declarations intentionally mirror only the signatures used by the
 * port; a real S60 SDK build must use the SDK headers instead.
 */

#include "e32base.h"
#include "e32std.h"

class CEikMenuPane;
class CEikonEnv;
class CEpaAppUi;
class CFbsScreenDevice;
class CBitmapContext;

class TPoint
    {
public:
    TPoint() : iX(0), iY(0) {}
    TPoint(TInt aX, TInt aY) : iX(aX), iY(aY) {}
    TInt iX;
    TInt iY;
    };

class TSize
    {
public:
    TSize() : iWidth(0), iHeight(0) {}
    TSize(TInt aWidth, TInt aHeight) : iWidth(aWidth), iHeight(aHeight) {}
    TInt iWidth;
    TInt iHeight;
    };

class TRect
    {
public:
    TRect() : iTl(0, 0), iBr(0, 0) {}
    TRect(TInt aLeft, TInt aTop, TInt aRight, TInt aBottom)
        : iTl(aLeft, aTop), iBr(aRight, aBottom) {}
    TRect(TPoint aPoint, TInt aWidth, TInt aHeight)
        : iTl(aPoint), iBr(aPoint.iX + aWidth, aPoint.iY + aHeight) {}
    TRect(TPoint aPoint, TSize aSize)
        : iTl(aPoint), iBr(aPoint.iX + aSize.iWidth, aPoint.iY + aSize.iHeight) {}
    TInt Width() const { return iBr.iX - iTl.iX; }
    TInt Height() const { return iBr.iY - iTl.iY; }
    TPoint Center() const { return TPoint((iTl.iX + iBr.iX) / 2, (iTl.iY + iBr.iY) / 2); }
    TPoint iTl;
    TPoint iBr;
    };

enum TEventCode
    {
    EEventKey = 0,
    EEventKeyDown = 1,
    EEventKeyUp = 2
    };

struct TKeyEvent
    {
    TInt iCode;
    TInt iScanCode;
    TInt iModifiers;
    TInt iRepeats;
    TKeyEvent() : iCode(0), iScanCode(0), iModifiers(0), iRepeats(0) {}
    };

class RWindow
    {
public:
    void SetOrdinaryPosition(TPoint) {}
    void SetExtent(TPoint, TSize) {}
    TInt Handle() const { return 0; }
    };

class RWsSession
    {
public:
    TInt Connect() { return KErrNone; }
    void Close() {}
    };

class MGraphicsDeviceMap
    {
public:
    virtual ~MGraphicsDeviceMap() {}
    };

class CFbsScreenDevice : public MGraphicsDeviceMap
    {
public:
    TUint8* GetScanLine(TInt) { return 0; }
    TInt DisplayMode() const { return 0; }
    };

class CBitmapContext : public CBase
    {
public:
    void SetClippingRegion(const TRect&) {}
    MGraphicsDeviceMap* Device() const { return 0; }
    };

class RDirectScreenAccess
    {
public:
    enum TTerminationReasons
        {
        ETerminationRestart = 0,
        ETerminationAppFocusLost,
        ETerminationAppMinimised,
        ETerminationAppExited
        };
    };

class MDirectScreenAccess
    {
public:
    virtual ~MDirectScreenAccess() {}
    virtual void Abort(RDirectScreenAccess::TTerminationReasons aReason) = 0;
    virtual void Restart(RDirectScreenAccess::TTerminationReasons aReason) = 0;
    };

class CDirectScreenAccess : public CBase, public RDirectScreenAccess
    {
public:
    static CDirectScreenAccess* NewL(RWsSession&, MGraphicsDeviceMap&,
                                     MDirectScreenAccess&, RWindow&)
        { return new (ELeave) CDirectScreenAccess; }
    void StartL() {}
    CBitmapContext* Gc() { static CBitmapContext context; return &context; }
    TRect DrawingRegionBoundingRect() const { return TRect(0, 0, 240, 320); }
    const TRect& DrawingRegion() const { static TRect region(0, 0, 240, 320); return region; }
    void Cancel() {}
    };

class CEikonEnv
    {
public:
    RWsSession& WsSession() { static RWsSession session; return session; }
    CFbsScreenDevice* ScreenDevice() { static CFbsScreenDevice device; return &device; }
    };

class CCoeControl : public CBase
    {
public:
    virtual ~CCoeControl() {}
    void CreateWindowL() {}
    void SetRect(const TRect& aRect) { iRect = aRect; }
    TRect Rect() const { return iRect; }
    void EnableDragEvents() {}
    RWindow& Window() { static RWindow window; return window; }
    virtual void Draw(const TRect& /*aRect*/) const {}
protected:
    CEikonEnv* iCoeEnv;
private:
    TRect iRect;
    };

class CApaApplication : public CBase
    {
public:
    virtual ~CApaApplication() {}
    virtual TUid AppDllUid() const = 0;
    };

class CEpaApplication : public CApaApplication
    {
public:
    virtual ~CEpaApplication() {}
    };

class CApaDocument : public CBase
    {
public:
    virtual ~CApaDocument() {}
    };

class CEpaDocument : public CApaDocument
    {
public:
    virtual ~CEpaDocument() {}
    };

class CAknDocument : public CEpaDocument
    {
public:
    CAknDocument(CEpaApplication*) {}
    virtual ~CAknDocument() {}
    virtual CEpaAppUi* CreateAppUiL() = 0;
    };

class CEpaAppUi : public CBase
    {
public:
    virtual ~CEpaAppUi() {}
    virtual void ConstructL() = 0;
    virtual TBool HandleKeyEventL(const TKeyEvent&, TEventCode) { return EFalse; }
    virtual void HandleCommandL(TInt) {}
    virtual void HandleClientRectChange() {}
    virtual void DynInitMenuPaneL(TInt, CEikMenuPane*) {}
    };

class CAknAppUi : public CEpaAppUi
    {
public:
    virtual ~CAknAppUi() {}
    TRect ClientRect() const { return TRect(0, 0, 240, 320); }
    void BaseConstructL(TInt) {}
    RWindow& Window() { static RWindow window; return window; }
    void AddToStackL(CCoeControl*) {}
    void RemoveFromStack(CCoeControl*) {}
    void Exit() {}
    };

class CAknApplication : public CEpaApplication
    {
public:
    virtual ~CAknApplication() {}
protected:
    virtual CApaDocument* CreateDocumentL() = 0;
    };

class CEikStart
    {
public:
    static void RunApplication(CApaApplication* (*aFactory)())
        { (void)aFactory; }
    };

typedef CEikStart EikStart;

const TInt EAknSoftkeyExit = 0x110;
const TInt EAknSoftkeyBack = 0x111;
const TInt EEikCmdExit = 0x112;
const TInt EAknEnableSkin = 0x2000;

const TInt EStdKeyUpArrow = 0x41;
const TInt EStdKeyDownArrow = 0x42;
const TInt EStdKeyLeftArrow = 0x43;
const TInt EStdKeyRightArrow = 0x44;
const TInt EStdKeyDevice3 = 0x45;
const TInt EStdKeyDevice0 = 0x4A;
const TInt EStdKeyDevice1 = 0x4B;
const TInt EStdKeyYes = 0x47;
const TInt EStdKeyNo = 0x48;
const TInt EStdKeyClear = 0x49;

#endif /* CS16_STUB_SYMBIAN_UI_H */
