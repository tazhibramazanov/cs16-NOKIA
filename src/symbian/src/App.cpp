/*
 * App.cpp -- Symbian application skeleton for CS16_E52 (S60 3rd FP2, GCCE).
 *
 * CAknApplication -> CAknDocument -> CAknAppUi -> CGameControl (DSA + frame
 * loop). The frame loop is driven by a nested CFrameTimer active object that
 * re-arms an RTimer::After; the frame delta is measured with
 * User::NTickCount().
 */

#include "App.h"

#include <e32keys.h>
#include <eikstart.h>
#include <e32std.h>

/* CAudioObserver: forwards audio events into the game logic. */
class CAudioObserverBridge : public MAudioObserver
    {
public:
    CAudioObserverBridge() : iGame(NULL) {}
    void SetGame(CGame* aGame) { iGame = aGame; }
    void VoiceRecorded(const TUint8* aPcm, TInt aLength)
        { if (iGame) iGame->VoiceRecorded(aPcm, aLength); }
    void AudioError(TInt aError)
        { if (iGame) iGame->AudioError(aError); }
private:
    CGame* iGame;
    };

static CApaApplication* NewApplication();

/* --------------------------------------------------------------- app ----- */

TUid CGameApplication::AppDllUid() const { return TUid::Uid(0x2003C516); }

CApaDocument* CGameApplication::CreateDocumentL()
    {
    return CGameDocument::NewL(this);
    }

/* ------------------------------------------------------------ document --- */

CGameDocument::CGameDocument(CEikApplication* aApplication)
    : CAknDocument(*aApplication)
    {
    }

CGameDocument* CGameDocument::NewL(CEikApplication* aApplication)
    {
    CGameDocument* self = new (ELeave) CGameDocument(aApplication);
    CleanupStack::PushL(self);
    self->ConstructL();
    CleanupStack::Pop(self);
    return self;
    }

CGameDocument::~CGameDocument() {}

void CGameDocument::ConstructL() {}

CEikAppUi* CGameDocument::CreateAppUiL()
    {
    return new (ELeave) CGameAppUi;
    }

/* --------------------------------------------------------------- app ui -- */

void CGameAppUi::ConstructL()
    {
    BaseConstructL(EAknEnableSkin);
    TRect rect = ClientRect();
    iControl = CGameControl::NewL(rect, *this);
    AddToStackL(iControl);
    iControl->StartL();
    }

CGameAppUi::~CGameAppUi()
    {
    if (iControl)
        {
        iControl->Stop();
        RemoveFromStack(iControl);
        delete iControl;
        iControl = NULL;
        }
    }

TKeyResponse CGameAppUi::HandleKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType)
    {
    if (!iControl) return EKeyWasNotConsumed;
    if (aType == EEventKey || aType == EEventKeyDown || aType == EEventKeyUp)
        {
        if (iControl->MapKey(aKeyEvent, aType != EEventKeyUp))
            return EKeyWasConsumed;
        }
    return EKeyWasNotConsumed;
    }

void CGameAppUi::HandleCommandL(TInt aCommand)
    {
    switch (aCommand)
        {
        case EAknSoftkeyExit:
        case EEikCmdExit:
            Exit();
            break;
        case EAknSoftkeyBack:
            if (iControl) iControl->Stop();
            break;
        default:
            break;
        }
    }

void CGameAppUi::HandleClientRectChange()
    {
    if (iControl) iControl->SetRect(ClientRect());
    }

/* ----------------------------------------------------------- frame timer - */

CGameControl::CFrameTimer::CFrameTimer(CGameControl& aOwner)
    : CActive(CActive::EPriorityIdle), iOwner(aOwner)
    {
    }

CGameControl::CFrameTimer* CGameControl::CFrameTimer::NewL(CGameControl& aOwner)
    {
    CFrameTimer* self = new (ELeave) CFrameTimer(aOwner);
    CleanupStack::PushL(self);
    self->iTimer.CreateLocal();
    CleanupStack::Pop(self);
    return self;
    }

CGameControl::CFrameTimer::~CFrameTimer()
    {
    Cancel();
    iTimer.Close();
    }

