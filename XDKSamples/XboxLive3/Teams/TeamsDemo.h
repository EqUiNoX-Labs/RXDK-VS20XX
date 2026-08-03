//-------------------------------------------------------------------------------------
// File: TeamsDemo.h
//
// Desc: This sample demonstrates how to create and manage
//       teams. Teams are a usefull way to create a community
//       with Xbox Live. Players can create teams and invite
//       friends to join. Teams may have their own statistics
//       and rankings.
//
// Hist: 08.10.04 - New for Sept release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#pragma once

#ifndef TEAMSDEMO_H
#define TEAMSDEMO_H

#include <vector>
#include "XBApp.h"
#include "XBNet.h"
#include "XBOnline.h"
#include "XBOnlineTask.h"
#include "XBRandName.h"

//-------------------------------------------------------------------------------------
// Constants
//-------------------------------------------------------------------------------------

// Colors used by the UI and rendering code
static const D3DCOLOR COLOR_YELLOW    = 0xffffff00; // Yellow
static const D3DCOLOR COLOR_GREEN     = 0xff00ff00; // Green
static const D3DCOLOR COLOR_WHITE     = 0xffffffff; // White
static const D3DCOLOR COLOR_RED       = 0xffff0000; // Red
static const D3DCOLOR COLOR_BLUE      = 0x000A0A6A; // Blue
static const D3DCOLOR COLOR_GREY      = 0xff999999; // Grey

static const D3DCOLOR COLOR_NORMAL    = COLOR_WHITE;
static const D3DCOLOR COLOR_HIGHLIGHT = COLOR_YELLOW;
static const D3DCOLOR COLOR_POINTER   = COLOR_GREEN;


// Used for displaying a window of accounts per Xbox
const INT NUM_ACCOUNTS_PER_WINDOW  = 4;

// Used for creating and finding matches
const INT MAX_USERS                = 4;
const INT MAX_XBOXES               = 4;
const INT MAX_MATCHERS             = MAX_USERS * MAX_XBOXES;

const INT MAX_MESSAGE_LENGTH       = 128;

// UI constants

const FLOAT SCREEN_SIZE_X          = 640.0f;
const FLOAT SCREEN_SIZE_Y          = 480.0f;
const FLOAT SCREEN_CENTER_X        = ( SCREEN_SIZE_X * 0.5f );
const FLOAT SCREEN_CENTER_Y        = ( SCREEN_SIZE_Y * 0.5f );

const FLOAT POS_SCREEN_TITLE_Y       = 80.0f;
const FLOAT POS_VERSUS_Y             = 200.0f;
const FLOAT WIDTH_VERSUS_X           = 30.0f;
const FLOAT POS_GAME_SCORE_PADDING   = 30.0f;
const FLOAT POS_MESSAGE_Y            = 240.0f;
const FLOAT POS_GAME_SETUP_Y         = 200.0f;
const FLOAT POS_MENU_START_Y         = 100.0f;
const FLOAT POS_HEADER_Y             = 40.0f;
const FLOAT POS_HEADER_LEFT          = 40.0f;
const FLOAT POS_HEADER_RIGHT         = ( SCREEN_SIZE_X - POS_HEADER_LEFT );
const FLOAT POS_FOOTER_Y             = 420.0f;
const FLOAT POS_FOOTER_LEFT          = 40.0f;
const FLOAT POS_FOOTER_RIGHT         = ( SCREEN_SIZE_X - POS_FOOTER_LEFT );
const FLOAT DEFAULT_TEXT_PADDING     = 30.0f;
const FLOAT TEXT_PADDING_INVITE_INFO = 20.0f;

// Constants for the leaderboard
const FLOAT LEADERBOARD_TEXT_PADDING = ( DEFAULT_TEXT_PADDING * 0.8f );
const FLOAT POS_LEADER_HEADER_Y      = POS_MENU_START_Y + LEADERBOARD_TEXT_PADDING;
const FLOAT POS_TEAM_X               = 100.0f;
const FLOAT POS_KILLS_X              = 250.0f;
const FLOAT POS_DEATHS_X             = 300.0f;
const FLOAT POS_ASSISTS_X            = 375.0f;
const FLOAT POS_RATING_X             = 475.0f;

