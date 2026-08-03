//-------------------------------------------------------------------------------------
// File: GameSession.cpp
//
// Desc: Holds code used to manage game sessions/matches
//
// Hist: 12.09.04 - New for January release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-------------------------------------------------------------------------------------

#include "XBRandName.h"
#include "XBUtil.h"
#include "GameSession.h"
#include "Teams.h"
#include "XBSocket.h"

//-------------------------------------------------------------------------------------
// Name: SessionInfo()
// Desc: Default Constructor
//-------------------------------------------------------------------------------------
SessionInfo::SessionInfo()
{
    ZeroMemory( &m_SessionID, sizeof( m_SessionID ) );
    ZeroMemory( &m_KeyExchangeKey, sizeof( m_KeyExchangeKey ) );
    ZeroMemory( &m_HostAddress, sizeof( m_HostAddress ) );

    m_dwPublicOpen    = 0;
    m_qwGameType      = TYPE_ANY;
    m_qwGameStyle     = STYLE_ANY;
    m_qwPlayerLevel   = LEVEL_ANY;
    *m_strOwnerName   = 0;
    *m_strSessionName = 0;
    m_ConfigInfo      = NULL_BLOB;
}

//-------------------------------------------------------------------------------------
// Name: SessionInfo()
// Desc: Constructor
//-------------------------------------------------------------------------------------
SessionInfo::SessionInfo( COptiMatchResult& Result )
{
    m_SessionID      = Result.SessionID;
    m_KeyExchangeKey = Result.KeyExchangeKey;
    m_HostAddress    = Result.HostAddress;
    m_dwPublicOpen   = Result.PublicOpen;
    m_qwGameType     = Result.GameType;
    m_qwGameStyle    = Result.GameStyle;
    m_qwPlayerLevel  = Result.PlayerLevel;
    m_ConfigInfo     = Result.ConfigInfo;

    SetOwnerName( Result.OwnerName );
    SetSessionName( Result.SessionName );
}

//-------------------------------------------------------------------------------------
// Name: SessionInfo()
// Desc: Constructor
//-------------------------------------------------------------------------------------
SessionInfo::SessionInfo( CFindSessionByIDResult& Result )
{
    m_SessionID       = Result.SessionID;
    m_KeyExchangeKey  = Result.KeyExchangeKey;
    m_HostAddress     = Result.HostAddress;
    m_dwPublicOpen    = Result.PublicOpen;
    m_qwGameType      = TYPE_ANY;
    m_qwGameStyle     = STYLE_ANY;
    m_qwPlayerLevel   = LEVEL_ANY;
    *m_strOwnerName   = 0;
    *m_strSessionName = 0;
    m_ConfigInfo      = NULL_BLOB;

}

//-------------------------------------------------------------------------------------
// Name: SetGameType()
// Desc: Set session game type
//-------------------------------------------------------------------------------------
VOID SessionInfo::SetGameType( ULONGLONG qwGameType )
{
    m_qwGameType = qwGameType;
}

//-------------------------------------------------------------------------------------
// Name: SetPlayerLevel()
// Desc: Set session player level
//-------------------------------------------------------------------------------------
VOID SessionInfo::SetPlayerLevel( ULONGLONG qwPlayerLevel )
{
    m_qwPlayerLevel = qwPlayerLevel;
}

//-------------------------------------------------------------------------------------
// Name: SetSessionName()
// Desc: Set session name
//-------------------------------------------------------------------------------------
VOID SessionInfo::SetSessionName( const WCHAR* strSessionName )
{
    assert( strSessionName != NULL );
    lstrcpynW( m_strSessionName, strSessionName, XATTRIB_SESSION_NAME_MAX_LEN );
}

//-------------------------------------------------------------------------------------
// Name: SetOwnerName()
// Desc: Set owner name
//-------------------------------------------------------------------------------------
VOID SessionInfo::SetOwnerName( const WCHAR* strOwnerName )
{
    assert( strOwnerName != NULL );
    lstrcpynW( m_strOwnerName, strOwnerName, XONLINE_GAMERTAG_SIZE );
}

