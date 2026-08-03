//-----------------------------------------------------------------------------
// File: ArbitrationManager.cpp
//
// Desc: Illustrates cheat prevention through arbitration on Xbox.
//       This source file contains most of the arbitration specific code.
//       The matchmaking, UI, and networking code is in the other source
//       files, as untouched as possible.
//
//       Based off of the MatchMaking sample
//
// Hist: 12.01.03 - New for December 2003 release 
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "MatchMaking.h"
#include <cassert>
#pragma warning( disable: 4355 )




//-----------------------------------------------------------------------------
// Name: WaitForTaskToComplete
// Desc: Helper function for waiting for a task to complete
//       In a real game, this would be part of your game loop
//       Returns TRUE for success, FALSE for failure.
//       Optionally returns the results code in pHR
//-----------------------------------------------------------------------------
BOOL WaitForTaskToComplete( CXBOnlineTask& Task, HRESULT* pHR )
{
    assert( pHR );
    HRESULT hr = XONLINETASK_S_RUNNING;

    while ( hr == XONLINETASK_S_RUNNING )
    {
        // In a real game, this would be part of your game loop- you wouldn't block on
        // this.
        // The logon task should also be pumped inside this loop.

        hr = Task.Continue();
        if ( FAILED( hr ))
        {
            *pHR = hr;
            return FALSE;
        }
        
        // put in a delay of about 1 frame so as not to spam with MessageSendGetProgress
        Sleep( 15 );
    }
    
    if ( hr != XONLINETASK_S_SUCCESS )
    {
        *pHR = hr;
        return FALSE;
    }

    *pHR = S_OK;
    return TRUE;
}




//-----------------------------------------------------------------------------
// Name: StartArbitratedGameRegistration()
// Desc: Start registering players for an arbitrated game. At this point no more players will be allowed
// to join, and drops will be recorded against players. The arbitration service
// will be used to record game results, in order to discourage cheating.
//-----------------------------------------------------------------------------
VOID CXBoxSample::StartArbitratedGameRegistration()
{
    if( m_Players.size() == 0 )
    {
        SetStatus( L"Can't start arbitrated game with one player" );
        return;
    }
    // At this point we can no longer let people join the game.
    m_ArbitrationStarted = TRUE;
    UpdateSession();

    // The arbitration id is created by the host, and then given to the other players.
    // This number uniquely identifies the game.
    ZeroMemory( &m_ArbID, sizeof( m_ArbID ) );

    // The creation of the session should not necessarily be the host.
    // Instead, the user with the best connection to the other players
    // should be selected.
    // However, to keep this sample as simple as possible, the game creator
    // is used as the host of the arbitrated match.
    assert( m_bIsHost );
    // Only the host creates the round id - the round id is then shared with all
    // players.
    m_ArbID.SessionID = m_HostedSession.SessionID;
    HRESULT hr = XOnlineArbitrationCreateRoundID( &m_ArbID.qwRoundID );
    if( FAILED( hr ) )
    {
        m_UI.SetErrorStr( L"XOnlineArbitrationCreateRoundID failed." );
        Reset( TRUE );
        return;
    }

    // Prior to starting the arbitration process the host should
    // share XNADDRs of all players with all players, give all players the
    // round id, and ask all Xboxes to register with arbitration.
    // The sample just sends the round ID.
    // The clients should all reply once they have registered.

    CXBNetBlob blob;
    blob.blobType = BLOB_ARB_ID;
    blob.dataSize = sizeof(m_ArbID);
    memcpy(blob.data, &m_ArbID, sizeof(m_ArbID) );
    m_GameMsg.SendBlob( m_Players, blob );
    // Indicate that you started the game
    SetStatus( L"Starting registration - waiting for responses" );

    m_RegistrationStart = m_fAppTime;
    m_PlayersRegistered = 0;

    // Wait for players to register, or for a reasonable time-out (5-10 seconds)
    // to expire. If any players failed to register, disconnect them.

    m_State = STATE_REGISTER_WAIT;
    m_dwCurrItem = 0;
}




