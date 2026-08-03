//-------------------------------------------------------------------------------------
// File: Tourney.cpp
//
// Desc: Holds supporting code used to deal with competitions.
//       Utility functions perform functions such as enter a team
//       into a competition, create competitions, etc.
//
// Hist: 12.09.04 - New for January release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#include <xtl.h>
#include <xonline.h>
#include <stdio.h>

#include <XBUtil.h>
#include <XBFont.h>
#include <XBOnlineTask.h>
#include <XBRandName.h>

#include "Tourney.h"
#include "GameSession.h"
#include "Menus.h"

//-----------------------------------------------------------------------------
// Name: TimeWarp()
// Desc: Warp through time - tell the server to adjust the competition times
//       back so that it is time for the next round. This is equivalent to
//       moving the user into the future. This can be done to allow testing
//       of competitions that would otherwise take days, and avoids the timing
//       problems of having very short rounds.
//-----------------------------------------------------------------------------
HRESULT TimeWarp( DWORD dwPortNumber, ULONGLONG qwCompetitionID )
{
    // Step 1
    //
    // Fill the data structures to submit to the
    // competition query command

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
    //const DWORD SECONDS_BEFORE_TOURNAMENT_CLOSE = SECONDS_PER_HOUR * 12;

    // Have the rounds happen daily. This is specified using minutes because
    // this makes it easier to change to shorter frequencies.
    //const DWORD SECONDS_BETWEEN_ROUNDS = SECONDS_BEFORE_TOURNAMENT_CLOSE * 2;

    // Specify how long the rounds should be. Make sure the round length is
    // shorter than the time between rounds.
    //const DWORD ROUND_LENGTH_IN_SECONDS = SECONDS_BETWEEN_ROUNDS / 2;

    // How many seconds to warp into the future when time-warping.
    //const DWORD TIME_WARP_SECONDS = ROUND_LENGTH_IN_SECONDS * 2;

    // The data set IDs or templates refer to the different databases that are
    // being referenced by different competitions APIs. When using XLast generated
    // code the only time we need to use a dataset ID directly is for time
    // warping.
    const DWORD COMPETITIONS_DATASET = 1;
    const DWORD NUM_ATTRIBUTES       = 1;

    XONLINE_ATTRIBUTE rwAttributes[ NUM_ATTRIBUTES ] =
    {
        { XONLINE_COMP_ATTR_DEBUG_ADVANCE_TIME, TRUE, TIME_WARP_SECONDS }
    };


    CXBOnlineTask hTimeWarpTask;
    DWORD dwCommand = XONLINE_COMP_ACTION_DEBUG_ADVANCE_TIME;


    // Step 2
    //
    // Initiate the command

    HRESULT hrTimeWarp = XOnlineQuerySelect(
        dwPortNumber,         // Controller port of the user making the call
        0,                    // Team ID
        COMPETITIONS_DATASET, // Dataset ID
        qwCompetitionID,      // Competition ID
        dwCommand,            // Command to execute
        NUM_ATTRIBUTES,       // Number of attributes
        rwAttributes,         // Attribute arrays
        NULL,                 // Event to be triggered upon completion
        &hTimeWarpTask        // Task to assign to
    );

    if( FAILED( hrTimeWarp ) )
        return hrTimeWarp;


    // Step 3
    //
    // Pump the task until it is finished

    WaitForTaskToComplete( hTimeWarpTask, &hrTimeWarp, TRUE );


    // Step 4
    //
    // Close the task and return success

    hTimeWarpTask.Close();

    return hrTimeWarp;
}

