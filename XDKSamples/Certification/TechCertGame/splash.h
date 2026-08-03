//-----------------------------------------------------------------------------
// File: Splash.h
//
// Desc: Splash screen
//
// Hist: 01.24.01 - New for May XDK release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef TECHCERTGAME_SPLASH_H
#define TECHCERTGAME_SPLASH_H
#include "Common.h"
#include <XBFont.h>
#include <XBInput.h>
#include <XBStopWatch.h>




//-----------------------------------------------------------------------------
// Name: class Splash
// Desc: Splash screen
//-----------------------------------------------------------------------------
class Splash
{

    CXBFont*     m_pFont;
    CXBStopWatch m_Timer;

public:

    Splash( CXBFont* );

    VOID    Start();
    VOID    End();
    FLOAT   GetElapsedSeconds() const;
    HRESULT Render();
    HRESULT FrameMove( const XBGAMEPAD* );

private:

    // Disabled
    Splash( const Splash& );
    Splash& operator=( const Splash& );

};




#endif // TECHCERTGAME_SPLASH_H
