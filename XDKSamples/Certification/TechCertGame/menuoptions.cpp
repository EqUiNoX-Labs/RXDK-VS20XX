//-----------------------------------------------------------------------------
// File: MenuOptions.cpp
//
// Desc: Options menu
//
// Hist: 04.10.01 - New for May XDK release 
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "MenuOptions.h"
#include "Controller.h"
#include "File.h"
#include "Text.h"
#include <XBApp.h>
#include <XBConfig.h>
#include <XBFont.h>




//-----------------------------------------------------------------------------
// Local structs
//-----------------------------------------------------------------------------

// Format of the options file
struct Options
{
    BOOL  bIsVibrationOn;
    BOOL  bIsDrawTitleSafeAreaOn;
    FLOAT fMusicVolume;
    FLOAT fEffectVolume;
    UINT  uSelectedSoundtrack;
};




//-----------------------------------------------------------------------------
// Constants
//-----------------------------------------------------------------------------

const CHAR* const strOPTIONS_FILE = "T:\\GameOptions.opt";

// Can't change menu items w/ joystick any faster than this (seconds)
const FLOAT JOY_MIN_MENU_MOVE = 0.2f;

// Joystick must be at least this far away from the center position to register
// ( 0.0f - 1.0f scale )
const FLOAT JOY_THRESHOLD = 0.35f;

// Adjust volume by this amount per frame
const FLOAT fVOLUME_JOY_ADJUST = 1.0f;      // fast adjustment
const FLOAT fVOLUME_DPAD_ADJUST = 0.25f;    // more fine-tuned

// Max volume level (0.0f is minimum)
const FLOAT fVOLUME_MAX = 100.0f;

// Menu items
enum
{
    VIBRATION_INDEX,
    TITLESAFE_INDEX,
    MUSIC_VOLUME_INDEX,
    EFFECT_VOLUME_INDEX,
    SOUNDTRACK_INDEX,
    SAVE_INDEX,

    MENU_ITEM_MAX
};




//-----------------------------------------------------------------------------
// Name: MenuOptions()
// Desc: Constructor
//-----------------------------------------------------------------------------
MenuOptions::MenuOptions( CXBFont* pFont, AudioEngine& audioengine )
:
    m_pMenuSelTexture( NULL ),
    m_iCurrIndex     ( VIBRATION_INDEX ),
    m_bIsVibrationOn ( TRUE ),
    m_fMusicVolume   ( fVOLUME_MAX ),
    m_fEffectVolume  ( fVOLUME_MAX ),
    m_bExitMenu      ( FALSE ),
    m_AudioEngine    ( audioengine ),
    m_uSelectedSoundtrack   ( 0 ),
    m_bIsDrawTitleSafeAreaOn( FALSE )
{
    m_JoyTimer.Start();

    m_pFont = pFont;

#if !defined(XDEMO)
    // TCR Hard Disk Usage
    // Open the options file (if it exists)
    File OptionsFile;
    if( !OptionsFile.Open( strOPTIONS_FILE, GENERIC_READ, 0 ) )
        return;

    // Read option information from disk
    Options options;
    DWORD dwRead;

    ZeroMemory( &options, sizeof( options ) );
    if( !OptionsFile.Read( &options, sizeof( options ), dwRead ) )
        return;

    // Store options
    m_bIsVibrationOn         = options.bIsVibrationOn;
    m_bIsDrawTitleSafeAreaOn = options.bIsDrawTitleSafeAreaOn;
    m_fMusicVolume           = options.fMusicVolume;
    m_fEffectVolume          = options.fEffectVolume;
    m_uSelectedSoundtrack    = options.uSelectedSoundtrack;
#endif    
}




//-----------------------------------------------------------------------------
// Name: Start()
// Desc: Called when option menu is initially displayed
//-----------------------------------------------------------------------------
VOID MenuOptions::Start( LPDIRECT3DTEXTURE8 pMenuSelTexture )
{
    m_pMenuSelTexture = pMenuSelTexture;
    m_iCurrIndex      = VIBRATION_INDEX;
    m_bExitMenu       = FALSE;

    if( m_uSelectedSoundtrack > m_AudioEngine.GetNumberOfSoundtracks() )
        m_uSelectedSoundtrack = 0;
}




