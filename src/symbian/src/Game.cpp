/*
 * Game.cpp -- игровой слой CS16_E52.
 *
 * Порт src/core/cs/Game.java, Move.java, Player.java, Bot.java, Nav.java
 * и Wp.java. Всё, что относится к отрисовке и меню, живёт в GameUi.cpp;
 * здесь только состояние мира, физика, оружие, боты, раунды и сеть.
 *
 * Все дополнительные массивы фиксированы и подготовлены в ConstructL/LoadMapL:
 * в Tick и других покадровых методах выделений памяти нет.
 */

#include "Game.h"
#include "Engine.h"

#include <e32std.h>
#include <e32base.h>
#include <e32math.h>
#include <string.h>

namespace {

// ------------------------------------------------------------------ константы
const TInt KGameActions = 22;
const TInt KMaxNavAreas = 2048;
const TInt KMaxNavLinks = 10;
const TInt KMaxNavSpots = 8;
const TInt KMaxNavPath = 96;
const TInt KMaxNavHeap = KMaxNavAreas * 4;
const TInt KMaxSpawns = 64;
const TInt KMaxSites = 16;
const TInt KMaxBuyZones = 32;
const TInt KMaxLadders = 32;
const TInt KMaxBreakables = 64;
const TInt KMaxSmokes = 6;
const TInt KMaxRadioQueue = 8;
const TInt KMaxVoicePlayers = KMaxPlayers;
const TInt KVoiceMaxPlayers = KMaxVoicePlayers;
const TInt KVoiceParts = 16;
const TInt KVoiceRate = 8000;
const TInt KVoiceMaxSamples = 48000;
const TInt KVoicePartSamples = 6000;

const TInt KTimeBomb    = 40;
const TInt KBuyWeapon   = 0;
const TInt KBuyEquip    = 1;
const TInt KBuyAmmo     = 2;

static const TText* const KMapNames[] =
    {
    (const TText*)L"de_dust2", (const TText*)L"de_dust", (const TText*)L"de_train",
    (const TText*)L"de_inferno", (const TText*)L"de_nuke", (const TText*)L"de_aztec",
    (const TText*)L"de_cbble", (const TText*)L"cs_italy", (const TText*)L"cs_office",
    (const TText*)L"cs_assault", (const TText*)L"cs_mansion"
    };
static const TInt KMapCount = 11;

static const TInt KRenderLevels[5] = { 320, 280, 240, 200, 160 };
static const TInt KNfrTargets[6] = { 0, 10, 15, 20, 25, 30 };

static const char* const KWeaponNames[KWeaponCount] =
    {
    "knife", "glock18", "usp", "p228", "deagle", "elite", "fiveseven",
    "m3", "xm1014", "mac10", "tmp", "mp5", "ump45", "p90",
    "galil", "famas", "ak47", "m4a1", "sg552", "aug", "scout", "awp",
    "g3sg1", "sg550", "m249", "hegrenade", "flashbang", "smokegrenade", "c4"
    };

static const TInt KZombieSpeed[3] = { 330, 225, 280 };
static const TInt KZombieDamage[3] = { 14, 55, 28 };
static const TInt KZombieHealth[3] = { 1500, 4000, 2500 };
static const TText* const KZombieNames[3] =
    { (const TText*)L"Zombie", (const TText*)L"Tank", (const TText*)L"Sprinter" };

static const char* const KWeaponSound[KWeaponCount] =
    {
    "weapons_knife_slash1", "weapons_glock18_2", "weapons_usp_unsil_1",
    "weapons_p228_1", "weapons_deagle_1", "weapons_elite_fire",
    "weapons_fiveseven_1", "weapons_m3_1", "weapons_xm1014_1",
    "weapons_mac10_1", "weapons_tmp_1", "weapons_mp5_1",
    "weapons_ump45_1", "weapons_p90_1", "weapons_galil_1",
    "weapons_famas_1", "weapons_ak47_1", "weapons_m4a1_unsil_1",
    "weapons_sg552_1", "weapons_aug_1", "weapons_scout_fire_1",
    "weapons_awp1", "weapons_g3sg1_1", "weapons_sg550_1",
    "weapons_m249_1", "weapons_pinpull", "weapons_pinpull",
    "weapons_pinpull", "weapons_c4_plant"
    };

static const char* const KRadioSound[21] =
    {
    "ct_coverme", "takepoint", "position", "regroup", "followme",
    "fireassis", "com_go", "fallback", "sticktog", "com_getinpos",
    "stormfront", "com_reportin", "ct_affirm", "ct_enemys",
    "ct_backup", "clear", "ct_inpos", "ct_reportingin", "blow",
    "negative", "enemydown"
    };

/* Коды телефонных клавиш по умолчанию (Binds.phoneDefaults, layout 0). */
static const TInt KDefaultBinds[KGameActions] =
    {
    '2', '8', '4', '6', -3, -4, -1, -2,
    '5', '1', '3', '7', '0', '9', 0,
    '*', '#', 0, 0, -6, -7, -10
    };

// ------------------------------------------------------------------ навигация
class CNavMesh
    {
public:
    CNavMesh() { Reset(); }

    void Reset()
        {
        iCount = 0;
        for (TInt i = 0; i < KMaxNavAreas; i++) iLinkCount[i] = 0;
        }

    TBool LoadL(RFs& aFs, const TDesC& aPath)
        {
        HBufC8* data = ReadWholeFileL(aFs, aPath);
        CleanupStack::PushL(data);
        TBinaryReader in(data->Ptr(), data->Length());
        TInt magic = in.ReadInteger();
        if (magic != 0x4E415631)         // "NAV1"
            {
            CleanupStack::PopAndDestroy(data);
            return EFalse;
            }
        TInt places = in.ReadUnsignedShort();
        for (TInt p = 0; p < places && p < 512; p++)
            {
            TInt length = in.ReadUnsignedShort();
            in.Skip(length);
            }
        iCount = in.ReadUnsignedShort();
        if (iCount > KMaxNavAreas) iCount = KMaxNavAreas;
        for (TInt i = 0; i < iCount; i++)
            {
            iX0[i] = in.ReadReal(); iY0[i] = in.ReadReal();
            iX1[i] = in.ReadReal(); iY1[i] = in.ReadReal();
            iZNW[i] = in.ReadReal(); iZNE[i] = in.ReadReal();
            iZSE[i] = in.ReadReal(); iZSW[i] = in.ReadReal();
            iAttr[i] = in.ReadByte();
            in.ReadUnsignedShort();      // place
            TInt links = in.ReadByte();
            if (links > KMaxNavLinks) links = KMaxNavLinks;
            iLinkCount[i] = links;
            for (TInt k = 0; k < links; k++)
                {
                iLinkDir[i][k] = in.ReadByte();
                iLink[i][k] = in.ReadUnsignedShort();
                }
            TInt spots = in.ReadByte();
            if (spots > KMaxNavSpots) spots = KMaxNavSpots;
            iSpotCount[i] = spots;
            for (TInt s = 0; s < spots; s++)
                {
                iSpot[i][s * 3] = in.ReadReal();
                iSpot[i][s * 3 + 1] = in.ReadReal();
                iSpot[i][s * 3 + 2] = in.ReadReal();
                }
            iCX[i] = (iX0[i] + iX1[i]) * 0.5f;
            iCY[i] = (iY0[i] + iY1[i]) * 0.5f;
            iCZ[i] = (iZNW[i] + iZNE[i] + iZSE[i] + iZSW[i]) * 0.25f;
            }
        CleanupStack::PopAndDestroy(data);
        return ETrue;
        }

    TReal32 ZAt(TInt aArea, TReal32 aX, TReal32 aY) const
        {
        TReal32 u = (iX1[aArea] - iX0[aArea]) > 0 ? (aX - iX0[aArea]) / (iX1[aArea] - iX0[aArea]) : 0;
        TReal32 v = (iY1[aArea] - iY0[aArea]) > 0 ? (aY - iY0[aArea]) / (iY1[aArea] - iY0[aArea]) : 0;
        u = Clamp(u, 0.0f, 1.0f); v = Clamp(v, 0.0f, 1.0f);
        TReal32 north = iZNW[aArea] + (iZNE[aArea] - iZNW[aArea]) * u;
        TReal32 south = iZSW[aArea] + (iZSE[aArea] - iZSW[aArea]) * u;
        return north + (south - north) * v;
        }

    TInt Find(TReal32 aX, TReal32 aY, TReal32 aZ) const
        {
        if (iCount <= 0) return -1;
        TInt best = -1; TReal32 bestDistance = 1e9f;
        for (TInt i = 0; i < iCount; i++)
            {
            if (aX >= iX0[i] - 1 && aX <= iX1[i] + 1 && aY >= iY0[i] - 1 && aY <= iY1[i] + 1)
                {
                TReal32 d = Abs(ZAt(i, aX, aY) - aZ);
                if (d < bestDistance && d < 80) { bestDistance = d; best = i; }
                }
            }
        if (best >= 0) return best;
        for (TInt i = 0; i < iCount; i++)
            {
            TReal32 dx = Max(Max(iX0[i] - aX, aX - iX1[i]), 0.0f);
            TReal32 dy = Max(Max(iY0[i] - aY, aY - iY1[i]), 0.0f);
            TReal32 dz = iCZ[i] - aZ;
            TReal32 d = dx * dx + dy * dy + dz * dz * 4;
            if (d < bestDistance) { bestDistance = d; best = i; }
            }
        return best;
        }

    void Portal(TInt aFrom, TInt aTo, TReal32 aTargetX, TReal32 aTargetY)
        {
        iPortalX = iCX[aTo]; iPortalY = iCY[aTo];
        TInt direction = -1;
        for (TInt k = 0; k < iLinkCount[aFrom]; k++)
            if (iLink[aFrom][k] == aTo) { direction = iLinkDir[aFrom][k]; break; }
        if (direction == 0 || direction == 2)
            {
            TReal32 lo = Max(iX0[aFrom], iX0[aTo]);
            TReal32 hi = Min(iX1[aFrom], iX1[aTo]);
            if (lo > hi) { TReal32 m = (lo + hi) * 0.5f; lo = hi = m; }
            TReal32 m = aTargetX;
            TReal32 pad = Min(16.0f, (hi - lo) * 0.5f);
            m = Clamp(m, lo + pad, hi - pad);
            iPortalX = m; iPortalY = direction == 0 ? iY0[aFrom] : iY1[aFrom];
            }
        else if (direction == 1 || direction == 3)
            {
            TReal32 lo = Max(iY0[aFrom], iY0[aTo]);
            TReal32 hi = Min(iY1[aFrom], iY1[aTo]);
            if (lo > hi) { TReal32 m = (lo + hi) * 0.5f; lo = hi = m; }
            TReal32 m = aTargetY;
            TReal32 pad = Min(16.0f, (hi - lo) * 0.5f);
            m = Clamp(m, lo + pad, hi - pad);
            iPortalY = m; iPortalX = direction == 1 ? iX1[aFrom] : iX0[aFrom];
            }
        iPortalZ = ZAt(aTo, iPortalX, iPortalY);
        }

    TInt Path(TInt aStart, TInt aGoal, TInt* aResult)
        {
        if (aStart < 0 || aGoal < 0 || aStart >= iCount || aGoal >= iCount) return 0;
        iStamp++;
        iHeapLength = 0;
        iG[aStart] = 0; iF[aStart] = Distance(aStart, aGoal);
        iFrom[aStart] = -1; iVisited[aStart] = iStamp; iClosed[aStart] = 0;
        Push(aStart);
        TInt iterations = 0;
        while (iHeapLength > 0 && iterations++ < 4000)
            {
            TInt a = Pop();
            if (iClosed[a] == iStamp) continue;
            iClosed[a] = iStamp;
            if (a == aGoal)
                {
                TInt length = 0;
                for (TInt k = a; k >= 0 && length < KMaxNavPath; k = iFrom[k]) length++;
                TInt write = length;
                for (TInt k = a; k >= 0 && write > 0; k = iFrom[k]) aResult[--write] = k;
                return length;
                }
            for (TInt k = 0; k < iLinkCount[a]; k++)
                {
                TInt b = iLink[a][k];
                if (b < 0 || b >= iCount || iClosed[b] == iStamp) continue;
                TReal32 climb = iCZ[b] - iCZ[a];
                if (climb > 60) continue;
                TReal32 cost = iG[a] + Distance(a, b) + (climb > 20 ? 60 : 0);
                if (iVisited[b] != iStamp || cost < iG[b])
                    {
                    iVisited[b] = iStamp; iG[b] = cost; iFrom[b] = a;
                    iF[b] = cost + Distance(b, aGoal);
                    Push(b);
                    }
                }
            }
        return 0;
        }

    TInt iCount;
    TReal32 iX0[KMaxNavAreas], iY0[KMaxNavAreas], iX1[KMaxNavAreas], iY1[KMaxNavAreas];
    TReal32 iZNW[KMaxNavAreas], iZNE[KMaxNavAreas], iZSE[KMaxNavAreas], iZSW[KMaxNavAreas];
    TReal32 iCX[KMaxNavAreas], iCY[KMaxNavAreas], iCZ[KMaxNavAreas];
    TUint8 iAttr[KMaxNavAreas];
    TUint8 iSpotCount[KMaxNavAreas];
    TReal32 iSpot[KMaxNavAreas][KMaxNavSpots * 3];
    TInt iLink[KMaxNavAreas][KMaxNavLinks];
    TUint8 iLinkDir[KMaxNavAreas][KMaxNavLinks];
    TUint8 iLinkCount[KMaxNavAreas];
    TReal32 iPortalX, iPortalY, iPortalZ;

private:
    TReal32 Distance(TInt a, TInt b) const
        {
        TReal32 dx = iCX[a] - iCX[b], dy = iCY[a] - iCY[b], dz = iCZ[a] - iCZ[b];
        return FloatSqrt(dx * dx + dy * dy + dz * dz);
        }
    void Push(TInt a)
        {
        if (iHeapLength >= KMaxNavHeap) return;
        TInt i = iHeapLength++;
        iHeap[i] = a;
        while (i > 0)
            {
            TInt parent = (i - 1) >> 1;
            if (iF[iHeap[parent]] <= iF[iHeap[i]]) break;
            TInt t = iHeap[parent]; iHeap[parent] = iHeap[i]; iHeap[i] = t; i = parent;
            }
        }
    TInt Pop()
        {
        TInt result = iHeap[0];
        iHeap[0] = iHeap[--iHeapLength];
        TInt i = 0;
        while (ETrue)
            {
            TInt left = i * 2 + 1, right = left + 1, small = i;
            if (left < iHeapLength && iF[iHeap[left]] < iF[iHeap[small]]) small = left;
            if (right < iHeapLength && iF[iHeap[right]] < iF[iHeap[small]]) small = right;
            if (small == i) break;
            TInt t = iHeap[small]; iHeap[small] = iHeap[i]; iHeap[i] = t; i = small;
            }
        return result;
        }

    TReal32 iG[KMaxNavAreas], iF[KMaxNavAreas];
    TInt iFrom[KMaxNavAreas], iVisited[KMaxNavAreas], iClosed[KMaxNavAreas];
    TInt iStamp;
    TInt iHeap[KMaxNavHeap], iHeapLength;
    };

// ------------------------------------------------------------------ состояние
struct TExtraPlayer
    {
    TReal32 iForward, iStrafe;
    TBool iJump, iDuck, iFire, iUse, iFire2;
    TBool iFirePrev, iUsePrev, iFire2Prev;
    TReal32 iNextAttack, iReloadEnd, iDrawEnd, iShellNext;
    TReal32 iRecoil, iJumpPenalty;
    TReal32 iPunchPitch, iPunchYaw;
    TInt iShotsFired, iSpawnSeq;
    TBool iHelmet, iDefuser, iZombie, iRemote;
    TInt iZType;
    TBool iSilM4, iSilUSP;
    TReal32 iFlashStart, iFlashUntil, iLastHurt, iLastHurtYaw;
    TReal32 iStepTime;
    TInt iLastAttacker;
    TInt iHe, iFlash, iSmoke;
    TBool iOnLadder, iJumpHeld;
    TInt iLast;
    TReal32 iDefuseProgress;
    TBool iDefusing;
    TReal32 iDrawTime;
    TInt iVmSet, iVmAnimation;
    TReal32 iVmTime, iVmLength;
    TText iVmName[32];

    // бот
    TInt iPath[KMaxNavPath];
    TInt iPathLength, iPathIndex, iGoal;
    TReal32 iRepathAt;
    TInt iTask, iSite, iEnemy, iIgnore;
    TReal32 iHoldUntil, iReactAt, iLastSeen, iLastSeenX, iLastSeenY, iLastSeenZ;
    TReal32 iErrYaw, iErrPitch, iEngageStart;
    TBool iSeen, iSpotted, iBought;
    TReal32 iNextPerceive, iStuckTime, iLastX, iLastY, iJumpUntil, iDuckUntil;
    TReal32 iBotStrafe, iStrafeTime, iBurstEnd, iPauseUntil;
    TReal32 iLookYaw, iLookPitch, iNextLook;
    TBool iHasLook;
    TReal32 iFlashedUntil, iHeardX, iHeardY, iHeardZ, iHeardAt;
    TReal32 iHoldYaw, iFollowUntil, iHoldHereUntil, iNextItemCheck;
    TInt iLeader;
    TReal32 iNextUnstuck, iIgnoreUntil;
    TBool iSniper;
    };

struct TDropExtra
    {
    TInt iClip, iAmmo, iOwner;
    TReal32 iYaw;
    TBool iSilent, iRest;
    };

struct TVoicePart
    {
    TBool iActive;
    TInt iSeq, iPart, iParts, iSamples;
    TUint8 iData[KVoicePartSamples / 2];
    };

struct TVoiceSlot
    {
    TInt iSeq;
    TInt iCount;
    TVoicePart iParts[KVoiceParts];
    };

struct TMatchState
    {
    TMatchState() { Mem::FillZ(this, sizeof(*this)); }

    TExtraPlayer iExtra[KMaxPlayers];
    TDropExtra iDropExtra[KMaxDrops];
    TInt iPrevKeys;
    CNavMesh iNav;
    TReal32 iSpawnT[KMaxSpawns * 4];
    TReal32 iSpawnCT[KMaxSpawns * 4];
    TInt iSpawnTCount, iSpawnCTCount;
    TInt iSiteModel[KMaxSites];
    TInt iSiteCount;
    TInt iBuyModel[KMaxBuyZones], iBuyTeam[KMaxBuyZones];
    TInt iBuyCount;
    TInt iBreakModel[KMaxBreakables], iBreakCount;
    TInt iDrawModel[KMaxBreakables], iDrawCount;
    TInt iLadders[KMaxLadders], iLadderCount;
    TInt iMode, iMpPhase, iHost, iMyNetId, iRoomPlayers, iOnline;
    TInt iLossStreakT, iLossStreakCT;
    TInt iRoundTime, iStartMoney;
    TReal32 iBuyTime, iFreezeTime, iFriendlyFire;
    TText iMapName[32];
    TInt iMapIndex, iPendingTeam;
    TBool iInfected;
    TReal32 iInfectAt;
    TInt iBombDropBy;
    TReal32 iBombDropTime;
    TInt iBinds[KGameActions];
    TText iServer[64], iPlayerName[24];
    TInt iServerPort;
    TBool iJoinRequested, iCreateRoom, iJoinSent, iConnectStarted;
    TBool iRosterRequested;
    TInt iPendingJoinId[KMaxPlayers];
    TText iPendingJoinName[KMaxPlayers][24];
    TInt iPendingJoinCount;
    TUint32 iJoinTick;
    TReal32 iSendAccum, iSnapAccum, iRosterAge;
    TInt iLastRound;
    TBool iRosterOk, iApplying, iPendingRoster;
    TUint8 iPendingData[512];
    TInt iPendingLength;
    TInt iVoiceSeq, iRoomTalkers;
    TReal32 iVoiceRecordStart;
    TBool iVoiceRecording, iVoiceHeld;
    TInt iMicTestState;
    TReal32 iMicTestAt;
    TUint16 iVoiceIn[KVoiceMaxSamples];
    TInt iVoiceInLength;
    TVoiceSlot iVoiceSlots[KVoiceMaxPlayers];
    TText iTalkName[24];
    TReal32 iTalkUntil;
    TUint16 iRecorded[KVoiceMaxSamples];
    TInt iRecordedLength;
    TBool iPendingVoice, iVoiceTooShort;
    TUint8 iPendingVoiceData[KVoicePartSamples / 2];
    TInt iPendingVoiceSamples, iPendingVoicePlayer;
    TInt iPendingVoiceSeq, iPendingVoicePart, iPendingVoiceParts;
    TInt iRadioQueuePlayer[KMaxRadioQueue], iRadioQueueCmd[KMaxRadioQueue];
    TReal32 iRadioQueueAt[KMaxRadioQueue];
    TBool iSmokeOn[KMaxSmokes];
    TInt iSmokeCount;
    TReal32 iSmokeX[KMaxSmokes], iSmokeY[KMaxSmokes], iSmokeZ[KMaxSmokes], iSmokeUntil[KMaxSmokes];
    TReal32 iTargetX[KMaxPlayers], iTargetY[KMaxPlayers], iTargetZ[KMaxPlayers], iTargetYaw[KMaxPlayers];
    TInt iRadarMode;
    TInt iNfrTick, iLastDrawTick, iNfrRealFrames, iNfrGenFrames, iNfrStatTick;
    TReal32 iNfrRealInterval, iNfrGenInterval, iNfrCool;
    TInt iFpsCount, iFpsTick;
    TInt iEffectHead;
    };

TMatchState gState;
CNavMesh* gNav = &gState.iNav;
static CGame* gGame = NULL;
static TReal32 gRayT = 0;
static TReal32 gTempForward[3];
static TReal32 gTempVectors[5 * 3];
#define gDropExtra gState.iDropExtra

/* Буфер низкого разрешения и таблицы голосового кодeka. */
static TUint16* gLowFrame = NULL;
static TInt gLowCapacity = 0;
static TUint8 gVoicePcm[KVoiceMaxSamples * 2];
static TUint8 gNetFrame[KNetBufferSize + 2];
static TReal32 gGeneratedX = 0;
static TReal32 gGeneratedY = 0;
static TReal32 gGeneratedZ = 0;
static TReal32 gGeneratedFov = 90.0f;
static TInt gGeneratedRenderWidth = 0;
static TInt gGeneratedRenderHeight = 0;
static TInt gNfrLeft = 0;
static TBool gNfrLastGenerated = EFalse;

static const TInt KVoiceStep[89] =
    {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272,
    2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635,
    13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
    };

static const TInt KVoiceIndex[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };

static const char* const KBotNames[] =
    {
    "Wolf", "Bear", "Fox", "Tiger", "Eagle", "Shark", "Snake", "Rhino", "Panther", "Viper",
    "Raven", "Lynx", "Hawk", "Moose", "Cobra", "Gator", "Rocket", "Ghost", "Storm", "Blade",
    "Flame", "Frost", "Thunder", "Ranger", "Rider", "Hunter", "Scout", "Major", "Colonel", "Doctor"
    };

// ------------------------------------------------------------------ утилиты
inline TReal32 Sqr(TReal32 a) { return a * a; }

inline TReal32 Sign(TReal32 a) { return a < 0 ? -1.0f : 1.0f; }

inline TInt BoundedIndex(TInt aValue, TInt aMaximum)
    { return aValue < 0 ? 0 : (aValue >= aMaximum ? aMaximum - 1 : aValue); }

inline TExtraPlayer& Ex(TPlayer& aPlayer)
    { return gState.iExtra[BoundedIndex(aPlayer.iId, KMaxPlayers)]; }

inline const TExtraPlayer& Ex(const TPlayer& aPlayer)
    { return gState.iExtra[BoundedIndex(aPlayer.iId, KMaxPlayers)]; }

inline TBool AsciiEquals(const char* aFirst, const char* aSecond)
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

inline void CopyAsciiToText(TText* aDestination, TInt aMaximum, const char* aSource)
    {
    TInt i = 0;
    for (; i < aMaximum - 1 && aSource && aSource[i]; i++)
        aDestination[i] = (TText)(TUint8)aSource[i];
    aDestination[i] = 0;
    }

inline TInt VmSetFor(const TPlayer& aPlayer)
    {
    const TExtraPlayer& x = Ex(aPlayer);
    if (aPlayer.iWeapon == 17 && x.iSilM4) return KWeaponCount;
    if (aPlayer.iWeapon == 2 && x.iSilUSP) return KWeaponCount + 1;
    return aPlayer.iWeapon;
    }

inline void SetVm(TPlayer& aPlayer, const char* aAnimation, TReal32 aLength, TReal32 aTime)
    {
    TExtraPlayer& x = Ex(aPlayer);
    x.iVmSet = AsciiEquals(aAnimation, "detach")
        ? (aPlayer.iWeapon == 17 ? KWeaponCount : KWeaponCount + 1) : VmSetFor(aPlayer);
    CopyAsciiToText(x.iVmName, 32, aAnimation);
    x.iVmTime = aTime;
    x.iVmLength = aLength;
    x.iVmAnimation = 0;
    }

inline TReal32 EyeZ(const TPlayer& aPlayer)
    { return aPlayer.iZ + (aPlayer.iDucked ? 12.0f : 17.0f); }

inline TReal32 SpeedFactor(const TPlayer& aPlayer)
    { return FloatSqrt(aPlayer.iVelocityX * aPlayer.iVelocityX + aPlayer.iVelocityY * aPlayer.iVelocityY); }

void CopyText(TText* aDestination, TInt aMaximum, const TDesC& aText)
    {
    TPtr destination(aDestination, aMaximum);
    destination.Copy(aText);
    }

void CopyText(TText* aDestination, TInt aMaximum, const TPtrC8& aText)
    {
    TInt length = Min(aMaximum - 1, aText.Length());
    if (length < 0) length = 0;
    for (TInt i = 0; i < length; i++)
        aDestination[i] = (TText)(TUint8)aText[i];
    aDestination[length] = 0;
    }

TBool IsSniperWeapon(TInt aWeapon)
    { return aWeapon == 20 || aWeapon == 21 || aWeapon == 22 || aWeapon == 23; }

TBool IsShotgunWeapon(TInt aWeapon)
    { return aWeapon == 7 || aWeapon == 8; }

TBool IsGun(TInt aWeapon)
    { return aWeapon >= 1 && aWeapon <= 24; }

TBool IsGrenade(TInt aWeapon)
    { return aWeapon >= 25 && aWeapon <= 27; }

TReal32 MatPower(TInt aMaterial)
    {
    if (aMaterial == 1) return 0.15f;
    if (aMaterial == 0 || aMaterial == 3) return 0.25f;
    if (aMaterial == 6 || aMaterial == 2) return 0.5f;
    if (aMaterial == 5) return 0.65f;
    if (aMaterial == 8) return 0.4f;
    return 1.0f;
    }

TReal32 MatDamage(TInt aMaterial)
    {
    if (aMaterial == 1) return 0.2f;
    if (aMaterial == 6) return 0.4f;
    if (aMaterial == 2 || aMaterial == 8) return 0.45f;
    if (aMaterial == 5) return 0.3f;
    if (aMaterial == 7) return 0.6f;
    return 0.5f;
    }

inline TReal32 WeaponRange(TInt aWeapon)
    {
    if (IsSniperWeapon(aWeapon)) return 5000;
    if ((aWeapon >= 14 && aWeapon <= 19) || aWeapon == 24) return 2600;
    if (IsShotgunWeapon(aWeapon)) return 700;
    if (aWeapon >= 9 && aWeapon <= 13) return 1500;
    if (aWeapon == 4) return 1600;
    if (aWeapon == 0) return 80;
    return 1100;
    }

TInt ParseFloatList(const TPtrC8& aText, TReal32* aOutput, TInt aMaximum)
    {
    TInt count = 0;
    TInt position = 0;
    while (position < aText.Length() && count < aMaximum)
        {
        while (position < aText.Length() && aText[position] <= 32) position++;
        TInt start = position;
        TBool negative = EFalse;
        if (position < aText.Length() && (aText[position] == '-' || aText[position] == '+'))
            {
            negative = aText[position] == '-';
            position++;
            }
        TBool digits = EFalse;
        TReal32 value = 0;
        while (position < aText.Length() && aText[position] >= '0' && aText[position] <= '9')
            { value = value * 10 + (aText[position] - '0'); position++; digits = ETrue; }
        if (position < aText.Length() && aText[position] == '.')
            {
            position++;
            TReal32 fraction = 0.1f;
            while (position < aText.Length() && aText[position] >= '0' && aText[position] <= '9')
                { value += (aText[position] - '0') * fraction; fraction *= 0.1f; position++; digits = ETrue; }
            }
        if (!digits)
            {
            if (position < aText.Length()) position++;
            continue;
            }
        aOutput[count++] = negative ? -value : value;
        if (position == start) position++;
        }
    return count;
    }

TInt ModelIndex(const TPtrC8& aValue)
    {
    if (aValue.Length() < 2 || aValue[0] != '*') return -1;
    TInt value = 0;
    for (TInt i = 1; i < aValue.Length(); i++)
        {
        if (aValue[i] < '0' || aValue[i] > '9') break;
        value = value * 10 + (aValue[i] - '0');
        }
    return value;
    }

TBool ClassIs(const TPtrC8& aValue, const char* aName)
    {
    TPtrC8 name(reinterpret_cast<const TUint8*>(aName), static_cast<TInt>(strlen(aName)));
    return aValue == name;
    }

TInt WriteByte(TUint8* aBuffer, TInt aOffset, TInt aValue)
    { aBuffer[aOffset] = (TUint8)aValue; return aOffset + 1; }

TInt WriteShort(TUint8* aBuffer, TInt aOffset, TInt aValue)
    {
    aBuffer[aOffset] = (TUint8)((TUint)aValue >> 8);
    aBuffer[aOffset + 1] = (TUint8)aValue;
    return aOffset + 2;
    }

TInt WriteInt(TUint8* aBuffer, TInt aOffset, TInt aValue)
    {
    aBuffer[aOffset] = (TUint8)((TUint32)aValue >> 24);
    aBuffer[aOffset + 1] = (TUint8)((TUint32)aValue >> 16);
    aBuffer[aOffset + 2] = (TUint8)((TUint32)aValue >> 8);
    aBuffer[aOffset + 3] = (TUint8)aValue;
    return aOffset + 4;
    }

TInt WriteText8(TUint8* aBuffer, TInt aOffset, const TDesC& aText, TInt aMaximum)
    {
    TInt count = aText.Length();
    if (count > aMaximum) count = aMaximum;
    aBuffer[aOffset++] = (TUint8)(count >> 8);
    aBuffer[aOffset++] = (TUint8)count;
    for (TInt i = 0; i < count; i++)
        {
        TText character = aText[i];
        aBuffer[aOffset++] = (TUint8)(character < 128 ? character : '?');
        }
    return aOffset;
    }

static TInt AbsI(TInt aValue) { return aValue < 0 ? -aValue : aValue; }
static TInt MinI(TInt aLeft, TInt aRight) { return aLeft < aRight ? aLeft : aRight; }
static TInt MaxI(TInt aLeft, TInt aRight) { return aLeft > aRight ? aLeft : aRight; }
static TReal32 AbsF(TReal32 aValue) { return aValue < 0 ? -aValue : aValue; }
static TReal32 MinF(TReal32 aLeft, TReal32 aRight) { return aLeft < aRight ? aLeft : aRight; }
static TReal32 MaxF(TReal32 aLeft, TReal32 aRight) { return aLeft > aRight ? aLeft : aRight; }

static TReal32 Atan2Deg(TReal32 aY, TReal32 aX)
    { return FloatAtan2(aY, aX) * 180.0f / 3.14159265358979323846f; }

static void CopyAscii8(const TPtrC8& aSource, TDes& aTarget)
    {
    aTarget.Zero();
    const TInt count = MinI(aSource.Length(), aTarget.MaxLength());
    for (TInt i = 0; i < count; i++)
        {
        const TUint8 c = aSource[i];
        aTarget.Append(c >= 0x20 && c < 0x7f ? (TText)c : (TText)'?');
        }
    }

static void SetAscii(TDes& aTarget, const char* aText)
    {
    aTarget.Zero();
    if (!aText) return;
    const TInt count = MinI((TInt)strlen(aText), aTarget.MaxLength());
    for (TInt i = 0; i < count; i++) aTarget.Append((TText)(TUint8)aText[i]);
    }

static TReal32 Dist2D(TReal32 aX1, TReal32 aY1, TReal32 aX2, TReal32 aY2)
    {
    const TReal32 dx = aX2 - aX1, dy = aY2 - aY1;
    return FloatSqrt(dx * dx + dy * dy);
    }

static void Normalize2D(TReal32& aX, TReal32& aY)
    {
    const TReal32 d = FloatSqrt(aX * aX + aY * aY);
    if (d < 0.00001f) { aX = 1.0f; aY = 0.0f; return; }
    aX /= d; aY /= d;
    }

static TInt RandomRange(TRandom& aRandom, TInt aLow, TInt aHigh)
    {
    if (aHigh <= aLow) return aLow;
    return aLow + (TInt)((aRandom.Next() >> 8) % (TUint32)(aHigh - aLow + 1));
    }

static TReal32 RandomReal(TRandom& aRandom, TReal32 aLow, TReal32 aHigh)
    { return aLow + aRandom.NextReal() * (aHigh - aLow); }

static TBool IsSniper(TInt aWeapon) { return IsSniperWeapon(aWeapon); }
static TBool IsShotgun(TInt aWeapon) { return IsShotgunWeapon(aWeapon); }
static TBool IsPistol(TInt aWeapon) { return aWeapon >= 1 && aWeapon <= 6; }

static const char* WeaponSound(TInt aWeapon)
    {
    if (aWeapon >= 0 && aWeapon < KWeaponCount && KWeaponSound[aWeapon])
        return KWeaponSound[aWeapon];
    return "weapons_glock18_2";
    }

static void VoiceEncode(const TUint16* aSamples, TInt aCount, TUint8* aOutput)
    {
    TInt predicted = 0;
    TInt index = 0;
    for (TInt i = 0; i < aCount; i++)
        {
        TInt step = KVoiceStep[index];
        TInt sample = (TInt)(TInt16)aSamples[i];
        TInt diff = sample - predicted;
        TInt code = 0;
        if (diff < 0) { code = 8; diff = -diff; }
        TInt value = step >> 3;
        if (diff >= step) { code |= 4; diff -= step; value += step; }
        step >>= 1;
        if (diff >= step) { code |= 2; diff -= step; value += step; }
        step >>= 1;
        if (diff >= step) { code |= 1; value += step; }
        predicted += (code & 8) ? -value : value;
        predicted = Clamp(predicted, -32768, 32767);
        index = Clamp(index + KVoiceIndex[code & 7], 0, 88);
        if ((i & 1) == 0) aOutput[i >> 1] = (TUint8)code;
        else aOutput[i >> 1] |= (TUint8)(code << 4);
        }
    }

static void VoiceDecode(const TUint8* aData, TInt aCount, TUint16* aOutput)
    {
    TInt predicted = 0;
    TInt index = 0;
    for (TInt i = 0; i < aCount; i++)
        {
        TInt code = (aData[i >> 1] >> ((i & 1) * 4)) & 15;
        TInt step = KVoiceStep[index];
        TInt value = step >> 3;
        if (code & 4) value += step;
        if (code & 2) value += step >> 1;
        if (code & 1) value += step >> 2;
        predicted += (code & 8) ? -value : value;
        predicted = Clamp(predicted, -32768, 32767);
        index = Clamp(index + KVoiceIndex[code & 7], 0, 88);
        aOutput[i] = (TUint16)(TInt16)predicted;
        }
    }

static void VoiceNormalize(TUint16* aSamples, TInt aCount)
    {
    if (aCount <= 0) return;
    TInt64 sum = 0;
    TInt peak = 1;
    for (TInt i = 0; i < aCount; i++) sum += (TInt)(TInt16)aSamples[i];
    const TInt dc = (TInt)(sum / aCount);
    for (TInt i = 0; i < aCount; i++)
        {
        TInt value = (TInt)(TInt16)aSamples[i] - dc;
        if (value < 0) value = -value;
        if (value > peak) peak = value;
        }
    TInt gain = (TInt)((TInt64)23000 * 256 / peak);
    if (gain > 8 * 256) gain = 8 * 256;
    for (TInt i = 0; i < aCount; i++)
        {
        TInt value = (((TInt)(TInt16)aSamples[i] - dc) * gain) >> 8;
        aSamples[i] = (TUint16)(TInt16)Clamp(value, -32768, 32767);
        }
    const TInt fade = MinI(120, aCount / 2);
    for (TInt i = 0; i < fade; i++)
        {
        aSamples[i] = (TUint16)(TInt16)(((TInt)(TInt16)aSamples[i] * i) / fade);
        aSamples[aCount - 1 - i] = (TUint16)(TInt16)(((TInt)(TInt16)aSamples[aCount - 1 - i] * i) / fade);
        }
    }

static void VoiceResample(const TUint16* aSource, TInt aCount, TInt aRate,
                          TUint16* aTarget, TInt& aTargetCount, TInt aMaximum)
    {
    TInt count = aCount;
    if (aRate != KVoiceRate)
        count = (TInt)((TInt64)aCount * KVoiceRate / aRate);
    if (count > aMaximum) count = aMaximum;
    for (TInt i = 0; i < count; i++)
        {
        if (aRate == KVoiceRate)
            {
            aTarget[i] = aSource[i];
            continue;
            }
        TInt64 position = (TInt64)i * aRate * 256 / KVoiceRate;
        TInt first = (TInt)(position >> 8);
        TInt fraction = (TInt)(position & 255);
        if (first >= aCount - 1) { aTarget[i] = aSource[aCount - 1]; continue; }
        TInt left = (TInt)(TInt16)aSource[first];
        TInt right = (TInt)(TInt16)aSource[first + 1];
        aTarget[i] = (TUint16)(TInt16)(left + ((right - left) * fraction >> 8));
        }
    aTargetCount = count;
    }

static void UpscaleBilinear(const TUint16* aSource, TInt aSourceWidth, TInt aSourceHeight,
                            TUint16* aTarget, TInt aTargetWidth, TInt aTargetHeight)
    {
    if (!aSource || !aTarget || aSourceWidth <= 0 || aSourceHeight <= 0) return;
    if (aSourceWidth == aTargetWidth && aSourceHeight == aTargetHeight)
        {
        Mem::Copy(aTarget, aSource, aSourceWidth * aSourceHeight * sizeof(TUint16));
        return;
        }
    for (TInt y = 0; y < aTargetHeight; y++)
        {
        TInt positionY = (TInt)((((TInt64)(2 * y + 1) * aSourceHeight * 256) / (2 * aTargetHeight)) - 128);
        if (positionY < 0) positionY = 0;
        if (positionY > (aSourceHeight - 1) * 256) positionY = (aSourceHeight - 1) * 256;
        const TInt y0 = positionY >> 8;
        const TInt y1 = MinI(y0 + 1, aSourceHeight - 1);
        const TInt fy = positionY & 255;
        const TInt iy = 256 - fy;
        for (TInt x = 0; x < aTargetWidth; x++)
            {
            TInt positionX = (TInt)((((TInt64)(2 * x + 1) * aSourceWidth * 256) / (2 * aTargetWidth)) - 128);
            if (positionX < 0) positionX = 0;
            if (positionX > (aSourceWidth - 1) * 256) positionX = (aSourceWidth - 1) * 256;
            const TInt x0 = positionX >> 8;
            const TInt x1 = MinI(x0 + 1, aSourceWidth - 1);
            const TInt fx = positionX & 255;
            const TInt ix = 256 - fx;
            const TUint16 a = aSource[y0 * aSourceWidth + x0];
            const TUint16 b = aSource[y0 * aSourceWidth + x1];
            const TUint16 c = aSource[y1 * aSourceWidth + x0];
            const TUint16 d = aSource[y1 * aSourceWidth + x1];
            TInt red = ((((a >> 11) & 31) * ix + ((b >> 11) & 31) * fx) * iy
                        + (((c >> 11) & 31) * ix + ((d >> 11) & 31) * fx) * fy) >> 16;
            TInt green = ((((a >> 5) & 63) * ix + ((b >> 5) & 63) * fx) * iy
                         + (((c >> 5) & 63) * ix + ((d >> 5) & 63) * fx) * fy) >> 16;
            TInt blue = (((a & 31) * ix + (b & 31) * fx) * iy
                        + ((c & 31) * ix + (d & 31) * fx) * fy) >> 16;
            aTarget[y * aTargetWidth + x] = (TUint16)((red << 11) | (green << 5) | blue);
            }
        }
    }

static TPtrC8 AsciiKey(const char* aText)
    { return TPtrC8(reinterpret_cast<const TUint8*>(aText), (TInt)strlen(aText)); }

static void ParseVector(const TPtrC8& aText, TReal32* aOutput, TInt aMaximum)
    {
    for (TInt i = 0; i < aMaximum; i++) aOutput[i] = 0;
    ParseFloatList(aText, aOutput, aMaximum);
    }

static TBool InBrushModel(const CMap& aMap, TInt aModel, TReal32 aX, TReal32 aY, TReal32 aZ, TReal32 aPad)
    {
    if (aModel < 0 || aModel >= aMap.numModels) return EFalse;
    return aX > aMap.mMins[aModel * 3] - aPad && aX < aMap.mMaxs[aModel * 3] + aPad
        && aY > aMap.mMins[aModel * 3 + 1] - aPad && aY < aMap.mMaxs[aModel * 3 + 1] + aPad
        && aZ > aMap.mMins[aModel * 3 + 2] - aPad - 36 && aZ < aMap.mMaxs[aModel * 3 + 2] + aPad + 36;
    }

static TReal32 ModelCenterX(const CMap& aMap, TInt aModel)
    { return aModel >= 0 && aModel < aMap.numModels ? (aMap.mMins[aModel * 3] + aMap.mMaxs[aModel * 3]) * 0.5f : 0; }

static TReal32 ModelCenterY(const CMap& aMap, TInt aModel)
    { return aModel >= 0 && aModel < aMap.numModels ? (aMap.mMins[aModel * 3 + 1] + aMap.mMaxs[aModel * 3 + 1]) * 0.5f : 0; }

static TBool InAnySite(const CMap& aMap, const TMatchState& aState,
                       TReal32 aX, TReal32 aY, TReal32 aZ, TReal32 aPad)
    {
    for (TInt i = 0; i < aState.iSiteCount; i++)
        if (InBrushModel(aMap, aState.iSiteModel[i], aX, aY, aZ, aPad)) return ETrue;
    return EFalse;
    }

static TInt SiteIndexAt(const CMap& aMap, const TMatchState& aState,
                        TReal32 aX, TReal32 aY, TReal32 aZ)
    {
    for (TInt i = 0; i < aState.iSiteCount; i++)
        if (InBrushModel(aMap, aState.iSiteModel[i], aX, aY, aZ, 64)) return i;
    TInt best = 0;
    TReal32 bestDistance = 1e12f;
    for (TInt i = 0; i < aState.iSiteCount; i++)
        {
        const TReal32 dx = ModelCenterX(aMap, aState.iSiteModel[i]) - aX;
        const TReal32 dy = ModelCenterY(aMap, aState.iSiteModel[i]) - aY;
        const TReal32 distance = dx * dx + dy * dy;
        if (distance < bestDistance) { bestDistance = distance; best = i; }
        }
    return best;
    }

// ------------------------------------------------------------------ общие утилиты

static TPtrC8 Ascii(const char* aText)
    { return TPtrC8(reinterpret_cast<const TUint8*>(aText), aText ? (TInt)strlen(aText) : 0); }

static TReal32 PowF(TReal32 aBase, TReal32 aExponent)
    {
    if (aBase <= 0) return 0;
    TReal logarithm = 0;
    TReal exponential = 0;
    if (Math::Log(logarithm, (TReal)aBase) != KErrNone) return 0;
    if (Math::Exp(exponential, (TReal)aExponent * logarithm) != KErrNone) return 0;
    return (TReal32)exponential;
    }

static void SettingsPath(TDes& aPath, const CAssetManager& aAssets)
    {
    aPath.Copy(aAssets.iDirectory);
    aPath.Append(_L("settings.bin"));
    }

static void NavPath(TDes& aPath, const CAssetManager& aAssets, const TDesC& aName)
    {
    aPath.Copy(aAssets.iDirectory);
    aPath.Append(aName);
    aPath.Append(_L(".nv"));
    }

static TInt ReadBe32(const TUint8* aData)
    { return (TInt)((TUint32)aData[0] << 24 | (TUint32)aData[1] << 16 | (TUint32)aData[2] << 8 | aData[3]); }

static void WriteBe32(TUint8* aData, TInt aValue)
    {
    aData[0] = (TUint8)((TUint32)aValue >> 24);
    aData[1] = (TUint8)((TUint32)aValue >> 16);
    aData[2] = (TUint8)((TUint32)aValue >> 8);
    aData[3] = (TUint8)aValue;
    }

static void ForwardVector(TReal32 aYaw, TReal32 aPitch, TReal32* aOutput)
    {
    const TReal32 cy = FloatCos(aYaw), sy = FloatSin(aYaw);
    const TReal32 cp = FloatCos(aPitch), sp = FloatSin(aPitch);
    aOutput[0] = cp * cy;
    aOutput[1] = cp * sy;
    aOutput[2] = -sp;
    }

static TBool Pressed(TInt aOld, TInt aNew, TInt aBit)
    { return (aNew & aBit) != 0 && (aOld & aBit) == 0; }

    } // anonymous namespace

