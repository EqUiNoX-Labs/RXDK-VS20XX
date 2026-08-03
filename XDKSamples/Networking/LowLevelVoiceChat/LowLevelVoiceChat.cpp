//-----------------------------------------------------------------------------
// File: LowLevelVoiceChat.cpp
//
// Desc: Demonstrates how a title can use the low-level voice chat components
//       (Microphone, Encoder, Voice Queue, Decoder, Headphone) to build a
//       drop-in custom voice engine.
//
// Hist: 04.29.02 - New for June02 XDK release 
//       01.14.03 - Renamed from SimpleVoice sample to LowLevelVoiceChat
//       03.19.03 - Updated to support reliable communications
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "LowLevelVoiceChat.h"
#include "dsstdfx.h"
#include <xvoice.h>




// Port 1000 gives 0 extra port overhead on the wire
// Ports 1001-1255 give 2 bytes overhead on the wire
// All other ports give 4 bytes overhead on the wire
const WORD  BROADCAST_PORT    = 1001;  // could be any port
const WORD  DIRECT_PORT       = 1000;  // any port other than BROADCAST_PORT
const WORD  RELIABLE_PORT     = 1002;  // Port for low-bandwidth reliable msgs

// Define some preset voice mask configurations
struct VOICE_MASK_PRESET
{
    WCHAR*      strLabel;
    XVOICE_MASK mask;
};

const VOICE_MASK_PRESET g_VoiceMasks[] =
{
    { L"None",        XVOICE_MASK_NONE },
    { L"Anonymous",   XVOICE_MASK_ANONYMOUS   },
    { L"Cartoon",     XVOICE_MASK_CARTOON },
    { L"Big Guy",     XVOICE_MASK_BIGGUY },
    { L"Child",       XVOICE_MASK_CHILD },
    { L"Robot",       XVOICE_MASK_ROBOT },
    { L"Dark Master", XVOICE_MASK_DARKMASTER },
    { L"Whisper",     XVOICE_MASK_WHISPER },
};
const DWORD NUM_VOICEMASKS = sizeof( g_VoiceMasks ) / sizeof( g_VoiceMasks[0] );



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
: 
    CXBApplication(),
    m_hLogFile    ( INVALID_HANDLE_VALUE ),
    m_Font        (),
    m_OnlineIconsFont(),
    m_Help        (),
    m_State       ( STATE_MENU ),
    m_LastState   ( STATE_MENU ),
    m_CurrItem    ( 0 ),
    m_GameNames   (),
    m_Games       (),
    m_Players     (),
    m_strError    (),
    m_strStatus   (),
    m_bIsOnline           ( FALSE ),
    m_bXnetStarted        ( FALSE ),
    m_bIsHost             ( FALSE ),
    m_bIsSessionRegistered( FALSE ),
    m_bHaveLocalAddress   ( FALSE ),
    m_bDrawDebugInfo      ( FALSE ),
    m_xnHostKeyID         (),
    m_xnHostKeyExchange   (),
    m_xnTitleAddress      (),
    m_inHostAddr          (),
    m_BroadSock           (),
    m_DirectSock          (),
    m_strGameName         (),
    m_strPlayerName       (),
    m_strHostName         (),
    m_Nonce               (),
    m_dwVoiceMask         ( 0 ),
    m_bLoopback           ( FALSE ),
    m_bVoiceThroughSpeakers( FALSE ),
    m_msgVoiceData        ( MSG_VOICEDATA )
{
    srand( GetTickCount() ); // for generating game/player names

    m_VoiceTimer.Start();

    Init();
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize device-dependant objects
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    // Create a font
    if( FAILED( m_Font.Create( "Font.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create a font for the online icons
    if( FAILED( m_OnlineIconsFont.Create( "OnlineIconsFont.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Initialize the help system
    if( FAILED( m_Help.Create( "Gamepad.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    LPDIRECTSOUND8 m_pDSound;
    DirectSoundCreate( NULL, &m_pDSound, NULL );

    DSEFFECTIMAGELOC dsImageLoc;
    dsImageLoc.dwI3DL2ReverbIndex = GraphI3DL2_I3DL2Reverb;
    dsImageLoc.dwCrosstalkIndex = GraphXTalk_XTalk;

    LPDSEFFECTIMAGEDESC pdsImageDesc;
    XAudioDownloadEffectsImage( "dsstdfx", &dsImageLoc, XAUDIO_DOWNLOADFX_XBESECTION, &pdsImageDesc );

    // Configure the voice manager.  Note that if we change dwVoicePacketTime,
    // then we have to update the COMPRESSED_PACKET_SIZE in Player.h, so
    // that we send the right amount of data over the network.
    VOICE_MANAGER_CONFIG VoiceConfig;
    VoiceConfig.dwVoicePacketTime       = 20;       // 20ms
    VoiceConfig.dwMaxRemotePlayers      = 12;       // 12 remote players
    VoiceConfig.dwFirstSRCEffectIndex   = GraphVoice_Voice_0;
    VoiceConfig.pEffectImageDesc        = pdsImageDesc;
    VoiceConfig.pDSound                 = m_pDSound;
    VoiceConfig.dwMaxStoredPackets      = 20;

    VoiceConfig.pCallbackContext        = this;
    VoiceConfig.pfnCommunicatorCallback = CommunicatorCallback;
    VoiceConfig.pfnVoiceDataCallback    = VoiceDataCallback;

    g_VoiceManager.Initialize( &VoiceConfig );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: XuidFromAddressAndPort
// Desc: Constructs a fake XUID from an IN_ADDR and controller port on the
//          box at that IN_ADDR.
//-----------------------------------------------------------------------------
XUID XuidFromAddressAndPort( IN_ADDR addr, WORD wPort )
{
    XUID x;
    x.qwUserID = ( ULONGLONG(addr.S_un.S_addr) << 32 ) | wPort;
    x.dwUserFlags = 0;

    return x;
}


//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    // TCR Robustness Against Lost Link
    // Check network status periodically
    if( !m_LinkStatusTimer.IsRunning() ||
        m_LinkStatusTimer.GetElapsedSeconds() > CHECK_LINK_STATUS )
    {
        m_LinkStatusTimer.StartZero();
        DWORD dwStatus = XNetGetEthernetLinkStatus();
        m_bIsOnline = ( dwStatus & XNET_ETHERNET_LINK_ACTIVE ) != 0;
    }

    Event ev = GetEvent();
    if( ev == EV_BUTTON_BLACK )
        m_bDrawDebugInfo = !m_bDrawDebugInfo;


    // Allow voice to be processed
    g_VoiceManager.ProcessVoice();

    switch( m_State )
    {
        case STATE_MENU:            FrameMoveMenu( ev );        break;
        case STATE_GAME:            FrameMoveGame( ev );        break;
        case STATE_HELP:            FrameMoveHelp( ev );        break;
        case STATE_SELECT_NAME:     FrameMoveSelectName( ev );  break;
        case STATE_START_NEW_GAME:  FrameMoveStartGame( ev );   break;
        case STATE_GAME_SEARCH:     FrameMoveGameSearch( ev );  break;
        case STATE_SELECT_GAME:     FrameMoveSelectGame( ev );  break;
        case STATE_REQUEST_CONNECT:
        case STATE_REQUEST_JOIN:    FrameMoveRequestJoin( ev ); break;
        case STATE_ERROR:           FrameMoveError( ev );       break;
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
        case STATE_MENU:            RenderMenu();        break;
        case STATE_GAME:            RenderGame();        break;
        case STATE_HELP:            RenderHelp();        break;
        case STATE_SELECT_NAME:     RenderSelectName();  break;
        case STATE_START_NEW_GAME:  RenderStartGame();   break;
        case STATE_GAME_SEARCH:     RenderGameSearch();  break;
        case STATE_SELECT_GAME:     RenderSelectGame();  break;
        case STATE_REQUEST_CONNECT:
        case STATE_REQUEST_JOIN:    RenderRequestJoin(); break;
        case STATE_ERROR:           RenderError();       break;
    }

    if( m_bDrawDebugInfo )
        g_VoiceManager.RenderDebugInfo( &m_Font );

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: GetEvent()
// Desc: Return the state of the controller
//-----------------------------------------------------------------------------
CXBoxSample::Event CXBoxSample::GetEvent()
{
    // "A" or "Start"
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_A ] ||
        m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_START )
    {
        return EV_BUTTON_A;
    }

    // "B" or "back"
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_B ] ||
        m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_BACK )
        return EV_BUTTON_B;

    // "X"
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_X ] )
        return EV_BUTTON_X;

    // "Y"
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_Y ] )
        return EV_BUTTON_Y;

    // "Black"
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_BLACK ] )
        return EV_BUTTON_BLACK;

    // "White"
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_WHITE ] )
        return EV_BUTTON_WHITE;

    // Triggers
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_LEFT_TRIGGER ] )
        return EV_TRIGGER_LEFT;
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_RIGHT_TRIGGER ] )
        return EV_TRIGGER_RIGHT;

    // Movement
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
        return EV_UP;
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
        return EV_DOWN;
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
        return EV_LEFT;
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
        return EV_RIGHT;

    return EV_NULL;
}




//-----------------------------------------------------------------------------
// Name: FrameMoveMenu()
// Desc: Animate menu
//-----------------------------------------------------------------------------
VOID CXBoxSample::FrameMoveMenu( Event ev )
{
    switch( ev )
    {
        case EV_BUTTON_A:

            // Prepare networking
            switch( InitXNet() )
            {
                case Success:
                    break;
                case NotConnected:
                    m_State = STATE_ERROR;
                    lstrcpynW( m_strError, L"This Xbox is not connected to\n"
                                           L"a hub or another Xbox",
                                           MAX_ERROR_STR );
                    return;
                case InitFailed:
                    m_State = STATE_ERROR;
                    lstrcpynW( m_strError, L"Failure initializing network\n"
                                           L"connections", 
                                           MAX_ERROR_STR );
                    return;
                default: assert( FALSE ); break;
            }

            switch( m_CurrItem )
            {
                case MAIN_MENU_START_GAME:
                {   

                    assert( !m_bIsSessionRegistered );

                    // Create the session key ID and exchange key
                    INT iKeyCreated = XNetCreateKey( &m_xnHostKeyID, 
                                                     &m_xnHostKeyExchange );

                    // Register the session
                    INT iKeyRegistered = XNetRegisterKey( &m_xnHostKeyID, 
                                                          &m_xnHostKeyExchange );
                    if( iKeyCreated != NO_ERROR || iKeyRegistered != NO_ERROR )
                    {
                        m_State = STATE_ERROR;
                        lstrcpynW( m_strError, L"Unable to start game session",
                                   MAX_ERROR_STR );

                        if( iKeyCreated != NO_ERROR )
                            LogXNetError( "XNetCreateKey", iKeyCreated );
                        if( iKeyRegistered != NO_ERROR )
                            LogXNetError( "XNetRegisterKey", iKeyRegistered );

                        break;
                    }

                    m_bIsSessionRegistered = TRUE;

                    // We're the host
                    m_bIsHost = TRUE;

                    // TCR Naming of Multiple Game Sessions for System Link Play
                    // Build a list of potential game names
                    assert( m_GameNames.empty() );
                    for( DWORD i = 0; i < MAX_GAME_NAMES; ++i )
                    {
                        WCHAR strGameName[ MAX_GAME_NAME ];
                        GenRandom( strGameName, MAX_GAME_NAME );
                        m_GameNames.push_back( strGameName );
                    }

                    // Start at the top of the list
                    m_CurrItem = 0;

                    m_State = STATE_SELECT_NAME;

                    break;
                }
                case MAIN_MENU_JOIN_GAME:
                    // Begin searching for games on the network
                    SendFindGame();
                    m_GameSearchTimer.StartZero();
                    m_State = STATE_GAME_SEARCH;


                    break;
            }
            break;

        case EV_UP:
            if( m_CurrItem == 0 )
                m_CurrItem = MAIN_MENU_MAX - 1;
            else
                --m_CurrItem;
            break;
        case EV_DOWN:
            if( m_CurrItem == MAIN_MENU_MAX - 1 )
                m_CurrItem = 0;
            else
                ++m_CurrItem;
            break;
        case EV_BUTTON_WHITE:
            m_LastState = m_State;
            m_State = STATE_HELP;
            break;
    }
}




