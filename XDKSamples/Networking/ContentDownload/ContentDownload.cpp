//-----------------------------------------------------------------------------
// File: ContentDownload.cpp
//
// Desc: Shows Xbox online content enumeration, download, installation
//       and removal.
//
// Hist: 08.08.01 - New for Aug M1 release
//       09.04.01 - Updated for Nov release; UI moved to UserInterface module
//       01.21.02 - Updated for Feb release
//       04.05.02 - Updated for May release; Added billable content and content
//                                           details.  Updated for the new
//                                           HD/DVD content enumeration API
//       06.05.02 - Updated for June release; Updated billing stuctures
//                                            Added removal of "bad" content
//       08.01.02 - Updated for Sept release; Added content downloader launch 
//       04.28.03 - Updated for June release; Decreased download time by
//                                            pumping download task handle
//                                            multiple times per frame
//
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "ContentDownload.h"
#include "XBStorageDevice.h"
#include "XBMemUnit.h"
#include "XBVoice.h"
#include <cassert>




//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_WHITE_BUTTON, XBHELP_PLACEMENT_2, L"Display\nhelp" },
    { XBHELP_A_BUTTON,     XBHELP_PLACEMENT_2, L"Select menu\nitem" },
    { XBHELP_B_BUTTON,     XBHELP_PLACEMENT_1, L"Cancel" },
    { XBHELP_DPAD,         XBHELP_PLACEMENT_1, L"Menu navigation" },
};

#define NUM_HELP_CALLOUTS 4




//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------
const DWORD ENUM_REQUEST_SIZE = 5;   // Don't request more than this number
                                     // at a time
const DWORD MAX_CONTENT_DISPLAYED = 5;    // Number to show on screen at once

