//------------------------------------------------------------------------------
// File: main.c
//
// XInput device monitor -- based on RXDK-Libs samples/xapi-input, extended
// with an on-screen terminal (libd3d8 + libxfont) so button presses, connects,
// and keystrokes are visible on the TV instead of only in the debug log.
//
// Polls for connection/disconnection of controllers, IR remotes, mice, and
// keyboards (XGetDeviceChanges), opens each as it appears (XInputOpen), and
// prints every button/movement event as a new terminal line. Once the visible
// area fills up, the oldest line scrolls off the top -- like a console.
//
// Keyboard keystrokes come through the XInputDebug keyboard queue API
// (XInputDebugInitKeyboardQueue + XInputDebugGetKeystroke) -- XInputGetState
// refuses keyboards. The queue is initialized once at startup and drained
// every frame.
//------------------------------------------------------------------------------

#include <xtl.h>
#include <xkbd.h>     // XDEVICE_TYPE_DEBUG_KEYBOARD
#include <xfont.h>
#include <stdio.h>
#include <stdarg.h>
#include <wchar.h>    // swprintf/vswprintf for the on-screen HUD text

// IR remote isn't in the public headers; declare it like the reference title.
extern XPP_DEVICE_TYPE XDEVICE_TYPE_IR_REMOTE_TABLE;
#define XDEVICE_TYPE_IR_REMOTE (&XDEVICE_TYPE_IR_REMOTE_TABLE)

// IR remote report: XInputGetState fills this small layout for an IR device.
// firstEvent==0 means "no event this poll".
typedef struct _XINPUT_STATEEX {
    DWORD dwPacketNumber;
    BYTE  wButtons;
    BYTE  region;
    BYTE  counter;
    BYTE  firstEvent;
} XINPUT_STATEEX;

#define SCREEN_W          640
#define SCREEN_H          480
#define MAX_PORTS         4
// Terminal area starts below a 2-line title bar (TERM_TOP) and each line
// advances by TERM_LINE_H; MAX_LOG_LINES fills the rest of the screen so the
// console behaves like a real terminal -- it fills up, then scrolls.
#define TERM_TOP          60
#define TERM_LINE_H        22
#define MAX_LOG_LINES     ((SCREEN_H - TERM_TOP) / TERM_LINE_H)
#define LOG_LINE_LEN      64
#define ANALOG_THRESHOLD  48

static const WCHAR *kGamepadDigital[] = {
    L"DPAD_UP", L"DPAD_DOWN", L"DPAD_LEFT", L"DPAD_RIGHT",
    L"START", L"BACK", L"L_THUMB", L"R_THUMB",
};
static const WORD kGamepadDigitalBits[] = {
    XINPUT_GAMEPAD_DPAD_UP, XINPUT_GAMEPAD_DPAD_DOWN, XINPUT_GAMEPAD_DPAD_LEFT, XINPUT_GAMEPAD_DPAD_RIGHT,
    XINPUT_GAMEPAD_START, XINPUT_GAMEPAD_BACK, XINPUT_GAMEPAD_LEFT_THUMB, XINPUT_GAMEPAD_RIGHT_THUMB,
};
static const WCHAR *kGamepadAnalog[] = {
    L"A", L"B", L"X", L"Y", L"BLACK", L"WHITE", L"L_TRIG", L"R_TRIG",
};
static const WCHAR *kMouseButtons[] = { L"LEFT", L"RIGHT", L"MIDDLE", L"X1", L"X2" };
static const BYTE kMouseBits[] = {
    XINPUT_DEBUG_MOUSE_LEFT_BUTTON, XINPUT_DEBUG_MOUSE_RIGHT_BUTTON, XINPUT_DEBUG_MOUSE_MIDDLE_BUTTON,
    XINPUT_DEBUG_MOUSE_XBUTTON1, XINPUT_DEBUG_MOUSE_XBUTTON2,
};

static HANDLE g_pad[MAX_PORTS];
static HANDLE g_mouse[MAX_PORTS];
static HANDLE g_ir[MAX_PORTS];
static HANDLE g_kbd[MAX_PORTS];

static WORD g_padPrevDigital[MAX_PORTS];
static BYTE g_padPrevAnalog[MAX_PORTS][8];
static BYTE g_mousePrevButtons[MAX_PORTS];
static DWORD g_irPrevPacket[MAX_PORTS];

static BOOL  g_padConnected[MAX_PORTS];

// --- On-screen scrolling terminal --------------------------------------------

static WCHAR g_log[MAX_LOG_LINES][LOG_LINE_LEN];
static int   g_logCount = 0;

static void PushLog(const WCHAR *text)
{
    int i, n;

    if (g_logCount < MAX_LOG_LINES) {
        n = g_logCount++;
    } else {
        // Drop the oldest line, shift everything up one slot.
        for (i = 1; i < MAX_LOG_LINES; i++) {
            int j = 0;
            while (g_log[i][j]) { g_log[i - 1][j] = g_log[i][j]; j++; }
            g_log[i - 1][j] = 0;
        }
        n = MAX_LOG_LINES - 1;
    }

    for (i = 0; text[i] && i < LOG_LINE_LEN - 1; i++) {
        g_log[n][i] = text[i];
    }
    g_log[n][i] = 0;
}

