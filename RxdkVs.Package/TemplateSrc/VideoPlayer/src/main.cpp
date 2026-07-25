// XMV playback demo - loads test.xmv and plays it back in an endless loop,
// reopening the decoder from the start each time end-of-file is hit.
//
// Uses GetNextFrame + D3D overlay (XMVPlayer / vendor xmv/test pattern). Play()
// can finish instantly on clips with bad timing metadata (e.g. VideoB.xmv).
//
// DirectSound bring-up is non-fatal: the leak decoder's own XMV audio path is
// not yet implemented, so a missing/failed effects image just means silent
// video (see RXDK-Libs samples/xmv-play for the reference behavior).
//
// Assets live in media/ next to this project. dsstdfx.bin is baked into the
// XBE itself (rxdk.project.json's "embed" entry -> XAudioDownloadEffectsImage
// loads it from the embedded XBE section; no external-file fallback -- if the
// embed is missing/broken, DirectSound effects just don't come up, same as
// any other InitDirectSound failure).
// test.xmv is too large to embed, so it's deployed/staged as a loose file:
// deployPaths copies media/test.xmv to xe:\<project>\media\test.xmv on
// deploy, and stages the same path into the .iso on build.

#include <xtl.h>
#include <d3d8.h>
#include <dsstdfx.h>
#include <xmv.h>

#define XMV_PATH            "d:\\media\\test.xmv"
#define PLAY_TIMEOUT_MS     120000
#define MIN_FRAMES          10

static LPDIRECT3DDEVICE8 g_pd3dDevice = NULL;

static void DbgTrace(const char* msg)
{
    OutputDebugStringA(msg);
}

static void DbgTraceU(const char* label, unsigned v)
{
    char buf[80];
    int n = 0;
    const char* p;

    for (p = "xmv-hw: "; *p && n + 1 < (int)sizeof(buf); ++p, ++n) {
        buf[n] = *p;
    }
    for (p = label; *p && n + 1 < (int)sizeof(buf); ++p, ++n) {
        buf[n] = *p;
    }
    if (n + 12 < (int)sizeof(buf)) {
        buf[n++] = ' ';
        if (v >= 10000u) {
            buf[n++] = (char)('0' + (v / 10000u) % 10u);
        }
        if (v >= 1000u) {
            buf[n++] = (char)('0' + (v / 1000u) % 10u);
        }
        if (v >= 100u) {
            buf[n++] = (char)('0' + (v / 100u) % 10u);
        }
        if (v >= 10u) {
            buf[n++] = (char)('0' + (v / 10u) % 10u);
        }
        buf[n++] = (char)('0' + v % 10u);
        buf[n++] = '\n';
        buf[n] = '\0';
        OutputDebugStringA(buf);
    }
}

static void DbgTraceHr(const char* step, HRESULT hr)
{
    char buf[96];
    unsigned uhr;
    int n = 0;
    const char* p;

    for (p = "xmv-hw: "; *p && n + 1 < (int)sizeof(buf); ++p, ++n) {
        buf[n] = *p;
    }
    for (p = step; *p && n + 1 < (int)sizeof(buf); ++p, ++n) {
        buf[n] = *p;
    }
    if (n + 12 < (int)sizeof(buf)) {
        static const char hex[] = "0123456789ABCDEF";
        int i;

        buf[n++] = ' ';
        buf[n++] = 'h';
        buf[n++] = 'r';
        buf[n++] = '=';
        buf[n++] = '0';
        buf[n++] = 'x';
        uhr = (unsigned)hr;
        for (i = 7; i >= 0; --i) {
            buf[n++] = hex[(uhr >> (i * 4)) & 0xFu];
        }
        buf[n++] = '\n';
        buf[n] = '\0';
        OutputDebugStringA(buf);
    }
}

static HRESULT InitD3D(void)
{
    D3DPRESENT_PARAMETERS d3dpp;
    HRESULT hr;

    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.BackBufferWidth = 640;
    d3dpp.BackBufferHeight = 480;
    d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8;
    d3dpp.BackBufferCount = 1;
    d3dpp.Windowed = FALSE;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

    Direct3D_SetPushBufferSize(512 * 1024, (512 * 1024) / 16);
    hr = Direct3D_CreateDevice(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        NULL,
        D3DCREATE_HARDWARE_VERTEXPROCESSING,
        &d3dpp,
        &g_pd3dDevice);
    if (FAILED(hr)) {
        DbgTraceHr("Direct3D_CreateDevice failed", hr);
        return hr;
    }

    g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xFF202040, 1.0f, 0);
    g_pd3dDevice->Present(NULL, NULL, NULL, NULL);
    DbgTrace("xmv-hw: D3D device created\n");
    return S_OK;
}

