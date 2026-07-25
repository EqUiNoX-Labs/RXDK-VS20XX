//------------------------------------------------------------------------------
// File: main.c
//
// RXDK network sample -- brings up the XNet stack (the MCPX NIC), waits for a
// DHCP lease, then hosts a tiny single-page HTTP server. Once the network is
// up, the screen shows the kit's IP address via libxfont so you know which
// URL to open in a browser without having to watch debug output.
//
// Written in plain C against the D3D8/XFONT free-function API (the same style
// samples/xfont-smoke uses) rather than the C++ COM style the other templates
// use: libxnet's prebuilt object code targets the MSVC C++ ABI internally, so
// keeping this whole title C-only sidesteps any ABI mismatch with the
// GNU-ABI D3D8/XFONT vtables.
//
// XNet + Berkeley socket declarations come from the real <winsockx.h>, same
// as an original XDK title -- no hand-rolled externs. winsockx.h's own
// #include <windows.h> is guarded by #ifndef _INC_WINDOWS; <xtl.h> doesn't
// define that guard (it stops short of pulling all of windows.h), so it's
// defined by hand first, exactly like RXDK's own xapi.h does for the same
// reason. Verified this leaves nothing else missing (D3D8/XFONT already
// worked via <xtl.h> alone).
//------------------------------------------------------------------------------

#include <xtl.h>
#include <xfont.h>
#include <stdio.h>
#include <wchar.h>    // swprintf for the on-screen URL text

#ifndef _INC_WINDOWS
#define _INC_WINDOWS
#endif
#include <winsockx.h>

#define SCREEN_W 640
#define SCREEN_H 480

// ---------------------------------------------------------------------------
// Tiny single-page HTTP/1.0 server. Every socket is non-blocking; the accept()
// poll and the render loop share one frame tick (see main), so nothing here
// ever blocks the thread that keeps the DPC-driven stack alive.
// ---------------------------------------------------------------------------

static const char g_page[] =
    "<!DOCTYPE html><html><head><meta charset=\"utf-8\">"
    "<title>RXDK Xbox</title></head>"
    "<body style=\"font-family:Segoe UI,sans-serif;text-align:center;"
    "background:#107C10;color:#fff;padding-top:12%\">"
    "<h1>&#x2714; Hello from the Xbox</h1>"
    "<p>This page is served by <b>libxnet</b> running on an original Xbox.</p>"
    "</body></html>";

// Send the whole buffer, polling past WSAEWOULDBLOCK so we never block the stack.
static int SendAll(SOCKET s, const char *buf, int len)
{
    int off = 0, n, idle = 0;
    while (off < len) {
        n = send(s, buf + off, len - off, 0);
        if (n > 0) { off += n; idle = 0; continue; }
        if (WSAGetLastError() != WSAEWOULDBLOCK)
            return -1;                       // real error
        if (++idle > 2000)
            return -1;                       // ~4s with nothing drained -> give up
        KeStallExecutionProcessor(2000);     // 2ms; lets the TX DPC drain the socket
    }
    return 0;
}

// Read and discard the request (we serve the same page for any GET), then write
// the response. Returns when the exchange is done.
static void HandleClient(SOCKET cli)
{
    unsigned long nb = 1;
    char  req[512], hdr[160];
    int   n, idle = 0, got = 0, hlen;

    ioctlsocket(cli, FIONBIO, &nb);

    while (got < 8192) {
        n = recv(cli, req, sizeof(req) - 1, 0);
        if (n > 0) {
            req[n] = 0; got += n; idle = 0;
            if (got >= 4 && (req[n-1] == '\n'))   // crude end-of-headers heuristic
                break;
        } else if (n == 0) {
            break;                            // peer closed
        } else {
            if (WSAGetLastError() != WSAEWOULDBLOCK) break;
            if (++idle > 200) break;          // ~1s with no request -> just respond
            KeStallExecutionProcessor(5000);  // 5ms
        }
    }

    hlen = sprintf(hdr,
                   "HTTP/1.0 200 OK\r\n"
                   "Content-Type: text/html\r\n"
                   "Content-Length: %d\r\n"
                   "Connection: close\r\n\r\n",
                   (int)(sizeof(g_page) - 1));

    if (SendAll(cli, hdr, hlen) == 0)
        SendAll(cli, g_page, (int)(sizeof(g_page) - 1));
}