// Constants for the account selection screen
const FLOAT POS_ACCOUNT_LIST_START   = POS_SCREEN_TITLE_Y + 
                                       ( DEFAULT_TEXT_PADDING * 2.0f );

// Constants for the Team Roster screen
const INT   NUM_ENTRIES_PER_SCREEN   = 8; // Only display 8 team members at once


const INT   MAX_SIZE_STATE_STACK   = 8; // The maximum size of the state stack


const DWORD LEADERBOARD_ID         = 3; // The ID of the leaderboard we will
                                        // Read and write from
const INT MAX_STAT_USERS           = 10; // Maximum number of teams to retrieve from a leaderboard
const INT NUM_STATS_SUBMITTED      = 5;  // Number of stats submitted


// Used for determining if a menu wraps around or not
enum
{
    MENU_WRAP_OFF,
    MENU_WRAP_ON
};

enum
{
    STAT_ID_KILLS = 1,         // Number of Kills
    STAT_ID_DEATHS,            // Number of Deaths
    STAT_ID_ASSISTS,           // Number of Assists
    STAT_ID_ACCURACY,          // Shooting accuracy
};


// Stat attribute ids
enum
{
    STAT_KILLS = 0,
    STAT_DEATHS,
    STAT_ASSISTS,
    STAT_ACCURACY,
    STAT_RATING,
    STAT_RANK,      // Note: Must be last!
    STAT_MAX
};

// Error returns for signing in
enum
{
    E_NETWORK_ERROR = 1,
    E_ACCOUNT_ERROR,

    NUM_ELO_ERRORS
};

// Now, add whatever services are appropriate for your title, but no
// more. Each service requires additional authentication time
// and network traffic.  For demonstration purposes, the
// matchmaking service is specified.  Additional services ids are
// specified in xonline.h.    
const DWORD SERVICES[]   = { XONLINE_STRING_SERVICE,
                             XONLINE_TEAM_SERVICE,
                             XONLINE_STORAGE_SERVICE,
                             XONLINE_MESSAGING_SERVICE,
                             XONLINE_STATISTICS_SERVICE };

const DWORD NUM_SERVICES = sizeof( SERVICES ) / sizeof( SERVICES[0] );


////////////////
// Game Menus //
////////////////

// GAME SETUP menu
const WCHAR* const MENU_GAME_SETUP[]   =
{
    L"Team Leaderboard",
    L"Teams"
};
enum
{
    MENU_GAME_SETUP_TEAMS_LEADERBOARD = 0,
    MENU_GAME_SETUP_TEAMS,
    NUM_ITEMS_GAME_SETUP_MENU
};


// TEAMS menu
const WCHAR* const MENU_TEAMS[]        =
{
    L"Inbox",
    L"Recent Players",
    L"View My Teams",
    L"Create A Team",
};
enum
{
    MENU_TEAMS_INBOX = 0,
    MENU_TEAMS_RECENT_PLAYERS,
    MENU_TEAMS_VIEW_MY_TEAMS,
    MENU_TEAMS_CREATE_TEAM,
    NUM_ITEMS_TEAMS_MENU
};

const WCHAR* const MENU_INVITE[]       =
{
    L"Accept Invite",
    L"Decline Invite",
    L"Never Accept Invite From This Player"
};
enum
{
    MENU_INVITE_ACCEPT = 0,
    MENU_INVITE_DECLINE,
    MENU_INVITE_NEVER,
    NUM_ITEMS_INVITE_MENU
};

// TEAM MEMBER OPERATIONS menu
const WCHAR* const MENU_MEMBER_OPS[] =
{
    L"Set To Owner Permissions",
    L"Set To Recruiter Permissions",
    L"Set To Peon Permissions",
    L"Kick Off the Team"
};
enum
{
    MENU_MEMBER_OPS_SET_OWNER = 0,
    MENU_MEMBER_OPS_SET_RECRUITER,
    MENU_MEMBER_OPS_SET_PEON,
    MENU_MEMBER_OPS_KICKOFF,
    NUM_ITEMS_MEMBER_OPS_MENU  = 4
};


//-------------------------------------------------------------------------------------
// General utility functions
//-------------------------------------------------------------------------------------

VOID BootToDash( DWORD dwReason );

//-------------------------------------------------------------------------------------
// Classes
//-------------------------------------------------------------------------------------