const DWORD DOWNLOAD_UPDATE_TIME = 10; // Amount of time (in ms) we can spare
                                       // per frame to pump the download task
                                       // handle. For optimal download speed,
                                       // it is best to call XOnlineTaskContinue
                                       // as much as possible per frame.
                                       
                                       




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
    m_State             = STATE_SELECT_DEVICE;
    m_NextState         = STATE_SELECT_DEVICE;
    m_dwCurrItem        = 0;
    m_dwTopItem         = 0;
    m_dwCurrUser        = 0;
    m_dwUserIndex       = 0;
    m_EnumType          = HARD_DISK;
    m_bIsLoggedOn       = FALSE;
    m_dwCurrContent     = 0;
    m_pEnumBuffer       = NULL;
    m_pDetailsBuffer    = NULL;
    m_fPercentComplete  = 0.0f;
    m_dwBlocksInstalled = 0;
    m_dwBlocksTotal     = 0;
    m_bHelp             = FALSE;
    
    m_EnumParams.dwOfferingType = XONLINE_OFFERING_CONTENT |
                                  XONLINE_OFFERING_SUBSCRIPTION;
    m_EnumParams.dwBitFilter = 0xffffffff;    // All offers
    m_EnumParams.wStartingIndex = 0; 
    m_EnumParams.wMaxResults = ENUM_REQUEST_SIZE; 
    m_EnumParams.dwDescriptionIndex = 0;
    
    *m_strUser = 0;
    m_pServices[0] = XONLINE_BILLING_OFFERING_SERVICE;
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
    if( FAILED( XBOnline_GetUserList( m_UserList ) ) )
        return E_FAIL;
    
    CXBMemUnit::GetMemUnitSnapshot();   

    // If no accounts, then player needs to create an account.
    // For development purposes, accounts are created using the
    // Online Dashboard or the XDK Launcher
    if( m_UserList.size() == 0 )
        m_State = STATE_CREATE_ACCOUNT;
    else
        // check for new content
        BeginCheckForNewContent();
    
    return S_OK;
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
        if( m_bIsLoggedOn )
        {
            m_hOnlineTask.Close();
            
            m_bIsLoggedOn = FALSE;
            SetErrorString( L"This Xbox has lost its online connection" );
            m_State = STATE_ERROR;
        }
    }
    
    // Maintain our connection once we've logged on
    if( m_bIsLoggedOn )
    {
        HRESULT hr = m_hOnlineTask.Continue();    
        if( FAILED( hr ) )
        {
            
            if( hr == XONLINE_E_LOGON_KICKED_BY_DUPLICATE_LOGON )
                SetErrorString( L"You have been signed out because your\n"
                                L"account signed in on another Xbox" );
            else
                SetErrorString( L"Connection was lost (error 0x%x)."
                                L"Must re-login", hr );
            m_State = STATE_ERROR;
            m_hOnlineTask.Close();
            m_bIsLoggedOn = FALSE;
        }
    }
    
    Event ev = GetEvent();

    // toggle help
    if( ev == EV_BUTTON_WHITE )
        m_bHelp = !m_bHelp;

    // don't accept events during help display
    if( m_bHelp )
        ev = EV_NULL;

    switch( m_State )
    {
        case STATE_CREATE_ACCOUNT:          UpdateStateCreateAccount( ev );      break;
        case STATE_SELECT_ACCOUNT:          UpdateStateSelectAccount( ev );      break;
        case STATE_LOGGING_ON:              UpdateStateLoggingOn( ev );          break;
        case STATE_SELECT_DEVICE:           UpdateStateSelectDevice( ev );       break;
        case STATE_CHECK_FOR_NEW_CONT:      UpdateStateCheckForNewContent (ev ); break;
        case STATE_ENUM_CONTENT:            UpdateStateEnumContent( ev );        break;
        case STATE_SELECT_CONTENT:          UpdateStateSelectContent( ev );      break;
        case STATE_GET_DETAILS:             UpdateStateGetDetails( ev );         break;
        case STATE_CONTENT_DETAILS:         UpdateStateContentDetails( ev );     break;
        case STATE_CONFIRM_PURCHASE:        UpdateStateConfirm( ev );            break;
        case STATE_PURCHASE:                UpdateStatePurchase( ev );           break;
        case STATE_CONFIRM_CANCEL_SUB:      UpdateStateConfirm( ev );            break;
        case STATE_CANCEL_SUB:              UpdateStateCancelSub( ev );          break;
        case STATE_INSTALL_CONTENT:         UpdateStateInstallContent( ev );     break;
        case STATE_VERIFY_CONTENT:          UpdateStateVerifyContent( ev );      break;
        case STATE_CONFIRM_REMOVE:          UpdateStateConfirm( ev );            break;
        case STATE_CONFIRM_PARTIAL_REMOVE:  UpdateStateConfirm( ev );            break;
        case STATE_CONFIRM_ABORT:           UpdateStateConfirm( ev );            break;
        case STATE_BAD_CONTENT:             UpdateStateBadContent( ev );         break;
        case STATE_ERROR:                   UpdateStateContinue( ev );           break;
        case STATE_SUCCESS:                 UpdateStateContinue( ev );           break;
        case STATE_CONTENT_METADATA:        UpdateStateContentMetadata( ev );    break;
        default:                            assert( FALSE );                     break;
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
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET, 0x000A0A6A, 1.0f, 0L );

    if( m_bHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        // Draw the app title
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 48, 36, 0xffffffff, L"Content Download" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );

        switch( m_State )
        {
            case STATE_CREATE_ACCOUNT:
                m_UI.RenderCreateAccount( TRUE );
                break;
            case STATE_SELECT_ACCOUNT:
                m_UI.RenderSelectAccount( m_dwCurrItem, m_UserList );
                break;
            case STATE_LOGGING_ON:
                m_UI.RenderLoggingOn();
                break;
            case STATE_SELECT_DEVICE:
                m_UI.RenderSelectDevice( m_dwCurrItem );
                break;
            case STATE_CHECK_FOR_NEW_CONT:
                m_UI.RenderMessage( L"Checking for new content", TRUE, FALSE );
                break;
            case STATE_ENUM_CONTENT:
                m_UI.RenderMessage( L"Enumerating content", TRUE, FALSE );
                break;
            case STATE_SELECT_CONTENT:
                m_UI.RenderSelectContent( m_ContentList, m_dwCurrItem, m_dwTopItem );
                break;
            case STATE_GET_DETAILS:
                m_UI.RenderMessage( L"Getting details", TRUE, FALSE );
                break;
            case STATE_CONTENT_DETAILS:
            {
                assert( m_dwCurrItem < m_ContentList.size() );
                ContentInfo& contentInfo = m_ContentList[ m_dwCurrItem ];
                m_UI.RenderContentDetails( contentInfo, BillingEnabled() );
                break;
            }
            case STATE_CONTENT_METADATA:
            {
                assert( m_dwCurrItem < m_ContentList.size() );
                ContentInfo& contentInfo = m_ContentList[ m_dwCurrItem ];
                m_UI.RenderContentMetadata( contentInfo );
                break;
            }
            case STATE_CONFIRM_PURCHASE:
            {
                if( XONLINE_OFFERING_SUBSCRIPTION == 
                    m_ContentList[m_dwCurrContent].GetOfferingType())
                {
                    m_UI.RenderConfirm( L"Are you sure you want to purchase\n"
                                       L"the subscription?", 
                                        m_dwCurrItem );
                }
                else if( XONLINE_OFFERING_CONTENT == 
                        m_ContentList[m_dwCurrContent].GetOfferingType())
                {
                    m_UI.RenderConfirm( L"Are you sure you want to purchase\n"
                                        L"the content?", 
                                        m_dwCurrItem );
                }
                break;
            }
            case STATE_PURCHASE:
                m_UI.RenderMessage( L"Purchasing content\n"
                                    L"Please do not turn off your XBox", FALSE, FALSE);
                break;
            case STATE_CONFIRM_CANCEL_SUB:
            { 
                m_UI.RenderConfirm( L"Are you sure you want to cancel\n"
                                    L"the subscription?",
                                    m_dwCurrItem );
                break;
            }
            case STATE_CANCEL_SUB:
                m_UI.RenderMessage( L"Cancelling subscription\n"
                                    L"Please do not turn off your XBox", FALSE, FALSE);
                break;
            case STATE_INSTALL_CONTENT:
            {
                m_UI.RenderInstallContent( m_fPercentComplete, 
                                        m_dwBlocksInstalled, m_dwBlocksTotal );
                break;
            }
            case STATE_VERIFY_CONTENT:
                m_UI.RenderMessage( L"Verifying content", FALSE, FALSE );
                break;
            case STATE_BAD_CONTENT:
                m_UI.RenderMessage( m_strError );
                break;
                
            case STATE_CONFIRM_REMOVE: 
            { 
                m_UI.RenderConfirm( L"Are you sure you want to remove\n"
                                    L"the content from the hard disk?",
                                    m_dwCurrItem );
                break;
            }
            case STATE_CONFIRM_PARTIAL_REMOVE: 
            { 
                m_UI.RenderConfirm( L"Do you want to remove the partial\n"
                                    L"content from the hard disk?\n"
                                    L"If you delete the partial download, you\n"
                                    L"must re-download the entire package.",
                                    m_dwCurrItem );
                break;
            }
            case STATE_CONFIRM_ABORT: 
            { 
                m_UI.RenderConfirm( L"Are you sure you want to abort the content\n"
                                    L"download and installation?",
                                    m_dwCurrItem );
                break;
            }
            case STATE_ERROR:
                m_UI.RenderMessage( m_strError );
                break;
            case STATE_SUCCESS:
                m_UI.RenderMessage( m_strSuccess );
                break;
            default:
                assert( FALSE );
        }
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
    // "A" 
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_A ] )
        return EV_BUTTON_A;

    // "Start
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_START )
        return EV_BUTTON_START;
    
    // "B"
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_B ] )
        return EV_BUTTON_B;
    
    // "X"
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_X ] )
        return EV_BUTTON_X;
    
    // "Y"
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_Y ] )
        return EV_BUTTON_Y;

    // "White"
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_WHITE ] )
        return EV_BUTTON_WHITE;
    
    // "Back"
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_BACK )
        return EV_BUTTON_BACK;
    
    // Movement
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
        return EV_UP;
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
        return EV_DOWN;
    
    return EV_NULL;
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
            // Boot into the account creation section of the online dash.
            LD_LAUNCH_DASHBOARD ld;
            ZeroMemory( &ld, sizeof(ld) );
            ld.dwReason = XLD_LAUNCH_DASHBOARD_NEW_ACCOUNT_SIGNUP;
            XLaunchNewImage( NULL, PLAUNCH_DATA( &ld ) );
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
        case EV_BUTTON_B:
            m_State = STATE_SELECT_DEVICE;
            m_dwCurrItem = 0;
            m_dwTopItem = 0;
            return;
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
        {
            // Cancel the task
            m_hOnlineTask.Close();
            
            // Return to list of devices
            m_State = STATE_SELECT_DEVICE;
            m_bIsLoggedOn = FALSE;
            m_dwCurrItem = 0;
            m_dwTopItem = 0;
            
            return;
        }
    }
    
    HRESULT hr = m_hOnlineTask.Continue();
    if( FAILED( hr ) )
    {
        m_hOnlineTask.Close();
        SetErrorString( hr, L"Login failure. Try again." );
        m_State = STATE_ERROR;
        return;
    }
    
    // Check login status; partial results indicate that login itself
    // has completed.
    if( hr == XONLINE_S_LOGON_CONNECTION_ESTABLISHED )
    {
        HRESULT hrService = S_OK;
        
        // Check for general errors
        if( FAILED(hr) )
        {
            m_hOnlineTask.Close();
            SetErrorString( hr, L"Login failed" );
            m_State = STATE_ERROR;
            return;
            
        }
        else
        {
            // Next, check if the user was actually logged on
            PXONLINE_USER pLoggedOnUsers = XOnlineGetLogonUsers();
            
            assert( pLoggedOnUsers );
            
            hr = pLoggedOnUsers[ m_dwUserIndex ].hr;
            
            if( FAILED( hr ) )
            {
                m_hOnlineTask.Close();
                SetErrorString( hr, L"User Login failed" );
                m_State = STATE_ERROR;
                return;
            }
            else
            {
                // Check for service errors  
                for( DWORD i = 0; i < NUM_SERVICES; ++i )
                {
                    if( FAILED( hrService = XOnlineGetServiceInfo( 
                        m_pServices[i],NULL ) ) )
                    {
                        m_hOnlineTask.Close();
                        SetErrorString( hrService, L"Login failed, "
                                                   L"service %d", m_pServices[i] );
                        m_State = STATE_ERROR;
                        return;
                    }
                }
            }

            // copy user data
            CopyMemory( &m_UserList[ m_dwCurrUser ], &pLoggedOnUsers[ m_dwUserIndex ],
                        sizeof( XONLINE_USER ) );
        }
   
        // We're now on the system
        m_bIsLoggedOn = TRUE;
        
        // Notify the world
        DWORD dwState = XONLINE_FRIENDSTATE_FLAG_ONLINE |
            XONLINE_FRIENDSTATE_FLAG_PLAYING;
        if( XBVoice_HasDevice() )
            dwState |= XONLINE_FRIENDSTATE_FLAG_VOICE;
        SetPlayerState( dwState );
        
        // Begin enumerating content on the selected device
        m_State = STATE_ENUM_CONTENT;
        BeginEnum();
    }
}



