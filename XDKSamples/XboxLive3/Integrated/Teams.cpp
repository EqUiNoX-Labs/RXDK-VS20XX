//-------------------------------------------------------------------------------------
// File: Teams.cpp
//
// Desc: Contains code used to manage teams.
//
// Hist: 12.09.04 - New for January release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#include <assert.h>
#include <xtl.h>
#include <xonline.h>
#include "XBRandName.h"
#include "XBStopWatch.h"

#include "Teams.h"
#include "Common.h"

//-------------------------------------------------------------------------------------
// Name: ChangeTeamProperties()
// Desc: Changes the name and description of the team selected by the user
//       to have new random information.
//       Returns FALSE if the operation fails.
//-------------------------------------------------------------------------------------
BOOL ChangeTeamProperties( DWORD dwControllerPort,
                           XONLINE_TEAM& team )
{
    // Step 1
    //
    // Create a new team properties structure
    // holding the updated information

    XONLINE_TEAM_PROPERTIES newTeamProps;

    ZeroMemory( &newTeamProps, sizeof( newTeamProps ) );

    newTeamProps.TeamDataSize = team.TeamProperties.TeamDataSize;

    lstrcpynW( newTeamProps.wszURL,
               L"http://www.xbox.com",
               XONLINE_MAX_TEAM_URL_SIZE );

    memcpy( newTeamProps.TeamData,
            team.TeamProperties.TeamData,
            newTeamProps.TeamDataSize );

    // Generate new and random team properties
    XBRandName_GetRandomName( newTeamProps.wszTeamName,
        XONLINE_MAX_TEAM_NAME_SIZE );

    XBRandName_GetRandomName( newTeamProps.wszDescription,
        XONLINE_MAX_TEAM_DESCRIPTION_SIZE );

    XBRandName_GetRandomName( newTeamProps.wszMotto,
        XONLINE_MAX_TEAM_MOTTO_SIZE );


    // Step 2
    //
    // Submit the new changed data to the XBox Live service

    CXBOnlineTask hTeamChangeTask;

    HRESULT hrTeamPropChange = XOnlineTeamSetProperties(
        dwControllerPort,   // User/controller requisting the change
        team.xuidTeam,      // XUID of the team to modify
        &newTeamProps,      // Updated properties
        NULL,               // An event
        &hTeamChangeTask ); // The task to assign to this action

    if( FAILED( hrTeamPropChange ) )
        return FALSE;


    // Step 3
    //
    // Pump the task until it is finished
    WaitForTaskToComplete( hTeamChangeTask, &hrTeamPropChange );


    // Step 4
    //
    // The task has finished and succeeded

    hTeamChangeTask.Close();

    return SUCCEEDED( hrTeamPropChange );
}

//-------------------------------------------------------------------------------------
// Name: SendTeamInvite()
// Desc: Sends an invitation to another user to join the given team
//-------------------------------------------------------------------------------------
HRESULT SendTeamInvite( DWORD dwControllerPort,
                        XUID xuidSender,
                        XUID xuidTeam,
                        XUID xuidNewRecruit )
{
    XONLINE_TEAM_MEMBER_PROPERTIES  teamMemberProps;
    CXBOnlineTask                   hInviteTask;

    // Do not reinvite ourselves to be a team member
    // of a team we already are a member of
    if( xuidSender.qwUserID
        == xuidNewRecruit.qwUserID )
    {
        return XONLINE_E_TEAMS_SELF;
    }


    // Step 1
    //
    // To recruit a member, you need to create a XONLINE_TEAM_MEMBER_PROPERTIES structure
    // for them.  This includes title specific data and a privilege level.
    // The privilege level is only enforced by the title

    ZeroMemory( &teamMemberProps, sizeof( teamMemberProps ) );

    teamMemberProps.dwPrivileges       = XONLINE_TEAM_RECRUIT_MEMBERS;
    teamMemberProps.TeamMemberDataSize = 0;

    // XOnlineTeamMemberRecruit sends a message to the recruited player 
    // inviting them to join the team.
    // you can get a message handle back here if you want to add custom message properties

    HRESULT hrInvite = XOnlineTeamMemberRecruit(
                dwControllerPort, // The controller port of the user requesting
                xuidTeam,         // The XUID of the team to recruit for
                xuidNewRecruit,   // The XUID of the player we want to recruit
                &teamMemberProps, // Initial properties of the new recruit
                NULL,             // The handle of a message we created with
                                  // XOnlineMessageCreate. Sending NULL
                                  // attaches a default message.
                NULL,             // A work event
                &hInviteTask      // The task handle for this job
            );

    if( FAILED( hrInvite ) )
        return hrInvite;


    // Step 2
    //
    // Pump the task until the invites is finished being sent

    WaitForTaskToComplete( hInviteTask, &hrInvite );


    // Step 3
    //
    // Close the task and return our
    // success (or failure) code

    hInviteTask.Close();

    return hrInvite;
}

