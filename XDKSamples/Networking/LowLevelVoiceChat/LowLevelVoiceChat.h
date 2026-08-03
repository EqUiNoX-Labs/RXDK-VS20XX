//-----------------------------------------------------------------------------
// File: LowLevelVoiceChat.h
//
// Desc: Demonstrates how a title can use the low-level voice chat components
//       (Microphone, Encoder, Voice Queue, Decoder, Headphone) to build a
//       drop-in custom voice engine.
//
// Hist: 04.29.02 - New for June02 XDK release 
//       01.14.03 - Renamed from SimpleVoice sample to LowLevelVoiceChat
//       03.19.03 - Updated to support reliable communications
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
#include "player.h"
#include "VoiceManager.h"




//-----------------------------------------------------------------------------
// Name: class Message
// Desc: Message object sent between players and hosts
//-----------------------------------------------------------------------------
class Message
{
    WORD m_cbGameData;
    BYTE m_byMessageId;

    union
    {
        MsgFindGame     m_FindGame;
        MsgGameFound    m_GameFound;
        MsgJoinGame     m_JoinGame;
        MsgJoinApproved m_JoinApproved;
        MsgPlayerJoined m_PlayerJoined;
        MsgVoiceInfo    m_VoiceInfo;
        MsgVoiceData    m_VoiceData;
    };

public:

    explicit Message( BYTE byMessageId = 0 ) : m_byMessageId( byMessageId ) { m_cbGameData = (WORD)GetEncryptedSize(); }
    ~Message() {}

    BYTE GetId() const      { return m_byMessageId; }
    INT  GetMaxSize() const { return sizeof(*this); }

    MsgFindGame&     GetFindGame()     { return m_FindGame;     }
    MsgGameFound&    GetGameFound()    { return m_GameFound;    }
    MsgJoinGame&     GetJoinGame()     { return m_JoinGame;     }
    MsgJoinApproved& GetJoinApproved() { return m_JoinApproved; }
    MsgPlayerJoined& GetPlayerJoined() { return m_PlayerJoined; }
    MsgVoiceInfo&    GetMsgVoiceInfo() { return m_VoiceInfo;    }
    MsgVoiceData&    GetMsgVoiceData() { return m_VoiceData;    }

    INT GetEncryptedSize() const
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
            case MSG_VOICEINFO:     return sizeof(BYTE) + sizeof(MsgVoiceInfo);
            case MSG_VOICEDATA:     return sizeof(BYTE);
            default: assert( FALSE ); return 0;
        }
    }

    INT GetUnEncryptedSize() const
    {
        switch( m_byMessageId )
        {
            case MSG_FIND_GAME:     return 0;
            case MSG_GAME_FOUND:    return 0;
            case MSG_JOIN_GAME:     return 0;
            case MSG_JOIN_APPROVED: return 0;
            case MSG_JOIN_DENIED:   return 0;
            case MSG_PLAYER_JOINED: return 0;
            case MSG_WAVE:          return 0;
            case MSG_HEARTBEAT:     return 0;
            case MSG_VOICEINFO:     return 0;
            case MSG_VOICEDATA:     return sizeof(WORD) + m_VoiceData.wVoicePackets * sizeof(VoicePacket);
            default: assert( FALSE ); return 0;
        }
    }

    // Total message size = Encrypted + UnEncrypted + 1 WORD for cbGameData
    INT GetSize() const { return sizeof( WORD ) + GetEncryptedSize() + GetUnEncryptedSize(); }
    static INT GetHeaderSize() { return sizeof( WORD ) + sizeof( BYTE ); }
};




//-----------------------------------------------------------------------------
// Name: class PendingMessage
// Desc: Used for parsing Messages out of a TCP stream.  Since fragments
//          of a given message could come in individually, they must be
//          buffered until complete
//-----------------------------------------------------------------------------
class PendingMessage
{
public:
    PendingMessage() : m_nBytesReceived( 0 ) { }

    HRESULT Read( SOCKET sock );   // Read message from socket
    VOID Reset() { m_nBytesReceived = 0; }

    Message m_msg;              // Buffer space for message
    INT     m_nBytesReceived;   // # of bytes received in message
};



//-----------------------------------------------------------------------------
// Name: struct ClientSocket
// Desc: Keeps track of socket and associated data for reliable client
//          connections
//-----------------------------------------------------------------------------
struct ClientSocket
{
    SOCKET          sock;       // Socket
    SOCKADDR_IN     sa;         // Address of client
    BOOL            bAccepted;  // Sent JoinAccepted message
    FLOAT           fTimeout;   // Timer for timing out connections
    PendingMessage  msgPending; // Pending message
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
        STATE_REQUEST_CONNECT,  // Joining game - waiting for connection
        STATE_REQUEST_JOIN,     // Joining game - waiting for answer
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
        GAME_MENU_LOOPBACK   = 1,
        GAME_MENU_VOICETHROUGHSPEAKERS = 2,
        GAME_MENU_LEAVE_GAME = 3,
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
    typedef std::vector< ClientSocket > SocketList;

    mutable HANDLE      m_hLogFile;      // Log file
    CXBFont      m_Font;                 // Game font
    CXBFont      m_OnlineIconsFont;      // Online icons font
    CXBHelp      m_Help;                 // Help screen
    BOOL         m_bDrawDebugInfo;       // Render debug info?
    State        m_State;                // Game state
    State        m_LastState;            // Last state
    DWORD        m_CurrItem;             // Current menu item
    NameList     m_GameNames;            // List of potential game names
    GameList     m_Games;                // List of available games
    PlayerList   m_Players;              // List of current players (not incl self)
    WCHAR        m_strError[ MAX_ERROR_STR ];   // Error message
    WCHAR        m_strStatus[ MAX_STATUS_STR ]; // Status
    CXBStopWatch m_LinkStatusTimer;      // Wait to check link status
    CXBStopWatch m_GameSearchTimer;      // Wait for game search to complete
    CXBStopWatch m_GameJoinTimer;        // Wait for game join to complete
    CXBStopWatch m_HeartbeatTimer;       // Keep-alive timer
    CXBStopWatch m_VoiceTimer;           // Voice packet timer