static HRESULT InitDirectSound(void)
{
    DSEFFECTIMAGELOC effectLoc;
    LPDIRECTSOUND pDs = NULL;
    HRESULT hr;

    hr = DirectSoundCreate(NULL, &pDs, NULL);
    if (FAILED(hr)) {
        DbgTraceHr("DirectSoundCreate failed", hr);
        return hr;
    }
    DbgTrace("xmv-hw: DirectSoundCreate ok\n");

    effectLoc.dwI3DL2ReverbIndex = GraphI3DL2_I3DL2Reverb;
    effectLoc.dwCrosstalkIndex = GraphXTalk_XTalk;

    // Loaded from the XBE section imagebld /INSERTFILE bakes in for the
    // "embed" entry in rxdk.project.json -- no external-file fallback.
    hr = XAudioDownloadEffectsImage(
        "dsstdfx", &effectLoc, XAUDIO_DOWNLOADFX_XBESECTION, NULL);
    if (FAILED(hr)) {
        DbgTraceHr("XAudioDownloadEffectsImage (XBESECTION) failed", hr);
        DbgTrace("xmv-hw: check rxdk.project.json's \"embed\" entry for dsstdfx\n");
        return hr;
    }
    DbgTrace("xmv-hw: effects image loaded from embedded XBE section\n");
    return S_OK;
}

