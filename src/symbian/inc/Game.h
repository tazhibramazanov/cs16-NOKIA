#ifndef CS16_GAME_H
#define CS16_GAME_H

/*
 * Game.h -- игровой слой: порт src/core/cs/Game.java, Move.java, Player.java,
 * Bot.java, Nav.java и Wp.java. Состояния, меню, настройки, раунды, оружие,
 * боты, бомба, HUD, Auto resolution и Nokia FR живут здесь.
 *
 *Igrovye dannye hranjatsja v fiksirovanh maslivah: v igrovom cikle net
 * allocatee, vse massivy i stroki podgotovleny v ConstructL.
 */

#include "Renderer.h"
#include "Audio.h"
#include "Network.h"
#include "LangData.h"
#include "WeaponData.h"

const TInt KMaxPlayers = 16;
const TInt KMaxGrenades = 16;
const TInt KMaxDrops = 24;
const TInt KMaxEffects = 48;
const TInt KMaxChatLines = 8;
const TInt KMaxSettings = 64;

enum TGameState
    {
    EStateMenu = 0,
    EStateLoad = 1,
    EStatePlay = 2,
    EStateEnd = 3
    };

enum TMenuId
    {
    EMenuNone = -1,
    EMenuMain = 0,
    EMenuCreate = 1,
    EMenuTeam = 2,
    EMenuOptions = 3,
    EMenuPause = 4,
    EMenuHelp = 5,
    EMenuGraphics = 6,
    EMenuControls = 7,
    EMenuSound = 8,
    EMenuFirstRun1 = 9,
    EMenuFirstRun2 = 10,
    EMenuGame = 11,
    EMenuRadio1 = 12,
    EMenuRadio2 = 13,
    EMenuRadio3 = 14,
    EMenuBinds = 15,
    EMenuMultiplayer = 16,
    EMenuSearch = 17,
    EMenuLan = 18,
    EMenuLanguage = 19,
    EMenuWorld = 20
    };

/* Биты действий совпадают с Game.A_* и Binds.ACT из Java-версии. */
enum TGameAction
    {
    EActionNone = 0,
    EActionForward = 1,
    EActionBack = 2,
    EActionTurnLeft = 4,
    EActionTurnRight = 8,
    EActionStrafeLeft = 16,
    EActionStrafeRight = 32,
    EActionLookUp = 64,
    EActionLookDown = 128,
    EActionFire = 256,
    EActionJump = 512,
    EActionDuck = 1024,
    EActionReload = 2048,
    EActionUse = 4096,
    EActionNextWeapon = 8192,
    EActionAlt = 16384,
    EActionBuy = 32768,
    EActionMenu = 65536,
    EActionScore = 131072,
    EActionMenuUp = 262144,
    EActionMenuDown = 524288,
    EActionSelect = 1048576,
    EActionLastWeapon = 1 << 21,
    EActionDrop = 1 << 22,
    EActionRadio = 1 << 23,
    EActionMenuLeft = 1 << 24,
    EActionMenuRight = 1 << 25,
    EActionEscape = 1 << 26,
    EActionTabLeft = 1 << 27,
    EActionTabRight = 1 << 28,
    EActionVoice = 1 << 29,
    EActionAltLeft = EActionTabLeft,
    EActionAltRight = EActionTabRight
    };

struct TPlayer
    {
    TInt iId;
    TBool iBot;
    TInt iTeam;             /* 1 = T, 2 = CT */
    TText iName[24];
    TBool iAlive;
    TReal32 iX, iY, iZ;
    TReal32 iVelocityX, iVelocityY, iVelocityZ;
    TReal32 iYaw, iPitch;
    TReal32 iHealth, iArmor;
    TInt iMoney;
    TBool iDucked;
    TBool iOnGround;
    TBool iOnLadder;
    TInt iWeapon;           /* текущее оружие */
    TInt iPrimary;
    TInt iSecondary;
    TInt iClip[29];
    TInt iAmmo[29];
    TBool iOwns[29];
    TReal32 iNextShot;
    TReal32 iReloadUntil;
    TReal32 iLastShot;
    TReal32 iDrawEnd;       /* время окончания анимации смены оружия */
    TBool iReloading;
    TInt iZoom;
    TBool iScope;
    TInt iKills, iDeaths;
    TReal32 iDeathTime;
    TReal32 iFlashTime;
    TReal32 iAnimTime;
    TInt iGait;
    TInt iRadio;
    TReal32 iLastRadio;
    TInt iNetId;
    TBool iHasC4;
    TBool iPlanting;
    TReal32 iPlantProgress;
    TInt iBotGoal;
    TReal32 iBotThinkTime;
    TReal32 iBotAimX, iBotAimY;
    TReal32 iSpeedFactor;
    TInt iModelId;
    TReal32 iViewBob;
    TBool iLastShotSilent;
    TInt iRandomSeed;
    };

