//-----------------------------------------------------------------------------
// File: Common.h
//
// Desc: Matchmaking global header
//
// Hist: 10.19.01 - New for Nov release
//       04.22.02 - Updated for May release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef MATCHMAKING_COMMON_H
#define MATCHMAKING_COMMON_H

#include "xtl.h"
#include "xonline.h"
#include "XBRandName.h"
#include "XBNetMsg.h"
#include "XBOnlineTask.h"
#include "MatchMaking.h" // RXDK: the sample ships MatchMaking.h; there is no match.h anywhere in the XDK

#pragma warning( disable: 4786 )
#include <vector>
#include <string>
#include <algorithm>



//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------
const DWORD MAX_STATUS_STR      = 128;
const DWORD MAX_TYPE_STR        = 8;
const DWORD MAX_LEVEL_STR       = 12;
const DWORD MAX_STYLE_STR       = 12;
const DWORD MAX_SESSION_ATTRIBS = 5;
enum
{
    // Match menu
    MATCH_QUICK = 0,
    MATCH_CUSTOM,
    MATCH_MAX,

    // Customize menu
    CUSTOM_TYPE = 0,
    CUSTOM_LEVEL,
    CUSTOM_STYLE,
    CUSTOM_NAME,
    CUSTOM_FIND,
    CUSTOM_MAX,

    // Game type menu
    TYPE_ANY = 0,
    TYPE_SHORT,
    TYPE_MEDIUM,
    TYPE_LONG,
    TYPE_MAX,

    // Player type menu
    LEVEL_ANY = 0,
    LEVEL_BEGINNER,
    LEVEL_INTERMEDIATE,
    LEVEL_ADVANCED,
    LEVEL_MAX,

    // Game style menu
    STYLE_ANY = 0,
    STYLE_HEAVY,
    STYLE_LIGHT,
    STYLE_MIXED,
    STYLE_MAX,

    // Game menu
    GAME_WAVE = 0,
    GAME_LEAVE,
    GAME_INVITE_FRIENDS,
    GAME_NON_HOST_MAX,
    // The host has one extra menu - they get to start the game.
    GAME_START_GAME = GAME_NON_HOST_MAX,
    GAME_MAX,

    // Abitrated game menu
    ARBITRATED_SCORE = 0,       // Score a point
    ARBITRATED_DROPPED_SCORE,   // Score a point, but don't tell anyone.
    ARBITRATED_GAME_OVER,       // Game is over, submit results
    ARBITRATED_MAX
};




//-----------------------------------------------------------------------------
// Strings
//-----------------------------------------------------------------------------
extern const WCHAR*  strANY;

extern const WCHAR*  strSHORT;
extern const WCHAR*  strMEDIUM;
extern const WCHAR*  strLONG;

extern const WCHAR*  strBEGINNER;
extern const WCHAR*  strINTERMEDIATE;
extern const WCHAR*  strADVANCED;

extern const WCHAR*  strHEAVY;
extern const WCHAR*  strLIGHT;
extern const WCHAR*  strMIXED;




//-----------------------------------------------------------------------------
// Name: class SessionInfo
// Desc: Session information from the matchmaking server
//-----------------------------------------------------------------------------
class SessionInfo
{
    XNKID      m_SessionID;
    XNKEY      m_KeyExchangeKey;
    XNADDR     m_HostAddress;
    DWORD      m_dwPublicOpen;

    ULONGLONG m_qwGameType;
    ULONGLONG m_qwPlayerLevel;
    ULONGLONG m_qwGameStyle;
    CBlob     m_ConfigInfo;
    WCHAR     m_strOwnerName[XATTRIB_OWNER_NAME_MAX_LEN+1];
    WCHAR     m_strSessionName[XATTRIB_SESSION_NAME_MAX_LEN+1];


public:
    SessionInfo();
    SessionInfo( COptiMatchResult& );
    SessionInfo( CFindSessionByIDResult& );

    XNKID*    GetSessionID()           { return &m_SessionID; }
    XNKEY*    GetKeyExchangeKey()      { return &m_KeyExchangeKey; }
    XNADDR*   GetHostAddr()            { return &m_HostAddress; }

    // Session attributes
    DWORD     GetPublicAvail()         { return m_dwPublicOpen; }
    ULONGLONG GetGameType()            { return m_qwGameType; }
    ULONGLONG GetPlayerLevel()         { return m_qwPlayerLevel; }
    ULONGLONG GetGameStyle()           { return m_qwGameStyle; }
    WCHAR*    GetSessionName()         { return m_strSessionName; }
    WCHAR*    GetOwnerName()           { return m_strOwnerName; }

    CBlob     GetConfigInfo()          { return m_ConfigInfo; }

    VOID SetGameType( ULONGLONG );
    VOID SetPlayerLevel( ULONGLONG );
    VOID SetGameStyle( ULONGLONG );
    VOID SetSessionName( const WCHAR* );
    VOID SetOwnerName( const WCHAR* );
    VOID SetConfigInfo( const CBlob & );

    VOID GenRandSessionName();

};




//-----------------------------------------------------------------------------
// Types
//-----------------------------------------------------------------------------
typedef std::vector< std::wstring > SessionNameList;
typedef std::vector< SessionInfo >  SessionList;
typedef std::vector< XONLINE_FRIEND > FriendList;




#endif // MATCHMAKING_COMMON_H
