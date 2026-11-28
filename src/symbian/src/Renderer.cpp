/*
 * Renderer.cpp -- программный рендер, порт src/core/cs/Render.java.
 *
 * Сохранены полностью: обход BSP спереди назад, построчный буфер покрытия
 * MAXSEG=64, z-буфер для brush-моделей и спрайтов, пять граней неба,
 * билинейные lightmap, перспективная коррекция с шагом 1<<stepShift пикселей,
 * три mip-уровня и все настройки качества.
 *
 * Отличия от Java заданы типом кадра: TUint16 RGB565 вместо int ARGB32.
 * Таблица iShade хранит уже RGB565, поэтому пиксельный цикл не делает
 * преобразование формата. Внутренние циклы используют 16.16 fixed point.
 * Ни один метод отрисовки не выделяет память.
 */

#include "Renderer.h"
#include <e32std.h>

namespace
    {
    const TReal32 KNearPlane = 1.0f;
    const TReal32 KModelNearPlane = 2.0f;
    const TInt KShadeLevelCount = 64;
    const TInt KShadeColorCount = 256;
    const TInt KDepthScale = 16777216;

    /* Высота шрифта для DrawGlyph: в заголовке нет параметра шрифта,
       поэтому DrawText запоминает её здесь перед вызовом DrawGlyph. */
    TInt gGlyphHeight = 0;

    /* Смещение lightmap текущей грани. В Renderer.h нет отдельного поля
       для этой величины, поэтому оно хранится здесь и не требует выделения памяти. */
    TInt gGradientLightOffset = -1;

    /* Буферы вершин и нормалей brush-моделей. Размер фиксирован константой
       KMaxMeshVerts из Renderer.h, выделение происходит в статической памяти. */
    TReal32 gMeshViewX[KMaxMeshVerts];
    TReal32 gMeshViewY[KMaxMeshVerts];
    TReal32 gMeshWorldX[KMaxMeshVerts];
    TReal32 gMeshWorldY[KMaxMeshVerts];
    TReal32 gMeshWorldZ[KMaxMeshVerts];
    TReal32 gMeshNormalX[KMaxMeshVerts];
    TReal32 gMeshNormalY[KMaxMeshVerts];
    TReal32 gMeshNormalZ[KMaxMeshVerts];

    inline TInt CeilValue(TReal32 aValue)
        {
        TInt truncated = (TInt)aValue;
        return aValue > (TReal32)truncated ? truncated + 1 : truncated;
        }

    inline TInt CeilHalf(TReal32 aValue)
        { return CeilValue(aValue - 0.5f); }

    inline TReal32 AbsReal(TReal32 aValue)
        { return aValue < 0 ? -aValue : aValue; }

    inline TInt AbsInt(TInt aValue)
        { return aValue < 0 ? -aValue : aValue; }

    inline TInt ClampInt(TInt aValue, TInt aLow, TInt aHigh)
        { return aValue < aLow ? aLow : (aValue > aHigh ? aHigh : aValue); }

    inline TUint16 ArgbToRgb565(TUint32 aColor)
        {
        return (TUint16)PackRgb565((TInt)((aColor >> 16) & 255),
                                   (TInt)((aColor >> 8) & 255),
                                   (TInt)(aColor & 255));
        }

    inline TInt Red565(TUint16 aColor)
        { return (TInt)((aColor >> 11) & 31); }

    inline TInt Green565(TUint16 aColor)
        { return (TInt)((aColor >> 5) & 63); }

    inline TInt Blue565(TUint16 aColor)
        { return (TInt)(aColor & 31); }

    inline TInt Red8(TUint16 aColor)
        { TInt value = Red565(aColor); return (value << 3) | (value >> 2); }

    inline TInt Green8(TUint16 aColor)
        { TInt value = Green565(aColor); return (value << 2) | (value >> 4); }

    inline TInt Blue8(TUint16 aColor)
        { TInt value = Blue565(aColor); return (value << 3) | (value >> 2); }

    inline TUint16 GammaRgb565(TUint16 aColor, const TUint8* aGamma)
        {
        return (TUint16)PackRgb565(aGamma[Red8(aColor)],
                                   aGamma[Green8(aColor)],
                                   aGamma[Blue8(aColor)]);
        }
    }

CRenderer::CRenderer(TInt aWidth, TInt aHeight, TUint16* aFrameBuffer)
    : iWidth(aWidth > 0 ? aWidth : 1),
      iHeight(aHeight > 0 ? aHeight : 1),
      iFrameBuffer(aFrameBuffer),
      iZBuffer(NULL),
      iMap(NULL),
      iEyeX(0), iEyeY(0), iEyeZ(0), iYaw(0), iPitch(0),
      iForwardX(0), iForwardY(0), iForwardZ(0),
      iRightX(0), iRightY(0), iRightZ(0),
      iUpX(0), iUpY(0), iUpZ(0),
      iXScale(0), iYScale(0), iCenterX(0), iCenterY(0), iFov(90.0f),
      iStatFaces(0), iStatSpans(0), iStatPixels(0), iStatTriangles(0),
      iFullBright(EFalse), iMipBias(0), iStepShift(4),
      iBilinear(EFalse), iFlatSky(EFalse), iMipScale(1.0f),
      iProjectedX(0), iProjectedY(0), iProjectedZ(0),
      iFrame(1), iVisFrame(0), iViewLeaf(-2),
      iLeafVis(NULL), iNodeVis(NULL), iFaceFrame(NULL), iVertexFrame(NULL),
      iTransformedX(NULL), iTransformedY(NULL), iTransformedZ(NULL),
      iSegmentA(NULL), iSegmentB(NULL), iSegmentCount(NULL),
      iCovered(0), iRowFrame(NULL), iRowMin(NULL), iRowMax(NULL), iRowStamp(0),
      iSkyCount(0), iSkyFlat(0), iAlphaCount(0),
      iGradientZx(0), iGradientZy(0), iGradientZ0(0),
      iGradientSx(0), iGradientSy(0), iGradientS0(0),
      iGradientTx(0), iGradientTy(0), iGradientT0(0),
      iGradientLx(0), iGradientLy(0), iGradientL0(0),
      iGradientMx(0), iGradientMy(0), iGradientM0(0),
      iGradientTexture(NULL), iGradientTextureShift(0),
      iGradientTextureMaskS(0), iGradientTextureMaskT(0),
      iGradientLightWidth(0), iGradientLightHeight(0),
      iGradientMasked(EFalse), iGradientConstLevel(0),
      iTextureShift(NULL), iTextureMaskS(NULL), iTextureMaskT(NULL),
      iTextureTShift(NULL)
    {
    SetSpriteGamma(iGamma);
    TInt pixels = iWidth * iHeight;
    if (iFrameBuffer)
        {
        iZBuffer = new TInt[pixels];
        }
    else
        {
        /* Один блок: буфер глубины из TInt, за ним кадр из TUint16.
           Деструктор освобождает только iZBuffer, то есть весь блок целиком. */
        TInt words = pixels + (pixels + 1) / 2;
        iZBuffer = new TInt[words];
        iFrameBuffer = (TUint16*)(iZBuffer + pixels);
        }
    iSegmentA = new TInt16[pixels * KMaxSegments];
    iSegmentB = new TInt16[pixels * KMaxSegments];
    iSegmentCount = new TUint8[iHeight];
    iRowFrame = new TInt[iHeight];
    iRowMin = new TReal32[iHeight];
    iRowMax = new TReal32[iHeight];

    for (TInt i = 0; i < KShadeColorCount; i++) iGamma[i] = (TUint8)i;
    ClearFrame(0);
    ClearZ();
    }

CRenderer::~CRenderer()
    {
    delete[] iZBuffer;
    delete[] iSegmentA;
    delete[] iSegmentB;
    delete[] iSegmentCount;
    delete[] iRowFrame;
    delete[] iRowMin;
    delete[] iRowMax;
    delete[] iLeafVis;
    delete[] iNodeVis;
    delete[] iFaceFrame;
    delete[] iVertexFrame;
    delete[] iTransformedX;
    delete[] iTransformedY;
    delete[] iTransformedZ;
    delete[] iTextureShift;
    delete[] iTextureMaskS;
    delete[] iTextureMaskT;
    delete[] iTextureTShift;
    }

void CRenderer::SetMap(CMap* aMap)
    {
    iMap = aMap;

    delete[] iLeafVis;
    delete[] iNodeVis;
    delete[] iFaceFrame;
    delete[] iVertexFrame;
    delete[] iTransformedX;
    delete[] iTransformedY;
    delete[] iTransformedZ;
    delete[] iTextureShift;
    delete[] iTextureMaskS;
    delete[] iTextureMaskT;
    delete[] iTextureTShift;

    iLeafVis = new TInt[aMap->numLeaves];
    iNodeVis = new TInt[aMap->numNodes > 0 ? aMap->numNodes : 1];
    iFaceFrame = new TInt[aMap->numFaces > 0 ? aMap->numFaces : 1];
    iVertexFrame = new TInt[aMap->numVerts > 0 ? aMap->numVerts : 1];
    iTransformedX = new TReal32[aMap->numVerts > 0 ? aMap->numVerts : 1];
    iTransformedY = new TReal32[aMap->numVerts > 0 ? aMap->numVerts : 1];
    iTransformedZ = new TReal32[aMap->numVerts > 0 ? aMap->numVerts : 1];

    TInt textureEntries = aMap->numTex * 3;
    iTextureShift = new TInt[textureEntries > 0 ? textureEntries : 1];
    iTextureMaskS = new TInt[textureEntries > 0 ? textureEntries : 1];
    iTextureMaskT = new TInt[textureEntries > 0 ? textureEntries : 1];
    iTextureTShift = new TInt[textureEntries > 0 ? textureEntries : 1];

    iViewLeaf = -2;
    iVisFrame = 0;
    iFrame = 1;

    for (TInt texture = 0; texture < aMap->numTex; texture++)
        {
        for (TInt mip = 0; mip < 3; mip++)
            {
            TInt width = aMap->texW[texture] >> mip;
            TInt height = aMap->texH[texture] >> mip;
            if (width < 1) width = 1;
            if (height < 1) height = 1;
            if (mip >= aMap->texMipLength[texture])
                {
                width = aMap->texW[texture];
                height = aMap->texH[texture];
                }
            TInt shift = 0;
            while ((1 << shift) < width) shift++;
            TInt index = texture * 3 + mip;
            iTextureShift[index] = shift;
            iTextureMaskS[index] = width - 1;
            iTextureMaskT[index] = (height - 1) << shift;
            iTextureTShift[index] = 16 - shift;
            }
        }

    BuildSky();

    for (TInt level = 0; level < KShadeLevelCount; level++)
        {
        TReal32 factor = (TReal32)level / 32.0f;
        TReal32 redTint = 0.25f + 0.75f * aMap->tintR;
        TReal32 greenTint = 0.25f + 0.75f * aMap->tintG;
        TReal32 blueTint = 0.25f + 0.75f * aMap->tintB;
        for (TInt color = 0; color < KShadeColorCount; color++)
            {
            TUint32 source = aMap->pal[color];
            TInt red = (TInt)(((TReal32)((source >> 16) & 255)) * factor * redTint);
            TInt green = (TInt)(((TReal32)((source >> 8) & 255)) * factor * greenTint);
            TInt blue = (TInt)(((TReal32)(source & 255)) * factor * blueTint);
            red = ClampInt(red, 0, 255);
            green = ClampInt(green, 0, 255);
            blue = ClampInt(blue, 0, 255);
            iShade[level * KShadeColorCount + color] =
                (TUint16)PackRgb565(iGamma[red], iGamma[green], iGamma[blue]);
            }
        }
    }

