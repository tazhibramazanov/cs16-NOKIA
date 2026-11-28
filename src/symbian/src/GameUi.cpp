#include "Game.h"

/*
 * GameUi.cpp -- меню, настройки, HUD и двумерная отрисовка CGame.
 * Мир, физика и сетевой протокол находятся в Game.cpp. Здесь определены
 * только объявленные в Game.h методы CGame и локальные помощники.
 * В кадровом цикле нет выделений памяти: строки, таблицы и буферы фиксированы.
 */

namespace
    {

const TInt KLanguageEnglish = 0;
const TInt KLanguageRussian = 1;
const TInt KLanguageFrench = 2;
const TInt KLanguageGerman = 3;
const TInt KLanguageJapanese = 4;
const TInt KLanguageVietnamese = 5;
const TInt KMaxMenuItems = 40;
const TInt KMenuTextLength = 112;
const TInt KMaxLanGames = 8;
const TInt KMaxWorldRooms = 8;
const TInt KMaxBinds = 22;
const TInt KMaxFramePixels = 320 * 320;
const TInt KKeyUp = 0x101;
const TInt KKeyDown = 0x102;
const TInt KKeyLeft = 0x103;
const TInt KKeyRight = 0x104;
const TInt KKeyCenter = 0x105;
const TInt KKeyRightSoft = 0x106;
const TInt KKeyYes = 0x107;
const TInt KKeyNo = 0x108;
const TInt KKeyClear = 0x109;
const TInt KKeyLeftSoft = 0x10A;

static const TInt KMapRenderWidth[5] = { 320, 280, 240, 200, 160 };
static const TInt KNfrTarget[6] = { 0, 10, 15, 20, 25, 30 };
static const TInt KOptTabs[5] = { EMenuGraphics, EMenuControls, EMenuBinds, EMenuSound, EMenuLanguage };
static const TInt KOptTabNames[5] = { 392, 277, 345, 412, 419 };
static const TInt KOptTabShort[5] = { 392, 417, 397, 412, 419 };
static const TInt KModeNames[3] = { 321, 340, 394 };
static const TInt KCrossNames[5] = { 361, 362, 410, 385, 360 };
static const TInt KDifficultyNames[4] = { 386, 363, 364, 367 };
static const TInt KGfxNames[4] = { 253, 328, 336, 237 };
static const TInt KCrossSizeIndices[3] = { 409, 365, 359 };
static const TInt KRadioNames[21] = {
    184, 222, 93, 160, 314, 69, 73, 271, 134, 54, 306, 94,
    324, 127, 155, 384, 245, 266, 76, 401, 198
    };
static const TInt KBindNames[KMaxBinds] = {
    369, 398, 305, 279, 217, 196, 232, 249, 400, 378, 325,
    256, 156, 242, 199, 162, 150, 152, 240, 183, 416, 92
    };
static const TInt KBindActions[KMaxBinds] = {
    EActionForward, EActionBack, EActionStrafeLeft, EActionStrafeRight,
    EActionTurnLeft, EActionTurnRight, EActionLookUp, EActionLookDown,
    EActionFire, EActionJump, EActionDuck, EActionReload, EActionUse,
    EActionNextWeapon, EActionLastWeapon, EActionAlt, EActionDrop,
    EActionBuy, EActionRadio, EActionEscape, EActionScore, EActionVoice
    };
static const TInt KDefaultBinds[KMaxBinds] = {
    '2', '8', '4', '6', -3, -4, -1, -2, '5', '1', '3', '7', '0', '9',
    0, '*', '#', 0, 0, -6, -7, -10
    };
static const TInt KButtonIndices[5] = { 372, 375, 326, 269, 398 };
static const TInt KBuyTitles[8] = { 302, 293, 195, 311, 349, 159, 139, 172 };
static const TInt KEquipTitles[6] = { 264, 132, 164, 123, 178, 239 };
static const TInt KEquipPrices[6] = { 650, 1000, 200, 300, 300, 200 };
static const TInt KBuyLists[5][10] = {
    { 1, 2, 3, 4, 5, 6, -1, -1, -1, -1 },
    { 7, 8, -1, -1, -1, -1, -1, -1, -1, -1 },
    { 9, 10, 11, 12, 13, -1, -1, -1, -1, -1 },
    { 14, 15, 16, 17, 18, 19, 20, 21, 22, 23 },
    { 24, -1, -1, -1, -1, -1, -1, -1, -1, -1 }
    };
static const TInt KBuyListLength[5] = { 6, 2, 5, 10, 1 };
static const char* const KMapNames[3] = { "de_dust2", "de_dust", "de_train" };
static const char* const KWeaponNames[KWeaponCount] = {
    "knife", "glock18", "usp", "p228", "deagle", "elite", "fiveseven", "m3",
    "xm1014", "mac10", "tmp", "mp5", "ump45", "p90", "galil", "famas", "ak47",
    "m4a1", "sg552", "aug", "scout", "awp", "g3sg1", "sg550", "m249",
    "hegrenade", "flashbang", "smokegrenade", "c4"
    };
static const char* const KWeaponAscii[KWeaponCount] = {
    "", "Glock 18", "USP .45", "SIG P228", "Desert Eagle", "Dual Elites",
    "Five-SeveN", "Benelli M3", "Benelli XM1014", "Ingram MAC-10", "Steyr TMP",
    "MP5 Navy", "UMP 45", "FN P90", "IMI Galil", "FAMAS", "AK-47", "Colt M4A1",
    "SG 552", "Steyr AUG", "Schmidt Scout", "AWP", "G3/SG-1", "SG 550", "M249",
    "", "", "", ""
    };
static const char* const KWeaponKillNames[KWeaponCount] = {
    "d_knife", "d_glock18", "d_usp", "d_p228", "d_deagle", "d_elite",
    "d_fiveseven", "d_m3", "d_xm1014", "d_mac10", "d_tmp", "d_mp5navy",
    "d_ump45", "d_p90", "d_galil", "d_famas", "d_ak47", "d_m4a1", "d_sg552",
    "d_aug", "d_scout", "d_awp", "d_g3sg1", "d_sg550", "d_m249", "d_grenade",
    "d_flashbang", "d_grenade", "d_skull"
    };
static const char* const KNumberNames[10] = {
    "number_0", "number_1", "number_2", "number_3", "number_4",
    "number_5", "number_6", "number_7", "number_8", "number_9"
    };
static const TInt KWeaponTitleIndices[KWeaponCount] = {
    425, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, 263, 383, 178, -1
    };
static const char* const KGaitNames[6] =
    { "idle1", "walk", "run", "crouch_idle", "crouchrun", "jump" };
static const char* const KDeathNames[7] =
    { "death1", "death2", "death3", "gutshot", "back", "head", "crouch_die" };
static const char* const KAnimationClasses[29] = {
    "knife", "onehanded", "onehanded", "onehanded", "onehanded", "dualpistols",
    "onehanded", "shotgun", "shotgun", "onehanded", "onehanded", "mp5",
    "carbine", "carbine", "ak47", "carbine", "ak47", "rifle", "rifle", "carbine",
    "rifle", "rifle", "rifle", "rifle", "m249", "grenade", "grenade", "grenade", "c4"
    };
static const TInt KHelpLines[3][8] = {
    { 13, 32, 5, 12, 9, 26, 8, 14 },
    { 23, 24, 10, 4, 39, 2, 15, 11 },
    { 16, 27, 10, 4, 39, 2, 15, 11 }
    };

/* ------------------------------------------------------------ буферы состояния */

struct TKeyBinding
    {
    TInt iFirst;
    TInt iSecond;
    };

struct TLanEntry
    {
    TInetAddr iAddress;
    TInt iPort;
    TText iName[48];
    TInt iLength;
    TInt iPlayers;
    TInt iMode;
    TInt iMap;
    };

struct TRoomEntry
    {
    TText iId[32];
    TInt iIdLength;
    TText iName[48];
    TInt iLength;
    TInt iPlayers;
    TInt iMode;
    TInt iMap;
    TUint32 iSeen;
    TBool iLive;
    };

struct TUiState
    {
    TBool iReady;
    TText iNick[32];
    TInt iNickLength;
    TText iServer[96];
    TInt iServerLength;
    TText iStatus[KMenuTextLength];
    TText iMapName[32];
    TText iLoadMessage[128];
    TInt iMpMode;
    TInt iMpMap;
    TInt iMode;
    TInt iCreateMap;
    TInt iCreateTeam;
    TInt iCreateBots;
    TInt iCreateRoundTime;
    TInt iCreateBuyTime;
    TInt iCreateFreeze;
    TInt iCreateMoney;
    TBool iCreateFriendlyFire;
    TInt iBuySelection;
    TInt iTabFocus;
    TInt iEditing;
    TInt iEditKey;
    TUint32 iEditKeyTick;
    TInt iLastRawCode;
    TUint32 iLastRawTick;
    TInt iBindCapture;
    TKeyBinding iBindings[KMaxBinds];
    TLanEntry iLan[KMaxLanGames];
    TUint32 iLanSeen[KMaxLanGames];
    TInt iLanCount;
    TRoomEntry iRooms[KMaxWorldRooms];
    TInt iRoomCount;
    TInt iRoomKeyCount;
    TUint32 iRoomRequestAt;
    TInt iMicState;
    TUint32 iMicAt;
    TInt iMicMessage;
    TUint32 iMicMessageUntil;
    TBool iNetworkRequest;
    TBool iNetworkCreate;
    TBool iNetworkLan;
    TInt iKeyRefs[30];
    TInt iHeldMasks[30];
    TUint8 iRecorded[16384];
    TInt iRecordedLength;
    TBool iMicPlayPending;
    RSocket iWorldSocket;
    TBool iWorldSocketOpen;
    TUint8 iWorldBuffer[4096];
    TInt iWorldPending;
    TInt iWorldBroker;
    TReal32 iNfrAccumulator;
    TInt iNfrConsecutive;
    TInt iNfrWidth;
    TInt iNfrHeight;
    TReal32 iNfrFov;
    TUint32 iLastApplyTick;
    };

static TUiState gUi;
void LoadUiSettings(CGame& aGame);
void CommitSettings(CGame& aGame);
void PollLan(CGame& aGame);
void CopyAsciiToText(TText* aDestination, TInt aMaximum, const char* aSource);
static TPlayerViewExtra gExtra[KMaxPlayers];
static TUint16 gNfrVmPixels[KMaxFramePixels];
static TUint8 gNfrVmMask[KMaxFramePixels];
static TInt gHudNumberId[10] = { -2, -2, -2, -2, -2, -2, -2, -2, -2, -2 };
static TInt gHudCrossId = -2;
static TInt gHudSuitId = -2;
static TInt gHudHelmetId = -2;
static TInt gHudDividerId = -2;
static TInt gHudDollarId = -2;
static TInt gHudBuyZoneId = -2;
static TInt gHudC4Id = -2;
static TInt gHudDefuserId = -2;
static TInt gHudStopwatchId = -2;
static TInt gHudHeadshotId = -2;
static const TUint32 KCrossColors[5] = { 0x32FA32u, 0xFA3232u, 0x3232FAu, 0xFAFA32u, 0x32FAFAu };
static TInt gScaleX[400];
static TInt gScaleY[400];
static TInt gScaleFractionX[400];
static TInt gScaleFractionY[400];

static CModel* gPlayerModel[2] = { NULL, NULL };
static CAnimation* gPlayerAnim[2] = { NULL, NULL };
static TBool gPlayer3DChecked = EFalse;
static TInt gGaitSequence[2][6];
static TInt gDeathSequence[2][7];
static TInt gAimSequence[2][29][2];
static TInt gShootSequence[2][29][2];
static TReal32 gPoseRotation[64 * 4];
static TReal32 gPosePosition[64 * 3];
static TReal32 gEntityMatrix[12];
static TReal32 gModelMatrix[64 * 12];
static TReal32 gMergedMatrix[64 * 12];
static CModel* gViewModel = NULL;
static CAnimation* gViewAnimation = NULL;
static TText gViewModelName[64];
static CSpriteSet gViewSprites;
static TInt gViewSpriteWeapon = -1;
static TBool gViewSpritesChecked = EFalse;
static TBool gWeaponModelChecked[KWeaponCount];
static TBool gWeaponAnimChecked[KWeaponCount];
static TInt gDrawModels[128];
static TInt gDrawModelCount = 0;
static const CMap* gDrawModelMap = NULL;

/* ------------------------------------------------------------ работа с текстом */

TInt MinI(TInt aLeft, TInt aRight)
    { return aLeft < aRight ? aLeft : aRight; }

TInt MaxI(TInt aLeft, TInt aRight)
    { return aLeft > aRight ? aLeft : aRight; }

TReal32 MaxF(TReal32 aLeft, TReal32 aRight)
    { return aLeft > aRight ? aLeft : aRight; }

TReal32 AbsF(TReal32 aValue)
    { return aValue < 0.0f ? -aValue : aValue; }

TInt BoundedIndex(TInt aIndex, TInt aCount)
    {
    if (aCount <= 0) return 0;
    return Clamp(aIndex, 0, aCount - 1);
    }

TInt AsciiLength(const char* aText)
    {
    TInt length = 0;
    while (aText && aText[length] && length < 4096) length++;
    return length;
    }

void AppendAscii(char* aDestination, TInt aMaximum, const char* aText)
    {
    TInt length = AsciiLength(aDestination);
    for (TInt i = 0; aText && aText[i] && length < aMaximum - 1; i++)
        aDestination[length++] = aText[i];
    aDestination[length] = 0;
    }

TPtrC8 Ascii(const char* aText)
    {
    return TPtrC8((const TUint8*)aText, AsciiLength(aText));
    }

TInt TextLength(const TText* aText, TInt aMaximum)
    {
    TInt length = 0;
    while (length < aMaximum && aText[length]) length++;
    return length;
    }

void CopyText(TText* aDestination, TInt aMaximum, const TDesC& aSource)
    {
    TInt length = Min(aSource.Length(), aMaximum - 1);
    Mem::Copy(aDestination, aSource.Ptr(), length * sizeof(TText));
    aDestination[length] = 0;
    }

void CopyTextToAscii(char* aDestination, TInt aMaximum, const TDesC& aSource)
    {
    TInt length = Min(aSource.Length(), aMaximum - 1);
    for (TInt i = 0; i < length; i++)
        aDestination[i] = aSource[i] < 128 ? (char)aSource[i] : '?';
    aDestination[length] = 0;
    }

TBool SameAsciiText(const TText* aText, const char* aAscii)
    {
    if (!aText || !aAscii) return EFalse;
    TInt i = 0;
    while (aText[i] && aAscii[i])
        {
        if ((char)aText[i] != aAscii[i]) return EFalse;
        i++;
        }
    return aText[i] == 0 && aAscii[i] == 0;
    }

TPtrC LocalText(TInt aIndex, TInt aLanguage)
    {
    if (aIndex < 0 || aIndex >= KLangCount) return TPtrC((const TText*)L"");
    if (aLanguage == KLanguageRussian) return TPtrC(KRussianText[aIndex]);
    return TPtrC(KEnglishText[aIndex]);
    }

void AppendLimited(TDes& aDestination, const TDesC& aSource)
    {
    TInt available = aDestination.MaxLength() - aDestination.Length();
    if (available > 0) aDestination.Append(aSource.Left(available));
    }

void AppendLocal(TDes& aDestination, TInt aIndex, TInt aLanguage)
    {
    if (aIndex >= 0 && aIndex < KLangCount) AppendLimited(aDestination, LocalText(aIndex, aLanguage));
    }

void AppendAscii(TDes& aDestination, const char* aText)
    {
    TInt length = AsciiLength(aText);
    TInt available = aDestination.MaxLength() - aDestination.Length();
    if (available > length) length = available;
    for (TInt i = 0; i < length; i++) aDestination.Append((TChar)(TUint16)aText[i]);
    }

void AppendMapName(TDes& aDestination, TInt aMap)
    {
    if (aMap < 0 || aMap > 2) aMap = 0;
    AppendAscii(aDestination, KMapNames[aMap]);
    }

void AppendResolution(TDes& aDestination, TInt aLevel, TInt aWidth, TInt aHeight)
    {
    aLevel = Clamp(aLevel, 0, 4);
    aDestination.AppendNum(KMapRenderWidth[aLevel] * aWidth / 320);
    aDestination.Append((TChar)'x');
    aDestination.AppendNum(KMapRenderWidth[aLevel] * aHeight / 320);
    }

void AppendTime(TDes& aDestination, TInt aSeconds)
    {
    aDestination.AppendNum(aSeconds / 60);
    aDestination.Append((TChar)':');
    if (aSeconds % 60 < 10) aDestination.Append((TChar)'0');
    aDestination.AppendNum(aSeconds % 60);
    }

void AppendWeaponTitle(TDes& aDestination, TInt aWeapon, TInt aLanguage)
    {
    if (aWeapon < 0 || aWeapon >= KWeaponCount) aWeapon = 0;
    if (KWeaponTitleIndices[aWeapon] >= 0)
        AppendLocal(aDestination, KWeaponTitleIndices[aWeapon], aLanguage);
    else
        AppendAscii(aDestination, KWeaponAscii[aWeapon]);
    }

TInt HudId(CAssetManager& aAssets, TInt& aCache, const char* aName)
    {
    if (aCache == -2)
        {
        aCache = -1;
        if (aName)
            {
            const TInt index = aAssets.iHud.Find(aName);
            if (index >= 0 && index < aAssets.iHud.iCount) aCache = index;
            }
        }
    return aCache;
    }

TUint16 Rgb565(TUint32 aArgb)
    {
    return (TUint16)PackRgb565((TInt)((aArgb >> 16) & 255),
                               (TInt)((aArgb >> 8) & 255), (TInt)(aArgb & 255));
    }

TBool IsGun(TInt aWeapon)
    { return aWeapon > 0 && aWeapon <= 24; }

TBool IsSniper(TInt aWeapon)
    { return aWeapon >= 0 && aWeapon < KWeaponCount && KWpZoom1[aWeapon] > 0; }

TBool IsGrenade(TInt aWeapon)
    { return aWeapon >= 25 && aWeapon <= 27; }

/* ------------------------------------------------------------ примитивы интерфейса */

void DrawShadowText(CRenderer& aRenderer, TInt aX, TInt aY, const CCSFont& aFont,
                    const TDesC& aText, TUint16 aColor)
    {
    aRenderer.DrawText(aX + 1, aY + 1, aFont, aText, 0);
    aRenderer.DrawText(aX, aY, aFont, aText, aColor);
    }

void DrawCenteredShadowText(CRenderer& aRenderer, TInt aCenterX, TInt aY,
                            const CCSFont& aFont, const TDesC& aText, TUint16 aColor)
    {
    DrawShadowText(aRenderer, aCenterX - aRenderer.TextWidth(aFont, aText) / 2,
                   aY, aFont, aText, aColor);
    }

void DrawBevel(CRenderer& aRenderer, TInt aX, TInt aY, TInt aWidth, TInt aHeight, TBool aRaised)
    {
    TUint16 light = Rgb565(0x889180u);
    TUint16 dark = Rgb565(0x282E22u);
    TUint16 first = aRaised ? light : dark;
    TUint16 second = aRaised ? dark : light;
    aRenderer.FillRect(aX, aY, aWidth, 1, first);
    aRenderer.FillRect(aX, aY, 1, aHeight, first);
    aRenderer.FillRect(aX, aY + aHeight - 1, aWidth, 1, second);
    aRenderer.FillRect(aX + aWidth - 1, aY, 1, aHeight, second);
    }

void DrawOutline(CRenderer& aRenderer, TInt aX, TInt aY, TInt aWidth, TInt aHeight, TUint16 aColor)
    {
    aRenderer.FillRect(aX, aY, aWidth, 1, aColor);
    aRenderer.FillRect(aX, aY + aHeight - 1, aWidth, 1, aColor);
    aRenderer.FillRect(aX, aY, 1, aHeight, aColor);
    aRenderer.FillRect(aX + aWidth - 1, aY, 1, aHeight, aColor);
    }

void DrawUiFrame(CRenderer& aRenderer, TInt aX, TInt aY, TInt aWidth, TInt aHeight,
                 const CCSFont& aFont, const TDesC& aTitle)
    {
    aRenderer.FillRect(aX, aY, aWidth, aHeight, Rgb565(0x4C5844u));
    DrawBevel(aRenderer, aX, aY, aWidth, aHeight, ETrue);
    if (aTitle.Length() <= 0) return;
    aRenderer.DrawText(aX + 8, aY + 3, aFont, aTitle, Rgb565(0xFFFFFFu));
    TInt closeX = aX + aWidth - 12;
    for (TInt i = 0; i < 6; i++)
        {
        aRenderer.SetPixel(closeX + i, aY + 4 + i, Rgb565(0xB6B6B6u));
        aRenderer.SetPixel(closeX + 5 - i, aY + 4 + i, Rgb565(0xB6B6B6u));
        }
    }

void DrawButton(CRenderer& aRenderer, TInt aX, TInt aY, TInt aWidth, TInt aHeight,
                const CCSFont& aFont, const TDesC& aLabel, TBool aFocus)
    {
    aRenderer.FillRect(aX, aY, aWidth, aHeight, Rgb565(aFocus ? 0x8E8923u : 0x4C5844u));
    DrawBevel(aRenderer, aX, aY, aWidth, aHeight, ETrue);
    if (aFocus) DrawOutline(aRenderer, aX - 1, aY - 1, aWidth + 2, aHeight + 2, 0);
    TInt width = aRenderer.TextWidth(aFont, aLabel);
    aRenderer.DrawText(aX + (aWidth - width) / 2, aY + (aHeight - aFont.iHeight) / 2,
                       aFont, aLabel, Rgb565(0xFFFFFFu));
    }

void DrawCheckbox(CRenderer& aRenderer, TInt aX, TInt aY, TBool aOn)
    {
    aRenderer.FillRect(aX, aY, 9, 9, Rgb565(0x3E4637u));
    DrawBevel(aRenderer, aX, aY, 9, 9, EFalse);
    if (!aOn) return;
    for (TInt i = 0; i < 5; i++)
        {
        TInt px = aX + 2 + i;
        TInt py = i < 2 ? aY + 4 + i : aY + 6 - (i - 2);
        aRenderer.FillRect(px, py, 1, 2, Rgb565(0xC4B550u));
        }
    }

void DrawCombo(CRenderer& aRenderer, TInt aX, TInt aY, TInt aWidth,
               const CCSFont& aFont, const TDesC& aValue, TBool aFocus)
    {
    aRenderer.FillRect(aX, aY, aWidth, 11, Rgb565(0x3E4637u));
    DrawBevel(aRenderer, aX, aY, aWidth, 11, EFalse);
    aRenderer.DrawText(aX + 3, aY + (11 - aFont.iHeight) / 2, aFont, aValue,
                       Rgb565(aFocus ? 0xFFFFFFu : 0xD8D8D8u));
    TInt boxX = aX + aWidth - 10;
    aRenderer.FillRect(boxX, aY + 1, 9, 9, Rgb565(0x4C5844u));
    DrawBevel(aRenderer, boxX, aY + 1, 9, 9, ETrue);
    for (TInt i = 0; i < 3; i++)
        aRenderer.FillRect(boxX + 2 + i, aY + 4 + i, 5 - i * 2, 1,
                           Rgb565(aFocus ? 0xFFFFFFu : 0xB6B6B6u));
    }

void DrawSlider(CRenderer& aRenderer, TInt aX, TInt aY, TInt aWidth, TInt aValue, TBool aFocus)
    {
    aRenderer.FillRect(aX, aY + 4, aWidth, 2, Rgb565(0x1F1F1Fu));
    for (TInt i = 0; i <= 4; i++)
        aRenderer.FillRect(aX + i * (aWidth - 1) / 4, aY + 8, 1, 2, Rgb565(0xB6B6B6u));
    TInt knob = aX + (aWidth - 5) * Clamp(aValue, 0, 1000) / 1000;
    aRenderer.FillRect(knob, aY, 5, 10, Rgb565(aFocus ? 0x8E8923u : 0x4C5844u));
    DrawBevel(aRenderer, knob, aY, 5, 10, ETrue);
    }

void DrawBackground(CRenderer& aRenderer)
    {
    for (TInt y = 0; y < aRenderer.iHeight; y++)
        {
        TUint16 color = (TUint16)(0x1018u + ((y * 24 / aRenderer.iHeight) << 5));
        aRenderer.FillRect(0, y, aRenderer.iWidth, 1, color);
        }
    }

/* ------------------------------------------------------------ меню и локальное состояние */

struct TMenuItem
    {
    TText iText[KMenuTextLength];
    TInt iLength;
    TBool iEnabled;
    };

static TMenuItem gMenuItems[KMaxMenuItems];
static TInt gMenuCount = 0;
static TBool gViewModelsChecked = EFalse;

void AddMenuItem(const TDesC& aText, TBool aEnabled = ETrue)
    {
    if (gMenuCount >= KMaxMenuItems) return;
    TMenuItem& item = gMenuItems[gMenuCount++];
    CopyText(item.iText, KMenuTextLength, aText);
    item.iLength = item.iText[0] ? TextLength(item.iText, KMenuTextLength) : 0;
    item.iEnabled = aEnabled;
    }

void AddNumber(TDes& aDestination, TInt aValue)
    {
    aDestination.AppendNum(aValue);
    }

void AppendOnOff(TDes& aDestination, TBool aValue, TInt aLanguage)
    {
    AppendLocal(aDestination, aValue ? 428 : 421, aLanguage);
    }

void AppendModeName(TDes& aDestination, TInt aMode, TInt aLanguage = KLanguageRussian)
    {
    if (aMode == 3) { AppendAscii(aDestination, "Terminator"); return; }
    if (aMode == 4) { AppendAscii(aDestination, "Hostage Rescue"); return; }
    aMode = Clamp(aMode, 0, 2);
    AppendLocal(aDestination, KModeNames[aMode], aLanguage);
    }

void AppendDiffName(TDes& aDestination, TInt aDifficulty, TInt aLanguage = KLanguageRussian)
    {
    aDifficulty = Clamp(aDifficulty, 0, 3);
    AppendLocal(aDestination, KDifficultyNames[aDifficulty], aLanguage);
    }

void AppendGfxName(TDes& aDestination, TInt aGfx, TInt aLanguage = KLanguageRussian)
    {
    aGfx = Clamp(aGfx, 0, 3);
    AppendLocal(aDestination, KGfxNames[aGfx], aLanguage);
    }

void AppendNfrName(TDes& aDestination, TInt aNfr, TInt aLanguage = KLanguageRussian)
    {
    aNfr = Clamp(aNfr, 0, 5);
    if (aNfr == 0) AppendLocal(aDestination, 421, aLanguage);
    else
        {
        aDestination.AppendNum(KNfrTarget[aNfr]);
        AppendAscii(aDestination, " FPS");
        }
    }

TInt NormalizeKeyCode(TInt aCode)
    {
    switch (aCode)
        {
    case 0x101: return -1;
    case 0x102: return -2;
    case 0x103: return -3;
    case 0x104: return -4;
    case 0x105: return -5;
    case 0x10A: return -6;
    case 0x106: return -7;
    case 0x108: return -8;
    case 0x107: return -10;
    default: return aCode;
        }
    }

void AppendKeyName(TDes& aDestination, TInt aCode, TInt aLanguage = KLanguageRussian)
    {
    switch (aCode)
        {
    case 0:
        /* В локализованных массивах нет отдельной строки для пустой клавиши. */
        aDestination.Append((TChar)0x2014);
        return;
    case -1: AppendLocal(aDestination, 214, aLanguage); return;
    case -2: AppendLocal(aDestination, 235, aLanguage); return;
    case -3: AppendLocal(aDestination, 215, aLanguage); return;
    case -4: AppendLocal(aDestination, 191, aLanguage); return;
    case -5: AppendLocal(aDestination, 223, aLanguage); return;
    case -6: AppendLocal(aDestination, 296, aLanguage); return;
    case -7: AppendLocal(aDestination, 273, aLanguage); return;
    case -8: AppendLocal(aDestination, 316, aLanguage); return;
    case -10: AppendLocal(aDestination, 179, aLanguage); return;
    default: break;
        }
    if (aCode >= '0' && aCode <= '9')
        {
        aDestination.Append((TChar)aCode);
        return;
        }
    if (aCode == '*' || aCode == '#' || (aCode >= 'A' && aCode <= 'Z') || (aCode >= 'a' && aCode <= 'z'))
        {
        aDestination.Append((TChar)aCode);
        return;
        }
    AppendLocal(aDestination, 317, aLanguage);
    aDestination.AppendNum(aCode);
    }

TBool IsOverlayMenu(TInt aMenu)
    {
    return aMenu == EMenuGame || aMenu == EMenuRadio1 || aMenu == EMenuRadio2 || aMenu == EMenuRadio3;
    }

TBool IsOptionsMenu(TInt aMenu)
    {
    return aMenu == EMenuGraphics || aMenu == EMenuControls || aMenu == EMenuBinds ||
           aMenu == EMenuSound || aMenu == EMenuLanguage;
    }

TBool IsDialogMenu(TInt aMenu)
    {
    return aMenu == EMenuCreate || aMenu == EMenuMultiplayer || aMenu == EMenuLan ||
           aMenu == EMenuWorld || aMenu == EMenuFirstRun1 || aMenu == EMenuFirstRun2 ||
           IsOptionsMenu(aMenu);
    }

TInt OptionTab(TInt aMenu)
    {
    for (TInt i = 0; i < 5; i++)
        if (KOptTabs[i] == aMenu) return i;
    return -1;
    }

void EnsureUi(CGame& aGame)
    {
    if (gUi.iReady) return;
    Mem::FillZ(&gUi, sizeof(gUi));
    Mem::FillZ(gExtra, sizeof(gExtra));
    for (TInt i = 0; i < KMaxPlayers; i++)
        {
        gExtra[i].iVmWeapon = -1;
        gExtra[i].iVmSet = -1;
        gExtra[i].iVmAnimation = -1;
        }
    gUi.iReady = ETrue;
    TPtrC nick((const TText*)L"Player");
    CopyText(gUi.iNick, 32, nick);
    gUi.iNickLength = 6;
    gUi.iMpMode = 0;
    gUi.iMpMap = Clamp(aGame.iMapIndex, 0, 10);
    gUi.iCreateMap = aGame.iMapIndex;
    gUi.iCreateTeam = aGame.iTeamChoice;
    gUi.iCreateBots = aGame.iBotCount;
    gUi.iCreateRoundTime = aGame.iRoundSeconds;
    gUi.iCreateBuyTime = aGame.iBuySeconds;
    gUi.iCreateFreeze = aGame.iFreezeSeconds;
    gUi.iCreateMoney = aGame.iStartMoney;
    gUi.iCreateFriendlyFire = aGame.iFriendlyFire;
    gUi.iBuySelection = 0;
    gUi.iTabFocus = EFalse;
    gUi.iBindCapture = -1;
    aGame.iBindCapture = -1;
    aGame.iBindUntil = 0.0f;
    for (TInt i = 0; i < KMaxBinds; i++)
        {
        gUi.iBindings[i].iFirst = KDefaultBinds[i];
        gUi.iBindings[i].iSecond = 0;
        }
    LoadUiSettings(aGame);
    if (gUi.iNickLength <= 0)
        {
        TPtrC nick((const TText*)L"Player");
        CopyText(gUi.iNick, 32, nick);
        gUi.iNickLength = 6;
        }
    if (gUi.iServerLength <= 0)
        {
        CopyAsciiToText(gUi.iServer, 96, "broker.emqx.io");
        gUi.iServerLength = 14;
        }
    gUi.iLastApplyTick = User::NTickCount();
    }

/* Строки меню строятся в фиксированном массиве; разделитель '\t' задаёт вторую колонку. */

void BuildMenuItems(CGame& aGame)
    {
    EnsureUi(aGame);
    if (aGame.iMenu == EMenuLan) PollLan(aGame);
    gMenuCount = 0;
    TBuf<KMenuTextLength> line;
    const TInt language = aGame.iLanguage;

    switch (aGame.iMenu)
        {
    case EMenuMain:
        AppendLocal(line, 270, language); AddMenuItem(line); line.Zero();
        AppendLocal(line, 254, language); AddMenuItem(line); line.Zero();
        AppendLocal(line, 300, language); AddMenuItem(line); line.Zero();
        AppendLocal(line, 393, language); AddMenuItem(line);
        break;

    case EMenuMultiplayer:
        AppendLocal(line, 351, language); AppendModeName(line, gUi.iMpMode, language); AddMenuItem(line, ETrue); line.Zero();
        AppendLocal(line, 343, language); AppendMapName(line, gUi.iMpMap); AddMenuItem(line, ETrue); line.Zero();
        AppendLocal(line, 399, language);
        if (gUi.iNickLength > 0) AppendLimited(line, TPtrC(gUi.iNick, gUi.iNickLength));
        else AppendAscii(line, "Player");
        AddMenuItem(line, ETrue); line.Zero();
        AppendLocal(line, 327, language);
        if (gUi.iServerLength > 0) AppendLimited(line, TPtrC(gUi.iServer, gUi.iServerLength));
        else AppendLocal(line, 420, language);
        AddMenuItem(line, ETrue); line.Zero();
        AppendLocal(line, 62, language); AddMenuItem(line); line.Zero();
        AppendLocal(line, 104, language); AddMenuItem(line); line.Zero();
        AppendLocal(line, 51, language); AddMenuItem(line); line.Zero();
        AppendLocal(line, 60, language); AddMenuItem(line); line.Zero();
        AppendLocal(line, 61, language); AddMenuItem(line); line.Zero();
        AppendLocal(line, 375, language); AddMenuItem(line);
        break;

    case EMenuLan:
        if (gUi.iLanCount <= 0)
            {
            if (gUi.iStatus[0]) line.Copy(TPtrC(gUi.iStatus));
            else AppendLocal(line, gUi.iNetworkLan ? 47 : 147, language);
            AddMenuItem(line, EFalse); line.Zero();
            }
        else
            {
            for (TInt i = 0; i < gUi.iLanCount; i++)
                {
                AppendAscii(line, "> ");
                AppendLimited(line, TPtrC(gUi.iLan[i].iName, gUi.iLan[i].iLength));
                line.Append((TChar)'\t');
                line.AppendNum(gUi.iLan[i].iPlayers);
                AppendAscii(line, "/10\t");
                AppendModeName(line, gUi.iLan[i].iMode, language);
                AppendAscii(line, ", ");
                AppendMapName(line, gUi.iLan[i].iMap);
                AddMenuItem(line);
                line.Zero();
                }
            }
        AppendLocal(line, 375, language); AddMenuItem(line);
        break;

    case EMenuWorld:
        if (gUi.iRoomCount <= 0)
            {
            if (gUi.iStatus[0]) line.Copy(TPtrC(gUi.iStatus));
            else if (gUi.iNetworkRequest) AppendLocal(line, 63, language);
            else AppendLocal(line, 20, language);
            AddMenuItem(line, EFalse); line.Zero();
            }
        else
            {
            for (TInt i = 0; i < gUi.iRoomCount; i++)
                {
                AppendAscii(line, "> ");
                AppendLimited(line, TPtrC(gUi.iRooms[i].iName, gUi.iRooms[i].iLength));
                line.Append((TChar)'\t');
                line.AppendNum(gUi.iRooms[i].iPlayers);
                AppendAscii(line, "/10\t");
                AppendModeName(line, gUi.iRooms[i].iMode, language);
                AppendAscii(line, ", ");
                AppendMapName(line, gUi.iRooms[i].iMap);
                AddMenuItem(line);
                line.Zero();
                }
            }
        AppendLocal(line, 21, language); AddMenuItem(line); line.Zero();
        AppendLocal(line, 104, language); AddMenuItem(line); line.Zero();
        AppendLocal(line, 375, language); AddMenuItem(line);
        break;

    case EMenuSearch:
        if (aGame.iConnection) AppendAscii(line, "0  ");
        AppendLocal(line, aGame.iConnection ? 375 : 398, language);
        AddMenuItem(line);
        break;

    case EMenuPause:
        AppendLocal(line, 148, language); AddMenuItem(line); line.Zero();
        AppendLocal(line, 187, language); AddMenuItem(line); line.Zero();
        AppendLocal(line, 300, language); AddMenuItem(line); line.Zero();
        AppendLocal(line, 255, language); AddMenuItem(line); line.Zero();
        AppendLocal(line, 393, language); AddMenuItem(line);
        break;

    case EMenuCreate:
        AppendLocal(line, 343, language); AppendMapName(line, gUi.iCreateMap); AddMenuItem(line, ETrue); line.Zero();
        AppendLocal(line, 295, language); AppendLocal(line, gUi.iCreateTeam == 1 ? 276 : gUi.iCreateTeam == 2 ? 352 : 142, language); AddMenuItem(line, ETrue); line.Zero();
        AppendLocal(line, 141, language); AppendDiffName(line, aGame.iDifficulty, language); AddMenuItem(line, ETrue); line.Zero();
        AppendLocal(line, 334, language); line.AppendNum(gUi.iCreateBots); AddMenuItem(line, ETrue); line.Zero();
        AppendLocal(line, 140, language); line.AppendNum(aGame.iMaxRounds); AddMenuItem(line, ETrue); line.Zero();
        AppendLocal(line, 190, language); AppendTime(line, gUi.iCreateRoundTime); AddMenuItem(line, ETrue); line.Zero();
        AppendLocal(line, 212, language); line.AppendNum(gUi.iCreateBuyTime); AppendLocal(line, 429, language); AddMenuItem(line, ETrue); line.Zero();
        AppendLocal(line, 331, language); line.AppendNum(gUi.iCreateFreeze); AppendLocal(line, 429, language); AddMenuItem(line, ETrue); line.Zero();
        AppendLocal(line, 167, language); AppendAscii(line, "$"); line.AppendNum(gUi.iCreateMoney); AddMenuItem(line, ETrue); line.Zero();
        AppendLocal(line, 193, language); AppendLocal(line, gUi.iCreateFriendlyFire ? 428 : 421, language); AddMenuItem(line, ETrue); line.Zero();
        AppendLocal(line, 372, language); AddMenuItem(line); line.Zero();
        AppendLocal(line, 375, language); AddMenuItem(line);
        break;

    case EMenuTeam:
        AppendLocal(line, 210, language); AddMenuItem(line); line.Zero();
        AppendLocal(line, 262, language); AddMenuItem(line); line.Zero();
        AppendLocal(line, 228, language); AddMenuItem(line); line.Zero();
        AppendLocal(line, 285, language); AddMenuItem(line);
        break;

    case EMenuGraphics:
        AppendAscii(line, "Render: ");
        if (aGame.iRenderMode == 0) AppendAscii(line, "Auto");
        else if (aGame.iRenderMode == 1) AppendAscii(line, aGame.iGpuAvailable ? "GPU (3D)" : "GPU (n/a)");
        else AppendAscii(line, "CPU");
        AddMenuItem(line, ETrue); line.Zero();
        AppendAscii(line, "Mod: ");
        {
        TInt mi = aGame.iAssets.CurrentModIndex();
        if (mi < 0) AppendAscii(line, "Default");
        else AppendLimited(line, TPtrC(aGame.iAssets.iModNames[mi]));
        }
        AddMenuItem(line, ETrue); line.Zero();
        AppendLocal(line, 291, language); AppendGfxName(line, aGame.iGfx, language); AddMenuItem(line, ETrue); line.Zero();
        AppendLocal(line, 241, language);
        if (aGame.iNokiaFrameRate > 0)
            {
            AppendAscii(line, "DLSS ");
            AppendResolution(line, aGame.iRenderLevel, aGame.iWidth, aGame.iHeight);
            }
        else
            {
            if (aGame.iQuality == 0) AppendLocal(line, 189, language);
            else if (aGame.iQuality == 1)
                {
                AppendLocal(line, 299, language);
                AppendResolution(line, 0, aGame.iWidth, aGame.iHeight);
                }
            else if (aGame.iQuality == 2)
                {
                AppendLocal(line, 328, language);
                AppendResolution(line, 2, aGame.iWidth, aGame.iHeight);
                }
            else
                {
                AppendLocal(line, 348, language);
                AppendResolution(line, 4, aGame.iWidth, aGame.iHeight);
                }
            }
        AddMenuItem(line, aGame.iNokiaFrameRate == 0); line.Zero();
        AppendLocal(line, 131, language); AppendNfrName(line, aGame.iNokiaFrameRate, language); AddMenuItem(line, ETrue); line.Zero();
        AppendLocal(line, 307, language); line.AppendNum(aGame.iGammaIndex + 1); AppendAscii(line, "/5"); AddMenuItem(line, ETrue); line.Zero();
        AppendLocal(line, 97, language); AppendOnOff(line, aGame.iViewBob, language); AddMenuItem(line, ETrue); line.Zero();
        AppendLocal(line, 204, language); AppendLocal(line, KCrossNames[Clamp(aGame.iCrossColor, 0, 4)], language); AddMenuItem(line, ETrue); line.Zero();
        AppendLocal(line, 163, language); AppendLocal(line, KCrossSizeIndices[Clamp(aGame.iCrossSize, 0, 2)], language); AddMenuItem(line, ETrue); line.Zero();
        AppendLocal(line, 88, language); AppendOnOff(line, aGame.iCrossDynamic, language); AddMenuItem(line, ETrue); line.Zero();
        AppendLocal(line, 350, language); AppendOnOff(line, aGame.iRadar, language); AddMenuItem(line, ETrue); line.Zero();
        AppendLocal(line, 161, language); AppendOnOff(line, aGame.iShowFps, language); AddMenuItem(line, ETrue); line.Zero();
        AppendLocal(line, 357, language); AppendLocal(line, aGame.iOrientation == 0 ? 260 : 224, language); AddMenuItem(line, ETrue); line.Zero();
        AppendLocal(line, 252, language);
        if (aGame.iFpsCap20) AppendLocal(line, 171, language); else AppendLocal(line, 280, language);
        AddMenuItem(line, ETrue); line.Zero();
        AppendLocal(line, 372, language); AddMenuItem(line);
        break;

    case EMenuControls:
        AppendLocal(line, 354, language); AppendLocal(line, aGame.iLayout == 0 ? 246 : aGame.iLayout == 1 ? 205 : 422, language); AddMenuItem(line, ETrue); line.Zero();
        AppendLocal(line, 129, language); line.AppendNum(aGame.iSensitivity); AddMenuItem(line, ETrue); line.Zero();
        AppendLocal(line, 231, language); line.AppendNum(aGame.iSensitivityVertical); AddMenuItem(line, ETrue); line.Zero();
        AppendLocal(line, 154, language); AppendOnOff(line, aGame.iInvertY, language); AddMenuItem(line, ETrue); line.Zero();
        AppendLocal(line, 111, language); AppendOnOff(line, aGame.iAutoAim, language); AddMenuItem(line, ETrue); line.Zero();
        AppendLocal(line, 372, language); AddMenuItem(line);
        break;

    case EMenuBinds:
        for (TInt i = 0; i < KMaxBinds; i++)
            {
            AppendLocal(line, KBindNames[i], language);
            line.Append((TChar)'\t');
            if (gUi.iBindings[i].iFirst == 0) AppendKeyName(line, 0, language);
            else
                {
                AppendKeyName(line, gUi.iBindings[i].iFirst, language);
                if (gUi.iBindings[i].iSecond != 0)
                    {
                    AppendAscii(line, ", ");
                    AppendKeyName(line, gUi.iBindings[i].iSecond, language);
                    }
                }
            AddMenuItem(line, ETrue);
            line.Zero();
            }
        AppendLocal(line, 326, language); AddMenuItem(line); line.Zero();
        AppendLocal(line, 372, language); AddMenuItem(line);
        break;

    case EMenuSound:
        AppendLocal(line, 370, language); AppendOnOff(line, aGame.iSoundOn, language); AddMenuItem(line, ETrue); line.Zero();
        AppendLocal(line, 250, language); line.AppendNum(aGame.iVolume); AppendAscii(line, "%"); AddMenuItem(line, ETrue); line.Zero();
        AppendLocal(line, 177, language); AppendOnOff(line, aGame.iVoiceOn, language); AddMenuItem(line, ETrue); line.Zero();
        AppendLocal(line, 118, language); line.AppendNum(aGame.iVoiceVolume); AppendAscii(line, "%"); AddMenuItem(line, ETrue); line.Zero();
        if (gUi.iMicState == 1) AppendLocal(line, 234, language);
        else if (gUi.iMicState == 2) line.Copy(TPtrC(gUi.iStatus));
        else AppendLocal(line, 319, language);
        AddMenuItem(line, ETrue); line.Zero();
        AppendLocal(line, 372, language); AddMenuItem(line);
        break;

    case EMenuLanguage:
        {
        if (aGame.iLanguage == KLanguageEnglish) line.Append((TChar)'>'); else line.Append((TChar)' ');
        AppendAscii(line, "English"); AddMenuItem(line); line.Zero();
        if (aGame.iLanguage == KLanguageRussian) line.Append((TChar)'>'); else line.Append((TChar)' ');
        AppendAscii(line, "Russian"); AddMenuItem(line); line.Zero();
        if (aGame.iLanguage == KLanguageFrench) line.Append((TChar)'>'); else line.Append((TChar)' ');
        AppendAscii(line, "French"); AddMenuItem(line); line.Zero();
        if (aGame.iLanguage == KLanguageGerman) line.Append((TChar)'>'); else line.Append((TChar)' ');
        AppendAscii(line, "German"); AddMenuItem(line); line.Zero();
        if (aGame.iLanguage == KLanguageJapanese) line.Append((TChar)'>'); else line.Append((TChar)' ');
        AppendAscii(line, "Japanese"); AddMenuItem(line); line.Zero();
        if (aGame.iLanguage == KLanguageVietnamese) line.Append((TChar)'>'); else line.Append((TChar)' ');
        AppendAscii(line, "Vietnamese"); AddMenuItem(line); line.Zero();
        AppendLocal(line, 372, language); AddMenuItem(line);
        break;
        }

    case EMenuFirstRun1:
        AppendLocal(line, 90, language); AddMenuItem(line); line.Zero();
        AppendLocal(line, 52, language); AddMenuItem(line);
        break;

    case EMenuFirstRun2:
        AppendLocal(line, 86, language); AddMenuItem(line); line.Zero();
        AppendLocal(line, 299, language); AddMenuItem(line); line.Zero();
        AppendLocal(line, 328, language); AddMenuItem(line); line.Zero();
        AppendLocal(line, 348, language); AddMenuItem(line);
        break;

    case EMenuHelp:
        AppendLocal(line, 398, language); AddMenuItem(line);
        break;

    case EMenuGame:
        AppendLocal(line, 200, language); AddMenuItem(line); line.Zero();
        AppendLocal(line, 218, language); AddMenuItem(line); line.Zero();
        AppendLocal(line, 219, language); AddMenuItem(line); line.Zero();
        AppendLocal(line, 150, language); AddMenuItem(line); line.Zero();
        AppendLocal(line, 117, language); AddMenuItem(line); line.Zero();
        AppendLocal(line, 342, language); AddMenuItem(line); line.Zero();
        AppendLocal(line, 341, language); AddMenuItem(line);
        break;

    case EMenuRadio1:
    case EMenuRadio2:
    case EMenuRadio3:
        {
        const TInt base = aGame.iMenu == EMenuRadio1 ? 0 : aGame.iMenu == EMenuRadio2 ? 6 : 12;
        const TInt end = aGame.iMenu == EMenuRadio1 ? 6 : aGame.iMenu == EMenuRadio2 ? 12 : 21;
        for (TInt i = base; i < end; i++)
            {
            AppendLocal(line, KRadioNames[i], language);
            AddMenuItem(line);
            line.Zero();
            }
        }
        break;

    default:
        break;
        }
    }

/* ------------------------------------------------------------ общие помощники меню */

void TitleText(CGame& aGame, TDes& aDestination)
    {
    aDestination.Zero();
    const TInt language = aGame.iLanguage;
    switch (aGame.iMenu)
        {
    case EMenuCreate: AppendLocal(aDestination, 270, language); break;
    case EMenuMultiplayer: AppendLocal(aDestination, 254, language); break;
    case EMenuSearch: AppendLocal(aDestination, 272, language); break;
    case EMenuLan: AppendLocal(aDestination, 107, language); break;
    case EMenuWorld: AppendLocal(aDestination, 120, language); break;
    case EMenuTeam: AppendLocal(aDestination, 149, language); break;
    case EMenuGraphics:
    case EMenuControls:
    case EMenuBinds:
    case EMenuSound:
    case EMenuLanguage: AppendLocal(aDestination, 300, language); break;
    case EMenuFirstRun1: AppendLocal(aDestination, 65, language); break;
    case EMenuFirstRun2: AppendLocal(aDestination, 64, language); break;
    case EMenuGame: AppendLocal(aDestination, 414, language); break;
    case EMenuRadio1: AppendLocal(aDestination, 200, language); break;
    case EMenuRadio2: AppendLocal(aDestination, 218, language); break;
    case EMenuRadio3: AppendLocal(aDestination, 219, language); break;
    default: break;
        }
    }

TInt SliderValue(const CGame& aGame, TInt aIndex)
    {
    if (aGame.iMenu == EMenuGraphics && aIndex == 5) return aGame.iGammaIndex * 250;
    if (aGame.iMenu == EMenuControls && aIndex == 1) return (aGame.iSensitivity - 1) * 1000 / 9;
    if (aGame.iMenu == EMenuControls && aIndex == 2) return (aGame.iSensitivityVertical - 1) * 1000 / 9;
    if (aGame.iMenu == EMenuSound && aIndex == 1) return (aGame.iVolume - 10) * 1000 / 90;
    return -1;
    }

TBool IsButtonItem(const TMenuItem& aItem, TInt aLanguage)
    {
    const TInt length = TextLength(aItem.iText, KMenuTextLength);
    if (length <= 0 || length >= 16) return EFalse;
    TPtrC value(aItem.iText, length);
    if (value == TPtrC((const TText*)L"OK")) return ETrue;
    for (TInt i = 0; i < 5; i++)
        {
        const TInt index = KButtonIndices[i];
        const TPtrC button = LocalText(index, aLanguage);
        if (button.Length() == length && value.Compare(button) == 0) return ETrue;
        }
    return EFalse;
    }

void BuildBuyItems(CGame& aGame)
    {
    EnsureUi(aGame);
    gMenuCount = 0;
    TBuf<KMenuTextLength> line;
    const TInt language = aGame.iLanguage;
    const TPlayer* me = aGame.iMyIndex >= 0 && aGame.iMyIndex < KMaxPlayers ? &aGame.iPlayers[aGame.iMyIndex] : NULL;
    const TInt money = me ? me->iMoney : 0;

    if (aGame.iBuyCategory < 0)
        {
        AppendLocal(line, 192, language); AddMenuItem(line); line.Zero();
        for (TInt i = 0; i < 8; i++)
            {
            line.AppendNum(i + 1);
            AppendAscii(line, "  ");
            AppendLocal(line, KBuyTitles[i], language);
            AddMenuItem(line);
            line.Zero();
            }
        AppendAscii(line, "9  ");
        AppendLocal(line, 414, language);
        AppendAscii(line, " / ");
        AppendLocal(line, 300, language);
        AddMenuItem(line);
        line.Zero();
        AppendAscii(line, "0  ");
        AppendLocal(line, 375, language);
        AddMenuItem(line);
        return;
        }

    if (aGame.iBuyCategory == 7)
        {
        AppendLocal(line, 275, language); AddMenuItem(line); line.Zero();
        for (TInt i = 0; i < 6; i++)
            {
            line.AppendNum(i + 1);
            AppendAscii(line, "  ");
            AppendLocal(line, KEquipTitles[i], language);
            AddMenuItem(line, KEquipPrices[i] <= money && (i != 5 || (me && me->iTeam == 2)));
            line.Zero();
            }
        AppendAscii(line, "0  ");
        AppendLocal(line, 398, language);
        AddMenuItem(line);
        return;
        }

    const TInt category = Clamp(aGame.iBuyCategory, 0, 4);
    AppendLocal(line, KBuyTitles[category], language); AddMenuItem(line); line.Zero();
    for (TInt i = 0; i < KBuyListLength[category]; i++)
        {
        const TInt weapon = KBuyLists[category][i];
        line.AppendNum(i + 1);
        AppendAscii(line, "  ");
        AppendWeaponTitle(line, weapon, language);
        AddMenuItem(line, KWpTeam[weapon] == 0 || !me || KWpTeam[weapon] == me->iTeam ? KWpPrice[weapon] <= money : EFalse);
        line.Zero();
        }
    AppendAscii(line, "0  ");
    AppendLocal(line, 398, language);
    AddMenuItem(line);
    }

void AssignBinding(TKeyBinding& aBinding, TInt aCode, TInt& aLayout)
    {
    if (aCode == 0) return;
    if (aBinding.iFirst == aCode) return;
    if (aBinding.iSecond == aCode)
        {
        aBinding.iSecond = aBinding.iFirst;
        aBinding.iFirst = aCode;
        return;
        }
    const TInt spare = aBinding.iSecond;
    aBinding.iSecond = aBinding.iFirst;
    aBinding.iFirst = aCode;
    for (TInt i = 0; i < KMaxBinds; i++)
        {
        TKeyBinding& other = gUi.iBindings[i];
        if (&other == &aBinding) continue;
        if (other.iSecond == aCode) other.iSecond = 0;
        if (other.iFirst == aCode)
            {
            other.iFirst = other.iSecond;
            other.iSecond = 0;
            if (other.iFirst == 0)
                {
                if (spare == 0 && aBinding.iSecond != 0)
                    {
                    other.iFirst = aBinding.iSecond;
                    aBinding.iSecond = 0;
                    }
                }
            }
        }
    aLayout = 2;
    }

void CopyAsciiToText(TText* aDestination, TInt aMaximum, const char* aSource)
    {
    TInt length = AsciiLength(aSource);
    if (length >= aMaximum) length = aMaximum - 1;
    for (TInt i = 0; i < length; i++) aDestination[i] = (TText)(TUint8)aSource[i];
    aDestination[length] = 0;
    }

void TextureName(TDes8& aDestination, const char* aName)
    {
    aDestination.Zero();
    const TInt length = AsciiLength(aName);
    for (TInt i = 0; i < length && i < aDestination.MaxLength(); i++)
        aDestination.Append((TUint8)aName[i]);
    }

void SendNetAction(CGame& aGame, TInt aOperation, TInt aFirst, TInt aSecond)
    {
    if (!aGame.iConnection || !aGame.iConnection->Connected()) return;
    aGame.iNetOut.Reset();
    aGame.iNetOut.U8(aOperation);
    if (aFirst >= 0) aGame.iNetOut.U8(aFirst);
    if (aSecond >= 0) aGame.iNetOut.U8(aSecond);
    aGame.iConnection->Send(EClientToHost, aGame.iNetOut);
    }

TBool CanBuy(const CGame& aGame, const TPlayer& aPlayer)
    {
    return aPlayer.iAlive && !aGame.iBombPlanted && aGame.iTime < aGame.iBuyEnd;
    }

void ConnectHost(CGame& aGame, TBool aHost, TBool aLan)
    {
    TInt error = KErrNone;
    if (!aGame.iConnection)
        TRAP(error, aGame.iConnection = CNetConnection::NewL(aGame.iSocketServer));
    if (error != KErrNone || !aGame.iConnection)
        {
        CopyText(gUi.iStatus, KMenuTextLength, LocalText(376, aGame.iLanguage));
        return;
        }
    aGame.iConnection->Close();
    if (aHost)
        {
        TRAP(error, aGame.iConnection->AcceptL(KNetPort));
        aGame.iNetworkHost = ETrue;
        }
    else
        {
        TBuf8<128> host;
        if (gUi.iServerLength > 0)
            {
            for (TInt i = 0; i < gUi.iServerLength && i < host.MaxLength(); i++)
                host.Append((TUint8)(gUi.iServer[i] & 0xFF));
            }
        else TextureName(host, "broker.emqx.io");
        TRAP(error, aGame.iConnection->ConnectL(host, KNetPort));
        aGame.iNetworkHost = EFalse;
        }
    if (error != KErrNone)
        {
        CopyText(gUi.iStatus, KMenuTextLength, LocalText(44, aGame.iLanguage));
        aGame.iConnection->Close();
        return;
        }
    aGame.iNetOut.Reset();
    aGame.iNetOut.U8(KNetProtocol).U8(gUi.iMpMode).U8(gUi.iMpMap)
        .Str(TPtrC(gUi.iNick, gUi.iNickLength))
        .U8(aHost ? 1 : 0);
    aGame.iConnection->Send(EClientJoin, aGame.iNetOut);
    gUi.iStatus[0] = 0;
    gUi.iNetworkCreate = aHost;
    gUi.iNetworkLan = aLan;
    gUi.iNetworkRequest = ETrue;
    aGame.iState = EStateLoad;
    aGame.iMenu = EMenuSearch;
    aGame.iMenuSelection = 0;
    CopyText(gUi.iLoadMessage, 128, LocalText(aHost ? 48 : 70, aGame.iLanguage));
    }

void StartLanSearch(CGame& aGame)
    {
    if (aGame.iLanFinder)
        {
        aGame.iLanFinder->Stop();
        delete aGame.iLanFinder;
        aGame.iLanFinder = NULL;
        }
    TInt error = KErrNone;
    TRAP(error, aGame.iLanFinder = CLanFinder::NewL(aGame.iSocketServer));
    if (error != KErrNone || !aGame.iLanFinder)
        {
        delete aGame.iLanFinder;
        aGame.iLanFinder = NULL;
        CopyText(gUi.iStatus, KMenuTextLength, LocalText(147, aGame.iLanguage));
        return;
        }
    TRAP(error, aGame.iLanFinder->StartL(KNetPort));
    if (error != KErrNone)
        {
        aGame.iLanFinder->Stop();
        delete aGame.iLanFinder;
        aGame.iLanFinder = NULL;
        CopyText(gUi.iStatus, KMenuTextLength, LocalText(147, aGame.iLanguage));
        return;
        }
    gUi.iLanCount = 0;
    gUi.iStatus[0] = 0;
    }

void CloseLanSearch(CGame& aGame)
    {
    if (!aGame.iLanFinder) return;
    aGame.iLanFinder->Stop();
    delete aGame.iLanFinder;
    aGame.iLanFinder = NULL;
    }

void CloseWorldSearch(CGame& aGame)
    {
    (void)aGame;
    if (gUi.iWorldSocketOpen)
        {
        gUi.iWorldSocket.Close();
        gUi.iWorldSocketOpen = EFalse;
        }
    gUi.iWorldPending = 0;
    gUi.iRoomCount = 0;
    gUi.iRoomRequestAt = 0;
    }

void PollLan(CGame& aGame)
    {
    if (!aGame.iLanFinder) return;
    TInetAddr address;
    TPtrC8 payload;
    const TUint32 now = User::NTickCount();
    while (aGame.iLanFinder->Poll(address, payload))
        {
        /* CLanFinder::Poll возвращает полный пакет: "CS16", Net.PROTO, затем тело маяка. */
        if (payload.Length() <= 5) continue;
        TNetReader reader(payload.Ptr() + 5, payload.Length() - 5);
        if (!reader.More()) continue;
        const TInt mode = reader.U8();
        const TInt map = reader.U8();
        const TInt players = reader.U8();
        TPtrC8 name = reader.Str();
        const TInt port = reader.More() ? reader.U16() : KNetPort;
        TInt slot = -1;
        for (TInt i = 0; i < gUi.iLanCount; i++)
            {
            if (gUi.iLanSeen[i] != 0 &&
                gUi.iLan[i].iAddress.Address() == address.Address() &&
                gUi.iLan[i].iAddress.Port() == address.Port())
                {
                slot = i;
                break;
                }
            }
        if (slot < 0 && gUi.iLanCount < KMaxLanGames) slot = gUi.iLanCount++;
        if (slot < 0) continue;
        TLanEntry& entry = gUi.iLan[slot];
        entry.iAddress = address;
        entry.iPort = port > 0 && port < 65536 ? port : KNetPort;
        TInt length = name.Length();
        if (length >= (TInt)(sizeof(entry.iName) / sizeof(TText))) length = sizeof(entry.iName) / sizeof(TText) - 1;
        for (TInt i = 0; i < length; i++) entry.iName[i] = (TText)(TUint8)name[i];
        entry.iName[length] = 0;
        entry.iLength = length;
        entry.iPlayers = players;
        entry.iMode = mode % 5;
        entry.iMap = map % 3;
        gUi.iLanSeen[slot] = now;
        }
    for (TInt i = gUi.iLanCount - 1; i >= 0; i--)
        {
        if (now - gUi.iLanSeen[i] < 4000u) continue;
        for (TInt j = i; j < gUi.iLanCount - 1; j++)
            {
            gUi.iLan[j] = gUi.iLan[j + 1];
            gUi.iLanSeen[j] = gUi.iLanSeen[j + 1];
            }
        gUi.iLanCount--;
        }
    }

/* ------------------------------------------------------------ сохранение настроек */

void WriteBe32(TUint8* aData, TInt aValue)
    {
    aData[0] = (TUint8)((TUint32)aValue >> 24);
    aData[1] = (TUint8)((TUint32)aValue >> 16);
    aData[2] = (TUint8)((TUint32)aValue >> 8);
    aData[3] = (TUint8)aValue;
    }

TInt ReadBe32(const TUint8* aData)
    {
    return (TInt)(((TUint32)aData[0] << 24) | ((TUint32)aData[1] << 16) |
                  ((TUint32)aData[2] << 8) | (TUint32)aData[3]);
    }

TInt WriteUiText(TUint8* aData, TInt aOffset, const TText* aText, TInt aMaximum)
    {
    TInt length = TextLength(aText, aMaximum);
    aData[aOffset++] = (TUint8)(length >> 8);
    aData[aOffset++] = (TUint8)length;
    for (TInt i = 0; i < length; i++)
        aData[aOffset++] = (TUint8)(aText[i] < 128 ? aText[i] : '?');
    return aOffset;
    }

void SaveUiSettingsL(CGame& aGame)
    {
    if (!aGame.iAssets.iFileServer) return;
    TBuf<192> path;
    path.Copy(aGame.iAssets.iDirectory);
    path.Append(_L("settings.bin"));
    RFile file;
    TInt error = file.Replace(*aGame.iAssets.iFileServer, path, EFileWrite);
    if (error != KErrNone)
        error = file.Create(*aGame.iAssets.iFileServer, path, EFileWrite);
    if (error != KErrNone) return;
    CleanupClosePushL(file);

    TUint8 data[320];
    Mem::FillZ(data, sizeof(data));
    TInt k = 0;
    data[k++] = 8;
    data[k++] = (TUint8)aGame.iSensitivity;
    data[k++] = (TUint8)aGame.iSensitivityVertical;
    data[k++] = (TUint8)(aGame.iInvertY ? 1 : 0);
    data[k++] = (TUint8)aGame.iLayout;
    data[k++] = (TUint8)aGame.iDifficulty;
    data[k++] = (TUint8)aGame.iVolume;
    data[k++] = (TUint8)aGame.iQuality;
    data[k++] = (TUint8)(aGame.iSoundOn ? 1 : 0);
    data[k++] = (TUint8)(aGame.iShowFps ? 1 : 0);
    data[k++] = (TUint8)aGame.iMaxRounds;
    data[k++] = (TUint8)(aGame.iAutoAim ? 1 : 0);
    data[k++] = (TUint8)aGame.iGammaIndex;
    data[k++] = (TUint8)aGame.iCrossColor;
    data[k++] = (TUint8)aGame.iCrossSize;
    data[k++] = (TUint8)(aGame.iCrossDynamic ? 1 : 0);
    data[k++] = (TUint8)(aGame.iViewBob ? 1 : 0);
    data[k++] = (TUint8)(aGame.iRadar ? 1 : 0);
    data[k++] = (TUint8)(aGame.iFirstRunDone ? 1 : 0);
    data[k++] = (TUint8)aGame.iWinScale;
    data[k++] = (TUint8)Clamp(aGame.iMapIndex, 0, 10);
    data[k++] = (TUint8)Clamp(aGame.iTeamChoice, 1, 3);
    data[k++] = (TUint8)Clamp(aGame.iBotCount, 1, 9);
    data[k++] = (TUint8)(Clamp(aGame.iRoundSeconds, 60, 300) / 15);
    data[k++] = (TUint8)(Clamp(aGame.iBuySeconds, 15, 90) / 5);
    data[k++] = (TUint8)Clamp(aGame.iFreezeSeconds, 0, 10);
    data[k++] = (TUint8)(Clamp(aGame.iStartMoney, 800, 16000) / 100 >> 8);
    data[k++] = (TUint8)(Clamp(aGame.iStartMoney, 800, 16000) / 100);
    data[k++] = (TUint8)(aGame.iFriendlyFire ? 1 : 0);
    data[k++] = (TUint8)(aGame.iFpsCap20 ? 1 : 0);
    data[k++] = (TUint8)(aGame.iPcMouse ? 1 : 0);
    data[k++] = (TUint8)Clamp(aGame.iGfx, 0, 3);
    data[k++] = (TUint8)Clamp(aGame.iNokiaFrameRate, 0, 5);
    data[k++] = (TUint8)Clamp(aGame.iOrientation, 0, 1);
    data[k++] = (TUint8)(aGame.iVoiceOn ? 1 : 0);
    data[k++] = (TUint8)Clamp(aGame.iVoiceVolume, 10, 100);
    data[k++] = (TUint8)Clamp(aGame.iLanguage, 0, 1);

    data[k++] = (TUint8)KMaxBinds;
    for (TInt i = 0; i < KMaxBinds; i++) WriteBe32(data + k + i * 8, gUi.iBindings[i].iFirst);
    k += KMaxBinds * 8;
    for (TInt i = 0; i < KMaxBinds; i++) WriteBe32(data + k + i * 4, gUi.iBindings[i].iSecond);
    k += KMaxBinds * 4;
    k = WriteUiText(data, k, gUi.iServer, 63);
    k = WriteUiText(data, k, gUi.iNick, 23);
    file.Write(TPtrC8(data, k));
    CleanupStack::PopAndDestroy(&file);
    }

void SaveUiSettings(CGame& aGame)
    {
    TInt error = KErrNone;
    TRAP(error, SaveUiSettingsL(aGame));
    }

void CommitSettings(CGame& aGame)
    {
    aGame.SaveSettings();
    SaveUiSettings(aGame);
    }

void LoadUiSettings(CGame& aGame)
    {
    if (!aGame.iAssets.iFileServer) return;
    TBuf<192> path;
    path.Copy(aGame.iAssets.iDirectory);
    path.Append(_L("settings.bin"));
    RFile file;
    if (file.Open(*aGame.iAssets.iFileServer, path, EFileRead) != KErrNone) return;
    TPtr8 buffer((TUint8*)gUi.iWorldBuffer, sizeof(gUi.iWorldBuffer));
    TInt length = 0;
    if (file.Read(buffer, length) != KErrNone || length < 39 + KMaxBinds * 8) return;
    const TUint8* data = buffer.Ptr();
    const TInt count = data[38];
    if (count <= 0 || count > KMaxBinds) return;
    for (TInt i = 0; i < KMaxBinds; i++)
        {
        gUi.iBindings[i].iFirst = i < count ? ReadBe32(data + 39 + i * 8) : KDefaultBinds[i];
        gUi.iBindings[i].iSecond = i < count ? ReadBe32(data + 39 + KMaxBinds * 8 + i * 4) : 0;
        }
    TInt offset = 39 + KMaxBinds * 16;
    if (offset + 2 <= length)
        {
        const TInt serverLength = MinI(MinI((TInt)data[offset] << 8 | data[offset + 1], 63), length - offset - 2);
        offset += 2;
        for (TInt i = 0; i < serverLength; i++) gUi.iServer[i] = (TText)(TUint8)data[offset + i];
        gUi.iServer[serverLength] = 0;
        gUi.iServerLength = serverLength;
        offset += serverLength;
        }
    if (offset + 2 <= length)
        {
        const TInt nickLength = MinI(MinI((TInt)data[offset] << 8 | data[offset + 1], 23), length - offset - 2);
        offset += 2;
        for (TInt i = 0; i < nickLength; i++) gUi.iNick[i] = (TText)(TUint8)data[offset + i];
        gUi.iNick[nickLength] = 0;
        gUi.iNickLength = nickLength;
        }
    }

/* ------------------------------------------------------------ имя игрока и сервер */

TText* EditTextBuffer(TInt aMode)
    { return aMode == 1 ? gUi.iNick : gUi.iServer; }

TInt& EditTextLength(TInt aMode)
    { return aMode == 1 ? gUi.iNickLength : gUi.iServerLength; }

TInt EditTextMaximum(TInt aMode)
    { return aMode == 1 ? 20 : 64; }

void TrimTextBuffer(TText* aBuffer, TInt& aLength)
    {
    while (aLength > 0 && aBuffer[aLength - 1] == (TText)' ') aLength--;
    TInt first = 0;
    while (first < aLength && aBuffer[first] == (TText)' ') first++;
    if (first > 0)
        {
        for (TInt i = first; i < aLength; i++) aBuffer[i - first] = aBuffer[i];
        aLength -= first;
        }
    aBuffer[aLength] = 0;
    }

void UpdateEditValues(CGame& aGame)
    {
    if (gUi.iEditing <= 0) return;
    TText* buffer = EditTextBuffer(gUi.iEditing);
    TInt& length = EditTextLength(gUi.iEditing);
    if (length < 0) length = 0;
    TBuf<112> value;
    AppendLimited(value, TPtrC(buffer, length));
    CopyText(aGame.iStatusLine, sizeof(aGame.iStatusLine) / sizeof(TText), value);
    }

void FinishTextEdit(CGame& aGame, TBool aCommit)
    {
    const TInt mode = gUi.iEditing;
    if (mode <= 0) return;
    TText* buffer = EditTextBuffer(mode);
    TInt& length = EditTextLength(mode);
    TrimTextBuffer(buffer, length);
    if (aCommit)
        {
        if (mode == 1)
            {
            if (length > 0)
                {
                CopyText(gUi.iNick, 32, TPtrC(buffer, length));
                gUi.iNickLength = length;
                CommitSettings(aGame);
                }
            }
        else
            {
            CopyText(gUi.iServer, 96, TPtrC(buffer, length));
            gUi.iServerLength = length;
            CommitSettings(aGame);
            }
        }
    gUi.iEditing = 0;
    aGame.iStatusLine[0] = 0;
    aGame.iMenu = EMenuMultiplayer;
    aGame.iMenuSelection = mode == 1 ? 2 : 3;
    }

TBool IsEditableCode(TInt aCode)
    {
    if (aCode >= 0x20 && aCode <= 0x7E) return ETrue;
    if (aCode == 0x400 || aCode == 0x401 || aCode == 0x451) return ETrue;
    return aCode >= 0x410 && aCode <= 0x44F;
    }

void ApplyNick(CGame& aGame)
    {
    gUi.iEditing = 1;
    aGame.iMenu = EMenuNone;
    UpdateEditValues(aGame);
    }

void ApplyServer(CGame& aGame)
    {
    gUi.iEditing = 2;
    aGame.iMenu = EMenuNone;
    UpdateEditValues(aGame);
    }

void SetStatus(CGame& aGame, TInt aIndex)
    {
    CopyText(gUi.iStatus, KMenuTextLength, LocalText(aIndex, aGame.iLanguage));
    const TInt length = TextLength(gUi.iStatus, KMenuTextLength);
    CopyText(aGame.iStatusLine, sizeof(aGame.iStatusLine) / sizeof(TText),
             TPtrC(gUi.iStatus, length));
    }

void ClearKeys(CGame& aGame)
    {
    aGame.iKeys = 0;
    for (TInt i = 0; i < 30; i++)
        {
        gUi.iHeldMasks[i] = 0;
        gUi.iKeyRefs[i] = 0;
        }
    }

/* ------------------------------------------------------------ раскладки клавиш */

void ResetBindings(TInt aLayout)
    {
    for (TInt i = 0; i < KMaxBinds; i++)
        {
        gUi.iBindings[i].iFirst = 0;
        gUi.iBindings[i].iSecond = 0;
        }
    if (aLayout == 1)
        {
        gUi.iBindings[0].iFirst = -1;
        gUi.iBindings[1].iFirst = -2;
        gUi.iBindings[4].iFirst = -3;
        gUi.iBindings[5].iFirst = -4;
        gUi.iBindings[6].iFirst = '2';
        gUi.iBindings[7].iFirst = '8';
        }
    else
        {
        gUi.iBindings[0].iFirst = '2';
        gUi.iBindings[1].iFirst = '8';
        gUi.iBindings[4].iFirst = -3;
        gUi.iBindings[5].iFirst = -4;
        gUi.iBindings[6].iFirst = -1;
        gUi.iBindings[7].iFirst = -2;
        }
    gUi.iBindings[2].iFirst = '4';
    gUi.iBindings[3].iFirst = '6';
    gUi.iBindings[8].iFirst = '5';
    gUi.iBindings[8].iSecond = -5;
    gUi.iBindings[9].iFirst = '1';
    gUi.iBindings[10].iFirst = '3';
    gUi.iBindings[11].iFirst = '7';
    gUi.iBindings[12].iFirst = '0';
    gUi.iBindings[13].iFirst = '9';
    gUi.iBindings[15].iFirst = '*';
    gUi.iBindings[17].iFirst = '#';
    gUi.iBindings[19].iFirst = -6;
    gUi.iBindings[19].iSecond = -8;
    gUi.iBindings[20].iFirst = -7;
    gUi.iBindings[21].iFirst = -10;
    }

TInt KeySlot(TInt aCode)
    {
    if (aCode >= '0' && aCode <= '9') return aCode - '0';
    if (aCode == '*') return 10;
    if (aCode == '#') return 11;
    switch (aCode)
        {
    case 0x101: return 12;
    case 0x102: return 13;
    case 0x103: return 14;
    case 0x104: return 15;
    case 0x105: return 16;
    case 0x107: return 17;
    case 0x108: return 18;
    case 0x109: return 19;
    case 0x10A: return 20;
    case 0x106: return 21;
    default: return 22 + (aCode & 7);
        }
    }

TInt CodeFromMask(TInt aMask)
    {
    if (aMask <= 0) return 0;
    if (aMask >= '0' && aMask <= '9') return aMask;
    if (aMask == (EActionAlt | EActionTabLeft)) return '*';
    if (aMask == (EActionBuy | EActionTabRight)) return '#';
    switch (aMask)
        {
    case EActionForward: return '2';
    case EActionBack: return '8';
    case EActionStrafeLeft: return '4';
    case EActionStrafeRight: return '6';
    case EActionFire: return '5';
    case EActionJump: return '1';
    case EActionDuck: return '3';
    case EActionReload: return '7';
    case EActionUse: return '0';
    case EActionNextWeapon: return '9';
    case EActionLookUp | EActionMenuUp: return 0x101;
    case EActionLookDown | EActionMenuDown: return 0x102;
    case EActionTurnLeft | EActionMenuLeft: return 0x103;
    case EActionTurnRight | EActionMenuRight: return 0x104;
    case EActionFire | EActionSelect: return 0x105;
    case EActionScore: return 0x106;
    case EActionVoice: return 0x107;
    case EActionEscape: return 0x108;
    case EActionMenu | EActionEscape: return 0x109;
    case EActionMenu: return 0x10A;
    default: return 0;
        }
    }

TInt BindingMask(TInt aCode)
    {
    if (aCode == 0) return 0;
    TInt mask = 0;
    for (TInt i = 0; i < KMaxBinds; i++)
        {
        if (gUi.iBindings[i].iFirst == aCode || gUi.iBindings[i].iSecond == aCode)
            mask |= KBindActions[i];
        }
    return mask;
    }

TInt MenuActionFromMask(TInt aMask)
    {
    if (aMask & EActionMenuUp) return EActionMenuUp;
    if (aMask & EActionMenuDown) return EActionMenuDown;
    if (aMask & EActionTabLeft) return EActionTabLeft;
    if (aMask & EActionTabRight) return EActionTabRight;
    if (aMask & EActionMenuLeft) return EActionMenuLeft;
    if (aMask & EActionMenuRight) return EActionMenuRight;
    if (aMask & EActionSelect) return EActionSelect;
    if (aMask & EActionEscape) return EActionEscape;
    return EActionNone;
    }

/* ------------------------------------------------------------ ввод и меню */

TBool IsOverlayMenuId(TInt aMenu)
    { return IsOverlayMenu(aMenu); }

void KeyAction(CGame& aGame, TInt aAction, TBool aDown)
    {
    if (aAction == EActionNone) return;
    if (aDown) aGame.iKeys |= aAction;
    else aGame.iKeys &= ~aAction;
    if (!aDown)
        {
        if (aAction == EActionTurnLeft || aAction == EActionTurnRight) aGame.iTurnHeld = 0;
        return;
        }
    if (aGame.iState == EStateMenu || aGame.iState == EStateEnd ||
        (aGame.iState == EStatePlay && aGame.iMenu != EMenuNone))
        {
        aGame.MenuKey(aAction);
        return;
        }
    if (aGame.iState != EStatePlay) return;
    if (aGame.iBuyOpen)
        {
        if (aAction == EActionMenu || aAction == EActionBuy || aAction == EActionEscape)
            {
            if (aGame.iBuyCategory >= 0) aGame.iBuyCategory = -1;
            else aGame.iBuyOpen = EFalse;
            }
        else if (aAction == EActionMenuUp || aAction == EActionLookUp)
            gUi.iBuySelection = MaxI(0, gUi.iBuySelection - 1);
        else if (aAction == EActionMenuDown || aAction == EActionLookDown)
            gUi.iBuySelection++;
        else if (aAction == EActionSelect)
            aGame.Digit(gUi.iBuySelection == 8 ? 9 : gUi.iBuySelection + 1);
        return;
        }
    if (aAction == EActionMenu) { aGame.iMenu = EMenuPause; aGame.iMenuSelection = 0; return; }
    if (aAction == EActionScore) { aGame.iScoreOpen = !aGame.iScoreOpen; return; }
    TPlayer* me = aGame.iMyIndex >= 0 && aGame.iMyIndex < KMaxPlayers
        ? &aGame.iPlayers[aGame.iMyIndex] : NULL;
    if (!me) return;
    if (!me->iAlive)
        {
        if (aAction == EActionFire || aAction == EActionSelect || aAction == EActionNextWeapon)
            {
            for (TInt k = 1; k <= KMaxPlayers; k++)
                {
                TInt index = (aGame.iSpectatorIndex < 0 ? 0 : aGame.iSpectatorIndex) + k;
                index %= aGame.iPlayerCount > 0 ? aGame.iPlayerCount : KMaxPlayers;
                if (aGame.iPlayers[index].iAlive &&
                    (aGame.iPlayers[index].iTeam == me->iTeam || !aGame.AnyAlive(me->iTeam)))
                    {
                    aGame.iSpectatorIndex = index;
                    return;
                    }
                }
            aGame.iSpectatorIndex = -1;
            }
        return;
        }
    if (aAction == EActionBuy)
        {
        if (aGame.iTime < aGame.iBuyEnd && !aGame.iBombPlanted)
            {
            aGame.iBuyOpen = ETrue;
            aGame.iBuyCategory = -1;
            gUi.iBuySelection = 0;
            }
        else { aGame.iMenu = EMenuGame; aGame.iMenuSelection = 0; }
        }
    else if (aAction == EActionNextWeapon) aGame.NextWeapon(*me);
    else if (aAction == EActionReload)
        {
        aGame.StartReload(*me);
        SendNetAction(aGame, 'L', -1, -1);
        }
    else if (aAction == EActionAlt) aGame.AlternateFire(*me);
    else if (aAction == EActionLastWeapon) aGame.SwitchWeapon(*me, gExtra[aGame.iMyIndex].iLast);
    else if (aAction == EActionDrop)
        {
        aGame.DropCurrent(*me);
        SendNetAction(aGame, 'D', -1, -1);
        }
    else if (aAction == EActionRadio) { aGame.iMenu = EMenuRadio1; aGame.iMenuSelection = 0; }
    }

void SwitchTab(CGame& aGame, TInt aDirection)
    {
    TInt tab = OptionTab(aGame.iMenu);
    if (tab < 0) return;
    tab = (tab + aDirection + 5) % 5;
    aGame.iMenu = KOptTabs[tab];
    if (aGame.iMenu == EMenuLanguage) aGame.iMenuSelection = aGame.iLanguage; else aGame.iMenuSelection = 0;
    }

void OpenOptions(CGame& aGame, TInt aReturn)
    {
    aGame.iOptReturn = aReturn;
    aGame.iOptReturnSelection = aGame.iMenuSelection;
    aGame.iMenu = EMenuGraphics;
    aGame.iMenuSelection = 0;
    gUi.iTabFocus = EFalse;
    }

void CloseOptions(CGame& aGame)
    {
    CommitSettings(aGame);
    aGame.iMenu = aGame.iOptReturn;
    aGame.iMenuSelection = aGame.iOptReturnSelection;
    gUi.iTabFocus = EFalse;
    gUi.iBindCapture = -1;
    aGame.iBindCapture = -1;
    }

void ApplyQuality(CGame& aGame, TInt aQuality)
    {
    aGame.iQuality = Clamp(aQuality, 0, 3);
    aGame.SetLevel(aGame.iQuality == 0 ? 0 : aGame.iQuality == 1 ? 0 :
                   aGame.iQuality == 2 ? 2 : 4);
    }

void ApplyGfxMode(CGame& aGame, TInt aGfx)
    {
    aGame.iGfx = Clamp(aGfx, 0, 3);
    aGame.iGfxSetting = aGame.iGfx;
    aGame.ApplyGfx();
    }

void BuyWeaponUi(CGame& aGame, TInt aWeapon)
    {
    TPlayer* me = aGame.iMyIndex >= 0 && aGame.iMyIndex < KMaxPlayers
        ? &aGame.iPlayers[aGame.iMyIndex] : NULL;
    if (!me || !me->iAlive || aWeapon < 0 || aWeapon >= KWeaponCount) return;
    if (KWpTeam[aWeapon] != 0 && KWpTeam[aWeapon] != me->iTeam) return;
    if (me->iMoney < KWpPrice[aWeapon]) return;
    me->iMoney -= KWpPrice[aWeapon];
    me->iOwns[aWeapon] = ETrue;
    const TInt ammo = KWpReserve[aWeapon];
    if (KWpSlot[aWeapon] == 0) me->iPrimary = aWeapon;
    else if (KWpSlot[aWeapon] == 1) me->iSecondary = aWeapon;
    me->iAmmo[aWeapon] = ammo;
    if (me->iClip[aWeapon] <= 0) me->iClip[aWeapon] = KWpClip[aWeapon];
    aGame.SwitchWeapon(*me, aWeapon);
    aGame.iBuyOpen = EFalse;
    SendNetAction(aGame, 'B', 0, aWeapon);
    }

void BuyEquipUi(CGame& aGame, TInt aKind)
    {
    TPlayer* me = aGame.iMyIndex >= 0 && aGame.iMyIndex < KMaxPlayers
        ? &aGame.iPlayers[aGame.iMyIndex] : NULL;
    if (!me || !me->iAlive || aKind < 0 || aKind >= 6) return;
    if (aKind == 5 && me->iTeam != 2) return;
    if (me->iMoney < KEquipPrices[aKind]) return;
    me->iMoney -= KEquipPrices[aKind];
    if (aKind == 0) me->iArmor = MaxF(me->iArmor, 100);
    else if (aKind == 1) { me->iArmor = MaxF(me->iArmor, 100); gExtra[aGame.iMyIndex].iHelmet = ETrue; }
    else gExtra[aGame.iMyIndex].iHe = aKind;
    SendNetAction(aGame, 'B', 1, aKind);
    }

void BuyAmmoUi(CGame& aGame, TInt aSlot)
    {
    TPlayer* me = aGame.iMyIndex >= 0 && aGame.iMyIndex < KMaxPlayers
        ? &aGame.iPlayers[aGame.iMyIndex] : NULL;
    if (!me || aSlot < 0 || aSlot > 1) return;
    const TInt weapon = aSlot == 0 ? me->iPrimary : me->iSecondary;
    if (weapon < 0 || weapon >= KWeaponCount) return;
    if (me->iMoney < KWpAmmoPrice[weapon]) return;
    me->iMoney -= KWpAmmoPrice[weapon];
    me->iAmmo[weapon] += KWpReserve[weapon];
    SendNetAction(aGame, 'B', 2, aSlot);
    }

void StartHost(CGame& aGame, TBool aLan)
    {
    gUi.iMpMode = Clamp(gUi.iMpMode, 0, 4);
    gUi.iMpMap = Clamp(gUi.iMpMap, 0, 10);
    gUi.iMode = gUi.iMpMode;
    aGame.iPendingMap = gUi.iMpMap;
    ConnectHost(aGame, ETrue, aLan);
    }

void StartWorldSearch(CGame& aGame, TBool aCreate)
    {
    gUi.iNetworkCreate = aCreate;
    gUi.iNetworkRequest = ETrue;
    gUi.iRoomRequestAt = User::NTickCount();
    gUi.iRoomCount = 0;
    gUi.iStatus[0] = 0;
    CopyText(gUi.iLoadMessage, 128, LocalText(aCreate ? 48 : 70, aGame.iLanguage));
    if (aCreate) ConnectHost(aGame, ETrue, EFalse);
    else ConnectHost(aGame, EFalse, EFalse);
    }

void ResetCreate(CGame& aGame)
    {
    gUi.iCreateMap = Clamp(aGame.iMapIndex, 0, 10);
    gUi.iCreateTeam = aGame.iTeamChoice;
    gUi.iCreateBots = aGame.iBotCount;
    gUi.iCreateRoundTime = aGame.iRoundSeconds;
    gUi.iCreateBuyTime = aGame.iBuySeconds;
    gUi.iCreateFreeze = aGame.iFreezeSeconds;
    gUi.iCreateMoney = aGame.iStartMoney;
    gUi.iCreateFriendlyFire = aGame.iFriendlyFire;
    }

void ApplyCreate(CGame& aGame)
    {
    CommitSettings(aGame);
    const TInt map = Clamp(gUi.iCreateMap, 0, 10);
    if (map == 7 || map == 8 || map == 9 || map == 10) aGame.iMode = 4;
    else aGame.iMode = 0;
    const TInt team = gUi.iCreateTeam == 3 ? 1 + (aGame.iRandom.Next() & 1) : gUi.iCreateTeam;
    aGame.StartMatch(map, team);
    }

void NextMapAfterEnd(CGame& aGame)
    {
    aGame.iState = EStateMenu;
    aGame.iMenu = EMenuMain;
    aGame.iMenuSelection = 0;
    aGame.StartMatch((aGame.iMapIndex + 1) % 11,
                     aGame.iMyIndex >= 0 && aGame.iMyIndex < KMaxPlayers
                     ? aGame.iPlayers[aGame.iMyIndex].iTeam : 2);
    }

void SwitchTeam(CGame& aGame, TInt aTeam)
    {
    TPlayer* me = aGame.iMyIndex >= 0 && aGame.iMyIndex < KMaxPlayers
        ? &aGame.iPlayers[aGame.iMyIndex] : NULL;
    if (!me) return;
    me->iTeam = Clamp(aTeam, 1, 2);
    aGame.iTeamChoice = me->iTeam;
    aGame.SetupPlayers(me->iTeam);
    aGame.NewRound(ETrue);
    }

void StartMicTest(CGame& aGame)
    {
    if (gUi.iMicState == 3) gUi.iMicState = 0;
    else if (gUi.iMicState != 0) return;
    if (!aGame.iAudio.StartRecording())
        {
        SetStatus(aGame, 376);
        gUi.iMicState = 2;
        gUi.iMicAt = User::NTickCount();
        return;
        }
    gUi.iMicState = 1;
    gUi.iMicAt = User::NTickCount();
    CopyText(gUi.iStatus, KMenuTextLength, LocalText(234, aGame.iLanguage));
    }

void ReleaseCapture(CGame& aGame)
    {
    if (aGame.iConnection) aGame.iConnection->Close();
    gUi.iNetworkRequest = EFalse;
    gUi.iNetworkCreate = EFalse;
    CloseLanSearch(aGame);
    CloseWorldSearch(aGame);
    aGame.iState = EStateMenu;
    aGame.iMenu = EMenuMultiplayer;
    aGame.iMenuSelection = 4;
    }

void MenuBack(CGame& aGame)
    {
    switch (aGame.iMenu)
        {
    case EMenuPause: aGame.iMenu = EMenuNone; break;
    case EMenuTeam:
        aGame.iMenu = aGame.iState == EStatePlay ? EMenuPause : EMenuMain;
        aGame.iMenuSelection = aGame.iState == EStatePlay ? 1 : 0;
        break;
    case EMenuGraphics: case EMenuControls: case EMenuSound:
    case EMenuBinds: case EMenuLanguage: CloseOptions(aGame); break;
    case EMenuFirstRun1: case EMenuFirstRun2: break;
    case EMenuGame: case EMenuRadio1: case EMenuRadio2: case EMenuRadio3:
        aGame.iMenu = EMenuNone;
        break;
    case EMenuCreate: CommitSettings(aGame); aGame.iMenu = EMenuMain; aGame.iMenuSelection = 0; break;
    case EMenuMultiplayer: aGame.iMenu = EMenuMain; aGame.iMenuSelection = 1; break;
    case EMenuLan: CloseLanSearch(aGame); aGame.iMenu = EMenuMultiplayer; aGame.iMenuSelection = 7; break;
    case EMenuWorld: CloseWorldSearch(aGame); aGame.iMenu = EMenuMultiplayer; aGame.iMenuSelection = 4; break;
    case EMenuSearch: ReleaseCapture(aGame); break;
    case EMenuHelp: aGame.iMenu = aGame.iHelpReturn; aGame.iHelpReturn = EMenuMain; aGame.iMenuSelection = 0; break;
    default:
        if (aGame.iState == EStatePlay) aGame.iMenu = EMenuNone;
        else { aGame.iMenu = EMenuMain; aGame.iMenuSelection = 0; }
        break;
        }
    }

void AppendNum8(TDes8& aDestination, TInt aValue)
    {
    TUint8 digits[12];
    TInt length = 0;
    TInt value = aValue < 0 ? -aValue : aValue;
    if (value == 0) digits[length++] = (TUint8)'0';
    while (value > 0 && length < 12) { digits[length++] = (TUint8)('0' + value % 10); value /= 10; }
    while (length > 0) aDestination.Append(digits[--length]);
    }

}  // anonymous namespace

