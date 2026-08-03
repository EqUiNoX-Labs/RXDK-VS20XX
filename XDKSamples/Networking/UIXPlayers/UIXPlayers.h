//-----------------------------------------------------------------------------
// File: UIXPlayers.h
//
// Desc: Class definitions for UIXPlayers sample
//
// Hist: 7.7.03 - Created
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <xgraphics.h>
#include <xact.h>

// Headers needed for UIX
#include <xonline.h>
#include <uix.h>

// XHV is optional for the players list - it is used here for tracking who is talking.
#include <xhv.h>

// Maximum number of voice streams to use for playback
const DWORD NUM_XHV_PLAYBACK_STREAMS = 2;




//-----------------------------------------------------------------------------
// Enum for tracking which screen/mode we are in.
//-----------------------------------------------------------------------------
enum WHICH_SCREEN
{
    // Various screens used to drive this sample
    SCREEN_ERROR,               // Generic error display screen
    SCREEN_LOGON,               // Logging on with UIX mode
    SCREEN_MAINMENU,            // Main menu - select how to test the players list.

    // Various screens used to demonstrate UIX players list usage
    SCREEN_LOBBY,               // Using the player's list to build a list of players
    SCREEN_MYTEAM_ONLY,         // Showing one team only, using bit flags
    SCREEN_GUEST_VIEW,          // The guest view, with no feedback options
    SCREEN_START_MENU_PLAYERS_LIST, // Typical start-menu players list
    SCREEN_DEPARTED_PLAYERS_ONLY,   // Showing departed players only

    // An options screen that can be invoked from the lobby
    SCREEN_OPTIONS_OVERLAY,     // Options menu that is displayed while the players feature is running

    SCREEN_COUNT                // Total number of screens
};




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication, public ITitleXHV
{
    CXBFont             m_Font;             // Font object

    // Sound data.
    VOID*          m_pWaveBankMemory;
    VOID*          m_pSoundBankMemory;
    PXACTWAVEBANK  m_pWaveBank;
    PXACTENGINE    m_pXactEngine;
    PXACTSOUNDBANK m_pSoundBank;

    // Variables/state needed for UIX
    ILiveEngine*        m_pLiveEngine;      // The UIX engine
    ITitleUIPlugin*     m_pUIPlugin;        // The plugin for UI rendering
    DWORD               m_dwLiveWorkFlags;  // Flags return from DoWork() and used elsewhere.

    PXONLINE_USER       m_pLogonUsers;      // Pointer to array of logged on users, or NULL.

    WHICH_SCREEN        m_WhichScreen;      // Which screen are we on - menu state machine
    BOOL                m_bLoggedOn;        // Am I currently logged on?
    BOOL                m_bLoggingOn;       // Is there a logon in progress?
    BOOL                m_bPlayersListOpen; // Is the players list active?

    WCHAR               m_strMessage[1024]; // Space for error message text.
    const WCHAR*        m_ScreenHeader;     // Text for the top of the UIX screen - drawn overtop
    const WCHAR*        m_ScreenFooter;     // Text for the bottom of the UIX screen - drawn overtop

    // Functions for checking and updating the player's state (online, etc.)
    VOID SetPlayerOnlineState( DWORD dwUserIndex, DWORD dwState );
    VOID CheckDeviceStates();

    // Function for changing state as needed when a UIX feature exits.
    VOID HandleFeatureExit();
    // Function for processing user input when no UIX feature is active.
    VOID ProcessInput();

    // Start a particular menu screen. Pass in the controller port number, or -1
    // if it is unknown or irrelevant.
    VOID StartScreen( WHICH_SCREEN whichScreen, DWORD portNumber = -1 );
    // Start a particular menu screen and push the previous screen onto the stack.
    VOID PushAndStartScreen( WHICH_SCREEN whichScreen );
    // Pop the previous screen from the screen stack.
    VOID PopScreen();

    struct LocalUser
    {
        WCHAR strGamertag[XONLINE_MAX_GAMERTAG_LENGTH+1];
        BOOL  bSignedOn;
        BOOL  bVoice;
        BOOL  bGuest;
        DWORD dwUserFlags;
    };

    DWORD               m_dwMicrophoneState;
    DWORD               m_dwHeadphoneState;
    LocalUser           m_Users[XONLINE_MAX_LOGON_USERS];

    // This is a fake session ID so that we can get UIX to issue
    // invitations.
    XNKID               m_FakeSessionID;

    // A copy of the filter flags being used in the UIXPlayers feature.
    // Zero if that feature is not active.
    DWORD               m_FilterFlags;

    // ITitleXHV callback functions
    STDMETHODIMP CommunicatorStatusUpdate( DWORD dwPort, XHV_VOICE_COMMUNICATOR_STATUS status );
    STDMETHODIMP LocalChatDataReady( DWORD dwPort, DWORD dwSize, PVOID pData );

public:
    virtual HRESULT Initialize();
    virtual HRESULT Render();
    virtual HRESULT FrameMove();
    VOID Reset();
    VOID StartLogon();
    VOID StartPlayersList( BOOL LobbyMode, BOOL DisplayDeparted, BOOL DisplayActive, DWORD portNumber, int FilterFlags );
    HRESULT InitSound();
    // Returns true if we are at a UIX screen, such as logon or managing
    // friends. It determines this by looking at m_WhichScreen. This
    // function will return FALSE if there is just a UIX message popup
    // being displayed. Use the render flag to determine if there is
    // any UIX rendering at all.
    BOOL IsUIXScreen();

    VOID RenderScreenOptionsOverlay();

    CXBoxSample();

    struct ScreenRecord
    {
        ScreenRecord() {}
        ScreenRecord( WHICH_SCREEN WhichScreen, const WCHAR* header, const WCHAR* footer )
        {
            m_WhichScreen = WhichScreen;
            m_ScreenHeader = header;
            m_ScreenFooter = footer;
        }
        WHICH_SCREEN    m_WhichScreen;
        const WCHAR*    m_ScreenHeader;
        const WCHAR*    m_ScreenFooter;
    };
    // LIFO stack of screens
    ScreenRecord    m_ScreenStack[10];
    // Count of how many items are on the stack - normally zero.
    DWORD           m_ScreenStackEntries;
};
