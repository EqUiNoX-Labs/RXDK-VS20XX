//-----------------------------------------------------------------------------
// File: Menu.h
//
// Desc: Main menu
//
// Hist: 04.10.01 - New for May XDK release 
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef TECHCERTGAME_MENU_H
#define TECHCERTGAME_MENU_H

#include "Common.h"
#include <XBFont.h>
#include <XBInput.h>
#include <XBResource.h>
#include <XBStopWatch.h>
#include "MenuOptions.h"




//-----------------------------------------------------------------------------
// Name: class Menu
// Desc: Main menu
//-----------------------------------------------------------------------------
class Menu
{

public:

    enum MenuItem
    {
        // Main menu
        MENU_ITEM_START,
        MENU_ITEM_LOAD_GAME,
        MENU_ITEM_OPTIONS,
        MENU_ITEM_EXIT,         // return to demo launcher

        // In game menu
        MENU_ITEM_RESUME,
        MENU_ITEM_SAVE_GAME,
        MENU_ITEM_QUIT,         // return to main menu

        MENU_ITEM_MAX
    };

    enum MenuType
    {
        Normal,
        InGame
    };

    enum MenuMode
    {
        MENU_MODE_MAIN,     // main menu
        MENU_MODE_OPTIONS   // options
    };

private:

    CXBPackedResource   m_xprResource;
    LPDIRECT3DTEXTURE8  m_pMenuSelTexture;
    CXBFont*            m_pFont;
    CXBStopWatch        m_InactiveTimer;
    CXBStopWatch        m_JoyTimer;
    MenuOptions         m_Options;
    MenuMode            m_MenuMode;
    MenuType            m_MenuType;
    MenuItem            m_arrMenu[ MENU_ITEM_MAX ];
    const WCHAR*        m_strMenu[ MENU_ITEM_MAX ];
    BOOL                m_bShowItem[ MENU_ITEM_MAX ];
    INT                 m_iCurrIndex;
    INT                 m_iMaxItems;

public:

    Menu( CXBFont*, AudioEngine& );

    VOID     Start( MenuType, DWORD );   // Begin menu display
    VOID     End();  // End menu display

    MenuItem GetCurrItem() const;
    FLOAT    GetInactiveSeconds() const;
    BOOL     StartGame() const;
    BOOL     LoadGame() const;
    MenuMode GetMenuMode() const;
    VOID     ChangeMode( MenuMode );
    VOID     FrameMoveMainMenu( const XBGAMEPAD* );
    VOID     RenderMainMenu();

    HRESULT  FrameMove( const XBGAMEPAD* );
    HRESULT  Render();

    BOOL     IsVibrationOn() const;
    BOOL     IsDrawTitleSafeAreaOn() const;
    FLOAT    GetMusicVolume() const;
    FLOAT    GetEffectsVolume() const;
    UINT     GetSoundtrack();

private:

    // Disabled
    Menu( const Menu& );
    Menu& operator=( const Menu& );
};




#endif // TECHCERTGAME_MENU_H