//-----------------------------------------------------------------------------
// Name: CreateTournament()
// Desc: Creates a competition. If the competition is created then
//       the given team is automatically joined. Returns the competition
//       data via the output parameter.
//-----------------------------------------------------------------------------
BOOL CreateTournament( DWORD dwPortNumber,
                       XUID creatorXUID,
                       XUID teamXUID,
                       CSECompetition& competition )
{
    // Step 1
    //
    // Fill in the details for our competition

    XONLINE_COMP_SINGLE_ELIMINATION_ATTRIBUTES compAttributes = { 0 };

    compAttributes.dwPrivateSlots  = 0;
    compAttributes.dwPublicSlots   = MAX_ENTRANTS - compAttributes.dwPrivateSlots;

    // The competition will be cancelled if the minimum number of players don't
    // sign up before registration closes.
    compAttributes.dwMinimumPlayers = MIN_ENTRANTS;

    FILETIME systemTime;
    GetSystemTimeAsFileTime( &systemTime );
    // Registration opens today.
    compAttributes.ftRegistrationOpen = systemTime;

    // Set the registration to close in 24 hours.
    compAttributes.ftRegistrationClose = AddSeconds( compAttributes.ftRegistrationOpen,
                                                     SECONDS_BEFORE_TOURNAMENT_CLOSE );

    // Make sure that competition start isn't earlier than registration close.
    // I set it to five seconds later - you would normally have a longer delay.
    const DWORD MINSTARTDELAYSECONDS = 5;
    compAttributes.ftCompetitionStart = AddSeconds( compAttributes.ftRegistrationClose,
                                                    MINSTARTDELAYSECONDS );

    compAttributes.dwMatchReminderAdvanceMinutes = REMINDER_ADVANCE_MINUTES;

    // The time from registration close to tournament start
    // needs to be longer than the reminder time.
    // There must be sufficient time between compStart and roundOneStart to fire a reminder.
    // The round one start time must be greater than the competition start time plus
    // the reminder time - I add one extra second to ensure it is greater.
    compAttributes.ftRoundOneStart = AddSeconds( compAttributes.ftCompetitionStart,
                                                 ( REMINDER_ADVANCE_MINUTES + 1 )
                                                    * SECONDS_PER_MINUTE );

    // The first round must end before the second round starts.
    compAttributes.ftRoundOneEnd = AddSeconds( compAttributes.ftRoundOneStart,
                                               ROUND_LENGTH_IN_SECONDS );

    // This code would set up daily rounds.
    // This says that we have a new round every day.
    //compAttributes.Interval = XONLINE_COMP_INTERVAL_DAILY;
    // When the Interval type is daily we set a day mask rather than a count.
    //compAttributes.UnitOrMask.DayMask = XONLINE_COMP_DAY_MASK_ALL;

    // Specify how frequently the rounds should happen.
    compAttributes.Interval = XONLINE_COMP_INTERVAL_MINUTE;
    compAttributes.UnitOrMask.dwUnitsOfTime = SECONDS_BETWEEN_ROUNDS / SECONDS_PER_MINUTE;

    // Team attributes
    compAttributes.fTeamCompetition = TRUE;
    compAttributes.dwTeamSize       = MIN_COMP_TEAM_SIZE;


    // This competition expects 3 additional arguments ...
    // This is specific to the way this competition is set up, other competitions may not
    // need attributes at all

    // With the generated code from XLAST, all we need to do
    // is pass the extra arguments into the competition-specific
    // Create function.
    const INT MAX_COMP_NAME_LENGTH      = 32;
    WCHAR wszName[MAX_COMP_NAME_LENGTH] = { 0 };

    XBRandName_GetRandomName( wszName, MAX_COMP_NAME_LENGTH );

    const BYTE  rgbBlobValue[7] = { 0x1, 0x2, 0x3, 0x4, 0x3, 0x2, 0x1 };
    const DWORD mapID = 1234;

    CBlob tempBlob( sizeof( rgbBlobValue ), (PVOID)rgbBlobValue );


    // Step 2
    //
    // Start the creation of the competition

    HRESULT hr = competition.Create(
                    dwPortNumber,                    // Controller port of user
                                                     // initiating the call
                    &compAttributes,                 // Attrbutes for the competition
                                                     // to have
                    wszName,                         // Display Name
                    (LPWSTR)wszDefaultSEDescription, // Description
                    (LPWSTR)wszDefaultSEURL,         // URL
                    teamXUID.qwTeamID,               // Team ID
                    mapID,                           // Map number/ID of "where"
                                                     // the competition will be held
                    wszName,                         // Name
                    tempBlob );                      // Any other information we want 
                                                     // to associate


    // Step 3
    //
    // Pump the task until complete

    // When the xonline task for creating this competition
    // is complete, competition will hold the new competition
    // ID for us.
    do
    {
        hr = competition.Process();
    }
    while( hr == XONLINETASK_S_RUNNING );


    // Step 4
    //
    // Check to see if the competition creation failed

    if( FAILED( hr ) )
    {
        // The quota for competitions will probably be about three active competitions
        // per user, although it may be higher on the test network. Competitions that
        // are recently cancelled will count against this quota.
        // Give a verbose error message for this particular error.
        // ( hr == XONLINE_E_QUERY_QUOTA_FULL )

        return FALSE;
    }


    // Step 5
    //
    // Join the competition we just created and return our success

    assert( competition.m_CreateResults.qwCompetitionID );

    // Have the creator join the competition they just created.
    hr = JoinCompetition( dwPortNumber,
                          teamXUID,
                          competition,
                          wszName );

    // NOTE:
    // If the creator can't join his own competition,
    // it may be a good idea to cancel the competition.
    return SUCCEEDED( hr );
}

//-----------------------------------------------------------------------------
// Name: WithdrawlFromCompetition()
// Desc: Withdraws the team from the given competition. Returns the result
//       code from the withdrawl operation.
//-----------------------------------------------------------------------------
HRESULT WithdrawlFromCompetition( DWORD dwPortNumber, XUID xuidTeam, ULONGLONG qwCompID )
{
    // Step 1:
    //
    // Start the canceling operation

    CXBOnlineTask hWithdrawlTask;

    HRESULT hrWithdrawl = XOnlineCompetitionManageEntrant(
                                XONLINE_COMP_ACTION_WITHDRAW, // Action
                                dwPortNumber,                 // Controller port
                                SE_ID,                        // Template ID number
                                                              // from XLAST
                                xuidTeam.qwTeamID,            // Team ID
                                qwCompID,                     // Competition ID
                                0,                            // Num attributes
                                NULL,                         // Attribute array
                                NULL,                         // Work event
                                &hWithdrawlTask );            // Task handle

    if( !SUCCEEDED( hrWithdrawl ) )
        return hrWithdrawl;


    // Step 2
    //
    // Pump the task until it is complete

    WaitForTaskToComplete( hWithdrawlTask, &hrWithdrawl );


    // Step 3
    //
    // Close the task and return our success

    hWithdrawlTask.Close();

    return hrWithdrawl;
}

