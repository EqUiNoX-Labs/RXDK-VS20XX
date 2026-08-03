//-----------------------------------------------------------------------------
// File: StartScreen.cpp
//
// Desc: Start screen
//
// Hist: 05.07.01 - New for June XDK release 
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "StartScreen.h"
#include "Controller.h"
#include "Text.h"
#include <XBApp.h>
#include <XBConfig.h>
#include <XBFont.h>




//-----------------------------------------------------------------------------
// Name: StartScreen()
// Desc: Constructor
//-----------------------------------------------------------------------------
StartScreen::StartScreen( CXBFont* pFont )
{
    m_Timer.Start();
    m_pFont = pFont;
}




//-----------------------------------------------------------------------------
// Name: Start()
// Desc: Start the start screen
//-----------------------------------------------------------------------------
VOID StartScreen::Start()
{
    m_Timer.StartZero();
}




//-----------------------------------------------------------------------------
// Name: End()
// Desc: Quit the start screen
//-----------------------------------------------------------------------------
VOID StartScreen::End()
{
    m_Timer.Stop();
}




//-----------------------------------------------------------------------------
// Name: GetElapsedSeconds()
// Desc: Returns the length of time elapsed since Start() called
//-----------------------------------------------------------------------------
FLOAT StartScreen::GetElapsedSeconds() const
{
    return m_Timer.GetElapsedSeconds();
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame for animating the start screen
//-----------------------------------------------------------------------------
HRESULT StartScreen::FrameMove( const XBGAMEPAD* )
{
    // Allow any controller to become the primary controller.
    // We do this on every loop because we're waiting for the Start button
    // to indicate the primary controller
    Controller::ClearPrimaryController();
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame for 3d rendering of the start screen
//-----------------------------------------------------------------------------
HRESULT StartScreen::Render()
{
    m_pFont->DrawText( 320, 100, 0xffffffff, STRING(GAME_NAME),
                       XBFONT_CENTER_X );
    m_pFont->DrawText( 320, 200, 0xffffffff, STRING(INTRO),
                       XBFONT_CENTER_X | XBFONT_CENTER_Y );

    // Press START
    m_pFont->DrawText( 320, 300, 0xffffffff, STRING(PRESS_START),
                       XBFONT_CENTER_X );

    return S_OK;
}