//-----------------------------------------------------------------------------
// Name: End()
// Desc: Called when option menu is removed from screen
//-----------------------------------------------------------------------------
VOID MenuOptions::End()
{
    m_pMenuSelTexture = NULL;
    m_JoyTimer.Stop();

#if !defined(XDEMO)
    // TCR Hard Disk Usage
    // If we're exiting because of inactivity (e.g. attract mode is starting)
    // restore the old settings
    if( !m_bExitMenu )
    {
        // Open the options file
        File OptionsFile;
        if( OptionsFile.Open( strOPTIONS_FILE, GENERIC_READ, 0 ) )
        {
            // Read option information from disk
            Options options;
            DWORD dwRead;
            if( OptionsFile.Read( &options, sizeof( options ), dwRead ) )
            {
                // Store options locally
                m_bIsVibrationOn = options.bIsVibrationOn;
                m_fMusicVolume   = options.fMusicVolume;
                m_fEffectVolume  = options.fEffectVolume;
                m_uSelectedSoundtrack = options.uSelectedSoundtrack;
                m_bIsDrawTitleSafeAreaOn = options.bIsDrawTitleSafeAreaOn;
            }
        }
    }
#endif    
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame for animating the menu
//-----------------------------------------------------------------------------
HRESULT MenuOptions::FrameMove( const XBGAMEPAD* pGamePad )
{
    if( pGamePad == NULL )
        return S_OK;

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
                return S_OK;
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
        --m_iCurrIndex;
        if( m_iCurrIndex < 0 )
            m_iCurrIndex = MENU_ITEM_MAX - 1;
    }
    else if( bMenuDown )
    {
        ++m_iCurrIndex;
        if( m_iCurrIndex == MENU_ITEM_MAX )
            m_iCurrIndex = 0;
    }

    // "A" button (or START)
    if( pGamePad->bPressedAnalogButtons[ XINPUT_GAMEPAD_A ] ||
        pGamePad->wPressedButtons & XINPUT_GAMEPAD_START )
    {
        if( m_iCurrIndex == VIBRATION_INDEX )
        {
            // TCR Vibration Option
            // Toggle vibration state
            m_bIsVibrationOn = !m_bIsVibrationOn;
        }
        else if( m_iCurrIndex == TITLESAFE_INDEX )
        {
            // Toggle drawing of title safe area
            m_bIsDrawTitleSafeAreaOn = !m_bIsDrawTitleSafeAreaOn;
        }
        else if( m_iCurrIndex == SAVE_INDEX )
        {
#if !defined(XDEMO)
            // TCR Hard Disk Usage
            // Save option information to disk
            File OptionsFile;
            if( OptionsFile.Create( strOPTIONS_FILE, GENERIC_WRITE, 0 ) )
            {
                Options options;
                options.bIsVibrationOn = m_bIsVibrationOn;            
                options.fMusicVolume   = m_fMusicVolume;
                options.fEffectVolume  = m_fEffectVolume;
                options.bIsDrawTitleSafeAreaOn = m_bIsDrawTitleSafeAreaOn;
                OptionsFile.Write( &options, sizeof( options ) );
            }
#endif            
            m_bExitMenu = TRUE;
        }
    }

    // Handle soundtrack change
    if( ( pGamePad->wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT ) &&
        m_iCurrIndex == SOUNDTRACK_INDEX )
    {
        m_uSelectedSoundtrack = ( m_uSelectedSoundtrack + 1 ) % m_AudioEngine.GetNumberOfSoundtracks();
    }
    else if( ( pGamePad->wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT ) &&
             m_iCurrIndex == SOUNDTRACK_INDEX )
    {
        m_uSelectedSoundtrack = ( m_uSelectedSoundtrack + m_AudioEngine.GetNumberOfSoundtracks() - 1 ) % m_AudioEngine.GetNumberOfSoundtracks();
    }


    // Handle volume control
    FLOAT fAdjust = 0.0f;
    if( pGamePad->fX2 > JOY_THRESHOLD )
    {
        fAdjust = fVOLUME_JOY_ADJUST;
    }
    else if( pGamePad->fX2 < -JOY_THRESHOLD )
    {
        fAdjust = -fVOLUME_JOY_ADJUST;
    }
    else if( pGamePad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
    {
        fAdjust = fVOLUME_DPAD_ADJUST;
    }
    else if( pGamePad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT )
    {
        fAdjust = -fVOLUME_DPAD_ADJUST;
    }

    if( fAdjust != 0.0f )
    {
        switch( m_iCurrIndex )
        {
            // TCR Game Soundtrack Volume Control
            case MUSIC_VOLUME_INDEX:
                m_fMusicVolume += fAdjust;
                break;
            case EFFECT_VOLUME_INDEX:
                m_fEffectVolume += fAdjust;
                break;
        }

        // Clamp
        if( m_fMusicVolume > fVOLUME_MAX )
            m_fMusicVolume = fVOLUME_MAX;
        if( m_fEffectVolume > fVOLUME_MAX )
            m_fEffectVolume = fVOLUME_MAX;

        if( m_fMusicVolume < 0.0f )
            m_fMusicVolume = 0.0f;
        if( m_fEffectVolume < 0.0f )
            m_fEffectVolume = 0.0f;
    }

    // "B" button cancels
    if( pGamePad->bPressedAnalogButtons[ XINPUT_GAMEPAD_B ] ||
        pGamePad->wPressedButtons & XINPUT_GAMEPAD_BACK )
    {
        // Restore default settings
        m_bIsVibrationOn = TRUE;
        m_bIsDrawTitleSafeAreaOn = FALSE;
        m_fMusicVolume = m_fEffectVolume = fVOLUME_MAX;

#if !defined(XDEMO)
        // TCR Hard Disk Usage
        // Open the options file
        File OptionsFile;
        if( OptionsFile.Open( strOPTIONS_FILE, GENERIC_READ, 0 ) )
        {
            // Read option information from disk
            Options options;
            DWORD dwRead;
            if( OptionsFile.Read( &options, sizeof( options ), dwRead ) )
            {
                // Store options locally
                m_bIsVibrationOn = options.bIsVibrationOn;               
                m_fMusicVolume   = options.fMusicVolume;
                m_fEffectVolume  = options.fEffectVolume;
                m_uSelectedSoundtrack = options.uSelectedSoundtrack;
                m_bIsDrawTitleSafeAreaOn = options.bIsDrawTitleSafeAreaOn;
            }
        }
#endif        
        m_bExitMenu = TRUE;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame for 3d rendering of the menu
//-----------------------------------------------------------------------------
HRESULT MenuOptions::Render()
{
    // Game name
    m_pFont->DrawText( 320, 50, 0xFFFFFFFF, STRING(GAME_NAME),
                       XBFONT_CENTER_X );

    // Game options
    m_pFont->DrawText( 320, 100, 0xFFFFFFFF, STRING(MENU_OPTIONS),
                       XBFONT_CENTER_X );

    const WCHAR* strMenu[ MENU_ITEM_MAX ] =
    {
        STRING(MENU_VIBRATION),
        NULL,                           // title safe is a special-case
        STRING(MENU_MUSIC_VOLUME),
        STRING(MENU_EFFECT_VOLUME),
        STRING(MENU_SOUNDTRACK),
        STRING(MENU_SAVE_OPTIONS)
    };

    const DWORD dwHighlight = 0xFFFFFF00; // Yellow
    const DWORD dwNormal    = 0xFFFFFFFF;

    FLOAT fYtop = 150.0f;
    FLOAT fYdelta = 37.0f;
    INT iRow = 0; 
    INT iSelectedRow = 0;

    // Show menu
    for( INT i = 0; i < MENU_ITEM_MAX; ++i, ++iRow )
    {
        DWORD dwColor = ( m_iCurrIndex == i ) ? dwHighlight : dwNormal;
        
        if( m_iCurrIndex == i )
        {
            iSelectedRow = iRow;
        }
        
        if( i != TITLESAFE_INDEX )
        {
            m_pFont->DrawText( 140, fYtop + (fYdelta * iRow), dwColor, strMenu[i] );
        }

        switch( i )
        {
            case VIBRATION_INDEX:
                m_pFont->DrawText( 360, fYtop + (fYdelta * iRow), dwColor, 
                                   m_bIsVibrationOn ? STRING(ON) : STRING(OFF) );
                break;
            case TITLESAFE_INDEX:
                // we may need to break it up into two lines
                m_pFont->DrawText( 140, fYtop + (fYdelta * iRow), dwColor, 
                                STRING(MENU_TITLESAFE_L1) );
                m_pFont->DrawText( 160, fYtop + (fYdelta * (iRow + 1)), dwColor, 
                                STRING(MENU_TITLESAFE_L2) );
                m_pFont->DrawText( 360, fYtop + (fYdelta * iRow), dwColor, 
                                   m_bIsDrawTitleSafeAreaOn ? STRING(ON) : STRING(OFF) );
                
                // all languages but english span two lines                                                                              
                if( STRING(MENU_TITLESAFE_L2)[ 0 ] != '\0' )
                    iRow++; // add an extra line
                break;
            case MUSIC_VOLUME_INDEX:
            {
                struct BACKGROUNDVERTEX
                { 
                    D3DXVECTOR4 p;
                    D3DCOLOR color;
                };
                BACKGROUNDVERTEX v[4];
                FLOAT x1 = 360.0f;
                FLOAT x2 = x1 + (150.0f * m_fMusicVolume) / 100.0f;
                FLOAT y1 = fYtop + (fYdelta * iRow);
                FLOAT y2 = y1 + 20.0f;
                v[0].p = D3DXVECTOR4( x1 - 0.5f, y1 - 0.5f, 1.0f, 1.0f );  v[0].color = 0xffffffff;
                v[1].p = D3DXVECTOR4( x2 - 0.5f, y1 - 0.5f, 1.0f, 1.0f );  v[1].color = 0xffffffff;
                v[2].p = D3DXVECTOR4( x1 - 0.5f, y2 - 0.5f, 1.0f, 1.0f );  v[2].color = 0xff00ff00;
                v[3].p = D3DXVECTOR4( x2 - 0.5f, y2 - 0.5f, 1.0f, 1.0f );  v[3].color = 0xff00ff00;

                g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, 
                                                    D3DTOP_DISABLE );
                g_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW | D3DFVF_DIFFUSE );
                g_pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, v, 
                                               sizeof(v[0]) );

                WCHAR strPercent[32];
                wsprintfW( strPercent, L"%d %%", INT(m_fMusicVolume) );
                m_pFont->DrawText( 520, fYtop + (fYdelta * iRow), dwColor, strPercent );
                break;
            }

            case EFFECT_VOLUME_INDEX:
            {
                struct BACKGROUNDVERTEX
                { 
                    D3DXVECTOR4 p;
                    D3DCOLOR color;
                };
                BACKGROUNDVERTEX v[4];
                FLOAT x1 = 360.0f;
                FLOAT x2 = x1 + (150.0f * m_fEffectVolume) / 100.0f;
                FLOAT y1 = fYtop + (fYdelta * iRow);
                FLOAT y2 = y1 + 20.0f;
                v[0].p = D3DXVECTOR4( x1 - 0.5f, y1 - 0.5f, 1.0f, 1.0f );  v[0].color = 0xffffffff;
                v[1].p = D3DXVECTOR4( x2 - 0.5f, y1 - 0.5f, 1.0f, 1.0f );  v[1].color = 0xffffffff;
                v[2].p = D3DXVECTOR4( x1 - 0.5f, y2 - 0.5f, 1.0f, 1.0f );  v[2].color = 0xff00ff00;
                v[3].p = D3DXVECTOR4( x2 - 0.5f, y2 - 0.5f, 1.0f, 1.0f );  v[3].color = 0xff00ff00;

                g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, 
                                                    D3DTOP_DISABLE );
                g_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW | D3DFVF_DIFFUSE );
                g_pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, v, 
                                               sizeof(v[0]) );

                WCHAR strPercent[32];
                wsprintfW( strPercent, L"%d %%", INT(m_fEffectVolume) );
                m_pFont->DrawText( 520, fYtop + (fYdelta * iRow), dwColor, strPercent );
                break;
            }

            case SOUNDTRACK_INDEX:
            {
                WCHAR strSoundtrackName[ MAX_SOUNDTRACK_NAME ];
                const CSoundtrack& sndtrk = m_AudioEngine.GetSoundtrack( m_uSelectedSoundtrack );

                sndtrk.GetSoundtrackName( strSoundtrackName );

                m_pFont->DrawText( 360, fYtop + (fYdelta * iRow), dwColor, strSoundtrackName );
                break;
            }
        }
    }

    // Show selected item with arrow (using the ">>" unicode character)
    FLOAT fTop = fYtop + (fYdelta * iSelectedRow );
    m_pFont->DrawText( 100, fTop, dwHighlight, L"\273" );
    
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: ExitMenu()
// Desc: Returns TRUE if time to leave the options menu
//-----------------------------------------------------------------------------
BOOL MenuOptions::ExitMenu() const
{
    return m_bExitMenu;
}