// Class to hold some UI and login
// information about each local user
class CUserInfo
{
public:
    // *** General Xbox Live variables ***
    // Is the player currently logging into the service
    // Is the user signed onto Xbox Live stores result of attempt to sign in user
    BOOL                    m_bSignedIn;
    INT                     m_iCurSelection;

    // *** User data ***
    // Index of the current user logged into Live Points to an index in the array found 
    // by XOnlineGetUsers
    WORD                    m_wUserIndex;

public:

    CUserInfo() : m_bSignedIn( FALSE ), m_iCurSelection( 0 ), m_wUserIndex( 0 ) {}

    ~CUserInfo() {}
};

// Sample Application Class
// Demonstrates Team management and statistics
//
class CXBoxSample : public CXBApplication
{
public:

    // Events IDs used to trigger
    // transitions in the state machine
    // These refer to actions from
    // the game controller:
    enum Event
    {
        EV_BUTTON_A,                // "A" button pressed
        EV_BUTTON_B,                // "B" button pressed
        EV_BUTTON_X,                // "X" button pressed
        EV_BUTTON_Y,                // "Y" button pressed
        EV_BUTTON_BACK,             // "BACK" button pressed
        EV_BUTTON_WHITE,            // "WHITE" button pressed
        EV_BUTTON_BLACK,            // "BLACK" button pressed
        EV_UP,                      // DPAD UP pressed
        EV_DOWN,                    // DPAD DOWN pressed
        EV_NULL                     // No Event / Idle event
    };

    // UI States:
    enum EUIStates
    {
        STATE_SELECT_ACCOUNT,       // Allows the player select the Live account they 
                                    // wish to logon with
        STATE_LOGIN,                // Attempts to log a player onto the Xbox Live 
                                    // service
        STATE_LOGIN_FAILED,         // Screen that tells the user they were unable to 
                                    // logon to Live
        STATE_NETWORK_ERROR,        // Screen that tells the user they have experienced 
                                    // network problems
        STATE_GAME_SETUP,           // Menu that allows the player to create or join a 
                                    // match.
        STATE_TEAMS_LEADERBOARD,    // Allows the user to view the teams leaderboard
        STATE_TEAMS,                // Main menu for team features
        STATE_RECENT_PLAYERS,       // List of players the user can send invites to
        STATE_SELECT_INVITE_TEAM,   // Lets the player select a team to invite another player to
        STATE_INBOX,                // Let the player see their inbox and respond
        STATE_INVITE_DETAILS,       // Allows the user to respond to an invite
        STATE_VIEW_MY_TEAMS,        // Allows the user to view a list of teams they are a member of
        STATE_MANAGE_TEAM,          // Menu for administrative options of a team
        STATE_VIEW_TEAM_ROSTER,     // State to view the roster of a team
        STATE_TEAM_MEMBER_OPS,      // Allows a user to be granted permissions or booted
        STATE_MESSAGE_WINDOW,       // Window to display a message
        NUM_STATES
    };

    // Bit flags used the the RenderFooter function
    // to determine which parts to display/render
    enum EFooterFlags
    {
        FOOTER_RENDER_NONE   = 0,  // Render nothing / reset
        FOOTER_RENDER_SELECT = 1,  // Render "Select" in the BR corner
        FOOTER_RENDER_CANCEL = 2   // Render "Cancel" in the BL corner
    };

    enum EScreenView
    {
        VIEW_UPPER_LEFT = 0,
        VIEW_UPPER_RIGHT,
        VIEW_LOWER_LEFT,
        VIEW_LOWER_RIGHT,
        MAX_VIEWS
    };

protected:

    CUserInfo               m_localUsers[MAX_USERS];
    BOOL                    m_bUsersSignedIn;
    BOOL                    m_bUsersSigningIn;
    WORD                    m_wControllingUser;

    // STATE SPECIFIC VARIABLES!

    // State Select Account
    BOOL                    m_bUserSelectedAccount[MAX_USERS];
    WORD                    m_wNumUsersSelectedAccounts;
    CXBStopWatch            m_flashTimer;
    D3DCOLOR                m_continueTextColor;

    // State Login
    INT                     m_iSignInResult;

    // State TeamsLeaderboard
    XONLINE_STAT_USER       m_rwLeaderboardUsers[MAX_STAT_USERS];
    XONLINE_STAT            m_rwLeaderboardStats[MAX_STAT_USERS * STAT_MAX];
    DWORD                   m_dwNumLeaderboardUsers;

