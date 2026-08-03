//-----------------------------------------------------------------------------
// File: Common.h
//
// Desc: Stats global header
//
// Hist: 04.10.02 - New for May release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef STATS_COMMON_H
#define STATS_COMMON_H

#include "xtl.h"
#include "xonline.h"
#include <vector>
#include <cassert>



//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------
const DWORD NUM_LEVELS = 4;
const DWORD MAX_PLAYERS = 4; // 1..XONLINE_MAX_LOGON_USERS
const DWORD MAX_STAT_USERS = 5;

const DWORD OVERALL_LEADERBOARD_ID = NUM_LEVELS + 1;

inline DWORD LevelIDToLeaderBoardID( DWORD dwLevelID )
{
    return dwLevelID + 1;
}

inline DWORD LeaderBoardIDToLevel( DWORD dwLeaderboardID )
{
    assert( dwLeaderboardID != OVERALL_LEADERBOARD_ID );
    return dwLeaderboardID - 1;
}


enum
{
    // Main menu
    ACTION_NEW_STATISTICS = 0,
    ACTION_RESET_STATISTICS,
    ACTION_USER_STATS,
    ACTION_OVERALL_STATS,
    ACTION_FRIENDS_STATS,
    ACTION_MAX,
};

//-----------------------------------------------------------------------------
// Typedefs
//-----------------------------------------------------------------------------
typedef std::vector< XONLINE_SERVICE_INFO > ServiceInfoList;
typedef std::vector< XONLINE_FRIEND > FriendList;
typedef std::vector< XONLINE_STAT_USER > StatUserList;
typedef std::vector< XONLINE_STAT > StatList;
typedef std::vector< XONLINE_STAT_SPEC > StatSpecList;




//-----------------------------------------------------------------------------
// Name: class CPlayerStats
// Desc: Player stats from the stats service
//-----------------------------------------------------------------------------
class CPlayerStats
{

public:
    CPlayerStats( XONLINE_STAT *pStats = NULL );
    VOID Clear();
    VOID SetKills( LONG );
    LONG GetKills() const;
    VOID SetDeaths( LONG );
    LONG GetDeaths() const;
    VOID SetAssists( LONG );
    LONG GetAssists() const;
    VOID SetStarted( LONG );
    LONG GetStarted()  const;
    VOID SetCompleted( LONG );
    LONG GetCompleted()  const;
    VOID SetRating( LONGLONG );
    LONGLONG GetRating()  const;
    LONG GetRank()  const;
    BOOL Missing()  const;

    PXONLINE_STAT GetWriteStats( DWORD *pdwNumStats );
    PXONLINE_STAT GetReadStats( DWORD *pdwNumStats );

    static PWORD GetStatIDs( DWORD *pdwNumStats );

private:


    // Stat attribute ids
    enum
    {
        STAT_KILLS,
        STAT_DEATHS,
        STAT_ASSISTS,
        STAT_STARTED,
        STAT_COMPLETED,
        STAT_RATING,
        STAT_RANK,      // Note: Must be last!
        STAT_MAX
    };


    XONLINE_STAT m_Stats[STAT_MAX];
};

//-----------------------------------------------------------------------------
// Name: class CPlayerInfo
// Desc: Information for a player in a "game"
//-----------------------------------------------------------------------------
class CPlayerInfo
{
public:
    CPlayerInfo();
    CPlayerInfo( XONLINE_STAT_USER & , XONLINE_STAT * ); 
    XUID xuid;
    WCHAR strUserName[ XONLINE_GAMERTAG_SIZE ];
    CPlayerStats  Stats;
};


typedef std::vector< CPlayerInfo > PlayerList;


#endif // STATS_COMMON_H
