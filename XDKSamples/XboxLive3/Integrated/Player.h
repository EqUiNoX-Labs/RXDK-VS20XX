#ifndef _PLAYER_H_
#define _PLAYER_H_

#include <xonline.h>
#include <vector>

//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------
const DWORD COLOR_LIGHTRED    = 0x803f0000;
const DWORD MAX_STATUS_STR    = 128;
const DWORD MAX_GAME_NAMES    = 6;      // Number of game names to choose from
const DWORD MAX_GAME_NAME     = 12;     // Includes null


//-----------------------------------------------------------------------------
// Local Player struct used by some messages
//-----------------------------------------------------------------------------
struct Player
{
    XNADDR xnAddr;                           // player's XNet address
    XUID   xuid;                             // player'x XUID
    CHAR   strGamertag[ XONLINE_GAMERTAG_SIZE ]; // player's name
};


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
    DWORD                   m_dwRenderStart;

    // *** User data ***
    // Index of the current user logged into Live Points to an index in the array found
    // by XOnlineGetUsers
    WORD                    m_wUserIndex;

public:

    CUserInfo() : m_bSignedIn( FALSE ), m_iCurSelection( 0 ),
                  m_dwRenderStart( 0 ), m_wUserIndex( 0 ) {}

    ~CUserInfo() {}
};
#endif // _PLAYER_H_