void CGame::KeyMask(TInt aMask, TBool aDown)
    {
    if (aMask == 0) return;
    const TBool menuish = iState != EStatePlay || iMenu != EMenuNone || iBuyOpen;
    if (menuish && aDown)
        {
        iKeys |= aMask;
        TInt representative = aMask & EActionMenuUp ? EActionMenuUp
            : aMask & EActionMenuDown ? EActionMenuDown
            : aMask & EActionSelect ? EActionSelect
            : aMask & EActionEscape ? EActionEscape
            : aMask & EActionTabLeft ? EActionTabLeft
            : aMask & EActionTabRight ? EActionTabRight
            : aMask & EActionMenuLeft ? EActionMenuLeft
            : aMask & EActionMenuRight ? EActionMenuRight
            : aMask & -aMask;
        KeyAction(*this, representative, ETrue);
        return;
        }
    for (TInt bit = 1; bit > 0 && bit <= EActionVoice; bit <<= 1)
        {
        if ((aMask & bit) == 0) continue;
        KeyAction(*this, bit, aDown);
        if (aDown && (iMenu != EMenuNone || iBuyOpen || iState != EStatePlay)) break;
        }
    }

TBool CGame::RawKey(TInt aCode)
    {
    EnsureUi(*this);
    if (gUi.iEditing != 0)
        {
        const TInt mode = gUi.iEditing;
        TText* buffer = EditTextBuffer(mode);
        TInt& length = EditTextLength(mode);
        if (aCode == EActionEscape || aCode == 0x108 || aCode == 0x109)
            {
            FinishTextEdit(*this, EFalse);
            return ETrue;
            }
        if (aCode == 8 || aCode == 0x7F)
            {
            if (length > 0)
                {
                length--;
                buffer[length] = 0;
                UpdateEditValues(*this);
                }
            return ETrue;
            }
        if (aCode == 0x105 || aCode == 0x10A || aCode == '\r' || aCode == '\n')
            {
            FinishTextEdit(*this, ETrue);
            return ETrue;
            }
        if (IsEditableCode(aCode) && length < EditTextMaximum(mode))
            {
            buffer[length++] = (TText)aCode;
            buffer[length] = 0;
            UpdateEditValues(*this);
            }
        return ETrue;
        }
    if (iBindCapture >= 0 && iMenu == EMenuBinds)
        {
        if ((TReal32)User::NTickCount() > iBindUntil)
            {
            iBindCapture = -1;
            return EFalse;
            }
        if (iPcMouse && aCode == 27)
            {
            iBindCapture = -1;
            return ETrue;
            }
        const TInt code = NormalizeKeyCode(aCode);
        if (code != 0)
            {
            AssignBinding(gUi.iBindings[iBindCapture], code, iLayout);
            if (!iPcMouse) iLayout = 2;
            iBindCapture = -1;
            ClearKeys(*this);
            CommitSettings(*this);
            }
        return ETrue;
        }
    if (iMenu == EMenuWorld && gUi.iRoomCount > 0 && aCode == EActionRadio)
        {
        TInt index = iMenuSelection;
        if (index >= 0 && index < gUi.iRoomCount)
            {
            gUi.iNetworkCreate = EFalse;
            gUi.iNetworkRequest = ETrue;
            TPtrC id(gUi.iRooms[index].iId, gUi.iRooms[index].iIdLength);
            TBuf8<64> host;
            for (TInt i = 0; i < id.Length() && i < host.MaxLength(); i++)
                host.Append((TUint8)(id[i] & 0xFF));
            if (host.Length() == 0) TextureName(host, "broker.emqx.io");
            TInt error = KErrNone;
            if (!iConnection) TRAP(error, iConnection = CNetConnection::NewL(iSocketServer));
            if (error == KErrNone && iConnection)
                {
                iConnection->Close();
                TRAP(error, iConnection->ConnectL(host, KNetPort));
                }
            if (error != KErrNone)
                {
                SetStatus(*this, 44);
                return ETrue;
                }
            iMode = gUi.iRooms[index].iMode;
            iPendingMap = Clamp(gUi.iRooms[index].iMap, 0, 2);
            iNetOut.Reset();
            iNetOut.U8(KNetProtocol).U8(iMode).U8(iPendingMap)
                   .Str(TPtrC(gUi.iNick, gUi.iNickLength)).U8(0);
            iConnection->Send(EClientJoin, iNetOut);
            iState = EStateLoad;
            iMenu = EMenuSearch;
            iMenuSelection = 0;
            }
        return ETrue;
        }
    // Terminator disguise key
    if (iState == EStatePlay && iMenu == EMenuNone && iMode == 3 && (aCode == (TInt)iKeyDisguise || aCode == '*'))
        {
        const TInt termIdx = (iTerminatorIdx >= 0 && iTerminatorIdx < iPlayerCount) ? iTerminatorIdx : -1;
        if (termIdx == iMyIndex && (iTerminatorClass == 2 || iTerminatorClass == 3) && iDisguiseCooldown <= 0.0f)
            {
            iIsDisguised = ETrue;
            iDisguiseTimer = (iTerminatorClass == 3) ? 30.0f : 20.0f;
            iDisguiseCooldown = (iTerminatorClass == 3) ? 30.0f : 45.0f;
            }
        return ETrue;
        }
    return EFalse;
    }

