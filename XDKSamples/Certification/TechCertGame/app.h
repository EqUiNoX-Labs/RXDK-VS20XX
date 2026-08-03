//-----------------------------------------------------------------------------
// File: App.h
//
// Desc: Technical Certification Requirement Sample Game
//
//       Fully playable game, including save/load, multiplayer (future)
//       Meets all technical certification requirements
//       Showcases integrated graphics and audio effects (future)
//       Includes elements of the standard reference UI (future)
//       Stresses both GPU and CPU (future)
//
// Hist: 01.19.01 - New for May XDK release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef TECHCERTGAME_APP_H
#define TECHCERTGAME_APP_H
#include "dsound.h"
#include "dsstdfx.h"
#include "Common.h"
#include <XBApp.h>
#include <XBFont.h>
#include <XBStopWatch.h>
#include "Splash.h"
#include "StartScreen.h"
#include "Demo.h"
#include "LoadSave.h"
#include "Menu.h"
#include "Game.h"




//-----------------------------------------------------------------------------
// Name: class TechCertGame
// Desc: Main application object
//-----------------------------------------------------------------------------
class TechCertGame : public CXBApplication
{
    enum GameMode
    {
        GAME_MODE_GAME,     // game is running
        GAME_MODE_MENU,     // menu subsystem
        GAME_MODE_DEMO,     // demo mode
        GAME_MODE_SPLASH,   // splash screen
        GAME_MODE_START,    // start screen
        GAME_MODE_LOAD,     // load game
        GAME_MODE_SAVE      // save game
    };

    CXBFont           m_Font;
    AudioEngine       m_AudioEngine;
    CSoundEffect      m_SoundEffect;
    GameMode          m_GameMode;
    GameMode          m_LastMode;
    Splash            m_Splash;
    StartScreen       m_StartScreen;
    Menu              m_Menu;
    Demo              m_Demo;
    Game              m_Game;
    LoadSave          m_LoadSave;
    CXBStopWatch      m_DemoTimer;
    LPDIRECTSOUND8    m_pdsndDevice;
    FLOAT             m_fGameTime;
    DWORD             m_dwLaunchStatus;
    DWORD             m_dwLaunchDataType;
    LAUNCH_DATA       m_LaunchData;

public:

    TechCertGame();

    virtual HRESULT Initialize();
    virtual HRESULT FrameMove();
    virtual HRESULT Render();

    VOID    DrawTitleSafeAreaBoxIfToggledOn();
private:

    HRESULT DownloadEffectsImage( PCHAR );
    VOID    ChangeMode( GameMode );

    static BOOL MemUnitWasInserted();
    static VOID GetSaveGameName( WCHAR* );

    BOOL ConfirmQuit();

    // Disabled
    TechCertGame( const TechCertGame& );
    TechCertGame& operator=( const TechCertGame& );

};

#endif // TECHCERTGAME_APP_H
