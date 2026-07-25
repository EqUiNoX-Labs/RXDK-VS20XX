//------------------------------------------------------------------------------
// File: main.cpp
//
// XFONT bitmap-font text rendering -- demoscene-style scroller sample for
// RXDK's libxfont text-rendering library.
//
// Opens the library's embedded default font (XFONT_OpenDefaultFont -- zero
// external assets, exercises the full memory-loading engine) and renders a
// scrolling ticker message: each character is drawn into a small offscreen
// buffer via TextOutToMemory, then stretch-blitted onto the back buffer at a
// size that pulses with a per-character sine wave (bitmap glyphs have no
// native scale API, so this is done by hand -- render small, blit big). The
// same sine phase drives a vertical bounce, and the text color cycles
// through the rainbow. A green "hacker terminal" readout types itself out in
// the background behind the scroller.
//------------------------------------------------------------------------------

#include <xtl.h>
#include <stdio.h>
#include <math.h>
#include <guiddef.h>
#include <d3d8.h>
#include <xfont.h>

namespace {

constexpr int kScreenW = 640;
constexpr int kScreenH = 480;

// Offscreen per-character render target for TextOutToMemory. Sized with
// headroom above the default font's ~24px cell height for bearing/descender
// overhang. Same D3DFMT_X8R8G8B8 layout as the back buffer, so the manual
// stretch-blit below is a plain DWORD copy with no per-pixel format conversion.
constexpr int kGlyphBufW = 56;
constexpr int kGlyphBufH = 56;
DWORD g_glyphBuf[kGlyphBufW * kGlyphBufH];

const WCHAR g_message[] =
    L"RXDK XFONT DEMO -- HELLO XBOX HOMEBREW -- BITMAP FONTS, SINE BOUNCE, "
    L"STRETCH-BLIT SCALING AND RAINBOW COLOR CYCLING, ALL RENDERED WITH THE "
    L"EMBEDDED DEFAULT FONT -- GREETZ TO THE SCENE -- ";

constexpr int kHackerLineCount = 6;
constexpr int kHackerLineMaxLen = 64;

const WCHAR* const g_hackerLines[kHackerLineCount] = {
    L"root@xbox:~# bypassing kernel signature check",
    L"injecting payload into xboxkrnl.exe",
    L"decrypting eeprom.......... ACCESS OK",
    L"scanning MCPX rom for vulnerabilities",
    L"mounting hidden partition.......... DONE",
    L"establishing reverse shell to devkit",
};

LPDIRECT3D8 g_pD3D = NULL;
LPDIRECT3DDEVICE8 g_pd3dDevice = NULL;
XFONT* g_pFont = NULL;

void Cleanup()
{
    if (g_pFont) {
        g_pFont->Release();
        g_pFont = NULL;
    }
    if (g_pd3dDevice) {
        g_pd3dDevice->Release();
        g_pd3dDevice = NULL;
    }
    if (g_pD3D) {
        g_pD3D->Release();
        g_pD3D = NULL;
    }
}

HRESULT InitDevice()
{
    D3DPRESENT_PARAMETERS d3dpp;
    HRESULT hr;

    printf("Creating Direct3D 8 interface\n");
    g_pD3D = Direct3DCreate8(D3D_SDK_VERSION);
    if (!g_pD3D) {
        printf("Failed to create Direct3D 8 interface\n");
        return E_FAIL;
    }

    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.BackBufferWidth = kScreenW;
    d3dpp.BackBufferHeight = kScreenH;
    d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8;
    d3dpp.BackBufferCount = 1;
    d3dpp.Windowed = FALSE;
    d3dpp.EnableAutoDepthStencil = FALSE;   // 2D text only, no Z-buffer needed
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.FullScreen_RefreshRateInHz = 60;
    d3dpp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

    Direct3D_SetPushBufferSize(512 * 1024, (512 * 1024) / 16);

    printf("Creating HAL device (640x480)\n");
    hr = Direct3D_CreateDevice(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        NULL,
        D3DCREATE_HARDWARE_VERTEXPROCESSING,
        &d3dpp,
        &g_pd3dDevice);
    if (FAILED(hr)) {
        return hr;
    }

    D3DVIEWPORT8 vp = {};
    vp.Width = d3dpp.BackBufferWidth;
    vp.Height = d3dpp.BackBufferHeight;
    vp.MinZ = 0.0f;
    vp.MaxZ = 1.0f;
    hr = g_pd3dDevice->SetViewport(&vp);
    if (FAILED(hr)) {
        return hr;
    }

    printf("Device created\n");
    return S_OK;
}

D3DCOLOR HueToColor(float hue)
{
    float h6 = hue * 6.0f;
    int i = (int)h6;
    float f = h6 - (float)i;
    float q = 1.0f - f;
    BYTE r, g, b;

    switch (i % 6) {
    default:
    case 0: r = 255;                g = (BYTE)(f * 255.0f); b = 0;                  break;
    case 1: r = (BYTE)(q * 255.0f); g = 255;                b = 0;                  break;
    case 2: r = 0;                  g = 255;                b = (BYTE)(f * 255.0f); break;
    case 3: r = 0;                  g = (BYTE)(q * 255.0f); b = 255;                break;
    case 4: r = (BYTE)(f * 255.0f); g = 0;                  b = 255;                break;
    case 5: r = 255;                g = 0;                  b = (BYTE)(q * 255.0f); break;
    }

    return 0xFF000000u | ((DWORD)r << 16) | ((DWORD)g << 8) | (DWORD)b;
}

void DrawHackerBackground(LPDIRECT3DSURFACE8 pBackBuffer, float t)
{
    const D3DCOLOR hackerGreen = 0xFF33FF33u;
    g_pFont->SetTextColor(hackerGreen);

    for (int line = 0; line < kHackerLineCount; line++) {
        const WCHAR* phrase = g_hackerLines[line];
        WCHAR buf[kHackerLineMaxLen + 1];
        int phraseLen = 0;
        while (phrase[phraseLen] && phraseLen < kHackerLineMaxLen) {
            phraseLen++;
        }

        // Type out, hold fully typed for a while, then restart from empty.
        // Each line has its own speed/offset so they don't retype in lockstep.
        const float lineSpeed = 12.0f;
        const float lineOffset = (float)line * 37.0f;
        float cyclePos = fmodf(t * lineSpeed + lineOffset, (float)(phraseLen + 25));
        int typedLen = (int)cyclePos;
        if (typedLen > phraseLen) typedLen = phraseLen;
        if (typedLen < 0) typedLen = 0;

        for (int i = 0; i < typedLen; i++) {
            buf[i] = phrase[i];
        }
        // Blinking cursor while still typing; blank once the line is complete.
        buf[typedLen] = (typedLen < phraseLen && fmodf(t, 0.6f) < 0.3f) ? L'_' : L' ';
        buf[typedLen + 1] = 0;

        int y = 300 + line * 22;
        g_pFont->TextOut(pBackBuffer, buf, (unsigned)-1, 20, y);
    }
}

void BlitGlyphStretched(LPDIRECT3DSURFACE8 pBackBuffer, int srcW, int srcH,
                         int dstX, int dstY, int dstW, int dstH)
{
    if (dstW <= 0 || dstH <= 0 || srcW <= 0 || srcH <= 0) {
        return;
    }

    D3DLOCKED_RECT lock;
    pBackBuffer->LockRect(&lock, NULL, 0);
    DWORD* pDst = (DWORD*)lock.pBits;
    int pitchPixels = lock.Pitch / sizeof(DWORD);

    for (int y = 0; y < dstH; y++) {
        int dy = dstY + y;
        if (dy < 0 || dy >= kScreenH) continue;

        int sy = (y * srcH) / dstH;
        if (sy >= srcH) sy = srcH - 1;

        for (int x = 0; x < dstW; x++) {
            int dx = dstX + x;
            if (dx < 0 || dx >= kScreenW) continue;

            int sx = (x * srcW) / dstW;
            if (sx >= srcW) sx = srcW - 1;

            // The offscreen buffer is cleared to 0 before each glyph render
            // and XFONT paints with a transparent background by default, so
            // any pixel still 0 here was never touched by the glyph -- skip
            // it so the scrolling background shows through instead of a
            // black box.
            DWORD pixel = g_glyphBuf[sy * kGlyphBufW + sx];
            if (pixel != 0) {
                pDst[dy * pitchPixels + dx] = pixel;
            }
        }
    }

    pBackBuffer->UnlockRect();
}

void RenderFrame(float t)
{
    int msgLen = (int)(sizeof(g_message) / sizeof(WCHAR)) - 1;
    unsigned totalWidth;
    g_pFont->GetTextExtent(g_message, msgLen, &totalWidth);

    LPDIRECT3DSURFACE8 pBackBuffer = NULL;
    g_pd3dDevice->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer);

