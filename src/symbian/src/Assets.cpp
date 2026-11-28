/*
 * Assets.cpp -- native loaders for the binary formats produced by conv/.
 *
 * The formats are byte-for-byte compatible with the Java reference code:
 * CSE3 maps, CSM2 models, CSA3 animations, CST1 model textures, HUD1 sprite
 * containers, FNT1 fonts and raw little-endian RGB565 sky strips.
 * No C++ exceptions are used. Every query works without allocating memory,
 * and all field arrays are allocated in LoadL and released in the destructor.
 */

#include "Assets.h"

#include <e32math.h>

const TInt KContentsEmpty = -1;
const TInt KContentsSolid = -2;

const TReal32 KDistanceEpsilon = 0.03125f;
const TReal32 KQuaternionScale = 1.0f / 32767.0f;

const TInt KMaxCachedModels = 128;
const TInt KMaxCachedAnimations = 128;
const TInt KMaxPathLength = 512;
const TInt KMaxEntityKeys = 32;
const TInt KSkyFacePixels = KSkySide * KSkySide;
const TInt KSkyStripBytes = 5 * KSkyFacePixels * 2;
const TUint16 KFlatSkyColour = 0x6FA8;
const TInt KMaxSideTableRecords = 4096;

typedef TBuf<KMaxPathLength> TPathBuffer;

/* ---------------------------------------------------------------- helpers */

static TInt AsciiLength(const char* aText)
    {
    TInt length = 0;
    if (aText)
        {
        while (aText[length] && length < 1024) length++;
        }
    return length;
    }

static TPtrC8 AsciiName(const char* aText)
    {
    return TPtrC8((const TUint8*)aText, AsciiLength(aText));
    }

static void AppendAscii(TDes& aTarget, const TPtrC8& aText)
    {
    for (TInt i = 0; i < aText.Length(); i++)
        aTarget.Append((TChar)aText[i]);
    }

static void AppendAscii(TDes& aTarget, const char* aText)
    {
    const TInt length = AsciiLength(aText);
    for (TInt i = 0; i < length; i++)
        aTarget.Append((TChar)(TUint8)aText[i]);
    }

static void AppendLeaf(TPathBuffer& aTarget, const TDesC& aName, const TDesC& aExtension)
    {
    aTarget.Copy(aName);
    aTarget.Append(aExtension);
    }

static void MakePath(TPathBuffer& aTarget, const TDesC& aDirectory, const TDesC& aLeaf)
    {
    aTarget.Copy(aDirectory);
    if (aTarget.Length() == 0 ||
        (aTarget[aTarget.Length() - 1] != (TUint16)'\\' && aTarget[aTarget.Length() - 1] != (TUint16)'/'))
        aTarget.Append(_L("\\"));
    aTarget.Append(aLeaf);
    }

static void MakeAsciiPath(TPathBuffer& aTarget, const TDesC& aDirectory,
                          const TDesC& aPrefix, const TPtrC8& aName, const TDesC& aExtension)
    {
    TPathBuffer leaf;
    leaf.Copy(aPrefix);
    AppendAscii(leaf, aName);
    leaf.Append(aExtension);
    MakePath(aTarget, aDirectory, leaf);
    }

static TBool FileExists(RFs& aFileServer, const TDesC& aPath)
    {
    RFile file;
    if (file.Open(aFileServer, aPath, EFileRead | EFileShareAny) != KErrNone) return EFalse;
    file.Close();
    return ETrue;
    }

static HBufC8* ReadFileOrNullL(RFs& aFileServer, const TDesC& aPath)
    {
    if (!FileExists(aFileServer, aPath)) return NULL;
    return ReadWholeFileL(aFileServer, aPath);
    }

template <class T> static T* NewArrayL(TInt aCount)
    {
    T* result = new T[aCount > 0 ? aCount : 1];
    if (!result) User::Leave(KErrNoMemory);
    return result;
    }

static void DeleteMapObject(TAny* aObject) { delete (CMap*)aObject; }
static void DeleteModelObject(TAny* aObject) { delete (CModel*)aObject; }
static void DeleteAnimationObject(TAny* aObject) { delete (CAnimation*)aObject; }

static TReal32 Smaller(TReal32 aFirst, TReal32 aSecond)
    { return aFirst < aSecond ? aFirst : aSecond; }

static TReal32 Larger(TReal32 aFirst, TReal32 aSecond)
    { return aFirst > aSecond ? aFirst : aSecond; }

/* The public model and animation structures do not expose the number of
 * submodels in a part or the number of used bones in a sequence. These side
 * tables keep those counts so that destructors and pose blending can walk
 * the arrays without changing the published header. */
struct TSubMeshRecord
    {
    TModelMesh** iArray;
    TInt iCount;
    };

struct TSequenceBoneRecord
    {
    const TInt* iArray;
    TInt iCount;
    };

static TSubMeshRecord gSubMeshRecords[KMaxSideTableRecords];
static TSequenceBoneRecord gSequenceBoneRecords[KMaxSideTableRecords];

static void RegisterSubMeshes(TModelMesh** aArray, TInt aCount)
    {
    for (TInt i = 0; i < KMaxSideTableRecords; i++)
        {
        if (gSubMeshRecords[i].iArray != NULL) continue;
        gSubMeshRecords[i].iArray = aArray;
        gSubMeshRecords[i].iCount = aCount;
        return;
        }
    }

static TInt SubMeshCount(TModelMesh** aArray)
    {
    for (TInt i = 0; i < KMaxSideTableRecords; i++)
        if (gSubMeshRecords[i].iArray == aArray) return gSubMeshRecords[i].iCount;
    return 0;
    }

static void UnregisterSubMeshes(TModelMesh** aArray)
    {
    for (TInt i = 0; i < KMaxSideTableRecords; i++)
        {
        if (gSubMeshRecords[i].iArray != aArray) continue;
        gSubMeshRecords[i].iArray = NULL;
        gSubMeshRecords[i].iCount = 0;
        return;
        }
    }

static void RegisterSequenceBones(const TInt* aArray, TInt aCount)
    {
    for (TInt i = 0; i < KMaxSideTableRecords; i++)
        {
        if (gSequenceBoneRecords[i].iArray != NULL) continue;
        gSequenceBoneRecords[i].iArray = aArray;
        gSequenceBoneRecords[i].iCount = aCount;
        return;
        }
    }

static TInt SequenceBoneCount(const TInt* aArray)
    {
    for (TInt i = 0; i < KMaxSideTableRecords; i++)
        if (gSequenceBoneRecords[i].iArray == aArray) return gSequenceBoneRecords[i].iCount;
    return 0;
    }

static void UnregisterSequenceBones(const TInt* aArray)
    {
    for (TInt i = 0; i < KMaxSideTableRecords; i++)
        {
        if (gSequenceBoneRecords[i].iArray != aArray) continue;
        gSequenceBoneRecords[i].iArray = NULL;
        gSequenceBoneRecords[i].iCount = 0;
        return;
        }
    }

/* ------------------------------------------------------------------- map */

CMap::CMap()
    : tintR(0), tintG(0), tintB(0),
      numTex(0), texName(NULL), texFlags(NULL), texW(NULL), texH(NULL),
      texMip(NULL), texMipLength(NULL), texXScale(NULL), texYScale(NULL),
      plNx(NULL), plNy(NULL), plNz(NULL), plD(NULL), plType(NULL),
      numVerts(0), vx(NULL), vy(NULL), vz(NULL), numFverts(0), fvert(NULL),
      numFaces(0), fPlane(NULL), fSide(NULL), fFirstV(NULL), fNumV(NULL),
      fTex(NULL), fS(NULL), fT(NULL), fLS(NULL), fLT(NULL),
      fLmOfs(NULL), fLmW(NULL), fLmH(NULL),
      numNodes(0), nPlane(NULL), nChild0(NULL), nChild1(NULL), nBox(NULL),
      nFirstFace(NULL), nNumFaces(NULL), nParent(NULL),
      numLeaves(0), lContents(NULL), lVisOfs(NULL), lBox(NULL),
      lFirstMark(NULL), lNumMarks(NULL), lParent(NULL), marks(NULL),
      vis(NULL), visLength(0),
      numClipNodes(0), cPlane(NULL), cChild0(NULL), cChild1(NULL),
      h0Plane(NULL), h0Child0(NULL), h0Child1(NULL),
      numModels(0), mMins(NULL), mMaxs(NULL), mOrigin(NULL), mHead(NULL),
      mFirstFace(NULL), mNumFaces(NULL),
      numEntities(0), entities(NULL), light(NULL), lightLength(0),
      solidModels(NULL), numSolidModels(0), modelActive(NULL),
      tAllSolid(EFalse), tStartSolid(EFalse), tInOpen(EFalse), tInWater(EFalse),
      tFrac(1), tEx(0), tEy(0), tEz(0), tNx(0), tNy(0), tNz(0), tPd(0),
      rFrac(1), rEx(0), rEy(0), rEz(0), rNx(0), rNy(0), rNz(0),
      rStartSolid(EFalse), rAllSolid(EFalse), rModel(0),
      iTraceHull(0), iTraceHead(0), iSurfaceTex(-1), iLightBest(-1)
    {
    Mem::FillZ(pal, sizeof(pal));
    Mem::FillZ(iRawPalette, sizeof(iRawPalette));
    Mem::FillZ(iQuantTable, sizeof(iQuantTable));
    Mem::FillZ(sky565, sizeof(sky565));
    }

CMap::~CMap()
    {
    delete[] texName;
    delete[] texFlags;
    delete[] texW;
    delete[] texH;
    if (texMip)
        {
        const TInt mipCount = numTex * 3;
        for (TInt i = 0; i < mipCount; i++)
            {
            if (!texMip[i]) continue;
            TBool shared = EFalse;
            for (TInt j = 0; j < i; j++)
                {
                if (texMip[j] == texMip[i]) { shared = ETrue; break; }
                }
            if (!shared) delete[] texMip[i];
            }
        }
    delete[] texMip;
    delete[] texMipLength;
    delete[] texXScale;
    delete[] texYScale;

    delete[] plNx;
    delete[] plNy;
    delete[] plNz;
    delete[] plD;
    delete[] plType;

    delete[] vx;
    delete[] vy;
    delete[] vz;
    delete[] fvert;

    delete[] fPlane;
    delete[] fSide;
    delete[] fFirstV;
    delete[] fNumV;
    delete[] fTex;
    delete[] fS;
    delete[] fT;
    delete[] fLS;
    delete[] fLT;
    delete[] fLmOfs;
    delete[] fLmW;
    delete[] fLmH;

    delete[] nPlane;
    delete[] nChild0;
    delete[] nChild1;
    delete[] nBox;
    delete[] nFirstFace;
    delete[] nNumFaces;
    delete[] nParent;

    delete[] lContents;
    delete[] lVisOfs;
    delete[] lBox;
    delete[] lFirstMark;
    delete[] lNumMarks;
    delete[] lParent;
    delete[] marks;
    delete[] vis;

    delete[] cPlane;
    delete[] cChild0;
    delete[] cChild1;
    /* h0Plane aliases nPlane and must not be deleted separately. */
    delete[] h0Child0;
    delete[] h0Child1;

    delete[] mMins;
    delete[] mMaxs;
    delete[] mOrigin;
    delete[] mHead;
    delete[] mFirstFace;
    delete[] mNumFaces;

    delete[] entities;
    delete[] light;
    delete[] solidModels;
    delete[] modelActive;
    }