//-------------------------------------------------------------------------------------
// Name: ProcessInvite
// Desc: Returns a response to the invite with the given index
//       Passing eAnswer=XONLINE_PEER_ANSWER_YES will accept the invite
//       Passing eAnswer=XONLINE_PEER_ANSWER_NO will decline the invite
//       Passing eAnswer=XONLINE_PEER_ANSWER_NEVER will decline the invite and all
//       future invites from that team
//-------------------------------------------------------------------------------------
HRESULT ProcessInvite( DWORD dwControllerPort,
                       XONLINE_MSG_SUMMARY msgSummary,
                       XONLINE_PEER_ANSWER_TYPE eAnswer )
{
    // If this is not a team message, ignore it
    if( !( msgSummary.dwMessageFlags & XONLINE_MSG_FLAG_TEAM_CONTEXT ) )
        return E_FAIL;

    // Only deal with a message trying to recuit us
    if( msgSummary.bMsgType != XONLINE_MSG_TYPE_TEAM_RECRUIT )
        return E_FAIL;


    // Step 1
    //
    // Fill in the response
    // The response is just the team id that we are
    // accepting the invitation for

    CXBOnlineTask hAcceptInviteTask;
    XUID          teamXUID;

    teamXUID.qwTeamID    = msgSummary.qwMessageContext;
    teamXUID.dwUserFlags = 0;

    // Send the actual response
    HRESULT hrAcceptInvite = XOnlineTeamMemberAnswerRecruit(
                                    dwControllerPort,  // Controller port of the user answering
                                    teamXUID,          // The XUID of the team we are 
                                                       // accepting the invitation for
                                    eAnswer,           // The answer (YES, NO, NEVER)
                                    NULL,              // Any event
                                    &hAcceptInviteTask // The task that this is
                                );

    if( FAILED( hrAcceptInvite ) )
        return hrAcceptInvite;


    // Step 2
    //
    // Continue the task until it is complete
    // A real title should not block for this
    // operations

    WaitForTaskToComplete( hAcceptInviteTask, &hrAcceptInvite );


    // Step 3
    //
    // Close the task and return S_OK

    hAcceptInviteTask.Close();

    // Sleep a little so we will not see the invite we just accepted
    // in the inbox again
    Sleep( 1000 );

    return hrAcceptInvite;
}