/* Прототипы helpers из нижней части файла: static-функции доступны
   всем методам CGame, определённым ниже. */
static TInt PlayerIndexById(const CGame& aGame, TInt aId);
static void DirectSwitch(TPlayer& aPlayer, TInt aWeapon, TReal32 aTime);
static void SimpleDrop(TPlayer& aPlayer, TInt aWeapon, TReal32 aSpeed, TReal32 aTime);
static TBool BuyWeaponFor(CGame& aGame, TPlayer& aPlayer, TInt aWeapon);
static TBool BuyEquipFor(TPlayer& aPlayer, TInt aKind);
static void BotBuy(CGame& aGame, TPlayer& aPlayer);
static TInt BotArea(const TPlayer& aPlayer);
static TBool BotInArea(const TPlayer& aPlayer, TInt aArea);
static TBool BotGo(CGame& aGame, TPlayer& aPlayer, TExtraPlayer& aExtra, TInt aArea);
static TInt BotGoalForSite(CGame& aGame, TInt aSite);
static TBool BotWanderGoal(CGame& aGame, TPlayer& aPlayer, TExtraPlayer& aExtra);
static TInt BotHoldSpot(CGame& aGame, TPlayer& aPlayer, TExtraPlayer& aExtra);
static TInt BotHuntTarget(CGame& aGame, TPlayer& aPlayer);
static void BotChooseGoal(CGame& aGame, TPlayer& aPlayer, TExtraPlayer& aExtra, TBool aArrived);
static void BotRadioReaction(TMatchState& aState, TPlayer& aBot, const TPlayer& aFrom,
                             TInt aCommand, TReal32 aTime);
static void LocalChat(CGame& aGame, const TDesC& aLine, TInt aColor);
static void BotifyPlayer(CGame& aGame, TPlayer& aPlayer);
static void PeerJoin(CGame& aGame, TMatchState& aState, TInt aNetId, const TDesC& aName);
static void FlushPendingJoins(TMatchState& aState, CGame& aGame);
static void PeerLeft(CGame& aGame, TMatchState& aState, TInt aNetId);
static void BecomeHost(CGame& aGame, TMatchState& aState);
static void NetworkFail(CGame& aGame, TMatchState& aState, const TDesC& aReason);
static void ParseNetAddress(const TDesC& aServer, TBuf8<96>& aHost, TInt& aPort);
static void HostSendRoster(CGame& aGame);
static void HostSendSnapshot(CGame& aGame);
static void ClientClearRound(CGame& aGame);
static void ApplyRoster(CGame& aGame, TNetReader& aReader);
static void ApplyMeSnapshot(CGame& aGame, TPlayer& aPlayer, TInt aFlags,
                            TReal32 aX, TReal32 aY, TReal32 aZ, TReal32 aYaw,
                            TInt aWeapon, TInt aHealth, TInt aArmor, TInt aMoney,
                            TInt aPrimary, TInt aSecondary, TInt aGrenades,
                            TInt aSeq, TInt aFlash);
static void ApplySnapshot(CGame& aGame, TNetReader& aReader);
static void InterpolatePeers(CGame& aGame, TReal32 aDelta);
static void ApplyVoicePart(CGame& aGame, TInt aPlayerId, TInt aSeq, TInt aPart,
                           TInt aParts, TInt aSamples, const TUint8* aData, TInt aBytes);
static void SendVoiceClip(CGame& aGame, const TUint16* aSamples, TInt aCount);
static void SendClientAction(CGame& aGame, TInt aOp, TInt aFirst, TInt aSecond);

// ------------------------------------------------------------------ жизненный цикл

CGame* CGame::NewL(CRenderer& aRenderer, CAssetManager& aAssets,
                   CAudio& aAudio, RSocketServ& aSocketServer,
                   TInt aWidth, TInt aHeight)
    {
    CGame* self = new (ELeave) CGame(aRenderer, aAssets, aAudio, aSocketServer,
                                     aWidth, aHeight);
    CleanupStack::PushL(self);
    self->ConstructL();
    CleanupStack::Pop(self);
    return self;
    }

CGame::CGame(CRenderer& aRenderer, CAssetManager& aAssets, CAudio& aAudio,
             RSocketServ& aSocketServer, TInt aWidth, TInt aHeight)
    : iRenderer(aRenderer), iAssets(aAssets), iAudio(aAudio),
      iSocketServer(aSocketServer)
    {
    Mem::FillZ(iPlayers, sizeof(iPlayers));
    Mem::FillZ(iGrenades, sizeof(iGrenades));
    Mem::FillZ(iDrops, sizeof(iDrops));
    Mem::FillZ(iEffects, sizeof(iEffects));

    iQuality = 0;
    iGfx = 1;
    iRenderLevel = 0;
    iFpsCap20 = EFalse;
    iGfxSetting = 1;
    iNokiaFrameRate = 0;
    iOrientation = 0;
    iVolume = 80;
    iSoundOn = ETrue;
    iShowFps = ETrue;
    iLanguage = 0;
    iSensitivity = 5;
    iSensitivityVertical = 5;
    iInvertY = EFalse;
    iLayout = 0;
    iDifficulty = 1;
    iMaxRounds = 15;
    iAutoAim = ETrue;
    iGammaIndex = 1;
    iCrossColor = 0;
    iCrossSize = 1;
    iCrossDynamic = ETrue;
    iViewBob = ETrue;
    iRadar = ETrue;
    iRenderMode = 0; /* auto */
    iGpuAvailable = EFalse;
    iFirstRunDone = EFalse;
    iVoiceOn = ETrue;
    iVoiceVolume = 100;
    iWinScale = 3;
    iMapIndex = 0;
    iTeamChoice = 3;
    iBotCount = 9;
    iRoundSeconds = 135;
    iBuySeconds = 45;
    iFreezeSeconds = 3;
    iStartMoney = 800;
    iFriendlyFire = EFalse;
    iPcMouse = EFalse;

    iState = EStateMenu;
    iMenu = EMenuMain;
    iMenuSelection = 0;
    iBuyOpen = EFalse;
    iBuyCategory = -1;
    iScoreOpen = EFalse;
    iWidth = aWidth > 0 ? aWidth : 240;
    iHeight = aHeight > 0 ? aHeight : 320;
    iTime = 0;
    iFps = 0;
    iRealDelta = 0;
    iPaused = EFalse;
    iTurned = EFalse;
    iPlayerCount = 0;
    iMyIndex = 0;
    iSpectatorIndex = -1;
    iRound = 0;
    iScoreT = 0;
    iScoreCT = 0;
    iRoundOver = EFalse;
    iBombPlanted = EFalse;
    iBombDropped = EFalse;
    iBombExploded = EFalse;
    iBombSite = -1;
    iConnection = NULL;
    iLanFinder = NULL;
    iHttpClient = NULL;
    iNetworkHost = EFalse;
    iCenterMessageUntil = 0;
    iCenterMessageColor = 0xFFFFFF;
    iChatHead = 0;
    iHurtFlash = 0;
    iMuzzleUntil = 0;
    iViewBobAmount = 0;
    iTurnHeld = 0;
    iLightLevel = 32;
    iMyLight = 32;
    iDeadCameraYaw = 0;
    iGeneratedFrame = NULL;
    iGeneratedWidth = 0;
    iGeneratedHeight = 0;
    iGeneratedYaw = 0;
    iGeneratedPitch = 0;
    iGeneratedValid = EFalse;
    iFramesGenerated = 0;
    iRenderSkip = 0;
    iRandom.Seed(0x13579BDFu);
    iKeys = 0;
    iBindCapture = -1;
    iPendingMap = 0;
    iPendingTeam = 3;
    iLoadStage = 0;
    iMapName[0] = 0;
    iRoundTimeSetting = 135;
    iBuyTimeSetting = 45;
    iFreezeSetting = 3;
    iCenterMessageTime = 0;
    iBombDropTime = 0;
    iBombDroppedBy = -1;
    iInfected = EFalse;
    iInfectAt = 0;
    iMode = 0;
    iTermCircleX = 0; iTermCircleY = 0; iTermCircleZ = 0;
    iTermCircleActive = EFalse;
    iTerminatorIdx = -1;
    iTerminatorClass = 0;
    iDisguiseTimer = 0; iDisguiseCooldown = 0; iIsDisguised = EFalse;
    iKeyDisguise = '*';
    iOptReturn = EMenuMain;
    iOptReturnSelection = 0;
    iHelpReturn = EMenuMain;
    iBindsReturn = EMenuMain;
    iBindUntil = 0;
    iMenuTab = 0;
    iKeysDown = 0;
    iAutoAccumulator = 0;
    iAutoFrames = 0;
    iAutoCooldown = 0;
    iStatusLine[0] = 0;
    iDifficultyName = 1;
    iSpectating = EFalse;

    for (TInt i = 0; i < KGameActions; i++) gState.iBinds[i] = KDefaultBinds[i];
    gState.iMode = 0;
    gState.iSendAccum = 0;
    gState.iSnapAccum = 0;
    gState.iRosterAge = 0;
    gState.iVoiceSeq = 0;
    gState.iMicTestState = 0;
    gState.iTalkUntil = 0;
    gState.iRadarMode = 0;
    gState.iPrevKeys = 0;
    }

CGame::~CGame()
    {
    gGame = NULL;
    delete iConnection;
    iConnection = NULL;
    delete iLanFinder;
    iLanFinder = NULL;
    delete iHttpClient;
    iHttpClient = NULL;
    delete[] iGeneratedFrame;
    iGeneratedFrame = NULL;
    delete[] gLowFrame;
    gLowFrame = NULL;
    gLowCapacity = 0;
    }

void CGame::ConstructL()
    {
    gGame = this;
    if (!iConnection)
        {
        iConnection = CNetConnection::NewL(iSocketServer);
        CleanupStack::PushL(iConnection);
        Resize(iWidth, iHeight);
        ApplyGfx();
        CleanupStack::Pop(iConnection);
        }
    else
        {
        Resize(iWidth, iHeight);
        ApplyGfx();
        }
    }

void CGame::InitL()
    {
    LoadSettings();
    iGpuAvailable = CsGpuAvailable();
    if (!iGpuAvailable && iRenderMode == 1) iRenderMode = 0;
    ApplyGfx();
    ApplyGamma();
    SetLevel(iQuality == 0 ? 0 : (iQuality == 1 ? 0 : (iQuality == 2 ? 2 : 4)));
    if (!iFirstRunDone)
        {
        iState = EStateMenu;
        iMenu = EMenuFirstRun1;
        iMenuSelection = 0;
        }
    for (TInt i = 0; i < KMaxChatLines; i++)
        {
        iChatLines[i][0] = 0;
        iChatColors[i] = 0xFFFFFF;
        iKillFeed[i][0] = iKillFeed[i][1] = iKillFeed[i][2] = -1;
        iKillFeedTime[i] = -100;
        }
    iAudio.SetMasterVolume(iVolume);
    }

void CGame::Resize(TInt aWidth, TInt aHeight)
    {
    if (aWidth > 0) iWidth = aWidth;
    if (aHeight > 0) iHeight = aHeight;
    const TInt pixels = iWidth * iHeight;
    if (pixels <= 0) return;
    if (gLowCapacity < pixels)
        {
        delete[] gLowFrame;
        gLowFrame = new TUint16[pixels];
        gLowCapacity = gLowFrame ? pixels : 0;
        }
    if (!iGeneratedFrame || iGeneratedWidth != iWidth || iGeneratedHeight != iHeight)
        {
        delete[] iGeneratedFrame;
        iGeneratedFrame = new TUint16[pixels];
        iGeneratedWidth = iGeneratedFrame ? iWidth : 0;
        iGeneratedHeight = iGeneratedFrame ? iHeight : 0;
        }
    iGeneratedValid = EFalse;
    SetLevel(iRenderLevel);
    }

void CGame::SetOrientation(TInt aOrientation)
    {
    iOrientation = aOrientation < 0 ? 0 : (aOrientation > 1 ? 1 : aOrientation);
    iTurned = iOrientation == 1;
    iGeneratedValid = EFalse;
    }

void CGame::ApplyGfx()
    {
    iRenderer.iMipBias = iGfx == 0 ? 1 : 0;
    iRenderer.iStepShift = iGfx == 0 ? 5 : (iGfx == 1 ? 4 : 3);
    iRenderer.iBilinear = iGfx == 3;
    iRenderer.iFlatSky = iGfx == 0;
    iRenderer.iMipScale = iGfx >= 2 ? 0.7f : 1.0f;
    iRenderer.iFullBright = EFalse;
    iGfxSetting = iGfx;
    }

void CGame::SetLevel(TInt aLevel)
    {
    iRenderLevel = Clamp(aLevel, 0, 4);
    ApplyGfx();
    }

void CGame::ApplyGamma()
    {
    const TReal32 gm = 1.0f + iGammaIndex * 0.15f;
    for (TInt i = 0; i < 256; i++)
        {
        TInt value = (TInt)(255.0f * PowF((TReal32)i / 255.0f, 1.0f / gm) + 0.5f);
        iRenderer.iGamma[i] = (TUint8)Clamp(value, 0, 255);
        }
    SetSpriteGamma(iRenderer.iGamma);
    if (iAssets.iMap) iRenderer.SetMap(iAssets.iMap);
    }

void CGame::CycleMod(TInt aDirection)
    {
    iAssets.ScanModsL();
    TInt count = iAssets.ModCount();
    if (count <= 0)
        {
        iAssets.SetMod(-1);
        iAudio.SetModDirectory(KNullDesC);
        return;
        }
    TInt cur = iAssets.CurrentModIndex();
    TInt next;
    if (cur < 0) next = (aDirection >= 0) ? 0 : (count - 1);
    else
        {
        next = cur + aDirection;
        if (next >= count) next = -1; /* wrap to Default */
        if (next < -1) next = count - 1;
        }
    iAssets.SetMod(next);
    iAudio.SetModDirectory(iAssets.CurrentModPath());
    /* clear model/anim caches by reloading is heavy; for safety, do nothing aggressive */
    }

// ------------------------------------------------------------------ настройки

void CGame::LoadSettings()
    {
    if (!iAssets.iFileServer) return;
    TBuf<192> path;
    SettingsPath(path, iAssets);
    RFile file;
    if (file.Open(*iAssets.iFileServer, path, EFileRead) != KErrNone) return;
    CleanupClosePushL(file);
    TInt size = 0;
    file.Size(size);
    if (size < 30)
        {
        CleanupStack::PopAndDestroy(&file);
        return;
        }
    HBufC8* buffer = HBufC8::NewLC(size);
    TPtr8 ptr = buffer->Des();
    file.Read(ptr, size);
    TBinaryReader in(ptr.Ptr(), ptr.Length());

    TInt version = in.ReadByte();
    if (version < 2 || version > 9)
        {
        CleanupStack::PopAndDestroy(buffer);
        CleanupStack::PopAndDestroy(&file);
        return;
        }
    iSensitivity = in.ReadByte();
    iSensitivityVertical = in.ReadByte();
    iInvertY = in.ReadByte() != 0;
    iLayout = in.ReadByte();
    iDifficulty = in.ReadByte();
    iVolume = in.ReadByte();
    iQuality = in.ReadByte();
    iSoundOn = in.ReadByte() != 0;
    iShowFps = in.ReadByte() != 0;
    iMaxRounds = in.ReadByte();
    iAutoAim = in.ReadByte() != 0;
    iGammaIndex = in.ReadByte();
    iCrossColor = in.ReadByte();
    iCrossSize = in.ReadByte();
    iCrossDynamic = in.ReadByte() != 0;
    iViewBob = in.ReadByte() != 0;
    iRadar = in.ReadByte() != 0;
    iFirstRunDone = in.ReadByte() != 0;
    iWinScale = in.ReadByte();
    iMapIndex = in.ReadByte();
    iTeamChoice = in.ReadByte();
    iBotCount = in.ReadByte();
    iRoundSeconds = in.ReadByte() * 15;
    iBuySeconds = in.ReadByte() * 5;
    iFreezeSeconds = in.ReadByte();
    iStartMoney = ((in.ReadByte() & 255) << 8 | (in.ReadByte() & 255)) * 100;
    iFriendlyFire = in.ReadByte() != 0;
    iFpsCap20 = in.ReadByte() != 0;
    iPcMouse = in.ReadByte() != 0;
    if (version >= 3) iGfx = Clamp((TInt)in.ReadByte(), 0, 3);
    if (version >= 4) iNokiaFrameRate = Clamp((TInt)in.ReadByte(), 0, 5);
    if (version >= 5) iOrientation = Clamp((TInt)in.ReadByte(), 0, 1);
    if (version >= 7)
        {
        iVoiceOn = in.ReadByte() != 0;
        iVoiceVolume = in.ReadByte();
        if (iVoiceVolume < 10 || iVoiceVolume > 100) iVoiceVolume = 100;
        }
    if (version >= 8) iLanguage = Clamp((TInt)in.ReadByte(), 0, 5);
    if (version >= 9) iRenderMode = Clamp((TInt)in.ReadByte(), 0, 2);

    iSensitivity = Clamp(iSensitivity, 1, 10);
    iLanguage = Clamp(iLanguage, 0, 5);
    iRenderMode = Clamp(iRenderMode, 0, 2);
    iSensitivityVertical = Clamp(iSensitivityVertical, 1, 10);
    iDifficulty = Clamp(iDifficulty, 0, 3);
    iQuality = Clamp(iQuality, 0, 3);
    iMaxRounds = Clamp(iMaxRounds, 3, 30);
    iGammaIndex = Clamp(iGammaIndex, 0, 4);
    iCrossColor = Clamp(iCrossColor, 0, 4);
    iCrossSize = Clamp(iCrossSize, 0, 2);
    iWinScale = Clamp(iWinScale, 1, 4);
    iMapIndex = Clamp(iMapIndex, 0, 2);
    iTeamChoice = Clamp(iTeamChoice, 1, 3);
    iBotCount = Clamp(iBotCount, 1, 9);
    iRoundSeconds = Clamp(iRoundSeconds, 60, 300);
    iBuySeconds = Clamp(iBuySeconds, 15, 90);
    iFreezeSeconds = Clamp(iFreezeSeconds, 0, 10);
    iStartMoney = Clamp(iStartMoney, 800, 16000);
    iLayout = Clamp(iLayout, 0, 2);

    const TInt bindCount = in.Remaining() > 0 ? in.ReadByte() : 0;
    if (bindCount > 0 && bindCount <= KGameActions)
        {
        for (TInt i = 0; i < KGameActions; i++)
            {
            TInt value = bindCount > i ? ReadBe32(in.Pointer() + i * 8) : KDefaultBinds[i];
            gState.iBinds[i] = value;
            }
        if (in.Remaining() >= KGameActions * 8 + 4)
            {
            in.Skip(KGameActions * 8);
            TPtrC8 server = in.ReadString8();
            TPtrC8 name = in.ReadString8();
            TPtr serverText(gState.iServer, sizeof(gState.iServer) / sizeof(TText));
            TPtr nameText(gState.iPlayerName, sizeof(gState.iPlayerName) / sizeof(TText));
            CopyAscii8(server, serverText);
            CopyAscii8(name, nameText);
            }
        }
    else
        {
        for (TInt i = 0; i < KGameActions; i++) gState.iBinds[i] = KDefaultBinds[i];
        if (iLayout == 2) iLayout = 0;
        }
    CleanupStack::PopAndDestroy(2, &file);
    }

void CGame::SaveSettings()
    {
    if (!iAssets.iFileServer) return;
    TBuf<192> path;
    SettingsPath(path, iAssets);
    RFile file;
    TInt error = file.Replace(*iAssets.iFileServer, path, EFileWrite);
    if (error != KErrNone)
        error = file.Create(*iAssets.iFileServer, path, EFileWrite);
    if (error != KErrNone) return;
    CleanupClosePushL(file);

    const TInt headerLength = 40;
    TUint8 header[headerLength];
    TInt k = 0;
    header[k++] = 9;
    header[k++] = (TUint8)iSensitivity;
    header[k++] = (TUint8)iSensitivityVertical;
    header[k++] = (TUint8)(iInvertY ? 1 : 0);
    header[k++] = (TUint8)iLayout;
    header[k++] = (TUint8)iDifficulty;
    header[k++] = (TUint8)iVolume;
    header[k++] = (TUint8)iQuality;
    header[k++] = (TUint8)(iSoundOn ? 1 : 0);
    header[k++] = (TUint8)(iShowFps ? 1 : 0);
    header[k++] = (TUint8)iMaxRounds;
    header[k++] = (TUint8)(iAutoAim ? 1 : 0);
    header[k++] = (TUint8)iGammaIndex;
    header[k++] = (TUint8)iCrossColor;
    header[k++] = (TUint8)iCrossSize;
    header[k++] = (TUint8)(iCrossDynamic ? 1 : 0);
    header[k++] = (TUint8)(iViewBob ? 1 : 0);
    header[k++] = (TUint8)(iRadar ? 1 : 0);
    header[k++] = (TUint8)(iFirstRunDone ? 1 : 0);
    header[k++] = (TUint8)iWinScale;
    header[k++] = (TUint8)iMapIndex;
    header[k++] = (TUint8)iTeamChoice;
    header[k++] = (TUint8)iBotCount;
    header[k++] = (TUint8)(iRoundSeconds / 15);
    header[k++] = (TUint8)(iBuySeconds / 5);
    header[k++] = (TUint8)iFreezeSeconds;
    header[k++] = (TUint8)((iStartMoney / 100) >> 8);
    header[k++] = (TUint8)(iStartMoney / 100);
    header[k++] = (TUint8)(iFriendlyFire ? 1 : 0);
    header[k++] = (TUint8)(iFpsCap20 ? 1 : 0);
    header[k++] = (TUint8)(iPcMouse ? 1 : 0);
    header[k++] = (TUint8)iGfx;
    header[k++] = (TUint8)iNokiaFrameRate;
    header[k++] = (TUint8)iOrientation;
    header[k++] = (TUint8)(iVoiceOn ? 1 : 0);
    header[k++] = (TUint8)iVoiceVolume;
    header[k++] = (TUint8)iLanguage;
    header[k++] = (TUint8)iRenderMode;
    file.Write(TPtrC8(header, k));

    TUint8 count = (TUint8)KGameActions;
    file.Write(TPtrC8(&count, 1));
    TUint8 binds[KGameActions * 4];
    for (TInt i = 0; i < KGameActions; i++) WriteBe32(binds + i * 4, gState.iBinds[i]);
    file.Write(TPtrC8(binds, sizeof(binds)));
    file.Write(TPtrC8(binds, sizeof(binds)));

    TUint8 strings[192];
    TInt offset = WriteText8(strings, 0, TPtrC(gState.iServer), 63);
    offset = WriteText8(strings, offset, TPtrC(gState.iPlayerName), 23);
    file.Write(TPtrC8(strings, offset));
    CleanupStack::PopAndDestroy(&file);
    }

// ------------------------------------------------------------------ загрузка карты

void CGame::StartMatch(TInt aMapIndex, TInt aTeam)
    {
    iPendingMap = Clamp(aMapIndex, 0, KMapCount - 1);
    iPendingTeam = Clamp(aTeam, 1, 3);
    if (iMode == 0 && (iPendingMap == 7 || iPendingMap == 8 || iPendingMap == 9 || iPendingMap == 10)) iMode = 4;
    gState.iMode = Clamp(iMode, 0, 4);
    gState.iMapIndex = iPendingMap;
    gState.iPendingTeam = iPendingTeam;
    iState = EStateLoad;
    iMenu = EMenuNone;
    iLoadStage = 0;
    Mem::FillZ(iLoadMessage, sizeof(iLoadMessage));
    TPtr loadMessage(iLoadMessage, sizeof(iLoadMessage) / sizeof(TText));
    loadMessage.Copy(_L("Загрузка "));
    loadMessage.Append(TPtrC(KMapNames[iPendingMap]));
    loadMessage.Append(_L("..."));
    }

void CGame::LoadMapL(const TDesC& aName)
    {
    CMap* map = iAssets.LoadMapL(aName);
    iRenderer.SetMap(map);

    TBuf<192> navPath;
    NavPath(navPath, iAssets, aName);
    gNav->LoadL(*iAssets.iFileServer, navPath);

    iMode = gState.iMode;
    iRoundTimeSetting = iRoundSeconds;
    iBuyTimeSetting = (TReal32)iBuySeconds;
    iFreezeSetting = (TReal32)iFreezeSeconds;
    gState.iRoundTime = iRoundTimeSetting;
    gState.iStartMoney = iStartMoney;
    gState.iBuyTime = iBuyTimeSetting;
    gState.iFreezeTime = iFreezeSetting;
    gState.iFriendlyFire = iFriendlyFire ? 1 : 0;
    if (iMode == 1)
        {
        gState.iRoundTime = 600;
        gState.iFreezeTime = 2;
        gState.iBuyTime = 600;
        gState.iStartMoney = 16000;
        iMaxRounds = 1;
        gState.iFriendlyFire = 0;
        }
    else if (iMode == 2)
        {
        gState.iRoundTime = 300;
        gState.iFreezeTime = 2;
        gState.iBuyTime = 45;
        gState.iStartMoney = 4000;
        iMaxRounds = 10;
        gState.iFriendlyFire = 0;
        }

    SetupEntities();
    SetupPlayers(iPendingTeam);
    CopyText(iMapName, sizeof(iMapName) / sizeof(TText), aName);
    iMapIndex = iPendingMap;
    iRound = 0;
    iScoreT = 0;
    iScoreCT = 0;
    gState.iLossStreakT = 0;
    gState.iLossStreakCT = 0;
    iTime = 0;
    NewRound(ETrue);
    iState = EStatePlay;
    iMenu = EMenuNone;
    if (gState.iMpPhase == 1)
        {
        gState.iMpPhase = 2;
        gState.iRosterRequested = EFalse;
        if (iNetworkHost)
            {
            gState.iMyNetId = gState.iMyNetId > 0 ? gState.iMyNetId : 1;
            iPlayers[iMyIndex].iNetId = gState.iMyNetId;
            }
        else
            {
            for (TInt i = 0; i < iPlayerCount; i++)
                {
                iPlayers[i].iBot = EFalse;
                Ex(iPlayers[i]).iRemote = ETrue;
                }
            }
        FlushPendingJoins(gState, *this);
        }
    }

void CGame::SetupPlayers(TInt aHumanTeam)
    {
    const TInt total = Clamp(iBotCount + 1, 2, KMaxPlayers);
    const TInt mine = (total + 1) / 2;
    iPlayerCount = total;
    Mem::FillZ(iPlayers, sizeof(iPlayers));
    for (TInt i = 0; i < KMaxPlayers; i++)
        Mem::FillZ(&gState.iExtra[i], sizeof(TExtraPlayer));

    TInt order[30];
    const TInt names = sizeof(KBotNames) / sizeof(KBotNames[0]);
    for (TInt i = 0; i < names; i++) order[i] = i;
    for (TInt i = names - 1; i > 0; i--)
        {
        const TInt j = iRandom.NextInt(i + 1);
        const TInt swap = order[i];
        order[i] = order[j];
        order[j] = swap;
        }
    TInt nameIndex = 0;
    for (TInt i = 0; i < total; i++)
        {
        TPlayer& p = iPlayers[i];
        p.iId = i;
        p.iTeam = i == 0 ? aHumanTeam : (i < mine ? aHumanTeam : 3 - aHumanTeam);
        p.iBot = i == 0 ? EFalse : ETrue;
        TPtr name(p.iName, sizeof(p.iName) / sizeof(TText));
        if (i == 0)
            {
            name.Copy(gState.iPlayerName[0] ? TPtrC(gState.iPlayerName) : _L("Игрок"));
            iMyIndex = 0;
            }
        else
            {
            SetAscii(name, KBotNames[order[nameIndex++ % names]]);
            gState.iExtra[i].iSniper = (i % 5) == 3;
            }
        p.iMoney = gState.iStartMoney;
        p.iSecondary = p.iTeam == 1 ? 1 : 2;
        p.iClip[p.iSecondary] = KWpClip[p.iSecondary];
        p.iAmmo[p.iSecondary] = p.iTeam == 1 ? 40 : 24;
        p.iWeapon = p.iSecondary;
        p.iPrimary = -1;
        p.iNextShot = 0;
        p.iDrawEnd = 0;
        p.iModelId = RandomRange(iRandom, 0, 3);
        p.iRandomSeed = (TInt)iRandom.Next();
        }
    }

void CGame::SetupEntities()
    {
    CMap* map = iAssets.iMap;
    gState.iSpawnTCount = 0;
    gState.iSpawnCTCount = 0;
    gState.iSiteCount = 0;
    gState.iBuyCount = 0;
    gState.iBreakCount = 0;
    gState.iDrawCount = 0;
    gState.iLadderCount = 0;
    if (!map) return;

    for (TInt e = 0; e < map->numEntities; e++)
        {
        const CMap::SEntity& entity = map->entities[e];
        const TAsciiName* className = map->EntityValue(e, Ascii("classname"));
        if (!className) continue;
        const TPtrC8 classValue = className->Des();
        const TAsciiName* originValue = map->EntityValue(e, Ascii("origin"));
        const TAsciiName* modelValue = map->EntityValue(e, Ascii("model"));
        TReal32 origin[3] = { 0, 0, 0 };
        if (originValue) ParseVector(originValue->Des(), origin, 3);
        TReal32 yaw = 0;
        if (ClassIs(classValue, "info_player_deathmatch") || ClassIs(classValue, "info_player_start"))
            {
            const TAsciiName* angles = map->EntityValue(e, Ascii("angles"));
            if (angles)
                {
                TReal32 parsed[3] = { 0, 0, 0 };
                ParseVector(angles->Des(), parsed, 3);
                yaw = parsed[1];
                }
            TReal32* destination = ClassIs(classValue, "info_player_deathmatch")
                ? gState.iSpawnT : gState.iSpawnCT;
            TInt& count = ClassIs(classValue, "info_player_deathmatch")
                ? gState.iSpawnTCount : gState.iSpawnCTCount;
            if (count < KMaxSpawns)
                {
                destination[count * 4] = origin[0];
                destination[count * 4 + 1] = origin[1];
                destination[count * 4 + 2] = origin[2];
                destination[count * 4 + 3] = yaw;
                count++;
                }
            }
        TInt model = modelValue ? ModelIndex(modelValue->Des()) : -1;
        if (model < 0 || model >= map->numModels) continue;
        TBool solid = EFalse;
        if (ClassIs(classValue, "func_wall") || ClassIs(classValue, "func_illusionary")
            || ClassIs(classValue, "func_breakable") || ClassIs(classValue, "func_door")
            || ClassIs(classValue, "func_wall_toggle"))
            {
            if (gState.iDrawCount < KMaxBreakables) gState.iDrawModel[gState.iDrawCount++] = model;
            solid = ETrue;
            }
        if (solid && gState.iBreakCount < KMaxBreakables
            && (ClassIs(classValue, "func_breakable") || ClassIs(classValue, "func_door")
                || ClassIs(classValue, "func_wall_toggle")))
            gState.iBreakModel[gState.iBreakCount++] = model;
        if (ClassIs(classValue, "func_bomb_target") && gState.iSiteCount < KMaxSites)
            gState.iSiteModel[gState.iSiteCount++] = model;
        if (ClassIs(classValue, "func_buyzone") && gState.iBuyCount < KMaxBuyZones)
            {
            gState.iBuyModel[gState.iBuyCount] = model;
            TInt team = 0;
            for (TInt k = 0; k < gState.iSpawnTCount; k++)
                if (InBrushModel(*map, model, gState.iSpawnT[k * 4], gState.iSpawnT[k * 4 + 1],
                                 gState.iSpawnT[k * 4 + 2], 40)) team = 1;
            for (TInt k = 0; k < gState.iSpawnCTCount; k++)
                if (InBrushModel(*map, model, gState.iSpawnCT[k * 4], gState.iSpawnCT[k * 4 + 1],
                                 gState.iSpawnCT[k * 4 + 2], 40)) team = 2;
            gState.iBuyTeam[gState.iBuyCount++] = team;
            }
        if (ClassIs(classValue, "func_ladder") && gState.iLadderCount < KMaxLadders)
            gState.iLadders[gState.iLadderCount++] = model;
        (void)entity;
        }

    if (gState.iSpawnTCount == 0 && gState.iSpawnCTCount > 0)
        {
        for (TInt i = 0; i < gState.iSpawnCTCount && gState.iSpawnTCount < KMaxSpawns; i++)
            {
            const TInt at = gState.iSpawnTCount * 4;
            const TInt from = i * 4;
            gState.iSpawnT[at] = gState.iSpawnCT[from];
            gState.iSpawnT[at + 1] = gState.iSpawnCT[from + 1];
            gState.iSpawnT[at + 2] = gState.iSpawnCT[from + 2];
            gState.iSpawnT[at + 3] = gState.iSpawnCT[from + 3] + 180;
            gState.iSpawnTCount++;
            }
        }
    if (gState.iSpawnCTCount == 0 && gState.iSpawnTCount > 0)
        {
        for (TInt i = 0; i < gState.iSpawnTCount && gState.iSpawnCTCount < KMaxSpawns; i++)
            {
            const TInt at = gState.iSpawnCTCount * 4;
            const TInt from = i * 4;
            gState.iSpawnCT[at] = gState.iSpawnT[from];
            gState.iSpawnCT[at + 1] = gState.iSpawnT[from + 1];
            gState.iSpawnCT[at + 2] = gState.iSpawnT[from + 2];
            gState.iSpawnCT[at + 3] = gState.iSpawnT[from + 3] + 180;
            gState.iSpawnCTCount++;
            }
        }
    }

static TReal32 AngleNorm(TReal32 aAngle)
    {
    while (aAngle > 180.0f) aAngle -= 360.0f;
    while (aAngle < -180.0f) aAngle += 360.0f;
    return aAngle;
    }