void CMap::BuildMips(const TUint8* aRawPalette, TInt aTexture)
    {
    const TInt base = aTexture * 3;
    const TInt width = texW[aTexture];
    const TInt height = texH[aTexture];
    const TBool masked = (texFlags[aTexture] & 2) != 0;

    for (TInt mipLevel = 1; mipLevel < 3; mipLevel++)
        {
        const TUint8* source = texMip[base + mipLevel - 1];
        TInt sourceWidth = width >> (mipLevel - 1);
        TInt sourceHeight = height >> (mipLevel - 1);
        if (sourceWidth < 1) sourceWidth = 1;
        if (sourceHeight < 1) sourceHeight = 1;

        if (sourceWidth < 2 || sourceHeight < 2 || (texFlags[aTexture] & 1) != 0)
            {
            texMip[base + mipLevel] = (TUint8*)source;
            texMipLength[base + mipLevel] = texMipLength[base + mipLevel - 1];
            continue;
            }

        const TInt destinationWidth = sourceWidth >> 1;
        const TInt destinationHeight = sourceHeight >> 1;
        TUint8* destination = NewArrayL<TUint8>(destinationWidth * destinationHeight);

        for (TInt y = 0; y < destinationHeight; y++)
            {
            for (TInt x = 0; x < destinationWidth; x++)
                {
                const TInt offset = (y * 2) * sourceWidth + x * 2;
                TInt c0 = source[offset];
                TInt c1 = source[offset + 1];
                TInt c2 = source[offset + sourceWidth];
                TInt c3 = source[offset + sourceWidth + 1];

                if (masked)
                    {
                    TInt transparent = 0;
                    if (c0 == 255) transparent++;
                    if (c1 == 255) transparent++;
                    if (c2 == 255) transparent++;
                    if (c3 == 255) transparent++;
                    if (transparent >= 2)
                        {
                        destination[y * destinationWidth + x] = 255;
                        continue;
                        }
                    if (c0 == 255) c0 = c3 != 255 ? c3 : (c1 != 255 ? c1 : c2);
                    if (c1 == 255) c1 = c0;
                    if (c2 == 255) c2 = c0;
                    if (c3 == 255) c3 = c0;
                    }

                const TInt p0 = ((aRawPalette[c0 * 3] & 255) << 16) |
                                ((aRawPalette[c0 * 3 + 1] & 255) << 8) |
                                (aRawPalette[c0 * 3 + 2] & 255);
                const TInt p1 = ((aRawPalette[c1 * 3] & 255) << 16) |
                                ((aRawPalette[c1 * 3 + 1] & 255) << 8) |
                                (aRawPalette[c1 * 3 + 2] & 255);
                const TInt p2 = ((aRawPalette[c2 * 3] & 255) << 16) |
                                ((aRawPalette[c2 * 3 + 1] & 255) << 8) |
                                (aRawPalette[c2 * 3 + 2] & 255);
                const TInt p3 = ((aRawPalette[c3 * 3] & 255) << 16) |
                                ((aRawPalette[c3 * 3 + 1] & 255) << 8) |
                                (aRawPalette[c3 * 3 + 2] & 255);

                const TInt red = ((p0 >> 16) & 255) + ((p1 >> 16) & 255) +
                                 ((p2 >> 16) & 255) + ((p3 >> 16) & 255);
                const TInt green = ((p0 >> 8) & 255) + ((p1 >> 8) & 255) +
                                   ((p2 >> 8) & 255) + ((p3 >> 8) & 255);
                const TInt blue = (p0 & 255) + (p1 & 255) + (p2 & 255) + (p3 & 255);

                TInt index = iQuantTable[((red >> 5) << 10) | ((green >> 5) << 5) | (blue >> 5)] & 255;
                if (index == 255) index = 254;
                destination[y * destinationWidth + x] = (TUint8)index;
                }
            }

        texMip[base + mipLevel] = destination;
        texMipLength[base + mipLevel] = destinationWidth * destinationHeight;
        }
    }

void CMap::LoadSkyL(RFs& aFileServer, const TDesC& aDirectory)
    {
    TPathBuffer leaf;
    leaf.Copy(_L("sky_"));
    AppendAscii(leaf, skyName.Des());
    leaf.Append(_L(".rgb565"));

    TPathBuffer path;
    MakePath(path, aDirectory, leaf);

    RFile file;
    TInt result = file.Open(aFileServer, path, EFileRead | EFileShareAny);
    if (result != KErrNone)
        {
        for (TInt face = 0; face < 5; face++)
            for (TInt i = 0; i < KSkyFacePixels; i++)
                sky565[face][i] = KFlatSkyColour;
        return;
        }

    TInt size = 0;
    result = file.Size(size);
    if (result != KErrNone || size < KSkyStripBytes)
        {
        file.Close();
        for (TInt face = 0; face < 5; face++)
            for (TInt i = 0; i < KSkyFacePixels; i++)
                sky565[face][i] = KFlatSkyColour;
        return;
        }

    TUint16* faces = &sky565[0][0];
    TUint8* raw = (TUint8*)faces;
    TPtr8 rawDescriptor(raw, KSkyStripBytes);
    rawDescriptor.SetLength(KSkyStripBytes);
    result = file.Read(rawDescriptor, KSkyStripBytes);
    file.Close();

    if (result != KErrNone || rawDescriptor.Length() < KSkyStripBytes)
        {
        for (TInt face = 0; face < 5; face++)
            for (TInt i = 0; i < KSkyFacePixels; i++)
                sky565[face][i] = KFlatSkyColour;
        return;
        }

    const TInt totalPixels = 5 * KSkyFacePixels;
    for (TInt i = 0; i < totalPixels; i++)
        faces[i] = (TUint16)(raw[i * 2] | ((TInt)raw[i * 2 + 1] << 8));
    }

void CMap::SetParent(TInt aChild, TInt aParent)
    {
    if (aChild >= 0)
        {
        if (aChild < numNodes) nParent[aChild] = aParent;
        }
    else
        {
        const TInt leaf = -aChild - 1;
        if (leaf >= 0 && leaf < numLeaves) lParent[leaf] = aParent;
        }
    }

void CMap::LoadL(RFs& aFileServer, const TDesC& aDirectory, const TDesC& aName)
    {
    TPathBuffer leaf;
    AppendLeaf(leaf, aName, _L(".cse"));
    TPathBuffer path;
    MakePath(path, aDirectory, leaf);

    HBufC8* file = ReadWholeFileL(aFileServer, path);
    CleanupStack::PushL(file);

    TBinaryReader in(file->Ptr(), file->Length());

    if (in.ReadInteger() != 0x43534533) User::Leave(KErrCorrupt);

    in.ReadBytes(iRawPalette, 768);
    for (TInt i = 0; i < 256; i++)
        pal[i] = ((TUint32)(iRawPalette[i * 3] & 255) << 16) |
                 ((TUint32)(iRawPalette[i * 3 + 1] & 255) << 8) |
                 (TUint32)(iRawPalette[i * 3 + 2] & 255);

    tintR = in.ReadReal();
    tintG = in.ReadReal();
    tintB = in.ReadReal();
    in.ReadBytes(iQuantTable, 32768);

    numTex = in.ReadInteger();
    if (numTex < 0) User::Leave(KErrCorrupt);

    texName = NewArrayL<TAsciiName>(numTex);
    texFlags = NewArrayL<TInt16>(numTex);
    texW = NewArrayL<TInt>(numTex);
    texH = NewArrayL<TInt>(numTex);
    texMip = NewArrayL<TUint8*>(numTex * 3);
    texMipLength = NewArrayL<TInt>(numTex * 3);
    texXScale = NewArrayL<TReal32>(numTex);
    texYScale = NewArrayL<TReal32>(numTex);
    TReal32* textureScaleX = NewArrayL<TReal32>(numTex);
    TReal32* textureScaleY = NewArrayL<TReal32>(numTex);

    for (TInt i = 0; i < numTex * 3; i++)
        {
        texMip[i] = NULL;
        texMipLength[i] = 0;
        }

    for (TInt i = 0; i < numTex; i++)
        {
        texName[i].Set(in.ReadString8());
        texFlags[i] = in.ReadSignedShort();
        texW[i] = in.ReadSignedShort();
        texH[i] = in.ReadSignedShort();
        textureScaleX[i] = in.ReadReal();
        textureScaleY[i] = in.ReadReal();
        texXScale[i] = textureScaleX[i];
        texYScale[i] = textureScaleY[i];

        TInt length = in.ReadInteger();
        if (length < 0) length = 0;
        texMip[i * 3] = NewArrayL<TUint8>(length);
        in.ReadBytes(texMip[i * 3], length);
        texMipLength[i * 3] = length;
        BuildMips(iRawPalette, i);
        }

    delete[] textureScaleX;
    delete[] textureScaleY;

    TInt count = in.ReadInteger();
    if (count < 0) count = 0;
    TReal32* textureVectors = NewArrayL<TReal32>(count * 8);
    TInt16* textureVectorIndex = NewArrayL<TInt16>(count);
    for (TInt i = 0; i < count; i++)
        {
        for (TInt k = 0; k < 8; k++) textureVectors[i * 8 + k] = in.ReadReal();
        textureVectorIndex[i] = in.ReadSignedShort();
        }

    count = in.ReadInteger();
    if (count < 0) count = 0;
    plNx = NewArrayL<TReal32>(count);
    plNy = NewArrayL<TReal32>(count);
    plNz = NewArrayL<TReal32>(count);
    plD = NewArrayL<TReal32>(count);
    plType = NewArrayL<TUint8>(count);
    for (TInt i = 0; i < count; i++)
        {
        plNx[i] = in.ReadReal();
        plNy[i] = in.ReadReal();
        plNz[i] = in.ReadReal();
        plD[i] = in.ReadReal();
        plType[i] = in.ReadByte();
        }

    numVerts = in.ReadInteger();
    if (numVerts < 0) numVerts = 0;
    vx = NewArrayL<TReal32>(numVerts);
    vy = NewArrayL<TReal32>(numVerts);
    vz = NewArrayL<TReal32>(numVerts);
    for (TInt i = 0; i < numVerts; i++)
        {
        vx[i] = in.ReadReal();
        vy[i] = in.ReadReal();
        vz[i] = in.ReadReal();
        }

    numFverts = in.ReadInteger();
    if (numFverts < 0) numFverts = 0;
    fvert = NewArrayL<TInt16>(numFverts);
    for (TInt i = 0; i < numFverts; i++) fvert[i] = in.ReadSignedShort();

    numFaces = in.ReadInteger();
    if (numFaces < 0) numFaces = 0;
    fPlane = NewArrayL<TInt16>(numFaces);
    fSide = NewArrayL<TUint8>(numFaces);
    fFirstV = NewArrayL<TInt>(numFaces);
    fNumV = NewArrayL<TInt16>(numFaces);
    fTex = NewArrayL<TInt16>(numFaces);
    fS = NewArrayL<TReal32>(numFaces * 4);
    fT = NewArrayL<TReal32>(numFaces * 4);
    fLS = NewArrayL<TReal32>(numFaces * 4);
    fLT = NewArrayL<TReal32>(numFaces * 4);
    fLmOfs = NewArrayL<TInt>(numFaces);
    fLmW = NewArrayL<TUint8>(numFaces);
    fLmH = NewArrayL<TUint8>(numFaces);

    for (TInt i = 0; i < numFaces; i++)
        {
        fPlane[i] = in.ReadSignedShort();
        fSide[i] = in.ReadByte();
        fFirstV[i] = in.ReadInteger();
        fNumV[i] = in.ReadSignedShort();
        fTex[i] = in.ReadSignedShort();
        TInt vector = in.ReadSignedShort();
        fLmOfs[i] = in.ReadInteger();
        fLmW[i] = in.ReadByte();
        fLmH[i] = in.ReadByte();
        const TInt mapS = in.ReadSignedShort();
        const TInt mapT = in.ReadSignedShort();
        const TInt texture = fTex[i];
        TReal32 scaleX = 1;
        TReal32 scaleY = 1;
        if (texture >= 0 && texture < numTex)
            {
            scaleX = texXScale[texture];
            scaleY = texYScale[texture];
            }
        if (vector < 0 || vector >= count) vector = 0;
        const TInt vectorBase = vector * 8;
        for (TInt k = 0; k < 4; k++)
            {
            fS[i * 4 + k] = textureVectors[vectorBase + k] * scaleX;
            fT[i * 4 + k] = textureVectors[vectorBase + 4 + k] * scaleY;
            fLS[i * 4 + k] = textureVectors[vectorBase + k] / 16.0f;
            fLT[i * 4 + k] = textureVectors[vectorBase + 4 + k] / 16.0f;
            }
        fLS[i * 4 + 3] = (textureVectors[vectorBase + 3] - mapS * 16) / 16.0f;
        fLT[i * 4 + 3] = (textureVectors[vectorBase + 7] - mapT * 16) / 16.0f;
        }

    delete[] textureVectors;
    delete[] textureVectorIndex;

    count = in.ReadInteger();
    if (count < 0) count = 0;
    numNodes = count;

    nPlane = NewArrayL<TInt>(numNodes);
    nChild0 = NewArrayL<TInt16>(numNodes);
    nChild1 = NewArrayL<TInt16>(numNodes);
    nBox = NewArrayL<TInt16>(numNodes * 6);
    nFirstFace = NewArrayL<TInt16>(numNodes);
    nNumFaces = NewArrayL<TInt16>(numNodes);
    nParent = NewArrayL<TInt>(numNodes);
    for (TInt i = 0; i < numNodes; i++)
        {
        nPlane[i] = in.ReadInteger();
        nChild0[i] = in.ReadSignedShort();
        nChild1[i] = in.ReadSignedShort();
        for (TInt k = 0; k < 6; k++) nBox[i * 6 + k] = in.ReadSignedShort();
        nFirstFace[i] = in.ReadSignedShort();
        nNumFaces[i] = in.ReadSignedShort();
        }

    numLeaves = in.ReadInteger();
    if (numLeaves < 0) numLeaves = 0;
    lContents = NewArrayL<TInt>(numLeaves);
    lVisOfs = NewArrayL<TInt>(numLeaves);
    lBox = NewArrayL<TInt16>(numLeaves * 6);
    lFirstMark = NewArrayL<TInt16>(numLeaves);
    lNumMarks = NewArrayL<TInt16>(numLeaves);
    lParent = NewArrayL<TInt>(numLeaves);
    for (TInt i = 0; i < numLeaves; i++)
        {
        lContents[i] = in.ReadInteger();
        lVisOfs[i] = in.ReadInteger();
        for (TInt k = 0; k < 6; k++) lBox[i * 6 + k] = in.ReadSignedShort();
        lFirstMark[i] = in.ReadSignedShort();
        lNumMarks[i] = in.ReadSignedShort();
        }

    count = in.ReadInteger();
    if (count < 0) count = 0;
    marks = NewArrayL<TInt16>(count);
    for (TInt i = 0; i < count; i++) marks[i] = in.ReadSignedShort();

    visLength = in.ReadInteger();
    if (visLength < 0) visLength = 0;
    vis = NewArrayL<TUint8>(visLength);
    in.ReadBytes(vis, visLength);

    numClipNodes = in.ReadInteger();
    if (numClipNodes < 0) numClipNodes = 0;
    cPlane = NewArrayL<TInt>(numClipNodes);
    cChild0 = NewArrayL<TInt16>(numClipNodes);
    cChild1 = NewArrayL<TInt16>(numClipNodes);
    for (TInt i = 0; i < numClipNodes; i++)
        {
        cPlane[i] = in.ReadInteger();
        cChild0[i] = in.ReadSignedShort();
        cChild1[i] = in.ReadSignedShort();
        }

    numModels = in.ReadInteger();
    if (numModels < 0) numModels = 0;
    mMins = NewArrayL<TReal32>(numModels * 3);
    mMaxs = NewArrayL<TReal32>(numModels * 3);
    mOrigin = NewArrayL<TReal32>(numModels * 3);
    mHead = NewArrayL<TInt>(numModels * 4);
    mFirstFace = NewArrayL<TInt>(numModels);
    mNumFaces = NewArrayL<TInt>(numModels);
    for (TInt i = 0; i < numModels; i++)
        {
        for (TInt k = 0; k < 3; k++) mMins[i * 3 + k] = in.ReadReal();
        for (TInt k = 0; k < 3; k++) mMaxs[i * 3 + k] = in.ReadReal();
        for (TInt k = 0; k < 3; k++) mOrigin[i * 3 + k] = in.ReadReal();
        for (TInt k = 0; k < 4; k++) mHead[i * 4 + k] = in.ReadInteger();
        in.ReadInteger();
        mFirstFace[i] = in.ReadInteger();
        mNumFaces[i] = in.ReadInteger();
        }

    numEntities = in.ReadInteger();
    if (numEntities < 0) numEntities = 0;
    entities = NewArrayL<SEntity>(numEntities);
    for (TInt i = 0; i < numEntities; i++)
        {
        entities[i].iCount = in.ReadSignedShort();
        if (entities[i].iCount > KMaxEntityKeys) entities[i].iCount = KMaxEntityKeys;
        if (entities[i].iCount < 0) entities[i].iCount = 0;
        for (TInt j = 0; j < entities[i].iCount; j++)
            {
            entities[i].iKeys[j].Set(in.ReadString8());
            entities[i].iValues[j].Set(in.ReadString8());
            }
        }

    lightLength = in.ReadInteger();
    if (lightLength < 0) lightLength = 0;
    light = NewArrayL<TUint8>(lightLength);
    in.ReadBytes(light, lightLength);

    skyName.Set(in.ReadString8());
    LoadSkyL(aFileServer, aDirectory);

    for (TInt i = 0; i < numNodes; i++)
        {
        SetParent(nChild0[i], i);
        SetParent(nChild1[i], i);
        }
    if (numNodes > 0) nParent[0] = -1;

    h0Plane = nPlane;
    h0Child0 = NewArrayL<TInt>(numNodes);
    h0Child1 = NewArrayL<TInt>(numNodes);
    for (TInt i = 0; i < numNodes; i++)
        {
        TInt child = nChild0[i];
        h0Child0[i] = child >= 0 ? child : lContents[-child - 1];
        child = nChild1[i];
        h0Child1[i] = child >= 0 ? child : lContents[-child - 1];
        }

    CleanupStack::PopAndDestroy(file);
    }