//-------------------------------------------------------------------------------------
// Name: CreateTeam()
// Desc: Attempts to create a team with a random name and description.
//       The user controlling the UI is set as the creator of the team.
//       Successfully creating a team returns S_OK, otherwise and error is returned.
//-------------------------------------------------------------------------------------
HRESULT CreateTeam( DWORD dwControllerPort,
                    XONLINE_TEAM_PROPERTIES& createdTeam )
{
    // Step 1
    //
    // Fill in the details of the team we want to
    // create and make ourselves the owner of

    XONLINE_TEAM_MEMBER_PROPERTIES teamMemberProperties[1];
    CXBOnlineTask                  hTeamCreateTask;

    XBRandName_GetRandomName( createdTeam.wszTeamName,     XONLINE_MAX_TEAM_NAME_SIZE );
    XBRandName_GetRandomName( createdTeam.wszDescription,  XONLINE_MAX_TEAM_DESCRIPTION_SIZE );
    XBRandName_GetRandomName( createdTeam.wszMotto,        XONLINE_MAX_TEAM_MOTTO_SIZE );

    lstrcpynW( createdTeam.wszURL, L"http://www.xbox.com", XONLINE_MAX_TEAM_URL_SIZE );

    createdTeam.TeamDataSize   = 0; // Team Data is optional
    ZeroMemory( createdTeam.TeamData, sizeof( createdTeam.TeamData ) );

    // Give all the priviledges to the team creator
    teamMemberProperties[0].dwPrivileges   = ( XONLINE_TEAM_DELETE |
                                               XONLINE_TEAM_MODIFY_DATA |
                                               XONLINE_TEAM_MODIFY_MEMBER_PERMISSIONS |
                                               XONLINE_TEAM_DELETE_MEMBER |
                                               XONLINE_TEAM_RECRUIT_MEMBERS );

    teamMemberProperties[0].TeamMemberDataSize = 0;
    ZeroMemory( teamMemberProperties[0].TeamMemberData, 
                sizeof( teamMemberProperties[0].TeamMemberData ) );


    // Step 2
    //
    // Send the new team to Xbox Live

    HRESULT hrTeamCreate = XOnlineTeamCreate(
                                dwControllerPort,     // Index of the controller of the 
                                                      // user creating the team
                                &createdTeam,         // Properties of the team
                                teamMemberProperties, // Properties of the initial team member
                                MAX_TEAM_SIZE,        // Maximum number of people on roster
                                NULL,                 // Event to keep track
                                &hTeamCreateTask );   // Task to keep track

    if( FAILED( hrTeamCreate ) )
        return hrTeamCreate;


    // Step 3
    //
    // Continue the task until it is completed

    if( !WaitForTaskToComplete( hTeamCreateTask, &hrTeamCreate ) )
    {
        // Failed, return the specific error
        // so the UI can display a nice error screen
        return hrTeamCreate;
    }
    else
    {
        // Step 4
        //
        // Get the resulting team information back from
        // Xbox Live

        XONLINE_TEAM teamData;

        hrTeamCreate = XOnlineTeamCreateGetResults(
                            hTeamCreateTask, // The task used to create the team
                            &teamData        // The data of the team just created
                       );

        // If we failed, then let the
        // UI know the specific cause for
        // an information window to be shown
        if( FAILED( hrTeamCreate ) )
            return hrTeamCreate;
    }


    // Step 5
    //
    // Close the task and return our success

    hTeamCreateTask.Close();

    return hrTeamCreate;
}

//-------------------------------------------------------------------------------------
// Name: RemoveTeamMember()
// Desc: Kicks the player with the given XUID of the team with the given XUID.
//       Returns S_OK if it succeeded, otherwise returns the specific error.
//-------------------------------------------------------------------------------------
HRESULT RemoveTeamMember( DWORD dwControllerPort,
                          XUID xuidTeam,
                          XUID xuidMember )
{
    // Step 1
    //
    // Start the deletion task for the member we want to remove

    CXBOnlineTask hTeamRemoveMembersTask;

    HRESULT hrRemoveMembers = XOnlineTeamMemberRemove(
                                // The controller port of the user starting the action
                                dwControllerPort,
                                // The XUID of the team with the member being removed
                                xuidTeam,
                                // The XUID of the team member being removed
                                xuidMember,
                                // Any work event
                                NULL,
                                // The task of the deletion process
                                &hTeamRemoveMembersTask );

    if( FAILED( hrRemoveMembers ) )
    {
        return hrRemoveMembers;
    }


    // Step 2
    //
    // Continue until the deletion task is finished

    WaitForTaskToComplete( hTeamRemoveMembersTask, &hrRemoveMembers );


    // Step 3
    //
    // Close the task and return our success

    hTeamRemoveMembersTask.Close();

    return hrRemoveMembers;
}