struct TGrenade
    {
    TBool iActive;
    TInt iType;             /* 25 HE, 26 flash, 27 smoke */
    TInt iOwner;
    TReal32 iX, iY, iZ;
    TReal32 iVelocityX, iVelocityY, iVelocityZ;
    TReal32 iTimer;
    };

struct TDrop
    {
    TBool iActive;
    TInt iWeapon;
    TReal32 iX, iY, iZ;
    TReal32 iVelocityX, iVelocityY, iVelocityZ;
    TReal32 iTime;
    };

struct TEffect
    {
    TBool iActive;
    TInt iType;             /* 0 sprite, 1 tracer, 2 blood, 3 shell, 4 smoke */
    TReal32 iX, iY, iZ;
    TReal32 iVX, iVY, iVZ;
    TReal32 iStart, iLife;
    TInt iColor;
    TInt iSize;
    };

/* Данные, которые нужны HUD и отрисовке моделей, но хранятся в приватных
   структурах игрового слоя. Копируются одним проходом без аллокаций. */
struct TPlayerViewExtra
    {
    TBool iHelmet, iDefuser, iC4, iPlanting, iDefusing, iZombie;
    TInt iFlash, iHe, iSmoke;
    TReal32 iNextAttack, iFlashUntil, iDefuseProgress;
    TInt iDeathAnim;
    TBool iLastShotSilent, iSilM4, iSilUSP;
    TInt iLast;
    TReal32 iPunchYaw, iPunchPitch, iRecoil;
    TInt iVmWeapon, iVmSet, iVmAnimation;
    TReal32 iVmTime, iVmLength;
    TText iVmName[32];
    };

