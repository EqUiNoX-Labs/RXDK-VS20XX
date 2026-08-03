//-------------------------------------------------------------------------------------
// File: EloDemo.h
//
// Desc: This sample demonstrates how to use a ratings statistic
// known as "Elo Rankings". To implement Elo Rankings
// the Arbitration and Match-making services are used.
//
// Elo Rankings are a comparitive way to adjust the ranking of
// two players based on their current ranking and the expected
// outcome of the match.
//
// If a player with a higher ranking is defeated by a player
// with a lower ranking, the the defeated player moves down
// the rankings while the victor gains ranking.
//
// Hist: 08.10.04 - New for Sept release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#pragma once

#ifndef ELODEMO_H
#define ELODEMO_H

#include <vector>
#include "XBApp.h"
#include "XBNet.h"
#include "XBOnline.h"
#include "XBOnlineTask.h"
#include "XBRandName.h"
#include "match.h"
#include "GameMsg.H"

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


// Used to detect player drop-out
const FLOAT PLAYER_HEARTBEAT       = 1.0f; // Send a connection keep-alive every second
const DWORD PLAYER_TIMEOUT         = 3000; // If no keep-alive is received in three
                                           // second then the player has dropped out

// Used for creating and finding matches
const INT TYPE_SHORT               = 0;
const INT LEVEL_BEGINNER           = 0;
const INT STYLE_HEAVY              = 0;
const INT MAX_USERS                = 4;
const INT MAX_MATCHERS             = MAX_USERS;
const INT MAX_PLAYERS_PER_GAME     = 2;

// General UI constants
const FLOAT SCREEN_CENTER_X        = 320.0f;
const FLOAT POS_SCREEN_TITLE_Y     = 80.0f;
const FLOAT POS_VERSUS_Y           = 200.0f;
const FLOAT WIDTH_VERSUS_X         = 30.0f;
const FLOAT POS_GAME_SCORE_PADDING = 30.0f;
const FLOAT POS_MESSAGE_Y          = 240.0f;
const FLOAT POS_GAME_SETUP_Y       = 200.0f;
const FLOAT POS_MENU_START_Y       = 300.0f;
const FLOAT POS_HEADER_Y           = 40.0f;
const FLOAT POS_HEADER_LEFT        = 40.0f;
const FLOAT POS_FOOTER_Y           = 420.0f;
const FLOAT POS_FOOTER_LEFT        = 40.0f;
const FLOAT POS_FOOTER_RIGHT       = 600.0f;
const FLOAT DEFAULT_TEXT_PADDING   = 30.0f;

const FLOAT POS_ACCOUNT_LIST_START = POS_SCREEN_TITLE_Y + 
                                     ( DEFAULT_TEXT_PADDING * 2.0f );


// constants relating to the stats we're requesting, and how many we want to see
const DWORD NUM_RANKINGS_TO_DISPLAY                 = 10;
const DWORD NUM_REQUESTED_STATS                     = 2;
const DWORD NUM_REQUESTED_ATTACHMENT_PATHS          = 0;

const WORD  REQUESTED_STAT_IDS[NUM_REQUESTED_STATS] =
{
    XONLINE_STAT_RANK,
    XONLINE_STAT_RATING
};

const DWORD STAT_EXTRAS_BUFFER_SIZE         = ( NUM_RANKINGS_TO_DISPLAY * 
                                                XONLINE_STAT_MAX_NICKNAME_LENGTH * 
                                                sizeof(WCHAR) ) + 
                                              ( NUM_REQUESTED_ATTACHMENT_PATHS * 
                                                XONLINESTORAGE_MAX_PATH * 
                                                sizeof(WCHAR) );

// Error returns for signing in
enum
{
    E_NETWORK_ERROR = 1,
    E_ACCOUNT_ERROR,

    NUM_ELO_ERRORS
};

// Constants for network messages
enum BlobType
{
    BLOB_ARB_ID,        // When this blob arrives each user should register with
                        // arbitration, and notify the host when they are done.
                        // The data in this case is an 
    BLOB_REGISTERED,    // This blob indicates that a player has registered with 
                        // arbitration
    BLOB_XUIDS,         // This blob contains an array of all XUIDs, sent by the host
    BLOB_GAME_START,    // This blob indicates that the host is starting the game
    BLOB_I_SCORE,       // This blob contains the gamer tag of a player who has just 
                        // scored
    BLOB_GAME_OVER,     // This blob is sent to finish the game
    BLOB_CLIENT_LEFT,   // The sending client has left the game lobby
    BLOB_COUNT          // Count of how many blob IDs there are
};