//-----------------------------------------------------------------------------
// Name: CancelCompetition()
// Desc: Cancels the given competition. Returns the result code of the
//       cancelling operation.
//-----------------------------------------------------------------------------
HRESULT CancelCompetition( DWORD dwPortNumber, XUID xuidTeam, ULONGLONG qwCompID )
{
    // Step 1
    //
    // Start the canceling operation

    CXBOnlineTask hCancelTask;

    HRESULT hrCancel = XOnlineCompetitionCancel(
                            dwPortNumber,      // Controller port of the user
                                               // triggering the operation
                            SE_ID,             // Competition database ID
                                               // ( generated by XLAST )
                            xuidTeam.qwTeamID, // ID of the team cancelling the competition.
                                               // Only the team that created the competition 
                                               // can cancel it
                            qwCompID,          // ID of the competition to be canceled
                            NULL,              // Event to be triggered up completion
                            &hCancelTask );    // Task to assign ( and pump )

    if( FAILED( hrCancel ) )
        return hrCancel;


    // Step 2
    //
    // Pump the task until complete

    WaitForTaskToComplete( hCancelTask, &hrCancel );


    // Step 3
    //
    // Close the task and return our success

    hCancelTask.Close();

    return hrCancel;
}

//-----------------------------------------------------------------------------
// Name: JoinCompetition()
// Desc: Joins the given team into the given competition. Returns
//       the result of the operation. Takes the competition structure.
//-----------------------------------------------------------------------------
HRESULT JoinCompetition( DWORD dwPortNumber,
                         XUID xuidTeam,
                         CSECompetition& competition,
                         WCHAR* wszCompName )
{
    // Step 1
    //
    // Fill in the details for the joining operation
    // that will be submitted to the competition manager

    DWORD             dwNumAttribs = 1;
    XONLINE_ATTRIBUTE xoAttrib     = { 0 };

    // IMPORTANT: This needs to be set
    // in order for the "MyCompetitions" query
    // to return a proper name for the competition
    xoAttrib.dwAttributeID       = (DWORD)ATT_ENTRANTS_COMP_NAME_ID;
    xoAttrib.fChanged            = TRUE;
    xoAttrib.info.string.lpValue = wszCompName;


    // You can specify extra data such as their seed - typically a rating from
    // a leaderboard
    // XONLINE_COMP_ATTR_ENTRANT_SEED


    // Step 2
    //
    // Submit the join request

    // Add the new user to the competition. This is asynchronous and you would
    // normally want to do other things while waiting for the task to complete.

    // The competition manager has a thin wrapper for XOnlineCompetitionManageEntrant
    // that makes it slightly easier for us - we don't need to worry about the
    // competition ID because that was saved when we created the competition
    // Note that we could also have set the competition ID manually.  For instance,
    // we could have searched for the currently available competitions, selected one,
    // and then joined it by setting competition.m_CreateResults.qwCompetitionID
    HRESULT hr = competition.ManageEntrant( XONLINE_COMP_ACTION_JOIN,
                                            dwPortNumber,
                                            xuidTeam.qwTeamID,
                                            dwNumAttribs,
                                            &xoAttrib );

    if( FAILED( hr ) )
    {
        assert( 0 && "XOnlineCompetitionManageEntrant failed" );

        return hr;
    }


    // Step 3
    //
    // Pump the task until complete and then return our success

    // If this fails with 0x80156206 (XONLINE_E_COMP_CANCELLED) it may mean that the
    // competition was closed before enough competitors signed up, and was
    // therefore automatically cancelled.
    // If this fails with 0x80156203 (XONLINE_E_COMP_REGISTRATION_CLOSED) it means that the
    // registration closed, presumably because the registration open time had expired.
    do
    {
        hr = competition.Process();
    }
    while( hr == XONLINETASK_S_RUNNING );

    return hr;
}

//-----------------------------------------------------------------------------
// Name: JoinCompetition()
// Desc: Joins the given team into the given competition. Returns
//       the result of the operation. Takes the ID of the competition.
//-----------------------------------------------------------------------------
HRESULT JoinCompetition( DWORD dwPortNumber,
                         XUID xuidTeam,
                         ULONGLONG qwCompID,
                         WCHAR* wszCompName )
{
    // Step 1
    //
    // Fill in the details that will be
    // submitted in the join request.

    assert( wszCompName );

    CXBOnlineTask     hJoinComp;
    XONLINE_ATTRIBUTE xoAttrib;
    DWORD             dwNumAttribs = 1;


    // IMPORTANT: This needs to be set
    // in order for the "MyCompetitions" query
    // to return a proper name for the competition
    xoAttrib.dwAttributeID       = (DWORD)ATT_ENTRANTS_COMP_NAME_ID;
    xoAttrib.fChanged            = TRUE;
    xoAttrib.info.string.lpValue = wszCompName;


    // Step 2
    //
    // Start the join request and submitt the data to the server

    HRESULT hrJoinComp = XOnlineCompetitionManageEntrant(
                            XONLINE_COMP_ACTION_JOIN, //Action
                            dwPortNumber,             // Controller port
                            SE_ID,                    // Template ID number from XLAST
                            xuidTeam.qwTeamID,        // Team ID
                            qwCompID,                 // Competition ID
                            dwNumAttribs,             // Num attributes
                            &xoAttrib,                // Attribute array
                            NULL,                     // Work event
                            &hJoinComp );             // Task handle

    if( !SUCCEEDED( hrJoinComp ) )
    {
        hJoinComp.Close();
        return hrJoinComp;
    }


    // Step 3
    //
    // Pump the task until complete

    WaitForTaskToComplete( hJoinComp, &hrJoinComp );


    // Step 4
    //
    // Close the task and return our success

    hJoinComp.Close();
    return hrJoinComp;
}

