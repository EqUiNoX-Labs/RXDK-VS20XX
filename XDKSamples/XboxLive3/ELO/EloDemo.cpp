//-------------------------------------------------------------------------------------
// File: EloDemo.cpp
//
// Desc: This sample demonstrates how to use a ratings statistic
//       known as "Elo Rankings". To implement Elo Rankings
//       the Arbitration and Match-making services are used.
//
//       Elo Rankings are a comparitive way to adjust the ranking of
//       two players based on their current ranking and the expected
//       outcome of the match.
//
//       If a player with a higher ranking is defeated by a player
//       with a lower ranking, the the defeated player moves down
//       the rankings while the victor gains ranking.
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
#include "EloDemo.h"

//-------------------------------------------------------------------------------------
// Name: class MatchInAddr()
// Desc: Predicate functor used to match on IN_ADDRs in player lists
//-------------------------------------------------------------------------------------
struct MatchInAddr
{
    IN_ADDR ia;
    
    explicit MatchInAddr( IN_ADDR inaddr )
    {
        ia = inaddr;
    }
    
    bool operator()( const CXBNetPlayerInfo& playerInfo ) const
    {
        return playerInfo.inAddr.s_addr == ia.s_addr;
    }
};

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

    while ( hrTask == XONLINETASK_S_RUNNING )
    {
        // In a real game, this would be part of your game loop-
        // you wouldn't block on this.
        // The logon task should also be pumped inside this loop.

        hrTask = Task.Continue();
        if ( FAILED( hrTask ))
        {
            *pHR = hrTask;
            return FALSE;
        }
        
        // put in a delay of about 1 frame so as not to
        // spam with MessageSendGetProgress
        Sleep( 15 );
    }
    
    if ( hrTask != XONLINETASK_S_SUCCESS )
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
INT CXBoxSample::StartSignIn( WORD wUserIndex )
{
    // Sanity check for the account index
    if ( ( wUserIndex < 0 ) || ( wUserIndex >= (INT)m_dwNumStoredUsers ) )
    {
        return E_ACCOUNT_ERROR;
    }

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
    XONLINE_USER rwLogonUsers[ XONLINE_MAX_LOGON_USERS ] = { 0 }; // Initially zeroed

    rwLogonUsers[0] = m_rwStoredUsers[m_wCurUserIndex];

    HRESULT hrLogon = XOnlineLogon( rwLogonUsers,
                                    SERVICES, NUM_SERVICES,
                                    NULL, &m_hLogonTask );

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

    HRESULT hrLogon = XOnlineTaskContinue( m_hLogonTask );

    // Check to make sure the logon is proceeding
    // without error, and act on any errors found

    if ( hrLogon == XONLINETASK_S_RUNNING )
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
        XOnlineTaskClose( m_hLogonTask );
        m_bSignedIn = FALSE;
        return E_NETWORK_ERROR;

    case XONLINE_E_LOGON_CANNOT_ACCESS_SERVICE:
    case XONLINE_E_LOGON_INVALID_USER:
    default:
        // Some other error - title is free to allow access to dash
        XOnlineTaskClose( m_hLogonTask );
        m_bSignedIn = FALSE;
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
                XOnlineTaskClose( m_hLogonTask );
                m_bSignedIn = FALSE;
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
            XOnlineTaskClose( m_hLogonTask );

            m_bSignedIn = FALSE;
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
            
            if ( !SUCCEEDED( hrNotification ) )
            {
                XOnlineTaskClose( m_hLogonTask );

                m_bSignedIn = FALSE;
                return E_ACCOUNT_ERROR;
            }
        }
    }

    m_bSignedIn = TRUE;
    return (INT)S_OK;
}

//-------------------------------------------------------------------------------------
// Name: CopyXUIDs()
// Desc: Copy the XUIDs (received from the host or from the arbitration service)
//       to the m_rwPlayerXUIDs array, in the same order as the elements in the
//       m_rwPlayers array (and the same order that scores
//       will go in the m_dwScores array).
//-------------------------------------------------------------------------------------
VOID CXBoxSample::CopyXUIDs( const XUID* pXUIDs )
{
    BOOL bHostFound = FALSE;

    for( DWORD i = 0; i < MAX_MATCHERS; ++i )
    {
        // Stop when we reach unused XUID entries
        if( pXUIDs[i].qwUserID == 0 )
            break;

        BOOL bCopied = FALSE;

        for( DWORD player = 0; player < m_rwPlayers.size(); ++player )
        {
            if( m_rwPlayers[player].qwUserID == pXUIDs[i].qwUserID )
            {
                bCopied = TRUE;
                // Copy the source XUID into the appropriate destination slot.
                m_rwPlayerXUIDs[player] = pXUIDs[i];
            }
        }

        // If we didn't find it in m_rwPlayers then it must be the local player
        // or the host (because the clients don't get the userID for the host).
        if( !bCopied )
        {
            if( pXUIDs[i].qwUserID
                == m_rwStoredUsers[m_wCurUserIndex].xuid.qwUserID)
            {
                // It's the local player - copy it to the local player's
                // slot (always the last entry).
                m_rwPlayerXUIDs[m_rwPlayers.size()] = pXUIDs[i];
            }
            else
            {
                // It must be the host. The host is always in entry zero
                // of m_rwPlayers, so we'll copy it there.
                assert( !m_bIsHost );
                assert( !bHostFound );

                bHostFound         = TRUE;
                m_rwPlayerXUIDs[0] = pXUIDs[i];
            }
        }
    }
}

//-------------------------------------------------------------------------------------
// Name: SendJoinRequest()
// Desc: Sends a request to a host Xbox asking for permission to join
//       the arbitrated game session. If the request is accepted then
//       a network message will be received.
//-------------------------------------------------------------------------------------
BOOL CXBoxSample::SendJoinRequest()
{
    assert( m_rwSessionList.size() > 0 );
    
    // Join the first game in the list
    SessionInfo& sessionInfo = m_rwSessionList[0];
    
    // Clear any registered sessions
    if( m_bJoinedGame )
    {
        INT iResult = XNetUnregisterKey( &m_xnJoinedSessionID );

        if( iResult != NO_ERROR )
        {
            return FALSE;
        }

        m_bJoinedGame = FALSE;

        ZeroMemory( &m_xnJoinedSessionID, sizeof( XNKID ) );
    }
    
    // We found a valid session with an available player slot.
    // Register the session key.
    INT iResult = XNetRegisterKey( sessionInfo.GetSessionID(),
                                   sessionInfo.GetKeyExchangeKey() );
    if( iResult != NO_ERROR )
    {
        SetState( STATE_GAME_SETUP );

        return FALSE;
    }

    m_bJoinedGame = TRUE;
    
    // Save the key ID because we need to unregister it laer
    CopyMemory( &m_xnJoinedSessionID,
                sessionInfo.GetSessionID(),
                sizeof( XNKID ) );
    
    // Store the game name
    m_sessionInfo.SetSessionName( sessionInfo.GetSessionName() );
    m_sessionInfo.SetGameType( sessionInfo.GetGameType() );
    m_sessionInfo.SetPlayerLevel( sessionInfo.GetPlayerLevel() );
    m_sessionInfo.SetOwnerName( sessionInfo.GetOwnerName() );
    m_sessionInfo.SetGameStyle( sessionInfo.GetGameStyle() );
    m_sessionInfo.SetConfigInfo( sessionInfo.GetConfigInfo() );
    
    // Convert the XNADDR of the host to the INADDR we'll use to
    // join the game
    iResult = XNetXnAddrToInAddr( sessionInfo.GetHostAddr(),
                                  &m_xnJoinedSessionID,
                                  &m_inHostAddr );

    assert( iResult == NO_ERROR );

    WCHAR strUserName[XONLINE_GAMERTAG_SIZE];
    XBUtil_GetWide( m_rwStoredUsers[m_wCurUserIndex].szGamertag,
                    strUserName, XONLINE_GAMERTAG_SIZE );
    
    m_networkMessageHandler.SetUser( strUserName, FALSE );
    m_networkMessageHandler.SetSessionID( m_xnJoinedSessionID );
    
    // Request join approval from the game and await a response
    SOCKADDR_IN sa;
    sa.sin_family       = AF_INET;
    sa.sin_addr         = m_inHostAddr;
    sa.sin_port         = htons( GameMsg::GAME_PORT );

    ULONGLONG ullUserId = m_rwStoredUsers[m_wCurUserIndex].xuid.qwUserID;

    m_networkMessageHandler.SendJoinGame( sa, strUserName,
                                          ullUserId );

    return TRUE;
}

//-------------------------------------------------------------------------------------
// Name: RemovePlayer()
// Desc: Removes a player from the hosted session. This generally gets
//       called as the result of a player dropping out of the game session.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RemovePlayer()
{
    assert( m_dwSlotsInUse );
    assert( m_bIsHost );

    if( m_dwSlotsInUse == MAX_PLAYERS_PER_GAME )
    {
        // The session used to be full.
        // Turn on Qos listening, so
        // that other consoles can probe us.
        m_hostedSession.Listen( TRUE );
    }

    --m_dwSlotsInUse;

    UpdateSession();
}

//-------------------------------------------------------------------------------------
// Name: UpdateSession()
// Desc: Updates the hosted session. Allows players to join, dropout, etc.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateSession()
{
    assert( m_hostedSession.Exists() != NULL );

    m_hostedSession.m_dwPublicFilled = m_dwSlotsInUse;
    m_hostedSession.m_dwPublicOpen   = MAX_PLAYERS_PER_GAME - m_dwSlotsInUse;

    // Don't let anybody else join.
    if( m_bArbitrationStarted )
        m_hostedSession.m_dwPublicOpen = 0;

    // A title may call CSession::Update repeatedly without having to
    // wait for the update to complete
    HRESULT hr = m_hostedSession.Update();

    if( FAILED( hr ) )
    {
        SetState( STATE_NETWORK_ERROR );
    }
}

//-------------------------------------------------------------------------------------
// Name: DeleteSession
// Desc: Deletes the current game session of the current user is host
//       Blocks execution of the game until the task is complete.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::DeleteSession()
{
    // Flag so we know when the deletion process is finished
    BOOL bDeleteFinished = FALSE;

    // Initialize the delete request
    HRESULT hrDelete = m_hostedSession.Delete();

    if( FAILED(hrDelete) )
    {
        bDeleteFinished = TRUE;
    }

    // Continue deletion until it is finished
    HRESULT hrProcess;
    while (! bDeleteFinished)
    {        
        hrProcess = m_hostedSession.Process();

        if( hrProcess != XONLINETASK_S_RUNNING )
        {
            bDeleteFinished = TRUE;
        }
    }

    // Reset the network variables
    m_rwPlayers.clear();
    m_hostedSession.Reset();
    m_matchQuery.Cancel();        
}

//-------------------------------------------------------------------------------------
// Name: LeaveGame
// Desc: Sends a network message from a client to the host notifying the
//       host that they are leaving the game. This message is only sent
//       while the client is in the Game Lobby. Doing this causes an
//       immediate dropout of the client to the host preventing the host
//       from trying to start the match before a network timeout occurs causing
//       the player drop-out to register.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::LeaveGame()
{
    CXBNetBlob blob;
    blob.blobType = BLOB_CLIENT_LEFT;
    blob.dataSize = 0;  // There is no data associated with this packet.

    // Let the other players know that the game is over.
    m_networkMessageHandler.SendBlob( m_rwPlayers, blob );
}

// Arbitration registration helper functions

