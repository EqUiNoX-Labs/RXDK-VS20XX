//-------------------------------------------------------------------------------------
// File: IntegratedDemo.h
//
// Desc: Definitions of the sample application, it's helper functions and constants
//
// Hist: 12.09.04 - New for January release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#pragma once

#ifndef INTEGRATEDDEMO_H
#define INTEGRATEDDEMO_H

#include <vector>
#include <Xbox.h>
#include <XOnline.h>
#include "XBApp.h"
#include "XBNet.h"
#include "XBOnline.h"
#include "XBOnlineTask.h"
#include "XBRandName.h"
#include "XHVVoiceManager.h"

#include "Player.h"
#include "Teams.h"
#include "UserContent.h"
#include "Menus.h"
#include "Match.h"
#include "GameSession.h"
#include "UserSettings.h"

#define CURRENT_USER m_rwStoredUsers[m_rwLocalUsers[m_wControllingUser].m_wUserIndex]

//-------------------------------------------------------------------------------------
// Constants
//-------------------------------------------------------------------------------------


// Used for displaying a window of accounts per Xbox
const INT NUM_ACCOUNTS_PER_WINDOW         = 4;
const INT MAX_MESSAGE_LENGTH              = 128; // Length of in game messages
const INT MAX_SIZE_STATE_STACK            = 10;  // The maximum size of the state stack

// Used to time network operations
const FLOAT QUICKMATCH_SEARCH_TIME        = 5.0f; // Maximum time to try to connect
                                                  // to a QuickMatch session
const FLOAT ARBITRATION_REGISTRATION_TIME = 5.0f; // Time to give all registrations
                                                  // to register with arbitration
                                                  // before starting the game session


// Maximum number of voice streams to use for playback
const DWORD NUM_XHV_PLAYBACK_STREAMS = 2;
const DWORD MAX_TEAM_MESSAGE_LENGTH  = 256;

// Error returns for signing in
enum
{
    E_NETWORK_ERROR = 1,
    E_ACCOUNT_ERROR,

    NUM_ELO_ERRORS
};


// Leaderboard ID numbers
const INT COMPETITION_LEADERBOARD_ID = 1;
const INT INVIDUAL_LEADERBOARD_ID    = 2;
const INT TEAM_LEADERBOARD_ID        = 3;