//-----------------------------------------------------------------------------
// Name: CheckinCompetition()
// Desc: Checks the teams into the round which says they are ready
//       to compete.
//-----------------------------------------------------------------------------
HRESULT CheckinCompetition( DWORD dwPortNumber,
                            ULONGLONG qwTeamID,
                            ULONGLONG qwCompID,
                            ULONGLONG qwEventID )
{
    // Step 1
    //
    // Start the checkin task

    CXBOnlineTask hCheckinTask;

    HRESULT hrCheckin = XOnlineCompetitionCheckin(
                            dwPortNumber,    // Controller port of the user checking in
                            SE_ID,           // Template ID (from XLAST)
                            qwTeamID,        // ID of the team checking in
                            qwCompID,        // ID of the competition being checked into
                            qwEventID,       // ID of the event ( round ID )
                            NULL,            // Event to be triggered when finished
                            &hCheckinTask ); // Task to assign

    if( FAILED( hrCheckin ) )
        return hrCheckin;


    // Step 2
    //
    // Pump the task until complete

    WaitForTaskToComplete( hCheckinTask, &hrCheckin );


    // Step 3
    //
    // Close the task and return our success

    hCheckinTask.Close();

    return hrCheckin;
}

//-----------------------------------------------------------------------------
// Name: EjectEntrantFromCompetition()
// Desc: Ejects the given team from the competition with the given ID.
//-----------------------------------------------------------------------------
HRESULT EjectEntrantFromCompetition( DWORD dwPortNumber,
                                     ULONGLONG qwTeamID,
                                     ULONGLONG qwTeamToEjectID,
                                     ULONGLONG qwCompID,
                                     ULONGLONG qwEventID )
{
    // Step 1
    //
    // Fill in the details:
    // The entitity ID
    // and the team we want to eject

    DWORD             dwNumAttribs = 2;
    XONLINE_ATTRIBUTE xoAttrib[2]  = { 0 };

    xoAttrib[0].dwAttributeID        = XONLINE_COMP_ATTR_ENTRANT_PUID;
    xoAttrib[0].fChanged             = FALSE;
    xoAttrib[0].info.integer.qwValue = qwTeamToEjectID;

    xoAttrib[1].dwAttributeID        = XONLINE_COMP_ATTR_EVENT_ENTITY_ID;
    xoAttrib[1].fChanged             = FALSE;
    xoAttrib[1].info.integer.qwValue = qwEventID;


    // Step 2
    //
    // Start the task and submitt to the server

    CXBOnlineTask hEjectTask;

    HRESULT hrEject = XOnlineCompetitionManageEntrant(
                                XONLINE_COMP_ACTION_EJECT, // Action to perform
                                dwPortNumber,              // Controller port
                                SE_ID,                     // Template ID number
                                                           // from XLAST
                                qwTeamID,                  // Team ID
                                qwCompID,                  // Competition ID
                                dwNumAttribs,              // Num attributes
                                xoAttrib,                  // Attribute array
                                NULL,                      // Work event
                                &hEjectTask );             // Task handle

    if( FAILED( hrEject ) )
        return hrEject;


    // Step 3
    //
    // Pump the task until complete

    WaitForTaskToComplete( hEjectTask, &hrEject );


    // Step 4
    //
    // Close the task and return our success

    hEjectTask.Close();

    return hrEject;
}

//-----------------------------------------------------------------------------
// Name: ForfeitCompetition()
// Desc: Forfeits the round ( and therefor the comepetition ) for the given
//       team resulting in a bye for the scheduled round opponent.
//-----------------------------------------------------------------------------
HRESULT ForfeitCompetition( DWORD dwPortNumber,
                            ULONGLONG qwTeamID,
                            ULONGLONG qwCompID,
                            ULONGLONG qwEventID )
{
    // Step 1
    //
    // Fill in the details that will be
    // submitted to the service

    DWORD dwNumAttribs         = 1;
    XONLINE_ATTRIBUTE xoAttrib;

    // IMPORTANT: This needs to be set
    // in order for the "MyCompetitions" query
    // to return a proper name for the competition
    xoAttrib.dwAttributeID        = XONLINE_COMP_ATTR_EVENT_ENTITY_ID;
    xoAttrib.fChanged             = FALSE;
    xoAttrib.info.integer.qwValue = qwEventID;


    // Step 2
    //
    // Start the request

    CXBOnlineTask hForfeitTask;

    HRESULT hrForfeit = XOnlineCompetitionManageEntrant(
                                XONLINE_COMP_ACTION_FORFEIT, // Action
                                dwPortNumber,                // Controller port
                                SE_ID,                       // Template ID number
                                                             // from XLAST
                                qwTeamID,                    // Team ID
                                qwCompID,                    // Competition ID
                                dwNumAttribs,                // Num attributes
                                &xoAttrib,                   // Attribute array
                                NULL,                        // Work event
                                &hForfeitTask                // Task handle
                            );

    if( !SUCCEEDED( hrForfeit ) )
        return hrForfeit;


    // Step 3
    //
    // Pump the task until complete

    WaitForTaskToComplete( hForfeitTask, &hrForfeit );


    // Step 4
    //
    // Close the task and return our success

    hForfeitTask.Close();

    return hrForfeit;
}