// Leaderboards used for stat retrieval
// (only using one leaderboard for this purpose)
enum
{
    DEFAULT_LEADERBOARD_ID  = 1,

    END_LEADERBOARDS
};

// View ratings errors
enum
{
    VIEW_RATING_NO_ERR = 0,
    VIEW_RATING_ERR_ENUMERATE,
    VIEW_RATING_ERR_RESULTS,
    VIEW_RATING_ERR_CLOSING_TASK
};

// Write Stats errors
enum
{
    STAT_WRITE_NO_ERR = 0,
    STAT_WRITE_ERR_FAILED,
    STAT_WRITE_ERR_CLOSING_TASK
};

// Task closing constants determining if task closing errors are set
enum
{
    TASK_CLOSE_NO_SET_ERRORS,
    TASK_CLOSE_SET_ERRORS
};

// Now, add whatever services are appropriate for your title, but no
// more. Each service requires additional authentication time
// and network traffic.  For demonstration purposes, the
// matchmaking service is specified.  Additional services ids are
// specified in xonline.h.    
const DWORD SERVICES[]      = { XONLINE_MATCHMAKING_SERVICE,
                                XONLINE_ARBITRATION_SERVICE,
                                XONLINE_STATISTICS_SERVICE };

const DWORD NUM_SERVICES    = sizeof( SERVICES ) / sizeof( SERVICES[0] );


////////////////
// Game Menus //
////////////////

// GAME SETUP menu
const WCHAR* const MENU_GAME_SETUP[]   =
{
    L"QuickMatch",
    L"Create Match",
    L"View Ratings"
};
const WORD NUM_ITEMS_GAME_SETUP_MENU   = 3;

// GAME LOBBY menu
const WCHAR* const MENU_GAME_LOBBY[]   =
{
    L"Leave Game",
    L"Start Game"
};
const WORD NUM_ITEMS_GAME_LOBBY        = 2;

// GAME SESSION menu
const WCHAR* const MENU_GAME_SESSION[] =
{
    L"Score a point!",
    L"Score a cheated point!",
    L"Leave Game"
};
const WORD NUM_ITEMS_GAME_SESSION      = 3;


// Used to store all the results of a match-making
// search. A title should do some sort of selection
// to provide for load-balancing and optimum game
// experience. This title will simply pick the
// first result
typedef std::vector< SessionInfo >  SessionList;

VOID BootToDash( DWORD dwReason );

//-------------------------------------------------------------------------------------
// Classes
//-------------------------------------------------------------------------------------

// Sample Application Class
// Demonstrates ELO ratings
//
// Remember: Elo is a statistic, not a reward!
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
        STATE_VIEW_RATINGS,         // Allows the player to view their ratings relative 
                                    // to other players
        STATE_QUICKMATCH,           // Attempts to find an existing match via the 
                                    // MatchMaking service
        STATE_CREATE_MATCH,         // Creates a match that other Live users can join
        STATE_GAME_LOBBY,           // Gathering place for all players about to start 
                                    // a match
        STATE_GAME_SESSION,         // The actual gameplay/match
        STATE_GAME_RESULTS,         // View the results of the match and submit 
                                    // statistics
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