TInt CMap::PointLeaf(TReal32 aX, TReal32 aY, TReal32 aZ) const
    {
    TInt node = 0;
    while (node >= 0)
        {
        const TInt plane = nPlane[node];
        const TReal32 distance = plNx[plane] * aX + plNy[plane] * aY + plNz[plane] * aZ - plD[plane];
        node = distance >= 0 ? nChild0[node] : nChild1[node];
        }
    return -node - 1;
    }

TInt CMap::HullPointContents(TInt aHull, TInt aNode, TReal32 aX, TReal32 aY, TReal32 aZ) const
    {
    if (aHull == 0)
        {
        while (aNode >= 0)
            {
            const TInt plane = h0Plane[aNode];
            const TInt type = plType[plane];
            TReal32 distance;
            if (type < 3)
                distance = (type == 0 ? aX : (type == 1 ? aY : aZ)) - plD[plane];
            else
                distance = plNx[plane] * aX + plNy[plane] * aY + plNz[plane] * aZ - plD[plane];
            aNode = distance < 0 ? h0Child1[aNode] : h0Child0[aNode];
            }
        }
    else
        {
        while (aNode >= 0)
            {
            const TInt plane = cPlane[aNode];
            const TInt type = plType[plane];
            TReal32 distance;
            if (type < 3)
                distance = (type == 0 ? aX : (type == 1 ? aY : aZ)) - plD[plane];
            else
                distance = plNx[plane] * aX + plNy[plane] * aY + plNz[plane] * aZ - plD[plane];
            aNode = distance < 0 ? cChild1[aNode] : cChild0[aNode];
            }
        }
    return aNode;
    }

TBool CMap::RecursiveHullCheck(TInt aNode, TReal32 aP1f, TReal32 aP2f,
                               TReal32 aP1x, TReal32 aP1y, TReal32 aP1z,
                               TReal32 aP2x, TReal32 aP2y, TReal32 aP2z)
    {
    if (aNode < 0)
        {
        if (aNode != KContentsSolid)
            {
            tAllSolid = EFalse;
            if (aNode == KContentsEmpty) tInOpen = ETrue;
            else tInWater = ETrue;
            }
        else
            {
            tStartSolid = ETrue;
            }
        return ETrue;
        }

    TInt plane;
    TInt childFront;
    TInt childBack;
    if (iTraceHull == 0)
        {
        plane = h0Plane[aNode];
        childFront = h0Child0[aNode];
        childBack = h0Child1[aNode];
        }
    else
        {
        plane = cPlane[aNode];
        childFront = cChild0[aNode];
        childBack = cChild1[aNode];
        }

    TReal32 distanceStart;
    TReal32 distanceEnd;
    const TInt type = plType[plane];
    if (type < 3)
        {
        if (type == 0)
            {
            distanceStart = aP1x - plD[plane];
            distanceEnd = aP2x - plD[plane];
            }
        else if (type == 1)
            {
            distanceStart = aP1y - plD[plane];
            distanceEnd = aP2y - plD[plane];
            }
        else
            {
            distanceStart = aP1z - plD[plane];
            distanceEnd = aP2z - plD[plane];
            }
        }
    else
        {
        distanceStart = plNx[plane] * aP1x + plNy[plane] * aP1y + plNz[plane] * aP1z - plD[plane];
        distanceEnd = plNx[plane] * aP2x + plNy[plane] * aP2y + plNz[plane] * aP2z - plD[plane];
        }

    if (distanceStart >= 0 && distanceEnd >= 0)
        return RecursiveHullCheck(childFront, aP1f, aP2f, aP1x, aP1y, aP1z, aP2x, aP2y, aP2z);
    if (distanceStart < 0 && distanceEnd < 0)
        return RecursiveHullCheck(childBack, aP1f, aP2f, aP1x, aP1y, aP1z, aP2x, aP2y, aP2z);

    TReal32 fraction = distanceStart < 0
                       ? (distanceStart + KDistanceEpsilon) / (distanceStart - distanceEnd)
                       : (distanceStart - KDistanceEpsilon) / (distanceStart - distanceEnd);
    if (fraction < 0) fraction = 0;
    if (fraction > 1) fraction = 1;

    TReal32 middleFraction = aP1f + (aP2f - aP1f) * fraction;
    TReal32 middleX = aP1x + fraction * (aP2x - aP1x);
    TReal32 middleY = aP1y + fraction * (aP2y - aP1y);
    TReal32 middleZ = aP1z + fraction * (aP2z - aP1z);

    const TInt side = distanceStart < 0 ? 1 : 0;
    const TInt sideChild = side == 0 ? childFront : childBack;
    const TInt otherChild = side == 0 ? childBack : childFront;

    if (!RecursiveHullCheck(sideChild, aP1f, middleFraction, aP1x, aP1y, aP1z,
                            middleX, middleY, middleZ))
        return EFalse;

    if (HullPointContents(iTraceHull, otherChild, middleX, middleY, middleZ) != KContentsSolid)
        return RecursiveHullCheck(otherChild, middleFraction, aP2f, middleX, middleY, middleZ,
                                  aP2x, aP2y, aP2z);

    if (tAllSolid) return EFalse;

    if (side == 0)
        {
        tNx = plNx[plane];
        tNy = plNy[plane];
        tNz = plNz[plane];
        tPd = plD[plane];
        }
    else
        {
        tNx = -plNx[plane];
        tNy = -plNy[plane];
        tNz = -plNz[plane];
        tPd = -plD[plane];
        }

    while (HullPointContents(iTraceHull, iTraceHead, middleX, middleY, middleZ) == KContentsSolid)
        {
        fraction -= 0.1f;
        if (fraction < 0)
            {
            tFrac = middleFraction;
            tEx = middleX;
            tEy = middleY;
            tEz = middleZ;
            return EFalse;
            }
        middleFraction = aP1f + (aP2f - aP1f) * fraction;
        middleX = aP1x + fraction * (aP2x - aP1x);
        middleY = aP1y + fraction * (aP2y - aP1y);
        middleZ = aP1z + fraction * (aP2z - aP1z);
        }

    tFrac = middleFraction;
    tEx = middleX;
    tEy = middleY;
    tEz = middleZ;
    return EFalse;
    }

void CMap::TraceModel(TInt aModel, TInt aHull, TReal32 aX1, TReal32 aY1, TReal32 aZ1,
                      TReal32 aX2, TReal32 aY2, TReal32 aZ2)
    {
    tAllSolid = ETrue;
    tStartSolid = EFalse;
    tInOpen = EFalse;
    tInWater = EFalse;
    tFrac = 1;
    tEx = aX2;
    tEy = aY2;
    tEz = aZ2;
    tNx = 0;
    tNy = 0;
    tNz = 0;
    tPd = 0;

    iTraceHull = aHull;
    iTraceHead = mHead[aModel * 4 + aHull];
    RecursiveHullCheck(iTraceHead, 0, 1, aX1, aY1, aZ1, aX2, aY2, aZ2);

    if (tAllSolid) tStartSolid = ETrue;
    if (tStartSolid) tFrac = 0;
    if (tFrac == 1)
        {
        tEx = aX2;
        tEy = aY2;
        tEz = aZ2;
        }
    }

void CMap::Trace(TInt aHull, TReal32 aX1, TReal32 aY1, TReal32 aZ1,
                 TReal32 aX2, TReal32 aY2, TReal32 aZ2)
    {
    TraceModel(0, aHull, aX1, aY1, aZ1, aX2, aY2, aZ2);
    rFrac = tFrac;
    rEx = tEx;
    rEy = tEy;
    rEz = tEz;
    rNx = tNx;
    rNy = tNy;
    rNz = tNz;
    rStartSolid = tStartSolid;
    rAllSolid = tAllSolid;
    rModel = 0;

    for (TInt i = 0; i < numSolidModels; i++)
        {
        const TInt model = solidModels[i];
        if (!modelActive[model]) continue;

        const TReal32 expansion = aHull == 0 ? 0 : 40;
        const TReal32 minimumX = Smaller(aX1, aX2) - expansion;
        const TReal32 maximumX = Larger(aX1, aX2) + expansion;
        const TReal32 minimumY = Smaller(aY1, aY2) - expansion;
        const TReal32 maximumY = Larger(aY1, aY2) + expansion;
        const TReal32 minimumZ = Smaller(aZ1, aZ2) - expansion;
        const TReal32 maximumZ = Larger(aZ1, aZ2) + expansion;

        if (maximumX < mMins[model * 3] || minimumX > mMaxs[model * 3] ||
            maximumY < mMins[model * 3 + 1] || minimumY > mMaxs[model * 3 + 1] ||
            maximumZ < mMins[model * 3 + 2] || minimumZ > mMaxs[model * 3 + 2])
            continue;

        TraceModel(model, aHull, aX1, aY1, aZ1, aX2, aY2, aZ2);
        if (tStartSolid) rStartSolid = ETrue;
        if (tFrac < rFrac)
            {
            rFrac = tFrac;
            rEx = tEx;
            rEy = tEy;
            rEz = tEz;
            rNx = tNx;
            rNy = tNy;
            rNz = tNz;
            rAllSolid = tAllSolid;
            rModel = model;
            }
        }
    }