//-------------------------------------------------------------------------------------
// Name: DeleteTeam()
// Desc: Removes the entire team from Xbox Live. This removes all players from the team,
//       wipes the team name out of the service.
//       Returns S_OK if it succeeded, otherwise returns the specific error.
//-------------------------------------------------------------------------------------
HRESULT DeleteTeam( DWORD dwControllerPort,
                    XUID xuidTeam )
{
    // Step 1
    //
    // Start the deletion task

    CXBOnlineTask hTeamDeleteTask;

    HRESULT hrTeamDelete =  XOnlineTeamDelete(
                                dwControllerPort,  // Controller port of the user 
                                                   // starting the action
                                xuidTeam,          // The XUID of the team to delete
                                NULL,              // Any work event
                                &hTeamDeleteTask); // The deletion task

    if( FAILED( hrTeamDelete ) )
        return hrTeamDelete;


    // Step 2
    //
    // Pump the task until it is finished

    WaitForTaskToComplete( hTeamDeleteTask, &hrTeamDelete );


    // Step 3
    //
    // The task is done, so close it
    // and return our success

    hTeamDeleteTask.Close();

    return hrTeamDelete;
}

//-----------------------------------------------------------------------------
// Name: SetPermissions()
// Desc: Sets the permissions of the team member with the given XUID to the
//       new privleges given for the team with the given XUID
//       If the operation fails, then the specific error is returned
//-----------------------------------------------------------------------------
HRESULT SetPermissions( DWORD dwControllerPort,
                        XUID xuidTeam,
                        XUID xuidMember,
                        DWORD dwNewPrivileges )
{
    // Step 1
    //
    // Start the permissions change task for the member we want to remove

    CXBOnlineTask hPermissionTask;

    XONLINE_TEAM_MEMBER_PROPERTIES newMemberProps;

    ZeroMemory( &newMemberProps, sizeof( newMemberProps ) );

    // The new privleges of the member
    newMemberProps.dwPrivileges       = dwNewPrivileges;
    newMemberProps.TeamMemberDataSize = 0;

    HRESULT hrChangePermissions = XOnlineTeamMemberSetProperties(
                                        // The controller port of the user 
                                        // requesting the change
                                        dwControllerPort,
                                        // The XUID of the team with the member being changed
                                        xuidTeam,
                                        // The XUID of the member being changed
                                        xuidMember,
                                        // The new properties the team member will have
                                        &newMemberProps,
                                        // A work event
                                        NULL,
                                        // The permission changing task
                                        &hPermissionTask );


    // We still may be able to change other users
    // permissions if we fail
    if( FAILED( hrChangePermissions ) )
        return hrChangePermissions;


    // Step 2
    //
    // Continue until the deletion task is finished

    WaitForTaskToComplete( hPermissionTask, &hrChangePermissions );


    // Step 3
    //
    // Finished! Close the task and return success

    hPermissionTask.Close();

    return hrChangePermissions;
}

//-----------------------------------------------------------------------------
// Name: GetStatIDs()
// Desc: Return an array of ids for the stats maintained internally
//-----------------------------------------------------------------------------
PWORD GetStatIDs( DWORD *pdwNumStats )
{

    // Stat attribute IDs.  These must match the number and order of
    // ids in the Stats array.

    static WORD StatIDs[STAT_MAX] =
    {
        STAT_ID_KILLS,
        STAT_ID_DEATHS,
        STAT_ID_ASSISTS,
        STAT_ID_ACCURACY,
        XONLINE_STAT_RATING,
        XONLINE_STAT_RANK
    };

    *pdwNumStats = STAT_MAX;

    return StatIDs;
}

