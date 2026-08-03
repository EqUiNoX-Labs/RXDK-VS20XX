//-------------------------------------------------------------------------------------
// File: IntegratedDemo.cpp
//
// Desc: Sample program that shows how to integrated XboxLive 3.0 features.
//
// Hist: 12.09.04 - New for January release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#include <algorithm>
#include <xtl.h>
#include <xonline.h>
#include <xhv.h>
#include <XBSound.h>
#include "dsound.h"
#include "dsstdfx.h"
#include "XBFont.h"
#include "XBMemUnit.h"
#include "XBVoice.h"

#include "Common.h"
#include "Menus.h"
#include "UserContent.h"
#include "Tourney.h"
#include "GameSession.h"
#include "IntegratedDemo.h"


/////////////
// GLOBALS //
/////////////

static CXHVVoiceManager  g_XHVVoiceManager; // Voice Chat engine

// Competition globals
// These are large structures that we do not want to
// place in the stack. These perform the queries and
// store the results.
static CSECompetition                                   g_competition;
static CSEEntrantsMyCompetitionsQuery                   g_joinedCompQuery;
static CSEEntrantsCurrentEntrantsQuery                  g_entrantsQuery;
static CSECompetitionsAvailableCompetitionsQueryResults g_availableResults;
static CSEEventsTopologyQueryResult                     g_rwTopology[ MAX_ROUND_EVENTS ];


// For use of sending text/voice

// value of buffer for 5 seconds of recorded voice data
//
// NOTE: if you wish to use a different maximum size for voice data, make sure to call the
// XHVGetVoiceMailBufferSize(dwDuration) macro'd function and pass it the maximum
// duration desired, in milliseconds.
const  INT  VOICE_BUFFER_SIZE = 0x18c6;

static BYTE g_rwVoiceMailBuffer[VOICE_BUFFER_SIZE]          = { 0 };
static WCHAR g_rwTeamTextMsgBuffer[MAX_TEAM_MESSAGE_LENGTH] = { 0 };


/////////////////////////////////////
// PendingMessage Member Functions //
/////////////////////////////////////

//-----------------------------------------------------------------------------
// Name: ReadFromSocket
// Desc: Attempts to read a message from the specified socket.  Since our
//          reliable sockets are stream-oriented, messages may be received
//          in small pieces (or many received at once), so it's important
//          to carefully parse the appropriate amount of data from the stream.
//          Returns TRUE if message is completely parsed
//-----------------------------------------------------------------------------
HRESULT PendingMessage::Read( SOCKET sock )
{
    // 1) The first thing we need is to complete the header - that way
    // we know how large the message is.  If we don't have any data, or haven't
    // completed the header, just ask for enough data to complete the header
    if( m_nBytesReceived < Message::GetHeaderSize() )
    {
        CHAR* pbReceive = ( (CHAR *)&m_msg ) + m_nBytesReceived;
        INT   nBytesReq = Message::GetHeaderSize() - m_nBytesReceived;
        INT   nBytes    = recv( sock, pbReceive, nBytesReq, 0 );

        // Check result
        if( nBytes == SOCKET_ERROR )
        {
            if( WSAGetLastError() != WSAEWOULDBLOCK )
                return E_FAIL;
        }
        else
        {
            m_nBytesReceived += nBytes;
        }
    }

    // If we have a complete header, but haven't yet finished parsing the
    // message payload, ask for just enough data to complete the payload
    if( m_nBytesReceived >= Message::GetHeaderSize() &&
        m_nBytesReceived < m_msg.GetSize() )
    {
        CHAR* pbReceive = ( (CHAR *)&m_msg ) + m_nBytesReceived;
        INT   nBytesReq = m_msg.GetSize() - m_nBytesReceived;
        INT   nBytes    = recv( sock, pbReceive, nBytesReq, 0 );

        // Check result
        if( nBytes == SOCKET_ERROR )
        {
            if( WSAGetLastError() != WSAEWOULDBLOCK )
                return E_FAIL;
        }
        else
        {
            m_nBytesReceived += nBytes;
        }
    }

    // Determine if we now have a complete message - note that we still have
    // to verify we have at least the header before asking for the size of the
    // message
    if( m_nBytesReceived >= Message::GetHeaderSize() &&
        m_nBytesReceived == m_msg.GetSize() )
    {
        return S_OK;
    }
    else
    {
        return S_FALSE;
    }
}


//////////////////////////////////
// CXBoxSample Member Functions //
//////////////////////////////////

//-----------------------------------------------------------------------------
// Name: UnregisterKey()
// Desc: Attempt to unregister the current key from use on this Xbox.
//       A single Xbox may have upto 4 keys registered simultaneously.
//-----------------------------------------------------------------------------
BOOL CXBoxSample::UnregisterKey()
{
    if( m_bIsKeyRegistered )
    {
        INT iUnregisterResult = XNetUnregisterKey( &m_xnHostKeyID );

        ZeroMemory( &m_xnHostKeyID,       sizeof( XNKID ) );
        ZeroMemory( &m_xnHostKeyExchange, sizeof( XNKEY ) );

        m_bIsKeyRegistered = FALSE;

        return( iUnregisterResult == NO_ERROR );
    }

    return FALSE;
}

//-----------------------------------------------------------------------------
// Name: RegisterKey()
// Desc: Attempt to register the given key for use on this Xbox.
//       A single Xbox may have upto 4 keys registered simultaneously.
//       Keys are used to keep the network secure and to translate
//       XNADDRs to INADDRs
//-----------------------------------------------------------------------------
BOOL CXBoxSample::RegisterKey( const XNKID *pxnkid, const XNKEY *pxnkey )
{
    // Check to see if the key we are going to register
    // is the same as the one we currently have registered
    // if so, then just keep the current key withou any action.
    if( m_bIsKeyRegistered )
    {
        if( !memcmp( pxnkid, &m_xnHostKeyID, sizeof( XNKID ) ) )
            return TRUE;
    }

    // If we have a key registered then we should
    // unregister it. An Xbox may have up to four
    // keys registered simultaneously
    UnregisterKey();

    // Now attempt to register the new key
    INT iRegisterResult = XNetRegisterKey( pxnkid, pxnkey );
    m_bIsKeyRegistered = ( iRegisterResult == NO_ERROR );

    // If we were able to register save the new key
    // and ID so we can unregister these later
    if( m_bIsKeyRegistered )
    {
        memcpy( &m_xnHostKeyID, pxnkid, sizeof( XNKID ) );
        memcpy( &m_xnHostKeyExchange, pxnkey, sizeof( XNKEY ) );
    }

    return m_bIsKeyRegistered;
}

//-----------------------------------------------------------------------------
// Name: AllLocalUsersAreOnSameTeam()
// Desc: Returns true if all the locally logged on users are on the same
//       team that was selected for the competition round.
//-----------------------------------------------------------------------------
BOOL CXBoxSample::AllLocalUsersAreOnSameTeam()
{
    // get roster of team selected
    BOOL bGotRoster = GetTeamRoster( m_wControllingUser,
                                     m_phTeamRosterTask,
                                     m_rwTeamXUIDS[m_iTeamSelected],
                                     m_rwTeamMembers,
                                     m_dwTeamMemberCount );

    // make sure the roster exists
    assert( bGotRoster );


    // go through all users on all ports
    for( DWORD dwUser = 0; dwUser < XGetPortCount(); ++dwUser )
    {
        // if user is signed in
        if( m_rwLocalUsers[dwUser].m_bSignedIn )
        {
            // get reference to user
            XONLINE_USER& user = m_rwStoredUsers[m_rwLocalUsers[dwUser].m_wUserIndex];
           
            // if user is not on my team, return false
            if( !IsOnMyTeam( user.xuid.qwUserID ) )
            {
                return FALSE;
            }
        }
    }

    return TRUE;
}

//-----------------------------------------------------------------------------
// Name: AllUsersAreOnSameTeam()
// Desc: Returns true if all the locally logged on users AND all
//       the remote users in the game lobby are on the same
//       team that was selected for the competition round.
//-----------------------------------------------------------------------------
BOOL CXBoxSample::AllUsersAreOnSameTeam()
{
    // Check the locally logged on users first
    if( !AllLocalUsersAreOnSameTeam() )
    {
        return FALSE;
    }


    // Check the remote players next
    PlayerList::iterator pCurPlayer = m_rwPlayers.begin();

    while( pCurPlayer != m_rwPlayers.end() )
    {
        if( !IsOnMyTeam( pCurPlayer->xuid.qwUserID ) )
            return FALSE;
    }

    return TRUE;
}

//-----------------------------------------------------------------------------
// Name: GetNumLoggedOnUsers();
// Desc: Returns the number of users logged on locally.
//-----------------------------------------------------------------------------
WORD CXBoxSample::GetNumLoggedOnUsers()
{
    WORD wNumUsers = 0;

    for( INT i = 0; i < XGetPortCount(); ++i )
    {
        wNumUsers += ( m_rwLocalUsers[i].m_bSignedIn ? 1 : 0 );
    }

    return wNumUsers;
}

//-------------------------------------------------------------------------------------
// Name: FindCompetitionSession()
// Desc: Try to find a session for this competition.
//       If a session can not be found, then then we should
//       try to create one
//-------------------------------------------------------------------------------------
HRESULT CXBoxSample::FindCompetitionSession( DWORD dwControllerPort,
                                             ULONGLONG qwCompID,
                                             ULONGLONG qwRoundID,
                                             IN_ADDR& hostAddr )
{
    // Step 1
    //
    // Fill out the search spec, the search attributes
    // and the results spec

    struct MATCH_SEARCH_RESULT
    {
        ULONGLONG qwCompID;
        ULONGLONG qwAttempt;
        ULONGLONG qwHostUserId;
    };

    const INT              NUM_MAX_RESULTS          = 9;
    MATCH_SEARCH_RESULT    results[NUM_MAX_RESULTS] = { 0 };
    XONLINE_ATTRIBUTE      rwAttributeSpecs[1]      = { 0 };
    XONLINE_ATTRIBUTE_SPEC rwResultSpec[3]          = { 0 };

    rwAttributeSpecs[0].dwAttributeID        = MATCH_EVENT_ENTITY_ID;
    rwAttributeSpecs[0].info.integer.qwValue = qwCompID;

    rwResultSpec[0].dwType = X_ATTRIBUTE_DATATYPE_ENTITY_ID ;
    rwResultSpec[1].dwType = X_ATTRIBUTE_DATATYPE_INTEGER;
    rwResultSpec[2].dwType = X_ATTRIBUTE_DATATYPE_INTEGER;


    // Step 2
    //
    // Start the search

    CXBOnlineTask hTask;


    DWORD dwNumResults          = sizeof( results ) / sizeof( results[0] );
    DWORD dwNumResultSpec       = sizeof( rwResultSpec ) / sizeof( rwResultSpec[0] );
    DWORD dwNumSearchAttributes = sizeof( rwAttributeSpecs ) / sizeof( rwAttributeSpecs[0] );

    HRESULT hrQuery = XOnlineQuerySearch(
                            MATCH_DATASET,         // Data Set ID
                            MATCH_FIND_EVENT_HOST, // MATCH_EVENT_ENTITY_ID, // Entity ID
                            0,                     // Page #
                            dwNumResults,          // Maximum number of results
                            dwNumResultSpec,       // Size of the result spec
                            rwResultSpec,          // The array of result spec
                            dwNumSearchAttributes, // number of search attributes
                            rwAttributeSpecs,      // array of search attributes
                            NULL,                  // Work event to be notified
                            &hTask );              // work task

    if( FAILED( hrQuery ) )
    {
        hTask.Close();

        return hrQuery;
    }


    // Step 3
    //
    // pump the task until it is complete or fails

    if( !WaitForTaskToComplete( hTask, &hrQuery, TRUE ) )
    {
        hTask.Close();

        return hrQuery;
    }


    // Step 4
    //
    // Get the results

    DWORD dwTotalResults    = NUM_MAX_RESULTS;
    DWORD dwReturnedResults = 1;
    DWORD dwResultsSize     = sizeof( results );

    DWORD dwMinBufferSize   = XOnlineQueryGetResultsBufferSize(
                                dwTotalResults, // The maximum number of results we can store
                                3,              // Number of items in the result spec
                                rwResultSpec ); // The result spec

    assert( dwMinBufferSize <= dwResultsSize );


    hrQuery = XOnlineQuerySearchGetResults(
                    hTask,              // Task used to start
                    &dwTotalResults,    // number of total results
                    &dwReturnedResults, // number of results returned
                    &dwResultsSize,     // size of the result buffer
                    (PBYTE)results      // result buffer
                );

    if( FAILED( hrQuery ) )
    {
        hTask.Close();

        return hrQuery;
    }


    // Step 5
    //
    // Try to join a host if one was found

    for( DWORD dwResult = 0; dwResult < dwTotalResults; ++dwResult )
    {
        if( results[dwResult].qwAttempt != qwRoundID )
        {
            XBUtil_DebugPrint( "SKIPPED qwRoundID = %0xd\n", results[dwResult].qwAttempt );
            continue;
        }

        CXBOnlineTask hSessionTask;
        XUID          host;

        ZeroMemory(&host, sizeof(host));
        host.qwUserID = results[dwResult].qwHostUserId;

        XBUtil_DebugPrint( "FindCompetitionSession: qwRoundID = %0xd\n", qwRoundID );
        XBUtil_DebugPrint( "FindCompetitionSession: Results found, trying to connect." );

        // If we got disconnected when trying to create this
        // session then it is possible for this to happen
        // so let's try to clean it up!
        if( host.qwUserID == CURRENT_USER.xuid.qwUserID )
        {
            // Remove the entry in the DB.
            // This is important. If the entry is not
            // removed the participating teams may
            // have difliculties setting up future rounds
            // in the competition
            HRESULT hrRemove = RemoveHostEntry( m_wControllingUser,
                                                m_rwTeamXUIDS[m_iTeamSelected].qwTeamID );

            XBUtil_DebugPrint( "Trying to remove a previous session: hrRemove 0x%x\n", 
                               hrRemove );

            return E_FAIL;
        }

        XONLINE_PEER_SESSION_RESULTS session;

        // NOTE: Make sure that you are not
        // trying to get session info for this box!

        HRESULT hrGetSession = E_FAIL;

        // Get the IP address of the current
        // session round.
        hrGetSession = XOnlineGetUserSession(
                            dwControllerPort, // Controller port of the user searching
                            host,             // XUID of the user hosting
                            NULL,             // Event to be triggered when finished
                            &hSessionTask,    // Task to assign
                            &session );       // Results to write to when finished

        if( FAILED( hrGetSession ) )
            return hrGetSession;

        if( !WaitForTaskToComplete( hSessionTask, &hrGetSession, TRUE ) )
        {
            return hrGetSession;
        }


        // Step 6
        //
        // attempt to connect to session.XNADDR,   If connected, done.

        XBUtil_DebugPrint( "Trying to connect to XNADDR\n" );

        BOOL bRegistered = RegisterKey( &session.xkid, &session.xnkey );
        assert( bRegistered );

        INT iResult = XNetXnAddrToInAddr(
                            &session.xnaddr,
                            &session.xkid,
                            &hostAddr
                        );


        iResult = XNetConnect( hostAddr );

        XBUtil_DebugPrint( iResult
                           ? "Failed to connect! %d\n"
                           : "Connected via FindCompetitionSession: %d\n",
                           iResult );

        return( iResult ? E_FAIL : S_OK );
    }


    return E_FAIL;
}

//-------------------------------------------------------------------------------------
// Name: CreateCompetitionSession()
// Desc: Creates a session that others can join so the competition may run.
//-------------------------------------------------------------------------------------
HRESULT CXBoxSample::CreateCompetitionSession( DWORD dwControllingUser,
                                               ULONGLONG qwTeamID,
                                               ULONGLONG qwCompID,
                                               ULONGLONG qwRoundID )
{

    // Step 1
    //
    // Fill out the information that will be used
    // by others to find this match

    XBUtil_DebugPrint( "Starting to create competition seesion\n" );
    XBUtil_DebugPrint( "CreateCompetitionSession: qwRoundID = %0xd\n", qwRoundID );

    const PXONLINE_USER rwLoggedOnUsers   = XOnlineGetLogonUsers();
    XONLINE_ATTRIBUTE   addAttributes[3]  = { 0 };

    addAttributes[0].dwAttributeID        = MATCH_EVENT_ENTITY_ID;
    addAttributes[0].fChanged             = TRUE;
    addAttributes[0].info.integer.qwValue = qwCompID;

    addAttributes[1].dwAttributeID        = MATCH_ATTEMPT;
    addAttributes[1].fChanged             = TRUE;
    addAttributes[1].info.integer.qwValue = qwRoundID;

    addAttributes[2].dwAttributeID        = MATCH_HOST_USER_ID;
    addAttributes[2].fChanged             = TRUE;
    addAttributes[2].info.integer.qwValue = rwLoggedOnUsers[dwControllingUser].xuid.qwUserID;


    // Step 2
    //
    // Tell the server that we are going to host
    // this round of the competition.

    CXBOnlineTask hCreateSessionTask;

    XBUtil_DebugPrint( "Starting to add query\n" );

    DWORD dwNumAttributes = sizeof( addAttributes ) / sizeof( addAttributes[0] );

    HRESULT hrCreate = XOnlineQueryAdd(
                            dwControllingUser,     // User who is creating the session
                            qwTeamID,              // ID of the team creating the session
                            MATCH_DATASET,         // ID of the database to add session to
                            dwNumAttributes,       // Number of attributes to add to the DB
                            addAttributes,         // Array of attributes to add
                            NULL,                  // Event to be triggered when finished
                            &hCreateSessionTask ); // Task to assign

    if( FAILED( hrCreate ) )
    {
        hCreateSessionTask.Close();

        return hrCreate;
    }


    // Step 3
    //
    // Pump the task until complete

    WaitForTaskToComplete( hCreateSessionTask, &hrCreate );

    if( FAILED( hrCreate ) )
    {
        XBUtil_DebugPrint( "QueryAdd failed with error 0x%x\n", hrCreate );

        hCreateSessionTask.Close();

        return hrCreate;
    }


    // Step 4
    //
    // Get the results

    XENTITY_ID entityID;

    HRESULT hrGetResults = XOnlineQueryAddGetResults(
                                hCreateSessionTask, // Task used to add to the DB
                                &entityID );        // The entity ID to assign

    assert( SUCCEEDED( hrGetResults ) );

    XBUtil_DebugPrint( "QueryAdd results : 0x%x%x\n", entityID >> 32, entityID );


    // Step 5
    //
    // Register the key we just created

    XNADDR xnaddr;
    XNKID  xnkid;
    XNKEY  xnkey;

    HRESULT hrRegister = XOnlineGetSession(
                            &xnaddr, // Xbox Address to assign to
                            &xnkid,  // Key ID to assign to
                            &xnkey   // Key to assign to
                         );

    assert( SUCCEEDED( hrRegister ) );

    BOOL bRegistered = RegisterKey( &xnkid, &xnkey );
    assert( bRegistered );


    // Step 6
    //
    // Close the task and return our success

    hCreateSessionTask.Close();

    XBUtil_DebugPrint( "CreateCompetitionSession finished: 0x%x\n", hrRegister );

    return hrRegister;
}

//-----------------------------------------------------------------------------
// Name: GetVoiceLevel
// Desc: Helper function for determining what level of voice functionality
//          a player should have
//-----------------------------------------------------------------------------
CXBoxSample::EVoiceLevel CXBoxSample::GetVoiceLevel( DWORD dwPort )
{
    XONLINE_USER& curXUser = m_rwStoredUsers[ m_rwLocalUsers[ dwPort ].m_wUserIndex ];

    // if user ID is zero
    if( curXUser.xuid.qwUserID == 0 )
    {
        return VOICE_LEVEL_NO_PLAYER;
    }
    // if user is denied voice features
    else if( !XOnlineIsUserVoiceAllowed( curXUser.xuid.dwUserFlags ) )
    {
        return VOICE_LEVEL_NOT_ALLOWED;
    }
    else
    {
        // get status of communicator
        XHV_LOCAL_TALKER_STATUS status;
        g_XHVVoiceManager.GetLocalTalkerStatus( dwPort, &status );
        
        // if inserted, return that everything is on, else return
        // that there is no communicator
        if( status.communicatorStatus == XHV_VOICE_COMMUNICATOR_STATUS_INSERTED )
        {
            return VOICE_LEVEL_EVERYTHING;
        }
        else
        {
            return VOICE_LEVEL_NO_COMMUNICATOR;
        }
    }
}

//-------------------------------------------------------------------------------------
// Name: UpdateTeamMessageToSend()
// Desc: updates global team message buffer with current message index
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateTeamMessageToSend()
{
    // copy indexed string into team message buffer
    lstrcpynW( g_rwTeamTextMsgBuffer,
               PRECONCEIVED_TEAM_MESSAGES[ m_iMessageSelected ],
               MAX_TEAM_MESSAGE_LENGTH );

}

//-------------------------------------------------------------------------------------
// Name: VoiceMailStopped
// Inherited from: ITitleXHV
// Desc: Notification callback indicating that the voice message has stopped
//-------------------------------------------------------------------------------------
HRESULT CXBoxSample::VoiceMailStopped( DWORD dwLocalPort )
{
    assert( m_bXHVInitialized );

    // if communicator is removed in the middle of recording
    // and we already had not gotten a signal of such an event
    if ( !m_bGotCommunicatorRemovalEvent &&
         g_XHVVoiceManager.RecordingNow() &&
         GetVoiceLevel( m_wControllingUser ) != VOICE_LEVEL_EVERYTHING )
    {
        // clear voice mail states
        HRESULT hrVoiceMailOp = g_XHVVoiceManager.ClearVoiceMail();

        m_bVoiceBufferPlayable = FALSE;
        ZeroMemory( g_rwVoiceMailBuffer, sizeof( g_rwVoiceMailBuffer ) );

        // show message stating error.
        if ( FAILED( hrVoiceMailOp ) )
        {
            PushMessageWindow( "Error caused by communicator removal." , TRUE );
        }
        else
        {
            PushMessageWindow( "Cannot continue recording. Communicator removed." , TRUE );
        }

        m_bGotCommunicatorRemovalEvent = TRUE;
    }
    else
    {
        g_XHVVoiceManager.AlertOfVoiceMailStopped();
    }

    return S_OK;
}

//-------------------------------------------------------------------------------------
// Name: VoiceMailDataReady
// Inherited from: ITitleXHV
// Desc: Notification callback indicating that the voice message data is ready
//-------------------------------------------------------------------------------------
HRESULT CXBoxSample::VoiceMailDataReady( DWORD dwLocalPort, DWORD dwDuration,
                                         DWORD dwSize )
{
    assert( m_bXHVInitialized );
    g_XHVVoiceManager.AlertOfVoiceMailDataReady();
    return S_OK;
}

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
    #pragma message("TCR: Title UI must prompt for a passcode and verify it before signing on.")

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
            rwLogonUsers[wUser] = m_rwStoredUsers[m_rwLocalUsers[wUser].m_wUserIndex];
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
// Name: CopyXUIDs()
// Desc: Copy the XUIDs (received from the host or from the arbitration service)
//       to the m_rwPlayerXUIDs array, in the same order as the elements in the
//       m_rwPlayers array (and the same order that scores
//       will go in the m_rwScores array).
//-------------------------------------------------------------------------------------
VOID CXBoxSample::CopyXUIDs( const XUID* pXUIDs )
{
    XBUtil_DebugPrint( "Received XUID package.\n" );

    if( m_bXUIDsCopied )
    {
        assert( 0 && "Reported as XUIDs already being copied!" );
        return;
    }

    for( DWORD i = 0; i < MAX_MATCHERS; ++i )
    {
        // Stop when we reach unused XUID entries
        if( pXUIDs[i].qwUserID == 0 )
            break;

        m_rwPlayerXUIDs[i] = pXUIDs[i];
    }

    m_bXUIDsCopied = TRUE;
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

    if( ( m_dwSlotsInUse == MAX_MATCHERS ) && ( !m_bArbitrationStarted ) )
    {
        // The session used to be full.
        // Turn on Qos listening, so
        // that other consoles can probe us.
        m_hostedSession.Listen( TRUE );
    }

    --m_dwSlotsInUse;
}

//-------------------------------------------------------------------------------------
// Name: SendVoiceMessage
// Desc: Sends a team/team member a voice message from the current user.  Message 
//       contains:
//       * a text message
//       * possibly a voice attachment
//
//       There are three protocols for sending a message:
//
//       VOICE_MAIL_SENT_TO_TEAM_SAVE_SELF - A team members sends a message to all 
//       members of its own team except the sender.  Specify the sender ID as the 
//       special "ID parameter"
//
//       VOICE_MAIL_SENT_TO_TEAM_OWNERS - A team member sends a message to the owners 
//       of another team.  Specify the team ID as the special "ID parameter"
//
//       VOICE_MAIL_SENT_TO_INDIVIDUAL - A team member sends a message to an individual 
//       of another team.  Specify the individual's ID as the special "ID parameter"
//-------------------------------------------------------------------------------------
CXBoxSample::EVoiceMessageSuccess CXBoxSample::SendVoiceMessage(
                                  XUID xuidSendingTeam,
                                  BOOL bHasVoice,
                                  CXBoxSample::EVoiceMessageProtocol eProtocol,
                                  ULONGLONG qwParamID )
{
    // init a message handler
    XONLINE_MSG_HANDLE hMsg = NULL;

    // the total number of properties we'll send in our message is the summation
    // of the number of text properties and the number of voice properties
    const WORD  NUM_TEXT_MESSAGE_PROPERTIES = 2;
    const WORD  NUM_VOICE_MESSAGE_PROPERTIES = 3;

    WORD wNumProperties = NUM_TEXT_MESSAGE_PROPERTIES;
    DWORD dwMessageFlags = XONLINE_MSG_FLAG_HAS_TEXT | XONLINE_MSG_FLAG_TEAM_CONTEXT;

    // if the message has voice, we add more properties and flags
    if ( bHasVoice )
    {
        wNumProperties += NUM_VOICE_MESSAGE_PROPERTIES;
        dwMessageFlags |= XONLINE_MSG_FLAG_HAS_VOICE;
    }


    // Create a text and/or voice message
    HRESULT hrMsgCreate = XOnlineMessageCreate(
                          XONLINE_MSG_TYPE_TITLE_CUSTOM, // custom title message
                          wNumProperties,                // for text, maybe more 
                                                         // for voice
                          0,                             // use default size for buffer
                          xuidSendingTeam.qwTeamID,      // message context (team id)
                          dwMessageFlags,                // flags denoting text, 
                                                         // team IDs, and maybe voice
                          0,                             // use default expiration
                          &hMsg
                          );

    // if we couldn't create the message, return appropriate error
    if( FAILED( hrMsgCreate ) )
    {
        return VOICE_MAIL_ERROR_COULDNT_CREATE_MESSAGE;
    }

    // if message has voice, we need to set pertaining to voice
    if ( bHasVoice )
    {
        const WORD wCodec = XONLINE_PROP_VOICE_DATA_CODEC_WMAVOICE_V90;
        const DWORD dwVoiceDuration = CXHVVoiceManager::MAX_VOICEMAIL_DURATION_MS;

        // property: voice data
        XOnlineMessageSetProperty( hMsg,                        // Message handle
                                   XONLINE_MSG_PROP_VOICE_DATA, // Voice data property
                                   VOICE_BUFFER_SIZE,           // buffer size
                                   g_rwVoiceMailBuffer,         // Value of property
                                   0 );                         // Extra flags

        // property: voice codec
        XOnlineMessageSetProperty( hMsg,                              // Message handle
                                   XONLINE_MSG_PROP_VOICE_DATA_CODEC, // Voice codec (WMA) 
                                   sizeof( WORD ),                    // 2-byte entity
                                   &wCodec,                           // Value of property
                                   0 );                               // Extra flags

        // property: voice message duration
        XOnlineMessageSetProperty( hMsg,                                 // Message handle
                                   XONLINE_MSG_PROP_VOICE_DATA_DURATION, // Voice msg duration
                                   sizeof( DWORD ),                      // 4-byte entity
                                   &dwVoiceDuration,                     // Value of property
                                   0 );                                  // Extra flags
    }

    // set properties pertaining to text
    const DWORD dwLanguage = XC_LANGUAGE_ENGLISH;

    // property: text message length
    XOnlineMessageSetProperty( hMsg,                    // Message handle
                               XONLINE_MSG_PROP_TEXT,   // text
                               MAX_TEAM_MESSAGE_LENGTH, // text length
                               &g_rwTeamTextMsgBuffer,  // text buffer
                               0 );                     // extra flags

    // property: text language type
    XOnlineMessageSetProperty( hMsg,                           // Message handle
                               XONLINE_MSG_PROP_TEXT_LANGUAGE, // text language
                               sizeof(DWORD),                  // size of language variable
                               &dwLanguage,                    // language variable (English)
                               0 );                            // extra flags

    // init team roster
    XUID rwTeamRoster[XONLINE_MAX_TEAM_MEMBER_COUNT] = { 0 };
    DWORD dwTeamRosterSize = 0;

    XUID xuidTeamIDToRoster;
    ZeroMemory( &xuidTeamIDToRoster , sizeof( xuidTeamIDToRoster ) );

    // if we're not sending a message to an individual, we need an ID for the
    // team, so we can get its roster
    if ( eProtocol != VOICE_MAIL_SENT_TO_INDIVIDUAL)
    {
        switch( eProtocol )
        {
        case VOICE_MAIL_SENT_TO_TEAM_SAVE_SELF:
            // user the provided team ID instead
            xuidTeamIDToRoster = xuidSendingTeam;
            break;

        case VOICE_MAIL_SENT_TO_TEAM_OWNERS:
        // set team id to given opposing team parameter
            xuidTeamIDToRoster.qwTeamID = qwParamID;
            break;
        }

        // make sure we got an ID for the team
        assert( xuidTeamIDToRoster.qwTeamID );

        // get roster
        BOOL bGotTeamRoster = GetTeamRoster(
                                m_wControllingUser,
                                m_phTeamRosterTask,
                                xuidTeamIDToRoster,
                                rwTeamRoster,
                                dwTeamRosterSize );

        // make sure we got the roster.
        // if the team roster is blank, then return appropriate error
        if( !bGotTeamRoster )
        {
            // close task
            m_phTeamRosterTask.Close();
            
            // destroy message
            XOnlineMessageDestroy( hMsg );

            return VOICE_MAIL_ERROR_NO_TEAM_EXISTS;
        }

    }

    // make array of XUIDs depending on the protocol..

    DWORD dwMessageRecipientsSize = 0;
    XUID* pMessageRecipients = NULL;

    // create an array of XUIDs depending on the protocol of sending this message
    switch( eProtocol )
    {
    case VOICE_MAIL_SENT_TO_TEAM_SAVE_SELF:
        {
            // we're sending this message to everybody on the team except the sender

            // specify sending member ID
            ULONGLONG qwSelfID = qwParamID;

            // set recipient size to be the size of the roster minus one (self)
            dwMessageRecipientsSize = dwTeamRosterSize - 1;

            // if the team consists ONLY of the sender, don't send the message
            // return the appropriate error
            if ( !dwMessageRecipientsSize )
            {
                // close task
                m_phTeamRosterTask.Close();
                
                // destroy message
                XOnlineMessageDestroy( hMsg );

                return VOICE_MAIL_ERROR_NO_RECIPIENTS;
            }

            // allocate the recipients
            pMessageRecipients = new XUID[ dwMessageRecipientsSize ];

            // make sure we were able to allocate them
            assert( pMessageRecipients && "Not enough memory to allocate temporary\
                                      team roster to send text/voice message" );

            // go through and assign each team member xuid to the recipients list,
            // making sure we skip ourself
            DWORD dwRecipientIndex = 0;
            for ( DWORD i = 0 ; i < dwTeamRosterSize ; ++i )
            {
                if ( rwTeamRoster[i].qwUserID != qwSelfID )
                {
                    pMessageRecipients[ dwRecipientIndex++ ] = rwTeamRoster[ i ];
                }
            }
        }
        break;

    case VOICE_MAIL_SENT_TO_TEAM_OWNERS:
        {
            // this message will be send to the owners of the team

            // allocate an array of bools for each possible team member
            // this will be used to flag those who are owners, and those who are not
            BOOL*   pIsTeamOwner = new BOOL[ dwTeamRosterSize ];

            // check if allocation succeeded
            assert( pIsTeamOwner && "Not enough memory to allocate temporary\
                                  team owner flags to send text/voice message" );

            // init team member info
            XONLINE_TEAM_MEMBER memberInfo;
            ZeroMemory( &memberInfo , sizeof(XONLINE_TEAM_MEMBER) );

            HRESULT hrMemberDetail;
            DWORD i;
            DWORD dwNumOwners = 0;

            // flag the roster as owners or not, while keeping a count of the 
            // owners themselves
            for ( i = 0 ; i < dwTeamRosterSize ; ++i )
            {
                // get team member info
                hrMemberDetail = XOnlineTeamMemberGetDetails(
	                                m_phTeamRosterTask, // The task used to retrieve 
                                                        // the team roster
	                                rwTeamRoster[i],    // The XUID of the member to 
                                                        // retrieve
	                                &memberInfo );      // The structure to populate 
                                                        // with details

                // make sure the call succeeded
                assert( SUCCEEDED( hrMemberDetail ) );

                // if the team member has the ability to delete another member, 
                // he/she is an owner, therefore up the owner counter, 
                // and flag as TRUE, else FALSE
	            if ( memberInfo.TeamMemberProperties.dwPrivileges & 
                     XONLINE_TEAM_DELETE_MEMBER)
                {
                    pIsTeamOwner[i] = TRUE;
                    ++dwNumOwners;
                }
                else
                {
                    pIsTeamOwner[i] = FALSE;
                }
            }

            // close task
            m_phTeamRosterTask.Close();

            // set recipient size to be the number of owners
            dwMessageRecipientsSize = dwNumOwners;

            // allocate XUIDS for owners
            pMessageRecipients = new XUID[ dwMessageRecipientsSize ];

            // make sure allocation succeeded
            assert( pMessageRecipients && "Not enough memory to allocate temporary\
                                      team roster to send text/voice message" );

            // assigned XUIDS if the team member is flagged as an owner
            DWORD dwRecipientIndex = 0;
            for ( i = 0 ; i < dwTeamRosterSize ; ++i )
            {
                if ( pIsTeamOwner[i] )
                {
                    pMessageRecipients[ dwRecipientIndex++ ] = rwTeamRoster[i];
                }
            }

            // clean up
            delete [] pIsTeamOwner;
        }
        break;

    case VOICE_MAIL_SENT_TO_INDIVIDUAL:
        {
            // specify sending member ID
            ULONGLONG qwRecipientID = qwParamID;

            // set recipient size to be one, the individual
            dwMessageRecipientsSize = 1;

            // allocate this one-sized array
            pMessageRecipients = new XUID[ dwMessageRecipientsSize ];

            // make sure allocation succeeded
            assert( pMessageRecipients && "Not enough memory to allocate temporary\
                                    team roster to send text/voice message" );

            // assign XUID to the individual recipient XUID
            ZeroMemory( pMessageRecipients , sizeof(XUID) );
            pMessageRecipients[ 0 ].qwUserID = qwRecipientID;

        }
        break;

    default:
        // check for any unforeseen message sending protocols
        assert( 0 && "Unforeseen message sending protocol" );
        break;
    }

    // make sure we have team members to send messages to
    assert( dwMessageRecipientsSize && "Unforeseen empty recipient list" );

    CXBOnlineTask hMessageSendTask;

    // Send the message to the recipient list
    HRESULT hrSend = XOnlineMessageSend(
                        m_wControllingUser,      // port number
                        hMsg,                    // message handle
                        dwMessageRecipientsSize, // num recipients
                        pMessageRecipients,      // array of recipient XUIDS
                        NULL,                    // Work event
                        &hMessageSendTask        // message sending task
                    );

    // make sure the task succeeded initially
    assert( SUCCEEDED( hrSend ) );

    // block until sending is complete
    WaitForTaskToComplete( hMessageSendTask , &hrSend );

    // make sure the task succeeded
    assert( SUCCEEDED( hrSend ) );

    // clean up

    // close task
    hMessageSendTask.Close();

    // destroy message
    XOnlineMessageDestroy( hMsg );

    // deallocate message recipients list
    delete [] pMessageRecipients;

    // clear message and voice data
    m_bVoiceBufferPlayable = FALSE;
    ZeroMemory( g_rwVoiceMailBuffer , sizeof( g_rwVoiceMailBuffer ) );
    ZeroMemory( g_rwTeamTextMsgBuffer , sizeof( g_rwTeamTextMsgBuffer ) );

    // return success!
    return VOICE_MAIL_SUCCESS;
}

