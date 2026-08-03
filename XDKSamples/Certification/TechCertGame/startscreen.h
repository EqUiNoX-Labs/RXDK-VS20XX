//-----------------------------------------------------------------------------
// File: StartScreen.h
//
// Desc: Start screen
//
// Hist: 05.07.01 - New for June XDK release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef TECHCERTGAME_START_H
#define TECHCERTGAME_START_H
#include "Common.h"
#include <XBFont.h>
#include <XBInput.h>
#include <XBStopWatch.h>




//-----------------------------------------------------------------------------
// Name: class StartScreen
// Desc: Start screen
//-----------------------------------------------------------------------------
class StartScreen
{
    CXBFont*     m_pFont;
    CXBStopWatch m_Timer;

public:

    StartScreen( CXBFont* );

    VOID    Start();
    VOID    End();
    FLOAT   GetElapsedSeconds() const;
    HRESULT Render();
    HRESULT FrameMove( const XBGAMEPAD* );

private:

    // Disabled
    StartScreen( const StartScreen& );
    StartScreen& operator=( const StartScreen& );

};




#endif // TECHCERTGAME_START_H