//-------------------------------------------------------------------------------------
// Name: RegisterForArbitration
// Desc: Attempts to register the current user for the arbitrated game session.
//       Returns FALSE if the registration fails for any reason.
//       If the Xbox is not the host, then a message is sent to the host
//       saying that registration is finished and that the player is
//       ready to start their game session.
//-------------------------------------------------------------------------------------
BOOL CXBoxSample::RegisterForArbitration()
{
    // Register for arbitration. This should be done once per box.
    // This will register all users who are logged in on the box
    CXBOnlineTask arbitrationHandle;

    // Specify zero or more flags that describe your competition.
    // The FFA flag implies that players are not on teams. All parameters must
    // be the same for all players of an arbitrated game.
    // If the XONLINE_ARB_REGISTER_FLAG_TIME_EXTENDABLE flag is specified
    // then the players can agree to extend the match time
    // using XOnlineArbitrationExtendRound.
    DWORD dwFlags                 = XONLINE_ARB_REGISTER_FLAG_FFA;
    CONST DWORD MAX_ROUND_SECONDS = 600;

    HRESULT hrRegister = XOnlineArbitrationRegister( &m_arbID, MAX_ROUND_SECONDS,
                                                     dwFlags, 0, &arbitrationHandle );
    if( FAILED( hrRegister ) )
    {
        return FALSE;
    }

    // Block execution until the registration process has finished
    if( !WaitForTaskToComplete( arbitrationHandle, &hrRegister ) )
    {
        return FALSE;
    }

    // Now we can ask to see what other boxes have registered.
    // Allocate a buffer for the registrants and zero it.
    XONLINE_ARB_REGISTRANT rwRegistrantsBuffer[ MAX_PLAYERS ] = { 0 };

    // You can find out how many machines have registered so far with
    // the specified arbitration ID. You can then iterate through
    // the results in the RegistrantsBuffer, adding up the users on
    // each machine, to get the total number of users.
    DWORD dwNumRegisteredBoxes = 0;


    HRESULT hrRegResults = XOnlineArbitrationRegisterGetResults( arbitrationHandle, 
                           MAX_PLAYERS, rwRegistrantsBuffer, &dwNumRegisteredBoxes );

    if( FAILED( hrRegResults ) )
    {
        arbitrationHandle.Close();

        return FALSE;
    }

    if( !m_bIsHost )
    {
        // Since we are a client/peer we need to
        // let the host know that we registered.
        CXBNetBlob blob;

        blob.blobType = BLOB_REGISTERED;
        blob.dataSize = 0;  // This blob has no payload.
        m_networkMessageHandler.SendBlob( m_rwPlayers, blob );
    }
    else
    {
        // If we are the host, then registration must be finished. Therefore
        // we should record all of the XUIDs and disconnect any players that
        // did not register.

        DWORD dwNumRegisteredPlayers = 0;
        // Zero the XUID array, to mark unused XUIDs.
        XUID rwPlayerXUIDs[MAX_PLAYERS] = { 0 };

        for( DWORD i = 0; i < dwNumRegisteredBoxes; ++i )
        {
            // If your game allows multiple players per box then you need to
            // loop over the results from each box, copying each player.
            for( DWORD player = 0; player < XONLINE_MAX_LOGON_USERS; ++player )
            {
                if( rwRegistrantsBuffer[i].xuidUsers[player].qwUserID )
                {
                    assert( dwNumRegisteredPlayers < MAX_PLAYERS );

                    rwPlayerXUIDs[dwNumRegisteredPlayers]   = 
                        rwRegistrantsBuffer[i].xuidUsers[player];
                    m_rwPlayerXUIDs[dwNumRegisteredPlayers] = 
                        rwRegistrantsBuffer[i].xuidUsers[player];

                    ++dwNumRegisteredPlayers;
                }
            }
        }

        // The clients need the XUIDs of all the other
        // players for proper submission of the match
        // results. Since only the host has the XUIDs
        // of all the successfully registered players
        // in the match, we need to send the XUIDs to
        // all the registered players.
        CXBNetBlob blob;
        blob.blobType = BLOB_XUIDS;
        blob.dataSize = sizeof( XUID ) * MAX_MATCHERS;
        memcpy( blob.data, rwPlayerXUIDs, blob.dataSize );

        m_networkMessageHandler.SendBlob( m_rwPlayers, blob );

        if( dwNumRegisteredPlayers < 2 )
        {
            // Arbitrated sessions must have at least two players. If we
            // don't have that many then we can't proceed. This can happen
            // if the other players quit as the registered game is starting,
            // or if their are connection problems that prevent them from
            // joining.
            arbitrationHandle.Close();

            return FALSE;
        }
    }

    arbitrationHandle.Close();

    return TRUE;
}

//-------------------------------------------------------------------------------------
// Name: StartArbitrationGame()
// Desc: Starts the arbitrated game session once registration is finished.
//       If the Xbox is hosting the game then a network message is sent
//       telling all peers to start their games.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::StartArbitratedGame()
{
    if( m_bIsHost )
    {
        if ( !RegisterForArbitration())
        {
            SetState( STATE_NETWORK_ERROR );
        }

        CXBNetBlob blob;
        blob.blobType = BLOB_GAME_START;
        blob.dataSize = 0;  // This blob has no payload.
        m_networkMessageHandler.SendBlob( m_rwPlayers, blob );
    }

    SetState( STATE_GAME_SESSION );
    ZeroMemory( m_dwScores, sizeof( m_dwScores ) );
}

//-------------------------------------------------------------------------------------
// Name: StartArbitratedGameRegistration()
// Desc: Start registering players for an arbitrated game. At this point
//       no more players will be allowed to join, and drops will be recorded
//       against players. The arbitration service will be used to record
//       game results, in order to discourage cheating.
//-------------------------------------------------------------------------------------
BOOL CXBoxSample::StartArbitratedGameRegistration()
{
    if( m_rwPlayers.size() == 0 )
    {
        return FALSE;
    }

    // At this point we can no longer let people join the game.
    m_bArbitrationStarted = TRUE;
    UpdateSession();

    // The arbitration id is created by the host, and then given to the other players.
    // This number uniquely identifies the game.
    ZeroMemory( &m_arbID, sizeof( m_arbID ) );

    // The creation of the session should not necessarily be the host.
    // Instead, the user with the best connection to the other players
    // should be selected.
    // However, to keep this sample as simple as possible, the game creator
    // is used as the host of the arbitrated match.
    assert( m_bIsHost );

    // Only the host creates the round id - the round id is then shared with all
    // players.
    m_arbID.SessionID = m_hostedSession.m_SessionID;
    HRESULT hrCreateRound = XOnlineArbitrationCreateRoundID( &m_arbID.qwRoundID );

    if( FAILED( hrCreateRound ) )
    {
        return FALSE;
    }

    // Prior to starting the arbitration process the host should
    // share XNADDRs of all players with all players, give all players the
    // round id, and ask all Xboxes to register with arbitration.
    // The sample just sends the round ID.
    // The clients should all reply once they have registered.
    CXBNetBlob blob;
    blob.blobType = BLOB_ARB_ID;
    blob.dataSize = sizeof(m_arbID);
    memcpy(blob.data, &m_arbID, sizeof(m_arbID) );

    m_networkMessageHandler.SendBlob( m_rwPlayers, blob );

    // Wait for players to register, or for a reasonable time-out (5-10 seconds)
    // to expire. If any players failed to register, disconnect them.
    m_fRegistrationStart  = m_fAppTime;
    m_dwPlayersRegistered = 0;

    return TRUE;
}

// Rating task helper functions

//-------------------------------------------------------------------------------------
// Name: CloseViewRatingTask()
// Desc: Attempts to close the parameter hTask.  If closing the task failed, and
//       the iSetErrors flag has a value indicating that "errors should be set",
//       the View Rating Error codes are set to the task closing errors appropriately.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::CloseViewRatingsTask( XONLINETASK_HANDLE hTask , INT iSetErrors )
{
    // This attempts to close the leader enumeration or leader enumeration results 
    // retrieval tasks.  If we allow the error to be set, they will be set and will
    // return an zero result.  Otherwise, we will exit with a non-zero result,
    // whether the task closing succeeded or not
    HRESULT hrCloseEnumerateTask = XOnlineTaskClose( hTask );

    if ( FAILED( hrCloseEnumerateTask ) )
    {
        if ( iSetErrors == TASK_CLOSE_SET_ERRORS )
        {
            m_iViewRatingError      = VIEW_RATING_ERR_CLOSING_TASK;
            m_dwViewRatingErrorCode = (DWORD)hrCloseEnumerateTask;
        }
    }
}

//-------------------------------------------------------------------------------------
// Name: CloseGameSessionTask()
// Desc: Attempts to close the parameter hTask.  If closing the task failed, and
//       the iSetErrors flag has a value indicating that "errors should be set",
//       the Stat Write Error codes are set to the task closing errors appropriately.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::CloseGameSessionTask( XONLINETASK_HANDLE hTask , INT iSetErrors )
{
    // This attempts to close the arbitration report write task.  
    // If we allow the error to be set, they will be set and will return an zero result.  
    // Otherwise, we will exit with a non-zero result, whether the task closing 
    // succeeded or not
    HRESULT hrCloseStatWriteTask = XOnlineTaskClose( hTask );
    if ( FAILED( hrCloseStatWriteTask ) )
    {
        if ( iSetErrors == TASK_CLOSE_SET_ERRORS )
        {
            m_iStatWriteError      = STAT_WRITE_ERR_CLOSING_TASK;
            m_dwStatWriteErrorCode = (DWORD)hrCloseStatWriteTask;
        }
    }
}

///////////////////////
// UI FSM state code //
///////////////////////

//-------------------------------------------------------------------------------------
// Name: GetEvent()
// Desc: Returns the state of the controller
//-------------------------------------------------------------------------------------
CXBoxSample::Event CXBoxSample::GetEvent() const
{
    // "A" or "Start"
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_A] ||
        m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_START )
    {
        return EV_BUTTON_A;
    }
    
    // "B"
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_B] )
        return EV_BUTTON_B;

    // "X"
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_X] )
        return EV_BUTTON_X;

    // "Y"
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_Y] )
        return EV_BUTTON_Y;
    
    // "Back"
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_BACK )
        return EV_BUTTON_BACK;
    
    // "White"
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_WHITE] )
        return EV_BUTTON_WHITE;
    
    // Movement
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
        return EV_UP;

    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
        return EV_DOWN;
    
    return EV_NULL;
}

//-------------------------------------------------------------------------------------
// Name: SetState()
// Desc: Transitions the UI and game from it's current state to the requested
//       state. When a transition occurs any code required by the previous
//       is executed. Any code required by the new state before entry is
//       also executed.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::SetState( EUIStates newState )
{
    // Do not execute "DOUBLE entry"
    // if this happens
    if ( newState == m_state )
        return;

    // Execute exit functionality
    switch( m_state )
    {
    case STATE_SELECT_ACCOUNT:      ExitStateSelectAccount();      break;
    case STATE_LOGIN:               ExitStateLogin();              break;
    case STATE_LOGIN_FAILED:        ExitStateLoginFailed();        break;
    case STATE_NETWORK_ERROR:       ExitStateNetworkError();       break;
    case STATE_GAME_SETUP:          ExitStateGameSetup();          break;
    case STATE_VIEW_RATINGS:        ExitStateViewRatings();        break;
    case STATE_QUICKMATCH:          ExitStateQuickMatch();         break;
    case STATE_CREATE_MATCH:        ExitStateCreateMatch();        break;
    case STATE_GAME_LOBBY:          ExitStateGameLobby();          break;
    case STATE_GAME_SESSION:        ExitStateGameSession();        break;
    case STATE_GAME_RESULTS:        ExitStateGameResults();        break;
        break;
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
    case STATE_VIEW_RATINGS:        EnterStateViewRatings();        break;
    case STATE_QUICKMATCH:          EnterStateQuickMatch();         break;
    case STATE_CREATE_MATCH:        EnterStateCreateMatch();        break;
    case STATE_GAME_LOBBY:          EnterStateGameLobby();          break;
    case STATE_GAME_SESSION:        EnterStateGameSession();        break;
    case STATE_GAME_RESULTS:        EnterStateGameResults();        break;
        break;
    default:
        assert( 0 && "Unknown/illegal state!" );
    }

    // Finally transition to the desited state
    m_state = newState;
}

/////////////////////////
// State SelectAccount //
/////////////////////////