//-----------------------------------------------------------------------------
// Name: GetCompetitionEntrants()
// Desc: Retrieves the list of entrants for the competition with the given ID.
//-----------------------------------------------------------------------------
HRESULT GetCompetitionEntrants( ULONGLONG& qwID,
                                CSEEntrantsCurrentEntrantsQuery& entrantsQuery )
{
    // Step 1
    //
    // Clear the previous query and start the new query

    entrantsQuery.Clear();

    HRESULT hrEntrants = entrantsQuery.Query( qwID, 0 );

    if( FAILED( hrEntrants ) )
        return hrEntrants;


    // Step 2
    //
    // Pump the task until complete

    do
    {
        hrEntrants = entrantsQuery.Process();
    }
    while( hrEntrants == XONLINETASK_S_RUNNING );


    // Step 3
    //
    // Return our success

    return hrEntrants;
}


//-----------------------------------------------------------------------------
// Name: TournamentSearch()
// Desc: Search for all tournaments associated with the selected user.
//       The list of tournaments is then displayed on the
//       select screen for the user to choose from.
//-----------------------------------------------------------------------------
BOOL TournamentSearch( DWORD dwUserIndex,
                       XUID xuidUser,
                       CSEEntrantsMyCompetitionsQuery& compQuery )
{
    // NOTE:
    //
    // You may query for competitions of different statuses
    // Each query for a competition of a different status
    // must be done separatly. You may combine the list
    // by doing several passes, or you may wish to allow the
    // user to select which status competition to search for

    const DWORD rwPasses[] = { XONLINE_COMP_STATUS_ACTIVE,
                               XONLINE_COMP_STATUS_PRE_INIT,
                               XONLINE_COMP_STATUS_COMPLETE,
                               XONLINE_COMP_STATUS_CANCELED };

    const INT numPasses = sizeof( rwPasses ) / sizeof( DWORD );

    compQuery.Clear();
    compQuery.dwItemsReturned      = 0;
    compQuery.dwTotalItemsInResult = 0;

    DWORD dwNewResultIndex = 0;


    // Errors on the first search pass will cause the user to be logged out, in
    // which case we need to stop searching - but may still want to display
    // whatever results we already obtained.
    for( DWORD dwSearchPass = 0;
         ( dwSearchPass < numPasses ) && ( dwNewResultIndex < MAX_COMP_SEARCH_RESULTS );
         ++dwSearchPass )
    {
        // compQuery is an instance of the class that performs
        // this query. All we have to do is reset it, then perform the
        // query. We call StopTask here because compQuery may
        // be in the STATE_DONE state, which means a previous query
        // completed successfully. We immediately use the results of the
        // previous query in this sample, so just reset the query class
        // instance and continue...
        DWORD dwQueryType = rwPasses[dwSearchPass];

        CSEEntrantsMyCompetitionsQuery newQuery;

        HRESULT hr = S_OK;

        newQuery.Clear();
        newQuery.dwItemsReturned      = 0;
        newQuery.dwTotalItemsInResult = 0;

        // Execute the query for the competitions
        hr = newQuery.Query( xuidUser.qwUserID, // ID of the user who joined
                             dwQueryType,       // Status of the compeition
                             0 );               // Page of results

        // This query did not have any results
        // so continue on to the next status search.
        if( FAILED( hr ) )
        {
            break;
        }

        // When the query task is completed, the query class instance
        // will reap the query results and store it in its local buffer
        do
        {
            RenderWorkingScreen();

            hr = newQuery.Process();
        }
        while( hr == XONLINETASK_S_RUNNING );

        // This query did not have any results
        // so continue on to the next status search.
        if( FAILED( hr ) )
        {
            break;
        }

        // We don't have to call XOnlineCompetitionSearchGetResults - the query
        // class did that for us.  We will use some information it picked up
        // from doing so, however.
        DWORD   cReturned = newQuery.dwItemsReturned;
        DWORD   cTotal    = newQuery.dwTotalItemsInResult;

        // Make sure we are getting reasonable results.
        assert( cReturned <= cTotal );

        // Make a list of strings to choose from.
        for( DWORD i = 0;
             ( i < cReturned )
                && ( i < MAX_RESULTS_PER_PAGE )
                && ( ( dwNewResultIndex + 1 ) < MAX_RESULTS_PER_PAGE );
             ++i )
        {
            assert( newQuery.Results[i].att_comp_id > 0 );

            compQuery.Results[dwNewResultIndex] = newQuery.Results[i];
            ++compQuery.dwItemsReturned;

            ++dwNewResultIndex;

            compQuery.dwItemsReturned      = dwNewResultIndex;
            compQuery.dwTotalItemsInResult = dwNewResultIndex;

            if( ( dwNewResultIndex + 1 ) >= MAX_COMP_SEARCH_RESULTS )
            {
                return ( compQuery.dwItemsReturned > 0 );
            }
        }
    }

    // Return our success

    return ( compQuery.dwItemsReturned > 0 );
}


