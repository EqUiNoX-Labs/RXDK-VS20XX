//-----------------------------------------------------------------------------
// File: HostMigration.h
//
// Hist: 09.11.02 - New for Oct02 XDK release 
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#pragma warning( disable: 4786 )
#include <XBApp.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <XBStopWatch.h>
#include <XBSocket.h>
#include <cassert>
#include <vector>
#include <algorithm>
#include "gamemessage.h"


//-----------------------------------------------------------------------------
// Name: class Message
// Desc: Message object sent between players and hosts
//-----------------------------------------------------------------------------
class Message
{
    BYTE m_byMessageId;

    union
    {
        MsgFindGame     m_FindGame;
        MsgGameFound    m_GameFound;
        MsgJoinGame     m_JoinGame;
        MsgJoinApproved m_JoinApproved;
        MsgPlayerJoined m_PlayerJoined;
        
        // Host Migration messages
        MsgConnectToMigratedHost m_ConnectToMigratedHost;
        MsgMigrateHostApproved   m_MigrateHostApproved;
        MsgPlayerRejoined        m_PlayerRejoined; 
    };

public:

    explicit Message( BYTE byMessageId = 0 ) : m_byMessageId( byMessageId ) { }
    ~Message() {}

    BYTE GetId() const      { return m_byMessageId; }
    INT  GetMaxSize() const { return sizeof(*this); }

    MsgFindGame&     GetFindGame()     { return m_FindGame;     }
    MsgGameFound&    GetGameFound()    { return m_GameFound;    }
    MsgJoinGame&     GetJoinGame()     { return m_JoinGame;     }
    MsgJoinApproved& GetJoinApproved() { return m_JoinApproved; }
    MsgPlayerJoined& GetPlayerJoined() { return m_PlayerJoined; }
    
    MsgConnectToMigratedHost& GetConnectToMigratedHost() { return m_ConnectToMigratedHost; } 
    MsgMigrateHostApproved&   GetMigrateHostApproved()   { return m_MigrateHostApproved; }
    MsgPlayerRejoined&        GetPlayerRejoined()        { return m_PlayerRejoined; }

    INT GetSize() const
    { 
        switch( m_byMessageId )
        {
            case MSG_FIND_GAME:     return sizeof(BYTE) + sizeof(MsgFindGame);
            case MSG_GAME_FOUND:    return sizeof(BYTE) + sizeof(MsgGameFound);
            case MSG_JOIN_GAME:     return sizeof(BYTE) + sizeof(MsgJoinGame);
            case MSG_JOIN_APPROVED: return sizeof(BYTE) + sizeof(MsgJoinApproved);
            case MSG_JOIN_DENIED:   return sizeof(BYTE);
            case MSG_PLAYER_JOINED: return sizeof(BYTE) + sizeof(MsgPlayerJoined);
            case MSG_WAVE:          return sizeof(BYTE);
            case MSG_HEARTBEAT:     return sizeof(BYTE);          
            case MSG_CONNECT_TO_MIGRATED_HOST:  return sizeof(BYTE) + sizeof(MsgConnectToMigratedHost);
            case MSG_MIGRATE_HOST_APPROVED:     return sizeof(BYTE) + sizeof(MsgMigrateHostApproved);            
            case MSG_PLAYER_REJOINED:           return sizeof(BYTE) + sizeof(MsgPlayerRejoined);
            default: assert( FALSE ); return 0;
        }
    }

private:

    // Disabled
    Message( const Message& );
    Message& operator=( const Message& );

};




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    enum State
    {
        STATE_MENU,             // Main menu
        STATE_GAME,             // Game menu
        STATE_HELP,             // Help screen
        STATE_SELECT_NAME,      // Select game name screen
        STATE_START_NEW_GAME,   // Starting new game        
        STATE_GAME_SEARCH,      // Searching for game
        STATE_SELECT_GAME,      // Game selection menu
        STATE_REQUEST_JOIN,     // Joining game
        
        // Host migration states
        STATE_FIND_NEW_HOST,    // Finding a new host
        STATE_RESUME_GAME,      // Resume game as host

        STATE_ERROR             // Error screen               
    };

    enum Event
    {
        EV_BUTTON_A,
        EV_BUTTON_B,
        EV_BUTTON_X,
        EV_BUTTON_Y,
        EV_BUTTON_BLACK,
        EV_BUTTON_WHITE,
        EV_TRIGGER_LEFT,
        EV_TRIGGER_RIGHT,
        EV_UP,
        EV_DOWN,
        EV_LEFT,
        EV_RIGHT,
        EV_DISCONNECT,
        EV_NULL
    };

    enum
    {
        // Main menu
        MAIN_MENU_START_GAME = 0,
        MAIN_MENU_JOIN_GAME  = 1,
        MAIN_MENU_MAX,

        // Game menu
        GAME_MENU_WAVE       = 0,
        GAME_MENU_LEAVE_GAME = 1,
        GAME_MENU_MAX
    };

    enum InitStatus
    {
        Success,
        NotConnected,
        InitFailed
    };

    typedef std::vector< std::wstring > NameList;
    typedef std::vector< GameInfo >     GameList;
    typedef std::vector< PlayerInfo >   PlayerList;

    mutable HANDLE      m_hLogFile;      // Log file
    CXBFont      m_Font;                 // game font
    CXBHelp      m_Help;                 // help screen    
    State        m_State;                // game state
    State        m_LastState;            // last state
    DWORD        m_CurrItem;             // current menu item
    NameList     m_GameNames;            // list of potential game names
    GameList     m_Games;                // list of available games
    PlayerList   m_Players;              // list of current players (including self)
    WCHAR        m_strError[ MAX_ERROR_STR ];   // error message
    WCHAR        m_strStatus[ MAX_STATUS_STR ]; // status
    CXBStopWatch m_LinkStatusTimer;      // wait to check link status
    CXBStopWatch m_GameSearchTimer;      // wait for game search to complete
    CXBStopWatch m_GameJoinTimer;        // wait for game join to complete
    CXBStopWatch m_HeartbeatTimer;       // keep-alive timer
    BOOL         m_bIsOnline;            // TRUE if link status good
    BOOL         m_bXnetStarted;         // TRUE if networking initialized    
    BOOL         m_bIsSessionRegistered; // TRUE if session key registered
    BOOL         m_bHaveLocalAddress;    // TRUE if local address acquired
    XNKID        m_xnHostKeyID;          // Host key ID
    XNKEY        m_xnHostKeyExchange;    // Host key exchange key
    XNADDR       m_xnTitleAddress;       // The XNet address of this machine/game
    IN_ADDR      m_inHostAddr;           // The "IP" address of the host
    CXBSocket    m_BroadSock;            // Broadcast socket for broadcast msgs
    CXBSocket    m_DirectSock;           // Direct socket for direct msgs
    WCHAR        m_strGameName[ MAX_GAME_NAME ];     // Game name
    WCHAR        m_strPlayerName[ MAX_PLAYER_NAME ]; // This player name
    WCHAR        m_strHostName[ MAX_PLAYER_NAME ];   // Host player name
    Nonce        m_Nonce;                            // Client identifier

    BOOL         m_bIsHost;               // TRUE if we're hosting the game
    CXBStopWatch m_HostTimeoutTimer;      // total time we'll wait on any new host
    CXBStopWatch m_HostRequestTimer;      // time we wait between requests    
    IN_ADDR      m_inHostTryAddr;         // the "ip" addr of the host                                            

    DWORD        m_dwNextID;              // the next ID we will give to someone who joins
    DWORD        m_dwMyID;                // my ID

