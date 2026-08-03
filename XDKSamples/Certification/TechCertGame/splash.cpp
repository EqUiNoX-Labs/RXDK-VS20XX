//-----------------------------------------------------------------------------
// File: Splash.cpp
//
// Desc: Splash screen
//
// Hist: 04.10.01 - New for May XDK release 
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "Splash.h"
#include "Text.h"
#include <XBApp.h>
#include <XBConfig.h>
#include <XBFont.h>




//-----------------------------------------------------------------------------
// Name: Splash()
// Desc: Constructor
//-----------------------------------------------------------------------------
Splash::Splash( CXBFont* pFont )
{
    m_Timer.Start();
    m_pFont = pFont;
}




//-----------------------------------------------------------------------------
// Name: Start()
// Desc: Start the splash
//-----------------------------------------------------------------------------
VOID Splash::Start()
{
    m_Timer.StartZero();
}




//-----------------------------------------------------------------------------
// Name: End()
// Desc: Quit the splash
//-----------------------------------------------------------------------------
VOID Splash::End()
{
    m_Timer.Stop();
}




//-----------------------------------------------------------------------------
// Name: GetElapsedSeconds()
// Desc: Returns the length of time elapsed since Start() called
//-----------------------------------------------------------------------------
FLOAT Splash::GetElapsedSeconds() const
{
    return m_Timer.GetElapsedSeconds();
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame for animating the splash
//-----------------------------------------------------------------------------
HRESULT Splash::FrameMove( const XBGAMEPAD* )
{
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame for 3d rendering of the splash
//-----------------------------------------------------------------------------
HRESULT Splash::Render()
{
    m_pFont->DrawText( 320, 100, 0xFFFFFFFF, STRING(GAME_NAME),
                       XBFONT_CENTER_X );
    m_pFont->DrawText( 320.0f, 240.0f, 0xFFFFFFFF, STRING(MS_XBOX),
                       XBFONT_CENTER_X | XBFONT_CENTER_Y );

    return S_OK;
}