// Bind + listen on :80, non-blocking. Returns INVALID_SOCKET on failure.
static SOCKET StartHttpServer(void)
{
    SOCKET srv;
    struct sockaddr_in addr;
    unsigned long nb = 1;
    int yes = 1;
    int i;

    srv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (srv == INVALID_SOCKET) {
        printf("net: socket() failed (err=%d)\n", WSAGetLastError());
        return INVALID_SOCKET;
    }
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof(yes));

    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    addr.sin_addr.s_addr = INADDR_ANY;
    for (i = 0; i < (int)sizeof(addr.sin_zero); i++) addr.sin_zero[i] = 0;

    if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        printf("net: bind(:80) failed (err=%d)\n", WSAGetLastError());
        closesocket(srv);
        return INVALID_SOCKET;
    }
    if (listen(srv, 4) == SOCKET_ERROR) {
        printf("net: listen() failed (err=%d)\n", WSAGetLastError());
        closesocket(srv);
        return INVALID_SOCKET;
    }
    ioctlsocket(srv, FIONBIO, &nb);   // non-blocking accept (don't starve DPCs)
    return srv;
}

// ---------------------------------------------------------------------------
// D3D8 + XFONT: tell the user where to point their browser.
// ---------------------------------------------------------------------------

static D3DDevice *g_pDevice;

static int InitDevice(void)
{
    D3DPRESENT_PARAMETERS pp;
    Direct3D *pD3D;
    D3DVIEWPORT8 vp;
    HRESULT hr;

    pD3D = Direct3DCreate8(D3D_SDK_VERSION);
    if (!pD3D) {
        printf("net: Direct3DCreate8 failed\n");
        return 0;
    }

    ZeroMemory(&pp, sizeof(pp));
    pp.BackBufferWidth = SCREEN_W;
    pp.BackBufferHeight = SCREEN_H;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferCount = 1;
    pp.Windowed = FALSE;
    pp.EnableAutoDepthStencil = FALSE;   // 2D text only, no Z-buffer needed
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.FullScreen_RefreshRateInHz = 60;
    pp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;

    Direct3D_SetPushBufferSize(512 * 1024, (512 * 1024) / 16);

    hr = Direct3D_CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, NULL,
                               D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &g_pDevice);
    if (FAILED(hr)) {
        printf("net: CreateDevice failed hr=0x%08x\n", (unsigned)hr);
        return 0;
    }

    ZeroMemory(&vp, sizeof(vp));
    vp.Width = pp.BackBufferWidth;
    vp.Height = pp.BackBufferHeight;
    vp.MinZ = 0.0f;
    vp.MaxZ = 1.0f;
    D3DDevice_SetViewport(&vp);

    return 1;
}

static void RenderFrame(XFONT *pFont, BOOL haveAddress, const unsigned char *o,
                        BOOL linkUp, DWORD hits)
{
    D3DSurface *pBackBuffer;
    WCHAR line[96];

    D3DDevice_GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer);

    D3DDevice_Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 16, 0), 1.0f, 0);
    D3DDevice_BeginScene();

    XFONT_SetTextColor(pFont, 0xFFFFFFFFu);
    XFONT_TextOut(pFont, pBackBuffer, L"RXDK NETWORK SAMPLE", (unsigned)-1, 40, 40);

    if (haveAddress) {
        XFONT_SetTextColor(pFont, 0xFF33FF33u);
        swprintf(line, 96, L"Network up (%s)", linkUp ? L"link active" : L"link down");
        XFONT_TextOut(pFont, pBackBuffer, line, (unsigned)-1, 40, 90);

        XFONT_SetTextColor(pFont, 0xFFFFFFFFu);
        XFONT_TextOut(pFont, pBackBuffer, L"Open this address in a browser on your PC:",
                     (unsigned)-1, 40, 140);

        XFONT_SetTextColor(pFont, 0xFFFFFF33u);
        swprintf(line, 96, L"http://%u.%u.%u.%u/", o[0], o[1], o[2], o[3]);
        XFONT_TextOut(pFont, pBackBuffer, line, (unsigned)-1, 40, 180);

        XFONT_SetTextColor(pFont, 0xFFAAAAAAu);
        swprintf(line, 96, L"Requests served: %lu", (unsigned long)hits);
        XFONT_TextOut(pFont, pBackBuffer, line, (unsigned)-1, 40, 230);
    } else {
        XFONT_SetTextColor(pFont, 0xFFFF3333u);
        XFONT_TextOut(pFont, pBackBuffer, L"No network address acquired.", (unsigned)-1, 40, 90);

        XFONT_SetTextColor(pFont, 0xFFFFFFFFu);
        XFONT_TextOut(pFont, pBackBuffer, L"Check the Ethernet cable / DHCP server and",
                     (unsigned)-1, 40, 130);
        XFONT_TextOut(pFont, pBackBuffer, L"restart the title.", (unsigned)-1, 40, 156);
    }

    D3DDevice_EndScene();
    D3DDevice_Swap(0);
}