TBool CGame::WantsDigits() const
    {
    return iState == EStateMenu || iState == EStateEnd ||
           (iState == EStatePlay && (iMenu != EMenuNone || iBuyOpen));
    }

TBool CGame::TurnDigits() const
    { return iState == EStatePlay && iMenu == EMenuNone && !iBuyOpen; }

void CGame::Digit(TInt aDigit)
    {
    EnsureUi(*this);
    if (iState != EStatePlay || iMenu != EMenuNone)
        {
        if (iState == EStateEnd || iMenu == EMenuHelp)
            {
            MenuKey(EActionSelect);
            return;
            }
        if (IsOptionsMenu(iMenu))
            {
            MenuKey(aDigit == 2 ? EActionMenuUp : aDigit == 8 ? EActionMenuDown
                : aDigit == 4 ? EActionTabLeft : aDigit == 6 ? EActionTabRight
                : aDigit == 5 ? EActionSelect : aDigit == 0 ? EActionMenu : EActionNone);
            return;
            }
        BuildMenuItems(*this);
        if (aDigit == 0) { MenuKey(EActionMenu); return; }
        if (aDigit >= 1 && aDigit <= gMenuCount)
            {
            iMenuSelection = aDigit - 1;
            MenuKey(EActionSelect);
            }
        else if (aDigit == 5) MenuKey(EActionSelect);
        return;
        }
    if (!iBuyOpen) return;
    TPlayer* me = iMyIndex >= 0 && iMyIndex < KMaxPlayers ? &iPlayers[iMyIndex] : NULL;
    if (!me) return;
    if (iBuyCategory < 0)
        {
        if (aDigit >= 1 && aDigit <= 5) { iBuyCategory = aDigit - 1; gUi.iBuySelection = 0; }
        else if (aDigit == 6) BuyAmmoUi(*this, 0);
        else if (aDigit == 7) BuyAmmoUi(*this, 1);
        else if (aDigit == 8) { iBuyCategory = 7; gUi.iBuySelection = 0; }
        else if (aDigit == 9) { iBuyOpen = EFalse; iMenu = EMenuGame; iMenuSelection = 0; }
        else if (aDigit == 0) iBuyOpen = EFalse;
        }
    else if (iBuyCategory == 7)
        {
        if (aDigit == 0) { iBuyCategory = -1; return; }
        BuyEquipUi(*this, aDigit - 1);
        }
    else
        {
        if (aDigit == 0) { iBuyCategory = -1; return; }
        const TInt category = Clamp(iBuyCategory, 0, 4);
        if (aDigit - 1 < KBuyListLength[category]) BuyWeaponUi(*this, KBuyLists[category][aDigit - 1]);
        }
    }

