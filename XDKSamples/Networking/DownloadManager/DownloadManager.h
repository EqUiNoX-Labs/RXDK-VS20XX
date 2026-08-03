//-----------------------------------------------------------------------------
// File: DownloadManager.h
//
// Desc: Shows Xbox online authentication protocols and using the Xbox downloader
//       Includes account creation, PIN entry, validation and logon.
//
// Hist: 10.16.02 - New for Nov release 
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef DOWNLOAD_H
#define DOWNLOAD_H

#include "Common.h"
#include "UserInterface.h"
#include "XBApp.h"
#include "XBFont.h"
#include "XBHelp.h"
#include "XBNet.h"
#include "XBOnlineTask.h"




//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

// Number of services to authenticate
const DWORD NUM_SERVICES = 2;




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    // Master state machine
    enum State
    {
        STATE_CREATE_ACCOUNT,   // Create user account
        STATE_USER_EVENTS,      // Run user state machines
        STATE_SIGNING_ON,       // Perform authentication
        STATE_MAIN_MENU,        // Main menu
        STATE_ERROR,            // Error
    };


    // Per-user signon states
    enum UserState
    {
        STATE_USER_PRE_SIGN_ON,      // Wait for controller activation
        STATE_USER_SELECT_ACCOUNT,   // Select user account
        STATE_USER_CONFIRM_SPONSOR,  // Confirm sponsor account selection
        STATE_USER_PIN_ENTRY,        // PIN Entry
        STATE_USER_WAIT_FOR_OTHERS,  // Wait for others to select accounts
        STATE_USER_BOOT_TO_DASH,     // Boot to dash
        STATE_USER_ERROR,            // Error
        STATE_USER_DONE              // Sign in process done (may not have succeeded)
    };

    enum Event
    {
        EV_BUTTON_A,
        EV_BUTTON_B,
        EV_BUTTON_X,
        EV_BUTTON_Y,
        EV_BUTTON_BACK,
        EV_BUTTON_WHITE,
        EV_BUTTON_BLACK,
        EV_UP,
        EV_DOWN,
        EV_LEFT,
        EV_RIGHT,
        EV_LEFT_TRIGGER,
        EV_RIGHT_TRIGGER,
        EV_NULL
    };

    UserInterface       m_UI;                // UI object
    State               m_State;             // current state
    State               m_NextState;         // return to this state
    BOOL                m_bAllowBootToDash;  // Boot to dash allowed on error
    DWORD               m_dwLaunchReason;    // Reason for booting to dash
    XBUserList          m_UserAccountList;   // list of available accounts
    // Online Info / state for users using this XBox
    // The list is in controller order. A real game title might also include
    // a pointer or index into an entry in a "players" list used to maintain
    // all the participants in a game session.
    class CUser
    {
    public:
        BYTE          Passcode[XONLINE_PASSCODE_LENGTH];    // Entered PIN Code
        WCHAR         strName[ XONLINE_GAMERTAG_SIZE + 30]; // Player name
        UserState     State;                   // current sign on state
        UserState     NextState;               // return to this state
        BOOL          bAllowBootToDash;        // Boot to dash allowed
        DWORD         dwLaunchReason;          // Reason for booting to dash
        BOOL          bSignedOn;               // Is the player signed on
        BOOL          bGuest;                  // Signing on as a guest
        BOOL          bVoice;                  // Voice enabled
        DWORD         dwTopItem;               // tracks the index of the top item
        DWORD         dwCurrItem;              // current selected menu item
        XONLINE_USER  Account;                 // XOnline user account
        WCHAR         strError[MAX_ERROR_STR]; // STATE_USER_ERROR error
    };
    
    CUser               m_Users[ XONLINE_MAX_LOGON_USERS ];
    XONLINE_USER        m_LogonUsers[XONLINE_MAX_LOGON_USERS];


    CXBNetLink          m_NetLink;                   // Network link checking
    DWORD               m_pServices[ NUM_SERVICES ]; // List of desired services
    ServiceInfoList     m_ServiceInfoList;           // List of service info
    CXBOnlineTask       m_hOnlineTask;               // Online task
    BOOL                m_bReadyForSignOn;           // Signon is now possible
    BOOL                m_bSignedOn;                 // Successfully signed
    BOOL                m_bShowHelp;                 // Display Help
    DWORD               m_dwMicrophoneState;         // Mic Devices state
    DWORD               m_dwHeadphoneState;          // Headphone Devices state

    CXBFont             m_Font;
    CXBHelp             m_Help;

public:

    virtual HRESULT Initialize();
    virtual HRESULT FrameMove();
    virtual HRESULT Render();

    CXBoxSample();

private:

    Event GetEvent() const;
    Event GetEvent( DWORD ) const;
    Event GetGamepadEvent( const XBGAMEPAD & ) const;

    VOID Reset();
    VOID ResetUser( DWORD );
    VOID HandleSignOnError( HRESULT  );
    VOID HandleServiceError( HRESULT , DWORD );
    VOID HandleUserSignOnError( CUser &, HRESULT );

    VOID UpdateStateCreateAccount( Event );
    VOID BeginSignOn();
    VOID BeginSelectAccount( CUser & );
    VOID UpdateStateSigningOn( Event );
    VOID UpdateStateMainMenu( Event );
    VOID UpdateStateError( Event );

    VOID UpdateUserStatePreSignOn( CUser & , Event );
    VOID UpdateUserStateSelectAccount( CUser & , Event );
    VOID UpdateUserStateWaitForOthers( CUser & , Event );

    VOID BeginUserPINEntry( CUser & );
    VOID UpdateUserStatePINEntry( CUser &, Event );
    VOID UpdateUserStateDone( CUser &, Event );
    VOID UpdateUserStateError( CUser &, Event );
    VOID UpdateUserStateConfirmSponsor( CUser &, Event );

    VOID SetPlayerOnlineState( DWORD, DWORD );
    VOID BootToDash( DWORD dwReason );
    VOID __cdecl SetUserErrorStr( CUser &, const WCHAR* , ... );

    BOOL HostAccountSelected( XUID & );
    BOOL IsUserInPinEntry();
    VOID CheckDeviceStates();
    VOID RestoreUsersFromLogonState( XONLINE_LOGON_STATE *pLogonState );


};




#endif // DOWNLOAD_H
