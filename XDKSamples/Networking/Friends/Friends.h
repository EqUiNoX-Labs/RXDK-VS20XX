//-----------------------------------------------------------------------------
// File: Friends.h
//
// Desc: Illustrates online friends on Xbox.
//
// Hist: 08.08.01 - New for Aug M1 release 
//       10.19.01 - Updated for Nov release
//       01.18.02 - Updated for Feb release
//       02.15.02 - Updated for Mar release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef FRIENDS_H
#define FRIENDS_H

#include "Common.h"
#include "UserInterface.h"
#include "XBApp.h"
#include "XBFont.h"
#include "XBHelp.h"
#include "XBNet.h"
#include "XBOnlineTask.h"
#include "FriendsManager.h"



//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------
const DWORD MAX_STATUS_STR = 64;

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
    enum State
    {
        STATE_CREATE_ACCOUNT,       // Create user account
        STATE_SELECT_ACCOUNT,       // Select user account
        STATE_LOGGING_ON,           // Perform authentication
            
        STATE_FRIEND_LIST,          // Friend list
        STATE_ACTION_MENU,          // Invite/join/remove friend
        STATE_NEW_FRIEND,           // Invite
        
        STATE_CONFIRM_REMOVE,       // Confirm removal action

        STATE_JOINING_GAME,         // Joining game
        STATE_BOOT_TO_DASH,         // Boot to dash
        STATE_ERROR,                // Error screen
        STATE_HELP,                 // Help screen
    };
    
    enum Event
    {
        EV_BUTTON_A,
        EV_BUTTON_B,
        EV_BUTTON_X,
        EV_BUTTON_Y,
        EV_BUTTON_BACK,
        EV_BUTTON_BLACK,
        EV_BUTTON_WHITE,
        EV_UP,
        EV_DOWN,
        EV_NULL
    };

    CXBHelp         m_Help;
    CXBFont         m_Font;
    
    UserInterface   m_UI;                 // UI object
    State           m_State;              // Current state
    State           m_NextState;          // Return to this state
    DWORD           m_dwCurrItem;         // Current selected menu item
    DWORD           m_dwTopItem;          // Tracks the index of the top item
    XONLINE_USER    m_UserList[XONLINE_MAX_STORED_ONLINE_USERS]; // Available accounts
    DWORD           m_dwNumUsers;         // Available accounts
    DWORD           m_dwCurrUser;         // Index of curr user in m_UserList
    ULONGLONG       m_qwUserID;           // Unique player ID
    DWORD           m_dwUserIndex;        // Which controller
    WCHAR           m_strUser[XONLINE_GAMERTAG_SIZE]; // Current user name
    CXBNetLink      m_NetLink;            // Network link checking
    DWORD           m_pServices[NUM_SERVICES]; // Desired services
    BOOL            m_bIsLoggedOn;      
    BOOL            m_bGameInvitePending;  // An unanswered game invite
    CXBStopWatch    m_StatusTimer;
    WCHAR           m_strStatus[MAX_STATUS_STR];
    CXBOnlineTask   m_hOnlineTask;
    
    XUID            m_xLoginUserID;         // xuid of the logged on user

    XONLINE_USER    m_PotentialFriendList[XONLINE_MAX_STORED_ONLINE_USERS]; // Potential friends
    DWORD           m_dwNumPotentialFriends;
    DWORD           m_dwCurrFriend;

    BOOL            m_bCloaked;            // Currently cloaked?
    ActionList      m_Actions;             // Action menu items    

    DWORD           m_dwOldState;          // Current cached online state

    WCHAR           m_strError[1024];      // Error string

private:
    Event GetEvent() const;
    
    VOID UpdateStateCreateAccount( Event );
    VOID UpdateStateSelectAccount( Event );
    VOID UpdateStateLoggingOn( Event );
    VOID UpdateStateCancelLogon( Event );
    
    VOID UpdateStateFriendList( Event );
    VOID UpdateStateActionMenu( Event );
    VOID UpdateStateNewFriend( Event );

    VOID UpdateStateSelectFeedback( Event );
   
    VOID UpdateStateConfirmRemove( Event );       

    VOID UpdateStateError( Event );

    VOID UpdateStateHelp( Event );

    VOID UpdateStateGetMuteList( Event );
    VOID UpdateStateInviteAccepted( Event );
    VOID UpdateStateJoiningGame( Event );
    
    VOID BeginLogin();
    HRESULT InitFriends();
    
    VOID SetPlayerState( DWORD );

    VOID UpdatePotentialFriends();
    VOID ConfigureActionMenu();
    VOID SetStatus( const WCHAR* );
    VOID BootToDash();
    VOID Reset();

public:
    virtual HRESULT Initialize();
    virtual HRESULT FrameMove();
    virtual HRESULT Render();
    
    CXBoxSample();
};




#endif // FRIENDS_H