//-----------------------------------------------------------------------------
// Name: RegisterForArbitration()
// Desc: This function is called by everybody who is joining the game. The
// host calls it last, and sends the results to the other players.
// Returns true for success.
//-----------------------------------------------------------------------------
BOOL CXBoxSample::RegisterForArbitration()
{
    // Register for arbitration. This should be done once per box. All logged on users on that
    // machine will be registered for arbitration.
    CXBOnlineTask ArbitrationHandle;
    // Specify zero or more flags that describe your competition. The FFA flag implies
    // that players are not on teams. All parameters must be the same for all players of
    // an arbitrated game.
    // If the XONLINE_ARB_REGISTER_FLAG_TIME_EXTENDABLE flag is specified then the players
    // can agree to extend the match time using XOnlineArbitrationExtendRound.
    DWORD flags = XONLINE_ARB_REGISTER_FLAG_FFA;
    CONST DWORD MAX_ROUND_SECONDS = 600;
    HRESULT hr = XOnlineArbitrationRegister( &m_ArbID, MAX_ROUND_SECONDS, flags,
                0, &ArbitrationHandle );
    if( FAILED( hr ) )
    {
        m_UI.SetErrorStr( L"XOnlineArbitrationRegister failed with 0x%x", hr );
        Reset( TRUE );
        return FALSE;
    }

    if( !WaitForTaskToComplete( ArbitrationHandle, &hr ) )
    {
        m_UI.SetErrorStr( L"XOnlineArbitrationRegister failed with 0x%x", hr );
        Reset( TRUE );
        return FALSE;
    }

    // Now we can ask to see what other boxes have registered.
    // Allocate a buffer for the registrants and zero it.
    XONLINE_ARB_REGISTRANT RegistrantsBuffer[ MAX_PLAYERS ] = { 0 };

    // You can find out how many machines have registered so far with the specified arbitration ID.
    // You can then iterate through the results in the RegistrantsBuffer, adding up the users on
    // each machine, to get the total number of users.
    DWORD NumRegisteredBoxes = 0;
    hr = XOnlineArbitrationRegisterGetResults( ArbitrationHandle, MAX_PLAYERS, RegistrantsBuffer, &NumRegisteredBoxes );
    if( FAILED( hr ) )
    {
        m_UI.SetErrorStr( L"ArbitrationRegisterGetResults failed with 0x%x", hr );
        Reset( TRUE );
        return FALSE;
    }

    // Let the host know that we registered.
    if( !m_bIsHost )
    {
        CXBNetBlob blob;
        blob.blobType = BLOB_REGISTERED;
        blob.dataSize = 0;  // This blob has no payload.
        m_GameMsg.SendBlob( m_Players, blob );
        // Indicate that you registered
        SetStatus( L"Registered." );
    }
    else
    {
        // If we are the host, then registration must be finished. Therefore
        // we should record all of the XUIDs and disconnect any players that
        // did not register.

        DWORD NumRegisteredPlayers = 0;
        // Zero the XUID array, to mark unused XUIDs.
        XUID playerXUIDs[ MAX_PLAYERS ] = { 0 };
        for( DWORD i = 0; i < NumRegisteredBoxes; ++i )
        {
            // If your game allows multiple players per box then you need to
            // loop over the results from each box, copying each player.
            for ( DWORD player = 0; player < XONLINE_MAX_LOGON_USERS; ++player )
            {
                if( RegistrantsBuffer[ i ].xuidUsers[player].qwUserID )
                {
                    assert( NumRegisteredPlayers < MAX_PLAYERS );
                    playerXUIDs[ NumRegisteredPlayers ] = RegistrantsBuffer[ i ].xuidUsers[ player ];
                    ++NumRegisteredPlayers;
                }
            }
        }

        if( NumRegisteredPlayers < 2 )
        {
            // Arbitrated sessions must have at least two players. If we
            // don't have that many then we can't proceed. This can happen
            // if the other players quit as the registered game is starting,
            // or if their are connection problems that prevent them from
            // joining.
            m_UI.SetErrorStr( L"Only one player registered. Two\n"
                              L"players required." );
            LeaveGame();
            m_NextState = m_State;
            m_State = STATE_ERROR;
            return FALSE;
        }

        // Send the XUIDs to all the registered players.
        CXBNetBlob blob;
        blob.blobType = BLOB_XUIDS;
        blob.dataSize = sizeof( playerXUIDs );
        memcpy( blob.data, playerXUIDs, sizeof( playerXUIDs ) );
        m_GameMsg.SendBlob( m_Players, blob );

        // Copy the XUIDs to the m_PlayerXUIDs array.
        CopyXUIDs( playerXUIDs );
    }

    return TRUE;
}




