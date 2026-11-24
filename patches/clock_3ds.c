#include "patches.h"
#include "input.h"

#define SECONDS_IN_THREE_DAYS (3 * 24 * 60 * 60)
#define SECONDS_IN_SIX_HOURS (6 * 60 * 60)
#define CLOCK_SECTION_HALFWIDTH 24
#define CLOCK_SECTION_WIDTH 48

Gfx* Gfx_DrawTexRectIA8_DropShadow(Gfx* gfx, TexturePtr texture, s16 textureWidth, s16 textureHeight, s16 rectLeft,
                                   s16 rectTop, s16 rectWidth, s16 rectHeight, u16 dsdx, u16 dtdy, s16 r, s16 g,
                                   s16 b, s16 a);
Gfx* Gfx_DrawTexRectIA8_DropShadowOffset(Gfx* gfx, TexturePtr texture, s16 textureWidth, s16 textureHeight,
                                         s16 rectLeft, s16 rectTop, s16 rectWidth, s16 rectHeight, u16 dsdx, u16 dtdy,
                                         s16 r, s16 g, s16 b, s16 a, s32 masks, s32 rects);

#ifndef PAUSE_STATE_OFF
#define PAUSE_STATE_OFF 0
#endif

#ifndef DEBUG_EDITOR_NONE
#define DEBUG_EDITOR_NONE 0
#endif

extern u8 gFinalHoursClockDigit0Tex[];
extern u8 gFinalHoursClockDigit1Tex[];
extern u8 gFinalHoursClockDigit2Tex[];
extern u8 gFinalHoursClockDigit3Tex[];
extern u8 gFinalHoursClockDigit4Tex[];
extern u8 gFinalHoursClockDigit5Tex[];
extern u8 gFinalHoursClockDigit6Tex[];
extern u8 gFinalHoursClockDigit7Tex[];
extern u8 gFinalHoursClockDigit8Tex[];
extern u8 gFinalHoursClockDigit9Tex[];
extern u8 gFinalHoursClockColonTex[];
extern u8 gThreeDayClockMoonHourTex[];
extern u8 gThreeDayClockSunHourTex[];

INCBIN(gThreeDayClock3DSEdgeTex, "gThreeDayClock3DSEdgeTex.ia8.bin");
INCBIN(gThreeDayClock3DSMiddleTex, "gThreeDayClock3DSMiddleTex.ia8.bin");
INCBIN(gThreeDayClock3DSFillTex, "gThreeDayClock3DSFillTex.ia8.bin");
INCBIN(gThreeDayClock3DSArrowTex, "gThreeDayClock3DSArrowTex.ia8.bin");
INCBIN(gThreeDayClock3DSTimeBackdropTex, "gThreeDayClock3DSTimeBackdropTex.ia8.bin");
INCBIN(gThreeDayClock3DSSlowTimeTex, "gThreeDayClock3DSSlowTimeTex.ia8.bin");
INCBIN(gThreeDayClock3DSFinalHoursMoonTex, "gThreeDayClock3DSFinalHoursMoonTex.ia8.bin");
INCBIN(gThreeDayClock3DSFinalHoursColonTex, "gThreeDayClock3DSFinalHoursColonTex.ia8.bin");
INCBIN(gThreeDayClock3DSFinalHoursDigit0Tex, "gThreeDayClock3DSFinalHoursDigit0Tex.ia8.bin");
INCBIN(gThreeDayClock3DSFinalHoursDigit1Tex, "gThreeDayClock3DSFinalHoursDigit1Tex.ia8.bin");
INCBIN(gThreeDayClock3DSFinalHoursDigit2Tex, "gThreeDayClock3DSFinalHoursDigit2Tex.ia8.bin");
INCBIN(gThreeDayClock3DSFinalHoursDigit3Tex, "gThreeDayClock3DSFinalHoursDigit3Tex.ia8.bin");
INCBIN(gThreeDayClock3DSFinalHoursDigit4Tex, "gThreeDayClock3DSFinalHoursDigit4Tex.ia8.bin");
INCBIN(gThreeDayClock3DSFinalHoursDigit5Tex, "gThreeDayClock3DSFinalHoursDigit5Tex.ia8.bin");
INCBIN(gThreeDayClock3DSFinalHoursDigit6Tex, "gThreeDayClock3DSFinalHoursDigit6Tex.ia8.bin");
INCBIN(gThreeDayClock3DSFinalHoursDigit7Tex, "gThreeDayClock3DSFinalHoursDigit7Tex.ia8.bin");
INCBIN(gThreeDayClock3DSFinalHoursDigit8Tex, "gThreeDayClock3DSFinalHoursDigit8Tex.ia8.bin");
INCBIN(gThreeDayClock3DSFinalHoursDigit9Tex, "gThreeDayClock3DSFinalHoursDigit9Tex.ia8.bin");

