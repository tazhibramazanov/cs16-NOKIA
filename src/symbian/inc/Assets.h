#ifndef CS16_ASSETS_H
#define CS16_ASSETS_H

/*
 * Assets.h -- загрузчики бинарных форматов, которые создаёт conv/.
 * Форматы не меняются: CSE3 (карты), CSM2 (модели), CSA3 (анимации),
 * CST1 (текстуры моделей), HUD1 (спрайты), FNT1 (шрифты), WAV (звуки).
 *
 * Все имена в файлах -- ASCII, поэтому TAsciiName хранит их inline и не
 * требует аллокаций при поиске текстур/костей/сущностей.
 */

#include "Engine.h"

const TInt KCsMaxAssetName = 64;
const TInt KSkySide = 128;
const TInt KModelTextures = 512;

struct TAsciiName
    {
    TUint8 iData[KCsMaxAssetName];
    TInt iLength;

    TAsciiName() : iLength(0) { iData[0] = 0; }

    void Set(const TPtrC8& aText)
        {
        iLength = aText.Length() < KCsMaxAssetName - 1 ? aText.Length() : KCsMaxAssetName - 1;
        Mem::Copy(iData, aText.Ptr(), iLength);
        iData[iLength] = 0;
        }

    TPtrC8 Des() const { return TPtrC8(iData, iLength); }

    TBool Equals(const TPtrC8& aText) const
        {
        return aText.Length() == iLength && Mem::Compare(iData, iLength, aText.Ptr(), iLength) == 0;
        }

    TBool Equals(const char* aText) const
        {
        TInt length = 0;
        while (aText[length] && length < KCsMaxAssetName) length++;
        return length == iLength && Mem::Compare(iData, iLength, (const TUint8*)aText, length) == 0;
        }
    };

/* ---------------------------------------------------------------- карта */

class CMap
    {
public:
    CMap();
    ~CMap();

    void LoadL(RFs& aFileServer, const TDesC& aDirectory, const TDesC& aName);

    TUint32 pal[256];
    TReal32 tintR, tintG, tintB;

    TInt numTex;
    TAsciiName* texName;
    TInt16* texFlags;
    TInt* texW;
    TInt* texH;
    TUint8** texMip;       /* [numTex*3][] */
    TInt* texMipLength;
    TReal32* texXScale;
    TReal32* texYScale;

    TReal32* plNx;
    TReal32* plNy;
    TReal32* plNz;
    TReal32* plD;
    TUint8* plType;

    TInt numVerts;
    TReal32* vx;
    TReal32* vy;
    TReal32* vz;
    TInt numFverts;
    TInt16* fvert;

    TInt numFaces;
    TInt16* fPlane;
    TUint8* fSide;
    TInt* fFirstV;
    TInt16* fNumV;
    TInt16* fTex;
    TReal32* fS;           /* numFaces*4 */
    TReal32* fT;
    TReal32* fLS;
    TReal32* fLT;
    TInt* fLmOfs;
    TUint8* fLmW;
    TUint8* fLmH;

    TInt numNodes;
    TInt* nPlane;
    TInt16* nChild0;
    TInt16* nChild1;
    TInt16* nBox;          /* numNodes*6 */
    TInt16* nFirstFace;
    TInt16* nNumFaces;
    TInt* nParent;

    TInt numLeaves;
    TInt* lContents;
    TInt* lVisOfs;
    TInt16* lBox;
    TInt16* lFirstMark;
    TInt16* lNumMarks;
    TInt* lParent;
    TInt16* marks;
    TUint8* vis;
    TInt visLength;

    TInt numClipNodes;
    TInt* cPlane;
    TInt16* cChild0;
    TInt16* cChild1;
    TInt* h0Plane;
    TInt* h0Child0;
    TInt* h0Child1;

    TInt numModels;
    TReal32* mMins;
    TReal32* mMaxs;
    TReal32* mOrigin;
    TInt* mHead;           /* numModels*4 */
    TInt* mFirstFace;
    TInt* mNumFaces;

    struct SEntity
        {
        TInt iCount;
        TAsciiName iKeys[32];
        TAsciiName iValues[32];
        };
    TInt numEntities;
    SEntity* entities;

    TUint8* light;
    TInt lightLength;
    TAsciiName skyName;
    TUint16 sky565[5][KSkySide * KSkySide];

    TInt* solidModels;
    TInt numSolidModels;
    TBool* modelActive;

    /* Состояние трассировки: результаты Trace в полях t и r. */
    TBool tAllSolid, tStartSolid, tInOpen, tInWater;
    TReal32 tFrac, tEx, tEy, tEz, tNx, tNy, tNz, tPd;
    TReal32 rFrac, rEx, rEy, rEz, rNx, rNy, rNz;
    TBool rStartSolid, rAllSolid;
    TInt rModel;

    TInt PointLeaf(TReal32 aX, TReal32 aY, TReal32 aZ) const;
    TInt HullPointContents(TInt aHull, TInt aNode, TReal32 aX, TReal32 aY, TReal32 aZ) const;
    void TraceModel(TInt aModel, TInt aHull, TReal32 aX1, TReal32 aY1, TReal32 aZ1,
                    TReal32 aX2, TReal32 aY2, TReal32 aZ2);
    void Trace(TInt aHull, TReal32 aX1, TReal32 aY1, TReal32 aZ1,
               TReal32 aX2, TReal32 aY2, TReal32 aZ2);
    TInt SurfaceTex(TInt aModel, TReal32 aX1, TReal32 aY1, TReal32 aZ1,
                    TReal32 aX2, TReal32 aY2, TReal32 aZ2);
    TBool InFace(TInt aFace, TReal32 aX, TReal32 aY, TReal32 aZ) const;
    TInt LightAt(TReal32 aX, TReal32 aY, TReal32 aZ);
    const TAsciiName* EntityValue(TInt aEntity, const TPtrC8& aKey) const;
    TInt FindEntity(const TPtrC8& aClassname) const;

private:
    TBool RecursiveHullCheck(TInt aNode, TReal32 aP1f, TReal32 aP2f,
                             TReal32 aP1x, TReal32 aP1y, TReal32 aP1z,
                             TReal32 aP2x, TReal32 aP2y, TReal32 aP2z);
    TBool SurfaceRecursive(TInt aNode, TReal32 aX1, TReal32 aY1, TReal32 aZ1,
                           TReal32 aX2, TReal32 aY2, TReal32 aZ2);
    TBool LightRecursive(TInt aNode, TReal32 aX1, TReal32 aY1, TReal32 aZ1,
                         TReal32 aX2, TReal32 aY2, TReal32 aZ2);
    void SetParent(TInt aChild, TInt aParent);
    void LoadSkyL(RFs& aFileServer, const TDesC& aDirectory);
    void BuildMips(const TUint8* aRawPalette, TInt aTexture);

private:
    TInt iTraceHull;
    TInt iTraceHead;
    TInt iSurfaceTex;
    TInt iLightBest;
    TUint8 iRawPalette[768];
    TUint8 iQuantTable[32768];
    };