void CRenderer::BuildSky()
    {
    for (TInt face = 0; face < 5; face++)
        {
        for (TInt i = 0; i < KSkySide * KSkySide; i++)
            iSkyGamma[face][i] = GammaRgb565(iMap->sky565[face][i], iGamma);
        }

    TInt64 red = 0;
    TInt64 green = 0;
    TInt64 blue = 0;
    TInt count = 0;
    for (TInt face = 0; face < 4; face++)
        {
        for (TInt i = 32 * KSkySide; i < 96 * KSkySide; i += 7)
            {
            TUint16 color = iSkyGamma[face][i];
            red += Red8(color);
            green += Green8(color);
            blue += Blue8(color);
            count++;
            }
        }
    if (count == 0)
        iSkyFlat = (TUint16)PackRgb565(0x80, 0x90, 0xA0);
    else
        iSkyFlat = (TUint16)PackRgb565((TInt)(red / count),
                                       (TInt)(green / count),
                                       (TInt)(blue / count));
    }

void CRenderer::SetCamera(TReal32 aX, TReal32 aY, TReal32 aZ,
                          TReal32 aYawDegrees, TReal32 aPitchDegrees, TReal32 aFovDegrees)
    {
    iEyeX = aX;
    iEyeY = aY;
    iEyeZ = aZ;
    iYaw = aYawDegrees;
    iPitch = aPitchDegrees;
    iFov = aFovDegrees;

    TReal32 cosYaw = FloatCos(aYawDegrees);
    TReal32 sinYaw = FloatSin(aYawDegrees);
    TReal32 cosPitch = FloatCos(aPitchDegrees);
    TReal32 sinPitch = FloatSin(aPitchDegrees);

    iForwardX = cosPitch * cosYaw;
    iForwardY = cosPitch * sinYaw;
    iForwardZ = -sinPitch;
    iRightX = sinYaw;
    iRightY = -cosYaw;
    iRightZ = 0;
    iUpX = sinPitch * cosYaw;
    iUpY = sinPitch * sinYaw;
    iUpZ = cosPitch;

    iCenterX = (TReal32)iWidth * 0.5f;
    iCenterY = (TReal32)iHeight * 0.5f;
    iXScale = iCenterX / FloatTan(aFovDegrees * 0.5f);
    iYScale = iXScale;
    }

void CRenderer::RenderWorld()
    {
    iFrame++;
    iCovered = 0;
    iStatFaces = 0;
    iStatSpans = 0;
    iStatPixels = 0;
    iSkyCount = 0;
    iAlphaCount = 0;
    for (TInt row = 0; row < iHeight; row++) iSegmentCount[row] = 0;

    TInt leaf = iMap->PointLeaf(iEyeX, iEyeY, iEyeZ);
    if (leaf != iViewLeaf)
        {
        iViewLeaf = leaf;
        MarkLeaves(leaf);
        }
    Walk(0, 15);
    if (iCovered < iWidth * iHeight) FillUncovered();
    if (iSkyCount > 0) DrawSky();
    }

void CRenderer::MarkLeaves(TInt aLeaf)
    {
    iVisFrame++;
    TInt offset = iMap->lVisOfs[aLeaf];
    TInt leafCount = iMap->numLeaves;
    if (offset < 0 || iMap->visLength == 0 || iMap->vis == NULL || aLeaf == 0)
        {
        for (TInt i = 0; i < iMap->numLeaves; i++) MarkLeaf(i);
        return;
        }

    const TUint8* visibility = iMap->vis;
    TInt leaf = 1;
    while (leaf < leafCount)
        {
        TInt bits = visibility[offset++] & 255;
        if (bits == 0)
            {
            leaf += 8 * (visibility[offset++] & 255);
            continue;
            }
        for (TInt bit = 0; bit < 8 && leaf < leafCount; bit++, leaf++)
            {
            if ((bits & (1 << bit)) != 0) MarkLeaf(leaf);
            }
        }
    MarkLeaf(aLeaf);
    }

void CRenderer::MarkLeaf(TInt aLeaf)
    {
    if (iLeafVis[aLeaf] == iVisFrame) return;
    iLeafVis[aLeaf] = iVisFrame;
    TInt node = iMap->lParent[aLeaf];
    while (node >= 0 && iNodeVis[node] != iVisFrame)
        {
        iNodeVis[node] = iVisFrame;
        node = iMap->nParent[node];
        }
    }

TInt CRenderer::CullBox(const TInt16* aBox, TInt aOffset, TInt aClip)
    {
    TInt clip = aClip;
    for (TInt plane = 0; plane < 4; plane++)
        {
        if ((clip & (1 << plane)) == 0) continue;
        TReal32 nx = 0;
        TReal32 ny = 0;
        TReal32 nz = 0;
        if (plane == 0)
            {
            nx = iRightX * iXScale + iForwardX * iCenterX;
            ny = iRightY * iXScale + iForwardY * iCenterX;
            nz = iRightZ * iXScale + iForwardZ * iCenterX;
            }
        else if (plane == 1)
            {
            nx = -iRightX * iXScale + iForwardX * (iWidth - iCenterX);
            ny = -iRightY * iXScale + iForwardY * (iWidth - iCenterX);
            nz = -iRightZ * iXScale + iForwardZ * (iWidth - iCenterX);
            }
        else if (plane == 2)
            {
            nx = -iUpX * iYScale + iForwardX * iCenterY;
            ny = -iUpY * iYScale + iForwardY * iCenterY;
            nz = -iUpZ * iYScale + iForwardZ * iCenterY;
            }
        else
            {
            nx = iUpX * iYScale + iForwardX * (iHeight - iCenterY);
            ny = iUpY * iYScale + iForwardY * (iHeight - iCenterY);
            nz = iUpZ * iYScale + iForwardZ * (iHeight - iCenterY);
            }

        TReal32 insideX = nx >= 0 ? aBox[aOffset + 3] : aBox[aOffset];
        TReal32 insideY = ny >= 0 ? aBox[aOffset + 4] : aBox[aOffset + 1];
        TReal32 insideZ = nz >= 0 ? aBox[aOffset + 5] : aBox[aOffset + 2];
        TReal32 distance = nx * (insideX - iEyeX) +
                           ny * (insideY - iEyeY) +
                           nz * (insideZ - iEyeZ);
        if (distance < 0) return -1;

        TReal32 outsideX = nx >= 0 ? aBox[aOffset] : aBox[aOffset + 3];
        TReal32 outsideY = ny >= 0 ? aBox[aOffset + 1] : aBox[aOffset + 4];
        TReal32 outsideZ = nz >= 0 ? aBox[aOffset + 2] : aBox[aOffset + 5];
        if (nx * (outsideX - iEyeX) + ny * (outsideY - iEyeY) +
            nz * (outsideZ - iEyeZ) >= 0)
            clip &= ~(1 << plane);
        }
    return clip;
    }

void CRenderer::Walk(TInt aNode, TInt aClip)
    {
    if (iCovered >= iWidth * iHeight) return;
    if (aNode < 0)
        {
        TInt leaf = -aNode - 1;
        if (iLeafVis[leaf] != iVisFrame) return;
        if (aClip != 0 && CullBox(iMap->lBox, leaf * 6, aClip) < 0) return;
        TInt first = iMap->lFirstMark[leaf] & 0xFFFF;
        TInt count = iMap->lNumMarks[leaf] & 0xFFFF;
        for (TInt i = 0; i < count; i++)
            iFaceFrame[iMap->marks[first + i] & 0xFFFF] = iFrame;
        return;
        }

    if (iNodeVis[aNode] != iVisFrame) return;
    TInt clip = aClip;
    if (clip != 0)
        {
        clip = CullBox(iMap->nBox, aNode * 6, clip);
        if (clip < 0) return;
        }

    TInt plane = iMap->nPlane[aNode];
    TReal32 distance = iMap->plNx[plane] * iEyeX +
                       iMap->plNy[plane] * iEyeY +
                       iMap->plNz[plane] * iEyeZ - iMap->plD[plane];
    TInt front = distance >= 0 ? iMap->nChild0[aNode] : iMap->nChild1[aNode];
    TInt back = distance >= 0 ? iMap->nChild1[aNode] : iMap->nChild0[aNode];

    Walk(front, clip);

    TInt firstFace = iMap->nFirstFace[aNode] & 0xFFFF;
    TInt faceCount = iMap->nNumFaces[aNode] & 0xFFFF;
    for (TInt face = firstFace; face < firstFace + faceCount; face++)
        {
        if (iFaceFrame[face] != iFrame) continue;
        iFaceFrame[face] = 0;
        TBool faceFront = iMap->fSide[face] == 0;
        if ((distance >= 0) != faceFront) continue;
        TInt texture = iMap->fTex[face];
        if (texture < 0) continue;
        TInt flags = iMap->texFlags[texture];
        if ((flags & 6) != 0)
            {
            if (iAlphaCount < KMaxAlphaFaces) iAlphaFaces[iAlphaCount++] = face;
            continue;
            }
        DrawFace(face, 0, 0, 0, (flags & 1) != 0 ? 1 : 0);
        if (iCovered >= iWidth * iHeight) return;
        }
    Walk(back, clip);
    }

void CRenderer::RenderWorldAlpha()
    {
    for (TInt i = 0; i < iAlphaCount; i++)
        DrawFace(iAlphaFaces[i], 0, 0, 0, 2);
    }

