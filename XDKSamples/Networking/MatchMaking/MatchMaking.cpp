//-----------------------------------------------------------------------------
// File: MatchMaking.cpp
//
// Desc: Illustrates online matchmaking on Xbox.
//       Allows player to find/create a new multiplayer game, using
//       either the QuickMatch or OptiMatch metaphor.
//       Once game has been chosen, allows players to connect and send
//       messages to other players.
//
// Hist: 08.08.01 - New for Aug M1 release 
//       01.16.02 - Updated for Feb release 
//       04.22.02 - New for May release
//       06.27.02 - Updated for Aug release
//       03.13.03 - Update for April release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "MatchMaking.h"
#include "XBMemUnit.h"
#include "XBVoice.h"
#include <cassert>
#pragma warning( disable: 4355 )




//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------
const DWORD MAX_SESSION_NAMES = 6;      // Max names to choose from
const FLOAT PLAYER_HEARTBEAT  = 3.0f;   // every 3 seconds
const DWORD PLAYER_TIMEOUT    = 12000;  // 12 seconds
const FLOAT GAME_JOIN_TIME    = 5.0f;   // 5 seconds



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
// Name: class MatchInAddr
// Desc: Predicate functor used to match on IN_ADDRs in player lists
//-----------------------------------------------------------------------------
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




//-----------------------------------------------------------------------------
// Name: SessionInfo()
// Desc: Default Constructor
//-----------------------------------------------------------------------------
SessionInfo::SessionInfo()
{
    ZeroMemory( &m_SessionID, sizeof( m_SessionID ) );
    ZeroMemory( &m_KeyExchangeKey, sizeof( m_KeyExchangeKey ) );
    ZeroMemory( &m_HostAddress, sizeof( m_HostAddress ) );
    m_dwPublicOpen = 0;
    m_qwGameType = TYPE_ANY;
    m_qwGameStyle = STYLE_ANY;
    m_qwPlayerLevel = LEVEL_ANY;
    *m_strOwnerName   = 0;
    *m_strSessionName = 0;
    m_ConfigInfo = NULL_BLOB;
}




//-----------------------------------------------------------------------------
// Name: SessionInfo()
// Desc: Constructor
//-----------------------------------------------------------------------------
SessionInfo::SessionInfo( COptiMatchResult& Result )
{
    m_SessionID      = Result.SessionID;
    m_KeyExchangeKey = Result.KeyExchangeKey;
    m_HostAddress    = Result.HostAddress;
    m_dwPublicOpen   = Result.PublicOpen;
    m_qwGameType     = Result.GameType;
    m_qwGameStyle    = Result.GameStyle;
    m_qwPlayerLevel  = Result.PlayerLevel;
    SetOwnerName( Result.OwnerName );
    SetSessionName( Result.SessionName );
    m_ConfigInfo = Result.ConfigInfo;
}




//-----------------------------------------------------------------------------
// Name: SessionInfo()
// Desc: Constructor
//-----------------------------------------------------------------------------
SessionInfo::SessionInfo( CFindSessionByIDResult& Result )
{
    m_SessionID      = Result.SessionID;
    m_KeyExchangeKey = Result.KeyExchangeKey;
    m_HostAddress    = Result.HostAddress;
    m_dwPublicOpen   = Result.PublicOpen;
    m_qwGameType = TYPE_ANY;
    m_qwGameStyle = STYLE_ANY;
    m_qwPlayerLevel = LEVEL_ANY;
    *m_strOwnerName   = 0;
    *m_strSessionName = 0;
    m_ConfigInfo = NULL_BLOB;

}




//-----------------------------------------------------------------------------
// Name: SetGameType()
// Desc: Set session game type
//-----------------------------------------------------------------------------
VOID SessionInfo::SetGameType( ULONGLONG qwGameType )
{
    m_qwGameType = qwGameType;
}




//-----------------------------------------------------------------------------
// Name: SetPlayerLevel()
// Desc: Set session player level
//-----------------------------------------------------------------------------
VOID SessionInfo::SetPlayerLevel( ULONGLONG qwPlayerLevel )
{
    m_qwPlayerLevel = qwPlayerLevel;
}




//-----------------------------------------------------------------------------
// Name: SetSessionName()
// Desc: Set session name
//-----------------------------------------------------------------------------
VOID SessionInfo::SetSessionName( const WCHAR* strSessionName )
{
    assert( strSessionName != NULL );
    lstrcpynW( m_strSessionName, strSessionName, XATTRIB_SESSION_NAME_MAX_LEN );
}




//-----------------------------------------------------------------------------
// Name: SetOwnerName()
// Desc: Set owner name
//-----------------------------------------------------------------------------
VOID SessionInfo::SetOwnerName( const WCHAR* strOwnerName )
{
    assert( strOwnerName != NULL );
    lstrcpynW( m_strOwnerName, strOwnerName, XONLINE_GAMERTAG_SIZE );
}




//-----------------------------------------------------------------------------
// Name: SetGameStyle()
// Desc: Set game style
//-----------------------------------------------------------------------------
VOID SessionInfo::SetGameStyle( ULONGLONG qwGameStyle )
{
    m_qwGameStyle = qwGameStyle;
}




//-----------------------------------------------------------------------------
// Name: GenRandSessionName()
// Desc: Set name of session to randomly generated value
//-----------------------------------------------------------------------------
VOID SessionInfo::GenRandSessionName()
{
    XBRandName_GetRandomName( m_strSessionName, XATTRIB_SESSION_NAME_MAX_LEN );
}




//-----------------------------------------------------------------------------
// Name: SetConfigInfo()
// Desc: Set "configuration" blob
//-----------------------------------------------------------------------------
VOID SessionInfo::SetConfigInfo( const CBlob & Value )
{
    m_ConfigInfo = Value;
}