void CGame::Look(TReal32 aDeltaYaw, TReal32 aDeltaPitch)
    {
    if (iState != EStatePlay || iMenu != EMenuNone || iMyIndex < 0 ||
        iMyIndex >= KMaxPlayers || !iPlayers[iMyIndex].iAlive || iBuyOpen) return;
    TPlayer& player = iPlayers[iMyIndex];
    const TReal32 zoom = player.iZoom == 0 ? 1.0f : player.iZoom == 1 ? 0.45f : 0.2f;
    player.iYaw += aDeltaYaw * zoom;
    player.iPitch += (iInvertY ? -aDeltaPitch : aDeltaPitch) * zoom;
    if (player.iPitch > 89.0f) player.iPitch = 89.0f;
    if (player.iPitch < -89.0f) player.iPitch = -89.0f;
    }

void CGame::PauseFromSystem()
    {
    ClearKeys(*this);
    if (iState == EStatePlay && (iMenu == EMenuNone || IsOverlayMenuId(iMenu)))
        {
        iMenu = EMenuPause;
        iMenuSelection = 0;
        }
    }

TInt CGame::MenuTitle(TDes& aBuffer)
    {
    EnsureUi(*this);
    TitleText(*this, aBuffer);
    return aBuffer.Length();
    }

TBool CGame::ChangeOption(TInt aDirection)
    {
    EnsureUi(*this);
    switch (iMenu)
        {
    case EMenuMultiplayer:
        if (iMenuSelection == 0) { gUi.iMpMode = (gUi.iMpMode + 5 + aDirection) % 5; return ETrue; }
        if (iMenuSelection == 1) { gUi.iMpMap = (gUi.iMpMap + 11 + aDirection) % 11; return ETrue; }
        return EFalse;
    case EMenuCreate:
        switch (iMenuSelection)
            {
        case 0: gUi.iCreateMap = (gUi.iCreateMap + 11 + aDirection) % 11; return ETrue;
        case 1: gUi.iCreateTeam = ((gUi.iCreateTeam - 1 + 3 + aDirection) % 3) + 1; return ETrue;
        case 2: iDifficulty = (iDifficulty + 4 + aDirection) % 4; return ETrue;
        case 3:
            gUi.iCreateBots += aDirection;
            if (gUi.iCreateBots > 9) gUi.iCreateBots = 1;
            if (gUi.iCreateBots < 1) gUi.iCreateBots = 9;
            return ETrue;
        case 4:
            iMaxRounds += aDirection * 3;
            if (iMaxRounds > 30) iMaxRounds = 3;
            if (iMaxRounds < 3) iMaxRounds = 30;
            return ETrue;
        case 5:
            gUi.iCreateRoundTime += aDirection * 15;
            if (gUi.iCreateRoundTime > 300) gUi.iCreateRoundTime = 60;
            if (gUi.iCreateRoundTime < 60) gUi.iCreateRoundTime = 300;
            return ETrue;
        case 6:
            gUi.iCreateBuyTime += aDirection * 15;
            if (gUi.iCreateBuyTime > 90) gUi.iCreateBuyTime = 15;
            if (gUi.iCreateBuyTime < 15) gUi.iCreateBuyTime = 90;
            return ETrue;
        case 7:
            gUi.iCreateFreeze += aDirection;
            if (gUi.iCreateFreeze > 10) gUi.iCreateFreeze = 0;
            if (gUi.iCreateFreeze < 0) gUi.iCreateFreeze = 10;
            return ETrue;
        case 8:
            gUi.iCreateMoney += aDirection * (gUi.iCreateMoney >= 4000 ? 1000 : 400);
            if (gUi.iCreateMoney > 16000) gUi.iCreateMoney = 800;
            if (gUi.iCreateMoney < 800) gUi.iCreateMoney = 16000;
            if (gUi.iCreateMoney > 4000 && gUi.iCreateMoney % 1000 != 0)
                gUi.iCreateMoney = (gUi.iCreateMoney / 1000) * 1000;
            return ETrue;
        case 9: gUi.iCreateFriendlyFire = !gUi.iCreateFriendlyFire; return ETrue;
            }
        return EFalse;
    case EMenuGraphics:
        switch (iMenuSelection)
            {
        case 0:
            {
            TInt m = iRenderMode;
            if (aDirection > 0)
                {
                m++;
                if (m == 1 && !iGpuAvailable) m = 2;
                if (m > 2) m = 0;
                }
            else
                {
                m--;
                if (m == 1 && !iGpuAvailable) m = 0;
                if (m < 0) m = 2;
                }
            iRenderMode = m;
            return ETrue;
            }
        case 1:
            CycleMod(aDirection);
            return ETrue;
        case 2: ApplyGfxMode(*this, (iGfx + 4 + aDirection) % 4); return ETrue;
        case 3:
            if (iNokiaFrameRate > 0) return ETrue;
            ApplyQuality(*this, (iQuality + 4 + aDirection) % 4);
            return ETrue;
        case 4:
            iNokiaFrameRate = (iNokiaFrameRate + 6 + aDirection) % 6;
            iGeneratedValid = EFalse;
            if (iNokiaFrameRate == 0) ApplyQuality(*this, iQuality);
            return ETrue;
        case 5: iGammaIndex = (iGammaIndex + 5 + aDirection) % 5; ApplyGamma(); return ETrue;
        case 6: iViewBob = !iViewBob; return ETrue;
        case 7: iCrossColor = (iCrossColor + 5 + aDirection) % 5; return ETrue;
        case 8: iCrossSize = (iCrossSize + 3 + aDirection) % 3; return ETrue;
        case 9: iCrossDynamic = !iCrossDynamic; return ETrue;
        case 10: iRadar = !iRadar; return ETrue;
        case 11: iShowFps = !iShowFps; return ETrue;
        case 12: iOrientation = 1 - iOrientation; SetOrientation(iOrientation); return ETrue;
        case 13: iFpsCap20 = !iFpsCap20; return ETrue;
            }
        return EFalse;
    case EMenuControls:
        switch (iMenuSelection)
            {
        case 0:
            iLayout = iLayout == 0 ? 1 : 0;
            ResetBindings(iLayout);
            return ETrue;
        case 1:
            iSensitivity += aDirection;
            if (iSensitivity > 10) iSensitivity = 1;
            if (iSensitivity < 1) iSensitivity = 10;
            return ETrue;
        case 2:
            iSensitivityVertical += aDirection;
            if (iSensitivityVertical > 10) iSensitivityVertical = 1;
            if (iSensitivityVertical < 1) iSensitivityVertical = 10;
            return ETrue;
        case 3: iInvertY = !iInvertY; return ETrue;
        case 4: iAutoAim = !iAutoAim; return ETrue;
            }
        return EFalse;
    case EMenuBinds:
        if (iMenuSelection < KMaxBinds && gUi.iBindings[iMenuSelection].iSecond != 0)
            {
            gUi.iBindings[iMenuSelection].iSecond = 0;
            iLayout = 2;
            return ETrue;
            }
        return iMenuSelection < KMaxBinds;
    case EMenuSound:
        switch (iMenuSelection)
            {
        case 0:
            iSoundOn = !iSoundOn;
            if (!iSoundOn) iAudio.StopAll();
            return ETrue;
        case 1:
            iVolume += aDirection * 10;
            if (iVolume > 100) iVolume = 10;
            if (iVolume < 10) iVolume = 100;
            return ETrue;
        case 2: iVoiceOn = !iVoiceOn; return ETrue;
        case 3:
            iVoiceVolume += aDirection * 10;
            if (iVoiceVolume > 100) iVoiceVolume = 10;
            if (iVoiceVolume < 10) iVoiceVolume = 100;
            return ETrue;
            }
        return EFalse;
    default:
        return EFalse;
        }
    }

