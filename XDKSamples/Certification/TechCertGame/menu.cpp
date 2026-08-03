//-----------------------------------------------------------------------------
// File: Menu.cpp
//
// Desc: Main menu and ingame menu
//
// Hist: 04.10.01 - New for May XDK release 
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "Menu.h"
#include <XbApp.h>
#include <XbConfig.h>
#include <XbFont.h>
#include "Controller.h"
#include "App.h"
#include "Text.h"




// The following header file is generated from "MenuResource.rdf" file
// using the Bundler tool. In addition to the header, the tool outputs a binary
// file (MenuResource.xpr) which contains compiled (i.e. bundled) resources
// and is loaded at runtime using the CXBPackedResource class.
#include "MenuResource.h"




//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

// Can't change menu items w/ joystick any faster than this (seconds)
const FLOAT JOY_MIN_MENU_MOVE = 0.2f;

// Joystick must be at least this far away from the center position to register
// ( 0.0f - 1.0f scale )
const FLOAT JOY_THRESHOLD = 0.35f;




//-----------------------------------------------------------------------------
// Name: Menu()
// Desc: Constructor
//-----------------------------------------------------------------------------
Menu::Menu( CXBFont* pFont, AudioEngine& audioengine )
:
    m_xprResource  (),
    m_pMenuSelTexture( NULL ),
    m_Options      ( pFont, audioengine ),
    m_MenuMode     ( MENU_MODE_MAIN ),
    m_MenuType     ( Normal ),
    m_arrMenu      (),
    m_strMenu      (),
    m_bShowItem    (),
    m_iCurrIndex   ( 0 ),
    m_iMaxItems    ( 0 )
{
    m_pFont = pFont;
    m_InactiveTimer.Start();
    m_JoyTimer.Start();
}




//-----------------------------------------------------------------------------
// Name: Start()
// Desc: Called when menu is initially displayed
//-----------------------------------------------------------------------------
VOID Menu::Start( MenuType menuType, DWORD dwLaunchStatus )
{
    m_MenuMode = MENU_MODE_MAIN;
    m_MenuType = menuType;
    m_iCurrIndex = 0;

    switch( m_MenuType )
    {
        case Normal:
        {
            m_arrMenu[ 0 ] = MENU_ITEM_START;
            m_arrMenu[ 1 ] = MENU_ITEM_LOAD_GAME;
            m_arrMenu[ 2 ] = MENU_ITEM_OPTIONS;
            m_arrMenu[ 3 ] = MENU_ITEM_EXIT;
            m_strMenu[ 0 ] = STRING(MENU_START);
            m_strMenu[ 1 ] = STRING(MENU_LOAD);
            m_strMenu[ 2 ] = STRING(MENU_OPTIONS);
            m_strMenu[ 3 ] = STRING(MENU_EXIT);
#if defined(XDEMO)            
            m_bShowItem[ 0 ] = TRUE;
            m_bShowItem[ 1 ] = FALSE;
            m_bShowItem[ 2 ] = TRUE;
            
            // C13-9 Exiting Demo
            // Show the exit menu item if we were launched from a demo launcher
            // or anything else that we can return to
            m_bShowItem[ 3 ] = dwLaunchStatus == ERROR_SUCCESS;
#else
            m_bShowItem[ 0 ] = TRUE;
            m_bShowItem[ 1 ] = TRUE;
            m_bShowItem[ 2 ] = TRUE;
            m_bShowItem[ 3 ] = FALSE;
#endif            
            m_iMaxItems = 4;
            break;
        }
        case InGame:
            m_arrMenu[ 0 ] = MENU_ITEM_RESUME;
            m_arrMenu[ 1 ] = MENU_ITEM_SAVE_GAME;
            m_arrMenu[ 2 ] = MENU_ITEM_LOAD_GAME;
            m_arrMenu[ 3 ] = MENU_ITEM_QUIT;
            m_strMenu[ 0 ] = STRING(MENU_RESUME);
            m_strMenu[ 1 ] = STRING(MENU_SAVE);
            m_strMenu[ 2 ] = STRING(MENU_LOAD);
            m_strMenu[ 3 ] = STRING(MENU_QUIT);
#if defined(XDEMO)            
            m_bShowItem[ 0 ] = TRUE;
            m_bShowItem[ 1 ] = FALSE;
            m_bShowItem[ 2 ] = FALSE;
            m_bShowItem[ 3 ] = TRUE;
#else
            m_bShowItem[ 0 ] = TRUE;
            m_bShowItem[ 1 ] = TRUE;
            m_bShowItem[ 2 ] = TRUE;
            m_bShowItem[ 3 ] = TRUE;
#endif            
            m_iMaxItems = 4;
            break;
        default:
            assert( FALSE );
            break;
    }

    m_InactiveTimer.StartZero();

    // Load our textures
    if( FAILED( m_xprResource.Create( "MenuResource.xpr", MenuResource_NUM_RESOURCES ) ) )
    {
        OUTPUT_DEBUG_STRING( "Menu::Start: failed to load textures\n");
        return;
    }

    // Load our textures from the bundled resource
    m_pMenuSelTexture = m_xprResource.GetTexture( MenuResource_MenuSelect_OFFSET );
}