//-----------------------------------------------------------------------------
// Name: FrameMoveGame()
// Desc: Animate game
//-----------------------------------------------------------------------------
VOID CXBoxSample::FrameMoveGame( Event ev )
{
    if( !m_bIsOnline )
    {
        // TCR Robustness Against Lost Link
        m_State = STATE_ERROR;
        lstrcpynW( m_strError, L"This Xbox has lost its System Link connection",
                               MAX_ERROR_STR );
        return;
    }

    // Handle net messages
    if( ProcessBroadcastMessage() )
        return;
    if( ProcessDirectMessage() )
        return;
    if( ProcessReliableMessage() )
        return;

    // Send keep-alives
    if( m_HeartbeatTimer.GetElapsedSeconds() > PLAYER_HEARTBEAT )
    {
        Heartbeat();
        m_HeartbeatTimer.StartZero();
    }

    // Make sure we send voice data at an appropriate rate
    if( m_VoiceTimer.GetElapsedSeconds() > VOICE_PACKET_INTERVAL )
    {
        SendVoiceDataToAll();
    }

    // Handle other players dropping
    if( ProcessPlayerDropouts() )
        return;

    switch( ev )
    {
        case EV_BUTTON_A:
            switch( m_CurrItem )
            {
                case GAME_MENU_WAVE:
                    Wave();
                    break;
                case GAME_MENU_LOOPBACK:
                    m_bLoopback = !m_bLoopback;
                    for( BYTE i = 0; i < XGetPortCount(); i++ )
                    {
                        g_VoiceManager.SetLoopback( i, m_bLoopback );
                    }
                    break;
                case GAME_MENU_VOICETHROUGHSPEAKERS:
                    m_bVoiceThroughSpeakers = !m_bVoiceThroughSpeakers;
                    g_VoiceManager.SetVoiceThroughSpeakers( m_bVoiceThroughSpeakers );
                    break;
                case GAME_MENU_LEAVE_GAME:
                    Init();
                    break;
            }
            break;
        case EV_BUTTON_X:
            OUTPUT_DEBUG_STRING("Sleeping\n");
            Sleep( rand() % 500 + 500 );
            break;
        case EV_BUTTON_Y:
            // For each box...
            for( PlayerList::iterator i = m_Players.begin(); i != m_Players.end(); ++i )
            {
                // And each port on that box...
                for( BYTE j = 0; j < XGetPortCount(); j++ )
                {
                    XUID xuid = XuidFromAddressAndPort( i->inAddr, j );
                    // Then for each local port
                    for( BYTE k = 0; k < XGetPortCount(); k++ )
                    {
                        SOCKADDR_IN addrDest;
                        addrDest.sin_family = AF_INET;
                        addrDest.sin_addr   = i->inAddr;
                        addrDest.sin_port   = htons( DIRECT_PORT );
                        if( g_VoiceManager.IsPlayerMuted( xuid, k ) )
                        {
                            // UnMute them
                            g_VoiceManager.UnMutePlayer( xuid, k );
                            SendVoiceInfo( VOICEINFO_REMOVEREMOTEMUTE, k, &(*i), j );
                        }
                        else
                        {
                            // Mute them
                            g_VoiceManager.MutePlayer( xuid, k );
                            SendVoiceInfo( VOICEINFO_ADDREMOTEMUTE, k, &(*i), j );
                        }
                    }
                }
            }
            break;
        case EV_UP:
            if( m_CurrItem == 0 )
                m_CurrItem = GAME_MENU_MAX - 1;
            else
                --m_CurrItem;
            break;
        case EV_DOWN:
            if( m_CurrItem == GAME_MENU_MAX - 1 )
                m_CurrItem = 0;
            else
                ++m_CurrItem;
            break;
        case EV_BUTTON_WHITE:
            m_LastState = m_State;
            m_State = STATE_HELP;
            break;
        case EV_TRIGGER_LEFT:
            m_dwVoiceMask = ( m_dwVoiceMask + NUM_VOICEMASKS - 1 ) % NUM_VOICEMASKS;
            for( BYTE i = 0; i < XGetPortCount(); i++ )
            {
                g_VoiceManager.SetVoiceMask( i, g_VoiceMasks[ m_dwVoiceMask ].mask );
            }
            break;
        case EV_TRIGGER_RIGHT:
            m_dwVoiceMask = ( m_dwVoiceMask + 1 ) % NUM_VOICEMASKS;
            for( BYTE i = 0; i < XGetPortCount(); i++ )
            {
                g_VoiceManager.SetVoiceMask( i, g_VoiceMasks[ m_dwVoiceMask ].mask );
            }
            break;
    }
}




//-----------------------------------------------------------------------------
// Name: FrameMoveHelp()
// Desc: Animate help
//-----------------------------------------------------------------------------
VOID CXBoxSample::FrameMoveHelp( Event ev )
{
    // Handle net messages
    if( ProcessBroadcastMessage() )
        return;
    if( ProcessDirectMessage() )
        return;
    if( ProcessReliableMessage() )
        return;

    if( ev != EV_NULL )
        m_State = m_LastState;
}




//-----------------------------------------------------------------------------
// Name: FrameMoveSelectName()
// Desc: Animate game name selection
//-----------------------------------------------------------------------------
VOID CXBoxSample::FrameMoveSelectName( Event ev )
{
    // TCR Naming of Multiple Game Sessions for System Link Play
    // Allow the player to cancel out of game name selection
    if( ev == EV_BUTTON_B || !m_bIsOnline )
    {
        Init();
        return;
    }

    switch( ev )
    {
        case EV_BUTTON_A:

            // Use the selected name
            lstrcpyW( m_strGameName, m_GameNames[ m_CurrItem ].c_str() );

            // Destroy the name list; we don't need it anymore
            m_GameNames.clear();

            // Set the default game item to "wave"
            m_CurrItem = 0;

            // Display when game begins
            lstrcpynW( m_strStatus, L"Game started", MAX_STATUS_STR );

            // If we have the local address, begin the game.
            // Otherwise, acquire the local address.
            if( m_bHaveLocalAddress )
            {
                m_State = STATE_GAME;
                m_HeartbeatTimer.StartZero();
                StartVoice();

                // Start listening for client connections
                m_ReliableSock.Listen();
            }
            else
                m_State = STATE_START_NEW_GAME;
            break;

        case EV_UP:
            if( m_CurrItem == 0 )
                m_CurrItem = m_GameNames.size() - 1;
            else
                --m_CurrItem;
            break;

        case EV_DOWN:
            if( m_CurrItem == m_GameNames.size() - 1 )
                m_CurrItem = 0;
            else
                ++m_CurrItem;
            break;

        case EV_BUTTON_B:
            Init();
            break;

        case EV_BUTTON_WHITE:
            m_LastState = m_State;
            m_State = STATE_HELP;
            break;
    }
}




//-----------------------------------------------------------------------------
// Name: FrameMoveStartGame()
// Desc: Animate start game
//-----------------------------------------------------------------------------
VOID CXBoxSample::FrameMoveStartGame( Event ev )
{
    // Allow the player to cancel out of game startup
    if( ev == EV_BUTTON_B || !m_bIsOnline )
    {
        Init();
        return;
    }

    // Asynchronous local address acquisition
    DWORD dwStatus = XNetGetTitleXnAddr( &m_xnTitleAddress );
    assert( dwStatus != XNET_GET_XNADDR_NONE );

    // If we've retrieved the local address, we're done
    m_bHaveLocalAddress = ( dwStatus != XNET_GET_XNADDR_PENDING );

    // When startup is complete, enter the game
    if( m_bHaveLocalAddress )
    {   
        m_HeartbeatTimer.StartZero();
        m_State = STATE_GAME;

        StartVoice();

        // Start listening for client connections
        m_ReliableSock.Listen();
    }
}




//-----------------------------------------------------------------------------
// Name: FrameMoveGameSearch()
// Desc: Animate game search
//-----------------------------------------------------------------------------
VOID CXBoxSample::FrameMoveGameSearch( Event ev )
{
    // Allow the player to cancel out of game search
    if( ev == EV_BUTTON_B || !m_bIsOnline )
    {
        Init();
        return;
    }

    // See if any games have replied
    if( ProcessBroadcastMessage() )
        return;

    // We search for up to GAME_SEARCH_TIME seconds. If the game 
    // search is complete, display the list of available games. If no games
    // were found, display an error message
    if( m_GameSearchTimer.GetElapsedSeconds() > GAME_SEARCH_TIME )
    {
        m_GameSearchTimer.Stop();
        if( m_Games.empty() )
        {
            m_State = STATE_ERROR;
            lstrcpynW( m_strError, L"No games available", MAX_ERROR_STR );
        }
        else if( m_Games.size() == 1 )
        {
            // One game; join automatically
            InitiateJoin( 0 );
        }
        else // at least two games
        {
            // at least two games; allow player selection
            m_State = STATE_SELECT_GAME;
            m_CurrItem = 0;
        }
    }
}




//-----------------------------------------------------------------------------
// Name: FrameMoveSelectGame()
// Desc: Animate game selection
//-----------------------------------------------------------------------------
VOID CXBoxSample::FrameMoveSelectGame( Event ev )
{
    if( !m_bIsOnline )
    {
        // TCR Robustness Against Lost Link
        m_State = STATE_ERROR;
        lstrcpynW( m_strError, L"This Xbox has lost its System Link connection",
                               MAX_ERROR_STR );
        return;
    }

    switch( ev )
    {
        case EV_BUTTON_A:
            InitiateJoin( m_CurrItem );
            break;

        case EV_UP:
            if( m_CurrItem == 0 )
                m_CurrItem = m_Games.size() - 1;
            else
                --m_CurrItem;
            break;

        case EV_DOWN:
            if( m_CurrItem == m_Games.size() - 1 )
                m_CurrItem = 0;
            else
                ++m_CurrItem;
            break;

        case EV_BUTTON_B:
            Init();
            break;

        case EV_BUTTON_WHITE:
            m_LastState = m_State;
            m_State = STATE_HELP;
            break;
    }
}