//-----------------------------------------------------------------------------
// Name: StartArbitratedGame()
// Desc: At this point everybody has registered so we can start the match.
//-----------------------------------------------------------------------------
VOID CXBoxSample::StartArbitratedGame()
{
    if( m_bIsHost )
    {
        CXBNetBlob blob;
        blob.blobType = BLOB_GAME_START;
        blob.dataSize = 0;  // This blob has no payload.
        m_GameMsg.SendBlob( m_Players, blob );
        // Indicate that you started the game
        SetStatus( L"Starting game." );
    }

    m_State = STATE_ARBITRATED_GAME;
    m_dwCurrItem = 0;
    ZeroMemory( m_Scores, sizeof( m_Scores ) );
}




//-----------------------------------------------------------------------------
// Name: CopyXUIDs()
// Desc: Copy the XUIDs (received from the host or from the arbitration service)
// to the m_PlayerXUIDs array, in the same order as the elements in the m_Players
// array (and the same order that scores will go in the m_Scores array).
//-----------------------------------------------------------------------------
VOID CXBoxSample::CopyXUIDs( const XUID* pXUIDs )
{
    BOOL hostFound = FALSE;
    for( DWORD i = 0; i < MAX_PLAYERS; ++i )
    {
        // Stop when we reach unused XUID entries
        if( pXUIDs[ i ].qwUserID == 0 )
            break;

        BOOL copied = FALSE;
        for( DWORD player = 0; player < m_Players.size(); ++player )
        {
            if( m_Players[ player ].qwUserID == pXUIDs[ i ].qwUserID )
            {
                copied = TRUE;
                // Copy the source XUID into the appropriate destination slot.
                m_PlayerXUIDs[ player ] = pXUIDs[ i ];
            }
        }

        // If we didn't find it in m_Players then it must be the local player
        // or the host (because the clients don't get the userID for the host).
        if( !copied )
        {
            if( pXUIDs[ i ].qwUserID == m_qwUserID )
            {
                // It's the local player - copy it to the local player's
                // slot (always the last entry).
                m_PlayerXUIDs[ m_Players.size() ] = pXUIDs[ i ];
            }
            else
            {
                // It must be the host. The host is always in entry zero
                // of m_Players, so we'll copy it there.
                assert( !m_bIsHost );
                assert( !hostFound );
                hostFound = TRUE;
                m_PlayerXUIDs[ 0 ] = pXUIDs[ i ];
            }
        }
    }
}




//-----------------------------------------------------------------------------
// Name: OnBlob()
// Desc: This function receives blobs of data, and handles them appropriately
// based on their blobType.
//-----------------------------------------------------------------------------
VOID CXBoxSample::OnBlob( const CXBNetPlayerInfo& playerInfo, const CXBNetBlob& blob )
{
    switch( blob.blobType )
    {
    case BLOB_ARB_ID:
        // We've received the arbitration ID from the host - an arbitrated game is
        // about to begin. This is our cue to register with the arbitration
        // service, and notify the host when we are done.
        assert( blob.dataSize == sizeof( m_ArbID ) );
        m_ArbID = *(XONLINE_ARB_ID*)blob.data;
        RegisterForArbitration();
        break;

    case BLOB_REGISTERED:
        // This message is received by the host whenever one of the players registers
        // with the arbitration server. If everybody registers then we can stop waiting.
        // We should increment our count of how many players are registered by the number
        // of players on that box - currently I just increment by one, which is adequate
        // if you only allow one player per box.
        // This message is currently broadcast to all players. All but the host should
        // ignore it.
        if( m_bIsHost )
            ++m_PlayersRegistered;
        break;

    case BLOB_XUIDS:
        // This message is sent by the host to all players when the host registers for
        // arbitration. It contains the XUIDs of all of the registered players.
        assert( sizeof( m_PlayerXUIDs ) == blob.dataSize );
        // Copy XUIDs to the appropriate locations.
        CopyXUIDs( (XUID*)blob.data );
        break;

    case BLOB_GAME_START:
        // The host says the game starts, so the game starts.
        StartArbitratedGame();
        break;

    case BLOB_I_SCORE:
    {
        // This message is sent by a player who scores. Everybody needs to record this so
        // that identical data is sent to the arbitration server by all clients.
        WCHAR* gamerTag = (WCHAR *)blob.data;
        // Find out who scored, and update their records.
        for( DWORD i = 0; i < m_Players.size(); ++i )
        {
            if( wcscmp(gamerTag, m_Players[i].strPlayerName ) == 0 )
                m_Scores[ i ] += 1;
        }
        break;
    }

    case BLOB_GAME_OVER:
        // This message is received when the game has been ended. All clients
        // need to make sure that they exchange any final game information,
        // to ensure that the game stat is synchronized. Then they can all
        // submit their results. The results package must be identical!
        SubmitArbitratedResults();
        LeaveGame();
        m_NextState = m_State;
        m_State = STATE_ERROR;
        break;

    default:
        assert( 0 );
    }
}




