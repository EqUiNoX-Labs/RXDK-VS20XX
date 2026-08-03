//-------------------------------------------------------------------------------------
// File: GameMsg.h
//
// Desc: Holds definitions used by the sample's networking
//       modules for sending messages.
//
// Hist: 12.09.04 - New for January release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#pragma once

#ifndef GAMEMSG_H
#define GAMEMSG_H

#include <xtl.h>
#include <xonline.h>
#include <vector>

#include "Player.h"

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

// Port 1000 gives 0 extra port overhead on the wire
// Ports 1001-1255 give 2 bytes overhead on the wire
// All other ports give 4 bytes overhead on the wire

const WORD  BROADCAST_PORT    = 1001;  // Could be any port
const WORD  DIRECT_PORT       = 1000;  // Any port other than BROADCAST_PORT
const WORD  RELIABLE_PORT     = 1002;  // Port for low-bandwidth reliable msgs

const DWORD NONCE_BYTES       = 8;      // Larger means less chance of random matches
const DWORD PLAYER_TIMEOUT    = 2000;   // 2 seconds
const FLOAT CHECK_LINK_STATUS = 0.5f;   // Check status twice/sec
const FLOAT PLAYER_HEARTBEAT  = 0.3f;   // ~3 times per second
const FLOAT GAME_JOIN_TIME    = 2.0f;   // 2 seconds

const DWORD MAX_REMOTE_USERS  = MAX_MATCHERS - 1;


// TCR Session Discovery Time for System Link Play

const FLOAT GAME_SEARCH_TIME = 2.0f;   // 2 seconds (may not exceed 3)


//-----------------------------------------------------------------------------
// Name: class PlayerInfo
// Desc: Player information used by players to store list of other players
//       in the game
//-----------------------------------------------------------------------------
struct PlayerInfo
{
    XNADDR  xnAddr;                                 // XNet address
    IN_ADDR inAddr;                                 // Xbox IP (not a "real" IP)
    XUID    xuid;                                   // Player's xuid
    CHAR    strGamertag[ XONLINE_GAMERTAG_SIZE ];   // player name
    DWORD   dwLastHeartbeat;                        // last heartbeat, in our clocks
};

// A list of players
typedef std::vector< PlayerInfo > PlayerList;

//-----------------------------------------------------------------------------
// Name: class GameInfo
// Desc: Game information used by clients to store available games
//-----------------------------------------------------------------------------
struct GameInfo
{
    XNKID  xnHostKeyID;                              // host key ID
    XNKEY  xnHostKey;                                // host key
    XNADDR xnHostAddr;                               // host XNet address
    BYTE   byNumPlayers;                             // number of players in game
    WCHAR  strGameName[ MAX_GAME_NAME ];             // name of the game
    CHAR   strHostGamertag[ XONLINE_GAMERTAG_SIZE ]; // name of the host player
};



//-----------------------------------------------------------------------------
// Message IDs
//
// A "host" is the player who started the game.
// A "client" is a potential player. A client is not currently playing a game.
// A "player" is anyone playing a game.
// Broadcast messages are sent via a UDP broadcast socket
// Direct    messages are sent via a UDP direct socket
// Reliable  messages are sent via a TCP socket (and relayed if needed)
//-----------------------------------------------------------------------------
enum
{                       // From     To      Type        Expected response
                        //-----------------------------------------------------
    MSG_REGISTERED,     // client   host    reliable    MSG_GAME_START
    MSG_JOIN_GAME,      // client   host    reliable    MSG_JOIN_APPROVED/DENIED
    MSG_JOIN_APPROVED,  // host     client  reliable    <none>
    MSG_JOIN_DENIED,    // host     client  reliable    <none>
    MSG_GAME_START,     // host     client  reliable    <none>
    MSG_GAME_OVER,      // host     client  reliable    <none>
    MSG_PLAYER_JOINED,  // host     player  reliable    <none>
    MSG_ARB_ID,         // host     client  reliable    MSG_REGISTERED
    MSG_XUIDS,          // host     client  reliable    <none>
    MSG_WAVE,           // player   player  direct      <none>
    MSG_HEARTBEAT,      // player   player  direct      <none>
    MSG_SCORE,          // player   player  direct      <none>
};

//-----------------------------------------------------------------------------
// Message payloads
//-----------------------------------------------------------------------------
// Pack to minimize network traffic
#pragma pack( push )
#pragma pack( 1 )


//-----------------------------------------------------------------------------
// Local Nonce struct used by some messages
//-----------------------------------------------------------------------------
struct Nonce
{
    // Used for client verification. The larger the number of NONCE_BYTES,
    // the less likely there is to be an accidental match between client & host
    BYTE byRandom[ NONCE_BYTES ];
};

//-----------------------------------------------------------------------------
// Broadcast by a client looking for available games
//-----------------------------------------------------------------------------
struct MsgFindGame
{
    Nonce nonce;    // Generated by client; used to verify host response
};

//-----------------------------------------------------------------------------
// Broadcast by a host in response to a MSG_FIND_GAME
//-----------------------------------------------------------------------------
struct MsgGameFound
{
    Nonce  nonce;                          // used for client verification
    XNKID  xnHostKeyID;                    // host key ID
    XNKEY  xnHostKey;                      // host key
    XNADDR xnHostAddr;                     // host XNet address
    BYTE   byNumPlayers;                   // number of players in game
    WCHAR  strGameName[ MAX_GAME_NAME ];   // game name
    CHAR   strHostGamertag[ XONLINE_GAMERTAG_SIZE ]; // game host player name
};