//-----------------------------------------------------------------------------
// Name: CXBoxSample()
// Desc: Constructor
//-----------------------------------------------------------------------------
CXBoxSample::CXBoxSample()
{
    m_GameMsg.SetAppPtr( this );

    m_State     = STATE_SELECT_ACCOUNT;
    m_NextState = STATE_SELECT_ACCOUNT;
    
    // Login to matchmaking service
    m_pServices[0] = XONLINE_MATCHMAKING_SERVICE;
    
    m_dwCurrItem     = 0;
    m_dwCurrUser     = 0;
    m_dwUserIndex    = 0;
    m_qwUserID       = 0;
    m_dwSessionIndex = 0;
    
    m_bIsLoggedOn          = FALSE;
    m_bJoinedGame          = FALSE;
    m_bIsQuickMatch        = FALSE;
    m_bIsHost              = FALSE;
    m_bAcceptedInvite      = FALSE;
    ZeroMemory( &m_AcceptedInvite, sizeof( m_AcceptedInvite ) );
    
    m_inHostAddr.s_addr = 0;
    m_dwSlotsInUse      = 0;
    
    *m_strUser   = 0;
    *m_strStatus = 0;
    
    ZeroMemory( &m_xnJoinedSessionID, sizeof( XNKID ) );
    ZeroMemory( &m_xnJoinedKeyExchangeKey, sizeof( XNKEY ) );
    
    // Set default session info
    m_SessionInfo.SetGameType( TYPE_ANY );
    m_SessionInfo.SetPlayerLevel( LEVEL_ANY );
    m_SessionInfo.SetGameStyle( STYLE_ANY );
    m_SessionInfo.SetOwnerName( L"" );
    m_SessionInfo.GenRandSessionName();
    m_SessionInfo.SetConfigInfo( NULL_BLOB );

    m_GameJoinTimer.Start();
    m_HeartbeatTimer.Start();
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
    
    if( FAILED( m_GameMsg.Initialize() ) )
       return E_FAIL;

    // Wait for any inserted MUs to mount
    while ( XGetDeviceEnumerationStatus() == XDEVICE_ENUMERATION_BUSY ) {}

   
    // Get information on all accounts for this Xbox
    if( FAILED( XBOnline_GetUserList( m_UserList ) ) )
        return E_FAIL;

    CXBMemUnit::GetMemUnitSnapshot();     
    
    // If no accounts, then player needs to create an account.
    // For development purposes, accounts are created using the
    // Online Dashboard or the XDK Launcher
    if( m_UserList.empty() )
    {
        m_State = STATE_CREATE_ACCOUNT;
        return S_OK;
    }
    else
    {
        // Check for an accepted game invitation
        HRESULT hr = XOnlineFriendsGetAcceptedGameInvite( &m_AcceptedInvite );
        // hr will be set S_OK if there an invite, S_FALSE if there
        // isn't one, and an error result on failure
        m_bAcceptedInvite = ( hr == S_OK );
        
        return hr;
    }
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    // Check the physical connection
    if( !m_NetLink.IsActive() )
    {
        m_UI.SetErrorStr( L"This Xbox has lost its online connection" );
        m_hOnlineTask.Close();
        m_bIsLoggedOn = FALSE;
        Reset( TRUE );
    }
    
    // Maintain our connection once we've logged on
    if( m_bIsLoggedOn )
    {
        HRESULT hr = m_hOnlineTask.Continue();    
        if( SUCCEEDED( hr ) && m_hFriendsTask )
        {
            hr = m_hFriendsTask.Continue();
        }
        
        // XOnlineFriendsGameInvite requires a friend enumeration task
        if( SUCCEEDED( hr ) && m_hFriendEnumTask )
        {
            hr = m_hFriendEnumTask.Continue();
            if( hr == XONLINETASK_S_RESULTS_AVAIL )
            {
                // An updated friends list is now available for us to retrieve
                // Reserve space for the friend list
                m_FriendList.resize( MAX_FRIENDS );
                
                XONLINE_FRIEND *pFriendList = &m_FriendList[0];
                DWORD dwNumFriends = XOnlineFriendsGetLatest( m_dwUserIndex,
                    MAX_FRIENDS,
                    pFriendList );
                
                // Resize to the actual number retrieved
                m_FriendList.resize( dwNumFriends );
            }
        }
        
        if( FAILED( hr ) )
        {
            m_hOnlineTask.Close();
            if( hr == XONLINE_E_LOGON_KICKED_BY_DUPLICATE_LOGON )
                m_UI.SetErrorStr( L"You have been signed out because your\n"
                L"account signed in on another Xbox" );
            else
                m_UI.SetErrorStr( L"Connection was lost. Must relogin" );
            m_bIsLoggedOn = FALSE;
            Reset( TRUE );
            m_State = STATE_ERROR;
            m_NextState = STATE_SELECT_ACCOUNT;
            
        }
    }
    
    Event ev = GetEvent();
    
    switch( m_State )
    {
    case STATE_CREATE_ACCOUNT: UpdateStateCreateAccount( ev );   break;
    case STATE_SELECT_ACCOUNT: UpdateStateSelectAccount( ev );   break;
    case STATE_LOGGING_ON:     UpdateStateLoggingOn( ev );       break;
    case STATE_SELECT_MATCH:   UpdateStateSelectMatch( ev );     break;
    case STATE_OPTIMATCH:      UpdateStateOptiMatch( ev );       break;
    case STATE_SELECT_TYPE:    UpdateStateSelectType( ev );      break;
    case STATE_SELECT_STYLE:   UpdateStateSelectStyle( ev );     break;
    case STATE_SELECT_LEVEL:   UpdateStateSelectLevel( ev );     break;
    case STATE_SELECT_NAME:    UpdateStateSelectName( ev );      break;
    case STATE_SELECT_SESSION: UpdateStateSelectSession( ev );   break;
    case STATE_FINISH_ENUM:    UpdateStateFinishEnum( ev );      break;
    case STATE_GAME_SEARCH:    UpdateStateGameSearch( ev );      break;
    case STATE_ID_SEARCH:      UpdateStateGameSearchByID( ev );  break;
    case STATE_REQUEST_JOIN:   UpdateStateRequestJoin( ev );     break;
    case STATE_CREATE_SESSION: UpdateStateCreateSession( ev );   break;
    case STATE_PLAY_GAME:      UpdateStatePlayGame( ev );        break;
    case STATE_DELETE_SESSION: UpdateStateDeleteSession( ev );   break;
    case STATE_ERROR:          UpdateStateError( ev );           break;
    case STATE_HELP:           UpdateStateHelp( ev );            break;
    default:                   assert( FALSE );                  break;
    }
    
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, the call is the entry point for 3D
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
    case STATE_LOGGING_ON:     m_UI.RenderLoggingOn();     break;
    case STATE_GAME_SEARCH:    m_UI.RenderGameSearch( FALSE ); break;
    case STATE_ID_SEARCH:      m_UI.RenderGameSearch( TRUE ); break;
    case STATE_REQUEST_JOIN:   m_UI.RenderRequestJoin();   break;
    case STATE_CREATE_SESSION: m_UI.RenderCreateSession(); break;
    case STATE_DELETE_SESSION: m_UI.RenderDeleteSession(); break;
    case STATE_ERROR:          m_UI.RenderError();         break;
    case STATE_HELP:           m_UI.RenderHelp();          break;
    case STATE_SELECT_MATCH:   m_UI.RenderSelectMatch( m_dwCurrItem ); break;
    case STATE_SELECT_TYPE:    m_UI.RenderSelectType( m_dwCurrItem );  break;
    case STATE_SELECT_STYLE:   m_UI.RenderSelectStyle( m_dwCurrItem ); break;
    case STATE_SELECT_LEVEL:   m_UI.RenderSelectLevel( m_dwCurrItem ); break;
    case STATE_FINISH_ENUM:    m_UI.RenderFinishFriendEnum();          break;
    case STATE_CREATE_ACCOUNT:
        m_UI.RenderCreateAccount( TRUE );
        break;
    case STATE_SELECT_ACCOUNT:
        m_UI.RenderSelectAccount( m_dwCurrItem, m_UserList, 
            m_AcceptedInvite.xuidAcceptedFriend );
        break;
    case STATE_OPTIMATCH:
        m_UI.RenderOptiMatch( m_SessionInfo, m_dwCurrItem );
        break;
    case STATE_SELECT_NAME:
        m_UI.RenderSelectName( m_dwCurrItem, m_SessionNames );
        break;
    case STATE_SELECT_SESSION:
        m_UI.RenderSelectSession( m_dwCurrItem, m_SessionList );
        break;
    case STATE_PLAY_GAME:
        {
            // m_Players list doesn't include ourself, so we add one
            // to get the number of total players
            DWORD dwTotalPlayers = m_Players.size() + 1;
            m_UI.RenderPlayGame( m_SessionInfo, m_strUser, m_strStatus, 
                dwTotalPlayers, m_dwCurrItem, m_bAcceptedInvite );
            break;
        }
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
    
    // "Back"
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_BACK )
        return EV_BUTTON_BACK;
    
    // "White"
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_WHITE ] )
        return EV_BUTTON_WHITE;
    
    // Movement
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
        return EV_UP;
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
        return EV_DOWN;
    
    return EV_NULL;
}