//-----------------------------------------------------------------------------
// Name: TournamentSearch()
// Desc: Searchs for open tourneys that can be joined by the team.
//       Returns the results via the output parameter.
//-----------------------------------------------------------------------------
BOOL TournamentSearch( DWORD dwUserIndex,
                       CSECompetitionsAvailableCompetitionsQueryResults& results )
{
    // Step 1
    //
    // Fill in the parameters that will be used to find
    // available competitions by the service

    // Get the system time and convert it to
    // a quad-word

    FILETIME    SystemTime;
    GetSystemTimeAsFileTime( &SystemTime );
    ULONGLONG   qwTime = ( ( (ULONGLONG)SystemTime.dwHighDateTime ) << 32 ) 
                         + SystemTime.dwLowDateTime;

    XONLINE_ATTRIBUTE rwCustomAttrs[1];
    ZeroMemory(rwCustomAttrs, sizeof( rwCustomAttrs ));

    rwCustomAttrs[0].dwAttributeID = 
        SE_COMPETITIONS_AVAILABLE_COMPETITIONS_P_CURRENT_TIME;

    rwCustomAttrs[0].info.integer.qwValue = qwTime;

    DWORD dwNumResults    = sizeof( results.v ) / sizeof( results.v[0] );
    DWORD dwNumAttributes = sizeof( rwCustomAttrs )/sizeof( rwCustomAttrs[0] );
    DWORD dwNumResultSpec = sizeof( results.ResultSpec ) / sizeof( results.ResultSpec[0] );

    DWORD dwSearchID      = SE_COMPETITIONS_AVAILABLE_COMPETITIONS_ID;
    DWORD dwDatabaseID    = SE_COMPETITIONS_ID;


    // Step 2
    //
    // Start the query

    CXBOnlineTask hSearchTask;

    HRESULT hr = XOnlineCompetitionSearch(
                    dwSearchID,         // Query ID number
                    dwDatabaseID,       // Target database ID
                    0,                  // results page
                    dwNumResults,       // Number of results that can be returned
                    dwNumAttributes,    // Number of custom search attributes
                    rwCustomAttrs,      // Custom attribute array
                    dwNumResultSpec,    // Number of items in the result spec
                    results.ResultSpec, // The result spec array
                    NULL,               // Event to be triggered
                    &hSearchTask );     // Task to be assigned

    if( FAILED( hr ) )
        return FALSE;


    // Step 3
    //
    // Pump the task until complete

    if( !WaitForTaskToComplete( hSearchTask, &hr, TRUE ) )
    {
        hSearchTask.Close();

        return FALSE;
    }


    // Step 4
    //
    // The task is completed - get the results

    results.SetSize( sizeof( results.v ) );

    DWORD dwTotalItemsInResult = 0;
    DWORD dwItemsReturned      = 0;

    hr = XOnlineCompetitionSearchGetResults(
                hSearchTask,           // Task used to search
                &dwTotalItemsInResult, // Total number of items found
                &dwItemsReturned,      // Number of items placed in results array
                &results.m_dwSize,     // size of the results buffer
                (PBYTE)results.v );    // buffer to write results to


    // Step 5
    //
    // Close the task and return our results.

    hSearchTask.Close();

    // On output, results.m_dwSize is the number of bytes used, rather
    // than the number of elements that are valid
    results.SetSize(dwItemsReturned);

    return ( SUCCEEDED( hr ) && ( dwItemsReturned > 0 ) );
}