    // State Inbox
    DWORD                   m_dwMessageSelected;
    DWORD                   m_dwNumMessages;
    XONLINE_MSG_SUMMARY     m_rwMessagesSummaries[XONLINE_MAX_NUM_MESSAGES];

    // State Recent Players
    DWORD                   m_dwPlayerSelected;
    DWORD                   m_dwPlayerRenderStart;

    // State InviteDetails
    XONLINE_STAT_USER       m_teamUserInvitedTo;
    XONLINE_TEAM            m_teamInfoInvitedTo;
    DWORD                   m_dwInviteResponseSelected;

    // State ViewMyTeams
    XUID                    m_rwTeamXUIDS[XONLINE_MAX_TEAM_COUNT];
    DWORD                   m_dwTeamCount;
    XONLINE_TEAM            *m_rwTeamInfo;
    XONLINE_TEAM_PROPERTIES m_createdTeamProps;

    // State ViewTeamRoster
    CXBOnlineTask           m_phTeamRosterTask;
    DWORD                   m_dwTeamMemberCount;
    DWORD                   m_dwRosterRenderStart;
    DWORD                   m_dwTeamMemberSelected;
    XUID                    m_rwTeamMembers[XONLINE_MAX_TEAM_MEMBER_COUNT];

    // State GameMessage
    WCHAR                   m_szGameMessage[MAX_MESSAGE_LENGTH];

    // *** UI specific variables ***
    // Font object used to render the UI's text
    CXBFont                 m_font;         
    // The current color of the UI's background
    D3DCOLOR                m_bgColor;
    // The current state of the UI
    EUIStates               m_state;
    EUIStates               m_stateStack[MAX_SIZE_STATE_STACK];
    WORD                    m_wStateStackSize;
    // Index of the item currently selected in the UI menu
    INT                     m_iItemSelected;

    // Task to logon the user and to send Keep-Alives to Xbox Live
    CXBOnlineTask           m_hLogonTask;

    // User data for all Xbox Live accounts held on the Hard Drive and memory units
    XONLINE_USER            m_rwStoredUsers[ XONLINE_MAX_STORED_ONLINE_USERS ];          
    // The number of users stored in m_rwStoredUsers
    DWORD                   m_dwNumStoredUsers;
    BOOL                    m_rwStoredUserSelected[XONLINE_MAX_STORED_ONLINE_USERS];

    // Network session helper functions
    INT     StartSignIn();
    INT     ContinueSignIn();
    INT     FinishSignIn();

    // Team managment helper functions!
    BOOL     ChangeTeamProperties( INT iUser );
    HRESULT  SendInvite( XUID xuidTeam, XUID xuidNewRecruit );
    HRESULT  ProcessInvite( INT iMessageIndex, XONLINE_PEER_ANSWER_TYPE eAnswer );
    HRESULT  CreateTeam();
    HRESULT  RemoveTeamMember( XUID xuidTeam, XUID xuidMember );
    HRESULT  DeleteTeam( XUID xuidTeam );
    HRESULT  SetPermissions( XUID xuidTeam, XUID xuidMember, DWORD dwNewPrivileges );
    PWORD    GetStatIDs( DWORD* pdwNumStats );
    BOOL     WriteTeamStatistics( LONG &lKills, LONG &lDeaths, LONG &lAssists,
                                  LONGLONG &llRating );
    BOOL     GetTeamLeaderboard();
    BOOL     GetTeamList();
    BOOL     GetTeamRoster();

    // State handling functions
    // Functions that handle the
    // entrance into, updating of,
    // rendering of, and exiting
    // of the UI/Game states

    Event   GetEvent() const;
    Event   GetEvent( INT iController ) const;
    VOID    PushState( EUIStates newState );
    VOID    PopState( BOOL bReinit = FALSE );
    VOID    ClearStack() { m_wStateStackSize = 1; m_state = m_stateStack[0];}
    VOID    PushMessageWindow( const CHAR* strTextMessage );

    VOID    RenderMenu( const WCHAR* strMenuName,
                        const WCHAR** rwMenuText,
                        const WORD wNumMenuItems,
                        const INT iCurMenuItem );

    INT     GetMenuPosition( INT iCurMenuPosition, INT iNumMenuItems, Event , 
                             INT iMenuWrap = MENU_WRAP_ON );

