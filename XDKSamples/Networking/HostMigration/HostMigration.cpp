//-----------------------------------------------------------------------------
// File: HostMigration.cpp
//
// Desc: Demonstrates a simple way a title can migrate the host of a game
//       session when the original host leaves. A single ID is used as a place
//       holder for game state data which is maintained when reconnecting to
//       the new host as part of host migration.
// 
// Hist: 09.11.02 - New for October release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "HostMigration.h"


// Port 1000 gives 0 extra port overhead on the wire
// Ports 1001-1255 give 2 bytes overhead on the wire
// All other ports give 4 bytes overhead on the wire
const WORD  BROADCAST_PORT    = 1001;  // Could be any port
const WORD  DIRECT_PORT       = 1000;  // Any port other than BROADCAST_PORT




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
:   m_GameNames   (),
    m_Games       (),
    m_Players     (),
    m_strError    (),
    m_strStatus   (),
    m_xnHostKeyID         (),
    m_xnHostKeyExchange   (),
    m_xnTitleAddress      (),
    m_inHostAddr          (),
    m_BroadSock           (),
    m_DirectSock          (),
    m_strGameName         (),
    m_strPlayerName       (),
    m_strHostName         (),
    m_Nonce               ()
{
    m_hLogFile    = INVALID_HANDLE_VALUE;
    m_State       = STATE_MENU;
    m_LastState   = STATE_MENU;
    m_CurrItem    = 0;
    m_bIsOnline           = FALSE;
    m_bXnetStarted        = FALSE;
    m_bIsHost             = FALSE;
    m_bIsSessionRegistered= FALSE;
    m_bHaveLocalAddress   = FALSE;
    m_dwNextID            = 0;
    m_dwMyID              = 0;

    m_HostTimeoutTimer.Start();
    m_HostRequestTimer.Start();

    srand( GetTickCount() ); // for generating game/player names

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

    // Initialize the help system
    if( FAILED( m_Help.Create( "Gamepad.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    return S_OK;
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
    
    switch( m_State )
    {
        case STATE_MENU:            FrameMoveMenu( ev );        break;
        case STATE_GAME:            FrameMoveGame( ev );        break;
        case STATE_HELP:            FrameMoveHelp( ev );        break;
        case STATE_SELECT_NAME:     FrameMoveSelectName( ev );  break;
        case STATE_START_NEW_GAME:  FrameMoveStartGame( ev );   break;
        case STATE_GAME_SEARCH:     FrameMoveGameSearch( ev );  break;
        case STATE_SELECT_GAME:     FrameMoveSelectGame( ev );  break;
        case STATE_REQUEST_JOIN:    FrameMoveRequestJoin( ev ); break;
        case STATE_FIND_NEW_HOST:   FrameMoveFindNewHost( ev ); break;
        case STATE_RESUME_GAME:     FrameMoveResumeGame( ev );  break;
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
        case STATE_REQUEST_JOIN:    RenderRequestJoin(); break;
        case STATE_FIND_NEW_HOST:   RenderFindNewHost(); break;
        case STATE_RESUME_GAME:     RenderResumeGame();  break;
        case STATE_ERROR:           RenderError();       break;
    }
    
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

    // Send keep-alives
    if( m_HeartbeatTimer.GetElapsedSeconds() > PLAYER_HEARTBEAT )
    {
        Heartbeat();
        m_HeartbeatTimer.StartZero();
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
                case GAME_MENU_LEAVE_GAME:
                    Init();
                    break;
            }
            break;
        case EV_BUTTON_X:
            OUTPUT_DEBUG_STRING("Sleeping\n");
            Sleep( rand() % 1000 );
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

    
    // When startup is complete, add us to the player list and enter the game
    if( m_bHaveLocalAddress )
    {   
        m_HeartbeatTimer.StartZero();
        m_State = STATE_GAME;
        m_bIsHost = TRUE;

        PlayerInfo Info;
        
        Info.dwLastHeartbeat = 0; 
        Info.xnAddr = m_xnTitleAddress;
        
        // Basic game data for us
        m_dwMyID = m_dwNextID++;
        Info.gameData.dwID = m_dwMyID;

        lstrcpynW( Info.strPlayerName, m_strPlayerName, MAX_PLAYER_NAME );        

        m_Players.push_back( Info );
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

    // See if the game has replied
    ProcessDirectMessage();

    // We wait for up to GAME_JOIN_TIME seconds. If the game didn't
    // respond, display an error message
    if( m_GameJoinTimer.GetElapsedSeconds() > GAME_JOIN_TIME )
    {
        m_GameJoinTimer.Stop();
        m_State = STATE_ERROR;
        lstrcpynW( m_strError, L"Game did not respond", MAX_ERROR_STR );
    }
}

//-----------------------------------------------------------------------------
// Name: FrameMoveFindNewHost()
// Desc: Find a new host
//-----------------------------------------------------------------------------
VOID CXBoxSample::FrameMoveFindNewHost( Event ev )
{
    if( ev == EV_BUTTON_B || !m_bIsOnline )
    {
        Init();
        return;
    }

    ProcessDirectMessage(); 
    
    // Check if we are the next host
    if( m_Players[ 0 ].gameData.dwID == m_dwMyID )
    {
        // We are the new host
        m_State = STATE_RESUME_GAME;
        DestroyPlayerList();                                      
        return;
    }

    // Check if we just started finding a new host (the timer isn't running), or
    // Our request timer has expired
    if( !m_HostTimeoutTimer.IsRunning() ||         
        ( m_HostRequestTimer.GetElapsedSeconds() > NEW_HOST_TIME_BETWEEN_TRIES ) )
    {
        // Send a ping to the one we think is the host
        
        // Start the response timer running
        if( !m_HostTimeoutTimer.IsRunning() )
            m_HostTimeoutTimer.StartZero();

        // Reset the request delay timer
        m_HostRequestTimer.StartZero();       

        // Send a connect request
        SOCKADDR_IN sa;
        sa.sin_family = AF_INET;
        sa.sin_addr   = m_inHostTryAddr;
        sa.sin_port   = htons( DIRECT_PORT );
        SendConnectToMigratedHost( sa );                
    }
    
    // Test if we have to give up on our current host
    if( m_HostTimeoutTimer.GetElapsedSeconds() > NEW_HOST_RESPONSE_TIME )
    {
        // Terminate the player who hasn't been cooperative
        m_Players.erase( m_Players.begin() );                     

        // Set the next host to try
        m_inHostTryAddr = m_Players[ 0 ].inAddr;
        
        // Reset the timers
        m_HostTimeoutTimer.Stop(); 
        m_HostRequestTimer.StartZero(); 
    }
}

//-----------------------------------------------------------------------------
// Name: FrameMoveResumeGame()
// Desc: Animate resume game
//-----------------------------------------------------------------------------
VOID CXBoxSample::FrameMoveResumeGame( Event ev )
{    
    // Allow the player to cancel out of game resuming (back to main menu)
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

    
    // When startup is complete, add us to the player list and enter the game
    if( m_bHaveLocalAddress )
    {   
        m_HeartbeatTimer.StartZero();
        m_State = STATE_GAME;
        m_bIsHost = TRUE;

        PlayerInfo Info;
        
        Info.dwLastHeartbeat = 0; 
        Info.xnAddr = m_xnTitleAddress;                      
        
        // Basic game data for us        
        lstrcpynW( Info.strPlayerName, m_strPlayerName, MAX_PLAYER_NAME );        

        // copy over our game data from before into the new player list
        Info.gameData.dwID = m_dwMyID;

        m_Players.push_back( Info );
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
        m_Font.DrawText( 220, fYtop + (fYdelta * m_CurrItem), 0xff00ff00,
                         GLYPH_RIGHT_TICK );

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
    WCHAR strGameInfo[ 64 + MAX_GAME_NAME + MAX_PLAYER_NAME ];
    wsprintfW( strGameInfo, L"Game name: %.*s\nYour name: %.*s (0x%04x)", 
               MAX_GAME_NAME, m_strGameName, MAX_PLAYER_NAME, m_strPlayerName, m_dwMyID );
    m_Font.DrawText( 70, 110, COLOR_GREEN, strGameInfo );

    // Number of players and current status
    m_Font.DrawText( 70, 158, COLOR_GREEN, m_strStatus );

    // Game options menu
    const WCHAR* const strMenu[] =
    {
        L"Wave To Other Players",
        L"Leave Game",
    };

    FLOAT fYtop = 240.0f;
    FLOAT fYdelta = 50.0f;

    // Show menu
    for( DWORD i = 0; i < GAME_MENU_MAX; ++i )
    {
        DWORD dwColor = ( m_CurrItem == i ) ? COLOR_HIGHLIGHT : COLOR_NORMAL;
        m_Font.DrawText( 110, fYtop + (fYdelta * i), dwColor, strMenu[i] );
    }

    // Show selected item with little triangle
    m_Font.DrawText( 70, fYtop + (fYdelta * m_CurrItem), 0xff00ff00,
                     GLYPH_RIGHT_TICK );

    // Render the list of players, including who has voice communicators
    FLOAT fX = 400.0f;
    FLOAT fY = 110.0f;
    m_Font.DrawText( fX, fY, COLOR_GREEN, L"Remote Players:" );
    fY += 30.0f;
    
    for( PlayerList::iterator i = m_Players.begin(); i != m_Players.end(); ++i )
    {     
        if( i->gameData.dwID == m_dwMyID ) 
            continue;
        WCHAR strPlayerInfo[ 32 + MAX_PLAYER_NAME ];
        wsprintfW( strPlayerInfo, L"%.*s (0x%04x)", MAX_PLAYER_NAME, i->strPlayerName, i->gameData.dwID );
        m_Font.DrawText( fX, fY, COLOR_GREEN, strPlayerInfo );
        fY += 30.0f;        
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
        { XBHELP_X_BUTTON,      XBHELP_PLACEMENT_1, L"Random sleep" },        
        { XBHELP_DPAD,          XBHELP_PLACEMENT_1, L"Menu navigation" },                
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
    m_Font.DrawText( 240, fYtop + (fYdelta * m_CurrItem), 0xff00ff00,
                     GLYPH_RIGHT_TICK );
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
// Name: RenderFindNewHost()
// Desc: Display game startup sequence
//-----------------------------------------------------------------------------
VOID CXBoxSample::RenderFindNewHost()
{
    RenderHeader();
    m_Font.DrawText( 320, 240, 0xffffffff, L"Trying to find a new Host\n\n"
                                           L"Press " GLYPH_B_BUTTON L" to cancel",
                     XBFONT_CENTER_X | XBFONT_CENTER_Y );
}

//-----------------------------------------------------------------------------
// Name: RenderResumeGame()
// Desc: Display game resume sequence
//-----------------------------------------------------------------------------
VOID CXBoxSample::RenderResumeGame()
{
    RenderHeader();
    m_Font.DrawText( 320, 240, 0xffffffff, L"Resuming Game\n\n"
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
    m_Font.DrawText( 100, fYtop + (fYdelta * m_CurrItem), 0xff00ff00,
                     GLYPH_RIGHT_TICK );
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
    lstrcpyW( strName, L"HostMigration" );
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
    m_xnHostKeyID = gameInfo.xnHostKeyID;
    m_xnHostKeyExchange = gameInfo.xnHostKey;
    
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

        // Request join approval from the game and await a response
        SOCKADDR_IN sa;
        sa.sin_family = AF_INET;
        sa.sin_addr   = m_inHostAddr;
        sa.sin_port   = htons( DIRECT_PORT );
        SendJoinGame( sa );
        m_GameJoinTimer.StartZero();
        m_State = STATE_REQUEST_JOIN;
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

    // Send a "wave" message to each of the other players in the game
    for( PlayerList::iterator i = m_Players.begin(); i != m_Players.end(); ++i )
    {
        if( i->gameData.dwID == m_dwMyID ) 
            continue;

        SOCKADDR_IN sa;
        sa.sin_family = AF_INET;
        sa.sin_addr   = i->inAddr;
        sa.sin_port   = htons( DIRECT_PORT );
        SendWave( sa );
    }
}

//-----------------------------------------------------------------------------
// Name: Heartbeat()
// Desc: Send heartbeat to players in the game
//-----------------------------------------------------------------------------
VOID CXBoxSample::Heartbeat()
{
    // Send a "heartbeat" message to each of the other players in the game
    // to let them know we're alive
    for( PlayerList::iterator i = m_Players.begin(); i != m_Players.end(); ++i )
    {
        if( i->gameData.dwID == m_dwMyID ) 
            continue;

        SOCKADDR_IN sa;
        sa.sin_family = AF_INET;
        sa.sin_addr   = i->inAddr;
        sa.sin_port   = htons( DIRECT_PORT );
        SendHeartbeat( sa );
    }
}




//-----------------------------------------------------------------------------
// Name: Init()
// Desc: Teardown any active games and player lists and return to main menu
//       Unregisters any active sessions.
//-----------------------------------------------------------------------------
VOID CXBoxSample::Init()
{
    // Don't clear m_bXnetStarted. We don't need to reinitialize Xnet once
    // it's been started.
    
    m_bHaveLocalAddress = FALSE;
    m_State     = STATE_MENU;
    m_LastState = STATE_MENU;
    m_CurrItem  = 0;
    
    m_GameNames.clear();
    DestroyGameList();
    DestroyPlayerList();

    // reset game data
    m_dwMyID = 0;
    m_dwNextID = 0;


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
    bSuccess = m_DirectSock.Open( CXBSocket::Type_UDP );
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
    
    //assert( nBytes == msgFindGame.GetSize() );
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
    msg.byNumPlayers = BYTE( m_Players.size() );
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
    
    //assert( nBytes == msgGameFound.GetSize() );
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

    // We can send this message directly to the host
    INT nBytes = m_DirectSock.SendTo( &msgJoinGame, msgJoinGame.GetSize(),
                                      &saGameHost );
    // This assert was removed because Send no longer is guaranteed to always work
    // If the security association times out, the number of bytes returned will 
    // NOT be equal to the size of the message.  A good thing to do here would
    // be to drop the player
    
    //assert( nBytes == msgJoinGame.GetSize() );
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
        
        msg.PlayerList[j].gameData.dwID = playerInfo.gameData.dwID;       
    }
    
    // We can send this message directly back to the requesting client
    INT nBytes = m_DirectSock.SendTo( &msgJoinApproved, msgJoinApproved.GetSize(),
                                      &saClient );
    // This assert was removed because Send no longer is guaranteed to always work
    // If the security association times out, the number of bytes returned will 
    // NOT be equal to the size of the message.  A good thing to do here would
    // be to drop the player
    //assert( nBytes == msgJoinApproved.GetSize() );
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

    // We can send this message directly back to the requesting client
    INT nBytes = m_DirectSock.SendTo( &msgJoinDenied, msgJoinDenied.GetSize(),
                                      &saClient );
    // This assert was removed because Send no longer is guaranteed to always work
    // If the security association times out, the number of bytes returned will 
    // NOT be equal to the size of the message.  A good thing to do here would
    // be to drop the player
    //assert( nBytes == msgJoinDenied.GetSize() );
    (VOID)nBytes;
}




//-----------------------------------------------------------------------------
// Name: SendPlayerJoined()
// Desc: Issue a MSG_PLAYER_JOINED from our host to a player in the game
//-----------------------------------------------------------------------------
VOID CXBoxSample::SendPlayerJoined( const Player& player, const SOCKADDR_IN& saPlayer )
{
    assert( m_bIsHost );
    Message msgPlayerJoined( MSG_PLAYER_JOINED );
    MsgPlayerJoined& msg = msgPlayerJoined.GetPlayerJoined();

    // The payload is the information about the player who just joined
    CopyMemory( &msg.player, &player, sizeof(player) );

    // We send this message directly to the player
    INT nBytes = m_DirectSock.SendTo( &msgPlayerJoined, msgPlayerJoined.GetSize(),
                                      &saPlayer );
    // This assert was removed because Send no longer is guaranteed to always work
    // If the security association times out, the number of bytes returned will 
    // NOT be equal to the size of the message.  A good thing to do here would
    // be to drop the player
    //assert( nBytes == msgPlayerJoined.GetSize() );
    (VOID)nBytes;
}

//-----------------------------------------------------------------------------
// Name: SendPlayerRejoin()
// Desc: Issue a MSG_PLAYER_REJOIN from our host to a player in the game
//-----------------------------------------------------------------------------
VOID CXBoxSample::SendPlayerRejoined( const Player& player, const SOCKADDR_IN& saPlayer )
{
    assert( m_bIsHost );

    Message msgPlayerRejoined( MSG_PLAYER_REJOINED );
    MsgPlayerRejoined& msg = msgPlayerRejoined.GetPlayerRejoined();
   
    // The payload is the information about the player who just rejoined
    CopyMemory( &msg.player, &player, sizeof(player) );

    // We send this message directly to the player
    INT nBytes = m_DirectSock.SendTo( &msgPlayerRejoined, msgPlayerRejoined.GetSize(),
                                      &saPlayer );
    // This assert was removed because Send no longer is guaranteed to always work
    // If the security association times out, the number of bytes returned will 
    // NOT be equal to the size of the message.  A good thing to do here would
    // be to drop the player
    //assert( nBytes == msgPlayerRejoined.GetSize() );
    (VOID)nBytes;
}




//-----------------------------------------------------------------------------
// Name: SendWave()
// Desc: Issue a MSG_WAVE from ourself (either a host or player) to another
//       player
//-----------------------------------------------------------------------------
VOID CXBoxSample::SendWave( const SOCKADDR_IN& saPlayer )
{
    Message msgWave( MSG_WAVE );
    INT nBytes = m_DirectSock.SendTo( &msgWave, msgWave.GetSize(), 
                                      &saPlayer );
    // This assert was removed because Send no longer is guaranteed to always work
    // If the security association times out, the number of bytes returned will 
    // NOT be equal to the size of the message.  A good thing to do here would
    // be to drop the player
    //assert( nBytes == msgWave.GetSize() );
    (VOID)nBytes;
}



//-----------------------------------------------------------------------------
// Name: SendHeartbeat()
// Desc: Issue a MSG_HEARTBEAT from ourself (either a host or player) to
//       another player
//-----------------------------------------------------------------------------
VOID CXBoxSample::SendHeartbeat( const SOCKADDR_IN& saPlayer )
{
    // Send the heartbeat
    Message msgHeartbeat( MSG_HEARTBEAT );
    INT nBytes = m_DirectSock.SendTo( &msgHeartbeat, msgHeartbeat.GetSize(),
                                      &saPlayer );
    // This assert was removed because Send no longer is guaranteed to always work
    // If the security association times out, the number of bytes returned will 
    // NOT be equal to the size of the message.  A good thing to do here would
    // be to drop the player
    
    //assert( nBytes == msgHeartbeat.GetSize() );
    (VOID)nBytes;
}

//-----------------------------------------------------------------------------
// Name: SendConnectToMigratedHost()
// Desc: Issue a MSG_CONNECT_TO_MIGRATED_HOST from ourself to
//       another player
//-----------------------------------------------------------------------------

VOID CXBoxSample::SendConnectToMigratedHost( const SOCKADDR_IN& saPlayer )
{

    Message msgConnect( MSG_CONNECT_TO_MIGRATED_HOST );
    
    lstrcpynW( msgConnect.GetConnectToMigratedHost().strPlayerName, m_strPlayerName, MAX_PLAYER_NAME );
    
    // send our game data as well
    msgConnect.GetConnectToMigratedHost().gameData.dwID = m_dwMyID;

    INT nBytes = m_DirectSock.SendTo( &msgConnect, msgConnect.GetSize(),
                                       &saPlayer );
   // This assert was removed because Send no longer is guaranteed to always work
    // If the security association times out, the number of bytes returned will 
    // NOT be equal to the size of the message.  A good thing to do here would
    // be to drop the player
    
    //assert( nBytes == msgConnect.GetSize() );
    (VOID)nBytes;
}


//-----------------------------------------------------------------------------
// Name: SendMigrateHostApproved()
// Desc: Issue a MSG_MIGRATE_HOST_APPROVED from ourself to
//       another player
//-----------------------------------------------------------------------------

VOID CXBoxSample::SendMigrateHostApproved( const SOCKADDR_IN& saPlayer )
{
    assert( m_bIsHost );
    Message msgMigrateHostApproved( MSG_MIGRATE_HOST_APPROVED );
    MsgMigrateHostApproved& msg = msgMigrateHostApproved.GetMigrateHostApproved();   

    // send our name

    lstrcpynW( msg.strHostName, m_strPlayerName, MAX_PLAYER_NAME );
    
    // Send the list of all the current players to the new player.
    
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

        msg.PlayerList[j].gameData.dwID = playerInfo.gameData.dwID;
    }

    // We can send this message directly back to the requesting client
    INT nBytes = m_DirectSock.SendTo( &msgMigrateHostApproved, msgMigrateHostApproved.GetSize(),
                                      &saPlayer );
    // This assert was removed because Send no longer is guaranteed to always work
    // If the security association times out, the number of bytes returned will 
    // NOT be equal to the size of the message.  A good thing to do here would
    // be to drop the player
    //assert( nBytes == msgMigrateHostApproved.GetSize() );
    (VOID)nBytes;
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
        
        // Messages for host migration
        case MSG_CONNECT_TO_MIGRATED_HOST:      
            ProcessConnectToMigratedHost( msg.GetConnectToMigratedHost(), saFrom ); break;
        
        case MSG_MIGRATE_HOST_APPROVED: 
            ProcessMigrateHostApproved( msg.GetMigrateHostApproved(), saFrom ); break;        
        
        case MSG_PLAYER_REJOINED:    
            ProcessPlayerRejoined( msg.GetPlayerRejoined(), saFrom ); break;
        

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

    // If for some reason we receive a "join game" message and we're not a
    // host, ignore it. Only hosts respond to "join game" messages
    if( !m_bIsHost )
        return;

    // We're hosting

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
    if( m_Players.size() + 1 <= MAX_PLAYERS )
    {       
        // Notify the other players about the new guy
        Player player;
        CopyMemory( &player.xnAddr, &xnAddrClient, sizeof( XNADDR ) );
        lstrcpynW( player.strPlayerName, joinGame.strPlayerName, MAX_PLAYER_NAME );
        player.gameData.dwID = m_dwNextID++;    // new ID for the new player

        for( PlayerList::iterator i = m_Players.begin(); i != m_Players.end(); ++i )
        {
            if( i->gameData.dwID == m_dwMyID ) 
                continue;

            SOCKADDR_IN sa;
            sa.sin_family = AF_INET;
            sa.sin_addr   = i->inAddr;
            sa.sin_port   = htons( DIRECT_PORT );
            SendPlayerJoined( player, sa );
        }

        // Add this new player to our player list
        PlayerInfo playerInfo;
        CopyMemory( &playerInfo.xnAddr, &xnAddrClient, sizeof( XNADDR ) );
        playerInfo.inAddr = saFrom.sin_addr;
        lstrcpynW( playerInfo.strPlayerName, joinGame.strPlayerName, 
                   MAX_PLAYER_NAME );
        playerInfo.dwLastHeartbeat = GetTickCount();
        playerInfo.gameData.dwID = player.gameData.dwID;
        
        m_Players.push_back( playerInfo );
     
        SendJoinApproved( saFrom );

        // Update status
        wsprintfW( m_strStatus, L"%.*s has joined the game", 
                   MAX_PLAYER_NAME, player.strPlayerName );
    }
    else
    {
        SendJoinDenied( saFrom );
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

    assert( m_Players.empty() );
    
    // Build the list of players - host will be 0, we will be last
    for( BYTE i = 0; i < joinApproved.byNumPlayers; ++i )
    {
        PlayerInfo playerInfo;        

        // Convert the XNADDR of the player to the INADDR we'll use to wave
        // to the player
        INT iResult = XNetXnAddrToInAddr( &joinApproved.PlayerList[ i ].xnAddr,
                                          &m_xnHostKeyID, &playerInfo.inAddr );
        if( iResult != 0 )
        {
            // If the client XNADDR can't be converted to an INADDR, then
            // the client does not have a valid session established, and
            // we ignore that client.
            assert( FALSE );
            LogXNetError( "XNetXnAddrToInAddr", iResult );
            continue;
        }
       
        playerInfo.xnAddr = joinApproved.PlayerList[ i ].xnAddr;

        // Save the player name
        lstrcpynW( playerInfo.strPlayerName, 
                   joinApproved.PlayerList[ i ].strPlayerName, MAX_PLAYER_NAME );

        // Last heartbeat
        playerInfo.dwLastHeartbeat = GetTickCount();
        playerInfo.gameData.dwID = joinApproved.PlayerList[ i ].gameData.dwID;
        
        m_Players.push_back( playerInfo );
    }

    assert( joinApproved.byNumPlayers >= 1 );
    
    // set my ID ( I'm the last player in the list )
    m_dwMyID = joinApproved.PlayerList[ joinApproved.byNumPlayers - 1 ].gameData.dwID;
    
    // set the next ID in case I become the server
    m_dwNextID = m_dwMyID + 1;

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
    PlayerInfo playerInfo;

    // Convert the XNADDR of the player to the INADDR we'll use to wave
    // to the player
    INT iResult = XNetXnAddrToInAddr( &player.xnAddr, &m_xnHostKeyID,
                                      &playerInfo.inAddr );
    if( iResult != 0 )
    {
        // If the client XNADDR can't be converted to an INADDR, then
        // this client does not have a valid session established, and
        // we ignore the message.
        LogXNetError( "XNetXnAddrToInAddr", iResult );
        assert( FALSE );
        return;
    }

    // Client doesn't need the XNADDR of the host anymore, 
    // so we just leave it zero. This data member is only used by hosts.
    ZeroMemory( &playerInfo.xnAddr, sizeof( XNADDR ) );

    // Save the player name
    lstrcpynW( playerInfo.strPlayerName, player.strPlayerName, MAX_PLAYER_NAME );

    // Last heartbeat
    playerInfo.dwLastHeartbeat = GetTickCount();

    // Game Data
    playerInfo.gameData.dwID = player.gameData.dwID;
    
    // make sure we update our next ID to be after the new player
    if( playerInfo.gameData.dwID >= m_dwNextID )
    {
        m_dwNextID = playerInfo.gameData.dwID + 1;
    }

    // Add the new player to our list
    m_Players.push_back( playerInfo ); 

    // Update status
    wsprintfW( m_strStatus, L"%.*s has joined the game", 
               MAX_PLAYER_NAME, player.strPlayerName );
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
// Name: ProcessConnectToMigratedHost
// Desc: Process the host reconnection message
//-----------------------------------------------------------------------------
VOID CXBoxSample::ProcessConnectToMigratedHost( const MsgConnectToMigratedHost &msgCMH,
                                                const SOCKADDR_IN& saFrom )
{    
    // If for some reason we receive this message and we're not a
    // host, ignore it
    
    if( !m_bIsHost )
    {
        return;
    }

    // We're hosting

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

    
    // Verify we aren't over the player limit- if we are, we just ignore it   
    if( m_Players.size() + 1 <= MAX_PLAYERS )
    {           
        // Notify the other players about the new guy
        Player player;
        CopyMemory( &player.xnAddr, &xnAddrClient, sizeof( XNADDR ) );
        lstrcpynW( player.strPlayerName, msgCMH.strPlayerName, MAX_PLAYER_NAME );
           
        // search to see if someone already has the ID he says he has

        // RXDK: hoisted out of the for-init (MSVC's old for-scope leaked it past the loop)
        PlayerList::iterator i;
        for( i = m_Players.begin(); i != m_Players.end(); ++i )
        {
            if( i->gameData.dwID == msgCMH.gameData.dwID ) break;
        }

        if( i == m_Players.end() )
        {
            // copy over the players old game data - he's not new
            player.gameData.dwID = msgCMH.gameData.dwID;         
        }
        else
        {
            // we need to reassign him a new id
            player.gameData.dwID = m_dwNextID++;
        }


        // Notify connect players that the player has rejoined            

        for( PlayerList::iterator i = m_Players.begin(); i != m_Players.end(); ++i )
        {
            if( i->gameData.dwID == m_dwMyID ) 
                continue;

            SOCKADDR_IN sa;
            sa.sin_family = AF_INET;
            sa.sin_addr   = i->inAddr;
            sa.sin_port   = htons( DIRECT_PORT );
            SendPlayerRejoined( player, sa );
        }

        // Add this new player to our player list
        PlayerInfo playerInfo;
        CopyMemory( &playerInfo.xnAddr, &xnAddrClient, sizeof( XNADDR ) );
        playerInfo.inAddr = saFrom.sin_addr;
        lstrcpynW( playerInfo.strPlayerName, msgCMH.strPlayerName, 
                   MAX_PLAYER_NAME );
        playerInfo.dwLastHeartbeat = GetTickCount();

        // copy over the gamedata we reconstructed earlier
        playerInfo.gameData.dwID = player.gameData.dwID;  
        
        m_Players.push_back( playerInfo );
     
        SendMigrateHostApproved( saFrom );      
    }    
    else
    {
        // This is a pathalogical case- someone has joined before we can reconnect
        // For this example, we just send a generic join denied error so the
        // client can display an error message

        SendJoinDenied( saFrom );
    }
}

//-----------------------------------------------------------------------------
// Name: ProcessMigrateHostApproved
// Desc: Process the Migrate Host Approved message
//-----------------------------------------------------------------------------

VOID CXBoxSample::ProcessMigrateHostApproved( const MsgMigrateHostApproved &msg,
                                              const SOCKADDR_IN& saFrom )
{        
    // if we aren't looking for a new host, ignore this message
    if( m_State != STATE_FIND_NEW_HOST ) 
        return;
   
    // Only clients should receive "Migrate host approved" messages
    assert( !m_bIsHost );

    DestroyPlayerList();
    
    // Build the list of players - host will be 0, we will be last
    for( BYTE i = 0; i < msg.byNumPlayers; ++i )
    {
        PlayerInfo playerInfo;        

        // Convert the XNADDR of the player to the INADDR we'll use to wave
        // to the player
        INT iResult = XNetXnAddrToInAddr( &msg.PlayerList[ i ].xnAddr,
                                          &m_xnHostKeyID, &playerInfo.inAddr );
        if( iResult != 0 )
        {
            // If the client XNADDR can't be converted to an INADDR, then
            // the client does not have a valid session established, and
            // we ignore that client.
            assert( FALSE );
            LogXNetError( "XNetXnAddrToInAddr", iResult );
            continue;
        }
       
        playerInfo.xnAddr = msg.PlayerList[ i ].xnAddr;

        // Save the player name
        lstrcpynW( playerInfo.strPlayerName, 
                   msg.PlayerList[ i ].strPlayerName, MAX_PLAYER_NAME );

        // Last heartbeat
        playerInfo.dwLastHeartbeat = GetTickCount();
        playerInfo.gameData.dwID = msg.PlayerList[ i ].gameData.dwID;

        m_Players.push_back( playerInfo );
    }

    // set my ID ( I'm the last player in the list - the host may have changed what I sent )
    m_dwMyID = msg.PlayerList[ msg.byNumPlayers - 1 ].gameData.dwID;
    
    // set the next ID in case I become the server
    m_dwNextID = m_dwMyID + 1;

    // Enter into the game UI
    m_State = STATE_GAME;

    // Set the default game item to "wave"
    m_CurrItem = 0;
    
    m_HeartbeatTimer.StartZero();
}


//-----------------------------------------------------------------------------
// Name: ProcessPlayerRejoin()
// Desc: Process the player rejoined message
//-----------------------------------------------------------------------------
VOID CXBoxSample::ProcessPlayerRejoined( const MsgPlayerRejoined& playerRejoined, 
                                         const SOCKADDR_IN& saFrom )
{
    // In a real Xbox game, we would handle this differently than join
    // Here we just don't print out a 'has joined' message

    // saFrom is the address of the host that sent this message, but we
    // we already have his address, so throw it away
    (VOID)saFrom;

    // if we are the host, ignore the message
    if( m_bIsHost )
        return;    

    const Player& player = playerRejoined.player;
    PlayerInfo playerInfo;

    // Convert the XNADDR of the player to the INADDR we'll use to wave
    // to the player
    INT iResult = XNetXnAddrToInAddr( &player.xnAddr, &m_xnHostKeyID,
                                      &playerInfo.inAddr );
    if( iResult != 0 )
    {
        // If the client XNADDR can't be converted to an INADDR, then
        // this client does not have a valid session established, and
        // we ignore the message.
        LogXNetError( "XNetXnAddrToInAddr", iResult );
        assert( FALSE );
        return;
    }

    // Client doesn't need the XNADDR of the new player anymore, 
    // so we just leave it zero. This data member is only used by hosts.
    ZeroMemory( &playerInfo.xnAddr, sizeof( XNADDR ) );

    // Save the player name
    lstrcpynW( playerInfo.strPlayerName, player.strPlayerName, MAX_PLAYER_NAME );

    // Last heartbeat
    playerInfo.dwLastHeartbeat = GetTickCount();

    // Game data
    playerInfo.gameData.dwID = player.gameData.dwID;

    // Add the new player to our list
    m_Players.push_back( playerInfo );    
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
// Name: ProcessPlayersDropouts()
// Desc: Process players and determine if anybody has left the game
//-----------------------------------------------------------------------------
BOOL CXBoxSample::ProcessPlayerDropouts()
{
    DWORD dwTickCount = GetTickCount();
    BOOL bDropHost;
    
    for( PlayerList::iterator i = m_Players.begin(); i != m_Players.end(); ++i )
    {
        if( i->gameData.dwID == m_dwMyID ) 
            continue;

        PlayerInfo playerInfo = *i;
        DWORD dwElapsed = dwTickCount - playerInfo.dwLastHeartbeat;
        if( dwElapsed > PLAYER_TIMEOUT )
        {
            // This player hasn't sent a heartbeat message in a long time.
            // Assume they left the game.

            wsprintfW( m_strStatus, L"%.*s left the game", 
                       MAX_PLAYER_NAME, playerInfo.strPlayerName );

            // See if it is the host that is leaving
            
            if( i == m_Players.begin() )
                bDropHost = TRUE;
            else
                bDropHost = FALSE;
            
            m_Players.erase( i );

            if ( bDropHost )
            {
                // Find a new host
                m_State = STATE_FIND_NEW_HOST;   
                m_HostTimeoutTimer.Stop();                     
                m_inHostTryAddr = m_Players[ 0 ].inAddr;               
            }

            return TRUE;
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