TBool CMap::InFace(TInt aFace, TReal32 aX, TReal32 aY, TReal32 aZ) const
    {
    const TInt plane = fPlane[aFace] & 0xFFFF;
    const TReal32 normalX = plNx[plane];
    const TReal32 normalY = plNy[plane];
    const TReal32 normalZ = plNz[plane];
    const TInt vertexCount = fNumV[aFace];
    const TInt firstVertex = fFirstV[aFace];
    TReal32 sign = 0;

    for (TInt i = 0; i < vertexCount; i++)
        {
        const TInt first = fvert[firstVertex + i] & 0xFFFF;
        const TInt second = fvert[firstVertex + (i + 1) % vertexCount] & 0xFFFF;
        const TReal32 edgeX = vx[second] - vx[first];
        const TReal32 edgeY = vy[second] - vy[first];
        const TReal32 edgeZ = vz[second] - vz[first];
        const TReal32 pointX = aX - vx[first];
        const TReal32 pointY = aY - vy[first];
        const TReal32 pointZ = aZ - vz[first];
        const TReal32 crossX = edgeY * pointZ - edgeZ * pointY;
        const TReal32 crossY = edgeZ * pointX - edgeX * pointZ;
        const TReal32 crossZ = edgeX * pointY - edgeY * pointX;
        const TReal32 distance = crossX * normalX + crossY * normalY + crossZ * normalZ;
        if (distance > 0.5f)
            {
            if (sign < 0) return EFalse;
            sign = 1;
            }
        else if (distance < -0.5f)
            {
            if (sign > 0) return EFalse;
            sign = -1;
            }
        }
    return ETrue;
    }

TBool CMap::SurfaceRecursive(TInt aNode, TReal32 aX1, TReal32 aY1, TReal32 aZ1,
                             TReal32 aX2, TReal32 aY2, TReal32 aZ2)
    {
    if (aNode < 0) return EFalse;

    const TInt plane = nPlane[aNode];
    const TReal32 distanceStart = plNx[plane] * aX1 + plNy[plane] * aY1 + plNz[plane] * aZ1 - plD[plane];
    const TReal32 distanceEnd = plNx[plane] * aX2 + plNy[plane] * aY2 + plNz[plane] * aZ2 - plD[plane];
    const TInt side = distanceStart < 0 ? 1 : 0;
    const TInt frontChild = side == 0 ? nChild0[aNode] : nChild1[aNode];
    const TInt backChild = side == 0 ? nChild1[aNode] : nChild0[aNode];

    if ((distanceEnd < 0 ? 1 : 0) == side)
        return SurfaceRecursive(frontChild, aX1, aY1, aZ1, aX2, aY2, aZ2);

    const TReal32 fraction = distanceStart / (distanceStart - distanceEnd);
    const TReal32 middleX = aX1 + (aX2 - aX1) * fraction;
    const TReal32 middleY = aY1 + (aY2 - aY1) * fraction;
    const TReal32 middleZ = aZ1 + (aZ2 - aZ1) * fraction;

    if (SurfaceRecursive(frontChild, aX1, aY1, aZ1, middleX, middleY, middleZ)) return ETrue;

    const TInt firstFace = nFirstFace[aNode] & 0xFFFF;
    const TInt faceCount = nNumFaces[aNode] & 0xFFFF;
    for (TInt i = firstFace; i < firstFace + faceCount; i++)
        {
        if (fTex[i] >= 0 && InFace(i, middleX, middleY, middleZ))
            {
            iSurfaceTex = fTex[i];
            return ETrue;
            }
        }
    return SurfaceRecursive(backChild, middleX, middleY, middleZ, aX2, aY2, aZ2);
    }

TInt CMap::SurfaceTex(TInt aModel, TReal32 aX1, TReal32 aY1, TReal32 aZ1,
                      TReal32 aX2, TReal32 aY2, TReal32 aZ2)
    {
    if (aModel > 0)
        {
        const TInt firstFace = mFirstFace[aModel];
        const TInt faceCount = mNumFaces[aModel];
        TReal32 best = 1e9f;
        TInt bestTexture = -1;
        for (TInt i = firstFace; i < firstFace + faceCount; i++)
            {
            const TInt plane = fPlane[i] & 0xFFFF;
            const TReal32 distanceStart = plNx[plane] * aX1 + plNy[plane] * aY1 + plNz[plane] * aZ1 - plD[plane];
            const TReal32 distanceEnd = plNx[plane] * aX2 + plNy[plane] * aY2 + plNz[plane] * aZ2 - plD[plane];
            if ((distanceStart >= 0) == (distanceEnd >= 0)) continue;
            const TReal32 fraction = distanceStart / (distanceStart - distanceEnd);
            const TReal32 hitX = aX1 + (aX2 - aX1) * fraction;
            const TReal32 hitY = aY1 + (aY2 - aY1) * fraction;
            const TReal32 hitZ = aZ1 + (aZ2 - aZ1) * fraction;
            if (fraction < best && InFace(i, hitX, hitY, hitZ))
                {
                best = fraction;
                bestTexture = fTex[i];
                }
            }
        return bestTexture;
        }

    iSurfaceTex = -1;
    SurfaceRecursive(0, aX1, aY1, aZ1, aX2, aY2, aZ2);
    return iSurfaceTex;
    }

TBool CMap::LightRecursive(TInt aNode, TReal32 aX1, TReal32 aY1, TReal32 aZ1,
                           TReal32 aX2, TReal32 aY2, TReal32 aZ2)
    {
    if (aNode < 0) return EFalse;

    const TInt plane = nPlane[aNode];
    const TReal32 distanceStart = plNx[plane] * aX1 + plNy[plane] * aY1 + plNz[plane] * aZ1 - plD[plane];
    const TReal32 distanceEnd = plNx[plane] * aX2 + plNy[plane] * aY2 + plNz[plane] * aZ2 - plD[plane];
    const TInt side = distanceStart < 0 ? 1 : 0;
    const TInt frontChild = side == 0 ? nChild0[aNode] : nChild1[aNode];
    const TInt backChild = side == 0 ? nChild1[aNode] : nChild0[aNode];

    if ((distanceEnd < 0 ? 1 : 0) == side)
        return LightRecursive(frontChild, aX1, aY1, aZ1, aX2, aY2, aZ2);

    const TReal32 fraction = distanceStart / (distanceStart - distanceEnd);
    const TReal32 middleX = aX1 + (aX2 - aX1) * fraction;
    const TReal32 middleY = aY1 + (aY2 - aY1) * fraction;
    const TReal32 middleZ = aZ1 + (aZ2 - aZ1) * fraction;

    if (LightRecursive(frontChild, aX1, aY1, aZ1, middleX, middleY, middleZ)) return ETrue;

    const TInt firstFace = nFirstFace[aNode] & 0xFFFF;
    const TInt faceCount = nNumFaces[aNode] & 0xFFFF;
    for (TInt i = firstFace; i < firstFace + faceCount; i++)
        {
        const TInt lightOffset = fLmOfs[i];
        if (lightOffset < 0) continue;
        const TReal32 lightS = fLS[i * 4] * middleX + fLS[i * 4 + 1] * middleY +
                               fLS[i * 4 + 2] * middleZ + fLS[i * 4 + 3];
        const TReal32 lightT = fLT[i * 4] * middleX + fLT[i * 4 + 1] * middleY +
                               fLT[i * 4 + 2] * middleZ + fLT[i * 4 + 3];
        const TInt width = fLmW[i] & 255;
        const TInt height = fLmH[i] & 255;
        if (lightS < -0.01f || lightT < -0.01f || lightS > width - 0.99f || lightT > height - 0.99f)
            continue;
        TInt sampleS = (TInt)(lightS + 0.5f);
        TInt sampleT = (TInt)(lightT + 0.5f);
        if (sampleS >= width) sampleS = width - 1;
        if (sampleT >= height) sampleT = height - 1;
        if (sampleS < 0) sampleS = 0;
        if (sampleT < 0) sampleT = 0;
        iLightBest = light[lightOffset + sampleT * width + sampleS] & 255;
        return ETrue;
        }
    return LightRecursive(backChild, middleX, middleY, middleZ, aX2, aY2, aZ2);
    }

TInt CMap::LightAt(TReal32 aX, TReal32 aY, TReal32 aZ)
    {
    iLightBest = -1;
    LightRecursive(0, aX, aY, aZ, aX, aY, aZ - 2048);
    return iLightBest < 0 ? 32 : iLightBest;
    }

const TAsciiName* CMap::EntityValue(TInt aEntity, const TPtrC8& aKey) const
    {
    if (aEntity < 0 || aEntity >= numEntities) return NULL;
    const SEntity& entity = entities[aEntity];
    for (TInt i = 0; i < entity.iCount; i++)
        if (entity.iKeys[i].Equals(aKey)) return &entity.iValues[i];
    return NULL;
    }

TInt CMap::FindEntity(const TPtrC8& aClassname) const
    {
    const TPtrC8 key((const TUint8*)"classname", 9);
    for (TInt i = 0; i < numEntities; i++)
        {
        const TAsciiName* value = EntityValue(i, key);
        if (value && value->Equals(aClassname)) return i;
        }
    return -1;
    }

/* ------------------------------------------------------------ meshes, models */

TModelMesh::TModelMesh()
    : iVertexCount(0), iNormalCount(0), iVertexBone(NULL), iVertex(NULL),
      iNormalBone(NULL), iNormal(NULL), iMaterialCount(0), iMaterial(NULL),
      iTriangles(NULL), iTriangleLength(NULL)
    {
    }

TModelMesh::~TModelMesh()
    {
    if (iTriangles)
        {
        for (TInt i = 0; i < iMaterialCount; i++) delete[] iTriangles[i];
        }
    delete[] iTriangles;
    delete[] iTriangleLength;
    delete[] iMaterial;
    delete[] iVertex;
    delete[] iVertexBone;
    delete[] iNormal;
    delete[] iNormalBone;
    }

TUint32 CModel::iModelPalette[256] = {0};
TUint8* CModel::iModelTexture[KModelTextures] = {NULL};
TInt CModel::iModelTextureWidth[KModelTextures] = {0};
TInt CModel::iModelTextureHeight[KModelTextures] = {0};
TInt CModel::iModelTextureCount = 0;

CModel::CModel()
    : iBoneCount(0), iParent(NULL), iBoneName(NULL), iDefaultPosition(NULL),
      iDefaultRotation(NULL), iPartCount(0), iPartName(NULL), iParts(NULL)
    {
    }

CModel::~CModel()
    {
    if (iParts)
        {
        for (TInt part = 0; part < iPartCount; part++)
            {
            if (!iParts[part]) continue;
            const TInt subModelCount = SubMeshCount(iParts[part]);
            for (TInt sub = 0; sub < subModelCount; sub++) delete iParts[part][sub];
            UnregisterSubMeshes(iParts[part]);
            delete[] iParts[part];
            }
        }
    delete[] iParts;
    delete[] iPartName;
    delete[] iDefaultRotation;
    delete[] iDefaultPosition;
    delete[] iBoneName;
    delete[] iParent;
    }

TInt CModel::LoadModelTexturesL(RFs& aFileServer, const TDesC& aDirectory)
    {
    if (CModel::iModelTextureCount > 0) return KErrNone;

    TPathBuffer leaf;
    leaf.Copy(_L("mdltex.bin"));
    TPathBuffer path;
    MakePath(path, aDirectory, leaf);

    HBufC8* file = ReadWholeFileL(aFileServer, path);
    CleanupStack::PushL(file);
    TBinaryReader in(file->Ptr(), file->Length());

    if (in.ReadInteger() != 0x43535431) User::Leave(KErrCorrupt);

    TInt count = in.ReadSignedShort();
    if (count < 0) count = 0;
    if (count > KModelTextures) count = KModelTextures;

    TUint8 rawPalette[768];
    in.ReadBytes(rawPalette, 768);
    for (TInt i = 0; i < 256; i++)
        iModelPalette[i] = ((TUint32)(rawPalette[i * 3] & 255) << 16) |
                           ((TUint32)(rawPalette[i * 3 + 1] & 255) << 8) |
                           (TUint32)(rawPalette[i * 3 + 2] & 255);

    for (TInt i = 0; i < count; i++)
        {
        TInt width = in.ReadSignedShort();
        TInt height = in.ReadSignedShort();
        if (width < 0) width = 0;
        if (height < 0) height = 0;
        const TInt size = width * height;
        TUint8* pixels = NewArrayL<TUint8>(size);
        in.ReadBytes(pixels, size);
        iModelTexture[i] = pixels;
        iModelTextureWidth[i] = width;
        iModelTextureHeight[i] = height;
        }
    iModelTextureCount = count;

    CleanupStack::PopAndDestroy(file);
    return KErrNone;
    }