// Plays test.xmv once, start to finish. Returns S_OK on a normal end-of-file
// (the caller loops back and reopens the decoder to replay from the start) or
// a failure HRESULT on a real decode/device error (the caller hangs).
static HRESULT PlayXmvOnce(BOOL audioReady)
{
    XMVDecoder* pDecoder = NULL;
    XMVVIDEO_DESC videoDesc;
    LPDIRECT3DSURFACE8 pSurfaceShow = NULL;
    LPDIRECT3DSURFACE8 pSurfaceDraw = NULL;
    LPDIRECT3DSURFACE8 pSurfaceSwap = NULL;
    RECT sourceRect;
    RECT destRect;
    XMVRESULT xr;
    DWORD frameCount = 0;
    DWORD elapsedMs = 0;
    BOOL overlayEnabled = FALSE;
    HRESULT hr;

    hr = XMVDecoder_CreateDecoderForFile(XMVFLAG_NONE, XMV_PATH, &pDecoder);
    if (FAILED(hr)) {
        DbgTraceHr("CreateDecoderForFile failed", hr);
        DbgTrace("xmv-hw: missing media\\test.xmv (deploy media\\ or boot ISO)\n");
        return hr;
    }
    DbgTrace("xmv-hw: decoder opened\n");

    XMVDecoder_GetVideoDescriptor(pDecoder, &videoDesc);
    DbgTraceU("video width", videoDesc.Width);
    DbgTraceU("video height", videoDesc.Height);
    DbgTraceU("video fps", videoDesc.FramesPerSecond);
    DbgTraceU("audio streams", videoDesc.AudioStreamCount);

    if (videoDesc.Width > 0 && videoDesc.Height > 0) {
        hr = g_pd3dDevice->CreateImageSurface(
            videoDesc.Width, videoDesc.Height, D3DFMT_YUY2, &pSurfaceShow);
        if (FAILED(hr)) {
            DbgTraceHr("CreateImageSurface show failed", hr);
            XMVDecoder_CloseDecoder(pDecoder);
            return hr;
        }
        hr = g_pd3dDevice->CreateImageSurface(
            videoDesc.Width, videoDesc.Height, D3DFMT_YUY2, &pSurfaceDraw);
        if (FAILED(hr)) {
            DbgTraceHr("CreateImageSurface draw failed", hr);
            XMVDecoder_CloseDecoder(pDecoder);
            return hr;
        }

        sourceRect.left = 0;
        sourceRect.top = 0;
        sourceRect.right = (LONG)videoDesc.Width;
        sourceRect.bottom = (LONG)videoDesc.Height;
        destRect.left = 0;
        destRect.top = 0;
        destRect.right = 640;
        destRect.bottom = 480;
    }

    if (videoDesc.AudioStreamCount > 0) {
        // Non-fatal: the leak decoder's XMV audio path is not yet implemented
        // (EnableAudioStream returns E_NOTIMPL for most clips), so a failure
        // here just means silent video rather than aborting playback.
        hr = XMVDecoder_EnableAudioStream(pDecoder, 0, 0, NULL, NULL);
        if (FAILED(hr)) {
            DbgTraceHr("EnableAudioStream failed (video-only)", hr);
        } else {
            DbgTrace("xmv-hw: audio stream 0 enabled\n");
        }
    }

    DbgTrace("xmv-hw: decoding started\n");
    for (;;) {
        LPDIRECT3DSURFACE8 decodeTarget = pSurfaceDraw;

        hr = XMVDecoder_GetNextFrame(pDecoder, decodeTarget, &xr, NULL);
        if (FAILED(hr)) {
            DbgTraceHr("GetNextFrame failed", hr);
            if (overlayEnabled) {
                g_pd3dDevice->EnableOverlay(FALSE);
            }
            XMVDecoder_CloseDecoder(pDecoder);
            return hr;
        }

        switch (xr) {
        case XMV_NOFRAME:
            break;

        case XMV_NEWFRAME:
            ++frameCount;
            if (pSurfaceDraw && pSurfaceShow) {
                pSurfaceSwap = pSurfaceShow;
                pSurfaceShow = pSurfaceDraw;
                pSurfaceDraw = pSurfaceSwap;

                if (!overlayEnabled) {
                    g_pd3dDevice->EnableOverlay(TRUE);
                    overlayEnabled = TRUE;
                }

                while (!g_pd3dDevice->GetOverlayUpdateStatus()) {
                    ;
                }

                g_pd3dDevice->UpdateOverlay(
                    pSurfaceShow, &sourceRect, &destRect, FALSE, 0);
            }
            break;

        case XMV_ENDOFFILE:
            if (overlayEnabled) {
                g_pd3dDevice->EnableOverlay(FALSE);
            }
            DbgTraceU("frames decoded", frameCount);
            DbgTraceU("elapsed ms", elapsedMs);
            XMVDecoder_CloseDecoder(pDecoder);
            if (videoDesc.Width > 0 && frameCount < MIN_FRAMES) {
                DbgTrace("xmv-hw: too few frames (use SimpleXMV test.xmv)\n");
                return E_FAIL;
            }
            DbgTrace("xmv-hw: playback finished\n");
            return S_OK;

        case XMV_FAIL:
            if (overlayEnabled) {
                g_pd3dDevice->EnableOverlay(FALSE);
            }
            DbgTrace("xmv-hw: decode failed\n");
            XMVDecoder_CloseDecoder(pDecoder);
            return E_FAIL;
        }

        if (audioReady) {
            DirectSoundDoWork();
        }
        g_pd3dDevice->BlockUntilVerticalBlank();
        elapsedMs += 16;
        if (elapsedMs >= PLAY_TIMEOUT_MS) {
            if (overlayEnabled) {
                g_pd3dDevice->EnableOverlay(FALSE);
            }
            DbgTraceU("decode timeout ms", elapsedMs);
            XMVDecoder_CloseDecoder(pDecoder);
            return E_FAIL;
        }
    }
}

int main(void)
{
    HRESULT hr;
    BOOL audioReady;

    DbgTrace("xmv-hw: starting XMV loop demo\n");

    hr = InitD3D();
    if (FAILED(hr)) {
        DbgTrace("xmv-hw: device init failed\n");
        for (;;) {
            Sleep(1000);
        }
    }

    // Non-fatal: a missing/failed effects image just means silent video (the
    // leak decoder's own XMV audio path isn't implemented yet anyway).
    audioReady = SUCCEEDED(InitDirectSound());
    if (!audioReady) {
        DbgTrace("xmv-hw: continuing without DirectSound (video only)\n");
    }

    for (;;) {
        hr = PlayXmvOnce(audioReady);
        if (FAILED(hr)) {
            DbgTrace("xmv-hw: playback failed\n");
            for (;;) {
                Sleep(1000);
            }
        }
        DbgTrace("xmv-hw: end of file -- looping\n");
    }

    return 0;
}
