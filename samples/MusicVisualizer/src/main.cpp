//------------------------------------------------------------------------------
// File: main.cpp
//
// DirectSound Ogg music player with a Direct3D 8 spectrum visualizer — a
// multi-project RXDK sample.
//
// Streams D:\media\music.ogg through the MCPX APU (libdsound) using the "vorbis"
// static library (libs/vorbis, referenced via projectReferences) to decode. Each
// decoded PCM chunk is both fed to DirectSound and run through a small FFT; the
// resulting frequency bands drive a bar-graph equalizer drawn with Direct3D 8.
//
// media\music.ogg is deployed next to the XBE (deployPaths: ["media"]), landing at
// D:\media\music.ogg on the console.
//------------------------------------------------------------------------------

#include <xtl.h>
#include <dsound.h>
#include <d3d8.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "vorbis_stream.h" // exported by the referenced vorbis library

namespace {

// --- Direct3D 8 (2D bar rendering with pre-transformed vertices) --------------
struct BarVertex {
    FLOAT x, y, z, rhw;
    DWORD color;
};
constexpr DWORD kBarFvf = D3DFVF_XYZRHW | D3DFVF_DIFFUSE;

constexpr int kScreenW = 640;
constexpr int kScreenH = 480;
constexpr int kNumBars = 24;

LPDIRECT3D8 g_pD3D = NULL;
LPDIRECT3DDEVICE8 g_pDev = NULL;
LPDIRECT3DVERTEXBUFFER8 g_pBarVB = NULL;

float g_bar[kNumBars] = { 0.0f }; // current bar heights, 0..1 (smoothed)

// --- Audio: streaming Ogg through DirectSound ---------------------------------
#define AUDIO_PACKETS         12
#define AUDIO_OUTPUT_BUF_SIZE 4096 // bytes per packet (lower = tighter visual sync)

LPDIRECTSOUND g_pDS = NULL;
LPDIRECTSOUNDSTREAM g_pStream = NULL;
VorbisStream* g_vorbis = NULL;
int g_channels = 2;

char* g_decodeBuffer = NULL;
DWORD* g_packetStatus = NULL;
DWORD* g_completedSize = NULL;

// --- FFT spectrum analysis ----------------------------------------------------
constexpr int kFftSize = 512; // power of two
float g_re[kFftSize];
float g_im[kFftSize];

void Fft(float* re, float* im, int n)
{
    for (int i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            float tr = re[i]; re[i] = re[j]; re[j] = tr;
            float ti = im[i]; im[i] = im[j]; im[j] = ti;
        }
    }
    for (int len = 2; len <= n; len <<= 1) {
        float ang = -6.28318530718f / (float)len;
        float wr = cosf(ang), wi = sinf(ang);
        for (int i = 0; i < n; i += len) {
            float cr = 1.0f, ci = 0.0f;
            for (int k = 0; k < len / 2; k++) {
                int a = i + k, b = a + len / 2;
                float tr = cr * re[b] - ci * im[b];
                float ti = cr * im[b] + ci * re[b];
                re[b] = re[a] - tr; im[b] = im[a] - ti;
                re[a] += tr;        im[a] += ti;
                float ncr = cr * wr - ci * wi;
                ci = cr * wi + ci * wr;
                cr = ncr;
            }
        }
    }
}

// Turn one interleaved-PCM chunk into per-band target levels, then ease the bars
// toward them (instant rise, gravity fall) for a lively but smooth equalizer.
void AnalyzeChunk(const short* pcm, int shorts)
{
    int frames = g_channels > 0 ? shorts / g_channels : shorts;
    if (frames <= 0) {
        return;
    }
    int n = frames < kFftSize ? frames : kFftSize;

    for (int i = 0; i < kFftSize; i++) {
        float s = 0.0f;
        if (i < n) {
            if (g_channels >= 2) {
                s = 0.5f * ((float)pcm[i * g_channels] + (float)pcm[i * g_channels + 1]);
            } else {
                s = (float)pcm[i];
            }
            s *= (1.0f / 32768.0f);
            // Hann window to reduce spectral leakage.
            float w = 0.5f - 0.5f * cosf(6.28318530718f * (float)i / (float)(kFftSize - 1));
            s *= w;
        }
        g_re[i] = s;
        g_im[i] = 0.0f;
    }

    Fft(g_re, g_im, kFftSize);

    const int bins = kFftSize / 2;
    for (int b = 0; b < kNumBars; b++) {
        // Log-spaced bin range so low frequencies aren't crammed into one bar.
        float f0 = powf((float)bins, (float)b / (float)kNumBars);
        float f1 = powf((float)bins, (float)(b + 1) / (float)kNumBars);
        int lo = 1 + (int)f0;
        int hi = 1 + (int)f1;
        if (hi <= lo) {
            hi = lo + 1;
        }
        if (hi > bins) {
            hi = bins;
        }

        float peak = 0.0f;
        for (int k = lo; k < hi; k++) {
            float mag = sqrtf(g_re[k] * g_re[k] + g_im[k] * g_im[k]);
            if (mag > peak) {
                peak = mag;
            }
        }
        // Compress the range (music energy is very peaky) and normalize.
        float level = sqrtf(peak * (2.0f / (float)kFftSize)) * 2.2f;
        if (level > 1.0f) {
            level = 1.0f;
        }

        if (level > g_bar[b]) {
            g_bar[b] = level;           // instant attack
        } else {
            g_bar[b] -= 0.04f;          // gravity fall
            if (g_bar[b] < level) {
                g_bar[b] = level;
            }
            if (g_bar[b] < 0.0f) {
                g_bar[b] = 0.0f;
            }
        }
    }
}

