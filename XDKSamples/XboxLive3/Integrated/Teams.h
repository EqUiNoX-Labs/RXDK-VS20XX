//-------------------------------------------------------------------------------------
// File: Teams.h
//
// Desc: Contains definitions for code relating to team managment.
//
// Hist: 12.09.04 - New for January release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#pragma once

#ifndef TEAMS_H
#define TEAMS_H

#include "XBOnlineTask.h"


enum
{
    STAT_ID_KILLS = 1,         // Number of Kills
    STAT_ID_DEATHS,            // Number of Deaths
    STAT_ID_ASSISTS,           // Number of Assists
    STAT_ID_ACCURACY,          // Shooting accuracy
};


// Stat attribute ids
enum
{
    STAT_KILLS = 0,
    STAT_DEATHS,
    STAT_ASSISTS,
    STAT_ACCURACY,
    STAT_RATING,
    STAT_RANK,      // Note: Must be last!
    STAT_MAX
};

const DWORD LEADERBOARD_ID         = 3;  // The ID of the leaderboard we will
                                         // Read and write from
const INT MAX_STAT_USERS           = 10; // Maximum number of teams to 
                                         // retrieve from a leaderboard
const INT NUM_STATS_SUBMITTED      = 5;  // Number of stats submitted
const INT MAX_TEAM_SIZE            = 25; // Maximum size of a team roster


// Team managment helper functions!

BOOL ChangeTeamProperties( DWORD dwControllerPort,
                           XONLINE_TEAM& team );

HRESULT SendTeamInvite( DWORD dwControllerPort,
                        XUID xuidSender,
                        XUID xuidTeam,
                        XUID xuidNewRecruit );

HRESULT ProcessInvite( DWORD dwControllerPort,
                       XONLINE_MSG_SUMMARY msgSummary,
                       XONLINE_PEER_ANSWER_TYPE eAnswer );

HRESULT CreateTeam( DWORD dwControllerPort,
                    XONLINE_TEAM_PROPERTIES& createdTeam );

HRESULT RemoveTeamMember( DWORD dwControllerPort,
                          XUID xuidTeam,
                          XUID xuidMember );

HRESULT DeleteTeam( DWORD dwControllerPort,
                    XUID xuidTeam );

HRESULT SetPermissions( DWORD dwControllerPort,
                        XUID xuidTeam,
                        XUID xuidMember,
                        DWORD dwNewPrivileges );

PWORD GetStatIDs( DWORD* pdwNumStats );

BOOL WriteTeamStatistics( XUID xuidTeam,
                          LONG& lKills, LONG& lDeaths,
                          LONG& lAssists, LONGLONG& llRating );

BOOL GetTeamLeaderboard( XONLINE_STAT_USER* rwLeaderboardUsers,
                         XONLINE_STAT* rwLeaderboardStats,
                         DWORD& dwNumLeaderboardUsers );

BOOL GetTeamList( DWORD dwControllerPort,
                  XUID xuidUser,
                  LPDIRECT3DTEXTURE8* &ppTeamLogoTextures,
                  DWORD& dwTeamLogoToDL,
                  XUID* rwTeamXUIDS,
                  XONLINE_TEAM* rwTeamInfo,
                  DWORD& dwTeamCount );

BOOL GetTeamRoster( DWORD dwControllerPort,
                    CXBOnlineTask& phTeamRosterTask,
                    XUID xuidTeam,
                    XUID* rwTeamMembers,
                    DWORD& dwTeamMemberCount );

#endif // TEAMS_H