void CGame::MenuKey(TInt aAction)
    {
    EnsureUi(*this);
    if (iState == EStateEnd)
        {
        if (aAction == EActionSelect || aAction == EActionFire ||
            aAction == EActionMenu || aAction == EActionEscape)
            {
            if (iConnection) ReleaseCapture(*this);
            else NextMapAfterEnd(*this);
            }
        return;
        }
    BuildMenuItems(*this);
    const TInt count = gMenuCount > 0 ? gMenuCount : 1;
    const TBool left = aAction == EActionTabLeft || aAction == EActionMenuLeft;
    const TBool right = aAction == EActionTabRight || aAction == EActionMenuRight;
    const TBool up = aAction == EActionMenuUp || aAction == EActionLookUp || aAction == EActionForward;
    const TBool down = aAction == EActionMenuDown || aAction == EActionLookDown || aAction == EActionBack;
    const TInt tab = OptionTab(iMenu);
    if (tab >= 0 && iBindCapture < 0)
        {
        if (aAction == EActionTabLeft || aAction == EActionTabRight)
            {
            SwitchTab(*this, aAction == EActionTabLeft ? -1 : 1);
            return;
            }
        if (gUi.iTabFocus)
            {
            if (left || right) { SwitchTab(*this, left ? -1 : 1); return; }
            if (down || aAction == EActionSelect || aAction == EActionFire)
                { gUi.iTabFocus = EFalse; iMenuSelection = 0; }
            if (down || aAction == EActionSelect || aAction == EActionFire || up) return;
            }
        else if (up && iMenuSelection == 0) { gUi.iTabFocus = ETrue; return; }
        }
    const TBool skipZero = iMenu == EMenuWorld && gUi.iRoomCount == 0;
    if (up)
        {
        iMenuSelection = (iMenuSelection + count - 1) % count;
        if (skipZero && iMenuSelection == 0) iMenuSelection = count - 1;
        return;
        }
    if (down)
        {
        iMenuSelection = (iMenuSelection + 1) % count;
        if (skipZero && iMenuSelection == 0) iMenuSelection = 1 % count;
        return;
        }
    if (aAction == EActionRadio)
        {
        if (iMenu == EMenuRadio1) { iMenu = EMenuRadio2; iMenuSelection = 0; return; }
        if (iMenu == EMenuRadio2) { iMenu = EMenuRadio3; iMenuSelection = 0; return; }
        if (iMenu == EMenuRadio3) { iMenu = EMenuNone; return; }
        }
    if (aAction == EActionEscape) aAction = EActionMenu;
    if (aAction == EActionMenu || (aAction == EActionScore && IsOverlayMenuId(iMenu)))
        {
        MenuBack(*this);
        return;
        }
    if ((left || right) && ChangeOption(left ? -1 : 1)) return;
    if (aAction != EActionSelect && aAction != EActionFire) return;
    if (iMenu != EMenuCreate && iMenu != EMenuBinds ||
        iMenu == EMenuCreate && iMenuSelection < 10)
        if (ChangeOption(1)) return;
    const TInt selection = iMenuSelection;
    switch (iMenu)
        {
    case EMenuFirstRun1:
        iLayout = selection;
        ResetBindings(iLayout);
        iMenu = EMenuFirstRun2;
        iMenuSelection = 0;
        break;
    case EMenuFirstRun2:
        ApplyQuality(*this, selection);
        iFirstRunDone = ETrue;
        CommitSettings(*this);
        iMenu = EMenuMain;
        iMenuSelection = 0;
        break;
    case EMenuMain:
        if (selection == 0) { ResetCreate(*this); iMenu = EMenuCreate; iMenuSelection = 10; }
        else if (selection == 1) { iMenu = EMenuMultiplayer; iMenuSelection = 0; }
        else if (selection == 2) OpenOptions(*this, EMenuMain);
        else { CommitSettings(*this); iState = EStateEnd; }
        break;
    case EMenuMultiplayer:
        if (selection == 2) ApplyNick(*this);
        else if (selection == 3) ApplyServer(*this);
        else if (selection == 4)
            {
            if (gUi.iServerLength > 0) ConnectHost(*this, EFalse, EFalse);
            else StartWorldSearch(*this, EFalse);
            }
        else if (selection == 5) StartHost(*this, EFalse);
        else if (selection == 6) StartHost(*this, ETrue);
        else if (selection == 7)
            {
            StartLanSearch(*this);
            iMenu = EMenuLan;
            iMenuSelection = 0;
            }
        else if (selection == 8)
            {
            ResetCreate(*this);
            iMode = gUi.iMpMode;
            gUi.iCreateMap = gUi.iMpMap;
            if (gUi.iMpMap == 7 || gUi.iMpMap == 8 || gUi.iMpMap == 9 || gUi.iMpMap == 10) iMode = 4;
            StartMatch(gUi.iMpMap, iMode == 2 ? 2 : 1 + (iRandom.Next() & 1));
            }
        else if (selection == 9) { iMenu = EMenuMain; iMenuSelection = 1; }
        break;
    case EMenuLan:
        if (gUi.iLanCount > 0 && selection < gUi.iLanCount)
            {
            const TLanEntry& entry = gUi.iLan[selection];
            CloseLanSearch(*this);
            gUi.iMpMode = entry.iMode;
            gUi.iMpMap = entry.iMap;
            gUi.iNetworkRequest = EFalse;
            gUi.iNetworkCreate = EFalse;
            gUi.iNetworkLan = ETrue;
            TBuf8<64> host;
            TInetAddr address = entry.iAddress;
            const TUint32 ip = address.Address();
            AppendNum8(host, (TInt)((ip >> 24) & 255));
            host.Append((TUint8)'.');
            AppendNum8(host, (TInt)((ip >> 16) & 255));
            host.Append((TUint8)'.');
            AppendNum8(host, (TInt)((ip >> 8) & 255));
            host.Append((TUint8)'.');
            AppendNum8(host, (TInt)(ip & 255));
            TInt error = KErrNone;
            if (!iConnection) TRAP(error, iConnection = CNetConnection::NewL(iSocketServer));
            if (iConnection) iConnection->Close();
            if (error == KErrNone && iConnection) TRAP(error, iConnection->ConnectL(host, entry.iPort));
            if (error != KErrNone) { SetStatus(*this, 44); break; }
            iMode = gUi.iMpMode;
            iPendingMap = gUi.iMpMap;
            iNetOut.Reset();
            iNetOut.U8(KNetProtocol).U8(iMode).U8(iPendingMap)
                   .Str(TPtrC(gUi.iNick, gUi.iNickLength)).U8(1);
            iConnection->Send(EClientJoin, iNetOut);
            iState = EStateLoad;
            iMenu = EMenuSearch;
            iMenuSelection = 0;
            }
        else { CloseLanSearch(*this); iMenu = EMenuMultiplayer; iMenuSelection = 7; }
        break;
    case EMenuWorld:
        if (gUi.iRoomCount > 0 && selection < gUi.iRoomCount)
            RawKey(EActionRadio);
        else if (selection == (gUi.iRoomCount > 0 ? gUi.iRoomCount : 1)) StartWorldSearch(*this, EFalse);
        else if (selection == (gUi.iRoomCount > 0 ? gUi.iRoomCount + 1 : 2)) StartWorldSearch(*this, ETrue);
        else { CloseWorldSearch(*this); iMenu = EMenuMultiplayer; iMenuSelection = 4; }
        break;
    case EMenuSearch:
        ReleaseCapture(*this);
        break;
    case EMenuCreate:
        if (selection == 11) { CommitSettings(*this); iMenu = EMenuMain; iMenuSelection = 0; }
        else if (selection == 10) ApplyCreate(*this);
        break;
    case EMenuTeam:
        if (selection == 3) { iMenu = EMenuPause; iMenuSelection = 1; break; }
        SwitchTeam(*this, selection == 2 ? 1 + (iRandom.Next() & 1) : selection + 1);
        iMenu = EMenuNone;
        break;
    case EMenuLanguage:
        {
        if (selection == 0) iLanguage = KLanguageEnglish;
        else if (selection == 1) iLanguage = KLanguageRussian;
        else if (selection == 2) iLanguage = KLanguageFrench;
        else if (selection == 3) iLanguage = KLanguageGerman;
        else if (selection == 4) iLanguage = KLanguageJapanese;
        else if (selection == 5) iLanguage = KLanguageVietnamese;
        else { CloseOptions(*this); break; }
        CommitSettings(*this);
        break;
        }
    case EMenuGraphics: case EMenuControls: case EMenuSound:
        if (iMenu == EMenuSound && selection == 4) { StartMicTest(*this); break; }
        if (selection == gMenuCount - 1) CloseOptions(*this);
        break;
    case EMenuBinds:
        if (selection < KMaxBinds)
            {
            iBindCapture = selection;
            iBindUntil = (TReal32)User::NTickCount() + 6000.0f;
            }
        else if (selection == KMaxBinds)
            {
            if (iLayout == 2) iLayout = 0;
            ResetBindings(iLayout);
            CommitSettings(*this);
            }
        else CloseOptions(*this);
        break;
    case EMenuHelp:
        iMenu = iHelpReturn;
        iHelpReturn = EMenuMain;
        iMenuSelection = 0;
        break;
    case EMenuPause:
        if (selection == 0) iMenu = EMenuNone;
        else if (selection == 1)
            {
            if (iConnection)
                Center(_L("В мультиплеере команды распределяет сервер"), 2.0f, 0xFFFFFF);
            else
                {
                iMenu = EMenuTeam;
                iMenuSelection = iMyIndex >= 0 && iMyIndex < KMaxPlayers &&
                    iPlayers[iMyIndex].iTeam == 2 ? 1 : 0;
                }
            }
        else if (selection == 2) OpenOptions(*this, EMenuPause);
        else if (selection == 3)
            {
            if (iConnection) ReleaseCapture(*this);
            else { iState = EStateMenu; iMenu = EMenuMain; iMenuSelection = 0; iAudio.StopAll(); }
            }
        else { CommitSettings(*this); iState = EStateEnd; }
        break;
    case EMenuGame:
        iMenu = EMenuNone;
        if (iMyIndex < 0 || iMyIndex >= KMaxPlayers || !iPlayers[iMyIndex].iAlive) break;
        if (selection <= 2) { iMenu = EMenuRadio1 + selection; iMenuSelection = 0; }
        else if (selection == 3)
            {
            DropCurrent(iPlayers[iMyIndex]);
            SendNetAction(*this, 'D', -1, -1);
            }
        else if (selection == 4)
            {
            if (iPlayers[iMyIndex].iWeapon == 17 || iPlayers[iMyIndex].iWeapon == 2) AlternateFire(iPlayers[iMyIndex]);
            else Center(_L("Глушитель есть только у M4A1 и USP"), 1.5f, 0xFFFFFF);
            }
        else if (selection == 5)
            {
            if (iTime < iBuyEnd && !iBombPlanted) { iBuyOpen = ETrue; iBuyCategory = -1; gUi.iBuySelection = 0; }
            else Center(_L("Здесь нельзя покупать"), 1.5f, 0xFFFFFF);
            }
        break;
    case EMenuRadio1: case EMenuRadio2: case EMenuRadio3:
        {
        const TInt base = iMenu == EMenuRadio1 ? 0 : iMenu == EMenuRadio2 ? 6 : 12;
        iMenu = EMenuNone;
        if (iMyIndex >= 0 && iMyIndex < KMaxPlayers) Radio(iPlayers[iMyIndex], base + selection);
        break;
        }
    default:
        break;
        }
    }

