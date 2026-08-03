//-----------------------------------------------------------------------------
// File: Friends.cpp
//
// Desc: Illustrates online friends on Xbox.
//
// Hist: 10.20.01 - New for Aug M1 release 
//       01.21.02 - Updated for Feb release
//       02.15.02 - Updated for Mar release 
//       03.11.02 - Update  for April release
//       07.16.02 - Updated for Aug release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "Friends.h"
#include "XBMemUnit.h"
#include "XBVoice.h"
#include <cassert>
#include <algorithm>




//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_WHITE_BUTTON, XBHELP_PLACEMENT_1, L"Display\nhelp" },
    { XBHELP_BLACK_BUTTON, XBHELP_PLACEMENT_1, L"Toggle online\nstate" },
    { XBHELP_A_BUTTON,     XBHELP_PLACEMENT_1, L"Select account" },
    { XBHELP_B_BUTTON,     XBHELP_PLACEMENT_1, L"Cancel" },
    { XBHELP_Y_BUTTON,     XBHELP_PLACEMENT_1, L"New friend" },
    { XBHELP_DPAD,         XBHELP_PLACEMENT_1, L"Menu navigation" },
};

#define NUM_HELP_CALLOUTS 6




//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------
const XNKID SESSION_ID = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 };




//-----------------------------------------------------------------------------
// Name: struct MatchXUID
// Desc: Predicate for searching the user and mute list
//-----------------------------------------------------------------------------
struct MatchXUID
{
    XUID* m_pxuid;
    
    MatchXUID( XUID* pxuid ) : m_pxuid( pxuid ) {}
    
    bool operator()( const XONLINE_USER& user ) const
    {
        return user.xuid.qwUserID == m_pxuid->qwUserID;
    }
   