static TInt TeamColor(TInt aTeam)
    { return aTeam == 1 ? 0xFF4040 : 0x99CCFF; }

static TInt ClampByte(TInt aValue)
    { return Clamp(aValue, 0, 255); }

static TPtrC8 SoundName(const char* aName)
    { return TPtrC8(reinterpret_cast<const TUint8*>(aName), (TInt)strlen(aName)); }

static TBool HasWeapon(const TPlayer& aPlayer, TInt aWeapon)
    {
    if (aWeapon < 0 || aWeapon >= KWeaponCount) return EFalse;
    if (aWeapon == 0) return ETrue;
    if (aWeapon == 28) return aPlayer.iHasC4;
    if (KWpSlot[aWeapon] == 0) return aPlayer.iPrimary == aWeapon;
    if (KWpSlot[aWeapon] == 1) return aPlayer.iSecondary == aWeapon;
    if (aWeapon == 25) return Ex(aPlayer).iHe > 0;
    if (aWeapon == 26) return Ex(aPlayer).iFlash > 0;
    if (aWeapon == 27) return Ex(aPlayer).iSmoke > 0;
    return EFalse;
    }

static void ResetPlayerForRound(TPlayer& aPlayer, TExtraPlayer& aExtra, CMap* aMap,
                                TReal32 aTime, TBool aFirst, TInt aMode)
    {
    aExtra.iForward = 0;
    aExtra.iStrafe = 0;
    aExtra.iJump = EFalse;
    aExtra.iDuck = EFalse;
    aExtra.iFire = EFalse;
    aExtra.iFire2 = EFalse;
    aExtra.iUse = EFalse;
    aExtra.iFirePrev = EFalse;
    aExtra.iUsePrev = EFalse;
    aExtra.iFire2Prev = EFalse;
    aExtra.iNextAttack = 0;
    aExtra.iReloadEnd = 0;
    aExtra.iDrawEnd = aTime + 0.5f;
    aExtra.iShellNext = 0;
    aExtra.iRecoil = 0;
    aExtra.iJumpPenalty = 0;
    aExtra.iPunchPitch = 0;
    aExtra.iPunchYaw = 0;
    aExtra.iShotsFired = 0;
    aExtra.iSilM4 = EFalse;
    aExtra.iSilUSP = EFalse;
    aExtra.iFlashStart = 0;
    aExtra.iFlashUntil = 0;
    aExtra.iLastHurt = -100;
    aExtra.iLastHurtYaw = 0;
    aExtra.iStepTime = 0;
    aExtra.iLastAttacker = -1;
    aExtra.iOnLadder = EFalse;
    aExtra.iJumpHeld = EFalse;
    aExtra.iDefuseProgress = 0;
    aExtra.iDefusing = EFalse;
    aExtra.iDrawTime = 0;
    aExtra.iVmSet = VmSetFor(aPlayer);
    aExtra.iVmAnimation = 0;
    aExtra.iVmTime = aTime;
    aExtra.iVmLength = 1.0f;
    CopyAsciiToText(aExtra.iVmName, 32, "idle");
    aExtra.iPathLength = 0;
    aExtra.iPathIndex = 0;
    aExtra.iEnemy = -1;
    aExtra.iIgnore = -1;
    aExtra.iSeen = EFalse;
    aExtra.iSpotted = EFalse;
    aExtra.iBought = EFalse;
    aExtra.iFlashedUntil = 0;
    aExtra.iHeardAt = -100;
    aExtra.iNextPerceive = aTime;
    aExtra.iNextItemCheck = 0;
    aExtra.iHoldUntil = 0;
    aExtra.iLastX = aPlayer.iX;
    aExtra.iLastY = aPlayer.iY;

    const TBool wasAlive = aPlayer.iAlive;
    aPlayer.iAlive = ETrue;
    aPlayer.iVelocityX = 0;
    aPlayer.iVelocityY = 0;
    aPlayer.iVelocityZ = 0;
    aPlayer.iPitch = 0;
    aPlayer.iHealth = 100;
    aPlayer.iDucked = EFalse;
    aPlayer.iOnGround = ETrue;
    aPlayer.iZoom = 0;
    aPlayer.iScope = EFalse;
    aPlayer.iReloading = EFalse;
    aPlayer.iLastShot = -100;
    aPlayer.iDeathTime = 0;
    aPlayer.iFlashTime = 0;
    aPlayer.iAnimTime = 0;
    aPlayer.iGait = 0;
    aPlayer.iRadio = 0;
    aPlayer.iLastRadio = -100;
    aPlayer.iHasC4 = EFalse;
    aPlayer.iPlanting = EFalse;
    aPlayer.iPlantProgress = 0;
    aPlayer.iBotThinkTime = 0;
    aPlayer.iBotAimX = 0;
    aPlayer.iBotAimY = 0;
    aPlayer.iSpeedFactor = 0;
    aPlayer.iViewBob = 0;
    aPlayer.iLastShotSilent = EFalse;
    if (aMode == 1)
        {
        aPlayer.iMoney = 16000;
        aPlayer.iArmor = 100;
        aExtra.iHelmet = ETrue;
        }
    else if (aMode == 2)
        aPlayer.iMoney = 16000;
    if (!aFirst && aPlayer.iMoney < 0) aPlayer.iMoney = 0;
    if (aFirst || !wasAlive)
        {
        aPlayer.iPrimary = -1;
        aExtra.iHe = 0;
        aExtra.iFlash = 0;
        aExtra.iSmoke = 0;
        aPlayer.iArmor = 0;
        aExtra.iHelmet = EFalse;
        aExtra.iDefuser = EFalse;
        }
    if (aFirst)
        {
        aPlayer.iDeaths = 0;
        aPlayer.iKills = 0;
        if (aPlayer.iTeam == 1 && aPlayer.iSecondary == 2) aPlayer.iSecondary = 1;
        }
    if (aPlayer.iSecondary < 0) aPlayer.iSecondary = aPlayer.iTeam == 1 ? 1 : 2;
    aPlayer.iWeapon = aPlayer.iPrimary >= 0 ? aPlayer.iPrimary : aPlayer.iSecondary;
    (void)aMap;
    }

static void AddRadioLater(TMatchState& aState, TInt aPlayerId, TInt aCommand, TReal32 aAt)
    {
    for (TInt i = 0; i < KMaxRadioQueue; i++)
        {
        if (aState.iRadioQueuePlayer[i] == 0)
            {
            aState.iRadioQueuePlayer[i] = aPlayerId + 1;
            aState.iRadioQueueCmd[i] = aCommand;
            aState.iRadioQueueAt[i] = aAt;
            return;
            }
        }
    }

// ------------------------------------------------------------------ раунды

void CGame::NewRound(TBool aFirst)
    {
    iRound++;
    iRoundOver = EFalse;
    iRoundEndAt = -1;
    iBombPlanted = EFalse;
    iBombDropped = EFalse;
    iBombExploded = EFalse;
    iBombSite = -1;
    iBombDroppedBy = -1;
    iBombTime = 0;
    iBombBeep = 0;
    iInfected = EFalse;
    iInfectAt = iTime + 14.0f;
    iTermCircleActive = (iMode == 3);
    iTerminatorIdx = -1; iTerminatorClass = 0;
    iDisguiseTimer = 0; iDisguiseCooldown = 0; iIsDisguised = EFalse;
    // place circle near map center
    if (iTermCircleActive)
        {
        iTermCircleX = 0; iTermCircleY = 0; iTermCircleZ = 0;
        TInt best = -1; TReal32 bestD = -1.0f;
        if (gNav && gNav->iCount > 0 && iAssets.iMap)
            {
            for (TInt a = 0; a < gNav->iCount; a++)
                {
                const TReal32 x = gNav->iCX[a], y = gNav->iCY[a], z = gNav->ZAt(a, x, y);
                iAssets.iMap->Trace(0, x, y, z + 30.0f, x, y, z - 50.0f);
                if (iAssets.iMap->rStartSolid) continue;
                TReal32 nearest = 1e12f;
                for (TInt j = 0; j < iPlayerCount; j++)
                    {
                    if (!iPlayers[j].iAlive) continue;
                    const TReal32 dx = x - iPlayers[j].iX, dy = y - iPlayers[j].iY;
                    const TReal32 dd = dx*dx + dy*dy;
                    if (dd < nearest) nearest = dd;
                    }
                if (nearest > bestD) { bestD = nearest; best = a; }
                }
            }
        if (best >= 0) { iTermCircleX = gNav->iCX[best]; iTermCircleY = gNav->iCY[best]; iTermCircleZ = gNav->ZAt(best, gNav->iCX[best], gNav->iCY[best]); }
        else if (gState.iSpawnCTCount > 0) { iTermCircleX = gState.iSpawnCT[0]; iTermCircleY = gState.iSpawnCT[1]; iTermCircleZ = gState.iSpawnCT[2]; }
        iTermCircleZ = GroundHeight(iTermCircleX, iTermCircleY, iTermCircleZ + 100.0f);
        }
    // Hostage Rescue init
    for (TInt h = 0; h < KMaxHostages; h++) { iHostageAlive[h] = EFalse; iHostageRescued[h] = EFalse; iHostageFollowIdx[h] = -1; iHostageX[h]=0; iHostageY[h]=0; iHostageZ[h]=0; }
    if (iMode == 4)
        {
        // spawn 4 hostages near T side (simple positions)
        const TReal32 baseX = -300.0f, baseY = -300.0f;
        for (TInt h = 0; h < 4 && h < KMaxHostages; h++)
            {
            iHostageX[h] = baseX + h * 60.0f;
            iHostageY[h] = baseY + (h % 2) * 60.0f;
            iHostageZ[h] = GroundHeight(iHostageX[h], iHostageY[h], 0);
            iHostageAlive[h] = ETrue; iHostageRescued[h] = EFalse; iHostageFollowIdx[h] = -1;
            }
        // rescue zone near CT side
        iRescueZoneX = 300.0f; iRescueZoneY = 300.0f; iRescueZoneZ = GroundHeight(300.0f,300.0f,0);
        }
    for (TInt i = 0; i < KMaxGrenades; i++) iGrenades[i].iActive = EFalse;
    for (TInt i = 0; i < KMaxEffects; i++) iEffects[i].iActive = EFalse;
    for (TInt i = 0; i < gState.iSmokeCount; i++) gState.iSmokeOn[i] = EFalse;
    for (TInt i = 0; i < KMaxDrops; i++)
        {
        iDrops[i].iActive = EFalse;
        gDropExtra[i].iRest = EFalse;
        }
    CMap* map = iAssets.iMap;
    if (map && map->modelActive)
        {
        for (TInt i = 0; i < map->numModels; i++) map->modelActive[i] = ETrue;
        }
    for (TInt i = 0; i < KMaxChatLines; i++) iKillFeedTime[i] = -100;
    iRoundStart = iTime;
    iFreezeEnd = iTime + gState.iFreezeTime;
    iBuyEnd = iFreezeEnd + gState.iBuyTime;
    TInt spawnIndex[2] = { 0, 0 };
    const TInt offT = iRandom.Next() & 0xFFFF;
    const TInt offC = iRandom.Next() & 0xFFFF;
    const TInt countT = gState.iSpawnTCount;
    const TInt countC = gState.iSpawnCTCount;
    if (iMode == 2)
        {
        for (TInt i = 0; i < iPlayerCount; i++)
            {
            TPlayer& p = iPlayers[i];
            TExtraPlayer& x = Ex(p);
            if (x.iZombie) { x.iZombie = EFalse; p.iAlive = EFalse; }
            p.iTeam = 2;
            x.iZombie = EFalse;
            }
        }
    for (TInt i = 0; i < iPlayerCount; i++)
        {
        TPlayer& p = iPlayers[i];
        TExtraPlayer& x = Ex(p);
        ResetPlayerForRound(p, x, map, iTime, aFirst, iMode);
        const TInt team = p.iTeam == 1 ? 0 : 1;
        const TInt count = team == 0 ? countT : countC;
        if (count > 0)
            {
            const TReal32* spawn = team == 0 ? gState.iSpawnT : gState.iSpawnCT;
            const TInt offset = team == 0 ? offT : offC;
            const TInt k = (spawnIndex[team]++ + offset) % count;
            p.iX = spawn[k * 4];
            p.iY = spawn[k * 4 + 1];
            p.iZ = spawn[k * 4 + 2] + 1;
            p.iYaw = spawn[k * 4 + 3];
            }
        if (map)
            {
            map->Trace(1, p.iX, p.iY, p.iZ, p.iX, p.iY, p.iZ - 64);
            if (!map->rStartSolid) p.iZ = map->rEz;
            }
        p.iOnGround = ETrue;
        p.iWeapon = p.iPrimary >= 0 ? p.iPrimary : (p.iSecondary >= 0 ? p.iSecondary : 0);
        p.iDrawEnd = iTime + KWpDrawT[p.iWeapon];
        x.iSpawnSeq++;
        x.iPathLength = 0;
        x.iPathIndex = 0;
        x.iGoal = -1;
        x.iRepathAt = 0;
        x.iTask = 0;
        x.iEnemy = -1;
        x.iIgnore = -1;
        x.iHoldUntil = 0;
        x.iReactAt = 0;
        x.iLastSeen = -100;
        x.iSeen = EFalse;
        x.iSpotted = EFalse;
        x.iBought = EFalse;
        x.iHasLook = EFalse;
        x.iFlashedUntil = 0;
        x.iHeardAt = -100;
        x.iLeader = -1;
        x.iFollowUntil = 0;
        x.iHoldHereUntil = 0;
        x.iIgnoreUntil = 0;
        x.iNextItemCheck = 0;
        x.iSite = gState.iSiteCount > 0 ? (TInt)(iRandom.Next() % (TUint32)gState.iSiteCount) : 0;
        if (p.iTeam == 2 && gState.iSiteCount > 1) x.iSite = p.iId % 2;
        if (p.iTeam == 1 && gState.iSiteCount > 0 && (iRandom.Next() & 3) != 0)
            x.iSite = (iRound * 7 + 3) % gState.iSiteCount;
        x.iNextPerceive = iTime + iRandom.NextReal() * 0.3f;
        }
    gState.iInfected = EFalse;
    gState.iInfectAt = iInfectAt;
    if (iMode == 0)
        {
        TInt terrorists = 0;
        for (TInt i = 0; i < iPlayerCount; i++) if (iPlayers[i].iTeam == 1) terrorists++;
        if (terrorists > 0)
            {
            const TInt pick = (iRandom.Next() & 0xFFFF) % terrorists;
            TInt seen = 0;
            for (TInt i = 0; i < iPlayerCount; i++)
                {
                if (iPlayers[i].iTeam != 1) continue;
                if (seen++ == pick) { iPlayers[i].iHasC4 = ETrue; break; }
                }
            }
        }
    iSpectatorIndex = -1;
    iBuyOpen = EFalse;
    iScoreOpen = EFalse;
    TBuf<96> message;
    if (iMode == 1) message.Copy(_L("Дезматч: 10 минут, возрождение через 3 с"));
    else if (iMode == 2) message.Copy(_L("Раунд начался: через 14 с один станет зомби. Продержитесь 5 минут!"));
    else
        {
        message.Copy(_L("Раунд "));
        message.AppendNum(iRound);
        }
    Center(message, 3.0f, 0xFFFFFF);
    if (!aFirst) PlaySound(Ascii("radio_moveout"), 70);
    for (TInt i = 1; i < iPlayerCount; i++)
        {
        if (iPlayers[i].iTeam != iPlayers[iMyIndex].iTeam) continue;
        if ((iRandom.Next() & 3) != 0) continue;
        AddRadioLater(gState, iPlayers[i].iId, (iRandom.Next() & 1) != 0 ? 6 : 8, iFreezeEnd + 0.5f);
        break;
        }
    }

void CGame::EndRound(TInt aWinner, const TDesC& aMessage, const TPtrC8& aSound, TInt aReason)
    {
    if (iRoundOver) return;
    iRoundOver = ETrue;
    iRoundEndAt = iTime + 5.0f;
    if (aWinner == 1) iScoreT++;
    else if (aWinner == 2) iScoreCT++;
    Center(aMessage, 5.0f, aWinner == 1 ? 0xFF6040 : 0x60A0FF);
    PlaySound(aSound, 90);
    const TInt winBonus = aReason == 1 ? 3500 : 3250;
    for (TInt i = 0; i < iPlayerCount; i++)
        {
        TPlayer& p = iPlayers[i];
        if (p.iTeam == aWinner) AddMoney(p, winBonus);
        else
            {
            const TInt streak = aWinner == 1 ? gState.iLossStreakCT : gState.iLossStreakT;
            AddMoney(p, 1400 + MinI(streak, 4) * 500);
            }
        }
    if (aWinner == 1) { gState.iLossStreakCT++; gState.iLossStreakT = 0; }
    else if (aWinner == 2) { gState.iLossStreakT++; gState.iLossStreakCT = 0; }
    }

void CGame::AddMoney(TPlayer& aPlayer, TInt aValue)
    {
    aPlayer.iMoney += aValue;
    aPlayer.iMoney = Clamp(aPlayer.iMoney, 0, 16000);
    }

TBool CGame::AnyAlive(TInt aTeam) const
    {
    for (TInt i = 0; i < iPlayerCount; i++)
        if (iPlayers[i].iTeam == aTeam && iPlayers[i].iAlive) return ETrue;
    return EFalse;
    }

TInt CGame::CountAlive(TInt aTeam) const
    {
    TInt count = 0;
    for (TInt i = 0; i < iPlayerCount; i++)
        if (iPlayers[i].iTeam == aTeam && iPlayers[i].iAlive) count++;
    return count;
    }

TPlayer* CGame::PlayerById(TInt aId)
    {
    for (TInt i = 0; i < iPlayerCount; i++) if (iPlayers[i].iId == aId) return &iPlayers[i];
    return NULL;
    }

TReal32 CGame::GroundHeight(TReal32 aX, TReal32 aY, TReal32 aZ) const
    {
    CMap* map = iAssets.iMap;
    if (!map) return aZ;
    map->Trace(0, aX, aY, aZ, aX, aY, aZ - 512);
    return map->rEz;
    }

void CGame::CheckRoundEnd()
    {
    if (iRoundOver) return;
    const TInt aliveT = CountAlive(1), aliveCT = CountAlive(2);
    TInt totalT = 0, totalCT = 0;
    for (TInt i = 0; i < iPlayerCount; i++)
        {
        if (iPlayers[i].iTeam == 1) totalT++;
        else totalCT++;
        }
    if (iMode == 1)
        {
        if (iTime > iFreezeEnd + gState.iRoundTime)
            {
            const TInt winner = iScoreT > iScoreCT ? 1 : (iScoreCT > iScoreT ? 2 : 0);
            iRoundOver = ETrue;
            iRoundEndAt = iTime + 5.0f;
            if (winner == 0) Center(_L("Ничья!"), 5.0f, 0xFFE080);
            else
                {
                TBuf<48> text;
                text.Copy(winner == 1 ? TPtrC(_L("Террористы")) : TPtrC(_L("Спецназ")));
                text.Append(_L(" победили в дезматче!"));
                Center(text, 5.0f, 0xFFE080);
                }
            PlaySound(Ascii(winner == 1 ? "radio_terwin" : "radio_ctwin"), 90);
            }
        return;
        }
    if (iMode == 2)
        {
        if (!iInfected) return;
        if (aliveCT == 0) EndRound(1, _L("Зомби победили!"), Ascii("radio_terwin"), 0);
        else if (iTime > iFreezeEnd + gState.iRoundTime)
            EndRound(2, _L("Люди продержались 5 минут и победили!"), Ascii("radio_ctwin"), 0);
        return;
        }
    if (iMode == 3)
        {
        const TInt termIdx = (iTerminatorIdx >= 0 && iTerminatorIdx < iPlayerCount) ? iTerminatorIdx : -1;
        if (termIdx < 0) return;   // wait until somebody enters the circle
        if (!iPlayers[termIdx].iAlive) { EndRound(2, _L("Terminator killed! Humans win!"), Ascii("radio_ctwin"), 0); return; }
        TInt aliveHumans = 0;
        for (TInt i = 0; i < iPlayerCount; i++) if (i != termIdx && iPlayers[i].iAlive) aliveHumans++;
        if (termIdx >= 0 && aliveHumans == 0) { EndRound(1, _L("Terminator wins!"), Ascii("radio_terwin"), 0); return; }
        if (iTime > iFreezeEnd + gState.iRoundTime) { EndRound(1, _L("Time up! Terminator wins!"), Ascii("radio_terwin"), 0); return; }
        return;
        }
    if (iMode == 4)
        {
        TInt rescued = 0;
        for (TInt h = 0; h < KMaxHostages; h++) if (iHostageRescued[h]) rescued++;
        if (rescued >= 4 || (rescued >= 1 && CountAlive(2) > 0 && CountAlive(1) == 0)) { EndRound(2, _L("Hostages rescued! CT win!"), Ascii("radio_ctwin"), 0); return; }
        if (aliveCT == 0 && totalCT > 0) { EndRound(1, _L("Terrorists win!"), Ascii("radio_terwin"), 0); return; }
        if (iTime > iFreezeEnd + gState.iRoundTime) { EndRound(1, _L("Time up! Terrorists win!"), Ascii("radio_terwin"), 0); return; }
        return;
        }
    if (iBombExploded)
        {
        EndRound(1, _L("Цель уничтожена! Террористы победили"), Ascii("radio_terwin"), 1);
        return;
        }
    if (aliveCT == 0 && totalCT > 0)
        {
        EndRound(1, _L("Террористы победили!"), Ascii("radio_terwin"), 0);
        return;
        }
    if (aliveT == 0 && totalT > 0 && !iBombPlanted)
        {
        EndRound(2, _L("Спецназ победил!"), Ascii("radio_ctwin"), 0);
        return;
        }
    if (!iBombPlanted && iTime > iFreezeEnd + gState.iRoundTime)
        EndRound(2, _L("Время вышло. Спецназ победил!"), Ascii("radio_ctwin"), 0);
    }

static TBool TestPosition(CMap* aMap, TReal32 aX, TReal32 aY, TReal32 aZ, TInt aHull)
    {
    if (!aMap) return ETrue;
    aMap->Trace(aHull, aX, aY, aZ, aX, aY, aZ);
    return !aMap->rStartSolid;
    }

static TInt PlayerHull(const TPlayer& aPlayer)
    { return aPlayer.iDucked ? 3 : 1; }

static void CategorizePlayer(CMap* aMap, TPlayer& aPlayer)
    {
    if (!aMap) return;
    if (aPlayer.iVelocityZ > 180.0f) { aPlayer.iOnGround = EFalse; return; }
    const TInt hull = PlayerHull(aPlayer);
    aMap->Trace(hull, aPlayer.iX, aPlayer.iY, aPlayer.iZ,
                aPlayer.iX, aPlayer.iY, aPlayer.iZ - 2.0f);
    if (aMap->rFrac < 1.0f && aMap->rNz >= 0.7f && !aMap->rAllSolid)
        {
        aPlayer.iOnGround = ETrue;
        if (!aMap->rStartSolid)
            {
            aPlayer.iX = aMap->rEx;
            aPlayer.iY = aMap->rEy;
            aPlayer.iZ = aMap->rEz;
            }
        }
    else aPlayer.iOnGround = EFalse;
    }

static void FlyMove(CMap* aMap, TPlayer& aPlayer, TReal32 aDelta)
    {
    if (!aMap) return;
    const TInt hull = PlayerHull(aPlayer);
    TInt planes = 0;
    TReal32 originalX = aPlayer.iVelocityX;
    TReal32 originalY = aPlayer.iVelocityY;
    TReal32 originalZ = aPlayer.iVelocityZ;
    TReal32 left = aDelta;
    for (TInt bump = 0; bump < 4; bump++)
        {
        if (aPlayer.iVelocityX == 0 && aPlayer.iVelocityY == 0 && aPlayer.iVelocityZ == 0) break;
        aMap->Trace(hull, aPlayer.iX, aPlayer.iY, aPlayer.iZ,
                    aPlayer.iX + aPlayer.iVelocityX * left,
                    aPlayer.iY + aPlayer.iVelocityY * left,
                    aPlayer.iZ + aPlayer.iVelocityZ * left);
        if (aMap->rAllSolid)
            {
            aPlayer.iVelocityX = 0;
            aPlayer.iVelocityY = 0;
            aPlayer.iVelocityZ = 0;
            return;
            }
        if (aMap->rFrac > 0)
            {
            aPlayer.iX = aMap->rEx;
            aPlayer.iY = aMap->rEy;
            aPlayer.iZ = aMap->rEz;
            planes = 0;
            originalX = aPlayer.iVelocityX;
            originalY = aPlayer.iVelocityY;
            originalZ = aPlayer.iVelocityZ;
            }
        if (aMap->rFrac == 1.0f) break;
        left -= left * aMap->rFrac;
        if (planes >= 5) { aPlayer.iVelocityX = 0; aPlayer.iVelocityY = 0; aPlayer.iVelocityZ = 0; break; }
        gTempVectors[planes * 3] = aMap->rNx;
        gTempVectors[planes * 3 + 1] = aMap->rNy;
        gTempVectors[planes * 3 + 2] = aMap->rNz;
        planes++;
        TInt kept = planes;
        for (TInt i = 0; i < planes; i++)
            {
            const TReal32 nx = gTempVectors[i * 3], ny = gTempVectors[i * 3 + 1], nz = gTempVectors[i * 3 + 2];
            const TReal32 dot = originalX * nx + originalY * ny + originalZ * nz;
            aPlayer.iVelocityX = originalX - nx * dot;
            aPlayer.iVelocityY = originalY - ny * dot;
            aPlayer.iVelocityZ = originalZ - nz * dot;
            if (AbsF(aPlayer.iVelocityX) < 0.01f) aPlayer.iVelocityX = 0;
            if (AbsF(aPlayer.iVelocityY) < 0.01f) aPlayer.iVelocityY = 0;
            if (AbsF(aPlayer.iVelocityZ) < 0.01f) aPlayer.iVelocityZ = 0;
            TInt j = 0;
            for (; j < planes; j++)
                {
                if (j == i) continue;
                const TReal32 px = gTempVectors[j * 3], py = gTempVectors[j * 3 + 1], pz = gTempVectors[j * 3 + 2];
                if (aPlayer.iVelocityX * px + aPlayer.iVelocityY * py + aPlayer.iVelocityZ * pz < 0) break;
                }
            if (j == planes) { kept = i; break; }
            }
        if (kept == planes)
            {
            if (planes != 2)
                {
                aPlayer.iVelocityX = 0;
                aPlayer.iVelocityY = 0;
                aPlayer.iVelocityZ = 0;
                break;
                }
            const TReal32 ax = gTempVectors[1], ay = gTempVectors[2], az = gTempVectors[5];
            const TReal32 bx = gTempVectors[3], by = gTempVectors[4], bz = gTempVectors[6];
            TReal32 cx = ay * bz - az * by, cy = az * bx - ax * bz, cz = ax * by - ay * bx;
            const TReal32 length = FloatSqrt(cx * cx + cy * cy + cz * cz);
            if (length > 0.0001f) { cx /= length; cy /= length; cz /= length; }
            const TReal32 d = cx * aPlayer.iVelocityX + cy * aPlayer.iVelocityY + cz * aPlayer.iVelocityZ;
            aPlayer.iVelocityX = cx * d;
            aPlayer.iVelocityY = cy * d;
            aPlayer.iVelocityZ = cz * d;
            }
        if (aPlayer.iVelocityX * originalX + aPlayer.iVelocityY * originalY + aPlayer.iVelocityZ * originalZ <= 0)
            {
            aPlayer.iVelocityX = 0;
            aPlayer.iVelocityY = 0;
            aPlayer.iVelocityZ = 0;
            break;
            }
        }
    }

static TBool SmokeBlocks(const TMatchState& aState, TReal32 aTime,
                         TReal32 aX1, TReal32 aY1, TReal32 aZ1,
                         TReal32 aX2, TReal32 aY2, TReal32 aZ2)
    {
    for (TInt i = 0; i < KMaxSmokes; i++)
        {
        if (!aState.iSmokeOn[i]) continue;
        const TReal32 age = aTime - (aState.iSmokeUntil[i] - 18.0f);
        TReal32 radius = MinF(1.0f, age / 2.0f) * 150.0f;
        const TReal32 remaining = aState.iSmokeUntil[i] - aTime;
        if (remaining < 3.0f) radius *= MaxF(0.0f, remaining / 3.0f);
        const TReal32 dx = aX2 - aX1, dy = aY2 - aY1, dz = aZ2 - aZ1;
        const TReal32 length = dx * dx + dy * dy + dz * dz;
        if (length < 1.0f) continue;
        TReal32 t = ((aState.iSmokeX[i] - aX1) * dx + (aState.iSmokeY[i] - aY1) * dy
                     + (aState.iSmokeZ[i] - aZ1) * dz) / length;
        t = Clamp(t, 0.0f, 1.0f);
        const TReal32 px = aX1 + dx * t - aState.iSmokeX[i];
        const TReal32 py = aY1 + dy * t - aState.iSmokeY[i];
        const TReal32 pz = aZ1 + dz * t - aState.iSmokeZ[i];
        if (px * px + py * py + pz * pz < radius * radius) return ETrue;
        }
    return EFalse;
    }

static TBool IsVisible(CMap* aMap, const TMatchState& aState, TReal32 aTime,
                       const TPlayer& aViewer, const TPlayer& aTarget)
    {
    if (!aMap) return EFalse;
    const TReal32 ez = EyeZ(aViewer);
    const TReal32 targetZ = aTarget.iDucked ? aTarget.iZ + 8 : aTarget.iZ + 22;
    aMap->Trace(0, aViewer.iX, aViewer.iY, ez, aTarget.iX, aTarget.iY, targetZ);
    TBool clear = aMap->rFrac >= 1.0f;
    if (!clear)
        {
        aMap->Trace(0, aViewer.iX, aViewer.iY, ez, aTarget.iX, aTarget.iY, aTarget.iZ);
        clear = aMap->rFrac >= 1.0f;
        }
    if (!clear) return EFalse;
    return !SmokeBlocks(aState, aTime, aViewer.iX, aViewer.iY, ez,
                        aTarget.iX, aTarget.iY, targetZ);
    }

// ------------------------------------------------------------------ ввод и физика

void CGame::HumanInput(TReal32 aDelta)
    {
    TPlayer& p = iPlayers[iMyIndex];
    TExtraPlayer& x = Ex(p);
    if (!p.iAlive)
        {
        if (iSpectatorIndex < 0) iDeadCameraYaw += aDelta * 10.0f;
        return;
        }
    TInt keys = iKeys;
    if (iBuyOpen) keys &= ~(EActionFire | EActionTurnLeft | EActionTurnRight | EActionLookUp | EActionLookDown);
    const TReal32 speed = 400.0f;
    x.iForward = ((keys & EActionForward) ? speed : 0.0f) - ((keys & EActionBack) ? speed : 0.0f);
    x.iStrafe = ((keys & EActionStrafeRight) ? speed : 0.0f) - ((keys & EActionStrafeLeft) ? speed : 0.0f);
    if ((keys & (EActionTurnLeft | EActionTurnRight)) != 0) iTurnHeld += aDelta;
    else iTurnHeld = 0;
    const TReal32 zoom = p.iZoom == 0 ? 1.0f : (p.iZoom == 1 ? 0.45f : 0.2f);
    const TReal32 turnSpeed = (70.0f + 22.0f * iSensitivity)
        * (0.45f + MinF(1.0f, iTurnHeld * 2.5f) * 0.9f) * zoom;
    if (keys & EActionTurnLeft) p.iYaw += turnSpeed * aDelta;
    if (keys & EActionTurnRight) p.iYaw -= turnSpeed * aDelta;
    TReal32 pitchSpeed = (55.0f + 12.0f * iSensitivityVertical) * zoom * (iInvertY ? -1.0f : 1.0f);
    if (keys & EActionLookUp) p.iPitch -= pitchSpeed * aDelta;
    if (keys & EActionLookDown) p.iPitch += pitchSpeed * aDelta;
    p.iPitch = Clamp(p.iPitch, -89.0f, 89.0f);
    p.iYaw = AngleNorm(p.iYaw);
    x.iJump = (keys & EActionJump) != 0;
    x.iDuck = (keys & EActionDuck) != 0;
    x.iFire = (keys & (EActionFire | EActionSelect)) != 0;
    x.iUse = (keys & EActionUse) != 0;
    if (iTime < iFreezeEnd) { x.iForward = 0; x.iStrafe = 0; x.iJump = EFalse; }
    if (iAutoAim && x.iFire) AimAssist(p, aDelta);
    }

void CGame::AimAssist(TPlayer& aPlayer, TReal32 aDelta)
    {
    ForwardVector(aPlayer.iYaw, aPlayer.iPitch, gTempForward);
    TPlayer* best = NULL;
    TReal32 bestAngle = 4.5f;
    for (TInt i = 0; i < iPlayerCount; i++)
        {
        TPlayer& q = iPlayers[i];
        if (&q == &aPlayer || !q.iAlive || q.iTeam == aPlayer.iTeam) continue;
        const TReal32 dx = q.iX - aPlayer.iX, dy = q.iY - aPlayer.iY;
        const TReal32 dz = q.iZ + (q.iDucked ? 4 : 10) - EyeZ(aPlayer);
        const TReal32 distance = FloatSqrt(dx * dx + dy * dy + dz * dz);
        if (distance < 1.0f) continue;
        const TReal32 yaw = FloatAtan2(dy, dx);
        const TReal32 pitch = -FloatAtan2(dz, FloatSqrt(dx * dx + dy * dy));
        const TReal32 angle = AbsF(AngleNorm(yaw - aPlayer.iYaw)) + AbsF(pitch - aPlayer.iPitch);
        if (angle < bestAngle && IsVisible(iAssets.iMap, gState, iTime, aPlayer, q))
            { bestAngle = angle; best = &q; }
        }
    if (!best) return;
    const TReal32 dx = best->iX - aPlayer.iX, dy = best->iY - aPlayer.iY;
    const TReal32 dz = best->iZ + (best->iDucked ? 4 : 10) - EyeZ(aPlayer);
    const TReal32 yaw = FloatAtan2(dy, dx);
    const TReal32 pitch = -FloatAtan2(dz, FloatSqrt(dx * dx + dy * dy));
    const TReal32 factor = MinF(1.0f, aDelta * 6.0f);
    aPlayer.iYaw += AngleNorm(yaw - aPlayer.iYaw) * factor;
    aPlayer.iPitch += (pitch - aPlayer.iPitch) * factor;
    }

TReal32 CGame::MaxSpeed(const TPlayer& aPlayer) const
    {
    const TExtraPlayer& x = Ex(aPlayer);
    TReal32 speed = (TReal32)KWpSpeed[BoundedIndex(aPlayer.iWeapon, KWeaponCount)];
    if (aPlayer.iZoom > 0 && IsSniper(aPlayer.iWeapon))
        speed = aPlayer.iWeapon == 21 ? 150.0f : 200.0f;
    if (aPlayer.iPlanting || x.iDefusing) speed = 0;
    if (x.iZombie) speed = (TReal32)KZombieSpeed[BoundedIndex(x.iZType, 3)];
    return speed;
    }

