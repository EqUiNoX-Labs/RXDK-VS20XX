//-----------------------------------------------------------------------------
// File: UIXFriends.h
//
// Desc: Class definitions for UIXFriends sample
//
// Hist: 7.7.03 - Created
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <xgraphics.h>
#include <xact.h>

// Header needed for detecting loss of network connectivity
#include <XbNet.h>

// XHV is optional for the friends list - it is used here for voice-mail.
#include <xhv.h>

// Maximum number of voice streams to use for playback
const DWORD NUM_XHV_PLAYBACK_STREAMS = 2;

// Headers needed for UIX
#include <xonline.h>
#include <uix.h>

// Live Aware is required for all Live titles from this point forward.
// This requires all Live titles to support Silent Sign-on as well as access
// to the Friends List and notification support within a single-player or 
// offline game experience.

// Games with no Live multiplayer gameplay are still encouraged to be
// Live Aware. This sample simulates this kind of game with the flag
// LIVE_AWARE_ONLY. If LIVE_AWARE_ONLY is defined then this sample still signs
// on silently and allows the player to manage their Friends List and receive
// notifications but the sample has no concept of a multiplayer session.

// Games with Live multiplayer gameplay will support sessions. This requires
// the title to inform Live of user state changes when the user leaves or joins
// a Live game session.

//#define LIVE_AWARE_ONLY


//-----------------------------------------------------------------------------
// Enum for tracking which screen/mode we are in.
//-----------------------------------------------------------------------------
enum WHICH_SCREEN
{
    SCREEN_INTRO,               // Intro screen/movie, shown prior to first button press
    SCREEN_MAIN,                // Main menu - branches to live menu and other game features
    SCREEN_LIVE,                // The live menu - access to friends feature
    SCREEN_LOGON,               // Logging on with UIX mode
    SCREEN_FRIENDS,             // Friends with UIX mode
    SCREEN_COUNT                // Total number of screens
};

//-----------------------------------------------------------------------------
// Enum for tracking which screen/mode we are in.
//-----------------------------------------------------------------------------
typedef struct _UIXFRIENDS_LAUNCH_DATA {
	DWORD dwID;
#ifndef LIVE_AWARE_ONLY
	XNKID SessionID;
#endif
	DWORD dwOtherStuffThatIsNotUsedInThisSample;
	BOOL bSilentLogon;
	XONLINE_LOGON_STATE LogonState;
} UIXFRIENDS_LAUNCH_DATA, *PUIXFRIENDS_LAUNCH_DATA;

#define UIXFRIENDS_LAUNCH_ID ( (DWORD) 0x333 )

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
	BOOL				m_bSilentLogon;		// Was the user logged in silently?

    WCHAR               m_strMessage[1024]; // Space for error message text.

    CXBNetLink          m_NetLink;          // Network link checking

    // Functions for checking and updating the player's state (online, etc.)
    VOID SetPlayerOnlineState( DWORD dwUserIndex, DWORD dwState );
    VOID CheckDeviceStates();

    // Function for changing state as needed when a UIX feature exits.
    VOID HandleFeatureExit();
    // Function for processing user input when no UIX feature is active.
    VOID ProcessInput();

    struct LocalUser
    {
        WCHAR strGamertag[XONLINE_MAX_GAMERTAG_LENGTH+1];
        BOOL  bSignedOn;
        BOOL  bVoice;
        BOOL  bGuest;
        DWORD dwUserFlags;
		DWORD dwState;

	};


    DWORD               m_dwMicrophoneState;
    DWORD               m_dwHeadphoneState;
	// The real state of communicators on each controller
	// REVIEW: Turn this into a bitfield?
	BOOL				m_dwVoice[XGetPortCount()];
    LocalUser           m_Users[XONLINE_MAX_LOGON_USERS];
	// Remember the user index and controller index used to pull up the friends menu
	DWORD				m_dwUserIdx;
	DWORD				m_dwCtlrIdx;

#ifndef LIVE_AWARE_ONLY
    // This is a session ID so that we can get UIX to issue invitations.
    XNKID               m_SessionID;
#endif

	// This is the potential launch data that was passed and retrieved via XGetLaunchInfo
	UIXFRIENDS_LAUNCH_DATA m_LaunchData;

public:
    virtual HRESULT Initialize();
    virtual HRESULT Render();
    virtual HRESULT FrameMove();
    VOID Reset();
    VOID StartLogon( BOOL bSilentLogon, XONLINE_ACCEPTED_GAMEINVITE* pInvite = NULL, XONLINE_LOGON_STATE* pState = NULL );
    HRESULT InitSound();
    // Returns true if we are at a UIX screen, such as logon or managing
    // friends. It determines this by looking at m_WhichScreen. This
    // function will return FALSE if there is just a UIX message popup
    // being displayed. Use the render flag to determine if there is
    // any UIX rendering at all.
    BOOL IsUIXScreen();
    VOID ShowLoginState();
	
#ifndef LIVE_AWARE_ONLY
	VOID ChangeSessionState( BOOL bJoiningSession );
	VOID JoinSession( XNKID sessionID );
	VOID CreateSession();
	VOID LeaveSession();
#endif // LIVE_AWARE_ONLY

    CXBoxSample();
};