CModel* CModel::LoadL(RFs& aFileServer, const TDesC& aDirectory, const TDesC& aName)
    {
    TPathBuffer leaf;
    AppendLeaf(leaf, aName, _L(".m3"));
    TPathBuffer path;
    MakePath(path, aDirectory, leaf);

    HBufC8* file = ReadFileOrNullL(aFileServer, path);
    if (!file) return NULL;
    CleanupStack::PushL(file);
    TBinaryReader in(file->Ptr(), file->Length());

    if (in.ReadInteger() != 0x43534D32)
        {
        CleanupStack::PopAndDestroy(file);
        return NULL;
        }

    CModel* model = new CModel;
    if (!model)
        {
        CleanupStack::PopAndDestroy(file);
        return NULL;
        }
    CleanupStack::PushL(TCleanupItem(DeleteModelObject, model));

    model->iBoneCount = in.ReadSignedShort();
    if (model->iBoneCount < 0) model->iBoneCount = 0;
    model->iParent = NewArrayL<TUint8>(model->iBoneCount);
    model->iBoneName = NewArrayL<TAsciiName>(model->iBoneCount);
    model->iDefaultPosition = NewArrayL<TReal32>(model->iBoneCount * 3);
    model->iDefaultRotation = NewArrayL<TReal32>(model->iBoneCount * 4);

    for (TInt bone = 0; bone < model->iBoneCount; bone++)
        {
        model->iParent[bone] = in.ReadByte();
        model->iBoneName[bone].Set(in.ReadString8());
        for (TInt k = 0; k < 3; k++) model->iDefaultPosition[bone * 3 + k] = in.ReadReal();
        for (TInt k = 0; k < 4; k++) model->iDefaultRotation[bone * 4 + k] = in.ReadReal();
        }

    model->iPartCount = in.ReadSignedShort();
    if (model->iPartCount < 0) model->iPartCount = 0;
    model->iPartName = NewArrayL<TAsciiName>(model->iPartCount);
    model->iParts = NewArrayL<TModelMesh**>(model->iPartCount);
    for (TInt part = 0; part < model->iPartCount; part++) model->iParts[part] = NULL;

    for (TInt part = 0; part < model->iPartCount; part++)
        {
        model->iPartName[part].Set(in.ReadString8());
        const TInt subModelCount = in.ReadByte();
        model->iParts[part] = NewArrayL<TModelMesh*>(subModelCount);
        for (TInt sub = 0; sub < subModelCount; sub++) model->iParts[part][sub] = NULL;
        RegisterSubMeshes(model->iParts[part], subModelCount);

        for (TInt sub = 0; sub < subModelCount; sub++)
            {
            TModelMesh* mesh = new TModelMesh;
            if (!mesh) User::Leave(KErrNoMemory);
            model->iParts[part][sub] = mesh;

            mesh->iVertexCount = in.ReadSignedShort();
            if (mesh->iVertexCount < 0) mesh->iVertexCount = 0;
            mesh->iVertexBone = NewArrayL<TUint8>(mesh->iVertexCount);
            mesh->iVertex = NewArrayL<TReal32>(mesh->iVertexCount * 3);
            for (TInt i = 0; i < mesh->iVertexCount; i++)
                {
                mesh->iVertexBone[i] = in.ReadByte();
                for (TInt k = 0; k < 3; k++)
                    mesh->iVertex[i * 3 + k] = (TReal32)in.ReadSignedShort() / 16.0f;
                }

            mesh->iNormalCount = in.ReadSignedShort();
            if (mesh->iNormalCount < 0) mesh->iNormalCount = 0;
            mesh->iNormalBone = NewArrayL<TUint8>(mesh->iNormalCount);
            mesh->iNormal = NewArrayL<TReal32>(mesh->iNormalCount * 3);
            for (TInt i = 0; i < mesh->iNormalCount; i++)
                {
                mesh->iNormalBone[i] = in.ReadByte();
                for (TInt k = 0; k < 3; k++)
                    mesh->iNormal[i * 3 + k] = (TReal32)(TInt8)in.ReadByte() / 127.0f;
                }

            mesh->iMaterialCount = in.ReadSignedShort();
            if (mesh->iMaterialCount < 0) mesh->iMaterialCount = 0;
            mesh->iMaterial = NewArrayL<TInt16>(mesh->iMaterialCount);
            mesh->iTriangles = NewArrayL<TInt16*>(mesh->iMaterialCount);
            mesh->iTriangleLength = NewArrayL<TInt>(mesh->iMaterialCount);
            for (TInt material = 0; material < mesh->iMaterialCount; material++)
                {
                mesh->iTriangles[material] = NULL;
                mesh->iMaterial[material] = in.ReadSignedShort();
                const TInt valueCount = in.ReadSignedShort() * 12;
                mesh->iTriangleLength[material] = valueCount;
                mesh->iTriangles[material] = NewArrayL<TInt16>(valueCount);
                for (TInt value = 0; value < valueCount; value++)
                    mesh->iTriangles[material][value] = in.ReadSignedShort();
                }
            }
        }

    CleanupStack::Pop(model);
    CleanupStack::PopAndDestroy(file);
    return model;
    }

TInt CModel::Bone(const TPtrC8& aName) const
    {
    for (TInt i = 0; i < iBoneCount; i++)
        if (iBoneName[i].Equals(aName)) return i;
    return -1;
    }

TInt CModel::Bone(const char* aName) const
    {
    for (TInt i = 0; i < iBoneCount; i++)
        if (iBoneName[i].Equals(aName)) return i;
    return -1;
    }

void CModel::Qmat(TReal32 aX, TReal32 aY, TReal32 aZ, TReal32 aW,
                  TReal32 aPx, TReal32 aPy, TReal32 aPz, TReal32* aOut, TInt aOffset)
    {
    aOut[aOffset] = 1 - 2 * aY * aY - 2 * aZ * aZ;
    aOut[aOffset + 1] = 2 * aX * aY - 2 * aW * aZ;
    aOut[aOffset + 2] = 2 * aX * aZ + 2 * aW * aY;
    aOut[aOffset + 3] = aPx;
    aOut[aOffset + 4] = 2 * aX * aY + 2 * aW * aZ;
    aOut[aOffset + 5] = 1 - 2 * aX * aX - 2 * aZ * aZ;
    aOut[aOffset + 6] = 2 * aY * aZ - 2 * aW * aX;
    aOut[aOffset + 7] = aPy;
    aOut[aOffset + 8] = 2 * aX * aZ - 2 * aW * aY;
    aOut[aOffset + 9] = 2 * aY * aZ + 2 * aW * aX;
    aOut[aOffset + 10] = 1 - 2 * aX * aX - 2 * aY * aY;
    aOut[aOffset + 11] = aPz;
    }

void CModel::Mul(const TReal32* aA, TInt aAOffset, const TReal32* aB, TInt aBOffset,
                 TReal32* aC, TInt aCOffset)
    {
    for (TInt row = 0; row < 3; row++)
        {
        const TReal32 a0 = aA[aAOffset + row * 4];
        const TReal32 a1 = aA[aAOffset + row * 4 + 1];
        const TReal32 a2 = aA[aAOffset + row * 4 + 2];
        const TReal32 a3 = aA[aAOffset + row * 4 + 3];
        aC[aCOffset + row * 4] = a0 * aB[aBOffset] + a1 * aB[aBOffset + 4] + a2 * aB[aBOffset + 8];
        aC[aCOffset + row * 4 + 1] = a0 * aB[aBOffset + 1] + a1 * aB[aBOffset + 5] + a2 * aB[aBOffset + 9];
        aC[aCOffset + row * 4 + 2] = a0 * aB[aBOffset + 2] + a1 * aB[aBOffset + 6] + a2 * aB[aBOffset + 10];
        aC[aCOffset + row * 4 + 3] = a0 * aB[aBOffset + 3] + a1 * aB[aBOffset + 7] +
                                     a2 * aB[aBOffset + 11] + a3;
        }
    }

void CModel::EntityMatrix(TReal32 aX, TReal32 aY, TReal32 aZ, TReal32 aYawDeg, TReal32* aOut)
    {
    const TReal32 cosine = FloatCos(aYawDeg);
    const TReal32 sine = FloatSin(aYawDeg);
    aOut[0] = cosine;
    aOut[1] = -sine;
    aOut[2] = 0;
    aOut[3] = aX;
    aOut[4] = sine;
    aOut[5] = cosine;
    aOut[6] = 0;
    aOut[7] = aY;
    aOut[8] = 0;
    aOut[9] = 0;
    aOut[10] = 1;
    aOut[11] = aZ;
    }

void CModel::Build(TReal32* aQ, TReal32* aP, const TReal32* aEntity, TReal32* aOut) const
    {
    TReal32 local[12];
    for (TInt bone = 0; bone < iBoneCount; bone++)
        {
        Qmat(aQ[bone * 4], aQ[bone * 4 + 1], aQ[bone * 4 + 2], aQ[bone * 4 + 3],
             aP[bone * 3], aP[bone * 3 + 1], aP[bone * 3 + 2], local, 0);
        const TInt parent = (TInt8)iParent[bone];
        if (parent < 0) Mul(aEntity, 0, local, 0, aOut, bone * 12);
        else Mul(aOut, parent * 12, local, 0, aOut, bone * 12);
        }
    }

void CModel::BuildDefault(const TReal32* aEntity, TReal32* aOut) const
    {
    Build(iDefaultRotation, iDefaultPosition, aEntity, aOut);
    }

TInt* CModel::MergeMapL(const CModel& aParent) const
    {
    TInt* mapping = NewArrayL<TInt>(iBoneCount);
    for (TInt bone = 0; bone < iBoneCount; bone++)
        mapping[bone] = aParent.Bone(iBoneName[bone].Des());
    return mapping;
    }

void CModel::BuildMerged(const TInt* aMap, const TReal32* aParentWorld,
                         const TReal32* aEntity, TReal32* aOut) const
    {
    TReal32 local[12];
    for (TInt bone = 0; bone < iBoneCount; bone++)
        {
        if (aMap[bone] >= 0)
            {
            for (TInt k = 0; k < 12; k++) aOut[bone * 12 + k] = aParentWorld[aMap[bone] * 12 + k];
            continue;
            }
        Qmat(iDefaultRotation[bone * 4], iDefaultRotation[bone * 4 + 1],
             iDefaultRotation[bone * 4 + 2], iDefaultRotation[bone * 4 + 3],
             iDefaultPosition[bone * 3], iDefaultPosition[bone * 3 + 1],
             iDefaultPosition[bone * 3 + 2], local, 0);
        const TInt parent = (TInt8)iParent[bone];
        if (parent < 0) Mul(aEntity, 0, local, 0, aOut, bone * 12);
        else Mul(aOut, parent * 12, local, 0, aOut, bone * 12);
        }
    }

/* ---------------------------------------------------------------- animations */

static TReal32 QuaternionComponentW(TReal32 aX, TReal32 aY, TReal32 aZ)
    {
    const TReal32 value = 1 - aX * aX - aY * aY - aZ * aZ;
    return value > 0 ? FastSqrt(value) : 0;
    }

CAnimation::CAnimation()
    : iBoneCount(0), iSequenceCount(0), iName(NULL), iFps(NULL), iFrames(NULL),
      iBlends(NULL), iBones(NULL), iPositionAnimated(NULL), iRotationAnimated(NULL),
      iConstantRotation(NULL), iConstantPosition(NULL), iOffset(NULL), iStride(NULL),
      iData(NULL)
    {
    }

CAnimation::~CAnimation()
    {
    if (iData)
        {
        for (TInt i = 0; i < iSequenceCount; i++) delete[] iData[i];
        }
    if (iBones)
        {
        for (TInt i = 0; i < iSequenceCount; i++)
            {
            UnregisterSequenceBones(iBones[i]);
            delete[] iBones[i];
            }
        }
    if (iOffset)
        {
        for (TInt i = 0; i < iSequenceCount; i++) delete[] iOffset[i];
        }
    if (iConstantPosition)
        {
        for (TInt i = 0; i < iSequenceCount; i++) delete[] iConstantPosition[i];
        }
    if (iConstantRotation)
        {
        for (TInt i = 0; i < iSequenceCount; i++) delete[] iConstantRotation[i];
        }
    if (iRotationAnimated)
        {
        for (TInt i = 0; i < iSequenceCount; i++) delete[] iRotationAnimated[i];
        }
    if (iPositionAnimated)
        {
        for (TInt i = 0; i < iSequenceCount; i++) delete[] iPositionAnimated[i];
        }
    delete[] iData;
    delete[] iStride;
    delete[] iOffset;
    delete[] iConstantPosition;
    delete[] iConstantRotation;
    delete[] iRotationAnimated;
    delete[] iPositionAnimated;
    delete[] iBones;
    delete[] iBlends;
    delete[] iFrames;
    delete[] iFps;
    delete[] iName;
    }