//-------------------------------------------------------------------------------------
// Classes
//-------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------
// Sample Application Class
// Demonstrates Team management and statistics
//-------------------------------------------------------------------------------------
class CXBoxSample : public CXBApplication, public ITitleXHV
{
public:

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
        STATE_MAIN,                 // Menu that allows the player to create or join a
                                    // match.
        STATE_TEAMS_LEADERBOARD,    // Allows the user to view the teams leaderboard
        STATE_TEAMS,                // Main menu for team features
        STATE_RECENT_PLAYERS,       // List of players the user can send invites to
        STATE_SELECT_INVITE_TEAM,   // Lets the player select a team to 
                                    // invite another player to
        STATE_QUICKMATCH,           // Attempts to find an existing match via the
                                    // MatchMaking service
        STATE_CREATE_MATCH,         // Creates a match that other Live users can join
        STATE_GAME_LOBBY,           // Gathering place for users before a match starts
        STATE_GAME_SESSION,         // The actual competition
        STATE_INBOX,                // Let the player see their inbox and respond
        STATE_SETTINGS_EDIT,        // Allows the user to edit settings
        STATE_INVITE_DETAILS,       // Allows the user to respond to an invite
        STATE_VIEW_MY_TEAMS,        // Allows the user to view a list of teams 
                                    // they are a member of
        STATE_TEAM_SEND_MESSAGE,    // handles sending messages to teams
        STATE_TEAM_SHOW_MESSAGE,    // handles showing messages to team members
        STATE_TEAM_OPS,             // Menu for team operations
        STATE_LIST_AVAILABLE_COMPS, // Allow the user to choose which comps to join
        STATE_MANAGE_TEAM,          // Menu for administrative options of a team
        STATE_VIEW_TEAM_ROSTER,     // State to view the roster of a team
        STATE_TEAM_MEMBER_OPS,      // Allows a user to be granted permissions or booted
        STATE_CONTENT_EDIT,         // Icon editor screen
        STATE_LIST_TOURNEYS,        // List of competitions the user has joined
        STATE_TOURNEY_RENDER,       // Topography screen of a specific competition
        STATE_MESSAGE_WINDOW,       // Window to display a message
        NUM_STATES
    };

    // View IDs for the multiscreen logon
    enum EScreenView
    {
        VIEW_UPPER_LEFT = 0,
        VIEW_UPPER_RIGHT,
        VIEW_LOWER_LEFT,
        VIEW_LOWER_RIGHT,
        MAX_VIEWS
    };

    // IDs for text/voice mail operations
    enum EVoiceLevel
    {
        VOICE_LEVEL_NO_PLAYER,
        VOICE_LEVEL_NOT_ALLOWED,
        VOICE_LEVEL_NO_COMMUNICATOR,
        VOICE_LEVEL_EVERYTHING,
        MAX_VOICE_LEVELS
    };

    enum EVoiceMessageSuccess
    {
        VOICE_MAIL_SUCCESS,
        VOICE_MAIL_ERROR_COULDNT_CREATE_MESSAGE,
        VOICE_MAIL_ERROR_NO_TEAM_EXISTS,
        VOICE_MAIL_ERROR_NO_RECIPIENTS
    };

    enum EVoiceMessageDownloadSuccess
    {
        INVALID_VOICE_MAIL_DOWNLOAD_ERROR = -1,
        VOICE_MAIL_DOWNLOAD_SUCCESS,
        VOICE_MAIL_DOWNLOAD_ERROR_COULDNT_ACCESS_MESSAGE,
        VOICE_MAIL_DOWNLOAD_ERROR_VOICE_ATTACHMENT_MISSING,
        VOICE_MAIL_DOWNLOAD_ERROR_DOWNLOAD_FAILED
    };

    enum EVoiceMessageProtocol
    {
        INVALID_VOICE_MESSAGE_PROTOCOL = -1,
        VOICE_MAIL_SENT_TO_TEAM_SAVE_SELF,
        VOICE_MAIL_SENT_TO_TEAM_OWNERS,
        VOICE_MAIL_SENT_TO_INDIVIDUAL
    };

