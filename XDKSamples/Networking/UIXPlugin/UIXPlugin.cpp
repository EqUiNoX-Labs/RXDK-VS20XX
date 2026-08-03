//-----------------------------------------------------------------------------
// File: UIXPlugin.cpp
//
// Desc: Demonstrates how to create your own UI plugin for UIX.
//
// Hist: 07.14.03 - New for August release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <xtl.h>
#include <xact.h>
#include <xonline.h>
#include <uix.h>
#include "XBApp.h"
#include "XBHelp.h"
#include "XBFont.h"
#include "XBOnline.h"
#include "CustomPlugin.h"




//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

// Number of services to authenticate
const DWORD NUM_SERVICES     = 2;
const CHAR* strWaveBankFile  = "d:\\Media\\UIXDefault.xwb";
const CHAR* strSoundBankFile = "d:\\Media\\UIXDefault.xsb";

// The main menu has three entries: Signon for 1, 2 and 4 users
const DWORD MAX_SIGNON_USERS_MENU = 3;

enum SIGNON_STATE
{
    STATE_SELECT_USER_COUNT,// Select number of users to sign on
    STATE_SIGNING_ON,       // Perform authentication
    STATE_SIGNED_ON,        // Run user state machines
    STATE_ERROR,            // Error
};

struct LOCALUSER
{
    WCHAR strGamertag[XONLINE_MAX_GAMERTAG_LENGTH+1];
    BOOL  bSignedOn;
    BOOL  bVoice;
    BOOL  bGuest;
    DWORD dwUserFlags;
    INT   iPane;
};




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    ILiveEngine*   m_pLiveEngine;       // The UIX Live Engine

    CXBFont        m_Font;              // Main font
    CXBFont        m_OnlineIconsFont;   // Font for online icons
    
    VOID*          m_pWaveBankMemory;
    VOID*          m_pSoundBankMemory;
    PXACTWAVEBANK  m_pWaveBank;
    PXACTENGINE    m_pXactEngine;
    PXACTSOUNDBANK m_pSoundBank;

    WCHAR          m_strMessage[1024];

    SIGNON_STATE   m_State;             // Current state
    SIGNON_STATE   m_NextState;         // Return to this state
    DWORD          m_dwMicrophoneState;
    DWORD          m_dwHeadphoneState;
    DWORD          m_dwNumSignOnUsers;
    DWORD          m_dwCurrItem;

    BOOL           m_bSignedOn;         // Successfully signed on
    DWORD          m_dwLiveWorkFlags;
    LOCALUSER      m_Users[XONLINE_MAX_LOGON_USERS];

    HRESULT BeginSignOn();
    VOID    UpdateStateSigningOn();
    VOID    UpdateStateSignedOn();
    VOID    UpdateStateError();
    VOID    UpdateStateSelectUserCount();
    VOID    Reset();

    VOID    SetPlayerOnlineState( DWORD dwUserIndex, DWORD dwState );
    VOID    CheckDeviceStates();
    HRESULT InitSound();

    VOID    RenderSelectUserCount( DWORD dwCurrItem );
    VOID    RenderAuthentication( );
    VOID    RenderMessage( const WCHAR* strMessage );
    VOID    RenderSignedOn( DWORD dwControllerIndex, DWORD iPane, const WCHAR* strGamerTag,
                            BOOL bSignedOn, BOOL bVoice, BOOL bGuest );