//-----------------------------------------------------------------------------
// Name: WriteUnitStatistics()
// Desc: Write unit statistics to a leaderboard
//
// Note: Attempting to write team statistics several times in
//       quick succession may cause the operation to cause a
//       throttling error.
//       The function returns the number of kills, deaths and assists added
//       to the team score along with with the rating added to the team.
//-----------------------------------------------------------------------------
BOOL WriteTeamStatistics( XUID xuidTeam,
                          LONG& lKills,
                          LONG& lDeaths,
                          LONG& lAssists,
                          LONGLONG& llRating )
{

    // This sample maintains a single leaderboard with the following
    // attributes:
    //              Kills           The number of kills in a session
    //              Deaths          The number of times the player died
    //              Assists         The number of assists the player provided
    //              Accuracy        Shooting accuracy (0 - 100%)


    // When writing statistics for a unit,
    // a title supplies a rating to the Live Service, which
    // is then used to rank that gamer against others on the leaderboard.
    // The service, and not the title, assigns the actual rank for
    // the gamer.
    //

    // The XONLINE_STAT structure is used to represent a piece
    // of statistical information associated with a leaderboard
    // entry.  This sample will write five pieces of information
    // for a leaderboard entry.  This information is specified
    // by passing 'teamAttributes',  an array of XONLINE_STAT structures,
    // to the XOnlineStatWrite function.  This array is indexed by the
    // following enumeration:

    XONLINE_STAT teamAttributes[NUM_STATS_SUBMITTED];


    // The XOnlineStatWriteEx function is used to write statistical
    // information.
    //
    // For demonstration purposes, this sample will write statistics for
    // a unit user to a single leaderboard, using the
    // XONLINE_STAT_PROCID_UPDATE_REPLACE procedure to completely replace
    // existing statistics with new values.  For some attributes, such as
    // kills or deaths, titles would actually want to use the
    // XONLINE_STAT_PROCID_UPDATE_INCREMENT procedure instead to add to the
    // existing values (or subtract, if the increment is negative).  Titles
    // may also choose to update players' ratings via the Elo scoring
    // system by using the XONLINE_STAT_PROCID_ELO procedure.  Powerful
    // server-evaluated 'if then' statements are even possible by using
    // XONLINE_STAT_PROCID_CONDITIONAL procedures.  If using conditionals,
    // the procedure(s) that depend on one MUST come after it in the array,
    // since all procedures are processed in order by the server.  The
    // dependent Replace, Increment or Elo procedure would then specify the
    // one-based index into the array of that previous conditional in the
    // dwConditionalIndex field in the appropriate union structure.
    // Procedures that are not dependent upon conditionals should specify 0,
    // like in this sample.
    //
    // The statistics service requires that the users be signed onto the
    // console from which write requests are made.  To meet this
    // requirement, this sample will treat all signed on users as
    // a unit and write stats on their behalf.


    // The XOnlineStatWritEx function takes an array of XONLINE_STAT_PROC
    // structures.  Each of these entries specifies the unit members, the
    // leaderboard (by ID), and an array on XONLINE_STAT
    // structures, which contain the information for that entry:
    //
    // +-------------------+      +----------------+----------------+
    // | XONLINE_STAT_PROC |      |  XONLINE_STAT  |  XONLINE_STAT  |
    // |     xuidMembers   |      |      wID       |      wID       |
    // |      pStats ------+----->|      type      |      type      |...
    // |    dwNumStats     |      |   union value  |   union value  |
    // |  dwLeaderBoardID  |      +----------------+----------------+
    // +-------------------+
    //
    // Since this sample only writes statistics for a single unit and
    // a single leaderboard, there is only one element in the XONLINE_STAT_PROC
    // array 'StatProc'.

    XONLINE_STAT_PROC StatProc;

    ZeroMemory( &StatProc, sizeof( StatProc ) );

    // Compute some random statistics
    lKills       = rand() % 20;
    lDeaths      = rand() % 20;
    lAssists     = rand() % 20;

    // The rating is just a 64-bit number.  The larger the number,
    // the better the rating.  This sample, uses the following
    // formula for calculating the rating.
    llRating     = 100*lKills + 10*lAssists - 5*lDeaths;

    DOUBLE   dAccuracy    = DOUBLE( rand() % 101 );

    StatProc.wProcedureID = XONLINE_STAT_PROCID_UPDATE_INCREMENT;

    StatProc.Update.xuid               = xuidTeam;
    StatProc.Update.dwLeaderBoardID    = LEADERBOARD_ID;
    StatProc.Update.dwConditionalIndex = 0;
    StatProc.Update.dwNumStats         = NUM_STATS_SUBMITTED;
    StatProc.Update.pStats             = teamAttributes;

    // Populate the teamAttributes array with the statistical information
    // The XONLINE_STAT structure needs three pieces of information:
    //
    // * The ID of the attribute.  This is a 16 bit unsigned value.
    //   Avoid larger values (0xF000 and above)
    //   since those values are reserved.
    //
    // * The Type of the attribute.  The type can be:
    //     XONLINE_STAT_LONG      a 32-bit integer
    //     XONLINE_STAT_LONGLONG  a 64-bit integer
    //     XONLINE_STAT_DOUBLE    a 64-bit (IEEE) real number
    //
    // * The Data.  The XONLINE_STAT contains an anonymous union
    //

    // Specify the Kills attribute (an integer)
    teamAttributes[ STAT_KILLS ].wID    = STAT_ID_KILLS;
    teamAttributes[ STAT_KILLS ].type   = XONLINE_STAT_LONG;
    teamAttributes[ STAT_KILLS ].lValue = lKills;

    // Specify the Deaths attribute (an integer)
    teamAttributes[ STAT_DEATHS ].wID    = STAT_ID_DEATHS;
    teamAttributes[ STAT_DEATHS ].type   = XONLINE_STAT_LONG;
    teamAttributes[ STAT_DEATHS ].lValue = lDeaths;

    // Specify the Assists attribute (an integer)
    teamAttributes[ STAT_ASSISTS ].wID    = STAT_ID_ASSISTS;
    teamAttributes[ STAT_ASSISTS ].type   = XONLINE_STAT_LONG;
    teamAttributes[ STAT_ASSISTS ].lValue = lAssists;

    // Specify the Accuracy attribute (a real number)
    teamAttributes[ STAT_ACCURACY ].wID    = STAT_ID_ACCURACY;
    teamAttributes[ STAT_ACCURACY ].type   = XONLINE_STAT_DOUBLE;
    teamAttributes[ STAT_ACCURACY ].dValue = dAccuracy;

    // Finally, specify the rating for entry using the
    // reserved XONLINE_STAT_RATING attribute id.  The rating
    // is a 64-bit integer, so XONLINE_STAT_LONGLONG is specified
    // for the attribute type.
    teamAttributes[ STAT_RATING ].wID     = XONLINE_STAT_RATING;
    teamAttributes[ STAT_RATING ].type    = XONLINE_STAT_LONGLONG;
    teamAttributes[ STAT_RATING ].llValue = llRating;


    // Initiate the writing process.  This will return a task handle
    CXBOnlineTask hWriteTask;

    HRESULT hr = XOnlineStatWriteEx(
                        1,          // Number of teams we are writing for
                        &StatProc,  // The updated stats
                        NULL,       // Events
                        &hWriteTask // Task
                  );

    if( FAILED( hr ) )
        return FALSE;

    // Service the write task until complete.  The title must also service
    // the logon task as well
    WaitForTaskToComplete( hWriteTask, &hr );

    hWriteTask.Close();

    return SUCCEEDED( hr );
}