/* ------------------------------------------------------- модели и анимации */

struct TModelMesh
    {
    TInt iVertexCount;
    TInt iNormalCount;
    TUint8* iVertexBone;
    TReal32* iVertex;
    TUint8* iNormalBone;
    TReal32* iNormal;
    TInt iMaterialCount;
    TInt16* iMaterial;
    TInt16** iTriangles;    /* iMaterialCount массивов по 12 short */
    TInt* iTriangleLength;

    TModelMesh();
    ~TModelMesh();
    };

class CModel
    {
public:
    CModel();
    ~CModel();

    static TInt LoadModelTexturesL(RFs& aFileServer, const TDesC& aDirectory);
    static CModel* LoadL(RFs& aFileServer, const TDesC& aDirectory, const TDesC& aName);
    TInt Bone(const TPtrC8& aName) const;
    TInt Bone(const char* aName) const;

    void Build(TReal32* aQ, TReal32* aP, const TReal32* aEntity, TReal32* aOut) const;
    void BuildDefault(const TReal32* aEntity, TReal32* aOut) const;
    void BuildMerged(const TInt* aMap, const TReal32* aParentWorld,
                     const TReal32* aEntity, TReal32* aOut) const;
    TInt* MergeMapL(const CModel& aParent) const;

    static void Qmat(TReal32 aX, TReal32 aY, TReal32 aZ, TReal32 aW,
                     TReal32 aPx, TReal32 aPy, TReal32 aPz, TReal32* aOut, TInt aOffset);
    static void Mul(const TReal32* aA, TInt aAOffset, const TReal32* aB, TInt aBOffset,
                    TReal32* aC, TInt aCOffset);
    static void EntityMatrix(TReal32 aX, TReal32 aY, TReal32 aZ, TReal32 aYawDeg, TReal32* aOut);

    TInt iBoneCount;
    TUint8* iParent;
    TAsciiName* iBoneName;
    TReal32* iDefaultPosition;
    TReal32* iDefaultRotation;
    TInt iPartCount;
    TAsciiName* iPartName;
    TModelMesh*** iParts;    /* [iPart][sub][mesh] */

    static TUint32 iModelPalette[256];
    static TUint8* iModelTexture[KModelTextures];
    static TInt iModelTextureWidth[KModelTextures];
    static TInt iModelTextureHeight[KModelTextures];
    static TInt iModelTextureCount;
    };