int main(void)
{
    int           rc, i;
    XNADDR        xna = {0};
    unsigned long st = XNET_GET_XNADDR_PENDING, link;
    unsigned char *o;
    XNetStartupParams xnsp = {0};
    BOOL haveAddress;
    SOCKET srv = INVALID_SOCKET;
    DWORD hits = 0;
    XFONT *pFont;
    HRESULT hr;

    xnsp.cfgSizeOfStruct = sizeof(xnsp);                 // must equal sizeof(XNetStartupParams)
    xnsp.cfgFlags        = XNET_STARTUP_BYPASS_SECURITY; // devkit insecure mode -> DHCP runs

    printf("net: XNetStartup...\n");
    rc = XNetStartup(&xnsp);
    printf("net: XNetStartup returned %d\n", rc);

    // Busy-poll for a configured address (DO NOT block: a blocking wait starves
    // DPCs on the dev kit, so the timer/NIC DPCs that drive DHCP would never run).
    for (i = 0; i < 300; i++) {              // ~30s
        KeStallExecutionProcessor(100000);   // 100ms busy wait keeps DPCs alive
        st = XNetGetTitleXnAddr(&xna);
        if (st & (XNET_GET_XNADDR_DHCP | XNET_GET_XNADDR_STATIC))
            break;                           // got a real lease / static IP
        if ((i % 10) == 9)
            printf("net: ...polling (t=%ds, xnaddr-flags=0x%lx)\n", (i + 1) / 10, st);
    }

    link = XNetGetEthernetLinkStatus();
    o = (unsigned char *)&xna.ina;   // network byte order
    haveAddress = (st & (XNET_GET_XNADDR_DHCP | XNET_GET_XNADDR_STATIC)) != 0;

    printf("net: link=0x%02lx flags=0x%lx IP=%u.%u.%u.%u\n",
           link, st, o[0], o[1], o[2], o[3]);

    if (!InitDevice()) {
        for (;;) { }
    }

    printf("net: XFONT_OpenDefaultFont\n");
    hr = XFONT_OpenDefaultFont(&pFont);
    if (FAILED(hr)) {
        printf("net: XFONT_OpenDefaultFont failed hr=0x%08x\n", (unsigned)hr);
        for (;;) { }
    }

    if (haveAddress) {
        WSADATA wsa;
        int wrc = WSAStartup(MAKEWORD(2, 2), &wsa);   // separate refcount from XNetStartup
        printf("net: WSAStartup returned %d\n", wrc);
        srv = StartHttpServer();
        if (srv != INVALID_SOCKET) {
            printf("net: serving on http://%u.%u.%u.%u/\n", o[0], o[1], o[2], o[3]);
        }
    } else {
        printf("net: no address acquired -- displaying error screen\n");
    }

    for (;;) {
        if (srv != INVALID_SOCKET) {
            SOCKET cli = accept(srv, NULL, NULL);
            if (cli != INVALID_SOCKET) {
                HandleClient(cli);
                closesocket(cli);
                ++hits;
            }
        }

        RenderFrame(pFont, haveAddress, o, (link & XNET_ETHERNET_LINK_ACTIVE) != 0, hits);
        KeStallExecutionProcessor(10000);   // 10ms; keeps DPCs alive between frames
    }

    return 0;
}