protected:

    // *** UI specific variables ***
    // Font object used to render the UI's text
    CXBFont                 m_font;         
    // The current color of the UI's background
    D3DCOLOR                m_bgColor;
    // The current state of the UI
    EUIStates               m_state;  
    // Index of the item currently selected in the UI menu
    INT                     m_iItemSelected;        
                                                   
    // *** General Xbox Live variables ***
    // Is the player currently logging into the service
    BOOL                    m_bIsSigningIn;                   
    // Is the user signed onto Xbox Live stores result of attempt to sign in user
    BOOL                    m_bSignedIn;       
    INT                     m_iSignInResult;
    // Task to logon the user and to send Keep-Alives to Xbox Live
    XONLINETASK_HANDLE      m_hLogonTask;                                                         

    // *** User data ***
    // Index of the current user logged into Live Points to an index in the array found 
    // by XOnlineGetUsers
    WORD                    m_wCurUserIndex;                         
    // User data for all Xbox Live accounts held on the Hard Drive and memory units
    XONLINE_USER            m_rwStoredUsers[ XONLINE_MAX_STORED_ONLINE_USERS ];          
    // The number of users stored in m_rwStoredUsers
    DWORD                   m_dwNumStoredUsers;                                                     

    // *** Match-Making/Hosting Variables
    // Is the user the host of the current game session 
    BOOL                    m_bIsHost;                             
    // Has the user joined the game 
    BOOL                    m_bJoinedGame;          
    // Is the user waiting to join the game
    BOOL                    m_bWaitingToJoin;              
    // Used to time out the QuickMatch search
    FLOAT                   m_fRequestTime;         
    // Object used to query the Match-Making service to find existing matches for the 
    // game on the Xbox Live service
    COptiMatchQuery         m_matchQuery;                                                                        
    // list of session information
    SessionList             m_rwSessionList;   
    // Index of the local host into m_dwScores
    WORD                    m_wLocalPlayerIndex; 
 
    // *** Game session variables ***
    // Object used to send network messages to all the players in the current match
    GameMsg                 m_networkMessageHandler;              
    // Timer used to keep track of when the next heartbeat/keep alive message needs to 
    // be sent
    CXBStopWatch            m_heartbeatTimer;                                   
    // The scores of the players in the current session
    DWORD                   m_dwScores[ MAX_MATCHERS ]; 
                                                         
    // *** Leader Enumeration/Ratings variables ***
    // array of user info used for viewing ratings
    XONLINE_STAT_USER       m_aStatUsers[ NUM_RANKINGS_TO_DISPLAY ];           
    // table of stats that exhibit all requested stats from all players for all 
    // their stats
    XONLINE_STAT            m_aStats[ NUM_RANKINGS_TO_DISPLAY * NUM_REQUESTED_STATS ];        
    // required array which contains additional information for an enumeration report
    BYTE                    m_aStatsExtra[ STAT_EXTRAS_BUFFER_SIZE ];                                                       
    // view rating error 
    INT                     m_iViewRatingError; 
    // related Xbox related error code
    DWORD                   m_dwViewRatingErrorCode;   
    // number of users actually returned when requesting a leader ranking report
    DWORD                   m_dwNumUsersEnumerated;    
                       
    // *** Report writing variables ***
    // user structure to contain ELO structure below
    XONLINE_STAT_PROC       m_statWriteProc;     
    // user structure to contain ELO information
    XONLINE_STAT_ELO        m_statElo;
    // report writing error 
    INT                     m_iStatWriteError; 
    // related Xbox related error code
    DWORD                   m_dwStatWriteErrorCode;
    // boolean, stating that the match results submitted were different
    BOOL                    m_bResultsDiffer;

    // *** Report writing variables ***
    // index of player (0 [outsider] or 1 [local]) that is the winner of a session
    INT                     m_iWinnerIndex;        
    // list of saved players' names
    WCHAR                   m_rwSavedPlayers[ MAX_MATCHERS ][ XONLINE_GAMERTAG_SIZE ];
                                                           
    // *** Creating an ELO match ***
    // List of players in the game session
    CXBNetPlayerList        m_rwPlayers;  
    // session information
    SessionInfo             m_sessionInfo;            
    // Created session information
    CSession                m_hostedSession;          
    // ID for the current (joined) session
    XNKID                   m_xnJoinedSessionID;      
    // Key Exchange Key for the current (joined) session
    XNKEY                   m_xnJoinedKeyExchangeKey; 
    // "IP" address of host
    IN_ADDR                 m_inHostAddr;             
    // The number of slots in use for the hosted session
    DWORD                   m_dwSlotsInUse;           

    // *** Arbitration variables ***
    // Unique arbitration ID for the current match
    XONLINE_ARB_ID          m_arbID;              
    // The time that arbitration registration was requested
    FLOAT                   m_fRegistrationStart;                     
    // Number of players that have registered with arbitration
    DWORD                   m_dwPlayersRegistered; 
    // XUIDs of all of the players.
    XUID                    m_rwPlayerXUIDs[ MAX_MATCHERS ];     
    // Has arbitration started?
    BOOL                    m_bArbitrationStarted;                


    // Network session helper functions
    INT                     StartSignIn( WORD wUserIndex );
    INT                     ContinueSignIn();
    INT                     FinishSignIn();
    VOID                    CopyXUIDs( const XUID* pXUIDs );
    BOOL                    SendJoinRequest();
    VOID                    RemovePlayer();
    VOID                    UpdateSession();
    VOID                    DeleteSession();
    VOID                    LeaveGame();
    VOID                    BackupPlayerNames();
    
    // Arbitration registration helper functions
    BOOL                    RegisterForArbitration();
    VOID                    StartArbitratedGame();
    BOOL                    StartArbitratedGameRegistration();

    // Rating task helper functions

    // will set View Ratings errors if set to write errors
    VOID                    CloseViewRatingsTask( XONLINETASK_HANDLE , 
                               INT iSetErrors = TASK_CLOSE_NO_SET_ERRORS ); 
    // will set Stat Write errors if set to write errors 
    VOID                    CloseGameSessionTask( XONLINETASK_HANDLE , 
                               INT iSetErrors = TASK_CLOSE_NO_SET_ERRORS );
 

    // State handling functions
    // Functions that handle the
    // entrance into, updating of,
    // rendering of, and exiting
    // of the UI/Game states

    Event                   GetEvent() const;
    VOID                    SetState( EUIStates newState );

    // State SelectAccount
    VOID                    EnterStateSelectAccount();
    VOID                    UpdateStateSelectAccount( Event );
    VOID                    RenderStateSelectAccount();
    VOID                    ExitStateSelectAccount();

    // State LoginPassword
    VOID                    EnterStateLogin();
    VOID                    UpdateStateLogin( Event );
    VOID                    RenderStateLogin();
    VOID                    ExitStateLogin() {}

    // State LoginFailed
    VOID                    EnterStateLoginFailed() {}
    VOID                    UpdateStateLoginFailed( Event );
    VOID                    RenderStateLoginFailed();
    VOID                    ExitStateLoginFailed() {}

    // State NetworkError
    VOID                    EnterStateNetworkError() {}
    VOID                    UpdateStateNetworkError( Event );
    VOID                    RenderStateNetworkError();
    VOID                    ExitStateNetworkError();

    // State GameSetup
    VOID                    EnterStateGameSetup();
    VOID                    UpdateStateGameSetup( Event );
    VOID                    RenderStateGameSetup();
    VOID                    ExitStateGameSetup() {}

    // State ViewRatings
    VOID                    EnterStateViewRatings();
    VOID                    UpdateStateViewRatings( Event );
    VOID                    RenderStateViewRatings();
    VOID                    ExitStateViewRatings();

    // State QuickMatch
    VOID                    EnterStateQuickMatch();
    VOID                    UpdateStateQuickMatch( Event );
    VOID                    RenderStateQuickMatch();
    VOID                    ExitStateQuickMatch();

    // State CreateMatch
    VOID                    EnterStateCreateMatch();
    VOID                    UpdateStateCreateMatch( Event );
    VOID                    RenderStateCreateMatch();
    VOID                    ExitStateCreateMatch() {}

    // State GameLobby
    VOID                    EnterStateGameLobby();
    VOID                    UpdateStateGameLobby( Event );
    VOID                    RenderStateGameLobby();
    VOID                    ExitStateGameLobby();

    // State GameSession
    VOID                    EnterStateGameSession();
    VOID                    UpdateStateGameSession( Event );
    VOID                    RenderStateGameSession();
    VOID                    ExitStateGameSession();

    // State GameResults
    VOID                    EnterStateGameResults();
    VOID                    UpdateStateGameResults( Event );
    VOID                    RenderStateGameResults();
    VOID                    ExitStateGameResults() {}

    // Extra rendering functions
    VOID                    RenderHeader();
    VOID                    RenderFooter( WORD flags );

public:

    // Functions to handle network messages
    VOID                    OnJoinGame( const CXBNetPlayerInfo& playerInfo );
    VOID                    OnJoinApproved( const CXBNetPlayerInfo& hostInfo );
    VOID                    OnJoinApprovedAddPlayer( const CXBNetPlayerInfo& 
                                                     playerInfo );
    VOID                    OnJoinDenied();
    VOID                    OnPlayerJoined( const CXBNetPlayerInfo& playerInfo );
    VOID                    OnWave( const CXBNetPlayerInfo& playerInfo );
    VOID                    OnBlob( const CXBNetPlayerInfo& playerInfo, 
                                    const CXBNetBlob& blob );
    VOID                    OnHeartbeat( const CXBNetPlayerInfo& playerInfo );
    VOID                    OnPlayerDropout( const CXBNetPlayerInfo& playerInfo, 
                                             BOOL bIsHost );

    // Overloaded functions defined by the application
    // class to execute game logic and rendering
    virtual HRESULT         Render();
    virtual HRESULT         Initialize();
    virtual HRESULT         FrameMove();
};

#endif // ELODEMO_H