//-----------------------------------------------------------------------------
// Name: SubmitArbitratedResults()
// Desc: This function submits results using the arbitration service.
//-----------------------------------------------------------------------------
VOID CXBoxSample::SubmitArbitratedResults()
{
    // It is critical that all machines submit identical results.

    // Due to possible packet loss it is important that all machines
    // check with each other to make sure any last minute events are
    // recorded by all. The sample omits this step in the interests
    // of clarity.

    // All machines must submit results in the same order. Therefore,
    // games must sort the results on each machine to guarantee a
    // consistent order. The sample sorts the scores by gamertag.
    DWORD flags = 0;
    if( m_bIsHost )
        flags |= XONLINE_ARB_REPORT_FLAG_WAS_HOST;
    CXBOnlineTask ReportHandle;

    // If there is any suspicious information to report - lost connectivity,
    // or game specific suspicious activity (specified by a text string)
    // this can be specified here.

    // There can be multiple stats per player, but the sample just has one.
    const DWORD STATS_PER_PLAYER = 1;
    XONLINE_STAT stats[ MAX_PLAYERS * STATS_PER_PLAYER ] = { 0 };

    XONLINE_STAT_PROC   results[ MAX_PLAYERS ] = { 0 };
    const DWORD numPlayers = m_Players.size() + 1;
    for( DWORD i = 0; i < numPlayers; ++i )
    {
        results[ i ].wProcedureID = XONLINE_STAT_PROCID_UPDATE_INCREMENT;
        //if( i < m_Players.size() )
        results[ i ].Update.xuid = m_PlayerXUIDs[ i ];
        results[ i ].Update.dwLeaderBoardID = 0;
        results[ i ].Update.dwConditionalIndex = 0; // 0 means always update
        results[ i ].Update.dwNumStats = STATS_PER_PLAYER;
        results[ i ].Update.pStats = stats + i * STATS_PER_PLAYER;

        stats[ i ].wID = XONLINE_STAT_RATING;
        stats[ i ].type = XONLINE_STAT_LONGLONG;
        stats[ i ].llValue = m_Scores[ i ];
    }
    // Now sort the results, to make them consistent on all boxes. It doesn't matter
    // what you sort them by, as long as you are guaranteed a consistent order. The
    // sample sorts them by XUID.qwUserID

    XUID playerXUIDs[ MAX_PLAYERS ];
    assert( sizeof( playerXUIDs ) == sizeof( m_PlayerXUIDs ) );
    // Make a copy before sorting so that the sample's state is not affected by this.
    // This copying isn't strictly necessary, but it seems appropriate.
    memcpy( playerXUIDs, m_PlayerXUIDs, sizeof( m_PlayerXUIDs ) );
    // Do a bubble sort of PlayerXUIDs, by qwUserID. Whenever items are swapped,
    // swap the corresponding items in the results array also.
    for( DWORD outer = 0; outer < numPlayers; ++outer )
    {
        for( DWORD inner = outer; inner < numPlayers - 1; ++inner )
        {
            if( playerXUIDs[ outer ].qwUserID > playerXUIDs[ outer + 1 ].qwUserID )
            {
                std::swap( playerXUIDs[ outer ], playerXUIDs[ outer + 1 ] );
                std::swap( results[ outer ], results[ outer + 1 ] );
                // The stats don't need to be swapped because they are pointed to
                // by the results, so their order is not significant.
            }
        }
    }

    HRESULT hr = XOnlineArbitrationReport( &m_ArbID, STATS_PER_PLAYER * m_Players.size() + 1, results, 0, flags, 0, &ReportHandle );
    if( FAILED( hr ) )
    {
        m_UI.SetErrorStr( L"Problem submitting results - hr = 0x%08x", hr );
        return;
    }

    if( !WaitForTaskToComplete( ReportHandle, &hr ) )
    {
        if( hr == XONLINE_S_ARBITRATION_DIFFERENT_RESULTS_DETECTED )
        {
            // This error message means that the results you just submitted
            // don't match some previously submitted results for this match.
            // This may indicate cheating, but could mean that the results
            // you submitted were not properly synchronized, due to a coding
            // error. Note that the first results submitted will never have
            // this error because there is nothing for them to be different
            // from.

            // In the sample this error will occur if the
            // "Score Point (not sent)" option is selected.
            m_UI.SetErrorStr( L"Different results detected." );
        }
        else
        {
            m_UI.SetErrorStr( L"Problem submitting results - hr = 0x%08lx", hr );
        }
    }
    else
    {
        m_UI.SetErrorStr( L"Results successfully submitted." );
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateStateArbitratedGame()
// Desc: This is the update function used once an arbitrated game is started,
// both during registration and during the game.
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateArbitratedGame( Event ev )
{
    // Once an arbitrated game has been initiated, most of the code
    // is the same, so the same state function is used.

    // Handle net messages
    if( m_GameMsg.ProcessMessages( m_Players ) )
        return;

    // A switch within a switch. Sigh...
    switch( m_State )
    {
    case STATE_REGISTER_WAIT:
        // We're waiting for other players to register. If we're the
        // host then we can start the game once everybody registers
        // or the delay time expires.
        if( m_bIsHost )
        {
            // We compare against m_dwSlotsInUse - 1 because we want to detect
            // when everybody has registered except for the game creator.
            if( m_fAppTime > m_RegistrationStart + MAX_REGISTRATION_TIME || m_PlayersRegistered == m_dwSlotsInUse - 1 )
            {
                // Now the host can register for arbitration.
                if( !RegisterForArbitration() )
                    return;
                StartArbitratedGame();
            }
        }
        break;

    case STATE_ARBITRATED_GAME:
        break;
    }

    // Send keep-alives
    if( m_HeartbeatTimer.GetElapsedSeconds() > PLAYER_HEARTBEAT )
    {
        m_GameMsg.SendHeartbeat( m_Players );
        m_HeartbeatTimer.StartZero();
    }
    
    // Handle other players dropping. If they drop after registering for
    // arbitration then that is much more serious than dropping normally,
    // as it will affect their reliability rating.
    if( m_GameMsg.ProcessPlayerDropouts( m_Players, PLAYER_TIMEOUT ) )
        return;
    
    // Handle session updates
    if( m_bIsHost )
    {
        HRESULT hr = m_HostedSession.Process();
        if( hr != XONLINETASK_S_RUNNING )
        {
            // Handle errors
            if( FAILED(hr) )
            {
                m_UI.SetErrorStr( L"XMatch failed with error %x", hr );
                Reset( TRUE );
                return;
            }
        }
    }
    
    switch( ev )
    {
    case EV_BUTTON_WHITE:
        m_NextState = m_State;
        m_State = STATE_HELP;
        break;
    CXBNetBlob blob;
    case EV_BUTTON_A:
        switch( m_dwCurrItem )
        {
        case ARBITRATED_SCORE:          // Score a point
            // The player on this box is always the last entry
            // in the score table.
            m_Scores[ m_Players.size() ] += 1;
            // Send this score information to all the other players.
            blob.blobType = BLOB_I_SCORE;
            blob.dataSize = sizeof(m_strUser);
            // Copy the entire gamer tag (should optimize and just copy
            // the valid characters).
            memcpy( blob.data, &m_strUser, sizeof(m_strUser) );
            m_GameMsg.SendBlob( m_Players, blob );
            break;

        case ARBITRATED_DROPPED_SCORE:  // Score a point, but don't tell anyone.
            m_Scores[ m_Players.size() ] += 1;
            break;

        case ARBITRATED_GAME_OVER:      // Let's end the game.
            // Any player can end the game. In a real game the exit conditions
            // would be handled differently.
            // When the game is over it is important that all the machines
            // synchronize their view of the game state so that any last minute
            // points are recorded by all.
            blob.blobType = BLOB_GAME_OVER;
            blob.dataSize = 0;  // There is no data associated with this packet.
            // Let the other players know that the game is over.
            m_GameMsg.SendBlob( m_Players, blob );
            // Submit results
            SubmitArbitratedResults();
            LeaveGame();
            m_NextState = m_State;
            m_State = STATE_ERROR;
            break;

        default:         
            assert( FALSE ); break;
        }
        break;
        
    case EV_UP:
        // Move to previous item; allow wrap to bottom
        if( m_dwCurrItem == 0 )
            m_dwCurrItem = ARBITRATED_MAX - 1;
        else
            --m_dwCurrItem;
        break;
        
    case EV_DOWN:
        // Move to next item; allow wrap to top
        if( m_dwCurrItem == ARBITRATED_MAX - 1 )
            m_dwCurrItem = 0;
        else
            ++m_dwCurrItem;
        break;
    }
}