void CRenderer::RenderBrushModel(TInt aModel, TReal32 aOffsetX, TReal32 aOffsetY,
                                 TReal32 aOffsetZ, TBool aMasked)
    {
    TInt firstFace = iMap->mFirstFace[aModel];
    TInt faceCount = iMap->mNumFaces[aModel];

    TReal32 minimumX = iMap->mMins[aModel * 3] + aOffsetX;
    TReal32 minimumY = iMap->mMins[aModel * 3 + 1] + aOffsetY;
    TReal32 minimumZ = iMap->mMins[aModel * 3 + 2] + aOffsetZ;
    TReal32 maximumX = iMap->mMaxs[aModel * 3] + aOffsetX;
    TReal32 maximumY = iMap->mMaxs[aModel * 3 + 1] + aOffsetY;
    TReal32 maximumZ = iMap->mMaxs[aModel * 3 + 2] + aOffsetZ;
    TReal32 centerX = (minimumX + maximumX) * 0.5f - iEyeX;
    TReal32 centerY = (minimumY + maximumY) * 0.5f - iEyeY;
    TReal32 centerZ = (minimumZ + maximumZ) * 0.5f - iEyeZ;
    TReal32 radius = (maximumX - minimumX + maximumY - minimumY +
                      maximumZ - minimumZ) * 0.5f;
    TReal32 viewZ = centerX * iForwardX + centerY * iForwardY + centerZ * iForwardZ;
    if (viewZ < -radius) return;
    TReal32 viewX = centerX * iRightX + centerY * iRightY + centerZ * iRightZ;
    if (AbsReal(viewX) - radius > (viewZ + radius) * iCenterX / iXScale + 1) return;

    for (TInt face = firstFace; face < firstFace + faceCount; face++)
        {
        TInt texture = iMap->fTex[face];
        if (texture < 0) continue;
        TInt plane = iMap->fPlane[face] & 0xFFFF;
        TReal32 distance = iMap->plNx[plane] * (iEyeX - aOffsetX) +
                           iMap->plNy[plane] * (iEyeY - aOffsetY) +
                           iMap->plNz[plane] * (iEyeZ - aOffsetZ) - iMap->plD[plane];
        if ((distance >= 0) != (iMap->fSide[face] == 0)) continue;
        DrawFace(face, aOffsetX, aOffsetY, aOffsetZ, 2);
        }
    (void)aMasked;
    }

void CRenderer::DrawFace(TInt aFace, TReal32 aOffsetX, TReal32 aOffsetY,
                         TReal32 aOffsetZ, TInt aMode)
    {
    TInt vertexCount = iMap->fNumV[aFace];
    TInt firstVertex = iMap->fFirstV[aFace];
    if (vertexCount > 60) return;

    TReal32 minimumZ = 1e9f;
    TInt count = 0;
    for (TInt i = 0; i < vertexCount; i++)
        {
        TInt vertex = iMap->fvert[firstVertex + i] & 0xFFFF;
        TReal32 x = 0;
        TReal32 y = 0;
        TReal32 z = 0;
        if (aOffsetX == 0 && aOffsetY == 0 && aOffsetZ == 0)
            {
            if (iVertexFrame[vertex] != iFrame)
                {
                TReal32 deltaX = iMap->vx[vertex] - iEyeX;
                TReal32 deltaY = iMap->vy[vertex] - iEyeY;
                TReal32 deltaZ = iMap->vz[vertex] - iEyeZ;
                iTransformedX[vertex] = deltaX * iRightX + deltaY * iRightY + deltaZ * iRightZ;
                iTransformedY[vertex] = deltaX * iUpX + deltaY * iUpY + deltaZ * iUpZ;
                iTransformedZ[vertex] = deltaX * iForwardX + deltaY * iForwardY + deltaZ * iForwardZ;
                iVertexFrame[vertex] = iFrame;
                }
            x = iTransformedX[vertex];
            y = iTransformedY[vertex];
            z = iTransformedZ[vertex];
            }
        else
            {
            TReal32 deltaX = iMap->vx[vertex] + aOffsetX - iEyeX;
            TReal32 deltaY = iMap->vy[vertex] + aOffsetY - iEyeY;
            TReal32 deltaZ = iMap->vz[vertex] + aOffsetZ - iEyeZ;
            x = deltaX * iRightX + deltaY * iRightY + deltaZ * iRightZ;
            y = deltaX * iUpX + deltaY * iUpY + deltaZ * iUpZ;
            z = deltaX * iForwardX + deltaY * iForwardY + deltaZ * iForwardZ;
            }
        iClipAX[count] = x;
        iClipAY[count] = y;
        iClipAZ[count] = z;
        count++;
        }

    count = ClipPlane(count, 0); if (count < 3) return;
    count = ClipPlane(count, 1); if (count < 3) return;
    count = ClipPlane(count, 2); if (count < 3) return;
    count = ClipPlane(count, 3); if (count < 3) return;
    count = ClipPlane(count, 4); if (count < 3) return;

    TInt rowMinimum = iHeight;
    TInt rowMaximum = -1;
    for (TInt i = 0; i < count; i++)
        {
        TReal32 inverseZ = 1.0f / iClipAZ[i];
        iProjectedSX[i] = iCenterX + iClipAX[i] * iXScale * inverseZ;
        iProjectedSY[i] = iCenterY - iClipAY[i] * iYScale * inverseZ;
        if (iClipAZ[i] < minimumZ) minimumZ = iClipAZ[i];
        }

    iRowStamp++;
    TInt stamp = iRowStamp;
    for (TInt i = 0; i < count; i++)
        {
        TInt next = i + 1 == count ? 0 : i + 1;
        TReal32 top = iProjectedSY[i];
        TReal32 bottom = iProjectedSY[next];
        TReal32 left = iProjectedSX[i];
        TReal32 right = iProjectedSX[next];
        if (top == bottom) continue;
        if (top > bottom)
            {
            TReal32 temporary = top; top = bottom; bottom = temporary;
            temporary = left; left = right; right = temporary;
            }
        TInt firstRow = CeilHalf(top);
        TInt lastRow = CeilHalf(bottom) - 1;
        if (firstRow < 0) firstRow = 0;
        if (lastRow > iHeight - 1) lastRow = iHeight - 1;
        if (firstRow > lastRow) continue;

        TReal32 slope = (right - left) / (bottom - top);
        TReal32 x = left + ((TReal32)firstRow + 0.5f - top) * slope;
        if (firstRow < rowMinimum) rowMinimum = firstRow;
        if (lastRow > rowMaximum) rowMaximum = lastRow;
        for (TInt row = firstRow; row <= lastRow; row++, x += slope)
            {
            if (iRowFrame[row] != stamp)
                {
                iRowFrame[row] = stamp;
                iRowMin[row] = x;
                iRowMax[row] = x;
                }
            else
                {
                if (x < iRowMin[row]) iRowMin[row] = x;
                if (x > iRowMax[row]) iRowMax[row] = x;
                }
            }
        }
    if (rowMaximum < rowMinimum) return;
    iStatFaces++;

    if (aMode == 1)
        {
        for (TInt row = rowMinimum; row <= rowMaximum; row++)
            {
            TInt left = CeilHalf(iRowMin[row]);
            TInt right = CeilHalf(iRowMax[row]);
            if (left < 0) left = 0;
            if (right > iWidth) right = iWidth;
            if (left < right) SpanInsert(row, left, right, ETrue);
            }
        return;
        }

    SetupGradients(aFace, aOffsetX, aOffsetY, aOffsetZ, minimumZ);
    for (TInt row = rowMinimum; row <= rowMaximum; row++)
        {
        TInt left = CeilHalf(iRowMin[row]);
        TInt right = CeilHalf(iRowMax[row]);
        if (left < 0) left = 0;
        if (right > iWidth) right = iWidth;
        if (left >= right) continue;
        if (aMode == 0) SpanInsert(row, left, right, EFalse);
        else DrawSpanZ(row, left, right);
        }
    }

TInt CRenderer::ClipPlane(TInt aCount, TInt aPlane)
    {
    TInt out = 0;
    TBool anyOutside = EFalse;
    for (TInt i = 0; i < aCount; i++)
        {
        if (PlaneDistance(aPlane, iClipAX[i], iClipAY[i], iClipAZ[i]) < 0)
            {
            anyOutside = ETrue;
            break;
            }
        }
    if (!anyOutside) return aCount;

    for (TInt i = 0; i < aCount; i++)
        {
        TInt next = i + 1 == aCount ? 0 : i + 1;
        TReal32 firstDistance = PlaneDistance(aPlane, iClipAX[i], iClipAY[i], iClipAZ[i]);
        TReal32 secondDistance = PlaneDistance(aPlane, iClipAX[next], iClipAY[next], iClipAZ[next]);
        if (firstDistance >= 0)
            {
            iClipBX[out] = iClipAX[i];
            iClipBY[out] = iClipAY[i];
            iClipBZ[out] = iClipAZ[i];
            out++;
            }
        if ((firstDistance >= 0) != (secondDistance >= 0))
            {
            TReal32 fraction = firstDistance / (firstDistance - secondDistance);
            iClipBX[out] = iClipAX[i] + (iClipAX[next] - iClipAX[i]) * fraction;
            iClipBY[out] = iClipAY[i] + (iClipAY[next] - iClipAY[i]) * fraction;
            iClipBZ[out] = iClipAZ[i] + (iClipAZ[next] - iClipAZ[i]) * fraction;
            out++;
            }
        if (out >= 62) break;
        }
    for (TInt i = 0; i < out; i++)
        {
        iClipAX[i] = iClipBX[i];
        iClipAY[i] = iClipBY[i];
        iClipAZ[i] = iClipBZ[i];
        }
    return out;
    }

TReal32 CRenderer::PlaneDistance(TInt aPlane, TReal32 aX, TReal32 aY, TReal32 aZ)
    {
    switch (aPlane)
        {
        case 0:  return aZ - KNearPlane;
        case 1:  return aX * iXScale + aZ * iCenterX + 0.01f;
        case 2:  return -aX * iXScale + aZ * (iWidth - iCenterX) + 0.01f;
        case 3:  return -aY * iYScale + aZ * iCenterY + 0.01f;
        default: return aY * iYScale + aZ * (iHeight - iCenterY) + 0.01f;
        }
    }

