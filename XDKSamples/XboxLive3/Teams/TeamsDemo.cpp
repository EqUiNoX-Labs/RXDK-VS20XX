//-------------------------------------------------------------------------------------
// File: TeamsDemo.cpp
//
// Desc: This sample demonstrates how to create and manage
//       teams. Teams are a usefull way to create a community
//       with Xbox Live. Players can create teams and invite
//       friends to join. Teams may have their own statistics
//       and rankings.
//
// Hist: 08.10.04 - New for Sept release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#include <algorithm>
#include <xtl.h>
#include <xonline.h>
#include "XBFont.h"
#include "XBMemUnit.h"
#include "XBVoice.h"
#include "TeamsDemo.h"

//-------------------------------------------------------------------------------------
// Name: BootToDash()
// Desc: Boot back into either the main or online dashes
//-------------------------------------------------------------------------------------
VOID BootToDash( DWORD dwReason )
{
    LD_LAUNCH_DASHBOARD ld;
    ZeroMemory( &ld, sizeof(ld) );
    ld.dwReason = dwReason;
    XLaunchNewImage( NULL, PLAUNCH_DATA( &ld ) );
    // XLaunchNewImage should never return
    assert( FALSE );
}

//-------------------------------------------------------------------------------------
// Name: WaitForTaskToComplete()
// Desc: Helper function for waiting for a task to complete
//       In a real game, this would be part of your game loop
//       Returns TRUE for success, FALSE for failure.
//       Optionally returns the results code in pHR
//-------------------------------------------------------------------------------------
BOOL WaitForTaskToComplete( CXBOnlineTask& Task, HRESULT* pHR )
{
    assert( pHR );
    HRESULT hrTask = XONLINETASK_S_RUNNING;

    while( hrTask == XONLINETASK_S_RUNNING )
    {
        // In a real game, this would be part of your game loop-
        // you wouldn't block on this.
        // The logon task should also be pumped inside this loop.

        hrTask = Task.Continue();
        if( FAILED( hrTask ))
        {
            *pHR = hrTask;
            return FALSE;
        }
        
        // put in a delay of about 1 frame so as not to
        // spam with MessageSendGetProgress
        Sleep( 15 );
    }
    
    if( hrTask != XONLINETASK_S_SUCCESS )
    {
        *pHR = hrTask;
        return FALSE;
    }

    *pHR = S_OK;
    return TRUE;
}

//////////////////////////////////
// CXBoxSample Member Functions //
//////////////////////////////////

//-------------------------------------------------------------------------------------
// Name: StartSignIn()
// Desc: Attempts to sign in the player at the given index to the XLogonUsers
//       array. Returns an error stating the type of problem (network
//       or account) encountered if the signon process fails.
//       Once StartSignIn() has been called, the task must be finished
//       by ContinueSignIn() and FinishSignIn()
//-------------------------------------------------------------------------------------
INT CXBoxSample::StartSignIn()
{
    // NOTE:
    // Before signing on, the title must check accounts for
    // passcodes. If present, the players must be prompted for them.
    // Passcodes are for *client-side* authentication only -- the
    // Xbox online service does not use them for authentication. For
    // demonstration purposes, we just make note of any passcode, and continue.
    // (The 'passcode' field of the XONLINE_USER structure contains the actual
    // passcode).
    #pragma message("TCR: Title UI must prompt for a passcode and verify it\
 before signing on.")

    // Initiate the authentication process.  The signon process
    // first authenticates the Xbox.  Next, it authenticates each
    // user, and finally authenticates against the requested services
    // (validating that both the users *and* the Xbox have access to them).
    // All three stages are handled by the client APIs, though the title
    // is required to check for errors and handle them appropriately.
    XONLINE_USER rwLogonUsers[ XONLINE_MAX_LOGON_USERS ]; // Initially zeroed

    ZeroMemory( rwLogonUsers, sizeof( rwLogonUsers ) );

    // Loop through to find all the local users that want to log in
    for( WORD wUser = 0; wUser < MAX_USERS; ++wUser )
    {
        if( m_bUserSelectedAccount[wUser] )
            rwLogonUsers[wUser] = m_rwStoredUsers[m_localUsers[wUser].m_wUserIndex];
    }

    HRESULT hrLogon = XOnlineLogon(
                            rwLogonUsers, // Array of users to login
                            SERVICES,     // The array of services we want
                            NUM_SERVICES, // The number of services we want
                            NULL,         // Event
                            &m_hLogonTask // The task to assign
                       );

    // Check for errors
    switch( hrLogon )
    {
    case S_OK:   
        // XOnlineLogon succeeded
        break;

    case XONLINE_E_LOGON_NO_NETWORK_CONNECTION:
        // Sign on failed because no network connection was
        // detected.  A title must give the player the
        // option of accessing the network configuration of the online dash
    default:
        return E_NETWORK_ERROR;
    }
    
    return S_OK;
}
    
//-------------------------------------------------------------------------------------
// Name: ContinueSignIn()
// Desc: Attempts to sign in the player to Xbox Live
//       Returns an error stating the type of problem (network or account)
//       encountered if the signon process fails.
//       Once a user has signed on they may create or join a game session.
//-------------------------------------------------------------------------------------
INT CXBoxSample::ContinueSignIn()
{
    // If sucessful, an asynchronous task handle (XONLINETASK_HANDLE) will
    // be returned.  As with the other Xbox online APIs that return
    // task handles, XOnlineTaskContinue() is used to perform a "unit" of
    // work.  The HRESULT returned by calling XOnlineTaskContinue will
    // indicate if additional work is required (XONLINETASK_S_RUNNING) or if
    // the task has failed.  Some of the result codes returned will depend on 
    // the actual type of task being pumped.  However, the SUCCEEDED and
    // FAILED macros can be used for error handling purposes.

    // Go into a loop, calling XOnlineTaskContinue on the logon task
    // until the task completes.  This can take up to a minute or more
    // depending on network conditions.  If successful, 
    // XONLINE_S_LOGON_CONNECTION_ESTABLISHED will be returned.
    // In a real title, this would appear inside your game loop.  

    HRESULT hrLogon = m_hLogonTask.Continue();

    // Check to make sure the logon is proceeding
    // without error, and act on any errors found

    if( hrLogon == XONLINETASK_S_RUNNING )
        return S_OK;
    
    // Check the results to see if we were successful
    // or if the login failed
    switch( hrLogon )
    {
    case XONLINE_S_LOGON_CONNECTION_ESTABLISHED:  
        // The Xbox has been validated and there are
        // no system authentication errors
        return XONLINE_S_LOGON_CONNECTION_ESTABLISHED;
        break;

    case XONLINE_E_LOGON_CONNECTION_LOST:
    case XONLINE_E_LOGON_SERVERS_TOO_BUSY:
        // Some other error - title is free to allow access to dash
        m_hLogonTask.Close();

        m_bUsersSignedIn  = FALSE;
        m_bUsersSigningIn = FALSE;

        return E_NETWORK_ERROR;

    case XONLINE_E_LOGON_CANNOT_ACCESS_SERVICE:
    case XONLINE_E_LOGON_INVALID_USER:
    default:
        // Some other error - title is free to allow access to dash
        m_hLogonTask.Close();
        
        m_bUsersSignedIn  = FALSE;
        m_bUsersSigningIn = FALSE;

        return E_ACCOUNT_ERROR;
    }
}

//-------------------------------------------------------------------------------------
// Name: FinishSignIn()
// Desc: Attempts to finish the SignIn task.
//       Returns an error stating the type of problem (network or account)
//       encountered if the signon process fails.
//       Checks to make sure that the services we need for the game are
//       available and annouces the player's presence onto the network.
//       Once a user has signed on they may create or join a game session.
//-------------------------------------------------------------------------------------
INT CXBoxSample::FinishSignIn()
{
    // Titles must check for, and handle, authentication errors in the
    // following order:
    // 1. System authentication errors (Done by StartSignIn)
    // 2. User authentication errors.
    // 3. Service authentication errors.
    // It important to check for, and handle user errors before service
    // errors.  Consider the case where there is an account maintenance issue 
    // for a user, AND a requested service is unavailable.   A title must
    // allow the user to deal with the account issues before service issues
    // (especially since an account issue could be the cause of
    // the service issue).
    
    // 2. Check for user authentication errors.
    // To check for user authentication errors, we call XOnlineGetLogonUsers.
    // This returns a pointer to an array of XONLINE_USER structures.  This
    // array is similar the User array we populated and passed into
    // XOnlineLogon, but it has the 'hr' field of each XONLINE_USER
    // set with a status code indicating whether or not authentication 
    // for that user succeeded.
    const PXONLINE_USER rwUsers = XOnlineGetLogonUsers();
    
    assert( rwUsers );
    
    for( DWORD i = 0; i < XONLINE_MAX_LOGON_USERS; ++i )
    {
        if( rwUsers[i].xuid.qwUserID != 0 ) // A valid user
        {
            // Check authentication results for this user
            switch( rwUsers[i].hr )
            {
            case S_OK:
                break;

            case XONLINE_E_LOGON_USER_ACCOUNT_REQUIRES_MANAGEMENT:
            default:
                m_hLogonTask.Close();

                return E_ACCOUNT_ERROR;
            }            
        }
        
    }
    
    // 3. Finally check the requested services
    for( DWORD i = 0; i < NUM_SERVICES; ++i )
    {
        HRESULT hrServiceInfo = XOnlineGetServiceInfo( SERVICES[i], NULL );
        
        switch( hrServiceInfo )
        {
        case S_OK:
            break;
        case XONLINE_E_LOGON_SERVICE_NOT_AUTHORIZED:
        case XONLINE_E_LOGON_SERVICE_TEMPORARILY_UNAVAILABLE:
        default:
            m_hLogonTask.Close();

            return E_ACCOUNT_ERROR;
        }            
    }   
    
    // Everything is OK at this point.  For each user (except guests)
    // set their online notification state so they are visible to their
    // friends. A real title would also check for the voice peripheral and 
    // specify the XONLINE_FRIENDSTATE_FLAG_VOICE if present.  
    for( DWORD i = 0; i < XONLINE_MAX_LOGON_USERS; ++i )
    {
        if( rwUsers[i].xuid.qwUserID != 0 && 
            !XOnlineIsUserGuest( rwUsers[i].xuid.dwUserFlags ) )
        {
            HRESULT hrNotification = 
                    XOnlineNotificationSetState( i,   // Controller index
                                                 XONLINE_FRIENDSTATE_FLAG_ONLINE,
                                                 XNKID(),
                                                 0,
                                                 NULL );
            
            if( !SUCCEEDED( hrNotification ) )
            {
                m_hLogonTask.Close();

                return E_ACCOUNT_ERROR;
            }
        }
    }

    return (INT)S_OK;
}