CAnimation* CAnimation::LoadL(RFs& aFileServer, const TDesC& aDirectory, const TDesC& aName)
    {
    TPathBuffer leaf;
    AppendLeaf(leaf, aName, _L(".a3"));
    TPathBuffer path;
    MakePath(path, aDirectory, leaf);

    HBufC8* file = ReadFileOrNullL(aFileServer, path);
    if (!file) return NULL;
    CleanupStack::PushL(file);
    TBinaryReader in(file->Ptr(), file->Length());

    if (in.ReadInteger() != 0x43534133)
        {
        CleanupStack::PopAndDestroy(file);
        return NULL;
        }

    CAnimation* animation = new CAnimation;
    if (!animation)
        {
        CleanupStack::PopAndDestroy(file);
        return NULL;
        }
    CleanupStack::PushL(TCleanupItem(DeleteAnimationObject, animation));

    animation->iBoneCount = in.ReadSignedShort();
    animation->iSequenceCount = in.ReadSignedShort();
    if (animation->iBoneCount < 0) animation->iBoneCount = 0;
    if (animation->iSequenceCount < 0) animation->iSequenceCount = 0;

    animation->iName = NewArrayL<TAsciiName>(animation->iSequenceCount);
    animation->iFps = NewArrayL<TReal32>(animation->iSequenceCount);
    animation->iFrames = NewArrayL<TInt>(animation->iSequenceCount);
    animation->iBlends = NewArrayL<TInt>(animation->iSequenceCount);
    animation->iBones = NewArrayL<TInt*>(animation->iSequenceCount);
    animation->iPositionAnimated = NewArrayL<TBool*>(animation->iSequenceCount);
    animation->iRotationAnimated = NewArrayL<TBool*>(animation->iSequenceCount);
    animation->iConstantRotation = NewArrayL<TInt16*>(animation->iSequenceCount);
    animation->iConstantPosition = NewArrayL<TInt16*>(animation->iSequenceCount);
    animation->iOffset = NewArrayL<TInt*>(animation->iSequenceCount);
    animation->iStride = NewArrayL<TInt>(animation->iSequenceCount);
    animation->iData = NewArrayL<TInt16*>(animation->iSequenceCount);

    for (TInt sequence = 0; sequence < animation->iSequenceCount; sequence++)
        {
        animation->iName[sequence].Set(in.ReadString8());
        animation->iFps[sequence] = in.ReadReal();
        animation->iFrames[sequence] = in.ReadSignedShort();
        animation->iBlends[sequence] = in.ReadByte();
        TInt usedBones = in.ReadSignedShort();
        if (usedBones < 0) usedBones = 0;
        if (animation->iFrames[sequence] < 1) animation->iFrames[sequence] = 1;
        if (animation->iBlends[sequence] < 1) animation->iBlends[sequence] = 1;

        animation->iBones[sequence] = NewArrayL<TInt>(usedBones);
        animation->iPositionAnimated[sequence] = NewArrayL<TBool>(usedBones);
        animation->iRotationAnimated[sequence] = NewArrayL<TBool>(usedBones);
        animation->iConstantRotation[sequence] = NewArrayL<TInt16>(usedBones * 3);
        animation->iConstantPosition[sequence] = NewArrayL<TInt16>(usedBones * 3);
        animation->iOffset[sequence] = NewArrayL<TInt>(usedBones);
        RegisterSequenceBones(animation->iBones[sequence], usedBones);

        TInt stride = 0;
        for (TInt k = 0; k < usedBones; k++)
            {
            animation->iBones[sequence][k] = (TInt8)in.ReadByte();
            const TInt flags = in.ReadByte();
            animation->iPositionAnimated[sequence][k] = (flags & 1) != 0 ? ETrue : EFalse;
            animation->iRotationAnimated[sequence][k] = (flags & 2) != 0 ? ETrue : EFalse;
            if (!animation->iPositionAnimated[sequence][k])
                {
                for (TInt axis = 0; axis < 3; axis++)
                    animation->iConstantPosition[sequence][k * 3 + axis] = in.ReadSignedShort();
                }
            if (!animation->iRotationAnimated[sequence][k])
                {
                for (TInt axis = 0; axis < 3; axis++)
                    animation->iConstantRotation[sequence][k * 3 + axis] = in.ReadSignedShort();
                }
            animation->iOffset[sequence][k] = stride;
            if (animation->iRotationAnimated[sequence][k]) stride += 3;
            if (animation->iPositionAnimated[sequence][k]) stride += 3;
            }
        animation->iStride[sequence] = stride;

        const TInt values = animation->iBlends[sequence] * animation->iFrames[sequence] * stride;
        animation->iData[sequence] = NewArrayL<TInt16>(values);
        for (TInt value = 0; value < values; value++)
            animation->iData[sequence][value] = in.ReadSignedShort();
        }

    CleanupStack::Pop(animation);
    CleanupStack::PopAndDestroy(file);
    return animation;
    }

TInt CAnimation::Find(const TPtrC8& aName) const
    {
    for (TInt i = 0; i < iSequenceCount; i++)
        if (iName[i].Equals(aName)) return i;
    return -1;
    }

TInt CAnimation::Find(const char* aName) const
    {
    for (TInt i = 0; i < iSequenceCount; i++)
        if (iName[i].Equals(aName)) return i;
    return -1;
    }

TReal32 CAnimation::Duration(TInt aSequence) const
    {
    if (aSequence < 0 || aSequence >= iSequenceCount) return 0;
    if (iFps[aSequence] == 0) return 0;
    return (TReal32)iFrames[aSequence] / iFps[aSequence];
    }

void CAnimation::Pose(TInt aSequence, TReal32 aFrame, TBool aLoop, TReal32 aBlend,
                      TReal32* aRotation, TReal32* aPosition) const
    {
    if (aSequence < 0 || aSequence >= iSequenceCount) return;

    const TInt frameCount = iFrames[aSequence];
    if (frameCount <= 0) return;
    TInt frameStart;
    TInt frameEnd;
    TReal32 frameFraction;

    if (aLoop)
        {
        aFrame -= (TReal32)((TInt)(aFrame / frameCount)) * frameCount;
        if (aFrame < 0) aFrame += frameCount;
        frameStart = (TInt)aFrame;
        frameEnd = frameStart + 1 >= frameCount ? 0 : frameStart + 1;
        frameFraction = aFrame - frameStart;
        }
    else
        {
        if (aFrame < 0) aFrame = 0;
        if (aFrame > frameCount - 1) aFrame = frameCount - 1;
        frameStart = (TInt)aFrame;
        frameEnd = frameStart + 1 > frameCount - 1 ? frameCount - 1 : frameStart + 1;
        frameFraction = aFrame - frameStart;
        }

    const TInt blendCount = iBlends[aSequence];
    if (aBlend < 0) aBlend = 0;
    if (aBlend > blendCount - 1) aBlend = blendCount - 1;
    const TInt blendStart = (TInt)aBlend;
    const TInt blendEnd = blendStart + 1 > blendCount - 1 ? blendCount - 1 : blendStart + 1;
    const TReal32 blendFraction = aBlend - blendStart;

    const TInt stride = iStride[aSequence];
    const TInt16* data = iData[aSequence];
    const TInt record00 = (blendStart * frameCount + frameStart) * stride;
    const TInt record01 = (blendStart * frameCount + frameEnd) * stride;
    const TInt record10 = (blendEnd * frameCount + frameStart) * stride;
    const TInt record11 = (blendEnd * frameCount + frameEnd) * stride;
    const TReal32 weight00 = (1 - frameFraction) * (1 - blendFraction);
    const TReal32 weight01 = frameFraction * (1 - blendFraction);
    const TReal32 weight10 = (1 - frameFraction) * blendFraction;
    const TReal32 weight11 = frameFraction * blendFraction;

    const TInt* bones = iBones[aSequence];
    const TInt* offsets = iOffset[aSequence];
    const TBool* positionAnimated = iPositionAnimated[aSequence];
    const TBool* rotationAnimated = iRotationAnimated[aSequence];
    const TInt16* constantPosition = iConstantPosition[aSequence];
    const TInt16* constantRotation = iConstantRotation[aSequence];
    const TInt usedBones = SequenceBoneCount(bones);

    for (TInt k = 0; k < usedBones; k++)
        {
        const TInt bone = bones[k];
        TInt offset = offsets[k];

        if (!rotationAnimated[k])
            {
            const TReal32 x = constantRotation[k * 3] * KQuaternionScale;
            const TReal32 y = constantRotation[k * 3 + 1] * KQuaternionScale;
            const TReal32 z = constantRotation[k * 3 + 2] * KQuaternionScale;
            aRotation[bone * 4] = x;
            aRotation[bone * 4 + 1] = y;
            aRotation[bone * 4 + 2] = z;
            aRotation[bone * 4 + 3] = QuaternionComponentW(x, y, z);
            }
        else
            {
            TReal32 quaternionX = 0;
            TReal32 quaternionY = 0;
            TReal32 quaternionZ = 0;
            TReal32 quaternionW = 0;

            const TReal32 ax = data[record00 + offset] * KQuaternionScale;
            const TReal32 ay = data[record00 + offset + 1] * KQuaternionScale;
            const TReal32 az = data[record00 + offset + 2] * KQuaternionScale;
            const TReal32 aw = QuaternionComponentW(ax, ay, az);
            quaternionX += ax * weight00;
            quaternionY += ay * weight00;
            quaternionZ += az * weight00;
            quaternionW += aw * weight00;

            if (weight01 > 0)
                {
                const TReal32 x = data[record01 + offset] * KQuaternionScale;
                const TReal32 y = data[record01 + offset + 1] * KQuaternionScale;
                const TReal32 z = data[record01 + offset + 2] * KQuaternionScale;
                const TReal32 w = QuaternionComponentW(x, y, z);
                const TReal32 sign = ax * x + ay * y + az * z + aw * w < 0 ? -weight01 : weight01;
                quaternionX += x * sign;
                quaternionY += y * sign;
                quaternionZ += z * sign;
                quaternionW += w * sign;
                }
            if (weight10 > 0)
                {
                const TReal32 x = data[record10 + offset] * KQuaternionScale;
                const TReal32 y = data[record10 + offset + 1] * KQuaternionScale;
                const TReal32 z = data[record10 + offset + 2] * KQuaternionScale;
                const TReal32 w = QuaternionComponentW(x, y, z);
                const TReal32 sign = ax * x + ay * y + az * z + aw * w < 0 ? -weight10 : weight10;
                quaternionX += x * sign;
                quaternionY += y * sign;
                quaternionZ += z * sign;
                quaternionW += w * sign;
                }
            if (weight11 > 0)
                {
                const TReal32 x = data[record11 + offset] * KQuaternionScale;
                const TReal32 y = data[record11 + offset + 1] * KQuaternionScale;
                const TReal32 z = data[record11 + offset + 2] * KQuaternionScale;
                const TReal32 w = QuaternionComponentW(x, y, z);
                const TReal32 sign = ax * x + ay * y + az * z + aw * w < 0 ? -weight11 : weight11;
                quaternionX += x * sign;
                quaternionY += y * sign;
                quaternionZ += z * sign;
                quaternionW += w * sign;
                }

            TReal32 length = FastSqrt(quaternionX * quaternionX + quaternionY * quaternionY +
                                      quaternionZ * quaternionZ + quaternionW * quaternionW);
            if (length < 1e-6f) length = 1;
            length = 1 / length;
            aRotation[bone * 4] = quaternionX * length;
            aRotation[bone * 4 + 1] = quaternionY * length;
            aRotation[bone * 4 + 2] = quaternionZ * length;
            aRotation[bone * 4 + 3] = quaternionW * length;
            offset += 3;
            }

        if (positionAnimated[k])
            {
            aPosition[bone * 3] = (data[record00 + offset] * weight00 + data[record01 + offset] * weight01 +
                                   data[record10 + offset] * weight10 + data[record11 + offset] * weight11) / 16.0f;
            aPosition[bone * 3 + 1] = (data[record00 + offset + 1] * weight00 + data[record01 + offset + 1] * weight01 +
                                      data[record10 + offset + 1] * weight10 + data[record11 + offset + 1] * weight11) / 16.0f;
            aPosition[bone * 3 + 2] = (data[record00 + offset + 2] * weight00 + data[record01 + offset + 2] * weight01 +
                                      data[record10 + offset + 2] * weight10 + data[record11 + offset + 2] * weight11) / 16.0f;
            }
        else
            {
            aPosition[bone * 3] = constantPosition[k * 3] / 16.0f;
            aPosition[bone * 3 + 1] = constantPosition[k * 3 + 1] / 16.0f;
            aPosition[bone * 3 + 2] = constantPosition[k * 3 + 2] / 16.0f;
            }
        }
    }

/* ----------------------------------------------------------- sprites, fonts */

