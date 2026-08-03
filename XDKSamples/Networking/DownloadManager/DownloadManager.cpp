//-----------------------------------------------------------------------------
// File: DownloadManager.cpp
//
// Desc: Shows how to use the Xbox downloader as as 
//       Xbox online authentication protocols.
//       Includes account creation, PIN entry, validation and logon.
//
// Hist: 10.16.02 - Created for November Release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "DownloadManager.h"
#include "XBMemUnit.h"
#include "XBVoice.h"
#include <cassert>




//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_WHITE_BUTTON, XBHELP_PLACEMENT_1, L"Display\nhelp" },
    { XBHELP_A_BUTTON,     XBHELP_PLACEMENT_1, L"Select menu\nitem" },
    { XBHELP_B_BUTTON,     XBHELP_PLACEMENT_1, L"Cancel" },
    { XBHELP_DPAD,         XBHELP_PLACEMENT_1, L"Menu navigation" },
};

#define NUM_HELP_CALLOUTS 4




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
    // Add whatever services are appropriate for your title, but no
    // more. Each service requires additional authentication time
    // and network traffic.
    m_pServices[0] = XONLINE_MATCHMAKING_SERVICE;
    m_pServices[1] = XONLINE_BILLING_OFFERING_SERVICE;

    Reset();
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize device-dependent objects
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
    if( FAILED( XBOnline_GetUserList( m_UserAccountList ) ) )
        return E_FAIL;

    CXBMemUnit::GetMemUnitSnapshot();     
   
    // If no accounts, then user needs to create an account.
    // For development purposes, accounts are created using the
    // Online Dashboard or the XDK Launcher
    if( m_UserAccountList.empty() )
        m_State = STATE_CREATE_ACCOUNT;
    else
        m_State = STATE_USER_EVENTS;

    // Check if a login state was passed as launch data
    DWORD dwLaunchType;
    // Downloader.xbe always expects an LD_DOWNLOADER struct to be passed
    // to it. Since downloader always passes back what it was passed, that
    // means that we receive an LD_DOWNLOADER struct back.
    LD_DOWNLOADER LaunchData;

    // See if we have indirectly passed ourselves some launch data. We have to
    // check launch type to distinguish between our data and that from the
    // dashboard or debugger.
    if( XGetLaunchInfo( &dwLaunchType, (PLAUNCH_DATA)&LaunchData ) == ERROR_SUCCESS && dwLaunchType == LDT_TITLE )
    {
        // Double check to make sure the data is coming from us.
        if ( LaunchData.dwID == LAUNCH_DATA_DOWNLOADER_ID )
        {
            // Restore the logon state.
            RestoreUsersFromLogonState( &LaunchData.LogonState );

            // If we had stored extra data in LaunchData.UserDefined we
            // could grab that now.
        }
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: RestoreUsersFromLogonState()
// Desc: Extract users from a save logon state and assign to controllers for
//       in preparation for authentication.
//-----------------------------------------------------------------------------
VOID CXBoxSample::RestoreUsersFromLogonState( XONLINE_LOGON_STATE *pLogonState )
{
    if( pLogonState->bType  == XONLINE_LOGON_STATE_TYPE &&
        pLogonState->cbSize == sizeof( XONLINE_LOGON_STATE ) )
    {
        DWORD rgServices[ XONLINE_MAX_LOGON_STATE_SERVICES ];
        DWORD dwNumServices = XONLINE_MAX_LOGON_STATE_SERVICES;
        XONLINE_USER LogonStateUsers[XONLINE_MAX_LOGON_USERS];

        XOnlineRetrieveLogonState( pLogonState, LogonStateUsers,
                                   rgServices, &dwNumServices );

        XBInput_RefreshDeviceList();
        DWORD dwDevices          = XGetDevices( XDEVICE_TYPE_GAMEPAD );
        DWORD dwNumUsersSaved    = 0; 
        DWORD dwNumUsersRestored = 0; 

        for( DWORD i = 0; i < XONLINE_MAX_LOGON_USERS; ++i )
        {
            // For each controller check if there was a user
            if( LogonStateUsers[i].xuid.qwUserID != 0 )
            {
                dwNumUsersSaved++;

                // Make sure the corresponding controller is plugged in
                if( dwDevices & ( 1 << i ) )
                {
                    // Check to see if the corresponding user account
                    // is still present (e.g. the MU with the account
                    // is still inserted) 
                    DWORD j;
                    for( j = 0; j < m_UserAccountList.size(); ++j )
                    {
                        // Compare users, using the UserID, since
                        // guest bits can may be set
                        if( LogonStateUsers[i].xuid.qwUserID == 
                            m_UserAccountList[j].xuid.qwUserID )
                        {
                            // Make WCHAR copy of User name
                            XBUtil_GetWide( 
                                m_UserAccountList[j].szGamertag, 
                                m_Users[i].strName, XONLINE_GAMERTAG_SIZE );
                            m_Users[i].bGuest = XOnlineIsUserGuest(
                                LogonStateUsers[i].xuid.dwUserFlags);
                            if( m_Users[i].bGuest )
                                wcscat( m_Users[i].strName, L"\n(guest)" );
                            dwNumUsersRestored++;
                            m_Users[i].Account = m_UserAccountList[j];
                            m_Users[i].State = STATE_USER_WAIT_FOR_OTHERS;
                            break;
                        }
                    }
                    if( j == m_UserAccountList.size() )
                    {
                        if( XOnlineIsUserGuest( LogonStateUsers[i].xuid.dwUserFlags) )
                        {
                            XBUtil_DebugPrint( "\n*** DownloadManager: Controller %d: "
                                               "guest of %s not restored (no account present)\n\n",
                                               i, LogonStateUsers[i].szGamertag );
                        }
                        else
                        {
                            XBUtil_DebugPrint( "\n*** DownloadManager: Controller %d: "
                                               "%s not restored (no account present)\n\n",
                                               i, LogonStateUsers[i].szGamertag );
                        }
                    }
                    else
                    {
                        if( XOnlineIsUserGuest( LogonStateUsers[i].xuid.dwUserFlags) )
                        {
                            XBUtil_DebugPrint( "\n*** DownloadManager: Controller %d: "
                                               "Controller %d: guest of %s restored\n\n",
                                               i, LogonStateUsers[i].szGamertag );
                        }
                        else
                        {
                            XBUtil_DebugPrint( "\n*** DownloadManager: Controller %d: "
                                               "Controller %d: %s restored\n\n",
                                               i, LogonStateUsers[i].szGamertag );
                        }
                    }
                }
            }
        }

        // If all users were restored just sign on automatically
        if( dwNumUsersRestored == dwNumUsersSaved )
        {
            m_bReadyForSignOn = TRUE;
            m_State = STATE_SIGNING_ON;
            BeginSignOn();
        }
    }
}




//-----------------------------------------------------------------------------
// Name: Reset()
// Desc: Prepare to restart the application at the front menu
//-----------------------------------------------------------------------------
VOID CXBoxSample::Reset()
{
    if( m_UserAccountList.empty() )
        m_State = STATE_CREATE_ACCOUNT;
    else
        m_State = STATE_USER_EVENTS;
    m_NextState        = m_State;
    m_bAllowBootToDash = FALSE;
    m_dwLaunchReason   = XLD_LAUNCH_DASHBOARD_MAIN_MENU;

    m_bReadyForSignOn  = FALSE;
    m_bSignedOn        = FALSE;
    m_bShowHelp        = FALSE;
    m_dwMicrophoneState = 0;
    m_dwHeadphoneState  = 0;
    ZeroMemory( &m_Users, sizeof( m_Users ) );
    ZeroMemory( &m_LogonUsers, sizeof( m_LogonUsers ) );

    for( DWORD i = 0; i < XONLINE_MAX_LOGON_USERS; ++i )
    {
        ResetUser( i ); 
    }
}




//-----------------------------------------------------------------------------
// Name: ResetUser()
// Desc: Reset the state for a user back to pre-signon
//-----------------------------------------------------------------------------
VOID CXBoxSample::ResetUser( DWORD dwUserIndex )
{
    ZeroMemory( &m_Users[dwUserIndex], sizeof( m_Users[dwUserIndex] ) );
    m_Users[dwUserIndex].State          = STATE_USER_PRE_SIGN_ON;
    m_Users[dwUserIndex].NextState      = STATE_USER_PRE_SIGN_ON;
    m_Users[dwUserIndex].dwLaunchReason = XLD_LAUNCH_DASHBOARD_MAIN_MENU; 
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
        m_State = STATE_ERROR;
    }
    
    if( m_bSignedOn )
    {
        HRESULT hr = m_hOnlineTask.Continue();
        
        if( FAILED( hr ) )
        {
            Reset();
            if( hr == XONLINE_E_LOGON_KICKED_BY_DUPLICATE_LOGON )
                m_UI.SetErrorStr( L"You have been signed out because a\n"
                L"duplicate account signed in on another Xbox" );
            else
                m_UI.SetErrorStr( L"Connection was lost. Must sign in again." );
            m_State = STATE_ERROR;
        }
        else
            // Check for microphone/headphone peripheral state changes
            CheckDeviceStates();
    }


    // Process master state machine events
    Event ev = GetEvent(); 

    // Check for the help button
    if( ev == EV_BUTTON_WHITE && m_State != STATE_SIGNING_ON && !IsUserInPinEntry() )
        m_bShowHelp = !m_bShowHelp;
    
    if( m_bShowHelp )
        return S_OK;

    switch( m_State )
    {
        case STATE_CREATE_ACCOUNT:  UpdateStateCreateAccount( ev ); break;
        case STATE_USER_EVENTS:
            // Process user events for each user as long as the master state
            // machine remains in STATE_USER_EVENTS
            for( DWORD i = 0; i < XONLINE_MAX_LOGON_USERS && m_State == STATE_USER_EVENTS; ++i )
            {
                // Get an event for a specific controller
                ev = GetEvent( i ); 

                switch( m_Users[i].State )
                {
                    case STATE_USER_PRE_SIGN_ON:
                        UpdateUserStatePreSignOn( m_Users[i], ev );
                        break;
                    case STATE_USER_SELECT_ACCOUNT:
                        UpdateUserStateSelectAccount( m_Users[i], ev );
                        break;
                    case STATE_USER_CONFIRM_SPONSOR:
                        UpdateUserStateConfirmSponsor( m_Users[i], ev );
                        break;
                    case STATE_USER_PIN_ENTRY:
                        UpdateUserStatePINEntry( m_Users[i], ev );
                        break;
                    case STATE_USER_WAIT_FOR_OTHERS:
                        UpdateUserStateWaitForOthers( m_Users[i], ev );
                        break;
                    case STATE_USER_ERROR:
                        UpdateUserStateError( m_Users[i], ev );
                        break;
                    case STATE_USER_BOOT_TO_DASH:
                        BootToDash( m_Users[i].dwLaunchReason );
                        break;
                    case STATE_USER_DONE:
                        UpdateUserStateDone( m_Users[i], ev );
                        break;
                    default:
                        assert( FALSE );
                }            
            }
            break;
        case STATE_SIGNING_ON:      UpdateStateSigningOn( ev );     break;
        case STATE_MAIN_MENU:       UpdateStateMainMenu( ev );      break;
        case STATE_ERROR:           UpdateStateError( ev );         break;
        default:
            assert( FALSE );
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
    DWORD i;
    
    // Clear the viewport
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET, 0x000A0A6A, 1.0f, 0L );
    
    if( m_bShowHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        // Draw the app title
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 48, 36, COLOR_NORMAL, L"DownloadManager" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );

        switch( m_State )
        {
            case STATE_CREATE_ACCOUNT:
                m_UI.RenderCreateAccount( TRUE );
                break;
            case STATE_USER_EVENTS:
                for( i = 0; i < XONLINE_MAX_LOGON_USERS; ++i )
                {
                    switch( m_Users[i].State )
                    {
                        case STATE_USER_PRE_SIGN_ON:
                            m_UI.RenderUserPreSignOn( i);
                            break;
                        case STATE_USER_SELECT_ACCOUNT:
                            m_UI.RenderUserSelectAccount( i, m_Users[i].dwCurrItem,
                                                          m_Users[i].dwTopItem, m_UserAccountList );
                            break;
                        case STATE_USER_CONFIRM_SPONSOR:
                            m_UI.RenderConfirmSponsor( i, m_UserAccountList[m_Users[i].dwCurrItem] );
                            break;
                            
                        case STATE_USER_PIN_ENTRY:
                            m_UI.RenderUserPINEntry( i, m_Users[i].dwCurrItem );
                            break;
                        case STATE_USER_WAIT_FOR_OTHERS:
                            m_UI.RenderUserWaitForOthers( i, m_Users[i].strName, 
                                                          m_bReadyForSignOn );
                            break;
                        case STATE_USER_ERROR:
                            m_UI.RenderUserError( i, m_Users[i].strError,
                                                  m_Users[i].bAllowBootToDash );
                            break;
                        case STATE_USER_DONE:
                            m_UI.RenderUserDone( i,  m_Users[i].strName,
                                                 m_Users[i].bSignedOn, m_Users[i].bVoice );
                            break;
                        case STATE_USER_BOOT_TO_DASH:
                            break;
                        default:
                            assert( FALSE );
                    }
                }
                break;
            case STATE_SIGNING_ON:
                m_UI.RenderSigningOn( m_LogonUsers );
                break;
            case STATE_MAIN_MENU:
                m_UI.RenderMainMenu();
                break;
            case STATE_ERROR:
                m_UI.RenderError( m_bAllowBootToDash );
                break;
            default:
                assert( FALSE );
                break;
        }
       
        // Render the help callout
        if( m_State != STATE_SIGNING_ON && !IsUserInPinEntry() )
            m_Font.DrawText( 360, 420, COLOR_NORMAL, GLYPH_WHITE_BUTTON L"Help" );    
    }
    
    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );
    
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: GetGamepadEvent()
// Desc: Return an event from a specific XBGAMEPAD
//-----------------------------------------------------------------------------
CXBoxSample::Event CXBoxSample::GetGamepadEvent( const XBGAMEPAD& Gamepad ) const
{
    // "A" or "Start"
    if( Gamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_A] ||
        Gamepad.wPressedButtons & XINPUT_GAMEPAD_START )
    {
        return EV_BUTTON_A;
    }
    
    // "B"
    if( Gamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_B] )
        return EV_BUTTON_B;
    
    // "X"
    if( Gamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_X] )
        return EV_BUTTON_X;
    
    // "Y"
    if( Gamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_Y] )
        return EV_BUTTON_Y;

    // "Back"
    if( Gamepad.wPressedButtons & XINPUT_GAMEPAD_BACK )
        return EV_BUTTON_BACK;

    // "White"
    if( Gamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_WHITE] )
        return EV_BUTTON_WHITE;

    // "Black"
    if( Gamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_BLACK] )
        return EV_BUTTON_BLACK;
    
    // "Left Trigger"
    if( Gamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_LEFT_TRIGGER] )
        return EV_LEFT_TRIGGER;

    // "Right Trigger"
    if( Gamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER] )
        return EV_RIGHT_TRIGGER;

    // Movement
    if( Gamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
        return EV_UP;
    if( Gamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
        return EV_DOWN;
    if( Gamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
        return EV_LEFT;
    if( Gamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
        return EV_RIGHT;
    
    return EV_NULL;
}




//-----------------------------------------------------------------------------
// Name: GetEvent()
// Desc: Return the state of ANY of the controllers
//-----------------------------------------------------------------------------
CXBoxSample::Event CXBoxSample::GetEvent() const
{
    // Pass the default gamepad, which combines events of all available
    // controllers
    return GetGamepadEvent( m_DefaultGamepad );
}




//-----------------------------------------------------------------------------
// Name: GetEvent()
// Desc: Return the state of a specific controller
//-----------------------------------------------------------------------------
CXBoxSample::Event CXBoxSample::GetEvent( DWORD dwUserIndex ) const
{
    if( m_Gamepad[dwUserIndex].hDevice )
        return GetGamepadEvent( m_Gamepad[dwUserIndex] );
    else    
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
// Name: BeginUserPINEntry()
// Desc: Start PIN Entry for a User
//-----------------------------------------------------------------------------
VOID CXBoxSample::BeginUserPINEntry( CUser& User )
{
    assert( User.Account.dwUserOptions & XONLINE_USER_OPTION_REQUIRE_PASSCODE );

    User.dwCurrItem = 0;
    ZeroMemory( User.Passcode, sizeof( User.Passcode ) );
    User.State = STATE_USER_PIN_ENTRY;
}




//-----------------------------------------------------------------------------
// Name: UpdateUserStatePINEntry()
// Desc: PIN Entry
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateUserStatePINEntry( CUser& User, Event ev )
{
    assert( User.Account.dwUserOptions & XONLINE_USER_OPTION_REQUIRE_PASSCODE );

    BYTE Key = 0;

    switch( ev )
    {
        case EV_BUTTON_A:
            // Check PIN
            User.dwCurrItem = 0;
            if( memcmp( User.Account.passcode, 
                        User.Passcode, 
                        XONLINE_PASSCODE_LENGTH ) == 0 )
            {
                User.State = STATE_USER_WAIT_FOR_OTHERS;
            }
            else
            {
                SetUserErrorStr( User, L"Invalid Passcode" );
                User.State = STATE_USER_ERROR;
                User.NextState = STATE_USER_SELECT_ACCOUNT;
            }
            return;
        case EV_BUTTON_BACK:
        case EV_BUTTON_B:
            BeginSelectAccount( User );
            return;
        case EV_BUTTON_X:
            Key = XONLINE_PASSCODE_GAMEPAD_X;
            break;
        case EV_BUTTON_Y:
            Key = XONLINE_PASSCODE_GAMEPAD_Y;
            break;
        case EV_UP:
            Key = XONLINE_PASSCODE_DPAD_UP;
            break;
        case EV_DOWN:
            Key = XONLINE_PASSCODE_DPAD_DOWN;
            break;
        case EV_LEFT:
            Key = XONLINE_PASSCODE_DPAD_LEFT;
            break;
        case EV_RIGHT:
            Key = XONLINE_PASSCODE_DPAD_RIGHT;
            break;
        case EV_LEFT_TRIGGER:
            Key = XONLINE_PASSCODE_GAMEPAD_LEFT_TRIGGER;
            break;
        case EV_RIGHT_TRIGGER:
            Key = XONLINE_PASSCODE_GAMEPAD_RIGHT_TRIGGER;
            break;

        default:
            return;
    }

    assert( Key != 0 );

    if( Key != 0 && User.dwCurrItem < XONLINE_PASSCODE_LENGTH )
    {
        User.Passcode[User.dwCurrItem++] = Key;
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateStateCreateAccount()
// Desc: Allow User to launch account creation tool
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateCreateAccount( Event ev )
{
    switch( ev )
    {
        case EV_BUTTON_A:
            BootToDash( XLD_LAUNCH_DASHBOARD_NEW_ACCOUNT_SIGNUP );
            break;
        default:
            // If any MUs are inserted, update the User list
            // and resume if there are any accounts
            DWORD dwInsertions;
            DWORD dwRemovals;
            if( CXBMemUnit::GetMemUnitChanges( dwInsertions, dwRemovals ) )
            {
                m_UserAccountList.clear();
                XBOnline_GetUserList( m_UserAccountList );
                if( !m_UserAccountList.empty() )
                {
                    m_State = STATE_USER_EVENTS;
                }
            }
            break;
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateUserStatePreSignOn()
// Desc: Allow User to select controller for sign on
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateUserStatePreSignOn( CUser& User, Event ev )
{
    switch( ev )
    {
        case EV_BUTTON_A:
            BeginSelectAccount( User );
            break;
        default:
            break;
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateStateMainMenu()
// Desc: Main menu selection
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateMainMenu( Event ev )
{
    switch( ev )
    {
        case EV_BUTTON_A:
        {
            // Let's see if A was pressed by a user that was logged in. If not
            // then we ignore the press. We also ignore presses from guests.
            // That's because only logged on non-guests can use downloader.xbe
            BYTE DesiredUserIndex = XONLINE_MAX_LOGON_USERS;
            for( BYTE i = 0; i < XONLINE_MAX_LOGON_USERS ; ++i )
            {
                // Get an event for a specific controller
                if ( m_Users[i].bSignedOn && !m_Users[i].bGuest && GetEvent( i ) == EV_BUTTON_A )
                {
                    DesiredUserIndex = i;
                }
            }
            // If a logged off user pressed A, then ignore it.
            if ( DesiredUserIndex == XONLINE_MAX_LOGON_USERS )
                return;

            // Declare and zero a LD_DOWNLOADER structure
            LD_DOWNLOADER LaunchData = { 0 };

            // dwID must be set to LAUNCH_DATA_DOWNLOADER_ID
            LaunchData.dwID = LAUNCH_DATA_DOWNLOADER_ID;

            // Set the filter to specify what types of content should be allowed.
            // This says we will allow all types. These filters can be used to filter
            // content for a given region, or for other purposes.
            LaunchData.dwBitFilter = 0xFFFFFFFF;

            // Get the current logon state.
            XONLINE_LOGON_STATE LogonState;
            HRESULT hr = XOnlineSaveLogonState(&LogonState);
            if (SUCCEEDED(hr))
            {
                // Copy the logon state into the launch data, if we were successful.
                memcpy(&(LaunchData.LogonState), &LogonState, sizeof(XONLINE_LOGON_STATE));

                // Specify which user requested the downloadable content. This user will
                // be automatically signed in, and will be the only user that can control
                // the downloader. This user must be logged in and must not be a guest.

                LaunchData.bPremiumLogonPort = DesiredUserIndex;

                // Specify any game state into the user defined area.  This can be
                // as big as sizeof(LaunchData.UserDefined)
                // Not used in this sample.

                // memcpy(&(LaunchData.UserDefined), &GameState, sizeof(GameState));

                // Launch Downloader
                XLaunchNewImage("d:\\downloader.xbe", (LAUNCH_DATA*)&LaunchData);

                assert( FALSE ); // Should never return
            }
            return;
        }
        case EV_BUTTON_BACK:
        case EV_BUTTON_B:
            Reset();
            m_hOnlineTask.Close();
            return;
    }   
}




//-----------------------------------------------------------------------------
// Name: HostAccountSelected()
// Desc: Check if one of the players has selected a host (non-guest) account
//       for signon
//-----------------------------------------------------------------------------
BOOL CXBoxSample::HostAccountSelected( XUID& xuid )
{
    for( DWORD i = 0; i < XONLINE_MAX_LOGON_USERS; ++i )
       if( m_Users[i].Account.xuid.qwUserID == xuid.qwUserID && !m_Users[i].bGuest )
           return TRUE;

    return FALSE;
}




//-----------------------------------------------------------------------------
// Name: UpdateUserStateSelectAccount()
// Desc: Allow User to choose account
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateUserStateSelectAccount( CUser& User, Event ev )
{
    switch( ev )
    {
        case EV_BUTTON_A:
        {
            // Check if this account has already been selected 
            if( HostAccountSelected( m_UserAccountList[User.dwCurrItem].xuid ) )
            {
                // The account has been selected.  Check if the
                // user would like to sign on as a guest of this
                // account
                User.State = STATE_USER_CONFIRM_SPONSOR;
            }
            else
            {
                // Save current account information
                User.Account = m_UserAccountList[User.dwCurrItem];
                
                // Make WCHAR copy of User name
                XBUtil_GetWide( m_UserAccountList[User.dwCurrItem].szGamertag, 
                                User.strName, XONLINE_GAMERTAG_SIZE );
                // Check if a passcode is required.  Note that
                // passcodes are strictly for *client* side
                // authentication
                if( m_UserAccountList[User.dwCurrItem].dwUserOptions & 
                    XONLINE_USER_OPTION_REQUIRE_PASSCODE )
                {
                    User.State = STATE_USER_PIN_ENTRY;
                    BeginUserPINEntry( User );
                }
                else
                    User.State = STATE_USER_WAIT_FOR_OTHERS;
            }
            break;
        }
        case EV_BUTTON_B:
        case EV_BUTTON_BACK:
            User.State = STATE_USER_PRE_SIGN_ON;
            break;
       
        case EV_UP:
            // If we're at the top of the displayed list, shift the display
            if( User.dwCurrItem == User.dwTopItem )
            {
                if( User.dwTopItem > 0 )
                    --User.dwTopItem;
            }
            
            // Move to the previous item
            if( User.dwCurrItem > 0 )
                --User.dwCurrItem;
            break;
        
        case EV_DOWN:
            // If we're at the bottom of the displayed list, shift the display
            if( User.dwCurrItem == User.dwTopItem + MAX_ACCOUNTS_DISPLAYED - 1 )
            {
                if( User.dwTopItem + MAX_ACCOUNTS_DISPLAYED < m_UserAccountList.size() )
                    ++User.dwTopItem;
            }
            
            // Move to next item
            if( User.dwCurrItem < m_UserAccountList.size() - 1 )
                ++User.dwCurrItem;

        default:
            // If any MUs are inserted/removed, need to update the
            // User account list
            DWORD dwInsertions;
            DWORD dwRemovals;
            if( CXBMemUnit::GetMemUnitChanges( dwInsertions, dwRemovals ) )
            {
                m_UserAccountList.clear();
                XBOnline_GetUserList( m_UserAccountList );
                User.dwCurrItem = 0;
                User.dwTopItem  = 0;
                if( m_UserAccountList.empty() )
                    m_State = STATE_CREATE_ACCOUNT;
            }
            break;
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateUserStateConfirmSponsor()
// Desc: Allow User to confirm guest account selection
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateUserStateConfirmSponsor( CUser& User, Event ev )
{
    // Check to make sure the sponsor account is still
    // selected...
    if( !HostAccountSelected( m_UserAccountList[User.dwCurrItem].xuid ) )
    {
        // Sponsor is gone, back to account selection...
        BeginSelectAccount( User );
        return;
    }

    switch( ev )
    {
        case EV_BUTTON_A:
        {
            // Signon as a guest of the current account 
            // Note: This requires that some other user
            // sign on with the actual (sponsor) account 
            User.Account = m_UserAccountList[User.dwCurrItem];
            User.bGuest  = TRUE;
            XBUtil_GetWide( m_UserAccountList[User.dwCurrItem].szGamertag, 
                User.strName, XONLINE_GAMERTAG_SIZE );
            wcscat( User.strName, L"\n(guest)" );
            User.State = STATE_USER_WAIT_FOR_OTHERS;
            break;
        }
        case EV_BUTTON_B:
        case EV_BUTTON_BACK:
            BeginSelectAccount( User );
            break;
        default:
            break;
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateUserStateWaitForOthers()
// Desc: Wait until for other users to select accounts
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateUserStateWaitForOthers( CUser& User, Event ev )
{
    switch( ev )
    {
        case EV_BUTTON_B:
        case EV_BUTTON_BACK:
            BeginSelectAccount( User );
            return;
    }

    m_bReadyForSignOn = TRUE;
    
    // First, make sure that every controller is either in the waiting for other
    // users state, or not selected for use
    for( DWORD i = 0; i < XONLINE_MAX_LOGON_USERS; ++i )
    {
        if( m_Users[i].State != STATE_USER_WAIT_FOR_OTHERS &&
            m_Users[i].State != STATE_USER_PRE_SIGN_ON )
        {
            m_bReadyForSignOn = FALSE;
            break;
        }
    }

    // Check that there are sponsor accounts for associated guest accounts
    for( DWORD i = 0; i < XONLINE_MAX_LOGON_USERS; ++i )
    {
        if( m_Users[i].State == STATE_USER_WAIT_FOR_OTHERS &&
            m_Users[i].bGuest  && 
            !HostAccountSelected( m_Users[i].Account.xuid ) )
        {
            // Sponsor account no longer available, go back to 
            // account selection
            BeginSelectAccount( m_Users[i] );
            m_bReadyForSignOn = FALSE;
        }
    }
    
    if( m_bReadyForSignOn && ev == EV_BUTTON_A )      
    {        
        // Ready to go...
        m_State = STATE_SIGNING_ON; 
        BeginSignOn();
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateStateSigningOn()
// Desc: Authentication is underway
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateSigningOn( Event ev )
{
    switch( ev )
    {
        case EV_BUTTON_B:
        case EV_BUTTON_BACK:
        {
            // Close the task (this cancels the sign on process)
            m_hOnlineTask.Close();
            for( DWORD i = 0; i < XONLINE_MAX_LOGON_USERS; ++i )
            {
                // Set all players who tried to sign in, back to the
                // account selection state
                if( m_Users[i].State != STATE_USER_PRE_SIGN_ON )
                    BeginSelectAccount(  m_Users[i] );
            }
            m_State = STATE_USER_EVENTS;
            return;
        }
    }
    
    HRESULT hr = m_hOnlineTask.Continue();
    
    if( hr != XONLINETASK_S_RUNNING )
    {
        if( hr != XONLINE_S_LOGON_CONNECTION_ESTABLISHED )
        {
            HandleSignOnError( hr );
            return;
        }

        m_bSignedOn = TRUE;

        // Next, check if the individual players
        XONLINE_USER* pLoggedOnUsers = XOnlineGetLogonUsers();
        assert( pLoggedOnUsers );
        
        // Get the initial states for the headphone and
        // microphone devices
        m_dwMicrophoneState = XGetDevices( XDEVICE_TYPE_VOICE_MICROPHONE );
        m_dwHeadphoneState  = XGetDevices( XDEVICE_TYPE_VOICE_HEADPHONE );

        BOOL bAllUsersSignedOn = TRUE;

        for( DWORD i = 0; i < XONLINE_MAX_LOGON_USERS; ++i )
        {
            if( m_Users[i].State != STATE_USER_PRE_SIGN_ON )
            {
                // Update the account information
                m_Users[i].Account = pLoggedOnUsers[i];

                hr = m_Users[i].Account.hr;
                
                if( SUCCEEDED( hr ) )
                {
                    if( !m_Users[i].bGuest )
                    {
                        DWORD dwState = XONLINE_FRIENDSTATE_FLAG_ONLINE;

                        m_Users[i].bVoice = XOnlineIsUserVoiceAllowed(
                                m_Users[i].Account.xuid.dwUserFlags ) && 
                            ( m_dwMicrophoneState & (1 << i) ) &&
                            ( m_dwHeadphoneState  & (1 << i) );

                        if( m_Users[i].bVoice )
                            dwState |= XONLINE_FRIENDSTATE_FLAG_VOICE;

                        SetPlayerOnlineState( i, dwState );
                    }
                    else
                    {
                        m_Users[i].bVoice = FALSE;
                    }
                    
                    m_Users[i].bSignedOn = TRUE;
                    
                    // Check if there are any messages for the User
                    if( pLoggedOnUsers[i].hr == 
                        XONLINE_S_LOGON_USER_HAS_MESSAGE )
                    {
                        SetUserErrorStr( m_Users[i], L"One or more messages are\n"
                                                     L"available. You may read\n"
                                                     L"them by visiting\n" 
                                                     L"the Xbox Dashboard." );
                        m_Users[i].bAllowBootToDash = TRUE;
                        m_Users[i].dwLaunchReason   = XLD_LAUNCH_DASHBOARD_ACCOUNT_MANAGEMENT;
                        m_Users[i].State            = STATE_USER_ERROR;
                        m_Users[i].NextState        = STATE_USER_DONE;
                    }
                    else
                        m_Users[i].State = STATE_USER_DONE;
                }
                else
                {
                    HandleUserSignOnError( m_Users[i], hr );
                    bAllUsersSignedOn = FALSE;
                }
            }
            else // Set controllers that didn't sign in to "done" state
                m_Users[i].State = STATE_USER_DONE;
        }

        // If all the users were signed on, go ahead and check that
        // the requested services are available.  
        if( bAllUsersSignedOn )
        {
            // Check for service errors and store service information
            m_ServiceInfoList.clear();
            for( DWORD  i = 0; i < NUM_SERVICES; ++i )
            {
                // Store service information
                XONLINE_SERVICE_INFO serviceInfo;
                hr = XOnlineGetServiceInfo( m_pServices[i], &serviceInfo );
                if( FAILED( hr ) )
                {
                    HandleServiceError( hr, m_pServices[i] );
                    return;
                }
                m_ServiceInfoList.push_back( serviceInfo );
            }

            m_State = STATE_MAIN_MENU;
        }
        else
        {
            // Back to processing individual controller events...
            m_State = STATE_USER_EVENTS;
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
            m_State = m_NextState;
            m_bAllowBootToDash = FALSE;
            break;
        case EV_BUTTON_X:
            if( m_bAllowBootToDash )
                BootToDash( m_dwLaunchReason );
            break;
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateUserStateError()
// Desc: A user specific error occurred
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateUserStateError( CUser& User, Event ev )
{
    switch( ev )
    {
        case EV_BUTTON_A:
            User.bAllowBootToDash = FALSE;
            // Transition to NextState, if this is account selection
            // then call BeginSelectAccount to prepare it first
            if( User.NextState == STATE_USER_SELECT_ACCOUNT )
                BeginSelectAccount( User );
            else
                User.State = User.NextState;
            break;

        case EV_BUTTON_X:
            if( User.bAllowBootToDash )
            {
            BootToDash( User.dwLaunchReason );
            }
    }
}




//-----------------------------------------------------------------------------
// Name: BeginSelectAccount()
// Desc: Initiate the account selection process
//-----------------------------------------------------------------------------
VOID CXBoxSample::BeginSelectAccount( CUser & User )
{
    User.dwCurrItem = 0;
    User.dwTopItem  = 0;
    User.bGuest     = FALSE;
    ZeroMemory( &User.Account, sizeof( User.Account ) );
    User.State      = STATE_USER_SELECT_ACCOUNT;
}

    
    
    
//-----------------------------------------------------------------------------
// Name: BeginSignOn()
// Desc: Initiate the authentication process
//-----------------------------------------------------------------------------
VOID CXBoxSample::BeginSignOn()
{
    // Close existing task handle
    m_hOnlineTask.Close();
    
    // XOnlineLogon() allows a list of up to 4 players (1 per controller)
    // to login in a single call.  The list must be a one-to-one match of
    // controller to user in order for the online system to recognize which 
    // user is using which controller.
    ZeroMemory( &m_LogonUsers, sizeof( m_LogonUsers ) );

    DWORD dwGuest = 1;
    for( DWORD i = 0; i < XONLINE_MAX_LOGON_USERS; ++i )
    {
        // Any unused controllers will be in the  
        // STATE_USER_PRE_SIGN_ON state
        if( m_Users[i].State != STATE_USER_PRE_SIGN_ON )
        {
            // If this account is a guest account, assign 
            // a unique guest id to it
            if( m_Users[i].bGuest )
            {
               XOnlineSetUserGuestNumber( 
                   m_Users[i].Account.xuid.dwUserFlags, dwGuest );
               dwGuest++;
            }
            m_LogonUsers[i] = m_Users[i].Account;
        }
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
// Desc: Handle machine authentication errors
//-----------------------------------------------------------------------------
VOID CXBoxSample::HandleSignOnError( HRESULT hr )
{
    HRESULT hrTitleUpdate;

    Reset();
    m_State = STATE_ERROR;

    switch( hr )
    {
        case XONLINE_E_LOGON_CONNECTION_LOST:
            m_UI.SetErrorStr( L"Network connection lost." );
            m_bAllowBootToDash = TRUE;
            m_dwLaunchReason   = XLD_LAUNCH_DASHBOARD_NETWORK_CONFIGURATION;
            break;
        case XONLINE_E_LOGON_INVALID_USER:
            m_UI.SetErrorStr( 
                L"This account is not current. Press " GLYPH_X_BUTTON L" to update the\n"
                L"account in Account Recovery."
                );
            m_bAllowBootToDash = TRUE;
            m_dwLaunchReason   = XLD_LAUNCH_DASHBOARD_ACCOUNT_MANAGEMENT;
            break;
        case XONLINE_E_LOGON_CANNOT_ACCESS_SERVICE:
            m_UI.SetErrorStr( L"Login failed with error 0x%x", hr );
            m_bAllowBootToDash = TRUE;    
            m_dwLaunchReason   = XLD_LAUNCH_DASHBOARD_NETWORK_CONFIGURATION;
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
// Desc: Handle User logon errors
//-----------------------------------------------------------------------------
VOID CXBoxSample::HandleUserSignOnError( CUser& User, HRESULT hr )
{
    User.State     = STATE_USER_ERROR;
    User.NextState = STATE_USER_DONE;

    switch( hr ) 
    {
        case XONLINE_E_LOGON_USER_ACCOUNT_REQUIRES_MANAGEMENT:
            SetUserErrorStr( User,  L"This account requires\nPlayer management" );
            User.dwLaunchReason = XLD_LAUNCH_DASHBOARD_ACCOUNT_MANAGEMENT;
            User.NextState      = STATE_USER_BOOT_TO_DASH;
            break;
        default:
            SetUserErrorStr( User, L"User Login failed\nwith error\n0x%x", hr );
    }
}




//-----------------------------------------------------------------------------
// Name: HandleServiceError()
// Desc: Handle service errors
//-----------------------------------------------------------------------------
VOID CXBoxSample::HandleServiceError( HRESULT hr, DWORD dwServiceId )
{
    Reset();
    m_State = STATE_ERROR;

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
// Name: UpdateUserStateDone()
// Desc: Display result of signon
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateUserStateDone( CUser & User, Event ev )
{
    switch( ev )
    {
        case EV_BUTTON_A:
            if( User.bSignedOn )
            {
                Reset();
                m_hOnlineTask.Close();
            }
            break;
        default:
            break;
    }
}




//-----------------------------------------------------------------------------
// Name: SetPlayerOnlineState()
// Desc: Broadcast updated User state for the world
//-----------------------------------------------------------------------------
VOID CXBoxSample::SetPlayerOnlineState( DWORD dwUserIndex, DWORD dwState )
{
    HRESULT hr = XOnlineNotificationSetState( dwUserIndex, dwState,
                                              XNKID(), 0, NULL );
    assert( SUCCEEDED( hr ) );
    (VOID)hr; // avoid compiler warning
}




//-----------------------------------------------------------------------------
// Name: SetUserErrorStr()
// Desc: Set error string
//-----------------------------------------------------------------------------
void __cdecl CXBoxSample::SetUserErrorStr( CUser& User,
                                           const WCHAR* strFormat, ... )
{
    va_list pArgList;
    va_start( pArgList, strFormat );
    
    INT iChars = wvsprintfW( User.strError, 
        strFormat, pArgList );
    assert( iChars < MAX_ERROR_STR );
    (void)iChars; // avoid compiler warning
    
    va_end( pArgList );
}




//-----------------------------------------------------------------------------
// Name: CheckDeviceStates()
// Desc: Check for voice peripheral state changes and update online state
//       This function is called once per frame as soon the title is online
//-----------------------------------------------------------------------------
VOID CXBoxSample::CheckDeviceStates()
{
    DWORD dwMicrophoneInsertions;
    DWORD dwMicrophoneRemovals;
    DWORD dwHeadphoneInsertions;
    DWORD dwHeadphoneRemovals;
    
    BOOL bMicrophoneChanges = XGetDeviceChanges( XDEVICE_TYPE_VOICE_MICROPHONE,
                                                 &dwMicrophoneInsertions,
                                                 &dwMicrophoneRemovals );
    BOOL bHeadphoneChanges = XGetDeviceChanges( XDEVICE_TYPE_VOICE_HEADPHONE,
                                                &dwHeadphoneInsertions,
                                                &dwHeadphoneRemovals );
    
    if( bMicrophoneChanges || bHeadphoneChanges )
    {
        // Update state for removals
        m_dwMicrophoneState &= ~dwMicrophoneRemovals;
        m_dwHeadphoneState  &= ~dwHeadphoneRemovals;
        
        // Then update state for new insertions
        m_dwMicrophoneState |= dwMicrophoneInsertions;
        m_dwHeadphoneState  |= dwHeadphoneInsertions;
        
        // Check the state of each local user
        for( DWORD i = 0; i < XONLINE_MAX_LOGON_USERS; ++i )
        {
            if( m_Users[i].bSignedOn &&
                XOnlineIsUserVoiceAllowed( m_Users[i].Account.xuid.dwUserFlags ) )
            {
                DWORD dwState = 0;
                
                // If either the microphone or the headphone was
                // removed since last call, the user no longer
                // has voice capability
                if( m_Users[i].bVoice )
                {
                    if( ( dwMicrophoneRemovals & ( 1 << i ) ) ||
                        ( dwHeadphoneRemovals  & ( 1 << i ) ) )
                    {
                        m_Users[i].bVoice = FALSE;
                        dwState = XONLINE_FRIENDSTATE_FLAG_ONLINE;
                    }
                }
                else
                {
                    // If both microphone and headphone are present, and
                    // the user didn't have voice capability, add it
                    if( ( m_dwMicrophoneState & ( 1 << i ) ) &&
                        ( m_dwHeadphoneState  & ( 1 << i ) ) )
                    {
                        m_Users[i].bVoice = TRUE;
                        dwState = XONLINE_FRIENDSTATE_FLAG_ONLINE | 
                            XONLINE_FRIENDSTATE_FLAG_VOICE;
                    }
                }    
                if( dwState ) // State has changed...
                    SetPlayerOnlineState( i, dwState );
            }
        }
    }
}




//-----------------------------------------------------------------------------
// Name: IsUserInPinEntry()
// Desc: Returns TRUE if any user is in the PIN entry state
//-----------------------------------------------------------------------------
BOOL CXBoxSample::IsUserInPinEntry()
{
    for( DWORD i = 0; i < XONLINE_MAX_LOGON_USERS; ++i )
    {
        if( m_Users[i].State == STATE_USER_PIN_ENTRY )
            return TRUE;
    }
    
    return FALSE;
}