// Every line gets a "> " prompt prefix for the terminal look.
static void LogF(const WCHAR *fmt, ...)
{
    WCHAR msg[LOG_LINE_LEN];
    WCHAR buf[LOG_LINE_LEN];
    va_list args;
    va_start(args, fmt);
    vswprintf(msg, LOG_LINE_LEN, fmt, args);
    va_end(args);
    swprintf(buf, LOG_LINE_LEN, L"> %ls", msg);
    PushLog(buf);
}

// --- Device polling (based on samples/xapi-input) ---------------------------

// Open/close devices of one type as they are inserted/removed, logging each.
static void process_changes(PXPP_DEVICE_TYPE type, const WCHAR *name,
                            HANDLE handles[MAX_PORTS],
                            PXINPUT_POLLING_PARAMETERS polling)
{
    DWORD insertions = 0;
    DWORD removals = 0;
    int port;

    if (XGetDeviceChanges(type, &insertions, &removals) != TRUE) {
        return;
    }

    for (port = 0; port < MAX_PORTS; ++port) {
        if (insertions & (1u << port)) {
            handles[port] = XInputOpen(type, port, XDEVICE_NO_SLOT, polling);
            LogF(L"%ls connected port=%d%ls", name, port,
                 handles[port] ? L"" : L" (open FAILED)");
        }
        if (removals & (1u << port)) {
            if (handles[port]) {
                XInputClose(handles[port]);
                handles[port] = NULL;
            }
            LogF(L"%ls disconnected port=%d", name, port);
        }
    }
}

static void poll_gamepads(void)
{
    int port, i;

    for (port = 0; port < MAX_PORTS; ++port) {
        XINPUT_STATE state;
        WORD digital, changed;

        g_padConnected[port] = (g_pad[port] != NULL);
        if (!g_pad[port] || XInputGetState(g_pad[port], &state) != ERROR_SUCCESS) {
            continue;
        }

        digital = state.Gamepad.wButtons;
        changed = (WORD)(digital ^ g_padPrevDigital[port]);
        for (i = 0; i < 8; ++i) {
            if (changed & kGamepadDigitalBits[i]) {
                LogF(L"pad%d %ls %ls", port, kGamepadDigital[i],
                     (digital & kGamepadDigitalBits[i]) ? L"down" : L"up");
            }
        }
        g_padPrevDigital[port] = digital;

        for (i = 0; i < 8; ++i) {
            BYTE now = state.Gamepad.bAnalogButtons[i];
            BYTE prev = g_padPrevAnalog[port][i];
            BOOL nowDown = now >= ANALOG_THRESHOLD;
            BOOL prevDown = prev >= ANALOG_THRESHOLD;
            if (nowDown != prevDown) {
                LogF(L"pad%d %ls %ls", port, kGamepadAnalog[i], nowDown ? L"down" : L"up");
            }
            g_padPrevAnalog[port][i] = now;
        }
    }
}

static void poll_mice(void)
{
    int port, i;

    for (port = 0; port < MAX_PORTS; ++port) {
        XINPUT_STATE state;
        BYTE buttons, changed;

        if (!g_mouse[port] || XInputGetState(g_mouse[port], &state) != ERROR_SUCCESS) {
            continue;
        }

        buttons = state.DebugMouse.bButtons;
        changed = (BYTE)(buttons ^ g_mousePrevButtons[port]);
        for (i = 0; i < 5; ++i) {
            if (changed & kMouseBits[i]) {
                LogF(L"mouse%d %ls %ls", port, kMouseButtons[i],
                     (buttons & kMouseBits[i]) ? L"down" : L"up");
            }
        }
        g_mousePrevButtons[port] = buttons;
    }
}

static void poll_ir(void)
{
    int port;

    for (port = 0; port < MAX_PORTS; ++port) {
        XINPUT_STATEEX state;
        if (!g_ir[port] ||
            XInputGetState(g_ir[port], (PXINPUT_STATE)&state) != ERROR_SUCCESS ||
            state.firstEvent == 0) {
            continue;
        }
        if (state.dwPacketNumber != g_irPrevPacket[port]) {
            LogF(L"remote%d button code=%u", port, (unsigned)state.wButtons);
            g_irPrevPacket[port] = state.dwPacketNumber;
        }
    }
}