//-----------------------------------------------------------------------------
// Name: RenderTournament()
// Desc: Renders the given competition topography to the screen.
//       Higlights any rounds that contain the given team ID.
//-----------------------------------------------------------------------------
VOID RenderTournament( CXBFont& font,
                       CSEEventsTopologyQueryResult* rwTopology,
                       DWORD dwTopologyCount,
                       ULONGLONG qwTeamID )
{
    assert( rwTopology );

    // Render the topology of the tournament from the data obtained previously.
    FLOAT hOffset = 40;
    FLOAT hDelta  = 200;
    FLOAT vDelta  = 81;
    FLOAT vStart  = 80;
    FLOAT vOffset = vStart + vDelta / 2;

    ULONGLONG currentRound = rwTopology[0].round;
    WCHAR     buffer[2000];

    font.SetScaleFactors( .9f, .9f );

    for( DWORD i = 0; i < dwTopologyCount; ++i )
    {
        // Make sure we have reasonable information - we don't want
        // a player playing themself or beating themself.
        // Make sure the players is not playing themself.
        assert( rwTopology[i].player1 == 0 ||
                rwTopology[i].player1 != rwTopology[i].player2 );

        // Make sure the loser is not also the winner.
        assert( rwTopology[i].loser == 0 ||
                rwTopology[i].loser != rwTopology[i].winner );

        // The loser, if set, should be player 1 or player 2.
        assert( rwTopology[i].loser == 0 ||
                rwTopology[i].loser == rwTopology[i].player1 ||
                rwTopology[i].loser == rwTopology[i].player2 );

        // The winner, if set, should be player 1 or player 2.
        assert( rwTopology[i].winner == 0 ||
                rwTopology[i].winner == rwTopology[i].player1 ||
                rwTopology[i].winner == rwTopology[i].player2 );

        if( rwTopology[i].round != currentRound )
        {
            // Skip to the next round - move horizontally and reset
            // the vertical position.
            currentRound = rwTopology[i].round;
            vDelta *= 2;
            hOffset += hDelta;
            vOffset = vStart + vDelta / 2;
        }

        if( rwTopology[i].player1 && rwTopology[i].player2 == 0 &&
                        rwTopology[i].winner )
        {
            // If a player is playing nobody and they won, that's a bye.
            swprintf( buffer, L"%s\ngets a bye", rwTopology[i].p1_gamertag );
        }
        else if( rwTopology[i].player2 && rwTopology[i].player1 == 0 &&
                        rwTopology[i].winner )
        {
            // If a player is playing nobody and they won, that's a bye.
            // Note that when there is a bye the non-zero player could be player 1
            // or player 2.
            swprintf( buffer, L"%s\ngets a bye", rwTopology[i].p2_gamertag );
        }
        else if( rwTopology[i].winner )
        {
            // Otherwise, if we have a winner then this round has been played.
            // Display the winner and loser.
            if( rwTopology[i].player1 == rwTopology[i].winner )
                swprintf( buffer, L"%s\nbeat\n%s", rwTopology[i].p1_gamertag,
                            rwTopology[i].p2_gamertag );
            else
                swprintf( buffer, L"%s\nbeat\n%s", rwTopology[i].p2_gamertag,
                            rwTopology[i].p1_gamertag );
        }
        else
        {
            // Otherwise we have an unplayed match - one or more users may not yet be
            // determined.
            swprintf( buffer, L"%s\nv.s.\n%s", rwTopology[i].p1_gamertag,
                        rwTopology[i].p2_gamertag );
        }

        DWORD dwColor = COLOR_NORMAL;

        // Highlight any round containing the team ID we were given
        if( ( qwTeamID == rwTopology[i].player1 ) || ( qwTeamID == rwTopology[i].player2 ) )
        {
            dwColor = COLOR_HIGHLIGHT;
        }

        font.DrawText( hOffset, vOffset, dwColor, buffer, XBFONT_CENTER_Y );
        vOffset += vDelta;
    }

    DWORD lastMatch = dwTopologyCount - 1;
    if( dwTopologyCount > 0 && rwTopology[lastMatch].winner )
    {
        WCHAR* winnerName = rwTopology[lastMatch].p1_gamertag;
        if( rwTopology[lastMatch].winner == rwTopology[lastMatch].player2 )
            winnerName = rwTopology[lastMatch].p2_gamertag;

        swprintf( buffer, L"%s won!", winnerName );
        font.DrawText( hOffset, 360, 0xffffffff, buffer, XBFONT_CENTER_Y );
    }

    font.SetScaleFactors( 1.0f, 1.0f );
}

//-----------------------------------------------------------------------------
// Name: RenderCompetitorList()
// Desc: Takes the topology and renders a list of competitors excluding
//       the ID if the given team.
//-----------------------------------------------------------------------------
VOID RenderCompetitorList( CXBFont& font,
                           WCHAR* wszCompetitionName,
                           CSEEntrantsCurrentEntrantsQuery& entrantsQuery,
                           DWORD& dwRenderStart,
                           DWORD& dwItemSelected )
{
    assert( wszCompetitionName );

    MENU_LIST rwNameList   = { 0 };
    MENU_LIST rwStatusList = { 0 };

    // Build up a list usable by our stanard rendering code

    for( INT i = 0; i < (INT)entrantsQuery.dwItemsReturned; ++i )
    {
        if( wcslen( entrantsQuery.Results[i].gamertag ) )
        {
            _snwprintf( rwNameList[i], 32,
                        L"%s\0",
                        entrantsQuery.Results[i].gamertag );
        }
        else
        {
            _snwprintf( rwNameList[i], 32,
                        L"%s\0",
                        L"TEAM DELETED" );
        }

        switch( entrantsQuery.Results[i].att_status )
        {
        case XONLINE_COMP_STATUS_ENTRANT_REGISTERED:
            wcscpy( rwStatusList[i], L"REGISTERED" );
            break;

        case XONLINE_COMP_STATUS_ENTRANT_PLAYING:
            wcscpy( rwStatusList[i], L"PLAYING" );
            break;

        case XONLINE_COMP_STATUS_ENTRANT_FORFEIT:
            wcscpy( rwStatusList[i], L"FORFEITED" );
            break;

        case XONLINE_COMP_STATUS_ENTRANT_PASS:
            wcscpy( rwStatusList[i], L"PASS" );
            break;

        case XONLINE_COMP_STATUS_ENTRANT_FINAL:
            wcscpy( rwStatusList[i], L"FINAL" );
            break;

        case XONLINE_COMP_STATUS_ENTRANT_ELIMINATED:
            wcscpy( rwStatusList[i], L"ELIMINATED" );
            break;

        case COMP_ENTRANT_STATUS_EJECTED:
            wcscpy( rwStatusList[i], L"EJECTED" );
            break;

        default:
            wcscpy( rwStatusList[i], L"OTHER" );
        }
    }

    RenderScrollingMenu( font,
                         wszCompetitionName,
                         dwRenderStart,
                         dwItemSelected,
                         entrantsQuery.dwItemsReturned,
                         L"NAME",
                         rwNameList,
                         L"STATUS",
                         rwStatusList,
                         TRUE );
}