//-------------------------------------------------------------------------------------
// Name: ChangeTeamProperties()
// Desc: Changes the name and description of the team selected by the user
//       to have new random information.
//       Returns FALSE if the operation fails.
//-------------------------------------------------------------------------------------
BOOL CXBoxSample::ChangeTeamProperties( INT iUser )
{
    if( m_iItemSelected < (INT)m_dwTeamCount )
    {
        assert( m_rwTeamInfo );

        // Step 1
        //
        // Create a new team properties structure
        // holding the updated information
        XONLINE_TEAM_PROPERTIES newTeamProps;

        ZeroMemory( &newTeamProps, sizeof( newTeamProps ) );

        newTeamProps.TeamDataSize = m_rwTeamInfo[m_iItemSelected].TeamProperties.TeamDataSize;

        lstrcpynW( newTeamProps.wszURL, L"http://www.xbox.com", XONLINE_MAX_TEAM_URL_SIZE );

        memcpy( newTeamProps.TeamData,
                m_rwTeamInfo[m_iItemSelected].TeamProperties.TeamData,
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

        XONLINETASK_HANDLE hTeamChangeTask;
        
        HRESULT hrTeamPropChange = XOnlineTeamSetProperties(
            iUser,                          // User/controller requisting the change
            m_rwTeamXUIDS[m_iItemSelected], // XUID of the team to modify
            &newTeamProps,                  // Updated properties
            NULL,                           // An event
            &hTeamChangeTask );             // The task to assign to this action

        if( FAILED( hrTeamPropChange ) )
            return FALSE;

        // Step 3
        //
        // Pump the task until it is finished
        do
        {
            hrTeamPropChange = XOnlineTaskContinue( hTeamChangeTask );

            if( FAILED( hrTeamPropChange ) )
            {
                XOnlineTaskClose( hTeamChangeTask );

                return FALSE;
            }
        }
        while( hrTeamPropChange == XONLINETASK_S_RUNNING );

        // Step 4
        //
        // The task has finished and succeeded
        XOnlineTaskClose( hTeamChangeTask );
    }

    return TRUE;
}

//-------------------------------------------------------------------------------------
// Name: SendInvite
// Desc: Sends a team invites from the current user to join the team specified by
//       the given XUID to the user with the given XUID.
//-------------------------------------------------------------------------------------
HRESULT CXBoxSample::SendInvite( XUID xuidTeam, XUID xuidNewRecruit )
{
    XONLINE_TEAM_MEMBER_PROPERTIES  teamMemberProps;
    XONLINETASK_HANDLE              hInviteTask;

    // Do not reinvite ourselves to be a team member
    // of a team we already are a member of
    if( m_rwStoredUsers[m_localUsers[m_wControllingUser].m_wUserIndex].xuid.qwUserID
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
    
    // XOnlineTeamMemberRecruit sends a message to the recruited player inviting them to join the team
    // you can get a message handle back here if you want to add custom message properties
    HRESULT hrInvite = XOnlineTeamMemberRecruit(
                m_wControllingUser, // The controller port of the user requesting
                xuidTeam,           // The XUID of the team to recruit for
                xuidNewRecruit,     // The XUID of the player we want to recruit
                &teamMemberProps,   // Initial properties of the new recruit
                NULL,               // The handle of a message we created with
                                    // XOnlineMessageCreate. Sending NULL
                                    // attaches a default message.
                NULL,               // A work event
                &hInviteTask        // The task handle for this job
            );

    if( FAILED( hrInvite ) )
        return hrInvite;

    // Step 2
    //
    // Pump the task until the invites is finished being sent
    do
    {
        hrInvite = XOnlineTaskContinue( hInviteTask );
    }
    while( hrInvite == XONLINETASK_S_RUNNING );


    // Step 3
    //
    // Close the task and return our
    // success (or failure) code

    XOnlineTaskClose( hInviteTask );

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
HRESULT CXBoxSample::ProcessInvite( INT iMessageIndex, XONLINE_PEER_ANSWER_TYPE eAnswer )
{
    // If this is not a team message, ignore it
    if( !( m_rwMessagesSummaries[iMessageIndex].dwMessageFlags & XONLINE_MSG_FLAG_TEAM_CONTEXT ) )
        return E_FAIL;

    // Only deal with a message trying to recuit us
    if( m_rwMessagesSummaries[iMessageIndex].bMsgType != XONLINE_MSG_TYPE_TEAM_RECRUIT )
        return E_FAIL;

    // Step 1
    //
    // Fill in the response
    // The response is just the team id that we are
    // accepting the invitation for

    XONLINETASK_HANDLE hAcceptInviteTask;
    XUID               teamXUID;

    teamXUID.qwTeamID    = m_rwMessagesSummaries[iMessageIndex].qwMessageContext;
    teamXUID.dwUserFlags = 0;

    // Send the actual response
    HRESULT hrAcceptInvite = XOnlineTeamMemberAnswerRecruit(
                                    m_wControllingUser, // Controller port of the user answering
                                    teamXUID,           // The XUID of the team we are accepting the invitation for
                                    eAnswer,            // The answer (YES, NO, NEVER)
                                    NULL,               // Any event
                                    &hAcceptInviteTask  // The task that this is
                                );

    if( FAILED( hrAcceptInvite ) )
        return hrAcceptInvite;

    // Step 2
    //
    // Continue the task until it is complete
    // A real title should not block for this
    // operations

    do
    {
        hrAcceptInvite = XOnlineTaskContinue( hAcceptInviteTask );

        if( FAILED( hrAcceptInvite ) )
        {
            return hrAcceptInvite;
        }
    }
    while( hrAcceptInvite == XONLINETASK_S_RUNNING );

    // Step 3
    //
    // Close the task and return S_OK

    XOnlineTaskClose( hAcceptInviteTask );

    // Sleep a little so we will not see the invite we just accepted
    // in the inbox again
    Sleep( 1000 );

    return S_OK;
}

//-------------------------------------------------------------------------------------
// Name: CreateTeam()
// Desc: Attempts to create a team with a random name and description.
//       The user controlling the UI is set as the creator of the team.
//       Successfully creating a team returns S_OK, otherwise and error is returned.
//-------------------------------------------------------------------------------------
HRESULT CXBoxSample::CreateTeam()
{
    // Step 1
    //
    // Fill in the details of the team we want to
    // create and make ourselves the owner of

    XONLINE_TEAM_MEMBER_PROPERTIES teamMemberProperties[1];
    DWORD                          dwMaxTeamSize = 10;
    CXBOnlineTask                  hTeamCreateTask;

    XBRandName_GetRandomName( m_createdTeamProps.wszTeamName,     XONLINE_MAX_TEAM_NAME_SIZE );
    XBRandName_GetRandomName( m_createdTeamProps.wszDescription,  XONLINE_MAX_TEAM_DESCRIPTION_SIZE );
    XBRandName_GetRandomName( m_createdTeamProps.wszMotto,        XONLINE_MAX_TEAM_MOTTO_SIZE );
    lstrcpynW( m_createdTeamProps.wszURL, L"http://www.xbox.com", XONLINE_MAX_TEAM_URL_SIZE );

    m_createdTeamProps.TeamDataSize   = 0; // Team Data is optional
    ZeroMemory( m_createdTeamProps.TeamData, sizeof( m_createdTeamProps.TeamData ) );

    // Give all the priviledges to the team creator
    teamMemberProperties[0].dwPrivileges   = ( XONLINE_TEAM_DELETE |
                                                XONLINE_TEAM_MODIFY_DATA |
                                                XONLINE_TEAM_MODIFY_MEMBER_PERMISSIONS |
                                                XONLINE_TEAM_DELETE_MEMBER |
                                                XONLINE_TEAM_RECRUIT_MEMBERS );

    teamMemberProperties[0].TeamMemberDataSize = 0;
    ZeroMemory( teamMemberProperties[0].TeamMemberData, sizeof( teamMemberProperties[0].TeamMemberData ) );

    // Step 2
    //
    // Send the new team to Xbox Live
    HRESULT hrTeamCreate = XOnlineTeamCreate( m_wControllingUser,   // Index of the controller of the user creating the team
                                              &m_createdTeamProps,  // Properties of the team
                                              teamMemberProperties, // Properties of the initial team member
                                              dwMaxTeamSize,        // Maximum number of people on roster
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

    hTeamCreateTask.Close();

    return S_OK;
}

//-------------------------------------------------------------------------------------
// Name: RemoveTeamMember()
// Desc: Kicks the player with the given XUID of the team with the given XUID.
//       Returns S_OK if it succeeded, otherwise returns the specific error.
//-------------------------------------------------------------------------------------
HRESULT CXBoxSample::RemoveTeamMember( XUID xuidTeam, XUID xuidMember )
{
    XONLINETASK_HANDLE hTeamRemoveMembersTask;

    // Step 1
    //
    // Start the deletion task for the member we want to remove
    HRESULT hrRemoveMembers = XOnlineTeamMemberRemove(
                                // The controller port of the user starting the action
                                m_wControllingUser,
                                // The XUID of the team with the member being removed
                                xuidTeam,
                                // The XUID of the team member being removed
                                xuidMember,
                                // Any work event
                                NULL, 
                                // The task of the deletion process
                                &hTeamRemoveMembersTask
                                );

    if( FAILED( hrRemoveMembers ) )
    {
        return hrRemoveMembers;
    }

    // Step 2
    // Continue until the deletion task is finished
    do
    {
        hrRemoveMembers = XOnlineTaskContinue( hTeamRemoveMembersTask );

        // We may not be able to remove all players
        // but if we fail to remove one player we still
        // may be able to kick off other members
        // so let's remember the error
        // and notify the user when we are done
        // that not everyone could be kicked off
        if( FAILED( hrRemoveMembers ) )
        {
            return hrRemoveMembers;
        }
    }
    while( hrRemoveMembers == XONLINETASK_S_RUNNING );

    XOnlineTaskClose( hTeamRemoveMembersTask );

    return S_OK;
}

//-------------------------------------------------------------------------------------
// Name: DeleteTeam()
// Desc: Removes the entire team from Xbox Live. This removes all players from the team,
//       wipes the team name out of the service.
//       Returns S_OK if it succeeded, otherwise returns the specific error.
//-------------------------------------------------------------------------------------
HRESULT CXBoxSample::DeleteTeam( XUID xuidTeam )
{
    // Step 1
    //
    // Start the deletion task
    XONLINETASK_HANDLE hTeamDeleteTask;

    HRESULT hrTeamDelete =  XOnlineTeamDelete(
                                m_wControllingUser, // Controller port of the user starting the action
                                xuidTeam,           // The XUID of the team to delete
                                NULL,               // Any work event
                                &hTeamDeleteTask    // The deletion task
                             );             

    if( FAILED( hrTeamDelete ) )
        return hrTeamDelete;

    // Step 2
    //
    // Pump the task until it is finished
    do
    {
        hrTeamDelete = XOnlineTaskContinue( hTeamDeleteTask );

        if( FAILED( hrTeamDelete ) )
            return hrTeamDelete;
    }
    while( hrTeamDelete == XONLINETASK_S_RUNNING );

    // Step 3
    //
    // The task is done, so close it
    // and return our success
    XOnlineTaskClose( hTeamDeleteTask );

    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: SetPermissions()
// Desc: Sets the permissions of the team member with the given XUID to the
//       new privleges given for the team with the given XUID
//       If the operation fails, then the specific error is returned
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::SetPermissions( XUID xuidTeam, XUID xuidMember,
                                     DWORD dwNewPrivileges )
{
    XONLINETASK_HANDLE hPermissionTask;

    // Step 1
    //
    // Start the permissions change task for the member we want to remove

    XONLINE_TEAM_MEMBER_PROPERTIES newMemberProps;

    ZeroMemory( &newMemberProps, sizeof( newMemberProps ) );

    // The new privleges of the member
    newMemberProps.dwPrivileges       = dwNewPrivileges;
    newMemberProps.TeamMemberDataSize = 0;

    HRESULT hrChangePermissions = XOnlineTeamMemberSetProperties(
                                        // The controller port of the user requesting the change
                                        m_wControllingUser,
                                        // The XUID of the team with the member being changed
                                        xuidTeam,
                                        // The XUID of the member being changed
                                        xuidMember,
                                        // The new properties the team member will have
                                        &newMemberProps,
                                        // A work event
                                        NULL,
                                        // The permission changing task
                                        &hPermissionTask
                                    );


    // We still may be able to change other users
    // permissions if we fail
    if( FAILED( hrChangePermissions ) )
        return hrChangePermissions;

    // Step 2
    //
    // Continue until the deletion task is finished
    do
    {
        hrChangePermissions = XOnlineTaskContinue( hPermissionTask );

        if( FAILED( hrChangePermissions ) )
        {
            XOnlineTaskClose( hPermissionTask );

            return hrChangePermissions;
        }
    }
    while( hrChangePermissions == XONLINETASK_S_RUNNING );

    // Step 3
    //
    // Finished! Close the task and return success
    XOnlineTaskClose( hPermissionTask );

    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: GetStatIDs()
// Desc: Return an array of ids for the stats maintained internally
//-----------------------------------------------------------------------------
PWORD CXBoxSample::GetStatIDs( DWORD *pdwNumStats )
{

    // Stat attribute IDs.  These must match the number and order of
    // ids in the m_Stats array.
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
BOOL CXBoxSample::WriteTeamStatistics( LONG &lKills, LONG &lDeaths, LONG &lAssists,
                                       LONGLONG &llRating )
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

    StatProc.Update.xuid               = m_rwTeamXUIDS[m_iItemSelected];
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
    XONLINETASK_HANDLE hWriteTask;

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
    do 
    { 
        hr = XOnlineTaskContinue( m_hLogonTask );

        if( FAILED( hr ) )
        {
            XOnlineTaskClose( hWriteTask );
            return FALSE;
        }

        hr = XOnlineTaskContinue( hWriteTask );
    } while( hr == XONLINETASK_S_RUNNING );

    XOnlineTaskClose( hWriteTask );

    if( FAILED( hr ) )
        return FALSE;

    return TRUE;
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
BOOL CXBoxSample::GetTeamLeaderboard()
{
    // Step 1
    //
    // Start the enumeration process to get the entire leader board

    CXBOnlineTask hLeaderboardTask;
    DWORD         dwNumStats       = 0;
    PWORD         pwStatsPerTeam   = GetStatIDs( &dwNumStats );

    m_dwNumLeaderboardUsers = 0;

    HRESULT hrLeaderEnumerate = XOnlineStatLeaderEnumerate(
                                    NULL,             // XUID of a team we want to include
                                                      // in the leader board. Providing
                                                      // NULL will simply retrieve only
                                                      // the top rated entries
                                    1,                // The number of leader boards to retrieve
                                    MAX_STAT_USERS,   // The number of teams to retrieve PER leaderboard
                                    LEADERBOARD_ID,   // The ID of the leaderboard to retrieve
                                    dwNumStats,       // The total number of statistics to retrieve
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

    ZeroMemory( m_rwLeaderboardUsers, sizeof( m_rwLeaderboardUsers ) );
    ZeroMemory( m_rwLeaderboardStats, sizeof( m_rwLeaderboardStats ) );

    INT iStatSize = dwNumStats * MAX_STAT_USERS;

    // Obtain the results of the enumeration.  Note that the XONLINE_STAT
    // attributes for alls users are stored in the Stats array.  If there
    // are N attributes, the first N elements are the attributes for the
    // first user, the second N elements are the attributes for the
    // second user.  The attributes for user "i" is at N*i.
    hrLeaderEnumerate = XOnlineStatLeaderEnumerateGetResults(
                            hLeaderboardTask,         // The task that started the read
                            MAX_STAT_USERS,           // Maximum number of teams to return
                            m_rwLeaderboardUsers,     // Names of the teams in the board
                            iStatSize,                // The total number of stats
                            m_rwLeaderboardStats,     // The array of statistics
                            &dwLeaderboardSize,       // The TOTAL size of the leader board
                            &m_dwNumLeaderboardUsers, // The number of results returned
                            0,                        // The size of the extra read buffer
                            NULL                      // Pointer to extra read buffer
                         );

    // Make sure that we got the proper results back
    assert( (INT)( m_dwNumLeaderboardUsers * dwNumStats ) <= iStatSize );

    if( FAILED( hrLeaderEnumerate ) )
        return FALSE;

    if( m_dwNumLeaderboardUsers < 1 )
        return FALSE;

    hLeaderboardTask.Close();

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
BOOL CXBoxSample::GetTeamList()
{
    // Step 1
    //
    // Start the task to enumerate the teams list
    // that the current user is a member of

    CXBOnlineTask hViewMyTeamsTask;

    HRESULT hrTeamFind = XOnlineTeamEnumerateByUserXUID(
        m_wControllingUser,
        m_rwStoredUsers[m_localUsers[m_wControllingUser].m_wUserIndex].xuid,
        NULL,
        &hViewMyTeamsTask );

    // Unable to start enumeration task
    if( FAILED( hrTeamFind ) )
    {
        hViewMyTeamsTask.Close();

        return FALSE;
    }

    // Step 2 -
    // Continue the task until complete
    if(! WaitForTaskToComplete( hViewMyTeamsTask, &hrTeamFind ) )
    {
        hViewMyTeamsTask.Close();

        return FALSE;
    }

    // Step 3
    // Get the results from the finished task
    // and populate the list
    m_dwTeamCount = 0;

    ZeroMemory( m_rwTeamXUIDS, sizeof( m_rwTeamXUIDS ) );

    hrTeamFind = XOnlineTeamEnumerateGetResults(
                    hViewMyTeamsTask, // The enumeration task
                    &m_dwTeamCount,   // The number of teams read
                    m_rwTeamXUIDS     // An array of all the team XUIDs read
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
    for( INT i = 0; i < (INT)m_dwTeamCount; ++i)
    {
        CXBOnlineTask hTeamDetailsTask;

        HRESULT hrTeamDetails = XOnlineTeamGetDetails(
                                    hViewMyTeamsTask, // The task that started the team read
                                    m_rwTeamXUIDS[i], // The list of XUIDs that we want details of
                                    &m_rwTeamInfo[i]  // The array of details to be populated
                                );

        // Unable to get team details
        if( FAILED( hrTeamDetails ) )
        {
            hViewMyTeamsTask.Close();

            return FALSE;
        }
    }

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
BOOL CXBoxSample::GetTeamRoster()
{
    m_dwRosterRenderStart = 0;

    if( m_phTeamRosterTask.IsOpen() )
        m_phTeamRosterTask.Close();

    // STEP 1
    // Start the read of the roster

    // Setting to XONLINE_TEAM_SHOW_RECRUITS
    // will return "members" who have not
    // yet accepted an invitation
    //
    // Setting to 0 will only show
    // members who have accepted an
    // invitation to join the team
    DWORD dwEnumerationFlags = XONLINE_TEAM_SHOW_RECRUITS;

    HRESULT hrTeamRoster = XOnlineTeamMembersEnumerate(
            m_wControllingUser,             // Controller index (zero-based) of the user making the request. 
            m_rwTeamXUIDS[m_iItemSelected], // XUID structure that uniquely identifies the team. 
            dwEnumerationFlags,             // Flags indicating how the team members should be enumerated.
                                            // XONLINE_TEAM_SHOW_RECRUITS Indicates that recruits should
                                            // be included in the returned results. 
            NULL,                           // Handle to an event (OPTIONAL)
            &m_phTeamRosterTask             // Pointer to an XONLINETASK_HANDLE returned
        );

    if( FAILED( hrTeamRoster ) )
        return FALSE;

    // STEP 2
    // Continue until the task is complete
    if(! WaitForTaskToComplete( m_phTeamRosterTask, &hrTeamRoster ) )
    {
        return FALSE;
    }

    // STEP 3
    // Now that the task is finished, get the results
    m_dwTeamMemberCount = 0;
    ZeroMemory( m_rwTeamMembers, sizeof( m_rwTeamMembers ) );

    hrTeamRoster = XOnlineTeamMembersEnumerateGetResults(
                        m_phTeamRosterTask,   // The task that read the roster
                        &m_dwTeamMemberCount, // The number of team members
                        m_rwTeamMembers       // The list of XUIDs to be populated
                    );

    return SUCCEEDED( hrTeamRoster );
}


///////////////////////
// UI FSM state code //
///////////////////////

//-------------------------------------------------------------------------------------
// Name: GetEvent()
// Desc: Returns the state of the controller at the given port
//-------------------------------------------------------------------------------------
CXBoxSample::Event CXBoxSample::GetEvent( INT iController ) const
{
    // "A" or "Start"
    if( g_Gamepads[iController].bPressedAnalogButtons[XINPUT_GAMEPAD_A] ||
        g_Gamepads[iController].wPressedButtons & XINPUT_GAMEPAD_START )
    {
        return EV_BUTTON_A;
    }
    
    // "B"
    if( g_Gamepads[iController].bPressedAnalogButtons[XINPUT_GAMEPAD_B] ||
        g_Gamepads[iController].wPressedButtons & XINPUT_GAMEPAD_BACK )
        return EV_BUTTON_B;

    // "X"
    if( g_Gamepads[iController].bPressedAnalogButtons[XINPUT_GAMEPAD_X] )
        return EV_BUTTON_X;

    // "Y"
    if( g_Gamepads[iController].bPressedAnalogButtons[XINPUT_GAMEPAD_Y] )
        return EV_BUTTON_Y;
    
    // "Back"
    if( g_Gamepads[iController].wPressedButtons & XINPUT_GAMEPAD_BACK )
        return EV_BUTTON_BACK;
    
    // "White"
    if( g_Gamepads[iController].bPressedAnalogButtons[XINPUT_GAMEPAD_WHITE] )
        return EV_BUTTON_WHITE;

    // "Black"
    if( g_Gamepads[iController].bPressedAnalogButtons[XINPUT_GAMEPAD_BLACK] )
        return EV_BUTTON_BLACK;
    
    // Movement
    if( g_Gamepads[iController].wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
        return EV_UP;

    if( g_Gamepads[iController].wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
        return EV_DOWN;
    
    return EV_NULL;
}

//-------------------------------------------------------------------------------------
// Name: GetEvent()
// Desc: Returns the state of the controller
//-------------------------------------------------------------------------------------
CXBoxSample::Event CXBoxSample::GetEvent() const
{
    return (GetEvent( 0 ));
}

//-------------------------------------------------------------------------------------
// Name: PushState()
// Desc: Transitions the UI and game from it's current state to the requested
//       state. When a transition occurs any code required by the previous
//       is executed. Any code required by the new state before entry is
//       also executed.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::PushState( EUIStates newState )
{
    // Do not execute "double entry"
    // if this happens
    if( newState == m_state )
        return;

    // Execute exit functionality
    switch( m_state )
    {
    case STATE_SELECT_ACCOUNT:      ExitStateSelectAccount();       break;
    case STATE_LOGIN:               ExitStateLogin();               break;
    case STATE_LOGIN_FAILED:        ExitStateLoginFailed();         break;
    case STATE_NETWORK_ERROR:       ExitStateNetworkError();        break;
    case STATE_GAME_SETUP:          ExitStateGameSetup();           break;
    case STATE_TEAMS_LEADERBOARD:   ExitStateTeamsLeaderboard();    break;
    case STATE_TEAMS:               ExitStateTeams();               break;
    case STATE_RECENT_PLAYERS:      ExitStateRecentPlayers();       break;
    case STATE_SELECT_INVITE_TEAM:  ExitStateSelectInviteTeam();    break;
    case STATE_INBOX:               ExitStateInbox();               break;
    case STATE_INVITE_DETAILS:      ExitStateInviteDetails();       break;
    case STATE_VIEW_MY_TEAMS:       ExitStateViewMyTeams();         break;
    case STATE_VIEW_TEAM_ROSTER:    ExitStateViewTeamRoster();      break;
    case STATE_TEAM_MEMBER_OPS:     ExitStateTeamMemberOps();       break;
    case STATE_MESSAGE_WINDOW:      ExitStateMessageWindow();       break;
    default:
        break; //assert(0 && "Unknown/illegal state!");
    };

    // Execute entry functionality
    switch( newState )
    {
    case STATE_SELECT_ACCOUNT:      EnterStateSelectAccount();      break;
    case STATE_LOGIN:               EnterStateLogin();              break;
    case STATE_LOGIN_FAILED:        EnterStateLoginFailed();        break;
    case STATE_NETWORK_ERROR:       EnterStateNetworkError();       break;
    case STATE_GAME_SETUP:          EnterStateGameSetup();          break;
    case STATE_TEAMS_LEADERBOARD:   EnterStateTeamsLeaderboard();   break;
    case STATE_TEAMS:               EnterStateTeams();              break;
    case STATE_RECENT_PLAYERS:      EnterStateRecentPlayers();      break;
    case STATE_SELECT_INVITE_TEAM:  EnterStateSelectInviteTeam();   break;
    case STATE_INBOX:               EnterStateInbox();              break;
    case STATE_INVITE_DETAILS:      EnterStateInviteDetails();      break;
    case STATE_VIEW_MY_TEAMS:       EnterStateViewMyTeams();        break;
    case STATE_VIEW_TEAM_ROSTER:    EnterStateViewTeamRoster();     break;
    case STATE_TEAM_MEMBER_OPS:     EnterStateTeamMemberOps();      break;
    case STATE_MESSAGE_WINDOW:      EnterStateMessageWindow();      break;
    default:
        assert( 0 && "Unknown/illegal state!" );
    }

    // Finally transition to the desited state
    // This is set for legacy reasons
    m_state = newState;

    // Push the new state on top of the stack
    assert( m_wStateStackSize < MAX_SIZE_STATE_STACK );
    m_stateStack[m_wStateStackSize++] = m_state;
}

//-------------------------------------------------------------------------------------
// Name: PopState
// Desc: Stops execution of the current state and resumes execution of the previous
//       state.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::PopState( BOOL bReinit )
{
    // Can't exit past the starting state!
    assert( m_wStateStackSize > 1 );

    --m_wStateStackSize;

    m_state = m_stateStack[m_wStateStackSize - 1];

    if( bReinit )
    {
        --m_wStateStackSize;

        EUIStates tempState = m_state;

        m_state = NUM_STATES;

        PushState( tempState );
    }
}

//-------------------------------------------------------------------------------------
// Name: PushMessageWindow
// Desc: Pushes the message window state and transitions to it to
//       display the given text message;
//-------------------------------------------------------------------------------------
VOID CXBoxSample::PushMessageWindow( const CHAR* strTextMessage )
{
    XBUtil_GetWide( strTextMessage,
                    m_szGameMessage, 
                    MAX_MESSAGE_LENGTH );

    PushState( STATE_MESSAGE_WINDOW );
}

//-------------------------------------------------------------------------------------
// Name: RenderMenu()
// Desc: Draws the given menu to the screen along with a point next to the
//       currently selected item
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderMenu( const WCHAR* strMenuName, const WCHAR** rwMenuText,
                 const WORD wNumMenuItems, const INT iCurMenuItem )
{
    // Menu Title
    m_font.DrawText( SCREEN_CENTER_X, POS_SCREEN_TITLE_Y, COLOR_NORMAL,
                     strMenuName,
                     XBFONT_CENTER_X );

    // Attempt to center the menu
    float fMenuStartPos = SCREEN_CENTER_Y - ( wNumMenuItems * DEFAULT_TEXT_PADDING * 0.5f );

    assert( ( fMenuStartPos > POS_SCREEN_TITLE_Y ) && "Menu too large!" );

    // If we are given an empty menu just render
    // the name of the screen and the footer
    if( wNumMenuItems > 0 )
    {
        // Menu Items
        for( WORD i = 0; i < wNumMenuItems; ++i )
        {
            // Highlight the selected item
            DWORD dwColor = ( iCurMenuItem == i ) ? COLOR_HIGHLIGHT : COLOR_NORMAL;

            m_font.DrawText( SCREEN_CENTER_X,
                            fMenuStartPos + (DEFAULT_TEXT_PADDING * i),
                            dwColor, rwMenuText[i], XBFONT_CENTER_X );
        }

        // Show selected item with a little triangle
        FLOAT fTextOffset   = m_font.GetTextWidth( rwMenuText[ iCurMenuItem ] ) / 2.0f;
        FLOAT fTextPos      = SCREEN_CENTER_X - 
                            ( fTextOffset + m_font.GetTextWidth( GLYPH_RIGHT_TICK ) );

        m_font.DrawText( fTextPos,
                        fMenuStartPos + ( DEFAULT_TEXT_PADDING * iCurMenuItem ),
                        COLOR_POINTER, GLYPH_RIGHT_TICK, XBFONT_CENTER_X );
    }
}

//-------------------------------------------------------------------------------------
// Name: GetMenuPosition()
// Desc: Takes the current menu position, the number of items in the menu
//       and the menu event and returns the new menu position. Handles wrap-around
//       of the menu in both directions
//-------------------------------------------------------------------------------------
INT CXBoxSample::GetMenuPosition( INT iCurMenuPosition, INT iNumMenuItems, Event event , INT iMenuWrap )
{
    switch( event )
    {
    case EV_UP:
        --iCurMenuPosition;

        switch( iMenuWrap )
        {
        case MENU_WRAP_ON:
        // Wrap the input to goto the bottom
            iCurMenuPosition = ( iCurMenuPosition < 0 ) ? 
                                 ( iNumMenuItems - 1 ) : iCurMenuPosition;
            break;

        case MENU_WRAP_OFF:
            // Don't wrap. Just stick to the first index
            iCurMenuPosition = ( iCurMenuPosition < 0 ) ? 
                                 0 : iCurMenuPosition;
            break;
        }

        break;

    case EV_DOWN:
        ++iCurMenuPosition;

        switch( iMenuWrap )
        {
        case MENU_WRAP_ON:
        // Wrap the input to goto the top
            iCurMenuPosition = ( iCurMenuPosition >= iNumMenuItems ) ? 
                                 0 : iCurMenuPosition;
            break;

        case MENU_WRAP_OFF:
            // Don't wrap. Just stick to the last index
            iCurMenuPosition = ( iCurMenuPosition >= iNumMenuItems ) ? 
                                 ( iNumMenuItems - 1 ) : iCurMenuPosition;
            break;
        }

        break;
    }

    return iCurMenuPosition;
}


/////////////////////////
// State SelectAccount //
/////////////////////////

//-------------------------------------------------------------------------------------
// Name: RenderAccountSelectionView
// Desc: Renders the specified screen (one of the four screens)
//       so upto four users can simultaneously log into Xbox Live
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderAccountSelectionView( EScreenView eViewWindow )
{
    // Adjust the text position based on
    // which view screen we want
    FLOAT fOffsetX = 0.0f;
    FLOAT fOffsetY = 0.0f;
    FLOAT fTitlePosYInitOffset = 10.0f;

    switch( eViewWindow )
    {
    case VIEW_UPPER_LEFT:
        fTitlePosYInitOffset = 75.0f;
        break;

    case VIEW_UPPER_RIGHT:
        fTitlePosYInitOffset = 75.0f;
        fOffsetX = SCREEN_CENTER_X;
        break;

    case VIEW_LOWER_LEFT:
        fOffsetY = SCREEN_CENTER_Y;
        break;

    case VIEW_LOWER_RIGHT:
        fOffsetX = SCREEN_CENTER_X;
        fOffsetY = SCREEN_CENTER_Y;
        break;

    default:
        assert(0 && "Invalid view!");
        break;
    }

    CONST FLOAT SCREEN_ACCOUNT_X_SCALE = 1.0f;
    CONST FLOAT SCREEN_ACCOUNT_Y_SCALE = 1.0f;


    // Render the adjusted view

    FLOAT fViewCenterX       = ( SCREEN_CENTER_X * 0.5f ) + fOffsetX;
    FLOAT fScrollArrowX      = ( SCREEN_CENTER_X * 0.7f ) + fOffsetX;
    FLOAT fViewCenterY       = ( SCREEN_CENTER_Y * 0.5f ) + fOffsetY;
    FLOAT fTitlePosY         = fTitlePosYInitOffset + fOffsetY;
    FLOAT fCntrlIndexPosY    = fTitlePosY + ( SCREEN_ACCOUNT_Y_SCALE * 20.0f );
    FLOAT fTextPaddingY      = ( SCREEN_ACCOUNT_Y_SCALE * 20.0f );
    FLOAT fUpArrowY          = fCntrlIndexPosY + fTextPaddingY;
    FLOAT fAccountListStartY = fUpArrowY + fTextPaddingY;
    FLOAT fDownArrowY        = fAccountListStartY + ( fTextPaddingY * NUM_ACCOUNTS_PER_WINDOW );

    // If the user has selected an account,
    // let them know that they can back out
    // or can continue
    if( m_bUserSelectedAccount[eViewWindow] )
    {
        m_font.DrawText( fViewCenterX, fViewCenterY, COLOR_RED,
                     GLYPH_B_BUTTON L"CANCEL",
                     XBFONT_CENTER_X );
    }
    else
    // The user is still selecting an account
    // so render the list of available accounts
    // to the screen along with the user's current
    // selection
    {
        // Minimize the font size so we can fit everthing thing in
        m_font.SetScaleFactors(SCREEN_ACCOUNT_X_SCALE, SCREEN_ACCOUNT_Y_SCALE);

        m_font.DrawText( fViewCenterX, fTitlePosY, COLOR_NORMAL,
                        L"SELECT ACCOUNT",
                        XBFONT_CENTER_X );

        const INT CNTRL_INDEX_STR_SIZE = 16;
        CHAR  strCntrlIndex[CNTRL_INDEX_STR_SIZE];
        WCHAR sCntrlIndex[CNTRL_INDEX_STR_SIZE];

        // display controller index
        _snprintf( strCntrlIndex , CNTRL_INDEX_STR_SIZE , "Controller #%d" , ( eViewWindow + 1 ) );
        XBUtil_GetWide( strCntrlIndex, sCntrlIndex, CNTRL_INDEX_STR_SIZE );
        m_font.DrawText( fViewCenterX, fCntrlIndexPosY, COLOR_NORMAL,
                         sCntrlIndex, XBFONT_CENTER_X );

         // get the current selection
        INT iCurSelection = m_localUsers[eViewWindow].m_iCurSelection;

        // see if we have more accounts than we can display (i.e. need to scroll)
        BOOL bNeedToScroll = ( m_dwNumStoredUsers > NUM_ACCOUNTS_PER_WINDOW ) ? TRUE : FALSE;
        
        INT iStartIndex;
        INT iEndIndex;

        // if we need to scroll, we need to calculate the start and end indices
        // of the accounts we want to display.  We want to "anchor" the currently
        // selected account to be as close to the middle as possible (or just past it)
        if( bNeedToScroll )
        {
            // start with the index that's half the window size behind the current selection
            iStartIndex = iCurSelection - ( NUM_ACCOUNTS_PER_WINDOW / 2 );
            
            // if the start index went too far behind the first account 
            if( iStartIndex < 0 )
            {
                // force the start index to be the first one, and the
                // last index to be the window size 
                iStartIndex = 0;
                iEndIndex   = NUM_ACCOUNTS_PER_WINDOW;
            }
            // if the start index went too far ahead past the last account
            else if( iStartIndex > ( (INT)m_dwNumStoredUsers - NUM_ACCOUNTS_PER_WINDOW ) )
            {
                // force the start index to be the last account minus the window size,
                // and the last index to be the last account 
                iStartIndex = (INT)( m_dwNumStoredUsers - NUM_ACCOUNTS_PER_WINDOW );
                iEndIndex   = (INT)( m_dwNumStoredUsers );
            }
            else
            {
                // otherwise, add half the window size to the current index
                // (plus a round-up factor if the window size is odd)
                iEndIndex   = iCurSelection + ( NUM_ACCOUNTS_PER_WINDOW / 2 ) + 
                              ( NUM_ACCOUNTS_PER_WINDOW % 2 );
            }
        }
        else
        {
            // just set the start and end indices to the first and last
            // accounts, respectively
            iStartIndex = 0;
            iEndIndex = (INT)m_dwNumStoredUsers;
        }

        // Show list of user accounts
        for( DWORD i = (DWORD)iStartIndex; i < (DWORD)iEndIndex; ++i )
        {
            // highlight the currently selected account name if this is the one
            DWORD dwColor = ( (DWORD)iCurSelection == i ) ? 
                                COLOR_HIGHLIGHT : COLOR_NORMAL;

            // Show an account already selected as greyed out
            dwColor = m_rwStoredUserSelected[i] ? COLOR_GREY : dwColor;

            // If we need to scroll
            if( bNeedToScroll )
            {
                // unless we're at the top, draw the up arrow
                if( iStartIndex > 0 )
                {
                    m_font.DrawText( fScrollArrowX, fUpArrowY,
                                    COLOR_GREEN, GLYPH_UP_ARROW, XBFONT_CENTER_X );
                }

                // Unless we're at the bottom, draw the down arrow
                if( iEndIndex < (INT)m_dwNumStoredUsers )
                {
                    m_font.DrawText( fScrollArrowX, fDownArrowY,
                                    COLOR_GREEN, GLYPH_DOWN_ARROW, XBFONT_CENTER_X );
                }
            }

            // Convert user name to WCHAR string
            WCHAR strUserName[XONLINE_GAMERTAG_SIZE];
            XBUtil_GetWide( m_rwStoredUsers[i].szGamertag, strUserName,
                            XONLINE_GAMERTAG_SIZE );

            // draw the account name
            m_font.DrawText( fViewCenterX,
                            fAccountListStartY + ( fTextPaddingY * ( i - (DWORD)iStartIndex ) ),
                            dwColor,
                            strUserName, XBFONT_CENTER_X );

            // if this is the currently selected item, draw the pointer
            if( i == (DWORD)iCurSelection )
            {
                // Show selected item with little triangle
                FLOAT fTextOffset   = ( m_font.GetTextWidth( strUserName ) / 2.0f );
                FLOAT fTextPos      = fViewCenterX - 
                                    ( fTextOffset + 
                                        m_font.GetTextWidth( GLYPH_RIGHT_TICK ) );

                m_font.DrawText( fTextPos, fAccountListStartY + 
                                ( fTextPaddingY * ( iCurSelection - iStartIndex ) ),
                                COLOR_POINTER,
                                GLYPH_RIGHT_TICK, XBFONT_CENTER_X );
            }
        }

        // Return the font to the proper size
        m_font.SetScaleFactors(1.0f, 1.0f);
    }
}

//-------------------------------------------------------------------------------------
// Name: EnterStateSelectAccount
// Desc: Executes setup code for STATE_SELECT_ACCOUNT
//       Finds the Xbox Live user accounts on the memory units and hard-drive
//-------------------------------------------------------------------------------------
VOID CXBoxSample::EnterStateSelectAccount()
{
    m_wNumUsersSelectedAccounts = 0;
    m_continueTextColor         = COLOR_GREEN;

    m_flashTimer.Stop();
    ZeroMemory( m_bUserSelectedAccount, sizeof( m_bUserSelectedAccount ) );
    ZeroMemory( m_rwStoredUserSelected, sizeof( m_rwStoredUserSelected ) );

    for( WORD wUser = 0; wUser < MAX_USERS; ++wUser)
    {
        m_localUsers[wUser].m_bSignedIn     = FALSE;
        m_localUsers[wUser].m_iCurSelection = 0;
        m_localUsers[wUser].m_wUserIndex    = 0;
    }

    if( m_bUsersSignedIn )
        m_hLogonTask.Close();

    m_bUsersSignedIn = FALSE;

    // If any MUs are inserted/removed, need to update the
    // user account list
    DWORD dwInsertions;
    DWORD dwRemovals;

    // Stall for mem unit mounting
    while( CXBMemUnit::GetMemUnitChanges( dwInsertions, dwRemovals ) );

    // Keep it in memory so we don't have to worry about insertion
    // and deletion once we get past login
    CXBMemUnit::GetMemUnitSnapshot();

    // First, obtain a list of user accounts on this Xbox. The XOnlineGetUsers
    // function will enumerate both the hard disk and any attached memory units
    // looking for accounts. 
    HRESULT hrGetUsers = XOnlineGetUsers( m_rwStoredUsers, &m_dwNumStoredUsers );

    // Reboot the user to create an Xbox Live account if
    // one isn't found
    if( FAILED( hrGetUsers ) )
    {
        BootToDash( XLD_LAUNCH_DASHBOARD_NEW_ACCOUNT_SIGNUP );
    }

    // If no accounts, then player needs to create an account.
    if( m_dwNumStoredUsers == 0 )
    {
        // Titles must give the player the *option* of going to
        // the online dash to create new account. In addition, it is
        // possible for a player to actually insert/remove an MU while
        // the title account selection UI is active.  A title must
        // call XOnlineGetUsers repeatedly to account for this.
        // For demonstration purposes, we boot to the account signup section
        // of the online dash
        BootToDash( XLD_LAUNCH_DASHBOARD_NEW_ACCOUNT_SIGNUP );
    }

    m_iItemSelected = 0;
}

//-------------------------------------------------------------------------------------
// Name: UpdateStateSelectAccount
// Desc: Allows the user to scroll through all accounts stored on the Xbox
//       and to select the account that they wish to logon with.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateSelectAccount( INT iUser, Event event )
{
    assert( iUser >= 0 );
    assert( iUser < MAX_USERS );

    m_localUsers[iUser].m_iCurSelection = GetMenuPosition( m_localUsers[iUser].m_iCurSelection,
                                                           m_dwNumStoredUsers, event , MENU_WRAP_OFF );

    switch( event )
    {
    case EV_BUTTON_A:
        // If the user has selected an account
        // then the may continue everybody on
        // to the next screen
        if( m_bUserSelectedAccount[iUser] )
        {
            assert( m_wNumUsersSelectedAccounts > 0 );

            PushState( STATE_LOGIN );
        }
        else
        {
            // Do not allow the user to select an account already
            // selected by another user
            if( m_rwStoredUserSelected[m_localUsers[iUser].m_iCurSelection] ) return;
            
            m_bUserSelectedAccount[iUser] = TRUE;
            ++m_wNumUsersSelectedAccounts;
            m_localUsers[iUser].m_wUserIndex = (WORD)m_localUsers[iUser].m_iCurSelection;
            m_rwStoredUserSelected[m_localUsers[iUser].m_wUserIndex] = TRUE;
        }
        break;

    case EV_BUTTON_B:
        if( !m_bUserSelectedAccount[iUser] ) return;

        m_rwStoredUserSelected[m_localUsers[iUser].m_wUserIndex] = FALSE;

        m_bUserSelectedAccount[iUser] = FALSE;
        --m_wNumUsersSelectedAccounts;
        break;

    default:
        break;
    }

    DWORD dwInsertions;
    DWORD dwRemovals;

    // If the user inserts a memory card, go ahead
    // and mount it!
    if( CXBMemUnit::GetMemUnitChanges( dwInsertions, dwRemovals ) )
    {
        // Cause re-initialization of the memory cards
        // and hard drive and find the accounts added
        // or removed by the device change.
        EnterStateSelectAccount();
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateSelectAccount
// Desc: Renders the screen for STATE_SELECT_ACCOUNT
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateSelectAccount()
{
    for( WORD wViewToRender = VIEW_UPPER_LEFT;
          wViewToRender < MAX_VIEWS;
          ++wViewToRender )
    {
        RenderAccountSelectionView( (EScreenView)wViewToRender );
    }

    // At least one person needs to signon per box
    if( m_wNumUsersSelectedAccounts > 0 )
    {
        // Flash the word continue
        if( !m_flashTimer.IsRunning() )
        {
            m_flashTimer.StartZero();
        }
        else
        {
            if( m_flashTimer.GetElapsedSeconds() > 0.5f )
            {
                m_continueTextColor = ( m_continueTextColor == COLOR_GREEN ) ? COLOR_BLUE : COLOR_GREEN;
                m_flashTimer.StartZero();
            }
        }

        m_font.DrawText( SCREEN_CENTER_X, SCREEN_CENTER_Y, m_continueTextColor,
                         GLYPH_A_BUTTON L"CONTINUE",
                         XBFONT_CENTER_X );
    }
    else
    {
        m_flashTimer.Stop();
    }

    // The user can not really back out of this screen
    RenderFooter( FOOTER_RENDER_SELECT );
}


/////////////////
// State Login //
/////////////////

//-------------------------------------------------------------------------------------
// Name: EnterStateLogin()
// Desc: Initialises data needed to login to the Xbox Live service
//-------------------------------------------------------------------------------------
VOID CXBoxSample::EnterStateLogin()
{
    HRESULT hr        = StartSignIn();

    m_iItemSelected   = 0;
    m_bUsersSigningIn = SUCCEEDED( hr );
    m_bUsersSignedIn  = FALSE;

    for( WORD wUser = 0; wUser < MAX_USERS; ++ wUser)
    {
        m_localUsers[wUser].m_bSignedIn = FALSE;
    }
}

//-------------------------------------------------------------------------------------
// Name: UpdateStateLogin
// Desc: Attempts to log the user into the Xbox Live service. If login fails
//       the user will be prompted try again or to fix the problem via
//       the Xbox Dashboard
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateLogin( Event event )
{
    if( m_bUsersSigningIn )
    {
        // Continue the login task and process
        // any results we get back
        m_iSignInResult = ContinueSignIn();

        switch( m_iSignInResult )
        {
        case S_OK: // The Login is still executing
            return;
            break;

        case XONLINE_S_LOGON_CONNECTION_ESTABLISHED:
            {
                // Finish sign in:
                // Announce our presence to Xbox Live
                // and check the required services that
                // we need to run the demo
                INT iSignInResult = FinishSignIn();

                if( iSignInResult == S_OK )
                {
                    for( WORD wUser = 0; wUser < MAX_USERS; ++wUser )
                    {
                        m_localUsers[wUser].m_bSignedIn    = m_bUserSelectedAccount[wUser];
                        
                        m_bUsersSigningIn = FALSE;
                    }

                    m_bUsersSignedIn = TRUE;

                    PopState();
                    PushState( STATE_GAME_SETUP );
                }
                else
                {
                    m_bUsersSignedIn = FALSE;

                    PushState( STATE_LOGIN_FAILED );
                }
                return;
            }
            break;

        case E_NETWORK_ERROR:
            m_bUsersSignedIn  = FALSE;
            m_bUsersSigningIn = FALSE;

            PushState( STATE_LOGIN_FAILED );
            return;
        break;

        case E_ACCOUNT_ERROR:
            m_bUsersSignedIn  = FALSE;
            m_bUsersSigningIn = FALSE;

            PushState( STATE_LOGIN_FAILED );
            return;
            break;

        default:
            assert( 0 && "Unexpected results" );
        }
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateLogin()
// Desc: Shows the user that they are being logged on to Xbox Live
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateLogin()
{
    // Render the scene
    m_font.DrawText( SCREEN_CENTER_X, POS_MESSAGE_Y, COLOR_NORMAL,
                     L"Logging In...", 
                     XBFONT_CENTER_X );

    RenderFooter( FOOTER_RENDER_NONE );
}


///////////////////////
// State LoginFailed //
///////////////////////

//-------------------------------------------------------------------------------------
// Name: UpdateStateLoginFailed
// Desc: Allows the user to choose to try again or fix the problem
//       preventing login via the Xbox Dashboard.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateLoginFailed( Event event )
{
    switch( event )
    {
    case EV_BUTTON_A: // Try again with a different account
        ClearStack();
        return;
        break;

    case EV_BUTTON_B: // Reboot to the Xbox Dashboard
                      // Depending on the error the user
                      // will be presented with different options
        switch( m_iSignInResult )
        {
        case E_NETWORK_ERROR: // Reboot to configure the network settings
            BootToDash( XLD_LAUNCH_DASHBOARD_NETWORK_CONFIGURATION );
            break;

        case E_ACCOUNT_ERROR: // Reboot to configure/create accounts
            BootToDash( XLD_LAUNCH_DASHBOARD_ONLINE_MENU );
            break;
        }
        break;

    default:
        break;
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateLoginFailed
// Desc: Renders the screen that tells the player their login to Xbox Live
//       failed.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateLoginFailed()
{
    const FLOAT HEADER_OFFSET_X_LOGIN_FAILED    = 75.0;
    const FLOAT HEADER_OFFSET_Y_LOGIN_FAILED    = 100.0;

    const FLOAT MSG_OFFSET_X_LOGIN_FAILED       = 75.0;
    const FLOAT MSG_START_OFFSET_Y_LOGIN_FAILED = 200.0;
    const FLOAT MSG_PADDING_LOGIN_FAILED        = 40.0;
    
    FLOAT curMsgYOffset = MSG_START_OFFSET_Y_LOGIN_FAILED;

    const WCHAR* HEADER_STR_LOGIN_FAILED[NUM_ELO_ERRORS] =
    {
        NULL, // (not applicable... we wouldn't be here without an error)
        L"Login Failed: Network Error.",
        L"Login Failed: Account Error."
    };

    const WCHAR* ERROR_CFG_STR_LOGIN_FAILED[NUM_ELO_ERRORS] =
    {
        NULL, // (not applicable... we wouldn't be here without an error)
        L"Press " GLYPH_B_BUTTON L" to configure network settings",
        L"Press " GLYPH_B_BUTTON L" to configure XBox Live user accounts"
    };

    // Render the scene

    // Render the header
    m_font.DrawText( HEADER_OFFSET_X_LOGIN_FAILED, HEADER_OFFSET_Y_LOGIN_FAILED, 
                     COLOR_NORMAL, HEADER_STR_LOGIN_FAILED[m_iSignInResult], 
                     XBFONT_LEFT );

    // Render the messages/instructions
    m_font.DrawText( MSG_OFFSET_X_LOGIN_FAILED, curMsgYOffset, COLOR_NORMAL, 
                     L"Press " GLYPH_A_BUTTON L" to continue", XBFONT_LEFT );
    curMsgYOffset += MSG_PADDING_LOGIN_FAILED;
    m_font.DrawText( MSG_OFFSET_X_LOGIN_FAILED, curMsgYOffset, COLOR_NORMAL, 
                     ERROR_CFG_STR_LOGIN_FAILED[m_iSignInResult], XBFONT_LEFT );
}


////////////////////////
// State NetworkError //
////////////////////////

//-------------------------------------------------------------------------------------
// Name: UpdateStateNetworkError
// Desc: Allows the user to choose what to do in case of a network error.
//       The user may login in again or reboot to the bashboard.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateNetworkError( Event event )
{
    switch(event)
    {
    case EV_BUTTON_A:
        ClearStack();
        return;
        break;

    case EV_BUTTON_B:
        BootToDash( XLD_LAUNCH_DASHBOARD_NETWORK_CONFIGURATION );
        break;

     default:
        break;
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateNetworkError()
// Desc: Displays the screen that tells the player they are
//       experiencing network problems.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateNetworkError()
{
    const FLOAT HEADER_OFFSET_X_NETWORK_ERROR    = 75.0;
    const FLOAT HEADER_OFFSET_Y_NETWORK_ERROR    = 100.0;

    const FLOAT MSG_OFFSET_X_NETWORK_ERROR       = 75.0;
    const FLOAT MSG_START_OFFSET_Y_NETWORK_ERROR = 200.0;
    const FLOAT MSG_PADDING_NETWORK_ERROR        = 40.0;
    
    FLOAT curMsgYOffset = MSG_START_OFFSET_Y_NETWORK_ERROR;

    // Render the scene

    // Render the header
    m_font.DrawText( HEADER_OFFSET_X_NETWORK_ERROR, HEADER_OFFSET_Y_NETWORK_ERROR, 
                     COLOR_NORMAL, L"Network Error", XBFONT_LEFT );

    // Render the message(s)
    m_font.DrawText( MSG_OFFSET_X_NETWORK_ERROR, curMsgYOffset, COLOR_NORMAL, 
                     L"Press " GLYPH_A_BUTTON L" to continue", XBFONT_LEFT );
    curMsgYOffset += MSG_PADDING_NETWORK_ERROR;
    m_font.DrawText( MSG_OFFSET_X_NETWORK_ERROR, curMsgYOffset, COLOR_NORMAL, 
                     L"Press " GLYPH_B_BUTTON L" to configure network settings", 
                     XBFONT_LEFT );

}

//-------------------------------------------------------------------------------------
// Name: ExitStateNetworkError
// Desc: Cleans up the network variables to allow for the user to login again.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::ExitStateNetworkError()
{
    m_hLogonTask.Close();
}


/////////////////////
// State GameSetup //
/////////////////////

//-------------------------------------------------------------------------------------
// Name: EnterStateGameSetup()
// Desc: Initialises the menu varaibles and does sanity checks so the player
//       may select their action.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::EnterStateGameSetup()
{
    m_iItemSelected = 0;

    assert( m_bUsersSignedIn );
}

//-------------------------------------------------------------------------------------
// Name: UpdateStateGameSetup
// Desc: Allows the user to choose to view the Team Leaderboard or Team Managment
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateGameSetup( INT iUser, Event event )
{
    m_iItemSelected = GetMenuPosition( m_iItemSelected, NUM_ITEMS_GAME_SETUP_MENU, event);

    switch( event )
    {
    case EV_BUTTON_A:
        assert( m_iItemSelected >= 0 );
        assert( m_iItemSelected < NUM_ITEMS_GAME_SETUP_MENU );

        m_wControllingUser = (WORD)iUser;

        switch( m_iItemSelected )
        {
        case MENU_GAME_SETUP_TEAMS_LEADERBOARD:  // View the leaderboard of all teams
            if( GetTeamLeaderboard() )
            {
                PushState( STATE_TEAMS_LEADERBOARD );
            }
            else
            {
                PushMessageWindow( "Unable to retrieve leaderboard." );
            }

            break;

        case MENU_GAME_SETUP_TEAMS:        // Goto the teams functions
            PushState( STATE_TEAMS );
            break;
        }
        break;

    case EV_BUTTON_B: // Back out and login with a different account
        PopState( TRUE );
        break;
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateGameSetup
// Desc: Shows the menu of options the player has.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateGameSetup()
{
    RenderMenu( L"GAME SETUP",
                (const WCHAR**)MENU_GAME_SETUP, NUM_ITEMS_GAME_SETUP_MENU,
                m_iItemSelected );

    // Bottom Help text
    RenderFooter( FOOTER_RENDER_SELECT | FOOTER_RENDER_CANCEL );
}

////////////////////////////
// State TeamsLeaderboard //
////////////////////////////

//-------------------------------------------------------------------------------------
// Name: UpdateStateTeamsLeaderboard
// Desc: Waits for the user to dismiss the UI screen and returns to the previous menu
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateTeamsLeaderboard( INT iUser, Event event )
{
    if( iUser != m_wControllingUser )
        return;

    switch( event )
    {
    case EV_BUTTON_B:
        PopState();
        break;
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateTeamsLeaderboard
// Desc: Draws the Leaderboard to the screen
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateTeamsLeaderboard()
{
    RenderControllingUser();

    RenderMenu( L"TEAMS LEADERBOARD", NULL, 0, 0 );

    // Draw the header that labels the table data
    m_font.DrawText( POS_TEAM_X,    POS_LEADER_HEADER_Y, COLOR_NORMAL, L"Team", XBFONT_LEFT );
    m_font.DrawText( POS_KILLS_X,   POS_LEADER_HEADER_Y, COLOR_NORMAL, L"Kills", XBFONT_LEFT );
    m_font.DrawText( POS_DEATHS_X,  POS_LEADER_HEADER_Y, COLOR_NORMAL, L"Deaths", XBFONT_LEFT );
    m_font.DrawText( POS_ASSISTS_X, POS_LEADER_HEADER_Y, COLOR_NORMAL, L"Assists", XBFONT_LEFT );
    m_font.DrawText( POS_RATING_X,  POS_LEADER_HEADER_Y, COLOR_NORMAL, L"Rating",  XBFONT_LEFT );

    // The calculated position of the team information
    FLOAT fYtop   = POS_LEADER_HEADER_Y + DEFAULT_TEXT_PADDING;
  
    // Show the stats for each entry in the leaderboard
    for( DWORD i = 0; i < m_dwNumLeaderboardUsers; ++i )
    {
        // Temp buffer to convert number to viewable strings
        WCHAR strText[32]; 

        // The statistics for ALL teams are stored in
        // a one dimensional array so we need to
        // caculate where the stats for the current team
        // starts
        INT   iTeamStatStart = i * STAT_MAX;
        FLOAT fPosDataY      = fYtop + ( i * LEADERBOARD_TEXT_PADDING );

        // Rank
        wsprintfW( strText, L"%lu) ", m_rwLeaderboardStats[iTeamStatStart + STAT_RANK].lValue );
        m_font.DrawText( POS_TEAM_X, fPosDataY, COLOR_NORMAL, strText, XBFONT_RIGHT );

        // Team Name
        m_font.DrawText( POS_TEAM_X, fPosDataY, COLOR_NORMAL, m_rwLeaderboardUsers[i].wszTeamName );

        // Kills
        wsprintfW( strText, L"%lu", m_rwLeaderboardStats[iTeamStatStart + STAT_KILLS].lValue );
        m_font.DrawText( POS_KILLS_X, fPosDataY, COLOR_NORMAL, strText, XBFONT_LEFT );

        // Deaths
        wsprintfW( strText, L"%lu", m_rwLeaderboardStats[iTeamStatStart + STAT_DEATHS].lValue );
        m_font.DrawText( POS_DEATHS_X, fPosDataY, COLOR_NORMAL, strText, XBFONT_LEFT );

        // Assists
        wsprintfW( strText, L"%lu", m_rwLeaderboardStats[iTeamStatStart + STAT_ASSISTS].lValue );
        m_font.DrawText( POS_ASSISTS_X, fPosDataY, COLOR_NORMAL, strText, XBFONT_LEFT );

        // Rating
        wsprintfW( strText, L"%lu", m_rwLeaderboardStats[iTeamStatStart + STAT_RATING].llValue );
        m_font.DrawText( POS_RATING_X, fPosDataY, COLOR_NORMAL, strText, XBFONT_LEFT );
    }

    // Tell the user they have the option to switch views
    RenderFooter( FOOTER_RENDER_CANCEL );
}

/////////////////
// State Teams //
/////////////////

//-------------------------------------------------------------------------------------
// Name: EnterStateTeams()
// Desc: Initializes the team menu
//-------------------------------------------------------------------------------------
VOID CXBoxSample::EnterStateTeams()
{
    m_iItemSelected = 0;
}

//-------------------------------------------------------------------------------------
// Name: UpdateStateTeams()
// Desc: Updates the Team menu and launches any sub menu
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateTeams( INT iUser, Event event)
{
    // Only allow the controlling user to
    // do work with the team
    if( iUser == m_wControllingUser)
    {
        m_iItemSelected = GetMenuPosition( m_iItemSelected, NUM_ITEMS_TEAMS_MENU, event);

        switch( event )
        {
            case EV_BUTTON_A:
                assert( m_iItemSelected >= 0 );
                assert( m_iItemSelected < NUM_ITEMS_TEAMS_MENU );

                switch( m_iItemSelected )
                {
                case MENU_TEAMS_INBOX: // View my incoming messages
                    PushState( STATE_INBOX );
                    break;

                case MENU_TEAMS_RECENT_PLAYERS:
                    PushState( STATE_RECENT_PLAYERS );
                    break;

                case MENU_TEAMS_VIEW_MY_TEAMS: //View My Teams
                    if( GetTeamList() )
                        PushState( STATE_VIEW_MY_TEAMS );
                    else
                        PushMessageWindow( "Unable to retrieve your teams list." );

                    break;

                case MENU_TEAMS_CREATE_TEAM: // Create A Team
                    // Attempt to create a team
                    // If successfull, show the updated team list
                    // Is a failure happens, then try to give some
                    // detailed information
                    switch( CreateTeam() )
                    {
                    case XONLINE_E_TEAMS_SERVER_BUSY:
                        PushMessageWindow( "Sever was too busy to create your team." );
                        break;

                    case XONLINE_E_TEAMS_TOO_MANY_REQUESTS:
                        PushMessageWindow( "Too many requests to create team." );
                        break;

                    case XONLINE_E_TEAMS_USER_TEAMS_FULL:
                        PushMessageWindow( "You can not create any more teams." );
                        break;

                    case XONLINE_E_TEAMS_NAME_CONTAINS_BAD_WORDS:
                    case XONLINE_E_TEAMS_DESCRIPTION_CONTAINS_BAD_WORDS:
                    case XONLINE_E_TEAMS_MOTTO_CONTAINS_BAD_WORDS:
                    case XONLINE_E_TEAMS_URL_CONTAINS_BAD_WORDS:
                        PushMessageWindow( "The team name, motto, URL or description contains bad language." );
                        break;

                    case S_OK: // Everything went well!
                        if( GetTeamList() )
                        {
                            CHAR szMessage[256];

                            _snprintf( szMessage, 256 ,
                                    "Created team %S",
                                    m_createdTeamProps.wszTeamName );

                            PushState( STATE_VIEW_MY_TEAMS );

                            PushMessageWindow( szMessage );

                            return;
                        }
                        else
                        {
                            PushMessageWindow( "Unable to update your team list" );
                        }

                        break;

                    default:
                        PushMessageWindow( "Unable to create team." );
                        break;
                    }
                    break;

                default:
                    break;
                }
                break;

            case EV_BUTTON_B: // Back out and login with a different account
                PopState( TRUE );
                break;
        }
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateTeams()
// Desc: Renders the Teams menu items
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateTeams()
{
    RenderControllingUser();

    RenderMenu( L"TEAMS", (const WCHAR**)MENU_TEAMS, NUM_ITEMS_TEAMS_MENU, m_iItemSelected );

    // Bottom Help text
    RenderFooter( FOOTER_RENDER_SELECT | FOOTER_RENDER_CANCEL );
}

/////////////////////////
// State RecentPlayers //
/////////////////////////

//-------------------------------------------------------------------------------------
// Name: EnterStateRecentPlayers()
// Desc: Initialize data used by the RecentPlayers state.
//       Builds a list of players that the user can send team invites to.
//       The list is actaully just the other Xbox Live accounts on the HD
//       to simulate the standard "Recent Players" list
//-------------------------------------------------------------------------------------
VOID CXBoxSample::EnterStateRecentPlayers()
{
    // Get the list of teams that we are a member of
    GetTeamList();

    m_dwPlayerSelected    = 0;
    m_dwPlayerRenderStart = 0;
}

//-------------------------------------------------------------------------------------
// Name: UpdateStateRecentPlayers()
// Desc: Takes the user input to scroll through the list of players
//       the user can send team invites to
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateRecentPlayers( INT iUser, Event event )
{
    if( iUser != m_wControllingUser )
        return;

    switch( event )
    {
    case EV_UP: // Move the roster list up until we hit the top
        m_dwPlayerSelected = ( m_dwPlayerSelected > 0 ) ? ( m_dwPlayerSelected - 1 ) : 0;

        if( m_dwPlayerSelected < m_dwPlayerRenderStart )
            m_dwPlayerRenderStart  = ( m_dwPlayerRenderStart > 0 ) ? ( m_dwPlayerRenderStart - 1 ) : 0;

        break;

    case EV_DOWN: // Move the roster list down until we hit the bottom
        m_dwPlayerSelected = ( m_dwPlayerSelected < ( m_dwNumStoredUsers - 1 ) ) ? ( m_dwPlayerSelected + 1 ) : m_dwPlayerSelected;
        
        if( m_dwPlayerSelected >= ( m_dwPlayerRenderStart + NUM_ENTRIES_PER_SCREEN ) )
        {
            ++m_dwPlayerRenderStart;

            if( m_dwPlayerRenderStart > ( m_dwNumStoredUsers - NUM_ENTRIES_PER_SCREEN ) )
                m_dwPlayerRenderStart = m_dwNumStoredUsers - NUM_ENTRIES_PER_SCREEN;
        }
        break;

    case EV_BUTTON_A: // Select the team to send an invite from
        if ( m_localUsers[m_wControllingUser].m_wUserIndex != m_dwPlayerSelected )
        {
            PushState( STATE_SELECT_INVITE_TEAM );
        }

        break;

    case EV_BUTTON_B:
        PopState( TRUE );
        break;
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateRecentPlayers()
// Desc: Draws the list of players the user can send invites to.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateRecentPlayers()
{
    RenderControllingUser();

    RenderMenu( L"RECENT PLAYERS", NULL, 0, 0 );

    float fPlayerListStartY = POS_SCREEN_TITLE_Y + ( DEFAULT_TEXT_PADDING * 2 );
    float fPlayerListStartX = SCREEN_CENTER_X * 0.75f;

    // If we are starting the render from a position
    // other than the first user in the list
    // the draw a little arrow on the side telling
    // the user they can scroll up
    if( m_dwPlayerRenderStart > 0 )
    {
        m_font.DrawText( fPlayerListStartX, fPlayerListStartY,
                         COLOR_HIGHLIGHT, GLYPH_UP_ARROW L"    ", XBFONT_RIGHT );
    }

    // Get the details of each team member
    for( DWORD i = m_dwPlayerRenderStart; i < m_dwNumStoredUsers; ++i )
    {
        // Stop rendering if we hit the maximum number
        // team members viewable at once
        if( ( i - m_dwPlayerRenderStart ) >= NUM_ENTRIES_PER_SCREEN )
        {
            // If more team roster entries are below
            // the last entry drawn, then add a down
            // arrow on the side telling the user
            // can scroll down
            float fDownArrowY = fPlayerListStartY
                                + ( DEFAULT_TEXT_PADDING * ( NUM_ENTRIES_PER_SCREEN - 1 ) );

            m_font.DrawText( fPlayerListStartX, fDownArrowY,
                             COLOR_HIGHLIGHT, GLYPH_DOWN_ARROW L"   ", XBFONT_RIGHT );

            break;
        }

        // Render to the screen!
        INT   iScreenItem = ( i - m_dwPlayerRenderStart );
        FLOAT fPosY       = fPlayerListStartY + ( DEFAULT_TEXT_PADDING * iScreenItem );

        // Allow the user to move the selector
        // up and down to select a specific user
        // to give an permissions to or
        // to remove from the team
        //
        // Show selected item with a little triangle
        FLOAT fIconPosY = fPlayerListStartY + ( DEFAULT_TEXT_PADDING * ( m_dwPlayerSelected - m_dwPlayerRenderStart ) );

        m_font.DrawText( fPlayerListStartX, fIconPosY,
                         COLOR_POINTER, GLYPH_RIGHT_TICK, XBFONT_RIGHT );


        WCHAR szwGamerTag[XONLINE_GAMERTAG_SIZE];
        XBUtil_GetWide( m_rwStoredUsers[i].szGamertag,
                        szwGamerTag,
                        XONLINE_GAMERTAG_SIZE );

        // Show that we can not select ourselves
        DWORD dwColor = ( m_localUsers[m_wControllingUser].m_wUserIndex == i )
                          ? COLOR_GREY : COLOR_NORMAL;

        m_font.DrawText( fPlayerListStartX, fPosY, dwColor,
                         szwGamerTag,
                         XBFONT_LEFT );

    }

    // Tell the user what the purpose of this screen is
    m_font.DrawText( SCREEN_CENTER_X, ( POS_FOOTER_Y - DEFAULT_TEXT_PADDING ),
                     COLOR_HIGHLIGHT,
                     L"SELECT A USER TO SEND A TEAM INVITE", XBFONT_CENTER_X );

    // Bottom Help text
    RenderFooter( FOOTER_RENDER_SELECT | FOOTER_RENDER_CANCEL );
}


////////////////////////////
// State SelectInviteTeam //
////////////////////////////

//-------------------------------------------------------------------------------------
// Name: EnterStateSelectInviteTeam()
// Desc: Intializes the screen that allows the user to select a team to invite another
//       player to join.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::EnterStateSelectInviteTeam()
{
    m_iItemSelected = 0;
}

//-------------------------------------------------------------------------------------
// Name: UpdateStateSelectInviteTeam()
// Desc: Lets the player scroll through the choices of teams to invite another
//       player to join. Calls the code to send the invitation.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateSelectInviteTeam( INT iUser, Event event )
{
    if ( iUser != m_wControllingUser )
        return;

    m_iItemSelected = GetMenuPosition( m_iItemSelected, m_dwTeamCount, event );

    switch( event )
    {
    case EV_BUTTON_A: // The user has choosen a team
        PopState();

        switch( SendInvite( m_rwTeamInfo[m_iItemSelected].xuidTeam,
                           m_rwStoredUsers[m_dwPlayerSelected].xuid ) )
        {
        case XONLINE_E_TEAMS_SELF:
            PushMessageWindow( "Cannot send invitation to self" );
            break;

        case XONLINE_E_TEAMS_INSUFFICIENT_PRIVILEGES:
            PushMessageWindow( "You do not have permissions to send invites" );
            break;

        case XONLINE_E_TEAMS_USER_ALREADY_EXISTS:
            PushMessageWindow( "Player is already a member of this team." );
            break;

        case S_OK:
        case XONLINETASK_S_SUCCESS:
            PushMessageWindow( "Invitation sent to player" );
            break;

        default:
            PushMessageWindow( "Unable to invite player to team" );
            break;
        }
        break;

    case EV_BUTTON_B: // Back out to the previous menu
        PopState();
        break;
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateSelectInviteTeam()
// Desc: Renders the list of teams
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateSelectInviteTeam()
{
    RenderControllingUser();

    RenderMenu( L"TEAMS TO INVITE PLAYER TO", NULL, 0, 0 );

    m_font.DrawText( 80, POS_SCREEN_TITLE_Y + DEFAULT_TEXT_PADDING,
                     COLOR_GREEN,
                     L"TEAM NAME",
                     XBFONT_LEFT );

    m_font.DrawText( 560, POS_SCREEN_TITLE_Y + DEFAULT_TEXT_PADDING,
                     COLOR_GREEN,
                     L"TEAM DESCRIPTION",
                     XBFONT_RIGHT );

    float fTeamListStartY = POS_SCREEN_TITLE_Y + ( DEFAULT_TEXT_PADDING * 2 );
    float fTeamNameStartX = 80.0f;
    float fTeamDescStartX = 560.0f;

    for( INT i = 0; i < (INT)m_dwTeamCount; ++i)
    {
        float fPosY = fTeamListStartY + ( DEFAULT_TEXT_PADDING * i );

        // Draw the name of the team
        m_font.DrawText( fTeamNameStartX, fPosY, COLOR_NORMAL,
                         m_rwTeamInfo[i].TeamProperties.wszTeamName,
                         XBFONT_LEFT );

        // Draw the team discription
        m_font.DrawText( fTeamDescStartX, fPosY, COLOR_NORMAL,
                         m_rwTeamInfo[i].TeamProperties.wszDescription,
                         XBFONT_RIGHT );
    }

    // If we have one or more teams
    // then allow the user to find the
    // roster of the selected team
    if( m_iItemSelected < (INT)m_dwTeamCount )
    {
        // Show selected item with a little triangle
        FLOAT fIconPosY = fTeamListStartY + ( DEFAULT_TEXT_PADDING * m_iItemSelected );

        m_font.DrawText( fTeamNameStartX, fIconPosY,
                         COLOR_POINTER, GLYPH_RIGHT_TICK, XBFONT_RIGHT );
    }

    // Bottom Help text
    RenderFooter( FOOTER_RENDER_SELECT | FOOTER_RENDER_CANCEL );
}


/////////////////
// State Inbox //
/////////////////

//-------------------------------------------------------------------------------------
// Name: EnterStateInbox()
// Desc: Retrieves all the messages waiting in live for the player
//-------------------------------------------------------------------------------------
VOID CXBoxSample::EnterStateInbox()
{
    // Step 1
    //
    // Start downloading all messages
    // and exit the function when done

    DWORD               dwFlags       = 0;

    m_dwMessageSelected = 0;
    m_dwNumMessages     = 0;

    XONLINE_NOTIFICATION_EX_INFO xnei;

    ZeroMemory( m_rwMessagesSummaries, sizeof( m_rwMessagesSummaries ) );

    // Start downloading any messages we may have
    // and continue until all messages are downloaded
    //
    // This is a "blocking" operation we
    // we have to pump the logon task
    // A real game could not block for
    // this operation
    do
    {
        XOnlineTaskContinue(m_hLogonTask );

        Sleep( 15 ); // Pause for a few so we don't throttle the task

        // check to see if the pending sync flag is set
        XOnlineGetNotificationEx( m_wControllingUser, &xnei, &dwFlags );

    } while( dwFlags & XONLINE_NOTIFICATION_STATE_FLAG_PENDING_SYNC );

    // Step 2
    //
    // Get the invites received

    HRESULT hrGetMessages = XOnlineMessageEnumerate(
                                m_wControllingUser,    // The controller port of the user cheking for messages
                                m_rwMessagesSummaries, // The array of messages to be populated
                                &m_dwNumMessages       // The number of messages received
                            );

    if( FAILED( hrGetMessages ) )
        m_dwNumMessages = 0;
}

//-------------------------------------------------------------------------------------
// Name: UpdateStateInbox()
// Desc: Allows the player to react to messages sent to them via XBox live
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateInbox( INT iUser, Event event )
{
    if ( iUser != (INT)m_wControllingUser )
        return;

    m_dwMessageSelected = GetMenuPosition( m_dwMessageSelected, m_dwNumMessages, event );

    switch( event )
    {
    case EV_BUTTON_A: // Accept or decline the invite
        // If this is not a team message, ignore it
        if( !( m_rwMessagesSummaries[m_dwMessageSelected].dwMessageFlags & XONLINE_MSG_FLAG_TEAM_CONTEXT ) )
            break;

        // Only deal with a message trying to recuit us
        if( m_rwMessagesSummaries[m_dwMessageSelected].bMsgType != XONLINE_MSG_TYPE_TEAM_RECRUIT )
            break;

        PushState( STATE_INVITE_DETAILS );

        break;

    case EV_BUTTON_B: // Return to the previous menu
        PopState();
        break;
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateInbox()
// Desc: Renders the message list
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateInbox()
{
    RenderControllingUser();

    RenderMenu( L"INBOX", NULL, 0, 0 );

    float fMessageListStartY   = POS_SCREEN_TITLE_Y + ( DEFAULT_TEXT_PADDING * 2 );
    float fMessageSenderStartX = 80.0f;
    float fMessageTypeStartX   = 560.0f;

    // Render the column headers
    m_font.DrawText( fMessageSenderStartX, POS_SCREEN_TITLE_Y + DEFAULT_TEXT_PADDING,
                     COLOR_GREEN,
                     L"SENDER NAME",
                     XBFONT_LEFT );

    m_font.DrawText( fMessageTypeStartX, POS_SCREEN_TITLE_Y + DEFAULT_TEXT_PADDING,
                     COLOR_GREEN,
                     L"MESSAGE TYPE",
                     XBFONT_RIGHT );


    // Render the summaries of the messages
    for( INT i = 0; i < (INT)m_dwNumMessages; ++i )
    {
        float fPosY = fMessageListStartY + ( DEFAULT_TEXT_PADDING * i );

        // Get the sender's name
        WCHAR szwGamerTag[XONLINE_GAMERTAG_SIZE];
        XBUtil_GetWide( m_rwMessagesSummaries[i].szSenderName,
                        szwGamerTag, XONLINE_GAMERTAG_SIZE );

        m_rwMessagesSummaries[i].dwMessageFlags;
        m_rwMessagesSummaries[i].qwMessageContext;

        // Get the message type
        WCHAR szwMessageType[MAX_TITLENAME_SIZE];

        ZeroMemory( szwMessageType, sizeof( szwMessageType ) );

        switch( m_rwMessagesSummaries[i].bMsgType )
        {
        case XONLINE_MSG_TYPE_FRIEND_REQUEST:
            lstrcpynW( szwMessageType, L"FRIEND REQUEST", MAX_TITLENAME_SIZE );
            break;
        case XONLINE_MSG_TYPE_GAME_INVITE:
            lstrcpynW( szwMessageType, L"GAME INVITE", MAX_TITLENAME_SIZE );
            break;
        case XONLINE_MSG_TYPE_COMP_REMINDER:
            lstrcpynW( szwMessageType, L"COMPETITION REMINDER", MAX_TITLENAME_SIZE );
            break;
        case XONLINE_MSG_TYPE_COMP_REQUEST:
            lstrcpynW( szwMessageType, L"COMPETITION REQUEST", MAX_TITLENAME_SIZE );
            break;
        case XONLINE_MSG_TYPE_LIVE_MESSAGE:
            lstrcpynW( szwMessageType, L"LIVE MESSAGE", MAX_TITLENAME_SIZE );
            break;
        case XONLINE_MSG_TYPE_TEAM_RECRUIT:
            lstrcpynW( szwMessageType, L"TEAM RECRUIT", MAX_TITLENAME_SIZE );
            break;
        default:
            lstrcpynW( szwMessageType, L"OTHER", MAX_TITLENAME_SIZE );
            break;
        };


        // Draw the message summary to the screen

        DWORD dwColor = ( m_rwMessagesSummaries[i].bMsgType == XONLINE_MSG_TYPE_TEAM_RECRUIT )
                            ? COLOR_NORMAL : COLOR_GREY;

        m_font.DrawText( fMessageSenderStartX, fPosY,
                     dwColor,
                     szwGamerTag,
                     XBFONT_LEFT );

        m_font.DrawText( fMessageTypeStartX, fPosY,
                     dwColor,
                     szwMessageType,
                     XBFONT_RIGHT );
    }


    // If we have one or more teams
    // then allow the user to find the
    // roster of the selected team
    if( m_dwMessageSelected < (INT)m_dwNumMessages )
    {
        // Show selected item with a little triangle
        FLOAT fIconPosY = fMessageListStartY + ( DEFAULT_TEXT_PADDING * m_dwMessageSelected );

        m_font.DrawText( fMessageSenderStartX, fIconPosY,
                         COLOR_POINTER, GLYPH_RIGHT_TICK, XBFONT_RIGHT );
    }

    RenderFooter( FOOTER_RENDER_CANCEL | FOOTER_RENDER_SELECT );
}


/////////////////////////
// State InviteDetails //
/////////////////////////

//-------------------------------------------------------------------------------------
// Name: EnterStateInviteDetails()
// Desc: Gets the details of the invite selected by the user
//-------------------------------------------------------------------------------------
VOID CXBoxSample::EnterStateInviteDetails()
{
    m_dwInviteResponseSelected = 0;

    CXBOnlineTask hDetailsTask;
    
    // Step 1
    //
    // Start getting the name of this team

    XUID xuidTeamInvitedTo;

    ZeroMemory( &xuidTeamInvitedTo,   sizeof( xuidTeamInvitedTo ) );
    ZeroMemory( &m_teamUserInvitedTo, sizeof( m_teamUserInvitedTo ) );
    ZeroMemory( &m_teamInfoInvitedTo, sizeof( m_teamInfoInvitedTo ) );

    xuidTeamInvitedTo.dwUserFlags = 0;
    xuidTeamInvitedTo.qwTeamID    = m_rwMessagesSummaries[m_dwMessageSelected].qwMessageContext;

    HRESULT hrGetDetails = XOnlineTeamEnumerate(
        m_wControllingUser,   // Controller port of the user making the call
        1,                    // Number of teams we are looking for
        &xuidTeamInvitedTo, // XUID of the team we are trying to find details of
        NULL,                 // Work event
        &hDetailsTask          // Task to assign to
    );

    assert( SUCCEEDED( hrGetDetails ) );


    // Step 2
    //
    // Pump the task until it's finished

    do
    {
        hrGetDetails = hDetailsTask.Continue();
    } while ( hrGetDetails == XONLINETASK_S_RUNNING );

    assert( SUCCEEDED( hrGetDetails ) );


    // Step 3
    //
    // Get The enumeration results
    
    DWORD dwNumTeamsRead = 0;
    hrGetDetails = XOnlineTeamEnumerateGetResults(
                    hDetailsTask,        // The enumeration task
                    &dwNumTeamsRead,     // The number of teams read
                    &xuidTeamInvitedTo // An array of all the team XUIDs read
                 );

    assert( dwNumTeamsRead == 1 );
    assert( SUCCEEDED( hrGetDetails ) );


    // Step 4
    //
    // Get the team details

    hrGetDetails = XOnlineTeamGetDetails(
        hDetailsTask,        // Task handle from the enumerate task
        xuidTeamInvitedTo, // XUID of the team we are finding details of
        &m_teamInfoInvitedTo // Structure to store the info in
    );

    assert( SUCCEEDED( hrGetDetails ) );

    hDetailsTask.Close();
}

//-------------------------------------------------------------------------------------
// Name: UpdateStateInviteDetails()
// Desc: Allows the user to respond to an individual invite
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateInviteDetails( INT iUser, Event event )
{
    if( iUser != m_wControllingUser )
        return;

    m_dwInviteResponseSelected = GetMenuPosition( m_dwInviteResponseSelected,
                                                  NUM_ITEMS_INVITE_MENU,
                                                  event );

    switch( event )
    {
    case EV_BUTTON_A:
        switch( m_dwInviteResponseSelected )
        {
        case MENU_INVITE_ACCEPT:
            switch( ProcessInvite( m_dwMessageSelected, XONLINE_PEER_ANSWER_YES ) )
            {
            case XONLINE_E_TEAMS_TEAM_FULL:
                PushMessageWindow( "Unable to join team. The team is full." );
                break;

            case XONLINE_E_TEAMS_USER_TEAMS_FULL:
                PushMessageWindow( "You may not join any more teams." );
                break;

            case XONLINE_E_TEAMS_USER_ALREADY_EXISTS:
                PushMessageWindow( "Unable to accept. You are already a member." );
                break;

            case S_OK:
                PopState( TRUE );
                PushMessageWindow( "Accepted team invite" );
                break;

            default:
                PushMessageWindow( "Unable to accept team invite" );
                break;
            }

            break;

        case MENU_INVITE_DECLINE:
            if ( SUCCEEDED( ProcessInvite( m_dwMessageSelected, XONLINE_PEER_ANSWER_NO ) ) )
            {
                PopState( TRUE );
                PushMessageWindow( "Declined team invite" );
            }
            else
            {
                PushMessageWindow( "Unable to decline team invite" );
            }

            break;

        case MENU_INVITE_NEVER:
            if( SUCCEEDED( ProcessInvite( m_dwMessageSelected, XONLINE_PEER_ANSWER_NEVER ) ) )
            {
                PopState( TRUE );
                PushMessageWindow( "Declined this invite, and all future invites from player." );
            }
            else
            {
                PushMessageWindow( "Unable to decline this, or future invites from player" );
            }

            break;
        }

        break;

    case EV_BUTTON_B:
        PopState( TRUE );
        break;
    }
}
    
//-------------------------------------------------------------------------------------
// Name: RenderStateInviteDetails()
// Desc: Draws the details of an invite along with a menu of options
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateInviteDetails()
{
    RenderControllingUser();

    RenderMenu( L"INVITE RESPONSE",
                (const WCHAR**)MENU_INVITE,
                NUM_ITEMS_INVITE_MENU,
                m_dwInviteResponseSelected );

    FLOAT fInviteTeamNamePosY = POS_SCREEN_TITLE_Y + ( 2.0f * DEFAULT_TEXT_PADDING );

    // Show the team name we have been invited to
    m_font.DrawText( SCREEN_CENTER_X, fInviteTeamNamePosY,
                     COLOR_NORMAL,
                     L"Invite To Team: ",
                     XBFONT_RIGHT );

    m_font.DrawText( SCREEN_CENTER_X, fInviteTeamNamePosY,
                     COLOR_GREEN,
                     m_teamInfoInvitedTo.TeamProperties.wszTeamName,
                     XBFONT_LEFT );

    // Show the team information:
    FLOAT fInviteTeamInfoPosY = POS_SCREEN_TITLE_Y + ( 8.0f * DEFAULT_TEXT_PADDING );

    // Team Description
    m_font.DrawText( SCREEN_CENTER_X, fInviteTeamInfoPosY,
                     COLOR_NORMAL,
                     L"Description: ",
                     XBFONT_RIGHT );

    m_font.DrawText( SCREEN_CENTER_X, fInviteTeamInfoPosY,
                     COLOR_GREEN,
                     m_teamInfoInvitedTo.TeamProperties.wszDescription,
                     XBFONT_LEFT );

    fInviteTeamInfoPosY += TEXT_PADDING_INVITE_INFO;


    // Team Motto:
    m_font.DrawText( SCREEN_CENTER_X, fInviteTeamInfoPosY,
                     COLOR_NORMAL,
                     L"Motto: ",
                     XBFONT_RIGHT );

    m_font.DrawText( SCREEN_CENTER_X, fInviteTeamInfoPosY,
                     COLOR_GREEN,
                     m_teamInfoInvitedTo.TeamProperties.wszMotto,
                     XBFONT_LEFT );

    fInviteTeamInfoPosY += TEXT_PADDING_INVITE_INFO;


    // Homepage
    m_font.DrawText( SCREEN_CENTER_X, fInviteTeamInfoPosY,
                     COLOR_NORMAL,
                     L"Homepage: ",
                     XBFONT_RIGHT );

    m_font.DrawText( SCREEN_CENTER_X, fInviteTeamInfoPosY,
                     COLOR_GREEN,
                     m_teamInfoInvitedTo.TeamProperties.wszURL,
                     XBFONT_LEFT );

    fInviteTeamInfoPosY += TEXT_PADDING_INVITE_INFO;


    // Number of members:
    WCHAR      szwMemberCount[XONLINE_GAMERTAG_SIZE];

    wsprintfW( szwMemberCount, L"%d", m_teamInfoInvitedTo.dwMemberCount, XONLINE_GAMERTAG_SIZE );

    m_font.DrawText( SCREEN_CENTER_X, fInviteTeamInfoPosY,
                     COLOR_NORMAL,
                     L"Members: ",
                     XBFONT_RIGHT );

    m_font.DrawText( SCREEN_CENTER_X, fInviteTeamInfoPosY,
                     COLOR_GREEN,
                     szwMemberCount,
                     XBFONT_LEFT );

    fInviteTeamInfoPosY += TEXT_PADDING_INVITE_INFO;


    // Show the date of when the team was created
    SYSTEMTIME systemTime;
    WCHAR      szwCreationDate[XONLINE_GAMERTAG_SIZE];

    FileTimeToSystemTime( &m_teamInfoInvitedTo.CreationTime, &systemTime );

    _snwprintf( szwCreationDate, XONLINE_GAMERTAG_SIZE, L"%d/%d/%d",
        systemTime.wMonth, systemTime.wDay, systemTime.wYear );

    m_font.DrawText( SCREEN_CENTER_X, fInviteTeamInfoPosY,
                     COLOR_NORMAL,
                     L"Created: ",
                     XBFONT_RIGHT );

    m_font.DrawText( SCREEN_CENTER_X, fInviteTeamInfoPosY,
                     COLOR_GREEN,
                     szwCreationDate,
                     XBFONT_LEFT );


    // Bottom Help text
    RenderFooter( FOOTER_RENDER_SELECT | FOOTER_RENDER_CANCEL );
}

///////////////////////
// State ViewMyTeams //
///////////////////////

//-------------------------------------------------------------------------------------
// Name: EnterStateViewMyTeams()
// Desc: Initializes the MyTeams screen after the team list has been read
//-------------------------------------------------------------------------------------
VOID CXBoxSample::EnterStateViewMyTeams()
{
    m_iItemSelected = 0;
}

//-------------------------------------------------------------------------------------
// Name: UpdateStateViewMyTeams
// Desc: Updates the ViewMyTeams state. Launches any submenus and
//       commands the user may have. Allows the user to select a
//       team and view it's roster.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateViewMyTeams( INT iUser, Event event)
{
    if( iUser != m_wControllingUser )
        return;

    m_iItemSelected = GetMenuPosition( m_iItemSelected, m_dwTeamCount, event);

    switch( event )
    {
    case EV_BUTTON_A: // View the team roster
        if( m_dwTeamCount < 1 )
            break;

        if( GetTeamRoster() )
            PushState( STATE_VIEW_TEAM_ROSTER );
        else
            PushMessageWindow( "Unable to retrieve team roster." );

        break;

    case EV_BUTTON_B:
        PopState( TRUE );
        break;

    case EV_BUTTON_Y: // Change team statistics
        if( m_dwTeamCount > 0 )
        {
            // Variables to keep the statistics
            // added to the team
            LONG     lKills   = 0;
            LONG     lDeaths  = 0;
            LONG     lAssists = 0;
            LONGLONG llRating = 0;

            if( WriteTeamStatistics( lKills, lDeaths, lAssists, llRating ) )
            {
                CHAR szMessage[256];
                _snprintf( szMessage, 256 ,
                        "%d Kills, %d Deaths, %d Assists, %d Rating added to stats",
                        lKills, lDeaths, lAssists, llRating );

                PushMessageWindow( szMessage );
            }
            else
            {
                PushMessageWindow( "Unable to write new team statistics." );
            }
        }

        break;

    case EV_BUTTON_BLACK:
        // Delete the entire team!
        // This removes the team from ALL of xbox live
        if( DeleteTeam( m_rwTeamXUIDS[m_iItemSelected] ) == S_OK )
        {
            PushMessageWindow( "The team was deleted." );

            // Reinitialize the data
            GetTeamList();
            EnterStateViewMyTeams();
            return;
        }
        else
        {
            PushMessageWindow( "Unable to delete the team." );
        }
        break;

    case EV_BUTTON_WHITE: // Change team properties
        // Only attempt to change properties if
        // we have a team to change
        if( m_dwTeamCount < 1 )
            break;

        if( ChangeTeamProperties( iUser ) )
        {
            // Cause a re-init to see the new info
            GetTeamList();
        }
        else
        {
            PushMessageWindow( "Unable to change team properties." );
        }
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateViewMyTeams()
// Desc: Renders the list of teams the user is a member of and the
//       input options available.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateViewMyTeams()
{
    RenderControllingUser();

    RenderMenu( L"MY TEAMS", NULL, 0, 0 );

    m_font.DrawText( 80, POS_SCREEN_TITLE_Y + DEFAULT_TEXT_PADDING,
                     COLOR_GREEN,
                     L"TEAM NAME",
                     XBFONT_LEFT );

    m_font.DrawText( 560, POS_SCREEN_TITLE_Y + DEFAULT_TEXT_PADDING,
                     COLOR_GREEN,
                     L"TEAM DESCRIPTION",
                     XBFONT_RIGHT );

    float fTeamListStartY = POS_SCREEN_TITLE_Y + ( DEFAULT_TEXT_PADDING * 2 );
    float fTeamNameStartX = 80.0f;
    float fTeamDescStartX = 560.0f;

    for( INT i = 0; i < (INT)m_dwTeamCount; ++i)
    {
        float fPosY = fTeamListStartY + ( DEFAULT_TEXT_PADDING * i );

        // Draw the name of the team
        m_font.DrawText( fTeamNameStartX, fPosY, COLOR_NORMAL,
                         m_rwTeamInfo[i].TeamProperties.wszTeamName,
                         XBFONT_LEFT );

        // Draw the team discription
        m_font.DrawText( fTeamDescStartX, fPosY, COLOR_NORMAL,
                         m_rwTeamInfo[i].TeamProperties.wszDescription,
                         XBFONT_RIGHT );
    }

    // If we have one or more teams
    // then allow the user to find the
    // roster of the selected team
    if( m_iItemSelected < (INT)m_dwTeamCount )
    {
        // Show selected item with a little triangle
        FLOAT fIconPosY = fTeamListStartY + ( DEFAULT_TEXT_PADDING * m_iItemSelected );

        m_font.DrawText( fTeamNameStartX, fIconPosY,
                         COLOR_POINTER, GLYPH_RIGHT_TICK, XBFONT_RIGHT );
    }

    // Show that the user can use the white to change team properties
    // or the black button to delete the team

    float fTextPosY = POS_FOOTER_Y - DEFAULT_TEXT_PADDING;

    m_font.DrawText( POS_FOOTER_RIGHT, fTextPosY,
                     COLOR_NORMAL, GLYPH_BLACK_BUTTON L" Delete ENTIRE team",
                     XBFONT_RIGHT );

    m_font.DrawText( POS_FOOTER_LEFT, fTextPosY,
                     COLOR_NORMAL, GLYPH_WHITE_BUTTON L" change properties", 
                     XBFONT_LEFT );

    m_font.DrawText( SCREEN_CENTER_X, POS_FOOTER_Y,
                     COLOR_NORMAL, GLYPH_Y_BUTTON L" add to team statistics",
                     XBFONT_CENTER_X );

    // Bottom Help text
    RenderFooter( FOOTER_RENDER_SELECT | FOOTER_RENDER_CANCEL );
}

//////////////////////////
// State ViewTeamRoster //
//////////////////////////

//-------------------------------------------------------------------------------------
// Name: UpdateStateViewTeamRoster()
// Desc: Allows the user to issue commands to the roster (delete team, kick members)
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateViewTeamRoster( INT iUser, Event event)
{
    if( iUser != m_wControllingUser )
        return;

    switch( event )
    {
    case EV_UP: // Move the roster list up until we hit the top
        m_dwTeamMemberSelected = ( m_dwTeamMemberSelected > 0 ) ? ( m_dwTeamMemberSelected - 1 ) : 0;

        if( m_dwTeamMemberSelected < m_dwRosterRenderStart )
            m_dwRosterRenderStart  = ( m_dwRosterRenderStart > 0 ) ? ( m_dwRosterRenderStart - 1 ) : 0;

        break;

    case EV_DOWN: // Move the roster list down until we hit the bottom
        m_dwTeamMemberSelected = ( m_dwTeamMemberSelected < ( m_dwTeamMemberCount - 1 ) ) ? ( m_dwTeamMemberSelected + 1 ) : m_dwTeamMemberSelected;
        
        if( m_dwTeamMemberSelected >= ( m_dwRosterRenderStart + NUM_ENTRIES_PER_SCREEN ) )
        {
            ++m_dwRosterRenderStart;

            if( m_dwRosterRenderStart > ( m_dwTeamMemberCount - NUM_ENTRIES_PER_SCREEN ) )
                m_dwRosterRenderStart = m_dwTeamMemberCount - NUM_ENTRIES_PER_SCREEN;
        }
        break;

    case EV_BUTTON_A:
        PushState( STATE_TEAM_MEMBER_OPS );
        break;

    case EV_BUTTON_B:
        m_phTeamRosterTask.Close();
        GetTeamList();
        PopState( TRUE );
        break;
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateViewTeamRoster()
// Desc: Renders the team roster to the screen.
//       Calls the function to get team member details to show specific details
//       about each member.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateViewTeamRoster()
{
    RenderControllingUser();

    RenderMenu( L"TEAM ROSTER", NULL, 0, 0 );

    float fTeamListStartY = POS_SCREEN_TITLE_Y + ( DEFAULT_TEXT_PADDING * 2 );
    float fTeamNameStartX = 80.0f;
    float fTeamDescStartX = 560.0f;

    m_font.DrawText( fTeamNameStartX, POS_SCREEN_TITLE_Y + DEFAULT_TEXT_PADDING,
                     COLOR_GREEN,
                     L"MEMBER NAME",
                     XBFONT_LEFT );

    m_font.DrawText( SCREEN_CENTER_X, POS_SCREEN_TITLE_Y + DEFAULT_TEXT_PADDING,
                     COLOR_GREEN,
                     L"RANK",
                     XBFONT_LEFT );

    m_font.DrawText( fTeamDescStartX, POS_SCREEN_TITLE_Y + DEFAULT_TEXT_PADDING,
                     COLOR_GREEN,
                     L"JOIN DATE",
                     XBFONT_RIGHT );

    HRESULT hrMemberDetail = S_OK;

    XONLINE_TEAM_MEMBER memberInfo;

    // If we are starting the render from a position
    // other than the first user in the list
    // the draw a little arrow on the side telling
    // the user they can scroll up
    if( m_dwRosterRenderStart > 0 )
    {
        m_font.DrawText( fTeamNameStartX, fTeamListStartY,

            COLOR_HIGHLIGHT, GLYPH_UP_ARROW L"    ", XBFONT_RIGHT );
    }

    // Get the details of each team member
    for( DWORD i = m_dwRosterRenderStart; i < m_dwTeamMemberCount; ++i )
    {
        // Stop rendering if we hit the maximum number
        // team members viewable at once
        if( ( i - m_dwRosterRenderStart ) >= NUM_ENTRIES_PER_SCREEN )
        {
            // If more team roster entries are below
            // the last entry drawn, then add a down
            // arrow on the side telling the user
            // can scroll down
            float fDownArrowY = fTeamListStartY
                                + ( DEFAULT_TEXT_PADDING * ( NUM_ENTRIES_PER_SCREEN - 1 ) );

            m_font.DrawText( fTeamNameStartX, fDownArrowY,
                             COLOR_HIGHLIGHT, GLYPH_DOWN_ARROW L"   ", XBFONT_RIGHT );

            break;
        }

        hrMemberDetail = XOnlineTeamMemberGetDetails(
                            m_phTeamRosterTask, // The task used to retrieve the team roster
                            m_rwTeamMembers[i], // The XUID of the member to retrieve
                            &memberInfo         // The structure to populate with details
                         );

        // Bail if we fail
        if( hrMemberDetail != S_OK )
            break;

        // Render to the screen!
        INT   iScreenItem = ( i - m_dwRosterRenderStart );
        FLOAT fPosY       = fTeamListStartY + ( DEFAULT_TEXT_PADDING * iScreenItem );

        // Allow the user to move the selector
        // up and down to select a specific user
        // to give an permissions to or
        // to remove from the team
        //
        // Show selected item with a little triangle
        FLOAT fIconPosY = fTeamListStartY + ( DEFAULT_TEXT_PADDING * ( m_dwTeamMemberSelected - m_dwRosterRenderStart ) );

        m_font.DrawText( fTeamNameStartX, fIconPosY,
                         COLOR_POINTER, GLYPH_RIGHT_TICK, XBFONT_RIGHT );


        WCHAR szwGamerTag[XONLINE_GAMERTAG_SIZE];
        XBUtil_GetWide( memberInfo.szGamertag, szwGamerTag, XONLINE_GAMERTAG_SIZE );

        m_font.DrawText( fTeamNameStartX, fPosY, COLOR_NORMAL,
                         szwGamerTag,
                         XBFONT_LEFT );

        // create a string representing their privilege level
        if( memberInfo.TeamMemberProperties.dwPrivileges & XONLINE_TEAM_DELETE_MEMBER )
            wcscpy( szwGamerTag, L"Owner" );
        else if( memberInfo.TeamMemberProperties.dwPrivileges & XONLINE_TEAM_RECRUIT_MEMBERS )
            wcscpy( szwGamerTag, L"Recruiter" );
        else 
            wcscpy( szwGamerTag, L"Peon" );

        m_font.DrawText( SCREEN_CENTER_X, fPosY, COLOR_NORMAL,
                     szwGamerTag,
                     XBFONT_LEFT );

        // Show the date of when the member joined
        SYSTEMTIME systemTime;

        FileTimeToSystemTime( &memberInfo.JoinDate, &systemTime );

        _snwprintf( szwGamerTag, XONLINE_GAMERTAG_SIZE, L"%d/%d/%d",
            systemTime.wMonth, systemTime.wDay, systemTime.wYear );

        m_font.DrawText( fTeamDescStartX, fPosY, COLOR_NORMAL,
                         szwGamerTag,
                         XBFONT_RIGHT );
    }


    // Bottom Help text
    RenderFooter( FOOTER_RENDER_SELECT | FOOTER_RENDER_CANCEL );
}

/////////////////////////
// State TeamMemberOps //
/////////////////////////

//-------------------------------------------------------------------------------------
// Name: UpdateStateTeamMemberOps
// Desc: Allows the user to change permissions or remove a member from the team
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateTeamMemberOps( INT iUser, Event event )
{
    m_localUsers[iUser].m_iCurSelection = GetMenuPosition( m_localUsers[iUser].m_iCurSelection,
                                                           NUM_ITEMS_MEMBER_OPS_MENU, event );

    if( iUser != (INT)m_wControllingUser ) return;

    XUID    xuidTeamMember     = m_rwTeamMembers[m_dwTeamMemberSelected];
    XUID    xuidTeam           = m_rwTeamXUIDS[m_iItemSelected];
    HRESULT hrPermissionChange = S_OK;

    switch( event )
    {
    case EV_BUTTON_A:
        switch( m_localUsers[iUser].m_iCurSelection )
        {
        case MENU_MEMBER_OPS_SET_OWNER:
            hrPermissionChange = SetPermissions( xuidTeam, xuidTeamMember,
                            ( XONLINE_TEAM_DELETE
                            | XONLINE_TEAM_MODIFY_DATA
                            | XONLINE_TEAM_MODIFY_MEMBER_PERMISSIONS
                            | XONLINE_TEAM_DELETE_MEMBER
                            | XONLINE_TEAM_RECRUIT_MEMBERS ) );
            break;

        case MENU_MEMBER_OPS_SET_RECRUITER:
            hrPermissionChange = SetPermissions( xuidTeam, xuidTeamMember,
                            XONLINE_TEAM_RECRUIT_MEMBERS );
            break;

        case MENU_MEMBER_OPS_SET_PEON:
            hrPermissionChange = SetPermissions( xuidTeam, xuidTeamMember, 0 );
            break;

        case MENU_MEMBER_OPS_KICKOFF:
            if( SUCCEEDED( RemoveTeamMember( xuidTeam, xuidTeamMember ) ) )
            {
                GetTeamRoster();
                PopState( TRUE );
                PushMessageWindow( "Player kicked off team." );

                // Re-init the menu
                EnterStateViewTeamRoster();
            }
            else
            {
                PushMessageWindow( "Unable to remove player." );
            }
            return;
            break;

        default:
            assert( 0 && "Illegal menu choice!" );
        }

        if( SUCCEEDED( hrPermissionChange ) )
        {
            GetTeamRoster();
            PopState();
            PushMessageWindow( "Permissions successfully changed." );
        }
        else
        {
            PushMessageWindow( "Unable to change permissions." );
        }
        break;

    case EV_BUTTON_B:
        PopState();
        break;
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateTeamMemberOps
// Desc: Shows the menu of member operations the user has available
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateTeamMemberOps()

{
    RenderControllingUser();

    XONLINE_TEAM_MEMBER memberInfo;

    ZeroMemory( &memberInfo, sizeof( memberInfo ) );

    HRESULT hrMemberDetail = XOnlineTeamMemberGetDetails(
                                // The task used to retrieve the team roster
                                m_phTeamRosterTask,
                                // The XUID of the member to retrieve
                                m_rwTeamMembers[m_dwTeamMemberSelected],
                                // The structure to populate with details
                                &memberInfo
                              );

    assert( SUCCEEDED( hrMemberDetail ) );

    WCHAR strUserName[XONLINE_GAMERTAG_SIZE];
    XBUtil_GetWide( memberInfo.szGamertag,
                    strUserName, XONLINE_GAMERTAG_SIZE );

    WCHAR strMenuTitle[XONLINE_GAMERTAG_SIZE + 22];
    wsprintfW( strMenuTitle, L"TEAM MEMBER OPERATIONS for %s", strUserName );

    RenderMenu( strMenuTitle,
                (const WCHAR**)MENU_MEMBER_OPS,
                NUM_ITEMS_MEMBER_OPS_MENU,
                m_localUsers[m_wControllingUser].m_iCurSelection );

    RenderFooter( FOOTER_RENDER_SELECT | FOOTER_RENDER_CANCEL );
}

/////////////////////////
// State MessageWindow //
/////////////////////////

//-------------------------------------------------------------------------------------
// Name: UpdateStateMessageWindow
// Desc: Waits for the user to dismiss the message window
//       then returns to the calling state.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateMessageWindow( INT iUser, Event event )
{
    if( iUser != (INT)m_wControllingUser ) return;

    switch( event )
    {
    case EV_BUTTON_A:
    case EV_BUTTON_B:
        PopState();
        break;
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateMessageWindow()
// Desc: Draws the message supplied to the screen.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateMessageWindow()
{
    m_font.DrawText( SCREEN_CENTER_X, SCREEN_CENTER_Y, COLOR_NORMAL,
                     m_szGameMessage,
                     XBFONT_CENTER_X );

    RenderFooter( FOOTER_RENDER_SELECT | FOOTER_RENDER_CANCEL );
}

// Extra rendering functions

//-------------------------------------------------------------------------------------
// Name: RenderHeader()
// Desc: Renders a small header at the top of the screen that is shown in
//       most of the UI states
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderHeader()
{
    // Render a header giving the user the name of the demo
    m_font.DrawText( POS_HEADER_LEFT, POS_HEADER_Y, COLOR_NORMAL,
                     L"Teams",
                     XBFONT_LEFT );
}

//-------------------------------------------------------------------------------------
// Name: RenderControllingUser()
// Desc: Renders the name of the user who has control of the UI in the UL corner
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderControllingUser()
{
    assert( ( m_wControllingUser >= 0 ) && "Invalid controlling user" );
    assert( ( m_wControllingUser < MAX_USERS ) && "Invalid controlling user" );

    WCHAR strUserName[XONLINE_GAMERTAG_SIZE];
    XBUtil_GetWide( m_rwStoredUsers[m_localUsers[m_wControllingUser].m_wUserIndex].szGamertag,
                    strUserName, XONLINE_GAMERTAG_SIZE );

    // Render a header giving the user the name of the demo
    m_font.DrawText( POS_HEADER_RIGHT, POS_HEADER_Y, COLOR_NORMAL,
                     strUserName, // The user who has control of the menu
                     XBFONT_RIGHT );
}

//-------------------------------------------------------------------------------------
// Name: RenderFooter()
// Desc: Renders a footer at the bottom of the screen. Takes a bitflag to
//       determine which items to render.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderFooter( WORD flags )
{
    if( flags & FOOTER_RENDER_CANCEL )
    {
        // Bottom Help text
        m_font.DrawText( POS_FOOTER_LEFT, POS_FOOTER_Y,
                         COLOR_NORMAL, GLYPH_B_BUTTON L" back", 
                         XBFONT_LEFT );
    }

    if( flags & FOOTER_RENDER_SELECT )
    {
        m_font.DrawText( POS_FOOTER_RIGHT, POS_FOOTER_Y,
                         COLOR_NORMAL, GLYPH_A_BUTTON L" select", 
                         XBFONT_RIGHT );
    }
}


// Overloaded functions defined by the application
// class to execute game logic and rendering

//-------------------------------------------------------------------------------------
// Name: Render()
// Desc: Render the game and the proper screen.
//-------------------------------------------------------------------------------------
HRESULT CXBoxSample::Render()
{
    // Clear the framebuffer
    m_pd3dDevice->Clear( 0L, NULL,
                         D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 
                         m_bgColor, 1.0f, 0L );

    RenderHeader();

    // Render the screen for the current state
    switch( m_state )
    {
    case STATE_SELECT_ACCOUNT:      RenderStateSelectAccount();     break;
    case STATE_LOGIN:               RenderStateLogin();             break;
    case STATE_LOGIN_FAILED:        RenderStateLoginFailed();       break;
    case STATE_NETWORK_ERROR:       RenderStateNetworkError();      break;
    case STATE_GAME_SETUP:          RenderStateGameSetup();         break;
    case STATE_TEAMS_LEADERBOARD:   RenderStateTeamsLeaderboard();  break;
    case STATE_TEAMS:               RenderStateTeams();             break;
    case STATE_RECENT_PLAYERS:      RenderStateRecentPlayers();     break;
    case STATE_SELECT_INVITE_TEAM:  RenderStateSelectInviteTeam();  break;
    case STATE_INBOX:               RenderStateInbox();             break;
    case STATE_INVITE_DETAILS:      RenderStateInviteDetails();     break;
    case STATE_VIEW_MY_TEAMS:       RenderStateViewMyTeams();       break;
    case STATE_VIEW_TEAM_ROSTER:    RenderStateViewTeamRoster();    break;
    case STATE_TEAM_MEMBER_OPS:     RenderStateTeamMemberOps();     break;
    case STATE_MESSAGE_WINDOW:      RenderStateMessageWindow();     break;
    default:
        RenderStateNetworkError();
        break; //assert(0 && "Unknown/illegal state!");
    };

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );
    
    return S_OK;
}

//-------------------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Update the game logic of the game for one tick/frame
//-------------------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    for( INT iUser = 0; iUser < MAX_USERS; ++iUser)
    {
        // Only allow controller input from a user who is
        // currently logged into xbox live when in gameplay
        // menus that depend on only one user controlling
        // the menu or that need to know which player is the
        // controlling player
        switch( m_state )
        {
        case STATE_SELECT_ACCOUNT:
        case STATE_LOGIN:
        case STATE_LOGIN_FAILED:
        case STATE_NETWORK_ERROR:
            break;

        default:
            if( !m_localUsers[iUser].m_bSignedIn )
                continue;
        }

        Event ev = GetEvent( iUser );

        switch( m_state )
        {
        case STATE_SELECT_ACCOUNT:      UpdateStateSelectAccount( iUser, ev );      break;
        case STATE_LOGIN:               UpdateStateLogin( ev );                     break;
        case STATE_LOGIN_FAILED:        UpdateStateLoginFailed( ev );               break;
        case STATE_NETWORK_ERROR:       UpdateStateNetworkError( ev );              break;
        case STATE_GAME_SETUP:          UpdateStateGameSetup( iUser, ev );          break;
        case STATE_TEAMS_LEADERBOARD:   UpdateStateTeamsLeaderboard( iUser, ev );   break;
        case STATE_TEAMS:               UpdateStateTeams( iUser, ev );              break;
        case STATE_RECENT_PLAYERS:      UpdateStateRecentPlayers( iUser, ev );      break;
        case STATE_SELECT_INVITE_TEAM:  UpdateStateSelectInviteTeam( iUser, ev );   break;
        case STATE_INBOX:               UpdateStateInbox( iUser, ev );              break;
        case STATE_INVITE_DETAILS:      UpdateStateInviteDetails( iUser, ev );      break;
        case STATE_VIEW_MY_TEAMS:       UpdateStateViewMyTeams( iUser, ev );        break;
        case STATE_VIEW_TEAM_ROSTER:    UpdateStateViewTeamRoster( iUser, ev );     break;
        case STATE_TEAM_MEMBER_OPS:     UpdateStateTeamMemberOps( iUser, ev );      break;
        case STATE_MESSAGE_WINDOW:      UpdateStateMessageWindow( iUser, ev );      break;
        default:
            assert(0 && "Unknown/illegal state!");
        };

        // If the player is signed in, check the status of the network
        // and report any found network errors
        if( m_localUsers[iUser].m_bSignedIn )
        {
            if( !SUCCEEDED( m_hLogonTask.Continue() ) )
            {
                m_localUsers[iUser].m_bSignedIn = FALSE;

                PushState( STATE_NETWORK_ERROR );
            }
        }
    }

    return S_OK;
}

//-------------------------------------------------------------------------------------
// Name: Initialize()
// Desc: Setup the clean initial values for the
//       member variables of the game class
//-------------------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    m_bgColor                        = COLOR_BLUE;
    m_bUsersSignedIn                 = FALSE;
    m_state                          = NUM_STATES;
    m_iItemSelected                  = 0;
    m_dwNumStoredUsers               = 0;
    m_wControllingUser               = 0;

    m_bUsersSigningIn                = FALSE;
    m_bUsersSignedIn                 = FALSE;
    m_iSignInResult                  = S_OK;

    ZeroMemory( m_stateStack, sizeof( m_stateStack ) );

    m_wStateStackSize                = 0;
    m_rwTeamInfo                     = new XONLINE_TEAM[XONLINE_MAX_TEAM_COUNT];

    for( WORD wUser = 0; wUser < MAX_USERS; ++wUser )
    {
        m_localUsers[wUser].m_bSignedIn     = FALSE;
    }

    XBUtil_GetWide( "", m_szGameMessage, MAX_MESSAGE_LENGTH );

    // Initialize the network stack
    if( FAILED( XBNet_OnlineInit( 0 ) ) )
        return E_FAIL;


    // Create the font
    if( FAILED( m_font.Create( "Font.xpr" ) ) )
        return E_FAIL;


    // Initialize Xbox Live!

    // Wait for any inserted MUs to mount
    while( XGetDeviceEnumerationStatus() == XDEVICE_ENUMERATION_BUSY );
    
    // Before any of the Xbox online APIs can be used, XOnlineStartup must be 
    // called.  XOnlineStartup automatically calls XNetStartup and 
    // WSAStartup with default parameters in order to initialize the 
    // Xbox Secure Network Libary and the Winsock layer. To specify non-default
    // startup parameters for XNetStartup or WSAStartup, call those functions 
    // prior to calling XOnlineStartup.
    
    HRESULT hrStartup = XOnlineStartup( NULL );

    if( !SUCCEEDED( hrStartup ) ) return E_FAIL;

    PushState( STATE_SELECT_ACCOUNT );

    return S_OK;
}

CXBoxSample::~CXBoxSample()
{
    delete [] m_rwTeamInfo;
}

//-------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program.
//-------------------------------------------------------------------------------------
VOID __cdecl main()
{
    CXBoxSample xbApp;

    if( FAILED( xbApp.Create() ) )
        return;

    xbApp.Run();
}