#ifndef _PLAYER_H_
#define _PLAYER_H_

#include <xonline.h>

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------
const DWORD COLOR_HIGHLIGHT   = 0xffffff00; // Yellow
const DWORD COLOR_GREEN       = 0xff00ff00;
const DWORD COLOR_NORMAL      = 0xffffffff;
const DWORD COLOR_RED         = 0xff3f0000;
const DWORD COLOR_LIGHTRED    = 0x803f0000;
const DWORD MAX_STATUS_STR    = 128;
const DWORD MAX_GAME_NAMES    = 6;      // Number of game names to choose from
const DWORD NONCE_BYTES       = 8;      // Larger means less chance of random matches
const DWORD MAX_GAME_NAME     = 12;     // Includes null
const DWORD PLAYER_TIMEOUT    = 2000;   // 2 seconds
const FLOAT CHECK_LINK_STATUS = 0.5f;   // Check status twice/sec
const FLOAT PLAYER_HEARTBEAT  = 0.3f;   // ~3 times per second
const FLOAT GAME_JOIN_TIME    = 2.0f;   // 2 seconds

const DWORD MAX_PLAYERS = 8;            // Max players
const DWORD MAX_REMOTE_USERS = MAX_PLAYERS - 1;

// TCR Session Discovery Time for System Link Play
const FLOAT GAME_SEARCH_TIME = 2.0f;   // 2 seconds (may not exceed 3)

// Sending out one network packet for each 20ms packet worth of voice would
// waste a lot of bandwidth in packet overhead.  Instead, we'll coalesce many
// voice packets together, and send at regular intervals
const FLOAT VOICE_PACKET_INTERVAL   = 0.1f;   // 100ms = 10 times/s
const DWORD MAX_VOICE_PER_PACKET    = 5 * 4;  // 5 packets per player = 100ms

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
    BOOL    bHasVoice;                              // TRUE if player has voice
    DWORD   bMuted:4;                               // Local has muted this player
    BOOL    bRemoteMuted:4;                         // This player has muted us
};

//-----------------------------------------------------------------------------
// Local Player struct used by some messages
//-----------------------------------------------------------------------------
struct Player
{
    XNADDR xnAddr;                           // player's XNet address
    XUID   xuid;                             // player'x XUID
    CHAR   strGamertag[ XONLINE_GAMERTAG_SIZE ]; // player's name
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
    MSG_FIND_GAME,      // client   host    broadcast   MSG_GAME_FOUND
    MSG_GAME_FOUND,     // host     client  broadcast   <none>
    MSG_JOIN_GAME,      // client   host    reliable    MSG_JOIN_APPROVED/DENIED
    MSG_JOIN_APPROVED,  // host     client  reliable    <none>
    MSG_JOIN_DENIED,    // host     client  reliable    <none>
    MSG_PLAYER_JOINED,  // host     player  reliable    <none>
    MSG_WAVE,           // player   player  direct      <none>
    MSG_HEARTBEAT,      // player   player  direct      <none>
    MSG_VOICEINFO,      // player   player  reliable    <none>
    MSG_VOICEDATA,      // player   player  direct      <none>
    
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
    DWORD  dwNumPlayers;
    CHAR   strGamertags[ XGetPortCount() ][ XONLINE_GAMERTAG_SIZE ];  // player who wants to join
    XUID   xuids[ XGetPortCount() ];
};

//-----------------------------------------------------------------------------
// Sent from a host to a client in response to a MSG_JOIN_GAME
//-----------------------------------------------------------------------------
struct MsgJoinApproved
{
    DWORD  dwNumHostPlayers;
    CHAR   strHostGamertags[ XGetPortCount() ][ XONLINE_GAMERTAG_SIZE ]; // host name
    XUID   xuids[ XGetPortCount() ];
    BYTE   byNumPlayers;                   // Players in the game (not incl host)
    Player PlayerList[ MAX_PLAYERS ];      // List of players (not incl host)
};

//-----------------------------------------------------------------------------
// Sent from a host to other players to notify them that a new player has joined
//-----------------------------------------------------------------------------
struct MsgPlayerJoined
{
    Player player; // The latest player to join the game
};

enum VOICEINFO
{
    VOICEINFO_ADDCHATTER,
    VOICEINFO_REMOVECHATTER,
    VOICEINFO_ADDREMOTEMUTE,
    VOICEINFO_REMOVEREMOTEMUTE,
};

// The VOICEINFO message is a little bit strange - since it
// must be sent reliably, it gets relayed by the host.  This
// means we need to identify the source and destination players
// inside the packet itself.  The only consistent player identifer
// we have in this sample is player name - ideally, this should
// be replaced with a smaller unique player identifier
struct MsgVoiceInfo
{
    VOICEINFO   action;             // Action requested
    XUID        xuidSrc;            // XUID of source player
    XUID        xuidDest;           // XUID of destination player
};

// This is dependent on the time per voice packet:
// CompressedSize = 2 + ( TimeInMs * 8 / 20 )
// 20ms -> 10 Bytes
// 40ms -> 18 Bytes
#define COMPRESSED_VOICE_SIZE 10
struct VoicePacket
{
    XUID xuidSrc;
    BYTE byData[COMPRESSED_VOICE_SIZE];
};

struct MsgVoiceData
{
    WORD        wVoicePackets;
    VoicePacket VoicePackets[MAX_VOICE_PER_PACKET];
};
#pragma pack( pop )



#endif // _PLAYER_H_