namespace
    {

const TUint32 KHudColor = 0xFFA000u;
const TUint32 KVguiColor = 0xFFB000u;
const TUint32 KVguiDisabledColor = 0x7A5200u;
const TUint32 KWhite = 0xFFFFFFu;
const TUint32 KBlack = 0x000000u;

const TInt KMaxUiModelBones = 64;
static TInt gPlayerMergeMap[2][KWeaponCount][KMaxUiModelBones];
static TBool gPlayerMergeChecked[2][KWeaponCount];
static TBool gPlayerMergeReady[2][KWeaponCount];
static TBool gPlayerSequencesReady = EFalse;

TInt MinF(TInt aLeft, TInt aRight)
    { return aLeft < aRight ? aLeft : aRight; }

TInt AbsI(TInt aValue)
    { return aValue < 0 ? -aValue : aValue; }

void CopyAscii(char* aDestination, TInt aMaximum, const char* aSource)
    {
    TInt length = AsciiLength(aSource);
    if (length >= aMaximum) length = aMaximum - 1;
    for (TInt i = 0; i < length; i++) aDestination[i] = aSource[i];
    aDestination[length] = 0;
    }

TBool SameAscii(const char* aFirst, const char* aSecond)
    {
    if (!aFirst || !aSecond) return EFalse;
    while (*aFirst && *aSecond)
        {
        if (*aFirst != *aSecond) return EFalse;
        aFirst++;
        aSecond++;
        }
    return *aFirst == 0 && *aSecond == 0;
    }

TBool StartsWithAscii(const char* aText, const char* aPrefix)
    {
    if (!aText || !aPrefix) return EFalse;
    while (*aPrefix)
        {
        if (*aText != *aPrefix) return EFalse;
        aText++;
        aPrefix++;
        }
    return ETrue;
    }

TInt PlayerIndex(const CGame& aGame, const TPlayer& aPlayer)
    {
    for (TInt i = 0; i < KMaxPlayers; i++)
        if (&aGame.iPlayers[i] == &aPlayer) return i;
    return -1;
    }

TPtrC PlayerName(const TPlayer& aPlayer)
    {
    return TPtrC(aPlayer.iName, TextLength(aPlayer.iName, 24));
    }

TInt HudSprite(CAssetManager& aAssets, TInt& aCache, const char* aName)
    { return HudId(aAssets, aCache, aName); }

void DrawHudSpritePass(CRenderer& aRenderer, const CSpriteSet::TSprite* aSprite,
                       TInt aX, TInt aY, TUint32 aColor, TInt aAlpha)
    {
    if (!aSprite || !aSprite->iPixels || aAlpha <= 0) return;
    const TInt red = (TInt)((aColor >> 16) & 255);
    const TInt green = (TInt)((aColor >> 8) & 255);
    const TInt blue = (TInt)(aColor & 255);
    for (TInt row = 0; row < aSprite->iHeight; row++)
        {
        const TInt y = aY + row;
        if (y < 0 || y >= aRenderer.iHeight) continue;
        TInt pixel = y * aRenderer.iWidth + aX;
        const TInt offset = row * aSprite->iWidth;
        for (TInt column = 0; column < aSprite->iWidth; column++, pixel++)
            {
            const TInt x = aX + column;
            if (x < 0 || x >= aRenderer.iWidth) continue;
            TInt alpha = ((TInt)aSprite->iPixels[offset + column] * 3 / 2);
            if (alpha > 255) alpha = 255;
            alpha = alpha * MinI(aAlpha, 256) >> 8;
            if (alpha <= 0) continue;
            const TUint16 background = aRenderer.iFrameBuffer[pixel];
            const TInt backgroundRed = ((background >> 11) & 31) * 255 / 31;
            const TInt backgroundGreen = ((background >> 5) & 63) * 255 / 63;
            const TInt backgroundBlue = (background & 31) * 255 / 31;
            const TInt newRed = backgroundRed + ((red - backgroundRed) * alpha >> 8);
            const TInt newGreen = backgroundGreen + ((green - backgroundGreen) * alpha >> 8);
            const TInt newBlue = backgroundBlue + ((blue - backgroundBlue) * alpha >> 8);
            aRenderer.iFrameBuffer[pixel] = (TUint16)PackRgb565(newRed, newGreen, newBlue);
            }
        }
    }

void DrawHudSprite(CRenderer& aRenderer, CAssetManager& aAssets, TInt aId,
                   TInt aX, TInt aY, TUint32 aColor, TInt aAlpha)
    {
    if (aId < 0 || aId >= aAssets.iHud.iCount || !aAssets.iHud.iSprites) return;
    const CSpriteSet::TSprite& sprite = aAssets.iHud.iSprites[aId];
    DrawHudSpritePass(aRenderer, &sprite, aX + 1, aY + 1, KBlack, aAlpha * 3 / 4);
    DrawHudSpritePass(aRenderer, &sprite, aX, aY, aColor, aAlpha);
    }

TInt HudSpriteWidth(CAssetManager& aAssets, TInt aId)
    {
    if (aId < 0 || aId >= aAssets.iHud.iCount || !aAssets.iHud.iSprites) return 0;
    return aAssets.iHud.iSprites[aId].iWidth;
    }

TInt NumberDigits(TInt aValue, TInt aMinimum, TText* aBuffer, TInt aMaximum)
    {
    if (aValue < 0) aValue = 0;
    TText reversed[16];
    TInt count = 0;
    do
        {
        reversed[count++] = (TText)('0' + aValue % 10);
        aValue /= 10;
        }
    while (aValue > 0 && count < 16);
    while (count < aMinimum && count < 16) reversed[count++] = (TText)'0';
    TInt result = 0;
    for (TInt i = count - 1; i >= 0 && result < aMaximum - 1; i--)
        aBuffer[result++] = reversed[i];
    aBuffer[result] = 0;
    return result;
    }

TInt HudNumberWidth(CAssetManager& aAssets)
    {
    const TInt digit = HudSprite(aAssets, gHudNumberId[0], KNumberNames[0]);
    return HudSpriteWidth(aAssets, digit) + 2;
    }

TInt DrawHudNumber(CRenderer& aRenderer, CAssetManager& aAssets, TInt aValue,
                   TInt aX, TInt aY, TUint32 aColor, TInt aAlpha, TInt aMinimum)
    {
    TText digits[16];
    const TInt count = NumberDigits(aValue, aMinimum, digits, 16);
    const TInt step = HudNumberWidth(aAssets);
    for (TInt i = 0; i < count; i++)
        {
        const TInt index = digits[i] - '0';
        if (index < 0 || index > 9) continue;
        const TInt id = HudSprite(aAssets, gHudNumberId[index], KNumberNames[index]);
        DrawHudSprite(aRenderer, aAssets, id, aX + i * step, aY, aColor, aAlpha);
        }
    return count * step;
    }

void TintScreen(CRenderer& aRenderer, TUint32 aColor, TInt aAlpha)
    {
    aRenderer.FillRectAlpha(0, 0, aRenderer.iWidth, aRenderer.iHeight,
                            Rgb565(aColor), Clamp(aAlpha, 0, 256));
    }

void DrawBlob(CRenderer& aRenderer, TReal32 aX, TReal32 aY, TReal32 aZ,
              TReal32 aRadius, TUint32 aColor, TInt aAlpha)
    {
    if (!aRenderer.Project(aX, aY, aZ) || !aRenderer.iFrameBuffer) return;
    const TBool smoke = aColor == 0xB4B4B4u;
    if (aRenderer.iProjectedZ < (smoke ? 4.0f : 24.0f)) return;
    TReal32 radius = aRadius * aRenderer.iXScale / aRenderer.iProjectedZ;
    TReal32 cap = aColor == 0xFFA030u ? (TReal32)aRenderer.iWidth / 4.0f
                                      : (TReal32)aRenderer.iWidth / 10.0f;
    if (!smoke && radius > cap) radius = cap;
    const TInt zIndex = CRenderer::ZOf(aRenderer.iProjectedZ);
    TInt x0 = (TInt)(aRenderer.iProjectedX - radius);
    TInt x1 = (TInt)(aRenderer.iProjectedX + radius);
    TInt y0 = (TInt)(aRenderer.iProjectedY - radius);
    TInt y1 = (TInt)(aRenderer.iProjectedY + radius);
    if (x1 < 0 || y1 < 0 || x0 >= aRenderer.iWidth || y0 >= aRenderer.iHeight) return;
    x0 = MaxI(0, x0);
    y0 = MaxI(0, y0);
    x1 = MinI(aRenderer.iWidth - 1, x1);
    y1 = MinI(aRenderer.iHeight - 1, y1);
    const TReal32 radiusSquared = radius * radius;
    if (radiusSquared <= 0) return;
    const TInt red = (TInt)((aColor >> 16) & 255);
    const TInt green = (TInt)((aColor >> 8) & 255);
    const TInt blue = (TInt)(aColor & 255);
    for (TInt y = y0; y <= y1; y++)
        {
        const TReal32 dy = y + 0.5f - aRenderer.iProjectedY;
        for (TInt x = x0; x <= x1; x++)
            {
            const TReal32 dx = x + 0.5f - aRenderer.iProjectedX;
            const TReal32 distance = dx * dx + dy * dy;
            if (distance > radiusSquared) continue;
            const TInt pixel = y * aRenderer.iWidth + x;
            if (aRenderer.iZBuffer && aRenderer.iZBuffer[pixel] > zIndex) continue;
            if (aAlpha >= 256)
                {
                aRenderer.iFrameBuffer[pixel] = Rgb565(aColor);
                continue;
                }
            const TInt localAlpha = Clamp((TInt)(aAlpha * (1.0f - distance / radiusSquared)), 0, 256);
            const TUint16 background = aRenderer.iFrameBuffer[pixel];
            const TInt backgroundRed = ((background >> 11) & 31) * 255 / 31;
            const TInt backgroundGreen = ((background >> 5) & 63) * 255 / 63;
            const TInt backgroundBlue = (background & 31) * 255 / 31;
            const TInt newRed = backgroundRed + ((red - backgroundRed) * localAlpha >> 8);
            const TInt newGreen = backgroundGreen + ((green - backgroundGreen) * localAlpha >> 8);
            const TInt newBlue = backgroundBlue + ((blue - backgroundBlue) * localAlpha >> 8);
            aRenderer.iFrameBuffer[pixel] = (TUint16)PackRgb565(newRed, newGreen, newBlue);
            }
        }
    }

void EnsurePlayerSequences(CGame& aGame)
    {
    if (gPlayerSequencesReady) return;
    gPlayerSequencesReady = ETrue;
    for (TInt team = 0; team < 2; team++)
        {
        const char* name = team == 0 ? "pl_t" : "pl_ct";
        TInt error = KErrNone;
        if (!gPlayerModel[team]) TRAP(error, gPlayerModel[team] = aGame.iAssets.Model(name));
        error = KErrNone;
        if (!gPlayerAnim[team]) TRAP(error, gPlayerAnim[team] = aGame.iAssets.Animation(name));
        CAnimation* animation = gPlayerAnim[team];
        if (!animation) continue;
        for (TInt i = 0; i < 6; i++)
            gGaitSequence[team][i] = animation->Find(KGaitNames[i]);
        for (TInt i = 0; i < 7; i++)
            gDeathSequence[team][i] = animation->Find(KDeathNames[i]);
        for (TInt weapon = 0; weapon < KWeaponCount; weapon++)
            {
            const TInt pose = weapon < 29 ? weapon : 0;
            for (TInt crouched = 0; crouched < 2; crouched++)
                {
                char aim[40];
                char shoot[40];
                CopyAscii(aim, 40, crouched ? "crouch_aim_" : "ref_aim_");
                CopyAscii(shoot, 40, crouched ? "crouch_shoot_" : "ref_shoot_");
                AppendAscii(aim, 40, KAnimationClasses[pose]);
                AppendAscii(shoot, 40, KAnimationClasses[pose]);
                TInt sequence = animation->Find(aim);
                if (sequence < 0) sequence = animation->Find(crouched ? "crouch_aim_ak47" : "ref_aim_ak47");
                gAimSequence[team][weapon][crouched] = sequence;
                sequence = animation->Find(shoot);
                if (sequence < 0) sequence = animation->Find(crouched ? "crouch_shoot_ak47" : "ref_shoot_ak47");
                gShootSequence[team][weapon][crouched] = sequence;
                }
            }
        }
    }

TBool UsePlayer3D(CGame& aGame)
    {
    EnsurePlayerSequences(aGame);
    return aGame.iGfx > 0 && (gPlayerModel[0] != NULL || gPlayerModel[1] != NULL);
    }

void WeaponItemName(char* aDestination, TInt aWeapon)
    {
    aDestination[0] = 'w';
    aDestination[1] = '_';
    const TInt weapon = Clamp(aWeapon, 0, KWeaponCount - 1);
    CopyAscii(aDestination + 2, 62, KWeaponNames[weapon]);
    }

void DrawStaticModel(CGame& aGame, const char* aName, TReal32 aX, TReal32 aY,
                     TReal32 aZ, TReal32 aYaw)
    {
    CModel* model = aGame.iAssets.Model(aName);
    if (!model || !aGame.iAssets.iMap) return;
    CModel::EntityMatrix(aX, aY, aZ, aYaw, gEntityMatrix);
    model->BuildDefault(gEntityMatrix, gModelMatrix);
    const TInt level = Clamp(aGame.iAssets.iMap->LightAt(aX, aY, aZ + 8.0f), 0, 63);
    for (TInt part = 0; part < model->iPartCount; part++)
        {
        if (!model->iParts || !model->iParts[part] || !model->iParts[part][0]) continue;
        aGame.iRenderer.DrawMesh(*model->iParts[part][0], gModelMatrix, level);
        }
    }

void DrawPlayer3D(CGame& aGame, const TPlayer& aPlayer, const TPlayerViewExtra& aExtra)
    {
    CModel* model = gPlayerModel[aPlayer.iTeam == 1 ? 0 : 1];
    CAnimation* animation = gPlayerAnim[aPlayer.iTeam == 1 ? 0 : 1];
    if (!model || !animation || !aGame.iAssets.iMap) return;
    const TInt team = aPlayer.iTeam == 1 ? 0 : 1;
    if (!aPlayer.iAlive)
        {
        TInt sequence = aExtra.iDeathAnim < 0 || aExtra.iDeathAnim > 6 ? 0 : aExtra.iDeathAnim;
        if (gDeathSequence[team][sequence] < 0) sequence = 0;
        animation->Pose(gDeathSequence[team][sequence],
                        (aGame.iTime - aPlayer.iDeathTime) * animation->iFps[MaxI(0, gDeathSequence[team][sequence])],
                        EFalse, 0.0f, gPoseRotation, gPosePosition);
        }
    else
        {
        TInt gait = 0;
        if (!aPlayer.iOnGround) gait = 5;
        else if (aPlayer.iDucked) gait = aPlayer.iSpeedFactor > 20.0f ? 4 : 3;
        else gait = aPlayer.iSpeedFactor > 150.0f ? 2 : (aPlayer.iSpeedFactor > 20.0f ? 1 : 0);
        TInt sequence = MaxI(0, gGaitSequence[team][gait]);
        TReal32 frame = 0;
        if (gait == 2) frame = aPlayer.iAnimTime * 1.4f * animation->iFrames[sequence];
        else if (gait == 1) frame = aPlayer.iAnimTime * 1.8f * animation->iFrames[sequence];
        else if (gait == 4) frame = aPlayer.iAnimTime * 2.5f * animation->iFrames[sequence];
        else if (gait == 5) frame = animation->iFrames[sequence] * 0.45f;
        else frame = aGame.iTime * animation->iFps[sequence];
        animation->Pose(sequence, frame, gait != 5, 0.0f, gPoseRotation, gPosePosition);
        const TInt weapon = Clamp(aPlayer.iWeapon, 0, KWeaponCount - 1);
        const TInt crouched = aPlayer.iDucked ? 1 : 0;
        TInt upper = MaxI(0, gAimSequence[team][weapon][crouched]);
        TReal32 upperFrame = 0;
        const TInt shooting = gShootSequence[team][weapon][crouched];
        if (aExtra.iPlanting && shooting >= 0)
            {
            upper = shooting;
            upperFrame = MinF(1.0f, (TInt)(aExtra.iDefuseProgress * 100.0f / 300.0f)) * (animation->iFrames[shooting] - 1);
            }
        else if (shooting >= 0 && aGame.iTime - aPlayer.iLastShot < animation->Duration(shooting))
            {
            upper = shooting;
            upperFrame = (aGame.iTime - aPlayer.iLastShot) * animation->iFps[shooting];
            }
        const TReal32 blend = animation->iBlends[upper] > 1 ? (aPlayer.iPitch + 45.0f) / 45.0f : 0.0f;
        animation->Pose(upper, upperFrame, EFalse, blend, gPoseRotation, gPosePosition);
        }
    CModel::EntityMatrix(aPlayer.iX, aPlayer.iY, aPlayer.iZ, aPlayer.iYaw, gEntityMatrix);
    model->Build(gPoseRotation, gPosePosition, gEntityMatrix, gModelMatrix);
    const TInt level = Clamp(aGame.iAssets.iMap->LightAt(aPlayer.iX, aPlayer.iY, aPlayer.iZ - 20.0f), 0, 63);
    if (model->iParts && model->iParts[0] && model->iParts[0][0])
        aGame.iRenderer.DrawMesh(*model->iParts[0][0], gModelMatrix, level);
    if (aExtra.iC4 && aPlayer.iAlive && model->iParts && model->iParts[1] && model->iParts[1][1])
        aGame.iRenderer.DrawMesh(*model->iParts[1][1], gModelMatrix, level);
    if (!aPlayer.iAlive) return;
    char itemName[64];
    WeaponItemName(itemName, aPlayer.iWeapon);
    CModel* weapon = aGame.iAssets.Model(itemName);
    if (!weapon || !weapon->iParts || !weapon->iParts[0] || !weapon->iParts[0][0]) return;
    const TInt weaponIndex = Clamp(aPlayer.iWeapon, 0, KWeaponCount - 1);
    if (!gPlayerMergeChecked[team][weaponIndex])
        {
        gPlayerMergeChecked[team][weaponIndex] = ETrue;
        if (weapon->iBoneCount > 0 && weapon->iBoneCount <= KMaxUiModelBones && weapon->iBoneName)
            {
            for (TInt bone = 0; bone < weapon->iBoneCount; bone++)
                gPlayerMergeMap[team][weaponIndex][bone] = model->Bone(weapon->iBoneName[bone].Des());
            gPlayerMergeReady[team][weaponIndex] = ETrue;
            }
        }
    if (!gPlayerMergeReady[team][weaponIndex]) return;
    weapon->BuildMerged(gPlayerMergeMap[team][weaponIndex], gModelMatrix, gEntityMatrix, gMergedMatrix);
    aGame.iRenderer.DrawMesh(*weapon->iParts[0][0], gMergedMatrix, level);
    }

void DrawPlayerSprites(CGame& aGame, const TPlayer& aPlayer, const TPlayerViewExtra& aExtra)
    {
    CSpriteSet* sprites = aPlayer.iTeam == 1 ? &aGame.iAssets.iSpritesT : &aGame.iAssets.iSpritesCT;
    if (!sprites || sprites->iAnimCount <= 0) return;
    TInt animation = -1;
    TInt frame = 0;
    if (!aPlayer.iAlive)
        {
        animation = sprites->Find(aExtra.iDeathAnim == 1 ? "die2" : "die");
        const TInt count = animation >= 0 ? sprites->iNfr[animation] : 0;
        frame = count > 0 ? (TInt)((aGame.iTime - aPlayer.iDeathTime) / 0.14f) : 0;
        if (frame >= count) frame = count - 1;
        }
    else
        {
        const TReal32 speed = aPlayer.iSpeedFactor;
        if (aPlayer.iDucked)
            {
            animation = sprites->Find(speed > 20.0f ? "crouchrun" : "crouch");
            if (speed > 20.0f && animation >= 0 && sprites->iNfr[animation] > 0)
                frame = (TInt)(aPlayer.iAnimTime * 3.0f) % sprites->iNfr[animation];
            }
        else if (speed > 20.0f)
            {
            animation = sprites->Find("run");
            if (animation >= 0 && sprites->iNfr[animation] > 0)
                frame = (TInt)(aPlayer.iAnimTime * 6.5f) % sprites->iNfr[animation];
            }
        else animation = sprites->Find("stand");
        }
    if (animation < 0 || animation >= sprites->iAnimCount) return;
    const TReal32 toCamera = FloatAtan2(aGame.iRenderer.iEyeY - aPlayer.iY,
                                        aGame.iRenderer.iEyeX - aPlayer.iX);
    const TReal32 relative = NormalizeAngle(toCamera - aPlayer.iYaw);
    TInt direction = (TInt)((relative + 22.5f) / 45.0f) % sprites->iDirs;
    while (direction < 0) direction += sprites->iDirs;
    const CSpriteSet::TSprite* sprite = sprites->Frame(animation, direction, frame);
    if (!sprite) return;
    const TInt level = aGame.iAssets.iMap
        ? Clamp(aGame.iAssets.iMap->LightAt(aPlayer.iX, aPlayer.iY, aPlayer.iZ - 20.0f) + 4, 0, 63) : 32;
    aGame.iRenderer.DrawSprite(sprite->iPixels, sprite->iWidth, sprite->iHeight,
                               sprites->Lit(level), aPlayer.iX, aPlayer.iY, aPlayer.iZ,
                               sprites->iAx - sprite->iOffsetX,
                               sprites->iAy - sprite->iOffsetY, sprites->iPpu);
    }

void UpdateMuzzlePlayers(CGame& aGame, const TPlayer& aPlayer, const TPlayerViewExtra& aExtra, TBool aThreeD)
    {
    if (!aPlayer.iAlive || IsGun(aPlayer.iWeapon) == EFalse) return;
    if (aGame.iTime - aPlayer.iLastShot < 0.0f || aGame.iTime - aPlayer.iLastShot >= 0.06f) return;
    if (aExtra.iLastShotSilent) return;
    const TReal32 yaw = FloatCos(aPlayer.iYaw) * 30.0f;
    const TReal32 pitch = FloatSin(aPlayer.iYaw) * 30.0f;
    const TReal32 height = aPlayer.iDucked ? 2.0f : 14.0f;
    if (aThreeD) DrawBlob(aGame.iRenderer, aPlayer.iX + yaw, aPlayer.iY + pitch,
                          aPlayer.iZ + height, 4.0f, 0xFFD070u, 200);
    else DrawBlob(aGame.iRenderer, aPlayer.iX + yaw, aPlayer.iY + pitch,
                  aPlayer.iZ + height, 4.0f, 0xFFD070u, 200);
    }

void ViewSetName(TInt aSet, char* aDestination)
    {
    if (aSet == KWeaponCount) CopyAscii(aDestination, 32, "m4a1s");
    else if (aSet == KWeaponCount + 1) CopyAscii(aDestination, 32, "usps");
    else CopyAscii(aDestination, 32, KWeaponNames[Clamp(aSet, 0, KWeaponCount - 1)]);
    }

void UpdateViewModelState(CGame& aGame, const TPlayer& aViewer, TInt aIndex)
    {
    if (aIndex < 0 || aIndex >= KMaxPlayers) return;
    aGame.CopyViewExtras(gExtra);
    TPlayerViewExtra& extra = gExtra[aIndex];
    extra.iVmWeapon = aViewer.iWeapon;
    if (extra.iVmSet < 0) extra.iVmSet = aViewer.iWeapon;
    if (extra.iVmName[0] == 0) CopyAsciiToText(extra.iVmName, 32, "idle");
    if (extra.iVmLength <= 0.0f)
        {
        extra.iVmTime = aGame.iTime;
        extra.iVmLength = 1.0f;
        }
    }

TBool EnsureViewModel3D(CGame& aGame, TInt aSet, const char* aSetName)
    {
    if (gViewModel && gViewAnimation && SameAsciiText(gViewModelName, aSetName)) return ETrue;
    if (aSet != gViewSpriteWeapon)
        {
        gViewSpriteWeapon = aSet;
        gViewSpritesChecked = EFalse;
        }
    char modelName[40];
    CopyAscii(modelName, 40, "");
    if (aSet == KWeaponCount) CopyAscii(modelName, 40, "m4a1");
    else if (aSet == KWeaponCount + 1) CopyAscii(modelName, 40, "usp");
    else CopyAscii(modelName, 40, aSetName);
    CModel* model = NULL;
    CAnimation* animation = NULL;
    TInt error = KErrNone;
    TRAP(error, model = aGame.iAssets.WeaponModel(modelName));
    if (error != KErrNone) model = NULL;
    error = KErrNone;
    TRAP(error, animation = aGame.iAssets.WeaponAnimation(aSetName));
    if (error != KErrNone) animation = NULL;
    if (!model || !animation) return EFalse;
    gViewModel = model;
    gViewAnimation = animation;
    CopyAsciiToText(gViewModelName, 64, aSetName);
    return ETrue;
    }

void EnsureViewSprites(CGame& aGame, TInt aSet, const char* aSetName)
    {
    if (gViewSpritesChecked && gViewSpriteWeapon == aSet) return;
    gViewSpriteWeapon = aSet;
    gViewSpritesChecked = ETrue;
    if (!aGame.iAssets.iFileServer) return;
    TBuf<192> path;
    path.Copy(aGame.iAssets.iDirectory);
    AppendAscii(path, "v_");
    AppendAscii(path, aSetName);
    AppendAscii(path, ".vws");
    TInt error = KErrNone;
    TRAP(error, gViewSprites.LoadL(*aGame.iAssets.iFileServer, path));
    }

void DrawWrappedText(CRenderer& aRenderer, const CCSFont& aFont, TInt aX, TInt aY,
                     TInt aMaximumWidth, const TDesC& aText, TUint16 aColor, TInt aLineHeight)
    {
    TBuf<160> line;
    TInt y = aY;
    for (TInt i = 0; i < aText.Length(); i++)
        {
        const TText character = aText[i];
        if (character == '\n' || (character == ' ' && aRenderer.TextWidth(aFont, line) + aRenderer.TextWidth(aFont, TPtrC(&character, 1)) > aMaximumWidth))
            {
            DrawShadowText(aRenderer, aX, y, aFont, line, aColor);
            y += aLineHeight;
            line.Zero();
            if (character == ' ') continue;
            }
        if (line.Length() < line.MaxLength()) line.Append(character);
        }
    if (line.Length() > 0) DrawShadowText(aRenderer, aX, y, aFont, line, aColor);
    }

void BuildItemsForMenu(CGame& aGame, TInt aMenu)
    {
    const TInt previous = aGame.iMenu;
    aGame.iMenu = aMenu;
    BuildMenuItems(aGame);
    aGame.iMenu = previous;
    }

void DrawMainMenuItems(CGame& aGame, TInt aMenu, TBool aActive)
    {
    EnsureUi(aGame);
    BuildItemsForMenu(aGame, aMenu);
    CRenderer& renderer = aGame.iRenderer;
    const TInt count = MinI(gMenuCount, KMaxMenuItems);
    const TInt top = MaxI(118, renderer.iHeight - 16 - count * 15)
                     - (aGame.iState == EStatePlay && renderer.iHeight < renderer.iWidth ? 44 : 0);
    DrawCenteredShadowText(renderer, renderer.iWidth / 2, MaxI(12, top - 34),
                           aGame.iAssets.iFontBig, TPtrC((const TText*)L"COUNTER-STRIKE"), Rgb565(KWhite));
    for (TInt i = 0; i < count; i++)
        {
        const TBool selected = aActive && i == aGame.iMenuSelection;
        DrawShadowText(renderer, 26, top + i * 15, aGame.iAssets.iFontSmall,
                       TPtrC(gMenuItems[i].iText, gMenuItems[i].iLength),
                       Rgb565(selected ? KWhite : 0xD8D8D8u));
        }
    TBuf<48> version;
    AppendAscii(version, "Nokia E52 Edition");
    DrawShadowText(renderer, renderer.iWidth - 6 - renderer.TextWidth(aGame.iAssets.iFontSmall, version),
                   renderer.iHeight - 13, aGame.iAssets.iFontSmall, version, Rgb565(0x9A9A9Au));
    }

void DrawDialogItems(CGame& aGame)
    {
    EnsureUi(aGame);
    BuildMenuItems(aGame);
    CRenderer& renderer = aGame.iRenderer;
    const CCSFont& font = aGame.iAssets.iFontSmall;
    TBuf<80> title;
    aGame.MenuTitle(title);
    const TInt count = MinI(gMenuCount, KMaxMenuItems);
    TInt buttonCount = 0;
    while (buttonCount < count && IsButtonItem(gMenuItems[count - 1 - buttonCount], aGame.iLanguage))
        buttonCount++;
    const TInt rowCount = count - buttonCount;
    const TInt x = 4;
    const TInt y = 4;
    const TInt width = renderer.iWidth - 8;
    const TInt height = renderer.iHeight - 8;
    DrawUiFrame(renderer, x, y, width, height, font, title);
    const TInt tab = OptionTab(aGame.iMenu);
    if (tab >= 0)
        {
        TInt positions[5];
        TInt total = 0;
        for (TInt i = 0; i < 5; i++)
            {
            positions[i] = total;
            total += renderer.TextWidth(font, LocalText(KOptTabShort[i], aGame.iLanguage)) + 8;
            }
        TInt offset = total > width - 12 ? width - 12 - total : 0;
        for (TInt i = 0; i < 5; i++)
            {
            const TInt tabWidth = renderer.TextWidth(font, LocalText(KOptTabShort[i], aGame.iLanguage)) + 8;
            const TInt tabX = x + 6 + positions[i] + offset;
            if (tabX < x + 4 || tabX + tabWidth > x + width - 4) continue;
            renderer.FillRect(tabX, y + 17, tabWidth, i == tab ? 15 : 13,
                              Rgb565(i == tab ? 0x4C5844u : 0x3E4637u));
            renderer.DrawText(tabX + 3, y + (i == tab ? 19 : 20), font,
                              LocalText(KOptTabShort[i], aGame.iLanguage),
                              Rgb565(i == tab ? (gUi.iTabFocus ? KWhite : 0xC4B550u) : 0xD8D8D8u));
            }
        }
    const TInt rowTop = y + (tab >= 0 ? 38 : 20);
    const TInt rowHeight = aGame.iMenu == EMenuBinds ? 22 : 14;
    const TInt visible = MinI(rowCount, MaxI(1, (height - 52) / rowHeight));
    TInt first = 0;
    if (rowCount > visible)
        {
        first = aGame.iMenuSelection - visible / 2;
        first = Clamp(first, 0, rowCount - visible);
        }
    for (TInt row = 0; row < visible; row++)
        {
        const TInt index = first + row;
        const TInt rowY = rowTop + row * rowHeight;
        const TBool selected = aGame.iMenuSelection == index;
        if (selected) renderer.FillRect(x + 7, rowY - 1, width - 14, rowHeight - 1, Rgb565(0x8E8923u));
        TPtrC item(gMenuItems[index].iText, gMenuItems[index].iLength);
        TInt split = -1;
        for (TInt i = 0; i < item.Length(); i++)
            if (item[i] == '\t') { split = i; break; }
        if (split < 0)
            {
            TInt colon = -1;
            for (TInt i = 0; i + 1 < item.Length(); i++)
                if (item[i] == ':' && item[i + 1] == ' ') { colon = i; break; }
            if (colon < 0)
                renderer.DrawText(x + 10, rowY, font, item, Rgb565(selected ? KWhite : KVguiColor));
            else
                {
                renderer.DrawText(x + 10, rowY, font, item.Left(colon), Rgb565(selected ? KWhite : 0xD8D8D8u));
                const TInt slider = SliderValue(aGame, index);
                TPtrC value(item.Ptr() + colon + 2, item.Length() - colon - 2);
                if (value == LocalText(428, aGame.iLanguage) || value == LocalText(421, aGame.iLanguage))
                    DrawCheckbox(renderer, x + width - 32, rowY, value == LocalText(428, aGame.iLanguage));
                else if (slider >= 0)
                    DrawSlider(renderer, x + width - 110, rowY, 80, slider, selected);
                else DrawCombo(renderer, x + width - 110, rowY - 2, 100, font, value, selected);
                }
            }
        else
            {
            renderer.DrawText(x + 10, rowY, font, item.Left(split), Rgb565(selected ? KWhite : 0xD8D8D8u));
            renderer.DrawText(x + width / 2, rowY, font, TPtrC(item.Ptr() + split + 1, item.Length() - split - 1),
                              Rgb565(selected ? KWhite : 0xC4B550u));
            }
        }
    TInt buttonX = x + width - 8;
    const TInt buttonY = y + height - 19;
    for (TInt button = buttonCount - 1; button >= 0; button--)
        {
        const TInt index = rowCount + button;
        const TInt buttonWidth = MaxI(56, renderer.TextWidth(font, TPtrC(gMenuItems[index].iText, gMenuItems[index].iLength)) + 16);
        buttonX -= buttonWidth;
        DrawButton(renderer, buttonX, buttonY, buttonWidth, 14, font,
                   TPtrC(gMenuItems[index].iText, gMenuItems[index].iLength),
                   aGame.iMenuSelection == index);
        buttonX -= 6;
        }
    }

void DrawTextMenuItems(CGame& aGame)
    {
    EnsureUi(aGame);
    BuildMenuItems(aGame);
    CRenderer& renderer = aGame.iRenderer;
    const CCSFont& font = aGame.iAssets.iFontSmall;
    const TInt count = MinI(gMenuCount, KMaxMenuItems);
    TInt height = 18 + count * 13 + 18;
    TInt top = MaxI(94, renderer.iHeight / 2 - height / 2);
    if (top + height > renderer.iHeight - 30) top = MaxI(20, renderer.iHeight - 30 - height);
    DrawShadowText(renderer, 10, top, font, TPtrC((const TText*)L"Меню"), Rgb565(KWhite));
    top += 18;
    for (TInt i = 0; i < count; i++)
        {
        TBuf<KMenuTextLength> line;
        if (i < 9) line.AppendNum(i + 1), line.Append((TChar)'.'), line.Append((TChar)' ');
        line.Append(TPtrC(gMenuItems[i].iText, gMenuItems[i].iLength));
        DrawShadowText(renderer, 10, top + i * 13, font, line,
                       Rgb565(i == aGame.iMenuSelection ? 0xFFE040u : KWhite));
        }
    DrawShadowText(renderer, 10, top + count * 13 + 5, font, TPtrC((const TText*)L"0. Выход"), Rgb565(KWhite));
    }

void VguiPanel(CGame& aGame, TInt aX, TInt aY, TInt aWidth, TInt aHeight, const TDesC& aTitle)
    {
    CRenderer& renderer = aGame.iRenderer;
    renderer.FillRectAlpha(aX, aY, aWidth, aHeight, Rgb565(KBlack), 200);
    renderer.DrawText(aX + 10, aY + 6, aGame.iAssets.iFontBig, aTitle, Rgb565(KVguiColor));
    renderer.FillRect(aX + 10, aY + 26, aWidth - 20, 1, Rgb565(KVguiColor));
    }

void VguiButton(CGame& aGame, TInt aX, TInt aY, TInt aWidth, const TDesC& aLabel,
                TBool aSelected, TBool aEnabled)
    {
    CRenderer& renderer = aGame.iRenderer;
    const TUint32 color = aEnabled ? KVguiColor : KVguiDisabledColor;
    if (aSelected) renderer.FillRectAlpha(aX, aY, aWidth, 15, Rgb565(color), 100);
    DrawOutline(renderer, aX, aY, aWidth, 15, Rgb565(color));
    renderer.DrawText(aX + 6, aY + 2, aGame.iAssets.iFontSmall, aLabel,
                      Rgb565(aSelected && aEnabled ? KWhite : color));
    }

void DrawTeamMenuItems(CGame& aGame)
    {
    CRenderer& renderer = aGame.iRenderer;
    const CCSFont& font = aGame.iAssets.iFontSmall;
    const TInt x = 6;
    const TInt y = 14;
    const TInt width = renderer.iWidth - 12;
    const TInt height = renderer.iHeight - 30;
    VguiPanel(aGame, x, y, width, height, TPtrC((const TText*)L"Выберите команду"));
    static const TText* const labels[4] = {
        (const TText*)L"1  Террористы", (const TText*)L"2  Спецназ",
        (const TText*)L"3  Автовыбор", (const TText*)L"0  Отмена"
        };
    for (TInt i = 0; i < 4; i++)
        VguiButton(aGame, x + 10, y + 36 + i * 20 + (i == 3 ? 12 : 0), width - 20,
                   TPtrC(labels[i]), i == aGame.iMenuSelection, ETrue);
    const TText* description = (const TText*)L"";
    if (aGame.iMenuSelection == 0) description = (const TText*)L"Взорвать точку A или B";
    else if (aGame.iMenuSelection == 1) description = (const TText*)L"Защитить точки A и B";
    else if (aGame.iMenuSelection == 2) description = (const TText*)L"Случайная команда";
    DrawCenteredShadowText(renderer, renderer.iWidth / 2, y + height - 22, font,
                           TPtrC(description), Rgb565(KVguiColor));
    }

void DrawBuyMenuItems(CGame& aGame)
    {
    EnsureUi(aGame);
    BuildBuyItems(aGame);
    CRenderer& renderer = aGame.iRenderer;
    const CCSFont& font = aGame.iAssets.iFontSmall;
    const TInt count = MinI(gMenuCount, KMaxMenuItems);
    const TInt x = 4;
    const TInt y = 8;
    const TInt width = renderer.iWidth - 8;
    const TInt height = renderer.iHeight - 16;
    TBuf<48> title;
    if (aGame.iBuyCategory < 0) AppendLocal(title, 192, aGame.iLanguage);
    else if (aGame.iBuyCategory == 7) AppendLocal(title, 414, aGame.iLanguage);
    else AppendWeaponTitle(title, MinI(aGame.iBuyCategory, 4), aGame.iLanguage);
    VguiPanel(aGame, x, y, width, height, title);
    const TInt rowHeight = 15;
    const TInt visible = MinI(count, MaxI(1, (height - 60) / rowHeight));
    for (TInt i = 0; i < visible; i++)
        VguiButton(aGame, x + 10, y + 33 + i * rowHeight, width - 20,
                   TPtrC(gMenuItems[i].iText, gMenuItems[i].iLength),
                   i == gUi.iBuySelection, gMenuItems[i].iEnabled);
    TBuf<64> money;
    AppendAscii(money, "$");
    money.AppendNum(aGame.iPlayers[BoundedIndex(aGame.iMyIndex, aGame.iPlayerCount)].iMoney);
    renderer.DrawText(x + width - 10 - renderer.TextWidth(font, money),
                      y + height - 20, font, money, Rgb565(0x80FF80u));
    DrawCenteredShadowText(renderer, renderer.iWidth / 2, y + height - 36, font,
                           TPtrC((const TText*)L"0 — назад"), Rgb565(KVguiColor));
    }

void DrawScoreboardTable(CGame& aGame)
    {
    CRenderer& renderer = aGame.iRenderer;
    const CCSFont& font = aGame.iAssets.iFontSmall;
    const TInt x = 10;
    const TInt y = 12;
    const TInt width = renderer.iWidth - 20;
    const TInt height = renderer.iHeight - 30;
    renderer.FillRectAlpha(x, y, width, height, Rgb565(KBlack), 175);
    DrawOutline(renderer, x, y, width, height, Rgb565(KVguiDisabledColor));
    TBuf<80> header;
    AppendAscii(header, KMapNames[Clamp(aGame.iMapIndex, 0, 10)]);
    AppendAscii(header, " · ");
    AppendLocal(header, 305, aGame.iLanguage);
    renderer.DrawText(x + 6, y + 4, font, header, Rgb565(KVguiColor));
    TInt rowY = y + 32;
    for (TInt team = 1; team <= 2; team++)
        {
        const TUint32 teamColor = team == 1 ? 0xFF5050u : 0x99CCFFu;
        TInt total = 0;
        TInt alive = 0;
        for (TInt i = 0; i < aGame.iPlayerCount; i++)
            if (aGame.iPlayers[i].iTeam == team)
                {
                total++;
                if (aGame.iPlayers[i].iAlive) alive++;
                }
        TBuf<48> teamLine;
        AppendLocal(teamLine, team == 1 ? 190 : 301, aGame.iLanguage);
        teamLine.Append((TChar)' ');
        teamLine.AppendNum(alive);
        teamLine.Append((TChar)'/');
        teamLine.AppendNum(total);
        renderer.DrawText(x + 6, rowY, font, teamLine, Rgb565(teamColor));
        rowY += 15;
        for (TInt i = 0; i < aGame.iPlayerCount; i++)
            {
            const TPlayer& player = aGame.iPlayers[i];
            if (player.iTeam != team) continue;
            TBuf<64> line;
            line.Append(PlayerName(player));
            if (player.iHasC4 && aGame.iTeamChoice == 1) AppendAscii(line, " [C4]");
            line.Append((TChar)' ');
            line.AppendNum(player.iKills);
            line.Append((TChar)'/');
            line.AppendNum(player.iDeaths);
            renderer.DrawText(x + 12, rowY, font, line, Rgb565(player.iAlive ? teamColor : 0x808080u));
            rowY += 11;
            if (rowY > y + height - 12) break;
            }
        rowY += 6;
        }
    }

void DrawSearchDialog(CGame& aGame)
    {
    CRenderer& renderer = aGame.iRenderer;
    const CCSFont& font = aGame.iAssets.iFontSmall;
    const TInt width = MinI(280, renderer.iWidth - 8);
    const TInt height = MinI(132, renderer.iHeight - 8);
    const TInt x = (renderer.iWidth - width) / 2;
    const TInt y = (renderer.iHeight - height) / 2;
    TBuf<48> title;
    AppendLocal(title, 272, aGame.iLanguage);
    DrawUiFrame(renderer, x, y, width, height, font, title);
    TBuf<160> status;
    status.Copy(TPtrC(gUi.iStatus, TextLength(gUi.iStatus, KMenuTextLength)));
    if (status.Length() <= 0)
        status.Copy(TPtrC(gUi.iLoadMessage, TextLength(gUi.iLoadMessage, 128)));
    if (status.Length() <= 0) AppendLocal(status, 70, aGame.iLanguage);
    DrawWrappedText(renderer, font, x + 10, y + 24, width - 20, status,
                    Rgb565(0xD8D8D8u), 13);
    const TInt segments = (TInt)((User::NTickCount() / 120) % 18) + 1;
    renderer.FillRect(x + 10, y + height - 52, width - 20, 10, Rgb565(0x3E4637u));
    DrawBevel(renderer, x + 10, y + height - 52, width - 20, 10, EFalse);
    for (TInt i = 0; i < segments && 2 + i * 11 + 9 < width - 20; i++)
        renderer.FillRect(x + 12 + i * 11, y + height - 50, 9, 6, Rgb565(0x8E8923u));
    DrawButton(renderer, x + width - 78, y + height - 19, 70, 14, font,
               LocalText(269, aGame.iLanguage), ETrue);
    }

void DrawHelpLines(CGame& aGame)
    {
    CRenderer& renderer = aGame.iRenderer;
    const CCSFont& font = aGame.iAssets.iFontSmall;
    TInt top = 24;
    renderer.FillRectAlpha(6, top, renderer.iWidth - 12, renderer.iHeight - top - 12,
                           Rgb565(KBlack), 180);
    DrawOutline(renderer, 6, top, renderer.iWidth - 12, renderer.iHeight - top - 12, Rgb565(KHudColor));
    const TInt rowCount = aGame.iLayout == 1 ? 1 : 2;
    for (TInt group = 0; group < 3; group++)
        {
        for (TInt column = 0; column < rowCount; column++)
            {
            const TInt index = KHelpLines[group][column * 4];
            DrawWrappedText(renderer, font, 12 + column * (renderer.iWidth - 30) / rowCount,
                            top + 8 + group * 26, (renderer.iWidth - 40) / rowCount,
                            LocalText(index, aGame.iLanguage), Rgb565(0xE0E0E0u), 13);
            }
        }
    DrawCenteredShadowText(renderer, renderer.iWidth / 2, renderer.iHeight - 22, font,
                           TPtrC((const TText*)L"5 — назад"), Rgb565(KHudColor));
    }

    }

