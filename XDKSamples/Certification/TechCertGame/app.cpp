//-----------------------------------------------------------------------------
// File: App.cpp
//
// Desc: Technical Certification Requirement Sample Game
//
// Hist: 04.10.01 - New for May XDK release 
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "App.h"
#include "Controller.h"
#include "Text.h"
#include <XbConfig.h>


//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------
const CHAR*  const strDEMO_SCRIPT = "D:\\Media\\Demo.Script";
const FLOAT fSOFT_RESET = 3.0f; // time until reset occurs

// The maximum amount of time for inactivity is 120 seconds. The game
// requires a couple of seconds to start up, so we only wait 100 seconds
// before initializing the game.

const FLOAT fINACTIVE_SECONDS = 100.0f; // time until attract mode begins
const FLOAT fSPLASH_SECONDS = 3.0f;

// vertex format for the title safe box

struct TITLESAFE_BOX_VERTEX
{
    D3DXVECTOR3 v;
    float       fRHW;
    D3DCOLOR    cDiffuse;
}; 

const FLOAT fTitleSafePercentage = 0.85f; 
const DWORD COLOR_WHITE = 0xFFFFFFFF; 

TechCertGame g_xbApp;  // the global application




//-----------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program. Initializes everything, and goes into a
//       message-processing loop. Idle time is used to render the scene.
//-----------------------------------------------------------------------------
VOID __cdecl main()
{
    if( FAILED( g_xbApp.Create() ) )
        return;

    g_xbApp.Run();
}




