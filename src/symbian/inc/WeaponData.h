#ifndef CS16_WEAPON_DATA_H
#define CS16_WEAPON_DATA_H

/* Generated from src/core/cs/Wp.java -- weapon values are identical. */

const TInt KWeaponCount = 29;

static const TInt KWpPrice[29] = { 0, 400, 500, 600, 650, 800, 750, 1700, 3000, 1400, 1250, 1500, 1700, 2350, 2000, 2250, 2500, 3100, 3500, 3500, 2750, 4750, 5000, 4200, 5750, 300, 200, 300, 0 };
static const TInt KWpSlot[29] = { 2, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 3, 3, 4 };
static const TInt KWpTeam[29] = { 0, 0, 0, 0, 0, 1, 2, 0, 0, 1, 2, 0, 0, 0, 1, 2, 1, 2, 1, 2, 0, 0, 1, 2, 0, 0, 0, 0, 1 };
static const TInt KWpClip[29] = { 0, 20, 12, 13, 7, 30, 20, 8, 7, 30, 30, 30, 25, 50, 35, 25, 30, 30, 30, 30, 10, 10, 20, 30, 100, 1, 1, 1, 1 };
static const TInt KWpReserve[29] = { 0, 120, 100, 52, 35, 120, 100, 32, 32, 100, 120, 120, 100, 100, 90, 90, 90, 90, 90, 90, 90, 30, 90, 90, 200, 0, 0, 0, 0 };
static const TInt KWpAmmoPrice[29] = { 0, 40, 50, 50, 80, 40, 50, 65, 65, 50, 40, 40, 50, 50, 60, 60, 80, 60, 60, 60, 80, 125, 80, 60, 60, 0, 0, 0, 0 };
static const TInt KWpDmg[29] = { 15, 25, 34, 32, 54, 36, 20, 20, 20, 29, 20, 26, 30, 21, 30, 30, 36, 32, 33, 32, 75, 115, 80, 70, 32, 0, 0, 0, 0 };
static const TReal32 KWpRangeMod[29] = { 1, .75f, .79f, .8f, .81f, .75f, .885f, .7f, .7f, .82f, .85f, .84f, .82f, .885f, .98f, .96f, .98f, .97f, .955f, .96f, .98f, .99f, .98f, .98f, .97f, 1, 1, 1, 1 };
static const TReal32 KWpCycle[29] = { .4f, .15f, .15f, .2f, .3f, .12f, .15f, .88f, .25f, .07f, .07f, .075f, .105f, .066f, .0875f, .0825f, .0955f, .0875f, .0825f, .0825f, 1.25f, 1.45f, .25f, .25f, .1f, 1, 1, 1, 1 };
static const TReal32 KWpReloadT[29] = { 0, 2.2f, 2.7f, 2.7f, 2.2f, 4.5f, 2.7f, .45f, .45f, 3.15f, 2.12f, 2.63f, 3.5f, 3.4f, 2.45f, 3.3f, 2.45f, 3.05f, 3.0f, 3.3f, 2.0f, 2.5f, 3.5f, 3.35f, 4.7f, 0, 0, 0, 0 };
static const TBool KWpAuto[29] = { EFalse, EFalse, EFalse, EFalse, EFalse, EFalse, EFalse, EFalse, ETrue, ETrue, ETrue, ETrue, ETrue, ETrue, ETrue, ETrue, ETrue, ETrue, ETrue, ETrue, EFalse, EFalse, EFalse, EFalse, ETrue, EFalse, EFalse, EFalse, EFalse };
static const TReal32 KWpArmorR[29] = { 1.7f, .5f, 1.0f, 1.25f, 1.5f, 1.05f, 1.5f, 1, 1, .95f, 1, 1, 1, 1.5f, 1.55f, 1.4f, 1.55f, 1.4f, 1.4f, 1.4f, 1.7f, 1.95f, 1.65f, 1.45f, 1.5f, 1, 1, 1, 1 };
static const TInt KWpSpeed[29] = { 250, 250, 250, 250, 250, 250, 250, 230, 240, 250, 250, 250, 250, 245, 240, 240, 221, 230, 235, 240, 260, 210, 210, 210, 220, 250, 250, 250, 250 };
static const TInt KWpPellets[29] = { 1, 1, 1, 1, 1, 1, 1, 9, 6, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
static const TReal32 KWpSpread[29] = { 0, .025f, .02f, .02f, .025f, .03f, .02f, .0675f, .0725f, .03f, .025f, .02f, .025f, .025f, .022f, .02f, .02f, .015f, .018f, .018f, .1f, .12f, .05f, .05f, .03f, 0, 0, 0, 0 };
static const TReal32 KWpMoveSpread[29] = { 0, .03f, .03f, .03f, .05f, .04f, .03f, .02f, .02f, .035f, .03f, .03f, .035f, .035f, .07f, .07f, .08f, .07f, .07f, .07f, .2f, .3f, .15f, .15f, .09f, 0, 0, 0, 0 };
static const TReal32 KWpRecoilSpread[29] = { 0, .012f, .01f, .012f, .03f, .012f, .01f, 0, 0, .006f, .005f, .004f, .005f, .004f, .006f, .006f, .008f, .006f, .006f, .006f, 0, 0, .02f, .02f, .006f, 0, 0, 0, 0 };
static const TReal32 KWpPunch[29] = { 0, 1.2f, 1.6f, 1.5f, 2.6f, 1.4f, 1.2f, 4.0f, 3.0f, .8f, .6f, .55f, .7f, .55f, .9f, .85f, 1.05f, .8f, .8f, .75f, 2.2f, 2.8f, 1.8f, 1.8f, .9f, 0, 0, 0, 0 };
static const TInt KWpZoom1[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 55, 55, 40, 40, 40, 40, 0, 0, 0, 0, 0 };
static const TInt KWpZoom2[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 15, 10, 15, 15, 0, 0, 0, 0, 0 };
static const TReal32 KWpDrawT[29] = { .75f, .75f, 1.0f, .75f, .75f, 1.0f, .75f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.2f, 1.0f, 1.0f, 1.2f, 1.3f, 1.2f, 1.2f, 1.2f, .7f, .7f, .7f, .8f };
static const TInt KWpPenPower[29] = { 0, 21, 15, 25, 30, 21, 30, 0, 0, 15, 21, 21, 15, 30, 35, 35, 39, 35, 35, 35, 39, 45, 39, 35, 35, 0, 0, 0, 0 };
static const TInt KWpPenCount[29] = { 0, 1, 1, 1, 2, 1, 1, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 3, 2, 2, 2, 0, 0, 0, 0 };

#endif