void CGameControl::CFrameTimer::Start(TInt aIntervalUs)
    {
    if (aIntervalUs < 1000) aIntervalUs = 1000;
    Cancel();
    iTimer.After(iStatus, aIntervalUs);
    SetActive();
    }

void CGameControl::CFrameTimer::Stop()
    {
    Cancel();
    }

void CGameControl::CFrameTimer::RunL()
    {
    iOwner.DoFrame();
    }

void CGameControl::CFrameTimer::DoCancel()
    {
    iTimer.Cancel();
    }

/* ------------------------------------------------------------- control --- */

CGameControl::CGameControl(CGameAppUi& aAppUi)
    : CCoeControl(), MDirectScreenAccess(),
      iAppUi(aAppUi), iDirectScreenAccess(NULL),
      iScreenDevice(NULL), iBitmapContext(NULL), iDsaBitmap(NULL), iRenderFrame(NULL),
      iRenderWidth(0), iRenderHeight(0), iScreenWidth(240), iScreenHeight(320),
      iOrientation(0), iLastFrameTick(0), iFrameTimer(NULL),
      iRunning(EFalse), iPaused(EFalse), iFrameIntervalUs(33333),
      iAssets(NULL), iRenderer(NULL), iAudio(NULL), iAudioBridge(NULL),
      iGame(NULL), iAssetsPathBuffer(NULL)
    {
    }

CGameControl* CGameControl::NewL(const TRect& aRect, CGameAppUi& aAppUi)
    {
    CGameControl* self = new (ELeave) CGameControl(aAppUi);
    CleanupStack::PushL(self);
    self->ConstructL(aRect);
    CleanupStack::Pop(self);
    return self;
    }

void CGameControl::ConstructL(const TRect& aRect)
    {
    CreateWindowL();
    SetRect(aRect);
    EnableDragEvents();

    User::LeaveIfError(iFileServer.Connect());
    User::LeaveIfError(iSocketServer.Connect());

    EngineInitialize();

    /* Assets live next to the executable: <drive>\data\cs16\ */
    TFileName processName = RProcess().FileName();
    TParsePtrC parsed(processName);
    iAssetsPathBuffer = HBufC::NewL(64);
    iAssetsPathBuffer->Des().Copy(parsed.Drive());
    iAssetsPathBuffer->Des().Append(_L("\\data\\cs16\\"));

    iAssets = new (ELeave) CAssetManager;
    iAssets->ConstructL(iFileServer, iAssetsPathBuffer->Des());

    iScreenWidth = aRect.Width();
    iScreenHeight = aRect.Height();
    if (iScreenWidth <= 0) iScreenWidth = 240;
    if (iScreenHeight <= 0) iScreenHeight = 320;
    iOrientation = iScreenWidth > iScreenHeight ? 1 : 0;

    iRenderWidth = iScreenWidth;
    iRenderHeight = iScreenHeight;
    iRenderFrame = new (ELeave) TUint16[iRenderWidth * iRenderHeight];
    Mem::FillZ(iRenderFrame, iRenderWidth * iRenderHeight * sizeof(TUint16));

    iRenderer = new (ELeave) CRenderer(iRenderWidth, iRenderHeight, iRenderFrame);
    iAudioBridge = new (ELeave) CAudioObserverBridge;
    iAudio = CAudio::NewL(iFileServer, iAssetsPathBuffer->Des(), *iAudioBridge);
    iAudio->SetModDirectory(iAssets->CurrentModPath());

    iGame = CGame::NewL(*iRenderer, *iAssets, *iAudio, iSocketServer,
                        iRenderWidth, iRenderHeight);
    iAudioBridge->SetGame(iGame);
    iGame->SetOrientation(iOrientation);
    iGame->ConstructL();
    iGame->InitL();

    iFrameTimer = CFrameTimer::NewL(*this);

    BeginDirectScreenAccessL();
    }