//-------------------------------------------------------------------------------------
// Name: SetGameStyle()
// Desc: Set game style
//-------------------------------------------------------------------------------------
VOID SessionInfo::SetGameStyle( ULONGLONG qwGameStyle )
{
    m_qwGameStyle = qwGameStyle;
}

//-------------------------------------------------------------------------------------
// Name: GenRandSessionName()
// Desc: Set name of session to randomly generated value
//-------------------------------------------------------------------------------------
VOID SessionInfo::GenRandSessionName()
{
    XBRandName_GetRandomName( m_strSessionName, XATTRIB_SESSION_NAME_MAX_LEN );
}

//-------------------------------------------------------------------------------------
// Name: SetConfigInfo()
// Desc: Set "configuration" blob
//-------------------------------------------------------------------------------------
VOID SessionInfo::SetConfigInfo( const CBlob & Value )
{
    m_ConfigInfo = Value;
}


//-------------------------------------------------------------------------------------
// Name: SetPresence()
// Desc: Sets the presences of the players with the given XUIDs
//-------------------------------------------------------------------------------------
HRESULT SetPresence( DWORD dwControllerPort, DWORD dwNumXUIDS, XUID* rwXUIDS )
{

    // Step 1
    //
    // Start setting the presence

    CXBOnlineTask hPresenceTask;

    HRESULT hrPresence = XOnlinePresenceInit(
                            dwControllerPort, // Controller port of the user
                            NULL,             // Event to trigger when finished
                            &hPresenceTask ); // Task to assign

    if( FAILED( hrPresence ) )
    {
        hPresenceTask.Close();

        return hrPresence;
    }


    // Step 2
    //
    // Create the group

    // you can create several groups- we're adding everyone to group '0'
    hrPresence = XOnlinePresenceAdd(
                        hPresenceTask, // The task assigned by PresenceInit
                        0,             // ID to assign group
                        dwNumXUIDS,    // Number of XUIDs
                        rwXUIDS );     // Array of XUIDs

    if( FAILED( hrPresence ) )
    {
        hPresenceTask.Close();

        return hrPresence;
    }


    // Step 3
    //
    // Submit the presence

    hrPresence = XOnlinePresenceSubmit( hPresenceTask );

    if ( FAILED( hrPresence ) )
    {
        hPresenceTask.Close();

        return hrPresence;
    }


    // Step 4
    //
    // Pump the task until complete

    WaitForTaskToComplete( hPresenceTask, &hrPresence );


    // Step 5
    //
    // Close the task and return our success

    hPresenceTask.Close();

    return hrPresence;
}

//-------------------------------------------------------------------------------------
// Name: CreateArbitratedRound()
// Desc: Start registering players for an arbitrated game. At this point
//       no more players will be allowed to join, and drops will be recorded
//       against players. The arbitration service will be used to record
//       game results, in order to discourage cheating.
//-------------------------------------------------------------------------------------
HRESULT CreateArbitratedRound( PlayerList& rwPlayers,
                               XONLINE_ARB_ID& arbitrationID,
                               CSession& hostedSession,
                               CXBSocket& networkMessageHandler )
{
    if( rwPlayers.size() == 0 )
    {
        return E_FAIL;
    }


    // The arbitration id is created by the host, and then given to the other players.
    // This number uniquely identifies the game.
    ZeroMemory( &arbitrationID, sizeof( arbitrationID ) );

    // The creation of the session should not necessarily be the host.
    // Instead, the user with the best connection to the other players
    // should be selected.
    // However, to keep this sample as simple as possible, the game creator
    // is used as the host of the arbitrated match.

    // Only the host creates the round id - the round id is then shared with all
    // players.
    arbitrationID.SessionID = hostedSession.SessionID;
    HRESULT hrCreateRound   = XOnlineArbitrationCreateRoundID( &arbitrationID.qwRoundID );

    return hrCreateRound;
}