//-----------------------------------------------------------------------------
// Name: FrameMoveRequestJoin()
// Desc: Animate join request
//-----------------------------------------------------------------------------
VOID CXBoxSample::FrameMoveRequestJoin( Event ev )
{
    // Allow the player to cancel out of join request
    if( ev == EV_BUTTON_B || !m_bIsOnline )
    {
        Init();
        return;
    }

    // We wait for up to GAME_JOIN_TIME seconds. If the game didn't
    // respond, display an error message
    if( m_GameJoinTimer.GetElapsedSeconds() > GAME_JOIN_TIME )
    {
        m_GameJoinTimer.Stop();
        m_State = STATE_ERROR;
        lstrcpynW( m_strError, L"Game did not respond", MAX_ERROR_STR );
    }

    // First, we have to wait to see if our connection has completed
    if( m_State == STATE_REQUEST_CONNECT )
    {
        BOOL bWrite;
        BOOL bError;
        m_ReliableSock.Select( NULL, &bWrite, &bError );
        if( bError )
        {
            m_State = STATE_ERROR;
            lstrcpynW( m_strError, L"Game did not respond", MAX_ERROR_STR );
        }
        else if( bWrite )
        {
            // Request join approval from the game and await a response
            SOCKADDR_IN sa;
            sa.sin_family = AF_INET;
            sa.sin_addr   = m_inHostAddr;
            sa.sin_port   = htons( DIRECT_PORT );
            SendJoinGame( sa );
            m_State = STATE_REQUEST_JOIN;
        }
    }
    else
    {
        // See if the host has replied
        ProcessReliableMessage();
    }
}




//-----------------------------------------------------------------------------
// Name: FrameMoveError()
// Desc: Animate error message
//-----------------------------------------------------------------------------
VOID CXBoxSample::FrameMoveError( Event ev )
{
    // Handle net messages
    if( ProcessBroadcastMessage() )
        return;
    if( ProcessDirectMessage() )
        return;

    // Any button exits
    if( ev != EV_NULL )
        Init();
}





//-----------------------------------------------------------------------------
// Name: RenderMenu()
// Desc: Display menu
//-----------------------------------------------------------------------------
VOID CXBoxSample::RenderMenu()
{
    RenderHeader();

    const WCHAR* const strMenu[] =
    {
        L"Start New Game",
        L"Join Existing Game",
    };

    FLOAT fYtop = 200.0f;
    FLOAT fYdelta = 50.0f;

    // System Link Play Menu Option
    for( DWORD i = 0; i < MAIN_MENU_MAX; ++i )
    {
        DWORD dwColor = ( m_CurrItem == i && m_bIsOnline ) ? COLOR_HIGHLIGHT : 
                                                             COLOR_NORMAL;
        m_Font.DrawText( 260, fYtop + (fYdelta * i), dwColor, strMenu[i] );
    }

    // Show selected item with little triangle
    if( m_bIsOnline )
        m_Font.DrawText( 220.0f, fYtop + ( fYdelta * m_CurrItem ), 0xffffffff, GLYPH_RIGHT_TICK );

    m_Font.DrawText( 320, 400, COLOR_NORMAL, m_bIsOnline ? 
                     L"System Link Connected" :
                     L"System Link NOT Connected", XBFONT_CENTER_X );
}




//-----------------------------------------------------------------------------
// Name: RenderGame()
// Desc: Display game
//-----------------------------------------------------------------------------
VOID CXBoxSample::RenderGame()
{
    RenderHeader();

    // Game name and player name
    WCHAR strGameInfo[ 32 + MAX_GAME_NAME + MAX_PLAYER_NAME ];
    wsprintfW( strGameInfo, L"Game name: %.*s\nYour name: %.*s", 
               MAX_GAME_NAME, m_strGameName, MAX_PLAYER_NAME, m_strPlayerName );
    m_Font.DrawText( 70, 110, COLOR_GREEN, strGameInfo );

    // Number of players and current status
    m_Font.DrawText( 70, 158, COLOR_GREEN, m_strStatus );

    // Game options menu
    const WCHAR* const strMenu[] =
    {
        L"Wave To Other Players",
        L"Toggle Loopback",
        L"Voice Through Speakers",
        L"Leave Game",
    };

    FLOAT fYtop = 240.0f;
    FLOAT fYdelta = 40.0f;

    // Show menu
    for( DWORD i = 0; i < GAME_MENU_MAX; ++i )
    {
        DWORD dwColor = ( m_CurrItem == i ) ? COLOR_HIGHLIGHT : COLOR_NORMAL;
        m_Font.DrawText( 110, fYtop + (fYdelta * i), dwColor, strMenu[i] );
        if( i == GAME_MENU_LOOPBACK )
            m_Font.DrawText( dwColor, m_bLoopback ? L": on" : L": off" );
        else if( i == GAME_MENU_VOICETHROUGHSPEAKERS )
            m_Font.DrawText( dwColor, m_bVoiceThroughSpeakers ? L": on" : L": off" );
    }

    // Show selected item with little triangle
    m_Font.DrawText( 70.0f, fYtop + ( fYdelta * m_CurrItem ), 0xffffffff, GLYPH_RIGHT_TICK );

    m_Font.DrawText( 110, 400, COLOR_NORMAL, L"Current Voice Mask:" );
    m_Font.DrawText( 330, 400, COLOR_NORMAL, g_VoiceMasks[ m_dwVoiceMask ].strLabel );

    // Render the list of players, including who has voice communicators
    FLOAT fX = 400.0f;
    FLOAT fY = 110.0f;
    m_Font.DrawText( fX, fY, COLOR_GREEN, L"Remote Players:" );
    fY += 30.0f;
    for( PlayerList::iterator i = m_Players.begin(); i != m_Players.end(); ++i )
    {
        // We only have one logical player per machine, even though there
        // may be several communicators plugged in on that machine.  We'll
        // check for each communicator, and add the port number in parentheses
        // after the player name
        BOOL bHadVoice = FALSE;
        for( WORD j = 0; j < XGetPortCount(); j++ )
        {
            XUID xuid = XuidFromAddressAndPort( i->inAddr, j );
            if( i->bHasVoice & ( 1 << j ) )
            {
                bHadVoice = TRUE;

                // See if the player is talking.  If we have them muted,
                // or they have us muted, then they should appear as if
                // they're not talking.
                BOOL bIsTalking     = g_VoiceManager.IsPlayerTalking( xuid );
                BOOL bIsMuted       = FALSE;
                BOOL bIsRemoteMuted = FALSE;
                for( BYTE k = 0; k < XGetPortCount(); k++ )
                {
                    if( g_VoiceManager.IsPlayerMuted( xuid, k ) )
                        bIsMuted = TRUE;
                    if( g_VoiceManager.IsPlayerRemoteMuted( xuid, k ) )
                        bIsRemoteMuted = TRUE;
                }
                if( bIsMuted || bIsRemoteMuted )
                    bIsTalking = FALSE;

                // Draw the voice icon, muted or not.
                const WCHAR VOICE_ICON = 6;
                const WCHAR MUTED_ICON = 7;
                WCHAR strIcon[2] = { bIsMuted ? MUTED_ICON : VOICE_ICON, 0 };
                m_OnlineIconsFont.DrawText( fX - 30.0f, fY, 0xffffffff, strIcon );

                WCHAR strName[ MAX_PLAYER_NAME + 4 ];
                swprintf( strName, L"%s (%d)", i->strPlayerName, j );

                DWORD dwColor = bIsTalking ? COLOR_NORMAL : COLOR_GREEN;
                m_Font.DrawText( fX, fY, dwColor, strName );
                fY += 30.0f;
            }
        }

        // No one had voice on this machine, so just display the 
        // regular old player name
        if( !bHadVoice )
        {
            m_Font.DrawText( fX, fY, COLOR_GREEN, i->strPlayerName );
            fY += 30.0f;
        }
    }
}




//-----------------------------------------------------------------------------
// Name: RenderHelp()
// Desc: Display help
//-----------------------------------------------------------------------------
VOID CXBoxSample::RenderHelp()
{
    XBHELP_CALLOUT HelpCallouts[] =
    {
        { XBHELP_WHITE_BUTTON,  XBHELP_PLACEMENT_1, L"Display\nhelp" },
        { XBHELP_A_BUTTON,      XBHELP_PLACEMENT_1, L"Select menu\nitem" },
        { XBHELP_B_BUTTON,      XBHELP_PLACEMENT_1, L"Cancel" },
        { XBHELP_X_BUTTON,      XBHELP_PLACEMENT_2, L"Random\nsleep" },
        { XBHELP_BLACK_BUTTON,  XBHELP_PLACEMENT_2, L"Toggle\ndebug info" },
        { XBHELP_DPAD,          XBHELP_PLACEMENT_1, L"Menu navigation" },
        { XBHELP_MISC_CALLOUT,  XBHELP_PLACEMENT_2, L"Triggers:\nChange Voice Mask" },
        { XBHELP_Y_BUTTON,      XBHELP_PLACEMENT_1, L"Mute players" },
    };
    m_Help.Render( &m_Font, HelpCallouts, sizeof( HelpCallouts ) / sizeof( HelpCallouts[0] ) );
}




//-----------------------------------------------------------------------------
// Name: RenderSelectName()
// Desc: Display game name selection
//-----------------------------------------------------------------------------
VOID CXBoxSample::RenderSelectName()
{
    assert( !m_GameNames.empty() );

    RenderHeader();

    m_Font.DrawText( 320, 110, 0xffffffff, L"Select a game name\n\n"
                                           L"Press " GLYPH_B_BUTTON L" to cancel", 
                     XBFONT_CENTER_X );

    FLOAT fYtop = 220.0f;
    FLOAT fYdelta = 30.0f;

    // Show list of game names
    for( DWORD i = 0; i < m_GameNames.size(); ++i )
    {
        DWORD dwColor = ( m_CurrItem == i ) ? COLOR_HIGHLIGHT : COLOR_NORMAL;
        m_Font.DrawText( 280, fYtop + (fYdelta * i), dwColor, 
                         m_GameNames[i].c_str() );
    }

    // Show selected item with little triangle
    m_Font.DrawText( 240.0f, fYtop + ( fYdelta * m_CurrItem ), 0xffffffff, GLYPH_RIGHT_TICK );
}




//-----------------------------------------------------------------------------
// Name: RenderStartGame()
// Desc: Display game startup sequence
//-----------------------------------------------------------------------------
VOID CXBoxSample::RenderStartGame()
{
    RenderHeader();
    m_Font.DrawText( 320, 240, 0xffffffff, L"Starting Game\n\n"
                                           L"Press " GLYPH_B_BUTTON L" to cancel",
                     XBFONT_CENTER_X | XBFONT_CENTER_Y );
}




//-----------------------------------------------------------------------------
// Name: RenderGameSearch()
// Desc: Display game search sequence
//-----------------------------------------------------------------------------
VOID CXBoxSample::RenderGameSearch()
{
    RenderHeader();
    m_Font.DrawText( 320, 240, 0xffffffff, L"Searching For Active Games\n\n"
                                           L"Press " GLYPH_B_BUTTON L" to cancel",
                     XBFONT_CENTER_X | XBFONT_CENTER_Y );
}




//-----------------------------------------------------------------------------
// Name: RenderSelectGame()
// Desc: Display list of available games
//-----------------------------------------------------------------------------
VOID CXBoxSample::RenderSelectGame()
{
    assert( !m_Games.empty() );

    RenderHeader();

    m_Font.DrawText( 320, 110, 0xffffffff, L"Select game to join\n\n"
                                           L"Press " GLYPH_B_BUTTON L" to cancel", 
                     XBFONT_CENTER_X );

    FLOAT fYtop = 220.0f;
    FLOAT fYdelta = 50.0f;

    // Show list of games
    for( DWORD i = 0; i < m_Games.size(); ++i )
    {
        DWORD dwColor = ( m_CurrItem == i ) ? COLOR_HIGHLIGHT : COLOR_NORMAL;
        WCHAR strGameInfo[ 64 + MAX_GAME_NAME + MAX_PLAYER_NAME ];
        wsprintfW( strGameInfo, L"\"%.*s\" hosted by \"%.*s\"; players: %d",
                   MAX_GAME_NAME, m_Games[i].strGameName,
                   MAX_PLAYER_NAME, m_Games[i].strHostName,
                   INT(m_Games[i].byNumPlayers) );

        // Denote full games
        if( m_Games[i].byNumPlayers == MAX_PLAYERS )
            lstrcatW( strGameInfo, L" (full)" );

        m_Font.DrawText( 140, fYtop + (fYdelta * i), dwColor, strGameInfo );
    }

    // Show selected item with little triangle
    m_Font.DrawText( 100.0f, fYtop + ( fYdelta * m_CurrItem ), 0xffffffff, GLYPH_RIGHT_TICK );
}