    bool operator()( const XONLINE_MUTELISTUSER& user ) const
    {
        return user.xuid.qwUserID == m_pxuid->qwUserID;
    }
};




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
// Name: CXBoxSample()
// Desc: Constructor
//-----------------------------------------------------------------------------
CXBoxSample::CXBoxSample()
{
    m_State        = STATE_SELECT_ACCOUNT;
    m_NextState    = STATE_SELECT_ACCOUNT;

    // Login to matchmaking service for access to friends
    m_pServices[0] = XONLINE_MATCHMAKING_SERVICE;
    m_pServices[1] = XONLINE_FEEDBACK_SERVICE;
    
    m_dwCurrItem   = 0;
    m_dwTopItem    = 0;
    m_dwCurrUser   = 0;
    
    m_qwUserID     = 0;
    m_dwUserIndex  = 0;
    m_bIsLoggedOn  = FALSE;
    (*m_strUser)   = 0;
    (*m_strStatus) = 0;
    m_bGameInvitePending = FALSE;   
    m_dwOldState   = 0;
    m_bCloaked     = FALSE;
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize device-dependant objects
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{    
    // Create the help
    if( FAILED( m_Help.Create( "Gamepad.xpr" ) ) )
        return E_FAIL;

    // Create a font
    if( FAILED( m_Font.Create( "Font.xpr" ) ) )
        return E_FAIL;

    // Initialize game UI
    if( FAILED( m_UI.Initialize() ) )
        return E_FAIL;
    
    // Initialize the network stack
    if( FAILED( XBNet_OnlineInit( 0 ) ) )
        return E_FAIL;
   
    // Get information on all accounts for this Xbox
    if( FAILED( XBOnline_GetUserList( m_UserList, &m_dwNumUsers ) ) )
        return E_FAIL;

    CXBMemUnit::GetMemUnitSnapshot();     
    
    // If no accounts, then player needs to create an account.
    // For development purposes, accounts are created using the
    // Online Dashboard or the XDK Launcher
    if( 0 == m_dwNumUsers )
    {
        m_State = STATE_CREATE_ACCOUNT;
        return S_OK;
    }
    
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
        swprintf( m_strError, L"This Xbox has lost its online connection" );
        Reset();
        return S_OK;
    }
    
    // Maintain our connection once we've logged on
    if( m_bIsLoggedOn )
    {
        if( m_bIsLoggedOn &&
            XOnlineGetNotification( m_dwUserIndex, 
            XONLINE_NOTIFICATION_GAME_INVITE ) )
                m_bGameInvitePending = TRUE;        

        HRESULT hr;
        
        hr = m_hOnlineTask.Continue();

        if( FAILED( hr ) )
        {
            if( hr == XONLINE_E_LOGON_KICKED_BY_DUPLICATE_LOGON )
                swprintf( m_strError, L"You have been signed out because your\n"
                                      L"account signed in on another Xbox" );
            else
                swprintf( m_strError, L"Connection was lost. Must relogin" );
            Reset();
            return S_OK;
        }
        else
        {
            if( FAILED( hr = g_FriendsManager.Process() ) )
            {
                swprintf( m_strError, L"Connection was lost. Must relogin" );
                Reset();
                return S_OK;
            }
        }

        // Update our player state
        DWORD dwState = XONLINE_FRIENDSTATE_FLAG_PLAYING;
            
        if( !m_bCloaked )
            dwState |= XONLINE_FRIENDSTATE_FLAG_ONLINE;
           
        if( XBVoice_HasDevice() && 
            ( !( m_xLoginUserID.dwUserFlags & XONLINE_USER_VOICE_NOT_ALLOWED ) ) )
            dwState |= XONLINE_FRIENDSTATE_FLAG_VOICE;
            
        // Our state is cached, so we can call this every frame-
        // it won't call XOnlineUpdate if nothing has changed
        SetPlayerState( dwState );
    }
    
    // Clear status string after 3 seconds
    if( m_StatusTimer.GetElapsedSeconds() > 3.0f )
    {
        SetStatus( L"" );
        m_StatusTimer.Stop();
    }
        
    Event ev = GetEvent();
    
    switch( m_State )
    {
        case STATE_CREATE_ACCOUNT:   UpdateStateCreateAccount( ev );   break;
        case STATE_SELECT_ACCOUNT:   UpdateStateSelectAccount( ev );   break;
        case STATE_LOGGING_ON:       UpdateStateLoggingOn( ev );       break;
        case STATE_FRIEND_LIST:      UpdateStateFriendList( ev );      break;
        case STATE_ACTION_MENU:      UpdateStateActionMenu( ev );      break;
        case STATE_NEW_FRIEND:       UpdateStateNewFriend( ev );       break;
        case STATE_CONFIRM_REMOVE:   UpdateStateConfirmRemove( ev );   break;
        case STATE_JOINING_GAME:     UpdateStateJoiningGame( ev );     break;
        case STATE_BOOT_TO_DASH:     BootToDash();                     break;
        case STATE_ERROR:            UpdateStateError( ev );           break;
        case STATE_HELP:             UpdateStateHelp( ev );            break;
        default:                     assert( FALSE );                  break;
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
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET,
                         0x000A0A6A, 1.0f, 0L );
    
    if( m_State == STATE_HELP )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        // Draw the app title
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 48, 36, 0xffffffff, L"Friends" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
    
        // Draw the UI
        switch( m_State )
        {
            case STATE_LOGGING_ON:
                m_UI.RenderLoggingOn();
                break;
            case STATE_ERROR:
                m_UI.RenderError( m_strError );
                break;
            case STATE_CREATE_ACCOUNT:
                m_UI.RenderCreateAccount( TRUE );
                break;
            case STATE_SELECT_ACCOUNT:
                m_UI.RenderSelectAccount( m_dwTopItem, m_dwCurrItem, m_UserList, m_dwNumUsers );
                break;
            case STATE_FRIEND_LIST:
                m_UI.RenderFriendList( m_dwTopItem, m_dwCurrItem, m_strStatus, m_bCloaked );
                break;
            case STATE_ACTION_MENU:
                assert( m_dwCurrFriend < g_FriendsManager.GetNumFriends( m_dwUserIndex ) );
                assert( !m_Actions.empty() );
                m_UI.RenderActionMenu( m_dwCurrItem, m_Actions, m_dwCurrFriend );
                break;
            case STATE_NEW_FRIEND:
                m_UI.RenderNewFriend( m_dwTopItem, m_dwCurrItem, m_PotentialFriendList, m_dwNumPotentialFriends );
                break;
            case STATE_CONFIRM_REMOVE:
                assert( m_dwCurrFriend < g_FriendsManager.GetNumFriends( m_dwUserIndex ) );
                m_UI.RenderConfirmRemove( m_dwCurrItem, m_dwCurrFriend );
                break;
            case STATE_JOINING_GAME:
                m_UI.RenderGameInvite();
                break;
            case STATE_BOOT_TO_DASH:
                break;
            default:
                assert( FALSE );
                break;
        }
    }

    // Draw the game invitation icon if there
    // is invitation we have received, but not
    // answered
    if( m_bGameInvitePending )
        m_UI.RenderGameInviteIcon();        
    
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
    
    // "Black"
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_BLACK ] )
        return EV_BUTTON_BLACK;
    
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
            // If any MUs are inserted/removed, need to update the
            // user account list
            DWORD dwInsertions;
            DWORD dwRemovals;
            if( CXBMemUnit::GetMemUnitChanges( dwInsertions, dwRemovals ) )
            {
                m_dwNumUsers = 0;
                XBOnline_GetUserList( m_UserList, &m_dwNumUsers );
                if( m_dwNumUsers )
                {
                    m_dwCurrItem = 0;
                    m_dwTopItem = 0;
                    m_State      = STATE_SELECT_ACCOUNT;
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
        case EV_BUTTON_A:
        {
            // Save current account information
            m_dwCurrUser = m_dwCurrItem;            
            m_qwUserID   = m_UserList[ m_dwCurrUser ].xuid.qwUserID;
            
            // Make WCHAR copy of user name
            swprintf( m_strUser, L"%S", m_UserList[ m_dwCurrUser ].szGamertag );
            
            m_State = STATE_LOGGING_ON;
            BeginLogin();
            break;
        }
            
        case EV_UP:
            // Move to previous user account; allow wrap to bottom
            if( m_dwCurrItem == 0 )
                m_dwCurrItem = m_dwNumUsers - 1;
            else
                --m_dwCurrItem;if ( m_dwCurrItem < m_dwTopItem ) m_dwTopItem = m_dwCurrItem;
            if ( m_dwCurrItem >= ( m_dwTopItem + MAX_ACCOUNTS_DISPLAYED ) )
                m_dwTopItem = m_dwCurrItem - MAX_ACCOUNTS_DISPLAYED + 1;
           
    
            if ( m_dwCurrItem < m_dwTopItem ) m_dwTopItem = m_dwCurrItem;
            if ( m_dwCurrItem >= ( m_dwTopItem + MAX_ACCOUNTS_DISPLAYED ) )
                m_dwTopItem = m_dwCurrItem - MAX_ACCOUNTS_DISPLAYED + 1;

            break;
            
        case EV_DOWN:
            // Move to next user account; allow wrap to top
            if( m_dwCurrItem == m_dwNumUsers - 1 )
                m_dwCurrItem = 0;
            else
                ++m_dwCurrItem;

            if ( m_dwCurrItem < m_dwTopItem ) m_dwTopItem = m_dwCurrItem;
            if ( m_dwCurrItem >= ( m_dwTopItem + MAX_ACCOUNTS_DISPLAYED ) )
                m_dwTopItem = m_dwCurrItem - MAX_ACCOUNTS_DISPLAYED + 1;

            break;

        case EV_BUTTON_WHITE:
            m_NextState = m_State;
            m_State = STATE_HELP;
            break;
           
        default:
            // If any MUs are inserted/removed, need to update the
            // user account list
            DWORD dwInsertions;
            DWORD dwRemovals;
            if( CXBMemUnit::GetMemUnitChanges( dwInsertions, dwRemovals ) )
            {
                m_dwNumUsers = 0;
                XBOnline_GetUserList( m_UserList, &m_dwNumUsers );
                if( 0 == m_dwNumUsers )
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
    HRESULT hr;
    
    switch( ev )
    {
        case EV_BUTTON_B:
        case EV_BUTTON_BACK:
            // Cancel the task
            m_hOnlineTask.Close();
            m_dwTopItem = 0;
            m_dwCurrItem = 0;
            m_State = STATE_SELECT_ACCOUNT;
            return;
    }
    
    hr = m_hOnlineTask.Continue();
    
    // Check login status
    if( hr != XONLINETASK_S_RUNNING )
    {
        BOOL bSuccess = TRUE;
        HRESULT hrService = S_OK;
        
        // Check for general errors
        if( hr != XONLINE_S_LOGON_CONNECTION_ESTABLISHED )
        {
            swprintf( m_strError, L"Login failed.\n\n"
                                  L"Error 0x%x returned by "
                                  L"XOnlineTaskContinue", hr );
            bSuccess = FALSE;
        }
        else
        {

            // Next, check if the user was actually logged on
            PXONLINE_USER pLoggedOnUsers = XOnlineGetLogonUsers();
            assert( pLoggedOnUsers );
            
            hr = pLoggedOnUsers[ m_dwUserIndex ].hr;
            
            if( FAILED( hr ) )
            {
                swprintf( m_strError, L"User Login failed (Error 0x%x)", hr );
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
                        swprintf( m_strError, L"Login failed.\n\n"
                                              L"Error 0x%x logging into service %d",
                                              hrService, m_pServices[i] );
                        bSuccess    = FALSE;
                        break;
                    }
                }
            }

            // set the xuid so we can verify voice info later
            m_xLoginUserID = pLoggedOnUsers[ m_dwUserIndex ].xuid;
        }
        
        if( bSuccess )
        {
            // We're now on the system
            m_bIsLoggedOn = TRUE;
            
            // Notify the world
            DWORD dwState = XONLINE_FRIENDSTATE_FLAG_ONLINE |
                XONLINE_FRIENDSTATE_FLAG_PLAYING;
            if( XBVoice_HasDevice() && 
                ( !( m_xLoginUserID.dwUserFlags & XONLINE_USER_VOICE_NOT_ALLOWED ) ) )
                dwState |= XONLINE_FRIENDSTATE_FLAG_VOICE;
            
            SetPlayerState( dwState );
                       
            // Start handling friend notifications and
            // begin the process of retrieving the
            // mute list
            if( SUCCEEDED( InitFriends() ) )
            {
                m_State = STATE_FRIEND_LIST;
                m_dwTopItem = 0;
                m_dwCurrItem = 0;            
            }
        }
        else
        {
            Reset();
        }
    }
}



