//-------------------------------------------------------------------------------------
// File: GameSession.h
//
// Desc: Holds constants and structures used to manage a match/game session.
//
// Hist: 12.09.04 - New for January release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#pragma once

#ifndef GAME_SESSION_H
#define GAME_SESSION_H

#include <xtl.h>
#include <xonline.h>
#include "common.h"
#include "player.h"
#include "GameMsg.h"
#include "Match.h"
#include "Comps.h"
#include "XBSocket.h"


// Used for creating and finding matches

const INT  TYPE_SHORT     = 0;
const INT  LEVEL_BEGINNER = 0;
const INT  STYLE_HEAVY    = 0;
const WORD TYPE_ANY       = 0;
const WORD STYLE_ANY      = 0;
const WORD LEVEL_ANY      = 0;


// Constants to find a game competition

const INT MATCH_DATASET         = 0xA1;
const INT MATCH_EVENT_ENTITY_ID = 0x1;
const INT MATCH_ATTEMPT         = 0x2;
const INT MATCH_HOST_USER_ID    = 0x3;

const INT MATCH_FIND_EVENT_HOST      = 0xA101;
const INT MATCH_REMOVE_HOSTED_EVENTS = 0xA102;


//-------------------------------------------------------------------------------------
// Name: class SessionInfo
// Desc: Session information from the matchmaking server
//-------------------------------------------------------------------------------------
class SessionInfo
{
    XNKID       m_SessionID;      // Session ID of the match
    XNKEY       m_KeyExchangeKey; // Key of the match
    XNADDR      m_HostAddress;    // IP address of the host
    DWORD       m_dwPublicOpen;   // Number of slots open to the public

    ULONGLONG   m_qwGameType;     // Type of game beging played
    ULONGLONG   m_qwPlayerLevel;  // Skill level of players
    ULONGLONG   m_qwGameStyle;    // Style of the game
    CBlob       m_ConfigInfo;     // Any extra information

    WCHAR       m_strOwnerName[XATTRIB_OWNER_NAME_MAX_LEN + 1];
    WCHAR       m_strSessionName[XATTRIB_SESSION_NAME_MAX_LEN + 1];


public:
    SessionInfo();
    SessionInfo( COptiMatchResult& );
    SessionInfo( CFindSessionByIDResult& );

    XNKID*      GetSessionID()           { return &m_SessionID; }
    XNKEY*      GetKeyExchangeKey()      { return &m_KeyExchangeKey; }
    XNADDR*     GetHostAddr()            { return &m_HostAddress; }

    // Session attributes
    DWORD       GetPublicAvail()         { return m_dwPublicOpen; }
    ULONGLONG   GetGameType()            { return m_qwGameType; }
    ULONGLONG   GetPlayerLevel()         { return m_qwPlayerLevel; }
    ULONGLONG   GetGameStyle()           { return m_qwGameStyle; }
    WCHAR*      GetSessionName()         { return m_strSessionName; }
    WCHAR*      GetOwnerName()           { return m_strOwnerName; }

    CBlob       GetConfigInfo()          { return m_ConfigInfo; }

    VOID        SetGameType( ULONGLONG );
    VOID        SetPlayerLevel( ULONGLONG );
    VOID        SetGameStyle( ULONGLONG );
    VOID        SetSessionName( const WCHAR* );
    VOID        SetOwnerName( const WCHAR* );
    VOID        SetConfigInfo( const CBlob & );

    VOID        GenRandSessionName();

};

// Used to store all the results of a match-making
// search. A title should do some sort of selection
// to provide for load-balancing and optimum game
// experience. This title will simply pick the
// first result
typedef std::vector< SessionInfo >  SessionList;

HRESULT SetPresence( DWORD dwControllerPort, DWORD dwNumXUIDS, XUID* rwXUIDS );

HRESULT CreateArbitratedRound( PlayerList& rwPlayers,
                               XONLINE_ARB_ID& arbitrationID,
                               CSession& hostedSession,
                               CXBSocket& networkMessageHandler );

HRESULT RegisterForArbitration( XONLINE_ARB_ID arbID,
                                BOOL bIsHost,
                                PlayerList& rwPlayers,
                                CXBSocket& networkMessageHandler );

HRESULT RegisterWithCompetition( XONLINE_ARB_ID* pArbitrationID,
                                 WORD wRoundLength );

HRESULT CreateSession( CUserInfo* m_localUsers,
                       DWORD& dwSlotsInUse,
                       CSession& hostedSession,
                       SessionInfo& sessionInfo,
                       CHAR* szGamerTag );

VOID UpdateSession( CSession& hostedSession,
                    DWORD& dwSlotsInUse,
                    BOOL& bArbitrationStarted );

HRESULT DeleteSession( CSession& hostedSession,
                       PlayerList& rwPlayers );

HRESULT RemoveHostEntry( DWORD dwControllerPort, ULONGLONG qwTeamID );

#endif GAME_SESSION_H