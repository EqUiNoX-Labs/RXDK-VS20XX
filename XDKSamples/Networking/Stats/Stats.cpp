//-----------------------------------------------------------------------------
// File: Stats.cpp
//
// Desc: Shows Xbox online stat API
//
// Hist: 04.10.02    New for May Release
//       07.16.02    Updated for Aug release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "Stats.h"
#include "XBMemUnit.h"
#include "XBVoice.h"
#include <cassert>
#include <algorithm>




//-----------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program.
//-----------------------------------------------------------------------------
VOID __cdecl main()
{
    CXBoxSample xbApp;
    if( FAILED( xbApp.Create() ) )
        return;
    xbApp.Run();
}




//-----------------------------------------------------------------------------
// Name: struct PlayerListCompare
// Desc: Predicate for sorting the player list
//-----------------------------------------------------------------------------
struct PlayerListCompare
{
    // Return true if x should appear before y on player list
    bool operator()( const CPlayerInfo& x, const CPlayerInfo& y ) const
    {
        LONGLONG  llRatingX = x.Stats.GetRating();
        LONGLONG  llRatingY = y.Stats.GetRating();
        
        // In the case of matching rating, players are sorted alphabetically
        if( llRatingX == llRatingY )
            return( wcscmp( x.strUserName, y.strUserName ) < 0 );
       
        // Larger ratings should appear earlier on the list
        return( llRatingX > llRatingY );
    }
};   




//-----------------------------------------------------------------------------
// Name: struct PlayerRankCompare
// Desc: Predicate for sorting based on rank
//-----------------------------------------------------------------------------
struct PlayerRankCompare
{
    // Return true if x should appear before y on a leaderboard
    bool operator()( const CPlayerInfo& x, const CPlayerInfo& y ) const
    {
        LONG lRankX = x.Stats.GetRank();
        LONG lRankY = y.Stats.GetRank();
       
        // Smaller ranks appear earlier on the list
        return( lRankX < lRankY );
    }
};   




//-----------------------------------------------------------------------------
// Name: CXBoxSample()
// Desc: Constructor
//-----------------------------------------------------------------------------
CXBoxSample::CXBoxSample()
{
    m_State           = STATE_SELECT_ACCOUNT;
    m_NextState       = STATE_SELECT_ACCOUNT;
    m_HelpResumeState = STATE_SELECT_ACCOUNT;
    m_dwCurrItem  = 0;
    m_dwCurrUser  = 0;
    m_dwUserIndex = 0;
    m_dwCurrLevel = 0;
    m_dwCurrLeaderBoard = 0;
    m_dwLaunchReason   = XLD_LAUNCH_DASHBOARD_MAIN_MENU;

    // Add whatever services are appropriate for your title, but no
    // more. Each service requires additional authentication time
    // and network traffic.

    // Note: Login to matchmaking service for access to friends
    m_pServices[0] = XONLINE_MATCHMAKING_SERVICE;
    m_pServices[1] = XONLINE_STATISTICS_SERVICE;
    
    *m_strUser = 0;
    m_bAllowBootToDash = FALSE;
    m_bIsLoggedOn = FALSE;
    m_pxuidPagePivot = NULL;
    m_bShowRating = FALSE;
    ZeroMemory( &m_LogonUsers, sizeof( m_LogonUsers ) );
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize device-dependant objects
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    // Initialize game UI
    if( FAILED( m_UI.Initialize() ) )
        return E_FAIL;
    
    // Initialize the network stack
    if( FAILED( XBNet_OnlineInit( 0 ) ) )
        return E_FAIL;
    

    // Get information on all accounts for this Xbox
    if( FAILED( XBOnline_GetUserList( m_UserList ) ) )
        return E_FAIL;
    CXBMemUnit::GetMemUnitSnapshot();     
    
    // If no accounts, then player needs to create an account.
    // For development purposes, accounts are created using the
    // Online Dashboard or the XDK Launcher
    if( m_UserList.size() == 0 )
        m_State = STATE_CREATE_ACCOUNT;
  
    srand( GetTickCount() ); // for picking random stat values
    
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    if( !m_NetLink.IsActive() )
    {
        m_UI.SetErrorStr( L"This Xbox has lost its online connection" );
        Reset();
    }

    if( m_bIsLoggedOn )
    {
        HRESULT hr = m_hOnlineTask.Continue();
        
        if( FAILED( hr ) )
        {
            if( hr == XONLINE_E_LOGON_KICKED_BY_DUPLICATE_LOGON )
                m_UI.SetErrorStr( L"You have been signed out because your\n"
                L"account signed in on another Xbox" );
            else
                m_UI.SetErrorStr( L"Connection was lost. Must relogin" );
            Reset();
        }
    }

    Event ev = GetEvent();
    
    switch( m_State )
    {
    case STATE_CREATE_ACCOUNT:   UpdateStateCreateAccount( ev );      break;
    case STATE_SELECT_ACCOUNT:   UpdateStateSelectAccount( ev );      break;
    case STATE_LOGGING_ON:       UpdateStateLoggingOn( ev );          break;
    case STATE_MAIN_MENU:        UpdateStateMainMenu( ev );           break;
    case STATE_SELECT_LEVEL:     UpdateStateSelectLevel( ev );        break;
    case STATE_END_GAME:         UpdateStateEndGame( ev );            break;
    case STATE_FRIEND_ENUM:      UpdateStateFriendEnum( ev );         break;
    case STATE_FINISH_ENUM:      UpdateStateFinishEnum( ev );         break;
    case STATE_STAT_GET:         UpdateStateStatGet( ev );            break;
    case STATE_STAT_SET:         UpdateStateStatSet( ev );            break;
    case STATE_STAT_LEADER_ENUM: UpdateStateLeaderEnum( ev );         break;
    case STATE_RESET_STATS:      UpdateStateResetStats( ev );         break;
    case STATE_ERROR:            UpdateStateError( ev );              break;
    case STATE_BOOT_TO_DASH:     BootToDash( m_dwLaunchReason );      break;
    case STATE_HELP:             UpdateStateHelp( ev );               break;
    case STATE_VIEW_LEADERBOARD: UpdateStateViewLeaderboard( ev );    break;
    case STATE_VIEW_FRIENDS_STATS: UpdateStateViewFriendsStats( ev ); break;
    default:
        assert( 0 );
    }
    
    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, the call is the entry point for 3d
//       rendering. This function sets up render states, clears the
//       viewport, and renders the scene.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Render()
{
    // Clear the viewport
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL, 
        0x000A0A6A, 1.0f, 0L );
    
    switch( m_State )
    {
    case STATE_CREATE_ACCOUNT:
        m_UI.RenderCreateAccount( TRUE );
        break;
    case STATE_SELECT_ACCOUNT:
        m_UI.RenderSelectAccount( m_dwCurrItem, m_UserList );
       break;
    case STATE_LOGGING_ON:
        m_UI.RenderLoggingOn( m_LogonUsers );
        break;
    case STATE_MAIN_MENU:
        m_UI.RenderMainMenu( m_dwCurrItem, m_strUser );
        break;
    case STATE_SELECT_LEVEL:
        m_UI.RenderSelectLevel( m_dwCurrItem );
        break;
    case STATE_END_GAME:
        m_UI.RenderEndGame( m_dwCurrLevel, m_Players );
        break;
    case STATE_ERROR:
        m_UI.RenderError( m_bAllowBootToDash );
        break;
    case STATE_BOOT_TO_DASH:
        break;
    case STATE_HELP:
        m_UI.RenderHelp();
        break;
    case STATE_FRIEND_ENUM:
        m_UI.RenderFriendEnum();
        break;
    case STATE_FINISH_ENUM:
        m_UI.RenderFinishFriendEnum();
        break;
    case STATE_STAT_GET:
    case STATE_STAT_LEADER_ENUM:
    case STATE_VIEW_FRIENDS_STATS:
        m_UI.RenderReadStats();
        break;
    case STATE_STAT_SET:
        m_UI.RenderWriteStats();
        break;
    case STATE_VIEW_LEADERBOARD:
        m_UI.RenderLeaderboard( m_pxuidPagePivot, m_dwCurrLeaderBoard, 
                                m_bShowRating, m_LeaderboardUsers );
        break;
    case STATE_RESET_STATS:
        m_UI.RenderResetStats( m_strUser );
        break;
    default:
        assert( FALSE );
        break;
    }
    
    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );
    
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: GetEvent()
// Desc: Return the state of the controller
//-----------------------------------------------------------------------------
CXBoxSample::Event CXBoxSample::GetEvent() const
{
    // "A" or "Start"
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_A ] ||
        m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_START )
    {
        return EV_BUTTON_A;
    }
    
    // "B"
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_B ] )
        return EV_BUTTON_B;
    
    // "X"
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_X ] )
        return EV_BUTTON_X;
    
    // "Y"
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_Y ] )
        return EV_BUTTON_Y;

    // "Back"
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_BACK )
        return EV_BUTTON_BACK;

    // "White"
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_WHITE ] )
        return EV_BUTTON_WHITE;
    
    // "Black"
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_BLACK ] )
        return EV_BUTTON_BLACK;

    // Movement
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
        return EV_UP;
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
        return EV_DOWN;
    
    return EV_NULL;
}