//-----------------------------------------------------------------------------
// Name: UpdateStateFriendList()
// Desc: Friend list navigation
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateFriendList( Event ev )
{               
    switch( ev )
    {
        case EV_BUTTON_B:
        case EV_BUTTON_BACK:        
            m_bCloaked    = FALSE;
            m_bGameInvitePending = FALSE;

            // return to account selection        
            Reset();
            m_dwTopItem = 0;
            m_dwCurrItem = 0;
            m_State = STATE_SELECT_ACCOUNT;
          
            return;

        case EV_BUTTON_WHITE:
            m_NextState = m_State;
            m_State = STATE_HELP;
            return;
    }

    UpdatePotentialFriends();

    // Update if a friend has requested us to be friends
    if( XOnlineGetNotification( m_dwUserIndex, XONLINE_NOTIFICATION_FRIEND_REQUEST ) )
        SetStatus( L"You have received a friend request" );

    // Handle the special case of an empty list
    if( g_FriendsManager.GetNumFriends( m_dwUserIndex ) == 0 )
    {
        switch( ev )
        {
            case EV_BUTTON_Y:
                m_State = STATE_NEW_FRIEND;
                m_dwCurrItem = 0;            
                m_dwTopItem = 0;            
                break;
        }
        return;
    }
    
    // If the list shrunk, may need to update last element ptr
    if( m_dwCurrItem >= g_FriendsManager.GetNumFriends( m_dwUserIndex ) )
    {
        m_dwCurrItem = g_FriendsManager.GetNumFriends( m_dwUserIndex ) - 1;
        
        // If we're at the top of the displayed list, shift the display
        if( m_dwCurrItem == m_dwTopItem )
        {
            if( m_dwTopItem > 0 )
                --m_dwTopItem;
        }
        
        // If we're at the bottom of the displayed list, shift the display
        if( m_dwCurrItem == m_dwTopItem + MAX_FRIENDS_DISPLAYED - 1 )
        {
            if( m_dwTopItem + MAX_FRIENDS_DISPLAYED < g_FriendsManager.GetNumFriends( m_dwUserIndex ) )
                ++m_dwTopItem;
        }
    }
    
    switch( ev )
    {
        case EV_BUTTON_A:
            // Track the current friend
            m_dwCurrFriend = m_dwCurrItem;
            m_State = STATE_ACTION_MENU;
            ConfigureActionMenu();
            m_dwTopItem = m_dwCurrItem = 0;
            break;

        case EV_BUTTON_Y:
            m_State = STATE_NEW_FRIEND;
            m_dwTopItem = m_dwCurrItem = 0;
            break;

        case EV_BUTTON_BLACK:
        {
            // Cloak/uncloak ourself
            
            m_bCloaked = !m_bCloaked;
            break;
        }
        case EV_UP:
            // If we're at the top of the displayed list, shift the display
            if( m_dwCurrItem == m_dwTopItem )
            {
                if( m_dwTopItem > 0 )
                    --m_dwTopItem;
            }
            
            // Move to the previous item
            if( m_dwCurrItem > 0 )
                --m_dwCurrItem;
            
            break;
            
        case EV_DOWN:
            // If we're at the bottom of the displayed list, shift the display
            if( m_dwCurrItem == m_dwTopItem + MAX_FRIENDS_DISPLAYED - 1 )
            {
                if( m_dwTopItem + MAX_FRIENDS_DISPLAYED < g_FriendsManager.GetNumFriends( m_dwUserIndex ) )
                    ++m_dwTopItem;
            }
            
            // Move to next item
            if( m_dwCurrItem < g_FriendsManager.GetNumFriends( m_dwUserIndex ) - 1 )
                ++m_dwCurrItem;
            
            break;
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateStateActionMenu()
// Desc: Friend action menu navigation
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateActionMenu( Event ev )
{ 
    switch( ev )
    {
        case EV_BUTTON_WHITE:
            m_NextState = m_State;
            m_State     = STATE_HELP;
            break;

        case EV_BUTTON_A:
            switch( m_Actions[ m_dwCurrItem ] )
            {
                case ACTION_INVITE:
                    g_FriendsManager.SendGameInvite( m_dwUserIndex, SESSION_ID, 
                                                     m_dwCurrFriend );
                    swprintf( m_strError, L"Invitation sent" );
                    break;
                case ACTION_REVOKE:
                    g_FriendsManager.RevokeGameInvite( m_dwUserIndex, SESSION_ID,
                                                       m_dwCurrFriend );
                    swprintf( m_strError, L"Invitation revoked" );
                    break;
                case ACTION_GAME_INVITE_ACCEPT:
                    g_FriendsManager.AnswerGameInvite( m_dwUserIndex, m_dwCurrFriend,
                                                       XONLINE_GAMEINVITE_YES );            
                                
                    if( XOnlineTitleIdIsSameTitle( g_FriendsManager.GetFriend( m_dwUserIndex, m_dwCurrFriend )->dwTitleID ) )
                    {
                        swprintf( m_strError, L"You have accepted an invitation\n"
                                              L"from a friend to play this title" );
                    }
                    else
                    {
                        // Invitation from another title                          
                        m_State = STATE_JOINING_GAME;                
                        return;
                    }
                    break;
                case ACTION_GAME_INVITE_DECLINE:
                    g_FriendsManager.AnswerGameInvite( m_dwUserIndex, m_dwCurrFriend,
                                                       XONLINE_GAMEINVITE_NO );            
                    swprintf( m_strError, L"Declined invitation to game" );
                    break;
                case ACTION_GAME_INVITE_REMOVE:
                    g_FriendsManager.AnswerGameInvite( m_dwUserIndex, m_dwCurrFriend,
                                                       XONLINE_GAMEINVITE_REMOVE );
                    swprintf( m_strError, L"Friend removed" );
                    break;
                case ACTION_FRIEND_REQUEST_ACCEPT:
                    g_FriendsManager.AnswerFriendRequest( m_dwUserIndex, m_dwCurrFriend,
                                                          XONLINE_REQUEST_YES );
                    swprintf( m_strError, L"Friend request accepted" );
                    break;
                case ACTION_FRIEND_REQUEST_DECLINE:
                    g_FriendsManager.AnswerFriendRequest( m_dwUserIndex, m_dwCurrFriend,
                                                          XONLINE_REQUEST_NO );
                    swprintf( m_strError, L"Friend request declined" );
                    break;
                case ACTION_FRIEND_REQUEST_BLOCK:
                    g_FriendsManager.AnswerFriendRequest( m_dwUserIndex, m_dwCurrFriend,
                                                          XONLINE_REQUEST_BLOCK );
                    swprintf( m_strError, L"Friend request declined, and blocked" );
                    break;
                case ACTION_REMOVE:
                    m_State      = STATE_CONFIRM_REMOVE;
                    m_dwCurrItem = CONFIRM_REMOVE_NO;
                    return;
                case ACTION_JOIN_GAME:
                    // Check to see if title ID is the same
                    if( XOnlineTitleIdIsSameTitle( g_FriendsManager.GetFriend( m_dwUserIndex, m_dwCurrFriend )->dwTitleID ) )
                    {
                        swprintf( m_strError, L"You have joined a friend who is playing this title" );
                    }
                    else
                    {
                        // If not then prepare for a reboot to the new title
                        g_FriendsManager.JoinCrossTitleGame( m_dwUserIndex, m_dwCurrFriend );
                        m_State = STATE_JOINING_GAME;                
                        return;
                    }
                    break;
            }
            m_dwTopItem = m_dwCurrItem = 0;
            m_State     = STATE_ERROR;
            m_NextState = STATE_FRIEND_LIST;
            break;
        
        case EV_BUTTON_B:
        case EV_BUTTON_BACK:
            m_State      = STATE_FRIEND_LIST;
            m_dwTopItem  = 0;
            m_dwCurrItem = 0;
            break;
            
        case EV_UP:
            if( m_dwCurrItem == 0 )
                m_dwCurrItem = m_Actions.size() - 1;
            else
                --m_dwCurrItem;
            break;
            
        case EV_DOWN:
            if( m_dwCurrItem == m_Actions.size() - 1 )
                m_dwCurrItem = 0;
            else
                ++m_dwCurrItem;
            break;
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateStateJoiningGame()
// Desc: Cleanup and prompt for disc insertion
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateJoiningGame( Event ev )
{    
    if( ev == EV_BUTTON_B )
    {
        m_dwTopItem = m_dwCurrItem = 0;
        m_State = STATE_FRIEND_LIST;
        return;
    }

    // In a real game, when they eject and insert the new disc,
    // we will automatically reboot to the new title, and it should detect
    // from XOnlineFriendsAnswerGameInvite or FriendsManager::JoinCrossTitleGame 
    // the new game to join using XOnlineFriendsGetAcceptedGameInvite
}



    
//-----------------------------------------------------------------------------
// Name: UpdateStateNewFriend()
// Desc: Add new friend
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateNewFriend( Event ev )
{
    HRESULT hr;
    // Handle the special case of an empty list
    if( 0 == m_dwNumPotentialFriends )
    {
        switch( ev )
        {
            case EV_BUTTON_A:
            case EV_BUTTON_B:
            case EV_BUTTON_BACK:
                m_State = STATE_FRIEND_LIST;
                m_dwTopItem = m_dwCurrItem = 0;
                break;
        }
        return;
    }
    
    switch( ev )
    {
        case EV_BUTTON_A:
            hr = g_FriendsManager.AddPlayerToFriendsList( m_dwUserIndex,
                                                          m_PotentialFriendList[ m_dwCurrItem ].xuid );        
            if( hr == XONLINE_E_NOTIFICATION_LIST_FULL )
                swprintf( m_strError, L"Friends list full" );
            else
                swprintf( m_strError, L"Friend request issued" );
            m_dwTopItem  = 0;
            m_dwCurrItem = 0;
            m_State      = STATE_ERROR;
            m_NextState  = STATE_NEW_FRIEND;
            break;
            
        case EV_BUTTON_B:
        case EV_BUTTON_BACK:
            m_State      = STATE_FRIEND_LIST;
            m_dwTopItem  = 0;
            m_dwCurrItem = 0;
            break;
            
        case EV_BUTTON_WHITE:
            m_NextState = m_State;
            m_State     = STATE_HELP;
            break;

        case EV_UP:
            // If we're at the top of the displayed list, shift the display
            if( m_dwCurrItem == m_dwTopItem )
            {
                if( m_dwTopItem > 0 )
                    --m_dwTopItem;
            }
            
            // Move to the previous item
            if( m_dwCurrItem > 0 )
                --m_dwCurrItem;

            if ( m_dwCurrItem < m_dwTopItem ) m_dwTopItem = m_dwCurrItem;
            if ( m_dwCurrItem >= ( m_dwTopItem + MAX_ACCOUNTS_DISPLAYED ) )
                m_dwTopItem = m_dwCurrItem - MAX_ACCOUNTS_DISPLAYED + 1;

            
            break;
            
        case EV_DOWN:
            
            // If we're at the bottom of the displayed list, shift the display
            if( m_dwCurrItem == m_dwTopItem + MAX_FRIENDS_DISPLAYED - 1 )
            {
                if( m_dwTopItem + MAX_FRIENDS_DISPLAYED < m_dwNumPotentialFriends )
                    ++m_dwTopItem;
            }
            
            // Move to next item
            if( m_dwCurrItem < m_dwNumPotentialFriends - 1 )
                ++m_dwCurrItem;

            if ( m_dwCurrItem < m_dwTopItem ) m_dwTopItem = m_dwCurrItem;
            if ( m_dwCurrItem >= ( m_dwTopItem + MAX_ACCOUNTS_DISPLAYED ) )
                m_dwTopItem = m_dwCurrItem - MAX_ACCOUNTS_DISPLAYED + 1;


            break;
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateStateConfirmRemove()
// Desc: Confirmation dialog for friend removal
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateConfirmRemove( Event ev )
{
    switch( ev )
    {
        case EV_BUTTON_A:
            if( m_dwCurrItem == CONFIRM_REMOVE_YES )
            {
                g_FriendsManager.RemoveFriendFromFriendsList( m_dwUserIndex, m_dwCurrFriend );
                swprintf( m_strError, L"Friend removed" );
                m_dwTopItem = m_dwCurrItem = 0;
                m_State = STATE_ERROR;
                m_NextState = STATE_FRIEND_LIST;
            }
            else // CONFIRM_REMOVE_NO
            {
                m_State = STATE_ACTION_MENU;
                m_dwTopItem = m_dwCurrItem = 0;
            }
            break;
            
        case EV_BUTTON_B:
        case EV_BUTTON_BACK:
            m_State = STATE_ACTION_MENU;
            m_dwTopItem = m_dwCurrItem = 0;
            break;
            
        case EV_UP:
            if( m_dwCurrItem == 0 )
                m_dwCurrItem = CONFIRM_REMOVE_MAX - 1;
            else
                --m_dwCurrItem;
            break;
            
        case EV_DOWN:
            if( m_dwCurrItem == CONFIRM_REMOVE_MAX - 1 )
                m_dwCurrItem = 0;
            else
                ++m_dwCurrItem;
            break;

        case EV_BUTTON_WHITE:
            m_NextState = m_State;
            m_State = STATE_HELP;
            break;
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
    
    m_UI.SetUserIndex( m_dwUserIndex );

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
        swprintf( m_strError, L"Login failed to start. Error 0x%x", hr );
        m_State = STATE_ERROR;
        m_dwTopItem = 0;
        m_dwCurrItem = 0;           
        m_NextState = STATE_SELECT_ACCOUNT;
    }
}




//-----------------------------------------------------------------------------
// Name: InitFriends()
// Desc: Initiate friends handling
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::InitFriends()
{
    HRESULT hr = g_FriendsManager.Initialize();
       
    g_FriendsManager.StartUpdatingFriends( m_dwUserIndex );

    // Build the list of potential friends. The "potential" friend list is
    // contrived for this sample. A real game would typically allow the
    // player to select anybody they happened to be playing with and request
    // that person be their friend. In other words, your game should not
    // be doing this!
    //
    // The list of potential friends starts out as the list of all users
    // known by this particular Xbox.
    memcpy( m_PotentialFriendList, m_UserList, sizeof(m_UserList) );
    m_dwNumPotentialFriends = m_dwNumUsers;

    return hr;
}




//-----------------------------------------------------------------------------
// Name: SetPlayerState()
// Desc: Broadcast current player state for the world
//-----------------------------------------------------------------------------
VOID CXBoxSample::SetPlayerState( DWORD dwState )
{
    // check if our state is the same as our old state
    if( dwState == m_dwOldState ) 
        return;
    
    m_dwOldState = dwState;

    HRESULT hr = XOnlineNotificationSetState( m_dwUserIndex, dwState,
        SESSION_ID, 0, NULL );
    assert( SUCCEEDED( hr ) );
    (VOID)hr; // avoid compiler warning
}




//-----------------------------------------------------------------------------
// Name: UpdatePotentialFriends()
// Desc: Update the potential friends list
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdatePotentialFriends()
{  
    // Cull the "potential" friend list accordingly
    memcpy( m_PotentialFriendList, m_UserList, sizeof(m_UserList) );
    m_dwNumPotentialFriends = m_dwNumUsers;
    
    // Remove any match between the two lists from the potential
    // friend list, where a match is a matching username
    
    BOOL bRemoved = TRUE;

    while( bRemoved )
    {
        bRemoved = FALSE;
        for( DWORD j = 0; j < m_dwNumPotentialFriends; j++ )
        {
            if( ( g_FriendsManager.FindPlayerInFriendsList( m_dwUserIndex, m_PotentialFriendList[ j ].xuid ) != NULL ) ||
                ( XOnlineAreUsersIdentical( &m_PotentialFriendList[ j ].xuid, &m_UserList[ m_dwCurrUser ].xuid ) ) )
            {
                // Erase this friend
                memcpy( &m_PotentialFriendList[j], &m_PotentialFriendList[m_dwNumPotentialFriends-1], sizeof(XONLINE_USER) );
                m_dwNumPotentialFriends--;

                bRemoved = TRUE;
                break;
            }
        }
    }         
    
    SetStatus( L"Friends list refreshed" );   
}




//-----------------------------------------------------------------------------
// Name: SetStatus()
// Desc: Set the status text
//-----------------------------------------------------------------------------
VOID CXBoxSample::SetStatus( const WCHAR* strStatus )
{
    lstrcpynW( m_strStatus, strStatus, MAX_STATUS_STR );
    m_StatusTimer.StartZero();
}




//-----------------------------------------------------------------------------
// Name: ConfigureActionMenu()
// Desc: Set the menu items visible in the action menu
//-----------------------------------------------------------------------------
VOID CXBoxSample::ConfigureActionMenu()
{
    assert( m_dwCurrFriend < g_FriendsManager.GetNumFriends( m_dwUserIndex ) );
    XONLINE_FRIEND* pFriend = g_FriendsManager.GetFriend( m_dwUserIndex, m_dwCurrFriend );

     // Player has sent you a request to be a friend
    BOOL bFriendRequest  = ( pFriend->dwFriendState &XONLINE_FRIENDSTATE_FLAG_RECEIVEDREQUEST );

    // You sent this player a  request to be a friend
    BOOL bSentRequest    = ( pFriend->dwFriendState & XONLINE_FRIENDSTATE_FLAG_SENTREQUEST );

    // Invitation from a friend
    BOOL bRecvGameInvite = ( pFriend->dwFriendState & XONLINE_FRIENDSTATE_FLAG_RECEIVEDINVITE );

    // Game invitation sent to a friend
    BOOL bSentGameInvite     = ( pFriend->dwFriendState & XONLINE_FRIENDSTATE_FLAG_SENTINVITE );
    BOOL bGameInviteAccepted = ( pFriend->dwFriendState & XONLINE_FRIENDSTATE_FLAG_INVITEACCEPTED );
    BOOL bGameInviteDeclined = ( pFriend->dwFriendState & XONLINE_FRIENDSTATE_FLAG_INVITEREJECTED );

    // Friend is in a joinable session
    BOOL bGameJoinable = ( pFriend->dwFriendState & XONLINE_FRIENDSTATE_FLAG_JOINABLE );

    m_Actions.clear();
    
    if( !bSentRequest && !bFriendRequest &&
        !bRecvGameInvite && (!bSentGameInvite || bGameInviteDeclined ) )
        m_Actions.push_back( ACTION_INVITE );

    if( bSentGameInvite && ! ( bGameInviteAccepted || bGameInviteDeclined ) )
        m_Actions.push_back( ACTION_REVOKE );

    if( bRecvGameInvite )
    {
        m_Actions.push_back( ACTION_GAME_INVITE_ACCEPT );
        m_Actions.push_back( ACTION_GAME_INVITE_DECLINE );
        m_Actions.push_back( ACTION_GAME_INVITE_REMOVE );
    }

    if( bFriendRequest )
    {
        m_Actions.push_back( ACTION_FRIEND_REQUEST_ACCEPT );
        m_Actions.push_back( ACTION_FRIEND_REQUEST_DECLINE );
        m_Actions.push_back( ACTION_FRIEND_REQUEST_BLOCK );
    }

    if( bGameJoinable )
    {
        m_Actions.push_back( ACTION_JOIN_GAME );
    }

    m_Actions.push_back( ACTION_REMOVE );
}




//-----------------------------------------------------------------------------
// Name: BootToDash()
// Desc: Boot to the dash
//-----------------------------------------------------------------------------
VOID CXBoxSample::BootToDash()
{
    // Return to Dashboard. Retail Dashboard will include
    // online account creation. Development XDK Launcher
    // includes the XDK Launcher or Xbox OnlineDash for creating accounts.
    LD_LAUNCH_DASHBOARD ld;
    ZeroMemory( &ld, sizeof(ld) );
    ld.dwReason = XLD_LAUNCH_DASHBOARD_MAIN_MENU;
    XLaunchNewImage( NULL, PLAUNCH_DATA( &ld ) );
}




//-----------------------------------------------------------------------------
// Name: Reset()
// Desc: Prepare to restart the application at the front menu
//-----------------------------------------------------------------------------
VOID CXBoxSample::Reset()
{
    g_FriendsManager.Shutdown();
    m_hOnlineTask.Close();                 

    m_State = STATE_ERROR;
    if( 0 == m_dwNumUsers )
        m_NextState = STATE_CREATE_ACCOUNT;
    else
        m_NextState = STATE_SELECT_ACCOUNT;

    m_bIsLoggedOn = FALSE;
    m_dwCurrItem  = 0;
    m_dwTopItem   = 0;
    m_dwOldState  = 0;
    m_bGameInvitePending = FALSE;
    m_bCloaked    = FALSE;
}