void CGame::MovePlayer(TPlayer& aPlayer, TReal32 aDelta)
    {
    CMap* map = iAssets.iMap;
    if (!map) return;
    TExtraPlayer& x = Ex(aPlayer);
    const TBool wasGround = aPlayer.iOnGround;
    const TReal32 previousZVelocity = aPlayer.iVelocityZ;
    if (x.iDuck && !aPlayer.iDucked)
        {
        if (aPlayer.iOnGround) { aPlayer.iZ -= 18.0f; aPlayer.iDucked = ETrue; }
        else aPlayer.iDucked = ETrue;
        if (!TestPosition(map, aPlayer.iX, aPlayer.iY, aPlayer.iZ, 3))
            { aPlayer.iZ += aPlayer.iOnGround ? 18.0f : 0.0f; aPlayer.iDucked = EFalse; }
        }
    else if (!x.iDuck && aPlayer.iDucked)
        {
        const TReal32 targetZ = aPlayer.iOnGround ? aPlayer.iZ + 18.0f : aPlayer.iZ;
        if (TestPosition(map, aPlayer.iX, aPlayer.iY, targetZ, 1))
            { aPlayer.iZ = targetZ; aPlayer.iDucked = EFalse; }
        }
    TReal32 maxSpeed = MaxSpeed(aPlayer);
    if (aPlayer.iDucked && aPlayer.iOnGround) maxSpeed *= 0.333f;

    const TReal32 cy = FloatCos(aPlayer.iYaw), sy = FloatSin(aPlayer.iYaw);
    const TReal32 forwardX = cy, forwardY = sy, rightX = sy, rightY = -cy;
    aPlayer.iOnLadder = EFalse;
    for (TInt i = 0; i < gState.iLadderCount; i++)
        {
        const TInt model = gState.iLadders[i];
        if (model < 0 || model >= map->numModels) continue;
        if (aPlayer.iX + 17 > map->mMins[model * 3] && aPlayer.iX - 17 < map->mMaxs[model * 3]
            && aPlayer.iY + 17 > map->mMins[model * 3 + 1] && aPlayer.iY - 17 < map->mMaxs[model * 3 + 1]
            && aPlayer.iZ + 37 > map->mMins[model * 3 + 2] && aPlayer.iZ - 37 < map->mMaxs[model * 3 + 2])
            { aPlayer.iOnLadder = ETrue; break; }
        }
    if (aPlayer.iOnLadder)
        {
        const TReal32 cp = FloatCos(aPlayer.iPitch), sp = FloatSin(aPlayer.iPitch);
        const TReal32 forward = x.iForward > 0 ? 200.0f : (x.iForward < 0 ? -200.0f : 0.0f);
        const TReal32 strafe = x.iStrafe > 0 ? 200.0f : (x.iStrafe < 0 ? -200.0f : 0.0f);
        aPlayer.iVelocityX = cy * cp * forward + rightX * strafe;
        aPlayer.iVelocityY = sy * cp * forward + rightY * strafe;
        aPlayer.iVelocityZ = 0;
        if (forward != 0)
            {
            const TReal32 sign = (-sp * forward >= 0) ? 1.0f : -1.0f;
            aPlayer.iVelocityZ = AbsF(forward) * (AbsF(sp) > 0.15f ? sign : (forward > 0 ? 1.0f : -1.0f));
            }
        if (x.iJump && !x.iJumpHeld)
            {
            aPlayer.iVelocityX = -cy * 270.0f;
            aPlayer.iVelocityY = -sy * 270.0f;
            aPlayer.iVelocityZ = 100.0f;
            aPlayer.iOnLadder = EFalse;
            }
        x.iJumpHeld = x.iJump;
        FlyMove(map, aPlayer, aDelta);
        CategorizePlayer(map, aPlayer);
        return;
        }

    if (aPlayer.iOnGround)
        {
        const TReal32 planar = FloatSqrt(aPlayer.iVelocityX * aPlayer.iVelocityX + aPlayer.iVelocityY * aPlayer.iVelocityY);
        if (planar < 0.1f) { aPlayer.iVelocityX = 0; aPlayer.iVelocityY = 0; }
        else
            {
            const TReal32 control = planar < 75.0f ? 75.0f : planar;
            const TReal32 factor = MaxF(0.0f, planar - control * 4.0f * aDelta) / planar;
            aPlayer.iVelocityX *= factor;
            aPlayer.iVelocityY *= factor;
            }
        }
    TReal32 wishX = forwardX * x.iForward + rightX * x.iStrafe;
    TReal32 wishY = forwardY * x.iForward + rightY * x.iStrafe;
    TReal32 wishSpeed = FloatSqrt(wishX * wishX + wishY * wishY);
    if (wishSpeed > 0.001f) { wishX /= wishSpeed; wishY /= wishSpeed; }
    if (wishSpeed > maxSpeed) wishSpeed = maxSpeed;
    if (aPlayer.iOnGround)
        {
        const TReal32 current = aPlayer.iVelocityX * wishX + aPlayer.iVelocityY * wishY;
        const TReal32 add = wishSpeed - current;
        if (add > 0)
            {
            TReal32 amount = 5.0f * aDelta * wishSpeed;
            if (amount > add) amount = add;
            aPlayer.iVelocityX += amount * wishX;
            aPlayer.iVelocityY += amount * wishY;
            }
        aPlayer.iVelocityZ = 0;
        if (x.iJump && !x.iJumpHeld)
            {
            aPlayer.iOnGround = EFalse;
            aPlayer.iVelocityZ = 268.3283f;
            if (x.iJumpPenalty > 0)
                {
                const TReal32 reduction = (100.0f - x.iJumpPenalty * 0.001f * 19.0f) * 0.01f;
                aPlayer.iVelocityX *= reduction;
                aPlayer.iVelocityY *= reduction;
                }
            x.iJumpPenalty = 1315.789429f;
            PlaySoundAt(aPlayer.iX, aPlayer.iY, aPlayer.iZ, Ascii("player_pl_jump1"), 60, 1800);
            }
        }
    x.iJumpHeld = x.iJump;
    if (aPlayer.iOnGround)
        {
        const TReal32 speedSquared = aPlayer.iVelocityX * aPlayer.iVelocityX + aPlayer.iVelocityY * aPlayer.iVelocityY;
        if (speedSquared < 1.0f) { aPlayer.iVelocityX = 0; aPlayer.iVelocityY = 0; }
        else
            {
            const TInt hull = PlayerHull(aPlayer);
            const TReal32 dx = aPlayer.iX + aPlayer.iVelocityX * aDelta;
            const TReal32 dy = aPlayer.iY + aPlayer.iVelocityY * aDelta;
            map->Trace(hull, aPlayer.iX, aPlayer.iY, aPlayer.iZ, dx, dy, aPlayer.iZ);
            if (map->rFrac == 1.0f)
                {
                aPlayer.iX = map->rEx;
                aPlayer.iY = map->rEy;
                aPlayer.iZ = map->rEz;
                }
            else
                {
                const TReal32 ox = aPlayer.iX, oy = aPlayer.iY, oz = aPlayer.iZ;
                const TReal32 ovx = aPlayer.iVelocityX, ovy = aPlayer.iVelocityY, ovz = aPlayer.iVelocityZ;
                FlyMove(map, aPlayer, aDelta);
                const TReal32 downX = aPlayer.iX, downY = aPlayer.iY, downZ = aPlayer.iZ;
                const TReal32 downVX = aPlayer.iVelocityX, downVY = aPlayer.iVelocityY, downVZ = aPlayer.iVelocityZ;
                aPlayer.iX = ox; aPlayer.iY = oy; aPlayer.iZ = oz;
                aPlayer.iVelocityX = ovx; aPlayer.iVelocityY = ovy; aPlayer.iVelocityZ = ovz;
                map->Trace(hull, aPlayer.iX, aPlayer.iY, aPlayer.iZ, aPlayer.iX, aPlayer.iY, aPlayer.iZ + 18.0f);
                if (!map->rStartSolid && !map->rAllSolid)
                    { aPlayer.iX = map->rEx; aPlayer.iY = map->rEy; aPlayer.iZ = map->rEz; }
                FlyMove(map, aPlayer, aDelta);
                map->Trace(hull, aPlayer.iX, aPlayer.iY, aPlayer.iZ, aPlayer.iX, aPlayer.iY, aPlayer.iZ - 18.0f);
                if (map->rFrac < 1.0f && map->rNz < 0.7f)
                    {
                    aPlayer.iX = downX; aPlayer.iY = downY; aPlayer.iZ = downZ;
                    aPlayer.iVelocityX = downVX; aPlayer.iVelocityY = downVY; aPlayer.iVelocityZ = downVZ;
                    }
                else
                    {
                    if (!map->rStartSolid && !map->rAllSolid)
                        { aPlayer.iX = map->rEx; aPlayer.iY = map->rEy; aPlayer.iZ = map->rEz; }
                    const TReal32 downDistance = (downX - ox) * (downX - ox) + (downY - oy) * (downY - oy);
                    const TReal32 upDistance = (aPlayer.iX - ox) * (aPlayer.iX - ox) + (aPlayer.iY - oy) * (aPlayer.iY - oy);
                    if (downDistance > upDistance)
                        {
                        aPlayer.iX = downX; aPlayer.iY = downY; aPlayer.iZ = downZ;
                        aPlayer.iVelocityX = downVX; aPlayer.iVelocityY = downVY; aPlayer.iVelocityZ = downVZ;
                        }
                    else aPlayer.iVelocityZ = downVZ;
                    }
                }
            }
        }
    else
        {
        const TReal32 airWish = wishSpeed > 30.0f ? 30.0f : wishSpeed;
        const TReal32 current = aPlayer.iVelocityX * wishX + aPlayer.iVelocityY * wishY;
        const TReal32 add = airWish - current;
        if (add > 0)
            {
            TReal32 amount = 10.0f * wishSpeed * aDelta;
            if (amount > add) amount = add;
            aPlayer.iVelocityX += amount * wishX;
            aPlayer.iVelocityY += amount * wishY;
            }
        aPlayer.iVelocityZ -= 800.0f * aDelta * 0.5f;
        FlyMove(map, aPlayer, aDelta);
        aPlayer.iVelocityZ -= 800.0f * aDelta * 0.5f;
        }
    if (x.iJumpPenalty > 0)
        {
        x.iJumpPenalty -= aDelta * 1000.0f;
        if (x.iJumpPenalty < 0) x.iJumpPenalty = 0;
        }
    CategorizePlayer(map, aPlayer);
    if (!wasGround && aPlayer.iOnGround)
        {
        if (-previousZVelocity > 580.0f)
            {
            const TInt fallDamage = (TInt)((-previousZVelocity - 580.0f) * (100.0f / (1024.0f - 580.0f)));
            if (fallDamage > 0)
                {
                Damage(aPlayer, NULL, fallDamage, KWeaponCount, EFalse, 0);
                PlaySoundAt(aPlayer.iX, aPlayer.iY, aPlayer.iZ, Ascii("player_pl_fallpain1"), 80, 1800);
                }
            }
        aPlayer.iVelocityZ = 0;
        }
    if (aPlayer.iZ < -8000.0f) Damage(aPlayer, NULL, 1000, KWeaponCount, EFalse, 0);
    }

void CGame::UpdatePlayer(TPlayer& aPlayer, TReal32 aDelta)
    {
    TExtraPlayer& x = Ex(aPlayer);
    if (!aPlayer.iAlive) { aPlayer.iAnimTime += aDelta; return; }
    if (iTime < iFreezeEnd) { x.iForward = 0; x.iStrafe = 0; x.iJump = EFalse; }
    if (!x.iRemote)
        {
        TReal32 remaining = aDelta;
        while (remaining > 0.0001f)
            {
            const TReal32 step = remaining > 0.034f ? 0.034f : remaining;
            MovePlayer(aPlayer, step);
            remaining -= step;
            }
        }
    const TReal32 planar = SpeedFactor(aPlayer);
    aPlayer.iSpeedFactor = planar;
    aPlayer.iViewBob = planar;
    if (aPlayer.iOnGround && planar > 150.0f && !aPlayer.iDucked)
        {
        x.iStepTime -= aDelta;
        if (x.iStepTime <= 0)
            {
            x.iStepTime = 0.34f;
            PlaySoundAt(aPlayer.iX, aPlayer.iY, aPlayer.iZ,
                        Ascii((iRandom.Next() & 1) ? "player_pl_step2" : "player_pl_step1"),
                        &aPlayer == &iPlayers[iMyIndex] ? 25 : 55, 1800);
            }
        }
    if (planar > 20.0f) aPlayer.iAnimTime += aDelta * planar / 220.0f;
    else aPlayer.iAnimTime = 0;
    TReal32 decay = aDelta * (10.0f + AbsF(x.iPunchPitch) * 4.0f);
    if (x.iPunchPitch > 0) { x.iPunchPitch = MaxF(0.0f, x.iPunchPitch - decay); }
    else if (x.iPunchPitch < 0) { x.iPunchPitch = MinF(0.0f, x.iPunchPitch + decay); }
    decay = aDelta * (10.0f + AbsF(x.iPunchYaw) * 4.0f);
    if (x.iPunchYaw > 0) { x.iPunchYaw = MaxF(0.0f, x.iPunchYaw - decay); }
    else if (x.iPunchYaw < 0) { x.iPunchYaw = MinF(0.0f, x.iPunchYaw + decay); }
    x.iRecoil = MaxF(0.0f, x.iRecoil - aDelta * 0.25f);
    if (!x.iFire && iTime > x.iNextAttack) x.iShotsFired = 0;
    Weapons(aPlayer, aDelta);
    if (!x.iRemote && iMode == 0)
        {
        CMap* map = iAssets.iMap;
        if (iBombDropped && aPlayer.iTeam == 1 && !iBombPlanted)
            {
            const TReal32 dx = aPlayer.iX - iBombX, dy = aPlayer.iY - iBombY;
            const TReal32 dz = aPlayer.iZ - 30.0f - iBombZ;
            if (dx * dx + dy * dy < 40.0f * 40.0f && AbsF(dz) < 60.0f)
                {
                iBombDropped = EFalse;
                aPlayer.iHasC4 = ETrue;
                if (&aPlayer == &iPlayers[iMyIndex]) Center(_L("Вы подобрали бомбу"), 2.0f, 0xFFFFFF);
                }
            }
        if (aPlayer.iWeapon == 28 && aPlayer.iHasC4 && map)
            {
            const TBool can = InAnySite(*map, gState, aPlayer.iX, aPlayer.iY, aPlayer.iZ, 0)
                && aPlayer.iOnGround && !iRoundOver && iTime > iFreezeEnd;
            if (x.iFire && can)
                {
                if (!aPlayer.iPlanting)
                    {
                    aPlayer.iPlanting = ETrue;
                    aPlayer.iPlantProgress = 0;
                    x.iDrawTime = iTime;
                    x.iDrawEnd = iTime + 3.0f;
                    SetVm(aPlayer, "shoot", 3.0f, iTime);
                    PlaySoundAt(aPlayer.iX, aPlayer.iY, aPlayer.iZ, Ascii("weapons_c4_plant"), 40, 1800);
                    }
                aPlayer.iPlantProgress += aDelta;
                if (aPlayer.iPlantProgress >= 3.0f)
                    {
                    aPlayer.iPlanting = EFalse;
                    aPlayer.iHasC4 = EFalse;
                    iBombPlanted = ETrue;
                    iBombX = aPlayer.iX;
                    iBombY = aPlayer.iY;
                    iBombZ = aPlayer.iZ - (aPlayer.iDucked ? 18.0f : 36.0f);
                    iBombTime = iTime + KTimeBomb;
                    iBombBeep = iTime;
                    iBombSite = SiteIndexAt(*map, gState, aPlayer.iX, aPlayer.iY, aPlayer.iZ);
                    PlaySound(Ascii("radio_bombpl"), 90);
                    Center(_L("Бомба заложена!"), 3.0f, 0xFF6040);
                    for (TInt k = 0; k < iPlayerCount; k++)
                        if (iPlayers[k].iTeam == 1)
                            AddMoney(iPlayers[k], &iPlayers[k] == &aPlayer ? 460 : 160);
                    const TInt next = aPlayer.iPrimary >= 0 ? aPlayer.iPrimary
                        : (aPlayer.iSecondary >= 0 ? aPlayer.iSecondary : 0);
                    SwitchWeapon(aPlayer, next);
                    aPlayer.iDrawEnd = iTime + 0.6f;
                    }
                }
            else
                {
                if (x.iFire && !x.iFirePrev && &aPlayer == &iPlayers[iMyIndex] && !can)
                    Center(_L("Бомбу можно заложить только на точке"), 2.0f, 0xFFFFFF);
                if (aPlayer.iPlanting)
                    {
                    aPlayer.iPlanting = EFalse;
                    SetVm(aPlayer, "idle", 1.0f, iTime);
                    }
                }
            }
        if (iBombPlanted && aPlayer.iTeam == 2 && !iRoundOver)
            {
            const TReal32 dx = aPlayer.iX - iBombX, dy = aPlayer.iY - iBombY;
            const TReal32 dz = (aPlayer.iZ - 36.0f) - iBombZ;
            const TBool near = dx * dx + dy * dy < 56.0f * 56.0f && AbsF(dz) < 64.0f;
            if (near && x.iUse)
                {
                if (!x.iDefusing)
                    {
                    x.iDefusing = ETrue;
                    x.iDefuseProgress = 0;
                    PlaySoundAt(aPlayer.iX, aPlayer.iY, aPlayer.iZ, Ascii("weapons_c4_disarm"), 60, 1800);
                    if (&aPlayer == &iPlayers[iMyIndex])
                        Center(x.iDefuser ? TPtrC(_L("Обезвреживание (набор)...")) : TPtrC(_L("Обезвреживание...")),
                               2.0f, 0xFFFFFF);
                    }
                x.iDefuseProgress += aDelta;
                if (x.iDefuseProgress >= (x.iDefuser ? 5.0f : 10.0f))
                    {
                    iBombPlanted = EFalse;
                    PlaySound(Ascii("weapons_c4_disarmed"), 90);
                    AddMoney(aPlayer, 250);
                    EndRound(2, _L("Бомба обезврежена! Спецназ победил"), Ascii("radio_bombdef"), 1);
                    }
                }
            else if (x.iDefusing) x.iDefusing = EFalse;
            }
        }
    x.iFirePrev = x.iFire;
    x.iFire2Prev = x.iFire2;
    x.iUsePrev = x.iUse;
    }

// ------------------------------------------------------------------ оружие

void CGame::SwitchWeapon(TPlayer& aPlayer, TInt aWeapon)
    {
    if (!HasWeapon(aPlayer, aWeapon) || aPlayer.iWeapon == aWeapon) return;
    TExtraPlayer& x = Ex(aPlayer);
    aPlayer.iPlanting = EFalse;
    x.iLast = aPlayer.iWeapon;
    aPlayer.iWeapon = aWeapon;
    aPlayer.iReloading = EFalse;
    aPlayer.iZoom = 0;
    x.iShotsFired = 0;
    aPlayer.iDrawEnd = iTime + KWpDrawT[aWeapon];
    x.iNextAttack = iTime + KWpDrawT[aWeapon] * 0.8f;
    x.iDrawTime = iTime;
    x.iDrawEnd = aPlayer.iDrawEnd;
    SetVm(aPlayer, "draw", KWpDrawT[aWeapon], iTime);
    if (&aPlayer == &iPlayers[iMyIndex])
        PlaySoundAt(aPlayer.iX, aPlayer.iY, aPlayer.iZ,
                    Ascii(aWeapon == 0 ? "weapons_knife_deploy1" : "weapons_slideback1"), 50, 1800);
    }

void CGame::NextWeapon(TPlayer& aPlayer)
    {
    TExtraPlayer& x = Ex(aPlayer);
    const TInt order[8] = { aPlayer.iPrimary, aPlayer.iSecondary, 0,
        x.iHe > 0 ? 25 : -1, x.iFlash > 0 ? 26 : -1, x.iSmoke > 0 ? 27 : -1,
        aPlayer.iHasC4 ? 28 : -1, -1 };
    TInt index = 0;
    for (TInt i = 0; i < 7; i++) if (order[i] == aPlayer.iWeapon) index = i;
    for (TInt k = 1; k <= 7; k++)
        {
        const TInt weapon = order[(index + k) % 7];
        if (weapon >= 0) { SwitchWeapon(aPlayer, weapon); return; }
        }
    }

void CGame::SelectSlot(TPlayer& aPlayer, TInt aSlot)
    {
    if (aSlot == 0 && aPlayer.iPrimary >= 0) SwitchWeapon(aPlayer, aPlayer.iPrimary);
    else if (aSlot == 1 && aPlayer.iSecondary >= 0) SwitchWeapon(aPlayer, aPlayer.iSecondary);
    else if (aSlot == 2) SwitchWeapon(aPlayer, 0);
    else if (aSlot == 3)
        {
        const TExtraPlayer& x = Ex(aPlayer);
        if (x.iHe > 0) SwitchWeapon(aPlayer, 25);
        else if (x.iFlash > 0) SwitchWeapon(aPlayer, 26);
        else if (x.iSmoke > 0) SwitchWeapon(aPlayer, 27);
        }
    else if (aSlot == 4 && aPlayer.iHasC4) SwitchWeapon(aPlayer, 28);
    }

void CGame::StartReload(TPlayer& aPlayer)
    {
    const TInt weapon = aPlayer.iWeapon;
    TExtraPlayer& x = Ex(aPlayer);
    if (!IsGun(weapon) || aPlayer.iReloading || aPlayer.iClip[weapon] >= KWpClip[weapon]
        || aPlayer.iAmmo[weapon] <= 0 || iTime < aPlayer.iDrawEnd) return;
    aPlayer.iReloading = ETrue;
    aPlayer.iZoom = 0;
    if (IsShotgun(weapon))
        {
        x.iReloadEnd = iTime + 0.5f;
        x.iShellNext = iTime + 0.5f;
        SetVm(aPlayer, "rstart", 0.5f, iTime);
        }
    else
        {
        x.iReloadEnd = iTime + KWpReloadT[weapon];
        SetVm(aPlayer, "reload", KWpReloadT[weapon], iTime);
        PlaySoundAt(aPlayer.iX, aPlayer.iY, aPlayer.iZ, Ascii("weapons_clipout1"),
                    &aPlayer == &iPlayers[iMyIndex] ? 60 : 40, 1800);
        }
    }

void CGame::AlternateFire(TPlayer& aPlayer)
    {
    const TInt weapon = aPlayer.iWeapon;
    TExtraPlayer& x = Ex(aPlayer);
    if ((weapon == 17 || weapon == 2) && !aPlayer.iReloading
        && iTime >= x.iNextAttack && iTime >= aPlayer.iDrawEnd)
        {
        const TBool attach = weapon == 17 ? !x.iSilM4 : !x.iSilUSP;
        const TReal32 length = weapon == 17 ? 2.0f : 3.1f;
        if (weapon == 17) x.iSilM4 = attach;
        else x.iSilUSP = attach;
        SetVm(aPlayer, attach ? "attach" : "detach", length, iTime);
        x.iNextAttack = iTime + length;
        aPlayer.iDrawEnd = iTime + length * 0.9f;
        x.iDrawTime = iTime;
        x.iDrawEnd = aPlayer.iDrawEnd;
        PlaySoundAt(aPlayer.iX, aPlayer.iY, aPlayer.iZ,
                    Ascii(weapon == 17 ? (attach ? "weapons_m4a1_silencer_on" : "weapons_m4a1_silencer_off")
                                       : (attach ? "weapons_usp_silencer_on" : "weapons_usp_silencer_off")),
                    60, 1800);
        return;
        }
    if (KWpZoom1[weapon] > 0 && !aPlayer.iReloading)
        {
        const TInt maximum = KWpZoom2[weapon] > 0 ? 2 : 1;
        aPlayer.iZoom = (aPlayer.iZoom + 1) % (maximum + 1);
        PlaySoundAt(aPlayer.iX, aPlayer.iY, aPlayer.iZ, Ascii("weapons_zoom"), 40, 1800);
        }
    else if (weapon == 0 && iTime >= x.iNextAttack) Knife(aPlayer, ETrue);
    }

void CGame::Weapons(TPlayer& aPlayer, TReal32 aDelta)
    {
    const TInt weapon = aPlayer.iWeapon;
    TExtraPlayer& x = Ex(aPlayer);
    (void)aDelta;
    if (aPlayer.iReloading)
        {
        if (IsShotgun(weapon))
            {
            if (iTime >= x.iShellNext)
                {
                if (aPlayer.iClip[weapon] < KWpClip[weapon] && aPlayer.iAmmo[weapon] > 0
                    && !(x.iFire && aPlayer.iClip[weapon] > 0))
                    {
                    aPlayer.iClip[weapon]++;
                    aPlayer.iAmmo[weapon]--;
                    x.iShellNext = iTime + KWpReloadT[weapon];
                    SetVm(aPlayer, "insert", KWpReloadT[weapon], iTime);
                    PlaySoundAt(aPlayer.iX, aPlayer.iY, aPlayer.iZ, Ascii("weapons_m3_insertshell"), 60, 1800);
                    }
                else
                    {
                    aPlayer.iReloading = EFalse;
                    x.iNextAttack = iTime + 0.3f;
                    SetVm(aPlayer, "rafter", 0.4f, iTime);
                    if (&aPlayer == &iPlayers[iMyIndex])
                        PlaySoundAt(aPlayer.iX, aPlayer.iY, aPlayer.iZ, Ascii("weapons_m3_pump"), 60, 1800);
                    }
                }
            }
        else if (iTime >= x.iReloadEnd)
            {
            const TInt needed = KWpClip[weapon] - aPlayer.iClip[weapon];
            const TInt taken = MinI(needed, aPlayer.iAmmo[weapon]);
            aPlayer.iClip[weapon] += taken;
            aPlayer.iAmmo[weapon] -= taken;
            aPlayer.iReloading = EFalse;
            SetVm(aPlayer, "idle", 1.0f, iTime);
            PlaySoundAt(aPlayer.iX, aPlayer.iY, aPlayer.iZ, Ascii("weapons_clipin1"), 60, 1800);
            }
        }
    if (aPlayer.iReloading) return;
    if (aPlayer.iPlanting || iTime < aPlayer.iDrawEnd || weapon == 28) return;
    if (x.iFire && iTime >= x.iNextAttack)
        {
        if (weapon == 0) { Knife(aPlayer, EFalse); return; }
        if (IsGrenade(weapon))
            {
            TInt slot = -1;
            for (TInt i = 0; i < KMaxGrenades; i++)
                if (!iGrenades[i].iActive) { slot = i; break; }
            if (slot >= 0)
                {
                const TReal32 pitch = aPlayer.iPitch - 10.0f;
                ForwardVector(aPlayer.iYaw, pitch, gTempForward);
                TGrenade& grenade = iGrenades[slot];
                grenade.iActive = ETrue;
                grenade.iType = weapon;
                grenade.iOwner = aPlayer.iId;
                grenade.iTimer = iTime + (weapon == 27 ? 1.5f : 1.6f);
                grenade.iX = aPlayer.iX + gTempForward[0] * 16.0f;
                grenade.iY = aPlayer.iY + gTempForward[1] * 16.0f;
                grenade.iZ = EyeZ(aPlayer) - 4.0f;
                grenade.iVelocityX = gTempForward[0] * 750.0f + aPlayer.iVelocityX;
                grenade.iVelocityY = gTempForward[1] * 750.0f + aPlayer.iVelocityY;
                grenade.iVelocityZ = gTempForward[2] * 750.0f + aPlayer.iVelocityZ;
                if (weapon == 25) x.iHe--;
                else if (weapon == 26) x.iFlash--;
                else x.iSmoke--;
                x.iNextAttack = iTime + 1.0f;
                x.iDrawTime = iTime;
                x.iDrawEnd = iTime + 0.5f;
                PlaySoundAt(aPlayer.iX, aPlayer.iY, aPlayer.iZ, Ascii("weapons_pinpull"), 50, 1800);
                if (aPlayer.iTeam == iPlayers[iMyIndex].iTeam)
                    PlaySound(Ascii("radio_ct_fireinhole"), 40);
                if (!HasWeapon(aPlayer, weapon))
                    {
                    aPlayer.iDrawEnd = iTime + 0.5f;
                    const TInt next = aPlayer.iPrimary >= 0 ? aPlayer.iPrimary
                        : (aPlayer.iSecondary >= 0 ? aPlayer.iSecondary : 0);
                    x.iLast = 0;
                    SwitchWeapon(aPlayer, next);
                    }
                }
            return;
            }
        if (!KWpAuto[weapon] && x.iFirePrev) return;
        if (aPlayer.iClip[weapon] <= 0)
            {
            if (!x.iFirePrev) PlaySoundAt(aPlayer.iX, aPlayer.iY, aPlayer.iZ, Ascii("weapons_dryfire_rifle"), 40, 1800);
            x.iNextAttack = iTime + 0.2f;
            if (aPlayer.iAmmo[weapon] > 0) StartReload(aPlayer);
            return;
            }
        Shoot(aPlayer);
        }
    if (!x.iFire && IsGun(weapon) && aPlayer.iClip[weapon] == 0
        && aPlayer.iAmmo[weapon] > 0 && !aPlayer.iReloading && iTime >= x.iNextAttack)
        StartReload(aPlayer);
    }

void CGame::Shoot(TPlayer& aPlayer)
    {
    const TInt weapon = aPlayer.iWeapon;
    TExtraPlayer& x = Ex(aPlayer);
    aPlayer.iClip[weapon]--;
    x.iNextAttack = iTime + KWpCycle[weapon];
    x.iShotsFired++;
    SetVm(aPlayer, "shoot", MinF(0.5f, MaxF(KWpCycle[weapon], 0.25f)), iTime);
    const TReal32 planar = SpeedFactor(aPlayer);
    TReal32 spread = KWpSpread[weapon];
    if (IsSniper(weapon) && aPlayer.iZoom > 0)
        spread = weapon == 21 ? 0.001f : (weapon == 20 ? 0.007f : 0.012f);
    spread += KWpMoveSpread[weapon] * MinF(1.0f, planar / 220.0f) * (planar > 40.0f ? 1.0f : 0.0f);
    if (!aPlayer.iOnGround) spread += 0.3f;
    if (aPlayer.iDucked) spread *= 0.75f;
    spread += x.iRecoil;
    x.iRecoil = MinF(0.09f, x.iRecoil + KWpRecoilSpread[weapon]);
    const TReal32 yaw = aPlayer.iYaw + x.iPunchYaw * 2.0f;
    const TReal32 pitch = aPlayer.iPitch + x.iPunchPitch * 2.0f;
    ForwardVector(yaw, pitch, gTempForward);
    const TReal32 fx = gTempForward[0], fy = gTempForward[1], fz = gTempForward[2];
    const TReal32 rx = FloatSin(yaw), ry = -FloatCos(yaw);
    const TReal32 ux = -fz * FloatCos(yaw), uy = -fz * FloatSin(yaw), uz = FloatCos(pitch);
    const TReal32 ex = aPlayer.iX, ey = aPlayer.iY, ez = EyeZ(aPlayer);
    const TInt pellets = KWpPellets[weapon];
    const TBool silent = (weapon == 17 && x.iSilM4) || (weapon == 2 && x.iSilUSP);
    TInt damageBase = KWpDmg[weapon];
    if (silent)
        {
        damageBase = weapon == 17 ? 33 : 30;
        spread *= 0.85f;
        }
    for (TInt k = 0; k < pellets; k++)
        {
        TReal32 a = RandomReal(iRandom, -spread, spread);
        TReal32 b = RandomReal(iRandom, -spread, spread);
        if (pellets > 1)
            {
            a = RandomReal(iRandom, -KWpSpread[weapon], KWpSpread[weapon]);
            b = RandomReal(iRandom, -KWpSpread[weapon] * 0.6f, KWpSpread[weapon] * 0.6f);
            }
        TReal32 dx = fx + rx * a + ux * b;
        TReal32 dy = fy + ry * a + uy * b;
        TReal32 dz = fz + uz * b;
        const TReal32 length = FloatSqrt(dx * dx + dy * dy + dz * dz);
        dx /= length; dy /= length; dz /= length;
        Bullet(aPlayer, weapon, ex, ey, ez, dx, dy, dz,
               pellets > 1 ? 3000.0f : 8192.0f, damageBase);
        }
    const TReal32 punch = KWpPunch[weapon];
    if (pellets == 1)
        {
        const TReal32 add = punch * (0.6f + MinI(x.iShotsFired, 8) * 0.12f);
        x.iPunchPitch = MaxF(-6.0f, x.iPunchPitch - add);
        x.iPunchYaw += RandomReal(iRandom, -1.0f, 1.0f) * punch * 0.35f
            * MinF(1.0f, x.iShotsFired / 4.0f);
        }
    else x.iPunchPitch -= punch;
    if (IsSniper(weapon) && (weapon == 21 || weapon == 20)) aPlayer.iZoom = 0;
    if (silent) PlaySoundAt(ex, ey, ez, Ascii(weapon == 17 ? "weapons_m4a1_1" : "weapons_usp1"),
                            &aPlayer == &iPlayers[iMyIndex] ? 60 : 45, 1800);
    else PlaySoundAt(ex, ey, ez, TPtrC8(reinterpret_cast<const TUint8*>(WeaponSound(weapon)),
                                       (TInt)strlen(WeaponSound(weapon))),
                     &aPlayer == &iPlayers[iMyIndex] ? 90 : 100, 2600);
    x.iDrawTime = iTime;
    x.iDrawEnd = iTime + MinF(0.5f, MaxF(KWpCycle[weapon], 0.25f));
    if (&aPlayer == &iPlayers[iMyIndex]) iMuzzleUntil = iTime + 0.05f;
    for (TInt i = 0; i < iPlayerCount; i++)
        {
        TPlayer& listener = iPlayers[i];
        if (!Ex(listener).iBought && !listener.iBot) continue;
        if (!listener.iAlive || listener.iTeam == aPlayer.iTeam) continue;
        const TReal32 dx = listener.iX - aPlayer.iX, dy = listener.iY - aPlayer.iY;
        if (silent && dx * dx + dy * dy > 500.0f * 500.0f) continue;
        Ex(listener).iHeardX = aPlayer.iX;
        Ex(listener).iHeardY = aPlayer.iY;
        Ex(listener).iHeardZ = aPlayer.iZ;
        Ex(listener).iHeardAt = iTime;
        }
    aPlayer.iLastShot = iTime;
    aPlayer.iLastShotSilent = silent;
    }

void CGame::Bullet(TPlayer& aPlayer, TInt aWeapon, TReal32 aOriginX, TReal32 aOriginY,
                   TReal32 aOriginZ, TReal32 aDirectionX, TReal32 aDirectionY,
                   TReal32 aDirectionZ, TReal32 aRange, TInt aBaseDamage)
    {
    CMap* map = iAssets.iMap;
    if (!map) return;
    TReal32 multiplier = 1.0f;
    TReal32 travelled = 0;
    TInt penetrations = KWpPenCount[aWeapon];
    const TReal32 power = (TReal32)KWpPenPower[aWeapon];
    TReal32 sx = aOriginX, sy = aOriginY, sz = aOriginZ;
    for (TInt iteration = 0; iteration < 4; iteration++)
        {
        const TReal32 left = aRange - travelled;
        if (left <= 1.0f) return;
        map->Trace(0, sx, sy, sz, sx + aDirectionX * left,
                   sy + aDirectionY * left, sz + aDirectionZ * left);
        const TBool wall = map->rFrac < 1.0f && !map->rStartSolid;
        const TReal32 wallDistance = map->rStartSolid ? 0.0f : map->rFrac * left;
        const TReal32 wallX = map->rEx, wallY = map->rEy, wallZ = map->rEz;
        const TInt hitModel = map->rModel;
        TPlayer* hit = NULL;
        TReal32 hitDistance = wallDistance;
        TInt group = 0;
        TReal32 hitZ = 0;
        for (TInt i = 0; i < iPlayerCount; i++)
            {
            TPlayer& q = iPlayers[i];
            if (&q == &aPlayer || !q.iAlive) continue;
            const TInt candidate = RayHit(q, sx, sy, sz, aDirectionX, aDirectionY,
                                           aDirectionZ, hitDistance);
            if (candidate >= 0)
                {
                hit = &q;
                hitDistance = gRayT;
                group = candidate;
                hitZ = sz + aDirectionZ * gRayT;
                }
            }
        if (hit)
            {
            TReal32 damage = aBaseDamage * multiplier
                * PowF(KWpRangeMod[aWeapon], (travelled + hitDistance) / 500.0f);
            if (IsShotgun(aWeapon) && travelled + hitDistance > 600.0f) damage *= 0.6f;
            if (group == 1) damage *= 4.0f;
            else if (group == 3) damage *= 1.25f;
            else if (group == 4) damage *= 0.75f;
            Damage(*hit, &aPlayer, (TInt)damage, aWeapon, group == 1, group);
            AddEffect(sx + aDirectionX * hitDistance, sy + aDirectionY * hitDistance, hitZ,
                      0xA00000, 0.25f, 4, 2);
            return;
            }
        if (!wall) return;
        AddEffect(wallX, wallY, wallZ, 0x303030, 4.0f, 2, 0);
        AddEffect(wallX - aDirectionX * 2, wallY - aDirectionY * 2, wallZ - aDirectionZ * 2,
                  0xC8B898, 0.35f, 5, 0);
        if ((iRandom.Next() & 7) == 0)
            PlaySoundAt(wallX, wallY, wallZ, Ascii("weapons_ric_conc_1"), 50, 900);
        if (penetrations <= 0 || power <= 0) return;
        const TInt texture = map->SurfaceTex(hitModel, wallX - aDirectionX * 4,
            wallY - aDirectionY * 4, wallZ - aDirectionZ * 4,
            wallX + aDirectionX * 4, wallY + aDirectionY * 4, wallZ + aDirectionZ * 4);
        if (texture < 0 || texture >= map->numTex) return;
        if ((map->texFlags[texture] & 1) != 0) return;
        const TInt material = (map->texFlags[texture] >> 4) & 15;
        const TReal32 penetration = power * MatPower(material);
        const TReal32 nx = wallX + aDirectionX * penetration;
        const TReal32 ny = wallY + aDirectionY * penetration;
        const TReal32 nz = wallZ + aDirectionZ * penetration;
        const TInt head = hitModel > 0 ? map->mHead[hitModel * 4] : map->mHead[0];
        if (map->HullPointContents(0, head, nx, ny, nz) == -1) return;
        if (hitModel > 0 && map->HullPointContents(0, map->mHead[0], nx, ny, nz) == -1) return;
        multiplier *= MatDamage(material);
        penetrations--;
        travelled += wallDistance + penetration;
        sx = nx; sy = ny; sz = nz;
        AddEffect(nx, ny, nz, 0xC8B898, 0.3f, 4, 0);
        }
    }

TInt CGame::RayHit(TPlayer& aPlayer, TReal32 aOriginX, TReal32 aOriginY, TReal32 aOriginZ,
                   TReal32 aDirectionX, TReal32 aDirectionY, TReal32 aDirectionZ,
                   TReal32 aMaximumDistance)
    {
    const TReal32 headZ = aPlayer.iDucked ? aPlayer.iZ + 10.0f : aPlayer.iZ + 26.0f;
    const TReal32 ox = aOriginX - aPlayer.iX;
    const TReal32 oy = aOriginY - aPlayer.iY;
    const TReal32 oz = aOriginZ - headZ;
    const TReal32 b = ox * aDirectionX + oy * aDirectionY + oz * aDirectionZ;
    const TReal32 c = ox * ox + oy * oy + oz * oz - 6.5f * 6.5f;
    const TReal32 discriminant = b * b - c;
    TReal32 best = aMaximumDistance;
    TInt group = -1;
    if (discriminant >= 0)
        {
        const TReal32 t = -b - FloatSqrt(discriminant);
        if (t > 0 && t < best) { best = t; group = 1; }
        }
    const TReal32 top = aPlayer.iDucked ? aPlayer.iZ + 4.0f : aPlayer.iZ + 20.0f;
    const TReal32 bottom = aPlayer.iZ - (aPlayer.iDucked ? 18.0f : 36.0f);
    const TReal32 horizontal = aDirectionX * aDirectionX + aDirectionY * aDirectionY;
    if (horizontal > 0.000001f)
        {
        const TReal32 b2 = ox * aDirectionX + oy * aDirectionY;
        const TReal32 c2 = ox * ox + oy * oy - 13.0f * 13.0f;
        const TReal32 disc2 = b2 * b2 - horizontal * c2;
        if (disc2 >= 0)
            {
            TReal32 t = (-b2 - FloatSqrt(disc2)) / horizontal;
            if (t < 0) t = 0;
            const TReal32 z = aOriginZ + aDirectionZ * t;
            if (t < best && z >= bottom && z <= top)
                {
                best = t;
                const TReal32 relative = (z - bottom) / (top - bottom);
                group = relative > 0.62f ? 2 : (relative > 0.42f ? 3 : 4);
                }
            }
        }
    gRayT = best;
    return group;
    }

void CGame::Knife(TPlayer& aPlayer, TBool aStab)
    {
    TExtraPlayer& x = Ex(aPlayer);
    x.iNextAttack = iTime + (aStab ? 1.1f : 0.45f);
    x.iDrawTime = iTime;
    x.iDrawEnd = iTime + (aStab ? 1.0f : 0.45f);
    SetVm(aPlayer, aStab ? "shoot2" : "shoot", aStab ? 1.0f : 0.45f, iTime);
    ForwardVector(aPlayer.iYaw, aPlayer.iPitch, gTempForward);
    const TReal32 ex = aPlayer.iX, ey = aPlayer.iY, ez = EyeZ(aPlayer);
    const TReal32 range = aStab ? 32.0f : 48.0f;
    TPlayer* hit = NULL;
    TReal32 hitDistance = range + 16.0f;
    for (TInt i = 0; i < iPlayerCount; i++)
        {
        TPlayer& q = iPlayers[i];
        if (&q == &aPlayer || !q.iAlive) continue;
        if (RayHit(q, ex, ey, ez, gTempForward[0], gTempForward[1], gTempForward[2],
                   range + 16.0f) >= 0 && gRayT < hitDistance)
            { hitDistance = gRayT; hit = &q; }
        }
    if (hit)
        {
        TInt damage = aStab ? 65 : 20;
        if (AbsF(AngleNorm(hit->iYaw - aPlayer.iYaw)) < 50.0f) damage *= 3;
        Damage(*hit, &aPlayer, damage, 0, EFalse, 2);
        PlaySoundAt(ex, ey, ez, Ascii("weapons_knife_hit1"), 80, 1800);
        }
    else if (iAssets.iMap)
        {
        CMap* map = iAssets.iMap;
        map->Trace(0, ex, ey, ez, ex + gTempForward[0] * range,
                   ey + gTempForward[1] * range, ez + gTempForward[2] * range);
        PlaySoundAt(ex, ey, ez, Ascii(map->rFrac < 1.0f ? "weapons_knife_hitwall1" : "weapons_knife_slash1"), 60, 1200);
        }
    }