void SaveGeneratedViewModel(const CGame& aGame)
    {
    if (aGame.iNokiaFrameRate <= 0 || !aGame.iGeneratedFrame || !aGame.iRenderer.iFrameBuffer) return;
    const TInt pixels = MinI(aGame.iRenderer.iWidth * aGame.iRenderer.iHeight, KMaxFramePixels);
    if (pixels <= 0) return;
    for (TInt i = 0; i < pixels; i++)
        {
        if (aGame.iRenderer.iZBuffer && aGame.iRenderer.iZBuffer[i] != 0)
            {
            gNfrVmPixels[i] = aGame.iRenderer.iFrameBuffer[i];
            gNfrVmMask[i] = 1;
            }
        else gNfrVmMask[i] = 0;
        }
    }

void CGame::Draw()
    {
    EnsureUi(*this);
    if (iState == EStateLoad)
        {
        DrawLoading();
        return;
        }
    if (iState == EStateMenu || iState == EStateEnd)
        {
        DrawMenu();
        return;
        }
    if (iState != EStatePlay || !iAssets.iMap || !iRenderer.iFrameBuffer) return;
    CopyViewExtras(gExtra);
    TInt viewIndex = iMyIndex;
    if (viewIndex < 0 || viewIndex >= iPlayerCount || !iPlayers[viewIndex].iAlive)
        viewIndex = iSpectatorIndex;
    if (viewIndex < 0 || viewIndex >= iPlayerCount) return;
    TPlayer& view = iPlayers[viewIndex];
    UpdateViewModelState(*this, view, viewIndex);

    iRenderer.RenderWorld();
    const CMap& map = *iAssets.iMap;
    for (TInt i = 0; i < map.numSolidModels && i < KMaxEffects * 4; i++)
        {
        if (map.modelActive && map.modelActive[map.solidModels[i]])
            iRenderer.RenderBrushModel(map.solidModels[i], 0.0f, 0.0f, 0.0f, ETrue);
        }
    iRenderer.RenderWorldAlpha();
    DrawPlayers(view);
    DrawWorldEffects();
    const TReal32 bob = iViewBob
        ? FloatSin(iTime * 9.0f) * MinF(1.0f, view.iSpeedFactor / 250.0f) * 1.2f : 0.0f;
    iRenderer.ClearZ();
    if (!DrawViewModel3D(view, bob)) DrawViewModel(view, bob);
    SaveGeneratedViewModel(*this);
    }

void CGame::DrawHud()
    {
    EnsureUi(*this);
    CopyViewExtras(gExtra);
    CRenderer& renderer = iRenderer;
    if (iState != EStatePlay || !renderer.iFrameBuffer) return;
    TInt viewIndex = iMyIndex;
    TBool spectating = viewIndex < 0 || viewIndex >= iPlayerCount || !iPlayers[viewIndex].iAlive;
    if (spectating) viewIndex = iSpectatorIndex;
    if (viewIndex < 0 || viewIndex >= iPlayerCount) viewIndex = iMyIndex;
    TPlayer* view = viewIndex >= 0 && viewIndex < iPlayerCount ? &iPlayers[viewIndex] : NULL;
    if (view) UpdateViewModelState(*this, *view, viewIndex);
    const CCSFont& small = iAssets.iFontSmall;
    const CCSFont& big = iAssets.iFontBig;
    TInt height = renderer.iHeight;
    TInt width = renderer.iWidth;
    if (iHurtFlash > 0.0f && !spectating)
        renderer.FillRectAlpha(0, 0, width, height, Rgb565(0xC00000u),
                               (TInt)(iHurtFlash * 300.0f));
    if (view && gExtra[viewIndex].iFlashUntil > iTime)
        {
        const TReal32 left = gExtra[viewIndex].iFlashUntil - iTime;
        TintScreen(renderer, KWhite, left > 1.5f ? 256 : (TInt)(left / 1.5f * 256.0f));
        }

    // Terminator disguise HUD
    if (iMode == 3 && iMyIndex == iTerminatorIdx && (iTerminatorClass == 2 || iTerminatorClass == 3))
        {
        TBuf<64> dmsg;
        if (iIsDisguised) dmsg.Copy(_L("[*] Mask: ACTIVE"));
        else if (iDisguiseCooldown > 0.0f) { dmsg.Copy(_L("[*] Mask: CD ")); dmsg.AppendNum((TInt)(iDisguiseCooldown+0.5f)); }
        else dmsg.Copy(_L("[*] Mask: READY"));
        renderer.DrawText(6, height - 30, small, dmsg, Rgb565(0xFFFFFFu));
        }
    if (view) DrawCrosshair(*view);
    TInt y = height - 16;
    if (view && !spectating)
        {
        const TUint32 healthColor = view->iHealth <= 25.0f ? 0xFF2020u : KHudColor;
        const TInt cross = HudSprite(iAssets, gHudCrossId, "cross");
        DrawHudSprite(renderer, iAssets, cross, 4, y - 1, healthColor, 200);
        DrawHudNumber(renderer, iAssets, (TInt)view->iHealth, 18, y, healthColor, 200, 1);
        const char* suit = gExtra[viewIndex].iHelmet ? "suithelmet_full" : "suit_full";
        const TInt suitId = HudSprite(iAssets, gHudSuitId, suit);
        DrawHudSprite(renderer, iAssets, suitId, 62, y - 1, KHudColor, view->iArmor > 0.0f ? 200 : 90);
        DrawHudNumber(renderer, iAssets, (TInt)view->iArmor, 76, y, KHudColor, 200, 1);
        const TInt weapon = Clamp(view->iWeapon, 0, KWeaponCount - 1);
        if (IsGun(weapon))
            {
            const TInt ammoWidth = HudNumberWidth(iAssets) * 3;
            DrawHudNumber(renderer, iAssets, view->iAmmo[weapon], width - 4 - ammoWidth, y, KHudColor, 200, 1);
            const TInt divider = HudSprite(iAssets, gHudDividerId, "divider");
            DrawHudSprite(renderer, iAssets, divider, width - 10 - ammoWidth, y - 4, KHudColor, 200);
            const TInt clipWidth = HudNumberWidth(iAssets) * 3;
            DrawHudNumber(renderer, iAssets, view->iClip[weapon],
                          width - 16 - ammoWidth - clipWidth, y, KHudColor, 200, 1);
            }
        else if (IsGrenade(weapon))
            {
            TInt grenades = 0;
            if (weapon == 25) grenades = gExtra[viewIndex].iHe;
            else if (weapon == 26) grenades = gExtra[viewIndex].iFlash;
            else grenades = gExtra[viewIndex].iSmoke;
            DrawHudNumber(renderer, iAssets, grenades, width - 16, y, KHudColor, 200, 1);
            }
        const TInt moneyWidth = HudNumberWidth(iAssets) * 4;
        const TInt dollar = HudSprite(iAssets, gHudDollarId, "dollar");
        DrawHudSprite(renderer, iAssets, dollar, width - 14 - moneyWidth, y - 22, KHudColor, 200);
        DrawHudNumber(renderer, iAssets, view->iMoney, width - 4 - moneyWidth, y - 22, KHudColor, 200, 1);
        if (CanBuy(*this, *view))
            DrawHudSprite(renderer, iAssets, HudSprite(iAssets, gHudBuyZoneId, "buyzone"),
                          2, height / 2 - 20, 0x00C000u, 220);
        if (gExtra[viewIndex].iC4)
            DrawHudSprite(renderer, iAssets, HudSprite(iAssets, gHudC4Id, "c4"),
                          2, height / 2 + 2, 0x00C000u, 220);
        if (gExtra[viewIndex].iDefuser)
            DrawHudSprite(renderer, iAssets, HudSprite(iAssets, gHudDefuserId, "defuser"),
                          2, height / 2 + 2, 0x00C000u, 220);
        }
    TReal32 timeLeft = 0.0f;
    TBool bombMode = ETrue;
    if (iBombPlanted) timeLeft = -1.0f;
    else if (iTime < iFreezeEnd) timeLeft = iFreezeEnd - iTime;
    else
        {
        timeLeft = iFreezeEnd + (TReal32)iRoundSeconds - iTime;
        if (timeLeft < 0.0f) timeLeft = 0.0f;
        }
    if (bombMode && timeLeft < 0.0f)
        {
        const TUint32 color = ((TInt)(iTime * 4.0f) & 1) == 0 ? 0xFF2020u : 0x800000u;
        DrawHudSprite(renderer, iAssets, HudSprite(iAssets, gHudC4Id, "c4"),
                      width / 2 - 8, y - 4, color, 230);
        }
    else
        {
        const TInt seconds = (TInt)timeLeft;
        const TInt timerX = width / 2 - 26;
        const TInt timerY = width > height ? y : y - 22;
        DrawHudSprite(renderer, iAssets, HudSprite(iAssets, gHudStopwatchId, "stopwatch"),
                      timerX - 14, timerY - 1, KHudColor, 200);
        const TInt minutesWidth = DrawHudNumber(renderer, iAssets, seconds / 60, timerX, timerY, KHudColor, 200, 1);
        renderer.FillRect(timerX + minutesWidth + 1, timerY + 3, 2, 2, Rgb565(KHudColor));
        renderer.FillRect(timerX + minutesWidth + 1, timerY + 8, 2, 2, Rgb565(KHudColor));
        DrawHudNumber(renderer, iAssets, seconds % 60, timerX + minutesWidth + 5, timerY, KHudColor, 200, 2);
        }
    if (iMode == 2 && view && gExtra[viewIndex].iZombie) TintScreen(renderer, 0x30FF30u, 40);
    TBuf<64> score;
    if (iMode == 2)
        {
        AppendLocal(score, 300, iLanguage);
        score.AppendNum(iScoreT);
        AppendAscii(score, " : ");
        score.AppendNum(iScoreCT);
        }
    else
        {
        AppendAscii(score, "T ");
        score.AppendNum(iScoreT);
        AppendAscii(score, " : ");
        score.AppendNum(iScoreCT);
        AppendAscii(score, " CT");
        }
    DrawCenteredShadowText(renderer, width / 2, 2, small, score, Rgb565(0xE0E0E0u));
    if (iRadar && view) DrawRadar(*view);
    for (TInt i = 0; i < KMaxChatLines; i++)
        {
        if (iChatLines[i][0] == 0 || iTime - iKillFeedTime[i] > 5.0f) continue;
        TPtrC line(iChatLines[i], TextLength(iChatLines[i], 160));
        DrawShadowText(renderer, 4, height - 56 - i * 13, small, line, Rgb565((TUint32)iChatColors[i]));
        }
    TInt killY = width > height ? 2 : 14;
    for (TInt i = 0; i < KMaxChatLines; i++)
        {
        if (iKillFeed[i][1] < 0 || iTime - iKillFeedTime[i] > 6.0f) continue;
        const TInt victimId = iKillFeed[i][1];
        const TInt attackerId = iKillFeed[i][0];
        const TInt weapon = iKillFeed[i][2] & 255;
        const TBool headshot = (iKillFeed[i][2] & 256) != 0;
        TPlayer* victim = PlayerById(victimId);
        TPlayer* attacker = PlayerById(attackerId);
        if (!victim) continue;
        TPtrC victimName = PlayerName(*victim);
        TPtrC attackerName = attacker ? PlayerName(*attacker) : TPtrC((const TText*)L"");
        const TInt attackerWidth = renderer.TextWidth(small, attackerName);
        const TInt victimWidth = renderer.TextWidth(small, victimName);
        const TInt weaponId = HudSprite(iAssets, gHudHeadshotId,
                                        weapon >= 0 && weapon < KWeaponCount ? KWeaponKillNames[weapon] : "d_skull");
        const TInt weaponWidth = HudSpriteWidth(iAssets, weaponId);
        const TInt headWidth = headshot
            ? HudSpriteWidth(iAssets, HudSprite(iAssets, gHudHeadshotId, "d_headshot")) + 2 : 0;
        const TInt start = width - 4 - victimWidth - weaponWidth - headWidth - attackerWidth - 6;
        renderer.FillRectAlpha(start - 2, killY, width - start, 13, Rgb565(KBlack), 110);
        renderer.DrawText(start, killY, small, attackerName, Rgb565(0xFF8080u));
        TInt cursor = start + attackerWidth + 3;
        DrawHudSprite(renderer, iAssets, weaponId, cursor, killY + 2, 0xFF4040u, 255);
        cursor += weaponWidth + 1;
        if (headshot)
            {
            DrawHudSprite(renderer, iAssets, HudSprite(iAssets, gHudHeadshotId, "d_headshot"),
                          cursor, killY + 2, 0xFF4040u, 255);
            cursor += headWidth;
            }
        renderer.DrawText(cursor + 2, killY, small, victimName, Rgb565(0xFF8080u));
        killY += 14;
        }
    if (view && !spectating && (gExtra[viewIndex].iPlanting || gExtra[viewIndex].iDefusing))
        {
        TReal32 progress = gExtra[viewIndex].iPlanting
            ? gExtra[viewIndex].iDefuseProgress / 3.0f
            : gExtra[viewIndex].iDefuseProgress / (gExtra[viewIndex].iDefuser ? 5.0f : 10.0f);
        progress = Clamp(progress, 0.0f, 1.0f);
        renderer.FillRectAlpha(width / 2 - 60, height - 52, 120, 8, Rgb565(KBlack), 160);
        renderer.FillRect(width / 2 - 59, height - 51, (TInt)(118.0f * progress), 6, Rgb565(KHudColor));
        }
    if (spectating)
        {
        renderer.FillRectAlpha(0, height - 18, width, 18, Rgb565(KBlack), 150);
        TBuf<96> spectator;
        AppendLocal(spectator, 306, iLanguage);
        if (view)
            {
            spectator.Append((TChar)' ');
            spectator.Append(PlayerName(*view));
            }
        DrawCenteredShadowText(renderer, width / 2, height - 15, small, spectator, Rgb565(KWhite));
        if (view)
            {
            TBuf<32> health;
            AppendAscii(health, "HP ");
            health.AppendNum((TInt)view->iHealth);
            DrawShadowText(renderer, 4, height - 32, small, health, Rgb565(KHudColor));
            }
        }
    if (iCenterMessage[0] && iTime < iCenterMessageTime)
        DrawCenteredShadowText(renderer, width / 2, height / 3 - 10, big,
                               TPtrC(iCenterMessage, TextLength(iCenterMessage, 256)),
                               Rgb565((TUint32)iCenterMessageColor));
    if (gUi.iMicState == 1)
        DrawCenteredShadowText(renderer, width / 2, height / 3 + 6, small,
                               LocalText(234, iLanguage), Rgb565(KWhite));
    else if (gUi.iMicState == 2 && gUi.iMicAt != 0 && User::NTickCount() - gUi.iMicAt < 2500u)
        DrawCenteredShadowText(renderer, width / 2, height / 3 + 6, small,
                               TPtrC(gUi.iStatus, TextLength(gUi.iStatus, KMenuTextLength)),
                               Rgb565(0xFF8060u));
    if (iTime < iFreezeEnd && !iRoundOver && !spectating)
        DrawCenteredShadowText(renderer, width / 2, height / 3 + 18, small,
                               TPtrC((const TText*)L"Закупка: #"), Rgb565(0xD0D0D0u));
    if (iShowFps)
        {
        TBuf<64> fpsText;
        fpsText.AppendNum(iFps);
        AppendAscii(fpsText, " fps");
        if (iNokiaFrameRate > 0) AppendAscii(fpsText, " +NFR");
        DrawShadowText(renderer, 4, 80, small, fpsText, Rgb565(0x80FF80u));
        }
    if (view) DrawMuzzle(*view);
    if (iBuyOpen) DrawBuyMenuItems(*this);
    if (iScoreOpen) DrawScoreboardTable(*this);
    if (iMenu != EMenuNone) DrawMenu();
    }

void CGame::DrawWorldEffects()
    {
    if (iGfx <= 0) return;
    for (TInt i = 0; i < KMaxEffects; i++)
        {
        const TEffect& effect = iEffects[i];
        if (!effect.iActive || effect.iLife <= 0.0f) continue;
        const TReal32 age = iTime - effect.iStart;
        if (age < 0.0f || age > effect.iLife) continue;
        const TReal32 fade = 1.0f - age / effect.iLife;
        TReal32 radius = effect.iSize;
        TInt alpha = (TInt)(200.0f * fade) + 30;
        if ((TUint32)effect.iColor == 0x303030u)
            {
            radius = effect.iSize;
            alpha = 256;
            }
        else if ((TUint32)effect.iColor == 0xC8B898u)
            DrawBlob(iRenderer, effect.iX, effect.iY, effect.iZ + age * 30.0f,
                     radius, (TUint32)effect.iColor, alpha);
        else if ((TUint32)effect.iColor == 0xB4B4B4u)
            {
            const TReal32 growth = MinF(1.0f, age / 2.0f);
            const TReal32 smokeRadius = growth * 130.0f;
            const TReal32 remaining = effect.iLife - age;
            const TReal32 fade = remaining < 3.0f ? remaining / 3.0f : 1.0f;
            for (TInt blob = 0; blob < 5; blob++)
                {
                TReal32 offsetX = 0.0f;
                TReal32 offsetY = 0.0f;
                if (blob == 1) offsetX = 60.0f;
                else if (blob == 2) offsetX = -60.0f;
                else if (blob == 3) offsetY = 60.0f;
                else if (blob == 4) offsetY = -60.0f;
                DrawBlob(iRenderer,
                         effect.iX + offsetX * growth,
                         effect.iY + offsetY * growth,
                         effect.iZ - (blob > 0 ? 20.0f : 0.0f),
                         smokeRadius * (blob == 0 ? 1.0f : 0.8f),
                         0xB4B4B4u, (TInt)(250.0f * fade));
                }
            }
        else
            {
            if ((TUint32)effect.iColor == 0xFFA030u) radius *= 1.4f - fade * 0.4f;
            DrawBlob(iRenderer, effect.iX, effect.iY, effect.iZ, radius,
                     (TUint32)effect.iColor, alpha);
            }
        }
    }