void CRenderer::SetupGradients(TInt aFace, TReal32 aOffsetX, TReal32 aOffsetY,
                               TReal32 aOffsetZ, TReal32 aMinimumZ)
    {
    TInt plane = iMap->fPlane[aFace] & 0xFFFF;
    TReal32 normalX = iMap->plNx[plane];
    TReal32 normalY = iMap->plNy[plane];
    TReal32 normalZ = iMap->plNz[plane];
    TReal32 distance = iMap->plD[plane] +
                       normalX * aOffsetX + normalY * aOffsetY + normalZ * aOffsetZ -
                       (normalX * iEyeX + normalY * iEyeY + normalZ * iEyeZ);
    if (distance > -0.01f && distance < 0.01f)
        distance = distance < 0 ? -0.01f : 0.01f;
    TReal32 inverseDistance = 1.0f / distance;
    TReal32 gradientX = (normalX * iRightX + normalY * iRightY + normalZ * iRightZ) *
                        inverseDistance / iXScale;
    TReal32 gradientY = -(normalX * iUpX + normalY * iUpY + normalZ * iUpZ) *
                        inverseDistance / iYScale;
    TReal32 gradient0 = (normalX * iForwardX + normalY * iForwardY + normalZ * iForwardZ) *
                        inverseDistance;
    iGradientZx = gradientX;
    iGradientZy = gradientY;
    iGradientZ0 = gradient0 - gradientX * iCenterX - gradientY * iCenterY;

    TInt texture = iMap->fTex[aFace];
    TInt offset = aFace * 4;

    TReal32 sLengthX = iMap->fS[offset];
    TReal32 sLengthY = iMap->fS[offset + 1];
    TReal32 sLengthZ = iMap->fS[offset + 2];
    TReal32 sLength = FloatSqrt(sLengthX * sLengthX +
                                sLengthY * sLengthY +
                                sLengthZ * sLengthZ);
    TReal32 texelsPerPixel = sLength * aMinimumZ / iXScale * iMipScale;
    TInt mip = texelsPerPixel < 1.6f ? 0 : (texelsPerPixel < 3.2f ? 1 : 2);
    mip += iMipBias;
    if (mip > 2) mip = 2;
    if (mip < 0) mip = 0;
    if (mip >= iMap->texMipLength[texture]) mip = iMap->texMipLength[texture] - 1;
    if (mip < 0) mip = 0;
    TReal32 mipScale = 1.0f / (TReal32)(1 << mip);

    iGradientTexture = iMap->texMip[texture * 3 + mip];
    TInt index = texture * 3 + mip;
    iGradientTextureShift = iTextureTShift[index];
    iGradientTextureMaskT = iTextureMaskT[index];
    iGradientTextureMaskS = iTextureMaskS[index];
    iGradientMasked = (iMap->texFlags[texture] & 2) != 0;

    /* Gradient записывает результат в iGradientSx/Sy/S0. Для S это сразу
       нужные градиенты, остальные копируются до следующего вызова. */
    Gradient(iMap->fS, offset, mipScale, aOffsetX, aOffsetY, aOffsetZ);
    Gradient(iMap->fT, offset, mipScale, aOffsetX, aOffsetY, aOffsetZ);
    iGradientTx = iGradientSx;
    iGradientTy = iGradientSy;
    iGradientT0 = iGradientS0;

    gGradientLightOffset = iMap->fLmOfs[aFace];
    if (gGradientLightOffset >= 0 && !iFullBright)
        {
        iGradientLightWidth = iMap->fLmW[aFace] & 255;
        iGradientLightHeight = iMap->fLmH[aFace] & 255;
        Gradient(iMap->fLS, offset, 1.0f, aOffsetX, aOffsetY, aOffsetZ);
        iGradientLx = iGradientSx;
        iGradientLy = iGradientSy;
        iGradientL0 = iGradientS0;
        Gradient(iMap->fLT, offset, 1.0f, aOffsetX, aOffsetY, aOffsetZ);
        iGradientMx = iGradientSx;
        iGradientMy = iGradientSy;
        iGradientM0 = iGradientS0;
        }
    else
        {
        gGradientLightOffset = -1;
        iGradientConstLevel = iFullBright ? 32 : 40;
        }
    }

void CRenderer::Gradient(const TReal32* aVertices, TInt aOffset, TReal32 aScale,
                         TReal32 aOffsetX, TReal32 aOffsetY, TReal32 aOffsetZ)
    {
    TReal32 scaledX = aVertices[aOffset] * aScale;
    TReal32 scaledY = aVertices[aOffset + 1] * aScale;
    TReal32 scaledZ = aVertices[aOffset + 2] * aScale;
    TReal32 constant = aVertices[aOffset + 3] * aScale +
                       scaledX * (iEyeX - aOffsetX) +
                       scaledY * (iEyeY - aOffsetY) +
                       scaledZ * (iEyeZ - aOffsetZ);
    TReal32 slopeX = (scaledX * iRightX + scaledY * iRightY + scaledZ * iRightZ) / iXScale;
    TReal32 slopeY = -(scaledX * iUpX + scaledY * iUpY + scaledZ * iUpZ) / iYScale;
    TReal32 origin = (scaledX * iForwardX + scaledY * iForwardY + scaledZ * iForwardZ) -
                     slopeX * iCenterX - slopeY * iCenterY;
    iGradientSx = slopeX + constant * iGradientZx;
    iGradientSy = slopeY + constant * iGradientZy;
    iGradientS0 = origin + constant * iGradientZ0;
    }

TInt CRenderer::LightSample(TReal32 aLightS, TReal32 aLightT)
    {
    TReal32 maximumS = (TReal32)(iGradientLightWidth - 1);
    TReal32 maximumT = (TReal32)(iGradientLightHeight - 1);
    if (aLightS < 0) aLightS = 0;
    else if (aLightS > maximumS) aLightS = maximumS;
    if (aLightT < 0) aLightT = 0;
    else if (aLightT > maximumT) aLightT = maximumT;

    TInt s0 = (TInt)aLightS;
    TInt t0 = (TInt)aLightT;
    TInt fractionS = (TInt)((aLightS - (TReal32)s0) * 256.0f);
    TInt fractionT = (TInt)((aLightT - (TReal32)t0) * 256.0f);
    TInt s1 = s0 < iGradientLightWidth - 1 ? s0 + 1 : s0;
    TInt t1 = t0 < iGradientLightHeight - 1 ? t0 + 1 : t0;

    const TUint8* light = iMap->light;
    TInt base = gGradientLightOffset;
    TInt first00 = light[base + t0 * iGradientLightWidth + s0] & 255;
    TInt first10 = light[base + t0 * iGradientLightWidth + s1] & 255;
    TInt first01 = light[base + t1 * iGradientLightWidth + s0] & 255;
    TInt first11 = light[base + t1 * iGradientLightWidth + s1] & 255;
    TInt top = (first00 << 8) + (first10 - first00) * fractionS;
    TInt bottom = (first01 << 8) + (first11 - first01) * fractionS;
    return (top << 8) + (bottom - top) * fractionT;
    }

void CRenderer::SpanInsert(TInt aY, TInt aX0, TInt aX1, TBool aSky)
    {
    TInt base = aY * KMaxSegments;
    TInt count = iSegmentCount[aY];
    TInt i = 0;
    while (i < count && iSegmentB[base + i] <= aX0) i++;
    TInt first = i;
    TInt current = aX0;
    while (current < aX1)
        {
        if (i < count && iSegmentA[base + i] <= current)
            {
            if (iSegmentB[base + i] > current) current = iSegmentB[base + i];
            i++;
            continue;
            }
        TInt end = i < count ? iSegmentA[base + i] : aX1;
        if (end > aX1) end = aX1;
        if (aSky) AddSky(aY, current, end);
        else DrawSpan(aY, current, end);
        iCovered += end - current;
        current = end;
        }

    TInt mergedStart = aX0;
    TInt mergedEnd = aX1;
    while (first > 0 && iSegmentB[base + first - 1] >= mergedStart) first--;
    TInt last = first;
    while (last < count && iSegmentA[base + last] <= mergedEnd)
        {
        if (iSegmentA[base + last] < mergedStart) mergedStart = iSegmentA[base + last];
        if (iSegmentB[base + last] > mergedEnd) mergedEnd = iSegmentB[base + last];
        last++;
        }
    TInt removed = last - first;
    if (removed == 0)
        {
        if (count >= KMaxSegments)
            {
            /* Переполнение: расширяем ближайшего соседа, зазор остаётся как есть. */
            if (first >= count) iSegmentB[base + count - 1] = (TInt16)mergedEnd;
            else if (first == 0) iSegmentA[base] = (TInt16)mergedStart;
            else if (mergedStart - iSegmentB[base + first - 1] < iSegmentA[base + first] - mergedEnd)
                iSegmentB[base + first - 1] = (TInt16)mergedEnd;
            else
                iSegmentA[base + first] = (TInt16)mergedStart;
            return;
            }
        for (TInt segment = count - 1; segment >= first; segment--)
            {
            iSegmentA[base + segment + 1] = iSegmentA[base + segment];
            iSegmentB[base + segment + 1] = iSegmentB[base + segment];
            }
        iSegmentA[base + first] = (TInt16)mergedStart;
        iSegmentB[base + first] = (TInt16)mergedEnd;
        iSegmentCount[aY] = (TUint8)(count + 1);
        }
    else
        {
        iSegmentA[base + first] = (TInt16)mergedStart;
        iSegmentB[base + first] = (TInt16)mergedEnd;
        if (removed > 1)
            {
            for (TInt segment = 0; segment < count - last; segment++)
                {
                iSegmentA[base + first + 1 + segment] = iSegmentA[base + last + segment];
                iSegmentB[base + first + 1 + segment] = iSegmentB[base + last + segment];
                }
            iSegmentCount[aY] = (TUint8)(count - removed + 1);
            }
        }
    }

void CRenderer::AddSky(TInt aY, TInt aX0, TInt aX1)
    {
    TInt offset = aY * iWidth;
    for (TInt x = aX0; x < aX1; x++) iZBuffer[offset + x] = 0;

    /* Массив sky-отрезков фиксирован. В обычном кадре их не больше высоты,
       поэтому переполнение возможно только при длинных разрывах покрытия. */
    if (iSkyCount >= KMaxSkySpans) return;
    iSkyY[iSkyCount] = (TInt16)aY;
    iSkyX0[iSkyCount] = (TInt16)aX0;
    iSkyX1[iSkyCount] = (TInt16)aX1;
    iSkyCount++;
    }

void CRenderer::FillUncovered()
    {
    for (TInt row = 0; row < iHeight; row++)
        {
        TInt base = row * KMaxSegments;
        TInt count = iSegmentCount[row];
        TInt current = 0;
        for (TInt i = 0; i <= count; i++)
            {
            TInt end = i < count ? iSegmentA[base + i] : iWidth;
            if (end > current) AddSky(row, current, end);
            if (i < count) current = iSegmentB[base + i];
            }
        }
    }

void CRenderer::DrawSpan(TInt aY, TInt aX0, TInt aX1)
    {
    iStatSpans++;
    iStatPixels += aX1 - aX0;
    if (iBilinear)
        {
        DrawSpanBilinear(aY, aX0, aX1);
        return;
        }

    TReal32 screenY = (TReal32)aY + 0.5f;
    TReal32 screenX = (TReal32)aX0 + 0.5f;
    TReal32 inverseZ = iGradientZ0 + iGradientZx * screenX + iGradientZy * screenY;
    TReal32 numeratorS = iGradientS0 + iGradientSx * screenX + iGradientSy * screenY;
    TReal32 numeratorT = iGradientT0 + iGradientTx * screenX + iGradientTy * screenY;
    TReal32 numeratorL = 0;
    TReal32 numeratorM = 0;
    TBool lit = gGradientLightOffset >= 0;
    if (lit)
        {
        numeratorL = iGradientL0 + iGradientLx * screenX + iGradientLy * screenY;
        numeratorM = iGradientM0 + iGradientMx * screenX + iGradientMy * screenY;
        }

    TInt textureShift = iGradientTextureShift;
    TInt textureMaskT = iGradientTextureMaskT;
    TInt textureMaskS = iGradientTextureMaskS;
    TInt pixel = aY * iWidth + aX0;
    TInt remaining = aX1 - aX0;
    TReal32 reciprocalZ = 1.0f / inverseZ;
    TFixed s = FxFromReal(numeratorS * reciprocalZ);
    TFixed t = FxFromReal(numeratorT * reciprocalZ);
    TFixed light = lit ? LightSample(numeratorL * reciprocalZ, numeratorM * reciprocalZ)
                       : (TFixed)(iGradientConstLevel << KFixedShift);
    TInt depth = (TInt)(inverseZ * KDepthScale);
    TInt depthStep = (TInt)(iGradientZx * KDepthScale);
    TInt step = 1 << iStepShift;

    while (remaining > 0)
        {
        TInt count = remaining > step ? step : remaining;
        inverseZ += iGradientZx * count;
        numeratorS += iGradientSx * count;
        numeratorT += iGradientTx * count;
        TReal32 nextReciprocalZ = 1.0f / inverseZ;
        TFixed nextS = FxFromReal(numeratorS * nextReciprocalZ);
        TFixed nextT = FxFromReal(numeratorT * nextReciprocalZ);
        TFixed nextLight = light;
        if (lit)
            {
            numeratorL += iGradientLx * count;
            numeratorM += iGradientMx * count;
            nextLight = LightSample(numeratorL * nextReciprocalZ,
                                    numeratorM * nextReciprocalZ);
            }
        TFixed stepS;
        TFixed stepT;
        TFixed stepLight;
        if (count == step)
            {
            stepS = (nextS - s) >> iStepShift;
            stepT = (nextT - t) >> iStepShift;
            stepLight = (nextLight - light) >> iStepShift;
            }
        else
            {
            stepS = (nextS - s) / count;
            stepT = (nextT - t) / count;
            stepLight = (nextLight - light) / count;
            }
        for (TInt k = count; k > 0; k--)
            {
            TInt texel = iGradientTexture[((t >> textureShift) & textureMaskT) |
                                          ((s >> KFixedShift) & textureMaskS)] & 255;
            iFrameBuffer[pixel] = iShade[((light >> 8) & 0x3F00) | texel];
            iZBuffer[pixel] = depth;
            pixel++;
            s += stepS;
            t += stepT;
            light += stepLight;
            depth += depthStep;
            }
        s = nextS;
        t = nextT;
        light = nextLight;
        remaining -= count;
        }
    }