//-------------------------------------------------------------------------------------
// Name: RegisterForArbitration
// Desc: Attempts to register the current user for the arbitrated game session.
//       Returns FALSE if the registration fails for any reason.
//       If the Xbox is not the host, then a message is sent to the host
//       saying that registration is finished and that the player is
//       ready to start their game session.
//-------------------------------------------------------------------------------------
HRESULT RegisterForArbitration( XONLINE_ARB_ID arbID,
                                BOOL bIsHost,
                                PlayerList& rwPlayers,
                                CXBSocket& networkMessageHandler)
{

    // Step 1
    //
    // Register for arbitration. This should be done once per box.
    // This will register all users who are logged in on the box

    CXBOnlineTask hArbitrationRegisterTask;

    // Specify zero or more flags that describe your competition.
    // The FFA flag implies that players are not on teams. All parameters must
    // be the same for all players of an arbitrated game.
    // If the XONLINE_ARB_REGISTER_FLAG_TIME_EXTENDABLE flag is specified
    // then the players can agree to extend the match time
    // using XOnlineArbitrationExtendRound.
    DWORD dwFlags                 = XONLINE_ARB_REGISTER_FLAG_FFA;
    CONST DWORD MAX_ROUND_SECONDS = 600;

    HRESULT hrRegister = XOnlineArbitrationRegister(
                            &arbID,                   // ID of the arbitrated game
                            MAX_ROUND_SECONDS,        // Length of round in seconds
                            dwFlags,                  // Any flags
                            NULL,                     // Event to trigger when finished
                            &hArbitrationRegisterTask // Task to assign
                         );

    if( FAILED( hrRegister ) )
    {
        return hrRegister;
    }


    // Step 2
    //
    // Pump the task until complete

    // Block execution until the registration process has finished
    if( !WaitForTaskToComplete( hArbitrationRegisterTask, &hrRegister ) )
    {
        return hrRegister;
    }


    // Step 3
    //
    // Now we can ask to see what other boxes have registered.
    // Allocate a buffer for the registrants and zero it.

    XONLINE_ARB_REGISTRANT rwRegistrantsBuffer[ MAX_MATCHERS ] = { 0 };

    // You can find out how many machines have registered so far with
    // the specified arbitration ID. You can then iterate through
    // the results in the RegistrantsBuffer, adding up the users on
    // each machine, to get the total number of users.
    DWORD dwNumRegisteredBoxes = 0;


    HRESULT hrRegResults = XOnlineArbitrationRegisterGetResults(
                                hArbitrationRegisterTask, // Task used to create the round
                                MAX_MATCHERS,             // Maximum number of players in the round
                                rwRegistrantsBuffer,      // Array to place registrants into
                                &dwNumRegisteredBoxes     // Number of boxes registered
                            );

    if( FAILED( hrRegResults ) )
    {
        hArbitrationRegisterTask.Close();

        return hrRegResults;
    }


    // Step 4
    //
    // If we are the host, then registration must be finished. Therefore
    // we should record all of the XUIDs and disconnect any players that
    // did not register.

    if( bIsHost )
    {
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
                    rwPlayerXUIDs[dwNumRegisteredPlayers] =
                        rwRegistrantsBuffer[i].xuidUsers[player];

                    ++dwNumRegisteredPlayers;
                }
            }
        }


        // Step 5
        //
        // Check to make sure enough players registered for the match

        if( dwNumRegisteredPlayers < 2 )
        {
            // Arbitrated sessions must have at least two players. If we
            // don't have that many then we can't proceed. This can happen
            // if the other players quit as the registered game is starting,
            // or if their are connection problems that prevent them from
            // joining.
            hArbitrationRegisterTask.Close();

            return XONLINE_E_ARBITRATION_1_XBOX_1_USER_SESSION_NOT_ALLOWED;
        }
    }


    // Step 6
    //
    // Close the task and return our success

    hArbitrationRegisterTask.Close();

    return S_OK;
}

