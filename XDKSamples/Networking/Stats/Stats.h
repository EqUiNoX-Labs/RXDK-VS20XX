//-----------------------------------------------------------------------------
// File: Stats.h
//
// Desc: Shows Xbox stats APIs
//
// Hist: 04.10.02 - New for May release 
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef STATS_H
#define STATS_H

#include "Common.h"
#include "UserInterface.h"
#include "XBApp.h"
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
    enum State
    {
        STATE_CREATE_ACCOUNT,          // Create user account
        STATE_SELECT_ACCOUNT,          // Select user account
        STATE_LOGGING_ON,              // Perform authentication
        STATE_ERROR,                   // Error
        STATE_MAIN_MENU,               // Main menu
        STATE_SELECT_LEVEL,            // Level selection
        STATE_FRIEND_ENUM,             // Fetching friend list
        STATE_FINISH_ENUM,             // Finishing friend enumeration
        STATE_END_GAME,                // End of game screen
        STATE_STAT_GET,                // XOnlineStatGet in progress
        STATE_STAT_SET,                // Write current game stats
        STATE_STAT_LEADER_ENUM,        // Stat leader enumeration in progress
        STATE_VIEW_LEADERBOARD,        // View statistics
        STATE_VIEW_FRIENDS_STATS,      // Prepare to view statistic for friends
        STATE_RESET_STATS,             // Reset statistics for current user
        STATE_BOOT_TO_DASH,            // Boot to dash
        STATE_HELP                     // Help Screen
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
        EV_NULL
    };

    UserInterface       m_UI;                // UI object
    State               m_State;             // current state
    State               m_NextState;         // transition to this state
    State               m_HelpResumeState;   // state to resume to after help
    BOOL                m_bAllowBootToDash;  // Boot to dash allowed on error
    DWORD               m_dwLaunchReason;    // Reason for booting to dash
    BOOL                m_bIsLoggedOn;
    DWORD               m_dwCurrItem;        // current selected menu item
    DWORD               m_dwCurrLevel;       // selected level (0..NUM_LEVELS)
    DWORD               m_dwCurrLeaderBoard; // current leaderboard
    BOOL                m_bShowRating;       // display rating on leaderboard
    XBUserList          m_UserList;          // list of available accounts
    DWORD               m_dwCurrUser;        // index of curr user in m_UserList
    DWORD               m_dwUserIndex;       // which controller
    WCHAR               m_strUser[ XONLINE_GAMERTAG_SIZE ]; // Current user
    CXBNetLink          m_NetLink;                   // Network link checking
    DWORD               m_pServices[ NUM_SERVICES ]; // List of desired services
    ServiceInfoList     m_ServiceInfoList;           // List of service info
    PlayerList          m_Players;            // Players in a game
    CXBOnlineTask       m_hOnlineTask;        // Online task
    FriendList          m_FriendList;         // current friends
    CXBOnlineTask       m_hFriendsTask;       // friends online task
    CXBOnlineTask       m_hFriendEnumTask;    // friend enumerate task
    CXBOnlineTask       m_hStatsWriteTask;    // XOnlineStatSet task    
    CXBOnlineTask       m_hStatsReadTask;     // XOnlineStatGet task
    CXBOnlineTask       m_hStatsEnumTask;     // XOnlineStatLeaderEnumerate task
    CXBOnlineTask       m_hStatsResetTask;    // XOnlineStatReset task
    StatSpecList        m_StatSpecList;       // Stat spec list used for Stat Set/Get
    PlayerList          m_LeaderboardUsers;   // Users on a leaderboard
    CPlayerStats        m_OverallStats[MAX_PLAYERS];  // Existing overall stats
    CPlayerStats        m_LevelStats[MAX_PLAYERS];    // Existing level stats
    XUID *              m_pxuidPagePivot;     // Leaderboard enum page pivot
    XONLINE_USER        m_LogonUsers[XONLINE_MAX_LOGON_USERS];

public:

    virtual HRESULT Initialize();
    virtual HRESULT FrameMove();
    virtual HRESULT Render();

    CXBoxSample();

private:

    Event GetEvent() const;

    VOID HandleSignOnError( HRESULT hr );
    VOID HandleServiceError( HRESULT hr, DWORD dwServiceId );
    VOID HandleUserSignOnError( HRESULT hr );

    VOID UpdateStateCreateAccount( Event );
    VOID UpdateStateSelectAccount( Event );
    VOID UpdateStateSelectLevel( Event );
    VOID UpdateStateLoggingOn( Event );
    VOID UpdateStateError( Event );
    VOID UpdateStateHelp( Event );
    VOID UpdateStateFriendEnum( Event );
    VOID UpdateStateFinishEnum( Event );
    VOID UpdateStateMainMenu( Event );
    VOID UpdateStateEndGame( Event );
    VOID UpdateStateLeaderEnum( Event );
    VOID UpdateStateStatGet( Event );
    VOID UpdateStateStatSet( Event );
    VOID UpdateStateViewLeaderboard( Event );
    VOID UpdateStateViewFriendsStats( Event );
    VOID UpdateStateResetStats( Event );


    VOID BeginWriteGameStats();
    VOID FinishFriendEnum();
    VOID BeginReadFriendsStats();

    VOID BeginLogin();
    VOID CreateEndGame();
    LONGLONG CalculateRating( LONG, LONG, LONG );
    VOID SetPlayerState( DWORD );
    VOID BootToDash( DWORD );
    VOID Reset();
};




#endif // STATS_H