//-------------------------------------------------------------------------------------
// Name: MessageHasVoiceAttachment( XONLINE_MSG_SUMMARY& msgSummary )
// Desc: TRUE if there is a voice attachment in message
//-------------------------------------------------------------------------------------
BOOL CXBoxSample::MessageHasVoiceAttachment( XONLINE_MSG_SUMMARY& msgSummary )
{
    return ( ( msgSummary.dwMessageFlags & XONLINE_MSG_FLAG_HAS_VOICE )? 
               TRUE : FALSE );
}

//-------------------------------------------------------------------------------------
// Name: DownloadTextVoiceMessage( XONLINE_MSG_SUMMARY& msgSummary )
// Desc: Attempts to download text/voice message enumerated by index.  TRUE if success.
//-------------------------------------------------------------------------------------
CXBoxSample::EVoiceMessageDownloadSuccess CXBoxSample::DownloadTextVoiceMessage( 
                                          XONLINE_MSG_SUMMARY& msgSummary )
{
    // check to see if message summary contains a voice message
    BOOL bHasVoice = MessageHasVoiceAttachment( msgSummary );

    // Step 1
    //
    // Start getting the message details

    CXBOnlineTask hMsgDetailsTask;
    HRESULT hrMsgDetails = XOnlineMessageDetails(
                                m_wControllingUser,     // controller port
                                msgSummary.dwMessageID, // unique message ID
                                XONLINE_MSG_FLAG_READ,  // Flags to set (read) 
                                0,                      // Flags to clear
                                NULL,                   // work event
                                &hMsgDetailsTask        // message details task
                                );  

    // if task failed initially
    if( FAILED( hrMsgDetails ) )
    {
        return VOICE_MAIL_DOWNLOAD_ERROR_COULDNT_ACCESS_MESSAGE;
    }

    // block until task completed
    WaitForTaskToComplete( hMsgDetailsTask, &hrMsgDetails );

    // if task failed
    if ( FAILED( hrMsgDetails ) )
    {
        return VOICE_MAIL_DOWNLOAD_ERROR_COULDNT_ACCESS_MESSAGE;
    }

    // Step 2
    //
    // Get the detail results

    DWORD dwMessageSize = 0;
    DWORD dwMessageFlags = 0;

    HRESULT hrDetailResults;
    HRESULT hrAttachmentDL;
    HRESULT hrGetResults;

    // Download required for this type of property!
    hrDetailResults = XOnlineMessageDetailsGetResultsProperty(
                      hMsgDetailsTask,         // Task used to start getting details
                      XONLINE_MSG_PROP_TEXT,   // constant for msg text property
                      MAX_TEAM_MESSAGE_LENGTH, // maximum team message length
                      g_rwTeamTextMsgBuffer,   // buffer for retrieval of message
                      &dwMessageSize,          // gets actual size of message
                      &dwMessageFlags );       // specific flag info about message


    // Step 3
    //
    // If we have voice, get the voice detail results

    if ( bHasVoice )
    {
        // Download required for this type of property!
        hrDetailResults = XOnlineMessageDetailsGetResultsProperty(
                          hMsgDetailsTask,             // task for retrieval
                          XONLINE_MSG_PROP_VOICE_DATA, // using voice data
                          VOICE_BUFFER_SIZE,           // voice buffer size
                          g_rwVoiceMailBuffer,         // voice buffer
                          &dwMessageSize,              // actual retrieved buffer size
                          &dwMessageFlags );           // misc flags about msg

        // if voice properties exist, yet download's not required, 
        // there's something wrong
        if( hrDetailResults != XONLINE_E_MESSAGE_PROPERTY_DOWNLOAD_REQUIRED )
        {
            return VOICE_MAIL_DOWNLOAD_ERROR_VOICE_ATTACHMENT_MISSING;
        }


        // Set 4
        //
        // Get the voice attachment
        CXBOnlineTask hDownloadTask;

        // download attachment
        hrAttachmentDL = XOnlineMessageDownloadAttachmentToMemory(
                    hMsgDetailsTask,             // message details task
                    XONLINE_MSG_PROP_VOICE_DATA, // asking for voice data
                    g_rwVoiceMailBuffer,         // buffer for voice data
                    dwMessageSize,               // size of buffer from previous call
                    NULL,                        // work event (not being used)
                    &hDownloadTask               // download task
                );

        // if task failed initially
        if ( FAILED( hrAttachmentDL ) )
        {
            return VOICE_MAIL_DOWNLOAD_ERROR_DOWNLOAD_FAILED;
        }

        // block until task completed
        WaitForTaskToComplete( hDownloadTask, &hrAttachmentDL );

        // if task failed initially
        if ( FAILED( hrAttachmentDL ) )
        {
            return VOICE_MAIL_DOWNLOAD_ERROR_DOWNLOAD_FAILED;
        }

        BYTE* pVoiceMailBufferRef;
        DWORD dwBytesActuallyReceived;
        DWORD dwTotalDataSize;

        // get results of download for verification purposes
        hrGetResults = XOnlineMessageDownloadAttachmentToMemoryGetResults(
                       XONLINETASK_HANDLE(hDownloadTask), // task handle
                       &pVoiceMailBufferRef,              // reference to voice buffer
                       &dwBytesActuallyReceived,          // bytes received
                       &dwTotalDataSize );                // total data size 
                                                          // (last two params should 
                                                          //  be the same for complete
                                                          //  download)

        // verify results
        assert( SUCCEEDED( hrGetResults ) );
        assert( pVoiceMailBufferRef == (PBYTE)g_rwVoiceMailBuffer );
        assert( dwBytesActuallyReceived == dwTotalDataSize);
        assert( dwBytesActuallyReceived == dwMessageSize);

        // close task
        hDownloadTask.Close();

        // set up XHV voice manager to play back voice mail
        g_XHVVoiceManager.PrepareVoiceMailPlay( 
                          m_wControllingUser , 
                          CXHVVoiceManager::MAX_VOICEMAIL_DURATION_MS ,
                          g_rwVoiceMailBuffer ,
                          dwTotalDataSize );

    }

    return VOICE_MAIL_DOWNLOAD_SUCCESS;
}


///////////////////////
// UI FSM state code //
///////////////////////