//-----------------------------------------------------------------------------
// Name: RenderRequestJoin()
// Desc: Display join request sequence
//-----------------------------------------------------------------------------
VOID CXBoxSample::RenderRequestJoin()
{
    RenderHeader();
    m_Font.DrawText( 320, 240, 0xffffffff, L"Joining game", 
                     XBFONT_CENTER_X | XBFONT_CENTER_Y );
}




//-----------------------------------------------------------------------------
// Name: RenderError()
// Desc: Display error message
//-----------------------------------------------------------------------------
VOID CXBoxSample::RenderError()
{
    RenderHeader();
    m_Font.DrawText( 320, 200, 0xffffffff, m_strError, XBFONT_CENTER_X );
    m_Font.DrawText( 320, 260, 0xffffffff, L"Press " GLYPH_A_BUTTON L" to continue", 
                     XBFONT_CENTER_X );
}




//-----------------------------------------------------------------------------
// Name: RenderHeader()
// Desc: Display standard text
//-----------------------------------------------------------------------------
VOID CXBoxSample::RenderHeader()
{
    WCHAR strName[32];
    lstrcpyW( strName, L"LowLevelVoiceChat" );
    if( m_bIsHost )
        lstrcatW( strName, L" (host)" );

    m_Font.DrawText(  64, 50, 0xffffffff, strName );
    m_Font.DrawText( 450, 50, 0xffffff00, m_strFrameRate );
}




//-----------------------------------------------------------------------------
// Name: InitiateJoin()
// Desc: Send a join request to the specified game
//-----------------------------------------------------------------------------
VOID CXBoxSample::InitiateJoin( DWORD iCurrGame )
{
    // Determine which game the player wants to join
    GameInfo gameInfo = m_Games[ iCurrGame ];

    // Establish a session with the host game
    INT iResult = XNetRegisterKey( &gameInfo.xnHostKeyID, 
                                   &gameInfo.xnHostKey );
    assert( iResult == NO_ERROR );
    if( iResult == NO_ERROR )
    {
        assert( m_bIsSessionRegistered == FALSE );
        m_bIsSessionRegistered = TRUE;

        // Save the key ID because we need to unregister it
        // Note that we don't need the key itself once it's been registered.
        CopyMemory( &m_xnHostKeyID, &gameInfo.xnHostKeyID, sizeof( XNKID ) );

        // Save the game and player name of the host
        lstrcpynW( m_strGameName, gameInfo.strGameName, MAX_GAME_NAME );
        lstrcpynW( m_strHostName, gameInfo.strHostName, MAX_PLAYER_NAME );

        // Convert the XNADDR of the host to the INADDR we'll use to
        // join the game
        iResult = XNetXnAddrToInAddr( &gameInfo.xnHostAddr,
                                      &m_xnHostKeyID, &m_inHostAddr );
        assert( iResult == NO_ERROR );

        // Open up a reliable socket to the host - this will be used
        // for low-bandwidth communications, such as join requests, 
        // communicator status, etc.  We have to wait for the connection
        // to complete before sending out join request
        SOCKADDR_IN saHost;
        saHost.sin_family = AF_INET;
        saHost.sin_addr   = m_inHostAddr;
        saHost.sin_port   = htons( RELIABLE_PORT );
        m_ReliableSock.Connect( &saHost );

        m_GameJoinTimer.StartZero();
        m_State = STATE_REQUEST_CONNECT;
    }
    else
    {
        m_State = STATE_ERROR;
        lstrcpynW( m_strError, L"Unable to establish session with game",
                   MAX_ERROR_STR );
        LogXNetError( "XNetRegisterKey", iResult );
    }

    // Don't need the game list anymore
    DestroyGameList();
}




//-----------------------------------------------------------------------------
// Name: Wave()
// Desc: Wave to other players in the game
//-----------------------------------------------------------------------------
VOID CXBoxSample::Wave()
{
    // Indicate that you waved
    lstrcpynW( m_strStatus, L"You waved", MAX_STATUS_STR );

    SendWaveToAll();
}




//-----------------------------------------------------------------------------
// Name: StartVoice()
// Desc: Initialize voice
//-----------------------------------------------------------------------------
VOID CXBoxSample::StartVoice()
{
    g_VoiceManager.EnterChatSession();
    m_msgVoiceData.GetMsgVoiceData().wVoicePackets = 0;
    m_VoiceTimer.StartZero();

    // Indicate that you sent voice
    lstrcpynW( m_strStatus, L"Voice chat enabled!", MAX_STATUS_STR );
}




//-----------------------------------------------------------------------------
// Name: Heartbeat()
// Desc: Send heartbeat to players in the game
//-----------------------------------------------------------------------------
VOID CXBoxSample::Heartbeat()
{
    // Send a "heartbeat" message to each of the other players in the game
    // to let them know we're alive
    SendHeartbeatToAll();
}




//-----------------------------------------------------------------------------
// Name: Init()
// Desc: Teardown any active games and player lists and return to main menu
//       Unregisters any active sessions.
//-----------------------------------------------------------------------------
VOID CXBoxSample::Init()
{
    // Disable voice
    if( g_VoiceManager.IsInChatSession() )
    {
        for( BYTE i = 0; i < XGetPortCount(); i++ )
        {
            SendVoiceInfo( VOICEINFO_REMOVECHATTER, i, NULL, 0 );
        }

        g_VoiceManager.LeaveChatSession();
    }


    // Don't clear m_bXnetStarted. We don't need to reinitialize Xnet once
    // it's been started.

    // Don't clear m_bHaveLocalAddress or m_xnTitleAddress. We don't need to 
    // reacquire the address once we have it.

    m_State     = STATE_MENU;
    m_LastState = STATE_MENU;
    m_CurrItem  = 0;

    m_GameNames.clear();
    DestroyGameList();
    DestroyPlayerList();

    *m_strError = 0;
    *m_strStatus = 0;

    m_LinkStatusTimer.Stop();
    m_GameSearchTimer.Stop();
    m_GameJoinTimer.Stop();
    m_HeartbeatTimer.Stop();

    m_bIsHost = FALSE;

 

    // Unregister the game session key
    if( m_bIsSessionRegistered )
    {
        INT iResult = XNetUnregisterKey( &m_xnHostKeyID );
        assert( iResult == NO_ERROR );
        (VOID)iResult;
        m_bIsSessionRegistered = FALSE;
    }

    // Obliterate old keys and XNADDR
    ZeroMemory( &m_xnHostKeyID,       sizeof( XNKID ) );
    ZeroMemory( &m_xnHostKeyExchange, sizeof( XNKEY ) );
    m_inHostAddr.s_addr = 0;

    // Close down the sockets
    m_BroadSock.Close();
    m_DirectSock.Close();
    m_ReliableSock.Close();

    for( SocketList::iterator it = m_ClientSockets.begin(); 
         it < m_ClientSockets.end();
         ++it )
    {
        closesocket( it->sock );
    }
    m_ClientSockets.clear();

    *m_strGameName   = 0;
    *m_strPlayerName = 0;
    *m_strHostName   = 0;

    ZeroMemory( &m_Nonce, sizeof(m_Nonce) );

    // Generate random player name
    // A real Xbox game would get this information from the player
    GenRandom( m_strPlayerName, MAX_PLAYER_NAME );
}




//-----------------------------------------------------------------------------
// Name: InitXNet()
// Desc: Initialize the network stack. Returns FALSE if Xbox is not connected.
//-----------------------------------------------------------------------------
CXBoxSample::InitStatus CXBoxSample::InitXNet()
{
    if( !m_bIsOnline )
        return NotConnected;

    // Only need to initialize network stack one time
    if( !m_bXnetStarted )
    {
        // Initialize the network stack
        INT iResult = XNetStartup( NULL );
        if( iResult != NO_ERROR )
        {
            LogXNetError( "XNetStartup", iResult );
            return InitFailed;
        }

        // Standard WinSock startup
        WSADATA WsaData;
        iResult = WSAStartup( MAKEWORD(2,2), &WsaData );
        if( iResult != NO_ERROR )
        {
            LogXNetError( "WSAStartup", iResult );
            return InitFailed;
        }

        m_bXnetStarted = TRUE;
    }

    // The broadcast socket is a non-blocking socket on port BROADCAST_PORT.
    // All broadcast messages are automatically always encrypted.
    BOOL bSuccess = m_BroadSock.Open( CXBSocket::Type_UDP );
    if( !bSuccess )
    {
        LogXNetError( "Broadcast socket open", WSAGetLastError() );
        return InitFailed;
    }

    SOCKADDR_IN broadAddr;
    broadAddr.sin_family      = AF_INET;
    broadAddr.sin_addr.s_addr = INADDR_ANY;
    broadAddr.sin_port        = htons( BROADCAST_PORT );
    INT iResult = m_BroadSock.Bind( &broadAddr );
    assert( iResult != SOCKET_ERROR );
    DWORD dwNonBlocking = 1;
    iResult = m_BroadSock.IoCtlSocket( FIONBIO, &dwNonBlocking );
    assert( iResult != SOCKET_ERROR );
    BOOL bBroadcast = TRUE;
    iResult = m_BroadSock.SetSockOpt( SOL_SOCKET, SO_BROADCAST,
                                      &bBroadcast, sizeof(bBroadcast) );
    assert( iResult != SOCKET_ERROR );

    // The direct socket is a non-blocking socket on port DIRECT_PORT.
    // Sockets are encrypted by default, but can have encryption disabled
    // as an optimization for non-secure messaging
    bSuccess = m_DirectSock.Open( CXBSocket::Type_VDP );
    if( !bSuccess )
    {
        LogXNetError( "Direct socket open", WSAGetLastError() );
        return InitFailed;
    }

    SOCKADDR_IN directAddr;
    directAddr.sin_family      = AF_INET;
    directAddr.sin_addr.s_addr = INADDR_ANY;
    directAddr.sin_port        = htons( DIRECT_PORT );
    iResult = m_DirectSock.Bind( &directAddr );
    assert( iResult != SOCKET_ERROR );
    iResult = m_DirectSock.IoCtlSocket( FIONBIO, &dwNonBlocking );
    assert( iResult != SOCKET_ERROR );

    // Create a reliable socket to use for low-bandwidth messages
    // that need to be sent reliably.  Clients will use this
    // socket to connect to the host.  The host uses this socket to
    // listen for incoming client connections.
    bSuccess = m_ReliableSock.Open( CXBSocket::Type_TCP );
    assert( bSuccess );
    SOCKADDR_IN reliableAddr;
    reliableAddr.sin_family      = AF_INET;
    reliableAddr.sin_addr.s_addr = INADDR_ANY;
    reliableAddr.sin_port        = htons( RELIABLE_PORT );
    iResult = m_ReliableSock.Bind( &reliableAddr );
    assert( iResult != SOCKET_ERROR );
    iResult = m_ReliableSock.IoCtlSocket( FIONBIO, &dwNonBlocking );
    assert( iResult != SOCKET_ERROR );

    // Note that this sample does not call either WSACleanup() or 
    // XNetCleanup(). These functions should be called by your game to
    // free system resources when the player is no longer online but
    // is still playing the game (e.g. switched to single-player mode).

    return Success;
}




