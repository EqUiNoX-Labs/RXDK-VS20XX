//-------------------------------------------------------------------------------------
// File: Tourney.h
//
// Desc: Holds supporting code used to deal with competitions.
//       Utility functions perform functions such as enter a team
//       into a competition, create competitions, etc.
//
// Hist: 12.09.04 - New for January release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#pragma once

#ifndef TOURNEY_H
#define TOURNEY_H

#include <xonline.h>
#include "Common.h"
#include "Comps.h"
#include "GameMsg.h"
#include "Match.h"

// When creating a real tournament you would want to have a reasonable time
// period for people to join the tournament. However, for testing purposes
// it is useful to have a short period, because you can't start playing
// tournament rounds until registration has closed.
// Short tournament delays mean you have to register players very quickly,
// especially if the server's clock is slightly ahead of the clients (it
// can be five minutes ahead or behind).
// SECONDS_BEFORE_TOURNAMENT_CLOSE is the delay in seconds from opening
// registration to closing it. If it is too small then even the automated
// registration of users may fail, especially if you sit too long in the
// debugger. With the TimeWarp functionality built in to the sample it is
// reasonable to have long delays between tournament open and close, and
// long delays between rounds.
const DWORD SECONDS_BEFORE_TOURNAMENT_CLOSE = SECONDS_PER_HOUR * 12;

// Have the rounds happen daily. This is specified using minutes because
// this makes it easier to change to shorter frequencies.
const DWORD SECONDS_BETWEEN_ROUNDS = SECONDS_BEFORE_TOURNAMENT_CLOSE * 2;

// Specify how long the rounds should be. Make sure the round length is
// shorter than the time between rounds.
const DWORD ROUND_LENGTH_IN_SECONDS = SECONDS_BETWEEN_ROUNDS / 2;

// Normally the reminder would be set to an hour or so. Five minutes is
// the minimum. Make sure it is shorter than the time between rounds.
const DWORD REMINDER_ADVANCE_MINUTES = 5 * SECONDS_PER_MINUTE;

// How many seconds to warp into the future when time-warping.
const DWORD TIME_WARP_SECONDS = ROUND_LENGTH_IN_SECONDS * 2;

// Maximum number of results from a query page.
// If more than 25 results are returned then a second
// page needs to be queried.
const INT   MAX_RESULTS_PER_PAGE = 25;

const DWORD COMP_ENTRANT_STATUS_EJECTED = 0x3;

// The data set IDs or templates refer to the different databases that are
// being referenced by different competitions APIs. When using XLast generated
// code the only time we need to use a dataset ID directly is for time
// warping.
const DWORD COMPETITIONS_DATASET = 1;

// The competitions sample supports a maximum of eight players in a competition.
// You can set this value to a larger number, but the tournament display code
// will need to be enhanced.
const DWORD MAX_ENTRANTS       = 8;
const DWORD MIN_ENTRANTS       = 4;
const DWORD MIN_COMP_TEAM_SIZE = 4;

// The maximum number of rounds that can be played in a particular
// tournament.
const DWORD MAX_ROUND_EVENTS = MAX_ENTRANTS - 1;

// The maximum number of results in a competition search
const DWORD MAX_COMP_SEARCH_RESULTS = 50;

HRESULT TimeWarp( DWORD dwPortNumber, ULONGLONG qwCompetitionID );

BOOL CreateTournament( DWORD dwPortNumber,
                       XUID creatorXUID, XUID teamXUID,
                       CSECompetition& competition );

HRESULT WithdrawlFromCompetition( DWORD dwPortNumber, XUID xuidTeam, ULONGLONG qwCompID );

HRESULT CancelCompetition( DWORD dwPortNumber, XUID xuidTeam, ULONGLONG qwCompID );

HRESULT JoinCompetition( DWORD dwPortNumber,
                         XUID xuidUser,
                         CSECompetition& competition,
                         WCHAR* wszCompName );

HRESULT JoinCompetition( DWORD dwPortNumber,
                         XUID xuidTeam,
                         ULONGLONG qwCompID,
                         WCHAR* wszCompName );

HRESULT CheckinCompetition( DWORD dwPortNumber,
                            ULONGLONG qwTeamID,
                            ULONGLONG qwCompID,
                            ULONGLONG qwEventID );

HRESULT EjectEntrantFromCompetition( DWORD dwPortNumber,
                                     ULONGLONG qwTeamID,
                                     ULONGLONG qwTeamToEjectID,
                                     ULONGLONG qwCompID,
                                     ULONGLONG qwEventID );

HRESULT ForfeitCompetition( DWORD dwPortNumber,
                            ULONGLONG qwTeamID,
                            ULONGLONG qwCompID,
                            ULONGLONG qwEventID );

HRESULT GetCompetitionEntrants( ULONGLONG& qwID,
                                CSEEntrantsCurrentEntrantsQuery& standingsQuery );

BOOL TournamentSearch( DWORD dwUserIndex, XUID xuidUser,
                       CSEEntrantsMyCompetitionsQuery& compQuery );

BOOL TournamentSearch( DWORD dwUserIndex,
                       CSECompetitionsAvailableCompetitionsQueryResults& results );

VOID RenderTournament( CXBFont& font,
                       CSEEventsTopologyQueryResult* rwTopology,
                       DWORD dwTopologyCount,
                       ULONGLONG qwTeamID = 0x0
                      );

VOID RenderCompetitorList( CXBFont& font,
                           WCHAR* wszCompetitionName,
                           CSEEntrantsCurrentEntrantsQuery& entrantsQuery,
                           DWORD& dwRenderStart,
                           DWORD& dwItemSelected );

ULONGLONG GetTournamentTopography( CSEEntrantsMyCompetitionsQueryResult& searchResult,
                                   CSEEventsTopologyQueryResult* rwTopology,
                                   DWORD& dwTopologyCount );

ULONGLONG GetRoundOpponent( CSEEventsTopologyQueryResult* rwTopology,
                            DWORD dwTopologyCount,
                            ULONGLONG qwTeamID,
                            DWORD& dwEventIndex,
                            ULONGLONG& qwEntityID );

#endif // TOURNEY_H