static TexturePtr sDigitTextures[] = {
    (TexturePtr)gFinalHoursClockDigit0Tex,
    (TexturePtr)gFinalHoursClockDigit1Tex,
    (TexturePtr)gFinalHoursClockDigit2Tex,
    (TexturePtr)gFinalHoursClockDigit3Tex,
    (TexturePtr)gFinalHoursClockDigit4Tex,
    (TexturePtr)gFinalHoursClockDigit5Tex,
    (TexturePtr)gFinalHoursClockDigit6Tex,
    (TexturePtr)gFinalHoursClockDigit7Tex,
    (TexturePtr)gFinalHoursClockDigit8Tex,
    (TexturePtr)gFinalHoursClockDigit9Tex,
    (TexturePtr)gFinalHoursClockColonTex,
};

static TexturePtr sFinalHoursDigitTextures[] = {
    (TexturePtr)gThreeDayClock3DSFinalHoursDigit0Tex,
    (TexturePtr)gThreeDayClock3DSFinalHoursDigit1Tex,
    (TexturePtr)gThreeDayClock3DSFinalHoursDigit2Tex,
    (TexturePtr)gThreeDayClock3DSFinalHoursDigit3Tex,
    (TexturePtr)gThreeDayClock3DSFinalHoursDigit4Tex,
    (TexturePtr)gThreeDayClock3DSFinalHoursDigit5Tex,
    (TexturePtr)gThreeDayClock3DSFinalHoursDigit6Tex,
    (TexturePtr)gThreeDayClock3DSFinalHoursDigit7Tex,
    (TexturePtr)gThreeDayClock3DSFinalHoursDigit8Tex,
    (TexturePtr)gThreeDayClock3DSFinalHoursDigit9Tex,
    (TexturePtr)gThreeDayClock3DSFinalHoursColonTex,
};

static s16 sFinalHoursClockSlots[8];

static s32 recomp_clamp_s32(s32 value, s32 min, s32 max) {
    if (value < min) {
        return min;
    }
    if (value > max) {
        return max;
    }
    return value;
}

static s32 recomp_min_s32(s32 a, s32 b) {
    return a < b ? a : b;
}

static s32 recomp_max_s32(s32 a, s32 b) {
    return a > b ? a : b;
}

static s32 recomp_get_3ds_clock_time_until_crash(s32* previousTimeCheck) {
    s32 timeUntilCrash = (s32)TIME_TO_SECONDS_F(TIME_UNTIL_MOON_CRASH);
    s32 elapsedTime = SECONDS_IN_THREE_DAYS - timeUntilCrash;

    if ((*previousTimeCheck != -1) && (*previousTimeCheck > elapsedTime)) {
        timeUntilCrash = SECONDS_IN_THREE_DAYS - *previousTimeCheck;
        *previousTimeCheck = elapsedTime;
    } else {
        *previousTimeCheck = elapsedTime;
    }

    return recomp_clamp_s32(timeUntilCrash, 0, SECONDS_IN_THREE_DAYS);
}