class CAnimation
    {
public:
    CAnimation();
    ~CAnimation();

    static CAnimation* LoadL(RFs& aFileServer, const TDesC& aDirectory, const TDesC& aName);
    TInt Find(const TPtrC8& aName) const;
    TInt Find(const char* aName) const;
    TReal32 Duration(TInt aSequence) const;
    void Pose(TInt aSequence, TReal32 aFrame, TBool aLoop, TReal32 aBlend,
              TReal32* aRotation, TReal32* aPosition) const;

    TInt iBoneCount;
    TInt iSequenceCount;
    TAsciiName* iName;
    TReal32* iFps;
    TInt* iFrames;
    TInt* iBlends;
    TInt** iBones;
    TBool** iPositionAnimated;
    TBool** iRotationAnimated;
    TInt16** iConstantRotation;
    TInt16** iConstantPosition;
    TInt** iOffset;
    TInt* iStride;
    TInt16** iData;
    };

/* --------------------------------------------------------------- спрайты */

/*
 * Универсальный набор спрайтов. Поддерживаются все три формата, которые
 * использует Java-версия: HUD1 (пиксели интерфейса), VWS1 (оружие от
 * первого лица) и PLS1 (8 направлений игроков). Порядок кадров в PLS1
 * совпадает с Java: [анимация][направление*nfr + кадр].
 */
class CSpriteSet
    {
public:
    CSpriteSet();
    ~CSpriteSet();
    void LoadL(RFs& aFileServer, const TDesC& aFullPath);

    struct TSprite
        {
        TAsciiName iName;
        TInt iWidth;
        TInt iHeight;
        TUint8* iPixels;
        TInt16 iOffsetX;      /* fx из VWS1/PLS1 */
        TInt16 iOffsetY;      /* fy из VWS1/PLS1 */
        };

    TInt Find(const char* aName) const;
    TInt FindAnim(const TPtrC8& aName) const;
    TInt FrameIndex(TInt aAnimation, TInt aDirection, TInt aFrame) const;
    const TSprite* Frame(TInt aAnimation, TInt aDirection, TInt aFrame) const;
    const TUint32* Lit(TInt aLevel);

    TInt iCount;
    TSprite* iSprites;

    TBool iPlayer;            /* ETrue для PLS1 */
    TInt iDirs;               /* 8 для PLS1, 1 для VWS1/HUD1 */
    TReal32 iPpu;             /* пикселей на игровую единицу */
    TInt iAx, iAy;            /* точка привязки */
    TInt iAnimCount;
    TAsciiName* iAnim;
    TReal32* iDur;
    TInt* iNfr;
    TInt* iFrameStart;
    TUint32 iPal[256];
    TUint32 iLitPal[256];
    TInt iLitLevel;

private:
    void Release();
    void BuildPalette();
    };

/* Гамма-таблица рендера используется и для подсветки спрайтов. */
void SetSpriteGamma(const TUint8* aGamma);

class CCSFont
    {
public:
    CCSFont();
    ~CCSFont();
    void LoadL(RFs& aFileServer, const TDesC& aFullPath);

    struct TGlyph
        {
        TUint16 iCode;
        TInt iWidth;
        TUint8* iPixels;
        };
    TInt iHeight;
    TInt iCount;
    TGlyph* iGlyphs;
    const TGlyph* Glyph(TUint16 aCode) const;
    };

/* -------------------------------------------------------------- менеджер */

const TInt KMaxMods = 8;

class CAssetManager
    {
public:
    CAssetManager();
    ~CAssetManager();

    void ConstructL(RFs& aFileServer, const TDesC& aAssetDirectory);
    void ScanModsL();
    TInt ModCount() const;
    TPtrC ModName(TInt aIndex) const;
    void SetMod(TInt aIndex); /* -1 = no mod (base) */
    TInt CurrentModIndex() const;
    TPtrC CurrentModName() const;
    TPtrC CurrentModPath() const;
    TBool HasModOverride(const TDesC& aRelativePath) const;
    void ResolveModPath(const TDesC& aRelativePath, TDes& aOut) const;
    void ResolveAssetPath(const TDesC& aRelativePath, TDes& aOut) const;

    CMap* LoadMapL(const TDesC& aName);
    CModel* Model(const char* aName);
    CAnimation* Animation(const char* aName);
    CModel* WeaponModel(const char* aName);
    CAnimation* WeaponAnimation(const char* aName);

    RFs* iFileServer;
    HBufC* iDirectoryBuffer;
    TPtrC iDirectory;
    HBufC* iModRootBuffer;
    TPtrC iModRoot; /* empty if none selected */
    TInt iCurrentModIndex;
    TInt iModCount;
    TText iModNames[KMaxMods][32];
    CMap* iMap;
    CSpriteSet iHud;
    CSpriteSet iSpritesT;
    CSpriteSet iSpritesCT;
    CCSFont iFontSmall;
    CCSFont iFontBig;
    };

#endif