void CGame::Damage(TPlayer& aVictim, TPlayer* aAttacker, TInt aDamage,
                   TInt aWeapon, TBool aHeadshot, TInt aGroup)
    {
    if (!aVictim.iAlive) return;
    TExtraPlayer& victim = Ex(aVictim);
    if (iMode == 2 && aAttacker && aAttacker != &aVictim)
        {
        TExtraPlayer& attacker = Ex(*aAttacker);
        if (attacker.iZombie && !victim.iZombie)
            {
            if (aWeapon != 0) return;
            TInt damage = KZombieDamage[BoundedIndex(attacker.iZType, 3)];
            if (aVictim.iArmor > 0)
                {
                const TInt absorbed = MinI((TInt)aVictim.iArmor, damage / 2);
                aVictim.iArmor -= absorbed;
                damage -= absorbed;
                }
            if (aVictim.iHealth - damage <= 0)
                {
                const TInt type = iRandom.NextInt(3);
                victim.iZombie = ETrue;
                victim.iZType = type;
                aVictim.iTeam = 1;
                aVictim.iHealth = (TReal32)KZombieHealth[type];
                aVictim.iArmor = 0;
                aVictim.iWeapon = 0;
                aVictim.iPrimary = -1;
                aVictim.iSecondary = -1;
                aVictim.iHasC4 = EFalse;
                TBuf<96> text;
                text.Copy(TPtrC(KZombieNames[type]));
                Center(text, 4.0f, 0x60FF60);
                CheckRoundEnd();
                return;
                }
            aVictim.iHealth -= damage;
            victim.iLastAttacker = aAttacker->iId;
            if (&aVictim == &iPlayers[iMyIndex])
                {
                iHurtFlash = 0.25f;
                victim.iPunchPitch -= 2.0f;
                }
            PlaySoundAt(aVictim.iX, aVictim.iY, aVictim.iZ, Ascii("player_bhit_flesh_1"), 80, 1800);
            return;
            }
        if (attacker.iZombie == victim.iZombie) return;
        }
    if (aAttacker && aAttacker->iTeam == aVictim.iTeam && aAttacker != &aVictim)
        {
        if (!gState.iFriendlyFire) return;
        aDamage = aDamage * 35 / 100;
        }
    if (aVictim.iArmor > 0 && aWeapon >= 0 && aWeapon < KWeaponCount
        && aGroup != 4 && (aGroup != 1 || victim.iHelmet))
        {
        const TReal32 ratio = KWpArmorR[aWeapon] * 0.5f;
        TReal32 newDamage = aDamage * ratio;
        TReal32 armorLoss = (aDamage - newDamage) * 0.5f;
        if (armorLoss > aVictim.iArmor)
            {
            newDamage = aDamage - aVictim.iArmor * 2.0f;
            armorLoss = aVictim.iArmor;
            }
        aVictim.iArmor -= (TInt)armorLoss;
        if (aVictim.iArmor < 0) aVictim.iArmor = 0;
        aDamage = (TInt)newDamage;
        }
    if (aDamage < 1) aDamage = 1;
    aVictim.iHealth -= aDamage;
    victim.iLastHurt = iTime;
    if (aAttacker)
        {
        victim.iLastAttacker = aAttacker->iId;
        victim.iLastHurtYaw = FloatAtan2(aAttacker->iY - aVictim.iY, aAttacker->iX - aVictim.iX);
        victim.iHeardX = aAttacker->iX;
        victim.iHeardY = aAttacker->iY;
        victim.iHeardAt = iTime;
        }
    victim.iPunchPitch -= aHeadshot ? 4.0f : 1.5f;
    if (&aVictim == &iPlayers[iMyIndex]) iHurtFlash = 0.25f;
    PlaySoundAt(aVictim.iX, aVictim.iY, aVictim.iZ,
                Ascii(aHeadshot ? (victim.iHelmet ? "player_bhit_helmet_1" : "player_headshot1")
                                : (aVictim.iArmor > 0 ? "player_bhit_kevlar_1" : "player_bhit_flesh_1")),
                70, 1800);
    if (aVictim.iHealth <= 0) Kill(aVictim, aAttacker, aWeapon, aHeadshot);
    }

void CGame::Kill(TPlayer& aVictim, TPlayer* aAttacker, TInt aWeapon, TBool aHeadshot)
    {
    if (iMode == 1 && aAttacker && aAttacker != &aVictim && aAttacker->iTeam != aVictim.iTeam)
        {
        if (aAttacker->iTeam == 1) iScoreT++;
        else iScoreCT++;
        }
    aVictim.iAlive = EFalse;
    aVictim.iHealth = 0;
    aVictim.iDeaths++;
    aVictim.iDeathTime = iTime;
    aVictim.iAnimTime = 0;
    aVictim.iPlanting = EFalse;
    Ex(aVictim).iDefusing = EFalse;
    PlaySoundAt(aVictim.iX, aVictim.iY, aVictim.iZ,
                Ascii((iRandom.Next() & 1) ? "player_die3" : "player_die1"), 80, 2200);
    if (aVictim.iHasC4)
        {
        aVictim.iHasC4 = EFalse;
        iBombDropped = ETrue;
        iBombX = aVictim.iX;
        iBombY = aVictim.iY;
        iBombZ = GroundHeight(aVictim.iX, aVictim.iY, aVictim.iZ);
        iBombDroppedBy = aVictim.iId;
        iBombDropTime = iTime;
        }
    const TInt weapon = aVictim.iPrimary >= 0 ? aVictim.iPrimary : aVictim.iSecondary;
    if (weapon >= 0 && (aVictim.iClip[weapon] > 0 || aVictim.iAmmo[weapon] > 0))
        SpawnDrop(aVictim, weapon, 120.0f);
    if (aAttacker && aAttacker != &aVictim)
        {
        if (iMode == 3)
            {
            const TInt termIdx = (iTerminatorIdx >= 0 && iTerminatorIdx < iPlayerCount) ? iTerminatorIdx : -1;
            if (termIdx >= 0 && aAttacker == &iPlayers[termIdx] && iIsDisguised) { iIsDisguised = EFalse; iDisguiseTimer = 0; }
            }
        if (aAttacker->iTeam != aVictim.iTeam)
            {
            aAttacker->iKills++;
            AddMoney(*aAttacker, 300);
            }
        else
            {
            aAttacker->iKills--;
            AddMoney(*aAttacker, -3300);
            }
        if (aAttacker == &iPlayers[iMyIndex] && aAttacker->iTeam != aVictim.iTeam)
            PlaySound(Ascii("radio_enemydown"), 45);
        else if (aAttacker->iBot && aAttacker->iTeam != aVictim.iTeam && (iRandom.Next() & 3) == 0)
            AddRadioLater(gState, aAttacker->iId, 20, iTime + 0.6f);
        }
    for (TInt i = KMaxChatLines - 1; i > 0; i--)
        {
        iKillFeed[i][0] = iKillFeed[i - 1][0];
        iKillFeed[i][1] = iKillFeed[i - 1][1];
        iKillFeed[i][2] = iKillFeed[i - 1][2];
        iKillFeedTime[i] = iKillFeedTime[i - 1];
        }
    iKillFeed[0][0] = (aAttacker && aAttacker != &aVictim) ? aAttacker->iId : -1;
    iKillFeed[0][1] = aVictim.iId;
    iKillFeed[0][2] = aWeapon | (aHeadshot ? 256 : 0);
    iKillFeedTime[0] = iTime;
    if (&aVictim == &iPlayers[iMyIndex])
        {
        iSpectatorIndex = -1;
        iBuyOpen = EFalse;
        aVictim.iZoom = 0;
        }
    if (iNetworkHost && !gState.iApplying && gState.iMpPhase == 2 && iConnection && gState.iRoomPlayers > 1)
        {
        iNetOut.Reset();
        iNetOut.U8('K')
               .U8(aAttacker && aAttacker != &aVictim ? aAttacker->iId : 255)
               .U8(aVictim.iId).U8(aWeapon).U8(aHeadshot ? 1 : 0);
        iConnection->Send(EServerFromHost, iNetOut);
        }
    }

// ------------------------------------------------------------------ предметы и эффекты

void CGame::AddEffect(TReal32 aX, TReal32 aY, TReal32 aZ, TInt aColor,
                      TReal32 aLife, TInt aSize, TInt aType)
    {
    TInt index = gState.iEffectHead;
    gState.iEffectHead = (gState.iEffectHead + 1) % KMaxEffects;
    TEffect& effect = iEffects[index];
    effect.iActive = ETrue;
    effect.iType = aType;
    effect.iX = aX;
    effect.iY = aY;
    effect.iZ = aZ;
    effect.iVX = 0;
    effect.iVY = 0;
    effect.iVZ = aType == 2 ? 60.0f : 0.0f;
    effect.iStart = iTime;
    effect.iLife = aLife;
    effect.iColor = aColor;
    effect.iSize = aSize;
    }

void CGame::SpawnDrop(TPlayer& aPlayer, TInt aWeapon, TReal32 aSpeed)
    {
    TInt slot = -1;
    TReal32 oldest = 1e9f;
    for (TInt i = 0; i < KMaxDrops; i++)
        {
        if (!iDrops[i].iActive) { slot = i; break; }
        if (iDrops[i].iTime < oldest) { oldest = iDrops[i].iTime; slot = i; }
        }
    if (slot < 0) return;
    TDrop& drop = iDrops[slot];
    TDropExtra& extra = gDropExtra[slot];
    const TReal32 cy = FloatCos(aPlayer.iYaw), sy = FloatSin(aPlayer.iYaw);
    drop.iActive = ETrue;
    drop.iWeapon = aWeapon;
    extra.iClip = aPlayer.iClip[aWeapon];
    extra.iAmmo = aPlayer.iAmmo[aWeapon];
    extra.iOwner = aPlayer.iId;
    drop.iTime = iTime;
    extra.iRest = EFalse;
    const TExtraPlayer& source = Ex(aPlayer);
    extra.iSilent = (aWeapon == 17 && source.iSilM4) || (aWeapon == 2 && source.iSilUSP);
    drop.iX = aPlayer.iX + cy * 12.0f;
    drop.iY = aPlayer.iY + sy * 12.0f;
    drop.iZ = EyeZ(aPlayer) - 10.0f;
    drop.iVelocityX = cy * aSpeed + aPlayer.iVelocityX;
    drop.iVelocityY = sy * aSpeed + aPlayer.iVelocityY;
    drop.iVelocityZ = aSpeed * 0.6f;
    extra.iYaw = aPlayer.iYaw + RandomReal(iRandom, -40.0f, 40.0f);
    }

void CGame::DropCurrent(TPlayer& aPlayer)
    {
    const TInt weapon = aPlayer.iWeapon;
    if (weapon == 28 && aPlayer.iHasC4)
        {
        aPlayer.iHasC4 = EFalse;
        iBombDropped = ETrue;
        iBombX = aPlayer.iX + FloatCos(aPlayer.iYaw) * 30.0f;
        iBombY = aPlayer.iY + FloatSin(aPlayer.iYaw) * 30.0f;
        iBombZ = GroundHeight(iBombX, iBombY, aPlayer.iZ);
        iBombDroppedBy = aPlayer.iId;
        iBombDropTime = iTime;
        }
    else if (IsGun(weapon))
        {
        SpawnDrop(aPlayer, weapon, 250.0f);
        if (weapon == aPlayer.iPrimary) aPlayer.iPrimary = -1;
        else if (weapon == aPlayer.iSecondary) aPlayer.iSecondary = -1;
        PlaySoundAt(aPlayer.iX, aPlayer.iY, aPlayer.iZ, Ascii("items_weapondrop1"), 50, 1800);
        }
    else
        {
        if (&aPlayer == &iPlayers[iMyIndex]) Center(_L("Это нельзя выбросить"), 1.2f, 0xFFFFFF);
        return;
        }
    aPlayer.iWeapon = 0;
    aPlayer.iReloading = EFalse;
    aPlayer.iZoom = 0;
    const TInt next = aPlayer.iPrimary >= 0 ? aPlayer.iPrimary
        : (aPlayer.iSecondary >= 0 ? aPlayer.iSecondary : 0);
    aPlayer.iWeapon = -1;
    SwitchWeapon(aPlayer, next);
    }

void CGame::UpdateDrops(TReal32 aDelta)
    {
    CMap* map = iAssets.iMap;
    for (TInt i = 0; i < KMaxDrops; i++)
        {
        TDrop& drop = iDrops[i];
        TDropExtra& extra = gDropExtra[i];
        if (!drop.iActive) continue;
        if (!extra.iRest && map)
            {
            drop.iVelocityZ -= 800.0f * aDelta;
            const TReal32 nx = drop.iX + drop.iVelocityX * aDelta;
            const TReal32 ny = drop.iY + drop.iVelocityY * aDelta;
            const TReal32 nz = drop.iZ + drop.iVelocityZ * aDelta;
            map->Trace(0, drop.iX, drop.iY, drop.iZ, nx, ny, nz);
            if (map->rFrac < 1.0f)
                {
                drop.iX = map->rEx + map->rNx;
                drop.iY = map->rEy + map->rNy;
                drop.iZ = map->rEz + map->rNz;
                if (map->rNz > 0.7f)
                    {
                    extra.iRest = ETrue;
                    drop.iZ = map->rEz + 1.0f;
                    }
                else
                    {
                    const TReal32 dot = drop.iVelocityX * map->rNx + drop.iVelocityY * map->rNy
                        + drop.iVelocityZ * map->rNz;
                    drop.iVelocityX = (drop.iVelocityX - 2.0f * dot * map->rNx) * 0.3f;
                    drop.iVelocityY = (drop.iVelocityY - 2.0f * dot * map->rNy) * 0.3f;
                    drop.iVelocityZ = (drop.iVelocityZ - 2.0f * dot * map->rNz) * 0.3f;
                    }
                }
            else
                {
                drop.iX = nx; drop.iY = ny; drop.iZ = nz;
                }
            if (iTime - drop.iTime > 4.0f) extra.iRest = ETrue;
            }
        for (TInt k = 0; k < iPlayerCount; k++)
            {
            TPlayer& player = iPlayers[k];
            if (!player.iAlive || Ex(player).iZombie) continue;
            if (extra.iOwner == player.iId && iTime - drop.iTime < 1.5f) continue;
            const TReal32 dx = player.iX - drop.iX, dy = player.iY - drop.iY;
            const TReal32 dz = (player.iZ - (player.iDucked ? 18.0f : 36.0f)) - drop.iZ;
            if (dx * dx + dy * dy > 36.0f * 36.0f || dz > 40.0f || dz < -40.0f) continue;
            const TInt weapon = drop.iWeapon;
            const TBool primary = KWpSlot[weapon] == 0;
            if ((primary && player.iPrimary >= 0) || (!primary && player.iSecondary >= 0)) continue;
            if (primary) player.iPrimary = weapon;
            else player.iSecondary = weapon;
            player.iClip[weapon] = extra.iClip;
            player.iAmmo[weapon] = extra.iAmmo;
            TExtraPlayer& target = Ex(player);
            if (weapon == 17) target.iSilM4 = extra.iSilent;
            if (weapon == 2) target.iSilUSP = extra.iSilent;
            drop.iActive = EFalse;
            PlaySoundAt(player.iX, player.iY, player.iZ, Ascii("items_gunpickup2"), 60, 1800);
            if (&player == &iPlayers[iMyIndex])
                {
                TBuf<64> text;
                TBuf<48> weaponName;
                text.Copy(_L("Подобрано: "));
                SetAscii(weaponName, KWeaponNames[weapon]);
                text.Append(weaponName);
                Center(text, 1.5f, 0xFFFFFF);
                }
            if (player.iBot || player.iWeapon == 0 || (!primary && player.iWeapon == player.iSecondary))
                SwitchWeapon(player, weapon);
            break;
            }
        }
    }

void CGame::UpdateGrenades(TReal32 aDelta)
    {
    CMap* map = iAssets.iMap;
    for (TInt i = 0; i < KMaxGrenades; i++)
        {
        TGrenade& grenade = iGrenades[i];
        if (!grenade.iActive) continue;
        grenade.iVelocityZ -= 800.0f * 0.4f * aDelta;
        const TReal32 nx = grenade.iX + grenade.iVelocityX * aDelta;
        const TReal32 ny = grenade.iY + grenade.iVelocityY * aDelta;
        const TReal32 nz = grenade.iZ + grenade.iVelocityZ * aDelta;
        if (map)
            {
            map->Trace(0, grenade.iX, grenade.iY, grenade.iZ, nx, ny, nz);
            if (map->rFrac < 1.0f)
                {
                const TReal32 dot = grenade.iVelocityX * map->rNx + grenade.iVelocityY * map->rNy
                    + grenade.iVelocityZ * map->rNz;
                grenade.iVelocityX = (grenade.iVelocityX - 2.0f * dot * map->rNx) * 0.45f;
                grenade.iVelocityY = (grenade.iVelocityY - 2.0f * dot * map->rNy) * 0.45f;
                grenade.iVelocityZ = (grenade.iVelocityZ - 2.0f * dot * map->rNz) * 0.45f;
                grenade.iX = map->rEx + map->rNx;
                grenade.iY = map->rEy + map->rNy;
                grenade.iZ = map->rEz + map->rNz;
                if (AbsF(dot) > 80.0f)
                    PlaySoundAt(grenade.iX, grenade.iY, grenade.iZ, Ascii("weapons_he_bounce_1"), 50, 1200);
                }
            else
                {
                grenade.iX = nx; grenade.iY = ny; grenade.iZ = nz;
                }
            }
        else
            {
            grenade.iX = nx; grenade.iY = ny; grenade.iZ = nz;
            }
        if (iTime >= grenade.iTimer)
            {
            grenade.iActive = EFalse;
            const TInt type = grenade.iType;
            const TReal32 x = grenade.iX, y = grenade.iY, z = grenade.iZ;
            TPlayer* owner = PlayerById(grenade.iOwner);
            if (type == 25)
                {
                PlaySoundAt(x, y, z, Ascii("weapons_hegrenade_1"), 100, 3000);
                for (TInt k = 0; k < 6; k++)
                    AddEffect(x + RandomReal(iRandom, -40.0f, 40.0f),
                              y + RandomReal(iRandom, -40.0f, 40.0f),
                              z + RandomReal(iRandom, 0.0f, 50.0f), 0xFFA030, 0.5f, 30, 0);
                for (TInt k = 0; k < iPlayerCount; k++)
                    {
                    TPlayer& target = iPlayers[k];
                    if (!target.iAlive) continue;
                    const TReal32 dx = target.iX - x, dy = target.iY - y, dz = target.iZ - z;
                    const TReal32 distance = FloatSqrt(dx * dx + dy * dy + dz * dz);
                    if (distance > 350.0f) continue;
                    if (map)
                        {
                        map->Trace(0, x, y, z + 8.0f, target.iX, target.iY, target.iZ);
                        if (map->rFrac < 1.0f) continue;
                        }
                    const TInt damage = (TInt)(100.0f * (1.0f - distance / 350.0f));
                    if (damage > 0) Damage(target, owner, damage, 25, EFalse, 2);
                    }
                }
            else if (type == 26)
                {
                PlaySoundAt(x, y, z, Ascii("weapons_flashbang_1"), 100, 2500);
                AddEffect(x, y, z, 0xFFFFFF, 0.2f, 40, 0);
                for (TInt k = 0; k < iPlayerCount; k++)
                    {
                    TPlayer& target = iPlayers[k];
                    if (!target.iAlive) continue;
                    const TReal32 dx = x - target.iX, dy = y - target.iY, dz = z - EyeZ(target);
                    const TReal32 distance = FloatSqrt(dx * dx + dy * dy + dz * dz);
                    if (distance > 1500.0f) continue;
                    if (map)
                        {
                        map->Trace(0, target.iX, target.iY, EyeZ(target), x, y, z);
                        if (map->rFrac < 0.99f) continue;
                        }
                    ForwardVector(target.iYaw, target.iPitch, gTempForward);
                    const TReal32 dot = (dx * gTempForward[0] + dy * gTempForward[1] + dz * gTempForward[2])
                        / MaxF(1.0f, distance);
                    TReal32 duration = (dot > 0.3f ? 4.5f : (dot > -0.3f ? 2.5f : 1.0f))
                        * (1.0f - distance / 1800.0f);
                    if (duration > 0.3f)
                        {
                        TExtraPlayer& targetExtra = Ex(target);
                        targetExtra.iFlashStart = iTime;
                        targetExtra.iFlashUntil = iTime + duration;
                        targetExtra.iFlashedUntil = iTime + duration * 0.8f;
                        target.iFlashTime = duration;
                        }
                    }
                }
            else if (type == 27)
                {
                PlaySoundAt(x, y, z, Ascii("weapons_sg_explode"), 80, 1500);
                for (TInt k = 0; k < KMaxSmokes; k++)
                    {
                    if (gState.iSmokeOn[k]) continue;
                    gState.iSmokeOn[k] = ETrue;
                    gState.iSmokeX[k] = x;
                    gState.iSmokeY[k] = y;
                    gState.iSmokeZ[k] = z + 40.0f;
                    gState.iSmokeUntil[k] = iTime + 18.0f;
                    gState.iSmokeCount = MaxI(gState.iSmokeCount, k + 1);
                    break;
                    }
                }
            }
        }
    for (TInt i = 0; i < KMaxSmokes; i++)
        {
        if (gState.iSmokeOn[i] && iTime > gState.iSmokeUntil[i]) gState.iSmokeOn[i] = EFalse;
        }
    }

void CGame::UpdateEffects(TReal32 aDelta)
    {
    (void)aDelta;
    for (TInt i = 0; i < KMaxEffects; i++)
        if (iEffects[i].iActive && iTime - iEffects[i].iStart > iEffects[i].iLife)
            iEffects[i].iActive = EFalse;
    }

// ------------------------------------------------------------------ связь и звук

void CGame::Radio(TPlayer& aPlayer, TInt aCommand)
    {
    if (aCommand < 0 || aCommand >= 21 || !aPlayer.iAlive) return;
    if (!gState.iApplying && !iNetworkHost && gState.iMpPhase == 2 && iConnection)
        {
        SendClientAction(*this, 'r', aCommand, -1);
        return;
        }
    if (iTime - aPlayer.iLastRadio < 1.2f) return;
    aPlayer.iLastRadio = iTime;
    aPlayer.iRadio = aCommand;
    if (&aPlayer == &iPlayers[iMyIndex] || aPlayer.iTeam == iPlayers[iMyIndex].iTeam)
        {
        PlaySound(Ascii(KRadioSound[aCommand]), 85);
        TBuf<160> text;
        TBuf<48> commandName;
        text.Copy(TPtrC(aPlayer.iName));
        text.Append(_L(" (РАДИО): "));
        SetAscii(commandName, KRadioSound[aCommand]);
        text.Append(commandName);
        Chat(text, TeamColor(aPlayer.iTeam));
        }
    for (TInt i = 0; i < iPlayerCount; i++)
        {
        TPlayer& bot = iPlayers[i];
        if (!bot.iBot || !bot.iAlive || &bot == &aPlayer || bot.iTeam != aPlayer.iTeam) continue;
        BotRadioReaction(gState, bot, aPlayer, aCommand, iTime);
        }
    if (iNetworkHost && !gState.iApplying && gState.iMpPhase == 2 && iConnection
        && gState.iRoomPlayers > 1)
        {
        iNetOut.Reset();
        iNetOut.U8('R').U8(aPlayer.iId).U8(aCommand);
        iConnection->Send(EServerFromHost, iNetOut);
        }
    }

void CGame::Chat(const TDesC& aLine, TInt aColor)
    {
    for (TInt i = KMaxChatLines - 1; i > 0; i--)
        {
        TPtr line(iChatLines[i], 160);
        line.Copy(TPtrC(iChatLines[i - 1]));
        iChatColors[i] = iChatColors[i - 1];
        }
    CopyText(iChatLines[0], KMaxChatLines ? 160 : 0, aLine);
    iChatColors[0] = aColor;
    iChatHead = 0;
    if (iNetworkHost && !gState.iApplying && gState.iMpPhase == 2 && iConnection && gState.iRoomPlayers > 1)
        {
        iNetOut.Reset();
        iNetOut.U8('T').Str(aLine).I32((TUint32)aColor);
        iConnection->Send(EServerFromHost, iNetOut);
        }
    }

void CGame::Center(const TDesC& aLine, TReal32 aDuration, TInt aColor)
    {
    CopyText(iCenterMessage, sizeof(iCenterMessage) / sizeof(TText), aLine);
    iCenterMessageUntil = iTime + aDuration;
    iCenterMessageColor = aColor;
    iCenterMessageTime = aDuration;
    if (iNetworkHost && !gState.iApplying && gState.iMpPhase == 2 && iConnection && gState.iRoomPlayers > 1)
        {
        iNetOut.Reset();
        iNetOut.U8('C').Str(aLine).U8(ClampByte((TInt)(aDuration * 10.0f))).I32((TUint32)aColor);
        iConnection->Send(EServerFromHost, iNetOut);
        }
    }

void CGame::PlaySound(const TPtrC8& aName, TInt aVolume)
    {
    if (!iSoundOn) return;
    iAudio.Play(aName, Clamp(aVolume * iVolume / 100, 0, 100));
    }

void CGame::PlaySoundAt(TReal32 aX, TReal32 aY, TReal32 aZ, const TPtrC8& aName,
                        TInt aVolume, TReal32 aRange)
    {
    if (!iSoundOn) return;
    const TPlayer& listener = iPlayers[iMyIndex];
    const TReal32 dx = aX - listener.iX, dy = aY - listener.iY, dz = aZ - EyeZ(listener);
    const TReal32 distance = FloatSqrt(dx * dx + dy * dy + dz * dz);
    const TReal32 attenuation = 1.0f - distance / MaxF(1.0f, aRange);
    if (iNetworkHost && !gState.iApplying && gState.iMpPhase == 2 && iConnection
        && gState.iRoomPlayers > 1 && attenuation > 0)
        {
        iNetOut.Reset();
        iNetOut.U8('N').U8(255).I16Float(aX).I16Float(aY).I16Float(aZ)
               .U8(ClampByte(aVolume)).U16((TInt)MaxF(0.0f, aRange)).Str(aName);
        iConnection->Send(EServerFromHost, iNetOut);
        }
    if (attenuation <= 0) return;
    iAudio.Play(aName, Clamp((TInt)(aVolume * attenuation) * iVolume / 100, 0, 100));
    }

// ------------------------------------------------------------------helpers

static TInt PlayerIndexById(const CGame& aGame, TInt aId)
    {
    if (aId < 0) return -1;
    for (TInt i = 0; i < aGame.iPlayerCount; i++)
        if (aGame.iPlayers[i].iId == aId) return i;
    return -1;
    }

static TPlayer* PlayerByNet(CGame& aGame, TInt aNetId)
    {
    if (aNetId <= 0) return NULL;
    for (TInt i = 0; i < aGame.iPlayerCount; i++)
        {
        TPlayer& player = aGame.iPlayers[i];
        if (player.iNetId == aNetId && Ex(player).iRemote) return &player;
        }
    return NULL;
    }

static void DirectSwitch(TPlayer& aPlayer, TInt aWeapon, TReal32 aTime)
    {
    if (!HasWeapon(aPlayer, aWeapon) || aWeapon < 0 || aWeapon >= KWeaponCount) return;
    if (aPlayer.iWeapon == aWeapon) return;
    aPlayer.iWeapon = aWeapon;
    aPlayer.iReloading = EFalse;
    aPlayer.iZoom = 0;
    Ex(aPlayer).iDrawEnd = aTime + KWpDrawT[aWeapon];
    }

static void SimpleDrop(TPlayer& aPlayer, TInt aWeapon, TReal32 aSpeed, TReal32 aTime)
    {
    if (!gGame || aWeapon < 0 || aWeapon >= KWeaponCount) return;
    TInt slot = -1;
    TReal64 oldest = 0;
    for (TInt i = 0; i < KMaxDrops; i++)
        {
        TDrop& candidate = gGame->iDrops[i];
        if (!candidate.iActive)
            {
            slot = i;
            break;
            }
        if (slot < 0 || candidate.iTime < oldest)
            {
            oldest = candidate.iTime;
            slot = i;
            }
        }
    if (slot < 0) return;
    TDrop& drop = gGame->iDrops[slot];
    TDropExtra& extra = gDropExtra[slot];
    drop.iActive = ETrue;
    drop.iWeapon = aWeapon;
    extra.iClip = aPlayer.iClip[aWeapon];
    extra.iAmmo = aPlayer.iAmmo[aWeapon];
    extra.iOwner = aPlayer.iId;
    extra.iSilent = (aWeapon == 17 && Ex(aPlayer).iSilM4) || (aWeapon == 2 && Ex(aPlayer).iSilUSP);
    extra.iRest = EFalse;
    extra.iYaw = aPlayer.iYaw + RandomReal(gGame->iRandom, -40.0f, 40.0f);
    drop.iX = aPlayer.iX + FloatCos(aPlayer.iYaw) * 12.0f;
    drop.iY = aPlayer.iY + FloatSin(aPlayer.iYaw) * 12.0f;
    drop.iZ = EyeZ(aPlayer) - 10.0f;
    drop.iVelocityX = FloatCos(aPlayer.iYaw) * aSpeed + aPlayer.iVelocityX;
    drop.iVelocityY = FloatSin(aPlayer.iYaw) * aSpeed + aPlayer.iVelocityY;
    drop.iVelocityZ = aSpeed * 0.6f;
    drop.iTime = aTime;
    }

static TBool BuyWeaponFor(CGame& aGame, TPlayer& aPlayer, TInt aWeapon)
    {
    if (aWeapon < 1 || aWeapon >= KWeaponCount) return EFalse;
    if (KWpTeam[aWeapon] != 0 && KWpTeam[aWeapon] != aPlayer.iTeam) return EFalse;
    if (aPlayer.iMoney < KWpPrice[aWeapon]) return EFalse;
    const TInt slot = KWpSlot[aWeapon];
    if (slot != 0 && slot != 1) return EFalse;
    if (slot == 0 && aPlayer.iPrimary >= 0 && aPlayer.iPrimary != aWeapon)
        SimpleDrop(aPlayer, aPlayer.iPrimary, 60.0f, aGame.iTime);
    if (slot == 1 && aPlayer.iSecondary >= 0 && aPlayer.iSecondary != aWeapon)
        SimpleDrop(aPlayer, aPlayer.iSecondary, 60.0f, aGame.iTime);
    if (aWeapon == 17) Ex(aPlayer).iSilM4 = EFalse;
    if (aWeapon == 2) Ex(aPlayer).iSilUSP = EFalse;
    if (slot == 0)
        {
        if (aPlayer.iPrimary == aWeapon) return EFalse;
        aPlayer.iPrimary = aWeapon;
        }
    else
        {
        if (aPlayer.iSecondary == aWeapon) return EFalse;
        aPlayer.iSecondary = aWeapon;
        }
    aPlayer.iMoney -= KWpPrice[aWeapon];
    aPlayer.iClip[aWeapon] = KWpClip[aWeapon];
    aPlayer.iAmmo[aWeapon] = 0;
    const TInt reserve = KWpReserve[aWeapon];
    const TInt perClip = MaxI(1, KWpClip[aWeapon]);
    const TInt price = MaxI(1, KWpAmmoPrice[aWeapon]);
    const TInt clips = reserve / perClip;
    for (TInt i = 0; i < clips && aPlayer.iMoney >= price && aPlayer.iAmmo[aWeapon] < reserve; i++)
        {
        aPlayer.iMoney -= price;
        aPlayer.iAmmo[aWeapon] = MinI(reserve, aPlayer.iAmmo[aWeapon] + perClip);
        }
    DirectSwitch(aPlayer, aWeapon, aGame.iTime);
    return ETrue;
    }

static TBool BuyEquipFor(TPlayer& aPlayer, TInt aKind)
    {
    TInt price = 99999;
    if (aKind == 0) price = 650;
    else if (aKind == 1) price = aPlayer.iArmor >= 100 ? 350 : 1000;
    else if (aKind == 2) price = 200;
    else if (aKind == 3) price = 300;
    else if (aKind == 4) price = 300;
    else if (aKind == 5) price = 200;
    if (aPlayer.iMoney < price) return EFalse;
    TExtraPlayer& x = Ex(aPlayer);
    if (aKind == 0)
        {
        if (aPlayer.iArmor >= 100) return EFalse;
        aPlayer.iArmor = 100;
        }
    else if (aKind == 1)
        {
        if (aPlayer.iArmor >= 100 && x.iHelmet) return EFalse;
        aPlayer.iArmor = 100;
        x.iHelmet = ETrue;
        }
    else if (aKind == 2)
        {
        if (x.iFlash >= 2) return EFalse;
        x.iFlash++;
        }
    else if (aKind == 3)
        {
        if (x.iHe >= 1) return EFalse;
        x.iHe++;
        }
    else if (aKind == 4)
        {
        if (x.iSmoke >= 1) return EFalse;
        x.iSmoke++;
        }
    else if (aKind == 5)
        {
        if (aPlayer.iTeam != 2 || x.iDefuser) return EFalse;
        x.iDefuser = ETrue;
        }
    else return EFalse;
    aPlayer.iMoney -= price;
    return ETrue;
    }

static void BotBuy(CGame& aGame, TPlayer& aPlayer)
    {
    TExtraPlayer& x = Ex(aPlayer);
    x.iBought = ETrue;
    const TInt money = aPlayer.iMoney;
    const TInt round = aGame.iRound;
    if (aPlayer.iTeam == 1)
        {
        if (x.iSniper && money >= 5750) BuyWeaponFor(aGame, aPlayer, 21);
        else if (money >= 3500) BuyWeaponFor(aGame, aPlayer, 16);
        else if (money >= 2700 && aPlayer.iPrimary < 0) BuyWeaponFor(aGame, aPlayer, 16);
        else if (money >= 2000 && aPlayer.iPrimary < 0 && round > 1) BuyWeaponFor(aGame, aPlayer, 14);
        else if (money >= 1400 && aPlayer.iPrimary < 0 && round > 2 && (aGame.iRandom.Next() & 1) == 0)
            BuyWeaponFor(aGame, aPlayer, 9);
        }
    else
        {
        if (x.iSniper && money >= 5750) BuyWeaponFor(aGame, aPlayer, 21);
        else if (money >= 4100) BuyWeaponFor(aGame, aPlayer, 17);
        else if (money >= 3100 && aPlayer.iPrimary < 0) BuyWeaponFor(aGame, aPlayer, 17);
        else if (money >= 2250 && aPlayer.iPrimary < 0 && round > 1) BuyWeaponFor(aGame, aPlayer, 15);
        else if (money >= 1500 && aPlayer.iPrimary < 0 && round > 2 && (aGame.iRandom.Next() & 1) == 0)
            BuyWeaponFor(aGame, aPlayer, 11);
        }
    if (aPlayer.iPrimary < 0 && aPlayer.iMoney >= 650 && round == 1
        && (aGame.iRandom.Next() & 1) == 0 && aPlayer.iSecondary != 4)
        BuyWeaponFor(aGame, aPlayer, 4);
    if (aPlayer.iMoney >= 1000) BuyEquipFor(aPlayer, 1);
    else if (aPlayer.iMoney >= 650) BuyEquipFor(aPlayer, 0);
    if (aPlayer.iTeam == 2 && aPlayer.iMoney >= 200 && aPlayer.iPrimary >= 0) BuyEquipFor(aPlayer, 5);
    if (aPlayer.iMoney >= 300 && (aGame.iRandom.Next() & 1) == 0) BuyEquipFor(aPlayer, 3);
    if (aPlayer.iMoney >= 200 && (aGame.iRandom.Next() & 3) == 0) BuyEquipFor(aPlayer, 2);
    if (aPlayer.iMoney >= 300 && (aGame.iRandom.Next() & 3) == 0) BuyEquipFor(aPlayer, 4);
    if (aPlayer.iPrimary == 17 && (aPlayer.iId & 1) == 0) x.iSilM4 = ETrue;
    if (aPlayer.iPrimary >= 0) DirectSwitch(aPlayer, aPlayer.iPrimary, aGame.iTime);
    else if (aPlayer.iSecondary >= 0) DirectSwitch(aPlayer, aPlayer.iSecondary, aGame.iTime);
    }

static TInt BotArea(const TPlayer& aPlayer)
    {
    return gNav->Find(aPlayer.iX, aPlayer.iY, aPlayer.iZ - (aPlayer.iDucked ? 18.0f : 36.0f));
    }

static TBool BotInArea(const TPlayer& aPlayer, TInt aArea)
    {
    if (aArea < 0 || aArea >= gNav->iCount) return EFalse;
    return aPlayer.iX >= gNav->iX0[aArea] + 6 && aPlayer.iX <= gNav->iX1[aArea] - 6
        && aPlayer.iY >= gNav->iY0[aArea] + 6 && aPlayer.iY <= gNav->iY1[aArea] - 6
        && AbsF(gNav->ZAt(aArea, aPlayer.iX, aPlayer.iY)
               - (aPlayer.iZ - (aPlayer.iDucked ? 18.0f : 36.0f))) < 40.0f;
    }

static TBool BotGo(CGame& aGame, TPlayer& aPlayer, TExtraPlayer& aExtra, TInt aArea)
    {
    const TInt start = BotArea(aPlayer);
    const TInt count = gNav->Path(start, aArea, aExtra.iPath);
    aExtra.iPathLength = count;
    aExtra.iPathIndex = 0;
    aExtra.iGoal = aArea;
    aExtra.iRepathAt = aGame.iTime + 6.0f;
    return count > 0;
    }

static TInt BotGoalForSite(CGame& aGame, TInt aSite)
    {
    if (!gNav || gNav->iCount <= 0) return -1;
    CMap* map = aGame.iAssets.iMap;
    if (!map || gState.iSiteCount <= 0) return gNav->Find(0, 0, 0);
    const TInt site = BoundedIndex(aSite, gState.iSiteCount);
    const TInt model = gState.iSiteModel[site];
    if (model < 0 || model >= map->numModels) return gNav->Find(0, 0, 0);
    const TReal32 cx = ModelCenterX(*map, model);
    const TReal32 cy = ModelCenterY(*map, model);
    const TReal32 sx = (map->mMaxs[model * 3] - map->mMins[model * 3]) * RandomReal(aGame.iRandom, -0.3f, 0.3f);
    const TReal32 sy = (map->mMaxs[model * 3 + 1] - map->mMins[model * 3 + 1]) * RandomReal(aGame.iRandom, -0.3f, 0.3f);
    return gNav->Find(cx + sx, cy + sy, map->mMins[model * 3 + 2]);
    }

static TBool BotWanderGoal(CGame& aGame, TPlayer& aPlayer, TExtraPlayer& aExtra)
    {
    for (TInt k = 0; k < 6 && gNav->iCount > 0; k++)
        {
        const TInt area = (TInt)(aGame.iRandom.Next() % (TUint32)gNav->iCount);
        if (BotGo(aGame, aPlayer, aExtra, area))
            {
            aExtra.iRepathAt = aGame.iTime + 10.0f + aGame.iRandom.NextReal() * 12.0f;
            return ETrue;
            }
        }
    return EFalse;
    }

static TInt BotHoldSpot(CGame& aGame, TPlayer& aPlayer, TExtraPlayer& aExtra)
    {
    CMap* map = aGame.iAssets.iMap;
    TReal32 cx = aPlayer.iX;
    TReal32 cy = aPlayer.iY;
    if (map && gState.iSiteCount > 0)
        {
        const TInt model = gState.iSiteModel[BoundedIndex(aExtra.iSite, gState.iSiteCount)];
        cx = ModelCenterX(*map, model);
        cy = ModelCenterY(*map, model);
        }
    for (TInt tries = 0; tries < 30 && gNav->iCount > 0; tries++)
        {
        const TInt area = (TInt)(aGame.iRandom.Next() % (TUint32)gNav->iCount);
        const TReal32 dx = gNav->iCX[area] - cx;
        const TReal32 dy = gNav->iCY[area] - cy;
        if (dx * dx + dy * dy < 700.0f * 700.0f && gNav->iSpotCount[area] > 0)
            {
            aExtra.iHoldYaw = AngleNorm(Atan2Deg(dy, dx));
            return area;
            }
        }
    return BotGoalForSite(aGame, aExtra.iSite);
    }

static TInt BotHuntTarget(CGame& aGame, TPlayer& aPlayer)
    {
    TExtraPlayer& x = Ex(aPlayer);
    if (aGame.iTime - x.iHeardAt < 6.0f)
        return gNav->Find(x.iHeardX, x.iHeardY, x.iHeardZ - 36.0f);
    const TInt count = aPlayer.iTeam == 1 ? gState.iSpawnCTCount : gState.iSpawnTCount;
    if (count > 0)
        {
        const TReal32* spawn = aPlayer.iTeam == 1 ? gState.iSpawnCT : gState.iSpawnT;
        const TInt k = ((TInt)(aGame.iRandom.Next() & 0xFFFF) % count) * 4;
        return gNav->Find(spawn[k], spawn[k + 1], spawn[k + 2] - 36.0f);
        }
    return -1;
    }