//-----------------------------------------------------------------------------
// Name: SendFindGame()
// Desc: Broadcast a MSG_FIND_GAME from our client to any available host
//-----------------------------------------------------------------------------
VOID CXBoxSample::SendFindGame()
{
    assert( !m_bIsHost );
    Message msgFindGame( MSG_FIND_GAME );
    MsgFindGame& msg = msgFindGame.GetFindGame();

    // Generate a nonce (random bytes). When a potential host responds with
    // information about a game, he must respond via a broadcast message
    // since a secure session hasn't been established. The broadcast message
    // will contain the same nonce so we can verify that message is really
    // for us. If we receive a broadcast "found game" message with a different
    // nonce, we ignore it, because it was broadcast to a different client
    // than us.
    INT iResult = XNetRandom( (BYTE*)(&msg.nonce), sizeof(msg.nonce) );
    assert( iResult == NO_ERROR );
    (VOID)iResult;

    // Save the nonce for comparison later
    CopyMemory( &m_Nonce, &msg.nonce, sizeof(msg.nonce) );

    SOCKADDR_IN saBroad;
    saBroad.sin_family      = AF_INET;
    saBroad.sin_addr.s_addr = INADDR_BROADCAST;
    saBroad.sin_port        = htons( BROADCAST_PORT );
    INT nBytes = m_BroadSock.SendTo( &msgFindGame, msgFindGame.GetSize(),
                                     &saBroad );
    // This assert was removed because Send no longer is guaranteed to always work
    // If the security association times out, the number of bytes returned will 
    // NOT be equal to the size of the message.  A good thing to do here would
    // be to drop the player
    //   assert( nBytes == SOCKET_ERROR || nBytes == msgFindGame.GetSize() );
    (VOID)nBytes;
}




//-----------------------------------------------------------------------------
// Name: SendGameFound()
// Desc: Broadcast a MSG_GAME_FOUND from our host to the world
//-----------------------------------------------------------------------------
VOID CXBoxSample::SendGameFound( const Nonce& nonceClient )
{
    assert( m_bIsHost );
    Message msgGameFound( MSG_GAME_FOUND );
    MsgGameFound& msg = msgGameFound.GetGameFound();

    // Resend the nonce that we received from the client so he can verify
    // that this message is really for him
    CopyMemory( &msg.nonce, &nonceClient, sizeof(nonceClient) );

    // Send information about the session that we're hosting
    CopyMemory( &msg.xnHostKeyID, &m_xnHostKeyID,       sizeof(XNKID) );
    CopyMemory( &msg.xnHostKey,   &m_xnHostKeyExchange, sizeof(XNKEY) );
    CopyMemory( &msg.xnHostAddr,  &m_xnTitleAddress,    sizeof(XNADDR) );

    // Send the current information about the game
    msg.byNumPlayers = BYTE( m_Players.size() + 1 );
    lstrcpynW( msg.strGameName, m_strGameName, MAX_GAME_NAME );
    lstrcpynW( msg.strHostName, m_strPlayerName, MAX_PLAYER_NAME );

    // We don't have the XNADDR of the requesting client, so we
    // can't send this message directly back. Instead, we broadcast the
    // message to everybody on the net. The requesting client can
    // check the nonce to verify that the response is really for them.
    // Broadcast messages are automatically encrypted.

    SOCKADDR_IN saBroad;
    saBroad.sin_family      = AF_INET;
    saBroad.sin_addr.s_addr = INADDR_BROADCAST;
    saBroad.sin_port        = htons( BROADCAST_PORT );
    INT nBytes = m_BroadSock.SendTo( &msgGameFound, msgGameFound.GetSize(),
                                     &saBroad );
    // This assert was removed because Send no longer is guaranteed to always work
    // If the security association times out, the number of bytes returned will 
    // NOT be equal to the size of the message.  A good thing to do here would
    // be to drop the player
    
    //assert( nBytes == SOCKET_ERROR || nBytes == msgGameFound.GetSize() );
    (VOID)nBytes;
}




//-----------------------------------------------------------------------------
// Name: SendJoinGame()
// Desc: Issue a MSG_JOIN_GAME from our client to the game host
//-----------------------------------------------------------------------------
VOID CXBoxSample::SendJoinGame( const SOCKADDR_IN& saGameHost )
{
    assert( !m_bIsHost );
    Message msgJoinGame( MSG_JOIN_GAME );
    MsgJoinGame& msg = msgJoinGame.GetJoinGame();

    // Include our player name
    lstrcpynW( msg.strPlayerName, m_strPlayerName, MAX_PLAYER_NAME );

    // Send join game message reliably to the host
    INT nBytes = SendMessage( &msgJoinGame, TRUE, &saGameHost );

    // This assert was removed because Send no longer is guaranteed to always work
    // If the security association times out, the number of bytes returned will 
    // NOT be equal to the size of the message.  A good thing to do here would
    // be to drop the player
    
    //assert( nBytes == SOCKET_ERROR || nBytes == msgJoinGame.GetSize() );
    (VOID)nBytes;
}




//-----------------------------------------------------------------------------
// Name: SendJoinApproved()
// Desc: Issue a MSG_JOIN_APPROVED from our host to the requesting client.
//-----------------------------------------------------------------------------
VOID CXBoxSample::SendJoinApproved( const SOCKADDR_IN& saClient )
{
    assert( m_bIsHost );
    Message msgJoinApproved( MSG_JOIN_APPROVED );
    MsgJoinApproved& msg = msgJoinApproved.GetJoinApproved();

    // The host is us
    lstrcpynW( msg.strHostName, m_strPlayerName, MAX_PLAYER_NAME );

    // Send the list of all the current players to the new player.
    // We don't send the host player info, since the new player 
    // already has all of the information it needs about the host player.
    msg.byNumPlayers = BYTE( m_Players.size() );
    BYTE j = 0;
    for( PlayerList::const_iterator i = m_Players.begin(); 
         i != m_Players.end(); ++i, ++j )
    {
        PlayerInfo playerInfo = *i;
        CopyMemory( &msg.PlayerList[j].xnAddr, &playerInfo.xnAddr, 
                    sizeof( XNADDR ) );
        lstrcpynW( msg.PlayerList[j].strPlayerName, 
                   playerInfo.strPlayerName, MAX_PLAYER_NAME );
    }

    // Send the join approved message reliably to the client
    INT nBytes = SendMessage( &msgJoinApproved, TRUE, &saClient );

    // This assert was removed because Send no longer is guaranteed to always work
    // If the security association times out, the number of bytes returned will 
    // NOT be equal to the size of the message.  A good thing to do here would
    // be to drop the player
    
    //assert( nBytes == SOCKET_ERROR || nBytes == msgJoinApproved.GetSize() );
    (VOID)nBytes;
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
    INT nBytes = SendMessage( &msgJoinDenied, TRUE, &saClient );

    // This assert was removed because Send no longer is guaranteed to always work
    // If the security association times out, the number of bytes returned will 
    // NOT be equal to the size of the message.  A good thing to do here would
    // be to drop the player
    
    //assert( nBytes == SOCKET_ERROR || nBytes == msgJoinDenied.GetSize() );
    (VOID)nBytes;
}




//-----------------------------------------------------------------------------
// Name: SendPlayerJoinedToAll()
// Desc: Issue a MSG_PLAYER_JOINED from our host to each player in the game
//-----------------------------------------------------------------------------
VOID CXBoxSample::SendPlayerJoinedToAll( const Player& player )
{
    assert( m_bIsHost );
    Message msgPlayerJoined( MSG_PLAYER_JOINED );
    MsgPlayerJoined& msg = msgPlayerJoined.GetPlayerJoined();

    // The payload is the information about the player who just joined
    CopyMemory( &msg.player, &player, sizeof(player) );

    // Send the player joined message reliably to all players in the game
    INT nBytes = SendMessage( &msgPlayerJoined, TRUE );

    // This assert was removed because Send no longer is guaranteed to always work
    // If the security association times out, the number of bytes returned will 
    // NOT be equal to the size of the message.  A good thing to do here would
    // be to drop the player
    
    //assert( nBytes == SOCKET_ERROR || nBytes == msgPlayerJoined.GetSize() );
    (VOID)nBytes;
}




//-----------------------------------------------------------------------------
// Name: SendWaveToAll()
// Desc: Issue a MSG_WAVE from ourself (either a host or player) to every
//       other player
//-----------------------------------------------------------------------------
VOID CXBoxSample::SendWaveToAll()
{
    Message msgWave( MSG_WAVE );

    // Send the WAVE message via VDP to all players in the game
    INT nBytes = SendMessage( &msgWave, FALSE );

    // This assert was removed because Send no longer is guaranteed to always work
    // If the security association times out, the number of bytes returned will 
    // NOT be equal to the size of the message.  A good thing to do here would
    // be to drop the player
    
    //assert( nBytes == SOCKET_ERROR || nBytes == msgWave.GetSize() );
    (VOID)nBytes;
}

//-----------------------------------------------------------------------------
// Name: SendVoiceInfo()
// Desc: Issue a MSG_VOICEINFO from ourself (either a host or player) to 
//          another player
//-----------------------------------------------------------------------------
VOID CXBoxSample::SendVoiceInfo( VOICEINFO          action, 
                                 WORD               wControllerPort, 
                                 PlayerInfo*        pDestPlayer,
                                 WORD               wDestinationPort )
{
    Message msgVoiceInfo( MSG_VOICEINFO );
    MsgVoiceInfo& msg = msgVoiceInfo.GetMsgVoiceInfo();
    msg.action = action;
    msg.wControllerPort = wControllerPort;

    // Since this message may be relayed by the host, we need to identify
    // who we are inside the message.  Sending a name is inefficient - a
    // unique player identifier should be used instead
    wcscpy( msg.strSrcPlayer, m_strPlayerName );

    // Adding/Removing remote mutes requires that we say exactly
    // which person we're talking about.  
    if( action == VOICEINFO_ADDREMOTEMUTE ||
        action == VOICEINFO_REMOVEREMOTEMUTE )
    {
        assert( pDestPlayer != NULL );
        msg.wDestPort = wDestinationPort;
    }

    // Send the message reliably - whether it's sent to all players
    // or just to one specific player is determined by what the
    // caller passed in for pDestPlayer
    INT nBytes;
    if( pDestPlayer )
    {
        // Send the voice info message reliably to the player
        // (note that it may be relayed by the host)
        wcscpy( msg.strDestPlayer, pDestPlayer->strPlayerName );
        SOCKADDR_IN sa;
        sa.sin_family = AF_INET;
        sa.sin_addr   = pDestPlayer->inAddr;
        sa.sin_port   = htons( DIRECT_PORT );
        nBytes = SendMessage( &msgVoiceInfo, TRUE, &sa );
    }
    else
    {
        // Send the voice info message reliably to all players in the game
        // (note that it will definitely be relayed by the host)
        msg.strDestPlayer[0] = L'\0';
        nBytes = SendMessage( &msgVoiceInfo, TRUE );
    }

    // This assert was removed because Send no longer is guaranteed to always work
    // If the security association times out, the number of bytes returned will 
    // NOT be equal to the size of the message.  A good thing to do here would
    // be to drop the player

    //assert( nBytes == SOCKET_ERROR || nBytes == msgVoiceInfo.GetSize() );
    (VOID)nBytes;
}