//-----------------------------------------------------------------------------
// Name: GetTeamLeaderboard
// Desc: Attempts to read a leaderboard from the Statistics service
//       Returns FALSE if it fails
//
// Note: Attempting to read the leaderboard several times in
//       quick succession may cause the operation to cause a
//       throttling error
//-----------------------------------------------------------------------------
BOOL GetTeamLeaderboard( XONLINE_STAT_USER* rwLeaderboardUsers,
                         XONLINE_STAT* rwLeaderboardStats,
                         DWORD& dwNumLeaderboardUsers )
{
    // Step 1
    //
    // Start the enumeration process to get the entire leader board

    CXBOnlineTask hLeaderboardTask;
    DWORD         dwNumStats       = 0;
    PWORD         pwStatsPerTeam   = GetStatIDs( &dwNumStats );

    dwNumLeaderboardUsers = 0;

    HRESULT hrLeaderEnumerate = XOnlineStatLeaderEnumerate(
                                NULL,             // XUID of a team we want to include
                                                  // in the leader board. Providing
                                                  // NULL will simply retrieve only
                                                  // the top rated entries
                                1,                // The number of leader boards 
                                                  // to retrieve
                                MAX_STAT_USERS,   // The number of teams to 
                                                  // retrieve PER leaderboard
                                LEADERBOARD_ID,   // The ID of the leaderboard to retrieve
                                dwNumStats,       // The total number of statistics 
                                                  // to retrieve
                                                  // This is should be equal to the number of
                                                  // teams TIMES the number of stats per team
                                pwStatsPerTeam,   // Array of IDs to the statistics we wish to
                                                  // retrieve
                                NULL,             // A work event
                                &hLeaderboardTask // The stat retrieval task
                                );

    if( FAILED( hrLeaderEnumerate ) )
        return FALSE;


    // Step 2
    //
    // Continue existing enumeration until
    // the task is finished

    do
    {
        RenderWorkingScreen();

        hrLeaderEnumerate = hLeaderboardTask.Continue();

        if( FAILED( hrLeaderEnumerate ) )
            return FALSE;
    }
    while( hrLeaderEnumerate == XONLINETASK_S_RUNNING );


    // Step 3
    //
    // Get the enumerated results and fill in our
    // data structures

    DWORD dwLeaderboardSize = 0;

    dwNumStats = 0;
    (VOID) GetStatIDs( &dwNumStats );

    ZeroMemory( rwLeaderboardUsers, sizeof( rwLeaderboardUsers ) );
    ZeroMemory( rwLeaderboardStats, sizeof( rwLeaderboardStats ) );

    INT iStatSize = dwNumStats * MAX_STAT_USERS;

    // Obtain the results of the enumeration.  Note that the XONLINE_STAT
    // attributes for alls users are stored in the Stats array.  If there
    // are N attributes, the first N elements are the attributes for the
    // first user, the second N elements are the attributes for the
    // second user.  The attributes for user "i" is at N*i.
    hrLeaderEnumerate = XOnlineStatLeaderEnumerateGetResults(
                            hLeaderboardTask,       // The task that started the read
                            MAX_STAT_USERS,         // Maximum number of teams to return
                            rwLeaderboardUsers,     // Names of the teams in the board
                            iStatSize,              // The total number of stats
                            rwLeaderboardStats,     // The array of statistics
                            &dwLeaderboardSize,     // The TOTAL size of the leader board
                            &dwNumLeaderboardUsers, // The number of results returned
                            0,                      // The size of the extra read buffer
                            NULL                    // Pointer to extra read buffer
                         );

    hLeaderboardTask.Close();

    if( FAILED( hrLeaderEnumerate ) || ( dwNumLeaderboardUsers < 1 ) )
        return FALSE;

    // Make sure that we got the proper results back
    assert( iStatSize >= (INT)( dwNumLeaderboardUsers * dwNumStats ) );

    // Keep this from getting throttled and
    // throwing an exception
    CXBStopWatch throttlePreventionTimer;
    throttlePreventionTimer.StartZero();

    do
    {
        RenderWorkingScreen();
    }
    while( throttlePreventionTimer.GetElapsedSeconds() < 2.0f );

    return TRUE;
}