HRESULT InitDevice()
{
    D3DPRESENT_PARAMETERS d3dpp;

    g_pD3D = Direct3DCreate8(D3D_SDK_VERSION);
    if (!g_pD3D) {
        return E_FAIL;
    }

    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.BackBufferWidth = kScreenW;
    d3dpp.BackBufferHeight = kScreenH;
    d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8;
    d3dpp.BackBufferCount = 1;
    d3dpp.Windowed = FALSE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.FullScreen_RefreshRateInHz = 60;
    d3dpp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

    HRESULT hr = Direct3D_CreateDevice(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        NULL,
        D3DCREATE_HARDWARE_VERTEXPROCESSING,
        &d3dpp,
        &g_pDev);
    if (FAILED(hr)) {
        return hr;
    }

    return g_pDev->CreateVertexBuffer(
        kNumBars * 6 * sizeof(BarVertex),
        D3DUSAGE_WRITEONLY,
        kBarFvf,
        D3DPOOL_MANAGED,
        &g_pBarVB);
}

HRESULT InitAudio()
{
    // TEMP diagnostic: check what the title actually sees on D:\ before
    // trying to open the file, since VorbisOpenFile fails silently.
    {
        DWORD attrD = GetFileAttributesA("D:\\");
        DWORD attrMedia = GetFileAttributesA("D:\\media");
        DWORD attrOgg = GetFileAttributesA("D:\\media\\music.ogg");
        char buf[192];
        sprintf(buf, "dsound-music: D:\\ attr=0x%08X media attr=0x%08X music.ogg attr=0x%08X (err=%u)\n",
                (unsigned)attrD, (unsigned)attrMedia, (unsigned)attrOgg, (unsigned)GetLastError());
        OutputDebugStringA(buf);

        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA("D:\\*", &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                char lineBuf[128];
                sprintf(lineBuf, "dsound-music: D:\\ entry: %s\n", fd.cFileName);
                OutputDebugStringA(lineBuf);
            } while (FindNextFileA(h, &fd));
            FindClose(h);
        } else {
            OutputDebugStringA("dsound-music: FindFirstFileA(D:\\*) failed\n");
        }
    }

    g_vorbis = VorbisOpenFile("D:\\media\\music.ogg");
    if (!g_vorbis) {
        OutputDebugStringA("dsound-music: could not open D:\\media\\music.ogg\n");
        return E_FAIL;
    }
    g_channels = VorbisChannels(g_vorbis);

    HRESULT hr = DirectSoundCreate(NULL, &g_pDS, NULL);
    if (FAILED(hr)) {
        return hr;
    }

    WAVEFORMATEX wfx;
    memset(&wfx, 0, sizeof(wfx));
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = (WORD)g_channels;
    wfx.nSamplesPerSec = VorbisSampleRate(g_vorbis);
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = (WORD)(g_channels * sizeof(short));
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

    DSSTREAMDESC sd;
    memset(&sd, 0, sizeof(sd));
    sd.dwMaxAttachedPackets = AUDIO_PACKETS;
    sd.lpwfxFormat = &wfx;

    hr = IDirectSound_CreateSoundStream(g_pDS, &sd, &g_pStream, NULL);
    if (FAILED(hr) || !g_pStream) {
        return FAILED(hr) ? hr : E_FAIL;
    }

    IDirectSoundStream_SetVolume(g_pStream, DSBVOLUME_MAX);
    IDirectSoundStream_SetHeadroom(g_pStream, 0);

    DSMIXBINVOLUMEPAIR mixPairs[2];
    mixPairs[0].dwMixBin = DSMIXBIN_FRONT_LEFT;  mixPairs[0].lVolume = DSBVOLUME_MAX;
    mixPairs[1].dwMixBin = DSMIXBIN_FRONT_RIGHT; mixPairs[1].lVolume = DSBVOLUME_MAX;
    DSMIXBINS mixBins;
    mixBins.dwMixBinCount = 2;
    mixBins.lpMixBinVolumePairs = mixPairs;
    IDirectSoundStream_SetMixBins(g_pStream, &mixBins);

    g_decodeBuffer = (char*)malloc(AUDIO_PACKETS * AUDIO_OUTPUT_BUF_SIZE);
    g_packetStatus = (DWORD*)malloc(AUDIO_PACKETS * sizeof(DWORD));
    g_completedSize = (DWORD*)malloc(AUDIO_PACKETS * sizeof(DWORD));
    if (!g_decodeBuffer || !g_packetStatus || !g_completedSize) {
        return E_OUTOFMEMORY;
    }
    memset(g_packetStatus, 0, AUDIO_PACKETS * sizeof(DWORD));
    memset(g_completedSize, 0, AUDIO_PACKETS * sizeof(DWORD));
    return S_OK;
}