//-----------------------------------------------------------------------------
// Name: UpdateStateSelectDevice()
// Desc: Handle enum device selection
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateSelectDevice( Event ev )
{
    switch( ev )
    {
        case EV_BUTTON_A:
            /// Begin enumerating content for the selected device
            assert( m_dwCurrItem < 3 );
            m_EnumType = EnumType( m_dwCurrItem );
            
            if( m_EnumType == ONLINE_DOWNLOADER )
            {
                // check for the existance of downloader
                if (GetFileAttributes( "d:\\downloader.xbe" ) == -1 )
                {
                    SetErrorString( L"Downloader.xbe not detected.\n"
                                    L"Please copy Downloader.xbe and the \n"
                                    L"downloader media directory to the same \n"
                                    L"directory as Contentdownload.xbe" );
                    m_State = STATE_ERROR;
                }
                else
                {
                    // Launch the Xbox content downloader tool to allow a
                    // user to sign on and download packages
                    XLaunchNewImage( "d:\\downloader.xbe", NULL );
                    assert( FALSE ); // Should never return
                }
            }
            else if( m_EnumType == HARD_DISK )
            {
                GetLocalContent( "T:\\" );
                m_State = STATE_SELECT_CONTENT;

                // verify HD content before displaying metadata
                m_NextState = STATE_VERIFY_CONTENT;

                m_dwCurrItem = 0;
                m_dwTopItem = 0;
            }
            else if( m_EnumType == ONLINE )
            {
                if( m_bIsLoggedOn )
                {
                    m_State = STATE_ENUM_CONTENT;
                    BeginEnum();
                }
                else
                {
                    m_State = STATE_SELECT_ACCOUNT;
                }
                m_dwCurrItem = 0;
                m_dwTopItem = 0;
            }
            else
                assert(FALSE);
            
            break;
            
        case EV_UP:
            if( m_dwCurrItem > 0 )
                m_dwCurrItem--;
            else
                m_dwCurrItem = 2;
            break;
            
        case EV_DOWN:
            if( m_dwCurrItem == 2 )
                m_dwCurrItem = 0;
            else
                ++m_dwCurrItem;
            break;
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateStateEnumContent()
// Desc: Spin in content enumeration
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateEnumContent( Event ev )
{
    switch( ev )
    {
        case EV_BUTTON_B:
            delete [] m_pEnumBuffer;
            m_pEnumBuffer = NULL;
            
            // Cancel the task
            // Return to list of devices
            m_hContentTask.Close();
            m_State = STATE_SELECT_DEVICE;
            m_dwCurrItem = 0;
            m_dwTopItem = 0;

            return;
    }
    
    HRESULT hr = m_hContentTask.Continue();
    if( hr != XONLINETASK_S_RUNNING )
    {
        // Handle pump errors
        if( FAILED(hr) )
        {
            SetErrorString( hr , L"Enumeration failed with" );
            m_State = STATE_ERROR;
            return;
        }
        
        // Extract the results
        PXONLINEOFFERING_INFO* ppInfo;
        DWORD dwItems;
        BOOL bPartial;
        
        hr = XOnlineOfferingEnumerateGetResults( m_hContentTask,
            &ppInfo, &dwItems, &bPartial );
        
        // Handle errors
        if( FAILED(hr) )
        {
            SetErrorString( hr, L"Enumeration failed" );
            m_State = STATE_ERROR;
            return;
        }
        
        // Save the results
        UINT dwStart = m_ContentList.size();
        m_ContentList.resize(dwStart + dwItems);
        for( DWORD i = 0; i < dwItems; ++i )
        {
            m_ContentList[i + dwStart].InitFromEnumInfo( *ppInfo[i] );
        }
        
        // If enumeration is not complete, continue enumerating
        if( bPartial )
        {
            // Enumeration is not complete, keep pumping for more results
            return;
        }
        
        // Enumeration is complete
        delete [] m_pEnumBuffer;
        m_pEnumBuffer = NULL;
        m_hContentTask.Close();
        m_State = STATE_SELECT_CONTENT;
        m_NextState = STATE_GET_DETAILS;
        m_dwCurrItem = 0;
        m_dwTopItem = 0;
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateStateSelectContent()
// Desc: Handle content selection
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateSelectContent( Event ev )
{
    switch( ev )
    {
        case EV_BUTTON_A:
            // If no items, return to device list
            if( m_ContentList.empty() )
            {
                Reset();
                return;
            }
            
            // Display content detail
            m_dwCurrContent = m_dwCurrItem;

            // hard disk has no content detail
            if( m_NextState == STATE_GET_DETAILS )
            {
                BeginGetDetails();
            }
            m_State = m_NextState;

            break;
            
            
        case EV_BUTTON_B:
            if( !m_ContentList.empty() )
                // Return to device menu
                Reset();
            break;
            
        case EV_UP:
            if( m_ContentList.empty() )
                break;
            
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
            if( m_ContentList.empty() )
                break;
            
            // If we're at the bottom of the displayed list, shift the display
            if( m_dwCurrItem == m_dwTopItem + MAX_CONTENT_DISPLAYED - 1 )
            {
                if( m_dwTopItem + MAX_CONTENT_DISPLAYED < m_ContentList.size() )
                    ++m_dwTopItem;
            }
            
            // Move to next item
            if( m_dwCurrItem < m_ContentList.size() - 1 )
                ++m_dwCurrItem;
            
            break;
        }
}




//-----------------------------------------------------------------------------
// Name: UpdateStateGetDetails()
// Desc: Spin in get details
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateGetDetails( Event ev )
{
    switch( ev )
    {
        case EV_BUTTON_B:
            
            delete [] m_pDetailsBuffer;
            m_pDetailsBuffer = NULL;
            
            // Cancel the task
            m_hContentTask.Close();
            m_State = STATE_SELECT_CONTENT;
            m_NextState = STATE_GET_DETAILS;
            return;
    }

    HRESULT hr = m_hContentTask.Continue();
    if( hr != XONLINETASK_S_RUNNING )
    {
        // Handle pump errors
        if( FAILED(hr) )
        {
            SetErrorString( hr, L"Get Details failed" );
            m_State = STATE_ERROR;
            return;
        }
        
        // Extract the results
        XONLINEOFFERING_DETAILS Details;
        
        hr = XOnlineOfferingDetailsGetResults( m_hContentTask, &Details);
                
        // Handle errors
        if( FAILED(hr) )
        {
            SetErrorString( hr, L"Get Details failed" );
            m_State = STATE_ERROR;
            return;
        }

        // save details 
        m_ContentList[m_dwCurrContent].InitFromDetails( Details );
        
        // get details is complete
        delete [] m_pDetailsBuffer;
        m_pDetailsBuffer = NULL;
        m_hContentTask.Close();
        m_State = STATE_CONTENT_DETAILS;
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateStateContentDetails()
// Desc: Handles content detail
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateContentDetails( Event ev )
{
    switch( ev )
    {
        case EV_BUTTON_A:
            // subscription
            if( XONLINE_OFFERING_SUBSCRIPTION ==
                m_ContentList[m_dwCurrContent].GetOfferingType() )
            {
                // don't allow purchase if purchase or cancel if permissions are off
                if(BillingEnabled())
                {
                    if( m_ContentList[m_dwCurrContent].GetNumInstances() == 0 )
                    {
                        m_State = STATE_CONFIRM_PURCHASE;
                        m_dwCurrItem = CONFIRM_YES;
                    }
                    else
                    {
                        m_State = STATE_CONFIRM_CANCEL_SUB;
                        m_dwCurrItem = CONFIRM_NO;
                    }
                }
            }
            // pay content (only pay the first time)
            else if( !m_ContentList[m_dwCurrContent].GetPrice().fOfferingIsFree &&
                    m_ContentList[m_dwCurrContent].GetNumInstances() == 0 )
            {
                // don't allow purchase if purchase or cancel if permissions are off
                if(BillingEnabled())
                {
                    m_State = STATE_CONFIRM_PURCHASE;
                    m_dwCurrItem = CONFIRM_YES;
                }
              
            }
            // free content
            else
            {
                m_State = STATE_INSTALL_CONTENT;
                BeginInstall();
            }
            
            break;
            
        case EV_BUTTON_B:
            // Return to content list
            m_State = STATE_SELECT_CONTENT;
            m_NextState = STATE_GET_DETAILS;
            break;
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateStateContentMetadata()
// Desc: Handles content metadata
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateContentMetadata( Event ev )
{
    switch( ev )
    {
        case EV_BUTTON_A:
            // remove content
            m_State = STATE_CONFIRM_REMOVE;
            m_dwCurrItem = CONFIRM_NO;
            break;
            
        case EV_BUTTON_B:
            // Return to content list
            m_State = STATE_SELECT_CONTENT;

            // verify HD content before displaying metadata
            m_NextState = STATE_VERIFY_CONTENT;
            break;
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateStateInstallContent()
// Desc: Spin during download/installation
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateInstallContent( Event ev )
{
    switch( ev )
    {
        case EV_BUTTON_B:
            // If a player expressly cancels an installation, it's up to the title
            // to handle content removal. However, if the installation
            // is aborted because of network failure or other reasons, it can
            // be resumed without requiring a full download. This is handled
            // automatically.  For demonstration purposes we prompt the user
            // to determine whether or not the content should be removed.
            m_dwCurrItem = CONFIRM_NO;
            m_State = STATE_CONFIRM_ABORT;
            return;
    }
    
    // Determine the download progress
    DWORD dwPercent;
    ULONGLONG qwBytesInstalled;
    ULONGLONG qwBytesTotal;
    
    HRESULT hr = XOnlineContentInstallGetProgress( m_hContentTask,
        &dwPercent, &qwBytesInstalled, &qwBytesTotal );
    m_fPercentComplete = FLOAT(dwPercent) / 100.0f;
    
    // Convert bytes to blocks
    const ULONGLONG qwBlockSize = ULONGLONG( CXBStorageDevice::GetBlockSize() );
    m_dwBlocksInstalled = DWORD( ( qwBytesInstalled + (qwBlockSize-1) ) / qwBlockSize );
    m_dwBlocksTotal = DWORD( ( qwBytesTotal + (qwBlockSize-1) ) / qwBlockSize );

    DWORD dwStartTime = timeGetTime();

    do
    {
        hr = m_hContentTask.Continue();
        if( hr != XONLINETASK_S_RUNNING )
        {
            // Installation complete
            m_hContentTask.Close();
        
            // Handle errors
            if( FAILED(hr) )
            {
                SetErrorString( hr, L"Installation failed" );
                m_State = STATE_ERROR;
                return;
            }
        
            // Move to the verification phase
            swprintf( m_strSuccess, L"Content installed" );
            m_State = STATE_SUCCESS;
            return;
        }
    }
    while( timeGetTime() - dwStartTime < DOWNLOAD_UPDATE_TIME );
}




//-----------------------------------------------------------------------------
// Name: UpdateStateVerifyContent()
// Desc: Spin during content verification phase
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateVerifyContent( Event ev )
{
    // Get the ID of the selected content
    assert( m_dwCurrContent < m_ContentList.size() );

    // if the content is verified, show its metadata
    if( VerifyContent( m_ContentList[m_dwCurrContent].GetContentDirectory() ) )
        m_State = STATE_CONTENT_METADATA;
    else
        m_State = STATE_BAD_CONTENT;
}




//-----------------------------------------------------------------------------
// Name: UpdateStateConfirm()
// Desc: Confirm state
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateConfirm( Event ev )
{
    switch( ev )
    {
        case EV_BUTTON_A:
            if( m_dwCurrItem == 0 ) // "Yes"
            {
                switch( m_State )
                {
                    case STATE_CONFIRM_REMOVE:
                        RemoveContent();
                        break;
                    case STATE_CONFIRM_PARTIAL_REMOVE:
                        m_hContentTask.Close();
                        RemoveContent();
                        break;
                    case  STATE_CONFIRM_ABORT:
                        m_State = STATE_CONFIRM_PARTIAL_REMOVE;
                        m_dwCurrItem = 1;
                        break;
                    case  STATE_CONFIRM_PURCHASE:
                        m_State = STATE_PURCHASE;
                        BeginPurchase();
                        break;
                    case STATE_CONFIRM_CANCEL_SUB: 
                        m_State = STATE_CANCEL_SUB;
                        BeginCancelSub();
                        break;
                    default:
                        assert( FALSE );
                }
            }
                   
            else // "No"
            {
                m_dwCurrItem = m_dwCurrContent;

                switch( m_State)
                {
                    case STATE_CONFIRM_REMOVE:
                        m_State = STATE_CONTENT_METADATA;
                        break;
                    case STATE_CONFIRM_PARTIAL_REMOVE:
                        m_hContentTask.Close();
                        swprintf( m_strSuccess, L"Download Aborted" );
                        m_State = STATE_SUCCESS;
                        break;
                    case STATE_CONFIRM_ABORT:
                        m_State = STATE_INSTALL_CONTENT;
                        break;
                    case STATE_CONFIRM_PURCHASE:
                        m_State = STATE_CONTENT_DETAILS;
                        break;
                    case STATE_CONFIRM_CANCEL_SUB:
                        m_State = STATE_CONTENT_DETAILS;
                        break;
                    default:
                        assert( FALSE );
                }

            }
            break;
            
        case EV_BUTTON_B:
            m_dwCurrItem = m_dwCurrContent;
            
            switch( m_State )
            {
                case STATE_CONFIRM_REMOVE:
                    m_State = STATE_CONTENT_METADATA;
                    break;
                case STATE_CONFIRM_PARTIAL_REMOVE:
                    m_State = STATE_CONFIRM_ABORT;
                    m_dwCurrItem = 1;
                    break;
                case STATE_CONFIRM_ABORT:
                    m_State = STATE_INSTALL_CONTENT;
                    break;
                case STATE_CONFIRM_PURCHASE:
                    m_State = STATE_CONTENT_DETAILS;
                    break;
                case STATE_CONFIRM_CANCEL_SUB:
                    m_State = STATE_CONTENT_DETAILS;
                    break;
                default:
                    assert( FALSE );
            }
            break;
            
        case EV_UP:
            if( m_dwCurrItem == 0 )
                m_dwCurrItem = CONFIRM_MAX - 1;
            else
                --m_dwCurrItem;
            break;
            
        case EV_DOWN:
            if( m_dwCurrItem == CONFIRM_MAX - 1 )
                m_dwCurrItem = 0;
            else
                ++m_dwCurrItem;
            break;
    }
}



//-----------------------------------------------------------------------------
// Name: UpdateStatePurchase()
// Desc: Spin during purchase
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStatePurchase( Event ev )
{
    switch( ev )
    {
        case EV_BUTTON_B:
            // XOnlineTaskClose() will not cancel the purchase process,
            break;
    }
    
    HRESULT hr = m_hContentTask.Continue();
    
    if( hr != XONLINETASK_S_RUNNING )
    {
        // purchase complete
        m_hContentTask.Close();
        
        // Handle errors
        if( FAILED(hr) )
        {
            SetErrorString( hr, L"Purchase failed" );
            m_State = STATE_ERROR;
            return;
        }

        if( XONLINE_OFFERING_SUBSCRIPTION == 
            m_ContentList[m_dwCurrContent].GetOfferingType() )
        {
            // Success! Return to front end.
            swprintf( m_strSuccess, L"Subscription purchased" );
            m_State = STATE_SUCCESS;
        }
        else if( XONLINE_OFFERING_CONTENT ==
            m_ContentList[m_dwCurrContent].GetOfferingType() )
        {   
            // install the content
            m_State = STATE_INSTALL_CONTENT;
            BeginInstall();
        }
        else
            assert( FALSE ); // only two types of content supported
        
    }
}



//-----------------------------------------------------------------------------
// Name: UpdateStateCancelSub()
// Desc: Spin during cancel subscription
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateCancelSub( Event ev )
{
    switch( ev )
    {
        case EV_BUTTON_B:
            // XOnlineTaskClose() will not cancel the cancel purchase process,
            break;
    }
    
    HRESULT hr = m_hContentTask.Continue();
    
    if( hr != XONLINETASK_S_RUNNING )
    {
        // purchase complete
        m_hContentTask.Close();

        // Handle errors
        if( FAILED(hr) )
        {
            SetErrorString( hr, L"Cancel purchase failed" );
            m_State = STATE_ERROR;
            return;
        }

        // Success! Return to front end.
        swprintf( m_strSuccess, L"Subscription cancelled" );
        m_State = STATE_SUCCESS;
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateStateCheckForNewContent()
// Desc: check for new content
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateCheckForNewContent( Event ev )
{
    switch( ev )
    {
        case EV_BUTTON_B:
            Reset();
            return;
    }

    HRESULT hr = m_hContentTask.Continue();
    
    if( hr != XONLINETASK_S_RUNNING )
    {
        // check complete
        m_hContentTask.Close();

        // Handle errors
        if( FAILED(hr) )
        {
            SetErrorString( hr, L"Check for new content failed" );
            m_State = STATE_ERROR;
            return;
        }

        if( hr == XONLINE_S_OFFERING_NEW_CONTENT)
        {
            swprintf( m_strSuccess, L"New content is available!" );
            m_State = STATE_SUCCESS;
        }
        else if( hr == XONLINE_S_OFFERING_NO_NEW_CONTENT )
        {
            swprintf( m_strSuccess, L"No new content is available." );
            m_State = STATE_SUCCESS;
        }
        else
        {
            SetErrorString( hr, L"Check for new content, unexpected result" );
            m_State = STATE_ERROR;
        }
    }
}
    


//-----------------------------------------------------------------------------
// Name: UpdateStateContinue()
// Desc: Press A to continue to next state
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateContinue( Event ev )
{
    switch( ev )
    {
        case EV_BUTTON_A:
            Reset();
            break;
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateStateBadContent()
// Desc: Press A to continue to next state
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateBadContent( Event ev )
{
    switch( ev )
    {
        case EV_BUTTON_A:
            if( m_State == STATE_BAD_CONTENT )
                RemoveContent();
            break;
    }
}




//-----------------------------------------------------------------------------
// Name: BeginLogin()
// Desc: Initiate the authentication process
//-----------------------------------------------------------------------------
VOID CXBoxSample::BeginLogin()
{
    // If we're already logged on, go directly to content enumeration
    if( m_bIsLoggedOn )
    {
        // Begin enumerating content on the selected device
        m_State = STATE_ENUM_CONTENT;
        BeginEnum();
        return;
    }
    
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
        // Cancel the task
        m_hOnlineTask.Close();
        if( hr == XONLINE_E_LOGON_NO_NETWORK_CONNECTION )
            SetErrorString( L"No network connection detected" );
        else
            SetErrorString( hr, L"Login failed to start" );
        m_State = STATE_ERROR;
    }
}




//-----------------------------------------------------------------------------
// Name: BeginEnum()
// Desc: Initiate the enumeration process
//-----------------------------------------------------------------------------
VOID CXBoxSample::BeginEnum()
{
    // Clear the existing list
    m_ContentList.clear();
    
    // Determine the buffer size required for enumeration
    DWORD dwBufferSize = XOnlineOfferingEnumerateMaxSize( &m_EnumParams, 0 ); 
    
    // Allocate the enumeration buffer
    assert( m_pEnumBuffer == NULL );
    m_pEnumBuffer = new BYTE [ dwBufferSize ];
    
    // Initiate the enumeration on the selected device, using the
    // credentials of the user on the current controller
    HRESULT hr = XOnlineOfferingEnumerate( m_dwUserIndex, &m_EnumParams, 
                                           m_pEnumBuffer, dwBufferSize,
                                           NULL, &m_hContentTask );
    if( FAILED(hr) )
    {
        SetErrorString( hr, L"Enumeration failed to start" );
        m_State = STATE_ERROR;
    }
}




//-----------------------------------------------------------------------------
// Name: GetLocalContent()
// Desc: Initiate the enumeration process
//-----------------------------------------------------------------------------
VOID CXBoxSample::GetLocalContent( const char* strRootPathName )
{
    m_ContentList.clear();

    XCONTENT_FIND_DATA FindData;

    HANDLE hFind = XFindFirstContent( strRootPathName, 0xFFFFFFFF, &FindData );
    if( hFind == INVALID_HANDLE_VALUE )
        return;

    do
    {
        m_ContentList.resize( m_ContentList.size() + 1 );
        m_ContentList.back().InitFromContentFindData( FindData );
    }
    while( XFindNextContent( hFind, &FindData) );
    
    CloseHandle( hFind );
}




//-----------------------------------------------------------------------------
// Name: VerifyContent()
// Desc: Recursively verifies the content of a given directory
//-----------------------------------------------------------------------------
BOOL CXBoxSample::VerifyContent( const char* strContentDirectory )
{
    assert( strContentDirectory );

    // get signatures handle 
    HANDLE hSig = XLoadContentSignatures( strContentDirectory );
    if ( hSig == NULL )
    {
        SetErrorString( L"Cound not load content signatures" );
        m_State = STATE_ERROR;
        return FALSE;
    }

    // create vector of search directories
    std::vector< std::string > Directories;

    // add initial search directory
    std::string RootDir( strContentDirectory );
    RootDir += "\\";
    Directories.push_back( RootDir );
    DWORD dwRootLen = RootDir.size();

    // check the content of root and child directories
    BOOL bSuccess = TRUE;
    while( Directories.size() > 0 && bSuccess )
    {
        std::string CurDir = Directories.back();
        Directories.pop_back();
        
        // find files
        WIN32_FIND_DATA FindData;
        HANDLE hFind = FindFirstFile( (CurDir + "*.*").c_str(), &FindData );
        if( hFind == INVALID_HANDLE_VALUE )
        {
            // no content in this directory
            continue;
        }

        do
        {
            // create local path name
            std::string LocalPath =
                CurDir.substr(dwRootLen) + FindData.cFileName;
            
            // create full path name
            std::string FullPath =
                CurDir + FindData.cFileName;

            // skip the metadata file
            if( _stricmp( FindData.cFileName, "contentmeta.xbx" ) == 0 )
                continue;
            
            // skip . and .. directories
            if( FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
            {
                if(  FindData.cFileName[0] != '.' )
                {
                    FullPath += std::string("\\");
                    Directories.push_back( FullPath );
                    continue;
                }
            }
            
            // verify content of file
            HANDLE hFile = CreateFile( FullPath.c_str(), GENERIC_READ,
                                       FILE_SHARE_READ, NULL,
                                       OPEN_EXISTING,
                                       FILE_ATTRIBUTE_NORMAL, NULL );
            if( hFile != INVALID_HANDLE_VALUE )
            {
                // read in file
                DWORD dwFileSize = GetFileSize(hFile, NULL);

                //skip files of 0 sizle
                if(dwFileSize == 0)
                    continue;

                BYTE* pbyBuffer = new BYTE[dwFileSize];
                DWORD dwNumBytesRead;
                if(! ReadFile( hFile, pbyBuffer,
                     dwFileSize, &dwNumBytesRead, NULL ) )
                {
                    SetErrorString( L"Could not read file to validate signature");
                    m_State = STATE_ERROR;
                    bSuccess = FALSE;
                }
                else
                {
                    // locate signature
                    BYTE* pbySignature = NULL;
                    DWORD dwSignatureSize = XCALCSIG_SIGNATURE_SIZE;
                    if(! XLocateSignatureByName( hSig, LocalPath.c_str(), 
                                                 0, dwFileSize,
                                                 &pbySignature, &dwSignatureSize ) )
                    {
                        SetErrorString( L"Could not locate signature" );
                        m_State = STATE_ERROR;
                        bSuccess = FALSE;
                    }
                    else
                    {
                        // compare signatures
                        DWORD dwVerifySize =  XCALCSIG_SIGNATURE_SIZE;
                        BYTE abyVerify[XCALCSIG_SIGNATURE_SIZE];
                        if( !XCalculateContentSignature( pbyBuffer, dwNumBytesRead,
                                                     abyVerify, &dwVerifySize ) )
                        {
                            SetErrorString( L"Could not calculate signature" );
                            m_State = STATE_ERROR;
                            bSuccess = FALSE;
                        }
                        else
                        {
                            if( ( dwVerifySize != dwSignatureSize ) ||
                                ( memcmp(abyVerify, pbySignature, dwVerifySize) != 0))
                            {
                                SetErrorString( L"Signature mismatch" );
                                m_State = STATE_ERROR;
                                bSuccess = FALSE;
                            }
                        }
                    }
                }

                // cleanup 
                CloseHandle( hFile );
                delete [] pbyBuffer;
            }
            else
            {
                SetErrorString( L"Could not open a file\n"
                                L"to to verify content signature" );
                m_State = STATE_ERROR;
                bSuccess = FALSE;
            }
        
        } while( FindNextFile( hFind, &FindData ) && bSuccess );

        // cleanup
        CloseHandle( hFind );
    }

    XCloseContentSignatures( hSig );

    return bSuccess;
}




//-----------------------------------------------------------------------------
// Name: BeginGetDetials()
// Desc: Initiate the get details process
//-----------------------------------------------------------------------------
VOID CXBoxSample::BeginGetDetails()
{
    // Determine the buffer size required for details.  
    DWORD dwBufferSize = XOnlineOfferingDetailsMaxSize( 0 ); 
    
    // Allocate the enumeration buffer
    assert( m_pDetailsBuffer == NULL );
    m_pDetailsBuffer = new BYTE [ dwBufferSize ];

    HRESULT hr = XOnlineOfferingDetails( m_dwUserIndex, m_ContentList[m_dwCurrContent].GetId(),
                                         XGetLanguage(), 0, m_pDetailsBuffer, 
                                         dwBufferSize, NULL, &m_hContentTask );
    if( FAILED(hr) )
    {
        SetErrorString( hr,  L"Get Details failed to start" );
        m_State = STATE_ERROR;
    }
}




//-----------------------------------------------------------------------------
// Name: BeginPurchase()
// Desc: Initiate the purchase process
//-----------------------------------------------------------------------------
VOID CXBoxSample::BeginPurchase()
{
    HRESULT hr = XOnlineOfferingPurchase( m_dwUserIndex,
                                          m_ContentList[m_dwCurrContent].GetId(),
                                          NULL, &m_hContentTask );

    if( FAILED(hr) )
    {
        SetErrorString( hr,  L"Purchase failed to start" );
        m_State = STATE_ERROR;
    }
}




//-----------------------------------------------------------------------------
// Name: BeginInstall()
// Desc: Initiate the download and installation process
//-----------------------------------------------------------------------------
VOID CXBoxSample::BeginInstall()
{
    // Clear progress bar
    m_fPercentComplete  = 0.0f;
    m_dwBlocksInstalled = 0;
    m_dwBlocksTotal     = 0;
    
    // Get the ID of the selected content
    assert( m_dwCurrContent < m_ContentList.size() );
    XOFFERING_ID id = m_ContentList[ m_dwCurrContent ].GetId();
    
    // Initiate the installation of the selected content
    HRESULT hr = XOnlineContentInstall( id, NULL, &m_hContentTask );
    
    if( FAILED(hr) )
    {
        SetErrorString( hr, L"Installation failed to start" );
        m_State = STATE_ERROR;
    }
}




//-----------------------------------------------------------------------------
// Name: CancelPurchase()
// Desc: Initiate the cancel purchase process
//-----------------------------------------------------------------------------
VOID CXBoxSample::BeginCancelSub()
{
    HRESULT hr = XOnlineOfferingCancel( m_dwUserIndex,
                                        m_ContentList[m_dwCurrContent].GetId(),
                                        NULL, &m_hContentTask );
    if( FAILED(hr) )
    {
        SetErrorString( hr, L"Cancel failed to start" );
        m_State = STATE_ERROR;
    }
}




//-----------------------------------------------------------------------------
// Name: RemoveContent()
// Desc: Removed content
//-----------------------------------------------------------------------------
VOID CXBoxSample::RemoveContent()
{
    // Get the ID of the selected content
    assert( m_dwCurrContent < m_ContentList.size() );
    XOFFERING_ID id = m_ContentList[ m_dwCurrContent ].GetId();

    CHAR strDirectory[MAX_PATH];
    if( !XGetContentInstallLocationFromIDs( 0, id, strDirectory ) )
    {
        SetErrorString( L"Could not find content installation location" );
        m_State = STATE_ERROR;
    }

    if( !XRemoveContent( strDirectory ) )
    {
        SetErrorString( L"Cound not remove content" );
        m_State = STATE_ERROR;
    }

    swprintf( m_strSuccess, L"Content removed" );
    m_State = STATE_SUCCESS;
}




//-----------------------------------------------------------------------------
// Name: BeginCheckForNewContent
// Desc: Begins the check for new content.
//-----------------------------------------------------------------------------
VOID CXBoxSample::BeginCheckForNewContent()
{
    m_State = STATE_CHECK_FOR_NEW_CONT;

    HRESULT hr = XOnlineOfferingIsNewContentAvailable( 0xffffffff, NULL, &m_hContentTask );

    if( FAILED( hr ) )
    {
        if( hr == XONLINE_E_LOGON_NO_NETWORK_CONNECTION )
            SetErrorString( L"No network connection detected" );
        else
            SetErrorString( hr, L"Is new content available failed to start" );
        m_State = STATE_ERROR;
        return;
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
// Name: SetErrorString()
// Desc: Starts the error state
//-----------------------------------------------------------------------------
VOID CXBoxSample::SetErrorString( const WCHAR* strFormat, ...)
{
    va_list pArgList;
    va_start( pArgList, strFormat );
    wvsprintfW( m_strError, strFormat, pArgList );
    va_end( pArgList );
}




//-----------------------------------------------------------------------------
// Name: SetErrorString()
// Desc: Starts the error state
//-----------------------------------------------------------------------------
VOID CXBoxSample::SetErrorString( HRESULT hr, const WCHAR* strFormat, ...)
{
    WCHAR strBuffer[512];
    const WCHAR* strHR = XBOnline_GetOnlineHResultString(hr);
    if( strHR )
        wsprintfW( strBuffer, L"%s\n%s", strFormat, strHR );
    else
        wsprintfW( strBuffer, L"%s\nHRESULT 0x%x", strFormat, hr );

    va_list pArgList;
    va_start( pArgList, strFormat );
    wvsprintfW( m_strError, strFormat, pArgList );
    va_end( pArgList );
}




//-----------------------------------------------------------------------------
// Name: Reset()
// Desc: Resets back to select device
//-----------------------------------------------------------------------------
VOID CXBoxSample::Reset()
{
    m_hContentTask.Close();
    
    m_State         = STATE_SELECT_DEVICE;
    m_dwCurrItem    = 0;
    m_dwTopItem     = 0;
    m_dwCurrContent = 0;
    
    delete [] m_pEnumBuffer;
    m_pEnumBuffer = NULL;
    delete [] m_pDetailsBuffer;
    m_pDetailsBuffer = NULL;
}




//-----------------------------------------------------------------------------
// Name: BillingEnabled()
// Desc: does the current logged on user have billing permissions
//-----------------------------------------------------------------------------
BOOL CXBoxSample::BillingEnabled()
{
   return XOnlineIsUserPurchaseAllowed( m_UserList[m_dwCurrUser].xuid.dwUserFlags );
}