static s32 recomp_should_draw_3ds_clock(PlayState* play) {
    MessageContext* msgCtx = &play->msgCtx;

    return (R_TIME_SPEED != 0) &&
           ((msgCtx->msgMode == MSGMODE_NONE) ||
            ((play->actorCtx.flags & ACTORCTX_FLAG_TELESCOPE_ON) && !Play_InCsMode(play)) ||
            ((msgCtx->currentTextId >= 0x100) && (msgCtx->currentTextId <= 0x200)) ||
            (gSaveContext.gameMode == GAMEMODE_END_CREDITS)) &&
           !FrameAdvance_IsEnabled(&play->state) && !Environment_IsTimeStopped() && (gSaveContext.save.day <= 3);
}

static s32 recomp_is_final_hours(void) {
    return (CURRENT_DAY >= 4) || ((CURRENT_DAY == 3) && (CURRENT_TIME >= (CLOCK_TIME(0, 0) + 5)) &&
                                  (CURRENT_TIME < CLOCK_TIME(6, 0)));
}

static void recomp_update_clock_texture_pack_state(PlayState* play, s32* previousTimeCheck) {
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    s32 timeUntilCrash;

    if (!recomp_should_draw_3ds_clock(play) || (play->pauseCtx.state != PAUSE_STATE_OFF) ||
        (play->pauseCtx.debugEditor != DEBUG_EDITOR_NONE)) {
        *previousTimeCheck = SECONDS_IN_THREE_DAYS - (s32)TIME_TO_SECONDS_F(TIME_UNTIL_MOON_CRASH);
        recomp_set_3ds_clock_state(false, 0, gSaveContext.save.day, (s32)TIME_TO_SECONDS_F(CURRENT_TIME),
                                   (s32)TIME_TO_SECONDS_F(TIME_UNTIL_MOON_CRASH),
                                   gSaveContext.save.timeSpeedOffset, false);
        return;
    }

    timeUntilCrash = recomp_get_3ds_clock_time_until_crash(previousTimeCheck);
    recomp_set_3ds_clock_state(true, interfaceCtx->bAlpha, gSaveContext.save.day, (s32)TIME_TO_SECONDS_F(CURRENT_TIME),
                               timeUntilCrash, gSaveContext.save.timeSpeedOffset, recomp_is_final_hours());
}