//-----------------------------------------------------------------------------
// Name: BootToDash()
// Desc: Boot to the dash
//-----------------------------------------------------------------------------
VOID CXBoxSample::BootToDash( DWORD dwReason )
{
    
    // Return to Dashboard. Retail Dashboard will include
    // online account creation. Development XDK Launcher
    // includes the XDK Launcher or Xbox OnlineDash for creating accounts.
    LD_LAUNCH_DASHBOARD ld;
    ZeroMemory( &ld, sizeof(ld) );
    ld.dwReason = dwReason;
    XLaunchNewImage( NULL, PLAUNCH_DATA( &ld ) );
}




//-----------------------------------------------------------------------------
// Name: UpdateStateCreateAccount()
// Desc: Allow player to launch account creation tool
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateCreateAccount( Event ev )
{
    switch( ev )
    {
    case EV_BUTTON_A:
        // Boot into the new user signup area in the online dash
        BootToDash( XLD_LAUNCH_DASHBOARD_NEW_ACCOUNT_SIGNUP );
        break;
    case EV_BUTTON_WHITE:
        m_HelpResumeState = m_State;
        m_State = STATE_HELP;
        break;
    default:
        // If any MUs are inserted, update the user list
        // and go to account selection if there are any accounts
        DWORD dwInsertions;
        DWORD dwRemovals;
        if( CXBMemUnit::GetMemUnitChanges( dwInsertions, dwRemovals ) )
        {
            m_UserList.clear();
            XBOnline_GetUserList( m_UserList );
            if( !m_UserList.empty() )
                m_State = STATE_SELECT_ACCOUNT;
        }
        break;
       
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateStateSelectAccount()
// Desc: Allow player to choose account
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateSelectAccount( Event ev )
{
    switch( ev )
    {
    case EV_BUTTON_A:
        {
            // Save current account information
            m_dwCurrUser = m_dwCurrItem;
            
            // Make WCHAR copy of user name
            XBUtil_GetWide( m_UserList[ m_dwCurrUser ].szGamertag, m_strUser, 
                XONLINE_GAMERTAG_SIZE );
            m_State = STATE_LOGGING_ON;
            BeginLogin();
            break;
        }
        
    case EV_BUTTON_WHITE:
        m_HelpResumeState = m_State;
        m_State = STATE_HELP;
        break;

    case EV_UP:
        // Move to previous user account; allow wrap to bottom
        if( m_dwCurrItem == 0 )
            m_dwCurrItem = m_UserList.size() - 1;
        else
            --m_dwCurrItem;
        break;
        
    case EV_DOWN:
        // Move to next user account; allow wrap to top
        if( m_dwCurrItem == m_UserList.size() - 1 )
            m_dwCurrItem = 0;
        else
            ++m_dwCurrItem;
        break;
        
    default:
        // If any MUs are inserted/removed, need to update the
        // user account list
        DWORD dwInsertions;
        DWORD dwRemovals;
        if( CXBMemUnit::GetMemUnitChanges( dwInsertions, dwRemovals ) )
        {
            m_UserList.clear();
            XBOnline_GetUserList( m_UserList );
            if( m_UserList.empty() )
                m_State = STATE_CREATE_ACCOUNT;
            else
                m_dwCurrItem = 0;
        }
        break;
    }
}







//-----------------------------------------------------------------------------
// Name: UpdateStateLoggingOn()
// Desc: Authentication is underway
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateLoggingOn( Event ev )
{
    switch( ev )
    {
    case EV_BUTTON_B:
    case EV_BUTTON_BACK:
        {
            // Close the task (this cancels the logon process)
            m_hOnlineTask.Close();
            
            // Return to list of user accounts
            m_State = STATE_SELECT_ACCOUNT;
            return;
        }
    }
    
    HRESULT hr = m_hOnlineTask.Continue();
    
    if ( hr != XONLINETASK_S_RUNNING )
    {
        if ( hr != XONLINE_S_LOGON_CONNECTION_ESTABLISHED  )
        {
            HandleSignOnError( hr );
            return;
        }
        
        
        // Next, check if each user was actually logged on
        PXONLINE_USER pLoggedOnUsers = XOnlineGetLogonUsers();
        
        assert( pLoggedOnUsers );

        for( DWORD i = 0; i < m_Players.size(); ++i )
        {
            hr = pLoggedOnUsers[ i ].hr;
            
            if( FAILED( hr ) )
            {
                HandleUserSignOnError( hr );
                return;
            }
        }
        
  

        // Check for service errors
        // RXDK: hoisted out of the for-init (MSVC's old for-scope leaked it to the later loop)
        DWORD i;
        for( i = 0; i < NUM_SERVICES; ++i )
        {
            hr = XOnlineGetServiceInfo( m_pServices[i], NULL );
            if( FAILED( hr ) )
            {
                HandleServiceError( hr, m_pServices[i] );
                return;
            }
        }

        m_bIsLoggedOn = TRUE; // We are now logged on

        m_ServiceInfoList.clear();
        for( i = 0; i < NUM_SERVICES; ++i )
        {
            // Stored service information for UI
            XONLINE_SERVICE_INFO serviceInfo;
            XOnlineGetServiceInfo( m_pServices[i], &serviceInfo );
            m_ServiceInfoList.push_back( serviceInfo );
        }
        
        // Notify the world of our state change
        DWORD dwState = XONLINE_FRIENDSTATE_FLAG_ONLINE;
        if( XBVoice_HasDevice() )
            dwState |= XONLINE_FRIENDSTATE_FLAG_VOICE;
        SetPlayerState( dwState );

        // Check if there are any messages for the currently logged on user
        // In a real game, you would check each logged on user
        if( pLoggedOnUsers[ m_dwUserIndex ].hr == 
                XONLINE_S_LOGON_USER_HAS_MESSAGE )
        {
            m_UI.SetErrorStr( L" One or more messages are available.\n"
                              L" You may read them by visiting the Xbox Dashboard." );
            m_bAllowBootToDash = TRUE;
            m_dwLaunchReason = XLD_LAUNCH_DASHBOARD_ACCOUNT_MANAGEMENT;
            m_State = STATE_ERROR;
            m_NextState = STATE_FRIEND_ENUM;
        }
        else
        {
            m_State = STATE_FRIEND_ENUM;
        }
    }
}





//-----------------------------------------------------------------------------
// Name: UpdateStateError()
// Desc: An error occurred
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateError( Event ev )
{
    switch( ev )
    {
    case EV_BUTTON_A:
        // A exits
        m_State = m_NextState;
        m_bAllowBootToDash = FALSE;
        break;
    case EV_BUTTON_X:
        if( m_bAllowBootToDash )
        {
           BootToDash( m_dwLaunchReason );
        }
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateStateHelp()
// Desc: Handle help screen
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateHelp( Event ev )
{
    switch( ev )
    {
    case EV_BUTTON_A:
    case EV_BUTTON_B:
    case EV_BUTTON_WHITE:
        m_State = m_HelpResumeState;
        break;
    }
}




//-----------------------------------------------------------------------------
// Name: BeginLogin()
// Desc: Initiate the authentication process
//-----------------------------------------------------------------------------
VOID CXBoxSample::BeginLogin()
{
    assert( MAX_PLAYERS <= XONLINE_MAX_LOGON_USERS );
    
    m_hOnlineTask.Close(); // Close existing task handle       

    // Select a reasonable controller for the current player by choosing
    // the first controller found. Game code should do this much more
    // precisely. See below for details.
    for( m_dwUserIndex = 0; m_dwUserIndex < XGetPortCount(); ++m_dwUserIndex )
    {
        if( m_Gamepad[m_dwUserIndex].hDevice )
            break;
    }
    if( m_dwUserIndex >= XONLINE_MAX_LOGON_USERS )
        m_dwUserIndex = 0;
    
    // XOnlineLogon() allows a list of up to 4 players (1 per controller)
    // to login in a single call. This sample shows how to authenticate
    // a single user. The list must be a one-to-one match of controller 
    // to player in order for the online system to recognize which player
    // is using which controller.
    ZeroMemory( &m_LogonUsers, sizeof( m_LogonUsers ) );

    // Populate the logon user list.  In addition to the logging on the
    // user selected account, log on additional accounts as "players" 
    // so that we can write stats for them
    DWORD dwNumPlayers = min( MAX_PLAYERS, m_UserList.size() );
    DWORD dwUser = m_dwCurrUser;
    DWORD dwIndex = m_dwUserIndex;

    m_Players.clear();

    while( m_Players.size() < dwNumPlayers )
    {

        CopyMemory( &m_LogonUsers[ dwIndex ], &m_UserList[ dwUser ],
            sizeof( XONLINE_USER ) );

        CPlayerInfo Info;
        Info.xuid = m_UserList[ dwUser ].xuid;
        XBUtil_GetWide( m_UserList[ dwUser ].szGamertag, Info.strUserName, 
            XONLINE_GAMERTAG_SIZE );

        m_Players.push_back( Info );

        // Advance ( possibly wrapping around ) to the next player and
        // controller indices
        dwUser = ( dwUser + 1 ) % m_UserList.size();
        dwIndex = ( dwIndex + 1 ) % XONLINE_MAX_LOGON_USERS;
       
    } 

    // Initiate the login process. XOnlineTaskContinue() is used to poll
    // the status of the login.
    HRESULT hr = XOnlineLogon( m_LogonUsers, m_pServices, NUM_SERVICES, 
        NULL, &m_hOnlineTask );
    
    if( FAILED(hr) )
    {
        HandleSignOnError( hr );
    }
}




//-----------------------------------------------------------------------------
// Name: HandleSignOnError()
// Desc: Handle system logon errors
//-----------------------------------------------------------------------------
VOID CXBoxSample::HandleSignOnError( HRESULT hr )
{
    HRESULT hrTitleUpdate;

    m_State = STATE_ERROR;
    m_NextState = STATE_SELECT_ACCOUNT;

    switch( hr )
    {
    case XONLINE_E_LOGON_CONNECTION_LOST:
        m_UI.SetErrorStr( L"Network connection lost." );
        m_bAllowBootToDash = TRUE;
        m_dwLaunchReason = XLD_LAUNCH_DASHBOARD_NETWORK_CONFIGURATION;
        break;
    case XONLINE_E_LOGON_INVALID_USER:
        m_UI.SetErrorStr( L"This account is not current.\nPress A to update the account in Account Recovery\nor B to cancel." );
        m_bAllowBootToDash = TRUE;
        m_dwLaunchReason = XLD_LAUNCH_DASHBOARD_ACCOUNT_MANAGEMENT;
        break;
    case XONLINE_E_LOGON_CANNOT_ACCESS_SERVICE:
        m_UI.SetErrorStr( L"Login failed with error 0x%x", hr );
        m_bAllowBootToDash = TRUE;    
        m_dwLaunchReason = XLD_LAUNCH_DASHBOARD_NETWORK_CONFIGURATION;
        break;
    case XONLINE_E_LOGON_UPDATE_REQUIRED:
        hrTitleUpdate = XOnlineTitleUpdate( 0 ); 
        // If successful, the system will reboot and update
        assert( FALSE );  // Should have rebooted!
        m_UI.SetErrorStr( L"Title update failed with error 0x%x", hrTitleUpdate );
        break;
    case XONLINE_E_LOGON_SERVERS_TOO_BUSY:
        m_UI.SetErrorStr( L"The Xbox Live Sign In servers are too busy. "
                          L"Try again later." );
        break;
    default:
        m_bAllowBootToDash = TRUE;      
        m_UI.SetErrorStr( L"Login failed with error 0x%x", hr );

    }

}




//-----------------------------------------------------------------------------
// Name: HandleUserSignOnError()
// Desc: Handle user logon errors
//-----------------------------------------------------------------------------
VOID CXBoxSample::HandleUserSignOnError( HRESULT hr )
{
    m_State = STATE_ERROR;

    switch( hr ) 
    {
    case XONLINE_E_LOGON_USER_ACCOUNT_REQUIRES_MANAGEMENT:
        // When rebooting to account management the main
        // logon task (m_hOnlineTask) MUST NOT BE CLOSED
        m_NextState = STATE_BOOT_TO_DASH;
        m_UI.SetErrorStr( L"This account requires user management" );
        m_dwLaunchReason = XLD_LAUNCH_DASHBOARD_ACCOUNT_MANAGEMENT;
        break;
    default:
        m_NextState = STATE_SELECT_ACCOUNT;
        m_UI.SetErrorStr( L"User Login failed with error 0x%x", hr );
    }


}




//-----------------------------------------------------------------------------
// Name: HandleServiceError()
// Desc: Handle service errors
//-----------------------------------------------------------------------------
VOID CXBoxSample::HandleServiceError( HRESULT hr, DWORD dwServiceId )
{
    m_State = STATE_ERROR;
    m_NextState = STATE_SELECT_ACCOUNT;

    switch( hr )
    {
    case XONLINE_E_LOGON_SERVICE_NOT_AUTHORIZED:
        m_UI.SetErrorStr( L"Access to service %d is denied",
            dwServiceId );
        
        break;
    case XONLINE_E_LOGON_SERVICE_TEMPORARILY_UNAVAILABLE:
        m_UI.SetErrorStr( L"Service %d is unavailable",
            dwServiceId );
        
        break;
    default:
        
        m_UI.SetErrorStr( L"Error 0x%x logging into service %d",
            hr, dwServiceId );
    }
}




//-----------------------------------------------------------------------------
// Name: SetPlayerState()
// Desc: Broadcast current player state for the world
//-----------------------------------------------------------------------------
VOID CXBoxSample::SetPlayerState( DWORD dwState )
{
    HRESULT hr = XOnlineNotificationSetState( m_dwUserIndex, dwState,
        XNKID(), 0, NULL );
    assert( SUCCEEDED( hr ) );
    (VOID)hr; // avoid compiler warning
}




//-----------------------------------------------------------------------------
// Name: UpdateStateFriendEnum()
// Desc: Start/Continue friend enumeration
//----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateFriendEnum( Event ev )
{
    HRESULT hr;

    switch( ev )
    {
    case EV_BUTTON_B:
        // Cancel the task
        m_NextState = STATE_SELECT_ACCOUNT;
        if( m_hFriendEnumTask )
            FinishFriendEnum();
        else
        {
            m_hFriendsTask.Close();
            m_State = STATE_SELECT_ACCOUNT;
        }
        return;
    }


    if( m_hFriendsTask )
    {
        hr = m_hFriendsTask.Continue();

        if( SUCCEEDED( hr ) )
            hr = m_hFriendEnumTask.Continue();

        if( FAILED( hr ) )
        {
            m_UI.SetErrorStr( L"Friend enumeration failed. Error 0x%x", hr );
            Reset();
            return;
        }
        
        if( hr == XONLINETASK_S_RESULTS_AVAIL )
        {
            // The friends list is now available for us to retrieve
            // Reserve space for the friend list
            m_FriendList.resize( MAX_FRIENDS );

            XONLINE_FRIEND *pFriendList = &m_FriendList[0];
            DWORD dwNumFriends = XOnlineFriendsGetLatest( m_dwUserIndex,
                MAX_FRIENDS,
                pFriendList );

            // Resize to the actual number retrieved
            m_FriendList.resize( dwNumFriends );

            // Remove any "pending" friends
            for( DWORD i = 0; i < m_FriendList.size(); )
            {
                DWORD dwFriendState = m_FriendList[i].dwFriendState;
                // Player has sent you a request to be a friend
                BOOL bFriendRequest  = dwFriendState &
                    XONLINE_FRIENDSTATE_FLAG_RECEIVEDREQUEST;

                // You sent this player a  request to be a friend
                BOOL bSentRequest    = dwFriendState & 
                    XONLINE_FRIENDSTATE_FLAG_SENTREQUEST;

                if( bFriendRequest || bSentRequest )
                {
                    m_FriendList.erase( m_FriendList.begin() + i );
                }
                else
                    ++i; // Advance to next friend
            }

            // Normally, a title would continually call XOnlineTaskContinue
            // on the friend enumeration task, checking for updates, but for
            // the purposes of this sample, we just want a snapshot for
            // demonstration purposes.

            // Finish off the enumeration and then display the main menu
            m_NextState = STATE_MAIN_MENU;
            FinishFriendEnum();
        }

    }
    else
    {
        // Kick off friend enumeration
        // Standard init
        hr = XOnlineFriendsStartup( NULL, &m_hFriendsTask );
        if( FAILED(hr) )
        {
            m_UI.SetErrorStr( L"Friends failed to initialize. Error 0x%x", hr );
            Reset();
        }
        else
        {
            // Query server for latest list of friends
            hr = XOnlineFriendsEnumerate( m_dwUserIndex, NULL, &m_hFriendEnumTask );
            if( FAILED(hr) )
            {
                m_UI.SetErrorStr( L"Friend enum failed to initialize. Error 0x%x", hr );
                Reset();
            }
        }
    }
}




//-----------------------------------------------------------------------------
// Name: FinishFriendEnum()
// Desc: Finish friend enumeration
//----------------------------------------------------------------------------
VOID CXBoxSample::FinishFriendEnum()
{

    assert( m_hFriendEnumTask != NULL );
    // Begin the process of ending friend enumeration.
    HRESULT hr = XOnlineFriendsEnumerateFinish( m_hFriendEnumTask );
    if( FAILED( hr ) )
    {
        m_UI.SetErrorStr( L"XOnlineFriendsEnumerateFinish failed.\n"
            L"\nError 0x%x ", hr );
        Reset();
    }
    else
    {
        m_State = STATE_FINISH_ENUM;
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateStateFinishEnum()
// Desc: Continue the process of ending friend enumeration
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateFinishEnum( Event ev )
{
    HRESULT hr = m_hFriendEnumTask.Continue();

    if( hr != XONLINETASK_S_RUNNING )
    {
        if( FAILED( hr ) )
        {
            m_UI.SetErrorStr( L"Failed to finish friend enumeration.\n"
                L"\nError 0x%x returned by " 
                L"XOnlineTaskContinue", hr );
            Reset();
        }
        else
        {
            // Finished.  Transition to m_NextState
            if( m_NextState == STATE_SELECT_ACCOUNT )
            {
                // Going to account select next, so 
                // make sure to log off first
                m_bIsLoggedOn = FALSE;
                m_hOnlineTask.Close();        
            }
            m_hFriendsTask.Close();
            m_hFriendEnumTask.Close();          
            m_dwCurrItem  = 0;
            m_State = m_NextState;  // Transition to desired next state
        }
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateStateMainMenu()
// Desc: Menu Menu
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateMainMenu( Event ev )
{
    switch( ev )
    {
    case EV_BUTTON_A:
        switch( m_dwCurrItem)
        {
        case ACTION_NEW_STATISTICS:
            CreateEndGame();
            m_dwCurrItem = 0;
            m_State = STATE_SELECT_LEVEL;
            // Transition to end game state after selection
            m_NextState = STATE_END_GAME;
            break;
        case ACTION_RESET_STATISTICS:
            m_State = STATE_RESET_STATS;
            break;
        case ACTION_USER_STATS:
            // Enumerate level leader board and pivot on user
            m_pxuidPagePivot = &m_UserList[m_dwCurrUser].xuid;
            m_dwCurrItem = 0;
            m_State = STATE_SELECT_LEVEL;
            // Transition to enumeration after selection
            m_NextState = STATE_STAT_LEADER_ENUM;
            break;
        case ACTION_OVERALL_STATS:
            // Enumerate overall leader board without a pivot
            m_pxuidPagePivot = NULL;
            m_dwCurrLeaderBoard = OVERALL_LEADERBOARD_ID;
            m_State = STATE_STAT_LEADER_ENUM;
            break;
        case ACTION_FRIENDS_STATS:
            m_pxuidPagePivot = &m_UserList[m_dwCurrUser].xuid;
            m_dwCurrItem = 0;
            m_dwCurrLeaderBoard = OVERALL_LEADERBOARD_ID;
            BeginReadFriendsStats();
            break;
        }
        break;
    case EV_BUTTON_B:
    case EV_BUTTON_BACK:
        m_bIsLoggedOn = FALSE;
        m_hOnlineTask.Close();
        m_State = STATE_SELECT_ACCOUNT;
        m_dwCurrItem = 0;
        break;
    case EV_BUTTON_WHITE:
        m_HelpResumeState = m_State;
        m_State = STATE_HELP;
        break;
    case EV_UP:
        // Move to previous choice; allow wrap to bottom
        if( m_dwCurrItem == 0 )
            m_dwCurrItem = ACTION_MAX - 1;
        else
            --m_dwCurrItem;
        break;
        
    case EV_DOWN:
        // Move to next choice; allow wrap to top
        if( m_dwCurrItem == ACTION_MAX - 1 )
            m_dwCurrItem = 0;
        else
            ++m_dwCurrItem;
        break;
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateStateSelectLevel()
// Desc: Allow player to choose a level
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateSelectLevel( Event ev )
{
    switch( ev )
    {
    case EV_BUTTON_A:
        {
            // Save current level
            m_dwCurrLevel = m_dwCurrItem;
            m_dwCurrLeaderBoard = LevelIDToLeaderBoardID( m_dwCurrLevel );
            m_dwCurrItem = 0;
            m_State = m_NextState;
            break;
        }
        
    case EV_BUTTON_B:
    case EV_BUTTON_BACK:
        m_State = STATE_MAIN_MENU;
        break;

    case EV_BUTTON_WHITE:
        m_HelpResumeState = m_State;
        m_State = STATE_HELP;
        break;

    case EV_UP:
        // Move to previous level; allow wrap to bottom
        if( m_dwCurrItem == 0 )
            m_dwCurrItem = NUM_LEVELS - 1;
        else
            --m_dwCurrItem;
        break;
        
    case EV_DOWN:
        // Move to next level; allow wrap to top
        if( m_dwCurrItem == NUM_LEVELS - 1 )
            m_dwCurrItem = 0;
        else
            ++m_dwCurrItem;
        break;
        
    default:
        break;
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateStateEndGame()
// Desc: End of game menu
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateEndGame( Event ev )
{
    switch( ev )
    {
    case EV_BUTTON_A:
        BeginWriteGameStats();
        break;
    case EV_BUTTON_B:
    case EV_BUTTON_BACK:
        m_State = STATE_SELECT_LEVEL;
        m_NextState = STATE_END_GAME;        
        m_dwCurrItem = 0;
        break;
    case EV_BUTTON_Y:
        CreateEndGame();
        break;
    case EV_BUTTON_WHITE:
        m_HelpResumeState = m_State;
        m_State = STATE_HELP;
        break;
    }
}




//-----------------------------------------------------------------------------
// Name: BeginWriteGameStats()
// Desc: Update level and overall leaderboards with player stats
//-----------------------------------------------------------------------------
VOID CXBoxSample::BeginWriteGameStats()
{

    // First, read the existing stats for the current level
    // and overall leaderboards.
    // XOnlineStatRead wants an array of XONLINE_STAT_SPEC
    // structures.  Each of these specifies a user id (XUID)
    // as well as a leaderboard id.  For each player
    // we want the existing stats from the current level
    // leaderboard as well as the overall one.  To do this,
    // we first populate the m_StatSpecs array with two entries
    // per user: one requesting the current level leaderboard
    // and one requesting the overall leaderboard
    DWORD j = 0;
    m_StatSpecList.resize( m_Players.size()*2 );

    for( DWORD i = 0; i < m_Players.size(); ++i )
    {
        
        DWORD dwNumStats;
        
        // Stat specs for the current level leaderboard
        m_LevelStats[i].Clear();
        m_StatSpecList[j].xuidUser = m_Players[i].xuid;
        m_StatSpecList[j].dwLeaderBoardID = m_dwCurrLeaderBoard;
        // Get the internal XONLINE_STAT array and size and
        // pass this directly so that the attributes are 
        // filled in place
        m_StatSpecList[j].pStats = 
            m_LevelStats[i].GetReadStats( &dwNumStats );
        m_StatSpecList[j].dwNumStats = dwNumStats;
        j++;
        
        // Stat specs for the overall leaderboard
        m_OverallStats[i].Clear();
        m_StatSpecList[j].xuidUser = m_Players[i].xuid;
        m_StatSpecList[j].dwLeaderBoardID = OVERALL_LEADERBOARD_ID;
        // Get the internal XONLINE_STAT array and size and
        // pass this directly so that the attributes are 
        // filled in place
        m_StatSpecList[j].pStats = 
            m_OverallStats[i].GetReadStats( &dwNumStats );
        m_StatSpecList[j].dwNumStats = dwNumStats;
        j++;
    }
    
    HRESULT hr = XOnlineStatRead(  m_Players.size() * 2,
        &m_StatSpecList[0], NULL, & m_hStatsReadTask );
    if( FAILED( hr ) )
    {
        m_UI.SetErrorStr( L"XOnlineStatRead Failed with 0x%x", hr );
        Reset();
    }
    else 
    {
        // First, retrieve the existing stats
        m_State = STATE_STAT_GET;
        // As soon as the stats are received, the state machine will
        // transition to m_StateNext.  We want to update the
        // stats we fetch and write them back out.
        m_NextState = STATE_STAT_SET;
    }
    
}




//-----------------------------------------------------------------------------
// Name: BeginReadFriendsStats()
// Desc: Read the current level stats for our friends
//-----------------------------------------------------------------------------
VOID CXBoxSample::BeginReadFriendsStats()
{

    if( m_FriendList.empty() )
    {
        m_State = STATE_ERROR;
        m_NextState = STATE_MAIN_MENU;
        m_UI.SetErrorStr( L"Your Friends list is empty" );
        return;
    }

    // XOnlineStatRead wants an array of XONLINE_STAT_SPEC
    // structures.  Each of these specifies a user id (XUID)
    // as well as a leaderboard id.  
    m_LeaderboardUsers.clear();
    m_StatSpecList.clear();
    DWORD dwNumUsers = min( m_FriendList.size(), MAX_STAT_USERS - 1 );

    // Resize to be large enough for the number of
    // friends and the current user
    m_StatSpecList.resize( dwNumUsers + 1 );
    m_LeaderboardUsers.reserve( dwNumUsers + 1 );

    for( DWORD i = 0; i < dwNumUsers; ++i )
    {
        
        DWORD dwNumStats;
        CPlayerInfo Info;
        
        // Add friend as a leaderboard user
        Info.xuid = m_FriendList[i].xuid;
        XBUtil_GetWide( m_FriendList[i].szGamertag, Info.strUserName, 
            XONLINE_GAMERTAG_SIZE );
        m_LeaderboardUsers.push_back( Info );
    
        // Stat specs for the current level leaderboard
        m_StatSpecList[i].xuidUser = m_FriendList[i].xuid;
        m_StatSpecList[i].dwLeaderBoardID = m_dwCurrLeaderBoard;
        // Get the internal XONLINE_STAT array and size of the
        // newly added leaderboard user and
        // pass this directly so that the attributes are 
        // filled in place
        m_StatSpecList[i].pStats = 
            m_LeaderboardUsers[i].Stats.GetReadStats( &dwNumStats );
        m_StatSpecList[i].dwNumStats = dwNumStats;
        
    }

    // Add current user
    CPlayerInfo Info;
    DWORD dwNumStats;
    Info.xuid = m_UserList[ m_dwCurrUser ].xuid;
    XBUtil_GetWide( m_UserList[ m_dwCurrUser ].szGamertag, Info.strUserName, 
        XONLINE_GAMERTAG_SIZE );
    m_LeaderboardUsers.push_back( Info );
    
    m_StatSpecList[dwNumUsers].xuidUser = Info.xuid;
    m_StatSpecList[dwNumUsers].dwLeaderBoardID = m_dwCurrLeaderBoard;
    m_StatSpecList[dwNumUsers].pStats = 
        m_LeaderboardUsers[dwNumUsers].Stats.GetReadStats( &dwNumStats );
    m_StatSpecList[dwNumUsers].dwNumStats = dwNumStats;
    

        
    HRESULT hr = XOnlineStatRead( m_StatSpecList.size(),
        &m_StatSpecList[0], NULL, & m_hStatsReadTask );
    if( FAILED( hr ) )
    {
        m_UI.SetErrorStr( L"XOnlineStatRead Failed with 0x%x", hr );
        Reset();
    }
    else 
    {
        // First, retrieve the existing stats
        m_State = STATE_STAT_GET;
        // As soon as the stats are received, the state machine will
        // transition to m_StateNext.  We want to post process the
        // data returned for our friends in order to remove entries
        // which have no actual stats
        m_NextState = STATE_VIEW_FRIENDS_STATS;
    }
    
}




//-----------------------------------------------------------------------------
// Name: UpdateStateViewFriendsStats()
// Desc: Post process the stats for our friends, before displaying them
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateViewFriendsStats( Event ev )
{
    
    switch( ev )
    {
    case EV_BUTTON_B:
    case EV_BUTTON_BACK:
        m_State = STATE_MAIN_MENU;
        m_dwCurrItem = 0;
        return;
    }

    // The m_LeaderboardUsers now has the stats for our friends.
    // Remove friends which have no stats for the current level
    for( DWORD i = 0; i < m_LeaderboardUsers.size(); )
    {
        if( m_LeaderboardUsers[i].Stats.Missing() )
        {
            m_LeaderboardUsers.erase( m_LeaderboardUsers.begin() + i );
        }
        else
            ++i; // Advance to next friend
    }

    if( !m_LeaderboardUsers.empty() )
    {
        // Sort the leaderboard by player rank
        std::sort( m_LeaderboardUsers.begin(), m_LeaderboardUsers.end(), 
            PlayerRankCompare() );
        m_bShowRating = FALSE;
        m_State = STATE_VIEW_LEADERBOARD;
    }
    else
    {
        m_State = STATE_ERROR;
        m_NextState = STATE_MAIN_MENU;
        m_UI.SetErrorStr( L"No Friend Statistics" );
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateStateStatGet()
// Desc: Continue processing an XOnlineStatRead task
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateStatGet( Event ev )
{
    
    switch( ev )
    {
    case EV_BUTTON_B:
    case EV_BUTTON_BACK:
        m_hStatsReadTask.Close();
        m_State = STATE_MAIN_MENU;
        m_dwCurrItem = 0;
        return;
    }

    HRESULT hr = m_hStatsReadTask.Continue();

    if ( hr != XONLINETASK_S_RUNNING )
    {
        if( FAILED( hr ) )
        {
            m_UI.SetErrorStr( L"XOnlineStatRead Failed with 0x%x", hr );
            Reset();
        }
        else
        {
            hr = XOnlineStatReadGetResult( m_hStatsReadTask,
                     m_StatSpecList.size(), &m_StatSpecList[0], 0, NULL );

            if( FAILED( hr ) )
            {
                m_UI.SetErrorStr( L"XOnlineStatReadGetResult Failed with 0x%x",
                          hr );
                Reset();
            }
            else
            {
                // Resume to the state specified by m_NextState
                m_hStatsReadTask.Close();
                m_State = m_NextState;                
            }
        }
    }
    
}




//-----------------------------------------------------------------------------
// Name: UpdateStateStatSet()
// Desc: Continue processing an XOnlineStatWrite task
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateStatSet( Event ev )
{
    HRESULT hr;

    switch( ev )
    {
    case EV_BUTTON_B:
    case EV_BUTTON_BACK:
        m_hStatsWriteTask.Close();
        m_State = STATE_MAIN_MENU;
        m_dwCurrItem = 0;
        return;
    }

    // Check if the write task has already been started
    if( m_hStatsWriteTask )
    {
        hr = m_hStatsWriteTask.Continue();
        
        if ( hr != XONLINETASK_S_RUNNING )
        {
            if( FAILED( hr ) )
            {
                m_UI.SetErrorStr( L"XOnlineStatWrite Failed with 0x%x", hr );
                Reset();
            }
            else
            {
                m_hStatsWriteTask.Close();
                m_UI.SetErrorStr( L"Stats Updated" );
                m_State = STATE_ERROR;
                m_NextState = STATE_MAIN_MENU;
            }
        }
    }
    else
    {

        // Update the numbers...
        // XOnlineStatWrite wants an array of XONLINE_STAT_SPEC
        // structures.  Each of these specifies a user id (XUID)
        // as well as a leaderboard id.  For each player
        // we want to write the stats for the current level
        // leaderboard as well as the overall one.  To do this,
        // we first populate the m_StatSpecs array with two entries
        // per user: one specifying the  new stats for the
        // current level leaderboard and one specifying the new stats for
        // the overall leaderboard
        DWORD j = 0;
        // Update the m_StatSpecList, which was filled in when we read
        // the existing stats

        assert( m_StatSpecList.size() == m_Players.size() * 2 );
        
        for( DWORD i = 0; i < m_Players.size(); ++i )
        {
            DWORD dwNumStats;

            // Get the internal XONLINE_STAT array for the current
            // level.  Pass this directly to XOnlineStatWrite so that
            // it can get at the values directly,
            m_StatSpecList[j].pStats = 
                m_LevelStats[i].GetWriteStats( &dwNumStats );
            m_StatSpecList[j].dwNumStats = dwNumStats;
            j++;
            // Get the internal XONLINE_STAT array for the overall
            // level.  Pass this directly to XOnlineStatWrite so that
            // it can get at the values directly,
            m_StatSpecList[j].pStats = 
                m_OverallStats[i].GetWriteStats( &dwNumStats );
            m_StatSpecList[j].dwNumStats = dwNumStats;
            j++;
            
            // Stat specs for the current level leaderboard
            if( m_LevelStats[i].Missing() )
                m_LevelStats[i].Clear();

            m_LevelStats[i].SetKills( 
                m_LevelStats[i].GetKills() + m_Players[i].Stats.GetKills() );
            m_LevelStats[i].SetAssists( 
                m_LevelStats[i].GetAssists() + m_Players[i].Stats.GetAssists() );
            m_LevelStats[i].SetDeaths( 
                m_LevelStats[i].GetDeaths() + m_Players[i].Stats.GetDeaths() );
            m_LevelStats[i].SetStarted( 
                m_LevelStats[i].GetStarted() + m_Players[i].Stats.GetStarted() );
            m_LevelStats[i].SetCompleted( 
                m_LevelStats[i].GetCompleted() + m_Players[i].Stats.GetCompleted() );
            m_LevelStats[i].SetRating( 
                CalculateRating( m_LevelStats[i].GetKills(), 
                                 m_LevelStats[i].GetDeaths(),
                                 m_LevelStats[i].GetAssists() ) );

            // Update overall statistics                                            
            if( m_OverallStats[i].Missing() )
                m_OverallStats[i].Clear();

            m_OverallStats[i].SetKills( 
                m_OverallStats[i].GetKills() + m_Players[i].Stats.GetKills() );
            m_OverallStats[i].SetAssists( 
                m_OverallStats[i].GetAssists() + m_Players[i].Stats.GetAssists() );
            m_OverallStats[i].SetDeaths( 
                m_OverallStats[i].GetDeaths() + m_Players[i].Stats.GetDeaths() );
            m_OverallStats[i].SetStarted( 
                m_OverallStats[i].GetStarted() + m_Players[i].Stats.GetStarted() );
            m_OverallStats[i].SetCompleted( 
                m_OverallStats[i].GetCompleted() + m_Players[i].Stats.GetCompleted() );
            m_OverallStats[i].SetRating( 
                CalculateRating( m_OverallStats[i].GetKills(), 
                                 m_OverallStats[i].GetDeaths(),
                                 m_OverallStats[i].GetAssists() ) );
        }
    
        hr = XOnlineStatWrite( m_StatSpecList.size(), 
            &m_StatSpecList[0], NULL, &m_hStatsWriteTask );
        if( FAILED( hr ) )
        {
            m_UI.SetErrorStr( L"XOnlineStatWrite Failed with 0x%x", hr );
            Reset();
        }
    }
    
}




//-----------------------------------------------------------------------------
// Name: UpdateStateLeaderEnum()
// Desc: Begin leaderboard enumeration using the current leaderboard and pivot 
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateLeaderEnum( Event ev )
{
    HRESULT hr;

    switch( ev )
    {
    case EV_BUTTON_B:
    case EV_BUTTON_BACK:
        m_hStatsEnumTask.Close();
        m_State = STATE_MAIN_MENU;
        m_dwCurrItem = 0;
        return;
    }


    if( m_hStatsEnumTask )
    {
        // Continue existing enumeration
        hr = m_hStatsEnumTask.Continue();
        if ( hr != XONLINETASK_S_RUNNING )
        {
            if( SUCCEEDED( hr ) )
            {
                DWORD dwNumStats;
                (VOID) CPlayerStats::GetStatIDs( &dwNumStats );

                StatUserList Users(MAX_STAT_USERS); 
                StatList Stats(dwNumStats*MAX_STAT_USERS);
                DWORD dwLeaderboardSize, dwReturnedResults;

                // Obtain the results of the enumeration.  Note that the XONLINE_STAT
                // attributes for alls users are stored in the Stats array.  If there
                // are N attributes, the first N elements are the attributes for the
                // first user, the second N elements are the attributes for the
                // second user.  The attributes for user "i" is at N*i.
                hr = XOnlineStatLeaderEnumerateGetResults( m_hStatsEnumTask,
                    Users.size(), &Users[0], Stats.size(), &Stats[0], 
                    &dwLeaderboardSize, &dwReturnedResults, 0, NULL );
                if( SUCCEEDED( hr ) )
                {
                    if( dwReturnedResults )
                    {
                        // Populate m_LeaderboardUsers with each user in the
                        // leaderboard
                        for( DWORD i = 0; i < dwReturnedResults; ++i)
                        {
                            CPlayerInfo Info( Users[i], &Stats[i*dwNumStats] );
                            m_LeaderboardUsers.push_back( Info );
                        }

                        m_bShowRating = FALSE;
                        m_State = STATE_VIEW_LEADERBOARD;
                    }
                    else
                    {
                        m_State = STATE_ERROR;
                        m_NextState = STATE_MAIN_MENU;
                        m_UI.SetErrorStr( L"No Statistics" );
                    }
                    m_hStatsEnumTask.Close();
                }
                else
                {
                    if( FAILED( hr ) )
                    {
                        m_UI.SetErrorStr( L"XOnlineStatLeaderEnumerateGetResults failed. Error 0x%x",
                            hr );
                        Reset();
                    }
                }
            }
            else if( FAILED( hr ) )
            {
                if ( hr == XONLINE_E_STAT_USER_NOT_FOUND )
                {
                    // A user XUID was specified as a pivot, and the user
                    // was not on the leaderboard
                    m_State = STATE_ERROR;
                    m_NextState = STATE_MAIN_MENU;
                    m_UI.SetErrorStr( L"No Statistics for this user" );
                    m_hStatsEnumTask.Close();
                }
                else
                {
                    m_UI.SetErrorStr( L"StatLeader enumeration failed. Error 0x%x",
                        hr );
                    Reset();
                }
            }
        }
    }
    else
    {
        // Start the enumeration process...
        DWORD dwNumStats;
        // Get an array of attribute IDs
        PWORD pwStatsPerUser = CPlayerStats::GetStatIDs( &dwNumStats );
        m_LeaderboardUsers.clear();     
        hr = XOnlineStatLeaderEnumerate( m_pxuidPagePivot,
            1, MAX_STAT_USERS, m_dwCurrLeaderBoard,
            dwNumStats,  pwStatsPerUser, NULL, &m_hStatsEnumTask );
        if( FAILED( hr ) )
        {
            m_UI.SetErrorStr( L"XOnlineStatLeaderEnumerate failed. Error 0x%x",
                hr );
            Reset();
        }       
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateStateViewLeaderboard()
// Desc: Current leaderboard display 
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateViewLeaderboard( Event ev )
{
    switch( ev )
    {
    case EV_BUTTON_B:
        m_State = STATE_MAIN_MENU;
        return;

    case EV_BUTTON_BLACK:
        m_bShowRating = !m_bShowRating;
        break;

    case EV_BUTTON_WHITE:
        m_HelpResumeState = m_State;
        m_State = STATE_HELP;
        break;
    }
}




//-----------------------------------------------------------------------------
// Name: CalculateRating()
// Desc: Calculate player rating 
//-----------------------------------------------------------------------------
LONGLONG CXBoxSample::CalculateRating( LONG lKills, LONG lDeaths, 
                                       LONG lAssists )
{
     return 100*lKills + 10*lAssists - 5*lDeaths;
}


    
    
//-----------------------------------------------------------------------------
// Name: CreateEndGame()
// Desc: Create a player list and end game statistics 
//-----------------------------------------------------------------------------
VOID CXBoxSample::CreateEndGame()
{

    // Generate random statistics
    for( DWORD i = 0; i < m_Players.size(); ++i )
    {
        LONG lKills = rand() % 20;
        LONG lDeaths = rand() % 20;
        LONG lAssists = rand() % 20;

        m_Players[i].Stats.SetKills( lKills );
        m_Players[i].Stats.SetAssists( lAssists );
        m_Players[i].Stats.SetDeaths( lDeaths);
        m_Players[i].Stats.SetStarted( 1 );
        m_Players[i].Stats.SetCompleted( rand() % 10 ? 1 : 0 );
        m_Players[i].Stats.SetRating( CalculateRating( lKills, lDeaths, lAssists ) );
    }

    // Resort by rating/name
    std::sort( m_Players.begin(), m_Players.end(), PlayerListCompare() );

}


//-----------------------------------------------------------------------------
// Name: UpdateStateResetStats()
// Desc: Reset user statistics for all leaderboards
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateResetStats( Event ev )
{
    HRESULT hr;

    switch( ev )
    {
    case EV_BUTTON_B:
    case EV_BUTTON_BACK:
        m_hStatsResetTask.Close();
        m_State = STATE_MAIN_MENU;
        m_dwCurrItem = 0;
        return;
    }

    if( m_hStatsResetTask )
    {
        hr = m_hStatsResetTask.Continue();
        if ( hr != XONLINETASK_S_RUNNING )
        {

            m_State = STATE_ERROR;
            if( SUCCEEDED( hr ) )
            {   
                m_hStatsResetTask.Close();
                m_UI.SetErrorStr( L"Stats Reset for %s", m_strUser );
                m_NextState = STATE_MAIN_MENU;
            }
            else
            {
                m_UI.SetErrorStr( L"Unable to Reset Stats for %s.\nError 0x%x", 
                    m_strUser, hr );
                Reset();
            }
        }
    }
    else
    {
        // Kick off the reset action
        // NOTE: Below a leaderboard id of zero is specified.  This
        // will remove stats for the user on EVERY leaderboard owned by
        // the title. 
        hr = XOnlineStatReset( m_UserList[ m_dwCurrUser ].xuid,
                0,
                NULL, &m_hStatsResetTask );
        if( FAILED( hr ) )
        {
            m_UI.SetErrorStr( L"XOnlineStatReset failed. Error 0x%x", hr );
            Reset();
        }
    }


}




//-----------------------------------------------------------------------------
// Name: Reset()
// Desc: Prepare to restart the application at the front menu
//-----------------------------------------------------------------------------
VOID CXBoxSample::Reset()
{
    m_hFriendsTask.Close();
    m_hFriendEnumTask.Close();
    m_hStatsWriteTask.Close();
    m_hStatsReadTask.Close();
    m_hStatsResetTask.Close();
    m_State = STATE_ERROR;
    if( m_UserList.size() == 0 )
        m_NextState = STATE_CREATE_ACCOUNT;
    else
        m_NextState = STATE_SELECT_ACCOUNT;
    m_HelpResumeState = STATE_SELECT_ACCOUNT;
    m_dwCurrItem = 0;
    m_dwCurrLevel = 0;
    m_dwCurrLeaderBoard = 0;
    m_bIsLoggedOn = FALSE;
    m_pxuidPagePivot = NULL;
    m_Players.clear();
    m_LeaderboardUsers.clear();
    m_FriendList.clear();
    m_bShowRating = FALSE;
    m_dwLaunchReason   = XLD_LAUNCH_DASHBOARD_MAIN_MENU;
    m_bAllowBootToDash = FALSE;
}




//-----------------------------------------------------------------------------
// Name: CPlayerStats()
// Desc: Constructor
//-----------------------------------------------------------------------------
CPlayerStats::CPlayerStats( XONLINE_STAT *pStats )
{
    // The Rank should be the last attribute in the enumeration
    assert( STAT_RANK == STAT_MAX - 1 );

    if( pStats )
    {
        // Make sure the IDs are in proper order
        assert( pStats[0].wID == STAT_KILLS );
        assert( pStats[1].wID == STAT_DEATHS );
        assert( pStats[2].wID == STAT_ASSISTS );
        assert( pStats[3].wID == STAT_STARTED );
        assert( pStats[4].wID == STAT_COMPLETED );
        assert( pStats[5].wID == XONLINE_STAT_RATING );
        assert( pStats[6].wID == XONLINE_STAT_RANK );

        for( DWORD i = 0; i < STAT_MAX; ++i )
            m_Stats[i] = pStats[i];
    }
    else
        Clear();
}



//-----------------------------------------------------------------------------
// Name: Clear()
// Desc: Clear stat data
//-----------------------------------------------------------------------------
VOID CPlayerStats::Clear()
{    

    // Fill with default data
    SetKills( 0 );
    SetDeaths( 0 );
    SetAssists( 0 );
    SetStarted( 0 );
    SetCompleted( 0 );
    SetRating( 0 );

    m_Stats[STAT_RANK].wID = XONLINE_STAT_RANK;
    m_Stats[STAT_RANK].type = XONLINE_STAT_LONG;
    m_Stats[STAT_RANK].lValue = -1;


}




//-----------------------------------------------------------------------------
// Name: Missing()
// Desc: Check player stats for missing values
//-----------------------------------------------------------------------------
BOOL CPlayerStats::Missing() const
{
    for( DWORD i = 0; i < STAT_MAX; ++i )
        if( m_Stats[i].type == XONLINE_STAT_NONE )
            return TRUE;

    // Check to see if a rank has been assigned
    if( m_Stats[STAT_RANK].lValue < 0 )
        return TRUE;

    return FALSE;
}



//-----------------------------------------------------------------------------
// Name: SetKills()
//-----------------------------------------------------------------------------
VOID CPlayerStats::SetKills( LONG lValue )
{
    assert( lValue >= 0 );
    m_Stats[STAT_KILLS].wID = STAT_KILLS;
    m_Stats[STAT_KILLS].type = XONLINE_STAT_LONG;
    m_Stats[STAT_KILLS].lValue = lValue;
}




//-----------------------------------------------------------------------------
// Name: GetKills()
//-----------------------------------------------------------------------------
LONG CPlayerStats::GetKills() const
{
    assert( m_Stats[STAT_KILLS].type == XONLINE_STAT_LONG );
    return m_Stats[STAT_KILLS].lValue;
}




//-----------------------------------------------------------------------------
// Name: SetDeaths()
//-----------------------------------------------------------------------------
VOID CPlayerStats::SetDeaths( LONG lValue )
{
    assert( lValue >= 0 );
    m_Stats[STAT_DEATHS].wID = STAT_DEATHS;
    m_Stats[STAT_DEATHS].type = XONLINE_STAT_LONG;
    m_Stats[STAT_DEATHS].lValue = lValue;
}




//-----------------------------------------------------------------------------
// Name: GetDeaths()
//-----------------------------------------------------------------------------
LONG CPlayerStats::GetDeaths() const
{
    assert( m_Stats[STAT_DEATHS].type == XONLINE_STAT_LONG );
    return m_Stats[STAT_DEATHS].lValue;
}




//-----------------------------------------------------------------------------
// Name: SetAssists()
//-----------------------------------------------------------------------------
VOID CPlayerStats::SetAssists( LONG lValue )
{
    assert( lValue >= 0 );
    m_Stats[STAT_ASSISTS].wID = STAT_ASSISTS;
    m_Stats[STAT_ASSISTS].type = XONLINE_STAT_LONG;
    m_Stats[STAT_ASSISTS].lValue = lValue;
}




//-----------------------------------------------------------------------------
// Name: GetAssists()
//-----------------------------------------------------------------------------
LONG CPlayerStats::GetAssists() const
{
    assert( m_Stats[STAT_ASSISTS].type == XONLINE_STAT_LONG );
    return m_Stats[STAT_ASSISTS].lValue;
}




//-----------------------------------------------------------------------------
// Name: SetStarted()
//-----------------------------------------------------------------------------
VOID CPlayerStats::SetStarted( LONG lValue )
{
    assert( lValue >= 0 );
    m_Stats[STAT_STARTED].wID = STAT_STARTED;
    m_Stats[STAT_STARTED].type = XONLINE_STAT_LONG;
    m_Stats[STAT_STARTED].lValue = lValue;
}




//-----------------------------------------------------------------------------
// Name: GetStarted()
//-----------------------------------------------------------------------------
LONG CPlayerStats::GetStarted() const
{
    assert( m_Stats[STAT_STARTED].type == XONLINE_STAT_LONG );
    return m_Stats[STAT_STARTED].lValue;
}




//-----------------------------------------------------------------------------
// Name: SetCompleted()
//-----------------------------------------------------------------------------
VOID CPlayerStats::SetCompleted( LONG lValue )
{
    assert( lValue >= 0 );
    m_Stats[STAT_COMPLETED].wID = STAT_COMPLETED;
    m_Stats[STAT_COMPLETED].type = XONLINE_STAT_LONG;
    m_Stats[STAT_COMPLETED].lValue = lValue;
}




//-----------------------------------------------------------------------------
// Name: GetCompleted()
//-----------------------------------------------------------------------------
LONG CPlayerStats::GetCompleted() const
{
    assert( m_Stats[STAT_COMPLETED].type == XONLINE_STAT_LONG );
    return m_Stats[STAT_COMPLETED].lValue;
}




//-----------------------------------------------------------------------------
// Name: SetRating()
//-----------------------------------------------------------------------------
VOID CPlayerStats::SetRating( LONGLONG llValue )
{
    m_Stats[STAT_RATING].wID = XONLINE_STAT_RATING;
    m_Stats[STAT_RATING].type = XONLINE_STAT_LONGLONG;
    m_Stats[STAT_RATING].llValue = llValue;
}




//-----------------------------------------------------------------------------
// Name: GetRating()
//-----------------------------------------------------------------------------
LONGLONG CPlayerStats::GetRating() const
{
    assert( m_Stats[STAT_RATING].type == XONLINE_STAT_LONGLONG );
    return m_Stats[STAT_RATING].llValue;
}




//-----------------------------------------------------------------------------
// Name: GetRank()
//-----------------------------------------------------------------------------
LONG CPlayerStats::GetRank() const
{
    assert( m_Stats[STAT_RANK].type == XONLINE_STAT_LONG );
    return m_Stats[STAT_RANK].lValue;
}




//-----------------------------------------------------------------------------
// Name: GetWriteStats()
// Desc: Return an XONLINE_STAT array suitable for  use with XOnlineStatWrite
//-----------------------------------------------------------------------------
PXONLINE_STAT CPlayerStats::GetWriteStats( DWORD *pdwNumStats )
{
    // Return the internal stat list, but since writing the rank is not
    // allowed, return a count one less than the max ( the rank attribute
    // is suppose to be the last in the list )
    assert( STAT_RANK == STAT_MAX - 1 );

    *pdwNumStats = STAT_MAX - 1;
    return m_Stats;
}




//-----------------------------------------------------------------------------
// Name: GetReadStats()
// Desc: Return an XONLINE_STAT array suitable for use with XOnlineStatRead
//-----------------------------------------------------------------------------
PXONLINE_STAT CPlayerStats::GetReadStats( DWORD *pdwNumStats )
{
    *pdwNumStats = STAT_MAX;
    return m_Stats;
}




//-----------------------------------------------------------------------------
// Name: GetStatIDs()
// Desc: Return an array of ids for the stats maintained internally
//-----------------------------------------------------------------------------
PWORD CPlayerStats::GetStatIDs( DWORD *pdwNumStats )
{

    // Stat attribute IDs.  These must match the number and order of
    // ids in the m_Stats array.
    static WORD StatIDs[STAT_MAX] = 
    {
        STAT_KILLS,
        STAT_DEATHS,
        STAT_ASSISTS,
        STAT_STARTED,
        STAT_COMPLETED,
        XONLINE_STAT_RATING,
        XONLINE_STAT_RANK
    };

    *pdwNumStats = STAT_MAX;

    return StatIDs;
}




//-----------------------------------------------------------------------------
// Name: CPlayerInfo()
// Desc: Constructor
//-----------------------------------------------------------------------------
CPlayerInfo::CPlayerInfo()
{
    ZeroMemory( &xuid, sizeof( xuid ) );
    strUserName[0] = L'\0';
}




//-----------------------------------------------------------------------------
// Name: CPlayerInfo()
// Desc: Constructor
//-----------------------------------------------------------------------------
CPlayerInfo::CPlayerInfo( XONLINE_STAT_USER & User, 
                          XONLINE_STAT *pStats ) : Stats( pStats )
{
    xuid = User.xuidUser;
    XBUtil_GetWide( User.szGamertag, strUserName, XONLINE_GAMERTAG_SIZE );
}