void CRenderer::DrawSpanBilinear(TInt aY, TInt aX0, TInt aX1)
    {
    TReal32 screenY = (TReal32)aY + 0.5f;
    TReal32 screenX = (TReal32)aX0 + 0.5f;
    TReal32 inverseZ = iGradientZ0 + iGradientZx * screenX + iGradientZy * screenY;
    TReal32 numeratorS = iGradientS0 + iGradientSx * screenX + iGradientSy * screenY;
    TReal32 numeratorT = iGradientT0 + iGradientTx * screenX + iGradientTy * screenY;
    TReal32 numeratorL = 0;
    TReal32 numeratorM = 0;
    TBool lit = gGradientLightOffset >= 0;
    if (lit)
        {
        numeratorL = iGradientL0 + iGradientLx * screenX + iGradientLy * screenY;
        numeratorM = iGradientM0 + iGradientMx * screenX + iGradientMy * screenY;
        }

    TInt textureShift = iGradientTextureShift;
    TInt textureMaskT = iGradientTextureMaskT;
    TInt textureMaskS = iGradientTextureMaskS;
    TInt pixel = aY * iWidth + aX0;
    TInt remaining = aX1 - aX0;
    TReal32 reciprocalZ = 1.0f / inverseZ;
    TFixed s = FxFromReal(numeratorS * reciprocalZ) - KFixedOne / 2;
    TFixed t = FxFromReal(numeratorT * reciprocalZ) - KFixedOne / 2;
    TFixed light = lit ? LightSample(numeratorL * reciprocalZ, numeratorM * reciprocalZ)
                       : (TFixed)(iGradientConstLevel << KFixedShift);
    TInt depth = (TInt)(inverseZ * KDepthScale);
    TInt depthStep = (TInt)(iGradientZx * KDepthScale);
    TInt step = 1 << iStepShift;

    while (remaining > 0)
        {
        TInt count = remaining > step ? step : remaining;
        inverseZ += iGradientZx * count;
        numeratorS += iGradientSx * count;
        numeratorT += iGradientTx * count;
        TReal32 nextReciprocalZ = 1.0f / inverseZ;
        TFixed nextS = FxFromReal(numeratorS * nextReciprocalZ) - KFixedOne / 2;
        TFixed nextT = FxFromReal(numeratorT * nextReciprocalZ) - KFixedOne / 2;
        TFixed nextLight = light;
        if (lit)
            {
            numeratorL += iGradientLx * count;
            numeratorM += iGradientMx * count;
            nextLight = LightSample(numeratorL * nextReciprocalZ,
                                    numeratorM * nextReciprocalZ);
            }
        TFixed stepS;
        TFixed stepT;
        TFixed stepLight;
        if (count == step)
            {
            stepS = (nextS - s) >> iStepShift;
            stepT = (nextT - t) >> iStepShift;
            stepLight = (nextLight - light) >> iStepShift;
            }
        else
            {
            stepS = (nextS - s) / count;
            stepT = (nextT - t) / count;
            stepLight = (nextLight - light) / count;
            }
        for (TInt k = count; k > 0; k--)
            {
            TInt level = (light >> 8) & 0x3F00;
            TInt sampleS = (s >> KFixedShift) & textureMaskS;
            TInt sampleSNext = (sampleS + 1) & textureMaskS;
            TInt row0 = (t >> textureShift) & textureMaskT;
            TInt row1 = ((t + KFixedOne) >> textureShift) & textureMaskT;
            TInt fractionU = (s >> 8) & 255;
            TInt fractionV = (t >> 8) & 255;
            TInt inverseU = 256 - fractionU;
            TInt inverseV = 256 - fractionV;
            TUint16 color00 = iShade[level | (iGradientTexture[row0 | sampleS] & 255)];
            TUint16 color10 = iShade[level | (iGradientTexture[row0 | sampleSNext] & 255)];
            TUint16 color01 = iShade[level | (iGradientTexture[row1 | sampleS] & 255)];
            TUint16 color11 = iShade[level | (iGradientTexture[row1 | sampleSNext] & 255)];

            TInt redTop = (Red565(color00) * inverseU + Red565(color10) * fractionU) >> 8;
            TInt greenTop = (Green565(color00) * inverseU + Green565(color10) * fractionU) >> 8;
            TInt blueTop = (Blue565(color00) * inverseU + Blue565(color10) * fractionU) >> 8;
            TInt redBottom = (Red565(color01) * inverseU + Red565(color11) * fractionU) >> 8;
            TInt greenBottom = (Green565(color01) * inverseU + Green565(color11) * fractionU) >> 8;
            TInt blueBottom = (Blue565(color01) * inverseU + Blue565(color11) * fractionU) >> 8;
            TInt red = (redTop * inverseV + redBottom * fractionV) >> 8;
            TInt green = (greenTop * inverseV + greenBottom * fractionV) >> 8;
            TInt blue = (blueTop * inverseV + blueBottom * fractionV) >> 8;

            iFrameBuffer[pixel] = (TUint16)PackRgb565(red, green, blue);
            iZBuffer[pixel] = depth;
            pixel++;
            s += stepS;
            t += stepT;
            light += stepLight;
            depth += depthStep;
            }
        s = nextS;
        t = nextT;
        light = nextLight;
        remaining -= count;
        }
    }

void CRenderer::DrawSpanZ(TInt aY, TInt aX0, TInt aX1)
    {
    TReal32 screenY = (TReal32)aY + 0.5f;
    TReal32 screenX = (TReal32)aX0 + 0.5f;
    TReal32 inverseZ = iGradientZ0 + iGradientZx * screenX + iGradientZy * screenY;
    TReal32 numeratorS = iGradientS0 + iGradientSx * screenX + iGradientSy * screenY;
    TReal32 numeratorT = iGradientT0 + iGradientTx * screenX + iGradientTy * screenY;
    TReal32 numeratorL = 0;
    TReal32 numeratorM = 0;
    TBool lit = gGradientLightOffset >= 0;
    if (lit)
        {
        numeratorL = iGradientL0 + iGradientLx * screenX + iGradientLy * screenY;
        numeratorM = iGradientM0 + iGradientMx * screenX + iGradientMy * screenY;
        }

    TInt textureShift = iGradientTextureShift;
    TInt textureMaskT = iGradientTextureMaskT;
    TInt textureMaskS = iGradientTextureMaskS;
    TBool masked = iGradientMasked;
    TInt pixel = aY * iWidth + aX0;
    TInt remaining = aX1 - aX0;
    TReal32 reciprocalZ = 1.0f / inverseZ;
    TFixed s = FxFromReal(numeratorS * reciprocalZ);
    TFixed t = FxFromReal(numeratorT * reciprocalZ);
    TFixed light = lit ? LightSample(numeratorL * reciprocalZ, numeratorM * reciprocalZ)
                       : (TFixed)(iGradientConstLevel << KFixedShift);
    TInt depth = (TInt)(inverseZ * KDepthScale);
    TInt depthStep = (TInt)(iGradientZx * KDepthScale);
    TInt step = 1 << iStepShift;

    while (remaining > 0)
        {
        TInt count = remaining > step ? step : remaining;
        inverseZ += iGradientZx * count;
        numeratorS += iGradientSx * count;
        numeratorT += iGradientTx * count;
        TReal32 nextReciprocalZ = 1.0f / inverseZ;
        TFixed nextS = FxFromReal(numeratorS * nextReciprocalZ);
        TFixed nextT = FxFromReal(numeratorT * nextReciprocalZ);
        TFixed nextLight = light;
        if (lit)
            {
            numeratorL += iGradientLx * count;
            numeratorM += iGradientMx * count;
            nextLight = LightSample(numeratorL * nextReciprocalZ,
                                    numeratorM * nextReciprocalZ);
            }
        TFixed stepS = (nextS - s) / count;
        TFixed stepT = (nextT - t) / count;
        TFixed stepLight = (nextLight - light) / count;
        for (TInt k = count; k > 0; k--)
            {
            if (depth > iZBuffer[pixel])
                {
                TInt texel = iGradientTexture[((t >> textureShift) & textureMaskT) |
                                              ((s >> KFixedShift) & textureMaskS)] & 255;
                if (!masked || texel != 255)
                    {
                    iFrameBuffer[pixel] = iShade[((light >> 8) & 0x3F00) | texel];
                    iZBuffer[pixel] = depth;
                    }
                }
            pixel++;
            s += stepS;
            t += stepT;
            light += stepLight;
            depth += depthStep;
            }
        s = nextS;
        t = nextT;
        light = nextLight;
        remaining -= count;
        }
    }