//-----------------------------------------------------------------------------
// Name: SendVoiceDataToAll
// Desc: Sends accumulated voice data out to other players in the game
//-----------------------------------------------------------------------------
VOID CXBoxSample::SendVoiceDataToAll()
{
    // Make sure we actually have data to send...
    if( m_msgVoiceData.GetMsgVoiceData().wVoicePackets > 0 )
    {
        // Send voice data via VDP directly to all other players
        INT nBytes = SendMessage( &m_msgVoiceData, FALSE );

        // This assert was removed because Send no longer is guaranteed to always work
        // If the security association times out, the number of bytes returned will 
        // NOT be equal to the size of the message.  A good thing to do here would
        // be to drop the player

        //assert( nBytes == SOCKET_ERROR || nBytes == m_msgVoiceData.GetSize() );
        (VOID)nBytes;
    }

    m_msgVoiceData.GetMsgVoiceData().wVoicePackets = 0;
    m_VoiceTimer.StartZero();
}



//-----------------------------------------------------------------------------
// Name: static VoiceDataCallback
// Desc: Called whenever voice data is produced by the voice system
//-----------------------------------------------------------------------------
VOID CXBoxSample::VoiceDataCallback( DWORD dwPort, DWORD dwSize, VOID* pvData, VOID* pContext )
{
    CXBoxSample* pThis = (CXBoxSample*)pContext;

    pThis->OnVoiceData( dwPort, dwSize, pvData );
}



//-----------------------------------------------------------------------------
// Name: OnVoiceData
// Desc: Sends out a packet of voice data to all other players
//-----------------------------------------------------------------------------
VOID CXBoxSample::OnVoiceData( DWORD dwControllerPort, DWORD dwSize, VOID* pvData )
{
    MsgVoiceData& msg = m_msgVoiceData.GetMsgVoiceData();

    msg.VoicePackets[ msg.wVoicePackets ].wControllerPort = (WORD)dwControllerPort;
    memcpy( msg.VoicePackets[ msg.wVoicePackets ].byData, pvData, dwSize );
    msg.wVoicePackets++;

    // We've set up our voice timer such that it SHOULD cause us to send out 
    // our buffered voice data before the buffer fills up.  However, things
    // like framerate glitches, etc., could cause us to fill up before we 
    // notice the timer has fired.
    if( msg.wVoicePackets == MAX_VOICE_PER_PACKET )
    {
        SendVoiceDataToAll();
    }
}



//-----------------------------------------------------------------------------
// Name: static CommunicatorCallback
// Desc: Called whenever a voice communicator event occurs
//-----------------------------------------------------------------------------
VOID CXBoxSample::CommunicatorCallback( DWORD dwPort, VOICE_COMMUNICATOR_EVENT event, VOID* pContext )
{
    CXBoxSample* pThis = (CXBoxSample*)pContext;

    pThis->OnCommunicatorEvent( dwPort, event );
}



//-----------------------------------------------------------------------------
// Name: OnCommunicatorEvent
// Desc: Updates our state for communicator insertions and removals
//-----------------------------------------------------------------------------
VOID CXBoxSample::OnCommunicatorEvent( DWORD dwControllerPort, VOICE_COMMUNICATOR_EVENT event )
{
    switch( event )
    {
    case VOICE_COMMUNICATOR_INSERTED:
        if( g_VoiceManager.IsInChatSession() )
        {
            SendVoiceInfo( VOICEINFO_ADDCHATTER, (WORD)dwControllerPort, NULL, 0 );
        }
        g_VoiceManager.SetVoiceMask( dwControllerPort, g_VoiceMasks[ m_dwVoiceMask ].mask );
        g_VoiceManager.SetLoopback( dwControllerPort, m_bLoopback );
        break;
    case VOICE_COMMUNICATOR_REMOVED:
        if( g_VoiceManager.IsInChatSession() )
        {
            SendVoiceInfo( VOICEINFO_REMOVECHATTER, (WORD)dwControllerPort, NULL, 0 );
        }
        break;
    }
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
    INT nBytes = SendMessage( &msgHeartbeat, FALSE );

    // This assert was removed because Send no longer is guaranteed to always work
    // If the security association times out, the number of bytes returned will 
    // NOT be equal to the size of the message.  A good thing to do here would
    // be to drop the player
    // assert( nBytes == SOCKET_ERROR || nBytes == msgHeartbeat.GetSize() );
    (VOID)nBytes;
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
                              BOOL bReliable, 
                              const SOCKADDR_IN* psaDest )
{
    INT nBytes = 0;

    if( bReliable )
    {
        // Reliable messages are sent via TCP connection, and so should
        // only be used for low-bandwidth, low-frequency messages.
        // Consider actively throttling the amount of data sent reliably
        if( m_bIsHost )
        {
            // The host can send directly to one client or directly
            // to all clients, because he's got a reliable connection
            // with every client
            if( psaDest )
            {
                // Send directly to the player over the reliable socket
                MatchInAddr matchInAddr( psaDest->sin_addr );
                SocketList::iterator it = std::find_if( m_ClientSockets.begin(), m_ClientSockets.end(), matchInAddr );
                assert( it != m_ClientSockets.end() );

                nBytes += send( it->sock, (char*)pMsg, pMsg->GetSize(), 0 );
            }
            else
            {
                // We're the host, so we can just iterate over each of our
                // reliable sockets and send them the message directly
                for( SocketList::iterator it = m_ClientSockets.begin();
                    it < m_ClientSockets.end();
                    ++it )
                {
                    if( it->bAccepted )
                    {
                        nBytes += send( it->sock, (char*)pMsg, pMsg->GetSize(), 0 );
                    }
                }
            }
        }
        else
        {
            // We're a client - our only reliable connection is to the
            // host, so send him the message and he will forward it if
            // necessary (see ProcessVoiceInfo)
            if( m_ReliableSock.IsOpen() )
                nBytes += m_ReliableSock.Send( pMsg, pMsg->GetSize() );
        }
    }
    else
    {
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
            for( PlayerList::iterator it = m_Players.begin(); it != m_Players.end(); ++it )
            {
                SOCKADDR_IN sa;
                sa.sin_family = AF_INET;
                sa.sin_addr   = it->inAddr;
                sa.sin_port   = htons( DIRECT_PORT );
                nBytes += m_DirectSock.SendTo( pMsg, pMsg->GetSize(), &sa );
            }
        }
    }

    return nBytes;
}



//-----------------------------------------------------------------------------
// Name: ProcessBroadcastMessage()
// Desc: Checks to see if any broadcast messages are waiting on the broadcast
//       socket. If a message is waiting, it is routed and processed.
//       If no messages are waiting, the function returns immediately.
//       Returns TRUE if a message was processed.
//-----------------------------------------------------------------------------
BOOL CXBoxSample::ProcessBroadcastMessage()
{
    if( !m_BroadSock.IsOpen() )
        return FALSE;

    // See if a network broadcast message is waiting for us
    Message msg;
    INT iResult = m_BroadSock.Recv( &msg, msg.GetMaxSize() );

    // If message waiting, process it
    if( iResult != SOCKET_ERROR && iResult > 0 )
    {
        // The only messages w/ unencrypted data are VOICEDATA messages,
        // and those should never be sent over broadcast.
        assert( msg.GetUnEncryptedSize() == 0 );

        assert( iResult == msg.GetSize() );
        ProcessMessage( msg );
        return TRUE;
    }
    return FALSE;
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
// Name: ReadFromSocket
// Desc: Attempts to read a message from the specified socket.  Since our
//          reliable sockets are stream-oriented, messages may be received
//          in small pieces (or many received at once), so it's important
//          to carefully parse the appropriate amount of data from the stream.
//          Returns S_OK if message is completely parsed
//          Returns S_FALSE if there is not a complete message
//          Returns failure code on error
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



//-----------------------------------------------------------------------------
// Name: ProcessReliableMessage()
// Desc: First checks to see if any new connections have been attempted, and if
//       we have room, accepts them.  Then, scans all reliable client 
//       connections to see if any have messages pending.
//       If a message is waiting, it is routed and processed.
//       If no messages are waiting, the function returns immediately.
//       Returns TRUE if a message was processed.
//-----------------------------------------------------------------------------
BOOL CXBoxSample::ProcessReliableMessage()
{
    if( !m_ReliableSock.IsOpen() )
        return FALSE;

    if( m_bIsHost )
    {
        // Process any pending socket connections
        for( ; ; )
        {
            ClientSocket cs;
            cs.sock = m_ReliableSock.Accept( &cs.sa );
            if( cs.sock == INVALID_SOCKET )
                break;

            // Initialize the ClientSocket struct - if we don't get a join
            // request on the socket w/in the timeout, we close the connection
            cs.bAccepted = FALSE;
            cs.fTimeout  = 0.0f;

            m_ClientSockets.push_back( cs );
        }

        // Poll each of our clients for messages and timeout
        for( SocketList::iterator it = m_ClientSockets.begin();
            it < m_ClientSockets.end();
            ++it )
        {
            if( !it->bAccepted )
            {
                it->fTimeout += m_fElapsedTime;
                if( it->fTimeout > PLAYER_TIMEOUT / 1000.0f )
                {
                    closesocket( it->sock );
                    m_ClientSockets.erase( it );
                    continue;
                }
            }

            // Try to parse out a message from the socket.  If message was
            // completed, process the message
            HRESULT hr = it->msgPending.Read( it->sock );
            if( FAILED( hr ) )
            {
                // Socket has been disconnected
                MatchInAddr matchInAddr( it->sa.sin_addr );
                PlayerList::iterator p = std::find_if( m_Players.begin(), m_Players.end(), matchInAddr );
                if( p != m_Players.end() )
                    OnPlayerDisconnect( &(*p) );
            }
            else if( S_OK == hr )
            {
                ProcessMessage( it->msgPending.m_msg, SOCKADDR_IN( it->sa ) );
                it->msgPending.Reset();
            }
        }
    }
    else
    {
        HRESULT hr = m_msgPending.Read( m_ReliableSock.GetSocket() );
        if( FAILED( hr ) )
        {
            // Socket has been disconnected
            if( m_Players.size() > 0 )
                OnPlayerDisconnect( &m_Players[0] );
        }
        else if( S_OK == hr )
        {
            SOCKADDR_IN sa;
            sa.sin_family = AF_INET;
            sa.sin_addr   = m_inHostAddr;
            sa.sin_port   = htons( RELIABLE_PORT );
            ProcessMessage( m_msgPending.m_msg, sa );
            m_msgPending.Reset();
        }
    }

    return FALSE;
}




//-----------------------------------------------------------------------------
// Name: ProcessMessage()
// Desc: Routes broadcast messages
//-----------------------------------------------------------------------------
VOID CXBoxSample::ProcessMessage( Message& msg )
{
    // Process the message
    switch( msg.GetId() )
    {
        // From client to host; processed by host
        case MSG_FIND_GAME:  ProcessFindGame( msg.GetFindGame() );   break;

        // From host to client: processed by client
        case MSG_GAME_FOUND: ProcessGameFound( msg.GetGameFound() ); break;

        // Any other message on this port is invalid and we ignore it
        default: assert( FALSE ); break;
    }
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

        // From host to client: processed by client
        case MSG_JOIN_APPROVED: ProcessJoinApproved( msg.GetJoinApproved(), saFrom ); break;
        case MSG_JOIN_DENIED:   ProcessJoinDenied( saFrom ); break;
        case MSG_PLAYER_JOINED: ProcessPlayerJoined( msg.GetPlayerJoined(), saFrom ); break;

        // From player to player: processed by client player
        case MSG_WAVE:          ProcessWave( saFrom ); break;
        case MSG_HEARTBEAT:     ProcessHeartbeat( saFrom ); break;
        case MSG_VOICEINFO:     ProcessVoiceInfo( msg.GetMsgVoiceInfo(), saFrom ); break;
        case MSG_VOICEDATA:     ProcessVoiceData( msg.GetMsgVoiceData(), saFrom ); break;


        // Any other message on this port is invalid and we ignore it
        default: assert( FALSE ); break;
    }
}