//-----------------------------------------------------------------------------
// Name: GetTournamentTopography()
// Desc: Initialize data for displaying the tournament topology.
//-----------------------------------------------------------------------------
ULONGLONG GetTournamentTopography( CSEEntrantsMyCompetitionsQueryResult& searchResult,
                                   CSEEventsTopologyQueryResult* rwTopology,
                                   DWORD& dwTopologyCount )
{
    assert( rwTopology );

    // Zero the count of topology entries.
    dwTopologyCount = 0;

    // Grab the competition ID from the selected entry in the search results.
    ULONGLONG qwCompetitionID = searchResult.att_comp_id;

    if( ( searchResult.att_comp_status == XONLINE_COMP_STATUS_PRE_INIT )
        || ( searchResult.att_comp_status == XONLINE_COMP_STATUS_CANCELED ) )
    {
        // Registration hasn't closed yet, so we can't display the tournament.
        // However we can give the user the option to advance time so that
        // registration has closed. This is a handy debug feature.
        return searchResult.att_comp_status;
    }

    // Get the topology results, one page at a time.
    // Normally a progress bar or other display would be shown at this point.
    // In many cases a single page of results will be sufficient for topologies.
    for( DWORD page = 0; ; ++page )
    {
        // The round IDs specify what range of rounds we will query on.
        // Any rounds whose IDs are >= qwStartingEventTopologyID and <= qwEndingEventTopologyID
        // are returned. The rounds and matches are zero based.
        // Normally you would not query the entire competition as that would be too
        // much data.
        const DWORD startRound = 0;
        const DWORD endRound = 3;   // Maximum four rounds for the sample.
        const DWORD startMatch = 0;
        const DWORD endMatch = 3;   // Maximum four matches per round for the sample.

        CSEEventsTopologyQuery query;

        HRESULT hr = query.Query(
                        qwCompetitionID,
                        TOPOLOGY_ID( startRound, startMatch ),
                        TOPOLOGY_ID( endRound, endMatch ),
                        page );

        if( FAILED( hr ) )
        {
            return searchResult.att_comp_status;
        }

        do
        {
            hr = query.Process();
        }
        while( hr == XONLINETASK_S_RUNNING );

        if( FAILED( hr ) )
            return searchResult.att_comp_status;


        // Make sure we are getting reasonable results.
        assert( query.dwItemsReturned <= query.dwTotalItemsInResult );

        // Copy the results of this particular query into our own buffer
        for( UINT i = 0; i < query.dwItemsReturned; ++i )
        {
            memcpy( rwTopology + dwTopologyCount, &query.Results[i], 
                    sizeof( CSEEventsTopologyQueryResult ) );
            ++dwTopologyCount;
        }

        // If we have all the results there are, we might as well stop querying.
        if( dwTopologyCount == query.dwTotalItemsInResult )
            break;
    }

    return searchResult.att_comp_status;
}

//-----------------------------------------------------------------------------
// Name: GetRoundOpponent()
// Desc: Returns the ID of the opponent the given team will face
//       in their current competition round. Returns 0x0 if no
//       opponent will be faced ( competition over, team elimintated, etc ).
//-----------------------------------------------------------------------------
ULONGLONG GetRoundOpponent( CSEEventsTopologyQueryResult* rwTopology,
                            DWORD dwTopologyCount,
                            ULONGLONG qwTeamID,
                            DWORD& dwEventIndex,
                            ULONGLONG& qwEntityID )
{
    // Normally a user would find out about matches that they need to play via
    // reminders. In this artificial scenario we are getting the topology and
    // then logging in the users who need to play the next round.
    for( dwEventIndex = 0; dwEventIndex < dwTopologyCount; ++ dwEventIndex )
    {
        // Look for a round that hasn't been played
        // and we are a part of
        if( rwTopology[dwEventIndex].winner == 0 )
        {
            if( rwTopology[dwEventIndex].player1 == qwTeamID )
            {
                qwEntityID = rwTopology[dwEventIndex].bi_entity_id;

                return rwTopology[dwEventIndex].player2;
            }

            if( rwTopology[dwEventIndex].player2 == qwTeamID )
            {
                qwEntityID = rwTopology[dwEventIndex].bi_entity_id;

                return rwTopology[dwEventIndex].player1;
            }
        }
    }

    // Unable to find round for this team!
    return 0x0;
}