//-------------------------------------------------------------------------------------
// Name: RegisterWithCompetition()
// Desc: Registers the client with the competition
//-------------------------------------------------------------------------------------
HRESULT RegisterWithCompetition( XONLINE_ARB_ID* pArbitrationID,
                                 WORD wRoundLength )
{

    // Step 1
    //
    // Register the user with the arbitratio ID of the round

    CXBOnlineTask hRegisterTask;

    HRESULT hrRegister = XOnlineCompetitionSessionRegister(
                            pArbitrationID,   // Arb ID
                            wRoundLength,     // Time of round in seconds
                            XONLINE_ARB_REGISTER_FLAG_USER_COMPETITION,
                            NULL,             // Event to be notified when complete
                            &hRegisterTask ); // Task to assign

    if( FAILED( hrRegister ) )
        return hrRegister;


    // Step 2
    //
    // Pump The task until complete

    if( !WaitForTaskToComplete( hRegisterTask, &hrRegister ) )
    {
        hRegisterTask.Close();

        return hrRegister;
    }


    // Step 3
    //
    // Get the results

    XONLINE_ARB_REGISTRANT rwRegistrants[MAX_MATCHERS] = { 0 };
    DWORD                  dwNumRegistrants            = 0;

    HRESULT hrRegisterResults = XOnlineCompetitionSessionRegisterGetResults(
                                    hRegisterTask,       // Task used to start
                                    MAX_MATCHERS,        // Number of atoms in array
                                    rwRegistrants,       // array
                                    &dwNumRegistrants ); // Output: num registrants

    // Step 4
    //
    // Close the task and return our success

    hRegisterTask.Close();

    return hrRegisterResults;
}

//-------------------------------------------------------------------------------------
// Name: CreateSession()
// Desc: Create a game session with the local users registered and the given
//       session info.
//-------------------------------------------------------------------------------------
HRESULT CreateSession( CUserInfo* m_localUsers,
                       DWORD& dwSlotsInUse,
                       CSession& hostedSession,
                       SessionInfo& sessionInfo,
                       CHAR* szGamerTag )
{
    // Generate a random session name if we don't currently have one
    if( *sessionInfo.GetSessionName() == 0 )
    {
        sessionInfo.GenRandSessionName();
    }

    assert( !hostedSession.Exists() );

    // Initialize the create request

    // Set session attributes

    // Game type
    sessionInfo.SetGameType( TYPE_SHORT );
    hostedSession.GameType = sessionInfo.GetGameType();

    // Player level
    sessionInfo.SetPlayerLevel( LEVEL_BEGINNER );
    hostedSession.PlayerLevel = sessionInfo.GetPlayerLevel();


    // Session name
    //---------------------------------------------------------------------
    // Always specified.
    // The second session string parameter.
    assert( *sessionInfo.GetSessionName() != 0 );
    hostedSession.SessionName = sessionInfo.GetSessionName();


    // Game style
    //---------------------------------------------------------------------
    // If not specified, default to STYLE_HEAVY.
    sessionInfo.SetGameStyle( STYLE_HEAVY );
    hostedSession.GameStyle = sessionInfo.GetGameStyle();

    // ConfigInfo
    //---------------------------------------------------------------------
    // The first and only blob attribute.
    // This is only used to demonstrate how to set a blob attribute
    sessionInfo.SetConfigInfo( B( 3, "ABC" ) );
    hostedSession.ConfigInfo = sessionInfo.GetConfigInfo();


    // The first (and only) user parameter is the player name
    WCHAR strUserName[XONLINE_GAMERTAG_SIZE];
    XBUtil_GetWide( szGamerTag,
                    strUserName,
                    XONLINE_GAMERTAG_SIZE );

    sessionInfo.SetOwnerName( strUserName );
    hostedSession.OwnerName = strUserName;

    dwSlotsInUse = 0;

    for( UINT i = 0; i < XGetPortCount(); ++i )
    {
        if( m_localUsers[i].m_bSignedIn )
            ++dwSlotsInUse;
    }

    assert( dwSlotsInUse > 0 );

    // Limit the number of players to MAX_PLAYERS_PER_GAME public slots and no
    // private (invitation only) slots. Note that we add ourself as a player.
    hostedSession.PublicFilled  = dwSlotsInUse;
    hostedSession.PublicOpen    = MAX_MATCHERS - dwSlotsInUse;
    hostedSession.PrivateFilled = 0;
    hostedSession.PrivateOpen   = 0;


    HRESULT hrCreate = hostedSession.Create();

    XBUtil_DebugPrint( "hostedSession.Create() : 0x%x\n", hrCreate );

    if( FAILED( hrCreate ) )
        return hrCreate;

    do
    {
        RenderWorkingScreen();

        hrCreate = hostedSession.Process();
    }
    while( hrCreate == XONLINETASK_S_RUNNING );


    // Step
    //
    // Register the key

    if( SUCCEEDED( hrCreate ) )
    {
        XNetRegisterKey( &hostedSession.SessionID, &hostedSession.KeyExchangeKey );
    }

    return hrCreate;
}