//-----------------------------------------------------------------------------
// Name: ProcessFindGame()
// Desc: Process the find game message
//-----------------------------------------------------------------------------
VOID CXBoxSample::ProcessFindGame( const MsgFindGame& findGame )
{
    // If we're not hosting a game, we don't care about receiving "find game"
    // messages. Only hosts respond to "find game" messages
    if( !m_bIsHost )
        return;

    // We're hosting a game
    // Respond with the game information
    SendGameFound( findGame.nonce );
}




//-----------------------------------------------------------------------------
// Name: ProcessGameFound()
// Desc: Process the game found message
//-----------------------------------------------------------------------------
VOID CXBoxSample::ProcessGameFound( const MsgGameFound& gameFound )
{
    // If we're hosting, we don't care about receiving "game found" messages.
    // Only potential clients care about "game found" messages.
    if( m_bIsHost )
        return;

    // If we didn't send the corresponding "find game" message, we don't
    // care about this particular "game found" message
    if( memcmp( &gameFound.nonce, &m_Nonce, NONCE_BYTES ) != 0 )
        return;

    // We found a game!
    // Add it to our list of potential games
    GameInfo gameInfo;
    CopyMemory( &gameInfo.xnHostKeyID, &gameFound.xnHostKeyID, sizeof( XNKID ) );
    CopyMemory( &gameInfo.xnHostKey,   &gameFound.xnHostKey,   sizeof( XNKEY ) );
    CopyMemory( &gameInfo.xnHostAddr,  &gameFound.xnHostAddr,  sizeof( XNADDR ) );
    gameInfo.byNumPlayers = gameFound.byNumPlayers;
    lstrcpynW( gameInfo.strGameName, gameFound.strGameName, MAX_GAME_NAME );
    lstrcpynW( gameInfo.strHostName, gameFound.strHostName, MAX_PLAYER_NAME );

    m_Games.push_back( gameInfo );
}