//-------------------------------------------------------------------------------------
// Name: EnterStateSelectAccount
// Desc: Executes setup code for STATE_SELECT_ACCOUNT
//       Finds the Xbox Live user accounts on the memory units and hard-drive
//-------------------------------------------------------------------------------------
VOID CXBoxSample::EnterStateSelectAccount()
{
    if ( m_bSignedIn )
        XOnlineTaskClose( m_hLogonTask );

    m_bSignedIn = FALSE;

    // If any MUs are inserted/removed, need to update the
    // user account list
    DWORD dwInsertions;
    DWORD dwRemovals;

    // Stall for mem unit mounting
    while ( CXBMemUnit::GetMemUnitChanges( dwInsertions, dwRemovals ) );

    // Keep it in memory so we don't have to worry about insertion
    // and deletion once we get past login
    CXBMemUnit::GetMemUnitSnapshot();

    // First, obtain a list of user accounts on this Xbox. The XOnlineGetUsers
    // function will enumerate both the hard disk and any attached memory units
    // looking for accounts. 
    HRESULT hrGetUsers = XOnlineGetUsers( m_rwStoredUsers, &m_dwNumStoredUsers );

    // Reboot the user to create an Xbox Live account if
    // one isn't found
    if (! SUCCEEDED( hrGetUsers ) )
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
VOID CXBoxSample::UpdateStateSelectAccount( Event event )
{
    switch( event )
    {
    case EV_UP:
        --m_iItemSelected;
        // Wrap the input to goto the bottom
        m_iItemSelected = 
            ( m_iItemSelected < 0 ) ? ( m_dwNumStoredUsers - 1 ) : m_iItemSelected;
        break;

    case EV_DOWN:
        ++m_iItemSelected;
        // Wrap the input to goto the top
        m_iItemSelected = 
            ( m_iItemSelected >= ( INT )m_dwNumStoredUsers ) ? 0 : m_iItemSelected;
        break;

    case EV_BUTTON_A:
        SetState( STATE_LOGIN );
        break;

    default:
        break;
    }

    DWORD dwInsertions;
    DWORD dwRemovals;

    // If the user inserts a memory card, go ahead
    // and mount it!
    if ( CXBMemUnit::GetMemUnitChanges( dwInsertions, dwRemovals ) )
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
    m_font.DrawText( SCREEN_CENTER_X, POS_SCREEN_TITLE_Y, COLOR_NORMAL,
                     L"SELECT ACCOUNT",
                     XBFONT_CENTER_X );

    // Show list of user accounts
    for( DWORD i = 0; i < m_dwNumStoredUsers; ++i )
    {
        DWORD dwColor = 
            ( (DWORD)m_iItemSelected == i ) ? COLOR_HIGHLIGHT : COLOR_NORMAL;

        // Convert user name to WCHAR string
        WCHAR strUserName[XONLINE_GAMERTAG_SIZE];
        XBUtil_GetWide( m_rwStoredUsers[i].szGamertag, strUserName,
                        XONLINE_GAMERTAG_SIZE );

        m_font.DrawText( SCREEN_CENTER_X,
                         POS_ACCOUNT_LIST_START + ( DEFAULT_TEXT_PADDING * i ),
                         dwColor,
                         strUserName, XBFONT_CENTER_X );

        if ( i == (DWORD)m_iItemSelected )
        {
            // Show selected item with little triangle
            FLOAT fTextOffset   = ( m_font.GetTextWidth( strUserName ) / 2.0f );
            FLOAT fTextPos      = SCREEN_CENTER_X - 
                                  ( fTextOffset + 
                                    m_font.GetTextWidth( GLYPH_RIGHT_TICK ) );

            m_font.DrawText( fTextPos, POS_ACCOUNT_LIST_START + 
                             ( DEFAULT_TEXT_PADDING * m_iItemSelected ),
                             COLOR_POINTER,
                             GLYPH_RIGHT_TICK, XBFONT_CENTER_X );
        }
    }

    // The user can not really back out of this screen
    RenderFooter( FOOTER_RENDER_SELECT );
}

//-------------------------------------------------------------------------------------
// Name: ExitStateSelectAccount
// Desc: Sets the proper user account select for login.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::ExitStateSelectAccount()
{
    m_wCurUserIndex = (WORD)m_iItemSelected;
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
    HRESULT hr      = StartSignIn( m_wCurUserIndex );

    m_iItemSelected = 0;
    m_bIsSigningIn  = ( hr == S_OK );
    m_bSignedIn     = FALSE;
}

//-------------------------------------------------------------------------------------
// Name: UpdateStateLogin
// Desc: Attempts to log the user into the Xbox Live service. If login fails
//       the user will be prompted try again or to fix the problem via
//       the Xbox Dashboard
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateLogin( Event event )
{
    switch( event )
    {
    case EV_BUTTON_B: // Allow the user to cancel and use a different account
        // Close the task to allow for somone else to signon
        if ( m_bSignedIn || m_bIsSigningIn )
        {
            XOnlineTaskClose( m_hLogonTask );
        }

        m_bSignedIn    = FALSE;
        m_bIsSigningIn = FALSE;

        SetState( STATE_SELECT_ACCOUNT );
        break;
    }

    if ( m_bIsSigningIn )
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
            // Finish sign in:
            // Announce our presence to Xbox Live
            // and check the required services that
            // we need to run the demo
            m_iSignInResult = FinishSignIn();

            if ( m_iSignInResult == S_OK )
            {
                m_bSignedIn = TRUE;
                    m_bIsSigningIn = FALSE;
                SetState( STATE_GAME_SETUP );
            }
            else
            {
                SetState( STATE_LOGIN_FAILED );
            }
            return;
            break;

        case E_NETWORK_ERROR:
            m_bSignedIn    = FALSE;
            m_bIsSigningIn = FALSE;
            SetState( STATE_LOGIN_FAILED );
            return;
        break;

        case E_ACCOUNT_ERROR:
            m_bSignedIn    = FALSE;
            m_bIsSigningIn = FALSE;
            SetState( STATE_LOGIN_FAILED );
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

    RenderFooter( FOOTER_RENDER_CANCEL );
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
        SetState( STATE_SELECT_ACCOUNT );
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
        SetState( STATE_SELECT_ACCOUNT );
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
    XOnlineTaskClose( m_hLogonTask );
}


/////////////////////
// State GameSetup //
/////////////////////

//-------------------------------------------------------------------------------------
// Name: EnterStateGameSetup()
// Desc: Initialises the menu varaibles and does sanity checks so the player
//       may select their action (Check Rating, Create Game, QuickMatch)
//-------------------------------------------------------------------------------------
VOID CXBoxSample::EnterStateGameSetup()
{
    m_iItemSelected = 0;
    m_rwPlayers.clear();

    assert( m_bSignedIn );
}

//-------------------------------------------------------------------------------------
// Name: UpdateStateGameSetup
// Desc: Allows the user to
//          1) Create a network game
//          2) Join an existing network game
//          3) Check their rating for this game
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateGameSetup( Event event )
{
    const INT numMenuItems = 3;

    switch( event )
    {
    case EV_UP:
        --m_iItemSelected;
        // Wrap the input to goto the bottom
        m_iItemSelected = ( m_iItemSelected < 0 )
                            ? ( numMenuItems - 1 ) : m_iItemSelected;
        break;

    case EV_DOWN:
        ++m_iItemSelected;
        // Wrap the input to goto the top
        m_iItemSelected = ( m_iItemSelected >= (INT)numMenuItems )
                            ? 0 : m_iItemSelected;
        break;

    case EV_BUTTON_A:
        assert( m_iItemSelected >= 0 );
        assert( m_iItemSelected < numMenuItems );

        switch( m_iItemSelected )
        {
        case 0: // Find a QuickMatch and join an existing game
            SetState( STATE_QUICKMATCH );
            break;

        case 1: // Create a match for other players to join
            SetState( STATE_CREATE_MATCH );
            break;

        case 2: // View your ratings for the game against other
                // Xbox Live players
            SetState( STATE_VIEW_RATINGS );
            break;
        }
        break;

    case EV_BUTTON_B: // Back out and login with a different account
        SetState( STATE_SELECT_ACCOUNT );
        break;
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateGameSetup
// Desc: Shows the menu of options the player has.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateGameSetup()
{
    // Header
    m_font.DrawText( SCREEN_CENTER_X, POS_SCREEN_TITLE_Y, COLOR_NORMAL,
                     L"GAME SETUP", 
                     XBFONT_CENTER_X );

    // Show list of user accounts
    for( DWORD i = 0; i < NUM_ITEMS_GAME_SETUP_MENU; ++i )
    {
        // Highlight the selected item
        DWORD dwColor = ( (DWORD)m_iItemSelected == i )
                          ? COLOR_HIGHLIGHT : COLOR_NORMAL;

        m_font.DrawText( SCREEN_CENTER_X,
                         POS_GAME_SETUP_Y + (DEFAULT_TEXT_PADDING * i),
                         dwColor, MENU_GAME_SETUP[i], XBFONT_CENTER_X );
    }

    // Show selected item with little triangle
    FLOAT fTextOffset   = m_font.GetTextWidth( MENU_GAME_SETUP[ m_iItemSelected ] ) 
                          / 2.0f;
    FLOAT fTextPos      = SCREEN_CENTER_X - 
                          ( fTextOffset + m_font.GetTextWidth( GLYPH_RIGHT_TICK ) );

    m_font.DrawText( fTextPos,
                     POS_GAME_SETUP_Y + ( DEFAULT_TEXT_PADDING * m_iItemSelected ),
                     COLOR_POINTER, GLYPH_RIGHT_TICK, XBFONT_CENTER_X );

    // Bottom Help text
    RenderFooter( FOOTER_RENDER_SELECT | FOOTER_RENDER_CANCEL );
}

///////////////////////
// State ViewRatings //
///////////////////////

//-------------------------------------------------------------------------------------
// Name: EnterStateViewRatings
// Desc: Retrieves the leaderboard for the current title with a window of results
//       containing the Xbox Live user's rank and rating.  If the user's 
//       rating and rank does not exist yet, the top players are retrieved instead.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::EnterStateViewRatings()
{
    // Zero the memory that holds the user stats
    ZeroMemory( m_aStatUsers, sizeof( m_aStatUsers ) );

    // set view ratings to true, for render function
    m_iViewRatingError                  = VIEW_RATING_NO_ERR;

    // declare a reference to our logged-in user's info
    const XONLINE_USER& curLoggedOnUser = m_rwStoredUsers[m_wCurUserIndex];
    
    // attempt to retrieve set of users
    XONLINETASK_HANDLE  hLeaderEnumerateTask;
    HRESULT hrLeaderEnumerate = XOnlineStatLeaderEnumerate(
            &( curLoggedOnUser.xuid ),  // user as pivot point in leader enumeration 
                                        // list
            0,                          // usually determined which position to start, 
                                        // but we specified a pivot point, so this is 
                                        // ignored
            NUM_RANKINGS_TO_DISPLAY,    // number of rankings to display
            DEFAULT_LEADERBOARD_ID,     // specified leaderbord for display (there is 
                                        // only one here) 
            NUM_REQUESTED_STATS,        // number of stats per user to retrieve
            REQUESTED_STAT_IDS,         // array of stat ids involved in retrieval
            NULL,                       // optional work event handle.. we will use 
                                        // XOnlineTaskContinue instead
            &hLeaderEnumerateTask       // our leader enumeration task
            );

    // error out if this fails right away
    if ( FAILED( hrLeaderEnumerate ) ) 
    {
        m_iViewRatingError      = VIEW_RATING_ERR_ENUMERATE;
        m_dwViewRatingErrorCode = (DWORD)hrLeaderEnumerate;

        CloseViewRatingsTask( hLeaderEnumerateTask );
        return;
    }

    // Loop through our leader enumeration task until we get a result
    HRESULT hrLogonTask;
    do 
    { 
        hrLogonTask = XOnlineTaskContinue( m_hLogonTask );

        // close enumeration task and exit to network error screen if our main 
        // online task ever fails
        if ( FAILED( hrLogonTask ) )
        {
            CloseViewRatingsTask( hLeaderEnumerateTask );

            SetState( STATE_NETWORK_ERROR );
            return;
        }

        hrLeaderEnumerate = XOnlineTaskContinue( hLeaderEnumerateTask );
    } 
    while ( hrLeaderEnumerate == XONLINETASK_S_RUNNING );

    // if we don't find ourselves in our enumeration list, or this fails....
    if ( FAILED( hrLeaderEnumerate ) )
    {
        // if we cannot found local user, attempt to display the Top N rankings instead
        if ( hrLeaderEnumerate == XONLINE_E_STAT_USER_NOT_FOUND)
        {
            // close the task
            CloseViewRatingsTask( hLeaderEnumerateTask );

            // we must add a little over a second's latency to avoid throttling 
            // the followup enumeration call
            const DWORD ENUMERATION_LEADERS_SLEEP_TIME = 1050;
            Sleep(ENUMERATION_LEADERS_SLEEP_TIME);

            // re-enumerate, except using a ranking pivot instead of a user pivot
            hrLeaderEnumerate = XOnlineStatLeaderEnumerate(
            NULL,                       // ignoring user pivot
            1,                          // determines which position to start
            NUM_RANKINGS_TO_DISPLAY,    // number of rankings to display
            DEFAULT_LEADERBOARD_ID,     // specified leaderbord for display 
                                        // (there is only one here) 
            NUM_REQUESTED_STATS,        // number of stats per user to retrieve
            REQUESTED_STAT_IDS,         // array of stat ids involved in retrieval
            NULL,                       // optional work event handle.. we will use 
                                        // XOnlineTaskContinue instead
            &hLeaderEnumerateTask       // our leader enumeration task
            );

            // if this fails, the entire attempt failed... so error out
            if ( FAILED( hrLeaderEnumerate ) )
            {
		        m_iViewRatingError      = VIEW_RATING_ERR_ENUMERATE;
		        m_dwViewRatingErrorCode = (DWORD)hrLeaderEnumerate;
                
                // close the task
                CloseViewRatingsTask( hLeaderEnumerateTask );
                return;
            }

            // Loop through our leader enumeration task until we get a result
            do 
            { 
                hrLogonTask = XOnlineTaskContinue( m_hLogonTask );

                // close enumeration task and exit to network error screen if our main 
                // online task ever fails
                if ( FAILED( hrLogonTask ) )
                {
                    CloseViewRatingsTask( hLeaderEnumerateTask );

                    SetState( STATE_NETWORK_ERROR );
                    return;
                }

                hrLeaderEnumerate = XOnlineTaskContinue( hLeaderEnumerateTask );
            } 
            while ( hrLeaderEnumerate == XONLINETASK_S_RUNNING );

            // failing that, we must error out
            if ( FAILED( hrLeaderEnumerate ) )
            {
                m_iViewRatingError      = VIEW_RATING_ERR_ENUMERATE;
                m_dwViewRatingErrorCode = (DWORD)hrLeaderEnumerate;

                CloseViewRatingsTask( hLeaderEnumerateTask );
            	return;
        	}
        }
        else // this truly failed
        {
            m_iViewRatingError      = VIEW_RATING_ERR_ENUMERATE;
            m_dwViewRatingErrorCode = (DWORD)hrLeaderEnumerate;

            CloseViewRatingsTask( hLeaderEnumerateTask );
            return;
       }
    }

    // XONLINE_STAT_USER
    DWORD   dwLeaderBoardSize = 0;
    m_dwNumUsersEnumerated = 0;

    ZeroMemory( m_aStats, sizeof( m_aStats ) );

    // Now attempt get results
    HRESULT hrLeaderResults = XOnlineStatLeaderEnumerateGetResults(
            hLeaderEnumerateTask,       // provide the leader enumerate task
            NUM_RANKINGS_TO_DISPLAY,    // number of users to get results for
            m_aStatUsers,               // buffer for stat users
            NUM_RANKINGS_TO_DISPLAY 
            * NUM_REQUESTED_STATS,      // number of stat types * number of user 
                                        // results = total number of stats returned
            m_aStats,                   // buffer to retrieve all stats
            &dwLeaderBoardSize,         // receives number of users of leaderboard
            &m_dwNumUsersEnumerated,    // retrieves actual number of users returned
            STAT_EXTRAS_BUFFER_SIZE,    // size of extras buffer
            m_aStatsExtra );            // extras buffer

    if ( FAILED( hrLeaderResults ) )
    {
        m_iViewRatingError      = VIEW_RATING_ERR_RESULTS;
        m_dwViewRatingErrorCode = (DWORD)hrLeaderResults;
        CloseViewRatingsTask( hLeaderEnumerateTask );
        return;
    }

    // close the task, and report closing errors if they occur
    CloseViewRatingsTask( hLeaderEnumerateTask , TASK_CLOSE_SET_ERRORS );
}

//-------------------------------------------------------------------------------------
// Name: UpdateStateViewRatings()
// Desc: Allows the user to continue on to the GameSetup screen when they
//       are done viewing the results of the leaderboard.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateViewRatings( Event event )
{
    switch( event )
    {
    case EV_BUTTON_B:
        SetState( STATE_GAME_SETUP );
        break;
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateViewRatings()
// Desc: Displays the leaderboard for the current title with previously retrieved
//       results.  The logged-in user's rank and rating are highlighted.  (If the user
//       does not appear in the leaderboard, the top players are displayed instead.)
//       If there was an error previously, this error and associated code will be 
//       displayed and nothing else.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateViewRatings()
{
    // Header
    m_font.DrawText( SCREEN_CENTER_X, POS_SCREEN_TITLE_Y, COLOR_NORMAL,
                     L"RATINGS", 
                     XBFONT_CENTER_X );
        
    // if we have an error, display it
    if ( m_iViewRatingError != VIEW_RATING_NO_ERR )
    {
        const FLOAT VIEW_RATING_ERROR_MSG_OFFSET_X = 50.0f;
        const FLOAT VIEW_RATING_ERROR_MSG_OFFSET_Y = 200.0f;
        const FLOAT VIEW_RATING_ERROR_MSG_PAD_Y    = DEFAULT_TEXT_PADDING;
        const INT VIEW_RATING_ERROR_MAX_MSG_SIZE   = 256;

        WCHAR sErrMsgW[VIEW_RATING_ERROR_MAX_MSG_SIZE];
        CHAR  sErrMsg[VIEW_RATING_ERROR_MAX_MSG_SIZE];

        switch( m_iViewRatingError )
        {
        case VIEW_RATING_ERR_ENUMERATE:
            _snprintf( sErrMsg, VIEW_RATING_ERROR_MAX_MSG_SIZE,
                       "Enumeration failed with error code 0x%x." , 
                       m_dwViewRatingErrorCode );
            break;

        case VIEW_RATING_ERR_RESULTS:
            _snprintf( sErrMsg, VIEW_RATING_ERROR_MAX_MSG_SIZE,
                       "Results retrieval failed with error code 0x%x." , 
                       m_dwViewRatingErrorCode );
            break;

        default:
            // should not happen
            assert( FALSE );
        }
           
        XBUtil_GetWide( sErrMsg, sErrMsgW, VIEW_RATING_ERROR_MAX_MSG_SIZE );

        // draw error
        m_font.DrawText( VIEW_RATING_ERROR_MSG_OFFSET_X,
                         VIEW_RATING_ERROR_MSG_OFFSET_Y , 
                         COLOR_NORMAL, L"View Ratings Error:", XBFONT_LEFT);

        m_font.DrawText( VIEW_RATING_ERROR_MSG_OFFSET_X,
                         VIEW_RATING_ERROR_MSG_OFFSET_Y + VIEW_RATING_ERROR_MSG_PAD_Y,
                         COLOR_NORMAL, sErrMsgW, XBFONT_LEFT);
    }
    else
    {
        const WORD RATINGS_POS_RANK_X   = 140;
        const WORD RATINGS_POS_TAG_X    = 220;
        const WORD RATINGS_POS_RATING_X = 380;
 
        const WORD RATINGS_POS_TITLE_Y  = (WORD)( POS_SCREEN_TITLE_Y + 
                                                  DEFAULT_TEXT_PADDING );
        const WORD RATINGS_POS_START_Y  = RATINGS_POS_TITLE_Y + 40;

        // wide char buffer to display ranking
        const DWORD RANK_STR_SIZE = 16;
        CHAR  strRank[RANK_STR_SIZE];
        WCHAR sRank[RANK_STR_SIZE];

        // user name
        WCHAR sUserName[XONLINE_GAMERTAG_SIZE];

        // user overall rank
        WCHAR sUserRatingValue[XONLINE_GAMERTAG_SIZE];

        // draw our table headers
        m_font.DrawText( RATINGS_POS_RANK_X,   RATINGS_POS_TITLE_Y, COLOR_RED,
                         L"RANK",     XBFONT_LEFT);

        m_font.DrawText( RATINGS_POS_TAG_X,    RATINGS_POS_TITLE_Y, COLOR_RED,
                         L"GAMERTAG", XBFONT_LEFT);

        m_font.DrawText( RATINGS_POS_RATING_X, RATINGS_POS_TITLE_Y, COLOR_RED,
                         L"RATING",   XBFONT_LEFT);

        FLOAT fYtop   = RATINGS_POS_START_Y;
        FLOAT fYdelta = 25.0f;

        // Show list of user accounts
        for( DWORD i = 0; i < m_dwNumUsersEnumerated; ++i )
        {
            // determine how far down the screen we will draw
            FLOAT yPos = fYtop + ( fYdelta * i );

            // construct color.  If this is local user, use a highlighted color
            D3DCOLOR color = COLOR_NORMAL;
            if ( m_aStatUsers[i].xuidUser.qwUserID
                 == m_rwStoredUsers[m_wCurUserIndex].xuid.qwUserID )
        	{
                color = COLOR_HIGHLIGHT;
            }

            // construct user name
            XBUtil_GetWide( m_aStatUsers[i].szGamertag ,
                            sUserName , XONLINE_GAMERTAG_SIZE );
           
            INT iNumDigits  = 0;

            for ( INT j = 0; j < (INT)NUM_REQUESTED_STATS; ++j )
            {
                INT iStatIndex = ( i * NUM_REQUESTED_STATS ) + j;

                const XONLINE_STAT& stat = m_aStats[iStatIndex];

                switch( stat.wID )
                {
                case XONLINE_STAT_RATING:
                    // convert rating to wide string
                    _i64tow( stat.llValue , sUserRatingValue , 10 );
                    break;

                case XONLINE_STAT_RANK:
                    // write rank number to one-byte string
                    _itoa( (INT)( stat.lValue ) , strRank , 10 );
                    iNumDigits = strlen( strRank );
                    // append parenthesis
		            strRank[iNumDigits] = ')';
                    // terminate string
		            strRank[iNumDigits+1] = (CHAR)0;
                    // convert wide string
		            XBUtil_GetWide( strRank , sRank , RANK_STR_SIZE );
                    break;

                default:
                    // ignore other types of stats for now
                    break;

                }
            }

            // draw the rank, user tag, and rating -- respectively
            m_font.DrawText( RATINGS_POS_RANK_X, yPos, color, sRank , XBFONT_LEFT );

            m_font.DrawText( RATINGS_POS_TAG_X, yPos, color, sUserName , XBFONT_LEFT );

            m_font.DrawText( RATINGS_POS_RATING_X, yPos, color, sUserRatingValue, 
                             XBFONT_LEFT );
        }

    }

    RenderFooter( FOOTER_RENDER_CANCEL );
}

//-------------------------------------------------------------------------------------
// Name: ExitStateViewRatings()
// Desc: Does any deinitialization of processes to show the leaderboard.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::ExitStateViewRatings()
{
}


//////////////////////
// State QuickMatch //
//////////////////////

//-------------------------------------------------------------------------------------
// Name: EnterStateQuickMatch()
// Desc: Sets up and starts a search for a match of any type for this game.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::EnterStateQuickMatch()
{
    m_matchQuery.Clear();

    // Match any game type, level or style
    ULONGLONG ulGameType    = X_MATCH_NULL_INTEGER;
    ULONGLONG ulPlayerLevel = X_MATCH_NULL_INTEGER;
    ULONGLONG ulGameStyle   = X_MATCH_NULL_INTEGER;

    // Since we are going to try to join a game
    // we are not the host
    m_bIsHost               = FALSE;

    // Start the search
    HRESULT hrQuery         = m_matchQuery.Query( ulGameType, ulPlayerLevel, 
                                                  ulGameStyle );
    m_fRequestTime          = -1.0f;

    if( FAILED( hrQuery ) )
    {
        SetState( STATE_NETWORK_ERROR );
    }
}

//-------------------------------------------------------------------------------------
// Name: UpdateStateQuickMatch()
// Desc: Continues the search for a game and allows the user to cancel the search.
//       If a game is found then it is joined and the player is sent to the
//       game lobby.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateQuickMatch( Event event )
{
    switch( event )
    {
    case EV_BUTTON_B:
    case EV_BUTTON_BACK: // Allow the user to cancel
        m_matchQuery.Cancel();
        SetState( STATE_GAME_SETUP );
        return;
    }

    // Wait for matchmaking server to return results
    HRESULT hrProcess = m_matchQuery.Process();

    if( hrProcess != XONLINETASK_S_RUNNING )
    {
        // Handle errors
        if( FAILED( hrProcess ) )
        {
            SetState( STATE_GAME_SETUP );
            return;
        }
        
        // Get the list returned by the matchmaking server
        DWORD dwResults = m_matchQuery.Results.Size();
        
        m_rwSessionList.clear();
        
        // Save the results (for a task initiated by
        // XOnlineMatchSessionFindFromID, there a single session returned )
        for( DWORD i = 0; i < dwResults; ++i )
        {
            m_rwSessionList.push_back( SessionInfo( m_matchQuery.Results[i] ) );
        }
                
        // If we found at least one game, join it automatically
        if( dwResults > 0 )
        {
            // If this a quick match join the first one
            SendJoinRequest();
            SetState( STATE_GAME_LOBBY );
        }
        else
        {
            SetState( STATE_GAME_SETUP );
        }
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateQuickMatch()
// Desc: Shows the user that a match is being searched for and that they
//       have the option to cancel the search.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateQuickMatch()
{
    // Still searching for a match
    if ( m_matchQuery.IsRunning() )
    {
        m_font.DrawText( SCREEN_CENTER_X, POS_MESSAGE_Y, COLOR_NORMAL,
                         L"Searching For Match...", 
                         XBFONT_CENTER_X );
    }
    // A match was found
    else if ( m_fRequestTime > 0.0f )
    {
        m_font.DrawText( SCREEN_CENTER_X, POS_MESSAGE_Y, COLOR_NORMAL,
                         L"Joining Match...", 
                         XBFONT_CENTER_X );
    }
    // No matches were found
    else
    {
        m_font.DrawText( SCREEN_CENTER_X, POS_MESSAGE_Y, COLOR_NORMAL,
                         L"No open matches found.", 
                         XBFONT_CENTER_X );
    }

    RenderFooter( FOOTER_RENDER_CANCEL );
}

//-------------------------------------------------------------------------------------
// Name: ExitStateQuickMatch()
// Desc: Cleans up the QuickMatch search.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::ExitStateQuickMatch()
{
    if ( m_matchQuery.IsRunning() || m_matchQuery.IsProbing() )
    {
        m_matchQuery.Cancel();
    }

    m_matchQuery.Clear();
    m_fRequestTime = -1.0f;
}


///////////////////////
// State CreateMatch //
///////////////////////

//-------------------------------------------------------------------------------------
// Name: EnterStateCreateMatch()
// Desc: Starts the creation of a match and announces it to the network.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::EnterStateCreateMatch()
{
    // Generate a random session name if we don't currently have one
    if( *m_sessionInfo.GetSessionName() == 0 )
        m_sessionInfo.GenRandSessionName();
    
    assert( !m_hostedSession.Exists() );
    
    // Initialize the create request
    
    // Set session attributes 
    
    // Game type
    m_sessionInfo.SetGameType( TYPE_SHORT );
    m_hostedSession.GameType = m_sessionInfo.GetGameType();
    
    // Player level
    m_sessionInfo.SetPlayerLevel( LEVEL_BEGINNER );    
    m_hostedSession.PlayerLevel = m_sessionInfo.GetPlayerLevel();


    // Session name
    //---------------------------------------------------------------------
    // Always specified.
    // The second session string parameter.
    assert( *m_sessionInfo.GetSessionName() != 0 );
    m_hostedSession.SessionName = m_sessionInfo.GetSessionName();


    // Game style
    //---------------------------------------------------------------------
    // If not specified, default to STYLE_HEAVY.
    m_sessionInfo.SetGameStyle( STYLE_HEAVY );
    m_hostedSession.GameStyle = m_sessionInfo.GetGameStyle();

    // ConfigInfo
    //---------------------------------------------------------------------
    // The first and only blob attribute.  
    // This is only used to demonstrate how to set a blob attribute
    m_sessionInfo.SetConfigInfo( B( 3, "ABC" ) );
    m_hostedSession.ConfigInfo = m_sessionInfo.GetConfigInfo();


    // The first (and only) user parameter is the player name
    WCHAR strUserName[XONLINE_GAMERTAG_SIZE];
    XBUtil_GetWide( m_rwStoredUsers[m_wCurUserIndex].szGamertag, strUserName, 
                    XONLINE_GAMERTAG_SIZE );

    m_sessionInfo.SetOwnerName( strUserName );
    m_hostedSession.OwnerName = strUserName; 
    
    m_dwSlotsInUse = 1;
    
    // Limit the number of players to MAX_PLAYERS_PER_GAME public slots and no
    // private (invitation only) slots. Note that we add ourself as a player.
    m_hostedSession.m_dwPublicFilled  = m_dwSlotsInUse;
    m_hostedSession.m_dwPublicOpen    = MAX_PLAYERS_PER_GAME - m_dwSlotsInUse;
    m_hostedSession.m_dwPrivateFilled = 0;
    m_hostedSession.m_dwPrivateOpen   = 0;

    m_bArbitrationStarted = FALSE;

    HRESULT hrCreate = m_hostedSession.Create();

    // Session failed to start
    if( FAILED( hrCreate ) )
    {
        SetState( STATE_GAME_SETUP );
    }
}

//-------------------------------------------------------------------------------------
// Name: UpdateStateCreateMatch
// Desc: Continues the match creation and allows the host the chance
//       to cancel the action.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateCreateMatch( Event event )
{
    // Wait for matchmaking server to save new session info
    HRESULT hrProcess = m_hostedSession.Process();

    // Handle errors
    if( FAILED( hrProcess ) )
    {
        SetState( STATE_GAME_SETUP );
        return;
    }
    else
    {
        if( hrProcess != XONLINETASK_S_RUNNING )
        {
            // Handle errors
            if( FAILED( hrProcess ) )
            {
                SetState( STATE_GAME_SETUP );
                return;
            }
            
            // We are now the host of a new game
            m_bIsHost = TRUE;

            WCHAR strUserName[XONLINE_GAMERTAG_SIZE];

            XBUtil_GetWide( m_rwStoredUsers[m_wCurUserIndex].szGamertag, strUserName, 
                            XONLINE_GAMERTAG_SIZE );
            m_networkMessageHandler.SetUser( strUserName, m_bIsHost );
            m_networkMessageHandler.SetSessionID( m_hostedSession.m_SessionID );
            
            // Note that the session remains "active" (m_hMatchTask isn't
            // closed), and must be pumped in order for the session to
            // remain active on the matchmaking server.
            m_heartbeatTimer.StartZero();
            SetState( STATE_GAME_LOBBY );
        }
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateCreateMatch()
// Desc: Shows the user that the match is being created.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateCreateMatch()
{
    // Tell the player that the match is being created.
    m_font.DrawText( SCREEN_CENTER_X, POS_MESSAGE_Y, COLOR_NORMAL, 
                     L"Creating Match...", XBFONT_CENTER_X );
}


/////////////////////
// State GameLobby //
/////////////////////

//-------------------------------------------------------------------------------------
// Name: EnterStateGameLobby()
// Desc: Setups the menu items for the game lobby.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::EnterStateGameLobby()
{
    m_iItemSelected  = 0;
    m_bWaitingToJoin = FALSE;
}

//-------------------------------------------------------------------------------------
// Name: UpdateStateGameLobby()
// Desc: Allows the users to interact before the game starts. This is the
//       collection point for all users. Users can leave the game if they wish.
//       The host (ONLY THE HOST) can start the game when all users are ready. When 
//       the match is started the host will send a network message to all peers 
//       requesting that they register with the arbitration service. Once all peers are
//       registered then the game may start.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateGameLobby( Event event )
{
    // Handle net messages
    if( m_networkMessageHandler.ProcessMessages( m_rwPlayers ) )
        return;
    
    // Send keep-alives
    if( m_heartbeatTimer.GetElapsedSeconds() > PLAYER_HEARTBEAT )
    {
        m_networkMessageHandler.SendHeartbeat( m_rwPlayers );
        m_heartbeatTimer.StartZero();
    }
    
    // Handle other players dropping
    if( m_networkMessageHandler.ProcessPlayerDropouts( m_rwPlayers,
                                                       PLAYER_TIMEOUT ) )
        return;
    
    // Handle session updates
    if( m_bIsHost )
    {
        HRESULT hrProcess = m_hostedSession.Process();
        if( hrProcess != XONLINETASK_S_RUNNING )
        {
            // Handle errors
            if( FAILED( hrProcess ) )
            {
                SetState( STATE_GAME_SETUP );
                return;
            }
        }
    }

    // Do not process any input while waiting for
    // arbitration registration to finish
    if ( m_bWaitingToJoin )
        return;

    // Give the user a menu & chance to exit the game before
    // it starts.
    // The host should have the extra option to start
    // the game. Perhaps the ELO rating of each
    // player should be displayed?
    switch( event )
    {
    case EV_BUTTON_B: // Leave the game
        if ( m_bIsHost )
            DeleteSession();
        else
            LeaveGame();

        m_bIsHost = FALSE;
        SetState( STATE_GAME_SETUP );
        break;

    case EV_BUTTON_A: // The user selected
        switch( m_iItemSelected )
        {
        case 0: // Leave the game
            if ( m_bIsHost )
                DeleteSession();
            else
                LeaveGame();

            m_bIsHost = FALSE;
            SetState( STATE_GAME_SETUP );
            break;

        case 1: //Start the arbitrated game
            assert( m_bIsHost );

            if ( m_dwSlotsInUse > 1 )
            {
                // Start the arbitration registration and ask
                // all the peers to register as well
                m_bWaitingToJoin = StartArbitratedGameRegistration();

                if ( m_bWaitingToJoin )
                    SetState( STATE_GAME_SESSION );
                else
                    SetState( STATE_GAME_LOBBY );
            }
            break;
        }
        break;

    case EV_UP:  // Move the cursor up and down
    case EV_DOWN:
        if ( m_bIsHost )
        {
            m_iItemSelected = ( m_iItemSelected == 0 ) ? 1 : 0;
        }
        break;
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateGameLobby()
// Desc: Show the options to the peers and the host. Only the host
//       can start the game, so we need to highlight options
//       differently is we are not the host.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateGameLobby()
{
    // Header
    m_font.DrawText( SCREEN_CENTER_X, POS_SCREEN_TITLE_Y, COLOR_NORMAL,
                     L"GAME LOBBY", XBFONT_CENTER_X );

    if ( m_bWaitingToJoin )
    {
        m_font.DrawText( SCREEN_CENTER_X, POS_MESSAGE_Y, COLOR_RED,
                         L"Starting match...", XBFONT_CENTER_X );
        return;
    }

    // Convert user name to WCHAR string
    WCHAR strUserName[XONLINE_GAMERTAG_SIZE];
    XBUtil_GetWide( m_rwStoredUsers[m_wCurUserIndex].szGamertag,
                    strUserName, XONLINE_GAMERTAG_SIZE );

    m_font.DrawText( ( SCREEN_CENTER_X - WIDTH_VERSUS_X ), POS_VERSUS_Y,
                     COLOR_GREEN, strUserName, XBFONT_RIGHT );

    m_font.DrawText( SCREEN_CENTER_X, POS_VERSUS_Y, COLOR_RED,
                     L"VS", XBFONT_CENTER_X );

    // Draw the opponent if they are in the lobby
    if ( m_rwPlayers.size() )
    {
        m_font.DrawText( ( SCREEN_CENTER_X + WIDTH_VERSUS_X ), POS_VERSUS_Y,
                         COLOR_GREEN, m_rwPlayers[0].strPlayerName,
                         XBFONT_LEFT );
    }

    // Show list of user accounts
    for( DWORD i = 0; i < NUM_ITEMS_GAME_LOBBY; ++i )
    {
        // Highlight the selected item
        DWORD dwColor = ( (DWORD)m_iItemSelected == i ) ? 
                          COLOR_HIGHLIGHT : COLOR_NORMAL;
        dwColor       = ( ( i == ((DWORD)NUM_ITEMS_GAME_LOBBY - 1 ) ) 
                          && ( !m_bIsHost ) ) ? COLOR_GREY : dwColor;

        // Grey out the "Start Game" item if we do not
        // have enough players for a match
        if ( m_bIsHost && ( i == ( (DWORD)NUM_ITEMS_GAME_LOBBY - 1) ) )
        {
            dwColor = ( m_bIsHost && ( m_dwSlotsInUse < 2 ) ) ? COLOR_GREY : dwColor;
        }

        // Render the account name
        m_font.DrawText( SCREEN_CENTER_X, POS_MENU_START_Y + 
                         ( DEFAULT_TEXT_PADDING * i ),
                         dwColor, MENU_GAME_LOBBY[i], XBFONT_CENTER_X );
    }

    // Show selected item with little triangle
    FLOAT fTextOffset   = ( m_font.GetTextWidth( MENU_GAME_LOBBY[m_iItemSelected] ) 
                            / 2.0f );
    FLOAT fTextPos      = SCREEN_CENTER_X - 
                          ( fTextOffset + m_font.GetTextWidth( GLYPH_RIGHT_TICK ) );

    m_font.DrawText( fTextPos, POS_MENU_START_Y + 
                     ( DEFAULT_TEXT_PADDING * m_iItemSelected ),
                     COLOR_POINTER, GLYPH_RIGHT_TICK, XBFONT_CENTER_X );


    RenderFooter( FOOTER_RENDER_CANCEL | FOOTER_RENDER_SELECT );
}

//-------------------------------------------------------------------------------------
// Name: ExitStateGameLobby()
// Desc: Exits the game lobby and initializes the menu selector
//-------------------------------------------------------------------------------------
VOID CXBoxSample::ExitStateGameLobby()
{
    m_iItemSelected = 0;
}


///////////////////////
// State GameSession //
///////////////////////

//-------------------------------------------------------------------------------------
// Name: EnterStateGameSession()
// Desc: Initializes the scores to zero and sets the menu to point
//       to "Score Point".
//-------------------------------------------------------------------------------------
VOID CXBoxSample::EnterStateGameSession()
{
    m_dwScores[0]       = 0;
    m_dwScores[1]       = 0;

    m_iItemSelected     = 0;
    m_wLocalPlayerIndex = (WORD)m_rwPlayers.size(); // the local player is 
                                                    // always the last

    assert(m_wLocalPlayerIndex > 0);

    BackupPlayerNames();
}

//-------------------------------------------------------------------------------------
// Name: UpdateStateGameSession()
// Desc: Updates the game. Allows the player to score a point, allows the player
//       to "cheat a point" and allows the player to leave the game.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateGameSession( Event event )
{
    // Send keep-alives
    if( m_heartbeatTimer.GetElapsedSeconds() > PLAYER_HEARTBEAT )
    {
        m_networkMessageHandler.SendHeartbeat( m_rwPlayers );
        m_heartbeatTimer.StartZero();
    }
    
    // Handle other players dropping. If they drop after registering for
    // arbitration then that is much more serious than dropping normally,
    // as it will affect their reliability rating.
    if( m_networkMessageHandler.ProcessPlayerDropouts( m_rwPlayers,
                                                       PLAYER_TIMEOUT ) )
        return;
    
    // Handle session updates
    if( m_bIsHost )
    {
        HRESULT hrProcess = m_hostedSession.Process();

        if( hrProcess != XONLINETASK_S_RUNNING )
        {
            // Handle errors
            if( FAILED(hrProcess) )
            {
                // Let the player know a network
                // error happened!
                SetState(STATE_GAME_RESULTS);
                return;
            }
        }
    }

    switch( event )
    {
    case EV_UP: // Move the menu cursor up
        --m_iItemSelected;
        // Wrap the input to goto the bottom
        m_iItemSelected = ( m_iItemSelected < 0 ) ? 
                          ( NUM_ITEMS_GAME_SESSION - 1 ) : m_iItemSelected;
        break;

    case EV_DOWN: // Move the menu cursor down
        ++m_iItemSelected;
        // Wrap the input to goto the top
        m_iItemSelected = ( m_iItemSelected >= NUM_ITEMS_GAME_SESSION ) ? 
                          0 : m_iItemSelected;
        break;

    case EV_BUTTON_A: // Select an item
        assert( m_iItemSelected >= 0 );
        assert( m_iItemSelected < NUM_ITEMS_GAME_SESSION );

        switch( m_iItemSelected )
        {
        case 0: // SCORE A POINT!
            {
                CXBNetBlob blob;

                m_dwScores[m_wLocalPlayerIndex] += 1;
                // Send this score information to all the other players.
                blob.blobType = BLOB_I_SCORE;

                // The blob expects a WIDE character array!
                WCHAR strUserName[XONLINE_GAMERTAG_SIZE];
                XBUtil_GetWide( m_rwStoredUsers[m_wCurUserIndex].szGamertag, 
                                strUserName, XONLINE_GAMERTAG_SIZE );

                blob.dataSize = sizeof(strUserName);

                // Copy the entire gamer tag (should optimize and just copy
                // the valid characters).
                memcpy( blob.data, strUserName, blob.dataSize );
                m_networkMessageHandler.SendBlob( m_rwPlayers, blob );
            }
            break;

        case 1: // CHEAT A POINT!
            // Scoring a point without telling the other
            // peers on the network will cause a discrepency
            // when the arbitration report is submitted
            m_dwScores[m_wLocalPlayerIndex] += 1;
            break;

        case 2: // QUIT GRACEFULLY
            // Send out a message that we
            // are quiting the game and that
            // arbitration reports are being submitted
            {
                CXBNetBlob blob;
                blob.blobType = BLOB_GAME_OVER;
                blob.dataSize = 0;  // There is no data associated with this packet.

                // Let the other players know that the game is over.
                m_networkMessageHandler.SendBlob( m_rwPlayers, blob );
            }

            SetState( STATE_GAME_RESULTS );
            break;
        }
        break;
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateGameSession()
// Desc: Renders the game to the screen. Show the scores of the current players
//       as well as the options available to the players.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateGameSession()
{
    // Header
    m_font.DrawText( SCREEN_CENTER_X, POS_SCREEN_TITLE_Y, COLOR_NORMAL,
                     L"GAME MATCH", 
                     XBFONT_CENTER_X );
    
    // Convert user name to WCHAR string
    WCHAR strUserName[XONLINE_GAMERTAG_SIZE];
    XBUtil_GetWide( m_rwStoredUsers[m_wCurUserIndex].szGamertag,
                    strUserName, XONLINE_GAMERTAG_SIZE );

    m_font.DrawText( ( SCREEN_CENTER_X - WIDTH_VERSUS_X ), POS_VERSUS_Y,
                     COLOR_GREEN, strUserName, XBFONT_RIGHT );

    // Score beneath local player
    _snwprintf( strUserName, XONLINE_GAMERTAG_SIZE, L"%i", m_dwScores[1] );
    m_font.DrawText( ( SCREEN_CENTER_X - WIDTH_VERSUS_X ),
                     ( POS_VERSUS_Y + POS_GAME_SCORE_PADDING ),
                     COLOR_NORMAL, strUserName, XBFONT_RIGHT );

    //////////////////////////////

    m_font.DrawText( SCREEN_CENTER_X, POS_VERSUS_Y, COLOR_RED,
                     L"VS",
                     XBFONT_CENTER_X );

    m_font.DrawText( ( SCREEN_CENTER_X + WIDTH_VERSUS_X ),
                     POS_VERSUS_Y, COLOR_GREEN,
                     m_rwPlayers[0].strPlayerName,
                     XBFONT_LEFT );

    // Score beneath opponent
    _snwprintf( strUserName, XONLINE_GAMERTAG_SIZE, L"%i",
                             m_dwScores[0] );

    m_font.DrawText( ( SCREEN_CENTER_X + WIDTH_VERSUS_X ),
                     ( POS_VERSUS_Y + POS_GAME_SCORE_PADDING ),
                     COLOR_NORMAL, strUserName, XBFONT_LEFT );

    // Show list of user accounts
    for( DWORD i = 0; i < NUM_ITEMS_GAME_SESSION; ++i )
    {
        // Highlight the selected item
        DWORD dwColor = ( (DWORD)m_iItemSelected == i ) ? 
                        COLOR_HIGHLIGHT : COLOR_NORMAL;

        m_font.DrawText( SCREEN_CENTER_X,
                         POS_MENU_START_Y + ( DEFAULT_TEXT_PADDING * i ),
                         dwColor, MENU_GAME_SESSION[i], XBFONT_CENTER_X );
    }

    // Show selected item with little triangle
    FLOAT fTextOffset   = ( m_font.GetTextWidth( MENU_GAME_SESSION[m_iItemSelected] ) 
                            / 2.0f );
    FLOAT fTextPos      = SCREEN_CENTER_X - 
                          ( fTextOffset + m_font.GetTextWidth( GLYPH_RIGHT_TICK ) );
    FLOAT yPos          = POS_MENU_START_Y + 
                          ( DEFAULT_TEXT_PADDING * m_iItemSelected );

    m_font.DrawText( fTextPos, yPos, COLOR_POINTER, GLYPH_RIGHT_TICK, 
                     XBFONT_CENTER_X );

    RenderFooter( FOOTER_RENDER_SELECT );
}

//-------------------------------------------------------------------------------------
// Name: BackupPlayerNames()
// Desc: Copies the game players' names to a backup buffer in case of a dropout, and
//       also for game results display purposes later.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::BackupPlayerNames()
{
    INT j = 0;

    // clear saved players, just in case
    for (j = 0; j < MAX_MATCHERS; ++j)
    {
        lstrcpynW( m_rwSavedPlayers[j], L"", 1 );
    }

    // iterate through our current players and
    // back them up into our saved player list
    CXBNetPlayerList::const_iterator i = m_rwPlayers.begin();
    j = 0;

    for( ; i != m_rwPlayers.end(); ++i )
    {
        CXBNetPlayerInfo playerInfo = *i;
        lstrcpynW( m_rwSavedPlayers[j], playerInfo.strPlayerName,
                   XONLINE_GAMERTAG_SIZE );
        ++j;

        assert( j < MAX_MATCHERS );
    }
}

//-------------------------------------------------------------------------------------
// Name: ExitStateGameSession()
// Desc: Exits the game session. Prepares an ELO report for submission to Xbox Live.
//       (If game results differ, a flag will be set to note that.)
//-------------------------------------------------------------------------------------
VOID CXBoxSample::ExitStateGameSession()
{
    // Cleanup to make sure the game session
    // has finished
    if ( m_bIsHost )
         DeleteSession();

    // no items to select
    m_iItemSelected           = 0;

    // reset write error to none
    m_iStatWriteError         = STAT_WRITE_NO_ERR;
    // and reset the results differ flag to false
    m_bResultsDiffer          = FALSE;

    // sort XUIDs by value... determine which is the smaller value
    // (note: both IDs should NOT be the same!)
    // The reason we want indices based on the values of player XUID user IDs is 
    // this allows an arbitration report to be sent by both users such that the 
    // order of players in the report sent is guaranteed to remain the same for 
    // both reports..
    assert( m_rwPlayerXUIDs[0].qwUserID != m_rwPlayerXUIDs[1].qwUserID );
    INT iLowerXuidIndex = ( ( m_rwPlayerXUIDs[0].qwUserID < 
                              m_rwPlayerXUIDs[1].qwUserID ) ?
                              0 : 1 );
    INT iHigherXuidIndex = ( iLowerXuidIndex + 1 ) % 2;

    // In our case, ELO Xuid1 will be the HIGHER valued XUID user ID,
    // and ELO Xuid2 will be the lower
    DWORD  dwScoreXuid1 = m_dwScores[ iHigherXuidIndex ];
    DWORD  dwScoreXuid2 = m_dwScores[ iLowerXuidIndex ];

    // determine the winner for future reference
    m_iWinnerIndex = ( ( m_dwScores[ iLowerXuidIndex ] > 
                         m_dwScores[ iHigherXuidIndex ] ) ? 
                         iLowerXuidIndex : iHigherXuidIndex );

    // calculate the summation of both scores
    DWORD dwScoreSum = dwScoreXuid1 + dwScoreXuid2;

    // to avoid divide-by-zero errors, if BOTH scores are zero, set the 
    // weights to 0.5, since both players tied. Else, just set the weight 
    // to the proportion of Xuid1's score over the total amount of points 
    // scored by both
    DOUBLE dWeight;
    if ( dwScoreSum == 0 )
    {
        dWeight = 0.50;
    }
    else
    {
        dWeight = (DOUBLE)( dwScoreXuid1 ) / dwScoreSum;
    }

    // task for arbitration report
    XONLINETASK_HANDLE hArbRep;
    
    // constants used for Elo arbitration reporting.
    // These should usually be set to 1.00 each, unless you require that
    // new players be given a break in regards to ELO ratings, in which case
    // setting them to something lower like 0.50 or 0.25 would be more
    // appropriate, as lower settings will not allow the rating values to 
    // fluctuate as much per match
    const DOUBLE DEFAULT_ELO_C1 = 1.00;
    const DOUBLE DEFAULT_ELO_C2 = 1.00;

    // construct an ELO structures
    ZeroMemory( &m_statElo, sizeof( m_statElo ) );
    ZeroMemory( &m_statWriteProc, sizeof( m_statWriteProc ) );

    // this proc uses Elo
    m_statWriteProc.wProcedureID = XONLINE_STAT_PROCID_ELO;

    // grab xuids from both players in order of higher xuid value first
    m_statElo.xuid1              = m_rwPlayerXUIDs[ iHigherXuidIndex ];
    m_statElo.xuid2              = m_rwPlayerXUIDs[ iLowerXuidIndex ];

    // use default leaderboard
    m_statElo.dwLeaderBoardID    = DEFAULT_LEADERBOARD_ID;

    // (conditional index not used)
    m_statElo.dwConditionalIndex = 0;

    // set weight of player 1
    m_statElo.W                  = dWeight;

    // set constants
    m_statElo.C1                 = DEFAULT_ELO_C1;
    m_statElo.C2                 = DEFAULT_ELO_C2;

    // set Elo in our proc
    m_statWriteProc.Elo          = m_statElo;

    // we need to create a flag depending on whether we are the host or not
    DWORD dwArbFlags = ( m_bIsHost ? XONLINE_ARB_REPORT_FLAG_WAS_HOST : 0 );

    // attempt to write the arbitration report
    HRESULT hrArbRep = XOnlineArbitrationReport(
            &m_arbID,                // Arbritration ID
            1,                       // number of statistical entries for stat procs
            &m_statWriteProc,        // address of stat procs
            NULL,                    // array of arbitration reports
            dwArbFlags,              // will flag as host, otherwise no flags
            NULL,                    // will pump task ourselves
            &hArbRep                 // arb report task
        );

    // if failed right away, error out
    if ( FAILED( hrArbRep ) )
    {
        m_iStatWriteError      = STAT_WRITE_ERR_FAILED;
        m_dwStatWriteErrorCode = (DWORD)hrArbRep;

        CloseGameSessionTask( hArbRep );
        return;
    }

    HRESULT hrLogonTask;

    // loop until the writing task is done
    do 
    { 
        hrLogonTask = XOnlineTaskContinue( m_hLogonTask );

        if ( FAILED( hrLogonTask ) )
        {
            // A network error occured.
            // Close the report task and let the user know
            CloseGameSessionTask( hArbRep );
            SetState( STATE_NETWORK_ERROR );
            return;
        }

        hrArbRep = XOnlineTaskContinue( hArbRep );
    } 
    while ( hrArbRep == XONLINETASK_S_RUNNING );

    // if failed, error out
    if ( FAILED( hrArbRep ) )
    {
        m_iStatWriteError      = STAT_WRITE_ERR_FAILED;
        m_dwStatWriteErrorCode = (DWORD)hrArbRep;

        // close the task
        CloseGameSessionTask( hArbRep );
        return;
    }
    else
    {
        // The report submission may succeed, but other errors might occur
        // in the context of our mission
        switch( hrArbRep )
        {
        // invalid xbox or user specified
        case XONLINE_S_ARBITRATION_INVALID_XBOX_SPECIFIED:
        case XONLINE_S_ARBITRATION_INVALID_USER_SPECIFIED:

            m_iStatWriteError      = STAT_WRITE_ERR_FAILED;
            m_dwStatWriteErrorCode = (DWORD)hrArbRep;

            // close the task
            CloseGameSessionTask( hArbRep );
            return;

        // results may differ (due to cheating or other events.)  
        // We will not error out, but will want to notify our players of this 
        // situation.  
        case XONLINE_S_ARBITRATION_DIFFERENT_RESULTS_DETECTED:
            m_bResultsDiffer = TRUE;
            break;

        default:
            // unexpected success result
            break;
        }

    }

    // close the task, and report closing errors if they occur
    CloseGameSessionTask( hArbRep , TASK_CLOSE_SET_ERRORS );
}

///////////////////////
// State GameResults //
///////////////////////

//-------------------------------------------------------------------------------------
// Name: EnterStateGameResults()
// Desc: Does any necessasry initialization to display the match results
//-------------------------------------------------------------------------------------
VOID CXBoxSample::EnterStateGameResults()
{
    m_iItemSelected = 0;
}

//-------------------------------------------------------------------------------------
// Name: UpdateStateGameResults()
// Desc: Allows the user to continue on to the GameSetup screen when they
//       are done viewing the results of the game.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateGameResults( Event event )
{
    switch( event )
    {
    case EV_BUTTON_A:
        SetState( STATE_GAME_SETUP );
        break;
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateGameResults()
// Desc: Draws the results of the match (winner, user, and score) to the screen. 
//       Will display an error if there was an error submitting an arbitration report.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateGameResults()
{
    
    // Header
    m_font.DrawText( SCREEN_CENTER_X, POS_SCREEN_TITLE_Y, COLOR_NORMAL,
                     L"GAME RESULTS", 
                     XBFONT_CENTER_X );
        
    // if there was a write error, display error
    if ( m_iStatWriteError != STAT_WRITE_NO_ERR )
    {
        const FLOAT STAT_WRITE_ERROR_MSG_OFFSET_X = 50.0f;
        const FLOAT STAT_WRITE_ERROR_MSG_OFFSET_Y = POS_MESSAGE_Y;
        const FLOAT STAT_WRITE_ERROR_MSG_PAD_Y    = DEFAULT_TEXT_PADDING;
        const INT STAT_WRITE_ERROR_MAX_MSG_SIZE   = 256;

        WCHAR sErrMsgW[STAT_WRITE_ERROR_MAX_MSG_SIZE];
        CHAR  sErrMsg[STAT_WRITE_ERROR_MAX_MSG_SIZE];

        switch( m_iStatWriteError )
        {
        case STAT_WRITE_ERR_FAILED:
            _snprintf( sErrMsg, STAT_WRITE_ERROR_MAX_MSG_SIZE,
                       "Arbitration reporting failed with error code 0x%x." , 
                       m_dwStatWriteErrorCode );
            break;

        case STAT_WRITE_ERR_CLOSING_TASK:
            _snprintf( sErrMsg, STAT_WRITE_ERROR_MAX_MSG_SIZE,
                       "Closing report task failed with error code 0x%x." , 
                       m_dwStatWriteErrorCode );
            break;

        default:
            // should not happen
            assert( FALSE );
        }
           
        XBUtil_GetWide( sErrMsg, sErrMsgW, STAT_WRITE_ERROR_MAX_MSG_SIZE );

        // draw error
        m_font.DrawText(STAT_WRITE_ERROR_MSG_OFFSET_X, STAT_WRITE_ERROR_MSG_OFFSET_Y , 
                        COLOR_NORMAL, L"Game Results Error:", XBFONT_LEFT);

        m_font.DrawText(STAT_WRITE_ERROR_MSG_OFFSET_X, STAT_WRITE_ERROR_MSG_OFFSET_Y 
                        + STAT_WRITE_ERROR_MSG_PAD_Y, COLOR_NORMAL, sErrMsgW, 
                        XBFONT_LEFT);

    }
    else
    {
        // we will display the winner's stats vertically

        // horizontal offsets 
        const FLOAT POS_WINNER_RANK_X = 265.0f;
        const FLOAT POS_LOSER_RANK_X = 450.0f;
        const FLOAT POS_INFO_X = 50.0f;

        // vertical offsets
        const FLOAT POS_TITLE_Y  = 140.0f;
        const FLOAT POS_PAD_Y = 40.0f;
        const FLOAT POS_START_Y =  POS_TITLE_Y + POS_PAD_Y;

        // type for each row display result
        enum
        {
            RESULTS_POS_Y_INDEX_WINNER_TITLE,
            RESULTS_POS_Y_INDEX_TAG,
            RESULTS_POS_Y_INDEX_SCORE,
 
            NUM_RESULTS_POS_Y_INDICES
        };

        // names for each row
        const WCHAR*    RESULTS_ROW_NAME[ NUM_RESULTS_POS_Y_INDICES ] =
        {
            L"",
            L"GAMERTAG",
            L"SCORE",
        };

        // precalculate the y offsets for each row
        FLOAT rfRowYOffset[ NUM_RESULTS_POS_Y_INDICES ];
        for ( DWORD i = 0 ; i < NUM_RESULTS_POS_Y_INDICES ; ++i )
        {
            rfRowYOffset[i] = POS_START_Y + ( i * POS_PAD_Y );
        }

        // user name
        WCHAR sUserName[XONLINE_GAMERTAG_SIZE];

        // user score
        WCHAR sUserScore[XONLINE_GAMERTAG_SIZE];

        // if different results occured, say so!
        if ( m_bResultsDiffer )
        {
            // draw results differ banner
            m_font.DrawText( POS_INFO_X , POS_TITLE_Y , COLOR_HIGHLIGHT , 
                L"NOTE: Match results differed!" , XBFONT_LEFT );
        }

        // winner ranks: "0" is the winner, "1" is the loser.. or if tied, 
        // it doesn't matter
        INT iWinnerRanks[ MAX_PLAYERS_PER_GAME ];
        iWinnerRanks[0] = m_iWinnerIndex;
        iWinnerRanks[1] = ( m_iWinnerIndex + 1 ) % 2;

        // draw headers for rank, user tag, and score
        for ( DWORD i = 0 ; i < NUM_RESULTS_POS_Y_INDICES ; ++i )
        {
            m_font.DrawText( POS_INFO_X , rfRowYOffset[ i ] , COLOR_RED , 
                             RESULTS_ROW_NAME[ i ] , XBFONT_LEFT );
        }

        // draw winner banner
        m_font.DrawText( POS_WINNER_RANK_X , 
                         rfRowYOffset[ RESULTS_POS_Y_INDEX_WINNER_TITLE ] ,
                         COLOR_HIGHLIGHT , L"**WINNER**" , XBFONT_CENTER_X );

        // Show list of user accounts
        for( DWORD i = 0 ; i < MAX_PLAYERS_PER_GAME ; ++i )
        {
            // get user index
            INT iUserIndex = iWinnerRanks[i];

            // construct user name
            // if user index is local player
            if ( iUserIndex == 1 )
            {
                XBUtil_GetWide( m_rwStoredUsers[m_wCurUserIndex].szGamertag , 
                                sUserName , XONLINE_GAMERTAG_SIZE);    
            }
            else 
            {
                // copy string from net player
                lstrcpynW( sUserName, m_rwSavedPlayers[iUserIndex], 
                           XONLINE_GAMERTAG_SIZE );
            }

            // construct score string
            _itow((INT)( m_dwScores[iUserIndex] ), sUserScore , 10 );

            // determine x offset
            FLOAT xPos = (FLOAT)( i ? POS_LOSER_RANK_X : POS_WINNER_RANK_X );
            
            // draw user name and score
            m_font.DrawText( xPos, rfRowYOffset[RESULTS_POS_Y_INDEX_TAG], 
                             COLOR_NORMAL, sUserName, XBFONT_CENTER_X );
            m_font.DrawText( xPos, rfRowYOffset[RESULTS_POS_Y_INDEX_SCORE], 
                             COLOR_NORMAL, sUserScore, XBFONT_CENTER_X );            
        }
    }
            
    RenderFooter( FOOTER_RENDER_SELECT );
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
                     L"Elo Ratings", 
                     XBFONT_LEFT );
}

//-------------------------------------------------------------------------------------
// Name: RenderFooter()
// Desc: Renders a footer at the bottom of the screen. Takes a bitflag to
//       determine which items to render.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderFooter( WORD flags )
{
    if ( flags & FOOTER_RENDER_CANCEL )
    {
        // Bottom Help text
        m_font.DrawText( POS_FOOTER_LEFT, POS_FOOTER_Y,
                         COLOR_NORMAL, GLYPH_B_BUTTON L" back", 
                         XBFONT_LEFT );
    }

    if ( flags & FOOTER_RENDER_SELECT )
    {
        m_font.DrawText( POS_FOOTER_RIGHT, POS_FOOTER_Y,
                         COLOR_NORMAL, GLYPH_A_BUTTON L" select", 
                         XBFONT_RIGHT );
    }
}

// Functions to handle network messages

//-------------------------------------------------------------------------------------
// Name: OnJoinGame()
// Desc: Handle new player joining game that we host
//-------------------------------------------------------------------------------------
VOID CXBoxSample::OnJoinGame( const CXBNetPlayerInfo& playerInfo )
{
    // Protect ourselves from stale messages
    if ( m_state != STATE_GAME_LOBBY )
    {
        return;
    }

    m_rwPlayers.push_back( playerInfo );

    // Add the player
    assert( m_bIsHost );
    ++m_dwSlotsInUse;

    if( m_dwSlotsInUse == MAX_PLAYERS_PER_GAME )
    {
        // The session is now full. Turn off Qos listening.
        // This will send "go-away" responses to probes from
        // other consoles who will see this session during 
        // matchmaking while it is being updated (once the
        // session is updated, new searches will not return
        // it since there will be no public slots)
        m_hostedSession.Listen( FALSE );
    }

    UpdateSession();
}

//-------------------------------------------------------------------------------------
// Name: OnJoinApproved()
// Desc: We've been approved for game entry by the given host
//-------------------------------------------------------------------------------------
VOID CXBoxSample::OnJoinApproved( const CXBNetPlayerInfo& hostInfo )
{
    if ( m_state != STATE_GAME_LOBBY )
        return;

    m_rwPlayers.push_back( hostInfo );
    
    // Enter into the game UI
    SetState(STATE_GAME_LOBBY);
    
    m_heartbeatTimer.StartZero();

    WCHAR strUserName[XONLINE_GAMERTAG_SIZE];
    XBUtil_GetWide( m_rwStoredUsers[m_wCurUserIndex].szGamertag,
                    strUserName, XONLINE_GAMERTAG_SIZE );

    m_networkMessageHandler.SetUser( strUserName, FALSE );
}

//-------------------------------------------------------------------------------------
// Name: OnJoinApprovedAddPlayer()
// Desc: Receiving information on others players already in the game
//-------------------------------------------------------------------------------------
VOID CXBoxSample::OnJoinApprovedAddPlayer( const CXBNetPlayerInfo& playerInfo )
{
    if ( m_state != STATE_GAME_LOBBY )
        return;

    if( playerInfo.qwUserID != m_rwStoredUsers[m_wCurUserIndex].xuid.qwUserID )
        m_rwPlayers.push_back( playerInfo );
}

//-------------------------------------------------------------------------------------
// Name: OnJoinDenied()
// Desc: Handle join denied
//-------------------------------------------------------------------------------------
VOID CXBoxSample::OnJoinDenied()
{
    // If for some reason we receive a "join denied" message and we're
    // already playing a game, ignore the message.
    if( m_state == STATE_GAME_SESSION )
        return;
    
    // The session we wanted to join is full. Display error
    SetState( STATE_GAME_SETUP );

    // NOTE:
    // A real game should have a mechanism to attempt to join another
    // game session so the player always will end find a game unless
    // absolutely no games exist
}

//-------------------------------------------------------------------------------------
// Name: OnPlayerJoined()
// Desc: The given player joined our game
//-------------------------------------------------------------------------------------
VOID CXBoxSample::OnPlayerJoined( const CXBNetPlayerInfo& playerInfo )
{
    if ( m_state != STATE_GAME_LOBBY )
        return;

    MatchInAddr matchInAddr( playerInfo.inAddr );
    
    // First check to make sure the player isn't already in the list.
    // If so, remove the player first.  This can happen if the player
    // drops out of a game and rejoins before the next heartbeat.
    CXBNetPlayerList::iterator i = 
        std::find_if( m_rwPlayers.begin(), m_rwPlayers.end(), matchInAddr );
    
    if( i != m_rwPlayers.end() )
    {       
        m_rwPlayers.erase( i );
    }
    
    m_rwPlayers.push_back( playerInfo );
}

//-------------------------------------------------------------------------------------
// Name: OnWave()
// Desc: The given player waved to us
//-------------------------------------------------------------------------------------
VOID CXBoxSample::OnWave( const CXBNetPlayerInfo& playerInfo )
{
    // Ignore waves for simplicity
}

//-------------------------------------------------------------------------------------
// Name: OnBlob
// Desc: React to a network blob (aka message) the we recieved from the given player
//-------------------------------------------------------------------------------------
VOID CXBoxSample::OnBlob( const CXBNetPlayerInfo& playerInfo,
                          const CXBNetBlob& blob )
{
    switch( blob.blobType )
    {
    case BLOB_ARB_ID:
        if ( m_state != STATE_GAME_LOBBY )
            return;

        // We've received the arbitration ID from the host - an arbitrated game is
        // about to begin. This is our cue to register with the arbitration
        // service, and notify the host when we are done.
        assert( blob.dataSize == sizeof( m_arbID ) );

        m_arbID               = *(XONLINE_ARB_ID*)blob.data;
        m_bArbitrationStarted = RegisterForArbitration();

        if (! m_bArbitrationStarted )
        {
            SetState( STATE_GAME_SETUP );
        }

        break;

    case BLOB_REGISTERED:
        // This message is received by the host whenever one of the players registers
        // with the arbitration server. If everybody registers then we can stop 
        // waiting. We should increment our count of how many players are registered 
        // by the number of players on that box - currently I just increment by one, 
        // which is adequate if you only allow one player per box. This message is 
        // currently broadcast to all players. All but the host should ignore it.
        if( m_bIsHost )
        {
            if ( ( m_state != STATE_GAME_LOBBY ) && ( m_state != STATE_GAME_SESSION ) )
                return;

            ++m_dwPlayersRegistered;

            m_bWaitingToJoin = ( m_dwPlayersRegistered == m_dwSlotsInUse );

            if ( !m_bWaitingToJoin )
                StartArbitratedGame();
        }
        break;

    case BLOB_XUIDS:
        // This message is sent by the host to all players when the host registers for
        // arbitration. It contains the XUIDs of all of the registered players.
        // Copy XUIDs to the appropriate locations.
        if ( m_state != STATE_GAME_LOBBY )
                return;

        CopyXUIDs( (XUID*)blob.data );
        break;

    case BLOB_GAME_START:
        // The host says the game starts, so the game starts.
        if ( m_state != STATE_GAME_LOBBY )
                return;

        StartArbitratedGame();
        break;

    case BLOB_I_SCORE:
        if ( m_state == STATE_GAME_SESSION )
        {
            // This message is sent by a player who scores. Everybody needs to record 
            // this so that identical data is sent to the arbitration server by all 
            // clients.
            const WCHAR* gamerTag = (WCHAR *)blob.data;

            // Find out who scored, and update their records.
            for( DWORD i = 0; i < m_rwPlayers.size(); ++i )
            {
                if( wcscmp( gamerTag, m_rwPlayers[i].strPlayerName ) == 0 )
                    m_dwScores[i] += 1;
            }
        }
        break;

    case BLOB_GAME_OVER:
        // This message is received when the game has been ended. All clients
        // need to make sure that they exchange any final game information,
        // to ensure that the game stat is synchronized. Then they can all
        // submit their results. The results package must be identical!
        if ( m_state != STATE_GAME_SESSION )
            return;

        SetState( STATE_GAME_RESULTS );
        break;

    case BLOB_CLIENT_LEFT:
        // This message is received when a client leaves the game
        // lobby. The host should remove them from the players
        // list and not allow the game to start
        //
        // Because we only support two players we can just
        // clear the player list
        assert( m_bIsHost );

        // Cause an immediate dropout
        m_networkMessageHandler.ProcessPlayerDropouts( m_rwPlayers, 0 );
        RemovePlayer();
        assert( m_dwSlotsInUse == 1 );
        break;

    default:
        assert( 0 );
    }
}

//-------------------------------------------------------------------------------------
// Name: OnHeartbeat()
// Desc: The given player sent us a heartbeat
//-------------------------------------------------------------------------------------
VOID CXBoxSample::OnHeartbeat( const CXBNetPlayerInfo& playerInfo )
{
    MatchInAddr matchInAddr( playerInfo.inAddr );
    
    // Find out who sent a heartbeat by matching on name
    CXBNetPlayerList::iterator i = 
        std::find_if( m_rwPlayers.begin(), m_rwPlayers.end(), matchInAddr );
    
    // We expect that we know about the player
    assert( i != m_rwPlayers.end() );
    
    i->dwLastHeartbeat = GetTickCount();
}

//-------------------------------------------------------------------------------------
// Name: OnPlayerDropout()
// Desc: The given player left the game
//-------------------------------------------------------------------------------------
VOID CXBoxSample::OnPlayerDropout( const CXBNetPlayerInfo& playerInfo, BOOL bIsHost )
{
    if( !m_bIsHost )
    {
        // If this console is the host, remove inform the matchmaking server
        if( m_bIsHost )
             RemovePlayer();
    }

    MatchInAddr matchInAddr( playerInfo.inAddr );
    
    // Find out who we need to delete by matching on name
    CXBNetPlayerList::iterator i = 
        std::find_if( m_rwPlayers.begin(), m_rwPlayers.end(), matchInAddr );
    
    // We expect that we know about the player
    assert( i != m_rwPlayers.end() );
    
    m_rwPlayers.erase( i );
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
    switch (m_state)
    {
    case STATE_SELECT_ACCOUNT:      RenderStateSelectAccount();      break;
    case STATE_LOGIN:               RenderStateLogin();              break;
    case STATE_LOGIN_FAILED:        RenderStateLoginFailed();        break;
    case STATE_NETWORK_ERROR:       RenderStateNetworkError();       break;
    case STATE_GAME_SETUP:          RenderStateGameSetup();          break;
    case STATE_VIEW_RATINGS:        RenderStateViewRatings();        break;
    case STATE_QUICKMATCH:          RenderStateQuickMatch();         break;
    case STATE_CREATE_MATCH:        RenderStateCreateMatch();        break;
    case STATE_GAME_LOBBY:          RenderStateGameLobby();          break;
    case STATE_GAME_SESSION:        RenderStateGameSession();        break;
    case STATE_GAME_RESULTS:        RenderStateGameResults();        break;
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
    Event ev = GetEvent();

    switch (m_state)
    {
    case STATE_SELECT_ACCOUNT:      UpdateStateSelectAccount( ev );      break;
    case STATE_LOGIN:               UpdateStateLogin( ev );              break;
    case STATE_LOGIN_FAILED:        UpdateStateLoginFailed( ev );        break;
    case STATE_NETWORK_ERROR:       UpdateStateNetworkError( ev );       break;
    case STATE_GAME_SETUP:          UpdateStateGameSetup( ev );          break;
    case STATE_VIEW_RATINGS:        UpdateStateViewRatings( ev );        break;
    case STATE_QUICKMATCH:          UpdateStateQuickMatch( ev );         break;
    case STATE_CREATE_MATCH:        UpdateStateCreateMatch( ev );        break;
    case STATE_GAME_LOBBY:          UpdateStateGameLobby( ev );          break;
    case STATE_GAME_SESSION:        UpdateStateGameSession( ev );        break;
    case STATE_GAME_RESULTS:        UpdateStateGameResults( ev );        break;
        break;
    default:
        assert(0 && "Unknown/illegal state!");
    };

    // If the player is signed in, check the status of the network
    // and report any found network errors
    if ( m_bSignedIn )
    {
        if ( !SUCCEEDED( XOnlineTaskContinue( m_hLogonTask ) ) )
        {
            m_bSignedIn = FALSE;

            SetState( STATE_NETWORK_ERROR );
        }
        else
        {
            m_networkMessageHandler.ProcessMessages( m_rwPlayers );
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
    m_state                          = NUM_STATES;
    m_iItemSelected                  = 0;
    m_dwNumStoredUsers               = 0;
    m_bIsSigningIn                   = FALSE;
    m_bSignedIn                      = FALSE;
    m_iSignInResult                  = (INT)S_OK;
    m_bIsHost                        = FALSE;
    m_bJoinedGame                    = FALSE;
    m_iViewRatingError               = VIEW_RATING_NO_ERR;
    m_dwViewRatingErrorCode          = (DWORD)-1;  
    m_iStatWriteError                = STAT_WRITE_NO_ERR;
    m_dwStatWriteErrorCode           = (DWORD)-1;
    m_bResultsDiffer                 = FALSE;
    m_iWinnerIndex                   = -1;
    m_dwNumUsersEnumerated           = 0;
    m_wLocalPlayerIndex              = 0;

    m_bArbitrationStarted            = FALSE;
    m_fRequestTime                   = -1.0f;
    m_bWaitingToJoin                 = FALSE;

    m_rwSessionList.clear();
    m_rwPlayers.clear();

    // Initialize the network stack
    if( FAILED( XBNet_OnlineInit( 0 ) ) )
        return E_FAIL;

    m_networkMessageHandler.SetAppPtr( this );

    if ( FAILED( m_networkMessageHandler.Initialize() ) )
        return E_FAIL;

    // Create the font
    if( FAILED( m_font.Create( "Font.xpr" ) ) )
        return E_FAIL;


    // Initialize Xbox Live!

    // Wait for any inserted MUs to mount
    while ( XGetDeviceEnumerationStatus() == XDEVICE_ENUMERATION_BUSY );
    
    // Before any of the Xbox online APIs can be used, XOnlineStartup must be 
    // called.  XOnlineStartup automatically calls XNetStartup and 
    // WSAStartup with default parameters in order to initialize the 
    // Xbox Secure Network Libary and the Winsock layer. To specify non-default
    // startup parameters for XNetStartup or WSAStartup, call those functions 
    // prior to calling XOnlineStartup.
    
    HRESULT hrStartup = XOnlineStartup( NULL );

    if ( !SUCCEEDED( hrStartup ) ) return E_FAIL;

    SetState( STATE_SELECT_ACCOUNT );

    m_heartbeatTimer.Start();

    return S_OK;
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