//-----------------------------------------------------------------------------
// Name: UpdateStateCreateAccount()
// Desc: Inform player that account must be generated using external tool
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateCreateAccount( Event ev )
{
    switch( ev )
    {
    case EV_BUTTON_A:
        
        // Boot into the account creation section of the
        // online dash
        LD_LAUNCH_DASHBOARD ld;
        ZeroMemory( &ld, sizeof(ld) );
        ld.dwReason = XLD_LAUNCH_DASHBOARD_NEW_ACCOUNT_SIGNUP;
        XLaunchNewImage( NULL, PLAUNCH_DATA( &ld ) );
        break;
    case EV_BUTTON_WHITE:
        m_NextState = m_State;
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
            {
                m_dwCurrItem = 0;
                m_State = STATE_SELECT_ACCOUNT;
            }
        }
        break;
        
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateStateSelectAccount()
// Desc: Allow player to select user account
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateSelectAccount( Event ev )
{
    switch( ev )
    {
    case EV_BUTTON_WHITE:
        m_NextState = m_State;
        m_State = STATE_HELP;
        break;
    case EV_BUTTON_A:
        {
            // Save current account information
            m_dwCurrUser = m_dwCurrItem;
            m_qwUserID = m_UserList[ m_dwCurrUser ].xuid.qwUserID;
            assert( XONLINE_GAMERTAG_SIZE == MAX_PLAYER_STR );
            
            // Make WCHAR copy of user name
            XBUtil_GetWide( m_UserList[ m_dwCurrUser ].szGamertag, m_strUser, 
                XONLINE_GAMERTAG_SIZE );            
            m_State = STATE_LOGGING_ON;
            BeginLogin();
            break;
        }
        
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
// Desc: Spin during authentication
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateLoggingOn( Event ev )
{
    switch( ev )
    {
    case EV_BUTTON_B:
    case EV_BUTTON_BACK:
        
        // Cancel the task
        m_hOnlineTask.Close();
        m_State = STATE_SELECT_ACCOUNT;
        return;
    }
    
    HRESULT hr = m_hOnlineTask.Continue();
    
    if( FAILED( hr ) )
    {
           m_UI.SetErrorStr( L"Login failed.\n\n"
                L"Error %x returned by "
                L"XOnlineTaskContinue", hr );
        m_hOnlineTask.Close();
        Reset( TRUE );
        return;
    }
    
    // Check login status to see if it has completed
    else if( hr == XONLINE_S_LOGON_CONNECTION_ESTABLISHED )
    {
        BOOL bSuccess = TRUE;
        HRESULT hrService = S_OK;
        
         // Next, check if the user was actually logged on
        PXONLINE_USER pLoggedOnUsers = XOnlineGetLogonUsers();
        
        assert( pLoggedOnUsers );
        
        hr = pLoggedOnUsers[ m_dwUserIndex ].hr;
        
        if( FAILED( hr ) )
        {
            m_UI.SetErrorStr( L"User Login failed (Error 0x%x)",
                hr );
            bSuccess = FALSE;
        }
        else
        {
            // Check for service errors
            for( DWORD i = 0; i < NUM_SERVICES; ++i )
            {
                if( FAILED( hrService = XOnlineGetServiceInfo( 
                    m_pServices[i],NULL ) ) )
                {
                    m_UI.SetErrorStr( L"Login failed.\n\n"
                        L"Error 0x%x logging into service %d",
                        hrService, m_pServices[i] );
                    bSuccess = FALSE;
                    break;
                }
            }
        }
        
        if( bSuccess )
        {
            // We're now on the system
            m_bIsLoggedOn = TRUE;
            
            // Notify the world
            DWORD dwState = XONLINE_FRIENDSTATE_FLAG_ONLINE;
            if( XBVoice_HasDevice() )
                dwState |= XONLINE_FRIENDSTATE_FLAG_VOICE;
            SetPlayerState( dwState );
            
            // Allow player to select match type
            m_dwCurrItem = 0;
            hr = InitFriends();
            if( SUCCEEDED( hr ) )
            {
                // Transition to match selection menu.
                m_State = STATE_SELECT_MATCH;
                
                if( m_bAcceptedInvite && 
                    XOnlineAreUsersIdentical( &m_AcceptedInvite.xuidAcceptedFriend, 
                    &m_UserList[ m_dwCurrUser ].xuid ) )
                    
                {
                    // There is a accepted invitation for this
                    // user, so just join the corresponding 
                    // session
                    
                    hr = BeginFindAcceptedSession(); 
                    if( FAILED( hr ) )
                        bSuccess = FALSE;
                }
            }
            else
                bSuccess = FALSE;
        }
        
        if( !bSuccess )
        {
            Reset( TRUE );
            m_State = STATE_ERROR;
            m_NextState = STATE_SELECT_ACCOUNT;
            m_hOnlineTask.Close();
            m_bIsLoggedOn = FALSE;

        }
    }
}




//-----------------------------------------------------------------------------
// Name: InitFriends()
// Desc: Start friend enumeration
//----------------------------------------------------------------------------
HRESULT CXBoxSample::InitFriends()
{
    HRESULT hr;
    
    // Kick off friend enumeration
    // Standard init
    hr = XOnlineFriendsStartup( NULL, &m_hFriendsTask );
    if( FAILED(hr) )
    {
        m_UI.SetErrorStr( L"Friends failed to initialize. Error 0x%x", hr );
    }
    else
    {
        // Query server for latest list of friends
        hr = XOnlineFriendsEnumerate( m_dwUserIndex, NULL, &m_hFriendEnumTask );
        if( FAILED(hr) )
        {
            m_UI.SetErrorStr( L"Friend enum failed to initialize. Error 0x%x", hr );
        }
    }
    
    return hr;
}




//-----------------------------------------------------------------------------
// Name: FinishFriendEnum()
// Desc: Start/Continue friend enumeration
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
        Reset( TRUE );
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
            Reset( TRUE );
        }
        else
        {
            if( m_NextState == STATE_SELECT_ACCOUNT )
            {
                m_bIsLoggedOn = FALSE;
                m_hOnlineTask.Close();        
            }
            m_hFriendEnumTask.Close();          
            m_dwCurrItem  = 0;
            m_State = m_NextState;  // Transition to desired next state
        }
    }
}




//-----------------------------------------------------------------------------
// Name: BeginFindAcceptedSession()
// Desc: Located session for an accepted game invite
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::BeginFindAcceptedSession()
{
    HRESULT hr;
    
    assert( m_bAcceptedInvite );

    hr = m_FindByIDQuery.Query( * (ULONGLONG *) &m_AcceptedInvite.InvitingFriend.sessionID );
    if( SUCCEEDED( hr ) )
    {
        m_State = STATE_ID_SEARCH;
    }
    else
    {
        m_UI.SetErrorStr( L"Find Session ID query failed. Error 0x%x",
            hr );
    }
    
    return hr;
}