static void BotChooseGoal(CGame& aGame, TPlayer& aPlayer, TExtraPlayer& aExtra, TBool aArrived)
    {
    const TInt mode = gState.iMode;
    if (mode == 2)
        {
        aExtra.iLeader = -1;
        aExtra.iHoldHereUntil = 0;
        if (aExtra.iZombie)
            {
            TPlayer* target = NULL;
            TReal32 best = 1e12f;
            for (TInt i = 0; i < aGame.iPlayerCount; i++)
                {
                TPlayer& other = aGame.iPlayers[i];
                if (!other.iAlive || Ex(other).iZombie) continue;
                const TReal32 dx = other.iX - aPlayer.iX, dy = other.iY - aPlayer.iY;
                const TReal32 distance = dx * dx + dy * dy;
                if (distance < best) { best = distance; target = &other; }
                }
            if (target && BotGo(aGame, aPlayer, aExtra, gNav->Find(target->iX, target->iY, target->iZ - 36.0f)))
                {
                aExtra.iRepathAt = aGame.iTime + 1.5f + aGame.iRandom.NextReal();
                return;
                }
            }
        else if (BotWanderGoal(aGame, aPlayer, aExtra)) return;
        }
    if (mode == 1)
        {
        aExtra.iLeader = -1;
        aExtra.iHoldHereUntil = 0;
        if (BotWanderGoal(aGame, aPlayer, aExtra)) return;
        }
    if (mode == 3)
        {
        aExtra.iLeader = -1;
        aExtra.iHoldHereUntil = 0;
        if (aGame.iTermCircleActive && gNav && BotGo(aGame, aPlayer, aExtra, gNav->Find(aGame.iTermCircleX, aGame.iTermCircleY, aGame.iTermCircleZ - 36.0f)))
            { aExtra.iRepathAt = aGame.iTime + 1.5f; return; }
        if (aGame.iTerminatorIdx >= 0 && aGame.iTerminatorIdx < aGame.iPlayerCount)
            {
            TPlayer& term = aGame.iPlayers[aGame.iTerminatorIdx];
            if (&aPlayer == &term)
                {
                TPlayer* target = NULL; TReal32 bd = 1e12f;
                for (TInt i = 0; i < aGame.iPlayerCount; i++)
                    {
                    TPlayer& q = aGame.iPlayers[i];
                    if (!q.iAlive || &q == &aPlayer) continue;
                    const TReal32 dx = q.iX - aPlayer.iX, dy = q.iY - aPlayer.iY;
                    const TReal32 d = dx*dx + dy*dy;
                    if (d < bd) { bd = d; target = &q; }
                    }
                if (target && gNav && BotGo(aGame, aPlayer, aExtra, gNav->Find(target->iX, target->iY, target->iZ - 36.0f)))
                    { aExtra.iRepathAt = aGame.iTime + 2.0f; return; }
                }
            else if (term.iAlive && gNav && BotGo(aGame, aPlayer, aExtra, gNav->Find(term.iX, term.iY, term.iZ - 36.0f)))
                { aExtra.iRepathAt = aGame.iTime + 1.5f; return; }
            }
        }
    if (mode == 4 && aPlayer.iTeam == 2)
        {
        aExtra.iLeader = -1;
        aExtra.iHoldHereUntil = 0;
        TBool escort = EFalse;
        for (TInt h = 0; h < aGame.KMaxHostages; h++)
            {
            const TInt f = aGame.iHostageFollowIdx[h];
            if (aGame.iHostageAlive[h] && !aGame.iHostageRescued[h] && f >= 0 && f < aGame.iPlayerCount && &aGame.iPlayers[f] == &aPlayer) { escort = ETrue; break; }
            }
        TReal32 tx = aGame.iRescueZoneX, ty = aGame.iRescueZoneY, tz = aGame.iRescueZoneZ;
        if (!escort)
            {
            TReal32 bd = 1e12f;
            for (TInt h = 0; h < aGame.KMaxHostages; h++)
                {
                if (!aGame.iHostageAlive[h] || aGame.iHostageRescued[h] || aGame.iHostageFollowIdx[h] >= 0) continue;
                const TReal32 dx = aGame.iHostageX[h] - aPlayer.iX, dy = aGame.iHostageY[h] - aPlayer.iY;
                const TReal32 d = dx * dx + dy * dy;
                if (d < bd) { bd = d; tx = aGame.iHostageX[h]; ty = aGame.iHostageY[h]; tz = aGame.iHostageZ[h]; }
                }
            }
        if (gNav && BotGo(aGame, aPlayer, aExtra, gNav->Find(tx, ty, tz - 36.0f)))
            {
            aExtra.iRepathAt = aGame.iTime + 2.0f;
            return;
            }
        }
    if (aExtra.iLeader >= 0 && aExtra.iLeader < KMaxPlayers && aExtra.iFollowUntil > aGame.iTime && aExtra.iTask != 3)
        {
        TPlayer* leader = PlayerIndexById(aGame, aExtra.iLeader) >= 0
            ? &aGame.iPlayers[PlayerIndexById(aGame, aExtra.iLeader)] : NULL;
        if (leader && leader->iAlive)
            {
            const TReal32 dx = leader->iX - aPlayer.iX, dy = leader->iY - aPlayer.iY;
            if (dx * dx + dy * dy < 140.0f * 140.0f)
                {
                aExtra.iPathLength = 0;
                aExtra.iRepathAt = aGame.iTime + 1.0f;
                aExtra.iHoldYaw = leader->iYaw;
                return;
                }
            if (BotGo(aGame, aPlayer, aExtra, gNav->Find(leader->iX, leader->iY, leader->iZ - 36.0f)))
                {
                aExtra.iRepathAt = aGame.iTime + 2.0f;
                return;
                }
            }
        }
    if (aGame.iTime < aExtra.iHoldHereUntil && aExtra.iTask != 3)
        {
        aExtra.iPathLength = 0;
        aExtra.iRepathAt = aGame.iTime + 2.0f;
        aExtra.iHoldYaw = aPlayer.iYaw;
        aExtra.iTask = 1;
        aExtra.iHoldUntil = aExtra.iHoldHereUntil;
        return;
        }
    if (aPlayer.iPrimary < 0 && aGame.iTime > aExtra.iNextItemCheck)
        {
        aExtra.iNextItemCheck = aGame.iTime + 2.0f;
        TInt best = -1;
        TReal32 bestDistance = 900.0f * 900.0f;
        for (TInt i = 0; i < KMaxDrops; i++)
            {
            if (!aGame.iDrops[i].iActive || KWpSlot[aGame.iDrops[i].iWeapon] != 0) continue;
            const TReal32 dx = aGame.iDrops[i].iX - aPlayer.iX;
            const TReal32 dy = aGame.iDrops[i].iY - aPlayer.iY;
            const TReal32 distance = dx * dx + dy * dy;
            if (distance < bestDistance) { bestDistance = distance; best = i; }
            }
        if (best >= 0 && BotGo(aGame, aPlayer, aExtra,
            gNav->Find(aGame.iDrops[best].iX, aGame.iDrops[best].iY, aGame.iDrops[best].iZ)))
            {
            aExtra.iRepathAt = aGame.iTime + 3.0f;
            return;
            }
        }
    TInt target = -1;
    switch (aExtra.iTask)
        {
        case 0:
            if (aArrived && (aPlayer.iTeam == 2 || !aPlayer.iHasC4))
                {
                aExtra.iTask = 1;
                aExtra.iHoldUntil = aGame.iTime + 5.0f + aGame.iRandom.NextReal() * 8.0f;
                }
            target = BotGoalForSite(aGame, aExtra.iSite);
            if (aExtra.iTask == 1) target = BotHoldSpot(aGame, aPlayer, aExtra);
            break;
        case 1:
            if (aGame.iTime > aExtra.iHoldUntil) aExtra.iTask = 2;
            else if (!aArrived) target = BotHoldSpot(aGame, aPlayer, aExtra);
            else
                {
                aExtra.iPathLength = 0;
                aExtra.iRepathAt = aGame.iTime + 3.0f;
                aExtra.iHoldYaw = aPlayer.iYaw + 180.0f;
                return;
                }
            if (aExtra.iTask == 2) target = BotHuntTarget(aGame, aPlayer);
            break;
        case 2:
            target = BotHuntTarget(aGame, aPlayer);
            break;
        case 3:
        case 5:
            target = gNav->Find(aGame.iBombX, aGame.iBombY, aGame.iBombZ);
            break;
        case 4:
            if (aArrived)
                {
                aExtra.iHoldYaw = AngleNorm(Atan2Deg(aPlayer.iY - aGame.iBombY, aPlayer.iX - aGame.iBombX));
                aExtra.iPathLength = 0;
                aExtra.iRepathAt = aGame.iTime + 8.0f;
                return;
                }
            {
            const TReal32 angle = aGame.iRandom.NextReal() * 6.283f;
            const TReal32 distance = 150.0f + aGame.iRandom.NextReal() * 350.0f;
            target = gNav->Find(aGame.iBombX + FloatCos(angle) * distance,
                                aGame.iBombY + FloatSin(angle) * distance, aGame.iBombZ);
            }
            break;
        }
    if (target < 0 && gNav->iCount > 0) target = (TInt)(aGame.iRandom.Next() % (TUint32)gNav->iCount);
    if (!BotGo(aGame, aPlayer, aExtra, target))
        {
        aExtra.iPathLength = 0;
        aExtra.iRepathAt = aGame.iTime + 1.0f;
        }
    }

static void BotRadioReaction(TMatchState& aState, TPlayer& aBot, const TPlayer& aFrom,
                             TInt aCommand, TReal32 aTime)
    {
    if (aState.iMode == 2) return;
    if (aCommand == 4 || aCommand == 3 || aCommand == 8 || aCommand == 0
        || aCommand == 5 || aCommand == 14) return;
    TExtraPlayer& x = aState.iExtra[BoundedIndex(aBot.iId, KMaxPlayers)];
    const TReal32 dx = aFrom.iX - aBot.iX, dy = aFrom.iY - aBot.iY;
    const TBool near = dx * dx + dy * dy < 1600.0f * 1600.0f;
    TBool reply = EFalse;
    if (aCommand == 2 || aCommand == 9)
        {
        x.iHoldHereUntil = aTime + 25.0f;
        x.iLeader = -1;
        x.iPathLength = 0;
        reply = ETrue;
        }
    else if (aCommand == 6 || aCommand == 10 || aCommand == 1)
        {
        x.iLeader = -1;
        x.iHoldHereUntil = 0;
        x.iTask = aBot.iTeam == 1 ? 0 : 2;
        x.iRepathAt = 0;
        reply = ETrue;
        }
    else if (aCommand == 7)
        {
        x.iLeader = -1;
        x.iHoldHereUntil = 0;
        x.iTask = 2;
        x.iPathLength = 0;
        const TInt count = aBot.iTeam == 1 ? aState.iSpawnTCount : aState.iSpawnCTCount;
        if (count > 0)
            {
            const TReal32* spawn = aBot.iTeam == 1 ? aState.iSpawnCT : aState.iSpawnT;
            x.iGoal = gNav->Find(spawn[0], spawn[1], spawn[2] - 36.0f);
            }
        reply = ETrue;
        }
    else if (aCommand == 11)
        {
        const TInt roll = (aBot.iRandomSeed + (TInt)(aTime * 1000.0f) + aCommand * 37) & 255;
        AddRadioLater(aState, aBot.iId, 17, aTime + 0.6f + (roll & 255) / 102.0f);
        return;
        }
    else if (aCommand == 13 && aTime - x.iHeardAt > 3.0f)
        {
        TReal32 forward[3];
        ForwardVector(aFrom.iYaw, 0, forward);
        x.iHeardX = aFrom.iX + forward[0] * 600.0f;
        x.iHeardY = aFrom.iY + forward[1] * 600.0f;
        x.iHeardZ = aFrom.iZ;
        x.iHeardAt = aTime;
        }
    (void)near;
    if (reply && !aFrom.iBot && !Ex(aFrom).iRemote)
        {
        const TInt roll = (aBot.iRandomSeed + (TInt)(aTime * 730.0f)) & 255;
        if (roll < 115) AddRadioLater(aState, aBot.iId, roll < 90 ? 12 : 19, aTime + 0.5f + (roll & 63) / 64.0f);
        }
    }

static void LocalChat(CGame& aGame, const TDesC& aLine, TInt aColor)
    {
    for (TInt i = KMaxChatLines - 1; i > 0; i--)
        {
        TPtr line(aGame.iChatLines[i], 160);
        line.Copy(TPtrC(aGame.iChatLines[i - 1]));
        aGame.iChatColors[i] = aGame.iChatColors[i - 1];
        }
    CopyText(aGame.iChatLines[0], 160, aLine);
    aGame.iChatColors[0] = aColor;
    aGame.iChatHead = 0;
    }

static void BotifyPlayer(CGame& aGame, TPlayer& aPlayer)
    {
    TExtraPlayer& x = Ex(aPlayer);
    x.iRemote = EFalse;
    aPlayer.iNetId = 0;
    aPlayer.iBot = ETrue;
    aPlayer.iKills = 0;
    aPlayer.iDeaths = 0;
    x.iForward = 0;
    x.iStrafe = 0;
    x.iFire = EFalse;
    x.iFire2 = EFalse;
    x.iUse = EFalse;
    TPtr name(aPlayer.iName, sizeof(aPlayer.iName) / sizeof(TText));
    SetAscii(name, KBotNames[(TInt)(aGame.iRandom.Next() % (sizeof(KBotNames) / sizeof(KBotNames[0])))]);
    }

static void PeerJoin(CGame& aGame, TMatchState& aState, TInt aNetId, const TDesC& aName)
    {
    if (aState.iMpPhase != 2 || !gGame) return;
    TInt humans[3] = { 0, 0, 0 };
    for (TInt i = 0; i < aGame.iPlayerCount; i++)
        if (!aGame.iPlayers[i].iBot && aGame.iPlayers[i].iTeam >= 1 && aGame.iPlayers[i].iTeam <= 2)
            humans[aGame.iPlayers[i].iTeam]++;
    const TInt want = gState.iMode == 2 ? 0 : (humans[1] <= humans[2] ? 1 : 2);
    TInt pick = -1;
    for (TInt i = 0; i < aGame.iPlayerCount; i++)
        if (aGame.iPlayers[i].iBot && (want == 0 || aGame.iPlayers[i].iTeam == want)) { pick = i; break; }
    if (pick < 0)
        for (TInt i = 0; i < aGame.iPlayerCount; i++)
            if (aGame.iPlayers[i].iBot) { pick = i; break; }
    if (pick < 0) return;
    TPlayer& player = aGame.iPlayers[pick];
    TExtraPlayer& x = Ex(player);
    player.iBot = EFalse;
    player.iNetId = aNetId;
    x.iRemote = ETrue;
    x.iSpawnSeq++;
    player.iKills = 0;
    player.iDeaths = 0;
    x.iForward = 0;
    x.iStrafe = 0;
    x.iFire = EFalse;
    x.iFire2 = EFalse;
    x.iUse = EFalse;
    CopyText(player.iName, sizeof(player.iName) / sizeof(TText), aName);
    TBuf<128> line;
    line.Copy(aName);
    line.Append(_L(" зашёл в игру"));
    LocalChat(aGame, line, 0xFFE080);
    }

static void FlushPendingJoins(TMatchState& aState, CGame& aGame)
    {
    for (TInt i = 0; i < aState.iPendingJoinCount; i++)
        {
        TPtrC name(aState.iPendingJoinName[i]);
        PeerJoin(aGame, aState, aState.iPendingJoinId[i], name);
        }
    aState.iPendingJoinCount = 0;
    }

static void PeerLeft(CGame& aGame, TMatchState& aState, TInt aNetId)
    {
    TPlayer* player = PlayerByNet(aGame, aNetId);
    if (!player) return;
    TBuf<160> line;
    line.Copy(TPtrC(player->iName));
    line.Append(_L(" вышел из игры"));
    LocalChat(aGame, line, 0xFFE080);
    BotifyPlayer(aGame, *player);
    aState.iRosterAge = -1.0f;
    }

static void BecomeHost(CGame& aGame, TMatchState& aState)
    {
    aGame.iNetworkHost = ETrue;
    aState.iHost = 1;
    for (TInt i = 0; i < aGame.iPlayerCount; i++)
        {
        TPlayer& player = aGame.iPlayers[i];
        TExtraPlayer& x = Ex(player);
        if (&player == &aGame.iPlayers[aGame.iMyIndex]) continue;
        if (player.iNetId == 0)
            {
            player.iBot = ETrue;
            x.iRemote = EFalse;
            }
        else
            {
            player.iBot = EFalse;
            x.iRemote = ETrue;
            }
        }
    LocalChat(aGame, _L("Вы теперь ведущий игры"), 0xFFE080);
    aState.iRosterAge = -1.0f;
    }

static void NetworkFail(CGame& aGame, TMatchState& aState, const TDesC& aReason)
    {
    aState.iMpPhase = 3;
    aState.iJoinRequested = EFalse;
    aState.iConnectStarted = EFalse;
    if (aGame.iConnection) aGame.iConnection->Close();
    aGame.iAudio.StopAll();
    aGame.iState = EStateMenu;
    aGame.iMenu = EMenuSearch;
    aGame.iMenuSelection = 0;
    CopyText(aGame.iStatusLine, sizeof(aGame.iStatusLine) / sizeof(TText), aReason);
    }

static void ParseNetAddress(const TDesC& aServer, TBuf8<96>& aHost, TInt& aPort)
    {
    aPort = KNetPort;
    aHost.Zero();
    TPtrC text(aServer);
    TInt colon = -1;
    for (TInt i = 0; i < text.Length(); i++)
        if (text[i] == ':') { colon = i; break; }
    TPtrC host = colon >= 0 ? text.Left(colon) : text;
    for (TInt i = 0; i < host.Length() && aHost.Length() < aHost.MaxLength(); i++)
        {
        const TText c = host[i];
        aHost.Append(c < 128 ? (TUint8)c : (TUint8)'?');
        }
    if (colon < 0) return;
    TInt port = 0;
    TBool digits = EFalse;
    for (TInt i = colon + 1; i < text.Length(); i++)
        {
        if (text[i] < '0' || text[i] > '9') break;
        port = port * 10 + (text[i] - '0');
        digits = ETrue;
        }
    if (digits && port > 0 && port < 65536) aPort = port;
    }

static void HostSendRoster(CGame& aGame)
    {
    if (!aGame.iConnection || !aGame.iNetworkHost || gState.iMpPhase != 2) return;
    TNetBuffer& out = aGame.iNetOut;
    out.Reset();
    out.U8('I').U8(gState.iMode).U16(aGame.iRoundSeconds).U8(aGame.iMaxRounds).U8(aGame.iPlayerCount);
    for (TInt i = 0; i < aGame.iPlayerCount; i++)
        {
        TPlayer& player = aGame.iPlayers[i];
        const TInt netId = i == aGame.iMyIndex ? gState.iMyNetId : player.iNetId;
        out.U16(netId).U8(player.iTeam).U8(player.iBot ? 1 : 0).Str(TPtrC(player.iName));
        }
    aGame.iConnection->Send(EServerFromHost, out);
    }

static void HostSendSnapshot(CGame& aGame)
    {
    if (!aGame.iConnection || !aGame.iNetworkHost || gState.iMpPhase != 2) return;
    TNetBuffer& out = aGame.iNetOut;
    out.Reset();
    TReal32 timeLeft = aGame.iTime < aGame.iFreezeEnd
        ? aGame.iFreezeEnd - aGame.iTime
        : MaxF(0.0f, aGame.iFreezeEnd + gState.iRoundTime - aGame.iTime);
    TInt flags = aGame.iTime < aGame.iFreezeEnd ? 1 : 0;
    if (aGame.iRoundOver) flags |= 2;
    if (aGame.iBombPlanted) flags |= 4;
    if (aGame.iBombDropped) flags |= 8;
    if (aGame.iState == EStateEnd) flags |= 16;
    out.U8('P').U8(aGame.iRound).U8(aGame.iScoreT).U8(aGame.iScoreCT)
       .U16((TInt)(timeLeft * 10.0f)).U8(flags)
       .U8(ClampByte((TInt)(aGame.iBuyEnd - aGame.iTime)))
       .I16Float(aGame.iBombX).I16Float(aGame.iBombY).I16Float(aGame.iBombZ)
       .U8(ClampByte((TInt)MaxF(0.0f, aGame.iBombTime - aGame.iTime)));
    out.U8(aGame.iPlayerCount);
    for (TInt i = 0; i < aGame.iPlayerCount && i < KMaxPlayers; i++)
        {
        TPlayer& player = aGame.iPlayers[i];
        TExtraPlayer& x = Ex(player);
        TInt playerFlags = player.iAlive ? 1 : 0;
        if (player.iDucked) playerFlags |= 2;
        if (aGame.iTime - player.iLastShot < 0.12f) playerFlags |= 4;
        if (player.iPlanting) playerFlags |= 8;
        if (x.iDefusing) playerFlags |= 16;
        if (player.iHasC4) playerFlags |= 32;
        if (x.iZombie) playerFlags |= 64;
        if (x.iHelmet) playerFlags |= 128;
        out.U8(playerFlags).U8(player.iTeam)
           .I16Float(player.iX).I16Float(player.iY).I16Float(player.iZ * 4.0f)
           .U16((TInt)(player.iYaw * 65536.0f / 360.0f) & 0xFFFF).U8(ClampByte((TInt)player.iPitch))
           .U8(player.iWeapon).U16(MaxI(0, MinI(65535, (TInt)player.iHealth)))
           .U8(ClampByte((TInt)player.iArmor)).U16(MaxI(0, MinI(65535, player.iMoney)))
           .U8(player.iPrimary < 0 ? 255 : (player.iPrimary & 255))
           .U8(player.iSecondary < 0 ? 255 : (player.iSecondary & 255))
           .U8((x.iHe & 3) | ((x.iFlash & 3) << 2) | ((x.iSmoke & 3) << 4) | (x.iDefuser ? 64 : 0))
           .U8(player.iKills & 255).U8(player.iDeaths & 255).U8(x.iSpawnSeq & 255)
           .U8(ClampByte((TInt)((x.iFlashUntil - aGame.iTime) * 10.0f))).U8(x.iZType & 255);
        }
    aGame.iConnection->Send(EServerFromHost, out);
    }

static void ClientClearRound(CGame& aGame)
    {
    aGame.iRoundOver = EFalse;
    aGame.iRoundEndAt = -1;
    aGame.iBombExploded = EFalse;
    aGame.iBombSite = -1;
    aGame.iInfected = EFalse;
    aGame.iBuyOpen = EFalse;
    for (TInt i = 0; i < KMaxGrenades; i++) aGame.iGrenades[i].iActive = EFalse;
    for (TInt i = 0; i < KMaxSmokes; i++) gState.iSmokeOn[i] = EFalse;
    for (TInt i = 0; i < KMaxDrops; i++) aGame.iDrops[i].iActive = EFalse;
    CMap* map = aGame.iAssets.iMap;
    if (map && map->modelActive)
        for (TInt i = 0; i < map->numModels; i++) map->modelActive[i] = ETrue;
    }

static void ApplyRoster(CGame& aGame, TNetReader& aReader)
    {
    gState.iMode = aReader.U8();
    aGame.iMode = gState.iMode;
    aGame.iRoundSeconds = aReader.U16();
    aGame.iMaxRounds = aReader.U8();
    const TInt count = MinI(aReader.U8(), KMaxPlayers);
    aGame.iPlayerCount = count > 1 ? count : 2;
    for (TInt i = 0; i < aGame.iPlayerCount; i++)
        {
        TPlayer& player = aGame.iPlayers[i];
        TExtraPlayer& x = Ex(player);
        player.iId = i;
        player.iNetId = aReader.U16();
        player.iTeam = aReader.U8();
        player.iBot = aReader.U8() != 0;
        TPtr nameText(player.iName, sizeof(player.iName) / sizeof(TText));
        CopyAscii8(aReader.Str(), nameText);
        x.iRemote = EFalse;
        Mem::FillZ(x.iPath, sizeof(x.iPath));
        x.iPathLength = 0;
        x.iEnemy = -1;
        if (player.iNetId == gState.iMyNetId && gState.iMyNetId > 0)
            {
            aGame.iMyIndex = i;
            player.iBot = EFalse;
            }
        }
    gState.iRosterOk = ETrue;
    gState.iPendingRoster = EFalse;
    }

static void ApplyMeSnapshot(CGame& aGame, TPlayer& aPlayer, TInt aFlags,
                            TReal32 aX, TReal32 aY, TReal32 aZ, TReal32 aYaw,
                            TInt aWeapon, TInt aHealth, TInt aArmor, TInt aMoney,
                            TInt aPrimary, TInt aSecondary, TInt aGrenades,
                            TInt aSeq, TInt aFlash)
    {
    TExtraPlayer& x = Ex(aPlayer);
    const TBool alive = (aFlags & 1) != 0;
    const TBool respawn = alive && (!aPlayer.iAlive || (aSeq & 255) != (x.iSpawnSeq & 255));
    x.iSpawnSeq = aSeq;
    if (respawn)
        {
        aPlayer.iAlive = ETrue;
        aPlayer.iX = aX;
        aPlayer.iY = aY;
        aPlayer.iZ = aZ;
        aPlayer.iYaw = aYaw;
        aPlayer.iPitch = 0;
        aPlayer.iVelocityX = 0;
        aPlayer.iVelocityY = 0;
        aPlayer.iVelocityZ = 0;
        aGame.iSpectatorIndex = -1;
        }
    else if (aPlayer.iAlive && !alive)
        {
        aPlayer.iAlive = EFalse;
        aPlayer.iHealth = 0;
        aPlayer.iDeathTime = aGame.iTime;
        aPlayer.iAnimTime = 0;
        aGame.iSpectatorIndex = -1;
        aGame.iBuyOpen = EFalse;
        aPlayer.iZoom = 0;
        }
    if (!alive) return;
    if (aHealth < aPlayer.iHealth)
        {
        aGame.iHurtFlash = 0.25f;
        x.iPunchPitch -= 1.5f;
        }
    aPlayer.iHealth = aHealth;
    aPlayer.iArmor = aArmor;
    aPlayer.iMoney = aMoney;
    x.iDefuser = (aGrenades & 64) != 0;
    x.iHe = aGrenades & 3;
    x.iFlash = (aGrenades >> 2) & 3;
    x.iSmoke = (aGrenades >> 4) & 3;
    const TInt primary = aPrimary == 255 ? -1 : aPrimary;
    const TInt secondary = aSecondary == 255 ? -1 : aSecondary;
    if (primary != aPlayer.iPrimary)
        {
        aPlayer.iPrimary = primary;
        if (primary >= 0)
            {
            aPlayer.iClip[primary] = KWpClip[primary];
            aPlayer.iAmmo[primary] = KWpReserve[primary];
            }
        }
    if (secondary != aPlayer.iSecondary)
        {
        aPlayer.iSecondary = secondary;
        if (secondary >= 0)
            {
            aPlayer.iClip[secondary] = KWpClip[secondary];
            aPlayer.iAmmo[secondary] = KWpReserve[secondary];
            }
        }
    if (respawn || !HasWeapon(aPlayer, aPlayer.iWeapon))
        {
        TInt wanted = aWeapon;
        if (!HasWeapon(aPlayer, wanted)) wanted = aPlayer.iPrimary >= 0 ? aPlayer.iPrimary
            : (aPlayer.iSecondary >= 0 ? aPlayer.iSecondary : 0);
        DirectSwitch(aPlayer, wanted, aGame.iTime);
        }
    if (aFlash > 0)
        {
        const TReal32 until = aGame.iTime + aFlash / 10.0f;
        if (until > x.iFlashUntil)
            {
            x.iFlashStart = aGame.iTime;
            x.iFlashUntil = until;
            aPlayer.iFlashTime = aFlash / 10.0f;
            }
        }
    }

static void ApplySnapshot(CGame& aGame, TNetReader& aReader)
    {
    const TInt round = aReader.U8();
    aGame.iScoreT = aReader.U8();
    aGame.iScoreCT = aReader.U8();
    const TReal32 timeLeft = aReader.U16() / 10.0f;
    const TInt flags = aReader.U8();
    const TReal32 buyLeft = aReader.U8();
    const TReal32 bombX = aReader.Float16();
    const TReal32 bombY = aReader.Float16();
    const TReal32 bombZ = aReader.Float16();
    const TReal32 bombLeft = aReader.U8();
    if (round != gState.iLastRound)
        {
        gState.iLastRound = round;
        ClientClearRound(aGame);
        }
    aGame.iRound = round;
    if ((flags & 1) != 0) aGame.iFreezeEnd = aGame.iTime + timeLeft;
    else aGame.iFreezeEnd = aGame.iTime + timeLeft - gState.iRoundTime;
    aGame.iBuyEnd = aGame.iTime + buyLeft;
    const TBool wasOver = aGame.iRoundOver;
    aGame.iRoundOver = (flags & 2) != 0;
    if (aGame.iRoundOver && !wasOver) aGame.iRoundEndAt = aGame.iTime + 5.0f;
    aGame.iBombPlanted = (flags & 4) != 0;
    aGame.iBombDropped = (flags & 8) != 0;
    if (aGame.iBombPlanted || aGame.iBombDropped)
        {
        aGame.iBombX = bombX;
        aGame.iBombY = bombY;
        aGame.iBombZ = bombZ;
        aGame.iBombTime = aGame.iTime + bombLeft;
        }
    if ((flags & 16) != 0 && aGame.iState == EStatePlay)
        {
        aGame.iState = EStateEnd;
        aGame.iAudio.StopAll();
        }
    const TInt count = MinI(aReader.U8(), aGame.iPlayerCount);
    for (TInt i = 0; i < count; i++)
        {
        TPlayer& player = aGame.iPlayers[i];
        TExtraPlayer& x = Ex(player);
        const TInt playerFlags = aReader.U8();
        player.iTeam = aReader.U8();
        const TReal32 px = aReader.Float16();
        const TReal32 py = aReader.Float16();
        const TReal32 pz = aReader.Float16() * 0.25f;
        const TReal32 yaw = (TReal32)(aReader.U16() & 0xFFFF) * 360.0f / 65536.0f;
        const TInt pitch = (TInt)(TInt8)aReader.U8();
        const TInt weapon = aReader.U8();
        const TInt health = aReader.U16();
        const TInt armor = aReader.U8();
        const TInt money = aReader.U16();
        const TInt primary = aReader.U8();
        const TInt secondary = aReader.U8();
        const TInt grenades = aReader.U8();
        player.iKills = (TInt)(TInt8)aReader.U8();
        player.iDeaths = (TInt)(TInt8)aReader.U8();
        const TInt seq = aReader.U8();
        const TInt flash = aReader.U8();
        const TInt zType = aReader.U8();
        const TBool wasZombie = x.iZombie;
        player.iHasC4 = (playerFlags & 32) != 0;
        x.iZombie = (playerFlags & 64) != 0;
        player.iPlanting = (playerFlags & 8) != 0;
        x.iDefusing = (playerFlags & 16) != 0;
        x.iHelmet = (playerFlags & 128) != 0;
        x.iZType = zType < 3 ? zType : 0;
        if ((playerFlags & 4) != 0) player.iLastShot = aGame.iTime;
        if (i == aGame.iMyIndex)
            {
            ApplyMeSnapshot(aGame, player, playerFlags, px, py, pz, yaw, weapon,
                            health, armor, money, primary, secondary, grenades, seq, flash);
            continue;
            }
        player.iHealth = health;
        player.iMoney = money;
        player.iArmor = armor;
        player.iPrimary = primary == 255 ? -1 : primary;
        player.iSecondary = secondary == 255 ? -1 : secondary;
        if (weapon >= 0 && weapon < KWeaponCount) player.iWeapon = weapon;
        const TBool alive = (playerFlags & 1) != 0;
        if (player.iAlive && !alive)
            {
            player.iAlive = EFalse;
            player.iDeathTime = aGame.iTime;
            player.iAnimTime = 0;
            }
        const TBool snap = (!player.iAlive && alive) || seq != (x.iSpawnSeq & 255);
        player.iAlive = alive;
        x.iSpawnSeq = seq;
        player.iDucked = (playerFlags & 2) != 0;
        player.iPitch = pitch;
        gState.iTargetX[i] = px;
        gState.iTargetY[i] = py;
        gState.iTargetZ[i] = pz;
        gState.iTargetYaw[i] = yaw;
        if (snap || Dist2D(px, py, player.iX, player.iY) > 300.0f)
            {
            player.iX = px;
            player.iY = py;
            player.iZ = pz;
            player.iYaw = yaw;
            gState.iTargetX[i] = px;
            gState.iTargetY[i] = py;
            gState.iTargetZ[i] = pz;
            gState.iTargetYaw[i] = yaw;
            }
        if (x.iZombie && !wasZombie && &player == &aGame.iPlayers[aGame.iMyIndex])
            aGame.iBuyOpen = EFalse;
        }
    }

static void InterpolatePeers(CGame& aGame, TReal32 aDelta)
    {
    if (aDelta <= 0) return;
    const TReal32 k = MinF(1.0f, aDelta * 12.0f);
    for (TInt i = 0; i < aGame.iPlayerCount; i++)
        {
        TPlayer& player = aGame.iPlayers[i];
        if (i == aGame.iMyIndex) continue;
        if (!player.iAlive)
            {
            player.iAnimTime += aDelta;
            continue;
            }
        const TReal32 oldX = player.iX, oldY = player.iY;
        player.iX += (gState.iTargetX[i] - player.iX) * k;
        player.iY += (gState.iTargetY[i] - player.iY) * k;
        player.iZ += (gState.iTargetZ[i] - player.iZ) * k;
        player.iYaw += AngleNorm(gState.iTargetYaw[i] - player.iYaw) * k;
        player.iVelocityX = (player.iX - oldX) / aDelta;
        player.iVelocityY = (player.iY - oldY) / aDelta;
        const TReal32 speed = SpeedFactor(player);
        if (speed > 20.0f) player.iAnimTime += aDelta * speed / 220.0f;
        else player.iAnimTime = 0;
        player.iOnGround = ETrue;
        }
    }

static void ApplyVoicePart(CGame& aGame, TInt aPlayerId, TInt aSeq, TInt aPart,
                           TInt aParts, TInt aSamples, const TUint8* aData, TInt aBytes)
    {
    if (aPlayerId < 0 || aPlayerId >= KMaxPlayers || aPlayerId >= aGame.iPlayerCount) return;
    if (aPlayerId == aGame.iMyIndex || aParts < 1 || aParts > KVoiceParts
        || aPart < 0 || aPart >= aParts || aSamples < 1 || aSamples > KVoiceMaxSamples) return;
    TMatchState& state = gState;
    TVoiceSlot& slot = state.iVoiceSlots[aPlayerId];
    if (slot.iSeq != aSeq)
        {
        slot.iSeq = aSeq;
        slot.iCount = 0;
        for (TInt i = 0; i < KVoiceParts; i++)
            {
            slot.iParts[i].iActive = EFalse;
            slot.iParts[i].iSamples = 0;
            }
        }
    TVoicePart& part = slot.iParts[aPart];
    if (part.iActive) return;
    const TInt bytes = (aSamples + 1) / 2;
    if (bytes > aBytes || bytes > (TInt)sizeof(part.iData)) return;
    Mem::Copy(part.iData, aData, bytes);
    part.iActive = ETrue;
    part.iSeq = aSeq;
    part.iPart = aPart;
    part.iParts = aParts;
    part.iSamples = aSamples;
    slot.iCount++;
    if (slot.iCount < aParts) return;
    TInt total = 0;
    for (TInt i = 0; i < aParts; i++)
        {
        if (!slot.iParts[i].iActive) return;
        total += slot.iParts[i].iSamples;
        }
    if (total > KVoiceMaxSamples) total = KVoiceMaxSamples;
    TInt at = 0;
    for (TInt i = 0; i < aParts && at < total; i++)
        {
        const TInt count = MinI(slot.iParts[i].iSamples, total - at);
        VoiceDecode(slot.iParts[i].iData, count, state.iVoiceIn + at);
        at += count;
        }
    for (TInt i = 0; i < KVoiceParts; i++) slot.iParts[i].iActive = EFalse;
    slot.iCount = 0;
    if (!aGame.iSoundOn || !aGame.iVoiceOn) return;
    const TInt volume = Clamp(aGame.iVoiceVolume * aGame.iVolume / 100, 30, 100);
    aGame.iAudio.PlayPcm(reinterpret_cast<const TUint8*>(state.iVoiceIn), total * 2, KVoiceRate, volume);
    CopyText(state.iTalkName, sizeof(state.iTalkName) / sizeof(TText), TPtrC(aGame.iPlayers[aPlayerId].iName));
    state.iTalkUntil = aGame.iTime + (TReal32)total / KVoiceRate + 0.3f;
    }

static void SendVoiceClip(CGame& aGame, const TUint16* aSamples, TInt aCount)
    {
    if (!aGame.iConnection || gState.iMpPhase != 2) return;
    TInt count = MinI(aCount, KVoiceMaxSamples);
    if (count < KVoiceRate / 5) return;
    VoiceNormalize(const_cast<TUint16*>(aSamples), count);
    const TInt parts = (count + KVoicePartSamples - 1) / KVoicePartSamples;
    if (parts < 1 || parts > KVoiceParts) return;
    const TInt seq = ++gState.iVoiceSeq & 255;
    TNetBuffer& out = aGame.iNetOut;
    for (TInt part = 0; part < parts; part++)
        {
        const TInt offset = part * KVoicePartSamples;
        const TInt samples = MinI(KVoicePartSamples, count - offset);
        VoiceEncode(aSamples + offset, samples, gVoicePcm);
        out.Reset();
        out.U8('V');
        if (aGame.iNetworkHost) out.U8(aGame.iPlayers[aGame.iMyIndex].iId);
        out.U8(seq).U8(part).U8(parts).U16(samples)
           .Raw(gVoicePcm, (samples + 1) / 2);
        aGame.iConnection->Send(aGame.iNetworkHost ? EServerFromHost : EClientToHost, out);
        }
    }