//-----------------------------------------------------------------------------
// Name: GetTeamList()
// Desc: Attempts to get a list of the current user's teams from the
//       Xbox Live service. Returns FALSE if it fails.
//
// Note: Attempting to read the team list several times in
//       quick succession may cause the operation to cause a
//       throttling error
//-----------------------------------------------------------------------------
BOOL GetTeamList( DWORD dwControllerPort,
                  XUID xuidUser,
                  LPDIRECT3DTEXTURE8* &ppTeamLogoTextures,
                  DWORD& dwTeamLogoToDL,
                  XUID* rwTeamXUIDS,
                  XONLINE_TEAM* rwTeamInfo,
                  DWORD& dwTeamCount )
{
    // Delete and initialize the texture array

    if( ppTeamLogoTextures )
        delete [] ppTeamLogoTextures;

    ppTeamLogoTextures = NULL;
    dwTeamLogoToDL     = 0;


    // Step 1
    //
    // Start the task to enumerate the teams list
    // that the current user is a member of

    CXBOnlineTask hViewMyTeamsTask;

    HRESULT hrTeamFind = XOnlineTeamEnumerateByUserXUID(
        dwControllerPort,
        xuidUser,
        NULL,
        &hViewMyTeamsTask );


    // Unable to start enumeration task
    if( FAILED( hrTeamFind ) )
    {
        hViewMyTeamsTask.Close();

        return FALSE;
    }


    // Step 2
    //
    // Continue the task until complete

    if(! WaitForTaskToComplete( hViewMyTeamsTask, &hrTeamFind ) )
    {
        hViewMyTeamsTask.Close();

        return FALSE;
    }


    // Step 3
    //
    // Get the results from the finished task
    // and populate the list

    dwTeamCount = 0;

    ZeroMemory( rwTeamXUIDS, sizeof( rwTeamXUIDS ) );

    hrTeamFind = XOnlineTeamEnumerateGetResults(
                    hViewMyTeamsTask, // The enumeration task
                    &dwTeamCount,     // The number of teams read
                    rwTeamXUIDS       // An array of all the team XUIDs read
                 );

    // Unable to get results
    if( FAILED( hrTeamFind ) )
    {
        hViewMyTeamsTask.Close();

        return FALSE;
    }


    // Step 4
    //
    // Now that we have the XUIDs of all the teams
    // the current user is a member of, we have to go
    // and create tasks to fill all the information about
    // each team so we can display
    //
    // NOTE: A real title would only have to display
    // the team details if the user wanted

    ZeroMemory( rwTeamInfo, sizeof( rwTeamInfo ) );

    for( INT i = 0; i < (INT)dwTeamCount; ++i)
    {
        HRESULT hrTeamDetails = XOnlineTeamGetDetails(
                                    hViewMyTeamsTask, // The task that started the team read
                                    rwTeamXUIDS[i],   // The list of XUIDs that we want 
                                                      // details of
                                    &rwTeamInfo[i]    // The array of details to be populated
                                );

        // Unable to get team details
        if( FAILED( hrTeamDetails ) )
        {
            hViewMyTeamsTask.Close();

            return FALSE;
        }
    }


    // Step 5
    //
    // Close the task, create a new texture array
    // and return our success

    ppTeamLogoTextures = new LPDIRECT3DTEXTURE8[dwTeamCount];
    assert( ppTeamLogoTextures );

    if( ppTeamLogoTextures )
        ZeroMemory( ppTeamLogoTextures, sizeof( LPDIRECT3DTEXTURE8 ) * dwTeamCount );

    hViewMyTeamsTask.Close();

    return TRUE;
}