    VOID    RenderAccountSelectionView( EScreenView eViewWindow );

    // State SelectAccount
    VOID    EnterStateSelectAccount();
    VOID    UpdateStateSelectAccount( INT iUser, Event event );
    VOID    RenderStateSelectAccount();
    VOID    ExitStateSelectAccount() {}

    // State LoginPassword
    VOID    EnterStateLogin();
    VOID    UpdateStateLogin( Event );
    VOID    RenderStateLogin();
    VOID    ExitStateLogin() {}

    // State LoginFailed
    VOID    EnterStateLoginFailed() {}
    VOID    UpdateStateLoginFailed( Event event );
    VOID    RenderStateLoginFailed();
    VOID    ExitStateLoginFailed() {}

    // State NetworkError
    VOID    EnterStateNetworkError() {}
    VOID    UpdateStateNetworkError( Event event );
    VOID    RenderStateNetworkError();
    VOID    ExitStateNetworkError();

    // State GameSetup
    VOID    EnterStateGameSetup();
    VOID    UpdateStateGameSetup( INT iUser, Event event );
    VOID    RenderStateGameSetup();
    VOID    ExitStateGameSetup() {}

    // State TeamsLeaderboard
    VOID    EnterStateTeamsLeaderboard() {}
    VOID    UpdateStateTeamsLeaderboard( INT iUser, Event event );
    VOID    RenderStateTeamsLeaderboard();
    VOID    ExitStateTeamsLeaderboard() {}

    // State Teams
    VOID    EnterStateTeams();
    VOID    UpdateStateTeams( INT iUser, Event event );
    VOID    RenderStateTeams();
    VOID    ExitStateTeams() {}

    // State RecentPlayers
    VOID    EnterStateRecentPlayers();
    VOID    UpdateStateRecentPlayers( INT iUser, Event event );
    VOID    RenderStateRecentPlayers();
    VOID    ExitStateRecentPlayers() {}

    // State SelectInviteTeam
    VOID    EnterStateSelectInviteTeam();
    VOID    UpdateStateSelectInviteTeam( INT iUser, Event event );
    VOID    RenderStateSelectInviteTeam();
    VOID    ExitStateSelectInviteTeam() {}

    // State Inbox
    VOID    EnterStateInbox();
    VOID    UpdateStateInbox( INT iUser, Event event );
    VOID    RenderStateInbox();
    VOID    ExitStateInbox() {}

    // State InviteDetails
    VOID    EnterStateInviteDetails();
    VOID    UpdateStateInviteDetails( INT iUser, Event event );
    VOID    RenderStateInviteDetails();
    VOID    ExitStateInviteDetails() {}

    // State ViewMyTeams
    VOID    EnterStateViewMyTeams();
    VOID    UpdateStateViewMyTeams( INT iUser, Event event );
    VOID    RenderStateViewMyTeams();
    VOID    ExitStateViewMyTeams() {}

    // State ViewTeamRoster
    VOID    EnterStateViewTeamRoster() { m_dwRosterRenderStart = 0; m_dwTeamMemberSelected = 0;}
    VOID    UpdateStateViewTeamRoster( INT iUser, Event event );
    VOID    RenderStateViewTeamRoster();
    VOID    ExitStateViewTeamRoster() {}

    // State TeamMemberOps
    VOID    EnterStateTeamMemberOps() {m_localUsers[m_wControllingUser].m_iCurSelection= 0;}
    VOID    UpdateStateTeamMemberOps( INT iUser, Event event );
    VOID    RenderStateTeamMemberOps();
    VOID    ExitStateTeamMemberOps() {}

    // State MessageWindow
    VOID    EnterStateMessageWindow() {}
    VOID    UpdateStateMessageWindow( INT iUser, Event event );
    VOID    RenderStateMessageWindow();
    VOID    ExitStateMessageWindow() {}

    // Extra rendering functions
    VOID    RenderHeader();
    VOID    RenderControllingUser();
    VOID    RenderFooter( WORD flags );

public:


    // Overloaded functions defined by the application
    // class to execute game logic and rendering
    virtual HRESULT         Render();
    virtual HRESULT         Initialize();
    virtual HRESULT         FrameMove();

    ~CXBoxSample();
};

#endif // TEAMSDEMO_H