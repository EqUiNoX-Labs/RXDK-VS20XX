#ifndef _GAMEMESSAGE_H_
#define _GAMEMESSAGE_H_

#include <xonline.h>

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------
const DWORD COLOR_HIGHLIGHT   = 0xffffff00; // Yellow
const DWORD COLOR_GREEN       = 0xff00ff00;
const DWORD COLOR_NORMAL      = 0xffffffff;
const DWORD MAX_ERROR_STR     = 64;
const DWORD MAX_STATUS_STR    = 128;
const DWORD MAX_GAME_NAMES    = 6;      // Number of game names to choose from
const DWORD NONCE_BYTES       = 8;      // Larger means less chance of random matches
const DWORD MAX_GAME_NAME     = 12;     // Includes null
const FLOAT CHECK_LINK_STATUS = 0.5f;   // Check status twice/sec
const FLOAT PLAYER_HEARTBEAT  = 0.3f;   // ~3 times per second
const FLOAT GAME_JOIN_TIME    = 2.0f;   // 2 seconds
const FLOAT PLAYER_TIMEOUT    = 2000;   // 2 seconds

const FLOAT NEW_HOST_RESPONSE_TIME = 2.0f;       // 2.0 seconds total waiting time
const FLOAT NEW_HOST_TIME_BETWEEN_TRIES = 0.3f;  // 0.3 seconds between retries

const DWORD MAX_PLAYERS = 8;            // Max players

// TCR Session Discovery Time for System Link Play
const FLOAT GAME_SEARCH_TIME = 2.0f;   // 2 seconds (may not exceed 3)

const DWORD MAX_PLAYER_NAME   = 12;     // Includes null

//-----------------------------------------------------------------------------
// Name: class GameData
// Desc: represents whatever data you want to be persistant between host migrations
//       this could be player location, stats, etc
//-----------------------------------------------------------------------------

struct GameData
{
    DWORD dwID;
};

//-----------------------------------------------------------------------------
// Name: class PlayerInfo
// Desc: Player information used by players to store list of other players
//       in the game
//-----------------------------------------------------------------------------
struct PlayerInfo
{   
    XNADDR  xnAddr;                           // XNet address
    IN_ADDR inAddr;                           // Xbox IP (not a "real" IP)
    WCHAR   strPlayerName[ MAX_PLAYER_NAME ]; // player name
    DWORD   dwLastHeartbeat;                  // last heartbeat, in our clocks
    
    GameData gameData;    
};

//-----------------------------------------------------------------------------
// Local Player struct used by some messages
//-----------------------------------------------------------------------------
struct Player
{
    XNADDR xnAddr;                           // player's XNet address
    WCHAR  strPlayerName[ MAX_PLAYER_NAME ]; // player's name
    
    GameData gameData;
};

typedef std::vector< PlayerInfo >   PlayerList;

//-----------------------------------------------------------------------------
// Name: class GameInfo
// Desc: Game information used by clients to store available games
//-----------------------------------------------------------------------------
struct GameInfo
{
    XNKID  xnHostKeyID;                    // host key ID
    XNKEY  xnHostKey;                      // host key
    XNADDR xnHostAddr;                     // host XNet address
    BYTE   byNumPlayers;                   // number of players in game
    WCHAR  strGameName[ MAX_GAME_NAME ];   // name of the game
    WCHAR  strHostName[ MAX_PLAYER_NAME ]; // name of the host player
};


//-----------------------------------------------------------------------------
// Name: class MatchInAddr
// Desc: Predicate functor used to match on IN_ADDRs in player lists
//-----------------------------------------------------------------------------
struct MatchInAddr
{
    IN_ADDR ia;
    explicit MatchInAddr( const SOCKADDR_IN& sa ) : ia( sa.sin_addr ) { }
    bool operator()( const PlayerInfo& playerInfo )
    {
        return playerInfo.inAddr.s_addr == ia.s_addr;
    }
};

//-----------------------------------------------------------------------------
// Message IDs
//
// A "host" is the player who started the game.
// A "client" is a potential player. A client is not currently playing a game.
// A "player" is anyone playing a game.
//-----------------------------------------------------------------------------
enum
{                       // From     To      Type        Expected response
                        //-----------------------------------------------------
    MSG_FIND_GAME,      // client   host    broadcast   MSG_GAME_FOUND
    MSG_GAME_FOUND,     // host     client  broadcast   <none>
    MSG_JOIN_GAME,      // client   host    direct      MSG_JOIN_APPROVED/DENIED
    MSG_JOIN_APPROVED,  // host     client  direct      <none>
    MSG_JOIN_DENIED,    // host     client  direct      <none>
    MSG_PLAYER_JOINED,  // host     player  direct      <none>
    MSG_WAVE,           // player   player  direct      <none>
    MSG_HEARTBEAT,      // player   player  direct      <none>     
    
                                    // From     To      Type        Exp. response
                                    //------------------------------------------
    MSG_CONNECT_TO_MIGRATED_HOST,   // player   newhost direct      MSG_MIGRATE_HOST_APPROVED
    MSG_MIGRATE_HOST_APPROVED,      // newhost  player  direct      <none>    
    MSG_PLAYER_REJOINED,            // newhost  player  direct      <none>              
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
    WCHAR  strHostName[ MAX_PLAYER_NAME ]; // game host player name
};

//-----------------------------------------------------------------------------
// Sent from a client to a host to join a game
//-----------------------------------------------------------------------------
struct MsgJoinGame
{
    WCHAR  strPlayerName[ MAX_PLAYER_NAME ];  // player who wants to join
};

//-----------------------------------------------------------------------------
// Sent from a host to a client in response to a MSG_JOIN_GAME
//-----------------------------------------------------------------------------
struct MsgJoinApproved
{
    WCHAR  strHostName[ MAX_PLAYER_NAME ]; // host name    
    BYTE   byNumPlayers;                   // Players in the game 
    Player PlayerList[ MAX_PLAYERS ];      // List of players 
};

//-----------------------------------------------------------------------------
// Sent from a host to other players to notify them that a new player has joined
//-----------------------------------------------------------------------------
struct MsgPlayerJoined
{
    Player player; // The latest player to join the game
};

//-----------------------------------------------------------------------------
// Host Migration messages
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Sent from a client to a host to rejoin a game
//-----------------------------------------------------------------------------
struct MsgConnectToMigratedHost
{
    WCHAR       strPlayerName[ MAX_PLAYER_NAME ];  // player who wants to rejoin
    GameData    gameData;                          // players game data
};

//-----------------------------------------------------------------------------
// Sent from the new host to the client - compare to the JoinApproved message
//-----------------------------------------------------------------------------

struct MsgMigrateHostApproved
{   
    WCHAR  strHostName[ MAX_PLAYER_NAME ]; // host name
    BYTE   byNumPlayers;                   // Players in the game
    Player PlayerList[ MAX_PLAYERS ];      // List of players 
};

//-----------------------------------------------------------------------------
// Sent from the new host to clients - compare to the MsgPlayerJoin message
//-----------------------------------------------------------------------------

struct MsgPlayerRejoined
{   
    Player player;
};


#pragma pack( pop )

#endif // _GAMEMESSAGE_H_