    g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
    g_pd3dDevice->BeginScene();

    // Background layer first: the typing "hacker terminal" readout, drawn at
    // native size directly to the surface. The colorful scroller below is
    // drawn on top of it every frame.
    DrawHackerBackground(pBackBuffer, t);

    // Ticker-tape wraparound: draw two copies of the message back-to-back so
    // the screen never shows a gap while one copy scrolls off and the next
    // scrolls on. The repeat period must cover the FULL travel distance --
    // from fully off-screen right (x = kScreenW) to fully off-screen left
    // (x = -totalWidth) -- not just the message's own width, or a copy would
    // wrap back to the right edge while its trailing end was still visible.
    const float scrollSpeed = 90.0f;
    const float period = (float)kScreenW + (float)totalWidth + 40.0f;

    for (int copy = 0; copy < 2; copy++) {
        float baseX = kScreenW - fmodf(t * scrollSpeed, period) + copy * period;
        float x = baseX;

        if (x > (float)kScreenW || x + (float)totalWidth < 0.0f) {
            continue;
        }

        for (unsigned ich = 0; ich < (unsigned)msgLen; ich++) {
            WCHAR wch = g_message[ich];
            float phase = t * 4.0f + (float)ich * 0.5f;
            float bounce = sinf(phase) * 10.0f;
            float scale = 1.0f + 0.45f * sinf(phase * 0.7f);
            float hue = fmodf(t * 0.15f + (float)ich * 0.04f, 1.0f);
            if (scale < 0.5f) scale = 0.5f;

            unsigned advance;
            g_pFont->GetTextExtent(&wch, 1, &advance);

            int dstW = (int)((float)kGlyphBufW * scale * 0.6f);
            int dstH = (int)((float)kGlyphBufH * scale * 0.6f);
            int dstX = (int)x;
            int dstY = (int)(40.0f + bounce);

            if (dstX + dstW >= 0 && dstX < kScreenW && wch != L' ') {
                ZeroMemory(g_glyphBuf, sizeof(g_glyphBuf));
                g_pFont->SetTextColor(HueToColor(hue));
                g_pFont->TextOutToMemory(g_glyphBuf, kGlyphBufW * sizeof(DWORD),
                                         kGlyphBufW, kGlyphBufH, D3DFMT_X8R8G8B8,
                                         &wch, 1, 4, 4);
                BlitGlyphStretched(pBackBuffer, kGlyphBufW, kGlyphBufH, dstX, dstY, dstW, dstH);
            }

            x += (float)advance;
        }
    }

    g_pd3dDevice->EndScene();
    pBackBuffer->Release();
}

} // namespace

int main()
{
    HRESULT hr;

    printf("Starting sample\n");

    hr = InitDevice();
    if (FAILED(hr)) {
        printf("Failed to initialize device (hr=0x%08X)\n", (unsigned)hr);
        Cleanup();
        return 1;
    }

    printf("Opening default bitmap font\n");
    hr = XFONT_OpenDefaultFont(&g_pFont);
    if (FAILED(hr)) {
        printf("Failed to open default font (hr=0x%08X)\n", (unsigned)hr);
        Cleanup();
        return 1;
    }

    printf("Entering render loop\n");
    float t = 0.0f;
    while (true) {
        RenderFrame(t);
        g_pd3dDevice->Present(NULL, NULL, NULL, NULL);
        t += 0.033f;
    }

    return 0;
}
