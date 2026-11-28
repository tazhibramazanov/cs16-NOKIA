#ifndef CS16_APP_H
#define CS16_APP_H

/*
 * App.h -- Symbian application skeleton for CS16_E52:
 * CAknApplication -> CAknDocument -> CAknAppUi -> CGameControl.
 *
 * CGameControl owns the DSA (direct screen access) and drives the frame loop
 * through a nested CFrameTimer (CActive + RTimer::After). Frame delta is
 * measured with User::NTickCount(). The CGameControl class must NOT inherit
 * CActive directly: both CCoeControl and CActive derive from CBase, which
 * would make CBase ambiguous.
 */

#include <aknapp.h>
#include <akndoc.h>
#include <aknappui.h>
#include <coecntrl.h>
#include <w32std.h>
#include <fbs.h>
#include <e32base.h>
#include <mdaaudiooutputstream.h>

#include "Engine.h"
#include "Renderer.h"
#include "Assets.h"
#include "Audio.h"
#include "Network.h"
#include "Game.h"

class CGameControl;

class CGameApplication : public CAknApplication
    {
public:
    TUid AppDllUid() const;

protected:
    CApaDocument* CreateDocumentL();
    };

class CGameDocument : public CAknDocument
    {
public:
    static CGameDocument* NewL(CEikApplication* aApplication);
    ~CGameDocument();

    CEikAppUi* CreateAppUiL();

private:
    CGameDocument(CEikApplication* aApplication);
    void ConstructL();
    };

class CGameAppUi : public CAknAppUi
    {
public:
    void ConstructL();
    ~CGameAppUi();

    TKeyResponse HandleKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType);
    void HandleCommandL(TInt aCommand);
    void HandleClientRectChange();

private:
    CGameControl* iControl;
    };

/* Direct screen access driven control: renders into an off-screen RGB565
 * frame buffer and blits it with a CWindowGc, then schedules the next frame
 * with RTimer::After. */
class CGameControl : public CCoeControl, public MDirectScreenAccess
    {
public:
    static CGameControl* NewL(const TRect& aRect, CGameAppUi& aAppUi);
    ~CGameControl();

    void ConstructL(const TRect& aRect);
    void StartL();
    void Stop();

    void Draw(const TRect& aRect) const;

    /* MDirectScreenAccess */
    void AbortNow(RDirectScreenAccess::TTerminationReasons aReason);
    void Restart(RDirectScreenAccess::TTerminationReasons aReason);

    TBool MapKey(const TKeyEvent& aKeyEvent, TBool aDown);

    /* Called by the nested frame timer. */
    void DoFrame();

private:
    CGameControl(CGameAppUi& aAppUi);
    void BeginDirectScreenAccessL();
    void StopDirectScreenAccess();
    void BlitFrame();
    void RequestNextFrame();
    void SetupFrameRate();

    class CFrameTimer : public CActive
        {
    public:
        static CFrameTimer* NewL(CGameControl& aOwner);
        ~CFrameTimer();
        void Start(TInt aIntervalUs);
        void Stop();
        void RunL();
        void DoCancel();
    private:
        CFrameTimer(CGameControl& aOwner);
        CGameControl& iOwner;
        RTimer iTimer;
        };

private:
    CGameAppUi& iAppUi;
    RFs iFileServer;
    RSocketServ iSocketServer;
    CDirectScreenAccess* iDirectScreenAccess;
    CFbsScreenDevice* iScreenDevice;
    CBitmapContext* iBitmapContext;
    CFbsBitmap* iDsaBitmap;
    TUint16* iRenderFrame;
    TInt iRenderWidth;
    TInt iRenderHeight;
    TInt iScreenWidth;
    TInt iScreenHeight;
    TRect iDrawingRect;
    TInt iOrientation;
    TUint32 iLastFrameTick;
    CFrameTimer* iFrameTimer;
    TBool iRunning;
    TBool iPaused;
    TInt iFrameIntervalUs;

    CAssetManager* iAssets;
    CRenderer* iRenderer;
    CAudio* iAudio;
    class CAudioObserverBridge* iAudioBridge;
    CGame* iGame;
    HBufC* iAssetsPathBuffer;
    };

#endif