//-----------------------------------------------------------------------------
// Name: GetTeamRoster()
// Desc: Attempts to get a Roster of the current user's selected team from the
//       Xbox Live service. Returns FALSE if it fails.
//
// Note: Attempting to read the team roster several times in
//       quick succession may cause the operation to cause a
//       throttling error
//-----------------------------------------------------------------------------
BOOL GetTeamRoster( DWORD dwControllerPort,
                    CXBOnlineTask& phTeamRosterTask,
                    XUID xuidTeam,
                    XUID* rwTeamMembers,
                    DWORD& dwTeamMemberCount )
{
    // Step 1
    //
    // Start the read of the roster

    if( phTeamRosterTask.IsOpen() )
        phTeamRosterTask.Close();

    // Setting to XONLINE_TEAM_SHOW_RECRUITS
    // will return "members" who have not
    // yet accepted an invitation
    //
    // Setting to 0 will only show
    // members who have accepted an
    // invitation to join the team

    DWORD dwEnumerationFlags = XONLINE_TEAM_SHOW_RECRUITS;

    HRESULT hrTeamRoster = XOnlineTeamMembersEnumerate(
            dwControllerPort,    // Controller index (zero-based) of the
                                 // user making the request.
            xuidTeam,            // XUID structure that uniquely identifies the team.
            dwEnumerationFlags,  // Flags indicating how the team members
                                 // should be enumerated.
                                 // XONLINE_TEAM_SHOW_RECRUITS Indicates that recruits should
                                 // be included in the returned results.
            NULL,                // Handle to an event (OPTIONAL)
            &phTeamRosterTask ); // Pointer to an XONLINETASK_HANDLE returned

    if( FAILED( hrTeamRoster ) )
        return FALSE;


    // Step 2
    //
    // Continue until the task is complete

    if(! WaitForTaskToComplete( phTeamRosterTask, &hrTeamRoster ) )
    {
        return FALSE;
    }


    // Step 3
    //
    // Now that the task is finished, get the results
    // and return our success

    dwTeamMemberCount = 0;
    ZeroMemory( rwTeamMembers, sizeof( rwTeamMembers ) );

    hrTeamRoster = XOnlineTeamMembersEnumerateGetResults(
                        phTeamRosterTask,   // The task that read the roster
                        &dwTeamMemberCount, // The number of team members
                        rwTeamMembers );    // The list of XUIDs to be populated

    return SUCCEEDED( hrTeamRoster );
}