CGameControl::~CGameControl()
    {
    Stop();
    delete iFrameTimer;
    iFrameTimer = NULL;
    delete iGame;
    iGame = NULL;
    delete iAudio;
    iAudio = NULL;
    delete iAudioBridge;
    iAudioBridge = NULL;
    delete iRenderer;
    iRenderer = NULL;
    delete iDsaBitmap;
    iDsaBitmap = NULL;
    delete[] iRenderFrame;
    iRenderFrame = NULL;
    delete iAssets;
    iAssets = NULL;
    delete iAssetsPathBuffer;
    iAssetsPathBuffer = NULL;
    iSocketServer.Close();
    iFileServer.Close();
    }

void CGameControl::BeginDirectScreenAccessL()
    {
    StopDirectScreenAccess();
    iDirectScreenAccess = CDirectScreenAccess::NewL(iCoeEnv->WsSession(),
                                                    *iCoeEnv->ScreenDevice(),
                                                    Window(), *this);
    iDirectScreenAccess->StartL();
    iBitmapContext = iDirectScreenAccess->Gc();
    iScreenDevice = static_cast<CFbsScreenDevice*>(iBitmapContext->Device());
    {
    RRegion* reg = iDirectScreenAccess->DrawingRegion();
    if (reg) iDrawingRect = reg->BoundingRect();
    else iDrawingRect = Window().Size();
    }
    iScreenWidth = iDrawingRect.Width();
    iScreenHeight = iDrawingRect.Height();
    if (iScreenWidth <= 0) iScreenWidth = 240;
    if (iScreenHeight <= 0) iScreenHeight = 320;
    }

void CGameControl::StopDirectScreenAccess()
    {
    if (iDirectScreenAccess)
        {
        iDirectScreenAccess->Cancel();
        delete iDirectScreenAccess;
        iDirectScreenAccess = NULL;
        }
    iBitmapContext = NULL;
    iScreenDevice = NULL;
    }

void CGameControl::StartL()
    {
    if (iRunning) return;
    iRunning = ETrue;
    iPaused = EFalse;
    iLastFrameTick = User::NTickCount();
    RequestNextFrame();
    }

void CGameControl::Stop()
    {
    iRunning = EFalse;
    if (iFrameTimer) iFrameTimer->Stop();
    StopDirectScreenAccess();
    if (iGame) iGame->PauseFromSystem();
    }

void CGameControl::RequestNextFrame()
    {
    if (!iRunning || !iFrameTimer) return;
    SetupFrameRate();
    iFrameTimer->Start(iFrameIntervalUs);
    }

void CGameControl::SetupFrameRate()
    {
    if (!iGame) return;
    static const TInt KNokiaFrameRates[] = { 0, 10, 15, 20, 25, 30 };
    TInt hertz = 0;
    if (iGame->iNokiaFrameRate > 0 && iGame->iNokiaFrameRate <= 5)
        hertz = KNokiaFrameRates[iGame->iNokiaFrameRate];
    if (hertz <= 0)
        hertz = iGame->FpsCap20() ? 20 : 30;
    if (hertz <= 0) hertz = 20;
    iFrameIntervalUs = 1000000 / hertz;
    if (iFrameIntervalUs < 16000) iFrameIntervalUs = 16000;
    }

void CGameControl::DoFrame()
    {
    if (!iRunning) return;

    TUint32 now = User::NTickCount();
    TUint32 sinceLast = now - iLastFrameTick;
    if (sinceLast > 200) sinceLast = 200;
    iLastFrameTick = now;

    if (iGame)
        {
        if (!iPaused)
            {
            /* NTickCount ticks at 64 kHz on S60. */
            TReal32 delta = (TReal32)sinceLast / 64000.0f;
            iGame->Tick(delta);
            }
        iGame->DrawFrame();
        }

    BlitFrame();
    RequestNextFrame();
    }