void CRenderer::DrawSky()
    {
    if (iFlatSky)
        {
        for (TInt i = 0; i < iSkyCount; i++)
            {
            TInt offset = iSkyY[i] * iWidth;
            TInt end = iSkyX1[i];
            for (TInt x = iSkyX0[i]; x < end; x++) iFrameBuffer[offset + x] = iSkyFlat;
            }
        return;
        }

    for (TInt i = 0; i < iSkyCount; i++)
        {
        TInt row = iSkyY[i];
        TInt start = iSkyX0[i];
        TInt end = iSkyX1[i];
        TReal32 vertical = (iCenterY - ((TReal32)row + 0.5f)) / iYScale;
        TReal32 baseX = iForwardX + iUpX * vertical;
        TReal32 baseY = iForwardY + iUpY * vertical;
        TReal32 baseZ = iForwardZ + iUpZ * vertical;
        TReal32 stepX = iRightX / iXScale;
        TReal32 stepY = iRightY / iXScale;
        TReal32 stepZ = iRightZ / iXScale;
        TInt pixel = row * iWidth + start;
        for (TInt x = start; x < end; x++)
            {
            TReal32 horizontal = ((TReal32)x + 0.5f) - iCenterX;
            TReal32 directionX = baseX + stepX * horizontal;
            TReal32 directionY = baseY + stepY * horizontal;
            TReal32 directionZ = baseZ + stepZ * horizontal;
            TReal32 absoluteX = AbsReal(directionX);
            TReal32 absoluteY = AbsReal(directionY);
            TReal32 absoluteZ = AbsReal(directionZ);
            TInt face = 0;
            TReal32 sampleS = 0;
            TReal32 sampleT = 0;
            if (absoluteZ >= absoluteX && absoluteZ >= absoluteY)
                {
                if (directionZ > 0)
                    {
                    face = 4;
                    TReal32 inverse = 1.0f / absoluteZ;
                    sampleS = -directionY * inverse;
                    sampleT = -directionX * inverse;
                    }
                else
                    {
                    iFrameBuffer[pixel++] = iSkyGamma[3][127 * KSkySide + 64];
                    continue;
                    }
                }
            else if (absoluteX >= absoluteY)
                {
                TReal32 inverse = 1.0f / absoluteX;
                if (directionX > 0)
                    {
                    face = 0;
                    sampleS = -directionY * inverse;
                    sampleT = directionZ * inverse;
                    }
                else
                    {
                    face = 1;
                    sampleS = directionY * inverse;
                    sampleT = directionZ * inverse;
                    }
                }
            else
                {
                TReal32 inverse = 1.0f / absoluteY;
                if (directionY > 0)
                    {
                    face = 2;
                    sampleS = directionX * inverse;
                    sampleT = directionZ * inverse;
                    }
                else
                    {
                    face = 3;
                    sampleS = -directionX * inverse;
                    sampleT = directionZ * inverse;
                    }
                }

            TInt skyX = (TInt)((sampleS + 1) * 63.99f);
            TInt skyY = (TInt)((1 - sampleT) * 63.99f);
            skyX = ClampInt(skyX, 0, 127);
            skyY = ClampInt(skyY, 0, 127);
            iFrameBuffer[pixel++] = iSkyGamma[face][(skyY << 7) | skyX];
            }
        }
    }

void CRenderer::DrawSprite(const TUint8* aPixels, TInt aSpriteWidth, TInt aSpriteHeight,
                           const TUint32* aPalette, TReal32 aWorldX, TReal32 aWorldY,
                           TReal32 aWorldZ, TReal32 aAnchorX, TReal32 aAnchorY,
                           TReal32 aPixelsPerUnit)
    {
    if (!aPixels || !aPalette || aPixelsPerUnit <= 0) return;
    TReal32 deltaX = aWorldX - iEyeX;
    TReal32 deltaY = aWorldY - iEyeY;
    TReal32 deltaZ = aWorldZ - iEyeZ;
    TReal32 depth = deltaX * iForwardX + deltaY * iForwardY + deltaZ * iForwardZ;
    if (depth < 22) return;
    TReal32 viewX = deltaX * iRightX + deltaY * iRightY + deltaZ * iRightZ;
    TReal32 viewY = deltaX * iUpX + deltaY * iUpY + deltaZ * iUpZ;
    TReal32 inverseDepth = 1.0f / depth;
    TReal32 screenX = iCenterX + viewX * iXScale * inverseDepth;
    TReal32 screenY = iCenterY - viewY * iYScale * inverseDepth;
    TReal32 scale = iXScale * inverseDepth / aPixelsPerUnit;
    TReal32 left = screenX - aAnchorX * scale;
    TReal32 top = screenY - aAnchorY * scale;

    TInt firstX = CeilHalf(left);
    TInt firstY = CeilHalf(top);
    TInt lastX = CeilHalf(left + aSpriteWidth * scale);
    TInt lastY = CeilHalf(top + aSpriteHeight * scale);
    if (lastX <= 0 || lastY <= 0 || firstX >= iWidth || firstY >= iHeight) return;

    TFixed step = (TFixed)(65536.0f / scale);
    TFixed u = (TFixed)((firstX + 0.5f - left) / scale * 65536.0f);
    TFixed v = (TFixed)((firstY + 0.5f - top) / scale * 65536.0f);
    if (firstX < 0) { u += -firstX * step; firstX = 0; }
    if (firstY < 0) { v += -firstY * step; firstY = 0; }
    if (lastX > iWidth) lastX = iWidth;
    if (lastY > iHeight) lastY = iHeight;

    TInt spriteDepth = (TInt)(inverseDepth * KDepthScale);
    for (TInt row = firstY; row < lastY; row++, v += step)
        {
        TInt sourceRow = v >> KFixedShift;
        if (sourceRow >= aSpriteHeight) break;
        if (sourceRow < 0) continue;
        TInt rowOffset = sourceRow * aSpriteWidth;
        TInt pixel = row * iWidth + firstX;
        TFixed column = u;
        for (TInt x = firstX; x < lastX; x++, pixel++, column += step)
            {
            TInt sourceColumn = column >> KFixedShift;
            if (sourceColumn >= aSpriteWidth) break;
            TInt colorIndex = aPixels[rowOffset + sourceColumn] & 255;
            if (colorIndex != 0 && spriteDepth > iZBuffer[pixel])
                iFrameBuffer[pixel] = ArgbToRgb565(aPalette[colorIndex]);
            }
        }
    }

TBool CRenderer::Project(TReal32 aWorldX, TReal32 aWorldY, TReal32 aWorldZ)
    {
    TReal32 deltaX = aWorldX - iEyeX;
    TReal32 deltaY = aWorldY - iEyeY;
    TReal32 deltaZ = aWorldZ - iEyeZ;
    TReal32 depth = deltaX * iForwardX + deltaY * iForwardY + deltaZ * iForwardZ;
    if (depth < 1) return EFalse;
    TReal32 viewX = deltaX * iRightX + deltaY * iRightY + deltaZ * iRightZ;
    TReal32 viewY = deltaX * iUpX + deltaY * iUpY + deltaZ * iUpZ;
    iProjectedX = iCenterX + viewX * iXScale / depth;
    iProjectedY = iCenterY - viewY * iYScale / depth;
    iProjectedZ = depth;
    return ETrue;
    }

void CRenderer::BuildModelShade(const TUint32* aPalette, TUint16* aDestination)
    {
    for (TInt level = 0; level < KShadeLevelCount; level++)
        {
        TReal32 factor = (TReal32)level / 32.0f;
        for (TInt color = 0; color < KShadeColorCount; color++)
            {
            TUint32 source = aPalette[color];
            TInt red = (TInt)(((TReal32)((source >> 16) & 255)) * factor);
            TInt green = (TInt)(((TReal32)((source >> 8) & 255)) * factor);
            TInt blue = (TInt)(((TReal32)(source & 255)) * factor);
            red = ClampInt(red, 0, 255);
            green = ClampInt(green, 0, 255);
            blue = ClampInt(blue, 0, 255);
            /* Гамма модели тождественная, как и статическая gamma в Java. */
            aDestination[level * KShadeColorCount + color] =
                (TUint16)PackRgb565(red, green, blue);
            }
        }
    }

void CRenderer::DrawMesh(const TModelMesh& aMesh, const TReal32* aMatrix, TInt aBaseLevel)
    {
    DrawMesh(aMesh, aMatrix, aBaseLevel, EFalse, EFalse);
    }

void CRenderer::DrawMesh(const TModelMesh& aMesh, const TReal32* aMatrix, TInt aBaseLevel,
                         TBool aMirror)
    {
    DrawMesh(aMesh, aMatrix, aBaseLevel, aMirror, EFalse);
    }