//-----------------------------------------------------------------------------
// Sent from a client to a host to join a game
//-----------------------------------------------------------------------------
struct MsgJoinGame
{
    // Number of player who are trying to join
    DWORD  dwNumPlayers;

    // player names who wants to join
    CHAR   strGamertags[ XGetPortCount() ][ XONLINE_GAMERTAG_SIZE ];

    // XUIDs of players joining
    XUID   xuids[ XGetPortCount() ];
};

//-----------------------------------------------------------------------------
// Sent from a host to a client in response to a MSG_JOIN_GAME
//-----------------------------------------------------------------------------
struct MsgJoinApproved
{
    // Number of players on the host machine
    DWORD  dwNumHostPlayers;

    // Names of players on the host
    CHAR   strHostGamertags[ XGetPortCount() ][ XONLINE_GAMERTAG_SIZE ];

    // XUIDs of the players on the host machine
    XUID   xuids[ XGetPortCount() ];

    // Players in the game (not incl host)
    BYTE   byNumPlayers;

    // List of players (not incl host)
    Player PlayerList[ MAX_MATCHERS ];
};

//-----------------------------------------------------------------------------
// Sent from a host to other players to notify them that a new player has joined
//-----------------------------------------------------------------------------
struct MsgPlayerJoined
{
    Player player; // The latest player to join the game
};

//-----------------------------------------------------------------------------
// Sent from a host to other players to notify them that a player has scored
//-----------------------------------------------------------------------------
struct MsgScore
{
    ULONGLONG qwID; // ID number of the player who scored
};

//-----------------------------------------------------------------------------
// Sent from a host to other clients to initiate arbitration registration
//-----------------------------------------------------------------------------
struct MsgArbID
{
    XONLINE_ARB_ID arbID; // ID number of the arbitration round
};

//-----------------------------------------------------------------------------
// Sent from a client to a host that the users have registered with arbitration
//-----------------------------------------------------------------------------
struct MsgRegistered
{
    WORD      wUserCount; // Number of users who registered
    ULONGLONG rwIDs[4];   // ID numbers of the registered users
};

//-----------------------------------------------------------------------------
// Sent from the host to clients. Contains the list of XUIDs of all the users
// who registered and are allowed to join the round.
//-----------------------------------------------------------------------------
struct MsgXUIDs
{
    WORD wXUIDCount;          // Number of users
    XUID xuids[MAX_MATCHERS]; // XUIDs of the users
};

#pragma pack( pop )


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
        MsgScore        m_Score;
        MsgArbID        m_ArbID;
        MsgRegistered   m_Registered;
        MsgXUIDs        m_XUIDs;
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
    MsgScore&        GetScore()        { return m_Score; }
    MsgArbID&        GetArbID()        { return m_ArbID; }
    MsgRegistered&   GetRegistered()   { return m_Registered; }
    MsgXUIDs&        GetXUIDs()        { return m_XUIDs; }

    INT GetEncryptedSize() const
    {
        switch( m_byMessageId )
        {
            case MSG_JOIN_GAME:     return sizeof(BYTE) + sizeof(MsgJoinGame);
            case MSG_JOIN_APPROVED: return sizeof(BYTE) + sizeof(MsgJoinApproved);
            case MSG_JOIN_DENIED:   return sizeof(BYTE);
            case MSG_GAME_START:    return sizeof(BYTE);
            case MSG_GAME_OVER:     return sizeof(BYTE);
            case MSG_SCORE:         return sizeof(BYTE) + sizeof(MsgScore);
            case MSG_PLAYER_JOINED: return sizeof(BYTE) + sizeof(MsgPlayerJoined);
            case MSG_ARB_ID:        return sizeof(BYTE) + sizeof(MsgArbID);
            case MSG_XUIDS:         return sizeof(BYTE) + sizeof(MsgXUIDs);
            case MSG_REGISTERED:    return sizeof(BYTE) + sizeof(MsgRegistered);
            case MSG_WAVE:          return sizeof(BYTE);
            case MSG_HEARTBEAT:     return sizeof(BYTE);
            default: assert( FALSE ); return 0;
        }
    }

    INT GetUnEncryptedSize() const
    {
        switch( m_byMessageId )
        {
            case MSG_JOIN_GAME:     return 0;
            case MSG_JOIN_APPROVED: return 0;
            case MSG_JOIN_DENIED:   return 0;
            case MSG_GAME_START:    return 0;
            case MSG_GAME_OVER:     return 0;
            case MSG_SCORE:         return 0;
            case MSG_PLAYER_JOINED: return 0;
            case MSG_ARB_ID:        return 0;
            case MSG_XUIDS:         return 0;
            case MSG_REGISTERED:    return 0;
            case MSG_WAVE:          return 0;
            case MSG_HEARTBEAT:     return 0;
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
//       of a given message could come in individually, they must be
//       buffered until complete
//-----------------------------------------------------------------------------
class PendingMessage
{
public:
    PendingMessage() : m_nBytesReceived( 0 ) { }

    HRESULT Read( SOCKET sock ); // Read message from socket
    VOID Reset() { m_nBytesReceived = 0; }

    Message m_msg;               // Buffer space for message
    INT     m_nBytesReceived;    // # of bytes received in message
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

typedef std::vector< std::wstring > NameList;
typedef std::vector< GameInfo >     GameList;
typedef std::vector< PlayerInfo >   PlayerList;
typedef std::vector< ClientSocket > SocketList;

#endif // GAMEMSG_H