class CGame : public MAudioObserver
    {
public:
    static CGame* NewL(CRenderer& aRenderer, CAssetManager& aAssets,
                       CAudio& aAudio, RSocketServ& aSocketServer,
                       TInt aWidth, TInt aHeight);
    ~CGame();

    void ConstructL();
    void InitL();

    void Tick(TReal32 aDelta);
    void Draw();
    void DrawFrame();

    void KeyMask(TInt aMask, TBool aDown);
    TBool RawKey(TInt aCode);
    TBool WantsDigits() const;
    TBool TurnDigits() const;
    void Digit(TInt aDigit);
    void Look(TReal32 aDeltaYaw, TReal32 aDeltaPitch);
    void PauseFromSystem();
    void Resize(TInt aWidth, TInt aHeight);
    void SetOrientation(TInt aOrientation);
    TBool FpsCap20() const { return iFpsCap20; }
    TInt CurrentFps() const { return iFps; }

    /* MAudioObserver */
    void VoiceRecorded(const TUint8* aPcm, TInt aLength);
    void AudioError(TInt aError);

    /* Публичные настройки для меню и рендера. */
    TInt iQuality;          /* 0 auto, 1 high, 2 medium, 3 low */
    TInt iGfx;              /* 0..3 as in Game.gfx */
    TInt iRenderLevel;      /* 0..4 -> LW[] */
    TInt iFpsCap20;
    TInt iGfxSetting;
    TInt iNokiaFrameRate;   /* NFR_T индекс */
    TInt iOrientation;
    TInt iVolume;
    TBool iSoundOn;
    TBool iShowFps;
    TInt iLanguage;         /* Lang.EN / Lang.RU */
    TInt iSensitivity;
    TInt iSensitivityVertical;
    TBool iInvertY;
    TInt iLayout;
    TInt iDifficulty;
    TInt iMaxRounds;
    TBool iAutoAim;
    TInt iGammaIndex;
    TInt iCrossColor;
    TInt iCrossSize;
    TBool iCrossDynamic;
    TBool iViewBob;
    TBool iRadar;
    TInt iRenderMode;       /* 0=auto,1=gpu,2=cpu */
    TBool iGpuAvailable;
    TBool iFirstRunDone;
    TBool iVoiceOn;
    TInt iVoiceVolume;
    TInt iWinScale;
    TInt iMapIndex;
    TInt iTeamChoice;
    TInt iBotCount;
    TInt iRoundSeconds;
    TInt iBuySeconds;
    TInt iFreezeSeconds;
    TInt iStartMoney;
    TBool iFriendlyFire;
    TBool iPcMouse;

    TInt iState;
    TInt iMenu;
    TInt iMenuSelection;
    TBool iBuyOpen;
    TInt iBuyCategory;
    TBool iScoreOpen;

    TInt iWidth;
    TInt iHeight;
    TReal32 iTime;
    TInt iFps;
    TReal32 iRealDelta;
    TBool iPaused;
    TBool iTurned;

    CRenderer& iRenderer;
    CAssetManager& iAssets;
    CAudio& iAudio;
    RSocketServ& iSocketServer;

    TPlayer iPlayers[KMaxPlayers];
    TInt iPlayerCount;
    TInt iMyIndex;
    TInt iSpectatorIndex;

    TInt iRound;
    TInt iScoreT;
    TInt iScoreCT;
    TReal32 iRoundStart;
    TReal32 iFreezeEnd;
    TReal32 iBuyEnd;
    TReal32 iRoundEndAt;
    TBool iRoundOver;
    TBool iBombPlanted;
    TBool iBombDropped;
    TBool iBombExploded;
    TReal32 iBombX, iBombY, iBombZ, iBombTime, iBombBeep;
    TInt iBombSite;

    TGrenade iGrenades[KMaxGrenades];
    TDrop iDrops[KMaxDrops];
    TEffect iEffects[KMaxEffects];

    TNetBuffer iNetOut;
    CNetConnection* iConnection;
    CLanFinder* iLanFinder;
    CHttpClient* iHttpClient;
    TBool iNetworkHost;

    TText iCenterMessage[256];
    TReal32 iCenterMessageUntil;
    TInt iCenterMessageColor;
    TText iChatLines[KMaxChatLines][160];
    TInt iChatColors[KMaxChatLines];
    TInt iChatHead;
    TInt iKillFeed[KMaxChatLines][3];
    TReal32 iKillFeedTime[KMaxChatLines];

    TReal32 iHurtFlash;
    TReal32 iMuzzleUntil;
    TReal32 iViewBobAmount; /* текущая фаза камеры; iViewBob -- настройка */
    TReal32 iTurnHeld;
    TInt iLightLevel;
    TInt iMyLight;
    TReal32 iDeadCameraYaw;

    /* Nokia FR: кэш последнего кадра для репроекции. */
    TUint16* iGeneratedFrame;
    TInt iGeneratedWidth;
    TInt iGeneratedHeight;
    TReal32 iGeneratedYaw;
    TReal32 iGeneratedPitch;
    TBool iGeneratedValid;
    TInt iFramesGenerated;
    TInt iRenderSkip;

    TRandom iRandom;
    TInt iKeys;
    TInt iBindCapture;

public:
    CGame(CRenderer& aRenderer, CAssetManager& aAssets, CAudio& aAudio,
          RSocketServ& aSocketServer, TInt aWidth, TInt aHeight);

    void LoadSettings();
    void SaveSettings();
    void ApplyGfx();
    void SetLevel(TInt aLevel);
    void ApplyGamma();
    void CycleMod(TInt aDirection);
    void StartMatch(TInt aMapIndex, TInt aTeam);
    void LoadMapL(const TDesC& aName);
    void SetupPlayers(TInt aHumanTeam);
    void SetupEntities();
    void NewRound(TBool aFirst);
    void CheckRoundEnd();
    void EndRound(TInt aWinner, const TDesC& aMessage, const TPtrC8& aSound, TInt aReason);
    void AddMoney(TPlayer& aPlayer, TInt aValue);

    void HumanInput(TReal32 aDelta);
    void UpdatePlayer(TPlayer& aPlayer, TReal32 aDelta);
    void BotThink(TPlayer& aPlayer, TReal32 aDelta);
    void MovePlayer(TPlayer& aPlayer, TReal32 aDelta);
    TReal32 MaxSpeed(const TPlayer& aPlayer) const;
    void AimAssist(TPlayer& aPlayer, TReal32 aDelta);
    void Weapons(TPlayer& aPlayer, TReal32 aDelta);
    void SwitchWeapon(TPlayer& aPlayer, TInt aWeapon);
    void NextWeapon(TPlayer& aPlayer);
    void SelectSlot(TPlayer& aPlayer, TInt aSlot);
    void StartReload(TPlayer& aPlayer);
    void AlternateFire(TPlayer& aPlayer);
    void Shoot(TPlayer& aPlayer);
    void Bullet(TPlayer& aPlayer, TInt aWeapon, TReal32 aOriginX, TReal32 aOriginY,
                TReal32 aOriginZ, TReal32 aDirectionX, TReal32 aDirectionY,
                TReal32 aDirectionZ, TReal32 aRange, TInt aBaseDamage);
    TInt RayHit(TPlayer& aPlayer, TReal32 aOriginX, TReal32 aOriginY, TReal32 aOriginZ,
                TReal32 aDirectionX, TReal32 aDirectionY, TReal32 aDirectionZ,
                TReal32 aMaximumDistance);
    void Knife(TPlayer& aPlayer, TBool aStab);
    void Damage(TPlayer& aVictim, TPlayer* aAttacker, TInt aDamage,
                TInt aWeapon, TBool aHeadshot, TInt aGroup);
    void Kill(TPlayer& aVictim, TPlayer* aAttacker, TInt aWeapon, TBool aHeadshot);
    void UpdateDrops(TReal32 aDelta);
    void UpdateGrenades(TReal32 aDelta);
    void UpdateEffects(TReal32 aDelta);
    void AddEffect(TReal32 aX, TReal32 aY, TReal32 aZ, TInt aColor,
                   TReal32 aLife, TInt aSize, TInt aType);
    void DropCurrent(TPlayer& aPlayer);
    void SpawnDrop(TPlayer& aPlayer, TInt aWeapon, TReal32 aSpeed);
    void Radio(TPlayer& aPlayer, TInt aCommand);
    void Chat(const TDesC& aLine, TInt aColor);
    void Center(const TDesC& aLine, TReal32 aDuration, TInt aColor);
    void PlaySound(const TPtrC8& aName, TInt aVolume);
    void PlaySoundAt(TReal32 aX, TReal32 aY, TReal32 aZ, const TPtrC8& aName,
                     TInt aVolume, TReal32 aRange);

    void MenuKey(TInt aAction);
    TBool ChangeOption(TInt aDirection);
    TInt MenuTitle(TDes& aBuffer);
    void CopyViewExtras(TPlayerViewExtra* aDestination) const;
    void DrawHud();
    void DrawWorldEffects();
    void DrawPlayers(const TPlayer& aViewer);
    TBool DrawViewModel3D(const TPlayer& aViewer, TReal32 aBob);
    void DrawViewModel(const TPlayer& aViewer, TReal32 aBob);
    void DrawScope();
    void DrawMuzzle(const TPlayer& aViewer);
    void DrawMenu();
    void DrawLoading();
    void DrawEndScreen();
    void DrawRadar(const TPlayer& aViewer);
    void DrawCrosshair(const TPlayer& aViewer);
    void ApplyNokiaFrameRate(TReal32 aDeltaYaw, TReal32 aDeltaPitch);
    void ReprojectFrame(TReal32 aDeltaYaw, TReal32 aDeltaPitch);
    void ResampleToScreen();

    TBool AnyAlive(TInt aTeam) const;
    TInt CountAlive(TInt aTeam) const;
    TPlayer* PlayerById(TInt aId);
    TReal32 GroundHeight(TReal32 aX, TReal32 aY, TReal32 aZ) const;
    void NetworkTick(TReal32 aDelta);
    void VoiceTick(TReal32 aDelta);

public:
    TInt iPendingMap;
    TInt iPendingTeam;
    TText iLoadMessage[128];
    TInt iLoadStage;
    TText iMapName[32];
    TInt iRoundTimeSetting;
    TReal32 iBuyTimeSetting;
    TReal32 iFreezeSetting;
    TReal32 iCenterMessageTime;
    TReal32 iBombDropTime;
    TInt iBombDroppedBy;
    TBool iInfected;
    TReal32 iInfectAt;
    TInt iMode;              /* 0 bomb, 1 deathmatch, 2 zombie, 3 terminator, 4 hostage */
    /* Terminator mode */
    TReal32 iTermCircleX, iTermCircleY, iTermCircleZ;
    TBool iTermCircleActive;
    TInt iTerminatorIdx;
    TInt iTerminatorClass; /* 1=T-800,2=T-1000,3=T-X */
    TReal32 iDisguiseTimer;
    TReal32 iDisguiseCooldown;
    TBool iIsDisguised;
    TInt iKeyDisguise; /* key code for disguise (default '*') */
    /* Hostage Rescue mode */
    static const TInt KMaxHostages = 4;
    TReal32 iHostageX[KMaxHostages];
    TReal32 iHostageY[KMaxHostages];
    TReal32 iHostageZ[KMaxHostages];
    TBool iHostageAlive[KMaxHostages];
    TBool iHostageRescued[KMaxHostages];
    TInt iHostageFollowIdx[KMaxHostages]; /* player index following, -1 if none */
    TReal32 iRescueZoneX, iRescueZoneY, iRescueZoneZ;
    TInt iOptReturn;
    TInt iOptReturnSelection;
    TInt iHelpReturn;
    TInt iBindsReturn;
    TReal32 iBindUntil;
    TInt iMenuTab;
    TInt iKeysDown;
    TReal32 iAutoAccumulator;
    TInt iAutoFrames;
    TReal32 iAutoCooldown;
    TText iStatusLine[160];
    TInt iDifficultyName;
    TBool iSpectating;
    };

#endif
