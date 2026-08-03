//-----------------------------------------------------------------------------
// File: MenuOptions.h
//
// Desc: Options menu
//
// Hist: 04.10.01 - New for May XDK release 
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef TECHCERTGAME_MENU_OPTIONS_H
#define TECHCERTGAME_MENU_OPTIONS_H
#include "Common.h"
#include <XBFont.h>
#include <XBInput.h>
#include <XBStopWatch.h>
#include "AudioEngine.h"





//-----------------------------------------------------------------------------
// Name: class MenuOptions
// Desc: Options menu
//-----------------------------------------------------------------------------
class MenuOptions
{
    CXBFont*            m_pFont;
    LPDIRECT3DTEXTURE8  m_pMenuSelTexture;
    CXBStopWatch        m_JoyTimer;
    INT                 m_iCurrIndex;
    BOOL                m_bIsVibrationOn;
    BOOL                m_bIsDrawTitleSafeAreaOn;
    FLOAT               m_fMusicVolume;
    FLOAT               m_fEffectVolume;
    BOOL                m_bExitMenu;
    UINT                m_uSelectedSoundtrack;
    AudioEngine&        m_AudioEngine;

public:

    MenuOptions( CXBFont*, AudioEngine& audioengine );

    VOID     Start( LPDIRECT3DTEXTURE8 );       // Display options menu
    VOID     End();                             // Remove options menu

    HRESULT  FrameMove( const XBGAMEPAD* );
    HRESULT  Render();

    BOOL     ExitMenu() const;

    BOOL     IsVibrationOn() const;
    BOOL     IsDrawTitleSafeAreaOn() const;
    FLOAT    GetMusicVolume() const;
    FLOAT    GetEffectsVolume() const;
    UINT     GetSoundtrack(); 

private:

    // Disabled
    MenuOptions( const MenuOptions& );
    MenuOptions& operator=( const MenuOptions& );

};




#endif // TECHCERTGAME_MENU_OPTIONS_H