//-------------------------------------------------------------------------------------
// Name: UpdateSession()
// Desc: Updates the hosted session. Allows players to join, dropout, etc.
//-------------------------------------------------------------------------------------
VOID UpdateSession( CSession& hostedSession,
                    DWORD& dwSlotsInUse,
                    BOOL& bArbitrationStarted )
{
    assert( hostedSession.Exists() != NULL );

    hostedSession.PublicFilled = dwSlotsInUse;
    hostedSession.PublicOpen   = MAX_MATCHERS - dwSlotsInUse;

    // Don't let anybody else join.
    if( bArbitrationStarted )
        hostedSession.PublicOpen = 0;

    // A title may call CSession::Update repeatedly without having to
    // wait for the update to complete
    HRESULT hr = hostedSession.Update();

    do
    {
        hr = hostedSession.Process();
    }
    while( hr == XONLINETASK_S_RUNNING );

    assert( SUCCEEDED( hr ) );
}

//-------------------------------------------------------------------------------------
// Name: DeleteSession
// Desc: Deletes the current game session of the current user is host
//       Blocks execution of the game until the task is complete.
//-------------------------------------------------------------------------------------
HRESULT DeleteSession( CSession& hostedSession,
                       PlayerList& rwPlayers )
{
    // Initialize the delete request
    HRESULT hrDelete = hostedSession.Delete();

    if( FAILED(hrDelete) )
    {
        return hrDelete;
    }

    // Continue deletion until it is finished
    do
    {
        hrDelete = hostedSession.Process();
    }
    while ( hrDelete == XONLINETASK_S_RUNNING );

    // Reset the network variables
    rwPlayers.clear();
    hostedSession.Reset();

    return hrDelete;
}

//-------------------------------------------------------------------------------------
// Name: RemoveHostEntry()
// Desc: Stops the round from beining announced.
//       Call this by the host at the end of the match
//-------------------------------------------------------------------------------------
HRESULT RemoveHostEntry( DWORD dwControllerPort, ULONGLONG qwTeamID )
{

    // Step 1
    //
    // Create the data packet

    const PXONLINE_USER rwLoggedOnUsers = XOnlineGetLogonUsers();

    XONLINE_ATTRIBUTE removeAttributes[1];
    removeAttributes[0].dwAttributeID        = MATCH_HOST_USER_ID;
    removeAttributes[0].info.integer.qwValue = rwLoggedOnUsers[dwControllerPort].xuid.qwUserID;


    // Step 2
    //
    // Start removing the hosted session from the servers

    CXBOnlineTask hTask;

    DWORD dwNumAttributes = sizeof( removeAttributes ) / sizeof( removeAttributes[0] );

    HRESULT hrRemove = XOnlineQueryRemove(
                            dwControllerPort,           // Controller port
                            qwTeamID,                   // Team ID
                            MATCH_DATASET,              // Command ID
                            MATCH_REMOVE_HOSTED_EVENTS, // Command
                            dwNumAttributes,            // Number of attributes to remove
                            removeAttributes,           // Array of attributes to remove
                            NULL,                       // Event to signal
                            &hTask );                   // Task to assign

    if( FAILED( hrRemove ) )
    {
        hTask.Close();

        return hrRemove;
    }


    // Step 3
    //
    // pump task until complete, close the task
    // and return our success

    WaitForTaskToComplete( hTask, &hrRemove );

    hTask.Close();

    return hrRemove;
}