//-----------------------------------------------------------------------------
// Name: UpdateStateSelectMatch()
// Desc: Player chooses matchmaking type
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateSelectMatch( Event ev )
{
    // If we're not logged in, need to do that before we can start matchmaking
    if( !m_bIsLoggedOn )
    {
        m_State = STATE_SELECT_ACCOUNT;
        return;
    }
    
    switch( ev )
    {
    case EV_BUTTON_B:
    case EV_BUTTON_BACK:
        Reset( TRUE );
        m_hOnlineTask.Close();
        m_bIsLoggedOn = FALSE;
        m_State = STATE_SELECT_MATCH;
        break;

    case EV_BUTTON_WHITE:
        m_NextState = m_State;
        m_State = STATE_HELP;
        break;
    case EV_BUTTON_A:
        
        // Match type chosen
        if( m_dwCurrItem == MATCH_QUICK )
        {
            // Start lookin' for games
            m_State = STATE_GAME_SEARCH;
            m_bIsQuickMatch = TRUE;
            BeginSessionSearch();
        }
        else
        {
            // Start building search criteria
            m_bIsQuickMatch = FALSE;
            m_dwCurrItem = 0;
            m_State = STATE_OPTIMATCH;
        }
        break;
        
    case EV_UP:
        // Move to previous item; allow wrap to bottom
        if( m_dwCurrItem == 0 )
            m_dwCurrItem = MATCH_MAX - 1;
        else
            --m_dwCurrItem;
        break;
        
    case EV_DOWN:
        // Move to next item; allow wrap to top
        if( m_dwCurrItem == MATCH_MAX - 1 )
            m_dwCurrItem = 0;
        else
            ++m_dwCurrItem;
        break;
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateStateOptiMatch()
// Desc: Player customizes game settings
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateOptiMatch( Event ev )
{
    switch( ev )
    {
    case EV_BUTTON_WHITE:
        m_NextState = m_State;
        m_State = STATE_HELP;
        break;
    case EV_BUTTON_A:
        switch( m_dwCurrItem )
        {
        case CUSTOM_TYPE:
            m_State = STATE_SELECT_TYPE;
            m_dwCurrItem = DWORD( m_SessionInfo.GetGameType() );
            break;
        case CUSTOM_LEVEL:
            m_State = STATE_SELECT_LEVEL;
            m_dwCurrItem = (DWORD) m_SessionInfo.GetPlayerLevel();
            break;
        case CUSTOM_STYLE:
            m_State = STATE_SELECT_STYLE;
            m_dwCurrItem = (DWORD) m_SessionInfo.GetGameStyle();
            break;
        case CUSTOM_NAME:
            m_State = STATE_SELECT_NAME;
            m_dwCurrItem = 0;

            // Build a list of potential session names
            m_SessionNames.clear();
            for( DWORD i = 0; i < MAX_SESSION_NAMES; ++i )
            {
                WCHAR strSessionName[ XATTRIB_SESSION_NAME_MAX_LEN ];
                XBRandName_GetRandomName( strSessionName, XATTRIB_SESSION_NAME_MAX_LEN );
                m_SessionNames.push_back( strSessionName );
            }
            break;
        case CUSTOM_FIND:
            // Time to initiate search
            m_State = STATE_GAME_SEARCH;
            BeginSessionSearch();
            break;
        default:
            assert( FALSE );
            break;
        }
        break;
        
        case EV_BUTTON_B:
        case EV_BUTTON_BACK:
            // Return to match menu
            Reset( FALSE );
            break;
            
        case EV_UP:
            // Move to previous item; allow wrap to bottom
            if( m_dwCurrItem == 0 )
                m_dwCurrItem = CUSTOM_MAX - 1;
            else
                --m_dwCurrItem;
            break;
            
        case EV_DOWN:
            // Move to next item; allow wrap to top
            if( m_dwCurrItem == CUSTOM_MAX - 1 )
                m_dwCurrItem = 0;
            else
                ++m_dwCurrItem;
            break;
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateStateSelectType()
// Desc: Select game type
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateSelectType( Event ev )
{
    switch( ev )
    {
    case EV_BUTTON_WHITE:
        m_NextState = m_State;
        m_State = STATE_HELP;
        break;
    case EV_BUTTON_A:
        // Game type was chosen
        m_SessionInfo.SetGameType( m_dwCurrItem );
        // Fall thru to return to previous menu
        
    case EV_BUTTON_B:
    case EV_BUTTON_BACK:
        // Return to customize menu
        m_State = STATE_OPTIMATCH;
        m_dwCurrItem = 0;
        break;
        
    case EV_UP:
        // Move to previous item; allow wrap to bottom
        if( m_dwCurrItem == 0 )
            m_dwCurrItem = TYPE_MAX - 1;
        else
            --m_dwCurrItem;
        break;
        
    case EV_DOWN:
        // Move to next item; allow wrap to top
        if( m_dwCurrItem == TYPE_MAX - 1 )
            m_dwCurrItem = 0;
        else
            ++m_dwCurrItem;
        break;
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateStateSelectLevel()
// Desc: Select player rating
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateSelectLevel( Event ev )
{
    switch( ev )
    {
    case EV_BUTTON_WHITE:
        m_NextState = m_State;
        m_State = STATE_HELP;
        break;
    case EV_BUTTON_A:
        // Player level was chosen
        assert( m_dwCurrItem >= LEVEL_ANY && m_dwCurrItem <= LEVEL_ADVANCED );

        m_SessionInfo.SetPlayerLevel( m_dwCurrItem );

        // Fall thru to return to previous menu
        
        case EV_BUTTON_B:
        case EV_BUTTON_BACK:
            // Return to customize menu
            m_State = STATE_OPTIMATCH;
            m_dwCurrItem = 0;
            break;
            
        case EV_UP:
            // Move to previous item; allow wrap to bottom
            if( m_dwCurrItem == 0 )
                m_dwCurrItem = LEVEL_MAX - 1;
            else
                --m_dwCurrItem;
            break;
            
        case EV_DOWN:
            // Move to next item; allow wrap to top
            if( m_dwCurrItem == LEVEL_MAX - 1 )
                m_dwCurrItem = 0;
            else
                ++m_dwCurrItem;
            break;
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateStateSelectStyle()
// Desc: Select game style
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateSelectStyle( Event ev )
{
    switch( ev )
    {
    case EV_BUTTON_WHITE:
        m_NextState = m_State;
        m_State = STATE_HELP;
        break;
    case EV_BUTTON_A:
        // Game style was chosen
        assert( m_dwCurrItem >= STYLE_ANY && m_dwCurrItem <= STYLE_MIXED );
        m_SessionInfo.SetGameStyle( m_dwCurrItem );

        // Fall thru to return to previous menu
        
        case EV_BUTTON_B:
        case EV_BUTTON_BACK:
            // Return to customize menu
            m_State = STATE_OPTIMATCH;
            m_dwCurrItem = 0;
            break;
            
        case EV_UP:
            // Move to previous item; allow wrap to bottom
            if( m_dwCurrItem == 0 )
                m_dwCurrItem = STYLE_MAX - 1;
            else
                --m_dwCurrItem;
            break;
            
        case EV_DOWN:
            // Move to next item; allow wrap to top
            if( m_dwCurrItem == STYLE_MAX - 1 )
                m_dwCurrItem = 0;
            else
                ++m_dwCurrItem;
            break;
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateStateSelectName()
// Desc: Select game name
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateSelectName( Event ev )
{
    switch( ev )
    {
    case EV_BUTTON_WHITE:
        m_NextState = m_State;
        m_State = STATE_HELP;
        break;
    case EV_BUTTON_A:
        // Game name was chosen
        m_SessionInfo.SetSessionName( m_SessionNames[ m_dwCurrItem ].c_str() );
        // Fall thru to return to previous menu
        
    case EV_BUTTON_B:
    case EV_BUTTON_BACK:
        // Return to customize menu
        m_State = STATE_OPTIMATCH;
        m_dwCurrItem = 0;
        break;
        
    case EV_UP:
        // Move to previous item; allow wrap to bottom
        if( m_dwCurrItem == 0 )
            m_dwCurrItem = m_SessionNames.size() - 1;
        else
            --m_dwCurrItem;
        break;
        
    case EV_DOWN:
        // Move to next item; allow wrap to top
        if( m_dwCurrItem == m_SessionNames.size() - 1 )
            m_dwCurrItem = 0;
        else
            ++m_dwCurrItem;
        break;
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateStateSelectSession()
// Desc: Select game session
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateSelectSession( Event ev )
{
    switch( ev )
    {
    case EV_BUTTON_WHITE:
        m_NextState = m_State;
        m_State = STATE_HELP;
        break;
    case EV_BUTTON_A:
        // Join selected session
        m_dwSessionIndex = m_dwCurrItem;
        m_State = STATE_REQUEST_JOIN;
        BeginJoinSession();
        break;
        
    case EV_BUTTON_B:
    case EV_BUTTON_BACK:
        // Return to customize menu
        m_State = STATE_OPTIMATCH;
        m_dwCurrItem = 0;
        break;
        
    case EV_UP:
        // Move to previous item; allow wrap to bottom
        if( m_dwCurrItem == 0 )
            m_dwCurrItem = m_SessionList.size() - 1;
        else
            --m_dwCurrItem;
        break;
        
    case EV_DOWN:
        // Move to next item; allow wrap to top
        if( m_dwCurrItem == m_SessionList.size() - 1 )
            m_dwCurrItem = 0;
        else
            ++m_dwCurrItem;
        break;
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateStateGameSearch()
// Desc: Searching for matching game session(s)
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateGameSearch( Event ev )
{
    switch( ev )
    {
    case EV_BUTTON_B:
    case EV_BUTTON_BACK:
        m_OptiMatchQuery.Cancel();
        m_dwCurrItem = 0;
        if( m_bIsQuickMatch )
        {
            m_State = STATE_SELECT_MATCH;
        }
        else
        {
            m_State = STATE_OPTIMATCH;
        }
        return;
    }
    
    // Wait for matchmaking server to return results
    HRESULT hr = m_OptiMatchQuery.Process();
    if( hr != XONLINETASK_S_RUNNING )
    {
        // Handle errors
        if( FAILED(hr) )
        {
            m_UI.SetErrorStr( L"Game search failed with error %x", hr );
            Reset( TRUE );
            return;
        }
        
        // Get the list returned by the matchmaking server
        DWORD dwResults = m_OptiMatchQuery.Results.Size();
        
        m_SessionList.clear();
        
        // Save the results (for a task initiated by
        // XOnlineMatchSessionFindFromID, there a single session returned )
        for( DWORD i = 0; i < dwResults; ++i )
        {
            m_SessionList.push_back( SessionInfo( m_OptiMatchQuery.Results[i] ) );
        }
                
        // If we found at least one game, join it automatically
        if( dwResults > 0 )
        {
            // If this a quick match join the first one
            if( m_bIsQuickMatch )
            {
                m_State = STATE_REQUEST_JOIN;
                m_dwSessionIndex = 0;
                BeginJoinSession();
            }
            else        
            {
                m_dwCurrItem = 0;
                m_State = STATE_SELECT_SESSION;
            }
        }
        else
        {
            // We didn't find any sessions, so we'll create our own
            m_State = STATE_CREATE_SESSION;
            BeginCreateSession();
        }
        
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateStateGameSearchByID()
// Desc: Searching for a session matching an ID
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateGameSearchByID( Event ev )
{
    switch( ev )
    {
    case EV_BUTTON_B:
    case EV_BUTTON_BACK:
        m_FindByIDQuery.Cancel();
        m_dwCurrItem = 0;
        m_bAcceptedInvite = FALSE;
        ZeroMemory( &m_AcceptedInvite, sizeof( m_AcceptedInvite ) );
        m_State = STATE_SELECT_MATCH;
        return;
    }
    
    // Wait for matchmaking server to return results
    HRESULT hr = m_FindByIDQuery.Process();
    if( hr != XONLINETASK_S_RUNNING )
    {
        // Handle errors
        if( FAILED(hr) )
        {
            m_UI.SetErrorStr( L"Invited Session search failed with error %x", hr );
            Reset( TRUE );
            return;
        }

        // Get the list returned by the matchmaking server
        DWORD dwResults = m_FindByIDQuery.Results.Size();
        
        m_SessionList.clear();
        
        if( dwResults == 1 )
        {
            m_SessionList.push_back( SessionInfo(  m_FindByIDQuery.Results[0] ) );
            // Found the session, now join it
            m_State = STATE_REQUEST_JOIN;
            m_dwSessionIndex = 0;
            BeginJoinSession();
        }
        else
        {
            // We didn't find the session, so we'll create our own
            m_State = STATE_CREATE_SESSION;
            BeginCreateSession();
        }
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateStateRequestJoin()
// Desc: Joining session
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateRequestJoin( Event ev )
{
    switch( ev )
    {
    case EV_BUTTON_B:
    case EV_BUTTON_BACK:
        // Cancel request; return to match menu
        Reset( FALSE );
        m_GameJoinTimer.Stop();
        break;
    }
    
    // See if the game has replied
    m_GameMsg.ProcessMessages( m_Players );
    
    // We wait for up to GAME_JOIN_TIME seconds. If the game didn't
    // respond, display an error message, then create our own session.
    if( m_GameJoinTimer.GetElapsedSeconds() > GAME_JOIN_TIME )
    {
        m_GameJoinTimer.Stop();
        m_State = STATE_ERROR;
        m_UI.SetErrorStr( L"Game did not respond" );
        m_NextState = STATE_CREATE_SESSION;
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateStateCreateSession()
// Desc: Creating session
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateCreateSession( Event ev )
{
    switch( ev )
    {
    case EV_BUTTON_B:
    case EV_BUTTON_BACK:
        // Cancel the task
        m_HostedSession.Reset();
        m_dwCurrItem = 0;
        m_State = STATE_SELECT_MATCH;
        return;
    }
    
    // Wait for matchmaking server to save new session info
    HRESULT hr = m_HostedSession.Process();
    // Handle errors
    if( FAILED(hr) )
    {
        m_UI.SetErrorStr( L"Game session creation failed with error %x", hr );
        Reset( TRUE );
        return;
    }
    else
    {
        if( hr != XONLINETASK_S_RUNNING )
        {
            // Handle errors
            if( FAILED(hr) )
            {
                m_UI.SetErrorStr( 
                    L"XOnlineMatchSessionGetInfo failed with error 0x%x", hr );
                Reset( TRUE );
                return;
            }

            
            // We are now the host of a new game
            m_bIsHost = TRUE;
            m_State = STATE_PLAY_GAME;
            m_dwCurrItem = 0;
            m_HeartbeatTimer.StartZero();
            SetStatus( L"Created Session" );
            
            // Notify the world of our state change
            DWORD dwState = XONLINE_FRIENDSTATE_FLAG_ONLINE |
                XONLINE_FRIENDSTATE_FLAG_PLAYING |
                XONLINE_FRIENDSTATE_FLAG_JOINABLE;
            if( XBVoice_HasDevice() )
                dwState |= XONLINE_FRIENDSTATE_FLAG_VOICE;
            SetPlayerState( dwState );
            
            m_GameMsg.SetUser( m_strUser, m_bIsHost );
            m_GameMsg.SetSessionID( m_HostedSession.SessionID );
            
            // Note that the session remains "active" (m_hMatchTask isn't
            // closed), and must be pumped in order for the session to
            // remain active on the matchmaking server.
        }
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateStatePlayGame()
// Desc: Play game
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStatePlayGame( Event ev )
{
    // Handle net messages
    if( m_GameMsg.ProcessMessages( m_Players ) )
        return;
    
    // Send keep-alives
    if( m_HeartbeatTimer.GetElapsedSeconds() > PLAYER_HEARTBEAT )
    {
        m_GameMsg.SendHeartbeat( m_Players );
        m_HeartbeatTimer.StartZero();
    }
    
    // Handle other players dropping
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
    case EV_BUTTON_A:
        switch( m_dwCurrItem )
        {
        case GAME_WAVE:           SendWave();      break;
        case GAME_LEAVE:          LeaveGame();     break;
        case GAME_INVITE_FRIENDS: InviteFriends(); break;
        default:         
            assert( FALSE ); break;
        }
        break;
        
        case EV_UP:
            // Move to previous item; allow wrap to bottom
            if( m_dwCurrItem == 0 )
                m_dwCurrItem = GAME_MAX - 1;
            else
                --m_dwCurrItem;
            break;
            
        case EV_DOWN:
            // Move to next item; allow wrap to top
            if( m_dwCurrItem == GAME_MAX - 1 )
                m_dwCurrItem = 0;
            else
                ++m_dwCurrItem;
            break;
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateStateDeleteSession()
// Desc: Delete game session from matchmaking server
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateDeleteSession( Event ev )
{
    switch( ev )
    {
    case EV_BUTTON_B:
    case EV_BUTTON_BACK:
        Reset(FALSE); // Return to matchmaking
        return;
    }
    
    HRESULT hr = m_HostedSession.Process();
    if( hr != XONLINETASK_S_RUNNING )
    {
        // Handle errors
        if( FAILED(hr) )
        {
            m_UI.SetErrorStr( L"Game session deletion failed with error %x", hr );
            Reset( TRUE );
            return;
        }
        
        // Return to matchmaking
        Reset( FALSE );
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateStateError()
// Desc: Handle error screen
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateError( Event ev )
{
    switch( ev )
    {
    case EV_BUTTON_A:
        m_State = m_NextState;
        
        // Special case: if the next state is "create session," we must
        // begin the session creation process
        if( m_State == STATE_CREATE_SESSION )
            BeginCreateSession();
        break;
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
        m_State = m_NextState;
        break;
    }
}




//-----------------------------------------------------------------------------
// Name: BeginLogin()
// Desc: Initiate the authentication process
//-----------------------------------------------------------------------------
VOID CXBoxSample::BeginLogin()
{
    // Select a reasonable controller for the current player by choosing
    // the first controller found. Game code should do this much more
    // precisely. See below for details.
    for( m_dwUserIndex = 0; m_dwUserIndex < XGetPortCount(); ++m_dwUserIndex )
    {
        if( m_Gamepad[m_dwUserIndex].hDevice )
            break;
    }
    if( m_dwUserIndex >= XGetPortCount() )
        m_dwUserIndex = 0;
    
    // XOnlineLogon() allows a list of up to 4 players (1 per controller)
    // to login in a single call. This sample shows how to authenticate
    // a single user. The list must be a one-to-one match of controller 
    // to player in order for the online system to recognize which player
    // is using which controller.
    XONLINE_USER pUserList[ XGetPortCount() ] = { 0 };
    CopyMemory( &pUserList[ m_dwUserIndex ], &m_UserList[ m_dwCurrUser ],
        sizeof( XONLINE_USER ) );
    
    // Initiate the login process. XOnlineTaskContinue() is used to poll
    // the status of the login.
    HRESULT hr = XOnlineLogon( pUserList, m_pServices, NUM_SERVICES, 
        NULL, &m_hOnlineTask );
    
    if( FAILED(hr) )
    {
        m_hOnlineTask.Close();
        m_UI.SetErrorStr( L"Login failed to start. Error %x", hr );
        Reset( TRUE );
    }
}




//-----------------------------------------------------------------------------
// Name: BeginSessionSearch()
// Desc: Initiate the game search process
//-----------------------------------------------------------------------------
VOID CXBoxSample::BeginSessionSearch()
{    
    ULONGLONG ulGameType;
    ULONGLONG ulPlayerLevel;
    ULONGLONG ulGameStyle;
    
    // Call the MatchSim generated query function with the appropriate parameters.
    if( !m_bIsQuickMatch )
    {
        if( TYPE_ANY == m_SessionInfo.GetGameType() )
        {
            ulGameType = X_MATCH_NULL_INTEGER;
        }
        else
        {
            ulGameType = m_SessionInfo.GetGameType();
        }
        
        if( LEVEL_ANY == m_SessionInfo.GetPlayerLevel() )
        {
            ulPlayerLevel = X_MATCH_NULL_INTEGER;
        }
        else
        {
            ulPlayerLevel = m_SessionInfo.GetPlayerLevel();
        }
                
        if( STYLE_ANY == m_SessionInfo.GetGameStyle() )
        {
            ulGameStyle = X_MATCH_NULL_INTEGER;
        }
        else
        {
            ulGameStyle = m_SessionInfo.GetGameStyle();
        }
        
    }
    else
    {
        // For QuickMatch we will just look for any session
        ulGameType = X_MATCH_NULL_INTEGER;
        ulGameStyle = X_MATCH_NULL_INTEGER;
        ulPlayerLevel = X_MATCH_NULL_INTEGER;
    }

    HRESULT hr = m_OptiMatchQuery.Query( ulGameType, ulPlayerLevel, ulGameStyle );

    if( FAILED(hr) )
    {
        m_UI.SetErrorStr( L"Game search failed to start. Error %x", hr );
        Reset( TRUE );
    }
}




//-----------------------------------------------------------------------------
// Name: BeginCreateSession()
// Desc: Initiate the game session creation process
//-----------------------------------------------------------------------------
VOID CXBoxSample::BeginCreateSession()
{
    // Generate a random session name if we don't currently have one
    if( *m_SessionInfo.GetSessionName() == 0 )
        m_SessionInfo.GenRandSessionName();
    
    assert( !m_HostedSession.Exists() );
    
    // Initialize the create request
    
    // Set session attributes 
    
    // Game type
    //---------------------------------------------------------------------
    // If not specified, default to TYPE_SHORT.
    // Game type is the first and only session integer parameter.
    if( m_SessionInfo.GetGameType() == TYPE_ANY )
        m_SessionInfo.SetGameType( TYPE_SHORT );

    m_HostedSession.GameType = m_SessionInfo.GetGameType();
    
    // Player level
    //---------------------------------------------------------------------
    // If not specified, default to LEVEL_BEGINNER.
    // Player level is the first session string parameter.
    if( m_SessionInfo.GetPlayerLevel() == LEVEL_ANY )
        m_SessionInfo.SetPlayerLevel( LEVEL_BEGINNER );    

    m_HostedSession.PlayerLevel = m_SessionInfo.GetPlayerLevel();


    // Session name
    //---------------------------------------------------------------------
    // Always specified.
    // The second session string parameter.
    assert( *m_SessionInfo.GetSessionName() != 0 );
    m_HostedSession.SessionName = m_SessionInfo.GetSessionName();


    // Game style
    //---------------------------------------------------------------------
    // If not specified, default to STYLE_HEAVY.
    if( m_SessionInfo.GetGameStyle() == STYLE_ANY )
        m_SessionInfo.SetGameStyle( STYLE_HEAVY );
    m_HostedSession.GameStyle = m_SessionInfo.GetGameStyle();

    // ConfigInfo
    //---------------------------------------------------------------------
    // The first and only blob attribute.  
    // This is only used to demonstrate how to set a blob attribute
    m_SessionInfo.SetConfigInfo( B( 3, "ABC" ) );
    m_HostedSession.ConfigInfo = m_SessionInfo.GetConfigInfo();


    // The first (and only) user parameter is the player name
    m_SessionInfo.SetOwnerName( m_strUser );
    m_HostedSession.OwnerName = m_strUser; 
    
    m_dwSlotsInUse = 1;
    
    // Limit the number of players to MAX_PLAYERS public slots and no
    // private (invitation only) slots. Note that we add ourself as a player.
    m_HostedSession.PublicFilled = m_dwSlotsInUse;
    m_HostedSession.PublicOpen   = MAX_PLAYERS - m_dwSlotsInUse;
    m_HostedSession.PrivateFilled = 0;
    m_HostedSession.PrivateOpen   = 0;



    HRESULT hr = m_HostedSession.Create();

    if( FAILED( hr ) )
    {
        m_UI.SetErrorStr( L"Game session failed to start. Error %x", hr );
        Reset( TRUE );
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateSession()
// Desc: Notify match server about session change
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateSession( )
{
    assert( m_HostedSession.Exists() != NULL );

    m_HostedSession.PublicFilled = m_dwSlotsInUse;
    m_HostedSession.PublicOpen   = MAX_PLAYERS - m_dwSlotsInUse;

    // A title may call CSession::Update repeatedly without having to
    // wait for the update to complete
    HRESULT hr = m_HostedSession.Update();

    if( FAILED(hr) )
    {
        m_UI.SetErrorStr( L"Game session failed to update. Error %x", hr );
        Reset( TRUE );
    }   
}




//-----------------------------------------------------------------------------
// Name: BeginDeleteSession()
// Desc: Initiate the game session removal process
//-----------------------------------------------------------------------------
VOID CXBoxSample::BeginDeleteSession()
{
    assert( m_HostedSession.Exists() );
    
    // Initialize the delete request
    HRESULT hr = m_HostedSession.Delete();
    if( FAILED(hr) )
    {
        m_UI.SetErrorStr( L"Failed to start session deletion. Error %x", hr );
        Reset( TRUE );
    }
}




//-----------------------------------------------------------------------------
// Name: BeginJoinSession()
// Desc: Attempt to join a game
//-----------------------------------------------------------------------------
VOID CXBoxSample::BeginJoinSession()
{
    assert( m_SessionList.size() > 0 );
    
    SessionInfo& Session = m_SessionList[ m_dwSessionIndex ];
    
    // Clear any registered sessions
    if( m_bJoinedGame )
    {
        INT iResult = XNetUnregisterKey( &m_xnJoinedSessionID );
        assert( iResult == NO_ERROR );
        (VOID)iResult;
        m_bJoinedGame = FALSE;
        ZeroMemory( &m_xnJoinedSessionID, sizeof( XNKID ) );
    }
    
    // We found a valid session with an available player slot.
    // Register the session key.
    INT iResult = XNetRegisterKey( Session.GetSessionID(),
        Session.GetKeyExchangeKey() );
    if( iResult != NO_ERROR )
    {
        m_UI.SetErrorStr( L"Unable to establish session with game" );
        Reset( TRUE );
        return;
    }
    m_bJoinedGame = TRUE;
    
    // Save the key ID because we need to unregister it laer
    CopyMemory( &m_xnJoinedSessionID, Session.GetSessionID(), sizeof( XNKID ) );
    
    // Store the game name
    m_SessionInfo.SetSessionName( Session.GetSessionName() );
    m_SessionInfo.SetGameType( Session.GetGameType() );
    m_SessionInfo.SetPlayerLevel( Session.GetPlayerLevel() );
    m_SessionInfo.SetOwnerName( Session.GetOwnerName () );
    m_SessionInfo.SetGameStyle( Session.GetGameStyle() );
    m_SessionInfo.SetConfigInfo( Session.GetConfigInfo() );
    
    // Convert the XNADDR of the host to the INADDR we'll use to
    // join the game
    iResult = XNetXnAddrToInAddr( Session.GetHostAddr(),
        &m_xnJoinedSessionID, &m_inHostAddr );
    assert( iResult == NO_ERROR );
    
    m_GameMsg.SetUser( m_strUser, FALSE );
    m_GameMsg.SetSessionID( m_xnJoinedSessionID );
    
    // Request join approval from the game and await a response
    SOCKADDR_IN sa;
    sa.sin_family = AF_INET;
    sa.sin_addr   = m_inHostAddr;
    sa.sin_port   = htons( GameMsg::GAME_PORT );
    m_GameMsg.SendJoinGame( sa, m_strUser, m_qwUserID );
    m_GameJoinTimer.StartZero();
}




//-----------------------------------------------------------------------------
// Name: AddPlayer()
// Desc: Notify match server about new player
//-----------------------------------------------------------------------------
VOID CXBoxSample::AddPlayer()
{
    assert( m_bIsHost );
    m_dwSlotsInUse++;
    if( m_dwSlotsInUse == MAX_PLAYERS )
    {
        // The session is now full. Turn off Qos listening.
        // This will send "go-away" responses to probes from
        // other consoles who will see this session during 
        // matchmaking while it is being updated (once the
        // session is updated, new searches will not return
        // it since there will be no public slots)
        m_HostedSession.Listen( FALSE );
    }
    UpdateSession();
}




//-----------------------------------------------------------------------------
// Name: RemovePlayer()
// Desc: Notify match server about player departure
//-----------------------------------------------------------------------------
VOID CXBoxSample::RemovePlayer()
{
    assert( m_dwSlotsInUse );
    assert( m_bIsHost );

    if( m_dwSlotsInUse == MAX_PLAYERS )
    {
        // The session used to be full.
        // Turn on Qos listening, so
        // that other consoles can probe us.
        m_HostedSession.Listen( TRUE );
    }

    m_dwSlotsInUse--;
    UpdateSession();
}




//-----------------------------------------------------------------------------
// Name: SendWave()
// Desc: Wave to all other players in game
//-----------------------------------------------------------------------------
VOID CXBoxSample::SendWave()
{
    // Indicate that you waved
    SetStatus( L"You waved" );
    m_GameMsg.SendWave( m_Players );
}




//-----------------------------------------------------------------------------
// Name: LeaveGame()
// Desc: Exit the game
//-----------------------------------------------------------------------------
VOID CXBoxSample::LeaveGame()
{
    // Other players will detect that we left because they will no longer
    // receive heartbeats
    
    // If the host leaves, remove the session from the matchmaking server
    if( m_bIsHost )
    {
        m_bIsHost = FALSE;
        m_HeartbeatTimer.Stop();
        m_Players.clear();
        m_State = STATE_DELETE_SESSION;
        BeginDeleteSession();
    }
    else
    {
        Reset( FALSE );
    }
    
    // Notify the world of our state change
    DWORD dwState = XONLINE_FRIENDSTATE_FLAG_ONLINE;
    if( XBVoice_HasDevice() )
        dwState |= XONLINE_FRIENDSTATE_FLAG_VOICE;
    SetPlayerState( dwState );
}




//-----------------------------------------------------------------------------
// Name: InviteFriends()
// Desc: Send game invites to friends
//-----------------------------------------------------------------------------
VOID CXBoxSample::InviteFriends()
{
    // Only shown as an example; not currently implemented
    if( m_FriendList.empty() ) 
    {
        m_UI.SetErrorStr( L"Your Friends list is empty" );
        m_State = STATE_ERROR;
        m_NextState = STATE_PLAY_GAME;
        return;
    }
    
    XONLINE_FRIEND* pFriends = &m_FriendList[0];
    HRESULT hr = XOnlineFriendsGameInvite( m_dwUserIndex, 
        m_bIsHost ? m_HostedSession.SessionID : m_xnJoinedSessionID, m_FriendList.size(), pFriends );
    assert( SUCCEEDED( hr ) );
    (VOID)hr; // avoid compiler warnings
    
    SetStatus( L"Invited friends" );
}




//-----------------------------------------------------------------------------
// Name: OnJoinGame()
// Desc: Handle new player joining game that we host
//-----------------------------------------------------------------------------
VOID CXBoxSample::OnJoinGame( const CXBNetPlayerInfo& playerInfo )
{
    m_Players.push_back( playerInfo );
    SetStatus( L"%.*s has joined the game", MAX_PLAYER_STR, playerInfo.strPlayerName );
    AddPlayer();
}




//-----------------------------------------------------------------------------
// Name: OnJoinApproved()
// Desc: We've been approved for game entry by the given host
//-----------------------------------------------------------------------------
VOID CXBoxSample::OnJoinApproved( const CXBNetPlayerInfo& hostInfo )
{
    m_Players.push_back( hostInfo );
    
    // Enter into the game UI
    m_State = STATE_PLAY_GAME;
    
    // Set the default game item to "wave"
    m_dwCurrItem = GAME_WAVE;
    
    SetStatus( L"You have joined the game" );
    m_HeartbeatTimer.StartZero();

    m_GameMsg.SetUser( m_strUser, FALSE );
    
    // Notify the world of our state change
    DWORD dwState = XONLINE_FRIENDSTATE_FLAG_ONLINE |
        XONLINE_FRIENDSTATE_FLAG_JOINABLE |
        XONLINE_FRIENDSTATE_FLAG_PLAYING;
    if( XBVoice_HasDevice() )
        dwState |= XONLINE_FRIENDSTATE_FLAG_VOICE;
    SetPlayerState( dwState );
}




//-----------------------------------------------------------------------------
// Name: OnJoinApprovedAddPlayer()
// Desc: Receiving information on others players already in the game
//-----------------------------------------------------------------------------
VOID CXBoxSample::OnJoinApprovedAddPlayer( const CXBNetPlayerInfo& playerInfo )
{
    if( playerInfo.qwUserID != m_qwUserID )
        m_Players.push_back( playerInfo );
}




//-----------------------------------------------------------------------------
// Name: OnJoinDenied()
// Desc: Handle join denied
//-----------------------------------------------------------------------------
VOID CXBoxSample::OnJoinDenied()
{
    // If for some reason we receive a "join denied" message and we're
    // already playing a game, ignore the message.
    if( m_State == STATE_PLAY_GAME )
        return;
    
    // The session we wanted to join is full. Display error
    Reset(FALSE);
    m_UI.SetErrorStr( L"The session is full.\nChoose another session." );
    m_State = STATE_ERROR;
    DWORD dwNumSessions = m_SessionList.size();
    m_NextState = dwNumSessions > 1 ?  STATE_SELECT_SESSION : STATE_SELECT_MATCH; 
}




//-----------------------------------------------------------------------------
// Name: OnPlayerJoined()
// Desc: The given player joined our game
//-----------------------------------------------------------------------------
VOID CXBoxSample::OnPlayerJoined( const CXBNetPlayerInfo& playerInfo )
{
    MatchInAddr matchInAddr( playerInfo.inAddr );
    
    // First check to make sure the player isn't already in the list.
    // If so, remove the player first.  This can happen if the player
    // drops out of a game and rejoins before the next heartbeat.
    CXBNetPlayerList::iterator i = 
        std::find_if( m_Players.begin(), m_Players.end(), matchInAddr );
    
    if( i != m_Players.end() )
    {       
        m_Players.erase( i );
    }
    
    m_Players.push_back( playerInfo );
    SetStatus( L"%.*s has joined the game", MAX_PLAYER_STR, playerInfo.strPlayerName );
}




//-----------------------------------------------------------------------------
// Name: OnWave()
// Desc: The given player waved to us
//-----------------------------------------------------------------------------
VOID CXBoxSample::OnWave( const CXBNetPlayerInfo& playerInfo )
{
    SetStatus( L"%.*s waved", MAX_PLAYER_STR, playerInfo.strPlayerName );
}




//-----------------------------------------------------------------------------
// Name: OnHeartbeat()
// Desc: The given player sent us a heartbeat
//-----------------------------------------------------------------------------
VOID CXBoxSample::OnHeartbeat( const CXBNetPlayerInfo& playerInfo )
{
    MatchInAddr matchInAddr( playerInfo.inAddr );
    
    // Find out who sent a heartbeat by matching on name
    CXBNetPlayerList::iterator i = 
        std::find_if( m_Players.begin(), m_Players.end(), matchInAddr );
    
    // We expect that we know about the player
    assert( i != m_Players.end() );
    
    i->dwLastHeartbeat = GetTickCount();
}




//-----------------------------------------------------------------------------
// Name: OnPlayerDropout()
// Desc: The given player left the game
//-----------------------------------------------------------------------------
VOID CXBoxSample::OnPlayerDropout( const CXBNetPlayerInfo& playerInfo, BOOL bIsHost )
{
    if( bIsHost )
    {
        SetStatus( L"Host %.*s left game.\nGame closed to new players.",
        MAX_PLAYER_STR, playerInfo.strPlayerName );
    }
    else
    {
        SetStatus( L"%.*s left the game", MAX_PLAYER_STR, playerInfo.strPlayerName );
        // If this console is the host, remove inform the matchmaking server
        if( m_bIsHost )
             RemovePlayer();
    }

    MatchInAddr matchInAddr( playerInfo.inAddr );
    
    // Find out who we need to delete by matching on name
    CXBNetPlayerList::iterator i = 
        std::find_if( m_Players.begin(), m_Players.end(), matchInAddr );
    
    // We expect that we know about the player
    assert( i != m_Players.end() );
    
    m_Players.erase( i );
}




//-----------------------------------------------------------------------------
// Name: SetStatus()
// Desc: Set the status string
//-----------------------------------------------------------------------------
VOID __cdecl CXBoxSample::SetStatus( const WCHAR* strFormat, ... )
{
    va_list pArgList;
    va_start( pArgList, strFormat );
    
    INT iChars = wvsprintfW( m_strStatus, strFormat, pArgList );
    assert( iChars < MAX_STATUS_STR );
    (VOID)iChars; // avoid compiler warning
    
    va_end( pArgList );
}




//-----------------------------------------------------------------------------
// Name: SetPlayerState()
// Desc: Broadcast current player state for the world
//-----------------------------------------------------------------------------
VOID CXBoxSample::SetPlayerState( DWORD dwState )
{
    HRESULT hr = XOnlineNotificationSetState( m_dwUserIndex, dwState,
        m_bIsHost ? m_HostedSession.SessionID : m_xnJoinedSessionID, 0, NULL );
    assert( SUCCEEDED( hr ) );
    (VOID)hr; // avoid compiler warning
}




//-----------------------------------------------------------------------------
// Name: Reset()
// Desc: Prepare to restart the application at the front menu
//-----------------------------------------------------------------------------
VOID CXBoxSample::Reset( BOOL bIsError )
{
    m_HostedSession.Reset();     
    m_OptiMatchQuery.Cancel();        
    m_FindByIDQuery.Cancel();
    m_hGettingAcceptedGameInviteTask.Close();
    m_bAcceptedInvite = FALSE;
    ZeroMemory( &m_AcceptedInvite, sizeof( m_AcceptedInvite ) );
    
    if( bIsError )
    {
        m_hFriendsTask.Close();
        m_hFriendEnumTask.Close();
        m_State = STATE_ERROR;
        if( m_UserList.empty() )
            m_NextState = STATE_CREATE_ACCOUNT;
        else
            m_NextState = STATE_SELECT_MATCH;
    }
    else
    {
        m_State = STATE_SELECT_MATCH;
    }
    
    m_dwCurrItem = 0;
    m_dwSessionIndex = 0; 
    
    m_bIsHost = FALSE;
    m_HeartbeatTimer.Stop();
    m_Players.clear();
    
    // Clear any registered sessions
    if( m_bJoinedGame )
    {
        INT iResult = XNetUnregisterKey( &m_xnJoinedSessionID );
        assert( iResult == NO_ERROR );
        (VOID)iResult; // avoid compiler warnings
        m_bJoinedGame = FALSE;
        ZeroMemory( &m_xnJoinedSessionID, sizeof( XNKID ) );
    }
}
