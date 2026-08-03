//-----------------------------------------------------------------------------
// File: Demo.h
//
// Desc: Demo mode
//
// Hist: 04.10.01 - New for May XDK release 
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef TECHCERTGAME_DEMO_H
#define TECHCERTGAME_DEMO_H
#include "Common.h"
#include "Game.h"
#include <XBInput.h>
#include <XBStopWatch.h>




//-----------------------------------------------------------------------------
// Name: class Demo
// Desc: Demo mode
//-----------------------------------------------------------------------------
class Demo
{
    CXBFont*     m_pFont;
    CXBStopWatch m_Timer;
    Game         m_GameDemo;

public:

    Demo( CXBFont*, AudioEngine& audioengine, CSoundEffect& sndeffect );

    VOID    Start( FLOAT fMusicVolume, FLOAT fEffectVolume, UINT uSelectedSoundtrack ); // Begin demo
    VOID    End();   // End demo

    HRESULT FrameMove( const XBGAMEPAD*, FLOAT fTime, FLOAT fElapsedTime );
    HRESULT Render();

    BOOL    IsComplete() const; // TRUE when demo over

private:

    // Disabled
    Demo( const Demo& );
    Demo& operator=( const Demo& );

};




#endif // TECHCERTGAME_DEMO_H