void CGame::BotThink(TPlayer& aPlayer, TReal32 aDelta)
    {
    if (!aPlayer.iAlive || !aPlayer.iBot) return;
    TExtraPlayer& x = Ex(aPlayer);
    const TReal32 dt = Clamp(aDelta, 0.0f, 0.1f);
    // Terminator mode bot logic
    if (iMode == 3)
        {
        if (iTermCircleActive)
            {
            // move toward circle
            const TReal32 dx = iTermCircleX - aPlayer.iX;
            const TReal32 dy = iTermCircleY - aPlayer.iY;
            const TReal32 dist = FloatSqrt(dx*dx + dy*dy);
            if (dist > 30.0f)
                {
                aPlayer.iYaw = FloatAtan2(dy, dx) * 57.2958f;
                x.iForward = 1; x.iStrafe = 0; x.iJump = (dist > 200 && iRandom.NextReal() < 0.02f);
                x.iFire = EFalse; x.iFire2 = EFalse; return;
                }
            }
        const TInt termIdx = (iTerminatorIdx >= 0 && iTerminatorIdx < iPlayerCount) ? iTerminatorIdx : -1;
        const TBool isTerm = (termIdx >= 0 && &aPlayer == &iPlayers[termIdx]);
        if (isTerm && (iTerminatorClass == 2 || iTerminatorClass == 3) && iDisguiseCooldown <= 0.0f && !iIsDisguised)
            {
            iIsDisguised = ETrue;
            iDisguiseTimer = (iTerminatorClass == 3) ? 30.0f : 20.0f;
            iDisguiseCooldown = (iTerminatorClass == 3) ? 30.0f : 45.0f;
            }
        }
    const TBool zHuman = !x.iZombie;
    TReal32 react = iDifficulty == 0 ? 0.65f : (iDifficulty == 1 ? 0.38f : (iDifficulty == 2 ? 0.22f : 0.14f));
    react = zHuman ? react * 1.5f + 0.12f : react;
    TReal32 turnSpeed = iDifficulty == 0 ? 220.0f : (iDifficulty == 1 ? 340.0f : (iDifficulty == 2 ? 520.0f : 720.0f));
    turnSpeed = zHuman ? turnSpeed * 0.8f : turnSpeed;
    TReal32 aimError = iDifficulty == 0 ? 5.0f : (iDifficulty == 1 ? 3.0f : (iDifficulty == 2 ? 1.6f : 0.9f));
    aimError = zHuman ? aimError * 1.4f + 0.5f : aimError;
    if (iTime < iFreezeEnd)
        {
        if (!x.iBought && iTime > iRoundStart + 0.5f + (aPlayer.iId % 5) * 0.2f)
            BotBuy(*this, aPlayer);
        x.iForward = 0; x.iStrafe = 0; x.iFire = EFalse; x.iFire2 = EFalse; x.iUse = EFalse; x.iJump = EFalse;
        return;
        }
    if (!x.iBought) BotBuy(*this, aPlayer);
    x.iFire = EFalse; x.iFire2 = EFalse; x.iUse = EFalse; x.iJump = EFalse;
    const TBool blind = iTime < x.iFlashedUntil;
    if (iTime >= x.iNextPerceive)
        {
        x.iNextPerceive = iTime + 0.12f + iRandom.NextReal() * 0.08f;
        TPlayer* best = NULL;
        TReal32 bestDistance = 1e9f;
        ForwardVector(aPlayer.iYaw, 0.0f, gTempForward);
        for (TInt i = 0; i < iPlayerCount; i++)
            {
            TPlayer& other = iPlayers[i];
            if (!other.iAlive || other.iTeam == aPlayer.iTeam || &other == &aPlayer) continue;
            if (iMode == 3)
                {
                if (iTerminatorIdx < 0 || iTerminatorIdx >= iPlayerCount) continue;
                const TBool selfTerm = (&aPlayer == &iPlayers[iTerminatorIdx]);
                const TBool otherTerm = (&other == &iPlayers[iTerminatorIdx]);
                if (selfTerm == otherTerm) continue;   // 1 Terminator vs all humans
                }
            if (x.iIgnore == other.iId && iTime < x.iIgnoreUntil) continue;
            const TReal32 dx = other.iX - aPlayer.iX;
            const TReal32 dy = other.iY - aPlayer.iY;
            const TReal32 dz = other.iZ - aPlayer.iZ;
            const TReal32 squared = dx * dx + dy * dy + dz * dz;
            if (squared > 3500.0f * 3500.0f) continue;
            const TReal32 distance = FloatSqrt(squared);
            const TReal32 dot = (dx * gTempForward[0] + dy * gTempForward[1] + dz * gTempForward[2])
                / MaxF(1.0f, distance);
            const TBool recentlyHurt = other.iId == x.iLastAttacker && iTime - x.iLastHurt < 1.5f;
            if (dot < 0.25f && !recentlyHurt && distance > 250.0f) continue;
            if (blind) continue;
            if (!IsVisible(iAssets.iMap, gState, iTime, aPlayer, other)) continue;
            if (squared < bestDistance) { bestDistance = squared; best = &other; }
            }
        if (best)
            {
            if (x.iEnemy != best->iId)
                {
                x.iEngageStart = iTime;
                if (!x.iSpotted && iRandom.NextReal() < 0.35f)
                    {
                    x.iSpotted = ETrue;
                    Radio(aPlayer, 13);
                    }
                }
            if (x.iEnemy != best->iId || !x.iSeen)
                {
                TReal32 reaction = react;
                if (x.iEnemy == best->iId && iTime - x.iLastSeen < 1.0f) reaction *= 0.3f;
                x.iReactAt = iTime + reaction * (0.8f + iRandom.NextReal() * 0.5f);
                x.iErrYaw = RandomReal(iRandom, -1.0f, 1.0f) * aimError * 3.0f;
                x.iErrPitch = RandomReal(iRandom, -1.0f, 1.0f) * aimError * 2.0f;
                }
            x.iEnemy = best->iId;
            x.iSeen = ETrue;
            x.iLastSeen = iTime;
            x.iLastSeenX = best->iX;
            x.iLastSeenY = best->iY;
            x.iLastSeenZ = best->iZ;
            }
        else
            {
            x.iSeen = EFalse;
            if (x.iEnemy >= 0)
                {
                TPlayer* old = PlayerById(x.iEnemy);
                if (!old || !old->iAlive || iTime - x.iLastSeen > 3.0f) x.iEnemy = -1;
                }
            }
        }
    if (x.iEnemy >= 0)
        {
        TPlayer* enemy = PlayerById(x.iEnemy);
        if (!enemy || !enemy->iAlive) { x.iEnemy = -1; x.iSeen = EFalse; }
        }
    if (iBombPlanted && aPlayer.iTeam == 2) x.iTask = 3;
    if (iBombPlanted && aPlayer.iTeam == 1 && x.iTask != 4)
        { x.iTask = 4; x.iPathLength = 0; }
    if (iBombDropped && aPlayer.iTeam == 1 && !iBombPlanted && x.iTask != 5)
        { x.iTask = 5; x.iPathLength = 0; }
    if (!iBombDropped && x.iTask == 5)
        { x.iTask = 0; x.iPathLength = 0; }
    const TInt weapon = aPlayer.iWeapon;
    if (!x.iSeen && IsGun(weapon) && aPlayer.iClip[weapon] < KWpClip[weapon] * 0.4f
        && aPlayer.iAmmo[weapon] > 0 && !aPlayer.iReloading)
        StartReload(aPlayer);
    if (!aPlayer.iPlanting && aPlayer.iWeapon != 28)
        {
        if (aPlayer.iPrimary >= 0
            && (aPlayer.iClip[aPlayer.iPrimary] > 0 || aPlayer.iAmmo[aPlayer.iPrimary] > 0)
            && aPlayer.iWeapon != aPlayer.iPrimary && !aPlayer.iReloading)
            SwitchWeapon(aPlayer, aPlayer.iPrimary);
        else if ((aPlayer.iPrimary < 0 || (aPlayer.iClip[aPlayer.iPrimary] == 0
                    && aPlayer.iAmmo[aPlayer.iPrimary] == 0)) && aPlayer.iSecondary >= 0
                 && aPlayer.iWeapon != aPlayer.iSecondary && !aPlayer.iReloading
                 && (aPlayer.iClip[aPlayer.iSecondary] > 0 || aPlayer.iAmmo[aPlayer.iSecondary] > 0))
            SwitchWeapon(aPlayer, aPlayer.iSecondary);
        }
    TBool planting = EFalse;
    CMap* map = iAssets.iMap;
    if (aPlayer.iHasC4 && map && !iBombPlanted && !x.iSeen)
        planting = InAnySite(*map, gState, aPlayer.iX, aPlayer.iY, aPlayer.iZ, 0);
    TBool defusing = EFalse;
    if (x.iTask == 3 && iBombPlanted && !x.iSeen)
        {
        const TReal32 dx = iBombX - aPlayer.iX, dy = iBombY - aPlayer.iY;
        defusing = dx * dx + dy * dy < 48.0f * 48.0f;
        }
    if (x.iSeen && x.iEnemy >= 0 && iTime - x.iEngageStart > 7.0f && iTime - x.iLastHurt > 4.0f)
        {
        TPlayer* enemy = PlayerById(x.iEnemy);
        if (enemy && Ex(*enemy).iLastAttacker != aPlayer.iId)
            {
            x.iIgnore = x.iEnemy;
            x.iIgnoreUntil = iTime + 4.0f;
            x.iEnemy = -1;
            x.iSeen = EFalse;
            }
        }
    const TBool fighting = x.iEnemy >= 0 && (x.iSeen || iTime - x.iLastSeen < 1.2f);
    TReal32 moveYaw = aPlayer.iYaw;
    TBool moving = EFalse;
    if (!planting && !defusing)
        {
        TBool arrived = EFalse;
        if (x.iPathLength <= 0 || x.iPathIndex >= x.iPathLength)
            {
            arrived = x.iPathLength > 0 && x.iPathIndex >= x.iPathLength;
            x.iPathLength = 0;
            x.iPathIndex = 0;
            if (iTime > x.iRepathAt - 5.5f || arrived)
                BotChooseGoal(*this, aPlayer, x, arrived);
            }
        else if ((x.iLeader >= 0 && iTime < x.iFollowUntil && iTime > x.iRepathAt)
                 || (iMode != 0 && iTime > x.iRepathAt))
            BotChooseGoal(*this, aPlayer, x, EFalse);
        if (x.iPathLength > 0 && x.iPathIndex < x.iPathLength)
            {
            const TInt current = x.iPath[x.iPathIndex];
            TReal32 tx = aPlayer.iX;
            TReal32 ty = aPlayer.iY;
            if (current >= 0 && current < gNav->iCount)
                {
                tx = gNav->iCX[current];
                ty = gNav->iCY[current];
                if (x.iPathIndex + 1 < x.iPathLength)
                    {
                    const TInt next = x.iPath[x.iPathIndex + 1];
                    if (next >= 0 && next < gNav->iCount)
                        {
                        gNav->Portal(current, next, aPlayer.iX, aPlayer.iY);
                        tx = gNav->iPortalX;
                        ty = gNav->iPortalY;
                        const TReal32 dx = tx - aPlayer.iX, dy = ty - aPlayer.iY;
                        if (dx * dx + dy * dy < 22.0f * 22.0f || BotInArea(aPlayer, next))
                            {
                            x.iPathIndex++;
                            if (gNav->iCZ[next] - (aPlayer.iZ - (aPlayer.iDucked ? 18.0f : 36.0f)) > 22.0f
                                && aPlayer.iOnGround)
                                x.iJump = ETrue;
                            if ((gNav->iAttr[next] & 1) != 0) x.iDuckUntil = iTime + 1.2f;
                            if ((gNav->iAttr[next] & 2) != 0 && aPlayer.iOnGround) x.iJump = ETrue;
                            }
                        }
                    }
                else
                    {
                    const TReal32 dx = tx - aPlayer.iX, dy = ty - aPlayer.iY;
                    if (dx * dx + dy * dy < 24.0f * 24.0f) x.iPathIndex++;
                    }
                moveYaw = Atan2Deg(ty - aPlayer.iY, tx - aPlayer.iX);
                moving = ETrue;
                }
            }
        }
    TReal32 wantYaw = aPlayer.iYaw;
    TReal32 wantPitch = aPlayer.iPitch;
    TBool wantMove = moving;
    if (fighting)
        {
        TPlayer* enemy = PlayerById(x.iEnemy);
        if (!enemy) { wantYaw = aPlayer.iYaw; wantPitch = 0; }
        else
            {
            TReal32 tx = x.iSeen ? enemy->iX : x.iLastSeenX;
            TReal32 ty = x.iSeen ? enemy->iY : x.iLastSeenY;
            const TReal32 baseZ = x.iSeen ? enemy->iZ : x.iLastSeenZ;
            const TBool head = iDifficulty >= 2 || (iDifficulty == 1 && (aPlayer.iId & 1) == 0);
            const TReal32 targetZ = baseZ + (enemy->iDucked ? (head ? 9.0f : 2.0f) : (head ? 25.0f : 12.0f));
            if (x.iSeen)
                {
                tx += enemy->iVelocityX * 0.05f;
                ty += enemy->iVelocityY * 0.05f;
                }
            const TReal32 dx = tx - aPlayer.iX, dy = ty - aPlayer.iY;
            const TReal32 dz = targetZ - EyeZ(aPlayer);
            const TReal32 horizontal = FloatSqrt(dx * dx + dy * dy);
            wantYaw = Atan2Deg(dy, dx) + x.iErrYaw;
            wantPitch = -Atan2Deg(dz, horizontal) + x.iErrPitch;
            const TReal32 decay = MaxF(0.0f, 1.0f - dt * 2.2f);
            x.iErrYaw *= decay;
            x.iErrPitch *= decay;
            const TReal32 distance = FloatSqrt(dx * dx + dy * dy + dz * dz);
            const TReal32 range = WeaponRange(aPlayer.iWeapon);
            const TReal32 error = AbsF(AngleNorm(wantYaw - aPlayer.iYaw)) + AbsF(wantPitch - aPlayer.iPitch);
            const TReal32 tolerance = MaxF(1.2f, 40.0f / MaxF(50.0f, distance) * 57.3f * 0.35f);
            if (x.iSeen && iTime >= x.iReactAt && error < tolerance && !blind && distance < range * 1.3f)
                {
                if (IsSniper(aPlayer.iWeapon) && aPlayer.iZoom == 0 && distance > 300.0f)
                    AlternateFire(aPlayer);
                else if (KWpAuto[BoundedIndex(aPlayer.iWeapon, KWeaponCount)])
                    {
                    if (distance > 900.0f)
                        {
                        if (iTime > x.iPauseUntil)
                            {
                            x.iFire = ETrue;
                            if (x.iBurstEnd == 0) x.iBurstEnd = iTime + 0.25f;
                            if (iTime > x.iBurstEnd) { x.iPauseUntil = iTime + 0.35f; x.iBurstEnd = 0; }
                            }
                        }
                    else if (distance > 450.0f)
                        {
                        if (iTime > x.iPauseUntil)
                            {
                            x.iFire = ETrue;
                            if (x.iBurstEnd == 0) x.iBurstEnd = iTime + 0.5f;
                            if (iTime > x.iBurstEnd) { x.iPauseUntil = iTime + 0.25f; x.iBurstEnd = 0; }
                            }
                        }
                    else x.iFire = ETrue;
                    }
                else x.iFire = !x.iFirePrev;
                }
            if (aPlayer.iWeapon == 0 && distance < 70.0f) x.iFire = ETrue;
            if (x.iSeen)
                {
                const TBool stand = (IsSniper(aPlayer.iWeapon) || (distance > 600.0f && distance < range))
                    && aPlayer.iWeapon != 0;
                if (stand)
                    {
                    wantMove = EFalse;
                    x.iForward = 0;
                    x.iStrafe = 0;
                    }
                else if (distance < 600.0f || aPlayer.iWeapon == 0)
                    {
                    x.iStrafeTime -= dt;
                    if (x.iStrafeTime <= 0.0f)
                        {
                        x.iStrafeTime = 0.4f + iRandom.NextReal() * 0.7f;
                        x.iBotStrafe = iRandom.NextReal() < 0.5f ? -1.0f : 1.0f;
                        if (iRandom.NextReal() < 0.25f) x.iBotStrafe = 0;
                        }
                    x.iForward = distance < 150.0f ? -150.0f : 0.0f;
                    x.iStrafe = x.iBotStrafe * 250.0f;
                    if (aPlayer.iWeapon == 0) { x.iForward = 400.0f; x.iStrafe = 0; }
                    if (x.iFire && KWpAuto[BoundedIndex(aPlayer.iWeapon, KWeaponCount)] && distance > 300.0f)
                        x.iStrafe = 0;
                    wantMove = EFalse;
                    }
                if (distance > 900.0f && x.iFire && iDifficulty >= 1 && !IsSniper(aPlayer.iWeapon) && stand)
                    x.iDuck = ETrue;
                else if (iTime > x.iDuckUntil) x.iDuck = EFalse;
                }
            else
                {
                if (!moving || horizontal < 600.0f)
                    {
                    moveYaw = Atan2Deg(x.iLastSeenY - aPlayer.iY, x.iLastSeenX - aPlayer.iX);
                    wantMove = ETrue;
                    }
                x.iDuck = EFalse;
                }
            }
        }
    else
        {
        if (aPlayer.iZoom > 0) aPlayer.iZoom = 0;
        x.iDuck = iTime < x.iDuckUntil;
        if (iTime - x.iHeardAt < 2.5f)
            {
            wantYaw = Atan2Deg(x.iHeardY - aPlayer.iY, x.iHeardX - aPlayer.iX);
            wantPitch = 0;
            }
        else if (x.iHasLook && iTime < x.iNextLook)
            {
            wantYaw = x.iLookYaw;
            wantPitch = x.iLookPitch;
            }
        else
            {
            wantYaw = moving ? moveYaw : aPlayer.iYaw;
            wantPitch = 0;
            x.iHasLook = EFalse;
            }
        if (!moving && x.iTask == 1 && iTime > x.iNextLook)
            {
            x.iLookYaw = AngleNorm(x.iHoldYaw + RandomReal(iRandom, -50.0f, 50.0f));
            x.iLookPitch = RandomReal(iRandom, -6.0f, 6.0f);
            x.iNextLook = iTime + 1.5f + iRandom.NextReal() * 2.5f;
            x.iHasLook = ETrue;
            }
        }
    if (wantMove)
        {
        const TReal32 relative = AngleNorm(moveYaw - aPlayer.iYaw) * 3.14159265358979323846f / 180.0f;
        x.iForward = FloatCos(relative) * 400.0f;
        x.iStrafe = -FloatSin(relative) * 400.0f;
        const TReal32 mdx = aPlayer.iX - x.iLastX, mdy = aPlayer.iY - x.iLastY;
        if (mdx * mdx + mdy * mdy < 12.0f * dt * dt * 400.0f)
            {
            x.iStuckTime += dt;
            if (x.iStuckTime > 0.7f && aPlayer.iOnGround && iTime > x.iNextUnstuck)
                {
                x.iJump = ETrue;
                x.iNextUnstuck = iTime + 0.8f;
                x.iBotStrafe = -x.iBotStrafe;
                }
            if (x.iStuckTime > 1.0f) x.iStrafe += x.iBotStrafe * 300.0f;
            if (x.iStuckTime > 2.2f)
                {
                x.iPathLength = 0;
                x.iStuckTime = 0;
                x.iRepathAt = iTime;
                }
            }
        else x.iStuckTime = MaxF(0.0f, x.iStuckTime - dt);
        }
    else if (!fighting)
        {
        x.iForward = 0;
        x.iStrafe = 0;
        x.iStuckTime = 0;
        }
    x.iLastX = aPlayer.iX;
    x.iLastY = aPlayer.iY;
    if (planting)
        {
        if (aPlayer.iWeapon != 28) SwitchWeapon(aPlayer, 28);
        x.iFire = ETrue;
        x.iForward = 0;
        x.iStrafe = 0;
        x.iDuck = ETrue;
        x.iJump = EFalse;
        }
    if (defusing)
        {
        x.iUse = ETrue;
        x.iForward = 0;
        x.iStrafe = 0;
        x.iDuck = ETrue;
        x.iJump = EFalse;
        wantPitch = 50.0f;
        }
    const TReal32 maximumTurn = turnSpeed * dt;
    TReal32 deltaYaw = Clamp(AngleNorm(wantYaw - aPlayer.iYaw), -maximumTurn, maximumTurn);
    aPlayer.iYaw = AngleNorm(aPlayer.iYaw + deltaYaw);
    TReal32 deltaPitch = Clamp(wantPitch - aPlayer.iPitch, -maximumTurn, maximumTurn);
    aPlayer.iPitch = Clamp(aPlayer.iPitch + deltaPitch, -89.0f, 89.0f);
    if (blind)
        {
        x.iFire = x.iEnemy >= 0 && iRandom.NextReal() < 0.1f;
        aPlayer.iYaw = AngleNorm(aPlayer.iYaw + RandomReal(iRandom, -90.0f, 90.0f) * dt);
        }
    }

static TBool CanBuyHere(CGame& aGame, const TPlayer& aPlayer)
    {
    const TExtraPlayer& x = Ex(aPlayer);
    if (x.iZombie || !aPlayer.iAlive) return EFalse;
    if (aGame.iMode == 1) return ETrue;
    if (aGame.iTime > aGame.iBuyEnd) return EFalse;
    CMap* map = aGame.iAssets.iMap;
    if (map)
        {
        for (TInt i = 0; i < gState.iBuyCount; i++)
            {
            const TInt team = gState.iBuyTeam[i];
            if ((team == 0 || team == aPlayer.iTeam)
                && InBrushModel(*map, gState.iBuyModel[i], aPlayer.iX, aPlayer.iY, aPlayer.iZ, 8.0f))
                return ETrue;
            }
        }
    if (gState.iBuyCount == 0)
        {
        const TReal32* spawn = aPlayer.iTeam == 1 ? gState.iSpawnT : gState.iSpawnCT;
        const TInt count = aPlayer.iTeam == 1 ? gState.iSpawnTCount : gState.iSpawnCTCount;
        for (TInt i = 0; i < count; i++)
            {
            const TReal32 dx = spawn[i * 4] - aPlayer.iX;
            const TReal32 dy = spawn[i * 4 + 1] - aPlayer.iY;
            if (dx * dx + dy * dy < 400.0f * 400.0f) return ETrue;
            }
        }
    return EFalse;
    }

static void BuyAmmoFor(TPlayer& aPlayer, TInt aSlot)
    {
    const TInt weapon = aSlot == 0 ? aPlayer.iPrimary : aPlayer.iSecondary;
    if (weapon < 0 || weapon >= KWeaponCount) return;
    while (aPlayer.iAmmo[weapon] < KWpReserve[weapon]
           && aPlayer.iMoney >= KWpAmmoPrice[weapon])
        {
        aPlayer.iMoney -= KWpAmmoPrice[weapon];
        aPlayer.iAmmo[weapon] = MinI(KWpReserve[weapon], aPlayer.iAmmo[weapon] + KWpClip[weapon]);
        }
    }

static void AddKillFeedEvent(CGame& aGame, TInt aAttacker, TInt aVictim, TInt aWeapon)
    {
    if (aVictim < 0 || aVictim >= aGame.iPlayerCount) return;
    for (TInt i = KMaxChatLines - 1; i > 0; i--)
        {
        aGame.iKillFeed[i][0] = aGame.iKillFeed[i - 1][0];
        aGame.iKillFeed[i][1] = aGame.iKillFeed[i - 1][1];
        aGame.iKillFeed[i][2] = aGame.iKillFeed[i - 1][2];
        aGame.iKillFeedTime[i] = aGame.iKillFeedTime[i - 1];
        }
    aGame.iKillFeed[0][0] = aAttacker >= 0 && aAttacker < aGame.iPlayerCount ? aAttacker : -1;
    aGame.iKillFeed[0][1] = aVictim;
    aGame.iKillFeed[0][2] = aWeapon;
    aGame.iKillFeedTime[0] = aGame.iTime;
    }

static void HostSendRosterTo(CGame& aGame, TInt aNetId)
    {
    if (!aGame.iConnection || !aGame.iNetworkHost || gState.iMpPhase != 2) return;
    TNetBuffer& out = aGame.iNetOut;
    out.Reset();
    out.U16(aNetId).U8('I').U8(gState.iMode).U16(aGame.iRoundSeconds)
       .U8(aGame.iMaxRounds).U8(aGame.iPlayerCount);
    for (TInt i = 0; i < aGame.iPlayerCount; i++)
        {
        TPlayer& player = aGame.iPlayers[i];
        const TInt netId = i == aGame.iMyIndex ? gState.iMyNetId : player.iNetId;
        out.U16(netId).U8(player.iTeam).U8(player.iBot ? 1 : 0).Str(TPtrC(player.iName));
        }
    aGame.iConnection->Send(EClientToOne, out);
    }

static void SendClientAction(CGame& aGame, TInt aOp, TInt aFirst, TInt aSecond)
    {
    if (!aGame.iConnection || aGame.iNetworkHost || gState.iMpPhase != 2 || gState.iApplying) return;
    TNetBuffer& out = aGame.iNetOut;
    out.Reset();
    out.U8(aOp);
    if (aFirst >= 0) out.U8(aFirst);
    if (aSecond >= 0) out.U8(aSecond);
    aGame.iConnection->Send(EClientToHost, out);
    }

static void HandleClientPacket(CGame& aGame, TInt aSenderId, TNetReader& aReader,
                               const TUint8* aFrame, TInt aFrameLength)
    {
    if (!aGame.iNetworkHost || gState.iMpPhase != 2) return;
    if (!aReader.More()) return;
    const TInt op = aReader.U8();
    if (op == 'Q')
        {
        FlushPendingJoins(gState, aGame);
        HostSendRosterTo(aGame, aSenderId);
        return;
        }
    TPlayer* player = PlayerByNet(aGame, aSenderId);
    if (!player) return;
    TExtraPlayer& x = Ex(*player);
    switch (op)
        {
        case 'S':
            {
            if (!player->iAlive) return;
            player->iX = aReader.Float16();
            player->iY = aReader.Float16();
            player->iZ = aReader.Float16() * 0.25f;
            player->iVelocityX = aReader.Float16();
            player->iVelocityY = aReader.Float16();
            player->iVelocityZ = aReader.Float16();
            player->iYaw = (TReal32)(aReader.U16() & 0xFFFF) * 360.0f / 65536.0f;
            player->iPitch = (TInt)(TInt8)aReader.U8();
            const TInt flags = aReader.U8();
            x.iFire = (flags & 1) != 0;
            x.iFire2 = (flags & 2) != 0;
            x.iUse = (flags & 4) != 0;
            x.iDuck = (flags & 8) != 0;
            player->iDucked = (flags & 16) != 0;
            player->iOnGround = (flags & 32) != 0;
            x.iSilM4 = (flags & 64) != 0;
            x.iSilUSP = (flags & 128) != 0;
            if (!aReader.More()) return;
            const TInt weapon = aReader.U8();
            player->iZoom = aReader.More() ? aReader.U8() : 0;
            if (weapon >= 0 && weapon < KWeaponCount && weapon != player->iWeapon
                && HasWeapon(*player, weapon))
                aGame.SwitchWeapon(*player, weapon);
            break;
            }
        case 'B':
            {
            if (!CanBuyHere(aGame, *player) || !aReader.More()) return;
            const TInt kind = aReader.U8();
            const TInt value = aReader.More() ? aReader.U8() : -1;
            if (value < 0) return;
            if (kind == 0) BuyWeaponFor(aGame, *player, value);
            else if (kind == 1) BuyEquipFor(*player, value);
            else if (kind == 2) BuyAmmoFor(*player, value);
            gState.iRosterAge = -1.0f;
            break;
            }
        case 'L':
            aGame.StartReload(*player);
            break;
        case 'D':
            aGame.DropCurrent(*player);
            break;
        case 'r':
            if (aReader.More()) aGame.Radio(*player, aReader.U8());
            break;
        case 'V':
            {
            const TInt seq = aReader.U8();
            const TInt part = aReader.U8();
            const TInt parts = aReader.U8();
            const TInt samples = aReader.U16();
            const TInt position = aReader.Position();
            const TInt length = MaxI(0, aFrameLength - position);
            ApplyVoicePart(aGame, player->iId, seq, part, parts, samples,
                           aFrame + position, length);
            if (gState.iRoomPlayers > 1)
                {
                TNetBuffer& out = aGame.iNetOut;
                out.Reset();
                out.U8('V').U8(player->iId).U8(seq).U8(part).U8(parts).U16(samples)
                   .Raw(aFrame + position, length);
                aGame.iConnection->Send(EServerFromHost, out);
                }
            break;
            }
        default:
            break;
        }
    }

static void HandleHostPacket(CGame& aGame, TNetReader& aReader,
                             const TUint8* aFrame, TInt aFrameLength)
    {
    const TInt op = aReader.U8();
    if (op == 'I')
        {
        if (aGame.iState != EStatePlay || gState.iMpPhase != 2)
            {
            const TInt length = MinI(aFrameLength, (TInt)sizeof(gState.iPendingData));
            Mem::Copy(gState.iPendingData, aFrame, length);
            gState.iPendingLength = length;
            gState.iPendingRoster = ETrue;
            return;
            }
        ApplyRoster(aGame, aReader);
        gState.iRosterRequested = EFalse;
        return;
        }
    if (gState.iMpPhase != 2 || aGame.iState != EStatePlay || !gState.iRosterOk) return;
    gState.iApplying = ETrue;
    switch (op)
        {
        case 'P':
            ApplySnapshot(aGame, aReader);
            break;
        case 'K':
            {
            const TInt attacker = aReader.U8();
            const TInt victim = aReader.U8();
            const TInt weapon = aReader.U8();
            const TInt headshot = aReader.U8();
            AddKillFeedEvent(aGame, attacker == 255 ? -1 : attacker, victim,
                             weapon | (headshot ? 256 : 0));
            break;
            }
        case 'C':
            {
            TBuf<192> text;
            CopyAscii8(aReader.Str(), text);
            const TReal32 duration = aReader.U8() / 10.0f;
            const TInt color = (TInt)aReader.I32();
            aGame.Center(text, duration, color);
            break;
            }
        case 'N':
            {
            const TInt source = aReader.U8();
            const TReal32 sx = aReader.Float16();
            const TReal32 sy = aReader.Float16();
            const TReal32 sz = aReader.Float16();
            const TInt volume = aReader.U8();
            const TReal32 range = (TReal32)aReader.U16();
            const TPtrC8 name = aReader.Str();
            if (source >= 0 && source < aGame.iPlayerCount && source == aGame.iMyIndex)
                {
                TPtrC8 ascii(name);
                const TBool weapons = ascii.Length() >= 8
                    && ascii.Left(8) == TPtrC8(reinterpret_cast<const TUint8*>("weapons_"), 8);
                const TBool step = ascii.Length() >= 12
                    && ascii.Left(12) == TPtrC8(reinterpret_cast<const TUint8*>("player_pl_st"), 12);
                const TBool items = ascii.Length() >= 6
                    && ascii.Left(6) == TPtrC8(reinterpret_cast<const TUint8*>("items_"), 6);
                if (weapons || step || items) break;
                }
            aGame.PlaySoundAt(sx, sy, sz, name, volume, range);
            break;
            }
        case 'R':
            {
            const TInt player = aReader.U8();
            const TInt command = aReader.U8();
            if (player >= 0 && player < aGame.iPlayerCount)
                {
                aGame.iPlayers[player].iLastRadio = -10.0f;
                aGame.Radio(aGame.iPlayers[player], command);
                }
            break;
            }
        case 'T':
            {
            TBuf<192> text;
            CopyAscii8(aReader.Str(), text);
            aGame.Chat(text, (TInt)aReader.I32());
            break;
            }
        case 'V':
            {
            const TInt player = aReader.U8();
            const TInt seq = aReader.U8();
            const TInt part = aReader.U8();
            const TInt parts = aReader.U8();
            const TInt samples = aReader.U16();
            const TInt position = aReader.Position();
            ApplyVoicePart(aGame, player, seq, part, parts, samples,
                           aFrame + position, MaxI(0, aFrameLength - position));
            break;
            }
        default:
            break;
        }
    gState.iApplying = EFalse;
    }

void CGame::NetworkTick(TReal32 aDelta)
    {
    if (gState.iMpPhase == 3 || !iConnection) return;
    if (iConnection->Error() != KErrNone)
        {
        NetworkFail(*this, gState, iConnection->ErrorText());
        return;
        }
    if (!gState.iJoinSent && gState.iJoinRequested)
        {
        if (!gState.iConnectStarted)
            {
            gState.iConnectStarted = ETrue;
            gState.iJoinTick = User::NTickCount();
            }
        if (iConnection->Connected())
            {
            gState.iJoinSent = ETrue;
            iNetOut.Reset();
            iNetOut.U8(KNetProtocol).U8(gState.iMode).U8(gState.iMapIndex)
                   .Str(gState.iPlayerName[0] ? TPtrC(gState.iPlayerName) : TPtrC(_L("Игрок")))
                   .U8(iNetworkHost || gState.iCreateRoom ? 1 : 0);
            iConnection->Send(EClientJoin, iNetOut);
            }
        else if (User::NTickCount() - gState.iJoinTick > 20000u)
            {
            NetworkFail(*this, gState, _L("Сервер не отвечает"));
            return;
            }
        }
    TInt length = 0;
    TInt guard = 0;
    while (iConnection->Poll(gNetFrame, length) && guard++ < 200)
        {
        if (length < 2) continue;
        TNetReader reader(gNetFrame + 1, length - 1);
        switch (gNetFrame[0])
            {
            case EServerWelcome:
                {
                gState.iMyNetId = reader.U16();
                const TInt host = reader.U8();
                reader.U16();
                gState.iRoomPlayers = reader.U8();
                gState.iOnline = gState.iRoomPlayers;
                gState.iHost = host;
                iNetworkHost = host != 0;
                gState.iMpPhase = 1;
                if (iState != EStateLoad && iState != EStatePlay)
                    {
                    iPendingMap = Clamp(gState.iMapIndex, 0, 2);
                    iPendingTeam = Clamp(gState.iPendingTeam > 0 ? gState.iPendingTeam
                        : (iTeamChoice > 0 ? iTeamChoice : 1), 1, 3);
                    gState.iPendingTeam = iPendingTeam;
                    StartMatch(iPendingMap, iPendingTeam);
                    }
                break;
                }
            case EServerRoom:
                gState.iRoomPlayers = reader.U8();
                gState.iOnline = reader.U16();
                break;
            case EServerError:
                {
                TBuf<192> reason;
                CopyAscii8(reader.Str(), reason);
                NetworkFail(*this, gState, reason);
                return;
                }
            case EServerHost:
                BecomeHost(*this, gState);
                break;
            case EServerPeerJoin:
                {
                const TInt id = reader.U16();
                TBuf<64> name;
                CopyAscii8(reader.Str(), name);
                if (gState.iMpPhase == 2) PeerJoin(*this, gState, id, name);
                else if (gState.iPendingJoinCount < KMaxPlayers)
                    {
                    gState.iPendingJoinId[gState.iPendingJoinCount] = id;
                    CopyText(gState.iPendingJoinName[gState.iPendingJoinCount], 24, name);
                    gState.iPendingJoinCount++;
                    }
                break;
                }
            case EServerPeerLeft:
                PeerLeft(*this, gState, reader.U16());
                break;
            case EServerPong:
                // KeepAlive() уже обновляет время последнего ответа; полезная нагрузка не нужна.
                break;
            case EServerFromClient:
                {
                const TInt sender = reader.U16();
                if (iNetworkHost && gState.iMpPhase == 2)
                    HandleClientPacket(*this, sender, reader, gNetFrame + 1, length - 1);
                break;
                }
            case EServerFromHost:
                if (!iNetworkHost) HandleHostPacket(*this, reader, gNetFrame + 1, length - 1);
                break;
            default:
                break;
            }
        }
    if (gState.iJoinSent) iConnection->KeepAlive();
    if (gState.iMpPhase != 2 || iState != EStatePlay) return;
    if (gState.iPendingRoster && gState.iPendingLength >= 3)
        {
        TNetReader pending(gState.iPendingData + 2, gState.iPendingLength - 2);
        ApplyRoster(*this, pending);
        gState.iPendingRoster = EFalse;
        gState.iPendingLength = 0;
        gState.iRosterRequested = EFalse;
        }
    if (!iNetworkHost && !gState.iRosterOk && !gState.iRosterRequested)
        {
        iNetOut.Reset();
        iNetOut.U8('Q');
        iConnection->Send(EClientToHost, iNetOut);
        gState.iRosterRequested = ETrue;
        }
    if (iNetworkHost)
        {
        if (gState.iRosterAge < 0)
            {
            HostSendRoster(*this);
            gState.iRosterAge = 0;
            }
        gState.iSnapAccum += aDelta;
        if (gState.iSnapAccum >= 0.1f && gState.iRoomPlayers > 1)
            {
            gState.iSnapAccum = 0;
            HostSendSnapshot(*this);
            }
        }
    else
        {
        gState.iSendAccum += aDelta;
        if (gState.iSendAccum >= 1.0f / 15.0f)
            {
            gState.iSendAccum = 0;
            TPlayer& me = iPlayers[iMyIndex];
            if (me.iAlive)
                {
                const TExtraPlayer& x = Ex(me);
                TInt flags = x.iFire ? 1 : 0;
                if (x.iFire2) flags |= 2;
                if (x.iUse) flags |= 4;
                if (x.iDuck) flags |= 8;
                if (me.iDucked) flags |= 16;
                if (me.iOnGround) flags |= 32;
                if (x.iSilM4) flags |= 64;
                if (x.iSilUSP) flags |= 128;
                iNetOut.Reset();
                iNetOut.U8('S').I16Float(me.iX).I16Float(me.iY).I16Float(me.iZ * 4.0f)
                       .I16Float(me.iVelocityX).I16Float(me.iVelocityY).I16Float(me.iVelocityZ)
                       .U16((TInt)(me.iYaw * 65536.0f / 360.0f) & 0xFFFF)
                       .U8((TInt)me.iPitch & 255).U8(flags).U8(me.iWeapon).U8(me.iZoom);
                iConnection->Send(EClientToHost, iNetOut);
                }
            }
        }
    }

// ------------------------------------------------------------------ режимы и возрождение

static void PlaceAtSpawn(CGame& aGame, TPlayer& aPlayer)
    {
    const TInt team = aPlayer.iTeam == 1 ? 0 : 1;
    const TReal32* spawn = team == 0 ? gState.iSpawnT : gState.iSpawnCT;
    const TInt count = team == 0 ? gState.iSpawnTCount : gState.iSpawnCTCount;
    if (count > 0)
        {
        const TInt k = (TInt)((aGame.iRandom.Next() & 0xFFFF) % (TUint32)count);
        aPlayer.iX = spawn[k * 4];
        aPlayer.iY = spawn[k * 4 + 1];
        aPlayer.iZ = spawn[k * 4 + 2] + 1.0f;
        aPlayer.iYaw = spawn[k * 4 + 3];
        }
    CMap* map = aGame.iAssets.iMap;
    if (map)
        {
        map->Trace(1, aPlayer.iX, aPlayer.iY, aPlayer.iZ, aPlayer.iX, aPlayer.iY, aPlayer.iZ - 64.0f);
        if (!map->rStartSolid) aPlayer.iZ = map->rEz;
        }
    aPlayer.iOnGround = ETrue;
    }

static void RespawnForMode(CGame& aGame, TPlayer& aPlayer)
    {
    TExtraPlayer& x = Ex(aPlayer);
    const TBool zombie = x.iZombie != EFalse && aGame.iMode == 2;
    CMap* map = aGame.iAssets.iMap;
    ResetPlayerForRound(aPlayer, x, map, aGame.iTime, EFalse, aGame.iMode);
    PlaceAtSpawn(aGame, aPlayer);
    aPlayer.iPitch = 0;
    aPlayer.iSecondary = aPlayer.iTeam == 1 ? 1 : 2;
    aPlayer.iClip[aPlayer.iSecondary] = KWpClip[aPlayer.iSecondary];
    aPlayer.iAmmo[aPlayer.iSecondary] = aPlayer.iTeam == 1 ? 40 : 24;
    aPlayer.iPrimary = -1;
    aPlayer.iMoney = zombie ? 0 : (aGame.iMode == 1 ? 16000 : gState.iStartMoney);
    aPlayer.iArmor = zombie ? 0 : (aGame.iMode == 1 ? 100 : aPlayer.iArmor);
    x.iHelmet = aGame.iMode == 1 && !zombie;
    x.iSpawnSeq++;
    x.iPathLength = 0;
    x.iPathIndex = 0;
    x.iGoal = -1;
    x.iRepathAt = 0;
    x.iEnemy = -1;
    x.iSeen = EFalse;
    x.iSpotted = EFalse;
    x.iHasLook = EFalse;
    x.iNextPerceive = aGame.iTime;
    DirectSwitch(aPlayer, aPlayer.iSecondary, aGame.iTime);
    if (zombie)
        {
        const TInt type = BoundedIndex(x.iZType, 3);
        aPlayer.iTeam = 1;
        aPlayer.iHealth = (TReal32)KZombieHealth[type];
        aPlayer.iArmor = 0;
        aPlayer.iPrimary = -1;
        aPlayer.iSecondary = -1;
        x.iHe = 0;
        x.iFlash = 0;
        x.iSmoke = 0;
        aPlayer.iWeapon = 0;
        if (&aPlayer == &aGame.iPlayers[aGame.iMyIndex])
            {
            TBuf<96> message;
            message.Copy(_L("Вы снова в игре: "));
            message.Append(TPtrC(KZombieNames[type]));
            aGame.Center(message, 2.0f, 0x60FF60);
            }
        }
    }