//-----------------------------------------------------------------------------
// Name: ExitState()
// Desc: Executes the exit code for the given state.
//-----------------------------------------------------------------------------
VOID CXBoxSample::ExitState( INT iStateExit )
{
    // Execute exit functionality
    switch( iStateExit )
    {
    case STATE_SELECT_ACCOUNT:      ExitStateSelectAccount();       break;
    case STATE_LOGIN:               ExitStateLogin();               break;
    case STATE_LOGIN_FAILED:        ExitStateLoginFailed();         break;
    case STATE_NETWORK_ERROR:       ExitStateNetworkError();        break;
    case STATE_MAIN:                ExitStateMain();                break;
    case STATE_TEAMS_LEADERBOARD:   ExitStateTeamsLeaderboard();    break;
    case STATE_TEAMS:               ExitStateTeams();               break;
    case STATE_RECENT_PLAYERS:      ExitStateRecentPlayers();       break;
    case STATE_SELECT_INVITE_TEAM:  ExitStateSelectInviteTeam();    break;
    case STATE_CREATE_MATCH:        ExitStateCreateMatch();         break;
    case STATE_QUICKMATCH:          ExitStateQuickMatch();          break;
    case STATE_GAME_LOBBY:          ExitStateGameLobby();           break;
    case STATE_GAME_SESSION:        ExitStateGameSession();         break;
    case STATE_INBOX:               ExitStateInbox();               break;
    case STATE_SETTINGS_EDIT:       ExitStateSettingsEdit();        break;
    case STATE_INVITE_DETAILS:      ExitStateInviteDetails();       break;
    case STATE_VIEW_MY_TEAMS:       ExitStateViewMyTeams();         break;
    case STATE_TEAM_SEND_MESSAGE:   ExitStateSendMessage();         break;
    case STATE_TEAM_SHOW_MESSAGE:   ExitStateShowMessage();         break;
    case STATE_TEAM_OPS:            ExitStateTeamOps();             break;
    case STATE_LIST_AVAILABLE_COMPS:ExitStateListAvailableComps();  break;
    case STATE_VIEW_TEAM_ROSTER:    ExitStateViewTeamRoster();      break;
    case STATE_TEAM_MEMBER_OPS:     ExitStateTeamMemberOps();       break;
    case STATE_CONTENT_EDIT:        ExitStateContentEdit();         break;
    case STATE_LIST_TOURNEYS:       ExitStateListTourneys();        break;
    case STATE_TOURNEY_RENDER:      ExitStateTourneyRender();       break;
    case STATE_MESSAGE_WINDOW:      ExitStateMessageWindow();       break;
    case NUM_STATES: break;
    default:
        assert(0 && "Unknown/illegal state!");
        break;
    };
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

    ExitState( m_state );

    BOOL bEnterResult = FALSE;

    // Execute entry functionality
    switch( newState )
    {
    case STATE_SELECT_ACCOUNT:      bEnterResult = EnterStateSelectAccount();      break;
    case STATE_LOGIN:               bEnterResult = EnterStateLogin();              break;
    case STATE_LOGIN_FAILED:        bEnterResult = EnterStateLoginFailed();        break;
    case STATE_NETWORK_ERROR:       bEnterResult = EnterStateNetworkError();       break;
    case STATE_MAIN:                bEnterResult = EnterStateMain();               break;
    case STATE_TEAMS_LEADERBOARD:   bEnterResult = EnterStateTeamsLeaderboard();   break;
    case STATE_TEAMS:               bEnterResult = EnterStateTeams();              break;
    case STATE_RECENT_PLAYERS:      bEnterResult = EnterStateRecentPlayers();      break;
    case STATE_SELECT_INVITE_TEAM:  bEnterResult = EnterStateSelectInviteTeam();   break;
    case STATE_CREATE_MATCH:        bEnterResult = EnterStateCreateMatch();        break;
    case STATE_QUICKMATCH:          bEnterResult = EnterStateQuickMatch();         break;
    case STATE_GAME_LOBBY:          bEnterResult = EnterStateGameLobby();          break;
    case STATE_GAME_SESSION:        bEnterResult = EnterStateGameSession();        break;
    case STATE_INBOX:               bEnterResult = EnterStateInbox();              break;
    case STATE_SETTINGS_EDIT:       bEnterResult = EnterStateSettingsEdit();       break;
    case STATE_INVITE_DETAILS:      bEnterResult = EnterStateInviteDetails();      break;
    case STATE_VIEW_MY_TEAMS:       bEnterResult = EnterStateViewMyTeams();        break;
    case STATE_TEAM_SEND_MESSAGE:   bEnterResult = EnterStateSendMessage();        break;
    case STATE_TEAM_SHOW_MESSAGE:   bEnterResult = EnterStateShowMessage();        break;
    case STATE_TEAM_OPS:            bEnterResult = EnterStateTeamOps();            break;
    case STATE_LIST_AVAILABLE_COMPS:bEnterResult = EnterStateListAvailableComps(); break;
    case STATE_VIEW_TEAM_ROSTER:    bEnterResult = EnterStateViewTeamRoster();     break;
    case STATE_TEAM_MEMBER_OPS:     bEnterResult = EnterStateTeamMemberOps();      break;
    case STATE_CONTENT_EDIT:        bEnterResult = EnterStateContentEdit();        break;
    case STATE_LIST_TOURNEYS:       bEnterResult = EnterStateListTourneys();       break;
    case STATE_TOURNEY_RENDER:      bEnterResult = EnterStateTourneyRender();      break;
    case STATE_MESSAGE_WINDOW:      bEnterResult = EnterStateMessageWindow();      break;
    default:
        assert( 0 && "Unknown/illegal state!" );
    }

    if( !bEnterResult )
    {
        PushMessageWindow( "Unable to complete command" );
        return;
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

    ExitState( m_state );

    m_state = m_stateStack[m_wStateStackSize - 1];

    // Call the initialization code
    // for the state
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
VOID CXBoxSample::PushMessageWindow( const CHAR* strTextMessage , BOOL bReInit )
{
    XBUtil_GetWide( strTextMessage,
                    m_szGameMessage,
                    MAX_MESSAGE_LENGTH );

    m_bReInitAfterWindowMessage = bReInit;

    PushState( STATE_MESSAGE_WINDOW );
}

//-------------------------------------------------------------------------------------
// Name: RenderAccountSelectionView
// Desc: Renders the specified screen (one of the four screens)
//       so upto four users can simultaneously log into Xbox Live
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderAccountSelectionView( EScreenView eViewWindow,
                                              BOOL bControllerInserted )
{
    // Adjust the text position based on
    // which view screen we want
    FLOAT fOffsetX             = 0.0f;
    FLOAT fOffsetY             = 0.0f;
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
        assert( 0 && "Invalid view!" );
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
    FLOAT fDownArrowY        = fAccountListStartY + ( fTextPaddingY * 
                               NUM_ACCOUNTS_PER_WINDOW );

    // If no contoller is inserted
    // then ask the user to insert
    // a controller
    if( !bControllerInserted )
    {
        m_font.SetScaleFactors( 0.75f, 0.75f );
        m_font.DrawText( fViewCenterX, fViewCenterY,
                         COLOR_RED,
                         L"INSERT CONTROLLER",
                         XBFONT_CENTER_X );
        m_font.SetScaleFactors( 1.0f, 1.0f );
    }
    // If the user has selected an account,
    // let them know that they can back out
    // or can continue
    else if( m_bUserSelectedAccount[eViewWindow] )
    {
        WCHAR wszGamertag[XONLINE_GAMERTAG_SIZE] = { 0 };

        XBUtil_GetWide( m_rwStoredUsers[m_rwLocalUsers[eViewWindow].m_wUserIndex].szGamertag,
                        wszGamertag, XONLINE_GAMERTAG_SIZE );

        m_font.DrawText( fViewCenterX,
                         fViewCenterY - DEFAULT_TEXT_PADDING,
                         COLOR_HIGHLIGHT,
                         wszGamertag,
                         XBFONT_CENTER_X );

        m_font.DrawText( fViewCenterX, fViewCenterY,
                         COLOR_RED,
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
        _snprintf( strCntrlIndex , CNTRL_INDEX_STR_SIZE , "Controller #%d" , 
                   ( eViewWindow + 1 ) );
        XBUtil_GetWide( strCntrlIndex, sCntrlIndex, CNTRL_INDEX_STR_SIZE );
        m_font.DrawText( fViewCenterX, fCntrlIndexPosY, COLOR_NORMAL,
                         sCntrlIndex, XBFONT_CENTER_X );

         // get the current selection
        INT iCurSelection = m_rwLocalUsers[eViewWindow].m_iCurSelection;

        // Show list of user accounts
        for( DWORD i = m_rwLocalUsers[eViewWindow].m_dwRenderStart;
             ( i < m_dwNumStoredUsers );
             ++i )
        {
            // highlight the currently selected account name if this is the one
            DWORD dwColor = ( (DWORD)iCurSelection == i ) ?
                                COLOR_HIGHLIGHT : COLOR_NORMAL;

            // Show an account already selected as greyed out
            dwColor = m_rwStoredUserSelected[i] ? COLOR_GREY : dwColor;

            // unless we're at the top, draw the up arrow
            if( m_rwLocalUsers[eViewWindow].m_dwRenderStart > 0 )
            {
                m_font.DrawText( fScrollArrowX, fUpArrowY,
                                COLOR_GREEN, GLYPH_UP_ARROW, XBFONT_CENTER_X );
            }

            // Unless we're at the bottom, draw the down arrow
            if( i >= ( m_rwLocalUsers[eViewWindow].m_dwRenderStart + 
                       NUM_ACCOUNTS_PER_WINDOW ) )
            {
                m_font.DrawText( fScrollArrowX, fDownArrowY,
                                COLOR_GREEN,
                                GLYPH_DOWN_ARROW,
                                XBFONT_CENTER_X );

                return;
            }

            // Convert user name to WCHAR string
            WCHAR strUserName[XONLINE_GAMERTAG_SIZE];
            XBUtil_GetWide( m_rwStoredUsers[i].szGamertag, strUserName,
                            XONLINE_GAMERTAG_SIZE );

            INT iScreenPos = i - m_rwLocalUsers[eViewWindow].m_dwRenderStart;

            // draw the account name
            m_font.DrawText( fViewCenterX,
                             fAccountListStartY + ( fTextPaddingY * iScreenPos ),
                             dwColor,
                             strUserName,
                             XBFONT_CENTER_X );

            // if this is the currently selected item, draw the pointer
            if( i == (DWORD)iCurSelection )
            {
                // Show selected item with little triangle
                FLOAT fTextOffset   = ( m_font.GetTextWidth( strUserName ) / 2.0f );
                FLOAT fTextPos      = fViewCenterX - ( fTextOffset + 
                                      m_font.GetTextWidth( GLYPH_RIGHT_TICK ) );

                m_font.DrawText( fTextPos, fAccountListStartY +
                                ( fTextPaddingY * iScreenPos ),
                                COLOR_POINTER,
                                GLYPH_RIGHT_TICK, XBFONT_CENTER_X );
            }
        }

        // Return the font to the proper size
        m_font.SetScaleFactors(1.0f, 1.0f);
    }
}


/////////////////////////
//   Progress Methods  //
/////////////////////////

//-------------------------------------------------------------------------------------
// Name: SetProgressTask
// Desc: initializes progress task and states thereof; and creates a progress
// message showing how much progress has occurred in an upload/download task
// (note: this should be in a printf style format to allow display of integer
// percentage number i.e. "Upload content progress so far: %u")
// also, message string should be no longer than MAX_MESSAGE_LENGTH - 1
//-------------------------------------------------------------------------------------
VOID    CXBoxSample::SetProgressTask( DWORD dwProgressActivity ,
                                      const CHAR* szMessageFormat )
{
    // turn progress bar on and assign task to progress task
    m_dwProgressActivity = dwProgressActivity;
    m_bProgressSucceeded = FALSE;
    m_dwProgressPercentage = 0;

    // initialize progress format string
    XBUtil_GetWide( szMessageFormat, m_wszProgressMessageFormat, MAX_MESSAGE_LENGTH );
}
//-------------------------------------------------------------------------------------
// Name: UpdateProgressForTask
// Desc: attempts to get progress for current progress task.  If attempt fails,
//       FALSE is returned, else TRUE
//-------------------------------------------------------------------------------------
BOOL    CXBoxSample::UpdateProgressForTask( CXBOnlineTask& task )
{
    // Get information of progress of upload or download task
    ULONGLONG      qwProgressNum, qwProgressDem;
    DWORD          dwNewProgressPercentage;
    HRESULT hrProgress = XOnlineStorageGetProgress(
                        (XONLINETASK_HANDLE)(task), // progress task
                        &dwNewProgressPercentage,   // gets percentage
                        &qwProgressNum,             // gets numerator (not used now)
                        &qwProgressDem );           // gets denominator (not used now)

    // just leave, if this call failed, leave stored previous percentage alone
    if ( FAILED( hrProgress ) )
    {
        return FALSE;
    }

    // assign new percentage
    m_dwProgressPercentage = dwNewProgressPercentage;

    return TRUE;

}

//-------------------------------------------------------------------------------------
// Name: RenderProgressWindow
// Desc: displays a screen with progress message
//-------------------------------------------------------------------------------------
VOID    CXBoxSample::RenderProgressWindow()
{
    // print out the value of the progress in our message string, using the format
    // string
    _snwprintf( m_wszProgressMessage , MAX_MESSAGE_LENGTH ,
                m_wszProgressMessageFormat , (INT)m_dwProgressPercentage );

    // draw the message in the center
    m_font.DrawText( SCREEN_CENTER_X , SCREEN_CENTER_Y, COLOR_NORMAL ,
                     m_wszProgressMessage, XBFONT_CENTER_X );

    // draw footer
    RenderFooter( m_font, FOOTER_RENDER_SELECT | FOOTER_RENDER_CANCEL );
}

//-------------------------------------------------------------------------------------
// Name: ClearProgressTask
// Desc: deinitializes progress task and states thereof
//-------------------------------------------------------------------------------------
VOID CXBoxSample::ClearProgressTask()
{
    // deinitialize progress variables
    m_dwProgressActivity = (DWORD)PROGRESS_ACTIVITY_NONE;
    m_bProgressSucceeded = FALSE;

    ZeroMemory( m_wszProgressMessageFormat , sizeof( m_wszProgressMessageFormat ) );
    ZeroMemory( m_wszProgressMessage , sizeof( m_wszProgressMessage ) );

}


///////////////////////////
// State Member Function //
///////////////////////////

/////////////////////////
// State SelectAccount //
/////////////////////////

//-------------------------------------------------------------------------------------
// Name: EnterStateSelectAccount
// Desc: Executes setup code for STATE_SELECT_ACCOUNT
//       Finds the Xbox Live user accounts on the memory units and hard-drive
//-------------------------------------------------------------------------------------
BOOL CXBoxSample::EnterStateSelectAccount()
{
    m_wNumUsersSelectedAccounts = 0;
    m_continueTextColor         = COLOR_GREEN;

    m_flashTimer.Stop();
    ZeroMemory( m_bUserSelectedAccount, sizeof( m_bUserSelectedAccount ) );
    ZeroMemory( m_rwStoredUserSelected, sizeof( m_rwStoredUserSelected ) );

    // Clear out the information for each controller port
    for( WORD wUser = 0; wUser < XGetPortCount(); ++wUser)
    {
        m_rwLocalUsers[wUser].m_bSignedIn     = FALSE;
        m_rwLocalUsers[wUser].m_iCurSelection = 0;
        m_rwLocalUsers[wUser].m_wUserIndex    = 0;
        m_rwLocalUsers[wUser].m_dwRenderStart = 0;
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
    HRESULT hrGetUsers = XOnlineGetUsers( m_rwStoredUsers,       // Array to store user info
                                          &m_dwNumStoredUsers ); // Number of accounts stored

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

    return TRUE;
}

//-------------------------------------------------------------------------------------
// Name: UpdateStateSelectAccount
// Desc: Allows the user to scroll through all accounts stored on the Xbox
//       and to select the account that they wish to logon with.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateSelectAccount( DWORD dwControllerPort, Event event )
{
    assert( dwControllerPort >= 0 );
    assert( dwControllerPort < MAX_USERS );

    // Check to see if the controller is inserted
    if( g_Gamepads[dwControllerPort].hDevice == NULL )
    {
        // Clear the user info for the corresponding user
        if( m_bUserSelectedAccount[dwControllerPort] )
        {
            m_rwStoredUserSelected[m_rwLocalUsers[dwControllerPort].m_wUserIndex] = FALSE;
            m_bUserSelectedAccount[dwControllerPort]                              = FALSE;
            m_rwLocalUsers[dwControllerPort].m_iCurSelection                      = 0;
            m_rwLocalUsers[dwControllerPort].m_dwRenderStart                      = 0;

            --m_wNumUsersSelectedAccounts;
        }

        return;
    }

    // Get the menu position and rendering data for the account and controller
    m_rwLocalUsers[dwControllerPort].m_iCurSelection = GetMenuPosition(
                       m_rwLocalUsers[dwControllerPort].m_iCurSelection,
                       event,
                       m_rwLocalUsers[dwControllerPort].m_dwRenderStart,
                       m_dwNumStoredUsers,
                       NUM_ACCOUNTS_PER_WINDOW );

    switch( event )
    {
    case EV_BUTTON_A:
        // If the user has selected an account
        // then the may continue everybody on
        // to the next screen
        if( m_bUserSelectedAccount[dwControllerPort] )
        {
            assert( m_wNumUsersSelectedAccounts > 0 );

            PushState( STATE_LOGIN );
        }
        else
        {
            // Do not allow the user to select an account already
            // selected by another user
            if( m_rwStoredUserSelected[m_rwLocalUsers[dwControllerPort].m_iCurSelection] ) 
            {
                return;
            }

            m_bUserSelectedAccount[dwControllerPort] = TRUE;

            ++m_wNumUsersSelectedAccounts;

            m_rwLocalUsers[dwControllerPort].m_wUserIndex = 
                (WORD)m_rwLocalUsers[dwControllerPort].m_iCurSelection;

            m_rwStoredUserSelected[m_rwLocalUsers[dwControllerPort].m_wUserIndex] = TRUE;
        }
        break;

    case EV_BUTTON_B:
        // The user wants to choose another
        // account or not log in at all
        if( !m_wNumUsersSelectedAccounts )
        {
            m_bUsersSignedIn = FALSE;

            PushState( STATE_MAIN );
        }

        if( !m_bUserSelectedAccount[dwControllerPort] )
            return;

        m_rwStoredUserSelected[m_rwLocalUsers[dwControllerPort].m_wUserIndex] = FALSE;

        m_bUserSelectedAccount[dwControllerPort] = FALSE;
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
    // Render each of the four views
    for( WORD wViewToRender = VIEW_UPPER_LEFT;
          wViewToRender < MAX_VIEWS;
          ++wViewToRender )
    {
        RenderAccountSelectionView( (EScreenView)wViewToRender,
                                    ( g_Gamepads[wViewToRender].hDevice != NULL ) );
    }


    // Create the flashing effect

    DWORD dwMsgColor = ( m_wNumUsersSelectedAccounts > 0 ) ? COLOR_GREEN : COLOR_RED;

    // Flash the word continue
    if( !m_flashTimer.IsRunning() )
    {
        m_flashTimer.StartZero();
    }
    else
    {
        if( m_flashTimer.GetElapsedSeconds() > 0.5f )
        {
            m_continueTextColor = 
                ( m_continueTextColor == COLOR_BLUE ) ? dwMsgColor : COLOR_BLUE;

            m_flashTimer.StartZero();
        }
    }

    // At least one person needs to signon per box
    if( m_wNumUsersSelectedAccounts > 0 )
    {
        m_font.DrawText( SCREEN_CENTER_X, SCREEN_CENTER_Y, m_continueTextColor,
                         GLYPH_A_BUTTON L"CONTINUE",
                         XBFONT_CENTER_X );
    }
    else
    {
        m_font.DrawText( SCREEN_CENTER_X, SCREEN_CENTER_Y, m_continueTextColor,
                         GLYPH_B_BUTTON L"SKIP LOGON",
                         XBFONT_CENTER_X );
    }

    // The user can not really back out of this screen
    RenderFooter( m_font, FOOTER_RENDER_SELECT );
}


/////////////////
// State Login //
/////////////////

//-------------------------------------------------------------------------------------
// Name: EnterStateLogin()
// Desc: Initialises data needed to login to the Xbox Live service
//-------------------------------------------------------------------------------------
BOOL CXBoxSample::EnterStateLogin()
{
    HRESULT hr        = StartSignIn();

    m_iItemSelected   = 0;
    m_bUsersSigningIn = SUCCEEDED( hr );
    m_bUsersSignedIn  = FALSE;

    for( WORD wUser = 0; wUser < MAX_USERS; ++ wUser)
    {
        m_rwLocalUsers[wUser].m_bSignedIn = FALSE;
    }

    return SUCCEEDED( hr );
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
                        m_rwLocalUsers[wUser].m_bSignedIn    = m_bUserSelectedAccount[wUser];

                        m_bUsersSigningIn = FALSE;
                    }

                    m_bUsersSignedIn = TRUE;


                    // Set the presence data
                    // Find a controller port
                    // being used to logon
                    // and find out how many
                    // users are trying to logon

                    DWORD dwNumLoggedOnUsers       = 0;
                    DWORD dwControllerPort         = 0;
                    XUID  rwXUIDS[XGetPortCount()] = { 0 };

                    for( INT i = 0; i < XGetPortCount(); ++i )
                    {
                        if( m_rwLocalUsers[i].m_bSignedIn )
                        {
                            rwXUIDS[dwNumLoggedOnUsers] = 
                                m_rwStoredUsers[m_rwLocalUsers[i].m_wUserIndex].xuid;

                            ++dwNumLoggedOnUsers;

                            dwControllerPort = i;
                        }
                    }

                    // Set our presence to make
                    // match making easier
                    SetPresence( dwControllerPort,
                                 dwNumLoggedOnUsers,
                                 rwXUIDS );

                    // make sure we're deinitialized
                    assert( !m_bXHVInitialized );
                    InitXHV();

                    PopState();
                    PushState( STATE_MAIN );
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

    RenderFooter( m_font, FOOTER_RENDER_NONE );
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
// State Main //
/////////////////////

//-------------------------------------------------------------------------------------
// Name: UpdateStateMain
// Desc: Allows the user to choose to view the Team Leaderboard or Team Managment
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateMain( DWORD dwControllerPort, Event event )
{
    m_iItemSelected = GetMenuPosition( m_iItemSelected, NUM_ITEMS_MAIN_MENU, event);

    switch( event )
    {
    case EV_BUTTON_A:
        assert( m_iItemSelected >= 0 );
        assert( m_iItemSelected < NUM_ITEMS_MAIN_MENU );

        m_wControllingUser = (WORD)dwControllerPort;

        switch( m_iItemSelected )
        {
        case MENU_MAIN_CREATE_MATCH:
            if( !m_bUsersSignedIn )
            {
                PushMessageWindow( "Please logon to Xbox Live" );
                break;
            }

            PushState( STATE_CREATE_MATCH );
            break;

        case MENU_MAIN_QUICKMATCH:
            if( !m_bUsersSignedIn )
            {
                PushMessageWindow( "Please logon to Xbox Live" );
                break;
            }

            PushState( STATE_QUICKMATCH );
            break;

        case MENU_MAIN_INBOX:
            if( !m_bUsersSignedIn )
            {
                PushMessageWindow( "Please logon to Xbox Live" );
                break;
            }

            PushState( STATE_INBOX );
            break;

        case MENU_MAIN_USER_SETTINGS:
            if( !m_bUsersSignedIn )
            {
                PushMessageWindow( "Please logon to Xbox Live" );
                break;
            }

            PushState( STATE_SETTINGS_EDIT );
            break;

        case MENU_MAIN_TEAMS_LEADERBOARD:  // View the leaderboard of all teams
            if( !m_bUsersSignedIn )
            {
                PushMessageWindow( "Please logon to Xbox Live" );
                break;
            }

            if( GetTeamLeaderboard( m_rwLeaderboardUsers,
                                    m_rwLeaderboardStats,
                                    m_dwNumLeaderboardUsers ) )
            {
                PushState( STATE_TEAMS_LEADERBOARD );
            }
            else
            {
                PushMessageWindow( "Unable to retrieve leaderboard." );
            }

            break;

        case MENU_MAIN_TEAMS:        // Goto the teams functions
            if( !m_bUsersSignedIn )
            {
                PushMessageWindow( "Please logon to Xbox Live" );
                break;
            }

            PushState( STATE_TEAMS );
            break;
        }
        break;

    case EV_BUTTON_B: // Back out and login with a different account

        if ( m_bUsersSignedIn )
        {
            // deinitiatize XHV Voice manager

            assert( m_bXHVInitialized );
            DeInitXHV();
        }

        PopState( TRUE );
        break;
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateMain
// Desc: Shows the menu of options the player has.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateMain()
{
    RenderMenu( m_font, L"GAME SETUP",
                (const WCHAR**)MENU_MAIN, NUM_ITEMS_MAIN_MENU,
                m_iItemSelected );

    // Bottom Help text
    RenderFooter( m_font, FOOTER_RENDER_SELECT | FOOTER_RENDER_CANCEL );
}

////////////////////////
// State SettingsEdit //
////////////////////////

//-------------------------------------------------------------------------------------
// Name: EnterStateSettingsEdit()
// Desc: Intializes the settings editing system
//-------------------------------------------------------------------------------------
BOOL CXBoxSample::EnterStateSettingsEdit()
{
    // Reset the menu cursor
    m_iItemSelected = 0;

    // get current user's user ID
    ULONGLONG qwCurUserID = CURRENT_USER.xuid.qwUserID;

    // call initialize of downloading settings
    HRESULT hrDownload = m_userSettings.EnterDownload(
                         m_hUserSettingsTask ,                 // download task
                         qwCurUserID ,                         // user index
                         m_wControllingUser ,                  // controller index
                         m_pSettingsReceiveBuffer ,            // download buffer
                         sizeof( m_pSettingsReceiveBuffer ) ); // d/l buffer suze

    // if the download failed
    if ( FAILED( hrDownload ) )
    {
        // reset editable settings to default values
        m_editableUserSettings.SetToDefaults();

        // if the download was due to not finding anything (because every user
        // that hasn't created settings before will get the "Storage File Not Found
        // Error"), then proceed to the Edit Settings screen.  Otherwise, display
        // an error beforehand
        if ( hrDownload != XONLINE_E_STORAGE_FILE_NOT_FOUND )
        {
            PushMessageWindow( "Unable to download previous settings" );
        }
    }
    else
    {
        // initialize progress screen for a download activity
        SetProgressTask( PROGRESS_ACTIVITY_DOWNLOAD ,
                         "Download User Settings : %u%% complete" );
    }

    return TRUE;
}

//-------------------------------------------------------------------------------------
// Name: UpdateStateSettingsEdit
// Desc: Allows the user to edit and save settings
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateSettingsEdit( DWORD dwControllerPort, Event event )
{
    if( dwControllerPort != m_wControllingUser )
        return;

    HRESULT hrSettingsTaskResult;

    switch ( m_dwProgressActivity )
    {
    case PROGRESS_ACTIVITY_NONE:
        // no activity currently running worth polling its progress
        break;

    case PROGRESS_ACTIVITY_DOWNLOAD:
        // update user settings task, whether is downloading or uploading
        hrSettingsTaskResult = m_userSettings.UpdateDownload( m_hUserSettingsTask );

        // update progress bar
        UpdateProgressForTask( m_hUserSettingsTask );

        // if download/upload task is done
        if ( hrSettingsTaskResult != XONLINETASK_S_RUNNING )
        {
            // make sure it succeeded
            if ( FAILED( hrSettingsTaskResult ) )
            {
                // set editable settings to default parameters
                m_editableUserSettings.SetToDefaults();

                // clear progress bar
                ClearProgressTask();

                // close download task
                m_hUserSettingsTask.Close();

                // set user settings to dirty
                m_editableUserSettings.SetDirty( TRUE );

                // if download failed because nothing was found, that's fine.
                // Proceed as usual.  If there was another reason, display an error
                if( hrSettingsTaskResult != XONLINE_E_STORAGE_FILE_NOT_FOUND )
                    PushMessageWindow( "Unable to download previous settings" );

            }
            else
            {
                // get current user's user ID
                ULONGLONG qwCurUserID = CURRENT_USER.xuid.qwUserID;

                // make sure progress bar is in sync
                assert( ProgressCompleted() );

                // clear progress bar
                ClearProgressTask();

                // if we succeed in getting the results
                if ( m_userSettings.ExitDownload( m_hUserSettingsTask ,     // d/l task
                                                  qwCurUserID ,             // user ID
                                                  m_pSettingsReceiveBuffer  // buffer
                                                  ) )
                {
                    // copy over the results to the editable settings
                    m_editableUserSettings = m_userSettings;
                }
                else
                {
                    // reset editable settings to default parameters
                    m_editableUserSettings.SetToDefaults();

                    // display an error, since there was a mismatch in the results
                    PushMessageWindow( "Error downloading results from settings" );
               }
            }
        }
        break;

    case PROGRESS_ACTIVITY_UPLOAD:
        // update user settings task, whether is downloading or uploading
        hrSettingsTaskResult = m_userSettings.UpdateUpload( m_hUserSettingsTask );

        // update progress bar
        UpdateProgressForTask( m_hUserSettingsTask );

        // if download/upload task is done
        if ( hrSettingsTaskResult != XONLINETASK_S_RUNNING )
        {
            // make sure it succeeded
            if ( FAILED( hrSettingsTaskResult ) )
            {
                // clear progress bar
                ClearProgressTask();

                // close download task
                m_hUserSettingsTask.Close();

                // display error
                PushMessageWindow( "Unable to upload settings" );

            }
            else
            {
                // make sure progress bar is in sync
                assert( ProgressCompleted() );

                // clear progress bar
                ClearProgressTask();

                // if we succeed in getting the results, deinitialize upload parameters
                m_userSettings.ExitUpload( m_hUserSettingsTask );
            }
        }
        break;

    }

    // Move the cursor / turtle around
    switch( event )
    {
    case EV_UP:
        // decrement item/setting index
        --m_iItemSelected;
        m_iItemSelected = ( m_iItemSelected < 0 ) ?
                          ( NUM_USER_SETTING_INDICES - 1 ) : m_iItemSelected;
        break;

    case EV_DOWN:
        // increment item/setting index
        ++m_iItemSelected;
        m_iItemSelected = ( m_iItemSelected >= NUM_USER_SETTING_INDICES ) ?
                            0 : m_iItemSelected;
        break;

    case EV_BUTTON_A:
        // increment item/setting value
        m_editableUserSettings.IncrementValue( (WORD)m_iItemSelected );
        break;

    case EV_BUTTON_B:
        // decrement item/setting value
        m_editableUserSettings.DecrementValue( (WORD)m_iItemSelected );
        break;

    case EV_BUTTON_START:
        {
            // do saving only if the editable settings are dirty
            if ( m_editableUserSettings.IsDirty() )
            {
                // get current user's user ID
                ULONGLONG qwCurUserID = CURRENT_USER.xuid.qwUserID;

                // set editable settings to false before copying them to
                // settings to be uploaded
                m_editableUserSettings.SetDirty( FALSE );
                m_userSettings = m_editableUserSettings;

                // initialize uploading
                BOOL bEnterUploadSucceeded = m_userSettings.EnterUpload(
                                             m_hUserSettingsTask , // upload task
                                             qwCurUserID ,         // user ID
                                             m_wControllingUser ); // controller idx

                // if initialize did not succeed
                if ( !bEnterUploadSucceeded )
                {
                    // display error
                    PushMessageWindow( "Unable to download previous settings" );
                }
                else
                {
                    // initialize progress screen for an upload activity
                    SetProgressTask( PROGRESS_ACTIVITY_UPLOAD ,
                                     "Upload User Settings : %u%% complete" );
                }

                return;
             }

            // otherwise, chastise user for trying to save changes that weren't made
            PushMessageWindow( "No changes were made to current settings" );
            return;
        }
        break;

    case EV_BUTTON_BACK:
        PopState( TRUE );

        // if changes were made, warn user that changes will be lost
        if( m_editableUserSettings.IsDirty() )
        {
            PushMessageWindow( "Throwing out changes." );
        }

        break;

    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateSettingsEdit()
// Desc: Renders the user settings to the screen and shows the user
//       their input options.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateSettingsEdit()
{
    RenderControllingUser();

    // if we're in the middle of a progress activity, render the progress
    // screen instead of displaying settings
    if ( m_dwProgressActivity != PROGRESS_ACTIVITY_NONE )
    {
        RenderProgressWindow();
    }
    else
    {
        // Display screen caption
        RenderMenu( m_font, L"EDIT SETTINGS", NULL, 0, 0 );

        // define constants for drawing the settings table
        const FLOAT SETTINGS_START_Y   = POS_SCREEN_TITLE_Y + 65.0f;
        const FLOAT SETTINGS_PAD_Y     = 40.0f;

        const FLOAT CHANGE_VALUE_X     = POS_HEADER_LEFT + 60.0f;
        const FLOAT CHANGE_SETTING_X   = POS_HEADER_RIGHT - 60.0f;
        const FLOAT SETTINGS_POINTER_X = 65.0f;
        const FLOAT SETTINGS_INDEX_X   = 220.0f;
        const FLOAT SETTINGS_VALUE_X   = 450.0f;

        // *** Draw the settings table
        for( WORD i = 0; i < NUM_USER_SETTING_INDICES; ++i )
        {
            // Highlight the selected item
            DWORD dwIndexColor = ( m_iItemSelected == i ) ? COLOR_HIGHLIGHT :
                                    COLOR_NORMAL;

            // Highlight the selected item's value in red, signalling that it can
            // change value
            DWORD dwValueColor = ( m_iItemSelected == i ) ? COLOR_RED :
                                    COLOR_NORMAL;

            // draw the name of the setting
            m_font.DrawText( SETTINGS_INDEX_X,
                             SETTINGS_START_Y + ( SETTINGS_PAD_Y * i ),
                             dwIndexColor, USER_SETTING_STR_INDEX[i], XBFONT_CENTER_X );

            // construct the string that displays the setting's value
            WCHAR strImageValue[ MAX_SETTINGS_IMAGE_VALUE_SIZE ] = { 0 };
            m_editableUserSettings.PutWideValueImage( strImageValue , i );

            // draw the value
            m_font.DrawText( SETTINGS_VALUE_X,
                            SETTINGS_START_Y + ( SETTINGS_PAD_Y * i ),
                            dwValueColor, strImageValue , XBFONT_CENTER_X );

        }

        // Show selected item with a little triangle
        m_font.DrawText( SETTINGS_POINTER_X,
                        SETTINGS_START_Y + ( SETTINGS_PAD_Y * m_iItemSelected ),
                        COLOR_POINTER, GLYPH_RIGHT_TICK, XBFONT_LEFT );

        // *** Draw the instructions in the footer

        // define constant for instructions
        FLOAT fFooterStartY = POS_FOOTER_Y - DEFAULT_TEXT_PADDING;

        // Tell the user how to change values
        m_font.DrawText( CHANGE_VALUE_X, fFooterStartY,
                         COLOR_NORMAL,
                         GLYPH_A_BUTTON GLYPH_B_BUTTON ,
                         XBFONT_LEFT );

        m_font.DrawText( POS_HEADER_LEFT, POS_FOOTER_Y,
                         COLOR_NORMAL,
                         L"CHANGE VALUE +/-",
                         XBFONT_LEFT );

        // Tell the user how to move
        m_font.DrawText( CHANGE_SETTING_X, fFooterStartY,
                         COLOR_NORMAL,
                         GLYPH_UP_ARROW GLYPH_DOWN_ARROW ,
                         XBFONT_RIGHT );

        m_font.DrawText( POS_HEADER_RIGHT, POS_FOOTER_Y,
                         COLOR_NORMAL,
                         L"CHOOSE SETTING",
                         XBFONT_RIGHT );

        // How to save
        m_font.DrawText( SCREEN_CENTER_X, fFooterStartY,
                         COLOR_NORMAL,
                         GLYPH_START1_BUTTON GLYPH_START2_BUTTON,
                         XBFONT_RIGHT );

        m_font.DrawText( SCREEN_CENTER_X, POS_FOOTER_Y,
                         COLOR_NORMAL,
                         L"SAVE ",
                         XBFONT_RIGHT );

        // How to exit
        m_font.DrawText( SCREEN_CENTER_X, fFooterStartY,
                         COLOR_NORMAL,
                         GLYPH_BACK1_BUTTON GLYPH_BACK2_BUTTON,
                         XBFONT_LEFT );

        m_font.DrawText( SCREEN_CENTER_X, POS_FOOTER_Y,
                         COLOR_NORMAL,
                         L" EXIT",
                         XBFONT_LEFT );
    }
}

////////////////////////////
// State TeamsLeaderboard //
////////////////////////////

//-------------------------------------------------------------------------------------
// Name: UpdateStateTeamsLeaderboard
// Desc: Waits for the user to dismiss the UI screen and returns to the previous menu
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateTeamsLeaderboard( DWORD dwControllerPort, Event event )
{
    if( dwControllerPort != m_wControllingUser )
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

    RenderMenu( m_font, L"TEAMS LEADERBOARD", NULL, 0, 0 );

    // Draw the header that labels the table data
    m_font.DrawText( POS_TEAM_X,    POS_LEADER_HEADER_Y, COLOR_NORMAL, L"Team", 
                     XBFONT_LEFT );

    m_font.DrawText( POS_KILLS_X,   POS_LEADER_HEADER_Y, COLOR_NORMAL, L"Kills", 
                     XBFONT_LEFT );

    m_font.DrawText( POS_DEATHS_X,  POS_LEADER_HEADER_Y, COLOR_NORMAL, L"Deaths", 
                     XBFONT_LEFT );

    m_font.DrawText( POS_ASSISTS_X, POS_LEADER_HEADER_Y, COLOR_NORMAL, L"Assists", 
                     XBFONT_LEFT );

    m_font.DrawText( POS_RATING_X,  POS_LEADER_HEADER_Y, COLOR_NORMAL, L"Rating",
                     XBFONT_LEFT );

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
        wsprintfW( strText, L"%lu) ", 
                   m_rwLeaderboardStats[iTeamStatStart + STAT_RANK].lValue );
        m_font.DrawText( POS_TEAM_X, fPosDataY, COLOR_NORMAL, strText, XBFONT_RIGHT );

        // Team Name
        m_font.DrawText( POS_TEAM_X, fPosDataY, COLOR_NORMAL, 
                         m_rwLeaderboardUsers[i].wszTeamName );

        // Kills
        wsprintfW( strText, L"%lu", 
                   m_rwLeaderboardStats[iTeamStatStart + STAT_KILLS].lValue );
        m_font.DrawText( POS_KILLS_X, fPosDataY, COLOR_NORMAL, strText, XBFONT_LEFT );

        // Deaths
        wsprintfW( strText, L"%lu", 
                   m_rwLeaderboardStats[iTeamStatStart + STAT_DEATHS].lValue );
        m_font.DrawText( POS_DEATHS_X, fPosDataY, COLOR_NORMAL, strText, XBFONT_LEFT );

        // Assists
        wsprintfW( strText, L"%lu", 
                   m_rwLeaderboardStats[iTeamStatStart + STAT_ASSISTS].lValue );
        m_font.DrawText( POS_ASSISTS_X, fPosDataY, COLOR_NORMAL, strText, XBFONT_LEFT );

        // Rating
        wsprintfW( strText, L"%lu", 
                   m_rwLeaderboardStats[iTeamStatStart + STAT_RATING].llValue );
        m_font.DrawText( POS_RATING_X, fPosDataY, COLOR_NORMAL, strText, XBFONT_LEFT );
    }

    // Tell the user they have the option to switch views
    RenderFooter( m_font, FOOTER_RENDER_CANCEL );
}

/////////////////
// State Teams //
/////////////////

//-------------------------------------------------------------------------------------
// Name: UpdateStateTeams()
// Desc: Updates the Team menu and launches any sub menu
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateTeams( DWORD dwControllerPort, Event event)
{
    // Only allow the controlling user to
    // do work with the team
    if( dwControllerPort == m_wControllingUser)
    {
        m_iItemSelected = GetMenuPosition( m_iItemSelected, NUM_ITEMS_TEAMS_MENU, event);

        switch( event )
        {
            case EV_BUTTON_A:
                assert( m_iItemSelected >= 0 );
                assert( m_iItemSelected < NUM_ITEMS_TEAMS_MENU );

                switch( m_iItemSelected )
                {
                case MENU_TEAMS_RECENT_PLAYERS:
                    PushState( STATE_RECENT_PLAYERS );
                    break;

                case MENU_TEAMS_VIEW_MY_TEAMS: //View My Teams
                    if( GetTeamList( dwControllerPort, CURRENT_USER.xuid,
                                     m_ppTeamLogoTextures,
                                     m_dwTeamLogoToDL, m_rwTeamXUIDS,
                                     m_rwTeamInfo, m_dwTeamCount ) )
                        PushState( STATE_VIEW_MY_TEAMS );
                    else
                        PushMessageWindow( "Unable to retrieve your teams list." );

                    break;

                case MENU_TEAMS_CREATE_TEAM: // Create A Team
                    // Attempt to create a team
                    // If successfull, show the updated team list
                    // Is a failure happens, then try to give some
                    // detailed information
                    switch( CreateTeam( dwControllerPort, m_createdTeamProps ) )
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
                        if( GetTeamList( dwControllerPort, CURRENT_USER.xuid,
                                         m_ppTeamLogoTextures,
                                         m_dwTeamLogoToDL, m_rwTeamXUIDS,
                                         m_rwTeamInfo, m_dwTeamCount ) )
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

    RenderMenu( m_font, L"TEAMS",
                (const WCHAR**)MENU_TEAMS, NUM_ITEMS_TEAMS_MENU, m_iItemSelected );

    // Bottom Help text
    RenderFooter( m_font, FOOTER_RENDER_SELECT | FOOTER_RENDER_CANCEL );
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
BOOL CXBoxSample::EnterStateRecentPlayers()
{
    // Get the list of teams that we are a member of
    m_bTeamListRetrieved  = FALSE;

    m_dwPlayerSelected    = 0;
    m_dwPlayerRenderStart = 0;

    return TRUE;
}

//-------------------------------------------------------------------------------------
// Name: UpdateStateRecentPlayers()
// Desc: Takes the user input to scroll through the list of players
//       the user can send team invites to
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateRecentPlayers( DWORD dwControllerPort, Event event )
{
    if( dwControllerPort != m_wControllingUser )
        return;

    // Get the list of teams that we are a member of
    if( ! m_bTeamListRetrieved )
    {
        m_bTeamListRetrieved = GetTeamList( dwControllerPort,
                                            CURRENT_USER.xuid,
                                            m_ppTeamLogoTextures,
                                            m_dwTeamLogoToDL,
                                            m_rwTeamXUIDS,
                                            m_rwTeamInfo,
                                            m_dwTeamCount );
    }

    assert( m_bTeamListRetrieved );

    m_dwPlayerSelected = GetMenuPosition(
                                m_dwPlayerSelected,
                                event,
                                m_dwPlayerRenderStart,
                                m_dwNumStoredUsers,
                                NUM_ENTRIES_PER_SCREEN
                            );

    switch( event )
    {
    case EV_BUTTON_A: // Select the team to send an invite from
        if ( m_rwLocalUsers[m_wControllingUser].m_wUserIndex != m_dwPlayerSelected )
        {
            PushState( STATE_SELECT_INVITE_TEAM );
        }

        break;

    case EV_BUTTON_B:
        PopState();
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

    RenderMenu( m_font, L"RECENT PLAYERS", NULL, 0, 0 );

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
        FLOAT fIconPosY = fPlayerListStartY + 
                          ( DEFAULT_TEXT_PADDING * ( m_dwPlayerSelected - 
                            m_dwPlayerRenderStart ) );

        m_font.DrawText( fPlayerListStartX, fIconPosY,
                         COLOR_POINTER, GLYPH_RIGHT_TICK, XBFONT_RIGHT );


        WCHAR szwGamerTag[XONLINE_GAMERTAG_SIZE];
        XBUtil_GetWide( m_rwStoredUsers[i].szGamertag,
                        szwGamerTag,
                        XONLINE_GAMERTAG_SIZE );

        // Show that we can not select ourselves
        DWORD dwColor = ( m_rwLocalUsers[m_wControllingUser].m_wUserIndex == i )
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
    RenderFooter( m_font, FOOTER_RENDER_SELECT | FOOTER_RENDER_CANCEL );
}


////////////////////////////
// State SelectInviteTeam //
////////////////////////////

//-------------------------------------------------------------------------------------
// Name: UpdateStateSelectInviteTeam()
// Desc: Lets the player scroll through the choices of teams to invite another
//       player to join. Calls the code to send the invitation.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateSelectInviteTeam( DWORD dwControllerPort, Event event )
{
    if ( dwControllerPort != m_wControllingUser )
        return;

    m_iTeamSelected = GetMenuPosition( m_iTeamSelected, m_dwTeamCount, event );

    switch( event )
    {
    case EV_BUTTON_A: // The user has choosen a team

        switch( SendTeamInvite( dwControllerPort,
                                CURRENT_USER.xuid,
                                m_rwTeamInfo[m_iTeamSelected].xuidTeam,
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

        case XONLINE_E_TEAMS_TEAM_FULL:
            PushMessageWindow( "This team is full. Unable to send invite." );
            break;

        case XONLINE_E_TEAMS_USER_TEAMS_FULL:
            PushMessageWindow( "User can not join anymore teams. Unable to send invite." );
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

    RenderMenu( m_font, L"TEAMS TO INVITE PLAYER TO", NULL, 0, 0 );

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
    if( m_iTeamSelected < (INT)m_dwTeamCount )
    {
        // Show selected item with a little triangle
        FLOAT fIconPosY = fTeamListStartY + ( DEFAULT_TEXT_PADDING * m_iTeamSelected );

        m_font.DrawText( fTeamNameStartX, fIconPosY,
                         COLOR_POINTER, GLYPH_RIGHT_TICK, XBFONT_RIGHT );
    }

    // Bottom Help text
    RenderFooter( m_font, FOOTER_RENDER_SELECT | FOOTER_RENDER_CANCEL );
}


///////////////////////
// State CreateMatch //
///////////////////////

//-------------------------------------------------------------------------------------
// Name: EnterStateCreateMatch()
// Desc: Starts the creation of a match and announces it to the network.
//-------------------------------------------------------------------------------------
BOOL CXBoxSample::EnterStateCreateMatch()
{
    // Intialize the network sockets
    HRESULT hrInit = InitXNet();

    if( FAILED( hrInit ) )
    {
        XBUtil_DebugPrint( "InitXNet failed (error 0x%x)\n", hrInit );

        return FALSE;
    }


    // Create a matchmaking session that will
    // be used by Quickmatch users to find us.
    HRESULT hrCreate = CreateSession( m_rwLocalUsers,
                                      m_dwSlotsInUse,
                                      m_hostedSession,
                                      m_sessionInfo,
                                      CURRENT_USER.szGamertag );

    if( FAILED( hrCreate ) )
    {
        XBUtil_DebugPrint( "CreateSession failed (error 0x%x)\n", hrCreate );

        return FALSE;
    }


    memcpy( &m_xnHostKeyID,
            &m_hostedSession.SessionID,
            sizeof( m_hostedSession.SessionID ) );
    memcpy( &m_xnHostKeyExchange,
            &m_hostedSession.KeyExchangeKey,
            sizeof( m_hostedSession.KeyExchangeKey ) );


    // Start listening for client connections

    m_bIsKeyRegistered = TRUE;

    m_bArbitrationStarted = FALSE;
    m_dwPlayersRegistered = 0;

    m_rwPlayers.clear();

    XBUtil_DebugPrint( "SessionCreate: 0x%x\n", hrCreate );

    // Session failed to start
    return( SUCCEEDED( hrCreate ) );
}

//-------------------------------------------------------------------------------------
// Name: UpdateStateCreateMatch
// Desc: Continues the match creation and allows the host the chance
//       to cancel the action.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateCreateMatch( DWORD dwControllerPort, Event event )
{
    // Wait for matchmaking server to save new session info
    HRESULT hrProcess = m_hostedSession.Process();

    if( hrProcess == XONLINETASK_S_RUNNING )
        return;

    // Handle errors
    if( FAILED( hrProcess ) )
    {
        PopState();
        PushMessageWindow( "Unable to create match" );
        return;
    }


    // We are now the host of a new game
    m_bIsHost = TRUE;

    WCHAR strUserName[XONLINE_GAMERTAG_SIZE];

    XBUtil_GetWide( CURRENT_USER.szGamertag,
                    strUserName,
                    XONLINE_GAMERTAG_SIZE );


    // Note that the session remains "active" (m_hMatchTask isn't
    // closed), and must be pumped in order for the session to
    // remain active on the matchmaking server.
    m_HeartbeatTimer.StartZero();

    PopState();

    PushState( STATE_GAME_LOBBY );
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


//////////////////////
// State QuickMatch //
//////////////////////

//-------------------------------------------------------------------------------------
// Name: EnterStateQuickMatch()
// Desc: Sets up and starts a search for a match of any type for this game.
//-------------------------------------------------------------------------------------
BOOL CXBoxSample::EnterStateQuickMatch()
{
    m_matchQuery.Clear();

    m_bXUIDsCopied = FALSE;

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

    assert( SUCCEEDED( hrQuery ) );

    // Render a screen that shows
    // work is being done while
    // we are finding a match.
    do
    {
        RenderWorkingScreen();

        hrQuery = m_matchQuery.Process();
    }
    while( hrQuery == XONLINETASK_S_RUNNING );

    m_GameSearchTimer.StartZero();

    // Handle errors
    if( FAILED( hrQuery ) )
        return FALSE;

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
        HRESULT hrInit = InitXNet();

        if( FAILED( hrInit ) )
            return FALSE;

        // Request join approval from the game and await a response
        memcpy( &m_xnHostKeyID,
                &m_matchQuery.Results[0].SessionID,
                sizeof( m_xnHostKeyID ) );
        memcpy( &m_xnHostKeyExchange,
                &m_matchQuery.Results[0].KeyExchangeKey,
                sizeof( m_xnHostKeyExchange ) );

        m_xnTitleAddress = m_matchQuery.Results[0].HostAddress;

        BOOL bRegistered = RegisterKey( &m_xnHostKeyID,
                                        &m_xnHostKeyExchange );
        assert( bRegistered );


        // Get a usable address for the host
        INT iResult = XNetXnAddrToInAddr( &m_xnTitleAddress,
                                          &m_xnHostKeyID,
                                          &m_inHostAddr );
        assert( iResult == NO_ERROR );


        SOCKADDR_IN sa;
        sa.sin_family = AF_INET;
        sa.sin_addr   = m_inHostAddr;
        sa.sin_port   = htons( DIRECT_PORT );

        m_GameJoinTimer.StartZero();

        SendJoinGame( sa );
    }

    return TRUE;
}

//-------------------------------------------------------------------------------------
// Name: UpdateStateQuickMatch()
// Desc: Continues the search for a game and allows the user to cancel the search.
//       If a game is found then it is joined and the player is sent to the
//       game lobby.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateQuickMatch( DWORD dwControllerPort, Event event )
{
    if( m_GameSearchTimer.GetElapsedSeconds() > QUICKMATCH_SEARCH_TIME )
    {
        // An error hit so no matches were found
        PopState();
        PushMessageWindow( "Unable to find a match" );
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateQuickMatch()
// Desc: Shows the user that a match is being searched for and that they
//       have the option to cancel the search.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateQuickMatch()
{
    m_font.DrawText( SCREEN_CENTER_X, POS_MESSAGE_Y,
                     COLOR_NORMAL,
                     L"Searching For Match...",
                     XBFONT_CENTER_X );
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


/////////////////////
// State GameLobby //
/////////////////////

//-------------------------------------------------------------------------------------
// Name: UpdateStateGameLobby()
// Desc: Allows the users to interact before the game starts. This is the
//       collection point for all users. Users can leave the game if they wish.
//       The host (ONLY THE HOST) can start the game when all users are ready. When
//       the match is started the host will send a network message to all peers
//       requesting that they register with the arbitration service. Once all peers are
//       registered then the game may start.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateGameLobby( DWORD dwControllerPort, Event event )
{
    // Send keep-alives
    if( ( m_HeartbeatTimer.GetElapsedSeconds() > PLAYER_HEARTBEAT )
        || ( !m_HeartbeatTimer.IsRunning() ) )
    {
        SendHeartbeatToAll();
        m_HeartbeatTimer.StartZero();
    }

    // Handle other players dropping
    if( ProcessPlayerDropouts() )
    {
        if( m_bIsHost && ( !m_bTourneySession ) )
        {
            UpdateSession( m_hostedSession,
                           m_dwSlotsInUse,
                           m_bArbitrationStarted );
        }

        return;
    }

    // Handle session updates
    if( m_bIsHost )
    {
        HRESULT hrProcess = m_hostedSession.Process();

        if( hrProcess != XONLINETASK_S_RUNNING )
        {
            // Handle errors
            if( FAILED( hrProcess ) )
            {
                PopState();
                return;
            }
        }
    }

    // Do not process any input while waiting for
    // arbitration registration to finish
    if( m_bWaitingToJoin )
    {
        if( m_regTimer.GetElapsedSeconds() > ARBITRATION_REGISTRATION_TIME )
        {
            StartArbitratedGame();
        }

        return;
    }

    switch( event )
    {
    case EV_BUTTON_B:
        // User wants to leave the lobby
        if ( m_bIsHost )
        {
            if( m_bTourneySession )
            {
                // Remove the entry in the DB.
                // This is important. If the entry is not
                // removed the participating teams may
                // have difliculties setting up future rounds
                // in the competition
                HRESULT hrRemove = RemoveHostEntry( m_wControllingUser,
                                                    m_rwTeamXUIDS[m_iTeamSelected].qwTeamID );

                if( FAILED( hrRemove ) )
                {
                    XBUtil_DebugPrint( "Failed to remove query session: 0x%x\n", hrRemove );
                }

            }
            else
            {
                // If this is a free-for-all
                // then remove the entry from
                // the match-making service
                DeleteSession( m_hostedSession,
                               m_rwPlayers );
            }
        }

        m_rwPlayers.clear();

        m_bIsHost = FALSE;
        PopState();

        return;

    case EV_BUTTON_A: // The user selected
        if( !m_bIsHost )
        {
            return;
        }
        else
        {
            BOOL bCanStartSession = ( !m_bTourneySession ) &&  m_rwPlayers.size();

            // Do not allow the round to start unless
            // at least one player from another team
            // is in the lobby
            if( m_bTourneySession )
                bCanStartSession = ( !AllUsersAreOnSameTeam() );

            // Tell the clients to register with arbitration.
            // Each client will signal the host when finished.
            // When all clients have registers OR registration
            // time has expired, the host will register
            // with the arbitration service and start the round.
            if ( bCanStartSession )
            {
                m_bWaitingToJoin = SendStartArbitrationRegistration();
            }
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
    RenderMenu( m_font, L"GAME LOBBY", NULL, 0, 0 );


    // Tell the user that we are working
    // and about the start the game
    if ( m_bWaitingToJoin )
    {
        m_font.DrawText( SCREEN_CENTER_X, POS_MESSAGE_Y, COLOR_RED,
                         L"Starting match...", XBFONT_CENTER_X );
        return;
    }

    float fPlayerListStartY = POS_SCREEN_TITLE_Y + ( DEFAULT_TEXT_PADDING * 2 );

    const FLOAT REMOTE_USER_POS_X = SCREEN_SIZE_X * 0.75f;
    const FLOAT LOCAL_USER_POS_X  = SCREEN_SIZE_X * 0.25f;

    // Render the list of remote players
    for( UINT i = 0; i < m_rwPlayers.size(); ++i)
    {
        WCHAR wszGamertag[16] = { 0 };
        _snwprintf( wszGamertag, 16, L"%S", m_rwPlayers[i].strGamertag );
        m_font.DrawText(
                REMOTE_USER_POS_X, fPlayerListStartY + ( i * DEFAULT_TEXT_PADDING ),
                COLOR_GREEN,
                wszGamertag,
                XBFONT_RIGHT
            );
    }


    // Render the list of local players

    WORD wScreenPos = 0;

    for( UINT i = 0; i < XGetPortCount(); ++i)
    {
        if( !m_rwLocalUsers[i].m_bSignedIn )
            continue;

        WCHAR wszGamertag[XONLINE_GAMERTAG_SIZE] = { 0 };
        XBUtil_GetWide(
            m_rwStoredUsers[m_rwLocalUsers[i].m_wUserIndex].szGamertag,
            wszGamertag,
            XONLINE_GAMERTAG_SIZE );

        m_font.DrawText(
                LOCAL_USER_POS_X, fPlayerListStartY + ( wScreenPos * DEFAULT_TEXT_PADDING ),
                COLOR_GREEN,
                wszGamertag,
                XBFONT_LEFT
            );

        ++wScreenPos;
    }

    BOOL bCanStartSession = m_bIsHost && ( !m_bTourneySession ) &&  m_rwPlayers.size();

    if( m_bTourneySession )
        bCanStartSession = m_bIsHost && ( !AllUsersAreOnSameTeam() );

    if( bCanStartSession )
    {
        m_font.DrawText( POS_FOOTER_RIGHT, POS_FOOTER_Y,
                         COLOR_NORMAL,
                         GLYPH_A_BUTTON L" Start Match",
                         XBFONT_RIGHT );
    }

    RenderFooter( m_font, FOOTER_RENDER_CANCEL );
}

///////////////////////
// State GameSession //
///////////////////////

//-----------------------------------------------------------------------------
// Name: UpdateStateGameSession()
// Desc: Sends keep-alive signals to all peers and processes scoring
//       and drop-out events.
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateGameSession( DWORD dwControllerPort, Event event )
{
    // Send keep-alives
    if( m_HeartbeatTimer.GetElapsedSeconds() > PLAYER_HEARTBEAT )
    {
        SendHeartbeatToAll();
        m_HeartbeatTimer.StartZero();
    }


    // Check to see if any players have not
    // sent their heartbeat/keep-alive.
    if( ProcessPlayerDropouts() )
        return;


    switch( event )
    {
    case EV_BUTTON_A:
        // Send a score message to the host
        SendScore( dwControllerPort );
        break;

    case EV_BUTTON_B:
        if( m_bIsHost )
        {
            // Send the game over message from the host
            // to all the clients - This should trigger
            // the clients to submit their arbitration
            // reports
            SendGameOver();

            if ( m_bTourneySession )
            {
                // Remove the entry in the DB.
                // This is important. If the entry is not
                // removed the participating teams may
                // have difliculties setting up future rounds
                // in the competition
                HRESULT hrRemove = RemoveHostEntry( m_wControllingUser,
                                                    m_rwTeamXUIDS[m_iTeamSelected].qwTeamID );

                assert( SUCCEEDED( hrRemove ) );
            }
        }

        PopState();

        // Submit our own results
        SubmitArbitrationResults();

        break;
    }
}

//-----------------------------------------------------------------------------
// Name: RenderStateGameSession()
// Desc: Renders the name and scores of the players in the session.
//       Gives the users instructions.
//-----------------------------------------------------------------------------
VOID CXBoxSample::RenderStateGameSession()
{
    // Header
    RenderMenu( m_font, L"GAME SESSION", NULL, 0, 0 );

    float fPlayerListStartY = POS_SCREEN_TITLE_Y + ( DEFAULT_TEXT_PADDING * 2 );

    const FLOAT REMOTE_USER_POS_X = SCREEN_SIZE_X * 0.75f;
    const FLOAT LOCAL_USER_POS_X  = SCREEN_SIZE_X * 0.25f;

    const SCORE_STRING_SIZE = XONLINE_GAMERTAG_SIZE + 32;


    // Show the remote players first
    if( m_bXUIDsCopied || m_bIsHost )
    {
        INT j = 0;

        for( PlayerList::iterator itor = m_rwPlayers.begin();
            itor != m_rwPlayers.end(); ++itor, ++j )
        {
            INT iPlayerScore = GetPlayerScore( itor->xuid.qwUserID );

            WCHAR wszScoreString[ SCORE_STRING_SIZE ] = { 0 };

            _snwprintf( wszScoreString,
                        SCORE_STRING_SIZE,
                        L"%S-%d",
                        itor->strGamertag,
                        iPlayerScore );

            m_font.DrawText(
                    REMOTE_USER_POS_X, fPlayerListStartY + ( j * DEFAULT_TEXT_PADDING ),
                    COLOR_GREEN,
                    wszScoreString,
                    XBFONT_RIGHT
                );
        }
    }


    // Show the local players

    WORD wScreenPos = 0;

    for( UINT i = 0; i < XGetPortCount(); ++i)
    {
        if( !m_rwLocalUsers[i].m_bSignedIn )
            continue;

        INT iPlayerScore = 
            GetPlayerScore( m_rwStoredUsers[m_rwLocalUsers[i].m_wUserIndex].xuid.qwUserID );

        WCHAR wszScoreString[ SCORE_STRING_SIZE ] = { 0 };

        _snwprintf( wszScoreString,
                    SCORE_STRING_SIZE,
                    L"%S - %d",
                    m_rwStoredUsers[m_rwLocalUsers[i].m_wUserIndex].szGamertag,
                    iPlayerScore );

        m_font.DrawText(
                LOCAL_USER_POS_X, fPlayerListStartY + ( wScreenPos * DEFAULT_TEXT_PADDING ),
                COLOR_GREEN,
                wszScoreString,
                XBFONT_LEFT
            );

        ++wScreenPos;
    }

    // Give the quick/end game options
    if( m_bIsHost  )
    {
        m_font.DrawText( POS_FOOTER_LEFT, POS_FOOTER_Y,
                         COLOR_NORMAL,
                         GLYPH_B_BUTTON L" End Match",
                         XBFONT_LEFT );
    }
    else
    {
        m_font.DrawText( POS_FOOTER_LEFT, POS_FOOTER_Y,
                         COLOR_NORMAL,
                         GLYPH_B_BUTTON L" Withdrawl from Match",
                         XBFONT_LEFT );
    }

    m_font.DrawText( POS_FOOTER_RIGHT, POS_FOOTER_Y,
                     COLOR_NORMAL,
                     GLYPH_A_BUTTON L" Score Point!",
                     XBFONT_RIGHT );
}

//-----------------------------------------------------------------------------
// Name: ExitStateGameSession()
// Desc: Exits the game session. If the machine is the host a message is
//       send to all clients that gives the instruction to submit arbitration
//       results. If the machine is the host, deletes the session from
//       the match-making service.
//-----------------------------------------------------------------------------
VOID CXBoxSample::ExitStateGameSession()
{
    if ( m_bIsHost )
    {
        // Delete the session from the match-making service
        DeleteSession( m_hostedSession,
                       m_rwPlayers );

        // Tell all the clients that the game is over
        // and to submit their results
        SendGameOver();
    }

    m_rwPlayers.clear();

    m_dwSlotsInUse = 0;
    m_bIsHost      = FALSE;
}


/////////////////
// State Inbox //
/////////////////

//-------------------------------------------------------------------------------------
// Name: EnterStateInbox()
// Desc: Retrieves all the messages waiting in live for the player
//-------------------------------------------------------------------------------------
BOOL CXBoxSample::EnterStateInbox()
{
    // Step 1
    //
    // Start downloading all messages
    // and exit the function when done

    DWORD dwFlags = 0;

    m_dwMessageRenderStart = 0;
    m_dwMessageSelected    = 0;
    m_dwNumMessages        = 0;

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
        RenderWorkingScreen();

        XOnlineTaskContinue(m_hLogonTask );

        Sleep( 15 ); // Pause for a few so we don't throttle the task

        // check to see if the pending sync flag is set
        XOnlineGetNotificationEx( m_wControllingUser, // Controller of user we are checking
                                  &xnei,              // Notification info to write to
                                  &dwFlags );         // Flags being outputed to

    } while( dwFlags & XONLINE_NOTIFICATION_STATE_FLAG_PENDING_SYNC );


    // Step 2
    //
    // Get the invites received

    HRESULT hrGetMessages = XOnlineMessageEnumerate(
                                m_wControllingUser,    // The controller port of the user 
                                                       // checking for messages
                                m_rwMessagesSummaries, // The array of messages to be populated
                                &m_dwNumMessages );    // The number of messages received

    // Limit the number of messages to list
    m_dwNumMessages = m_dwNumMessages > MAX_NUM_MENU_ITEMS
                      ? MAX_NUM_MENU_ITEMS : m_dwNumMessages;

    if( FAILED( hrGetMessages ) )
        m_dwNumMessages = 0;

    return TRUE;
}

//-------------------------------------------------------------------------------------
// Name: UpdateStateInbox()
// Desc: Allows the player to react to messages sent to them via XBox live
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateInbox( DWORD dwControllerPort, Event event )
{
    if ( dwControllerPort != (INT)m_wControllingUser )
        return;

    m_dwMessageSelected = GetMenuPosition( m_dwMessageSelected,
                                           event,
                                           m_dwMessageRenderStart,
                                           m_dwNumMessages,
                                           NUM_ENTRIES_PER_SCREEN );

    switch( event )
    {
    case EV_BUTTON_A: // Accept or decline the invite
        {
            XONLINE_MSG_SUMMARY& msgSummary = m_rwMessagesSummaries[ m_dwMessageSelected ];

            // Initial support for downloading messages
            //
            // Check to see if the message has a text/voice attachment

            if ( msgSummary.bMsgType == XONLINE_MSG_TYPE_TITLE_CUSTOM )
            {
                if ( msgSummary.dwMessageFlags & XONLINE_MSG_FLAG_HAS_TEXT )
                {
                    // clear message summary
                    ZeroMemory( &m_curMessageSummary , sizeof( m_curMessageSummary ) );

                    // assign current message summary
                    m_curMessageSummary = msgSummary;

                    // show message
                    PushState( STATE_TEAM_SHOW_MESSAGE );
                    return;
                }
            }

            // If this is not a team message, ignore it
            if( !( msgSummary.dwMessageFlags & XONLINE_MSG_FLAG_TEAM_CONTEXT ) )
                break;

            // Only deal with a message trying to recuit us
            if( msgSummary.bMsgType != XONLINE_MSG_TYPE_TEAM_RECRUIT )
                break;

            PushState( STATE_INVITE_DETAILS );
        }
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

    MENU_LIST rwSenderList      = { 0 };
    MENU_LIST rwMessageTypeList = { 0 };

    for( DWORD i = m_dwMessageRenderStart;
         ( ( i < m_dwNumMessages ) && ( i < MAX_NUM_MENU_ITEMS ) );
         ++i )
    {
        XBUtil_GetWide( m_rwMessagesSummaries[i].szSenderName,
                        rwSenderList[i], MAX_MENU_STRING_SIZE );

        // Generate the type of message
        switch( m_rwMessagesSummaries[i].bMsgType )
        {
        case XONLINE_MSG_TYPE_FRIEND_REQUEST:
            lstrcpynW( rwMessageTypeList[i], L"FRIEND REQUEST", MAX_MENU_STRING_SIZE );
            break;
        case XONLINE_MSG_TYPE_GAME_INVITE:
            lstrcpynW( rwMessageTypeList[i], L"GAME INVITE", MAX_MENU_STRING_SIZE );
            break;
        case XONLINE_MSG_TYPE_COMP_REMINDER:
            lstrcpynW( rwMessageTypeList[i], L"COMPETITION REMINDER", MAX_MENU_STRING_SIZE );
            break;
        case XONLINE_MSG_TYPE_COMP_REQUEST:
            lstrcpynW( rwMessageTypeList[i], L"COMPETITION REQUEST", MAX_MENU_STRING_SIZE );
            break;
        case XONLINE_MSG_TYPE_LIVE_MESSAGE:
            lstrcpynW( rwMessageTypeList[i], L"LIVE MESSAGE", MAX_MENU_STRING_SIZE );
            break;
        case XONLINE_MSG_TYPE_TEAM_RECRUIT:
            lstrcpynW( rwMessageTypeList[i], L"TEAM RECRUIT", MAX_MENU_STRING_SIZE );
            break;
        case XONLINE_MSG_TYPE_TITLE_CUSTOM:
            if ( m_rwMessagesSummaries[i].dwMessageFlags & XONLINE_MSG_FLAG_HAS_TEXT )
            {
                if ( m_rwMessagesSummaries[i].dwMessageFlags & XONLINE_MSG_FLAG_HAS_VOICE )
                {
                    lstrcpynW( rwMessageTypeList[i], L"TEXT/VOICE", MAX_MENU_STRING_SIZE );
                }
                else
                {
                    lstrcpynW( rwMessageTypeList[i], L"TEXT", MAX_MENU_STRING_SIZE );
                }
            }
            else
            {
                lstrcpynW( rwMessageTypeList[i], L"CUSTOM (OTHER)", MAX_MENU_STRING_SIZE );
            }
            break;

        default:
            lstrcpynW( rwMessageTypeList[i], L"OTHER", MAX_MENU_STRING_SIZE );
            break;
        };
    }

    RenderScrollingMenu( m_font,
                         L"INBOX",
                         m_dwMessageRenderStart,
                         m_dwMessageSelected,
                         m_dwNumMessages,
                         L"SENDER",
                         rwSenderList,
                         L"MESSAGE TYPE",
                         rwMessageTypeList );


    // Render the bottom help text with
    // the correct flags

    INT iFooterFlags = ( ( m_dwNumMessages > 0 ) ? FOOTER_RENDER_SELECT : FOOTER_RENDER_NONE )
                       | FOOTER_RENDER_CANCEL;

    RenderFooter( m_font, (WORD)iFooterFlags );
}


/////////////////////////
// State InviteDetails //
/////////////////////////

//-------------------------------------------------------------------------------------
// Name: EnterStateInviteDetails()
// Desc: Gets the details of the invite selected by the user
//-------------------------------------------------------------------------------------
BOOL CXBoxSample::EnterStateInviteDetails()
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
    xuidTeamInvitedTo.qwTeamID    = 
        m_rwMessagesSummaries[m_dwMessageSelected].qwMessageContext;

    HRESULT hrGetDetails = XOnlineTeamEnumerate(
        m_wControllingUser, // Controller port of the user making the call
        1,                  // Number of teams we are looking for
        &xuidTeamInvitedTo, // XUID of the team we are trying to find details of
        NULL,               // Work event
        &hDetailsTask );    // Task to assign to

    if( FAILED( hrGetDetails ) )
        return FALSE;


    // Step 2
    //
    // Pump the task until it's finished

    do
    {
        hrGetDetails = hDetailsTask.Continue();
    } while ( hrGetDetails == XONLINETASK_S_RUNNING );

    if( FAILED( hrGetDetails ) )
        return FALSE;


    // Step 3
    //
    // Get The enumeration results

    DWORD dwNumTeamsRead = 0;
    hrGetDetails = XOnlineTeamEnumerateGetResults(
                    hDetailsTask,      // The enumeration task
                    &dwNumTeamsRead,   // The number of teams read
                    &xuidTeamInvitedTo // An array of all the team XUIDs read
                 );

    if( ( FAILED( hrGetDetails ) ) || ( dwNumTeamsRead != 1 ) )
        return FALSE;


    // Step 4
    //
    // Get the team details

    hrGetDetails = XOnlineTeamGetDetails(
        hDetailsTask,        // Task handle from the enumerate task
        xuidTeamInvitedTo,   // XUID of the team we are finding details of
        &m_teamInfoInvitedTo // Structure to store the info in
    );

    hDetailsTask.Close();

    return( SUCCEEDED( hrGetDetails ) );
}

//-------------------------------------------------------------------------------------
// Name: UpdateStateInviteDetails()
// Desc: Allows the user to respond to an individual invite
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateInviteDetails( DWORD dwControllerPort, Event event )
{
    if( dwControllerPort != m_wControllingUser )
        return;

    m_dwInviteResponseSelected = GetMenuPosition( m_dwInviteResponseSelected,
                                                  NUM_ITEMS_INVITE_MENU,
                                                  event );

    switch( event )
    {
    case EV_BUTTON_A:
        switch( m_dwInviteResponseSelected )
        {
        // Try to join the team
        case MENU_INVITE_ACCEPT:
            switch( ProcessInvite( dwControllerPort, 
                                   m_rwMessagesSummaries[m_dwMessageSelected], 
                                   XONLINE_PEER_ANSWER_YES ) )
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

        // Say no
        case MENU_INVITE_DECLINE:
            if ( SUCCEEDED( ProcessInvite( dwControllerPort, 
                 m_rwMessagesSummaries[m_dwMessageSelected], XONLINE_PEER_ANSWER_NO ) ) )
            {
                PopState( TRUE );
                PushMessageWindow( "Declined team invite" );
            }
            else
            {
                PushMessageWindow( "Unable to decline team invite" );
            }

            break;

        // Never accept an invite from this person
        case MENU_INVITE_NEVER:
            if( SUCCEEDED( ProcessInvite( dwControllerPort, 
                m_rwMessagesSummaries[m_dwMessageSelected], XONLINE_PEER_ANSWER_NEVER ) ) )
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

    // Back out
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

    RenderMenu( m_font,
                L"INVITE RESPONSE",
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

    wsprintfW( szwMemberCount, L"%d", m_teamInfoInvitedTo.dwMemberCount);

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
    WCHAR      szwCreationDate[XONLINE_GAMERTAG_SIZE] = { 0 };

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
    RenderFooter( m_font, FOOTER_RENDER_SELECT | FOOTER_RENDER_CANCEL );
}

///////////////////////
// State ViewMyTeams //
///////////////////////

//-------------------------------------------------------------------------------------
// Name: UpdateStateViewMyTeams
// Desc: Updates the ViewMyTeams state. Launches any submenus and
//       commands the user may have. Allows the user to select a
//       team and view it's roster.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateViewMyTeams( DWORD dwControllerPort, Event event)
{
    if( dwControllerPort != m_wControllingUser )
        return;


    if( m_dwTeamLogoToDL < m_dwTeamCount )
    {
        assert( m_ppTeamLogoTextures );

        CUserContent dlBuffer;

        // Attempt to DL their buddy icon
        // If the DL fails then it probably
        // is due to the lack of available content
        if( SUCCEEDED( dlBuffer.Download( dwControllerPort,
                                          0, // Get the shared team data
                                          m_rwTeamXUIDS[m_dwTeamLogoToDL].qwTeamID,
                                          TEAM_LOGO_FILENAME ) ) )
        {
            // If we already have a texture created, then there is
            // no reason to make the GPU shuffle resources
            if ( m_ppTeamLogoTextures[m_dwTeamLogoToDL] != NULL )
            {
                dlBuffer.UpdateTexture( m_ppTeamLogoTextures[m_dwTeamLogoToDL] );
            }
            else
            {
                m_ppTeamLogoTextures[m_dwTeamLogoToDL] = 
                    dlBuffer.CreateTexture( m_pd3dDevice );
            }
        }

        ++m_dwTeamLogoToDL;
    }

    m_iTeamSelected = GetMenuPosition( m_iTeamSelected, m_dwTeamCount, event);

    switch( event )
    {
    // Team selected - move to submenu
    case EV_BUTTON_A:
        if( m_dwTeamCount < 1 )
            break;

        PushState( STATE_TEAM_OPS );
        break;

    // Back out
    case EV_BUTTON_B:
        PopState( TRUE );
        break;

    case EV_BUTTON_Y:
        // if we have teams, we should enable the Y button to send a message
        if ( m_dwTeamCount )
        {
            // Sending to own team
            m_eMessageProtocol = VOICE_MAIL_SENT_TO_TEAM_SAVE_SELF;

            // start with default message
            m_iMessageSelected = 0;

            // include ourself (the user ID) in the protocol ID parameter
            m_qwProtocolParamId = CURRENT_USER.xuid.qwUserID;

            PushState( STATE_TEAM_SEND_MESSAGE );
        }
        break;
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

    RenderMenu( m_font, L"MY TEAMS", NULL, 0, 0 );

    FLOAT fTeamListStartY   = POS_SCREEN_TITLE_Y + ( DEFAULT_TEXT_PADDING * 2 );
    FLOAT fTeamIconStartX   = 80.0f;
    FLOAT fTeamNameStartX   = 120.0f;
    FLOAT fTeamDescStartX   = 560.0f;
    FLOAT fTeamNamePaddingY = ICON_SIZE * 1.1f;

    m_font.DrawText( fTeamNameStartX, POS_SCREEN_TITLE_Y + DEFAULT_TEXT_PADDING,
                     COLOR_GREEN,
                     L"TEAM NAME",
                     XBFONT_LEFT );

    m_font.DrawText( fTeamDescStartX, POS_SCREEN_TITLE_Y + DEFAULT_TEXT_PADDING,
                     COLOR_GREEN,
                     L"TEAM DESCRIPTION",
                     XBFONT_RIGHT );


    for( INT i = 0; i < (INT)m_dwTeamCount; ++i)
    {
        FLOAT fPosY = fTeamListStartY + ( fTeamNamePaddingY * i );

        // Render the little buddy icon next to their name
        assert( m_ppTeamLogoTextures );

        if( m_ppTeamLogoTextures[i] )
        {
            SetFacePos( m_pLogoVerts, fTeamIconStartX, fPosY );
            RenderSprite( m_pd3dDevice, m_pLogoVerts, m_ppTeamLogoTextures[i] );
        }

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
    if( ( m_iTeamSelected < (INT)m_dwTeamCount ) && ( m_dwTeamCount > 0 ) )
    {
        // Show selected item with a little triangle
        FLOAT fIconPosY = fTeamListStartY + ( fTeamNamePaddingY * m_iTeamSelected );

        m_font.DrawText( fTeamIconStartX, fIconPosY,
                         COLOR_POINTER, GLYPH_RIGHT_TICK, XBFONT_RIGHT );
    }


    // Render Footer & bottom help text

    // send message button should appear if we have teams
    if ( m_dwTeamCount )
    {
        m_font.DrawText( SCREEN_CENTER_X , POS_FOOTER_Y,
                          COLOR_NORMAL,
                         GLYPH_Y_BUTTON L" send message",
                          XBFONT_CENTER_X );
    }

    RenderFooter( m_font, FOOTER_RENDER_SELECT | FOOTER_RENDER_CANCEL );
}

///////////////////////
// State SendMessage //
///////////////////////

//-------------------------------------------------------------------------------------
// Name: EnterStateSendMessage()
// Desc: Inits Send Message screen
//-------------------------------------------------------------------------------------
BOOL CXBoxSample::EnterStateSendMessage()
{
    // make sure the XHV engine is initialized!
    assert( m_bXHVInitialized );

    // flag message being sent as false
    m_bMessageSent = FALSE;

    // update the text of our current message id (which was set previous to
    // this state)
    UpdateTeamMessageToSend();

    return TRUE;
}

//-------------------------------------------------------------------------------------
// Name: UpdateStateSendMessage
// Desc: Updates the Send Message state.
//       Allows the user to perform message operations
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateSendMessage( DWORD dwControllerPort, Event event )
{
    if( dwControllerPort != m_wControllingUser )
        return;

    // if in the middle of recording
    if ( g_XHVVoiceManager.RecordingNow() )
    {
        HRESULT hrStopRecording = S_OK;

        // Stop voice mail processing if user requests so. Error if attempt to stop
        // fails
        switch( event )
        {
        case EV_BUTTON_B:
            hrStopRecording = g_XHVVoiceManager.StopVoiceMail();
            if ( FAILED( hrStopRecording ) )
            {
                PushMessageWindow( "Error stopping voice mail recording" , TRUE );
            }
            break;
        }
    }
    // in the middle of playback
    else if ( g_XHVVoiceManager.PlayingNow() )
    {
        HRESULT hrStopPlaying;

        // Stop voice mail playback if user requests so. Error if attempt to stop
        // fails
        switch( event )
        {
        case EV_BUTTON_B:
            hrStopPlaying = g_XHVVoiceManager.StopVoiceMail();
            if ( FAILED( hrStopPlaying ) )
            {
                PushMessageWindow( "Error stopping voice mail playback" , TRUE);
            }
            break;
        }
    }
    // if message has been sent
    else if ( m_bMessageSent )
    {
        // pop state and reinitialize previous state when user backs out
        switch( event )
        {
        case EV_BUTTON_B:
            m_bMessageSent = FALSE;

            PopState( TRUE );
            break;
        }
    }
    else
    {
        HRESULT hrMessageOp;

        DWORD dwMaxVoiceMailDurationMs;
        DWORD dwVoiceMailBufferSize;

        // update/poll communicator insertion status
        m_bCommunicatorInserted = ( GetVoiceLevel( dwControllerPort ) 
                                    == VOICE_LEVEL_EVERYTHING );

        switch( event )
        {
        // Attach/record voice mail
        case EV_BUTTON_BLACK:
            // if communicator is inserted and there is no voice attachment
            if ( m_bCommunicatorInserted && !m_bVoiceBufferPlayable )
            {
                m_bGotCommunicatorRemovalEvent = FALSE;


                // do something to warn that we're about to record over old buffer
                m_bVoiceBufferPlayable = FALSE;
                ZeroMemory( g_rwVoiceMailBuffer, sizeof( g_rwVoiceMailBuffer ) );

                // define max duration
                dwMaxVoiceMailDurationMs = CXHVVoiceManager::MAX_VOICEMAIL_DURATION_MS;
                // set buffer size for this duration (pre-calculated.. see constant
                // definition for explanation)
                dwVoiceMailBufferSize = VOICE_BUFFER_SIZE;

                // attempt to start recording voicemail into allocated buffer
                hrMessageOp = g_XHVVoiceManager.StartRecordVoiceMail(
                                    m_wControllingUser,
                                    dwMaxVoiceMailDurationMs,
                                    g_rwVoiceMailBuffer,
                                    dwVoiceMailBufferSize );

                // if failure occurred, alert user, other flag buffer as playable
                if ( FAILED( hrMessageOp ) )
                {

                    ZeroMemory( g_rwVoiceMailBuffer, sizeof( g_rwVoiceMailBuffer ) );

	                PushMessageWindow( "Error starting recording of voice attachment",
                                       TRUE);
                }
                else
                {
                    m_bVoiceBufferPlayable = TRUE;
                }
            }
            break;

        // clear voice attachment
        case EV_BUTTON_WHITE:
            // if a voice attachment exists
            if ( m_bVoiceBufferPlayable )
            {
                // clear voice mail
                hrMessageOp = g_XHVVoiceManager.ClearVoiceMail();

                // if clearing failed, alert user
                if ( FAILED( hrMessageOp ) )
                {
                    PushMessageWindow( "Error clearing voice attachment" , TRUE );
                }
                else
                {
                    // deinit voice mail buffer
                    m_bVoiceBufferPlayable = FALSE;
                    ZeroMemory( g_rwVoiceMailBuffer, sizeof( g_rwVoiceMailBuffer ) );


                }
            }
           break;

        // change message
        case EV_BUTTON_Y:

            // increment team message index
            m_iMessageSelected++;

            // cycle back if we went through all of them
            if ( m_iMessageSelected == NUM_PRECONCEIVED_TEAM_MESSAGES)
            {
                m_iMessageSelected = 0;
            }

            // copy indexed string into team message buffer
            UpdateTeamMessageToSend();
            break;

        // play back voice attachment if exists
        case EV_BUTTON_X:
            // if there is a voice attachment
            if ( m_bVoiceBufferPlayable )
            {
                // Play voice mail...
                // if communicator is detected, play through headphones,
                // otherwise, play through monitor speakers
                // (TRUE, means outputting to speakers)
                hrMessageOp = g_XHVVoiceManager.PlayRecordedVoiceMail( 
                                            !m_bCommunicatorInserted );



                // if playback failed, alert user
                if ( FAILED( hrMessageOp ) )
                {
                    PushMessageWindow( "Error starting playback of voice attachment", 
                                       TRUE );
                }
            }
            break;

        case EV_BUTTON_A:
            {
                // attempt to send text and/or voice message
                EVoiceMessageSuccess  eVoiceMailSuccess =
                SendVoiceMessage( m_rwTeamXUIDS[ m_iTeamSelected ], 
                                  m_bVoiceBufferPlayable, m_eMessageProtocol , 
                                  m_qwProtocolParamId );

                // flag message sent if message was sent successfully, otherwise
                // alert user of specific error
                switch( eVoiceMailSuccess )
                {
                case VOICE_MAIL_SUCCESS:
                    m_bMessageSent = TRUE;
                    break;

                case VOICE_MAIL_ERROR_COULDNT_CREATE_MESSAGE:
                    PushMessageWindow( "Could not create message." , TRUE );
                    break;

                case VOICE_MAIL_ERROR_NO_TEAM_EXISTS:
                    PushMessageWindow( "Team does not exist." , TRUE );
                    break;

                case VOICE_MAIL_ERROR_NO_RECIPIENTS:
                    PushMessageWindow( "Message has no recipients.", TRUE );
                    break;

                default:
                    assert( 0 && "Did not handle Voice Mail Send case" );
                    break;
                }
            }
            break;

        // back out if user requests it
        case EV_BUTTON_B:
            PopState( TRUE );
            break;
        }
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateSendMessage()
// Desc: Renders Send Message state
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateSendMessage()
{
    RenderControllingUser();

    // if in the middle of recording
    if ( g_XHVVoiceManager.RecordingNow() )
    {
        // provide a main message
        m_font.DrawText( SCREEN_CENTER_X, SCREEN_CENTER_Y, COLOR_NORMAL,
                    L"Recording Voice Attachment Now...", XBFONT_CENTER_X );

        // provide instructions to stop recording
        m_font.DrawText( POS_FOOTER_LEFT, POS_FOOTER_Y,
                         COLOR_NORMAL, GLYPH_B_BUTTON L" stop recording",
                         XBFONT_LEFT );

    }
    // if in the middle of playback
    else if ( g_XHVVoiceManager.PlayingNow() )
    {
        // provide a main message
        m_font.DrawText( SCREEN_CENTER_X, SCREEN_CENTER_Y, COLOR_NORMAL,
                    L"Playing Voice Attachment Now...", XBFONT_CENTER_X );

       // provide instructions to stop playback
        m_font.DrawText( POS_FOOTER_LEFT, POS_FOOTER_Y,
                         COLOR_NORMAL, GLYPH_B_BUTTON L" stop playback",
                         XBFONT_LEFT );
    }
    // if message sent
    else if ( m_bMessageSent )
    {
        // provide a main message
        m_font.DrawText( SCREEN_CENTER_X, SCREEN_CENTER_Y, COLOR_NORMAL,
                    L"Message Sent", XBFONT_CENTER_X );

        // provide back out instructions
        m_font.DrawText( POS_FOOTER_LEFT, POS_FOOTER_Y,
                         COLOR_NORMAL, GLYPH_B_BUTTON L" back",
                         XBFONT_LEFT );
    }
    else
    {
        const FLOAT RENDER_PADDING = 30.0f;
        const FLOAT RENDER_TEXT_MESSAGE_OFFSET_Y = SCREEN_SIZE_Y * 0.33f;
        const FLOAT RENDER_VOICE_MESSAGE_OFFSET_Y = SCREEN_SIZE_Y * 0.55f;

        // render header of send message screen depending on message sending protocol
        switch( m_eMessageProtocol )
        {
        case VOICE_MAIL_SENT_TO_TEAM_SAVE_SELF:
            {
                // Screen title
                m_font.DrawText( SCREEN_CENTER_X, POS_SCREEN_TITLE_Y,
                                 COLOR_NORMAL, 
                                 L"SEND MESSAGE to members of your team",
                                 XBFONT_CENTER_X );
            }
            break;

        case VOICE_MAIL_SENT_TO_TEAM_OWNERS:
            {
                // Screen title
                m_font.DrawText( SCREEN_CENTER_X, POS_SCREEN_TITLE_Y,
	                             COLOR_NORMAL, 
                                 L"SEND MESSAGE to owners of opposing team",
                                 XBFONT_CENTER_X );
            }
            break;

        case VOICE_MAIL_SENT_TO_INDIVIDUAL:
            {
                // create a string containing the sendee's gamer tag 
                // (calculated previously)
                const DWORD SEND_MESSAGE_HEADER_SIZE = XONLINE_GAMERTAG_SIZE + 32;
                WCHAR swzSendMessageHeader[SEND_MESSAGE_HEADER_SIZE];
	            _snwprintf( swzSendMessageHeader , 
                            SEND_MESSAGE_HEADER_SIZE , 
                            L"SEND MESSAGE to %s" , 
                            m_swzRecipientTag );

                // Screen title
                m_font.DrawText( SCREEN_CENTER_X, POS_SCREEN_TITLE_Y,
                                 COLOR_NORMAL, swzSendMessageHeader, XBFONT_CENTER_X );
            }
            break;
        }

        // Message text
        m_font.DrawText( SCREEN_CENTER_X, RENDER_TEXT_MESSAGE_OFFSET_Y,
                         COLOR_NORMAL, g_rwTeamTextMsgBuffer,
                         XBFONT_CENTER_X );

        // if there is a voice attachment
        if ( m_bVoiceBufferPlayable )
        {
            // provide footer voice attachment display
            m_font.DrawText( SCREEN_CENTER_X, RENDER_VOICE_MESSAGE_OFFSET_Y,
                             COLOR_HIGHLIGHT, 
                             L"VOICE ATTACHMENT PRESENT", 
                             XBFONT_CENTER_X );

            // provide footer voice attachment playback instructions
            m_font.DrawText( SCREEN_CENTER_X, 
                             RENDER_VOICE_MESSAGE_OFFSET_Y + RENDER_PADDING,
                             COLOR_NORMAL, 
                             GLYPH_X_BUTTON L" play voice attachment", 
                             XBFONT_CENTER_X );

            // provide footer voice attachment clearing instructions
            m_font.DrawText( POS_FOOTER_RIGHT, POS_FOOTER_Y - RENDER_PADDING,
                             COLOR_NORMAL, 
                             GLYPH_WHITE_BUTTON L" clear voice attachment", 
                             XBFONT_RIGHT );
        }
        else
        {

            // provide footer voice attachment display.. lack thereof
            m_font.DrawText( SCREEN_CENTER_X, RENDER_VOICE_MESSAGE_OFFSET_Y,
                        COLOR_NORMAL, L"(no voice attachment)",
                        XBFONT_CENTER_X );

            // if communicator is inserted
            if ( m_bCommunicatorInserted )
            {
                // provide footer voice attachment recording instructions
                m_font.DrawText( POS_FOOTER_RIGHT, POS_FOOTER_Y - RENDER_PADDING,
	                        COLOR_NORMAL, 
                            GLYPH_BLACK_BUTTON L" record voice attachment", 
                            XBFONT_RIGHT );
            }

        }

        // provide text message toggle instructions
        m_font.DrawText( POS_FOOTER_LEFT, POS_FOOTER_Y - RENDER_PADDING,
                       COLOR_NORMAL, GLYPH_Y_BUTTON L" change message",
                       XBFONT_LEFT );

        // back
        m_font.DrawText( POS_FOOTER_LEFT, POS_FOOTER_Y,
                       COLOR_NORMAL, GLYPH_B_BUTTON L" back",
                       XBFONT_LEFT );

        // send message instructions
        m_font.DrawText( POS_FOOTER_RIGHT, POS_FOOTER_Y,
                       COLOR_NORMAL, GLYPH_A_BUTTON L" send message",
                       XBFONT_RIGHT );
    }
}

//-------------------------------------------------------------------------------------
// Name: ExitStateSendMessage()
// Desc: exits Send Message state
//-------------------------------------------------------------------------------------
VOID CXBoxSample::ExitStateSendMessage()
{
    m_bMessageSent = FALSE;

    // clear voice mail from manager
    g_XHVVoiceManager.ClearVoiceMail();

    // clear voice mail buffer
    m_bVoiceBufferPlayable = FALSE;

    // zero out memory for voice buffer and text buffer
    ZeroMemory( g_rwVoiceMailBuffer, sizeof( g_rwVoiceMailBuffer ) );
    ZeroMemory( g_rwTeamTextMsgBuffer , sizeof( g_rwTeamTextMsgBuffer ) );
}

///////////////////////
// State ShowMessage //
///////////////////////

//-------------------------------------------------------------------------------------
// Name: EnterStateShowMessage()
// Desc: Inits Send Message screen
//-------------------------------------------------------------------------------------
BOOL CXBoxSample::EnterStateShowMessage()
{
    // make sure XHV is initialized
    assert( m_bXHVInitialized );

    // initialize voice buffer as unplayable
    m_bVoiceBufferPlayable = MessageHasVoiceAttachment( m_curMessageSummary );

    // attempt to download text and/or voice message
    m_eMessageDownloadSuccess = DownloadTextVoiceMessage( m_curMessageSummary );

    // flag message as not deleted
    m_bMessageDeleted = FALSE;

    return TRUE;
}

//-------------------------------------------------------------------------------------
// Name: UpdateStateShowMessage
// Desc: Updates the Send Message state.
//       Allows the user to perform message operations
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateShowMessage( DWORD dwControllerPort, Event event )
{
    if( dwControllerPort != m_wControllingUser )
        return;

    // if in the middle of playing
    if ( g_XHVVoiceManager.PlayingNow() )
    {
        HRESULT hrStopPlaying;

        // if users requests stopping playback, do so. Show error if one occurs.
        switch( event )
        {
        case EV_BUTTON_B:
            hrStopPlaying = g_XHVVoiceManager.StopVoiceMail();
            if ( FAILED( hrStopPlaying ) )
            {
                PushMessageWindow("Error stopping voice mail playback");
            }
            break;
        }
    }
    // if either the message has not downloaded or the message was deleted
    else if ( ( m_eMessageDownloadSuccess != VOICE_MAIL_DOWNLOAD_SUCCESS ) 
               || m_bMessageDeleted )
    {
        // go back to previous state (inbox) when user requests it
        switch( event )
        {
        case EV_BUTTON_B:
            m_bMessageDeleted = FALSE;

            PopState( TRUE );

            break;
        }
    }
    else
    {
        HRESULT hrMessageOp;

        switch( event )
        {

        // play back voice attachment if exists
        case EV_BUTTON_X:
            // if there is a voice attachment
            if ( m_bVoiceBufferPlayable )
            {
                // Play voice mail
                // if communicator is detected, play through headphones,
                // otherwise, play through monitor speakers
                // (TRUE, means outputting to speakers)
                BOOL bCommunicatorInserted =
                    ( GetVoiceLevel( m_wControllingUser ) == VOICE_LEVEL_EVERYTHING );
                hrMessageOp = g_XHVVoiceManager.PlayRecordedVoiceMail( 
                                              !bCommunicatorInserted );

                // if error occurred, alert user
                if ( FAILED( hrMessageOp ) )
                {
                    PushMessageWindow("Error starting playback of voice attachment");
                }
            }
            break;

        case EV_BUTTON_A:
            // delete message, and make sure it gets deleted
            hrMessageOp = XOnlineMessageDelete( 
                          m_wControllingUser,              // user id
                          m_curMessageSummary.dwMessageID, // message id
                          FALSE );                         // don't filter out sender

            assert( SUCCEEDED ( hrMessageOp ) );

            m_bMessageDeleted = TRUE;

            break;

        case EV_BUTTON_B:
            PopState( TRUE );
            break;
        }
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateSendMessage()
// Desc: Renders Send Message state
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateShowMessage()
{
    RenderControllingUser();

    if ( g_XHVVoiceManager.PlayingNow() )
    {
        // main message
        m_font.DrawText( SCREEN_CENTER_X, SCREEN_CENTER_Y, COLOR_NORMAL,
                    L"Playing Voice Attachment Now...", XBFONT_CENTER_X );

        // Bottom Help text
        m_font.DrawText( POS_FOOTER_LEFT, POS_FOOTER_Y,
                         COLOR_NORMAL, GLYPH_B_BUTTON L" stop playback",
                         XBFONT_LEFT );
    }
    else if ( m_eMessageDownloadSuccess != VOICE_MAIL_DOWNLOAD_SUCCESS )
    {
        const DWORD DOWNLOAD_ERROR_STR_SIZE = 256;
        WCHAR szwErrorStr[ DOWNLOAD_ERROR_STR_SIZE ];
 
        switch( m_eMessageDownloadSuccess )
        {
        case VOICE_MAIL_DOWNLOAD_ERROR_COULDNT_ACCESS_MESSAGE:
            XBUtil_GetWide( "Could not access text/voice message" , 
                            szwErrorStr , DOWNLOAD_ERROR_STR_SIZE );
            break;

        case VOICE_MAIL_DOWNLOAD_ERROR_VOICE_ATTACHMENT_MISSING:
            XBUtil_GetWide( "Could not access voice attachment in message" , 
                            szwErrorStr , DOWNLOAD_ERROR_STR_SIZE );
            break;

        case VOICE_MAIL_DOWNLOAD_ERROR_DOWNLOAD_FAILED:
            XBUtil_GetWide( "Download of text/voice message failed" , 
                            szwErrorStr , DOWNLOAD_ERROR_STR_SIZE );
            break;

        default:
            assert( 0 && "Unhandled text/voice message download error case" );
            break;
        }

        // main message
        m_font.DrawText( SCREEN_CENTER_X, SCREEN_CENTER_Y, COLOR_NORMAL,
                         szwErrorStr , XBFONT_CENTER_X );

        // Bottom Help text
        m_font.DrawText( POS_FOOTER_LEFT, POS_FOOTER_Y,
                         COLOR_NORMAL, GLYPH_B_BUTTON L" back",
                         XBFONT_LEFT );
    }
    else if ( m_bMessageDeleted )
    {
        // main message
        m_font.DrawText( SCREEN_CENTER_X, SCREEN_CENTER_Y, COLOR_NORMAL,
                    L"Message deleted", XBFONT_CENTER_X );

        // Bottom Help text
        m_font.DrawText( POS_FOOTER_LEFT, POS_FOOTER_Y,
                         COLOR_NORMAL, GLYPH_B_BUTTON L" back",
                         XBFONT_LEFT );
   }
    else
    {
        const FLOAT RENDER_PADDING = 30.0f;
        const FLOAT RENDER_TEXT_MESSAGE_OFFSET_Y = SCREEN_SIZE_Y * 0.33f;
        const FLOAT RENDER_VOICE_MESSAGE_OFFSET_Y = SCREEN_SIZE_Y * 0.55f;

        // Screen title
        m_font.DrawText( SCREEN_CENTER_X, POS_SCREEN_TITLE_Y,
                         COLOR_NORMAL, L"SHOW MESSAGE",
                         XBFONT_CENTER_X );

        // Message text
        m_font.DrawText( SCREEN_CENTER_X, RENDER_TEXT_MESSAGE_OFFSET_Y,
                         COLOR_NORMAL, g_rwTeamTextMsgBuffer,
                         XBFONT_CENTER_X );

        if ( m_bVoiceBufferPlayable )
        {
            // footer voice attachment function
            m_font.DrawText( SCREEN_CENTER_X, RENDER_VOICE_MESSAGE_OFFSET_Y,
                             COLOR_HIGHLIGHT, 
                             L"VOICE ATTACHMENT PRESENT", 
                             XBFONT_CENTER_X );

            // footer voice attachment function
            m_font.DrawText( SCREEN_CENTER_X, 
                             RENDER_VOICE_MESSAGE_OFFSET_Y + RENDER_PADDING,
                             COLOR_NORMAL, 
                             GLYPH_X_BUTTON L" play voice attachment", 
                             XBFONT_CENTER_X );

        }
        else
        {

            // footer voice attachment function
            m_font.DrawText( SCREEN_CENTER_X, RENDER_VOICE_MESSAGE_OFFSET_Y,
                        COLOR_NORMAL, L"(no voice attachment)",
                        XBFONT_CENTER_X );

        }

        // Bottom Help text
        m_font.DrawText( POS_FOOTER_LEFT, POS_FOOTER_Y,
                       COLOR_NORMAL, GLYPH_B_BUTTON L" back",
                       XBFONT_LEFT );

        m_font.DrawText( POS_FOOTER_RIGHT, POS_FOOTER_Y,
                       COLOR_NORMAL, GLYPH_A_BUTTON L" delete message",
                       XBFONT_RIGHT );
    }
}

//-------------------------------------------------------------------------------------
// Name: ExitStateShowMessage()
// Desc: exits Send Message state
//-------------------------------------------------------------------------------------
VOID CXBoxSample::ExitStateShowMessage()
{
    m_eMessageDownloadSuccess = INVALID_VOICE_MAIL_DOWNLOAD_ERROR;
    m_bMessageDeleted = FALSE;

    g_XHVVoiceManager.ClearVoiceMail();

    // clear voice mail buffer
    m_bVoiceBufferPlayable = FALSE;

    ZeroMemory( &m_curMessageSummary , sizeof( m_curMessageSummary ) );
    ZeroMemory( g_rwVoiceMailBuffer, sizeof( g_rwVoiceMailBuffer ) );
    ZeroMemory( g_rwTeamTextMsgBuffer , sizeof( g_rwTeamTextMsgBuffer ) );
}

///////////////////
// State TeamOps //
///////////////////

//-------------------------------------------------------------------------------------
// Name: UpdateStateTeamOps
// Desc: Updates the team operations menu.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateTeamOps( DWORD dwControllerPort, Event event )
{
    if( dwControllerPort != m_wControllingUser )
        return;

    m_iItemSelected = GetMenuPosition( m_iItemSelected,
                                       NUM_ITEMS_TEAM_OPS_MENU,
                                       event );

    switch( event )
    {
    case EV_BUTTON_B:
        PopState();
        break;

    case EV_BUTTON_A:
        switch( m_iItemSelected )
        {
        // Retrieve and view the roster for this team
        case MENU_TEAM_OPS_VIEW_ROSTER:
            if( GetTeamRoster( dwControllerPort, m_phTeamRosterTask,
                               m_rwTeamXUIDS[m_iTeamSelected],
                               m_rwTeamMembers, m_dwTeamMemberCount ) )
            {
                m_bDisableRosterOps = FALSE;

                PushState( STATE_VIEW_TEAM_ROSTER );
            }
            else
                PushMessageWindow( "Unable to retrieve team roster." );

            break;

        // Add statistics for this team to
        // the leaderboard
        case MENU_TEAM_OPS_CHANGE_STATS:
            // Change team statistics
            if( m_dwTeamCount > 0 )
            {
                // Variables to keep the statistics
                // added to the team
                LONG     lKills   = 0;
                LONG     lDeaths  = 0;
                LONG     lAssists = 0;
                LONGLONG llRating = 0;

                if( WriteTeamStatistics( m_rwTeamXUIDS[m_iTeamSelected],
                                         lKills, lDeaths, lAssists, llRating ) )
                {
                    // Keep this from getting throttled and
                    // throwing an exception
                    CXBStopWatch throttlePreventionTimer;
                    throttlePreventionTimer.StartZero();

                    do
                    {
                        RenderWorkingScreen();
                    }
                    while( throttlePreventionTimer.GetElapsedSeconds() < 2.0f );


                    // Tell the user what stats they added
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

        // Edit the team logo icon
        case MENU_TEAM_OPS_EDIT_ICON:
            {
            // Try to edit the team logo icon
            m_userContent.Clear();

            // Force an update to the icon when we return from editing
            m_dwTeamLogoToDL = 0;

            m_bUploadInsteadOfSave = TRUE;
            m_bTeamLogo            = TRUE;
            PushState( STATE_CONTENT_EDIT );

            m_wszFilename = TEAM_LOGO_FILENAME;

            HRESULT hrDownload = m_userContent.Download( 
                                 dwControllerPort,
                                 0, // Get the shared team logo
                                 m_rwTeamXUIDS[m_iTeamSelected].qwTeamID,
                                 TEAM_LOGO_FILENAME );

            // Attempt to download the existing team logo
            if( FAILED( hrDownload ) && ( hrDownload != XONLINE_E_STORAGE_FILE_NOT_FOUND ))
            {
                PushMessageWindow( "Unable to retrieve previous version" );
            }
            }
            break;

        // Retreive a list of competitions
        // we have joined
        case MENU_TEAM_OPS_SEARCH_JOINED_TOURNEYS:
            if( TournamentSearch( dwControllerPort,
                                  m_rwTeamXUIDS[m_iTeamSelected],
                                  g_joinedCompQuery ) )
            {
                PushState( STATE_LIST_TOURNEYS );
            }
            else
            {
                PushMessageWindow( "You have not joined any competitions" );
            }
            break;

        // View a list of more competitions
        // that we could join
        case MENU_TEAM_OPS_SEARCH_AVAILABLE_TOURNEYS:
            if( TournamentSearch( dwControllerPort,
                                  g_availableResults ) )
            {
                PushState( STATE_LIST_AVAILABLE_COMPS );
            }
            else
            {
                PushMessageWindow( "Unable to find any open competitions" );
            }
            break;

        // Create and join a competition
        case MENU_TEAM_OPS_CREATE_TOURNEY:
            if( CreateTournament(
                    dwControllerPort,
                    CURRENT_USER.xuid,
                    m_rwTeamXUIDS[m_iTeamSelected],
                    g_competition ) )
            {
                PushMessageWindow( "Created Competition" );
            }
            else
            {
                PushMessageWindow( "Failed to create competition" );
            }
            break;

        // Change the name and description of the team
        case MENU_TEAM_OPS_CHANGE_NAME:
            // Change team properties
            // Only attempt to change properties if
            // we have a team to change
            if( m_dwTeamCount < 1 )
                break;

            if( ChangeTeamProperties( dwControllerPort, m_rwTeamInfo[m_iTeamSelected] ) )
            {
                // Cause a re-init to see the new info
                GetTeamList( dwControllerPort,
                             CURRENT_USER.xuid,
                             m_ppTeamLogoTextures,
                             m_dwTeamLogoToDL, m_rwTeamXUIDS,
                             m_rwTeamInfo, m_dwTeamCount );

                PopState( TRUE );
            }
            else
            {
                PushMessageWindow( "Unable to change team properties." );
            }
            break;

        // Delete the entire team!
        case MENU_TEAM_OPS_DELETE:
            // This removes the team from ALL of xbox live
            if( SUCCEEDED( DeleteTeam( dwControllerPort, m_rwTeamXUIDS[m_iTeamSelected] ) ) )
            {
                PopState( TRUE );
                PushMessageWindow( "The team was deleted." );

                // Reinitialize the data
                GetTeamList( dwControllerPort,
                             CURRENT_USER.xuid,
                             m_ppTeamLogoTextures,
                             m_dwTeamLogoToDL, m_rwTeamXUIDS,
                             m_rwTeamInfo, m_dwTeamCount );

                return;
            }
            else
            {
                PushMessageWindow( "Unable to delete the team." );
            }
            break;
        }
        break;
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateTeamOps()
// Desc: Renders the team operations menu.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateTeamOps()
{
    RenderControllingUser();

    RenderMenu( m_font,
                L"TEAM OPTIONS",
                (const WCHAR**)MENU_TEAM_OPS, NUM_ITEMS_TEAM_OPS_MENU,
                m_iItemSelected );

    // Bottom Help text
    RenderFooter( m_font, FOOTER_RENDER_SELECT | FOOTER_RENDER_CANCEL );
}

//////////////////////////////
// State ListAvailableComps //
//////////////////////////////

//-------------------------------------------------------------------------------------
// Name: EnterStateListAvailableComps()
// Desc: Initializes any variables needed to retrieve the list of
//       available competitions.
//-------------------------------------------------------------------------------------
BOOL CXBoxSample::EnterStateListAvailableComps()
{
    m_dwAvailableCompSelected    = 0;
    m_dwAvailableCompRenderStart = 0;

    return TRUE;
}

//-------------------------------------------------------------------------------------
// Name: UpdateStateListAvailableComps()
// Desc: Allows the user to choose a competition to join.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateListAvailableComps( DWORD dwControllerPort, Event event )
{
    m_dwAvailableCompSelected = GetMenuPosition( m_dwAvailableCompSelected,
                                                 event,
                                                 m_dwAvailableCompRenderStart,
                                                 g_availableResults.GetSize(),
                                                 NUM_TEAMS_PER_SCREEN );

    switch( event )
    {
    case EV_BUTTON_B:
        PopState();
        break;

    case EV_BUTTON_A:
        {
        // Attempt to join the competition
        HRESULT hrJoin = JoinCompetition( 
                         dwControllerPort,
                         m_rwTeamXUIDS[m_iTeamSelected],
                         g_availableResults[m_dwAvailableCompSelected].bi_entity_id,
                         g_availableResults[m_dwAvailableCompSelected].att_name );

        if( SUCCEEDED( hrJoin ) )
        {
            PushMessageWindow( "Joined Competition" );
        }
        else
        {
            PushMessageWindow( hrJoin == XONLINE_E_COMP_ALREADY_REGISTERED
                               ? "Your team has already joined"
                               : "Unable to join competition" );
        }
        }
        break;
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateListAvailableComps()
// Desc: Renders the list of compeititions the user can join.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateListAvailableComps()
{
    RenderControllingUser();

    MENU_LIST rwNameList  = { 0 };
    MENU_LIST rwStateList = { 0 };

    for( DWORD i = m_dwAvailableCompRenderStart;
         ( ( i < g_availableResults.GetSize() ) && 
           ( i < ( m_dwAvailableCompRenderStart + NUM_ENTRIES_PER_SCREEN ) ) );
         ++i )
    {
        // Place all the data in the array starting with
        // index 0. This keeps us from having to keep
        // a monster sized array for a 1-to-1 correspondence
        INT iDataIndex = i - m_dwAvailableCompRenderStart;

        // Construct the name
        _snwprintf( rwNameList[iDataIndex], ( MAX_MENU_STRING_SIZE - 1 ),
                    L"%s\0",
                    g_availableResults[i].att_name );

        lstrcpynW( rwStateList[iDataIndex], L"REGISTERING", MAX_MENU_STRING_SIZE );
    }

    RenderScrollingMenu( m_font,
                         L"AVAILABLE COMPETITIONS",
                         m_dwAvailableCompRenderStart,
                         m_dwAvailableCompSelected,
                         g_availableResults.GetSize(),
                         L"NAME",
                         rwNameList,
                         L"STATUS",
                         rwStateList,
                         TRUE );

    // Bottom Help text
    RenderFooter( m_font, FOOTER_RENDER_CANCEL );

    m_font.DrawText( POS_FOOTER_RIGHT, POS_FOOTER_Y,
                     COLOR_NORMAL,
                     GLYPH_A_BUTTON L"Join",
                     XBFONT_RIGHT );
}

//////////////////////////
// State ViewTeamRoster //
//////////////////////////

//-------------------------------------------------------------------------------------
// Name: EnterStateViewTeamRoster()
// Desc: Initialises the roster menu. Clears out the texture list, and retrieves the
//       team roster.
//-------------------------------------------------------------------------------------
BOOL CXBoxSample::EnterStateViewTeamRoster()
{
    m_dwRosterRenderStart     = 0;
    m_dwTeamMemberSelected    = 0;
    m_dwTeamMemberTextureToDL = 0;

    // Initialize the array of pointers to use for
    // the team member buddy icons

    if( m_ppTeammateTextures )
        delete[] m_ppTeammateTextures;

    m_ppTeammateTextures = NULL;

    m_ppTeammateTextures = new LPDIRECT3DTEXTURE8[m_dwTeamMemberCount];
    assert( m_ppTeammateTextures );

    if( m_ppTeammateTextures )
    {
        ZeroMemory( m_ppTeammateTextures,
                    sizeof( LPDIRECT3DTEXTURE8 ) * m_dwTeamMemberCount );
    }

    return TRUE;
}

//-------------------------------------------------------------------------------------
// Name: UpdateStateViewTeamRoster()
// Desc: Allows the user to issue commands to the roster (delete team, kick members)
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateViewTeamRoster( DWORD dwControllerPort, Event event)
{
    if( dwControllerPort != m_wControllingUser )
        return;

    // Download an icon a tick
    if( m_dwTeamMemberTextureToDL < m_dwTeamMemberCount )
    {
        assert( m_ppTeammateTextures );

        CUserContent dlBuffer;

        // Attempt to DL their buddy icon
        // If the DL fails then it probably
        // is due to the lack of available content
        if( SUCCEEDED( dlBuffer.Download( dwControllerPort,
                                          m_rwTeamMembers[m_dwTeamMemberTextureToDL].qwUserID,
                                          m_rwTeamXUIDS[m_iTeamSelected].qwTeamID ) ) )
        {
            // If we already have a texture created, then there is
            // no reason to make the GPU shuffle resources
            if ( m_ppTeammateTextures[m_dwTeamMemberTextureToDL] != NULL )
            {
                dlBuffer.UpdateTexture( m_ppTeammateTextures[m_dwTeamMemberTextureToDL] );
            }
            else
            {
                m_ppTeammateTextures[m_dwTeamMemberTextureToDL] = 
                    dlBuffer.CreateTexture( m_pd3dDevice );
            }
        }

        ++m_dwTeamMemberTextureToDL;
    }

    m_dwTeamMemberSelected = GetMenuPosition(
                                m_dwTeamMemberSelected,
                                event,
                                m_dwRosterRenderStart,
                                m_dwTeamMemberCount,
                                NUM_TEAMS_PER_SCREEN
                            );

    switch( event )
    {
    case EV_BUTTON_A:
        // If we selected ourselves then edit our icon
        if( m_rwTeamMembers[m_dwTeamMemberSelected].qwUserID
            == CURRENT_USER.xuid.qwUserID )
        {
            m_userContent.Clear();

            // Force an update to the icon when we return from editing
            m_dwTeamMemberTextureToDL = 0;

            m_bUploadInsteadOfSave = TRUE;
            m_bTeamLogo            = FALSE;
            PushState( STATE_CONTENT_EDIT );


            HRESULT hrDownload =  m_userContent.Download( 
                                  dwControllerPort,
                                  CURRENT_USER.xuid.qwUserID,
                                  m_rwTeamXUIDS[m_iTeamSelected].qwTeamID );

            if( FAILED( hrDownload ) && ( hrDownload != XONLINE_E_STORAGE_FILE_NOT_FOUND ) )
            {
                PushMessageWindow( "Unable to retrieve previous version" );
            }
        }
        // If we are a team owner, then we
        // can promote, demote, kickout, etc the user
        else if( !m_bDisableRosterOps )
        {
            PushState( STATE_TEAM_MEMBER_OPS );
        }

        break;

    case EV_BUTTON_B:
        // Back out and update our team list
        m_phTeamRosterTask.Close();
        if( !m_bDisableRosterOps )
        {
            GetTeamList( dwControllerPort,
                        CURRENT_USER.xuid,
                        m_ppTeamLogoTextures,
                        m_dwTeamLogoToDL, m_rwTeamXUIDS,
                        m_rwTeamInfo, m_dwTeamCount );
        }
        PopState( TRUE );
        break;

    case EV_BUTTON_Y:
        // we'll only send a message if not selecting self, 
        // and we have at least one team member
        if( m_rwTeamMembers[m_dwTeamMemberSelected].qwUserID
            != CURRENT_USER.xuid.qwUserID )
        {
            // sending message to individual
            m_eMessageProtocol = VOICE_MAIL_SENT_TO_INDIVIDUAL;

            // start with default message
            m_iMessageSelected = 0;

            // we're sending to just this team member
            m_qwProtocolParamId = m_rwTeamMembers[m_dwTeamMemberSelected].qwUserID;

            // we need to get the member detail in order to get the member's tag
            XONLINE_TEAM_MEMBER memberInfo;
            HRESULT hrMemberDetail = S_OK;

            hrMemberDetail = XOnlineTeamMemberGetDetails(
                              // The task used to retrieve the team roster
                              m_phTeamRosterTask,                      
                              // The XUID of the member to retrieve
                              m_rwTeamMembers[m_dwTeamMemberSelected], 
                              // The structure to populate with details
                              &memberInfo                              
                             );


            // Make up a gamer tag if we fail (since this is gratuitous)
            if( hrMemberDetail != S_OK )
            {
                PushMessageWindow("Could not retrieve team member's gamer tag.");
                const CHAR DEFAULT_ERROR_GAMER_TAG_STR[] = "(Unknown)";
                XBUtil_GetWide( DEFAULT_ERROR_GAMER_TAG_STR, m_swzRecipientTag , 
                                XONLINE_GAMERTAG_SIZE );
            }
            else
            {
                XBUtil_GetWide( memberInfo.szGamertag, m_swzRecipientTag, 
                                XONLINE_GAMERTAG_SIZE );
            }

            PushState( STATE_TEAM_SEND_MESSAGE );

        }
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

    RenderMenu( m_font, L"TEAM ROSTER", NULL, 0, 0 );

    FLOAT fTeamListStartY     = POS_SCREEN_TITLE_Y + ( DEFAULT_TEXT_PADDING * 2.0f );
    FLOAT fTeamIconStartX     = 80.0f;
    FLOAT fTeammateNameStartX = 120.0f;
    FLOAT fTeamDescStartX     = 560.0f;
    FLOAT fRosterPaddingY     = ICON_SIZE * 1.25f;


    m_font.DrawText( fTeammateNameStartX, POS_SCREEN_TITLE_Y + DEFAULT_TEXT_PADDING,
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


    XONLINE_TEAM_MEMBER memberInfo;
    HRESULT             hrMemberDetail = S_OK;


    // If we are starting the render from a position
    // other than the first user in the list
    // the draw a little arrow on the side telling
    // the user they can scroll up
    if( m_dwRosterRenderStart > 0 )
    {
        m_font.DrawText( fTeamIconStartX, fTeamListStartY,

            COLOR_HIGHLIGHT, GLYPH_UP_ARROW L"    ", XBFONT_RIGHT );
    }


    // Get the details of each team member
    for( DWORD i = m_dwRosterRenderStart; i < m_dwTeamMemberCount; ++i )
    {
        // Stop rendering if we hit the maximum number
        // team members viewable at once
        if( ( i - m_dwRosterRenderStart ) >= ( NUM_ENTRIES_PER_SCREEN - 1 ) )
        {
            // If more team roster entries are below
            // the last entry drawn, then add a down
            // arrow on the side telling the user
            // can scroll down
            FLOAT fDownArrowY = fTeamListStartY
                                + ( fRosterPaddingY * ( NUM_ENTRIES_PER_SCREEN - 2 ) );

            m_font.DrawText( fTeamIconStartX, fDownArrowY,
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
        FLOAT fPosY       = fTeamListStartY + ( fRosterPaddingY * iScreenItem );

        // Allow the user to move the selector
        // up and down to select a specific user
        // to give an permissions to or
        // to remove from the team
        //
        // Show selected item with a little triangle

        FLOAT fIconPosY = fTeamListStartY + 
                          ( fRosterPaddingY * 
                          ( m_dwTeamMemberSelected - m_dwRosterRenderStart ) );

        m_font.DrawText( fTeamIconStartX, fIconPosY,
                         COLOR_POINTER, GLYPH_RIGHT_TICK, XBFONT_RIGHT );

        // Render the little buddy icon next to their name
        assert( m_ppTeammateTextures );

        if( m_ppTeammateTextures[i] )
        {
            SetFacePos( m_pLogoVerts, fTeamIconStartX, fPosY );
            RenderSprite( m_pd3dDevice, m_pLogoVerts, m_ppTeammateTextures[i] );
        }

        DWORD dwColor = ( m_rwTeamMembers[i].qwUserID == CURRENT_USER.xuid.qwUserID )
                        ? COLOR_HIGHLIGHT : COLOR_NORMAL;

        WCHAR szwGamerTag[XONLINE_GAMERTAG_SIZE] = { 0 };

        XBUtil_GetWide( memberInfo.szGamertag, szwGamerTag, XONLINE_GAMERTAG_SIZE );

        m_font.DrawText( fTeammateNameStartX, fPosY, dwColor,
                         szwGamerTag,
                         XBFONT_LEFT );

        // create a string representing their privilege level
        if( memberInfo.TeamMemberProperties.dwPrivileges & XONLINE_TEAM_DELETE_MEMBER )
        {
            wcscpy( szwGamerTag, L"Owner" );

            // If we are the owner then we get to promote,
            // demote and kickoff players from the team
            if( CURRENT_USER.xuid.qwUserID == m_rwTeamMembers[i].qwUserID )
            {
                m_bDisableRosterOps = FALSE;
            }
        }
        else if( memberInfo.TeamMemberProperties.dwPrivileges & XONLINE_TEAM_RECRUIT_MEMBERS )
        {
            wcscpy( szwGamerTag, L"Recruiter" );

            if( CURRENT_USER.xuid.qwUserID == m_rwTeamMembers[i].qwUserID )
            {
                m_bDisableRosterOps = TRUE;
            }
        }
        else
        {
            wcscpy( szwGamerTag, L"Peon" );

            if( CURRENT_USER.xuid.qwUserID == m_rwTeamMembers[i].qwUserID )
            {
                m_bDisableRosterOps = TRUE;
            }
        }

        m_font.DrawText( SCREEN_CENTER_X, fPosY, dwColor,
                     szwGamerTag,
                     XBFONT_LEFT );

        // Show the date of when the member joined
        SYSTEMTIME systemTime;

        FileTimeToSystemTime( &memberInfo.JoinDate, &systemTime );

        _snwprintf( szwGamerTag, XONLINE_GAMERTAG_SIZE, L"%d/%d/%d\0",
            systemTime.wMonth, systemTime.wDay, systemTime.wYear );

        m_font.DrawText( fTeamDescStartX, fPosY, dwColor,
                         szwGamerTag,
                         XBFONT_RIGHT );
    }


    // Tell the player that they can change their buddy icon
    // or that they can view a  teammates icon depending
    // if they have themselves or a teammate selected

    if( m_rwTeamMembers[m_dwTeamMemberSelected].qwUserID
        == CURRENT_USER.xuid.qwUserID )
    {
        m_font.DrawText( POS_FOOTER_RIGHT, POS_FOOTER_Y,
                        COLOR_NORMAL, GLYPH_A_BUTTON L" Edit My Buddy Icon",
                        XBFONT_RIGHT );
    }
    else if ( !m_bDisableRosterOps )
    {
        // send message
        m_font.DrawText( SCREEN_CENTER_X , POS_FOOTER_Y,
                        COLOR_NORMAL, GLYPH_Y_BUTTON L" send message",
                        XBFONT_CENTER_X );

        if ( !m_bDisableRosterOps )
        {
            // select
            m_font.DrawText( POS_FOOTER_RIGHT, POS_FOOTER_Y,
                            COLOR_NORMAL, GLYPH_A_BUTTON L" select",
                            XBFONT_RIGHT );
        }
    }

    // Bottom Help text
    RenderFooter( m_font, FOOTER_RENDER_CANCEL );
}

/////////////////////////
// State TeamMemberOps //
/////////////////////////

//-------------------------------------------------------------------------------------
// Name: UpdateStateTeamMemberOps
// Desc: Allows the user to change permissions or remove a member from the team
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateTeamMemberOps( DWORD dwControllerPort, Event event )
{
    m_rwLocalUsers[dwControllerPort].m_iCurSelection = 
        GetMenuPosition( m_rwLocalUsers[dwControllerPort].m_iCurSelection,
                         NUM_ITEMS_MEMBER_OPS_MENU, event );

    if( dwControllerPort != (INT)m_wControllingUser ) return;

    XUID    xuidTeamMember     = m_rwTeamMembers[m_dwTeamMemberSelected];
    XUID    xuidTeam           = m_rwTeamXUIDS[m_iTeamSelected];
    HRESULT hrPermissionChange = S_OK;

    switch( event )
    {
    case EV_BUTTON_A:
        switch( m_rwLocalUsers[dwControllerPort].m_iCurSelection )
        {
        case MENU_MEMBER_OPS_SET_OWNER:
            hrPermissionChange = SetPermissions( dwControllerPort, xuidTeam, xuidTeamMember,
                            ( XONLINE_TEAM_DELETE
                            | XONLINE_TEAM_MODIFY_DATA
                            | XONLINE_TEAM_MODIFY_MEMBER_PERMISSIONS
                            | XONLINE_TEAM_DELETE_MEMBER
                            | XONLINE_TEAM_RECRUIT_MEMBERS ) );
            break;

        case MENU_MEMBER_OPS_SET_RECRUITER:
            hrPermissionChange = SetPermissions( dwControllerPort, xuidTeam, xuidTeamMember,
                                                 XONLINE_TEAM_RECRUIT_MEMBERS );
            break;

        case MENU_MEMBER_OPS_SET_PEON:
            hrPermissionChange = SetPermissions( dwControllerPort, xuidTeam, 
                                                 xuidTeamMember, 0 );
            break;

        case MENU_MEMBER_OPS_KICKOFF:
            if( SUCCEEDED( RemoveTeamMember( dwControllerPort, xuidTeam, xuidTeamMember ) ) )
            {
                GetTeamRoster( dwControllerPort, m_phTeamRosterTask,
                               m_rwTeamXUIDS[m_iTeamSelected],
                               m_rwTeamMembers, m_dwTeamMemberCount );
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
            GetTeamRoster( dwControllerPort, m_phTeamRosterTask,
                           m_rwTeamXUIDS[m_iTeamSelected],
                           m_rwTeamMembers, m_dwTeamMemberCount );
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

    if( FAILED( hrMemberDetail ) )
        return;

    WCHAR strUserName[XONLINE_GAMERTAG_SIZE];
    XBUtil_GetWide( memberInfo.szGamertag,
                    strUserName, XONLINE_GAMERTAG_SIZE );

    WCHAR strMenuTitle[XONLINE_GAMERTAG_SIZE + 22];
    wsprintfW( strMenuTitle, L"TEAM MEMBER OPERATIONS for %s", strUserName );

    RenderMenu( m_font,
                strMenuTitle,
                (const WCHAR**)MENU_MEMBER_OPS,
                NUM_ITEMS_MEMBER_OPS_MENU,
                m_rwLocalUsers[m_wControllingUser].m_iCurSelection );

    RenderFooter( m_font, FOOTER_RENDER_SELECT | FOOTER_RENDER_CANCEL );
}


///////////////////////
// State ContentEdit //
///////////////////////

//-------------------------------------------------------------------------------------
// Name: EnterStateContentEdit()
// Desc: Intializes the content editing system
//-------------------------------------------------------------------------------------
BOOL CXBoxSample::EnterStateContentEdit()
{
    m_iTurtleX            = 0;
    m_iTurtleY            = 0;
    m_dwTurtleColor       = COLOR_NORMAL;

    if( !m_lpPreviewTexture )
    {
        m_lpPreviewTexture = m_userContent.CreateTexture( m_pd3dDevice );

        assert( m_lpPreviewTexture );
    }

    m_turtleFlashTimer.StartZero();

    return TRUE;
}

//-------------------------------------------------------------------------------------
// Name: UpdateStateContentEdit
// Desc: Allows the user to edit and save (or discard) content
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateContentEdit( DWORD dwControllerPort, Event event )
{
    const FLOAT fTurtleBlinkInterval = 0.5f;

    m_userContent.UpdateTexture( m_lpPreviewTexture );

    // Flip the color of the turtle to
    // make a blinking effect
    if( m_turtleFlashTimer.GetElapsedSeconds() > fTurtleBlinkInterval )
    {
        m_dwTurtleColor = ( m_dwTurtleColor == COLOR_NORMAL ) ? COLOR_CLEAR : COLOR_NORMAL;

        m_turtleFlashTimer.StartZero();
    }


    // Move the cursor / turtle around
    switch( event )
    {
    case EV_UP:
        // Move the turtle up and wrap to the bottom when we go past the top
        --m_iTurtleY;

        m_iTurtleY = ( m_iTurtleY < 0 ) ? ( m_userContent.GetSize() - 1 ) : m_iTurtleY;
        break;

    case EV_DOWN:
        // Move the turtle down and wrap to the top when go past the bottom
        ++m_iTurtleY;

        m_iTurtleY = ( m_iTurtleY >= m_userContent.GetSize() ) ? 0 : m_iTurtleY;
        break;

    case EV_LEFT:
        // Move the turtle left and wrap to the right side when we go past the border
        --m_iTurtleX;

        m_iTurtleX = ( m_iTurtleX < 0 ) ? ( m_userContent.GetSize() - 1 ) : m_iTurtleX;
        break;

    case EV_RIGHT:
        // Move the turtle right and wrap to the left side when we go past the border
        ++m_iTurtleX;

        m_iTurtleX = ( m_iTurtleX >= m_userContent.GetSize() ) ? 0 : m_iTurtleX;
        break;
    }

    // Paint the scene with user input!
    switch( event )
    {
    case EV_BUTTON_START:
        m_userContent.SetDirty( FALSE );

        {
            // if there doesn't exist a filename, pass the user's ID, otherwise, pass
            // the team's ID to the upload content method
            ULONGLONG qwID = ( m_bTeamLogo ? 0
                                : CURRENT_USER.xuid.qwUserID );

            WCHAR* pFilename = m_bTeamLogo ? TEAM_LOGO_FILENAME : NULL;

            // Save the content in memory to the HD
            if( m_userContent.Upload( dwControllerPort,
                                      qwID,
                                      m_rwTeamXUIDS[m_iTeamSelected].qwTeamID,
                                      pFilename ) )
            {
                PushMessageWindow( "Uploaded progress" );
            }
            else
            {
                PushMessageWindow( "Unable to upload progress" );
            }
        }
        break;

    case EV_BUTTON_BACK:
        m_wszFilename = NULL;

        PopState( TRUE );

        if( m_userContent.IsDirty() )
        {
            PushMessageWindow( "Throwing out progress." );
        }

        break;

    case EV_BUTTON_A:
    case EV_BUTTON_B:
    case EV_BUTTON_X:
    case EV_BUTTON_Y:
    case EV_BUTTON_BLACK:
    case EV_BUTTON_WHITE:
        // Set the color of the block the cursor is on
        // to the color mapped to the controller button
        m_userContent.SetColor( m_iTurtleX, m_iTurtleY,
                                        m_rwButtonColorMap[(INT)event] );
        break;
    }
}

//-------------------------------------------------------------------------------------
// Name: RenderStateContentEdit()
// Desc: Renders the user content to the screen and shows the user
//       their input options.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::RenderStateContentEdit()
{
    RenderControllingUser();

    if( m_lpPreviewTexture )
    {
        float fPreviewPosX = 96.0f;
        float fPreviewPosY = 160.0f;

        m_font.DrawText( ( fPreviewPosX + ( ICON_SIZE * .5f ) ), ( fPreviewPosY - ICON_SIZE ),
                         COLOR_NORMAL,
                         L"Preview",
                         XBFONT_CENTER_X );

        SetFacePos( m_pLogoVerts, fPreviewPosX, fPreviewPosY );
        RenderSprite( m_pd3dDevice, m_pLogoVerts, m_lpPreviewTexture );
    }

    // RenderControllingUser();
    RenderMenu( m_font, L"EDIT CONTENT", NULL, 0, 0 );

    m_font.SetScaleFactors( 0.5f, 0.5f );

    FLOAT fTextWidth  = m_font.GetTextWidth( GLYPH_FILLED_CIRCLE );
    FLOAT fTextHeight = m_font.GetFontHeight() * 0.5f;

    FLOAT fStartX = SCREEN_CENTER_X - ( 0.5f * fTextWidth * m_userContent.GetSize() );
    FLOAT fStartY = SCREEN_CENTER_Y - ( 0.5f * fTextHeight * m_userContent.GetSize() );

    FLOAT fPosX = fStartX;
    FLOAT fPosY = fStartY;

    // Draw the lightbrite text
    for( INT iY = 0; iY < m_userContent.GetSize(); ++iY )
    {
        fPosX = fStartX;

        for( INT iX = 0; iX < m_userContent.GetSize(); ++iX )
        {
            m_font.DrawText( fPosX, fPosY,
                             m_userContent.GetColor( iX, iY ),
                             GLYPH_FILLED_CIRCLE,
                             XBFONT_CENTER_X );

            fPosX += fTextWidth;
        }

        fPosY += fTextHeight;
    }

    // Draw the blinking turtle
    FLOAT fTurtlePosX = fStartX + ( m_iTurtleX * fTextWidth );
    FLOAT fTurtlePosY = fStartY + ( m_iTurtleY * fTextHeight );

    m_font.DrawText( fTurtlePosX, fTurtlePosY,
                     m_dwTurtleColor,
                     GLYPH_HAND,
                     XBFONT_CENTER_X );

    m_font.SetScaleFactors( 1.0f, 1.0f );

    // Draw the instructions in the footer

    FLOAT fFooterStartY = POS_FOOTER_Y - DEFAULT_TEXT_PADDING;

    // Tell the user how to set the color

    m_font.DrawText( POS_HEADER_LEFT, fFooterStartY,
                     COLOR_NORMAL,
                     GLYPH_A_BUTTON GLYPH_B_BUTTON GLYPH_X_BUTTON GLYPH_Y_BUTTON GLYPH_WHITE_BUTTON GLYPH_BLACK_BUTTON,
                     XBFONT_LEFT );

    m_font.DrawText( POS_HEADER_LEFT, POS_FOOTER_Y,
                     COLOR_NORMAL,
                     L"SET PIXEL COLOR",
                     XBFONT_LEFT );

    // Tell the user how to move

    m_font.DrawText( POS_HEADER_RIGHT, fFooterStartY,
                     COLOR_NORMAL,
                     GLYPH_UP_ARROW GLYPH_DOWN_ARROW GLYPH_LEFT_ARROW GLYPH_RIGHT_ARROW,
                     XBFONT_RIGHT );

    m_font.DrawText( POS_HEADER_RIGHT, POS_FOOTER_Y,
                     COLOR_NORMAL,
                     L"MOVE TURTLE",
                     XBFONT_RIGHT );

    // How to save
    m_font.DrawText( SCREEN_CENTER_X, fFooterStartY,
                     COLOR_NORMAL,
                     GLYPH_START1_BUTTON GLYPH_START2_BUTTON,
                     XBFONT_RIGHT );

    m_font.DrawText( SCREEN_CENTER_X, POS_FOOTER_Y,
                     COLOR_NORMAL,
                     L"SAVE ",
                     XBFONT_RIGHT );

    // How to exit
    m_font.DrawText( SCREEN_CENTER_X, fFooterStartY,
                     COLOR_NORMAL,
                     GLYPH_BACK1_BUTTON GLYPH_BACK2_BUTTON,
                     XBFONT_LEFT );

    m_font.DrawText( SCREEN_CENTER_X, POS_FOOTER_Y,
                     COLOR_NORMAL,
                     L" EXIT",
                     XBFONT_LEFT );
}


////////////////////////
// State ListTourneys //
////////////////////////

//-------------------------------------------------------------------------------------
// Name: UpdateStateListTourneys()
// Desc: Allows the user to select from a list of competitions they have joined
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateListTourneys( DWORD dwControllerPort, Event event )
{
    if( dwControllerPort != m_wControllingUser )
        return;

    m_dwTourneySelected = GetMenuPosition(
                                m_dwTourneySelected,
                                event,
                                m_dwTourneyRenderStart,
                                g_joinedCompQuery.dwItemsReturned,
                                NUM_ENTRIES_PER_SCREEN
                            );

    switch( event )
    {
    // Return to the previous menu
    case EV_BUTTON_B:
        PopState( TRUE );
        break;

    // Time warp the competition forward
    case EV_BUTTON_X:
        TimeWarp( dwControllerPort,
                  g_joinedCompQuery.Results[m_dwTourneySelected].att_comp_id );

        // Small pause so the tourney search will work upon re-entry
        Sleep( 15 );

        PopState( TRUE );
        PushMessageWindow( "Time warped competition forward." );

        break;

    // Move to view the topography or competitor list
    case EV_BUTTON_A:
        {
            assert( g_joinedCompQuery.dwItemsReturned > 0 );

            ZeroMemory( g_rwTopology, sizeof( g_rwTopology ) );
            m_dwTopologyCount = 0;

            HRESULT hrGetEntrants = GetCompetitionEntrants(
                                    g_joinedCompQuery.Results[m_dwTourneySelected].att_comp_id,
                                    g_entrantsQuery
                                    );

            assert( SUCCEEDED( hrGetEntrants ) );

            m_qwCompetitionStatus = GetTournamentTopography(
                                    g_joinedCompQuery.Results[m_dwTourneySelected],
                                    g_rwTopology,
                                    m_dwTopologyCount
                                    );

            PushState( STATE_TOURNEY_RENDER );
        }
    }
}

//-----------------------------------------------------------------------------
// Name: RenderStateListTourney()
// Desc: Shows a list of competitions the team has joined.
//-----------------------------------------------------------------------------
VOID CXBoxSample::RenderStateListTourneys()
{
    RenderControllingUser();

    MENU_LIST rwNameList  = { 0 };
    MENU_LIST rwStateList = { 0 };

    for( DWORD i = m_dwTourneyRenderStart;
         ( ( i < g_joinedCompQuery.dwItemsReturned ) && 
           ( i < ( m_dwTourneyRenderStart + NUM_ENTRIES_PER_SCREEN ) ) );
         ++i )
    {
        // Place all the data in the array starting with
        // index 0. This keeps us from having to keep
        // a monster sized array for a 1-to-1 correspondence
        INT iDataIndex = i - m_dwTourneyRenderStart;

        INT iStringLength = wcslen( g_joinedCompQuery.Results[i].att_comp_name );

        if( iStringLength )
        {
            _snwprintf( rwNameList[iDataIndex], ATT_COMP_NAME_SIZE - 1,
                        L"%s\0",
                        g_joinedCompQuery.Results[i].att_comp_name );
        }
        else
        {
            // Construct the name
            UINT iHi = (UINT)( g_joinedCompQuery.Results[i].att_comp_id >> 32 );
            UINT iLo = (UINT)g_joinedCompQuery.Results[i].att_comp_id;

            _snwprintf( rwNameList[iDataIndex], MAX_MENU_STRING_SIZE,
                        L"%08x%08x",
                        iHi, iLo );
        }

        // The XLAST generated query code saved the results in a local buffer
        // All we have to do is iterate over the results member of the query class instance
        switch( g_joinedCompQuery.Results[i].att_comp_status )
        {
        case XONLINE_COMP_STATUS_ACTIVE:
            lstrcpynW( rwStateList[iDataIndex], L"ACTIVE", MAX_MENU_STRING_SIZE );
            break;

        case XONLINE_COMP_STATUS_PRE_INIT:
            lstrcpynW( rwStateList[iDataIndex], L"REGISTERING", MAX_MENU_STRING_SIZE );
            break;

        case XONLINE_COMP_STATUS_COMPLETE:
            lstrcpynW( rwStateList[iDataIndex], L"COMPLETE", MAX_MENU_STRING_SIZE );
            break;

        case XONLINE_COMP_STATUS_CANCELED:
            lstrcpynW( rwStateList[iDataIndex], L"CANCELED", MAX_MENU_STRING_SIZE );
            break;

        default:
            assert( 0 && "Unknown competition status!" );
        }
    }

    RenderScrollingMenu( m_font,
                         L"JOINED COMPETITIONS",
                         m_dwTourneyRenderStart,
                         m_dwTourneySelected,
                         g_joinedCompQuery.dwItemsReturned,
                         L"NAME",
                         rwNameList,
                         L"STATUS",
                         rwStateList,
                         TRUE );


    m_font.DrawText( SCREEN_CENTER_X, POS_FOOTER_Y,
                     COLOR_NORMAL,
                     GLYPH_X_BUTTON L"Time Warp Forward",
                     XBFONT_CENTER_X );

    // Bottom Help text
    RenderFooter( m_font, FOOTER_RENDER_CANCEL | FOOTER_RENDER_SELECT );
}

/////////////////////////
// State TourneyRender //
/////////////////////////

//-----------------------------------------------------------------------------
// Name: EnterStateTourneyRender()
// Desc: Initializes the screen that shows the competition topography
//       and the list of competitors in the competition.
//-----------------------------------------------------------------------------
BOOL CXBoxSample::EnterStateTourneyRender()
{
    m_dwCompetitorRenderStart = 0;
    m_dwCompetitorSelected    = 0;
    m_bRenderCompetitors      = FALSE;

    return TRUE;
}

//-----------------------------------------------------------------------------
// Name: UpdateStateTourneyRender()
// Desc: Renders the competition to the screen in a tree format
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateTourneyRender( DWORD dwControllerPort, Event event )
{
    if( dwControllerPort != m_wControllingUser )
        return;

    // Determine if the user can switch between the competitor list
    // and the topography. If the competition is still registering
    // then no topography has been determined yet.
    BOOL bNotSwitchable = ( ( m_qwCompetitionStatus == 
                              (ULONGLONG)XONLINE_COMP_STATUS_CANCELED )
                            || ( m_qwCompetitionStatus == 
                              (ULONGLONG)XONLINE_COMP_STATUS_PRE_INIT ) );

    if( bNotSwitchable )
    {
        m_bRenderCompetitors = TRUE;
    }

    // Render the list of competitors
    if( m_bRenderCompetitors )
    {
        m_dwCompetitorSelected = GetMenuPosition(
                                    m_dwCompetitorSelected,
                                    event,
                                    m_dwCompetitorRenderStart,
                                    g_entrantsQuery.dwItemsReturned,
                                    NUM_ENTRIES_PER_SCREEN );

    }

    ULONGLONG qwCompID   = g_joinedCompQuery.Results[m_dwTourneySelected].att_comp_id;
    ULONGLONG qwTeamID   = m_rwTeamXUIDS[m_iTeamSelected].qwTeamID;
    ULONGLONG qwEventID  = g_joinedCompQuery.Results[m_dwTourneySelected].att_current_event;
    ULONGLONG qwEntityID = 0;

    ULONGLONG qwExpectedOpponent = GetRoundOpponent(
                                        g_rwTopology,
                                        m_dwTopologyCount,
                                        qwTeamID,
                                        m_dwEventRound,
                                        qwEntityID
                                    );

    switch( event )
    {
    // Toggle the screen (if we can)
    case EV_BUTTON_BLACK:
        if( !bNotSwitchable )
        {
            m_bRenderCompetitors = !m_bRenderCompetitors;
        }

        break;

    case EV_BUTTON_B:
        // Return to the list
        PopState();
        break;

    case EV_BUTTON_X:
        // Eject the selected entrant from the competition if the team is not us,
        // and the team exists
        if( m_bRenderCompetitors
            && ( g_entrantsQuery.Results[m_dwCompetitorSelected].att_puid != 
                 m_rwTeamXUIDS[m_iTeamSelected].qwTeamID )
            && ( ( m_qwCompetitionStatus == XONLINE_COMP_STATUS_PRE_INIT ) || 
                 ( m_qwCompetitionStatus == XONLINE_COMP_STATUS_ACTIVE ) )
            && wcslen( g_entrantsQuery.Results[m_dwCompetitorSelected].gamertag ) )
        {
            if( g_entrantsQuery.Results[m_dwCompetitorSelected].att_status != 
                COMP_ENTRANT_STATUS_EJECTED )
            {
                HRESULT hrEject = EjectEntrantFromCompetition(
                                  dwControllerPort,
                                  m_rwTeamXUIDS[m_iTeamSelected].qwTeamID,
                                  g_entrantsQuery.Results[m_dwCompetitorSelected].att_puid,
                                  qwCompID,
                                  qwEventID
                                  );

                PopState();

                PushMessageWindow( SUCCEEDED( hrEject )
                    ? "Entrant ejected from competition!"
                    : "Unable to eject entrant from competition." );

                return;
            }
        }
        // Attempt to cancel the competition. If that fails,
        // then just withdraw
        else if( m_qwCompetitionStatus == XONLINE_COMP_STATUS_PRE_INIT )
        {
            if( SUCCEEDED( CancelCompetition( 
                           dwControllerPort,
                           m_rwTeamXUIDS[m_iTeamSelected],
                           g_joinedCompQuery.Results[m_dwTourneySelected].att_comp_id 
                           ) ) )
            {
                // Update the list of competitions we are in
                TournamentSearch( dwControllerPort,
                                  m_rwTeamXUIDS[m_iTeamSelected],
                                  g_joinedCompQuery );

                PopState( TRUE );
                PushMessageWindow( "Canceled competition" );

                return;
            }
            else if( SUCCEEDED( WithdrawlFromCompetition( 
                                dwControllerPort,
                                m_rwTeamXUIDS[m_iTeamSelected],
                                g_joinedCompQuery.Results[m_dwTourneySelected].att_comp_id ) 
                                ) )
            {
                // Update the list of competitions we are in
                TournamentSearch( dwControllerPort,
                                  m_rwTeamXUIDS[m_iTeamSelected],
                                  g_joinedCompQuery );

                PopState( TRUE );
                PushMessageWindow( "Withdrew from competition!" );

                return;
            }
            else
            {
                PushMessageWindow( "Unable to cancel or withdraw from competition" );
            }
        }
        // Forfeit this round to the competitor
        else if( (! m_bRenderCompetitors )
            && ( qwExpectedOpponent != 0x0 )
            && ( g_joinedCompQuery.Results[m_dwTourneySelected].att_status == 
                 XONLINE_COMP_STATUS_ACTIVE ) )
        {
            HRESULT hrForfeit = ForfeitCompetition(
                                        dwControllerPort,
                                        qwTeamID,
                                        qwCompID,
                                        qwEventID
                                    );

            PopState();
            PushMessageWindow( SUCCEEDED( hrForfeit ) ?
                               "You have forfeited the round"
                               : "Unable to forfeit" );
        }
        break;

    case EV_BUTTON_A:
        // View the roster of the selected team
        if( m_bRenderCompetitors )
        {
            XUID xuidTeam;
            xuidTeam.dwUserFlags = 0;
            xuidTeam.qwTeamID    = g_entrantsQuery.Results[m_dwCompetitorSelected].att_puid;

            if( GetTeamRoster( dwControllerPort,
                               m_phTeamRosterTask,
                               xuidTeam,
                               m_rwTeamMembers,
                               m_dwTeamMemberCount ) )
            {
                m_bDisableRosterOps = TRUE;

                PushState( STATE_VIEW_TEAM_ROSTER );

                return;
            }
        }
        // Try to join an existing session for this
        // round. If a session is not found,
        // attempt to create a session that others
        // can join.
        //
        // NOTE:
        // A real title should check the current
        // time against the round start time.
        // If the current time is before the round
        // start time then the user should
        // not be allowed to join or create a session.
        else if( qwExpectedOpponent != 0x0 )
        {
            // Initialize the network sockets and start the checkin procedure
            // if the competition is active.
            if( g_joinedCompQuery.Results[m_dwTourneySelected].att_status == 
                XONLINE_COMP_STATUS_ACTIVE )
            {
                // Check to make sure that all
                // the local users are on the
                // team that will be checking
                // into the competition
                if( !AllLocalUsersAreOnSameTeam() )
                {
                    PopState();
                    PushMessageWindow( "Not all logged on users are members of this team." );

                    return;
                }

                HRESULT hrInit = InitXNet();

                if( FAILED( hrInit ) )
                {
                    XBUtil_DebugPrint( "InitXNet failed (error 0x%x)\n", hrInit );

                    assert( FALSE );
                }

                // Try to find the competition first
                HRESULT hrFind = FindCompetitionSession(
                                    dwControllerPort,
                                    g_joinedCompQuery.Results[m_dwTourneySelected].att_comp_id,
                                    g_rwTopology[m_dwEventRound].bi_entity_id,
                                    m_inHostAddr
                                );

                XBUtil_DebugPrint( "FindCompetitionSession: 0x%x\n", hrFind );

                // Found a session, lets checkin to the
                // competition
                if( SUCCEEDED( hrFind ) )
                {

                    // Establish a connection with the session host
                    XBUtil_DebugPrint( "Found session, establishing connection\n" );

                    SOCKADDR_IN sa;
                    sa.sin_family = AF_INET;
                    sa.sin_addr   = m_inHostAddr;
                    sa.sin_port   = htons( DIRECT_PORT );

                    INT iConnect = m_DirectSock.Connect( &sa );
                    assert( !iConnect );

                    m_GameJoinTimer.StartZero();

                    m_bTourneySession = TRUE;

                    ULONGLONG qwCheckinCompID  = 
                        g_joinedCompQuery.Results[m_dwTourneySelected].att_comp_id;

                    ULONGLONG qwCheckinTeamID  = 
                        m_rwTeamXUIDS[m_iTeamSelected].qwTeamID;

                    ULONGLONG qwCheckinEventID = 
                        g_joinedCompQuery.Results[m_dwTourneySelected].att_current_event;

                    CheckinCompetition( dwControllerPort,
                                        qwCheckinTeamID,
                                        qwCheckinCompID,
                                        qwCheckinEventID );

                    // Ask to join the game session
                    SendJoinGame( sa );
                }
                // Check to make sure that the NAT that the user
                // is going through is open. Hosting a sesssion with
                // a "moderate" or "strict" NAT will keep other players
                // with similar NATs from joining the round.
                else if( IsOpenNAT() )
                {
                    

                    // Unable to find a game session,
                    // so lets create one.
                    // NOTE: A real title will want
                    // to implement host migration.
                    HRESULT hrCreate = CreateCompetitionSession(
                                dwControllerPort,
                                m_rwTeamXUIDS[m_iTeamSelected].qwTeamID,
                                g_joinedCompQuery.Results[m_dwTourneySelected].att_comp_id,
                                g_rwTopology[m_dwEventRound].bi_entity_id
                                );

                    if( SUCCEEDED( hrCreate ) )
                    {
                        // Checkin to the competition
                        // so clients can find and join
                        // our session
                        m_dwSlotsInUse      = GetNumLoggedOnUsers();
                        m_bIsHost           = TRUE;
                        m_bTourneySession   = TRUE;

                        ULONGLONG qwCheckinCompID  = 
                            g_joinedCompQuery.Results[m_dwTourneySelected].att_comp_id;

                        ULONGLONG qwCheckinTeamID  = 
                            m_rwTeamXUIDS[m_iTeamSelected].qwTeamID;

                        ULONGLONG qwCheckinEventID = 
                            g_joinedCompQuery.Results[m_dwTourneySelected].att_current_event;

                        HRESULT   hrCheckin = CheckinCompetition(
                                                dwControllerPort,
                                                qwCheckinTeamID,
                                                qwCheckinCompID,
                                                qwCheckinEventID
                                            );

                        if( FAILED( hrCheckin ) )
                        {
                            PushMessageWindow( hrCheckin == XONLINE_E_COMP_EVENT_SCORED
                                               ? "Round has already been played."
                                               : "Unable to checkin" );

                            return;
                        }

                        PopState();
                        PushState( STATE_GAME_LOBBY );
                    }
                    else
                    {
                        PushMessageWindow( "Unable to create or find match." );
                    }
                }
                // Could not find a session, but can not create one.
                else
                {
                    PopState();
                    // Please visit www.Xbox.com for details about restrictive NATs
                    PushMessageWindow( "Your NAT is restrictive. You may not host a session." );
                }
            }
        }
        break;

    // Send a message to the competing team
    case EV_BUTTON_Y:
        // if we're in the render competitors view, we've selected a team
        // that is not ourselves, and the team is not deleted
        if( m_bRenderCompetitors &&
            ( g_entrantsQuery.Results[m_dwCompetitorSelected].att_puid !=
              m_rwTeamXUIDS[m_iTeamSelected].qwTeamID ) && 
              wcslen( g_entrantsQuery.Results[m_dwCompetitorSelected].gamertag ) )
        {
            // sending to owners of team
            m_eMessageProtocol = VOICE_MAIL_SENT_TO_TEAM_OWNERS;

            // default start message
            m_iMessageSelected = 0;

            // the owners of the opposing team are the recipients
            m_qwProtocolParamId = 
                g_entrantsQuery.Results[m_dwCompetitorSelected].att_puid;

            PushState( STATE_TEAM_SEND_MESSAGE );
        }
        break;

    }
}

//-----------------------------------------------------------------------------
// Name: RenderStateTourneyRender()
// Desc: Renders the topography of the competition using a nifty tree format.
//-----------------------------------------------------------------------------
VOID CXBoxSample::RenderStateTourneyRender()
{
    // Render the list of competitors
    if( m_bRenderCompetitors )
    {
        RenderCompetitorList( m_font,
                              g_joinedCompQuery.Results[m_dwTourneySelected].att_comp_name,
                              g_entrantsQuery,
                              m_dwCompetitorRenderStart,
                              m_dwCompetitorSelected );

        // Show that we can try to eject the entrant if the team
        // selected is not our own, and it exists
        if( ( g_entrantsQuery.Results[m_dwCompetitorSelected].att_puid != 
              m_rwTeamXUIDS[m_iTeamSelected].qwTeamID )
            && ( ( m_qwCompetitionStatus == XONLINE_COMP_STATUS_PRE_INIT ) || 
                 ( m_qwCompetitionStatus == XONLINE_COMP_STATUS_ACTIVE ) )
            && wcslen( g_entrantsQuery.Results[m_dwCompetitorSelected].gamertag ) )
        {
            if( g_entrantsQuery.Results[m_dwCompetitorSelected].att_status != 
                COMP_ENTRANT_STATUS_EJECTED )
            {
                m_font.DrawText( SCREEN_CENTER_X, POS_FOOTER_Y - DEFAULT_TEXT_PADDING,
                                COLOR_NORMAL,
                                GLYPH_X_BUTTON L"EJECT ENTRANT",
                                XBFONT_CENTER_X );
            }
        }
        // Try to cancel or withdraw our team from the competition.
        else if( m_qwCompetitionStatus == XONLINE_COMP_STATUS_PRE_INIT )
        {
            m_font.DrawText( SCREEN_CENTER_X, POS_FOOTER_Y - ( DEFAULT_TEXT_PADDING * 2.0f ),
                             COLOR_NORMAL,
                             GLYPH_X_BUTTON L"WITHDRAW/CANCEL",
                             XBFONT_CENTER_X );
        }

        // if we're not selecting ourselves, and the team is not deleted
        if ( ( g_entrantsQuery.Results[m_dwCompetitorSelected].att_puid
             != m_rwTeamXUIDS[m_iTeamSelected].qwTeamID ) &&
             wcslen( g_entrantsQuery.Results[m_dwCompetitorSelected].gamertag ) )
        {
            // send message
            m_font.DrawText( SCREEN_CENTER_X , POS_FOOTER_Y,
                            COLOR_NORMAL, GLYPH_Y_BUTTON L" send message",
                            XBFONT_CENTER_X );
        }
    }
    else
    // Render the topography of the competition.
    {
        RenderTournament( m_font,
                          g_rwTopology,
                          m_dwTopologyCount,
                          m_rwTeamXUIDS[m_iTeamSelected].qwTeamID );

        ULONGLONG qwTeamID   = m_rwTeamXUIDS[m_iTeamSelected].qwTeamID;
        ULONGLONG qwEntityID = 0;

        ULONGLONG qwExpectedOpponent = GetRoundOpponent(
                                            g_rwTopology,
                                            m_dwTopologyCount,
                                            qwTeamID,
                                            m_dwEventRound,
                                            qwEntityID
                                        );

        if( qwExpectedOpponent != 0x0 )
        {
            if( g_joinedCompQuery.Results[m_dwTourneySelected].att_status == 
                XONLINE_COMP_STATUS_ACTIVE )
            {
                m_font.DrawText( SCREEN_CENTER_X, 
                                 POS_FOOTER_Y - DEFAULT_TEXT_PADDING,
                                 COLOR_NORMAL,
                                 GLYPH_A_BUTTON L"Join Competition Round",
                                 XBFONT_CENTER_X );

                m_font.DrawText( SCREEN_CENTER_X, 
                                 POS_FOOTER_Y - ( DEFAULT_TEXT_PADDING * 2.0f ),
                                 COLOR_NORMAL,
                                 GLYPH_X_BUTTON L"Forfeit Round",
                                 XBFONT_CENTER_X );
            }
        }
    }


    // Determine if we should draw the "toggle" help text
    BOOL bNotSwitchable = ( ( m_qwCompetitionStatus == 
                              (ULONGLONG)XONLINE_COMP_STATUS_CANCELED )
                          || ( m_qwCompetitionStatus == 
                              (ULONGLONG)XONLINE_COMP_STATUS_PRE_INIT ) );

    if( !bNotSwitchable )
    {
        m_font.DrawText( POS_FOOTER_RIGHT, ( POS_FOOTER_Y - DEFAULT_TEXT_PADDING ),
                         COLOR_NORMAL, GLYPH_BLACK_BUTTON L" toggle view",
                         XBFONT_RIGHT );
    }

    // Render the footer with the correct flags
    INT iFooterFlags = FOOTER_RENDER_CANCEL;

    if( m_bRenderCompetitors )
        iFooterFlags = iFooterFlags | FOOTER_RENDER_SELECT;

    RenderFooter( m_font, (WORD)iFooterFlags );
}


/////////////////////////
// State MessageWindow //
/////////////////////////

//-------------------------------------------------------------------------------------
// Name: UpdateStateMessageWindow
// Desc: Waits for the user to dismiss the message window
//       then returns to the calling state.
//-------------------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateMessageWindow( DWORD dwControllerPort, Event event )
{
    if( dwControllerPort != (INT)m_wControllingUser ) return;

    switch( event )
    {
    case EV_BUTTON_A:
    case EV_BUTTON_B:
        PopState( m_bReInitAfterWindowMessage );
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

    RenderFooter( m_font, FOOTER_RENDER_SELECT | FOOTER_RENDER_CANCEL );
}

////////////////////////////////
// End State Member Functions //
////////////////////////////////


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
                     L"Tsunami Integrated Sample",
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

    if( !m_bUsersSignedIn )
        return;

    WCHAR strUserName[XONLINE_GAMERTAG_SIZE];
    XBUtil_GetWide( CURRENT_USER.szGamertag,
                    strUserName, XONLINE_GAMERTAG_SIZE );

    // Render a header giving the user the name of the demo
    m_font.DrawText( POS_HEADER_RIGHT, POS_HEADER_Y, COLOR_NORMAL,
                     strUserName, // The user who has control of the menu
                     XBFONT_RIGHT );
}

//////////////////////////////////////
// Helpers to send network messages //
//////////////////////////////////////

//-----------------------------------------------------------------------------
// Name: SendJoinGame()
// Desc: Issue a MSG_JOIN_GAME from our client to the game host
//-----------------------------------------------------------------------------
VOID CXBoxSample::SendJoinGame( const SOCKADDR_IN& saGameHost )
{
    assert( !m_bIsHost );

    // Form the message packet
    Message msgJoinGame( MSG_JOIN_GAME );
    MsgJoinGame& msg = msgJoinGame.GetJoinGame();

    // Include our player name
    msg.dwNumPlayers = 0;
    for( DWORD i = 0; i < XGetPortCount(); i++ )
    {
        if( m_rwLocalUsers[i].m_bSignedIn )
        {
            strncpy( msg.strGamertags[msg.dwNumPlayers],
                     m_rwStoredUsers[m_rwLocalUsers[i].m_wUserIndex].szGamertag,
                     XONLINE_MAX_GAMERTAG_LENGTH );

            msg.xuids[msg.dwNumPlayers] = m_rwStoredUsers[m_rwLocalUsers[i].m_wUserIndex].xuid;
            ++msg.dwNumPlayers;
        }
    }

    // Send join game message reliably to the host
    INT nBytes = SendMessage( &msgJoinGame, &saGameHost );

    // This assert was removed because Send no longer is guaranteed to always work
    // If the security association times out, the number of bytes returned will
    // NOT be equal to the size of the message.  A good thing to do here would
    // be to drop the player
    assert( nBytes != SOCKET_ERROR );
}

//-----------------------------------------------------------------------------
// Name: SendJoinApproved()
// Desc: Issue a MSG_JOIN_APPROVED from our host to the requesting client.
//-----------------------------------------------------------------------------
VOID CXBoxSample::SendJoinApproved( const SOCKADDR_IN& saClient )
{
    assert( m_bIsHost );

    // Form the message packet
    Message msgJoinApproved( MSG_JOIN_APPROVED );
    MsgJoinApproved& msg = msgJoinApproved.GetJoinApproved();

    // The host is us
    msg.dwNumHostPlayers = 0;
    for( DWORD i = 0; i < XGetPortCount(); i++ )
    {
        if( !m_rwLocalUsers[i].m_bSignedIn )
            continue;

        strncpy( msg.strHostGamertags[msg.dwNumHostPlayers],
                 m_rwStoredUsers[m_rwLocalUsers[i].m_wUserIndex].szGamertag,
                 XONLINE_MAX_GAMERTAG_LENGTH );

        msg.xuids[msg.dwNumHostPlayers] = m_rwStoredUsers[m_rwLocalUsers[i].m_wUserIndex].xuid;
        msg.dwNumHostPlayers++;
    }

    // Send the list of all the current players to the new player.
    // We don't send the host player info, since the new player
    // already has all of the information it needs about the host player.
    msg.byNumPlayers = BYTE( m_rwPlayers.size() );
    BYTE j = 0;
    for( PlayerList::const_iterator i = m_rwPlayers.begin();
         i != m_rwPlayers.end(); ++i, ++j )
    {
        PlayerInfo playerInfo = *i;
        CopyMemory( &msg.PlayerList[j].xnAddr, &playerInfo.xnAddr,
                    sizeof( XNADDR ) );
        strncpy( msg.PlayerList[j].strGamertag,
                   playerInfo.strGamertag, XONLINE_MAX_GAMERTAG_LENGTH );

        msg.PlayerList[j].xuid = playerInfo.xuid;

    }

    // Send the join approved message reliably to the client
    SendMessage( &msgJoinApproved, &saClient );
}

//-----------------------------------------------------------------------------
// Name: SendJoinDenied()
// Desc: Issue a MSG_JOIN_DENIED from our host to the requesting client
//-----------------------------------------------------------------------------
VOID CXBoxSample::SendJoinDenied( const SOCKADDR_IN& saClient )
{
    assert( m_bIsHost );
    Message msgJoinDenied( MSG_JOIN_DENIED );

    // Send join denied message reliably back to the client
    SendMessage( &msgJoinDenied, &saClient );
}

//-----------------------------------------------------------------------------
// Name: SendPlayerJoinedToAll()
// Desc: Issue a MSG_PLAYER_JOINED from our host to each player in the game
//-----------------------------------------------------------------------------
VOID CXBoxSample::SendPlayerJoinedToAll( const Player& player )
{
    if( !m_bIsHost )
    {
        XBUtil_DebugPrint( "Attempting to send player joined to all when not host!" );
    }

    // Form the message packet
    Message msgPlayerJoined( MSG_PLAYER_JOINED );
    MsgPlayerJoined& msg = msgPlayerJoined.GetPlayerJoined();

    // The payload is the information about the player who just joined
    CopyMemory( &msg.player, &player, sizeof(player) );

    // Send the player joined message reliably to all players in the game
    SendMessage( &msgPlayerJoined );
}

//-----------------------------------------------------------------------------
// Name: SendStartArbitrationRegistration()
// Desc: Sends a message from the host to all clients instructing them
//       to register with the arbitration service and to start the game.
//-----------------------------------------------------------------------------
BOOL CXBoxSample::SendStartArbitrationRegistration()
{
    if( m_rwPlayers.size() == 0 )
    {
        return FALSE;
    }

    // At this point we can no longer let people join the game.
    m_bArbitrationStarted = TRUE;

    if( !m_bTourneySession )
    {
        // Update the session info in
        // the matchmaking service
        UpdateSession( m_hostedSession,
                       m_dwSlotsInUse,
                       m_bArbitrationStarted );
    }

    // The arbitration ID is created by the host, and then given to the other players.
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
    m_arbID.SessionID = ( m_bTourneySession ? m_xnHostKeyID : m_hostedSession.SessionID );

    // Create the round and provide a variable to write the new ID to
    HRESULT hrCreateRound = XOnlineArbitrationCreateRoundID( &m_arbID.qwRoundID );

    if( FAILED( hrCreateRound ) )
    {
        assert( 0 && "Failed to create round!" );

        return FALSE;
    }


    // Prior to starting the arbitration process the host should
    // share XNADDRs of all players with all players, give all players the
    // round id, and ask all Xboxes to register with arbitration.
    // The sample just sends the round ID.
    // The clients should all reply once they have registered.
    Message msgStartArb( MSG_ARB_ID );
    MsgArbID& msg = msgStartArb.GetArbID();

    // The payload is the information about the player who just joined
    CopyMemory( &msg.arbID, &m_arbID, sizeof( m_arbID ) );

    // Send the player joined message reliably to all players in the game
    INT nBytes = SendMessage( &msgStartArb );

    // This assert was removed because Send no longer is guaranteed to always work
    // If the security association times out, the number of bytes returned will
    // NOT be equal to the size of the message.  A good thing to do here would
    // be to drop the player

    assert( nBytes != SOCKET_ERROR );

    // Wait for players to register, or for a reasonable time-out (5-10 seconds)
    // to expire. If any players failed to register, disconnect them.
    m_regTimer.StartZero();
    m_dwPlayersRegistered = 0;


    return TRUE;
}

//-----------------------------------------------------------------------------
// Name: SendScore()
// Desc: Sends a message to all peers that the user on the given controller
//       port has scored a point.
//-----------------------------------------------------------------------------
VOID CXBoxSample::SendScore( DWORD dwControllerPort )
{
    // Wait for the host to message this back to us!
    // Add to our own local score
    if( m_bIsHost )
    {
        XONLINE_USER& user = m_rwStoredUsers[m_rwLocalUsers[dwControllerPort].m_wUserIndex];
        ProcessScore( user.xuid.qwUserID );
    }

    // Since we are a client/peer we need to
    // let the host know that we registered.
    Message msgScore( MSG_SCORE );
    MsgScore& msg = msgScore.GetScore();

    // Get the ID of the player claiming they scored
    msg.qwID = m_rwStoredUsers[m_rwLocalUsers[dwControllerPort].m_wUserIndex].xuid.qwUserID;

    // If we are the host then tell all the clients
    // that the score has been processed
    if( m_bIsHost )
    {
        SendMessage( &msgScore );
    }
    // If we are a client, tell the host that
    // we have scored
    else
    {
        SOCKADDR_IN sa;

        sa.sin_family = AF_INET;
        sa.sin_addr   = m_inHostAddr;
        sa.sin_port   = htons( DIRECT_PORT );

        SendMessage( &msgScore, &sa );
    }
}

//-----------------------------------------------------------------------------
// Name: SendGameOver()
// Desc: Sends a message from the host to all clients that the round is
//       over and to submit their arbitration results.
//-----------------------------------------------------------------------------
VOID CXBoxSample::SendGameOver()
{
    assert( m_bIsHost );

    Message msgGameOver( MSG_GAME_OVER );

    INT nBytes = SendMessage( &msgGameOver );

    assert( nBytes != SOCKET_ERROR );
}

//-----------------------------------------------------------------------------
// Name: SendHeartbeatToAll()
// Desc: Issue a MSG_HEARTBEAT from ourself (either a host or player) to
//       every other player
//-----------------------------------------------------------------------------
VOID CXBoxSample::SendHeartbeatToAll()
{
    // Send the heartbeat
    Message msgHeartbeat( MSG_HEARTBEAT );

    // Send hearbeat via VDP directly to all other players
    SendMessage( &msgHeartbeat );
}

//-----------------------------------------------------------------------------
// Name: SendScoreToAll
// Desc: Sends a message that indicates a player scored to all players.
//-----------------------------------------------------------------------------
VOID CXBoxSample::SendScoreToAll( Message& msg )
{
    SendMessage( &msg );
}

//-----------------------------------------------------------------------------
// Name: SendMessage
// Desc: Handles the logic of actually sending a message out over the network
//       There are two options, each with two possibilites
//       bReliable - If TRUE, send over reliable channel.  This host is
//          responsible for relaying reliable messages between clients.  If
//          FALSE, then send directly via VDP.
//       psaDest - Optional parameter that defaults to NULL.  If a player
//          address is specified, the message is intended for that player only
//
//-----------------------------------------------------------------------------
INT CXBoxSample::SendMessage( const Message* pMsg,
                              const SOCKADDR_IN* psaDest )
{
    INT nBytes = 0;

    // Non-reliable message - these get sent directly via VDP
    // regardless of whether or not we're the host
    if( psaDest )
    {
        // If destined for a specific player, send straight to them
        nBytes += m_DirectSock.SendTo( pMsg, pMsg->GetSize(), psaDest );
    }
    else
    {
        // If destined for everyone, loop over the players list
        // Multiple players can be be on a single box,
        // so we need to make sure the message is only
        // sent ONCE per IP
        INT  iNumSent   = 0;
        INT  iNumFailed = 0;

        for( PlayerList::iterator it = m_rwPlayers.begin(); it != m_rwPlayers.end(); ++it )
        {
            BOOL bAlreadySent = FALSE;

            for( PlayerList::iterator prevPlayers = m_rwPlayers.begin();
                 prevPlayers != it; ++prevPlayers )
            {
                if( !memcmp( &it->inAddr, &prevPlayers->inAddr, sizeof( IN_ADDR ) ) )
                    bAlreadySent = TRUE;
            }

            if( !bAlreadySent )
            {
                ++iNumSent;

                SOCKADDR_IN sa;
                sa.sin_family = AF_INET;
                sa.sin_addr   = it->inAddr;
                sa.sin_port   = htons( DIRECT_PORT );

                INT nJustSent = m_DirectSock.SendTo( pMsg, pMsg->GetSize(), &sa );

                nBytes += nJustSent;

                if( nJustSent < 0 )
                    ++iNumFailed;
            }
        }

        // Log any failures
        if( iNumFailed )
        {
            XBUtil_DebugPrint( "Sent to %d IPS and %d sends failed\n",
                               iNumSent, iNumFailed );
        }
    }

    return nBytes;
}

//-----------------------------------------------------------------------------
// Name: ProcessDirectMessage()
// Desc: Checks to see if any direct messages are waiting on the direct socket.
//       If a message is waiting, it is routed and processed.
//       If no messages are waiting, the function returns immediately.
//       Returns TRUE if a message was processed.
//-----------------------------------------------------------------------------
BOOL CXBoxSample::ProcessDirectMessage()
{
    if( !m_DirectSock.IsOpen() )
        return FALSE;

    // See if a network message is waiting for us
    Message msg;
    SOCKADDR_IN saFromIn;
    INT iResult;

    // Process until no more messages are available
    do
    {
        iResult = m_DirectSock.RecvFrom( &msg, msg.GetMaxSize(), &saFromIn );
        SOCKADDR_IN saFrom( saFromIn );

        // If message waiting, process it
        if( iResult != SOCKET_ERROR && iResult > 0 )
        {
            assert( iResult == msg.GetSize() );
            ProcessMessage( msg, saFrom );
        }
        else
        {
            assert( WSAGetLastError() == WSAEWOULDBLOCK );
        }
    } while( iResult != SOCKET_ERROR && iResult > 0 );

    return FALSE;
}

//-----------------------------------------------------------------------------
// Name: ProcessMessage()
// Desc: Routes any direct messages
//-----------------------------------------------------------------------------
VOID CXBoxSample::ProcessMessage( Message& msg, const SOCKADDR_IN& saFrom )
{
    // Process the message
    switch( msg.GetId() )
    {
        // From client to host; processed by host
        case MSG_JOIN_GAME:     ProcessJoinGame( msg.GetJoinGame(), saFrom ); break;
        case MSG_REGISTERED:    ProcessClientRegistered( msg.GetRegistered() ); break;

        // From host to client: processed by client
        case MSG_JOIN_APPROVED: ProcessJoinApproved( msg.GetJoinApproved(), saFrom ); break;
        case MSG_JOIN_DENIED:   ProcessJoinDenied( saFrom ); break;
        case MSG_GAME_START:    assert( !m_bIsHost ); StartArbitratedGame(); break;
        case MSG_PLAYER_JOINED: ProcessPlayerJoined( msg.GetPlayerJoined(), saFrom ); break;
        case MSG_ARB_ID:
            if( m_bIsHost )
            {
                return;
            }
            else
            {
                ZeroMemory( &m_arbID, sizeof( m_arbID ) );
                m_bXUIDsCopied = FALSE;

                CopyMemory( &m_arbID, &msg.GetArbID().arbID, sizeof( m_arbID ) );

                HRESULT hrRegister = RegisterForArbitration();

                if( FAILED( hrRegister ) )
                {
                    PopState();

                    PushMessageWindow( "Failed to register with arbitration," );

                    return;
                }
            }
            break;

        case MSG_XUIDS:
            if( m_bIsHost )
                return;

            CopyXUIDs( msg.GetXUIDs().xuids );
            m_dwPlayersRegistered = msg.GetXUIDs().wXUIDCount;
            break;

        case MSG_GAME_OVER:
            if( m_state == STATE_GAME_SESSION )
            {
                HRESULT hrSubmit = SubmitArbitrationResults();
                PopState();

                PushMessageWindow( SUCCEEDED( hrSubmit ) ?
                                   "The Round has finished."
                                   : "Error submitting arb report" );
            }
            break;

        // From player to player: processed by client player
        case MSG_SCORE:         
            ProcessScore( msg.GetScore().qwID ); 
            if( m_bIsHost ) 
            {
                SendScoreToAll( msg );
            }
            break;

        // Ignore waves
        case MSG_WAVE:          
            break; 

        case MSG_HEARTBEAT:     
            ProcessHeartbeat( saFrom ); 
            break;

        // Any other message on this port is invalid and we ignore it
        default: 
            assert( FALSE ); 
            break;
    }
}

//-----------------------------------------------------------------------------
// Name: ProcessJoinGame()
// Desc: Process the join game message
//-----------------------------------------------------------------------------
VOID CXBoxSample::ProcessJoinGame( const MsgJoinGame& joinGame,
                                   const SOCKADDR_IN& saFrom )
{
    // Only hosts should receive "join game" messages
    if( !m_bIsHost )
    {
        XBUtil_DebugPrint( "Received join game request when not host!" );

        SendJoinDenied( saFrom );
        return;
    }

    if( m_state != STATE_GAME_LOBBY )
    {
        SendJoinDenied( saFrom );
        return;
    }

    // A session exists between us (the host) and the client. We can now
    // convert the incoming IP address (saFrom) into a valid XNADDR.
    XNADDR xnAddrClient;
    INT iResult = XNetInAddrToXnAddr( saFrom.sin_addr,
                                      &xnAddrClient,
                                      &m_xnHostKeyID );
    if( iResult == SOCKET_ERROR )
    {
        // If the client INADDR can't be converted to an XNADDR, then
        // this client does not have a valid session established
        assert( 0 && "XNetInAddrToXnAddr");

        return;
    }

    // A player may join if we haven't reached the player limit.
    // In a real game, you would need to "lock" the game during a join
    // or track the number of joins in progress so that if multiple
    // players were attempting to join at the same time, they wouldn't
    // all be granted access and then exceed the player maximum.
    if(( m_hostedSession.PublicOpen >= joinGame.dwNumPlayers )
       || m_bTourneySession )
    {
        // Notify the other players about the new guy
        for( DWORD i = 0; i < joinGame.dwNumPlayers; i++ )
        {
            Player player;

            CopyMemory( &player.xnAddr, &xnAddrClient, sizeof( XNADDR ) );
            strncpy( player.strGamertag, joinGame.strGamertags[i], 
                     XONLINE_MAX_GAMERTAG_LENGTH );
            player.xuid = joinGame.xuids[i];

            SendPlayerJoinedToAll( player );
        }

        // We send the approval to the player AFTER we've told
        // everyone else.  This way, he doesn't get a PlayerJoined
        // message for himself
        SendJoinApproved( saFrom );

        for( DWORD i = 0; i < joinGame.dwNumPlayers; i++ )
        {
            // Handle the joining of the new player
            in_addr inaddr = saFrom.sin_addr;
            OnPlayerJoined( joinGame.strGamertags[i],
                            joinGame.xuids[i],
                            xnAddrClient,
                            &inaddr );
        }


        m_dwSlotsInUse += joinGame.dwNumPlayers;

        // Update the hosted session
        if( !m_bTourneySession )
        {
            UpdateSession( m_hostedSession,
                           m_dwSlotsInUse,
                           m_bArbitrationStarted );
        }
    }
    else
    {
        SendJoinDenied( saFrom );
    }
}

//-----------------------------------------------------------------------------
// Name: ProcessClientRegistered()
// Desc: Takes a message that the player registered with arbitration.
//       Everyone has finished registering, then the round is started.
//-----------------------------------------------------------------------------
VOID CXBoxSample::ProcessClientRegistered( const MsgRegistered& msgRegistered )
{
    // This message is received by the host whenever one of the players registers
    // with the arbitration server. If everybody registers then we can stop
    // waiting. We should increment our count of how many players are registered
    // by the number of players on that box - currently I just increment by one,
    // which is adequate if you only allow one player per box. This message is
    // currently broadcast to all players. All but the host should ignore it.
    if( m_bIsHost )
    {
        m_dwPlayersRegistered += msgRegistered.wUserCount;

        m_bWaitingToJoin = ( m_dwPlayersRegistered == m_dwSlotsInUse );

        if ( !m_bWaitingToJoin )
        {
            StartArbitratedGame();
        }
    }
}

//-----------------------------------------------------------------------------
// Name: ProcessJoinApproved()
// Desc: Process the join approved message
//-----------------------------------------------------------------------------
VOID CXBoxSample::ProcessJoinApproved( const MsgJoinApproved& joinApproved,
                                       const SOCKADDR_IN& saFrom )
{
    // Only clients should receive "join approved" messages
    assert( !m_bIsHost );

    // If for some reason we receive a "join approved" message and we're hosting
    // a game, ignore the message. Only clients handle this message
    if( m_bIsHost )
        return;

    // Add the host
    XNADDR  xnAddr = {0};
    in_addr inaddr = saFrom.sin_addr;
    for( DWORD i = 0; i < joinApproved.dwNumHostPlayers; i++ )
    {
        OnPlayerJoined( joinApproved.strHostGamertags[i],
                        joinApproved.xuids[i],
                        xnAddr,
                        &inaddr );
    }

    // Build the list of the other players
    for( BYTE i = 0; i < joinApproved.byNumPlayers; ++i )
    {
        OnPlayerJoined( joinApproved.PlayerList[ i ].strGamertag,
                        joinApproved.PlayerList[ i ].xuid,
                        joinApproved.PlayerList[ i ].xnAddr,
                        NULL );
    }

    m_HeartbeatTimer.StartZero();

    // We should be in the quickmatch state
    // if not, then something is wrong
    assert( m_state == STATE_QUICKMATCH || m_bTourneySession );

    // Transition to the game lobby
    PopState();
    PushState( STATE_GAME_LOBBY );
}

//-----------------------------------------------------------------------------
// Name: ProcessJoinDenied()
// Desc: Process the join denied message
//-----------------------------------------------------------------------------
VOID CXBoxSample::ProcessJoinDenied( const SOCKADDR_IN& )
{
    // If for some reason we receive a "join denied" message and we're hosting
    // a game, ignore the message. Only clients handle this message
    assert( !m_bIsHost );

    PushMessageWindow( "Join request was denied" );
}

//-----------------------------------------------------------------------------
// Name: ProcessPlayerJoined()
// Desc: Process the player joined message
//-----------------------------------------------------------------------------
VOID CXBoxSample::ProcessPlayerJoined( const MsgPlayerJoined& playerJoined,
                                       const SOCKADDR_IN& saFrom )
{
    // saFrom is the address of the host that sent this message, but we
    // we already have his address, so throw it away
    (VOID)saFrom;

    const Player& player = playerJoined.player;
    OnPlayerJoined( player.strGamertag, player.xuid, player.xnAddr, NULL );
}

//-----------------------------------------------------------------------------
// Name: ProcessScore()
// Desc: Processes a message from the user with the given ID number
//       that indicates the user scored. Adds a point to the appropraite user.
//-----------------------------------------------------------------------------
VOID CXBoxSample::ProcessScore( ULONGLONG qwID )
{
    // Search through all the ID numbers and
    // add the point to the correct user
    for( INT i = 0; i < (INT)m_dwPlayersRegistered; ++i )
    {
        if( qwID == m_rwPlayerXUIDs[i].qwUserID )
        {
            ++m_rwScores[i];

            return;
        }
    }

    // Log the failure
    XBUtil_DebugPrint( "Unable to find qwID 0x%x\n", qwID );
}


//-----------------------------------------------------------------------------
// Name: ProcessHeartbeat()
// Desc: Process the heartbeat message
//-----------------------------------------------------------------------------
VOID CXBoxSample::ProcessHeartbeat( const SOCKADDR_IN& saFrom )
{
    MatchInAddr matchInAddr( saFrom );

    // Find out who sent a heartbeat by matching the INADDR
    for( DWORD i = 0; i < m_rwPlayers.size(); i++ )
    {
        if( saFrom.sin_addr.s_addr == m_rwPlayers[i].inAddr.s_addr )
            m_rwPlayers[i].dwLastHeartbeat = GetTickCount();
    }
}

//-----------------------------------------------------------------------------
// Name: OnPlayerJoined
// Desc: Called whenever a new player joins the game. Adds the clients
//       address and players to the list used by messaging.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::OnPlayerJoined( const CHAR* strName,
                                     XUID xuid,
                                     const XNADDR& xnAddr,
                                     IN_ADDR* pinAddr )
{
    // Build the player information
    PlayerInfo playerInfo;

    strncpy( playerInfo.strGamertag, strName, XONLINE_MAX_GAMERTAG_LENGTH );

    playerInfo.xuid             = xuid;
    playerInfo.xnAddr           = xnAddr;
    playerInfo.dwLastHeartbeat  = GetTickCount();

    assert( playerInfo.xuid.qwUserID != 0xcccccccccccccccc );
    assert( playerInfo.xuid.qwUserID != 0 );

    if( pinAddr != NULL )
    {
        playerInfo.inAddr = *pinAddr;
    }
    else
    {
        // Need to convert XNADDR to in_addr
        INT iResult = XNetXnAddrToInAddr( &playerInfo.xnAddr,
                                          &m_xnHostKeyID,
                                          &playerInfo.inAddr );

        if( iResult == SOCKET_ERROR )
        {
            return E_FAIL;
        }
    }


    // Add the new player to our list
    // But only if they are not already in the list
    // This situation can happen if they drop out
    // and then quickly re-join

    BOOL bFound = FALSE;

    for( INT i = 0; i < (INT)m_rwPlayers.size(); ++i )
    {
        if( !strcmp( playerInfo.strGamertag, m_rwPlayers[i].strGamertag ) )
        {
            bFound = TRUE;

            memcpy( &m_rwPlayers[i], &playerInfo, sizeof( playerInfo ) );

            XBUtil_DebugPrint( "NOT ADDING: %s\n", playerInfo.strGamertag );
        }
    }

    for( INT i = 0; i < XGetPortCount(); ++i )
    {
        if( !m_rwLocalUsers[i].m_bSignedIn )
            continue;

        if( !strcmp( m_rwStoredUsers[m_rwLocalUsers[i].m_wUserIndex].szGamertag,
            playerInfo.strGamertag ) )
        {
            bFound = TRUE;
            break;
        }
    }

    if( !bFound )
    {
        m_rwPlayers.push_back( playerInfo );
    }

    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: OnPlayerDisconnect
// Desc: Called whenever we've detected a player disconnect
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::OnPlayerDisconnect( PlayerInfo* pPlayer )
{
    // Update status message to reflect the newly departed player
    if( ( !m_bIsHost ) && ( pPlayer->inAddr.S_un.S_addr == m_inHostAddr.S_un.S_addr ) )
    {
        // Since we don't have a reliable channel without the host,
        // and don't support host migration, we should end the game here
        assert( !m_bIsHost );

        m_inHostAddr.s_addr = 0;
    }

    // The host needs to close down the reliable channel
    if( m_bIsHost )
    {
        // Find the matching entry in list of reliable sockets
        MatchInAddr matchInAddr( pPlayer->inAddr );
        SocketList::iterator it = std::find_if( m_ClientSockets.begin(), 
                                  m_ClientSockets.end(), matchInAddr );
        if( it != m_ClientSockets.end() )
        {
            // Close the socket
            closesocket( it->sock );
            m_ClientSockets.erase( it );
        }
    }

    // Find each player at this address and disconnect them
    for( ; ; )
    {
        // Remove from players list
        MatchInAddr matchInAddr( pPlayer->inAddr );
        PlayerList::iterator it = std::find_if( m_rwPlayers.begin(), 
                                  m_rwPlayers.end(), matchInAddr );
        if( it == m_rwPlayers.end() )
            break;

        m_rwPlayers.erase( it );

        // Update the match making session so we can
        // allow a new player to join the game
        if( m_bIsHost )
            RemovePlayer();
    }

    return S_OK;
}

//-------------------------------------------------------------------------------------
// Name: OnPlayerDropout()
// Desc: The given player left the game
//-------------------------------------------------------------------------------------
VOID CXBoxSample::OnPlayerDropout( const PlayerInfo& playerInfo, BOOL bIsHost )
{
    if( !m_bIsHost )
    {
        // If this console is the host, remove inform the matchmaking server
        if( m_bIsHost )
        {
             RemovePlayer();
        }
    }
}

//-----------------------------------------------------------------------------
// Name: ProcessPlayersDropouts()
// Desc: Process players and determine if anybody has left the game
//-----------------------------------------------------------------------------
BOOL CXBoxSample::ProcessPlayerDropouts()
{
    INT   iNumPlayersDropedout = 0;
    DWORD dwTickCount          = GetTickCount();

    for( INT i = 0; i < (INT)m_rwPlayers.size(); ++ i )
    {
        DWORD dwElapsed = dwTickCount - m_rwPlayers[i].dwLastHeartbeat;
        if( dwElapsed > PLAYER_TIMEOUT )
        {
            OnPlayerDisconnect( &m_rwPlayers[i] );
            ++iNumPlayersDropedout;
        }
    }

    return ( iNumPlayersDropedout > 0 );
}

//-----------------------------------------------------------------------------
// Name: RegisterForArbitration()
// Desc: Registers this machine and all users logged on with the arbitration
//       service. Returns FALSE if registration fails.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::RegisterForArbitration()
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
    DWORD dwFlags                 = m_bTourneySession ? 
                                    ( XONLINE_ARB_REGISTER_FLAG_TEAMS | 
                                      XONLINE_ARB_REGISTER_FLAG_USER_COMPETITION ) : 
                                      XONLINE_ARB_REGISTER_FLAG_FFA;

    CONST DWORD MAX_ROUND_SECONDS = 600;

    HRESULT hrRegister = XOnlineArbitrationRegister(
                             &m_arbID,          // ID number of the arbitration session
                             MAX_ROUND_SECONDS, // Time in seconds of the round
                             dwFlags,           // Any flags
                             NULL,              // Any work event to be triggered
                             &arbitrationHandle // Handle of assign
                         );

    if( FAILED( hrRegister ) )
    {
        return hrRegister;
    }

    // Block execution until the registration process has finished
    if( !WaitForTaskToComplete( arbitrationHandle, &hrRegister ) )
    {
        return hrRegister;
    }

    // Now we can ask to see what other boxes have registered.
    // Allocate a buffer for the registrants and zero it.
    XONLINE_ARB_REGISTRANT rwRegistrantsBuffer[ MAX_MATCHERS ] = { 0 };

    // You can find out how many machines have registered so far with
    // the specified arbitration ID. You can then iterate through
    // the results in the RegistrantsBuffer, adding up the users on
    // each machine, to get the total number of users.
    DWORD dwNumRegisteredBoxes = 0;

    HRESULT hrRegResults = XOnlineArbitrationRegisterGetResults(
                                arbitrationHandle,       // Task used to create round
                                MAX_MATCHERS,            // Maximum number of players
                                rwRegistrantsBuffer,     // Buffer to write player info to
                                &dwNumRegisteredBoxes ); // Number of Xboxes that registered

    if( FAILED( hrRegResults ) )
    {
        arbitrationHandle.Close();

        return hrRegResults;
    }

    if( !m_bIsHost )
    {
        // Since we are a client/peer we need to
        // let the host know that we registered.
        Message msgRegistered( MSG_REGISTERED );
        MsgRegistered& msg = msgRegistered.GetRegistered();

        msg.wUserCount = 0;

        for( INT i = 0; i < XGetPortCount(); ++i )
        {
            if( m_rwLocalUsers[i].m_bSignedIn )
            {
                msg.rwIDs[msg.wUserCount] = 
                    m_rwStoredUsers[m_rwLocalUsers[i].m_wUserIndex].xuid.qwUserID;

                ++msg.wUserCount;
            }
        }

        // Send the player joined message reliably to all players in the game
        INT nBytes = SendMessage( &msgRegistered );

        // This assert was removed because Send no longer is guaranteed to always work
        // If the security association times out, the number of bytes returned will
        // NOT be equal to the size of the message.  A good thing to do here would
        // be to drop the player

        assert( nBytes != SOCKET_ERROR );
    }
    else
    {
        // If we are the host, then registration must be finished. Therefore
        // we should record all of the XUIDs and disconnect any players that
        // did not register.

        DWORD dwNumRegisteredPlayers = 0;
        // Zero the XUID array, to mark unused XUIDs.
        XUID rwPlayerXUIDs[MAX_MATCHERS] = { 0 };

        for( DWORD i = 0; i < dwNumRegisteredBoxes; ++i )
        {
            // If your game allows multiple players per box then you need to
            // loop over the results from each box, copying each player.
            for( DWORD player = 0; player < XONLINE_MAX_LOGON_USERS; ++player )
            {
                if( rwRegistrantsBuffer[i].xuidUsers[player].qwUserID )
                {
                    assert( dwNumRegisteredPlayers < MAX_MATCHERS );

                    rwPlayerXUIDs[dwNumRegisteredPlayers]   =
                        rwRegistrantsBuffer[i].xuidUsers[player];
                    m_rwPlayerXUIDs[dwNumRegisteredPlayers] =
                        rwRegistrantsBuffer[i].xuidUsers[player];

                    ++dwNumRegisteredPlayers;
                }
            }
        }

        m_dwPlayersRegistered = dwNumRegisteredPlayers;


        // The clients need the XUIDs of all the other
        // players for proper submission of the match
        // results. Since only the host has the XUIDs
        // of all the successfully registered players
        // in the match, we need to send the XUIDs to
        // all the registered players.

        m_bXUIDsCopied = TRUE;

        Message   msgXUIDs( MSG_XUIDS );
        MsgXUIDs& msg = msgXUIDs.GetXUIDs();
        msg.wXUIDCount = (WORD)m_dwPlayersRegistered;
        ZeroMemory( msg.xuids, sizeof( XUID ) * MAX_MATCHERS );

        XBUtil_DebugPrint( "Building XUID packet to send to clients. Message consists of %d XUIDS\n",
                            m_dwPlayersRegistered );

        for( INT i = 0; i < (INT)m_dwPlayersRegistered; ++i)
        {
            XBUtil_DebugPrint( "XUID[%d].qwID = 0x%x\n",
                               i, m_rwPlayerXUIDs[i].qwUserID );

            msg.xuids[i] = m_rwPlayerXUIDs[i];
        }

        INT nBytes = SendMessage( &msgXUIDs );
        assert( nBytes != SOCKET_ERROR );


        if( dwNumRegisteredPlayers < 2 )
        {
            // Arbitrated sessions must have at least two players. If we
            // don't have that many then we can't proceed. This can happen
            // if the other players quit as the registered game is starting,
            // or if their are connection problems that prevent them from
            // joining.
            arbitrationHandle.Close();

            return E_FAIL;
        }
    }


    // Close the task and return our success

    arbitrationHandle.Close();

    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: SubmitFFAArbitrationResults
// Desc: Sumbits the score results of a Free For All match to the arbitration
//       server. Returns the success code.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::SubmitFFAArbitrationResults( BOOL bWithdrawl )
{
    // Step 1
    //
    // Determine our report flags.
    // If we are quitting before the normal round end
    // the XONLINE_ARB_REPORT_FLAG_VOLUNTARILY_QUITTING
    // flag needs to be included
    DWORD dwReportFlags = m_bIsHost ? XONLINE_ARB_REPORT_FLAG_WAS_HOST : 0x0;

    if( bWithdrawl )
        dwReportFlags = dwReportFlags | XONLINE_ARB_REPORT_FLAG_VOLUNTARILY_QUITTING;


    // Step 2
    //
    // Build the stat submission package

    XONLINE_STAT_PROC rwStatReport[MAX_MATCHERS] = { 0 };
    XONLINE_STAT      rwNewStat[MAX_MATCHERS]    = { 0 };

    for( INT i = 0; i < (INT)m_dwPlayersRegistered; ++i )
    {
        XONLINE_STAT_UPDATE newReport;

        // Add the player's score to their rating
        // so their rating is the sum of all previous
        // scores
        rwNewStat[i].wID     = XONLINE_STAT_RATING;
        rwNewStat[i].type    = XONLINE_STAT_LONGLONG;
        rwNewStat[i].llValue = m_rwScores[i];

        newReport.xuid               = m_rwPlayerXUIDs[i];
        newReport.dwLeaderBoardID    = m_bTourneySession ? COMPETITION_LEADERBOARD_ID : 
                                       INVIDUAL_LEADERBOARD_ID;
        newReport.dwConditionalIndex = 0; // Always update
        newReport.dwNumStats         = 1; // Only one stat
        newReport.pStats             = &rwNewStat[i];

        rwStatReport[i].wProcedureID = XONLINE_STAT_PROCID_UPDATE_INCREMENT;
        rwStatReport[i].Update       = newReport;
    }


    // Step 3
    //
    // Start the submission of the statistics

    CXBOnlineTask hSubmitTask;

    HRESULT hrSubmitResults = XOnlineArbitrationReport(
        &m_arbID,              // Pointer to arbitration ID
        m_dwPlayersRegistered, // Number of stat procs to be submitted
        rwStatReport,          // Array of stat report data
        NULL,                  // Optional - Array of data of suspicious info
        dwReportFlags,         // Report flags
        NULL,                  // Work event to be triggered when finished
        &hSubmitTask );        // Task to be assigned


    if( FAILED( hrSubmitResults ) )
        return hrSubmitResults;


    // Step 4
    //
    // Pump the task until complete

    WaitForTaskToComplete( hSubmitTask, &hrSubmitResults );


    // Step 5
    //
    // Close the task and return our success

    hSubmitTask.Close();

    return hrSubmitResults;
}

//-----------------------------------------------------------------------------
// Name: SubmitCompetitionArbitrationResults
// Desc: Submits the results of a match between two teams from a competition
//       to the arbitration servers. Returns the success code.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::SubmitCompetitionArbitrationResults( BOOL bWithdrawl )
{
    // Step 1
    //
    // Determine our report flags

    // NOTE: XONLINE_ARB_REPORT_FLAG_VOLUNTARILY_QUITTING is
    //       not supported by competitions.
    DWORD dwReportFlags = m_bIsHost ? XONLINE_ARB_REPORT_FLAG_WAS_HOST : 0x0;


    // Step 2
    //
    // Calculate the scores of the two teams

    ULONGLONG qwTeam1Score = 0;
    ULONGLONG qwTeam2Score = 0;

    BOOL bCalcedScores = CalcTeamScores( g_rwTopology[m_dwEventRound].player1,
                                         qwTeam1Score,
                                         g_rwTopology[m_dwEventRound].player2,
                                         qwTeam2Score );

    assert( bCalcedScores );


    // Step 3
    //
    // Now we build the results package that will be submitted
    // to the statistics service

    BOOL              PlayerOneWins                       = qwTeam1Score >= qwTeam2Score;
    const DWORD       NUM_RESULT_ATTRIBUTES               = 3;
    XONLINE_ATTRIBUTE rgAttributes[NUM_RESULT_ATTRIBUTES] = { 0 };

    ULONGLONG qwEntityID   = 0;
    ULONGLONG qwTeamID     = m_rwTeamXUIDS[m_iTeamSelected].qwTeamID;
    DWORD     dwEventIndex = 0;

    GetRoundOpponent( g_rwTopology, m_dwTopologyCount,
                      qwTeamID, dwEventIndex,qwEntityID );

    m_dwEventRound = dwEventIndex;

    // The attributes must be in this order - entityid, winner, then loser.

    // Build the entity ID
    rgAttributes[0].dwAttributeID        = XONLINE_COMP_ATTR_EVENT_ENTITY_ID;
    rgAttributes[0].info.integer.qwValue = qwEntityID;

    // Build the winner
    rgAttributes[1].dwAttributeID        = XONLINE_COMP_ATTR_EVENT_WINNER;
    rgAttributes[1].info.integer.qwValue = PlayerOneWins ?
            g_rwTopology[m_dwEventRound].player1 : g_rwTopology[m_dwEventRound].player2;

    // Build the looser
    rgAttributes[2].dwAttributeID        = XONLINE_COMP_ATTR_EVENT_LOSER;
    rgAttributes[2].info.integer.qwValue = PlayerOneWins ?
            g_rwTopology[m_dwEventRound].player2 : g_rwTopology[m_dwEventRound].player1;


    // Step 4
    //
    // Start the results submission

    CXBOnlineTask hSubmitTask;

    ULONGLONG qwCompID = g_joinedCompQuery.Results[m_dwTourneySelected].att_comp_id;

    HRESULT hrSubmitResults = XOnlineCompetitionSubmitResults(
                                SE_ID,                 // Template ID
                                qwCompID,              // Comp ID
                                &m_arbID,              // arb ID
                                dwReportFlags,         // report flags
                                NULL,                  // report data per player
                                0,                     // num stat procs
                                NULL,                  // stat procs
                                NUM_RESULT_ATTRIBUTES, // Num additional attribs
                                rgAttributes,          // Additional attribs
                                NULL,                  // Event to signal
                                &hSubmitTask );        // Task to assign

    if( FAILED( hrSubmitResults ) )
        return hrSubmitResults;


    // Step 5
    //
    // Pump the task until complete
    WaitForTaskToComplete( hSubmitTask, &hrSubmitResults );


    // Step 6
    //
    // Close the task and return our success

    hSubmitTask.Close();

    return hrSubmitResults;
}

//-----------------------------------------------------------------------------
// Name: SubmitArbitrationResults()
// Desc: Submits the results of the round to the arbitration service.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::SubmitArbitrationResults( BOOL bWithdrawl )
{
    HRESULT hrSubmit = E_FAIL;

    if( m_bTourneySession )
    {
        hrSubmit = SubmitCompetitionArbitrationResults( bWithdrawl );
    }
    else
    {
        hrSubmit = SubmitFFAArbitrationResults( bWithdrawl );
    }

    // Clear the player list.
    // Results submission should be the last
    // action performed after a round
    m_rwPlayers.clear();

    return hrSubmit;
}

//-----------------------------------------------------------------------------
// Name: GetPlayerScore()
// Desc: Returns the score of the player with the given ID number
//-----------------------------------------------------------------------------
INT CXBoxSample::GetPlayerScore( ULONGLONG qwID )
{
    for( INT i = 0; i < (INT)m_dwPlayersRegistered; ++i )
    {
        // Found a match, return their score
        if( m_rwPlayerXUIDs[i].qwUserID == qwID )
        {
            return m_rwScores[i];
        }
    }

    return 0;
}

//-----------------------------------------------------------------------------
// Name: IsOnMyTeam
// Desc: Takes the ID number of a user and returns TRUE if the
//       user is on the controlling user's team.
//-----------------------------------------------------------------------------
BOOL CXBoxSample::IsOnMyTeam( ULONGLONG qwUserID )
{
    for( DWORD dwUser = 0; dwUser < m_dwTeamMemberCount; ++dwUser )
    {
        if( qwUserID == m_rwTeamMembers[dwUser].qwUserID )
            return TRUE;
    }

    return FALSE;
}

//-----------------------------------------------------------------------------
// Name: CalcTeamScores
// Desc: Takes the ID numbers of the teams that were just competing in
//       a match and returns their respective scores via output parameters.
//       Returns FALSE if the scores can not be summed.
//-----------------------------------------------------------------------------
BOOL CXBoxSample::CalcTeamScores( ULONGLONG& qwTeam1ID,
                                  ULONGLONG& qwTeam1Score,
                                  ULONGLONG& qwTeam2ID,
                                  ULONGLONG& qwTeam2Score )
{
    // Get the roster for our current team
    BOOL bGotRoster = GetTeamRoster( m_wControllingUser,
                                     m_phTeamRosterTask,
                                     m_rwTeamXUIDS[m_iTeamSelected],
                                     m_rwTeamMembers,
                                     m_dwTeamMemberCount );

    if( !bGotRoster )
        return FALSE;

    DWORD dwScoreSum     = 0;
    DWORD dwMyTeamSum    = 0;
    DWORD dwOtherTeamSum = 0;

    // Sum the local scores first
    for( DWORD dwUser = 0; dwUser < m_dwPlayersRegistered; ++dwUser )
    {
        dwScoreSum += m_rwScores[dwUser];

        if( IsOnMyTeam( m_rwPlayerXUIDs[dwUser].qwUserID ) )
            dwMyTeamSum += m_rwScores[dwUser];
        else
            dwOtherTeamSum += m_rwScores[dwUser];

        assert( dwScoreSum == ( dwOtherTeamSum + dwMyTeamSum ) );
    }

    // The competitor scores + our score should equal the sum
    ULONGLONG qwMyTeamID = m_rwTeamXUIDS[m_iTeamSelected].qwTeamID;

    qwTeam1Score = ( qwMyTeamID == qwTeam1ID ) ? dwMyTeamSum : dwOtherTeamSum;
    qwTeam2Score = ( qwMyTeamID == qwTeam2ID ) ? dwMyTeamSum : dwOtherTeamSum;

    return TRUE;
}

//-----------------------------------------------------------------------------
// Name: IsOpenNAT()
// Desc: Checks to see if the NAT deployed by the ISP ( or the user's router )
//       is restrictive. A moderate or strict NAT should not host
//       competition rounds. Other players on moderate or strict NATs
//       will not be able to join the round.
//-----------------------------------------------------------------------------
BOOL CXBoxSample::IsOpenNAT()
{
    // Check the NAT type. If incompatable
    // warn the user
    XONLINE_NAT_TYPE nat = XOnlineGetNatType();

    switch( nat )
    {
    // This is type of NAT should
    // not cause any problems
    case XONLINE_NAT_OPEN:
        return TRUE;
        break;

    // These NATs will cause
    // problems.
    case XONLINE_NAT_MODERATE:
    case XONLINE_NAT_STRICT:
        return FALSE;
    }

    return FALSE;
}

//-----------------------------------------------------------------------------
// Name: InitiateJoin()
// Desc: Send a join request to the specified game
//-----------------------------------------------------------------------------
VOID CXBoxSample::InitiateJoin( GameInfo& gameInfo )
{
    // Establish a session with the host game
    BOOL bRegistered = RegisterKey( &gameInfo.xnHostKeyID,
                                    &gameInfo.xnHostKey );

    if( bRegistered )
    {
        // Convert the XNADDR of the host to the INADDR we'll use to
        // join the game
        INT iResult = XNetXnAddrToInAddr( &gameInfo.xnHostAddr,
                                          &gameInfo.xnHostKeyID,
                                          &m_inHostAddr );
        assert( iResult == NO_ERROR );

        // Save the key info
        m_xnHostKeyID       = gameInfo.xnHostKeyID;
        m_xnHostKeyExchange = gameInfo.xnHostKey;

        // Open up a reliable socket to the host - this will be used
        // for low-bandwidth communications, such as join requests,
        // communicator status, etc.  We have to wait for the connection
        // to complete before sending out join request
        SOCKADDR_IN saHost;
        ZeroMemory( &saHost, sizeof( saHost ) );
        saHost.sin_family = AF_INET;
        saHost.sin_addr   = m_inHostAddr;
        saHost.sin_port   = htons( RELIABLE_PORT );

        m_GameJoinTimer.StartZero();
    }
}

//-----------------------------------------------------------------------------
// Name: StartArbitratedGame()
// Desc: Transitions the game to the game session. If this machine is the
//       host, then it sends a message to all clients to transition to
//       the game session state.
//-----------------------------------------------------------------------------
VOID CXBoxSample::StartArbitratedGame()
{
    // Ignore if the game has already started
    if( m_state == STATE_GAME_SESSION )
        return;

    // If we are the host, register ourselves
    // with arbitration last. A packet
    // containing all the XUIDS will be
    // sent to the other client machines
    if( m_bIsHost )
    {
        if ( FAILED( RegisterForArbitration() ) )
        {
            PopState();
            PushMessageWindow( "Unable to register host with arbitration." );

            return;
        }

        // Send the game start message to the other boxes
        Message msgGameStart( MSG_GAME_START );

        INT nBytes = SendMessage( &msgGameStart );

        assert( nBytes != SOCKET_ERROR );
    }

    // Transition to the game screen

    PopState();
    PushState( STATE_GAME_SESSION );

    ZeroMemory( m_rwScores, sizeof( m_rwScores ) );
}

//-----------------------------------------------------------------------------
// Name: InitXHV
// Desc: Initializes XHV
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::InitXHV()
{
    HRESULT hr = S_OK;

    // Set up parameters for the Voice Chat engine
    XHV_RUNTIME_PARAMS xhvParams = {0};
    xhvParams.dwMaxLocalTalkers         = XGetPortCount();
    xhvParams.dwMaxRemoteTalkers        = MAX_REMOTE_USERS;
    xhvParams.dwMaxCompressedBuffers    = 4;                    // 4 buffers per local talker
    xhvParams.dwFlags                   = 0;
    xhvParams.pEffectImageDesc          = m_pdsImageDesc;
    xhvParams.dwEffectsStartIndex       = GraphVoice_Voice_0;

    // The out-of-sync threshold allows the title to control how aggresive
    // XHV is about determining that we've lost synchronization with a
    // remote talker.  A voice packet that is received significantly before or
    // after its expected time is considered to be "out-of-sync."  If this
    // many consecutive packets from a given remote talker are determined to
    // be out-of-sync, that remote talker will be reset, causing a brief pause
    // in their voice playback.
    // This number should be roughly twice as many packets as the title holds
    // in their network send buffer.
    xhvParams.dwOutOfSyncThreshold      = 10;

    // Create the engine and use this object for the callbacks
    if( FAILED( g_XHVVoiceManager.Initialize( m_pDSound, &xhvParams, this ) ) )
        return E_FAIL;
    g_XHVVoiceManager.SetMaxPlaybackStreamsCount( NUM_XHV_PLAYBACK_STREAMS );
    m_bXHVInitialized = TRUE;

    // First, check for voice banned players - if anyone is voice banned,
    // we must disable voice through speakers
    for( DWORD i = 0; i < XGetPortCount(); i++ )
    {
        XONLINE_USER& curXUser = m_rwStoredUsers[m_rwLocalUsers[i].m_wUserIndex];
        if( curXUser.xuid.qwUserID != 0 )
        {
            // if user is allowed to use voice features
            if( XOnlineIsUserVoiceAllowed( curXUser.xuid.dwUserFlags ) )
            {
                g_XHVVoiceManager.RegisterLocalTalker( i );
            }
        }
    }

    // Put communicators into voice chat mode
    for( WORD i = 0; i < XGetPortCount(); i++ )
    {
        if( GetVoiceLevel( i ) >= VOICE_LEVEL_NO_COMMUNICATOR )
            g_XHVVoiceManager.SetProcessingMode( i, XHV_VOICEMAIL_MODE );
    }

    return hr;
}

//-----------------------------------------------------------------------------
// Name: DeInitXHV
// Desc: de-initializes XHV
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::DeInitXHV()
{
    // Put communicators into voice chat mode
    for( WORD i = 0; i < XGetPortCount(); i++ )
    {
        if( GetVoiceLevel( i ) >= VOICE_LEVEL_NO_COMMUNICATOR )
            g_XHVVoiceManager.SetProcessingMode( i, XHV_INACTIVE_MODE );
    }

    HRESULT hr = g_XHVVoiceManager.Shutdown();

    m_bXHVInitialized = FALSE;

    return hr;
}

HRESULT CXBoxSample::InitSound()
{
    // Create DirectSound
    if( FAILED( DirectSoundCreate( NULL, &m_pDSound, NULL ) ) )
        return E_FAIL;

    // There are 2 options for 3-D sound processing:
    // 1) DirectSoundUseFullHRTF - full hardware HRTF-based processing
    // 2) DirectSoundUseLightHRTF - hardware HRTF processing, but without
    //      any vertical component (azimuth only).  Saves ~60k of memory
    DirectSoundUseFullHRTF();

    static const FLOAT DOPPLER_FACTOR = 2.0f;
    // Exaggerate the doppler effect for demonstration purposes.
    m_pDSound->SetDopplerFactor( DOPPLER_FACTOR, DS3D_IMMEDIATE );

    // download the standard DirectSound effects image
    DSEFFECTIMAGELOC EffectLoc;
    EffectLoc.dwI3DL2ReverbIndex = GraphI3DL2_I3DL2Reverb;
    EffectLoc.dwCrosstalkIndex   = GraphXTalk_XTalk;
    if( FAILED( XAudioDownloadEffectsImage( "d:\\media\\dsstdfx.bin",
                                            &EffectLoc,
                                            XAUDIO_DOWNLOADFX_EXTERNFILE,
                                            &m_pdsImageDesc ) ) )
    {
        return E_FAIL;
    }

    return S_OK;
}

HRESULT CXBoxSample::InitTextures()
{
    // Create some quads and initialize our
    // texture array pointer
    m_dwTeamMemberTextureToDL = 0;
    m_ppTeamLogoTextures      = NULL;
    m_ppTeammateTextures      = NULL;
    m_lpPreviewTexture        = NULL;
    m_pLogoVerts              = CreateFace( m_pd3dDevice, 0.0f, 0.0f );


    // Assign a nifty color map to make the
    // content editing code nicer
    m_rwButtonColorMap[EV_BUTTON_A]     = COLOR_GREEN;
    m_rwButtonColorMap[EV_BUTTON_B]     = COLOR_RED;
    m_rwButtonColorMap[EV_BUTTON_X]     = COLOR_BLUE;
    m_rwButtonColorMap[EV_BUTTON_Y]     = COLOR_YELLOW;
    m_rwButtonColorMap[EV_BUTTON_WHITE] = COLOR_WHITE;
    m_rwButtonColorMap[EV_BUTTON_BLACK] = COLOR_BLACK;

    // Create the font
    if( FAILED( m_font.Create( "Font.xpr" ) ) )
        return E_FAIL;

    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: InitXNet()
// Desc: Initialize the network stack. Returns FALSE if Xbox is not connected.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::InitXNet( BOOL bInitialOnly )
{
    DWORD dwStatus  = XNetGetEthernetLinkStatus();
    BOOL  bIsOnline = ( dwStatus & XNET_ETHERNET_LINK_ACTIVE ) != 0;

    m_bTourneySession = FALSE;

    if( !bIsOnline )
        return E_FAIL;

    // Only need to initialize network stack one time
    if( !m_bXnetStarted )
    {
        // Initialize the network stack
        INT iResult = XNetStartup( NULL );
        if( iResult != NO_ERROR )
        {
            assert( 0 && "XNetStartup" );
            return E_FAIL;
        }

        // Standard WinSock startup
        WSADATA WsaData;
        iResult = WSAStartup( MAKEWORD(2,2), &WsaData );
        if( iResult != NO_ERROR )
        {
            assert( 0 && "WSAStartup" );
            return E_FAIL;
        }

        // Online startup
        if( FAILED( XOnlineStartup( NULL ) ) )
        {
            assert( 0 && "XOnlineStartup" );
            return E_FAIL;
        }

        m_bXnetStarted = TRUE;
    }

    if( bInitialOnly )
        return S_OK;


    // Unregister the game session key
    UnregisterKey();

    // Obliterate old keys and XNADDR
    ZeroMemory( &m_xnHostKeyID,       sizeof( XNKID ) );
    ZeroMemory( &m_xnHostKeyExchange, sizeof( XNKEY ) );
    m_inHostAddr.s_addr = 0;


    // The direct socket is a non-blocking socket on port DIRECT_PORT.
    // Sockets are encrypted by default, but can have encryption disabled
    // as an optimization for non-secure messaging
    BOOL bSuccess = m_DirectSock.Open( CXBSocket::Type_VDP );

    if( !bSuccess )
    {
        assert( 0 && "Direct socket open" );
        return E_FAIL;
    }

    SOCKADDR_IN directAddr;
    directAddr.sin_family      = AF_INET;
    directAddr.sin_addr.s_addr = INADDR_ANY;
    directAddr.sin_port        = htons( DIRECT_PORT );

    INT iResult = m_DirectSock.Bind( &directAddr );
    assert( iResult != SOCKET_ERROR );

    DWORD dwNonBlocking = 1;
    iResult = m_DirectSock.IoCtlSocket( FIONBIO, &dwNonBlocking );
    assert( iResult != SOCKET_ERROR );


    // Get our local address

    BOOL bHaveLocalAddress = FALSE;

    do
    {
        // Asynchronous local address acquisition
        DWORD dwAddrStatus = XNetGetTitleXnAddr( &m_xnTitleAddress );
        assert( dwAddrStatus != XNET_GET_XNADDR_NONE );

        // If we've retrieved the local address, we're done
        bHaveLocalAddress = ( dwAddrStatus != XNET_GET_XNADDR_PENDING );
    }
    while( !bHaveLocalAddress );


    return S_OK;
}

/////////////////////////////////////////////////////
// Overloaded functions defined by the application //
// class to execute game logic and rendering       //
/////////////////////////////////////////////////////

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
    case STATE_MAIN:                RenderStateMain();              break;
    case STATE_TEAMS_LEADERBOARD:   RenderStateTeamsLeaderboard();  break;
    case STATE_TEAMS:               RenderStateTeams();             break;
    case STATE_RECENT_PLAYERS:      RenderStateRecentPlayers();     break;
    case STATE_SELECT_INVITE_TEAM:  RenderStateSelectInviteTeam();  break;
    case STATE_CREATE_MATCH:        RenderStateCreateMatch();       break;
    case STATE_QUICKMATCH:          RenderStateQuickMatch();        break;
    case STATE_GAME_LOBBY:          RenderStateGameLobby();         break;
    case STATE_GAME_SESSION:        RenderStateGameSession();       break;
    case STATE_INBOX:               RenderStateInbox();             break;
    case STATE_SETTINGS_EDIT:       RenderStateSettingsEdit();      break;
    case STATE_INVITE_DETAILS:      RenderStateInviteDetails();     break;
    case STATE_VIEW_MY_TEAMS:       RenderStateViewMyTeams();       break;
    case STATE_TEAM_SEND_MESSAGE:   RenderStateSendMessage();       break;
    case STATE_TEAM_SHOW_MESSAGE:   RenderStateShowMessage();       break;
    case STATE_TEAM_OPS:            RenderStateTeamOps();           break;
    case STATE_LIST_AVAILABLE_COMPS:RenderStateListAvailableComps();break;
    case STATE_VIEW_TEAM_ROSTER:    RenderStateViewTeamRoster();    break;
    case STATE_TEAM_MEMBER_OPS:     RenderStateTeamMemberOps();     break;
    case STATE_CONTENT_EDIT:        RenderStateContentEdit();       break;
    case STATE_LIST_TOURNEYS:       RenderStateListTourneys();      break;
    case STATE_TOURNEY_RENDER:      RenderStateTourneyRender();     break;
    case STATE_MESSAGE_WINDOW:      RenderStateMessageWindow();     break;
    default:
        RenderStateNetworkError();
        break;
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
    if ( m_bXHVInitialized )
    {
        g_XHVVoiceManager.DoWork();
    }

    // Handle net messages
    if( ProcessDirectMessage() )
        return S_OK;


    for( DWORD dwControllerPort = 0; dwControllerPort < MAX_USERS; ++dwControllerPort)
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
            if( m_bUsersSignedIn  && !m_rwLocalUsers[dwControllerPort].m_bSignedIn )
                continue;
        }

        Event ev = GetEvent( dwControllerPort );

        switch( m_state )
        {
        case STATE_SELECT_ACCOUNT:      UpdateStateSelectAccount( dwControllerPort, ev );       break;
        case STATE_LOGIN:               UpdateStateLogin( ev );                                 break;
        case STATE_LOGIN_FAILED:        UpdateStateLoginFailed( ev );                           break;
        case STATE_NETWORK_ERROR:       UpdateStateNetworkError( ev );                          break;
        case STATE_MAIN:                UpdateStateMain( dwControllerPort, ev );                break;
        case STATE_TEAMS_LEADERBOARD:   UpdateStateTeamsLeaderboard( dwControllerPort, ev );    break;
        case STATE_TEAMS:               UpdateStateTeams( dwControllerPort, ev );               break;
        case STATE_RECENT_PLAYERS:      UpdateStateRecentPlayers( dwControllerPort, ev );       break;
        case STATE_SELECT_INVITE_TEAM:  UpdateStateSelectInviteTeam( dwControllerPort, ev );    break;
        case STATE_CREATE_MATCH:        UpdateStateCreateMatch( dwControllerPort, ev );         break;
        case STATE_QUICKMATCH:          UpdateStateQuickMatch( dwControllerPort, ev );          break;
        case STATE_GAME_LOBBY:          UpdateStateGameLobby( dwControllerPort, ev );           break;
        case STATE_GAME_SESSION:        UpdateStateGameSession( dwControllerPort, ev );         break;
        case STATE_INBOX:               UpdateStateInbox( dwControllerPort, ev );               break;
        case STATE_SETTINGS_EDIT:       UpdateStateSettingsEdit( dwControllerPort, ev );        break;
        case STATE_INVITE_DETAILS:      UpdateStateInviteDetails( dwControllerPort, ev );       break;
        case STATE_VIEW_MY_TEAMS:       UpdateStateViewMyTeams( dwControllerPort, ev );         break;
        case STATE_TEAM_SEND_MESSAGE:   UpdateStateSendMessage( dwControllerPort, ev );         break;
        case STATE_TEAM_SHOW_MESSAGE:   UpdateStateShowMessage( dwControllerPort, ev );         break;
        case STATE_TEAM_OPS:            UpdateStateTeamOps( dwControllerPort, ev );             break;
        case STATE_LIST_AVAILABLE_COMPS:UpdateStateListAvailableComps( dwControllerPort, ev );  break;
        case STATE_VIEW_TEAM_ROSTER:    UpdateStateViewTeamRoster( dwControllerPort, ev );      break;
        case STATE_TEAM_MEMBER_OPS:     UpdateStateTeamMemberOps( dwControllerPort, ev );       break;
        case STATE_CONTENT_EDIT:        UpdateStateContentEdit( dwControllerPort, ev );         break;
        case STATE_LIST_TOURNEYS:       UpdateStateListTourneys( dwControllerPort, ev );        break;
        case STATE_TOURNEY_RENDER:      UpdateStateTourneyRender( dwControllerPort, ev );       break;
        case STATE_MESSAGE_WINDOW:      UpdateStateMessageWindow( dwControllerPort, ev );       break;
        default:
            assert(0 && "Unknown/illegal state!");
        };

        // If the player is signed in, check the status of the network
        // and report any found network errors
        if( m_rwLocalUsers[dwControllerPort].m_bSignedIn )
        {
            if( !SUCCEEDED( m_hLogonTask.Continue() ) )
            {
                m_rwLocalUsers[dwControllerPort].m_bSignedIn = FALSE;

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
    // Initialize RNG for random game size and random game name.
    // Your game does not need to do this
    srand( GetTickCount() );

    m_bXnetStarted = FALSE;
    m_bIsKeyRegistered = FALSE;

    if( FAILED( InitSound() ) )
        return E_FAIL;


    if( FAILED( InitTextures() ) )
        return E_FAIL;


    m_bXHVInitialized              = FALSE;
    m_bVoiceBufferPlayable         = FALSE;
    m_bGotCommunicatorRemovalEvent = FALSE;
    m_bCommunicatorInserted        = FALSE;

    // Send message
    m_bMessageSent      = FALSE;
    m_eMessageProtocol  = INVALID_VOICE_MESSAGE_PROTOCOL;
    m_qwProtocolParamId = 0x0;
    ZeroMemory( m_swzRecipientTag , sizeof(m_swzRecipientTag) );

    // Show message
    ZeroMemory( &m_curMessageSummary , sizeof( m_curMessageSummary ) );
    m_eMessageDownloadSuccess   = INVALID_VOICE_MAIL_DOWNLOAD_ERROR;
    m_bMessageDeleted           = FALSE;


    m_bgColor                        = COLOR_BLUE;
    m_state                          = NUM_STATES;
    m_iItemSelected                  = 0;
    m_dwNumStoredUsers               = 0;
    m_wControllingUser               = 0;

    m_bUsersSigningIn                = FALSE;
    m_bUsersSignedIn                 = FALSE;
    m_iSignInResult                  = S_OK;

    ZeroMemory( m_stateStack, sizeof( m_stateStack ) );

    m_wStateStackSize                = 0;

    for( WORD wUser = 0; wUser < MAX_USERS; ++wUser )
    {
        m_rwLocalUsers[wUser].m_bSignedIn     = FALSE;
    }

    XBUtil_GetWide( "", m_szGameMessage, MAX_MESSAGE_LENGTH );

    g_pFont = &m_font;


    // Initialize Xbox Live!

    // Wait for any inserted MUs to mount
    while( XGetDeviceEnumerationStatus() == XDEVICE_ENUMERATION_BUSY );

    if( FAILED( InitXNet( TRUE ) ) )
        return E_FAIL;

    PushState( STATE_SELECT_ACCOUNT );

    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: ~CXBoxSample()
// Desc: Fress allocated memory used by the sample
//-----------------------------------------------------------------------------
CXBoxSample::~CXBoxSample()
{
    if( m_ppTeammateTextures )
        delete [] m_ppTeammateTextures;

    if( m_ppTeamLogoTextures )
        delete [] m_ppTeamLogoTextures;
}

static CXBoxSample g_xbApp;

//-------------------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program.
//-------------------------------------------------------------------------------------
VOID __cdecl main()
{
    if( FAILED( g_xbApp.Create() ) )
        return;

    g_xbApp.Run();
}