// Refill every packet the APU has finished with; analyze the freshest chunk.
void PumpAudio()
{
    DirectSoundDoWork();
    for (int i = 0; i < AUDIO_PACKETS; i++) {
        if (g_packetStatus[i] == (DWORD)XMEDIAPACKET_STATUS_PENDING) {
            continue;
        }

        short* off = (short*)(g_decodeBuffer + i * AUDIO_OUTPUT_BUF_SIZE);
        int wantShorts = AUDIO_OUTPUT_BUF_SIZE / (int)sizeof(short);
        int shorts = VorbisReadLooping(g_vorbis, off, wantShorts);
        if (shorts <= 0) {
            continue;
        }
        int bytes = shorts * (int)sizeof(short);
        if (bytes < AUDIO_OUTPUT_BUF_SIZE) {
            memset((char*)off + bytes, 0, AUDIO_OUTPUT_BUF_SIZE - bytes);
        }

        AnalyzeChunk(off, shorts);

        XMEDIAPACKET pkt;
        memset(&pkt, 0, sizeof(pkt));
        pkt.pvBuffer = off;
        pkt.dwMaxSize = (DWORD)bytes;
        pkt.pdwCompletedSize = &g_completedSize[i];
        pkt.pdwStatus = &g_packetStatus[i];
        IDirectSoundStream_Process(g_pStream, &pkt, NULL);
    }
}

DWORD BarColor(int band, float height)
{
    // Spectrum hue across bars (blue lows -> red highs), brightened by level.
    float t = (float)band / (float)(kNumBars - 1);
    float bright = 0.35f + 0.65f * height;
    int r = (int)(255.0f * t * bright);
    int g = (int)(255.0f * (1.0f - fabsf(t - 0.5f) * 2.0f) * bright);
    int b = (int)(255.0f * (1.0f - t) * bright);
    return D3DCOLOR_XRGB(r, g, b);
}

void RenderBars()
{
    const float margin = 40.0f;
    const float usableW = (float)kScreenW - 2.0f * margin;
    const float slot = usableW / (float)kNumBars;
    const float barW = slot * 0.72f;
    const float baseY = 440.0f;
    const float maxH = 380.0f;

    BarVertex* v = NULL;
    if (SUCCEEDED(g_pBarVB->Lock(0, 0, (BYTE**)&v, 0))) {
        for (int i = 0; i < kNumBars; i++) {
            float h = g_bar[i] * maxH;
            float x0 = margin + (float)i * slot + (slot - barW) * 0.5f;
            float x1 = x0 + barW;
            float y0 = baseY - h;
            float y1 = baseY;
            DWORD c = BarColor(i, g_bar[i]);
            BarVertex* q = &v[i * 6];
            q[0].x = x0; q[0].y = y0; q[1].x = x1; q[1].y = y0; q[2].x = x1; q[2].y = y1;
            q[3].x = x0; q[3].y = y0; q[4].x = x1; q[4].y = y1; q[5].x = x0; q[5].y = y1;
            for (int k = 0; k < 6; k++) {
                q[k].z = 0.0f;
                q[k].rhw = 1.0f;
                q[k].color = c;
            }
        }
        g_pBarVB->Unlock();
    }

    g_pDev->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(10, 10, 20), 1.0f, 0);
    g_pDev->BeginScene();
    g_pDev->SetRenderState(D3DRS_ZENABLE, FALSE);
    g_pDev->SetRenderState(D3DRS_LIGHTING, FALSE);
    g_pDev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    g_pDev->SetStreamSource(0, g_pBarVB, sizeof(BarVertex));
    g_pDev->SetVertexShader(kBarFvf);
    g_pDev->DrawPrimitive(D3DPT_TRIANGLELIST, 0, kNumBars * 2);
    g_pDev->EndScene();
}

} // namespace

int main()
{
    OutputDebugStringA("dsound-music: starting\n");

    if (FAILED(InitDevice())) {
        OutputDebugStringA("dsound-music: D3D init failed\n");
        for (;;) { Sleep(1000); }
    }
    if (FAILED(InitAudio())) {
        OutputDebugStringA("dsound-music: audio init failed\n");
        for (;;) { Sleep(1000); }
    }

    OutputDebugStringA("dsound-music: playing + visualizing\n");
    for (;;) {
        PumpAudio();
        RenderBars();
        g_pDev->Present(NULL, NULL, NULL, NULL);
    }

    return 0;
}