/* Гамма спрайтов та же, что и у карты: рендер задаёт её в SetMap. */
namespace
    {
    TUint8 gSpriteGamma[256] =
        {
        0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,
        16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,
        32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,
        48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,
        64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,
        80,  81,  82,  83,  84,  85,  86,  87,  88,  89,  90,  91,  92,  93,  94,  95,
        96,  97,  98,  99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
        112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127,
        128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143,
        144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
        160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175,
        176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191,
        192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207,
        208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223,
        224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
        240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255
        };
    const TInt KMaxSpriteFrames = 16384;
    const TInt KMaxSpriteAnimations = 512;
    const TInt KMaxSpriteFrameDimension = 1024;
    }

void SetSpriteGamma(const TUint8* aGamma)
    {
    if (aGamma) Mem::Copy(gSpriteGamma, aGamma, 256);
    }

CSpriteSet::CSpriteSet()
    : iCount(0), iSprites(NULL), iPlayer(EFalse), iDirs(1), iPpu(1.0f),
      iAx(0), iAy(0), iAnimCount(0), iAnim(NULL), iDur(NULL), iNfr(NULL),
      iFrameStart(NULL), iLitLevel(-1)
    {
    Mem::FillZ(iPal, sizeof(iPal));
    Mem::FillZ(iLitPal, sizeof(iLitPal));
    }

CSpriteSet::~CSpriteSet()
    {
    Release();
    }

void CSpriteSet::Release()
    {
    if (iSprites)
        {
        for (TInt i = 0; i < iCount; i++) delete[] iSprites[i].iPixels;
        }
    delete[] iSprites;
    delete[] iAnim;
    delete[] iDur;
    delete[] iNfr;
    delete[] iFrameStart;

    iCount = 0;
    iSprites = NULL;
    iPlayer = EFalse;
    iDirs = 1;
    iPpu = 1.0f;
    iAx = 0;
    iAy = 0;
    iAnimCount = 0;
    iAnim = NULL;
    iDur = NULL;
    iNfr = NULL;
    iFrameStart = NULL;
    iLitLevel = -1;
    }

void CSpriteSet::BuildPalette()
    {
    Mem::FillZ(iLitPal, sizeof(iLitPal));
    }

const TUint32* CSpriteSet::Lit(TInt aLevel)
    {
    if (aLevel == iLitLevel) return iLitPal;
    iLitLevel = aLevel;
    TInt k = aLevel * 8;
    for (TInt i = 1; i < 256; i++)
        {
        TInt c = (TInt)iPal[i];
        TInt red = (((c >> 16) & 255) * k) >> 8;
        TInt green = (((c >> 8) & 255) * k) >> 8;
        TInt blue = ((c & 255) * k) >> 8;
        if (red > 255) red = 255;
        if (green > 255) green = 255;
        if (blue > 255) blue = 255;
        iLitPal[i] = ((TUint32)gSpriteGamma[red] << 16) |
                     ((TUint32)gSpriteGamma[green] << 8) |
                      (TUint32)gSpriteGamma[blue];
        }
    return iLitPal;
    }

void CSpriteSet::LoadL(RFs& aFileServer, const TDesC& aFullPath)
    {
    Release();
    HBufC8* file = ReadWholeFileL(aFileServer, aFullPath);
    CleanupStack::PushL(file);
    TBinaryReader in(file->Ptr(), file->Length());
    TUint32 magic = (TUint32)in.ReadInteger();

    if (magic == 0x48554431)
        {
        iCount = in.ReadSignedShort();
        if (iCount < 0 || iCount > KMaxSpriteFrames) User::Leave(KErrCorrupt);
        iSprites = NewArrayL<TSprite>(iCount);
        for (TInt i = 0; i < iCount; i++)
            {
            iSprites[i].iPixels = NULL;
            iSprites[i].iWidth = 0;
            iSprites[i].iHeight = 0;
            iSprites[i].iOffsetX = 0;
            iSprites[i].iOffsetY = 0;
            }
        for (TInt i = 0; i < iCount; i++)
            {
            iSprites[i].iName.Set(in.ReadString8());
            iSprites[i].iWidth = in.ReadSignedShort();
            iSprites[i].iHeight = in.ReadSignedShort();
            TInt size = iSprites[i].iWidth * iSprites[i].iHeight;
            if (size < 0 || size > KMaxSpriteFrameDimension * KMaxSpriteFrameDimension)
                User::Leave(KErrCorrupt);
            iSprites[i].iPixels = NewArrayL<TUint8>(size);
            in.ReadBytes(iSprites[i].iPixels, size);
            }
        CleanupStack::PopAndDestroy(file);
        return;
        }

    if (magic != 0x56575331 && magic != 0x504C5331) User::Leave(KErrCorrupt);

    TUint8 rawPalette[768];
    in.ReadBytes(rawPalette, sizeof(rawPalette));
    for (TInt i = 0; i < 256; i++)
        iPal[i] = ((TUint32)rawPalette[i * 3] << 16) |
                  ((TUint32)rawPalette[i * 3 + 1] << 8) |
                   (TUint32)rawPalette[i * 3 + 2];
    BuildPalette();

    iPlayer = magic == 0x504C5331 ? ETrue : EFalse;
    if (iPlayer)
        {
        iPpu = in.ReadReal();
        iAx = in.ReadSignedShort();
        iAy = in.ReadSignedShort();
        iDirs = 8;
        }
    else
        {
        iPpu = 1.0f;
        iDirs = 1;
        }

    iAnimCount = in.ReadSignedShort();
    if (iAnimCount < 0 || iAnimCount > KMaxSpriteAnimations) User::Leave(KErrCorrupt);
    iAnim = NewArrayL<TAsciiName>(iAnimCount);
    iDur = NewArrayL<TReal32>(iAnimCount);
    iNfr = NewArrayL<TInt>(iAnimCount);
    iFrameStart = NewArrayL<TInt>(iAnimCount);
    Mem::FillZ(iDur, iAnimCount * sizeof(TReal32));
    Mem::FillZ(iNfr, iAnimCount * sizeof(TInt));
    Mem::FillZ(iFrameStart, iAnimCount * sizeof(TInt));

    /* Первый проход считает кадры, чтобы выделить один непрерывный массив. */
    TBinaryReader scan(file->Ptr(), file->Length());
    scan.ReadInteger();
    scan.Skip(sizeof(rawPalette));
    if (iPlayer)
        {
        scan.ReadReal();
        scan.ReadSignedShort();
        scan.ReadSignedShort();
        }
    scan.ReadSignedShort();
    TInt totalFrames = 0;
    for (TInt a = 0; a < iAnimCount; a++)
        {
        scan.ReadString8();
        scan.ReadReal();
        TInt frames = scan.ReadSignedShort();
        if (frames < 0) frames = 0;
        iNfr[a] = frames;
        iFrameStart[a] = totalFrames;
        totalFrames += frames * iDirs;
        if (totalFrames > KMaxSpriteFrames) User::Leave(KErrCorrupt);
        for (TInt k = 0; k < frames * iDirs; k++)
            {
            scan.ReadSignedShort();
            scan.ReadSignedShort();
            TInt width = scan.ReadSignedShort();
            TInt height = scan.ReadSignedShort();
            if (width < 0 || height < 0 ||
                width > KMaxSpriteFrameDimension || height > KMaxSpriteFrameDimension)
                User::Leave(KErrCorrupt);
            scan.Skip(width * height);
            }
        }

    iCount = totalFrames;
    iSprites = iCount > 0 ? NewArrayL<TSprite>(iCount) : NULL;
    for (TInt i = 0; i < iCount; i++)
        {
        iSprites[i].iPixels = NULL;
        iSprites[i].iWidth = 0;
        iSprites[i].iHeight = 0;
        iSprites[i].iOffsetX = 0;
        iSprites[i].iOffsetY = 0;
        }

    for (TInt a = 0; a < iAnimCount; a++)
        {
        iAnim[a].Set(in.ReadString8());
        iDur[a] = in.ReadReal();
        TInt frames = in.ReadSignedShort();
        for (TInt k = 0; k < frames * iDirs; k++)
            {
            TSprite& sprite = iSprites[iFrameStart[a] + k];
            sprite.iName.Set(iAnim[a].Des());
            sprite.iOffsetX = in.ReadSignedShort();
            sprite.iOffsetY = in.ReadSignedShort();
            sprite.iWidth = in.ReadSignedShort();
            sprite.iHeight = in.ReadSignedShort();
            TInt size = sprite.iWidth * sprite.iHeight;
            sprite.iPixels = NewArrayL<TUint8>(size);
            in.ReadBytes(sprite.iPixels, size);
            }
        }

    CleanupStack::PopAndDestroy(file);
    }

TInt CSpriteSet::Find(const char* aName) const
    {
    for (TInt i = 0; i < iCount; i++)
        if (iSprites[i].iName.Equals(aName)) return i;
    return -1;
    }

TInt CSpriteSet::FindAnim(const TPtrC8& aName) const
    {
    for (TInt i = 0; i < iAnimCount; i++)
        if (iAnim[i].Equals(aName)) return i;
    return -1;
    }

TInt CSpriteSet::FrameIndex(TInt aAnimation, TInt aDirection, TInt aFrame) const
    {
    if (aAnimation < 0 || aAnimation >= iAnimCount) return -1;
    TInt direction = Clamp(aDirection, 0, iDirs - 1);
    TInt frame = Clamp(aFrame, 0, iNfr[aAnimation] - 1);
    TInt index = iFrameStart[aAnimation] + direction * iNfr[aAnimation] + frame;
    return index >= 0 && index < iCount ? index : -1;
    }

const CSpriteSet::TSprite* CSpriteSet::Frame(TInt aAnimation, TInt aDirection, TInt aFrame) const
    {
    TInt index = FrameIndex(aAnimation, aDirection, aFrame);
    return index >= 0 ? &iSprites[index] : NULL;
    }

CCSFont::CCSFont()
    : iHeight(0), iCount(0), iGlyphs(NULL)
    {
    }

CCSFont::~CCSFont()
    {
    if (iGlyphs)
        {
        for (TInt i = 0; i < iCount; i++) delete[] iGlyphs[i].iPixels;
        }
    delete[] iGlyphs;
    }

void CCSFont::LoadL(RFs& aFileServer, const TDesC& aFullPath)
    {
    HBufC8* file = ReadWholeFileL(aFileServer, aFullPath);
    CleanupStack::PushL(file);
    TBinaryReader in(file->Ptr(), file->Length());

    if (in.ReadInteger() != 0x464E5431) User::Leave(KErrCorrupt);

    iHeight = in.ReadSignedShort();
    iCount = in.ReadSignedShort();
    if (iHeight < 0) iHeight = 0;
    if (iCount < 0) iCount = 0;
    iGlyphs = NewArrayL<TGlyph>(iCount);
    for (TInt i = 0; i < iCount; i++)
        {
        iGlyphs[i].iPixels = NULL;
        iGlyphs[i].iCode = 0;
        iGlyphs[i].iWidth = 0;
        }

    for (TInt i = 0; i < iCount; i++)
        {
        iGlyphs[i].iCode = in.ReadUnsignedShort();
        iGlyphs[i].iWidth = in.ReadByte();
        TInt size = iGlyphs[i].iWidth * iHeight;
        if (size < 0) size = 0;
        iGlyphs[i].iPixels = NewArrayL<TUint8>(size);
        in.ReadBytes(iGlyphs[i].iPixels, size);
        }

    for (TInt i = 1; i < iCount; i++)
        {
        const TGlyph current = iGlyphs[i];
        TInt position = i - 1;
        while (position >= 0 && iGlyphs[position].iCode > current.iCode)
            {
            iGlyphs[position + 1] = iGlyphs[position];
            position--;
            }
        iGlyphs[position + 1] = current;
        }

    CleanupStack::PopAndDestroy(file);
    }

const CCSFont::TGlyph* CCSFont::Glyph(TUint16 aCode) const
    {
    TInt low = 0;
    TInt high = iCount - 1;
    while (low <= high)
        {
        const TInt middle = (low + high) >> 1;
        if (iGlyphs[middle].iCode == aCode) return &iGlyphs[middle];
        if (iGlyphs[middle].iCode < aCode) low = middle + 1;
        else high = middle - 1;
        }
    return NULL;
    }

/* ------------------------------------------------------------------ manager */

struct TModelCacheSlot
    {
    CAssetManager* iOwner;
    TAsciiName iName;
    CModel* iObject;
    };

struct TAnimationCacheSlot
    {
    CAssetManager* iOwner;
    TAsciiName iName;
    CAnimation* iObject;
    };

static TModelCacheSlot gModelCache[KMaxCachedModels];
static TAnimationCacheSlot gAnimationCache[KMaxCachedAnimations];

CAssetManager::CAssetManager()
    : iFileServer(NULL), iDirectoryBuffer(NULL), iModRootBuffer(NULL), iMap(NULL)
    {
    iModRoot.Set(KNullDesC);
    iCurrentModIndex = -1;
    iModCount = 0;
    for (TInt i = 0; i < KMaxMods; i++) iModNames[i][0] = 0;
    }