protected:

    // Network sockets and variables
    CXBSocket       m_DirectSock;           // Direct UDP socket for direct msgs
    PendingMessage  m_msgPending;           // Pending message for client
    SocketList      m_ClientSockets;        // Reliable sockets for low-bandwidth msgs
    XNKID           m_xnHostKeyID;          // Host key ID
    XNKEY           m_xnHostKeyExchange;    // Host key exchange key
    XNADDR          m_xnTitleAddress;       // The XNet address of this machine/game
    IN_ADDR         m_inHostAddr;           // The "IP" address of the host
    BOOL            m_bXnetStarted;         // Have we started XNet?
    BOOL            m_bIsKeyRegistered;     // Have we registered a key?

    CXBStopWatch m_LinkStatusTimer;      // Wait to check link status
    CXBStopWatch m_GameSearchTimer;      // Wait for game search to complete
    CXBStopWatch m_GameJoinTimer;        // Wait for game join to complete
    CXBStopWatch m_HeartbeatTimer;       // Keep-alive timer
    CXBStopWatch m_VoiceTimer;           // Voice packet timer

    LPDIRECTSOUND8        m_pDSound;              // DirectSound object
    LPDSEFFECTIMAGEDESC   m_pdsImageDesc;         // DirectSound effect descrip

    BOOL          m_bXHVInitialized;              // TRUE if we've initialized XHV
    BOOL          m_bVoiceBufferPlayable;         // boolean determining if buffer is playable
    BOOL          m_bGotCommunicatorRemovalEvent; // TRUE if callback caught removal of
    BOOL          m_bCommunicatorInserted;        // TRUE if communicator is inserted 
                                                  // during send message screen


    // User Info
    CUserInfo     m_rwLocalUsers[MAX_USERS]; // Array corresponding to controllers
                                             // Stores the user's index into
                                             // m_rwStoredUsers
    BOOL          m_bUsersSignedIn;          // Are any users signed in?
    BOOL          m_bUsersSigningIn;         // Are any users in the process of signing in?
    WORD          m_wControllingUser;        // Which controller port has exclusive access
    CXBOnlineTask m_hLogonTask;              // Task to logon the user and
                                             // to send Keep-Alives to Xbox Live
    DWORD         m_dwNumStoredUsers;        // Number of user accounts at the
                                             // time of logon

    // User data for all Xbox Live accounts held on the
    // Hard Drive and memory units at the time of logon
    XONLINE_USER  m_rwStoredUsers[ XONLINE_MAX_STORED_ONLINE_USERS ];

    // Array to keep track of which accounts have been selected
    // for logon. Used to keep mulitple controllers from
    // attempting to logon with the same account
    BOOL          m_rwStoredUserSelected[XONLINE_MAX_STORED_ONLINE_USERS];


    // Send Message
    INT                          m_iMessageSelected;  // index of team message selected
    BOOL                         m_bMessageSent;      // TRUE if message sent
    EVoiceMessageProtocol        m_eMessageProtocol;  // message sending protocol
    ULONGLONG                    m_qwProtocolParamId; // id of team/user in context of message sending protocol

    // gamer tag of message user recipient
    WCHAR                        m_swzRecipientTag[ XONLINE_GAMERTAG_SIZE ];

    // Show Message
    XONLINE_MSG_SUMMARY          m_curMessageSummary;       // message summary for currently
                                                            // selected message
    EVoiceMessageDownloadSuccess m_eMessageDownloadSuccess; // TRUE if download succeeded
    BOOL                         m_bMessageDeleted;         // TRUE if text/voice message
                                                            // just deleted


    //////////////////////////////
    // STATE SPECIFIC VARIABLES //
    //////////////////////////////

    // State Select Account
    BOOL                    m_bUserSelectedAccount[MAX_USERS]; // Keep track of which accounts
                                                               // will be used to logon
                                                               // so they cannot be selected
                                                               // twice.
    WORD                    m_wNumUsersSelectedAccounts;       // Number of accounts selected
    CXBStopWatch            m_flashTimer;                      // Timer to keep track
                                                               // of "Continue" and "Cancel"
                                                               // text
    D3DCOLOR                m_continueTextColor;               // Color of the "Continue"
                                                               // and "Cancel" text

    // State Login
    INT                     m_iSignInResult;

    // State EditSettings
    CXBOnlineTask           m_hUserSettingsTask;
    BYTE                    m_pSettingsReceiveBuffer[SETTINGS_DL_BUFFER_SIZE];
    CUserSettings           m_userSettings;
    CUserSettings           m_editableUserSettings;

    // State TeamsLeaderboard
    XONLINE_STAT_USER       m_rwLeaderboardUsers[MAX_STAT_USERS];
    XONLINE_STAT            m_rwLeaderboardStats[MAX_STAT_USERS * STAT_MAX];
    DWORD                   m_dwNumLeaderboardUsers;

    // State ContentEdit
    CUserContent            m_userContent;
    INT                     m_iTurtleX;                      // Position of the pen
    INT                     m_iTurtleY;                      // Position of the pen
    CXBStopWatch            m_turtleFlashTimer;              // Used to make the pen flash
    D3DCOLOR                m_dwTurtleColor;                 // Color of the "pen"/turtle
    LPDIRECT3DTEXTURE8      m_lpPreviewTexture;              // Texture handle to rasterize
                                                             // the content to to be displayed
    DWORD                   m_rwButtonColorMap[6];           // Quick lookup/map of the button
                                                             // to controller input
    WCHAR                   m_swzBaseFilename[MAX_GAMENAME];
    BOOL                    m_bUploadInsteadOfSave;          // Should we upload or 
                                                             // save to disk?
    BOOL                    m_bTeamLogo;                     // Is this content a team logo?
    WCHAR*                  m_wszFilename;


    // State ListTourneys
    ULONGLONG m_qwCompetitionStatus;  // The status of the competition selected
    DWORD     m_dwTourneyRenderStart; // Used to occlude competitions not shown on the screen
    DWORD     m_dwTourneySelected;    // Index of the competition selected
    DWORD     m_dwEventRound;         // Round number that the user will participate in next

    // State TourneyRender
    DWORD     m_dwCompetitorRenderStart; // Used to occlude teams not being rendered
    DWORD     m_dwCompetitorSelected;    // Index of team currently hilighted
    BOOL      m_bRenderCompetitors;      // Should we render the list of competitors
                                         // or the competition topography
    DWORD     m_dwTopologyCount;         // How many rounds of data did we
                                         // get about the competition topology.

    // State CreateMatch
    SessionInfo             m_sessionInfo;
    CSession                m_hostedSession;               // Object used to update
                                                           // and control the match
                                                           // with the MatchMaking
                                                           // Service.
    DWORD                   m_dwSlotsInUse;                // The number of players
                                                           // who will try to register
                                                           // with arbitration
    DWORD                   m_dwPlayersRegistered;         // The number of players
                                                           // who have registered with
                                                           // arbitration.
    BOOL                    m_bArbitrationStarted;         // Has arbitration registration
                                                           // been closed?
    CXBStopWatch            m_regTimer;                    // Used to limit the amount
                                                           // of time the clients have
                                                           // to register with arbitration.
    BOOL                    m_bIsHost;                     // Is this machine the host?
    BOOL                    m_bTourneySession;             // Is this game session
        //                                                 // a competition round?
    BOOL                    m_bXUIDsCopied;                // Have the XUIDs of all
                                                           // the players been copied
                                                           // from the host yet?
    DWORD                   m_rwScores[MAX_MATCHERS];      // Scores of all the players
                                                           // in the match
    XUID                    m_rwPlayerXUIDs[MAX_MATCHERS]; // XUIDs of all the players
                                                           // in the match


    // State QuickMatch
    FLOAT                   m_fRequestTime;
    COptiMatchQuery         m_matchQuery;
    SessionList             m_rwSessionList;
    BOOL                    m_bJoinedGame;

    // State GameLobby
    BOOL                    m_bWaitingToJoin;
    PlayerList              m_rwPlayers;
    XONLINE_ARB_ID          m_arbID;

    // State Inbox
    DWORD                   m_dwMessageSelected;
    DWORD                   m_dwMessageRenderStart;
    DWORD                   m_dwNumMessages;
    XONLINE_MSG_SUMMARY     m_rwMessagesSummaries[XONLINE_MAX_NUM_MESSAGES];

    // State RecentPlayers
    DWORD                   m_dwPlayerSelected;
    DWORD                   m_dwPlayerRenderStart;
    BOOL                    m_bTeamListRetrieved;


    // State InviteDetails
    XONLINE_STAT_USER       m_teamUserInvitedTo;
    XONLINE_TEAM            m_teamInfoInvitedTo;
    DWORD                   m_dwInviteResponseSelected;


    // State ViewMyTeams
    XUID                    m_rwTeamXUIDS[XONLINE_MAX_TEAM_COUNT];
    DWORD                   m_dwTeamCount;
    XONLINE_TEAM_PROPERTIES m_createdTeamProps;
    INT                     m_iTeamSelected;
    XONLINE_TEAM            m_rwTeamInfo[XONLINE_MAX_TEAM_COUNT];
    LPDIRECT3DTEXTURE8*     m_ppTeamLogoTextures;
    DWORD                   m_dwTeamLogoToDL;

    // State ListAvailableComps
    DWORD                   m_dwAvailableCompSelected;
    DWORD                   m_dwAvailableCompRenderStart;

    // State ViewTeamRoster
    BOOL                    m_bDisableRosterOps;
    CXBOnlineTask           m_phTeamRosterTask;
    DWORD                   m_dwTeamMemberCount;
    DWORD                   m_dwRosterRenderStart;
    DWORD                   m_dwTeamMemberSelected;
    XUID                    m_rwTeamMembers[XONLINE_MAX_TEAM_MEMBER_COUNT];
    DWORD                   m_dwTeamMemberTextureToDL;
    LPDIRECT3DTEXTURE8*     m_ppTeammateTextures;
    LPDIRECT3DVERTEXBUFFER8 m_pLogoVerts;


    // Progress bar variables
    DWORD                   m_dwProgressActivity;
    BOOL                    m_bProgressSucceeded;
    DWORD                   m_dwProgressPercentage;
    WCHAR                   m_wszProgressMessageFormat[MAX_MESSAGE_LENGTH];
    WCHAR                   m_wszProgressMessage[MAX_MESSAGE_LENGTH];

    // State GameMessage
    WCHAR                   m_szGameMessage[MAX_MESSAGE_LENGTH]; // Message text to display
    BOOL                    m_bReInitAfterWindowMessage;         // TRUE if were are
                                                                 // reinitializing the
                                                                 // previous state upon exit


    // UI specific variables
    CXBFont                 m_font;                             // Font object used to
                                                                // render the UI's text
    D3DCOLOR                m_bgColor;                          // The current color of the
                                                                // UI's background
    EUIStates               m_state;                            // The current state of the UI
    EUIStates               m_stateStack[MAX_SIZE_STATE_STACK]; // Stack of UI states
    WORD                    m_wStateStackSize;                  // The number of states
                                                                // in the state stack
    INT                     m_iItemSelected;                    // Index of the item currently
                                                                // selected in the UI menu


    //////////////////////
    // Helper functions //
    //////////////////////

    BOOL    UnregisterKey();
    BOOL    RegisterKey( const XNKID *pxnkid, const XNKEY *pxnkey );

    BOOL AllLocalUsersAreOnSameTeam();
    BOOL AllUsersAreOnSameTeam();
    WORD GetNumLoggedOnUsers();

    // Competition helper functions
    HRESULT FindCompetitionSession( DWORD dwControllerPort,
                                    ULONGLONG qwCompID,
                                    ULONGLONG qwRoundID,
                                    IN_ADDR& hostAddr );

    HRESULT CreateCompetitionSession( DWORD dwControllingUser,
                                      ULONGLONG qwTeamID,
                                      ULONGLONG qwCompID,
                                      ULONGLONG qwRoundID );

    EVoiceLevel GetVoiceLevel( DWORD dwPort );

    // Send Message helper
    VOID    UpdateTeamMessageToSend();

    // Callback functions inherited from iTitleXHV
    STDMETHOD_( HRESULT, VoiceMailStopped)( DWORD );
    STDMETHOD_( HRESULT, VoiceMailDataReady)( DWORD, DWORD, DWORD );

    // Network session helper functions
    INT StartSignIn();
    INT ContinueSignIn();
    INT FinishSignIn();

    VOID CopyXUIDs( const XUID* pXUIDs );
    VOID RemovePlayer();

    // Voice message methods
    EVoiceMessageSuccess             SendVoiceMessage( 
                                     XUID xuidSendingTeam, 
                                     BOOL bHasVoice , 
                                     EVoiceMessageProtocol eProtocol ,
                                     ULONGLONG qwParamID );

    BOOL                             MessageHasVoiceAttachment( 
                                     XONLINE_MSG_SUMMARY& msgSummary );

    EVoiceMessageDownloadSuccess     DownloadTextVoiceMessage( 
                                     XONLINE_MSG_SUMMARY& msgSummary );

    // State handling functions
    // Functions that handle the
    // entrance into, updating of,
    // rendering of, and exiting
    // of the UI/Game states

    VOID ExitState( INT iState );
    VOID PushState( EUIStates newState );
    VOID PopState( BOOL bReinit = FALSE );
    VOID ClearStack() { m_wStateStackSize = 1; m_state = m_stateStack[0];}
    VOID PushMessageWindow( const CHAR* strTextMessage , BOOL bReInit = FALSE );

    VOID RenderAccountSelectionView( EScreenView eViewWindow,
                                     BOOL bControllerInserted );


    // Progress methods
    // initializes progress task and states thereof; and creates a progress
    // message showing how much progress has occurred in an upload/download task
    // (note: this should be in a printf style format to allow display of integer
    // percentage number i.e. "Upload content progress so far: %u")
    // also, message string should be no longer than MAX_MESSAGE_LENGTH - 1
    VOID    SetProgressTask( DWORD dwProgressActivity,
                             const CHAR* szMessageFormat );
    // attempts to get progress for current progress task.  If attempt fails,
    // FALSE is returned, else TRUE
    BOOL    UpdateProgressForTask( CXBOnlineTask& task );
    // displays a screen with progress message
    VOID    RenderProgressWindow();
    // TRUE if progress has completed (100%) otherwise false
    BOOL    ProgressCompleted()  { return (BOOL)( m_dwProgressPercentage == 100 ); }
    // deinitializes progress task and states thereof
    VOID    ClearProgressTask();

    ///////////////////////////
    // State Member Function //
    ///////////////////////////

    // State SelectAccount
    BOOL EnterStateSelectAccount();
    VOID UpdateStateSelectAccount( DWORD dwControllerPort, Event event );
    VOID RenderStateSelectAccount();
    VOID ExitStateSelectAccount() {}

    // State LoginPassword
    BOOL EnterStateLogin();
    VOID UpdateStateLogin( Event );
    VOID RenderStateLogin();
    VOID ExitStateLogin() {}

    // State LoginFailed
    BOOL EnterStateLoginFailed() { return TRUE; }
    VOID UpdateStateLoginFailed( Event event );
    VOID RenderStateLoginFailed();
    VOID ExitStateLoginFailed() {}

    // State NetworkError
    BOOL EnterStateNetworkError() { return TRUE; }
    VOID UpdateStateNetworkError( Event event );
    VOID RenderStateNetworkError();
    VOID ExitStateNetworkError();

    // State Main
    BOOL EnterStateMain() { m_iItemSelected = 0; return TRUE; }
    VOID UpdateStateMain( DWORD dwControllerPort, Event event );
    VOID RenderStateMain();
    VOID ExitStateMain() {}

    // State SettingsEdit
    BOOL EnterStateSettingsEdit();
    VOID UpdateStateSettingsEdit( DWORD dwControllerPort, Event event );
    VOID RenderStateSettingsEdit();
    VOID ExitStateSettingsEdit() {}

    // State TeamsLeaderboard
    BOOL EnterStateTeamsLeaderboard() { return TRUE; }
    VOID UpdateStateTeamsLeaderboard( DWORD dwControllerPort, Event event );
    VOID RenderStateTeamsLeaderboard();
    VOID ExitStateTeamsLeaderboard() {}

    // State Teams
    BOOL EnterStateTeams() { m_iItemSelected = 0; return TRUE; }
    VOID UpdateStateTeams( DWORD dwControllerPort, Event event );
    VOID RenderStateTeams();
    VOID ExitStateTeams() {}

    // State RecentPlayers
    BOOL EnterStateRecentPlayers();
    VOID UpdateStateRecentPlayers( DWORD dwControllerPort, Event event );
    VOID RenderStateRecentPlayers();
    VOID ExitStateRecentPlayers() { m_bTeamListRetrieved = FALSE; }

    // State SelectInviteTeam
    BOOL EnterStateSelectInviteTeam() { m_iTeamSelected = 0; return TRUE; }
    VOID UpdateStateSelectInviteTeam( DWORD dwControllerPort, Event event );
    VOID RenderStateSelectInviteTeam();
    VOID ExitStateSelectInviteTeam() {}

    // State CreateMatch
    BOOL EnterStateCreateMatch();
    VOID UpdateStateCreateMatch( DWORD dwControllerPort, Event event );
    VOID RenderStateCreateMatch();
    VOID ExitStateCreateMatch() {}

    // State QuickMatch
    BOOL EnterStateQuickMatch();
    VOID UpdateStateQuickMatch( DWORD dwControllerPort, Event event );
    VOID RenderStateQuickMatch();
    VOID ExitStateQuickMatch();

    // State GameLobby
    BOOL EnterStateGameLobby() { m_iItemSelected  = 0; m_bWaitingToJoin = FALSE; return TRUE; }
    VOID UpdateStateGameLobby( DWORD dwControllerPort, Event event );
    VOID RenderStateGameLobby();
    VOID ExitStateGameLobby() { m_iItemSelected = 0; }

    // State GameSession
    BOOL EnterStateGameSession() { return TRUE; }
    VOID UpdateStateGameSession( DWORD dwControllerPort, Event event );
    VOID RenderStateGameSession();
    VOID ExitStateGameSession();

    // State Inbox
    BOOL EnterStateInbox();
    VOID UpdateStateInbox( DWORD dwControllerPort, Event event );
    VOID RenderStateInbox();
    VOID ExitStateInbox() {}

    // State InviteDetails
    BOOL EnterStateInviteDetails();
    VOID UpdateStateInviteDetails( DWORD dwControllerPort, Event event );
    VOID RenderStateInviteDetails();
    VOID ExitStateInviteDetails() {}

    // State ViewMyTeams
    BOOL EnterStateViewMyTeams() { m_iTeamSelected = 0; return TRUE; }
    VOID UpdateStateViewMyTeams( DWORD dwControllerPort, Event event );
    VOID RenderStateViewMyTeams();
    VOID ExitStateViewMyTeams() {}

    // State MessageCenter
    BOOL EnterStateSendMessage();
    VOID UpdateStateSendMessage( DWORD dwControllerPort, Event event );
    VOID RenderStateSendMessage();
    VOID ExitStateSendMessage();

    // State Show Message
    BOOL EnterStateShowMessage();
    VOID UpdateStateShowMessage( DWORD dwControllerPort, Event event );
    VOID RenderStateShowMessage();
    VOID ExitStateShowMessage();

    // State TeamOps
    BOOL EnterStateTeamOps() { m_iItemSelected = 0; return TRUE; }
    VOID UpdateStateTeamOps( DWORD dwControllerPort, Event event );
    VOID RenderStateTeamOps();
    VOID ExitStateTeamOps() {}

    // State ListAvailableComps
    BOOL EnterStateListAvailableComps();
    VOID UpdateStateListAvailableComps( DWORD dwControllerPort, Event event );
    VOID RenderStateListAvailableComps();
    VOID ExitStateListAvailableComps() {}

    // State ViewTeamRoster
    BOOL EnterStateViewTeamRoster();
    VOID UpdateStateViewTeamRoster( DWORD dwControllerPort, Event event );
    VOID RenderStateViewTeamRoster();
    VOID ExitStateViewTeamRoster() { m_dwRosterRenderStart = 0; m_dwTeamMemberTextureToDL = 0;}

    // State TeamMemberOps
    BOOL EnterStateTeamMemberOps() 
        { m_rwLocalUsers[m_wControllingUser].m_iCurSelection = 0; return TRUE; }
    VOID UpdateStateTeamMemberOps( DWORD dwControllerPort, Event event );
    VOID RenderStateTeamMemberOps();
    VOID ExitStateTeamMemberOps() {}

    // State ContentEdit
    BOOL EnterStateContentEdit();
    VOID UpdateStateContentEdit( DWORD dwControllerPort, Event event );
    VOID RenderStateContentEdit();
    VOID ExitStateContentEdit() {}

    // State ListTourneys
    BOOL EnterStateListTourneys() { m_dwTourneySelected = 0;
                                    m_dwTourneyRenderStart = 0;
                                    return TRUE; }
    VOID UpdateStateListTourneys( DWORD dwControllerPort, Event event );
    VOID RenderStateListTourneys();
    VOID ExitStateListTourneys() {}

    // State TourneyRender
    BOOL EnterStateTourneyRender();
    VOID UpdateStateTourneyRender( DWORD dwControllerPort, Event event );
    VOID RenderStateTourneyRender();
    VOID ExitStateTourneyRender() {}

    // State MessageWindow
    BOOL EnterStateMessageWindow() { return TRUE; }
    VOID UpdateStateMessageWindow( DWORD dwControllerPort, Event event );
    VOID RenderStateMessageWindow();
    VOID ExitStateMessageWindow() {}

    // Extra rendering functions
    VOID RenderHeader();
    VOID RenderControllingUser();