static void InfectForMode(CGame& aGame, TPlayer& aPlayer, TPlayer* aBy)
    {
    TExtraPlayer& x = Ex(aPlayer);
    x.iZombie = ETrue;
    x.iZType = (TInt)((aGame.iRandom.Next() & 0xFFFF) % 3u);
    aPlayer.iTeam = 1;
    aPlayer.iHealth = (TReal32)KZombieHealth[x.iZType] * (aBy ? 1.0f : 1.5f);
    aPlayer.iArmor = 0;
    x.iHelmet = EFalse;
    aPlayer.iPrimary = -1;
    aPlayer.iSecondary = -1;
    x.iHe = 0;
    x.iFlash = 0;
    x.iSmoke = 0;
    x.iDefuser = EFalse;
    aPlayer.iHasC4 = EFalse;
    aPlayer.iZoom = 0;
    aPlayer.iScope = EFalse;
    aPlayer.iReloading = EFalse;
    aPlayer.iPlanting = EFalse;
    x.iDefusing = EFalse;
    aPlayer.iWeapon = 0;
    x.iSpawnSeq++;
    x.iPathLength = 0;
    x.iPathIndex = 0;
    x.iGoal = -1;
    x.iEnemy = -1;
    x.iSeen = EFalse;
    x.iNextPerceive = aGame.iTime;
    aGame.PlaySoundAt(aPlayer.iX, aPlayer.iY, aPlayer.iZ, Ascii("player_die3"), 80, 2200.0f);
    if (&aPlayer == &aGame.iPlayers[aGame.iMyIndex])
        {
        TBuf<128> message;
        message.Copy(_L("Вы заражены! Вы — "));
        message.Append(TPtrC(KZombieNames[x.iZType]));
        aGame.Center(message, 4.0f, 0x60FF60);
        aGame.iBuyOpen = EFalse;
        }
    TBuf<128> chat;
    chat.Copy(TPtrC(aPlayer.iName));
    chat.Append(_L(" стал зомби: "));
    chat.Append(TPtrC(KZombieNames[x.iZType]));
    aGame.Chat(chat, 0x80FF80);
    if (aBy)
        {
        aBy->iKills++;
        AddKillFeedEvent(aGame, aBy->iId, aPlayer.iId, 0);
        if (aGame.iNetworkHost && !gState.iApplying && gState.iMpPhase == 2 && aGame.iConnection
            && gState.iRoomPlayers > 1)
            {
            aGame.iNetOut.Reset();
            aGame.iNetOut.U8('K').U8(aBy->iId).U8(aPlayer.iId).U8(0).U8(0);
            aGame.iConnection->Send(EServerFromHost, aGame.iNetOut);
            }
        }
    }

static void ModeTick(CGame& aGame)
    {
    if (aGame.iMode == 1)
        {
        if (aGame.iRoundOver) return;
        for (TInt i = 0; i < aGame.iPlayerCount; i++)
            {
            TPlayer& player = aGame.iPlayers[i];
            if (!player.iAlive && aGame.iTime - player.iDeathTime > 3.0f)
                RespawnForMode(aGame, player);
            }
        return;
        }
    if (aGame.iMode != 2) return;
    if (aGame.iInfected && !aGame.iRoundOver)
        {
        for (TInt i = 0; i < aGame.iPlayerCount; i++)
            {
            TPlayer& player = aGame.iPlayers[i];
            if (!player.iAlive && Ex(player).iZombie && aGame.iTime - player.iDeathTime > 5.0f)
                RespawnForMode(aGame, player);
            }
        return;
        }
    if (aGame.iInfected || aGame.iRoundOver || aGame.iTime < aGame.iInfectAt) return;
    aGame.iInfected = ETrue;
    gState.iInfected = ETrue;
    TInt candidates = 0;
    for (TInt i = 0; i < aGame.iPlayerCount; i++)
        if (aGame.iPlayers[i].iAlive && !Ex(aGame.iPlayers[i]).iZombie) candidates++;
    if (candidates > 0)
        {
        const TInt pick = (TInt)((aGame.iRandom.Next() & 0xFFFF) % (TUint32)candidates);
        TInt seen = 0;
        for (TInt i = 0; i < aGame.iPlayerCount; i++)
            {
            TPlayer& player = aGame.iPlayers[i];
            if (!player.iAlive || Ex(player).iZombie) continue;
            if (seen++ == pick) { InfectForMode(aGame, player, NULL); break; }
            }
        }
    aGame.Center(_L("Заражение началось! Бегите от зомби"), 3.0f, 0x60FF60);
    aGame.PlaySound(Ascii("radio_takepoint"), 80);
    }

static void UpdateBomb(CGame& aGame)
    {
    if (!aGame.iBombPlanted || aGame.iBombExploded) return;
    const TReal32 left = aGame.iBombTime - aGame.iTime;
    TReal32 interval = 0.2f;
    if (left > 20.0f) interval = 1.4f;
    else if (left > 10.0f) interval = 0.9f;
    else if (left > 5.0f) interval = 0.45f;
    if (aGame.iTime >= aGame.iBombBeep)
        {
        aGame.iBombBeep = aGame.iTime + interval;
        aGame.PlaySoundAt(aGame.iBombX, aGame.iBombY, aGame.iBombZ,
                          Ascii("weapons_c4_beep1"), 70, 2500.0f);
        }
    if (aGame.iTime < aGame.iBombTime) return;
    aGame.iBombExploded = ETrue;
    aGame.iBombPlanted = EFalse;
    aGame.PlaySoundAt(aGame.iBombX, aGame.iBombY, aGame.iBombZ,
                      Ascii("weapons_c4_explode1"), 100, 6000.0f);
    for (TInt i = 0; i < 10; i++)
        aGame.AddEffect(aGame.iBombX + RandomReal(aGame.iRandom, -150.0f, 150.0f),
                        aGame.iBombY + RandomReal(aGame.iRandom, -150.0f, 150.0f),
                        aGame.iBombZ + RandomReal(aGame.iRandom, 0.0f, 200.0f),
                        0xFFA030, 1.2f, 90, 0);
    for (TInt i = 0; i < aGame.iPlayerCount; i++)
        {
        TPlayer& player = aGame.iPlayers[i];
        if (!player.iAlive) continue;
        const TReal32 dx = player.iX - aGame.iBombX;
        const TReal32 dy = player.iY - aGame.iBombY;
        const TReal32 dz = player.iZ - aGame.iBombZ;
        const TReal32 distance = FloatSqrt(dx * dx + dy * dy + dz * dz);
        if (distance >= 1000.0f) continue;
        const TReal32 falloff = 1.0f - distance / 1000.0f;
        aGame.Damage(player, NULL, (TInt)(500.0f * falloff * falloff * 1.2f + 0.5f),
                     28, EFalse, 2);
        }
    CMap* map = aGame.iAssets.iMap;
    if (map && map->modelActive)
        {
        for (TInt i = 0; i < gState.iBreakCount; i++)
            {
            const TInt model = gState.iBreakModel[i];
            if (model < 0 || model >= map->numModels) continue;
            const TReal32 dx = ModelCenterX(*map, model) - aGame.iBombX;
            const TReal32 dy = ModelCenterY(*map, model) - aGame.iBombY;
            if (dx * dx + dy * dy < 900.0f * 900.0f) map->modelActive[model] = EFalse;
            }
        }
    aGame.CheckRoundEnd();
    }

static void NextSpectator(CGame& aGame)
    {
    if (aGame.iPlayerCount <= 0)
        {
        aGame.iSpectatorIndex = -1;
        return;
        }
    const TInt mine = BoundedIndex(aGame.iMyIndex, aGame.iPlayerCount);
    const TPlayer& me = aGame.iPlayers[mine];
    const TInt start = aGame.iSpectatorIndex < 0 ? 0 : aGame.iSpectatorIndex;
    for (TInt k = 1; k <= aGame.iPlayerCount; k++)
        {
        const TInt index = (start + k) % aGame.iPlayerCount;
        const TPlayer& player = aGame.iPlayers[index];
        if (player.iAlive && (player.iTeam == me.iTeam || !aGame.AnyAlive(me.iTeam)))
            {
            aGame.iSpectatorIndex = index;
            return;
            }
        }
    aGame.iSpectatorIndex = -1;
    }

// ------------------------------------------------------------------ Nokia FR

static void ReprojectCached(CGame& aGame, TReal32 aYaw, TReal32 aPitch)
    {
    TUint16* destination = aGame.iRenderer.iFrameBuffer;
    const TUint16* source = aGame.iGeneratedFrame;
    const TInt width = aGame.iRenderer.iWidth;
    const TInt height = aGame.iRenderer.iHeight;
    if (!destination || !source || width <= 0 || height <= 0) return;
    TReal32 dyaw = AngleNorm(aYaw - aGame.iGeneratedYaw);
    TReal32 dpitch = aPitch - aGame.iGeneratedPitch;
    dyaw = Clamp(dyaw, -40.0f, 40.0f);
    dpitch = Clamp(dpitch, -40.0f, 40.0f);
    const TReal32 halfFov = MaxF(1.0f, gGeneratedFov) * 0.5f * 3.14159265f / 180.0f;
    const TReal32 scale = (TReal32)MaxI(1, width) * 0.5f / MaxF(0.01f, FloatTan(halfFov));
    const TInt shiftX = (TInt)(FloatTan(dyaw * 3.14159265f / 180.0f) * scale);
    const TInt shiftY = (TInt)(-FloatTan(dpitch * 3.14159265f / 180.0f) * scale);
    const TInt x0 = shiftX > 0 ? MinI(shiftX, width) : 0;
    const TInt x1 = shiftX < 0 ? MaxI(0, width + shiftX) : width;
    for (TInt y = 0; y < height; y++)
        {
        TInt sourceY = y - shiftY;
        if (sourceY < 0) sourceY = 0;
        else if (sourceY >= height) sourceY = height - 1;
        const TInt sourceRow = sourceY * width;
        const TInt destinationRow = y * width;
        if (x1 > x0)
            Mem::Copy(destination + destinationRow + x0,
                      source + sourceRow + x0 - shiftX, (x1 - x0) * sizeof(TUint16));
        if (x0 > 0)
            {
            const TUint16 edge = source[sourceRow];
            for (TInt x = 0; x < x0; x++) destination[destinationRow + x] = edge;
            }
        if (x1 < width)
            {
            const TUint16 edge = source[sourceRow + width - 1];
            for (TInt x = x1; x < width; x++) destination[destinationRow + x] = edge;
            }
        }
    }

// ------------------------------------------------------------------ голос

void CGame::VoiceRecorded(const TUint8* aPcm, TInt aLength)
    {
    if (!aPcm || aLength < 2) return;
    TInt samples = aLength / 2;
    if (samples > KVoiceMaxSamples) samples = KVoiceMaxSamples;
    for (TInt i = 0; i < samples; i++)
        {
        const TUint16 value = (TUint16)((TUint16)aPcm[i * 2] | ((TUint16)aPcm[i * 2 + 1] << 8));
        gState.iRecorded[i] = value;
        }
    gState.iRecordedLength = samples;
    if (gState.iMicTestState == 2)
        {
        TInt peak = 0;
        for (TInt i = 0; i < samples; i++)
            {
            const TInt value = AbsI((TInt)(TInt16)gState.iRecorded[i]);
            if (value > peak) peak = value;
            }
        if (peak < 60)
            {
            gState.iMicTestState = 0;
            CopyText(iStatusLine, sizeof(iStatusLine) / sizeof(TText),
                     _L("Тишина — микрофон выключен или занят"));
            gState.iRecordedLength = 0;
            return;
            }
        VoiceNormalize(gState.iRecorded, samples);
        if (iSoundOn && iVoiceOn)
            iAudio.PlayPcm(reinterpret_cast<const TUint8*>(gState.iRecorded), samples * 2,
                           KVoiceRate, Clamp(iVoiceVolume * iVolume / 100, 30, 100));
        gState.iMicTestState = 3;
        gState.iMicTestAt = User::NTickCount() + (TUint32)(samples * 1000 / KVoiceRate + 300);
        return;
        }
    if (gState.iVoiceTooShort || samples < KVoiceRate / 10)
        {
        gState.iRecordedLength = 0;
        return;
        }
    gState.iPendingVoice = ETrue;
    }

void CGame::AudioError(TInt aError)
    {
    TBuf<128> message;
    message.Copy(_L("Микрофон: ошибка "));
    message.AppendNum(aError);
    CopyText(iStatusLine, sizeof(iStatusLine) / sizeof(TText), message);
    if (gState.iMicTestState == 1 || gState.iMicTestState == 2)
        {
        gState.iMicTestState = 0;
        gState.iRecordedLength = 0;
        }
    if (gState.iVoiceRecording)
        {
        gState.iVoiceRecording = EFalse;
        gState.iPendingVoice = EFalse;
        Center(message, 3.0f, 0xFF8060);
        }
    }

void CGame::VoiceTick(TReal32 /*aDelta*/)
    {
    const TUint32 now = User::NTickCount();
    if (gState.iMicTestState == 1 && now - gState.iMicTestAt >= 3000u)
        {
        gState.iMicTestState = 2;
        gState.iMicTestAt = now;
        gState.iVoiceTooShort = EFalse;
        iAudio.StopRecording();
        }
    if (gState.iMicTestState == 2)
        {
        if (gState.iRecordedLength > 0)
            {
            gState.iRecordedLength = 0;
            }
        else if (now - gState.iMicTestAt > 6000u)
            {
            gState.iMicTestState = 0;
            CopyText(iStatusLine, sizeof(iStatusLine) / sizeof(TText), _L("Запись не пришла"));
            }
        }
    if (gState.iMicTestState == 3 && now >= gState.iMicTestAt)
        {
        gState.iMicTestState = 0;
        CopyText(iStatusLine, sizeof(iStatusLine) / sizeof(TText), _L("Готово. Слышали себя?"));
        }

    const TBool held = (iKeys & EActionVoice) != 0 && iState == EStatePlay
        && iMenu == EMenuNone && !iBuyOpen;
    if (held && !gState.iVoiceHeld && !gState.iVoiceRecording && gState.iMicTestState == 0)
        {
        if (gState.iMpPhase != 2 || !iConnection)
            Center(_L("Голосовой чат — только в сетевой игре"), 2.0f, 0xFFE080);
        else if (!iVoiceOn)
            Center(_L("Голосовой чат выключен (Настройки → Звук)"), 2.0f, 0xFFE080);
        else if (iAudio.StartRecording())
            {
            gState.iVoiceRecording = ETrue;
            gState.iVoiceRecordStart = (TReal32)now;
            gState.iRecordedLength = 0;
            }
        else
            Center(_L("Микрофон недоступен"), 2.0f, 0xFF8060);
        }
    if (gState.iVoiceRecording && (!held || now - (TUint32)gState.iVoiceRecordStart >= 6000u))
        {
        gState.iVoiceRecording = EFalse;
        gState.iVoiceTooShort = now - (TUint32)gState.iVoiceRecordStart < 250u;
        iAudio.StopRecording();
        }
    gState.iVoiceHeld = held;
    if (gState.iPendingVoice && gState.iRecordedLength > 0)
        {
        gState.iPendingVoice = EFalse;
        TInt count = 0;
        VoiceResample(gState.iRecorded, gState.iRecordedLength, KVoiceRate,
                      gState.iVoiceIn, count, KVoiceMaxSamples);
        gState.iRecordedLength = 0;
        if (gState.iMpPhase == 2 && iConnection && count >= KVoiceRate / 10)
            {
            SendVoiceClip(*this, gState.iVoiceIn, count);
            if (gState.iRoomPlayers <= 1)
                Center(_L("Голос отправлен, но в комнате больше никого нет"), 2.0f, 0xFFE080);
            }
        }
    }

// ------------------------------------------------------------------ главный цикл

void CGame::Tick(TReal32 aDelta)
    {
    iRealDelta = aDelta;
    VoiceTick(aDelta);
    const TInt previousKeys = gState.iPrevKeys;

    if (iQuality == 0 && iNokiaFrameRate == 0 && iState == EStatePlay && iMenu == EMenuNone)
        {
        iAutoAccumulator += aDelta;
        iAutoFrames++;
        if (iAutoAccumulator >= 1.5f)
            {
            const TReal32 measured = (TReal32)iAutoFrames / MaxF(0.0001f, iAutoAccumulator);
            iAutoAccumulator = 0;
            iAutoFrames = 0;
            if (iTime > iAutoCooldown)
                {
                if (measured < 18.0f && iRenderLevel < 4)
                    {
                    SetLevel(iRenderLevel + 1);
                    iAutoCooldown = iTime + 2.5f;
                    }
                else if (measured > 28.0f && iRenderLevel > 0)
                    {
                    SetLevel(iRenderLevel - 1);
                    iAutoCooldown = iTime + 5.0f;
                    }
                }
            }
        }

    TReal32 dt = aDelta;
    if (dt > 0.1f) dt = 0.1f;
    if (dt < 0.0f) dt = 0.0f;
    NetworkTick(dt);

    const TUint32 fpsNow = User::NTickCount();
    gState.iFpsCount++;
    if (gState.iFpsTick == 0) gState.iFpsTick = fpsNow;
    if (fpsNow - gState.iFpsTick >= 1000u)
        {
        iFps = gState.iFpsCount;
        gState.iFpsCount = 0;
        gState.iFpsTick = fpsNow;
        }

    if (iState == EStateLoad)
        {
        if (iLoadStage++ < 1)
            {
            gState.iPrevKeys = iKeys;
            return;
            }
        TInt loadError = KErrNone;
        TRAP(loadError, LoadMapL(TPtrC(KMapNames[BoundedIndex(iPendingMap, KMapCount)])));
        if (loadError != KErrNone)
            {
            TPtr message(iLoadMessage, sizeof(iLoadMessage) / sizeof(TText));
            message.Copy(_L("Ошибка загрузки: "));
            message.AppendNum(loadError);
            iState = EStateMenu;
            iMenu = EMenuMain;
            }
        gState.iPrevKeys = iKeys;
        return;
        }
    if (iState == EStateMenu || iState == EStateEnd)
        {
        iTime += dt;
        gState.iPrevKeys = iKeys;
        return;
        }
    const TBool pausedMenu = iMenu != EMenuNone && iMenu != EMenuGame
        && iMenu != EMenuRadio1 && iMenu != EMenuRadio2 && iMenu != EMenuRadio3;
    if (pausedMenu && !iConnection)
        {
        gState.iPrevKeys = iKeys;
        return;
        }

    iTime += dt;
    const TBool client = !iNetworkHost && gState.iMpPhase == 2 && iConnection != NULL;
    if (client) InterpolatePeers(*this, dt);

    if (iPlayerCount > 0 && iMyIndex >= 0 && iMyIndex < iPlayerCount)
        {
        TPlayer& me = iPlayers[iMyIndex];
        if (me.iAlive && iMenu == EMenuNone && !iBuyOpen)
            {
            TExtraPlayer& x = Ex(me);
            if (Pressed(previousKeys, iKeys, EActionReload))
                {
                StartReload(me);
                if (client) SendClientAction(*this, 'L', -1, -1);
                }
            if (Pressed(previousKeys, iKeys, EActionDrop))
                {
                DropCurrent(me);
                if (client) SendClientAction(*this, 'D', -1, -1);
                }
            if (Pressed(previousKeys, iKeys, EActionNextWeapon)) NextWeapon(me);
            if (Pressed(previousKeys, iKeys, EActionLastWeapon))
                SwitchWeapon(me, x.iLast >= 0 ? x.iLast : 0);
            }
        HumanInput(dt);
        }

    for (TInt i = 0; i < iPlayerCount; i++)
        {
        TPlayer& player = iPlayers[i];
        if (!player.iBot || !player.iAlive) continue;
        if (!client || i == iMyIndex) BotThink(player, dt);
        }
    for (TInt i = 0; i < iPlayerCount; i++)
        {
        if (client && i != iMyIndex) continue;
        UpdatePlayer(iPlayers[i], dt);
        }
    // Terminator mode: circle pickup and timers
    if (!client && iMode == 3)
        {
        if (iDisguiseTimer > 0) { iDisguiseTimer -= dt; if (iDisguiseTimer <= 0) { iDisguiseTimer = 0; iIsDisguised = EFalse; } }
        if (iDisguiseCooldown > 0) { iDisguiseCooldown -= dt; if (iDisguiseCooldown < 0) iDisguiseCooldown = 0; }
        if (iTermCircleActive)
            {
            for (TInt i = 0; i < iPlayerCount; i++)
                {
                TPlayer& p = iPlayers[i];
                if (!p.iAlive) continue;
                const TReal32 dx = iTermCircleX - p.iX, dy = iTermCircleY - p.iY, dz = iTermCircleZ - p.iZ;
                if (FloatSqrt(dx*dx+dy*dy+dz*dz) < 60.0f)
                    {
                    iTermCircleActive = EFalse;
                    iTerminatorIdx = i;
                    // pick class: 70% T-800, 20% T-1000, 10% T-X
                    TInt roll = (iRandom.Next() & 0x7FFFFFFF) % 100;
                    iTerminatorClass = roll < 70 ? 1 : (roll < 90 ? 2 : 3);
                    p.iTeam = 1;
                    for (TInt q = 0; q < iPlayerCount; q++) if (q != i) iPlayers[q].iTeam = 2;
                    // set stats
                    if (iTerminatorClass == 1) p.iHealth = 1500;
                    else if (iTerminatorClass == 2) p.iHealth = 2500;
                    else p.iHealth = 3500;
                    p.iArmor = 200; Ex(p).iHelmet = ETrue;
                    // give all weapons (simplified: set primary/secondary to strong ones)
                    p.iPrimary = 16; p.iSecondary = 6; // AK47, Deagle as example
                    p.iWeapon = p.iPrimary;
                    Center(_L("Terminator: kill all humans!"), 4.0f, 0xFF4444);
                    break;
                    }
                }
            }
        }
    // Hostage Rescue mode logic
    if (!client && iMode == 4)
        {
        // CTs use key to pick up nearby hostages
        for (TInt pi = 0; pi < iPlayerCount; pi++)
            {
            TPlayer& pl = iPlayers[pi];
            if (!pl.iAlive || pl.iTeam != 2) continue;
            if (!pl.iBot && !Ex(pl).iUse) continue;
            // find nearest free hostage (bots pick up automatically within a larger radius)
            TReal32 bestd = 90.0f; TInt besth = -1;
            for (TInt h = 0; h < KMaxHostages; h++)
                {
                if (!iHostageAlive[h] || iHostageRescued[h] || iHostageFollowIdx[h] >= 0) continue;
                const TReal32 dxh = iHostageX[h] - pl.iX, dyh = iHostageY[h] - pl.iY;
                const TReal32 dd = FloatSqrt(dxh*dxh + dyh*dyh);
                if (dd < bestd) { bestd = dd; besth = h; }
                }
            if (besth >= 0) { iHostageFollowIdx[besth] = pi; Center(_L("Hostage following!"), 2.0f, 0x00FF00); }
            }
        // move hostages toward follower or to rescue zone
        for (TInt h = 0; h < KMaxHostages; h++)
            {
            if (!iHostageAlive[h] || iHostageRescued[h]) continue;
            TInt fidx = iHostageFollowIdx[h];
            if (fidx < 0)
                {
                TInt best = -1; TReal32 bd = 1e12f;
                for (TInt i = 0; i < iPlayerCount; i++)
                    {
                    if (!iPlayers[i].iAlive || iPlayers[i].iTeam != 2) continue;
                    const TReal32 dx = iHostageX[h] - iPlayers[i].iX, dy = iHostageY[h] - iPlayers[i].iY;
                    const TReal32 dd = dx*dx + dy*dy;
                    if (dd < bd) { bd = dd; best = i; }
                    }
                fidx = best; iHostageFollowIdx[h] = best;
                }
            if (fidx >= 0 && fidx < iPlayerCount && iPlayers[fidx].iAlive && iPlayers[fidx].iTeam == 2)
                {
                TPlayer& fol = iPlayers[fidx];
                const TReal32 dx = fol.iX - iHostageX[h], dy = fol.iY - iHostageY[h];
                const TReal32 dist = FloatSqrt(dx*dx + dy*dy);
                if (dist > 40.0f)
                    {
                    const TReal32 spd = 60.0f * dt;
                    const TReal32 nx = iHostageX[h] + (dx / (dist + 0.001f)) * spd;
                    const TReal32 ny = iHostageY[h] + (dy / (dist + 0.001f)) * spd;
                    if (iAssets.iMap)
                        {
                        iAssets.iMap->Trace(0, iHostageX[h], iHostageY[h], iHostageZ[h] + 10.0f, nx, ny, iHostageZ[h] + 10.0f);
                        if (iAssets.iMap->rFrac >= 0.99f) { iHostageX[h] = nx; iHostageY[h] = ny; }
                        else
                            {
                            iAssets.iMap->Trace(0, iHostageX[h], iHostageY[h], iHostageZ[h] + 10.0f, nx, iHostageY[h], iHostageZ[h] + 10.0f);
                            if (iAssets.iMap->rFrac >= 0.99f) iHostageX[h] = nx;
                            iAssets.iMap->Trace(0, iHostageX[h], iHostageY[h], iHostageZ[h] + 10.0f, iHostageX[h], ny, iHostageZ[h] + 10.0f);
                            if (iAssets.iMap->rFrac >= 0.99f) iHostageY[h] = ny;
                            }
                        }
                    else { iHostageX[h] = nx; iHostageY[h] = ny; }
                    iHostageZ[h] = GroundHeight(iHostageX[h], iHostageY[h], 0);
                    }
                // check rescue zone
                const TReal32 rdx = iRescueZoneX - iHostageX[h], rdy = iRescueZoneY - iHostageY[h];
                if (FloatSqrt(rdx*rdx + rdy*rdy) < 60.0f)
                    {
                    iHostageRescued[h] = ETrue; iHostageAlive[h] = EFalse; iHostageFollowIdx[h] = -1;
                    AddMoney(fol, 300);
                    Center(_L("Hostage rescued! +$300"), 3.0f, 0x00FF00);
                    }
                }
            else
                {
                iHostageFollowIdx[h] = -1; // lost follower
                }
            }
        }
    UpdateGrenades(dt);
    UpdateEffects(dt);
    if (!client)
        {
        UpdateDrops(dt);
        UpdateBomb(*this);
        ModeTick(*this);
        }

    for (TInt i = 0; i < KMaxRadioQueue; i++)
        {
        if (gState.iRadioQueuePlayer[i] <= 0 || iTime < gState.iRadioQueueAt[i]) continue;
        const TInt id = gState.iRadioQueuePlayer[i] - 1;
        const TInt command = gState.iRadioQueueCmd[i];
        gState.iRadioQueuePlayer[i] = 0;
        TPlayer* sender = PlayerById(id);
        if (sender && sender->iAlive) Radio(*sender, command);
        }

    if (!iRoundOver && !client) CheckRoundEnd();
    if (iRoundOver && iTime > iRoundEndAt && !client)
        {
        const TBool matchOver = iMode == 1 || iScoreT >= (iMaxRounds / 2 + 1)
            || iScoreCT >= (iMaxRounds / 2 + 1) || iRound >= iMaxRounds;
        if (matchOver)
            {
            if (iNetworkHost && iConnection) HostSendSnapshot(*this);
            iState = EStateEnd;
            iAudio.StopAll();
            if (iNetworkHost && iConnection) HostSendSnapshot(*this);
            }
        else
            NewRound(EFalse);
        }
    if (iHurtFlash > 0.0f)
        {
        iHurtFlash -= dt;
        if (iHurtFlash < 0.0f) iHurtFlash = 0.0f;
        }
    if (iCenterMessageTime > 0.0f)
        {
        iCenterMessageTime -= dt;
        if (iCenterMessageTime < 0.0f) iCenterMessageTime = 0.0f;
        }
    if (iPlayerCount > 0 && iMyIndex >= 0 && iMyIndex < iPlayerCount)
        {
        TPlayer& me = iPlayers[iMyIndex];
        if (!me.iAlive && (iSpectatorIndex < 0 || iSpectatorIndex >= iPlayerCount
            || !iPlayers[iSpectatorIndex].iAlive))
            NextSpectator(*this);
        }
    gState.iPrevKeys = iKeys;
    }

// ------------------------------------------------------------------ кадр

void CGame::DrawFrame()
    {
    const TInt fullWidth = iRenderer.iWidth > 0 ? iRenderer.iWidth : iWidth;
    const TInt fullHeight = iRenderer.iHeight > 0 ? iRenderer.iHeight : iHeight;
    TUint16* fullBuffer = iRenderer.iFrameBuffer;
    if (fullWidth <= 0 || fullHeight <= 0 || !fullBuffer) return;

    TPlayer* view = NULL;
    TBool spectating = EFalse;
    if (iPlayerCount > 0 && iMyIndex >= 0 && iMyIndex < iPlayerCount)
        {
        view = &iPlayers[iMyIndex];
        spectating = !view->iAlive;
        if (spectating && iSpectatorIndex >= 0 && iSpectatorIndex < iPlayerCount)
            view = &iPlayers[iSpectatorIndex];
        }
    TReal32 cameraX = 0, cameraY = 0, cameraZ = 0, yaw = 0, pitch = 0;
    TReal32 fov = 90.0f;
    TBool onGround = ETrue;
    TReal32 speed = 0;
    if (view)
        {
        cameraX = view->iX;
        cameraY = view->iY;
        cameraZ = EyeZ(*view);
        const TExtraPlayer& extra = Ex(*view);
        yaw = view->iYaw + extra.iPunchYaw;
        pitch = view->iPitch + extra.iPunchPitch;
        onGround = view->iOnGround != EFalse;
        speed = SpeedFactor(*view);
        if (view->iZoom > 0 && view->iWeapon >= 0 && view->iWeapon < KWeaponCount)
            {
            if (view->iZoom == 1 && KWpZoom1[view->iWeapon] > 0)
                fov = (TReal32)KWpZoom1[view->iWeapon];
            else if (view->iZoom == 2 && KWpZoom2[view->iWeapon] > 0)
                fov = (TReal32)KWpZoom2[view->iWeapon];
            }
        }
    if (spectating && iSpectatorIndex < 0 && iPlayerCount > 0
        && iMyIndex >= 0 && iMyIndex < iPlayerCount)
        {
        yaw = iDeadCameraYaw + iPlayers[iMyIndex].iYaw;
        pitch = 15.0f;
        cameraZ = iPlayers[iMyIndex].iZ + 30.0f;
        }
    if (onGround && speed > 30.0f)
        iViewBobAmount += speed * 0.018f * 0.033f * 30.0f
            / MaxF(10.0f, (TReal32)(iFps > 0 ? iFps : 20));
    const TReal32 bob = iViewBob
        ? FloatSin(iTime * 9.0f) * MinF(1.0f, speed / 250.0f) * 1.2f : 0.0f;

    TInt renderWidth = fullWidth;
    TInt renderHeight = fullHeight;
    const TBool lowRes = iState == EStatePlay && iRenderLevel > 0 && iRenderLevel < 5
        && gLowFrame != NULL;
    if (lowRes)
        {
        renderWidth = MaxI(1, KRenderLevels[BoundedIndex(iRenderLevel, 5)] * fullWidth / 320);
        renderHeight = MaxI(1, KRenderLevels[BoundedIndex(iRenderLevel, 5)] * fullHeight / 320);
        }
    iRenderer.iWidth = renderWidth;
    iRenderer.iHeight = renderHeight;
    iRenderer.iFrameBuffer = lowRes ? gLowFrame : fullBuffer;
    iRenderer.ClearFrame(0);
    iRenderer.SetCamera(cameraX, cameraY, cameraZ + (onGround ? bob * 0.5f : 0.0f),
                        yaw, pitch, fov);

    const TBool nfr = iNokiaFrameRate > 0 && iNokiaFrameRate <= 5
        && iState == EStatePlay && iMenu == EMenuNone;
    const TUint32 drawNow = User::NTickCount();
    TBool generatedFrame = EFalse;
    if (nfr)
        {
        if (gState.iLastDrawTick > 0)
            {
            const TInt interval = MinI(500, (TInt)(drawNow - gState.iLastDrawTick));
            const TInt target = MaxI(1, KNfrTargets[BoundedIndex(iNokiaFrameRate, 6)]);
            if (gNfrLastGenerated)
                gState.iNfrGenInterval += ((TReal32)interval - gState.iNfrGenInterval) * 0.2f;
            else
                gState.iNfrRealInterval += ((TReal32)MaxI(interval, 1000 / target)
                    - gState.iNfrRealInterval) * 0.25f;
            }
        gState.iLastDrawTick = drawNow;
        const TReal32 dx = cameraX - gGeneratedX;
        const TReal32 dy = cameraY - gGeneratedY;
        const TReal32 dz = cameraZ - gGeneratedZ;
        if (gNfrLeft > 0 && iGeneratedValid && iGeneratedFrame && fov == gGeneratedFov
            && renderWidth == gGeneratedRenderWidth && renderHeight == gGeneratedRenderHeight
            && dx * dx + dy * dy + dz * dz < 48.0f * 48.0f)
            {
            gNfrLeft--;
            iRenderSkip = gNfrLeft;
            gNfrLastGenerated = ETrue;
            iFramesGenerated++;
            ReprojectCached(*this, yaw, pitch);
            iRenderer.iWidth = fullWidth;
            iRenderer.iHeight = fullHeight;
            iRenderer.iFrameBuffer = fullBuffer;
            DrawHud();
            return;
            }
        }
    else
        {
        iGeneratedValid = EFalse;
        iRenderSkip = 0;
        gNfrLeft = 0;
        gState.iLastDrawTick = 0;
        }
    gNfrLastGenerated = EFalse;
    iFramesGenerated = 0;

    Draw();

    if (lowRes && iRenderer.iFrameBuffer && iRenderer.iFrameBuffer != fullBuffer)
        {
        UpscaleBilinear(iRenderer.iFrameBuffer, renderWidth, renderHeight,
                        fullBuffer, fullWidth, fullHeight);
        }
    iRenderer.iWidth = fullWidth;
    iRenderer.iHeight = fullHeight;
    iRenderer.iFrameBuffer = fullBuffer;

    if (nfr && iGeneratedFrame)
        {
        const TInt pixels = MinI(fullWidth * fullHeight, iGeneratedWidth * iGeneratedHeight);
        if (pixels > 0)
            {
            Mem::Copy(iGeneratedFrame, fullBuffer, pixels * sizeof(TUint16));
            gGeneratedX = cameraX;
            gGeneratedY = cameraY;
            gGeneratedZ = cameraZ;
            gGeneratedFov = fov;
            gGeneratedRenderWidth = renderWidth;
            gGeneratedRenderHeight = renderHeight;
            iGeneratedValid = ETrue;
            generatedFrame = ETrue;
            }
        }
    if (!generatedFrame) iGeneratedValid = EFalse;
    if (iState == EStatePlay) DrawHud();

    if (nfr)
        {
        const TInt target = MaxI(1, KNfrTargets[BoundedIndex(iNokiaFrameRate, 6)]);
        const TReal32 targetMs = 1000.0f / (TReal32)target;
        const TReal32 realMs = MaxF(gState.iNfrRealInterval,
            1000.0f / MaxF(1.0f, (TReal32)(iFps > 0 ? iFps : 20)));
        const TReal32 genMs = MaxF(2.0f, gState.iNfrGenInterval);
        TInt planned = 0;
        if (realMs > targetMs * 1.05f && genMs < targetMs)
            {
            const TReal32 excess = realMs - targetMs;
            const TReal32 perFrame = MaxF(1.0f, targetMs - genMs);
            planned = (TInt)(excess / perFrame + 0.999f);
            }
        gNfrLeft = Clamp(planned, 0, 3);
        iRenderSkip = gNfrLeft;

        if (gState.iNfrStatTick == 0) gState.iNfrStatTick = drawNow;
        if (drawNow - gState.iNfrStatTick >= 1000u)
            {
            const TReal32 seconds = MaxF(0.001f, (TReal32)(drawNow - gState.iNfrStatTick) / 1000.0f);
            const TInt realFps = (TInt)(gState.iNfrRealFrames / seconds + 0.5f);
            const TInt generatedFps = (TInt)(gState.iNfrGenFrames / seconds + 0.5f);
            gState.iNfrRealFrames = 0;
            gState.iNfrGenFrames = 0;
            gState.iNfrStatTick = drawNow;
            if (iTime > gState.iNfrCool && iState == EStatePlay && iMenu == EMenuNone)
                {
                if ((realFps < target * 0.5f || realFps + generatedFps < target * 0.9f)
                    && iRenderLevel < 4)
                    {
                    SetLevel(iRenderLevel + 1);
                    gState.iNfrCool = iTime + 2.0f;
                    iGeneratedValid = EFalse;
                    }
                else if (realFps > target * 0.8f && iRenderLevel > 0)
                    {
                    SetLevel(iRenderLevel - 1);
                    gState.iNfrCool = iTime + 4.0f;
                    iGeneratedValid = EFalse;
                    }
                }
            }
        if (gNfrLastGenerated) gState.iNfrGenFrames++;
        else gState.iNfrRealFrames++;
        }
    }

void CGame::CopyViewExtras(TPlayerViewExtra* aDestination) const
    {
    if (!aDestination) return;
    for (TInt i = 0; i < KMaxPlayers; i++)
        {
        const TPlayer& player = iPlayers[i];
        const TExtraPlayer& source = gState.iExtra[i];
        TPlayerViewExtra& destination = aDestination[i];
        destination.iHelmet = source.iHelmet;
        destination.iDefuser = source.iDefuser;
        destination.iC4 = player.iHasC4;
        destination.iPlanting = player.iPlanting;
        destination.iDefusing = source.iDefusing;
        destination.iZombie = source.iZombie;
        destination.iFlash = source.iFlash;
        destination.iHe = source.iHe;
        destination.iSmoke = source.iSmoke;
        destination.iNextAttack = source.iNextAttack;
        destination.iFlashUntil = source.iFlashUntil;
        destination.iDefuseProgress = source.iDefuseProgress;
        destination.iDeathAnim = source.iZType < 0 ? 0 : (source.iZType > 6 ? 6 : source.iZType);
        destination.iLastShotSilent = player.iLastShotSilent;
        destination.iSilM4 = source.iSilM4;
        destination.iSilUSP = source.iSilUSP;
        destination.iLast = source.iLast;
        destination.iPunchYaw = source.iPunchYaw;
        destination.iPunchPitch = source.iPunchPitch;
        destination.iRecoil = source.iRecoil;
        destination.iVmWeapon = player.iWeapon;
        destination.iVmSet = source.iVmSet;
        destination.iVmAnimation = source.iVmAnimation;
        destination.iVmTime = source.iVmTime;
        destination.iVmLength = source.iVmLength;
        for (TInt character = 0; character < 32; character++)
            destination.iVmName[character] = source.iVmName[character];
        }
    }