public:  
    virtual HRESULT Initialize();
    virtual HRESULT FrameMove();
    virtual HRESULT Render();

    CXBoxSample();

private: 
    Event GetEvent();

    VOID FrameMoveMenu( Event );
    VOID FrameMoveGame( Event );
    VOID FrameMoveHelp( Event );
    VOID FrameMoveSelectName( Event );
    VOID FrameMoveStartGame( Event );
    VOID FrameMoveGameSearch( Event );
    VOID FrameMoveSelectGame( Event );
    VOID FrameMoveRequestJoin( Event );
    VOID FrameMoveFindNewHost( Event );
    VOID FrameMoveResumeGame( Event );
    VOID FrameMoveError( Event );
  
    
    VOID RenderMenu();
    VOID RenderGame();
    VOID RenderHelp();
    VOID RenderSelectName();
    VOID RenderStartGame();
    VOID RenderGameSearch();
    VOID RenderSelectGame();
    VOID RenderRequestJoin();
    VOID RenderFindNewHost();
    VOID RenderResumeGame();   
    VOID RenderError();
    VOID RenderHeader();    

    VOID InitiateJoin( DWORD );
    VOID Wave();    
    VOID Heartbeat();

    VOID Init();

    // Initialization
    InitStatus InitXNet();

    // Send messages
    VOID SendFindGame();
    VOID SendGameFound( const Nonce& );
    VOID SendJoinGame( const SOCKADDR_IN& );
    VOID SendJoinApproved( const SOCKADDR_IN& );
    VOID SendJoinDenied( const SOCKADDR_IN& );
    VOID SendPlayerJoined( const Player&, const SOCKADDR_IN& );
    VOID SendWave( const SOCKADDR_IN& );
    VOID SendHeartbeat( const SOCKADDR_IN& );
   
    VOID SendConnectToMigratedHost( const SOCKADDR_IN& );
    VOID SendMigrateHostApproved( const SOCKADDR_IN& );    
    VOID SendPlayerRejoined( const Player& player, const SOCKADDR_IN& );

    // Receive messages
    BOOL ProcessBroadcastMessage();
    BOOL ProcessDirectMessage();
    VOID ProcessMessage( Message& );
    VOID ProcessMessage( Message&, const SOCKADDR_IN& );

    // Process incoming messages
    VOID ProcessFindGame( const MsgFindGame& );
    VOID ProcessGameFound( const MsgGameFound& );
    VOID ProcessJoinGame( const MsgJoinGame&, const SOCKADDR_IN& );
    VOID ProcessJoinApproved( const MsgJoinApproved&, const SOCKADDR_IN& );
    VOID ProcessJoinDenied( const SOCKADDR_IN& );
    VOID ProcessPlayerJoined( const MsgPlayerJoined&, const SOCKADDR_IN& );
    VOID ProcessWave( const SOCKADDR_IN& );
    VOID ProcessHeartbeat( const SOCKADDR_IN& );

    // Process host migration messages
    VOID ProcessConnectToMigratedHost( const MsgConnectToMigratedHost &, const SOCKADDR_IN& );
    VOID ProcessMigrateHostApproved( const MsgMigrateHostApproved&, const SOCKADDR_IN& );    
    VOID ProcessPlayerRejoined( const MsgPlayerRejoined&, const SOCKADDR_IN & );

    // Handle keep-alive
    BOOL ProcessPlayerDropouts();

    // Utility
    VOID DestroyGameList();
    VOID DestroyPlayerList();
    static VOID GenRandom( WCHAR*, DWORD );
    static WCHAR GetRandVowel();
    static WCHAR GetRandConsonant();
    static VOID AppendConsonant( WCHAR*, BOOL );
    static VOID AppendVowel( WCHAR* );

    VOID LogXNetError( const CHAR*, INT ) const;

};
