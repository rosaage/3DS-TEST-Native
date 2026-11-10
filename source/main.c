// Native 3DS input tester. Distributed under the repository's MIT license.
#include <3ds.h>
#include <citro2d.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "cpp.h"

static C2D_TextBuf textBuffer;
static u32 background, panel, ink, muted, accent, active;
static bool newModel;
static CppInput cppInput;
static bool audioReady;
static s16 *tone;
static ndspWaveBuf wave;

static void text(float x, float y, float scale, u32 color, const char *format, ...)
{
    char line[128];
    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    C2D_Text parsed;
    C2D_TextParse(&parsed, textBuffer, line);
    C2D_DrawText(&parsed, C2D_WithColor, x, y, 0.0f, scale, scale, color);
}

static void centered(float x, float y, float scale, u32 color, const char *label)
{
    C2D_Text parsed;
    float width;
    C2D_TextParse(&parsed, textBuffer, label);
    C2D_TextGetDimensions(&parsed, scale, scale, &width, NULL);
    C2D_DrawText(&parsed, C2D_WithColor, x - width / 2, y, 0.0f, scale, scale, color);
}

static void button(float x, float y, float width, const char *label,
                   u32 key, u32 held, bool available)
{
    bool down = available && (held & key);
    C2D_DrawRectSolid(x, y, 0, width, 25, down ? active : panel);
    centered(x + width / 2, y + 4, 0.45f,
             available ? ink : muted, label);
}

static float plotPosition(int value, bool cppRaw)
{
    // Only the visual marker is clamped; the printed values remain untouched.
    float position = cppRaw ? (value - 2048) / 2048.0f : value / 156.0f;
    return fmaxf(-1.0f, fminf(1.0f, position)) * 40.0f;
}

static void stick(float x, const char *label, circlePosition pos, bool available,
                  bool cppRaw, u32 held, u32 left, u32 right, u32 up, u32 down)
{
    const float y = 112;
    centered(x, 45, 0.55f, ink, label);
    C2D_DrawCircleSolid(x, y, 0, 47, muted);
    C2D_DrawCircleSolid(x, y, 0, 46, panel);
    C2D_DrawLine(x - 40, y, muted, x + 40, y, muted, 1, 0);
    C2D_DrawLine(x, y - 40, muted, x, y + 40, muted, 1, 0);
    if (available) {
        C2D_DrawCircleSolid(x + plotPosition(pos.dx, cppRaw), y - plotPosition(pos.dy, cppRaw),
                           0, 5, active);
        char values[64];
        snprintf(values, sizeof(values), cppRaw ? "X: %4d Y: %4d" : "X: %+d  Y: %+d", pos.dx, pos.dy);
        centered(x, 164, 0.48f, ink, values);
        char directions[64];
        snprintf(directions, sizeof(directions), "%s %s %s %s",
                 held & left ? "L" : "-", held & right ? "R" : "-",
                 held & up ? "U" : "-", held & down ? "D" : "-");
        centered(x, 188, 0.42f, accent, cppRaw ? "RAW 12-BIT / 0..4095" : directions);
    } else {
        centered(x, y - 9, 0.5f, muted, "WAITING");
        centered(x, 164, 0.42f, muted, "CPP not connected");
        if (R_FAILED(cppInput.error)) {
            char error[40];
            snprintf(error, sizeof(error), "IR error: %08lX", (unsigned long)cppInput.error);
            centered(x, 188, 0.37f, muted, error);
        } else {
            centered(x, 188, 0.40f, accent, "Press CPP ZL/ZR to wake");
        }
    }
}

static void initAudio(void)
{
    if (R_FAILED(ndspInit())) return;
    tone = linearAlloc(2048 * sizeof(s16));
    if (!tone) {
        ndspExit();
        return;
    }
    for (int i = 0; i < 2048; ++i) {
        float envelope = 1.0f - i / 2048.0f;
        tone[i] = (s16)(sinf(i * 2.0f * 3.14159265f * 660.0f / 22050.0f)
                        * 7000 * envelope);
    }
    DSP_FlushDataCache(tone, 2048 * sizeof(s16));
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, 22050);
    ndspChnSetFormat(0, NDSP_FORMAT_MONO_PCM16);
    float mix[12] = {1, 1};
    ndspChnSetMix(0, mix);
    audioReady = true;
}

static void beep(void)
{
    if (!audioReady) return;
    ndspChnWaveBufClear(0);
    memset(&wave, 0, sizeof(wave));
    wave.data_vaddr = tone;
    wave.nsamples = 2048;
    ndspChnWaveBufAdd(0, &wave);
}

static void drawTop(circlePosition circle, circlePosition cstick, u32 held)
{
    C2D_DrawRectSolid(0, 0, 0, 400, 34, panel);
    text(16, 5, 0.7f, ink, "3DS TEST");
    text(235, 12, 0.42f, accent, "LIVE ANALOG INPUT");
    stick(104, "CIRCLE PAD", circle, true, false, held,
          KEY_CPAD_LEFT, KEY_CPAD_RIGHT, KEY_CPAD_UP, KEY_CPAD_DOWN);
    stick(296, newModel ? "C-STICK" : "CIRCLE PAD PRO", cstick,
          newModel || cppInput.connected, !newModel, held,
          KEY_CSTICK_LEFT, KEY_CSTICK_RIGHT, KEY_CSTICK_UP, KEY_CSTICK_DOWN);
    centered(200, 216, 0.40f, muted, "RAW VALUES  /  +X RIGHT  /  +Y UP");
}