public:

    // Helpers to send network messages
    VOID SendJoinGame( const SOCKADDR_IN& );
    VOID SendJoinApproved( const SOCKADDR_IN& );
    VOID SendJoinDenied( const SOCKADDR_IN& );
    VOID SendPlayerJoinedToAll( const Player& );
    BOOL SendStartArbitrationRegistration();
    VOID SendScore( DWORD dwControllerPort );
    VOID SendGameOver();
    VOID SendHeartbeatToAll();
    VOID SendScoreToAll( Message& msg );
    INT  SendMessage( const Message* pMsg, const SOCKADDR_IN* psaDest = NULL );

    // Helpers to receive messages
    BOOL ProcessDirectMessage();
    VOID ProcessMessage( Message&, const SOCKADDR_IN& );

    // Helpers to process network messages
    VOID ProcessJoinGame( const MsgJoinGame&, const SOCKADDR_IN& );
    VOID ProcessClientRegistered( const MsgRegistered& msgRegistered );
    VOID ProcessJoinApproved( const MsgJoinApproved&, const SOCKADDR_IN& );
    VOID ProcessJoinDenied( const SOCKADDR_IN& );
    VOID ProcessPlayerJoined( const MsgPlayerJoined&, const SOCKADDR_IN& );
    VOID ProcessScore( ULONGLONG qwID );
    VOID ProcessHeartbeat( const SOCKADDR_IN& );

    // Helpers for to handle players joining and leaving
    HRESULT OnPlayerJoined( const CHAR* strName, XUID xuid, const XNADDR& xnAddr, 
                            in_addr* pinAddr );
    HRESULT OnPlayerDisconnect( PlayerInfo* pPlayer );
    VOID OnPlayerDropout( const PlayerInfo& playerInfo, BOOL bIsHost );
    BOOL ProcessPlayerDropouts();

    // Arbitration Helper functions
    HRESULT RegisterForArbitration();
    HRESULT SubmitFFAArbitrationResults( BOOL bWithdrawl = FALSE );
    HRESULT SubmitCompetitionArbitrationResults( BOOL bWithdrawl = FALSE );
    HRESULT SubmitArbitrationResults( BOOL bWithdrawl = FALSE );


    INT  GetPlayerScore( ULONGLONG qwID );
    BOOL IsOnMyTeam( ULONGLONG qwUserID );
    BOOL CalcTeamScores( ULONGLONG& qwTeam1ID, ULONGLONG& qwTeam1Score,
                         ULONGLONG& qwTeam2ID, ULONGLONG& qwTeam2Score );


    // Game session and lobby helpers
    BOOL IsOpenNAT();
    VOID InitiateJoin( GameInfo& gameInfo );
    VOID StartArbitratedGame();


    // Initialization functions
    HRESULT InitXHV();
    HRESULT DeInitXHV();
    HRESULT InitSound();
    HRESULT InitTextures();
    HRESULT InitXNet( BOOL bInitialOnly = FALSE );

    // Overloaded functions defined by the application
    // class to execute game logic and rendering
    virtual HRESULT Render();
    virtual HRESULT Initialize();
    virtual HRESULT FrameMove();

    ~CXBoxSample();
};

#endif // INTEGRATEDDEMO_H