public:
    virtual HRESULT Initialize();
    virtual HRESULT FrameMove();
    virtual HRESULT Render();

    CXBoxSample();
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
    m_State             = STATE_SELECT_USER_COUNT;
    m_NextState         = STATE_SELECT_USER_COUNT;
    m_bSignedOn         = FALSE;
    m_strMessage[0]     = '\0';
    m_dwMicrophoneState = 0;
    m_dwHeadphoneState  = 0;
    m_dwNumSignOnUsers  = 0;
    m_dwCurrItem        = 0;
    m_pLiveEngine       = NULL;
    m_dwLiveWorkFlags   = 0L;
    ZeroMemory( &m_Users, sizeof(m_Users) );
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize device-dependent objects
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    HRESULT hr;

    // Select the font file based on the languages setting of the Xbox
    CHAR* strFontFile = "Font.xpr";
    switch( XGetLanguage() )
    {
        case XC_LANGUAGE_TCHINESE:
            strFontFile = "Font_cht.xpr";
            break;
        case XC_LANGUAGE_JAPANESE:
            strFontFile = "Font_jpn.xpr";
            break;
        case XC_LANGUAGE_KOREAN:
            strFontFile = "Font_kor.xpr";
            break;
    }

    // Create the font
    if( FAILED( hr = m_Font.Create( strFontFile ) ) )
        return hr;

    // Create the online icons font
    if( FAILED( hr = m_OnlineIconsFont.Create( "OnlineIconsFont.xpr" ) ) )
        return hr;

    // Initialize the network stack
    if( FAILED( hr = XBNet_OnlineInit( 0 ) ) )
        return hr;

    // Create the live engine
    if( FAILED( hr = UIXCreateLiveEngine( "d:\\Media\\UIXPlugin.uix",
                                          XGetLanguage(), &m_pLiveEngine ) ) )
        return hr;

    // Create a custom UI plugin for UIX
    CUIPlugin* pUIPlugin = new CUIPlugin();
    pUIPlugin->Initialize( &m_Font );
    m_pLiveEngine->SetUIPlugin( pUIPlugin );

    // Create an audio plugin object
    hr = InitSound();
    if( SUCCEEDED(hr) )
    {
        ITitleAudioPlugin* pAudioPlugin;

        hr = UIXCreateAudioPlugin( m_pXactEngine, m_pSoundBank, &pAudioPlugin );

        if( SUCCEEDED(hr) )
            m_pLiveEngine->SetAudioPlugin( pAudioPlugin );
    }

    // After the plugins are in place, enable the UIX features we want
    m_pLiveEngine->EnableFeature( UIX_LOGON_FEATURE );
    
    m_pLiveEngine->SetProperty( UIX_PROPERTY_DISPLAY_CONNECTION_ERRORS, (DWORD)TRUE );
    
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Reset()
// Desc: Reset back to main menu
//-----------------------------------------------------------------------------
VOID CXBoxSample::Reset()
{
    m_State             = STATE_SELECT_USER_COUNT;
    m_NextState         = STATE_SELECT_USER_COUNT;
    m_bSignedOn         = FALSE;
    m_strMessage[0]     = '\0';
    m_dwMicrophoneState = 0;
    m_dwHeadphoneState  = 0;
    m_dwNumSignOnUsers  = 0;
    m_dwCurrItem        = 0;
    ZeroMemory( &m_Users, sizeof(m_Users) );
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    // If the user in signing on, UIX will inform them of connection errors.
    // Otherwise, let's periodically check for a lost connection.
    if( m_State != STATE_SIGNING_ON )
    {
        // Once per second, check for a lost connection
        static FLOAT m_fLastLinkCheckedTime = 0.0f;
        if( m_fTime - m_fLastLinkCheckedTime > 1.0f )
        {
            m_fLastLinkCheckedTime = m_fTime;

            // Poll status
            if( 0 == XNetGetEthernetLinkStatus() )
            {
                Reset();
                wcscpy( m_strMessage, L"This Xbox has lost its online connection" );
                m_State = STATE_ERROR;
            }
        }
    }

    // Pass input to the UIX engine. We do this every frame, even when UIX is
    // not active, so that when a UIX feature starts up it can distinguish
    // between a button that was just pressed and a button that has been pressed
    // down for a while.
    for( DWORD i = 0; i < XGetPortCount(); i++ )
    {
        if( g_Gamepads[i].hDevice )
            m_pLiveEngine->SetInput( i, &g_InputStates[i] );
        else
            m_pLiveEngine->SetInput( i, NULL );
    }

    // Let the UIX engine do work and query flags so our app knows what to do.
    // This should be called every frame, even when UIX is dormant.
    m_pLiveEngine->DoWork( &m_dwLiveWorkFlags );

    // Check the status bits returned by the engine
    if( m_dwLiveWorkFlags & UIX_DOWORK_NEED_TO_REBOOT )
    {
        // The title should reboot. Note that this function does not return.
        m_pLiveEngine->Reboot(0);
    }

    // If no UIX features are using the input then process the input ourselves.
    if( 0 == ( m_dwLiveWorkFlags & UIX_DOWORK_PROCESSING_INPUT )  )
    {
        switch( m_State )
        {
            case STATE_SELECT_USER_COUNT:
                UpdateStateSelectUserCount();
                break;
            case STATE_SIGNING_ON:
                UpdateStateSigningOn();
                break;
            case STATE_SIGNED_ON:
                UpdateStateSignedOn();
                break;
            case STATE_ERROR:
                UpdateStateError();
                break;
        }
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: UpdateStateSelectUserCount()
// Desc: Select the number of users to sign in
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateSelectUserCount()
{
    switch( m_DefaultGamepad.Event )
    {
        case XBGAMEPAD_START:
        case XBGAMEPAD_A:
            switch( m_dwCurrItem )
            {
                case 0:  m_dwNumSignOnUsers = 1; break;
                case 1:  m_dwNumSignOnUsers = 2; break;
                case 2:  m_dwNumSignOnUsers = 4; break;
            }

            if( FAILED( BeginSignOn() ) )
            {
                Reset();
                wsprintfW( m_strMessage, L"Could not begin Sign On process!"  );
                m_State = STATE_ERROR;
            }
            else
            {
                m_State = STATE_SIGNING_ON;
            }
            break;

        case XBGAMEPAD_DPAD_UP:
            // If we're at the top of the displayed list, wrap
            if( m_dwCurrItem == 0 )
                m_dwCurrItem = MAX_SIGNON_USERS_MENU - 1;
            else
                m_dwCurrItem--;
            break;

        case XBGAMEPAD_DPAD_DOWN:
            // If we're at the bottom of the displayed list, wrap
            if( m_dwCurrItem == MAX_SIGNON_USERS_MENU - 1 )
                m_dwCurrItem = 0;
            else
                m_dwCurrItem++;
            break;
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateStateSigningOn()
// Desc: Continue the sign on process
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateSigningOn()
{
    if( m_dwLiveWorkFlags & UIX_DOWORK_FEATURE_EXIT )
    {
        // The authentication process has completed and the title
        // should check the exit code
        UIX_EXIT_INFO ExitInfo;
        m_pLiveEngine->GetExitInfo( &ExitInfo );

        switch( ExitInfo.ExitCode )
        {
            case UIX_EXIT_LOGON_SUCCESSFUL:
            {
                // Authentication successful
                // A title should next verify that the requested services
                // are available. A title may decide to degrade functionality
                // if some services are unavailable.
                XONLINE_SERVICE_INFO ServiceInfo;
                HRESULT hr = XOnlineGetServiceInfo( XONLINE_MATCHMAKING_SERVICE, &ServiceInfo );
                if( FAILED(hr) )
                {
                    OutputDebugStringA( "Matchmaking service not available.\r\n" );
                }

                // Get the initial states for the headphone and
                // microphone devices
                m_dwMicrophoneState = XGetDevices( XDEVICE_TYPE_VOICE_MICROPHONE );
                m_dwHeadphoneState  = XGetDevices( XDEVICE_TYPE_VOICE_HEADPHONE );
                XONLINE_USER UserAccounts[XONLINE_MAX_STORED_ONLINE_USERS];
                DWORD dwNumUsers;
                XOnlineGetUsers( UserAccounts, &dwNumUsers );
                XONLINE_USER* pUsers = XOnlineGetLogonUsers();
                for( DWORD i = 0; i < XONLINE_MAX_LOGON_USERS; i++ )
                {
                    m_Users[i].bSignedOn   = pUsers[i].xuid.qwUserID != 0;
                    m_Users[i].dwUserFlags = pUsers[i].xuid.dwUserFlags;
                    m_Users[i].bVoice      = FALSE;
                    m_Users[i].bGuest      = XOnlineIsUserGuest(  pUsers[i].xuid.dwUserFlags );
                    m_Users[i].iPane       = UIX_INVALID_VALUE;

                    if( m_Users[i].bSignedOn )
                    {
                        swprintf( m_Users[i].strGamertag, L"%S", pUsers[i].szGamertag );

                        if( !m_Users[i].bGuest )
                        {
                            DWORD dwState = XONLINE_FRIENDSTATE_FLAG_ONLINE;
                            m_Users[i].bVoice = XOnlineIsUserVoiceAllowed( m_Users[i].dwUserFlags ) &&
                                                                           ( m_dwMicrophoneState & (1 << i) ) &&
                                                                           ( m_dwHeadphoneState  & (1 << i) );

                            if( m_Users[i].bVoice )
                                dwState |= XONLINE_FRIENDSTATE_FLAG_VOICE;

                            SetPlayerOnlineState( i, dwState );
                        }
                    }
                    else
                        m_Users[i].strGamertag[0] = '\0';
                }

                // Fill in the pane that each signed on user was bound to by
                // UIX during sign on. The pPaneToControllersMapping array is
                // indexed by "pane" and contains a controller index.
                DWORD* pdwPaneToControllersMapping = (DWORD*)(ExitInfo.pExitData);

                for( DWORD i = 0; i < XONLINE_MAX_LOGON_USERS; i++ )
                {
                    if( pdwPaneToControllersMapping[i] != UIX_INVALID_VALUE )
                    {
                        m_Users[ pdwPaneToControllersMapping[i] ].iPane = i;
                    }
                }

                m_State = STATE_SIGNED_ON;

                break;
            }

            case UIX_EXIT_LOGON_FAILED:
                Reset();
                wsprintfW( m_strMessage, L"Authentication Failed with Error 0x%x", ExitInfo.hr  );
                m_State = STATE_ERROR;
                break;

            case UIX_EXIT_LOGON_USER_EXIT:
                // User aborted authentication
                Reset(); // Back to main menu
                break;
        }
    }
}




//-----------------------------------------------------------------------------
// Name: UpdateStateSignedOn()
// Desc: Check device states and continue UIX processing for signed on users
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateSignedOn()
{
    // Check for Live connection errors
    if( m_dwLiveWorkFlags & UIX_DOWORK_FEATURE_EXIT )
    {
        UIX_EXIT_INFO ExitInfo;
        m_pLiveEngine->GetExitInfo( &ExitInfo );
        if( ExitInfo.ExitCode == UIX_EXIT_LOGON_FAILED )
        {
            Reset();
            return;
        }
    }

    switch( m_DefaultGamepad.Event )
    {
        case XBGAMEPAD_X:
        {
            for( DWORD i = 0; i < XONLINE_MAX_LOGON_USERS; i++ )
            {
                if( m_Users[i].bSignedOn && !m_Users[i].bGuest )
                    SetPlayerOnlineState( i, 0 );
            }
            m_pLiveEngine->LogOff();
            Reset();
            return;
        }
    }

    // Check for microphone/headphone peripheral state changes
    CheckDeviceStates();
}




//-----------------------------------------------------------------------------
// Name: BeginSignOn()
// Desc: Begin the sign on process
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::BeginSignOn()
{
    // Start the logon feature
    UIX_LOGON_PARAMS LogonParams;
    ZeroMemory( &LogonParams, sizeof(LogonParams) );
    LogonParams.StructSize         = sizeof(LogonParams);
    LogonParams.LogonUserCount     = m_dwNumSignOnUsers;
    LogonParams.LogonServiceIDs[0] = XONLINE_MATCHMAKING_SERVICE;

    return m_pLiveEngine->StartFeature( UIX_LOGON_FEATURE, &LogonParams );
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
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET, 0x000a0a6a, 1.0f, 0L );

    switch( m_State )
    {
        case STATE_SELECT_USER_COUNT:
            RenderSelectUserCount( m_dwCurrItem );
            break;

        case STATE_SIGNING_ON:
            RenderAuthentication();
            break;

        case STATE_SIGNED_ON:
        {
            for( DWORD i = 0; i < XONLINE_MAX_LOGON_USERS; ++i )
            {
                RenderSignedOn( i, m_Users[i].iPane,
                                m_Users[i].strGamertag, m_Users[i].bSignedOn,
                                m_Users[i].bVoice, m_Users[i].bGuest );
            }
            break;
        }

        case STATE_ERROR:
            RenderMessage( m_strMessage );
            break;

        default:
            assert( FALSE );
            break;
    }

    // If active, render the UIX stuff
    if( m_dwLiveWorkFlags & UIX_DOWORK_NEED_TO_RENDER )
    {
        m_pLiveEngine->Render( m_pBackBuffer );
    }
    else
    {
        // Draw the app title
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 48, 36, 0xffffffff, L"UIXPlugin" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: UpdateStateError()
// Desc: An error occurred
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateStateError()
{
    switch( m_DefaultGamepad.Event )
    {
        case XBGAMEPAD_START:
        case XBGAMEPAD_A:
            m_State = m_NextState;
            break;
    }
}




//-----------------------------------------------------------------------------
// Name: SetPlayerOnlineState()
// Desc: Broadcast updated User state for the world
//-----------------------------------------------------------------------------
VOID CXBoxSample::SetPlayerOnlineState( DWORD dwUserIndex, DWORD dwState )
{
    HRESULT hr;
    
    hr = m_pLiveEngine->NotificationSetState( dwUserIndex, dwState,
                                             XNKID(), 0, NULL );

    // Normally NotificationSetState will always succeed, however if the user
    // has been logged out (due to network problems or a duplicate logon for
    // instance) and the title has not yet been notified of this, then this
    // function can fail. Such failures can be safely ignored.
    (VOID)hr; // avoid compiler warning
}




//-----------------------------------------------------------------------------
// Name: CheckDeviceStates()
// Desc: Check for voice peripheral state changes and update online state
//       This function is called once per frame as soon the title is online
//       This function is called once per frame as soon the title is online
// Be sure to set other flags, such as XONLINE_FRIENDSTATE_FLAG_JOINABLE
// and XONLINE_FRIENDSTATE_FLAG_PLAYING as appropriate.
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
            if( m_Users[i].bSignedOn && !m_Users[i].bGuest &&
                XOnlineIsUserVoiceAllowed( m_Users[i].dwUserFlags ) )
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
                        dwState           = XONLINE_FRIENDSTATE_FLAG_ONLINE;
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
                        dwState           = XONLINE_FRIENDSTATE_FLAG_ONLINE |
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
// Name: InitSound()
// Desc: Initialize XACT support for UIX
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::InitSound()
{
    XACT_RUNTIME_PARAMETERS Params;
    VOID*   pSoundBankData;
    VOID*   pWaveBankData;
    DWORD   dwWaveBankSize;
    DWORD   dwSoundBankSize;
    HRESULT hr;

    // Read the wave bank and the sound bank media files into memory
    if( FAILED( XBUtil_LoadFile( strWaveBankFile, &pWaveBankData, &dwWaveBankSize ) ) )
        return E_FAIL;

    if( FAILED( XBUtil_LoadFile( strSoundBankFile, &pSoundBankData, &dwSoundBankSize ) ) )
        return E_FAIL;

    // Create the Xact engine
    ZeroMemory( &Params, sizeof(Params) );
    Params.dwMaxConcurrentStreams = 16;
    Params.dwMax2DHwVoices        = 40;
    Params.dwMax3DHwVoices        = 10;

    hr = XACTEngineCreate( &Params, &m_pXactEngine );
    if( FAILED(hr) )
        return hr;

    // Register the wave bank
    hr = m_pXactEngine->RegisterWaveBank( pWaveBankData, dwWaveBankSize,
                                          &m_pWaveBank );
    if( FAILED(hr) )
    {
        m_pXactEngine->Release();
        m_pXactEngine = NULL;
        return hr;
    }

    // Register the sound bank
    hr = m_pXactEngine->CreateSoundBank( pSoundBankData, dwSoundBankSize, &m_pSoundBank );
    if( FAILED(hr) )
    {
        m_pXactEngine->Release();
        m_pXactEngine = NULL;
        return hr;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// UI Constants
//-----------------------------------------------------------------------------
const D3DCOLOR COLOR_HIGHLIGHT = 0xffffff00; // Yellow
const D3DCOLOR COLOR_GREEN     = 0xff00ff00; // Green
const D3DCOLOR COLOR_NORMAL    = 0xffffffff; // White
const FLOAT    REGION_WIDTH    = 250.0f;
const FLOAT    REGION_HEIGHT   = 164.0f;
const FLOAT    REGION_X        =  64.0f;
const FLOAT    REGION_Y        =  80.0f;
const FLOAT    REGION_GAP      =   8.0f;




//-----------------------------------------------------------------------------
// Name: RenderSelectUserCount()
// Desc: Render user count selection screen
//-----------------------------------------------------------------------------
VOID CXBoxSample::RenderSelectUserCount( DWORD dwCurrItem )
{
    assert( dwCurrItem <= XONLINE_MAX_LOGON_USERS );

    m_Font.DrawText( 320, 100, COLOR_NORMAL, L"Select number of players",
                     XBFONT_CENTER_X );

    FLOAT fYtop   = 160.0f;
    FLOAT fYdelta =  30.0f;
    static const WCHAR* strMenu[MAX_SIGNON_USERS_MENU] = 
    {
        L"One player",  L"Two players", L"Four players"
    };

    // Show list of user accounts
    for( DWORD i = 0; i < sizeof(strMenu)/sizeof(strMenu[0]); ++i )
    {
        DWORD dwColor = ( dwCurrItem == i ) ? COLOR_HIGHLIGHT : COLOR_NORMAL;

        m_Font.DrawText( 280, fYtop + (fYdelta * i), dwColor, strMenu[i] );
    }

    // Show selected item with little triangle
    m_Font.DrawText( 240.0f, fYtop + (fYdelta * dwCurrItem ), 
                     COLOR_GREEN, GLYPH_RIGHT_TICK );

}




//-----------------------------------------------------------------------------
// Name: RenderAuthentication()
// Desc: Display UIX authentication screen
//-----------------------------------------------------------------------------
VOID CXBoxSample::RenderAuthentication( )
{
}




//-----------------------------------------------------------------------------
// Name: RenderSignedOn()
// Desc: Display "signed on" screen
//-----------------------------------------------------------------------------
VOID CXBoxSample::RenderSignedOn( DWORD dwControllerIndex, DWORD iPane,
                                  const WCHAR* strGamerTag, BOOL bSignedOn, 
                                  BOOL bVoice, BOOL bGuest )
{
    // Get the region position
    FLOAT x1 = 0.0f, y1 = 0.0f;
    switch( iPane )
    {
        case 0: 
            x1 = REGION_X; 
            y1 = REGION_Y;
            break;
        case 1:
            x1 = REGION_X + REGION_WIDTH +  REGION_GAP; 
            y1 = REGION_Y;
            break;
        case 2:
            x1 = REGION_X; 
            y1 = REGION_Y + REGION_HEIGHT + REGION_GAP; 
            break;
        case 3:
            x1 = REGION_X + REGION_WIDTH  + REGION_GAP; 
            y1 = REGION_Y + REGION_HEIGHT + REGION_GAP;
            break;
        default:
            return;
    }
    
    // Render the gamertag
    FLOAT x2 = x1 + REGION_WIDTH;
    FLOAT y2 = y1 + REGION_HEIGHT;
    m_Font.DrawText( (x1+x2)/2, y1, COLOR_NORMAL, strGamerTag, XBFONT_CENTER_X );

    // Render the region border
    D3DDevice::SetTexture( 0, NULL );
    D3DDevice::SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
    D3DDevice::SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TFACTOR );
    D3DDevice::SetRenderState( D3DRS_TEXTUREFACTOR, COLOR_NORMAL );
    D3DDevice::SetVertexShader( D3DFVF_XYZRHW );
    D3DDevice::Begin( D3DPT_LINELOOP );
    D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, x1-0.5f, y1-0.5f, 1.0f, 1.0f );
    D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, x2-0.5f, y1-0.5f, 1.0f, 1.0f );
    D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, x2-0.5f, y2-0.5f, 1.0f, 1.0f );
    D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, x1-0.5f, y2-0.5f, 1.0f, 1.0f );
    D3DDevice::End();
    
    // Draw signed on text
    WCHAR* strSignedOn;
    if( FALSE == bSignedOn )
        strSignedOn = L"Not Signed On";
    else if( bGuest )
        strSignedOn = L"Guest Signed On";
    else
        strSignedOn = L"Signed On";
    m_Font.DrawText( (x1+x2)/2, (y1+y2)/2, COLOR_NORMAL, strSignedOn, XBFONT_CENTER_X );

    if( bSignedOn )
        m_Font.DrawText( (x1+x2)/2, y2-30.0f, COLOR_NORMAL,
                         L"Press " GLYPH_X_BUTTON L" to Sign Off", XBFONT_CENTER_X );
    
    // Draw voice icon
    const WCHAR strIcon[2] = { ONLINEICON_PLAYER_VOICE, 0 };
    if( bVoice )
        m_OnlineIconsFont.DrawText( (x1+x2)/2 - 20.0f, (y1+y2)/2 - 35.0f,
                                    COLOR_NORMAL, strIcon );
}




//-----------------------------------------------------------------------------
// Name: RenderMessage()
// Desc: Display message
//-----------------------------------------------------------------------------
VOID CXBoxSample::RenderMessage( const WCHAR* strMessage )
{
    m_Font.DrawText( 320, 200, COLOR_NORMAL, strMessage, XBFONT_CENTER_X );
    m_Font.DrawText( 320, 300, COLOR_NORMAL, L"Press " GLYPH_A_BUTTON L" to continue", 
                     XBFONT_CENTER_X );
}