    BOOL         m_bIsOnline;            // TRUE if link status good
    BOOL         m_bXnetStarted;         // TRUE if networking initialized
    BOOL         m_bIsHost;              // TRUE if we're hosting the game
    BOOL         m_bIsSessionRegistered; // TRUE if session key registered
    BOOL         m_bHaveLocalAddress;    // TRUE if local address acquired
    XNKID        m_xnHostKeyID;          // Host key ID
    XNKEY        m_xnHostKeyExchange;    // Host key exchange key
    XNADDR       m_xnTitleAddress;       // The XNet address of this machine/game
    IN_ADDR      m_inHostAddr;           // The "IP" address of the host

    // We use several types of sockets
    // 1) Broadcast socket for finding sessions
    // 2) Direct UDP socket for game and voice data
    // 3) Reliable TCP socket for infrequent critical messages
    // Note that the host uses m_ReliableSock to listen for incoming
    // connections from clients, and then maintains a list of TCP
    // sockets to each client
    CXBSocket       m_BroadSock;            // Broadcast socket for broadcast msgs
    CXBSocket       m_DirectSock;           // Direct socket for direct msgs
    CXBSocket       m_ReliableSock;         // Reliable socket (or listen socket for host)
    PendingMessage  m_msgPending;           // Pending message for client
    SocketList      m_ClientSockets;        // Reliable sockets for low-bandwidth msgs

    WCHAR        m_strGameName[ MAX_GAME_NAME ];     // Game name
    WCHAR        m_strPlayerName[ MAX_PLAYER_NAME ]; // This player name
    WCHAR        m_strHostName[ MAX_PLAYER_NAME ];   // Host player name
    Nonce        m_Nonce;                            // Client identifier
    DWORD        m_dwVoiceMask;          // Current voice mask preset index
    BOOL         m_bLoopback;            // Loopback toggle
    BOOL         m_bVoiceThroughSpeakers;   // Voice through speakers

    Message      m_msgVoiceData;         // Voice data packet for buffering voice

public:
    // Public so that CVoiceManager can get to them
    static VOID VoiceDataCallback( DWORD dwPort, DWORD dwSize, VOID* pvData, VOID* pContext );
    static VOID CommunicatorCallback( DWORD dwPort, VOICE_COMMUNICATOR_EVENT event, VOID* pContext );

    virtual HRESULT Initialize();
    virtual HRESULT FrameMove();
    virtual HRESULT Render();

    CXBoxSample();

private:

    VOID OnVoiceData( DWORD dwPort, DWORD dwSize, VOID* pvData );
    VOID OnCommunicatorEvent( DWORD dwPort, VOICE_COMMUNICATOR_EVENT event );

    Event GetEvent();

    VOID FrameMoveMenu( Event );
    VOID FrameMoveGame( Event );
    VOID FrameMoveHelp( Event );
    VOID FrameMoveSelectName( Event );
    VOID FrameMoveStartGame( Event );
    VOID FrameMoveGameSearch( Event );
    VOID FrameMoveSelectGame( Event );
    VOID FrameMoveRequestJoin( Event );
    VOID FrameMoveError( Event );

    VOID RenderMenu();
    VOID RenderGame();
    VOID RenderHelp();
    VOID RenderSelectName();
    VOID RenderStartGame();
    VOID RenderGameSearch();
    VOID RenderSelectGame();
    VOID RenderRequestJoin();
    VOID RenderError();
    VOID RenderHeader();

    VOID InitiateJoin( DWORD );
    VOID Wave();
    VOID StartVoice();
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
    VOID SendPlayerJoinedToAll( const Player& );
    VOID SendWaveToAll();
    VOID SendHeartbeatToAll();
    VOID SendVoiceInfo( VOICEINFO           action, 
                        WORD                wControllerPort, 
                        PlayerInfo*         pDestPlayer,
                        WORD                wDestinationPort );
    VOID SendVoiceDataToAll();
    INT  SendMessage( const Message* pMsg, BOOL bReliable, const SOCKADDR_IN* psaDest = NULL );


    // Receive messages
    BOOL ProcessBroadcastMessage();
    BOOL ProcessDirectMessage();
    BOOL ProcessReliableMessage();
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
    VOID ProcessVoiceInfo( const MsgVoiceInfo&, const SOCKADDR_IN& );
    VOID ProcessVoiceData( const MsgVoiceData&, const SOCKADDR_IN& );

    // Handle keep-alive
    HRESULT OnPlayerJoined( const WCHAR* strName, XNADDR xnAddr, in_addr* pinAddr );
    HRESULT OnPlayerDisconnect( PlayerInfo* pPlayer );
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

//-----------------------------------------------------------------------------
// Name: class MatchInAddr
// Desc: Predicate functor used to match on IN_ADDRs in player lists
//-----------------------------------------------------------------------------
struct MatchInAddr
{
    IN_ADDR ia;
    explicit MatchInAddr( const SOCKADDR_IN& sa ) : ia( sa.sin_addr ) { }
    explicit MatchInAddr( const in_addr& addr ) : ia( addr ) { }
    bool operator()( const PlayerInfo& playerInfo )
    {
        return playerInfo.inAddr.s_addr == ia.s_addr;
    }
    bool operator()( const ClientSocket& cs )
    {
        return cs.sa.sin_addr.s_addr == ia.s_addr;
    }
};