//-----------------------------------------------------------------------------
// Name: End()
// Desc: Called when menu is no longer displayed
//-----------------------------------------------------------------------------
VOID Menu::End()
{
    m_xprResource.Destroy();
    m_InactiveTimer.Stop();
    m_JoyTimer.Stop();
    m_Options.End();
}




//-----------------------------------------------------------------------------
// Name: GetCurrItem()
// Desc: Returns the currently selected menu item
//-----------------------------------------------------------------------------
Menu::MenuItem Menu::GetCurrItem() const
{
    return m_arrMenu[ m_iCurrIndex ];
}




//-----------------------------------------------------------------------------
// Name: GetInactiveSeconds()
// Desc: Returns the length of time that the menu has been inactive (no button
//       presses, etc.)
//-----------------------------------------------------------------------------
FLOAT Menu::GetInactiveSeconds() const
{
    return m_InactiveTimer.GetElapsedSeconds();
}




//-----------------------------------------------------------------------------
// Name: GetMenuMode()
// Desc: Get current menu
//-----------------------------------------------------------------------------
Menu::MenuMode Menu::GetMenuMode() const
{
    return m_MenuMode;
}




//-----------------------------------------------------------------------------
// Name: ChangeMode()
// Desc: Switch to new menu mode
//-----------------------------------------------------------------------------
VOID Menu::ChangeMode( MenuMode iNewMode )
{
    // End the current mode
    switch( m_MenuMode )
    {
        case MENU_MODE_MAIN:                        break;
        case MENU_MODE_OPTIONS: m_Options.End();    break;
        default:                assert( FALSE );    break;
    }

    // Start the new mode
    switch( iNewMode )
    {
        case MENU_MODE_MAIN:    break;
        case MENU_MODE_OPTIONS: m_Options.Start( m_pMenuSelTexture );  break;
        default:                assert( FALSE ); break;
    }

    m_MenuMode = iNewMode;
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame for animating the menu
//-----------------------------------------------------------------------------
HRESULT Menu::FrameMove( const XBGAMEPAD* pGamePad )
{
    if( pGamePad == NULL )
        return S_OK;

    // If any button is active, then reset the inactive timer
    if( Controller::IsAnyButtonActive( pGamePad ) )
        m_InactiveTimer.StartZero();

    switch( m_MenuMode )
    {
        case MENU_MODE_MAIN:
            
            FrameMoveMainMenu( pGamePad );
            break;

        case MENU_MODE_OPTIONS:

            m_Options.FrameMove( pGamePad );
            if( m_Options.ExitMenu() )
                ChangeMode( MENU_MODE_MAIN );
            break;

        default:

            assert( FALSE );
            break;

    }
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame for 3d rendering of the menu
//-----------------------------------------------------------------------------
HRESULT Menu::Render()
{
    switch( m_MenuMode )
    {
        case MENU_MODE_MAIN:        RenderMainMenu();       break;
        case MENU_MODE_OPTIONS:     m_Options.Render();     break;
        default:                    assert( FALSE );        break;
    }
    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: FrameMoveMainMenu()
// Desc: Called once per frame for animating the main menu
//-----------------------------------------------------------------------------
VOID Menu::FrameMoveMainMenu( const XBGAMEPAD* pGamePad )
{
    if( pGamePad == NULL )
        return;

    // Detect menu change
    BOOL bMenuUp( FALSE );
    BOOL bMenuDown( FALSE );

    // Is the joystick active
    if( pGamePad->fY1 > JOY_THRESHOLD ||
        pGamePad->fY1 < -JOY_THRESHOLD )
    {
        // If we've previously registered a joystick menu move,
        // ignore the joystick until JOY_MIN_MENU_MOVE seconds
        // has elapsed
        if( m_JoyTimer.IsRunning() )
        {
            if( m_JoyTimer.GetElapsedSeconds() < JOY_MIN_MENU_MOVE )
                return;
            else
                m_JoyTimer.StartZero();
        }
        else
        {
            m_JoyTimer.StartZero();
        }

        if( pGamePad->fY1 > JOY_THRESHOLD )
            bMenuUp = TRUE;
        else
            bMenuDown = TRUE;
    }
    else
    {
        m_JoyTimer.Stop();
    }

    // Gamepad also moves menu cursor
    // TCR Menu Navigation
    bMenuUp   = bMenuUp   || pGamePad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP;
    bMenuDown = bMenuDown || pGamePad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN;

    if( bMenuUp )
    {
        do
        {
            --m_iCurrIndex;
            if( m_iCurrIndex < 0 )
                m_iCurrIndex = m_iMaxItems - 1;
        } while( !m_bShowItem[ m_iCurrIndex ] );
    }
    else if( bMenuDown )
    {
        do
        {
            ++m_iCurrIndex;
            if( m_iCurrIndex == m_iMaxItems )
                m_iCurrIndex = 0;
        } while( !m_bShowItem[ m_iCurrIndex ] );
    }

    // "A" button (or START)
    if( pGamePad->bPressedAnalogButtons[ XINPUT_GAMEPAD_A ] ||
        pGamePad->wPressedButtons & XINPUT_GAMEPAD_START )
    {
        switch( m_arrMenu[ m_iCurrIndex ] )
        {
            case MENU_ITEM_OPTIONS:
                ChangeMode( MENU_MODE_OPTIONS );
                break;
            default:
                break;
        }
    }
}




//-----------------------------------------------------------------------------
// Name: RenderMainMenu()
// Desc: Called once per frame for 3d rendering of the menu
//-----------------------------------------------------------------------------
VOID Menu::RenderMainMenu()
{
    if( m_MenuType == Normal )
    {
        m_pFont->DrawText( 320, 100, 0xFFFFFFFF, STRING(GAME_NAME),
                           XBFONT_CENTER_X );
    }

    const DWORD dwHighlight = 0xffffff00; // Yellow
    const DWORD dwNormal    = 0xffffffff;

    FLOAT fYtop = 150.0f;
    FLOAT fYdelta = 50.0f;

    // Show menu
    INT j = 0;
    for( INT i = 0; i < m_iMaxItems; ++i )
    {
        DWORD dwColor = ( m_iCurrIndex == i ) ? dwHighlight : dwNormal;

        // If item can't be shown in demo mode, skip it
        if( m_bShowItem[ i ] )
        {
            m_pFont->DrawText( 260, fYtop + (fYdelta * j), dwColor, m_strMenu[i] );

            // Show selected item with arrow (using the ">>" unicode character)
            if( m_iCurrIndex == i )
            {
                FLOAT fTop = fYtop + (fYdelta * j );
                m_pFont->DrawText( 220, fTop, dwHighlight, L"\273" );
            }
            ++j;
        }
    }
}




//-----------------------------------------------------------------------------
// Name: IsVibrationOn()
// Desc: Returns status of vibration
//-----------------------------------------------------------------------------
BOOL Menu::IsVibrationOn() const
{
    return m_Options.IsVibrationOn();
}


//-----------------------------------------------------------------------------
// Name: IsDrawTitleSafeAreaOn()
// Desc: Returns status of whether we draw the title safe area box
//-----------------------------------------------------------------------------
BOOL Menu::IsDrawTitleSafeAreaOn() const
{   
    return m_Options.IsDrawTitleSafeAreaOn();
}



//-----------------------------------------------------------------------------
// Name: GetMusicVolume()
// Desc: Returns status of music volume
//-----------------------------------------------------------------------------
FLOAT Menu::GetMusicVolume() const
{
    return m_Options.GetMusicVolume();
}




//-----------------------------------------------------------------------------
// Name: GetEffectsVolume()
// Desc: Returns status of effects volume
//-----------------------------------------------------------------------------
FLOAT Menu::GetEffectsVolume() const
{
    return m_Options.GetEffectsVolume();
}




//-----------------------------------------------------------------------------
// Name: GetSoundtrack()
// Desc: Returns the index of the selected soundtrack
//-----------------------------------------------------------------------------
UINT Menu::GetSoundtrack()
{
    return m_Options.GetSoundtrack();
}