static void poll_keyboard(void)
{
    XINPUT_DEBUG_KEYSTROKE ks;
    // Drain every queued keystroke this tick (queue API; XInputGetState refuses
    // keyboards). SINGLE_KEYBOARD_ONLY -> one queue, no handle argument.
    while (XInputDebugGetKeystroke(&ks) == ERROR_SUCCESS) {
        const WCHAR *evt = (ks.Flags & XINPUT_DEBUG_KEYSTROKE_FLAG_KEYUP)  ? L"up"
                         : (ks.Flags & XINPUT_DEBUG_KEYSTROKE_FLAG_REPEAT) ? L"repeat"
                                                                           : L"down";
        if (ks.Ascii >= 32 && ks.Ascii < 127) {
            LogF(L"key %ls '%c'", evt, (int)ks.Ascii);
        } else {
            LogF(L"key %ls vk=0x%02x", evt, (unsigned)ks.VirtualKey);
        }
    }
}

// --- D3D8 + XFONT on-screen HUD ---------------------------------------------

static D3DDevice *g_pDevice;

static int init_device(void)
{
    D3DPRESENT_PARAMETERS pp;
    Direct3D *pD3D;
    D3DVIEWPORT8 vp;
    HRESULT hr;

    pD3D = Direct3DCreate8(D3D_SDK_VERSION);
    if (!pD3D) {
        printf("xinput-hud: Direct3DCreate8 failed\n");
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
        printf("xinput-hud: CreateDevice failed hr=0x%08x\n", (unsigned)hr);
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

static void render_frame(XFONT *pFont)
{
    D3DSurface *pBackBuffer;
    WCHAR status[80];
    int port, i, y, n;

    D3DDevice_GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer);

    // Classic black-on-green terminal look.
    D3DDevice_Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
    D3DDevice_BeginScene();

    XFONT_SetTextColor(pFont, 0xFF33FF33u);
    XFONT_TextOut(pFont, pBackBuffer, L"RXDK XINPUT MONITOR", (unsigned)-1, 16, 4);

    n = swprintf(status, 80, L"Pads:");
    for (port = 0; port < MAX_PORTS; ++port) {
        n += swprintf(status + n, 80 - n, L" %d:%ls", port, g_padConnected[port] ? L"ON" : L"--");
    }
    XFONT_SetTextColor(pFont, 0xFF808080u);
    XFONT_TextOut(pFont, pBackBuffer, status, (unsigned)-1, 16, 28);

    // Terminal body: fills the rest of the screen, oldest line at the top --
    // once MAX_LOG_LINES is reached, PushLog scrolls everything up a slot.
    XFONT_SetTextColor(pFont, 0xFF33FF33u);
    y = TERM_TOP;
    for (i = 0; i < g_logCount; ++i) {
        XFONT_TextOut(pFont, pBackBuffer, g_log[i], (unsigned)-1, 16, y);
        y += TERM_LINE_H;
    }

    D3DDevice_EndScene();
    D3DDevice_Swap(0);
}

int main(void)
{
    XINPUT_POLLING_PARAMETERS kbdPolling;
    XINPUT_DEBUG_KEYQUEUE_PARAMETERS kbdQueue;
    XFONT *pFont;
    HRESULT hr;

    XInitDevices(0, NULL);

    // Set up the keyboard keystroke queue (down + up + auto-repeat). This also
    // installs the XID keyboard service hook so reports get translated to keys.
    kbdQueue.dwFlags = XINPUT_DEBUG_KEYQUEUE_FLAG_KEYDOWN |
                       XINPUT_DEBUG_KEYQUEUE_FLAG_KEYUP |
                       XINPUT_DEBUG_KEYQUEUE_FLAG_KEYREPEAT;
    kbdQueue.dwQueueSize = 40;
    kbdQueue.dwRepeatDelay = 400;
    kbdQueue.dwRepeatInterval = 100;
    XInputDebugInitKeyboardQueue(&kbdQueue);

    if (!init_device()) {
        for (;;) { }
    }

    hr = XFONT_OpenDefaultFont(&pFont);
    if (FAILED(hr)) {
        printf("xinput-hud: XFONT_OpenDefaultFont failed hr=0x%08x\n", (unsigned)hr);
        for (;;) { }
    }

    printf("xinput-hud: press buttons / move mouse / aim the remote; connect & disconnect devices.\n");

    kbdPolling.fAutoPoll = TRUE;
    kbdPolling.fInterruptOut = TRUE;
    kbdPolling.bInputInterval = 32;
    kbdPolling.bOutputInterval = 32;
    kbdPolling.ReservedMBZ1 = 0;
    kbdPolling.ReservedMBZ2 = 0;

    for (;;) {
        process_changes(XDEVICE_TYPE_GAMEPAD, L"pad", g_pad, NULL);
        process_changes(XDEVICE_TYPE_IR_REMOTE, L"remote", g_ir, NULL);
        process_changes(XDEVICE_TYPE_DEBUG_MOUSE, L"mouse", g_mouse, NULL);
        process_changes(XDEVICE_TYPE_DEBUG_KEYBOARD, L"keyboard", g_kbd, &kbdPolling);

        poll_gamepads();
        poll_mice();
        poll_ir();
        poll_keyboard();

        render_frame(pFont);
    }

    return 0;
}