static void drawBottom(u32 held, touchPosition touch, bool exitHeld, int exitFrames)
{
    text(12, 5, 0.55f, ink, "BUTTON TEST");
    text(202, 9, 0.38f, audioReady ? accent : muted,
         audioReady ? "SOUND ON" : "SOUND UNAVAILABLE");
    button(12, 33, 60, "L", KEY_L, held, true);
    button(77, 33, 60, "ZL", KEY_ZL, held, newModel || cppInput.connected);
    button(183, 33, 60, "ZR", KEY_ZR, held, newModel || cppInput.connected);
    button(248, 33, 60, "R", KEY_R, held, true);
    button(43, 72, 42, "UP", KEY_DUP, held, true);
    button(12, 101, 49, "LEFT", KEY_DLEFT, held, true);
    button(66, 101, 49, "RIGHT", KEY_DRIGHT, held, true);
    button(43, 130, 42, "DOWN", KEY_DDOWN, held, true);
    button(244, 72, 32, "X", KEY_X, held, true);
    button(212, 101, 32, "Y", KEY_Y, held, true);
    button(276, 101, 32, "A", KEY_A, held, true);
    button(244, 130, 32, "B", KEY_B, held, true);
    button(125, 91, 73, "START", KEY_START, held, true);
    button(125, 123, 73, "SELECT", KEY_SELECT, held, true);
    C2D_DrawRectSolid(12, 166, 0, 296, 40, held & KEY_TOUCH ? active : panel);
    if (held & KEY_TOUCH)
        text(22, 175, 0.48f, ink, "TOUCH  X: %3u  Y: %3u", touch.px, touch.py);
    else
        text(22, 175, 0.48f, muted, "Touch the screen to test");
    centered(160, 216, 0.39f, muted, "Hold START + SELECT for 1s to exit");
    if (exitHeld)
        C2D_DrawRectSolid(12, 236, 0, 296 * (exitFrames / 60.0f), 3, active);
}

static void appHook(APT_HookType hook, void *user)
{
    (void)user;
    if (newModel) return;
    if (hook == APTHOOK_ONSUSPEND || hook == APTHOOK_ONSLEEP)
        cppSuspend(true);
    else if (hook == APTHOOK_ONRESTORE || hook == APTHOOK_ONWAKEUP)
        cppSuspend(false);
}

int main(void)
{
    gfxInitDefault();
    if (!C3D_Init(C3D_DEFAULT_CMDBUF_SIZE)) {
        gfxExit();
        return 1;
    }
    if (!C2D_Init(C2D_DEFAULT_MAX_OBJECTS)) {
        C3D_Fini();
        gfxExit();
        return 1;
    }
    C2D_Prepare();
    textBuffer = C2D_TextBufNew(4096);
    if (!textBuffer) {
        C2D_Fini();
        C3D_Fini();
        gfxExit();
        return 1;
    }
    C3D_RenderTarget *top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
    C3D_RenderTarget *bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);
    if (!top || !bottom) {
        C2D_TextBufDelete(textBuffer);
        C2D_Fini();
        C3D_Fini();
        gfxExit();
        return 1;
    }
    background = C2D_Color32(17, 22, 33, 255);
    panel = C2D_Color32(34, 43, 60, 255);
    ink = C2D_Color32(242, 245, 250, 255);
    muted = C2D_Color32(126, 144, 166, 255);
    accent = C2D_Color32(96, 202, 237, 255);
    active = C2D_Color32(29, 150, 108, 255);
    APT_CheckNew3DS(&newModel);
    // libctru initializes and scans IRRST with HID on supported systems.
    if (!newModel) cppInit();
    aptHookCookie hook;
    aptHook(&hook, appHook, NULL);
    initAudio();
    int exitFrames = 0;
    u32 previousHeld = 0;
    while (aptMainLoop()) {
        hidScanInput();
        u32 held = hidKeysHeld();
        circlePosition circle = {0}, cstick = {0};
        touchPosition touch = {0};
        hidCircleRead(&circle);
        if (newModel) hidCstickRead(&cstick);
        else {
            cppPoll(&cppInput);
            if (cppInput.connected) {
                cstick = cppInput.position;
                held |= cppInput.keys;
            }
        }
        u32 pressed = held & ~previousHeld;
        previousHeld = held;
        hidTouchRead(&touch);
        bool exitHeld = (held & (KEY_START | KEY_SELECT)) == (KEY_START | KEY_SELECT);
        exitFrames = exitHeld ? exitFrames + 1 : 0;
        if (exitFrames >= 60) break;
        if (pressed) beep();

        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TextBufClear(textBuffer);
        C2D_TargetClear(top, background);
        C2D_SceneBegin(top);
        drawTop(circle, cstick, held);
        C2D_TargetClear(bottom, background);
        C2D_SceneBegin(bottom);
        drawBottom(held, touch, exitHeld, exitFrames);
        C3D_FrameEnd(0);
    }
    aptUnhook(&hook);
    if (!newModel) cppExit();
    if (audioReady) {
        ndspChnWaveBufClear(0);
        ndspExit();
        linearFree(tone);
    }
    C2D_TextBufDelete(textBuffer);
    C2D_Fini();
    C3D_Fini();
    gfxExit();
    return 0;
}