void CRenderer::DrawMesh(const TModelMesh& aMesh, const TReal32* aMatrix, TInt aBaseLevel,
                         TBool aMirror, TBool aByNormals)
    {
    if (!aMatrix || aMesh.iVertexCount > KMaxMeshVerts ||
        aMesh.iNormalCount > KMaxMeshVerts) return;

    TInt vertexCount = aMesh.iVertexCount;
    TInt normalCount = aMesh.iNormalCount;
    TInt verticesInFront = 0;
    for (TInt i = 0; i < vertexCount; i++)
        {
        TInt matrixOffset = aMesh.iVertexBone[i] * 12;
        TReal32 vertexX = aMesh.iVertex[i * 3];
        TReal32 vertexY = aMesh.iVertex[i * 3 + 1];
        TReal32 vertexZ = aMesh.iVertex[i * 3 + 2];
        TReal32 worldX = aMatrix[matrixOffset] * vertexX + aMatrix[matrixOffset + 1] * vertexY +
                         aMatrix[matrixOffset + 2] * vertexZ + aMatrix[matrixOffset + 3] - iEyeX;
        TReal32 worldY = aMatrix[matrixOffset + 4] * vertexX + aMatrix[matrixOffset + 5] * vertexY +
                         aMatrix[matrixOffset + 6] * vertexZ + aMatrix[matrixOffset + 7] - iEyeY;
        TReal32 worldZ = aMatrix[matrixOffset + 8] * vertexX + aMatrix[matrixOffset + 9] * vertexY +
                         aMatrix[matrixOffset + 10] * vertexZ + aMatrix[matrixOffset + 11] - iEyeZ;
        TReal32 depth = worldX * iForwardX + worldY * iForwardY + worldZ * iForwardZ;
        iModelZ[i] = depth;
        gMeshWorldX[i] = worldX;
        gMeshWorldY[i] = worldY;
        gMeshWorldZ[i] = worldZ;
        TReal32 viewX = worldX * iRightX + worldY * iRightY + worldZ * iRightZ;
        TReal32 viewY = worldX * iUpX + worldY * iUpY + worldZ * iUpZ;
        gMeshViewX[i] = viewX;
        gMeshViewY[i] = viewY;
        if (depth > KModelNearPlane)
            {
            verticesInFront++;
            TReal32 inverseDepth = 1.0f / depth;
            iModelX[i] = iCenterX + viewX * iXScale * inverseDepth;
            iModelY[i] = iCenterY - viewY * iYScale * inverseDepth;
            }
        else
            {
            iModelX[i] = 0;
            iModelY[i] = 0;
            }
        }
    if (verticesInFront == 0) return;

    for (TInt i = 0; i < normalCount; i++)
        {
        TInt matrixOffset = aMesh.iNormalBone[i] * 12;
        TReal32 normalX = aMesh.iNormal[i * 3];
        TReal32 normalY = aMesh.iNormal[i * 3 + 1];
        TReal32 normalZ = aMesh.iNormal[i * 3 + 2];
        TReal32 worldX = aMatrix[matrixOffset] * normalX + aMatrix[matrixOffset + 1] * normalY +
                         aMatrix[matrixOffset + 2] * normalZ;
        TReal32 worldY = aMatrix[matrixOffset + 4] * normalX + aMatrix[matrixOffset + 5] * normalY +
                         aMatrix[matrixOffset + 6] * normalZ;
        TReal32 worldZ = aMatrix[matrixOffset + 8] * normalX + aMatrix[matrixOffset + 9] * normalY +
                         aMatrix[matrixOffset + 10] * normalZ;
        gMeshNormalX[i] = worldX;
        gMeshNormalY[i] = worldY;
        gMeshNormalZ[i] = worldZ;
        TReal32 diffuse = -(worldX * 0.30f + worldY * 0.20f + worldZ * -0.93f);
        TReal32 factor = 0.62f + (diffuse > 0 ? diffuse * 0.55f : diffuse * 0.12f);
        TInt level = (TInt)((TReal32)aBaseLevel * factor) + 3;
        iModelLevel[i] = ClampInt(level, 0, 63);
        }

    for (TInt material = 0; material < aMesh.iMaterialCount; material++)
        {
        TInt textureIndex = aMesh.iMaterial[material];
        const TUint8* texture = CModel::iModelTexture[textureIndex];
        TInt textureWidth = CModel::iModelTextureWidth[textureIndex];
        TInt textureHeight = CModel::iModelTextureHeight[textureIndex];
        TInt triangleCount = aMesh.iTriangleLength[material];
        const TInt16* triangles = aMesh.iTriangles[material];
        for (TInt k = 0; k < triangleCount * 12; k += 12)
            {
            TInt first = triangles[k];
            TInt second = triangles[k + 4];
            TInt third = triangles[k + 8];
            if (aByNormals)
                {
                TInt firstNormal = triangles[k + 1];
                TInt secondNormal = triangles[k + 5];
                TInt thirdNormal = triangles[k + 9];
                TReal32 facing =
                    gMeshNormalX[firstNormal] * gMeshWorldX[first] +
                    gMeshNormalY[firstNormal] * gMeshWorldY[first] +
                    gMeshNormalZ[firstNormal] * gMeshWorldZ[first] +
                    gMeshNormalX[secondNormal] * gMeshWorldX[second] +
                    gMeshNormalY[secondNormal] * gMeshWorldY[second] +
                    gMeshNormalZ[secondNormal] * gMeshWorldZ[second] +
                    gMeshNormalX[thirdNormal] * gMeshWorldX[third] +
                    gMeshNormalY[thirdNormal] * gMeshWorldY[third] +
                    gMeshNormalZ[thirdNormal] * gMeshWorldZ[third];
                if (facing >= 0) continue;
                }
            if (iModelZ[first] <= KModelNearPlane ||
                iModelZ[second] <= KModelNearPlane ||
                iModelZ[third] <= KModelNearPlane)
                {
                if (iModelZ[first] <= KModelNearPlane &&
                    iModelZ[second] <= KModelNearPlane &&
                    iModelZ[third] <= KModelNearPlane) continue;
                ClipTriangle(triangles + k, texture, textureWidth, textureHeight,
                             aMirror, aByNormals);
                continue;
                }

            TReal32 ax = iModelX[first];
            TReal32 ay = iModelY[first];
            TReal32 bx = iModelX[second];
            TReal32 by = iModelY[second];
            TReal32 cx = iModelX[third];
            TReal32 cy = iModelY[third];
            TReal32 area = (bx - ax) * (cy - ay) - (cx - ax) * (by - ay);
            if (!aByNormals && (aMirror ? area >= 0 : area <= 0)) continue;
            iStatTriangles++;
            Triangle(ax, ay, 1.0f / iModelZ[first],
                     (TReal32)triangles[k + 2], (TReal32)triangles[k + 3],
                     (TReal32)iModelLevel[triangles[k + 1]],
                     bx, by, 1.0f / iModelZ[second],
                     (TReal32)triangles[k + 6], (TReal32)triangles[k + 7],
                     (TReal32)iModelLevel[triangles[k + 5]],
                     cx, cy, 1.0f / iModelZ[third],
                     (TReal32)triangles[k + 10], (TReal32)triangles[k + 11],
                     (TReal32)iModelLevel[triangles[k + 9]],
                     texture, textureWidth, textureHeight, aBaseLevel, aMirror);
            }
        }
    }

void CRenderer::ClipTriangle(const TInt16* aTriangle, const TUint8* aTexture,
                             TInt aTextureWidth, TInt aTextureHeight,
                             TBool aMirror, TBool aByNormals)
    {
    TInt count = 0;
    for (TInt i = 0; i < 3; i++)
        {
        TInt vertex = aTriangle[i * 4];
        iClipAX[count] = gMeshViewX[vertex];
        iClipAY[count] = gMeshViewY[vertex];
        iClipAZ[count] = iModelZ[vertex];
        iProjectedSX[count] = (TReal32)aTriangle[i * 4 + 2];
        iProjectedSY[count] = (TReal32)aTriangle[i * 4 + 3];
        iClipBX[count] = (TReal32)iModelLevel[aTriangle[i * 4 + 1]];
        count++;
        }

    TInt clipped = 0;
    for (TInt i = 0; i < count; i++)
        {
        TInt next = (i + 1) % count;
        TBool inside = iClipAZ[i] > KModelNearPlane;
        TBool nextInside = iClipAZ[next] > KModelNearPlane;
        if (inside)
            {
            iClipAX[clipped] = iClipAX[i];
            iClipAY[clipped] = iClipAY[i];
            iClipAZ[clipped] = iClipAZ[i];
            iProjectedSX[clipped] = iProjectedSX[i];
            iProjectedSY[clipped] = iProjectedSY[i];
            iClipBX[clipped] = iClipBX[i];
            clipped++;
            }
        if (inside != nextInside)
            {
            TReal32 fraction = (KModelNearPlane + 0.01f - iClipAZ[i]) /
                               (iClipAZ[next] - iClipAZ[i]);
            iClipAX[clipped] = iClipAX[i] + (iClipAX[next] - iClipAX[i]) * fraction;
            iClipAY[clipped] = iClipAY[i] + (iClipAY[next] - iClipAY[i]) * fraction;
            iClipAZ[clipped] = iClipAZ[i] + (iClipAZ[next] - iClipAZ[i]) * fraction;
            iProjectedSX[clipped] = iProjectedSX[i] +
                                    (iProjectedSX[next] - iProjectedSX[i]) * fraction;
            iProjectedSY[clipped] = iProjectedSY[i] +
                                    (iProjectedSY[next] - iProjectedSY[i]) * fraction;
            iClipBX[clipped] = iClipBX[i] + (iClipBX[next] - iClipBX[i]) * fraction;
            clipped++;
            }
        }
    if (clipped < 3) return;

    for (TInt i = 0; i < clipped; i++)
        {
        TReal32 inverseDepth = 1.0f / iClipAZ[i];
        TReal32 projectedX = iCenterX + iClipAX[i] * iXScale * inverseDepth;
        TReal32 projectedY = iCenterY - iClipAY[i] * iYScale * inverseDepth;
        iClipAX[i] = projectedX;
        iClipAY[i] = projectedY;
        iClipAZ[i] = inverseDepth;
        }

    for (TInt i = 1; i + 1 < clipped; i++)
        {
        TInt second = i;
        TInt third = i + 1;
        TReal32 area = (iClipAX[second] - iClipAX[0]) * (iClipAY[third] - iClipAY[0]) -
                       (iClipAX[third] - iClipAX[0]) * (iClipAY[second] - iClipAY[0]);
        if (!aByNormals && (aMirror ? area >= 0 : area <= 0)) continue;
        Triangle(iClipAX[0], iClipAY[0], iClipAZ[0],
                 iProjectedSX[0], iProjectedSY[0], (TInt)iClipBX[0],
                 iClipAX[second], iClipAY[second], iClipAZ[second],
                 iProjectedSX[second], iProjectedSY[second], (TInt)iClipBX[second],
                 iClipAX[third], iClipAY[third], iClipAZ[third],
                 iProjectedSX[third], iProjectedSY[third], (TInt)iClipBX[third],
                 aTexture, aTextureWidth, aTextureHeight, 0, aMirror);
        }
    }

void CRenderer::Triangle(TReal32 aX0, TReal32 aY0, TReal32 aZ0, TReal32 aU0, TReal32 aV0, TReal32 aL0,
                         TReal32 aX1, TReal32 aY1, TReal32 aZ1, TReal32 aU1, TReal32 aV1, TReal32 aL1,
                         TReal32 aX2, TReal32 aY2, TReal32 aZ2, TReal32 aU2, TReal32 aV2, TReal32 aL2,
                         const TUint8* aTexture, TInt aTextureWidth, TInt aTextureHeight,
                         TInt aBaseLevel, TBool aMirror)
    {
    (void)aBaseLevel;
    (void)aMirror;
    if (!aTexture || aTextureWidth <= 0 || aTextureHeight <= 0) return;

    TReal32 deltaX1 = aX1 - aX0;
    TReal32 deltaY1 = aY1 - aY0;
    TReal32 deltaX2 = aX2 - aX0;
    TReal32 deltaY2 = aY2 - aY0;
    TReal32 denominator = deltaX1 * deltaY2 - deltaX2 * deltaY1;
    if (denominator > -0.01f && denominator < 0.01f) return;
    TReal32 inverse = 1.0f / denominator;
    TReal32 depthX = ((aZ1 - aZ0) * deltaY2 - (aZ2 - aZ0) * deltaY1) * inverse;
    TReal32 depthY = ((aZ2 - aZ0) * deltaX1 - (aZ1 - aZ0) * deltaX2) * inverse;
    TReal32 uX = ((aU1 - aU0) * deltaY2 - (aU2 - aU0) * deltaY1) * inverse;
    TReal32 uY = ((aU2 - aU0) * deltaX1 - (aU1 - aU0) * deltaX2) * inverse;
    TReal32 vX = ((aV1 - aV0) * deltaY2 - (aV2 - aV0) * deltaY1) * inverse;
    TReal32 vY = ((aV2 - aV0) * deltaX1 - (aV1 - aV0) * deltaX2) * inverse;
    TReal32 lightX = ((aL1 - aL0) * deltaY2 - (aL2 - aL0) * deltaY1) * inverse;
    TReal32 lightY = ((aL2 - aL0) * deltaX1 - (aL1 - aL0) * deltaX2) * inverse;

    TReal32 ax = aX0;
    TReal32 ay = aY0;
    TReal32 bx = aX1;
    TReal32 by = aY1;
    TReal32 cx = aX2;
    TReal32 cy = aY2;
    TReal32 temporary = 0;
    if (ay > by)
        {
        temporary = ax; ax = bx; bx = temporary;
        temporary = ay; ay = by; by = temporary;
        }
    if (by > cy)
        {
        temporary = bx; bx = cx; cx = temporary;
        temporary = by; by = cy; cy = temporary;
        }
    if (ay > by)
        {
        temporary = ax; ax = bx; bx = temporary;
        temporary = ay; ay = by; by = temporary;
        }

    TInt firstRow = CeilHalf(ay);
    TInt lastRow = CeilHalf(cy);
    if (firstRow < 0) firstRow = 0;
    if (lastRow > iHeight) lastRow = iHeight;
    if (firstRow >= lastRow) return;

    TReal32 slopeAC = (cx - ax) / (cy - ay);
    TReal32 slopeAB = by - ay > 0.0001f ? (bx - ax) / (by - ay) : 0;
    TReal32 slopeBC = cy - by > 0.0001f ? (cx - bx) / (cy - by) : 0;
    TInt depthStep = (TInt)(depthX * KDepthScale);
    TFixed uStep = FxFromReal(uX);
    TFixed vStep = FxFromReal(vX);
    TFixed lightStep = FxFromReal(lightX);
    TInt textureMaskU = aTextureWidth - 1;
    TInt textureMaskV = aTextureHeight - 1;

    for (TInt row = firstRow; row < lastRow; row++)
        {
        TReal32 pixelY = (TReal32)row + 0.5f;
        TReal32 left = ax + (pixelY - ay) * slopeAC;
        TReal32 right = pixelY < by ? ax + (pixelY - ay) * slopeAB
                                    : bx + (pixelY - by) * slopeBC;
        if (left > right)
            {
            temporary = left; left = right; right = temporary;
            }
        TInt firstColumn = CeilHalf(left);
        TInt lastColumn = CeilHalf(right);
        if (firstColumn < 0) firstColumn = 0;
        if (lastColumn > iWidth) lastColumn = iWidth;
        if (firstColumn >= lastColumn) continue;

        TReal32 fromLeft = (TReal32)firstColumn + 0.5f - aX0;
        TReal32 fromTop = pixelY - aY0;
        TInt depth = (TInt)((aZ0 + depthX * fromLeft + depthY * fromTop) * KDepthScale);
        TFixed u = FxFromReal(aU0 + uX * fromLeft + uY * fromTop);
        TFixed v = FxFromReal(aV0 + vX * fromLeft + vY * fromTop);
        TFixed light = FxFromReal(aL0 + lightX * fromLeft + lightY * fromTop);
        TInt pixel = row * iWidth + firstColumn;
        TInt endPixel = row * iWidth + lastColumn;
        for (; pixel < endPixel; pixel++)
            {
            if (depth > iZBuffer[pixel])
                {
                TInt sourceU = u >> KFixedShift;
                TInt sourceV = v >> KFixedShift;
                TInt level = light >> KFixedShift;
                if (sourceU < 0) sourceU = 0;
                else if (sourceU > textureMaskU) sourceU = textureMaskU;
                if (sourceV < 0) sourceV = 0;
                else if (sourceV > textureMaskV) sourceV = textureMaskV;
                if (level < 0) level = 0;
                else if (level > 63) level = 63;
                TInt texel = aTexture[sourceV * aTextureWidth + sourceU] & 255;
                iFrameBuffer[pixel] = iModelShade[(level << 8) | texel];
                iZBuffer[pixel] = depth;
                }
            depth += depthStep;
            u += uStep;
            v += vStep;
            light += lightStep;
            }
        }
    }