static void recomp_draw_2ship_3ds_clock(PlayState* play, s32* previousTimeCheck) {
    static s32 sPreviousTimeCheck = -1;
    InterfaceContext* interfaceCtx = &play->interfaceCtx;
    s32 timeUntilCrash;
    s16 sThreeDayClockAlpha;
    s16 posX = 160;
    s16 posY = 215;
    s32 fillAlpha;
    s32 timeOffset;
    u16 counterX;
    u16 counterY;
    u16 curMinutes;
    u16 curHours;

    if (!recomp_should_draw_3ds_clock(play)) {
        *previousTimeCheck = SECONDS_IN_THREE_DAYS - (s32)TIME_TO_SECONDS_F(TIME_UNTIL_MOON_CRASH);
        return;
    }

    if ((play->pauseCtx.state != PAUSE_STATE_OFF) || (play->pauseCtx.debugEditor != DEBUG_EDITOR_NONE)) {
        *previousTimeCheck = SECONDS_IN_THREE_DAYS - (s32)TIME_TO_SECONDS_F(TIME_UNTIL_MOON_CRASH);
        return;
    }

    sThreeDayClockAlpha = interfaceCtx->bAlpha;
    timeUntilCrash = recomp_get_3ds_clock_time_until_crash(previousTimeCheck);

    OPEN_DISPS(play->state.gfxCtx);

    Gfx_SetupDL42_Overlay(play->state.gfxCtx);
    gDPSetRenderMode(OVERLAY_DISP++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPSetAlphaCompare(OVERLAY_DISP++, G_AC_THRESHOLD);

    OVERLAY_DISP =
        Gfx_DrawTexRectIA8_DropShadow(OVERLAY_DISP, (TexturePtr)gThreeDayClock3DSEdgeTex,
                                      CLOCK_SECTION_HALFWIDTH * 3, 12, posX - (CLOCK_SECTION_HALFWIDTH * 4), posY,
                                      CLOCK_SECTION_HALFWIDTH * 3, 12, 1 << 10, 1 << 10, 255, 255, 255,
                                      sThreeDayClockAlpha);

    OVERLAY_DISP = Gfx_DrawTexRectIA8_DropShadow(OVERLAY_DISP, (TexturePtr)gThreeDayClock3DSMiddleTex,
                                                 CLOCK_SECTION_WIDTH, 12, posX - CLOCK_SECTION_HALFWIDTH, posY,
                                                 CLOCK_SECTION_WIDTH, 12, 1 << 10, 1 << 10, 255, 255, 255,
                                                 sThreeDayClockAlpha);

    OVERLAY_DISP = Gfx_DrawTexRectIA8_DropShadowOffset(
        OVERLAY_DISP, (TexturePtr)gThreeDayClock3DSEdgeTex, CLOCK_SECTION_HALFWIDTH * 3, 12,
        posX + CLOCK_SECTION_HALFWIDTH, posY, CLOCK_SECTION_HALFWIDTH * 3, 12, 1 << 10, 1 << 10, 255, 255, 255,
        sThreeDayClockAlpha, 3, (CLOCK_SECTION_HALFWIDTH * 3) << 5);

    fillAlpha = (gSaveContext.save.day <= 1) ? 255 : 64;
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 0, 128, 255, (fillAlpha * sThreeDayClockAlpha) / 255);
    OVERLAY_DISP = Gfx_DrawTexRectIA8(OVERLAY_DISP, (TexturePtr)gThreeDayClock3DSFillTex, CLOCK_SECTION_WIDTH, 12,
                                      posX - CLOCK_SECTION_HALFWIDTH - CLOCK_SECTION_WIDTH, posY, CLOCK_SECTION_WIDTH,
                                      12, 1 << 10, 1 << 10);

    fillAlpha = (gSaveContext.save.day == 2) ? 255 : 64;
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 192, 0, (fillAlpha * sThreeDayClockAlpha) / 255);
    OVERLAY_DISP = Gfx_DrawTexRectIA8(OVERLAY_DISP, (TexturePtr)gThreeDayClock3DSFillTex, CLOCK_SECTION_WIDTH, 12,
                                      posX - CLOCK_SECTION_HALFWIDTH, posY, CLOCK_SECTION_WIDTH, 12, 1 << 10,
                                      1 << 10);

    fillAlpha = (gSaveContext.save.day >= 3) ? 255 : 64;
    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 64, 0, (fillAlpha * sThreeDayClockAlpha) / 255);
    OVERLAY_DISP = Gfx_DrawTexRectIA8(OVERLAY_DISP, (TexturePtr)gThreeDayClock3DSFillTex, CLOCK_SECTION_WIDTH, 12,
                                      posX + CLOCK_SECTION_HALFWIDTH, posY, CLOCK_SECTION_WIDTH, 12, 1 << 10,
                                      1 << 10);

    timeOffset = recomp_max_s32(
        recomp_min_s32((3 * CLOCK_SECTION_WIDTH) - ((timeUntilCrash * 3 * CLOCK_SECTION_WIDTH) / SECONDS_IN_THREE_DAYS),
                       3 * CLOCK_SECTION_WIDTH),
        0);
    counterX = posX - (CLOCK_SECTION_HALFWIDTH * 3) + timeOffset;
    counterY = posY - 4;

    gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 200, 0, 0, sThreeDayClockAlpha);
    OVERLAY_DISP = Gfx_DrawTexRectIA8(OVERLAY_DISP, (TexturePtr)gThreeDayClock3DSArrowTex, 8, 8, counterX - 4,
                                      posY + 4, 8, 8, 1 << 10, 1 << 10);

    curMinutes = (s32)TIME_TO_MINUTES_F(CURRENT_TIME) % 60;
    curHours = (s32)TIME_TO_MINUTES_F(CURRENT_TIME) / 60;

    if (recomp_is_final_hours()) {
        static s32 sFinalHoursIntro = 0;
        s32 timeInSeconds = timeUntilCrash % 60;
        s32 timeInMinutes = (timeUntilCrash / 60) % 60;
        s32 timeInHours = (timeUntilCrash / 60) / 60;
        u16 finalTimerSpacing = 8;
        u16 finalTimerPos = posX - finalTimerSpacing * 4 - finalTimerSpacing / 2;
        s32 percentToCrash = recomp_min_s32(recomp_max_s32((timeUntilCrash * 255) / SECONDS_IN_SIX_HOURS, 0), 255);
        u16 finalHoursR = 255;
        u16 finalHoursG = (((255 - percentToCrash) * 0) + (percentToCrash * 255)) / 255;
        u16 finalHoursB = 0;
        s32 finalHoursOffset = 10;
        s32 finalHoursModifier = 2;
        s16 i;

        sFinalHoursClockSlots[0] = recomp_min_s32(timeInHours / 10, 9);
        sFinalHoursClockSlots[1] = timeInHours % 10;
        sFinalHoursClockSlots[2] = 10;
        sFinalHoursClockSlots[3] = timeInMinutes / 10;
        sFinalHoursClockSlots[4] = timeInMinutes % 10;
        sFinalHoursClockSlots[5] = 10;
        sFinalHoursClockSlots[6] = timeInSeconds / 10;
        sFinalHoursClockSlots[7] = timeInSeconds % 10;

        if (sFinalHoursIntro < finalHoursOffset * finalHoursModifier) {
            sFinalHoursIntro++;
        }

        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, finalHoursR, finalHoursG, finalHoursB,
                        (sThreeDayClockAlpha * sFinalHoursIntro) / (finalHoursOffset * finalHoursModifier));
        for (i = 0; i < 8; i++) {
            OVERLAY_DISP = Gfx_DrawTexRectIA8(OVERLAY_DISP, sFinalHoursDigitTextures[sFinalHoursClockSlots[i]], 16, 16,
                                              finalTimerPos,
                                              posY - 14 - finalHoursOffset + (sFinalHoursIntro / finalHoursModifier),
                                              16, 16, 1 << 10, 1 << 10);
            finalTimerPos += finalTimerSpacing;
        }

        OVERLAY_DISP =
            Gfx_DrawTexRectIA8(OVERLAY_DISP, (TexturePtr)gThreeDayClock3DSFinalHoursMoonTex, 16, 16, posX - 8,
                               posY - 28 - finalHoursOffset + (sFinalHoursIntro / finalHoursModifier), 16, 16,
                               1 << 10, 1 << 10);
    } else {
        TexturePtr dayNightMarker;
        u16 curTensHours;
        u16 curTensMinutes;
        u16 timerSpacing = 6;

        if (gSaveContext.save.timeSpeedOffset == -2) {
            u16 pulseTime = ((s32)TIME_TO_SECONDS_F(CURRENT_TIME) % 120);
            u16 pulse = (pulseTime < 60) ? ((pulseTime * 255) / 60) : (255 - ((pulseTime - 60) * 255) / 60);

            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, pulse, pulse, 0, sThreeDayClockAlpha / 2);
        } else {
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 0, 0, 0, sThreeDayClockAlpha / 2);
        }

        OVERLAY_DISP = Gfx_DrawTexRectIA8(OVERLAY_DISP, (TexturePtr)gThreeDayClock3DSTimeBackdropTex,
                                          CLOCK_SECTION_WIDTH, 16, counterX - CLOCK_SECTION_HALFWIDTH, counterY - 4,
                                          CLOCK_SECTION_WIDTH, 16, 1 << 10, 1 << 10);

        counterX -= 8;
        if ((curHours < 6) || (curHours >= 18)) {
            dayNightMarker = (TexturePtr)gThreeDayClockMoonHourTex;
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 200, 200, 0, sThreeDayClockAlpha);
        } else {
            dayNightMarker = (TexturePtr)gThreeDayClockSunHourTex;
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 200, 64, 0, sThreeDayClockAlpha);
        }
        OVERLAY_DISP =
            Gfx_DrawTexRectI8(OVERLAY_DISP, dayNightMarker, 24, 24, counterX - 11, counterY - 2, 12, 12, 1 << 11,
                              1 << 11);

        curHours %= 12;
        if (curHours == 0) {
            curHours = 12;
        }

        curTensHours = curHours / 10;
        curHours %= 10;
        curTensMinutes = curMinutes / 10;
        curMinutes %= 10;

        gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 255, 255, 255, sThreeDayClockAlpha);

        if (gSaveContext.save.timeSpeedOffset == -2) {
            OVERLAY_DISP = Gfx_DrawTexRectIA8(OVERLAY_DISP, (TexturePtr)gThreeDayClock3DSSlowTimeTex, 16, 16,
                                              counterX + 8 + 20, counterY - 4, 16, 16, 1 << 10, 1 << 10);
            gDPSetPrimColor(OVERLAY_DISP++, 0, 0, 64, 192, 255, sThreeDayClockAlpha);
        }

        if (curTensHours > 0) {
            timerSpacing = 4;
            OVERLAY_DISP = Gfx_DrawTexRectI8(OVERLAY_DISP, sDigitTextures[curTensHours], 8, 8, counterX, counterY, 8,
                                             8, 1 << 10, 1 << 10);
            counterX += timerSpacing;
        }

        OVERLAY_DISP = Gfx_DrawTexRectI8(OVERLAY_DISP, sDigitTextures[curHours], 8, 8, counterX, counterY, 8, 8,
                                         1 << 10, 1 << 10);
        counterX += timerSpacing;
        OVERLAY_DISP = Gfx_DrawTexRectI8(OVERLAY_DISP, sDigitTextures[10], 8, 8, counterX, counterY, 8, 8, 1 << 10,
                                         1 << 10);
        counterX += timerSpacing;
        OVERLAY_DISP = Gfx_DrawTexRectI8(OVERLAY_DISP, sDigitTextures[curTensMinutes], 8, 8, counterX, counterY, 8, 8,
                                         1 << 10, 1 << 10);
        counterX += timerSpacing;
        OVERLAY_DISP = Gfx_DrawTexRectI8(OVERLAY_DISP, sDigitTextures[curMinutes], 8, 8, counterX, counterY, 8, 8,
                                         1 << 10, 1 << 10);
    }

    gDPPipeSync(OVERLAY_DISP++);
    CLOSE_DISPS(play->state.gfxCtx);
}

void Recomp_Draw3DSClock(PlayState* play) {
    static s32 sPreviousTimeCheck = -1;

    if (recomp_should_use_3ds_clock_overlay()) {
        recomp_update_clock_texture_pack_state(play, &sPreviousTimeCheck);
    } else {
        recomp_set_3ds_clock_state(false, 0, gSaveContext.save.day, (s32)TIME_TO_SECONDS_F(CURRENT_TIME),
                                   (s32)TIME_TO_SECONDS_F(TIME_UNTIL_MOON_CRASH),
                                   gSaveContext.save.timeSpeedOffset, false);
        recomp_draw_2ship_3ds_clock(play, &sPreviousTimeCheck);
    }
}