//-----------------------------------------------------------------------------
// Name: IsVibrationOn()
// Desc: Returns status of vibration
//-----------------------------------------------------------------------------
BOOL MenuOptions::IsVibrationOn() const
{
    return m_bIsVibrationOn;
}


//-----------------------------------------------------------------------------
// Name: IsDrawTitleSafeAreaOn()
// Desc: Returns whether we are drawing the title safe rectangle
//-----------------------------------------------------------------------------
BOOL MenuOptions::IsDrawTitleSafeAreaOn() const
{
    return m_bIsDrawTitleSafeAreaOn;
}


//-----------------------------------------------------------------------------
// Name: GetMusicVolume()
// Desc: Returns status of music volume
//-----------------------------------------------------------------------------
FLOAT MenuOptions::GetMusicVolume() const
{
    return m_fMusicVolume;
}




//-----------------------------------------------------------------------------
// Name: GetEffectsVolume()
// Desc: Returns status of effects volume
//-----------------------------------------------------------------------------
FLOAT MenuOptions::GetEffectsVolume() const
{
    return m_fEffectVolume;
}




//-----------------------------------------------------------------------------
// Name: GetSoundtrack()
// Desc: Returns the index of the selected soundtrack
//-----------------------------------------------------------------------------
UINT MenuOptions::GetSoundtrack()
{
    // We have to do this check here, because when MenuOptions
    // is constructed, the audio engine has not yet been
    // initialized.
    if( m_uSelectedSoundtrack >= m_AudioEngine.GetNumberOfSoundtracks() )
        m_uSelectedSoundtrack = 0;

    return m_uSelectedSoundtrack;
}
