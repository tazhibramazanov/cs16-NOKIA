#ifndef CS16_RENDERER_H
#define CS16_RENDERER_H

/*
 * Renderer.h -- программный рендер, порт src/core/cs/Render.java.
 *
 * Сохранены полностью: BSP front-to-back, построчный span-буфер (MAXSEG=64),
 * z-буфер для brush-моделей и спрайтов, 5 граней неба, билинейные lightmap,
 * перспективная коррекция с шагом 1<<stepShift, три mip-уровня, все
 * настройки качества. Единственное изменение -- RGB565 вместо ARGB32:
 * таблица shade сразу содержит RGB565, поэтому в пиксельном цикле нет
 * конверсии и нет лишнего прохода drawRGB.
 */

#include <e32std.h>
#include "Assets.h"

const TInt KMaxSegments = 64;
const TInt KMaxSkySpans = 2048;
const TInt KMaxAlphaFaces = 512;
const TInt KMaxClipVerts = 64;
const TInt KMaxMeshVerts = 512;

class CRenderer
    {
public:
    CRenderer(TInt aWidth, TInt aHeight, TUint16* aFrameBuffer);
    ~CRenderer();

    void SetMap(CMap* aMap);
    void SetCamera(TReal32 aX, TReal32 aY, TReal32 aZ,
                   TReal32 aYawDegrees, TReal32 aPitchDegrees, TReal32 aFovDegrees);

    void RenderWorld();
    void RenderWorldAlpha();
    void RenderBrushModel(TInt aModel, TReal32 aOffsetX, TReal32 aOffsetY,
                          TReal32 aOffsetZ, TBool aMasked);

    void DrawSprite(const TUint8* aPixels, TInt aSpriteWidth, TInt aSpriteHeight,
                    const TUint32* aPalette, TReal32 aWorldX, TReal32 aWorldY,
                    TReal32 aWorldZ, TReal32 aAnchorX, TReal32 aAnchorY,
                    TReal32 aPixelsPerUnit);

    TBool Project(TReal32 aWorldX, TReal32 aWorldY, TReal32 aWorldZ);

    void DrawMesh(const TModelMesh& aMesh, const TReal32* aMatrix, TInt aBaseLevel);
    void DrawMesh(const TModelMesh& aMesh, const TReal32* aMatrix, TInt aBaseLevel,
                  TBool aMirror);
    void DrawMesh(const TModelMesh& aMesh, const TReal32* aMatrix, TInt aBaseLevel,
                  TBool aMirror, TBool aByNormals);

    void ClearZ();
    static TInt ZOf(TReal32 aZ);

    /* Плоские 2D-примитивы для HUD/меню: работают с тем же RGB565-кадром. */
    void ClearFrame(TUint16 aColor);
    void FillRect(TInt aX, TInt aY, TInt aWidth, TInt aHeight, TUint16 aColor);
    void FillRectAlpha(TInt aX, TInt aY, TInt aWidth, TInt aHeight,
                       TUint16 aColor, TInt aAlpha);
    void DrawLine(TInt aX0, TInt aY0, TInt aX1, TInt aY1, TUint16 aColor);
    void SetPixel(TInt aX, TInt aY, TUint16 aColor);
    void DrawBitmap(TInt aX, TInt aY, const CSpriteSet::TSprite& aSprite,
                    const TUint32* aPalette, TBool aTransparent);
    void DrawBitmapScaled(TInt aX, TInt aY, TInt aWidth, TInt aHeight,
                          const CSpriteSet::TSprite& aSprite,
                          const TUint32* aPalette, TBool aTransparent);
    void DrawGlyph(TInt aX, TInt aY, const CCSFont::TGlyph& aGlyph, TUint16 aColor);
    TInt TextWidth(const CCSFont& aFont, const TDesC& aText) const;
    void DrawText(TInt aX, TInt aY, const CCSFont& aFont, const TDesC& aText,
                  TUint16 aColor);
    void DrawTextCentered(TInt aCenterX, TInt aY, const CCSFont& aFont,
                          const TDesC& aText, TUint16 aColor);

    /* Публичное состояние, используемое игровым слоем. */
    TInt iWidth;
    TInt iHeight;
    TUint16* iFrameBuffer;
    TInt* iZBuffer;
    CMap* iMap;

    TReal32 iEyeX, iEyeY, iEyeZ, iYaw, iPitch;
    TReal32 iForwardX, iForwardY, iForwardZ;
    TReal32 iRightX, iRightY, iRightZ;
    TReal32 iUpX, iUpY, iUpZ;
    TReal32 iXScale, iYScale, iCenterX, iCenterY, iFov;

    TUint16 iShade[64 * 256];
    TUint8 iGamma[256];
    TUint16 iModelShade[64 * 256];

    TInt iStatFaces;
    TInt iStatSpans;
    TInt iStatPixels;
    TInt iStatTriangles;
    TBool iFullBright;
    TInt iMipBias;
    TInt iStepShift;
    TBool iBilinear;
    TBool iFlatSky;
    TReal32 iMipScale;

    TReal32 iProjectedX, iProjectedY, iProjectedZ;

    static void BuildModelShade(const TUint32* aPalette, TUint16* aDestination);

private:
    void BuildSky();
    void MarkLeaves(TInt aLeaf);
    void MarkLeaf(TInt aLeaf);
    TInt CullBox(const TInt16* aBox, TInt aOffset, TInt aClip);
    void Walk(TInt aNode, TInt aClip);
    TInt ClipPlane(TInt aCount, TInt aPlane);
    TReal32 PlaneDistance(TInt aPlane, TReal32 aX, TReal32 aY, TReal32 aZ);
    void DrawFace(TInt aFace, TReal32 aOffsetX, TReal32 aOffsetY,
                  TReal32 aOffsetZ, TInt aMode);
    void SetupGradients(TInt aFace, TReal32 aOffsetX, TReal32 aOffsetY,
                        TReal32 aOffsetZ, TReal32 aMinimumZ);
    void Gradient(const TReal32* aVertices, TInt aOffset, TReal32 aScale,
                  TReal32 aOffsetX, TReal32 aOffsetY, TReal32 aOffsetZ);
    TInt LightSample(TReal32 aLightS, TReal32 aLightT);
    void SpanInsert(TInt aY, TInt aX0, TInt aX1, TBool aSky);
    void AddSky(TInt aY, TInt aX0, TInt aX1);
    void FillUncovered();
    void DrawSpan(TInt aY, TInt aX0, TInt aX1);
    void DrawSpanBilinear(TInt aY, TInt aX0, TInt aX1);
    void DrawSpanZ(TInt aY, TInt aX0, TInt aX1);
    void DrawSky();

    void ClipTriangle(const TInt16* aTriangle, const TUint8* aTexture,
                      TInt aTextureWidth, TInt aTextureHeight,
                      TBool aMirror, TBool aByNormals);
    void Triangle(TReal32 aX0, TReal32 aY0, TReal32 aZ0, TReal32 aU0, TReal32 aV0, TReal32 aL0,
                  TReal32 aX1, TReal32 aY1, TReal32 aZ1, TReal32 aU1, TReal32 aV1, TReal32 aL1,
                  TReal32 aX2, TReal32 aY2, TReal32 aZ2, TReal32 aU2, TReal32 aV2, TReal32 aL2,
                  const TUint8* aTexture, TInt aTextureWidth, TInt aTextureHeight, TInt aBaseLevel,
                  TBool aMirror);

private:
    TInt iFrame;
    TInt iVisFrame;
    TInt iViewLeaf;
    TInt* iLeafVis;
    TInt* iNodeVis;
    TInt* iFaceFrame;
    TInt* iVertexFrame;
    TReal32* iTransformedX;
    TReal32* iTransformedY;
    TReal32* iTransformedZ;

    TInt16* iSegmentA;
    TInt16* iSegmentB;
    TUint8* iSegmentCount;
    TInt iCovered;
    TInt* iRowFrame;
    TReal32* iRowMin;
    TReal32* iRowMax;
    TInt iRowStamp;

    TReal32 iClipAX[KMaxClipVerts];
    TReal32 iClipAY[KMaxClipVerts];
    TReal32 iClipAZ[KMaxClipVerts];
    TReal32 iClipBX[KMaxClipVerts];
    TReal32 iClipBY[KMaxClipVerts];
    TReal32 iClipBZ[KMaxClipVerts];
    TReal32 iProjectedSX[KMaxClipVerts];
    TReal32 iProjectedSY[KMaxClipVerts];

    TInt iSkyCount;
    TInt16 iSkyY[KMaxSkySpans];
    TInt16 iSkyX0[KMaxSkySpans];
    TInt16 iSkyX1[KMaxSkySpans];
    TUint16 iSkyGamma[5][KSkySide * KSkySide];
    TUint16 iSkyFlat;

    TInt iAlphaCount;
    TInt iAlphaFaces[KMaxAlphaFaces];

    /* Градиенты текущей грани. */
    TReal32 iGradientZx, iGradientZy, iGradientZ0;
    TReal32 iGradientSx, iGradientSy, iGradientS0;
    TReal32 iGradientTx, iGradientTy, iGradientT0;
    TReal32 iGradientLx, iGradientLy, iGradientL0;
    TReal32 iGradientMx, iGradientMy, iGradientM0;
    const TUint8* iGradientTexture;
    TInt iGradientTextureShift;
    TInt iGradientTextureMaskS;
    TInt iGradientTextureMaskT;
    TInt iGradientLightWidth;
    TInt iGradientLightHeight;
    TBool iGradientMasked;
    TInt iGradientConstLevel;

    TReal32 iModelX[KMaxMeshVerts];
    TReal32 iModelY[KMaxMeshVerts];
    TReal32 iModelZ[KMaxMeshVerts];
    TInt iModelLevel[KMaxMeshVerts];

    TInt* iTextureShift;
    TInt* iTextureMaskS;
    TInt* iTextureMaskT;
    TInt* iTextureTShift;
    };

#endif