//-----------------------------------------------------------------------------
// Name: TechCertGame()
// Desc: Constructor
//-----------------------------------------------------------------------------
TechCertGame::TechCertGame()
:
    CXBApplication    (),
    m_Font            (),
    m_AudioEngine     (),
    m_SoundEffect     (),
    m_GameMode        ( GAME_MODE_SPLASH ),     // Game begins in splash mode
    m_LastMode        ( GAME_MODE_MENU ),
    m_Splash          ( &m_Font ),
    m_StartScreen     ( &m_Font ),
    m_Menu            ( &m_Font, m_AudioEngine ),
    m_Demo            ( &m_Font, m_AudioEngine, m_SoundEffect ),
    m_Game            ( &m_Font, m_AudioEngine, m_SoundEffect ),
    m_LoadSave        (),
    m_pdsndDevice     ( NULL ),
    m_dwLaunchStatus  ( 0 ),
    m_dwLaunchDataType( 0 ),
    m_LaunchData      ()
{
    LoadStrings( XGetLanguage() );
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: One time application initialization
//-----------------------------------------------------------------------------
HRESULT TechCertGame::Initialize()
{
    // Set the matrices
    D3DXVECTOR3 vEye(-2.5f, 2.0f, -4.0f );
    D3DXVECTOR3 vAt( 0.0f, 0.0f, 0.0f );
    D3DXVECTOR3 vUp( 0.0f, 1.0f, 0.0f );

    D3DXMATRIX matWorld, matView, matProj;
    D3DXMatrixIdentity( &matWorld );
    D3DXMatrixLookAtLH( &matView, &vEye,&vAt, &vUp );
    D3DXMatrixPerspectiveFovLH( &matProj, D3DX_PI/4, 4.0f/3.0f, 1.0f, 100.0f );

    m_pd3dDevice->SetTransform( D3DTS_WORLD,      &matWorld );
    m_pd3dDevice->SetTransform( D3DTS_VIEW,       &matView );
    m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &matProj );

    // Create the western font
    if( FAILED( m_Font.Create( "Font16.xpr" ) ) )
    {
        OUTPUT_DEBUG_STRING( "Initialize: failed to load fonts\n" );
        return XBAPPERR_MEDIANOTFOUND;
    }

    // Create the DirectSound device
    if( FAILED( DirectSoundCreate(NULL, &m_pdsndDevice, NULL ) ) )
    {
        OUTPUT_DEBUG_STRING( "Initialize: failed to initialize direct sound\n" );
        return E_FAIL;
    }

    // Initialize the Sound Effect Object
    if( FAILED( m_SoundEffect.Initialize( m_pdsndDevice ) ) )
        return E_FAIL;

    // Initialize the audio engine
    m_AudioEngine.Initialize();
    
    // Save launch data
    m_dwLaunchStatus = XGetLaunchInfo( &m_dwLaunchDataType, &m_LaunchData );
    
    // TCR Return Launch Context
    // TCR Launch Context Validation
    // Validate launch data. If the launch data is not a demo launcher,
    // we ignore it.
    if( m_dwLaunchStatus == ERROR_SUCCESS )
    {
        if( m_dwLaunchDataType != LDT_TITLE )
            m_dwLaunchStatus = ERROR_NOT_FOUND;

        PLD_DEMO pLauncher = (PLD_DEMO)(&m_LaunchData);

        if( pLauncher->dwRunmode != XLDEMO_RUNMODE_KIOSKMODE &&
            pLauncher->dwRunmode != XLDEMO_RUNMODE_USERSELECTED )
            m_dwLaunchStatus = ERROR_NOT_FOUND;

        m_DemoTimer.Start();
    }

    // Splash starts
    m_Splash.Start();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//-----------------------------------------------------------------------------
HRESULT TechCertGame::FrameMove()
{
    // Detect controller state
    BOOL bHavePrimaryController = Controller::HavePrimaryController();

    // Access the primary controller (may be NULL)
    const XBGAMEPAD* pGamePad = Controller::GetPrimaryController();
    
    // TCR Demo Timeout
    if( m_dwLaunchStatus == ERROR_SUCCESS )
    {
        PLD_DEMO pLauncher = (PLD_DEMO)(&m_LaunchData);
        if( m_DemoTimer.GetElapsedMilliseconds() > pLauncher->dwTimeout )
        {
            // Return to demo launcher
            PLD_DEMO pLauncher = (PLD_DEMO)(&m_LaunchData);
            XLaunchNewImage( pLauncher->szLauncherXBE, &m_LaunchData );
        }
    }

    // Let the audio engine do some work
    // TODO: Remove DSoundDoWork
    m_AudioEngine.DoWork();
    DirectSoundDoWork();

    switch( m_GameMode )
    {
        case GAME_MODE_GAME:

            if( m_Game.IsPaused() )
            {
                m_Menu.FrameMove( pGamePad );

                if( pGamePad == NULL )
                    break;

                // If we just set the primary controller via the Start button,
                // ignore the button so that the initial Start doesn't 
                // activate a menu item
                if( !bHavePrimaryController &&
                    pGamePad->wPressedButtons & XINPUT_GAMEPAD_START )
                    break;

                // "A" button (or START)
                if( pGamePad->bPressedAnalogButtons[ XINPUT_GAMEPAD_A ] ||
                    pGamePad->wPressedButtons & XINPUT_GAMEPAD_START )
                {
                    switch( m_Menu.GetCurrItem() )
                    {
                        case Menu::MENU_ITEM_RESUME:
                            m_Game.SetPaused( FALSE );
                            m_Menu.End();
                            break;
                        case Menu::MENU_ITEM_SAVE_GAME:
                        {
                            // Generate a save game name
                            WCHAR strGameName[ MAX_GAMENAME ];
                            GetSaveGameName( strGameName );

                            // Reserve space for the world state
                            m_LoadSave.SetGameData( strGameName, 
                                                    m_Game.GetSaveGameSize(),
                                                    m_Game.GetScreenShot() );
                            m_Game.GetSaveGameData( m_LoadSave.GetGameDataPtr() );

                            ChangeMode( GAME_MODE_SAVE );
                            break;
                        }
                        case Menu::MENU_ITEM_LOAD_GAME:
                            if( ConfirmQuit() )
                            {
                                ChangeMode( GAME_MODE_LOAD );
                            }
                            break;
                        case Menu::MENU_ITEM_QUIT:
                            if( ConfirmQuit() )
                            {
                                m_Game.End();
                                ChangeMode( GAME_MODE_MENU );
                            }
                            break;
                    }
                }

                // "B" button
                if( pGamePad->bPressedAnalogButtons[ XINPUT_GAMEPAD_B ] ||
                    pGamePad->wPressedButtons & XINPUT_GAMEPAD_BACK )
                {
                    m_Game.SetPaused( FALSE );
                    m_Menu.End();
                }

                break;
            }

            m_Game.FrameMove( pGamePad, m_fGameTime, m_fElapsedTime );
            m_fGameTime += m_fElapsedTime;

            // TCR In-Game Pause
            // TCR Loss of Controller
            if( pGamePad == NULL ||
                pGamePad->wPressedButtons & XINPUT_GAMEPAD_START )
            {
                // TCR Vibration Function During Pause
                Controller::SetVibration( pGamePad, 0.0f, 0.0f );
                m_Game.SetPaused( TRUE );
                m_Menu.Start( Menu::InGame, m_dwLaunchStatus );
            }

            break;

        case GAME_MODE_MENU:
        
            m_Menu.FrameMove( pGamePad );

            // TCR Initial Interactive Screen Duration
            if( m_Menu.GetInactiveSeconds() > fINACTIVE_SECONDS &&
                m_Menu.GetMenuMode() == Menu::MENU_MODE_MAIN )
                ChangeMode( GAME_MODE_DEMO );

            if( pGamePad == NULL )
                break;

            // If we just set the primary controller via the Start button,
            // ignore the button so that the initial Start doesn't 
            // activate a menu item
            if( !bHavePrimaryController &&
                pGamePad->wPressedButtons & XINPUT_GAMEPAD_START )
                break;

            // "A" button (or START)
            if( pGamePad->bPressedAnalogButtons[ XINPUT_GAMEPAD_A ] ||
                pGamePad->wPressedButtons & XINPUT_GAMEPAD_START )
            {
                switch( m_Menu.GetCurrItem() )
                {
                    case Menu::MENU_ITEM_START:
                        m_Game.SetPaused( FALSE );
                        ChangeMode( GAME_MODE_GAME );
                        break;
                    case Menu::MENU_ITEM_LOAD_GAME:
                        m_Game.SetPaused( FALSE );
                        ChangeMode( GAME_MODE_LOAD );
                        break;
                    case Menu::MENU_ITEM_EXIT:
                    {
                        // TCR Exiting Demo
                        // TCR Returing Launch Content Data
                        // Call back into the demo launcher
                        PLD_DEMO pLauncher = (PLD_DEMO)(&m_LaunchData);
                        XLaunchNewImage( pLauncher->szLauncherXBE, &m_LaunchData );
                        break;
                    }
                }
            }

            break;

        case GAME_MODE_DEMO:

            m_Demo.FrameMove( pGamePad, m_fTime, m_fElapsedTime );

            // Return to start screen when demo complete
            if( m_Demo.IsComplete() )
                ChangeMode( GAME_MODE_START );

            // TCR Attract Mode Interrupt
            // "A" button (or START)
            if( Controller::IsAnyAOrSTARTActive() )
            {
                ChangeMode( GAME_MODE_START );
            }
            break;

        case GAME_MODE_SPLASH:

            m_Splash.FrameMove( pGamePad );

            // If player pressed a controller button or 3 seconds expired, 
            // move on to start screen
            if( Controller::IsAnyButtonActive() || 
                m_Splash.GetElapsedSeconds() > fSPLASH_SECONDS )
            {
                ChangeMode( GAME_MODE_START );
            }

            break;

        case GAME_MODE_START:
        {
            m_StartScreen.FrameMove( pGamePad );

            // TCR Initial Interactive Screen Duration
            if( m_StartScreen.GetElapsedSeconds() > fINACTIVE_SECONDS )
                ChangeMode( GAME_MODE_DEMO );

            if( pGamePad == NULL )
                break;

            // If player pressed the start button, move to the main menu
            if( pGamePad->wPressedButtons & XINPUT_GAMEPAD_START )
                ChangeMode( GAME_MODE_MENU );

            break;
        }
        case GAME_MODE_LOAD:

            m_Game.FrameMove( pGamePad, m_fTime, m_fElapsedTime );
            m_LoadSave.FrameMove( pGamePad );

            if( m_LoadSave.WasCancelled() )
                ChangeMode( m_LastMode );

            if( m_LoadSave.IsGameLoaded() )
            {
                m_Game.End();
                m_Game.SetPaused( FALSE );
                ChangeMode( GAME_MODE_GAME );
            }

            break;

        case GAME_MODE_SAVE:

            m_Game.FrameMove( pGamePad, m_fTime, m_fElapsedTime );
            m_LoadSave.FrameMove( pGamePad );

            if( m_LoadSave.WasCancelled() ||
                m_LoadSave.WasGameSaved() )
            {
                // We don't need the save game information anymore
                m_LoadSave.FreeGameData();
                ChangeMode( GAME_MODE_GAME );
            }

            break;

        default:
            assert( FALSE );
            break;
    }
    
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, the call is the entry point for 3d
//       rendering. This function sets up render states, clears the
//       viewport, and renders the scene.
//-----------------------------------------------------------------------------
HRESULT TechCertGame::Render()
{
    // Clear the viewport, zbuffer, and stencil buffer
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,
                         0x000A0A6A, 1.0f, 0L );

    switch( m_GameMode )
    {
        case GAME_MODE_GAME:    
            m_Game.Render( m_strFrameRate );
            if( m_Game.IsPaused() )
            {
                m_Menu.Render();
                const XBGAMEPAD* pGamePad = Controller::GetPrimaryController();
                if( pGamePad == NULL )
                {
                    m_Font.DrawText( 320, 350, 0xFFFFFFFF, 
                                     STRING(RECONNECT_CNTRLR), XBFONT_CENTER_X );
                    Controller::ClearPrimaryController();
                }
            }
            break;
        case GAME_MODE_MENU:
        {
            m_Menu.Render();
            const XBGAMEPAD* pGamePad = Controller::GetPrimaryController();
            if( pGamePad == NULL )
            {
                m_Font.DrawText( 320, 350, 0xFFFFFFFF, 
                                 STRING(RECONNECT_CNTRLR), XBFONT_CENTER_X );
                Controller::ClearPrimaryController();
            }
            break;
        }
        case GAME_MODE_DEMO:    m_Demo.Render();                    break;
        case GAME_MODE_SPLASH:  m_Splash.Render();                  break;
        case GAME_MODE_START:   m_StartScreen.Render();             break;
        case GAME_MODE_LOAD:    m_LoadSave.Render();                break;
        case GAME_MODE_SAVE:    m_LoadSave.Render();                break;
        default:                assert( FALSE );                    break;
    }
    
    // Render the title safe area box

    DrawTitleSafeAreaBoxIfToggledOn();
    
    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: DrawTitleSafeAreaBox
// Desc: draws the title safe area box on the screen
//-----------------------------------------------------------------------------

VOID TechCertGame::DrawTitleSafeAreaBoxIfToggledOn()
{
    // don't draw the title safe area box if it's not toggled on in the options
    if( !m_Menu.IsDrawTitleSafeAreaOn () )
        return;
    
    D3DDISPLAYMODE d3dDisplayMode;

    ZeroMemory( &d3dDisplayMode, sizeof( d3dDisplayMode ) );
    HRESULT hr = m_pd3dDevice->GetDisplayMode( &d3dDisplayMode );
    assert( SUCCEEDED( hr ) );
    (VOID)hr; // avoid compiler warning

    // Calculate the pixel width / height based on the resolution
    float fScreenWidth = (float) d3dDisplayMode.Width;
    float fScreenHeight = (float) d3dDisplayMode.Height;

    float fBoxWidth = ( fScreenWidth * fTitleSafePercentage );
    float fBoxHeight = ( fScreenHeight * fTitleSafePercentage );

    float x1 = ( fScreenWidth - fBoxWidth ) / 2.0f;
    float x2 = fScreenWidth - x1;
    float y1 = ( fScreenHeight - fBoxHeight ) / 2.0f;
    float y2 = fScreenHeight - y1;

    TITLESAFE_BOX_VERTEX box[4];

    for (INT i = 0; i < 4; i++)
    {
        box[i].v.z = 0.000001f;
        box[i].fRHW = 1.0f/box[i].v.z;
        box[i].cDiffuse = COLOR_WHITE;
    }

    box[0].v.x = x1; box[0].v.y = y2;
    box[1].v.x = x1; box[1].v.y = y1;
    box[2].v.x = x2; box[2].v.y = y1;
    box[3].v.x = x2; box[3].v.y = y2;
 
    m_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW | D3DFVF_DIFFUSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_LIGHTING, FALSE );

    m_pd3dDevice->SetTexture( 0, NULL );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
    m_pd3dDevice->DrawVerticesUP( D3DPT_LINELOOP, 4, box, sizeof( struct TITLESAFE_BOX_VERTEX ) );
}




//-----------------------------------------------------------------------------
// Name: ChangeMode()
// Desc: Switch to new game mode
//-----------------------------------------------------------------------------
VOID TechCertGame::ChangeMode( GameMode iNewMode )
{
    // End the current mode
    switch( m_GameMode )
    {
        case GAME_MODE_GAME:
            // We never end a game in progress at this point, because
            // we might resume it later. Games are ended explicitly
            // when new games are started
            break;
        case GAME_MODE_MENU:    m_Menu.End();           break;
        case GAME_MODE_DEMO:    m_Demo.End();           break;
        case GAME_MODE_SPLASH:  m_Splash.End();         break;
        case GAME_MODE_START:   m_StartScreen.End();    break;
        case GAME_MODE_LOAD:    m_LoadSave.End();       break;
        case GAME_MODE_SAVE:    m_LoadSave.End();       break;
        default:                assert( FALSE );        break;
    }

    // Prepare for the new mode
    switch( iNewMode )
    {
        case GAME_MODE_GAME:
        {
            // Initialize the game
            BOOL bRecordDemo = FALSE; // Change to TRUE to record new demo script
            BOOL bPlayDemo = FALSE;
            m_Game.Start( bRecordDemo, bPlayDemo, m_Menu.IsVibrationOn(), 
                          m_Menu.GetMusicVolume(), m_Menu.GetEffectsVolume(),
                          m_Menu.GetSoundtrack(), strDEMO_SCRIPT );

            // If it's not a new game, load the saved game
            if( m_LoadSave.IsGameLoaded() )
            {
                m_Game.LoadSaveGame( m_LoadSave.GetGameDataPtr(),
                                     m_LoadSave.GetGameDataSize() );

                // We don't need the save game information anymore
                m_LoadSave.FreeGameData();
            }

            m_fGameTime = 0.0f;

            break;
        }
        case GAME_MODE_MENU:    m_Menu.Start( Menu::Normal, 
                                              m_dwLaunchStatus ); break;
        case GAME_MODE_DEMO:    m_Demo.Start( m_Menu.GetMusicVolume(), 
                                              m_Menu.GetEffectsVolume(),
                                              m_Menu.GetSoundtrack() ); break;
        case GAME_MODE_SPLASH:  m_Splash.Start();               break;
        case GAME_MODE_START:   m_StartScreen.Start();          break;

        case GAME_MODE_LOAD:

            m_LoadSave.Start( LoadSave::MODE_LOAD );
            break;

        case GAME_MODE_SAVE:

            m_LoadSave.Start( LoadSave::MODE_SAVE );
            break;

        default: assert( FALSE ); break;
    }

    // Change modes
    m_LastMode = m_GameMode;
    m_GameMode = iNewMode;
}




//-----------------------------------------------------------------------------
// Name: MemUnitWasInserted()
// Desc: TRUE if any memory unit has been inserted since the last time we checked
//-----------------------------------------------------------------------------
BOOL TechCertGame::MemUnitWasInserted() // static
{
    DWORD dwInsertions;
    DWORD dwRemovals;
    CXBMemUnit::GetMemUnitChanges( dwInsertions, dwRemovals );
    if( dwInsertions )
        return TRUE;
    return FALSE;
}




//-----------------------------------------------------------------------------
// Name: GetSaveGameName()
// Desc: Generates a default save game name
//-----------------------------------------------------------------------------
VOID TechCertGame::GetSaveGameName( WCHAR* strGameName ) // static
{
    assert( strGameName != NULL );

    FILETIME ftZulu;
    GetSystemTimeAsFileTime( &ftZulu );

    // Convert to local time
    FILETIME ftTimeLocal;
    FileTimeToLocalFileTime( &ftZulu, &ftTimeLocal );

    // Extract date/time data
    SYSTEMTIME SystemTime;
    FileTimeToSystemTime( &ftTimeLocal, &SystemTime );

    // Determine day of week
    WCHAR strDayOfWeek[32];

    switch( SystemTime.wDayOfWeek )
    {
        case 0: lstrcpyW( strDayOfWeek, STRING(SUNDAY) );    break;
        case 1: lstrcpyW( strDayOfWeek, STRING(MONDAY) );    break;
        case 2: lstrcpyW( strDayOfWeek, STRING(TUESDAY) );   break;
        case 3: lstrcpyW( strDayOfWeek, STRING(WEDNESDAY) ); break;
        case 4: lstrcpyW( strDayOfWeek, STRING(THURSDAY) );  break;
        case 5: lstrcpyW( strDayOfWeek, STRING(FRIDAY) );    break;
        case 6: lstrcpyW( strDayOfWeek, STRING(SATURDAY) );  break;
        default: assert( FALSE ); break;
    }

    // TCR Save Game Descriptive Name
    wsprintfW( strGameName, STRING(GAME_NAME_FORMAT), strDayOfWeek );
}




//-----------------------------------------------------------------------------
// Name: Confirm()
// Desc: Make sure player really wants to leave
//-----------------------------------------------------------------------------
BOOL TechCertGame::ConfirmQuit()
{
    // TCR Player Confirmation of Destructive Actions
    m_Game.Render( m_strFrameRate );

    // Show verification text
    m_Font.DrawText( 320, 240, 0xFFFFFFFF, STRING(CONFIRM_QUIT),
                     XBFONT_CENTER_X | XBFONT_CENTER_Y );

    DrawTitleSafeAreaBoxIfToggledOn();    

    // Show the screen
    g_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    for( ;; )
    {
        XBInput_GetInput();
        const XBGAMEPAD* pGamePad = Controller::GetPrimaryController();
        
        // If controller removed, back out
        if( pGamePad == NULL )
        {        
            return FALSE;
        }
        
        if( pGamePad->bPressedAnalogButtons[ XINPUT_GAMEPAD_A ] ||
            pGamePad->wPressedButtons & XINPUT_GAMEPAD_START )
        {
            return TRUE;
        }

        if( pGamePad->bPressedAnalogButtons[ XINPUT_GAMEPAD_B ] ||
            pGamePad->wPressedButtons & XINPUT_GAMEPAD_BACK )
        {
            return FALSE;
        }
    }
}