//-----------------------------------------------------------------------------
// Name: ProcessJoinGame()
// Desc: Process the join game message
//-----------------------------------------------------------------------------
VOID CXBoxSample::ProcessJoinGame( const MsgJoinGame& joinGame,
                                   const SOCKADDR_IN& saFrom )
{
    // Only hosts should receive "join game" messages
    assert( m_bIsHost );

    // Find this client in our pool of client connections to mark
    // the socket as accepted
    MatchInAddr matchInAddr( saFrom.sin_addr );
    SocketList::iterator it = std::find_if( m_ClientSockets.begin(), m_ClientSockets.end(), matchInAddr );
    assert( it != m_ClientSockets.end() );

    // A session exists between us (the host) and the client. We can now
    // convert the incoming IP address (saFrom) into a valid XNADDR.
    XNADDR xnAddrClient;
    INT iResult = XNetInAddrToXnAddr( saFrom.sin_addr, &xnAddrClient, 
                                      &m_xnHostKeyID );
    if( iResult == SOCKET_ERROR )
    {
        // If the client INADDR can't be converted to an XNADDR, then
        // this client does not have a valid session established, and
        // we ignore the message.
        LogXNetError( "XNetInAddrToXnAddr", iResult );
        assert( FALSE );
        return;
    }

    // A player may join if we haven't reached the player limit.
    // In a real game, you would need to "lock" the game during a join
    // or track the number of joins in progress so that if multiple
    // players were attempting to join at the same time, they wouldn't
    // all be granted access and then exceed the player maximum.
    if( m_Players.size() + 1 < MAX_PLAYERS )
    {
        // Notify the other players about the new guy
        Player player;
        CopyMemory( &player.xnAddr, &xnAddrClient, sizeof( XNADDR ) );
        lstrcpynW( player.strPlayerName, joinGame.strPlayerName, MAX_PLAYER_NAME );
        
        SendPlayerJoinedToAll( player );

        // We send the approval to the player AFTER we've told
        // everyone else.  This way, he doesn't get a PlayerJoined
        // message for himself
        SendJoinApproved( saFrom );
        it->bAccepted = TRUE;

        // Handle the joining of the new player
        in_addr inaddr = saFrom.sin_addr;
        OnPlayerJoined( joinGame.strPlayerName, 
                        xnAddrClient, 
                        &inaddr );
    }
    else
    {
        SendJoinDenied( saFrom );
        closesocket( it->sock );
        m_ClientSockets.erase( it );
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

    // Call StartVoice before we start adding players
    StartVoice();

    // Add the host
    XNADDR  xnAddr = {0};
    in_addr inaddr = saFrom.sin_addr;
    OnPlayerJoined( joinApproved.strHostName, xnAddr, &inaddr );

    // Build the list of the other players
    for( BYTE i = 0; i < joinApproved.byNumPlayers; ++i )
    {
        OnPlayerJoined( joinApproved.PlayerList[ i ].strPlayerName, 
                        joinApproved.PlayerList[ i ].xnAddr,
                        NULL );
    }

    // Enter into the game UI
    m_State = STATE_GAME;

    // Set the default game item to "wave"
    m_CurrItem = 0;

    lstrcpynW( m_strStatus, L"You have joined the game", MAX_STATUS_STR );
    m_HeartbeatTimer.StartZero();
}




//-----------------------------------------------------------------------------
// Name: ProcessJoinDenied()
// Desc: Process the join denied message
//-----------------------------------------------------------------------------
VOID CXBoxSample::ProcessJoinDenied( const SOCKADDR_IN& )
{
    // Only clients should receive "join denied" messages
    assert( !m_bIsHost );

    // If for some reason we receive a "join denied" message and we're hosting
    // a game, ignore the message. Only clients handle this message
    if( m_bIsHost )
        return;

    // Only clients who are not currently playing should receive this message
    assert( m_State != STATE_GAME );

    // If for some reason we receive a "join denied" message and we're
    // already playing a game, ignore the message.
    if( m_State == STATE_GAME )
        return;

    // The game we wanted to join is full. Display error
    m_State = STATE_ERROR;
    lstrcpynW( m_strError, L"The game is full.\nChoose another game.",
               MAX_ERROR_STR );
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
    OnPlayerJoined( player.strPlayerName, player.xnAddr, NULL );
}




//-----------------------------------------------------------------------------
// Name: ProcessWave()
// Desc: Process the wave message
//-----------------------------------------------------------------------------
VOID CXBoxSample::ProcessWave( const SOCKADDR_IN& saFrom )
{
    MatchInAddr matchInAddr( saFrom );

    // Find out who waved by matching the INADDR
    PlayerList::iterator i = std::find_if( m_Players.begin(), m_Players.end(), 
                                           matchInAddr );

    // Update status
    if( i != m_Players.end() )
    {
        wsprintfW( m_strStatus, L"%.*s waved", 
                   MAX_PLAYER_NAME, i->strPlayerName );
    }
}


//-----------------------------------------------------------------------------
// Name: ProcessVoiceInfo()
// Desc: Process the voiceport message
//-----------------------------------------------------------------------------
VOID CXBoxSample::ProcessVoiceInfo( const MsgVoiceInfo& msg, const SOCKADDR_IN& saFrom )
{
    if( !g_VoiceManager.IsInChatSession() )
        return;

    // We can't just look at the INADDR of the sender, since this message
    // may have been relayed by the host
    PlayerList::iterator it;
    for( it = m_Players.begin(); it < m_Players.end(); ++it )
    {
        if( !wcscmp( it->strPlayerName, msg.strSrcPlayer ) )
            break;
    }

    // If we get a message from a player we've dropped, ignore it
    if( it == m_Players.end() )
        return;

    // This message may or may not be intended for us.  If there's no
    // destination player specified, it's meant for everyone.  Otherwise,
    // we should only process it if it's got our name on it
    if( msg.strDestPlayer[0] == L'\0' ||
        !wcscmp( m_strPlayerName, msg.strDestPlayer ) )
    {
        // Update 
        XUID xuidSrc = XuidFromAddressAndPort( it->inAddr, msg.wControllerPort );
        switch( msg.action )
        {
        case VOICEINFO_ADDCHATTER:
            it->bHasVoice |= ( 1 << msg.wControllerPort );
            g_VoiceManager.ResetChatter( xuidSrc );
            break;
        case VOICEINFO_REMOVECHATTER:
            it->bHasVoice &= ~( 1 << msg.wControllerPort );
            break;
        case VOICEINFO_ADDREMOTEMUTE:
            // We should only get this message if it's meant for someone
            // on this box
            g_VoiceManager.RemoteMutePlayer( xuidSrc, msg.wDestPort );
            break;
        case VOICEINFO_REMOVEREMOTEMUTE:
            // We should only get this message if it's meant for someone
            // on this box
            g_VoiceManager.UnRemoteMutePlayer( xuidSrc, msg.wDestPort );
            break;
        default:
            assert( FALSE );
            break;
        }
    }

    // If the host wasn't the recipient of this message, or the message
    // is intended for all players, it's the host's responsibility to
    // relay it to all clients
    if( m_bIsHost )
    {
        Message msgVoiceInfo( MSG_VOICEINFO );
        msgVoiceInfo.GetMsgVoiceInfo() = msg;

        if( msg.strDestPlayer[0] == L'\0' )
        {
            // Intended for everyone - send to all BUT the source
            for( SocketList::iterator it = m_ClientSockets.begin();
                 it < m_ClientSockets.end();
                 ++it )
            {
                if( it->sa.sin_addr.s_addr != saFrom.sin_addr.s_addr )
                {
                    SOCKADDR_IN saDest( it->sa );
                    SendMessage( &msgVoiceInfo, TRUE, &saDest );
                }
            }
        }
        else if( wcscmp( msg.strDestPlayer, m_strPlayerName ) )
        {
            // Intended for a specific player (not us) - 
            // Find that player and send to them
            for( PlayerList::iterator it = m_Players.begin();
                 it < m_Players.end();
                 ++it )
            {
                if( !wcscmp( msg.strDestPlayer, it->strPlayerName ) )
                {
                    SOCKADDR_IN saDest;
                    saDest.sin_family = AF_INET;
                    saDest.sin_addr   = it->inAddr;
                    saDest.sin_port   = htons( RELIABLE_PORT );
                    SendMessage( &msgVoiceInfo, TRUE, &saDest );
                }
            }
        }
    }
}



//-----------------------------------------------------------------------------
// Name: ProcessVoiceData
// Desc: Handles receipt of a voice data packet
//-----------------------------------------------------------------------------
VOID CXBoxSample::ProcessVoiceData( const MsgVoiceData& msg, const SOCKADDR_IN& saFrom )
{
    if( g_VoiceManager.IsInChatSession() )
    {
        for( WORD i = 0; i < msg.wVoicePackets; i++ )
        {
            const VoicePacket* pPacket = &msg.VoicePackets[i];
            g_VoiceManager.ReceivePacket( XuidFromAddressAndPort( saFrom.sin_addr, pPacket->wControllerPort ), (VOID*)pPacket->byData, COMPRESSED_VOICE_SIZE );
        }
    }
}



//-----------------------------------------------------------------------------
// Name: ProcessHeartbeat()
// Desc: Process the heartbeat message
//-----------------------------------------------------------------------------
VOID CXBoxSample::ProcessHeartbeat( const SOCKADDR_IN& saFrom )
{
    MatchInAddr matchInAddr( saFrom );

    // Find out who sent a heartbeat by matching the INADDR
    PlayerList::iterator i = std::find_if( m_Players.begin(), m_Players.end(), 
                                           matchInAddr );

    // Update that player's heartbeat time
    if( i != m_Players.end() )
    {
        i->dwLastHeartbeat = GetTickCount();
    }
}




//-----------------------------------------------------------------------------
// Name: OnPlayerJoined 
// Desc: Called whenever a new player joins the game
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::OnPlayerJoined( const WCHAR* strName, XNADDR xnAddr, IN_ADDR* pinAddr )
{
    PlayerInfo playerInfo;

    lstrcpynW( playerInfo.strPlayerName, strName, MAX_PLAYER_NAME );
    playerInfo.xnAddr           = xnAddr;
    playerInfo.dwLastHeartbeat  = GetTickCount();
    playerInfo.bHasVoice        = 0;

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
            // If the client XNADDR can't be converted to an INADDR, then
            // this client does not have a valid session established, and
            // we ignore the message.
            LogXNetError( "XNetXnAddrToInAddr", iResult );
            return E_FAIL;
        }
    }

    // Add the new player to our list
    m_Players.push_back( playerInfo );

    // Notify the voice manager of the new player(s)
    for( WORD i = 0; i < XGetPortCount(); i++ )
    {
        XUID xuid = XuidFromAddressAndPort( playerInfo.inAddr, i );
        g_VoiceManager.AddChatter( xuid );
    }

    // Tell the new player about who has a communicator
    for( BYTE i = 0; i < XGetPortCount(); i++ )
    {
        if( g_VoiceManager.IsCommunicatorInserted( i ) )
        {
            SendVoiceInfo( VOICEINFO_ADDCHATTER, i, &playerInfo, 0 );
        }
    }

    // Update status
    wsprintfW( m_strStatus, L"%.*s has joined the game", 
                MAX_PLAYER_NAME, strName );

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: OnPlayerDisconnect
// Desc: Called whenever we've detected a player disconnect
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::OnPlayerDisconnect( PlayerInfo* pPlayer )
{
    // Update status message to reflect the newly departed player
    if( pPlayer->inAddr.S_un.S_addr == m_inHostAddr.S_un.S_addr )
    {
        // TODO: Since we don't have a reliable channel without the host,
        // and don't support host migration, we should end the game here
        assert( !m_bIsHost );
        wsprintfW( m_strStatus, L"Host left game -\nClosed to new players" );
        m_ReliableSock.Close();
    }
    else
    {
        wsprintfW( m_strStatus, L"%.*s left the game", 
                   MAX_PLAYER_NAME, pPlayer->strPlayerName );
    }

    // The host needs to close down the reliable channel
    if( m_bIsHost )
    {
        // Find the matching entry in list of reliable sockets
        MatchInAddr matchInAddr( pPlayer->inAddr );
        SocketList::iterator it = std::find_if( m_ClientSockets.begin(), m_ClientSockets.end(), matchInAddr );
        assert( it != m_ClientSockets.end() );
        
        // Close the socket
        closesocket( it->sock );
        m_ClientSockets.erase( it );
    }

    // Delete the player from the list of chatters
    for( WORD j = 0; j < XGetPortCount(); j++ )
    {
        XUID xuid = XuidFromAddressAndPort( pPlayer->inAddr, j );
        g_VoiceManager.RemoveChatter( xuid );
    }

    // Remove from players list
    MatchInAddr matchInAddr( pPlayer->inAddr );
    PlayerList::iterator it = std::find_if( m_Players.begin(), m_Players.end(), matchInAddr );
    assert( it != m_Players.end() );
    m_Players.erase( it );

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: ProcessPlayersDropouts()
// Desc: Process players and determine if anybody has left the game
//-----------------------------------------------------------------------------
BOOL CXBoxSample::ProcessPlayerDropouts()
{
    DWORD dwTickCount = GetTickCount();
    for( PlayerList::iterator i = m_Players.begin(); i != m_Players.end(); ++i )
    {
        PlayerInfo playerInfo = *i;
        DWORD dwElapsed = dwTickCount - playerInfo.dwLastHeartbeat;
        if( dwElapsed > PLAYER_TIMEOUT )
        {
            OnPlayerDisconnect( &(*i) );
            break;
        }
    }
    return FALSE;
}




//-----------------------------------------------------------------------------
// Name: DestroyGameList()
// Desc: Clear the list of games
//-----------------------------------------------------------------------------
VOID CXBoxSample::DestroyGameList()
{
    // Physically clear the list of games to obliterate the key and XNADDR info
    // from prying eyes. This particular method works because m_Games 
    // is a vector; if m_Games is not a vector each game must be 
    // cleared individually
    if( !m_Games.empty() )
    {
        GameInfo* pGameList = &m_Games[0];
        ZeroMemory( pGameList, m_Games.size() * sizeof( GameInfo ) );

        // Destroy the list of games
        m_Games.clear();
    }
}




//-----------------------------------------------------------------------------
// Name: DestroyPlayerList()
// Desc: Clear the list of players
//-----------------------------------------------------------------------------
VOID CXBoxSample::DestroyPlayerList()
{
    // Physically clear the list of players to obliterate the XNADDR info
    // from prying eyes. This particular method works because m_Players
    // is a vector; if m_Players is not a vector each player must be 
    // cleared individually
    if( !m_Players.empty() )
    {
        PlayerInfo* pPlayerList = &m_Players[0];
        ZeroMemory( pPlayerList, m_Players.size() * sizeof( PlayerInfo ) );

        // Destroy the list of players
        m_Players.clear();
    }
}

//-----------------------------------------------------------------------------
// Name: GenRandom()
// Desc: Generate a random name
//-----------------------------------------------------------------------------
VOID CXBoxSample::GenRandom( WCHAR* strName, DWORD dwSize ) // static
{
    // Name consists of two to five parts.
    //
    // 1) consonant or consonant group (e.g. th, qu, st) [optional]
    // 2) vowel or vowel group (e.g. ea, ee, au)
    // 3) consonant or consonant group
    // 4) vowel or vowel group [optional]
    // 5) consonant or consonant group [optional]

    WCHAR strRandom[ 128 ];
    strRandom[ 0 ] = 0;
    if( ( rand() % 2 == 0 ) )
        AppendConsonant( strRandom, TRUE );
    AppendVowel( strRandom );
    AppendConsonant( strRandom, FALSE );
    if( ( rand() % 2 == 0 ) )
    {
        AppendVowel( strRandom );
        if( ( rand() % 2 == 0 ) )
            AppendConsonant( strRandom, FALSE );
    }

    *strRandom = towupper( *strRandom );
    lstrcpynW( strName, strRandom, dwSize );
}




//-----------------------------------------------------------------------------
// Name: GetRandVowel()
// Desc: Get a random vowel
//-----------------------------------------------------------------------------
WCHAR CXBoxSample::GetRandVowel() // static
{
    for(;;)
    {
        WCHAR c = WCHAR( L'a' + ( rand() % 26 ) );
        if( wcschr( L"aeiou", c ) != NULL )
            return c;
    }
}




//-----------------------------------------------------------------------------
// Name: GetRandConsonant()
// Desc: Get a random consonant
//-----------------------------------------------------------------------------
WCHAR CXBoxSample::GetRandConsonant() // static
{
    for(;;)
    {
        WCHAR c = WCHAR( L'a' + ( rand() % 26 ) );
        if( wcschr( L"aeiou", c ) == NULL )
            return c;
    }
}




//-----------------------------------------------------------------------------
// Name: AppendConsonant()
// Desc: Append consonant or consonant group to string
//-----------------------------------------------------------------------------
VOID CXBoxSample::AppendConsonant( WCHAR* strRandom, BOOL bLeading ) // static
{
    if( ( rand() % 2 == 0 ) )
    {
        WCHAR strChar[ 2 ] = { GetRandConsonant(), 0 };
        lstrcatW( strRandom, strChar );
    }
    else
    {
        WCHAR* strLeadConGroup[32] = 
        {
            L"bl", L"br", L"cl", L"cr", L"dr", L"fl", L"fr", L"gh", L"gl", L"gn", 
            L"gr", L"kl", L"kn", L"kr", L"ph", L"pl", L"pr", L"ps", L"qu", L"sc", 
            L"sk", L"sl", L"sn", L"sp", L"st", L"sw", L"th", L"tr", L"vh", L"vl", 
            L"wh", L"zh"
        };
        WCHAR* strTrailConGroup[32] = 
        {
            L"rt", L"ng", L"bs", L"cs", L"ds", L"gs", L"hs", L"sh", L"ss", L"ks",
            L"ms", L"ns", L"ps", L"rs", L"ts", L"gh", L"ph", L"sk", L"st", L"tt",
            L"nd", L"nk", L"nt", L"nx", L"pp", L"rd", L"rg", L"rk", L"rn", L"rv",
            L"th", L"ys"
        };
        if( bLeading )
            lstrcatW( strRandom, strLeadConGroup[ rand() % 32 ] );
        else
            lstrcatW( strRandom, strTrailConGroup[ rand() % 32 ] );
    }
}




//-----------------------------------------------------------------------------
// Name: AppendVowel()
// Desc: Append vowel or vowel group to string
//-----------------------------------------------------------------------------
VOID CXBoxSample::AppendVowel( WCHAR* strRandom ) // static
{
    if( ( rand() % 2 == 0 ) )
    {
        WCHAR strChar[ 2 ] = { GetRandVowel(), 0 };
        lstrcatW( strRandom, strChar );
    }
    else
    {
        WCHAR* strVowelGroup[10] =
        {
            L"ai", L"au", L"ay", L"ea", L"ee", L"ie", L"oa", L"oi", L"oo", L"ou"
        };
        lstrcatW( strRandom, strVowelGroup[ rand() % 10 ] );
    }
}




//-----------------------------------------------------------------------------
// Name: LogXNetError()
// Desc: Log errors to the hard drive. When testing xnets.lib, there's no
//       debugging channel, so it's useful to log failures to the hard drive.
//-----------------------------------------------------------------------------
VOID CXBoxSample::LogXNetError( const CHAR* strError, INT iError ) const
{
    // Make sure that we're not logging anything in the final release
#ifndef FINAL_BUILD
    if( m_hLogFile == INVALID_HANDLE_VALUE )
    {
        m_hLogFile = CreateFile( "U:\\XNetError.log", GENERIC_WRITE, 0, NULL,
                                 CREATE_ALWAYS, 0, NULL );
        if( m_hLogFile == INVALID_HANDLE_VALUE )
            return;
    }

    // Write out the error message
    CHAR strBuffer[256];
    wsprintfA( strBuffer, "%s error: %d\r\n", strError, iError );
    DWORD dwWritten;
    WriteFile( m_hLogFile, strBuffer, lstrlenA( strBuffer ), &dwWritten, NULL );

    // Make sure the message makes it to the disk
    FlushFileBuffers( m_hLogFile );
#endif
}