void CGameControl::BlitFrame()
    {
    if (!iRenderFrame || !iDirectScreenAccess) return;
    iBitmapContext = iDirectScreenAccess->Gc();
    if (!iBitmapContext) return;

    if (!iDsaBitmap)
        {
        iDsaBitmap = new (ELeave) CFbsBitmap;
        if (iDsaBitmap->Create(TSize(iRenderWidth, iRenderHeight),
                               EColor16) != KErrNone)
            {
            delete iDsaBitmap;
            iDsaBitmap = NULL;
            return;
            }
        }

    TUint16* bits = (TUint16*)iDsaBitmap->DataAddress();
    if (!bits) return;

    TInt stride = iDsaBitmap->DataStride();
    TInt copyWidth = iRenderWidth;
    if (stride < copyWidth) copyWidth = stride;
    TInt copyHeight = iRenderHeight;
    if (copyHeight > iScreenHeight) copyHeight = iScreenHeight;

    for (TInt y = 0; y < copyHeight; y++)
        {
        Mem::Copy(bits + y * stride, iRenderFrame + y * iRenderWidth,
                  copyWidth * (TInt)sizeof(TUint16));
        }

    iBitmapContext->BitBlt(TPoint(0, 0), iDsaBitmap);
    }

void CGameControl::Draw(const TRect& /*aRect*/) const {}

void CGameControl::AbortNow(RDirectScreenAccess::TTerminationReasons /*aReason*/)
    {
    iPaused = ETrue;
    if (iGame) iGame->PauseFromSystem();
    StopDirectScreenAccess();
    }

void CGameControl::Restart(RDirectScreenAccess::TTerminationReasons /*aReason*/)
    {
    TRAPD(error, BeginDirectScreenAccessL());
    if (error == KErrNone) iPaused = EFalse;
    }

/* --------------------------------------------------------------- keys ---- */

TBool CGameControl::MapKey(const TKeyEvent& aKeyEvent, TBool aDown)
    {
    if (!iGame) return EFalse;
    TInt code = aKeyEvent.iCode;
    switch (aKeyEvent.iScanCode)
        {
        case EStdKeyUpArrow:    code = 0x101; break;
        case EStdKeyDownArrow:  code = 0x102; break;
        case EStdKeyLeftArrow:  code = 0x103; break;
        case EStdKeyRightArrow: code = 0x104; break;
        case EStdKeyDevice3:    code = 0x105; break;
        case EStdKeyDevice0:    code = 0x10A; break;
        case EStdKeyDevice1:    code = 0x106; break;
        case EStdKeyYes:        code = 0x107; break;
        case EStdKeyNo:         code = 0x108; break;
        case EStdKeyBackspace:  code = 0x109; break;
        default: break;
        }
    if (iGame->RawKey(code)) return ETrue;
    if (iGame->WantsDigits() && code >= '0' && code <= '9')
        {
        if (aDown) iGame->Digit(code - '0');
        return ETrue;
        }
    TInt mask = 0;
    switch (code)
        {
        case '2': mask = EActionForward; break;
        case '8': mask = EActionBack; break;
        case '4': mask = EActionStrafeLeft; break;
        case '6': mask = EActionStrafeRight; break;
        case '5': mask = EActionFire; break;
        case '1': mask = EActionJump; break;
        case '3': mask = EActionDuck; break;
        case '7': mask = EActionReload; break;
        case '0': mask = EActionUse; break;
        case '9': mask = EActionNextWeapon; break;
        case '*': mask = EActionAlt | EActionTabLeft; break;
        case '#': mask = EActionBuy | EActionTabRight; break;
        case 0x101: mask = EActionLookUp | EActionMenuUp; break;
        case 0x102: mask = EActionLookDown | EActionMenuDown; break;
        case 0x103: mask = EActionTurnLeft | EActionMenuLeft; break;
        case 0x104: mask = EActionTurnRight | EActionMenuRight; break;
        case 0x105: mask = EActionFire | EActionSelect; break;
        case 0x106: mask = EActionScore; break;
        case 0x107: mask = EActionVoice; break;
        case 0x108: mask = EActionEscape; break;
        case 0x109: mask = EActionMenu | EActionEscape; break;
        case 0x10A: mask = EActionMenu; break;
        default: break;
        }
    iGame->KeyMask(mask, aDown);
    return mask != 0;
    }

/* --------------------------------------------------------------- entry --- */

GLDEF_C TInt E32Main()
    {
    EikStart::RunApplication(NewApplication);
    return KErrNone;
    }

static CApaApplication* NewApplication()
    {
    return new CGameApplication;
    }