void CRenderer::ClearZ()
    {
    TInt length = iWidth * iHeight;
    for (TInt i = length - 1; i >= 0; i--) iZBuffer[i] = 0;
    }

TInt CRenderer::ZOf(TReal32 aZ)
    { return (TInt)((TReal32)KDepthScale / aZ); }

void CRenderer::ClearFrame(TUint16 aColor)
    {
    TInt length = iWidth * iHeight;
    for (TInt i = 0; i < length; i++) iFrameBuffer[i] = aColor;
    }

void CRenderer::FillRect(TInt aX, TInt aY, TInt aWidth, TInt aHeight, TUint16 aColor)
    {
    if (aX < 0) { aWidth += aX; aX = 0; }
    if (aY < 0) { aHeight += aY; aY = 0; }
    if (aX + aWidth > iWidth) aWidth = iWidth - aX;
    if (aY + aHeight > iHeight) aHeight = iHeight - aY;
    if (aWidth <= 0 || aHeight <= 0) return;
    for (TInt row = aY; row < aY + aHeight; row++)
        {
        TInt pixel = row * iWidth + aX;
        for (TInt column = 0; column < aWidth; column++) iFrameBuffer[pixel++] = aColor;
        }
    }

void CRenderer::FillRectAlpha(TInt aX, TInt aY, TInt aWidth, TInt aHeight,
                              TUint16 aColor, TInt aAlpha)
    {
    if (aAlpha <= 0) return;
    if (aX < 0) { aWidth += aX; aX = 0; }
    if (aY < 0) { aHeight += aY; aY = 0; }
    if (aX + aWidth > iWidth) aWidth = iWidth - aX;
    if (aY + aHeight > iHeight) aHeight = iHeight - aY;
    if (aWidth <= 0 || aHeight <= 0) return;

    TInt red = Red8(aColor);
    TInt green = Green8(aColor);
    TInt blue = Blue8(aColor);
    for (TInt row = aY; row < aY + aHeight; row++)
        {
        TInt pixel = row * iWidth + aX;
        for (TInt column = 0; column < aWidth; column++, pixel++)
            {
            if (aAlpha >= 256)
                {
                iFrameBuffer[pixel] = aColor;
                continue;
                }
            TUint16 background = iFrameBuffer[pixel];
            TInt newRed = Red8(background) + ((red - Red8(background)) * aAlpha >> 8);
            TInt newGreen = Green8(background) + ((green - Green8(background)) * aAlpha >> 8);
            TInt newBlue = Blue8(background) + ((blue - Blue8(background)) * aAlpha >> 8);
            iFrameBuffer[pixel] = (TUint16)PackRgb565(newRed, newGreen, newBlue);
            }
        }
    }

void CRenderer::DrawLine(TInt aX0, TInt aY0, TInt aX1, TInt aY1, TUint16 aColor)
    {
    TInt stepX = aX1 > aX0 ? 1 : -1;
    TInt stepY = aY1 > aY0 ? 1 : -1;
    TInt deltaX = AbsInt(aX1 - aX0);
    TInt deltaY = -AbsInt(aY1 - aY0);
    TInt error = deltaX + deltaY;
    TInt x = aX0;
    TInt y = aY0;
    for (;;)
        {
        SetPixel(x, y, aColor);
        if (x == aX1 && y == aY1) return;
        TInt doubled = error << 1;
        if (doubled >= deltaY) { error += deltaY; x += stepX; }
        if (doubled <= deltaX) { error += deltaX; y += stepY; }
        }
    }

void CRenderer::SetPixel(TInt aX, TInt aY, TUint16 aColor)
    {
    if (aX < 0 || aY < 0 || aX >= iWidth || aY >= iHeight) return;
    iFrameBuffer[aY * iWidth + aX] = aColor;
    }

void CRenderer::DrawBitmap(TInt aX, TInt aY, const CSpriteSet::TSprite& aSprite,
                           const TUint32* aPalette, TBool aTransparent)
    {
    if (!aSprite.iPixels || !aPalette) return;
    TInt sourceStart = 0;
    TInt sourceEnd = aSprite.iWidth;
    if (aX < 0) { sourceStart = -aX; aX = 0; }
    if (aX + sourceEnd > iWidth) sourceEnd = iWidth - aX;
    for (TInt row = 0; row < aSprite.iHeight; row++)
        {
        TInt targetY = aY + row;
        if (targetY < 0 || targetY >= iHeight) continue;
        TInt sourceOffset = row * aSprite.iWidth;
        TInt pixel = targetY * iWidth + aX;
        for (TInt column = sourceStart; column < sourceEnd; column++, pixel++)
            {
            TInt colorIndex = aSprite.iPixels[sourceOffset + column] & 255;
            if (aTransparent && (colorIndex == 0 || colorIndex == 255)) continue;
            iFrameBuffer[pixel] = ArgbToRgb565(aPalette[colorIndex]);
            }
        }
    }

void CRenderer::DrawBitmapScaled(TInt aX, TInt aY, TInt aWidth, TInt aHeight,
                                 const CSpriteSet::TSprite& aSprite,
                                 const TUint32* aPalette, TBool aTransparent)
    {
    if (!aSprite.iPixels || !aPalette || aWidth <= 0 || aHeight <= 0 ||
        aSprite.iWidth <= 0 || aSprite.iHeight <= 0) return;
    TInt destinationStart = 0;
    TInt destinationEnd = aWidth;
    if (aX < 0) { destinationStart = -aX; aX = 0; }
    if (aX + destinationEnd > iWidth) destinationEnd = iWidth - aX;
    for (TInt row = 0; row < aHeight; row++)
        {
        TInt targetY = aY + row;
        if (targetY < 0 || targetY >= iHeight) continue;
        TInt sourceRow = row * aSprite.iHeight / aHeight;
        TInt sourceOffset = sourceRow * aSprite.iWidth;
        TInt pixel = targetY * iWidth + aX;
        for (TInt column = destinationStart; column < destinationEnd; column++, pixel++)
            {
            TInt sourceColumn = column * aSprite.iWidth / aWidth;
            TInt colorIndex = aSprite.iPixels[sourceOffset + sourceColumn] & 255;
            if (aTransparent && (colorIndex == 0 || colorIndex == 255)) continue;
            iFrameBuffer[pixel] = ArgbToRgb565(aPalette[colorIndex]);
            }
        }
    }

void CRenderer::DrawGlyph(TInt aX, TInt aY, const CCSFont::TGlyph& aGlyph, TUint16 aColor)
    {
    if (!aGlyph.iPixels || aGlyph.iWidth <= 0 || gGlyphHeight <= 0) return;
    for (TInt row = 0; row < gGlyphHeight; row++)
        {
        TInt targetY = aY + row;
        if (targetY < 0 || targetY >= iHeight) continue;
        TInt sourceOffset = row * aGlyph.iWidth;
        TInt pixel = targetY * iWidth;
        for (TInt column = 0; column < aGlyph.iWidth; column++)
            {
            TInt targetX = aX + column;
            if (targetX < 0 || targetX >= iWidth) continue;
            if (aGlyph.iPixels[sourceOffset + column] != 0)
                iFrameBuffer[pixel + targetX] = aColor;
            }
        }
    }

TInt CRenderer::TextWidth(const CCSFont& aFont, const TDesC& aText) const
    {
    TInt width = 0;
    for (TInt i = 0; i < aText.Length(); i++)
        {
        const CCSFont::TGlyph* glyph = aFont.Glyph((TUint16)aText[i]);
        width += glyph ? glyph->iWidth : aFont.iHeight / 3;
        }
    return width;
    }

void CRenderer::DrawText(TInt aX, TInt aY, const CCSFont& aFont, const TDesC& aText,
                         TUint16 aColor)
    {
    gGlyphHeight = aFont.iHeight;
    TInt x = aX;
    for (TInt i = 0; i < aText.Length(); i++)
        {
        const CCSFont::TGlyph* glyph = aFont.Glyph((TUint16)aText[i]);
        if (!glyph)
            {
            x += aFont.iHeight / 3;
            continue;
            }
        DrawGlyph(x, aY, *glyph, aColor);
        x += glyph->iWidth;
        }
    }

void CRenderer::DrawTextCentered(TInt aCenterX, TInt aY, const CCSFont& aFont,
                                 const TDesC& aText, TUint16 aColor)
    {
    TInt x = aCenterX - TextWidth(aFont, aText) / 2;
    DrawText(x + 1, aY + 1, aFont, aText, 0);
    DrawText(x, aY, aFont, aText, aColor);
    }