CAssetManager::~CAssetManager()
    {
    delete iMap;
    for (TInt i = 0; i < KMaxCachedModels; i++)
        {
        if (gModelCache[i].iOwner != this) continue;
        delete gModelCache[i].iObject;
        gModelCache[i].iOwner = NULL;
        gModelCache[i].iObject = NULL;
        }
    for (TInt i = 0; i < KMaxCachedAnimations; i++)
        {
        if (gAnimationCache[i].iOwner != this) continue;
        delete gAnimationCache[i].iObject;
        gAnimationCache[i].iOwner = NULL;
        gAnimationCache[i].iObject = NULL;
        }
    delete iDirectoryBuffer;
    iDirectoryBuffer = NULL;
    delete iModRootBuffer;
    iModRootBuffer = NULL;
    }

void CAssetManager::ConstructL(RFs& aFileServer, const TDesC& aAssetDirectory)
    {
    iFileServer = &aFileServer;
    delete iDirectoryBuffer;
    iDirectoryBuffer = aAssetDirectory.AllocL();
    iDirectory.Set(*iDirectoryBuffer);
    delete iModRootBuffer;
    iModRootBuffer = NULL;
    iModRoot.Set(KNullDesC);
    iCurrentModIndex = -1;
    iModCount = 0;
    for (TInt i = 0; i < KMaxMods; i++) iModNames[i][0] = 0;

    CModel::LoadModelTexturesL(aFileServer, iDirectory);

    TPathBuffer path;
    ResolveModPath(_L("hud.bin"), path);
    iHud.LoadL(aFileServer, path);

    ResolveModPath(_L("f1.fnt"), path);
    iFontSmall.LoadL(aFileServer, path);
    ResolveModPath(_L("f2.fnt"), path);
    iFontBig.LoadL(aFileServer, path);

    ResolveModPath(_L("t.pls"), path);
    if (FileExists(aFileServer, path)) iSpritesT.LoadL(aFileServer, path);
    else
        {
        ResolveModPath(_L("t.hud"), path);
        if (FileExists(aFileServer, path)) iSpritesT.LoadL(aFileServer, path);
        }
    ResolveModPath(_L("ct.pls"), path);
    if (FileExists(aFileServer, path)) iSpritesCT.LoadL(aFileServer, path);
    else
        {
        ResolveModPath(_L("ct.hud"), path);
        if (FileExists(aFileServer, path)) iSpritesCT.LoadL(aFileServer, path);
        }
    }

static void ScanModsFromDir(RFs& aFs, const TDesC& aModsDir, CAssetManager& aMgr)
    {
    RDir dir;
    TInt err = dir.Open(aFs, aModsDir, KEntryAttDir);
    if (err != KErrNone) return;
    TEntryArray entries;
    while (dir.Read(entries) == KErrNone && entries.Count() > 0)
        {
        for (TInt i = 0; i < entries.Count() && aMgr.iModCount < KMaxMods; i++)
            {
            const TEntry& e = entries[i];
            if ((e.iAtt & KEntryAttDir) == 0) continue;
            TPtrC name = e.iName;
            TBool exists = EFalse;
            for (TInt j = 0; j < aMgr.iModCount; j++)
                {
                if (TPtrC(aMgr.iModNames[j]).CompareF(name) == 0) { exists = ETrue; break; }
                }
            if (exists) continue;
            TInt copyLen = name.Length();
            if (copyLen >= 32) copyLen = 31;
            for (TInt c = 0; c < copyLen; c++) aMgr.iModNames[aMgr.iModCount][c] = name[c];
            aMgr.iModNames[aMgr.iModCount][copyLen] = 0;
            aMgr.iModCount++;
            }
        }
    dir.Close();
    }

void CAssetManager::ScanModsL()
    {
    iModCount = 0;
    for (TInt i = 0; i < KMaxMods; i++) iModNames[i][0] = 0;
    delete iModRootBuffer;
    iModRootBuffer = NULL;
    iModRoot.Set(KNullDesC);
    iCurrentModIndex = -1;
    if (!iFileServer) return;

    TBuf<256> modsDir;

    /* E:\cs16\mods\ */
    modsDir.Copy(_L("E:\\cs16\\mods\\"));
    ScanModsFromDir(*iFileServer, modsDir, *this);

    /* C:\cs16\mods\ */
    modsDir.Copy(_L("C:\\cs16\\mods\\"));
    ScanModsFromDir(*iFileServer, modsDir, *this);

    /* <iDirectory>mods\ */
    modsDir = iDirectory;
    if (modsDir.Length() > 0 && modsDir[modsDir.Length()-1] != '\\' && modsDir[modsDir.Length()-1] != '/')
        modsDir.Append(_L("\\"));
    modsDir.Append(_L("mods\\"));
    ScanModsFromDir(*iFileServer, modsDir, *this);
    }

TInt CAssetManager::ModCount() const
    {
    return iModCount;
    }

TPtrC CAssetManager::ModName(TInt aIndex) const
    {
    if (aIndex < 0 || aIndex >= iModCount) return TPtrC();
    return TPtrC(iModNames[aIndex]);
    }

void CAssetManager::SetMod(TInt aIndex)
    {
    delete iModRootBuffer;
    iModRootBuffer = NULL;
    iModRoot.Set(KNullDesC);
    iCurrentModIndex = -1;
    if (!iFileServer || aIndex < 0 || aIndex >= iModCount) return;
    TPtrC name(iModNames[aIndex]);
    TBuf<256> tryPath;

    /* Try E:\cs16\mods\<name>\ */
    tryPath.Copy(_L("E:\\cs16\\mods\\"));
    tryPath.Append(name);
    if (tryPath.Length() > 0 && tryPath[tryPath.Length()-1] != '\\') tryPath.Append(_L("\\"));
    {
    RDir d;
    if (d.Open(*iFileServer, tryPath, KEntryAttDir) == KErrNone) { d.Close(); iModRootBuffer = tryPath.Alloc(); if (iModRootBuffer) { iModRoot.Set(*iModRootBuffer); iCurrentModIndex = aIndex; } return; }
    }
    /* Try C:\cs16\mods\<name>\ */
    tryPath.Copy(_L("C:\\cs16\\mods\\"));
    tryPath.Append(name);
    if (tryPath.Length() > 0 && tryPath[tryPath.Length()-1] != '\\') tryPath.Append(_L("\\"));
    {
    RDir d;
    if (d.Open(*iFileServer, tryPath, KEntryAttDir) == KErrNone) { d.Close(); iModRootBuffer = tryPath.Alloc(); if (iModRootBuffer) { iModRoot.Set(*iModRootBuffer); iCurrentModIndex = aIndex; } return; }
    }
    /* Try <iDirectory>mods\<name>\ */
    tryPath = iDirectory;
    if (tryPath.Length() > 0 && tryPath[tryPath.Length()-1] != '\\' && tryPath[tryPath.Length()-1] != '/') tryPath.Append(_L("\\"));
    tryPath.Append(_L("mods\\"));
    tryPath.Append(name);
    if (tryPath.Length() > 0 && tryPath[tryPath.Length()-1] != '\\') tryPath.Append(_L("\\"));
    {
    RDir d;
    if (d.Open(*iFileServer, tryPath, KEntryAttDir) == KErrNone) { d.Close(); iModRootBuffer = tryPath.Alloc(); if (iModRootBuffer) { iModRoot.Set(*iModRootBuffer); iCurrentModIndex = aIndex; } return; }
    }
    }

TInt CAssetManager::CurrentModIndex() const
    {
    return iCurrentModIndex;
    }

TPtrC CAssetManager::CurrentModName() const
    {
    if (iCurrentModIndex < 0) return TPtrC();
    return TPtrC(iModNames[iCurrentModIndex]);
    }

TPtrC CAssetManager::CurrentModPath() const
    {
    return iModRoot;
    }

TBool CAssetManager::HasModOverride(const TDesC& aRelativePath) const
    {
    if (!iFileServer || iModRoot.Length() == 0) return EFalse;
    TFileName p;
    p.Copy(iModRoot);
    p.Append(aRelativePath);
    return FileExists(*iFileServer, p);
    }

void CAssetManager::ResolveModPath(const TDesC& aRelativePath, TDes& aOut) const
    {
    if (iFileServer && iModRoot.Length() > 0)
        {
        TFileName p;
        p.Copy(iModRoot);
        p.Append(aRelativePath);
        if (FileExists(*iFileServer, p))
            {
            aOut.Copy(p);
            return;
            }
        }
    aOut.Copy(iDirectory);
    if (aOut.Length() > 0 && aOut[aOut.Length()-1] != '\\' && aOut[aOut.Length()-1] != '/')
        aOut.Append(_L("\\"));
    aOut.Append(aRelativePath);
    }

void CAssetManager::ResolveAssetPath(const TDesC& aRelativePath, TDes& aOut) const
    {
    ResolveModPath(aRelativePath, aOut);
    }

CMap* CAssetManager::LoadMapL(const TDesC& aName)
    {
    delete iMap;
    iMap = NULL;

    CMap* map = new CMap;
    if (!map) User::Leave(KErrNoMemory);
    CleanupStack::PushL(TCleanupItem(DeleteMapObject, map));
    /* Try mod override: map file name is aName + ".cse" */
    if (iModRoot.Length() > 0)
        {
        TPathBuffer leaf;
        AppendLeaf(leaf, aName, _L(".cse"));
        TFileName modPath;
        modPath.Copy(iModRoot);
        modPath.Append(leaf);
        if (FileExists(*iFileServer, modPath))
            {
            map->LoadL(*iFileServer, iModRoot, aName);
            CleanupStack::Pop(map);
            iMap = map;
            return iMap;
            }
        }
    map->LoadL(*iFileServer, iDirectory, aName);
    CleanupStack::Pop(map);
    iMap = map;
    return iMap;
    }

CModel* CAssetManager::Model(const char* aName)
    {
    if (!iFileServer || !aName) return NULL;
    const TPtrC8 key = AsciiName(aName);

    for (TInt i = 0; i < KMaxCachedModels; i++)
        {
        if (gModelCache[i].iOwner == this && gModelCache[i].iName.Equals(key))
            return gModelCache[i].iObject;
        }

    TPathBuffer name;
    name.Zero();
    AppendAscii(name, key);
    CModel* model = CModel::LoadL(*iFileServer, iDirectory, name);
    if (!model) return NULL;

    for (TInt i = 0; i < KMaxCachedModels; i++)
        {
        if (gModelCache[i].iOwner != NULL) continue;
        gModelCache[i].iOwner = this;
        gModelCache[i].iName.Set(key);
        gModelCache[i].iObject = model;
        return model;
        }

    delete model;
    return NULL;
    }

CAnimation* CAssetManager::Animation(const char* aName)
    {
    if (!iFileServer || !aName) return NULL;
    const TPtrC8 key = AsciiName(aName);

    for (TInt i = 0; i < KMaxCachedAnimations; i++)
        {
        if (gAnimationCache[i].iOwner == this && gAnimationCache[i].iName.Equals(key))
            return gAnimationCache[i].iObject;
        }

    TPathBuffer name;
    name.Zero();
    AppendAscii(name, key);
    CAnimation* animation = CAnimation::LoadL(*iFileServer, iDirectory, name);
    if (!animation) return NULL;

    for (TInt i = 0; i < KMaxCachedAnimations; i++)
        {
        if (gAnimationCache[i].iOwner != NULL) continue;
        gAnimationCache[i].iOwner = this;
        gAnimationCache[i].iName.Set(key);
        gAnimationCache[i].iObject = animation;
        return animation;
        }

    delete animation;
    return NULL;
    }

CModel* CAssetManager::WeaponModel(const char* aName)
    {
    if (!iFileServer || !aName) return NULL;
    const TPtrC8 key = AsciiName(aName);
    TPathBuffer prefixedPath;
    MakeAsciiPath(prefixedPath, iDirectory, _L("v_"), key, _L(".m3"));
    if (!FileExists(*iFileServer, prefixedPath)) return Model(aName);

    char prefixedName[KMaxName];
    prefixedName[0] = 'v';
    prefixedName[1] = '_';
    TInt length = key.Length();
    if (length > KMaxName - 3) length = KMaxName - 3;
    for (TInt i = 0; i < length; i++) prefixedName[2 + i] = (char)key[i];
    prefixedName[2 + length] = 0;
    return Model(prefixedName);
    }

CAnimation* CAssetManager::WeaponAnimation(const char* aName)
    {
    if (!iFileServer || !aName) return NULL;
    const TPtrC8 key = AsciiName(aName);
    TPathBuffer prefixedPath;
    MakeAsciiPath(prefixedPath, iDirectory, _L("v_"), key, _L(".a3"));
    if (!FileExists(*iFileServer, prefixedPath)) return Animation(aName);

    char prefixedName[KMaxName];
    prefixedName[0] = 'v';
    prefixedName[1] = '_';
    TInt length = key.Length();
    if (length > KMaxName - 3) length = KMaxName - 3;
    for (TInt i = 0; i < length; i++) prefixedName[2 + i] = (char)key[i];
    prefixedName[2 + length] = 0;
    return Animation(prefixedName);
    }