void CGame::DrawPlayers(const TPlayer& aViewer)
    {
    const TBool threeD = UsePlayer3D(*this);
    CopyViewExtras(gExtra);
    for (TInt i = 0; i < iPlayerCount && i < KMaxPlayers; i++)
        {
        const TPlayer& player = iPlayers[i];
        if (&player == &aViewer) continue;
        if (!player.iAlive && iTime - player.iDeathTime > 15.0f) continue;
        const TReal32 deltaX = player.iX - iRenderer.iEyeX;
        const TReal32 deltaY = player.iY - iRenderer.iEyeY;
        if (threeD)
            {
            if (deltaX * deltaX + deltaY * deltaY < 20.0f * 20.0f) continue;
            DrawPlayer3D(*this, player, gExtra[i]);
            }
        else DrawPlayerSprites(*this, player, gExtra[i]);
        UpdateMuzzlePlayers(*this, player, gExtra[i], threeD);
        }
    char itemName[64];
    if (iBombPlanted || iBombDropped)
        {
        WeaponItemName(itemName, iBombPlanted ? 28 : 28);
        if (iBombPlanted) CopyAscii(itemName, 64, "w_c4");
        else CopyAscii(itemName, 64, "w_backpack");
        if (threeD) DrawStaticModel(*this, itemName, iBombX, iBombY, iBombZ + 1.0f, 0.0f);
        if (iBombPlanted && ((TInt)(iTime * 4.0f) & 1) == 0)
            DrawBlob(iRenderer, iBombX, iBombY, iBombZ + 6.0f, 1.5f, 0xFF2020u, 256);
        }
    if (iMode == 3 && iTermCircleActive)
        {
        DrawBlob(iRenderer, iTermCircleX, iTermCircleY, iTermCircleZ + 6.0f, 2.5f, 0x00FFFFu, 200);
        }
    if (iMode == 4)
        {
        for (TInt h = 0; h < 4; h++)
            {
            if (iHostageAlive[h] && !iHostageRescued[h])
                DrawBlob(iRenderer, iHostageX[h], iHostageY[h], iHostageZ[h] + 8.0f, 2.0f, 0xFFFF00u, 220);
            }
        DrawBlob(iRenderer, iRescueZoneX, iRescueZoneY, iRescueZoneZ + 6.0f, 3.0f, 0x00FF00u, 180);
        }
    for (TInt i = 0; i < KMaxDrops; i++)
        {
        const TDrop& drop = iDrops[i];
        if (!drop.iActive) continue;
        WeaponItemName(itemName, drop.iWeapon);
        if (threeD) DrawStaticModel(*this, itemName, drop.iX, drop.iY, drop.iZ, drop.iTime * 36.0f);
        }
    for (TInt i = 0; i < KMaxGrenades; i++)
        {
        const TGrenade& grenade = iGrenades[i];
        if (!grenade.iActive) continue;
        if (grenade.iType == 25) CopyAscii(itemName, 64, "w_hegrenade");
        else if (grenade.iType == 26) CopyAscii(itemName, 64, "w_flashbang");
        else CopyAscii(itemName, 64, "w_smokegrenade");
        if (threeD)
            DrawStaticModel(*this, itemName, grenade.iX, grenade.iY, grenade.iZ - 3.0f,
                            iTime * 540.0f + i * 77.0f);
        else
            {
            TUint32 color = 0x506080u;
            if (grenade.iType == 25) color = 0x405030u;
            else if (grenade.iType == 26) color = 0xB0B0B0u;
            DrawBlob(iRenderer, grenade.iX, grenade.iY, grenade.iZ, 2.5f, color, 256);
            }
        }
    }

TBool CGame::DrawViewModel3D(const TPlayer& aViewer, TReal32 aBob)
    {
    if (!aViewer.iAlive || (aViewer.iZoom > 0 && IsSniper(aViewer.iWeapon))) return EFalse;
    const TInt index = PlayerIndex(*this, aViewer);
    if (index < 0) return EFalse;
    CopyViewExtras(gExtra);
    const TPlayerViewExtra& extra = gExtra[index];
    char setName[32];
    ViewSetName(extra.iVmSet >= 0 ? extra.iVmSet : aViewer.iWeapon, setName);
    if (!EnsureViewModel3D(*this, extra.iVmSet >= 0 ? extra.iVmSet : aViewer.iWeapon, setName)) return EFalse;
    char animationName[32];
    CopyAscii(animationName, 32, "");
    for (TInt i = 0; i < 31 && extra.iVmName[i]; i++)
        animationName[i] = (char)extra.iVmName[i];
    animationName[31] = 0;
    TInt sequence = gViewAnimation->Find(animationName);
    TBool shooting = StartsWithAscii(animationName, "shoot");
    if (sequence < 0 || (!SameAscii(animationName, "idle") &&
        (shooting ? iTime - extra.iVmTime >= gViewAnimation->Duration(MaxI(0, sequence))
                  : iTime - extra.iVmTime >= extra.iVmLength)))
        {
        sequence = gViewAnimation->Find("idle");
        CopyAscii(animationName, 32, "idle");
        }
    if (sequence < 0) return EFalse;
    TReal32 frame = 0.0f;
    if (SameAscii(animationName, "idle")) frame = iTime * gViewAnimation->iFps[sequence];
    else if (shooting) frame = (iTime - extra.iVmTime) * gViewAnimation->iFps[sequence];
    else frame = extra.iVmLength > 0.0f
        ? (iTime - extra.iVmTime) / extra.iVmLength * (gViewAnimation->iFrames[sequence] - 1)
        : (TReal32)(gViewAnimation->iFrames[sequence] - 1);
    gViewAnimation->Pose(sequence, frame, SameAscii(animationName, "idle"), 0.0f,
                         gPoseRotation, gPosePosition);
    const TInt target = iAssets.iMap ? iAssets.iMap->LightAt(aViewer.iX, aViewer.iY, aViewer.iZ) : 32;
    TReal32 level = iLightLevel + (target - iLightLevel) * 0.2f;
    iLightLevel = (TInt)level;
    const TInt light = Clamp((TInt)(level * 0.8f + 8.0f), 0, 63);
    const TReal32 speed = iViewBob ? aViewer.iSpeedFactor : 0.0f;
    const TReal32 bobAmount = MinF(1.0f, (TInt)(speed / 250.0f));
    const TReal32 side = FloatSin(iTime * 4.5f) * bobAmount * 0.6f;
    const TReal32 vertical = -AbsF(FloatSin(iTime * 4.5f)) * bobAmount * 0.6f - extra.iPunchPitch * 0.08f;
    const TReal32 originX = iRenderer.iEyeX + iRenderer.iRightX * side + iRenderer.iUpX * vertical;
    const TReal32 originY = iRenderer.iEyeY + iRenderer.iRightY * side + iRenderer.iUpY * vertical;
    const TReal32 originZ = iRenderer.iEyeZ + iRenderer.iRightZ * side + iRenderer.iUpZ * vertical;
    gEntityMatrix[0] = iRenderer.iForwardX; gEntityMatrix[1] = iRenderer.iRightX; gEntityMatrix[2] = iRenderer.iUpX; gEntityMatrix[3] = originX;
    gEntityMatrix[4] = iRenderer.iForwardY; gEntityMatrix[5] = iRenderer.iRightY; gEntityMatrix[6] = iRenderer.iUpY; gEntityMatrix[7] = originY;
    gEntityMatrix[8] = iRenderer.iForwardZ; gEntityMatrix[9] = iRenderer.iRightZ; gEntityMatrix[10] = iRenderer.iUpZ; gEntityMatrix[11] = originZ;
    gViewModel->Build(gPoseRotation, gPosePosition, gEntityMatrix, gModelMatrix);
    iRenderer.ClearZ();
    for (TInt part = 0; part < gViewModel->iPartCount; part++)
        {
        if (!gViewModel->iParts || !gViewModel->iParts[part] || !gViewModel->iParts[part][0]) continue;
        iRenderer.DrawMesh(*gViewModel->iParts[part][0], gModelMatrix, light, ETrue, EFalse);
        }
    return ETrue;
    }

void CGame::DrawViewModel(const TPlayer& aViewer, TReal32 aBob)
    {
    if (aViewer.iZoom > 0 && IsSniper(aViewer.iWeapon))
        {
        DrawScope();
        return;
        }
    if (!aViewer.iAlive) return;
    const TInt index = PlayerIndex(*this, aViewer);
    if (index < 0) return;
    CopyViewExtras(gExtra);
    const TPlayerViewExtra& extra = gExtra[index];
    const TInt set = extra.iVmSet >= 0 ? extra.iVmSet : aViewer.iWeapon;
    char setName[32];
    ViewSetName(set, setName);
    EnsureViewSprites(*this, set, setName);
    if (!gViewSprites.iAnimCount || !gViewSprites.iSprites) return;
    char animationName[32];
    CopyAscii(animationName, 32, "idle");
    for (TInt i = 0; i < 31 && extra.iVmName[i]; i++)
        animationName[i] = (char)extra.iVmName[i];
    animationName[31] = 0;
    TInt animation = gViewSprites.Find(animationName);
    if (animation < 0) animation = gViewSprites.Find("idle");
    if (animation < 0) animation = 0;
    TReal32 elapsed = iTime - extra.iVmTime;
    if (elapsed < 0.0f) elapsed = 0.0f;
    const TInt frames = MaxI(1, gViewSprites.iNfr[animation]);
    TInt frame = 0;
    if (extra.iVmLength > 0.0f && elapsed < extra.iVmLength)
        frame = (TInt)(elapsed / extra.iVmLength * frames);
    else if (!SameAscii(animationName, "idle"))
        {
        const TInt idle = gViewSprites.Find("idle");
        if (idle >= 0) animation = idle;
        frame = 0;
        }
    frame = Clamp(frame, 0, frames - 1);
    const CSpriteSet::TSprite* sprite = gViewSprites.Frame(animation, 0, frame);
    if (!sprite) return;
    const TInt target = iAssets.iMap ? iAssets.iMap->LightAt(aViewer.iX, aViewer.iY, aViewer.iZ) : 32;
    TReal32 level = iLightLevel + (target - iLightLevel) * 0.2f;
    iLightLevel = (TInt)level;
    const TInt light = Clamp((TInt)(level * 0.85f + 10.0f), 0, 63);
    const TReal32 speed = iViewBob ? aViewer.iSpeedFactor : 0.0f;
    const TInt bobX = (TInt)(FloatSin(iTime * 4.5f) * MinF(1.0f, (TInt)(speed / 250.0f)) * 3.0f);
    const TInt bobY = (TInt)(AbsF(FloatSin(iTime * 4.5f)) * MinF(1.0f, (TInt)(speed / 250.0f)) * 3.0f)
                      + (TInt)(-extra.iPunchPitch * 0.6f);
    iRenderer.DrawBitmap(sprite->iOffsetX + bobX, sprite->iOffsetY + bobY + 2,
                         *sprite, gViewSprites.Lit(light), ETrue);
    }

void CGame::DrawScope()
    {
    CRenderer& renderer = iRenderer;
    if (!renderer.iFrameBuffer || renderer.iWidth <= 0 || renderer.iHeight <= 0) return;
    const TInt centerX = renderer.iWidth / 2;
    const TInt centerY = renderer.iHeight / 2;
    const TInt radius = MaxI(1, renderer.iHeight / 2 - 4);
    for (TInt y = 0; y < renderer.iHeight; y++)
        {
        const TInt dy = y - centerY;
        TInt half = -1;
        if (dy * dy < radius * radius) half = (TInt)FloatSqrt((TReal32)(radius * radius - dy * dy));
        for (TInt x = 0; x < renderer.iWidth; x++)
            if (half < 0 || x < centerX - half || x > centerX + half)
                renderer.iFrameBuffer[y * renderer.iWidth + x] = 0;
        }
    renderer.FillRect(0, centerY, renderer.iWidth, 1, 0);
    renderer.FillRect(centerX, 0, 1, renderer.iHeight, 0);
    }

void CGame::DrawMuzzle(const TPlayer& aViewer)
    {
    if (iTime >= iMuzzleUntil || !IsGun(aViewer.iWeapon)) return;
    if (&aViewer != &iPlayers[BoundedIndex(iMyIndex, iPlayerCount)]) return;
    const TInt centerX = iRenderer.iWidth / 2 + 46;
    const TInt centerY = iRenderer.iHeight / 2 + 34;
    for (TInt i = 0; i < 3; i++)
        iRenderer.FillRectAlpha(centerX - 6 + i * 2, centerY - 4 + i,
                                12 - i * 4, 8 - i * 2,
                                Rgb565(i == 2 ? 0xFFFFE0u : 0xFFC040u), 160 + i * 40);
    }

void CGame::DrawMenu()
    {
    EnsureUi(*this);
    if (iState == EStateLoad)
        {
        DrawLoading();
        return;
        }
    if (iState == EStateEnd)
        {
        DrawEndScreen();
        return;
        }
    CRenderer& renderer = iRenderer;
    if (iState == EStateMenu)
        {
        DrawBackground(renderer);
        if (gUi.iEditing != 0)
            {
            const TInt width = MinI(280, renderer.iWidth - 8);
            const TInt height = MinI(88, renderer.iHeight - 8);
            const TInt x = (renderer.iWidth - width) / 2;
            const TInt y = (renderer.iHeight - height) / 2;
            TBuf<112> title;
            AppendLocal(title, gUi.iEditing == 1 ? 399 : 327, iLanguage);
            DrawUiFrame(renderer, x, y, width, height, iAssets.iFontSmall, title);
            TBuf<160> value;
            AppendLimited(value, TPtrC(iStatusLine, TextLength(iStatusLine,
                sizeof(iStatusLine) / sizeof(TText))));
            DrawWrappedText(renderer, iAssets.iFontSmall, x + 10, y + 30, width - 20, value,
                            Rgb565(0xE0E0E0u), 13);
            const TInt buttonWidth = (width - 26) / 2;
            DrawButton(renderer, x + 8, y + height - 22, buttonWidth, 14, iAssets.iFontSmall,
                       LocalText(398, iLanguage), ETrue);
            DrawButton(renderer, x + 18 + buttonWidth, y + height - 22, buttonWidth, 14,
                       iAssets.iFontSmall, LocalText(269, iLanguage), EFalse);
            return;
            }
        if (iMenu == EMenuSearch)
            {
            DrawMainMenuItems(*this, EMenuMain, EFalse);
            TintScreen(renderer, KBlack, 120);
            DrawSearchDialog(*this);
            return;
            }
        if (iMenu == EMenuTeam)
            {
            DrawMainMenuItems(*this, EMenuMain, EFalse);
            TintScreen(renderer, KBlack, 156);
            DrawTeamMenuItems(*this);
            return;
            }
        const TBool dialog = IsDialogMenu(iMenu) || IsOptionsMenu(iMenu);
        DrawMainMenuItems(*this, dialog ? EMenuMain : iMenu, !dialog);
        if (dialog)
            {
            TintScreen(renderer, KBlack, 110);
            DrawDialogItems(*this);
            }
        return;
        }
    if (iState != EStatePlay) return;
    if (iMenu == EMenuNone && !iBuyOpen && !iScoreOpen) return;
    TintScreen(renderer, KBlack, iBuyOpen || iScoreOpen ? 110 : 90);
    if (iBuyOpen)
        {
        DrawBuyMenuItems(*this);
        return;
        }
    if (iScoreOpen)
        {
        DrawScoreboardTable(*this);
        return;
        }
    if (IsOverlayMenu(iMenu))
        {
        DrawTextMenuItems(*this);
        return;
        }
    switch (iMenu)
        {
    case EMenuTeam: DrawTeamMenuItems(*this); break;
    case EMenuPause: DrawMainMenuItems(*this, iMenu, ETrue); break;
    case EMenuHelp: DrawHelpLines(*this); break;
    default: DrawDialogItems(*this); break;
        }
    }

void CGame::DrawLoading()
    {
    CRenderer& renderer = iRenderer;
    DrawBackground(renderer);
    const TInt width = MinI(230, renderer.iWidth - 8);
    const TInt height = 58;
    const TInt x = (renderer.iWidth - width) / 2;
    const TInt y = (renderer.iHeight - height) / 2 + 30;
    TBuf<80> title;
    AppendLocal(title, 65, iLanguage);
    DrawUiFrame(renderer, x, y, width, height, iAssets.iFontSmall, title);
    TPtrC message = gUi.iLoadMessage[0] ? TPtrC(gUi.iLoadMessage, TextLength(gUi.iLoadMessage, 128))
                                        : TPtrC(iLoadMessage, TextLength(iLoadMessage, 128));
    DrawWrappedText(renderer, iAssets.iFontSmall, x + 10, y + 18, width - 20, message,
                    Rgb565(0xD8D8D8u), 13);
    const TInt segments = (TInt)((User::NTickCount() / 120) % 18) + 1;
    renderer.FillRect(x + 10, y + 34, width - 20, 12, Rgb565(0x3E4637u));
    DrawBevel(renderer, x + 10, y + 34, width - 20, 12, EFalse);
    for (TInt i = 0; i < segments && 2 + i * 11 + 9 < width - 20; i++)
        renderer.FillRect(x + 12 + i * 11, y + 36, 9, 8, Rgb565(0x8E8923u));
    }

void CGame::DrawEndScreen()
    {
    CRenderer& renderer = iRenderer;
    DrawBackground(renderer);
    DrawScoreboardTable(*this);
    TBuf<96> result;
    if (iScoreT == iScoreCT) AppendLocal(result, 341, iLanguage);
    else
        {
        AppendLocal(result, iScoreT > iScoreCT ? 190 : 301, iLanguage);
        AppendLocal(result, 371, iLanguage);
        }
    renderer.FillRectAlpha(0, renderer.iHeight - 17, renderer.iWidth, 17, Rgb565(KBlack), 200);
    DrawCenteredShadowText(renderer, renderer.iWidth / 2, renderer.iHeight - 14,
                           iAssets.iFontSmall, result, Rgb565(KVguiColor));
    }

void CGame::DrawRadar(const TPlayer& aViewer)
    {
    CRenderer& renderer = iRenderer;
    const TInt radius = MinI(30, renderer.iHeight / 6);
    const TInt centerX = 4 + radius;
    const TInt centerY = 16 + radius;
    for (TInt y = -radius; y <= radius; y++)
        {
        const TInt half = (TInt)FloatSqrt((TReal32)(radius * radius - y * y));
        renderer.FillRectAlpha(centerX - half, centerY + y, half * 2 + 1, 1,
                               Rgb565(0x103010u), 150);
        }
    const TReal32 cosine = FloatCos(aViewer.iYaw);
    const TReal32 sine = FloatSin(aViewer.iYaw);
    const TReal32 scale = (TReal32)radius / 1600.0f;
    for (TInt i = 0; i < iPlayerCount && i < KMaxPlayers; i++)
        {
        const TPlayer& player = iPlayers[i];
        if (!player.iAlive || &player == &aViewer) continue;
        if (player.iTeam != aViewer.iTeam &&
            !(iTime - gExtra[i].iNextAttack > -1.2f && IsGun(player.iWeapon) &&
              iTime - gExtra[i].iNextAttack < 1.0f)) continue;
        const TReal32 dx = player.iX - aViewer.iX;
        const TReal32 dy = player.iY - aViewer.iY;
        TReal32 forward = dx * cosine + dy * sine;
        TReal32 right = dx * sine - dy * cosine;
        TInt px = (TInt)(right * scale);
        TInt py = (TInt)(-forward * scale);
        const TInt limit = radius - 2;
        const TInt distance = px * px + py * py;
        if (distance > limit * limit)
            {
            const TReal32 length = FloatSqrt((TReal32)distance);
            if (length > 0.0f)
                {
                px = (TInt)(px / length * limit);
                py = (TInt)(py / length * limit);
                }
            }
        renderer.FillRect(centerX + px - 1, centerY + py - 1, 3, 3,
                          Rgb565(player.iTeam != aViewer.iTeam ? 0xFF3030u : 0x4080FFu));
        }
    if ((iBombPlanted || iBombDropped) && aViewer.iTeam == 1)
        {
        const TUint32 color = ((TInt)(iTime * 4.0f) & 1) == 0 ? 0xFF2020u : KWhite;
        const TReal32 dx = iBombX - aViewer.iX;
        const TReal32 dy = iBombY - aViewer.iY;
        renderer.FillRect(centerX + (TInt)((dx * sine - dy * cosine) * scale) - 1,
                          centerY + (TInt)(-(dx * cosine + dy * sine) * scale) - 1,
                          3, 3, Rgb565(color));
        }
    renderer.FillRect(centerX, centerY, 2, 2, Rgb565(KWhite));
    }

void CGame::DrawCrosshair(const TPlayer& aViewer)
    {
    if (iMenu != EMenuNone || iBuyOpen || iScoreOpen) return;
    const TInt weapon = Clamp(aViewer.iWeapon, 0, KWeaponCount - 1);
    const TBool scoped = aViewer.iZoom > 0 && IsSniper(weapon);
    const TUint16 color = Rgb565(KCrossColors[Clamp(iCrossColor, 0, 4)]);
    const TInt centerX = iRenderer.iWidth / 2;
    const TInt centerY = iRenderer.iHeight / 2;
    if (!scoped && IsGun(weapon) && !IsSniper(weapon))
        {
        const TInt index = PlayerIndex(*this, aViewer);
        const TReal32 recoil = index >= 0 ? gExtra[index].iRecoil : 0.0f;
        TInt gap = 2 + iCrossSize;
        if (iCrossDynamic)
            gap += (TInt)(KWpSpread[weapon] * 60.0f + recoil * 200.0f
                          + MinF(1.0f, (TInt)(aViewer.iSpeedFactor / 220.0f)) * KWpMoveSpread[weapon] * 100.0f
                          + (aViewer.iOnGround ? 0 : 10));
        else gap += 1;
        const TInt length = 3 + iCrossSize * 2;
        iRenderer.FillRect(centerX - gap - length, centerY, length, 1, color);
        iRenderer.FillRect(centerX + gap + 1, centerY, length, 1, color);
        iRenderer.FillRect(centerX, centerY - gap - length, 1, length, color);
        iRenderer.FillRect(centerX, centerY + gap + 1, 1, length, color);
        }
    else if (!scoped && (weapon == 0 || IsGrenade(weapon) || weapon == 28 || IsSniper(weapon)))
        iRenderer.FillRect(centerX, centerY, 1, 1, color);
    }

void CGame::ApplyNokiaFrameRate(TReal32 aDeltaYaw, TReal32 aDeltaPitch)
    {
    ReprojectFrame(aDeltaYaw, aDeltaPitch);
    }

void CGame::ReprojectFrame(TReal32 aDeltaYaw, TReal32 aDeltaPitch)
    {
    if (!iGeneratedFrame || !iRenderer.iFrameBuffer || !iGeneratedValid) return;
    const TInt width = iGeneratedWidth;
    const TInt height = iGeneratedHeight;
    if (width <= 0 || height <= 0 || width > KMaxFramePixels / MaxI(1, height)) return;
    const TReal32 yaw = Clamp(aDeltaYaw, -40.0f, 40.0f);
    const TReal32 pitch = Clamp(aDeltaPitch, -40.0f, 40.0f);
    const TInt shiftX = (TInt)(FloatTan(yaw) * iRenderer.iXScale);
    const TInt shiftY = (TInt)(-FloatTan(pitch) * iRenderer.iYScale);
    for (TInt y = 0; y < height; y++)
        {
        const TInt sourceY = Clamp(y - shiftY, 0, height - 1);
        const TInt sourceRow = sourceY * width;
        const TInt destinationRow = y * width;
        for (TInt x = 0; x < width; x++)
            iRenderer.iFrameBuffer[destinationRow + x] =
                iGeneratedFrame[sourceRow + Clamp(x - shiftX, 0, width - 1)];
        }
    const TInt pixels = MinI(width * height, KMaxFramePixels);
    for (TInt i = 0; i < pixels; i++)
        {
        if (gNfrVmMask[i]) iRenderer.iFrameBuffer[i] = gNfrVmPixels[i];
        }
    }

void CGame::ResampleToScreen()
    {
    if (!iGeneratedFrame || !iRenderer.iFrameBuffer) return;
    const TInt width = MinI(iGeneratedWidth, iRenderer.iWidth);
    const TInt height = MinI(iGeneratedHeight, iRenderer.iHeight);
    if (width <= 0 || height <= 0) return;
    for (TInt y = 0; y < height; y++)
        Mem::Copy(iRenderer.iFrameBuffer + y * iRenderer.iWidth,
                  iGeneratedFrame + y * iGeneratedWidth, width * sizeof(TUint16));
    }
