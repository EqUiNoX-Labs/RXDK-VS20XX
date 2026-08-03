//-----------------------------------------------------------------------------
// File: Main.cpp
//
// Desc: Simple app to play a streaming sound and render the percent completion
//       of the sound.  Processes synchronously on a worker thread
//
// Hist: 12.15.00 - New for December XDK release
//       05.07.01 - Updated for hardware ADPCM and to spin a worker thread
//       03.06.02 - Added audio level meters for April 02 XDK release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <XBSound.h>
#include <XBUtil.h>
#include "FileStream.h"
#include "dsstdfx.h"


//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_BACK_BUTTON,  XBHELP_PLACEMENT_1, L"Display help" },
    { XBHELP_A_BUTTON,     XBHELP_PLACEMENT_1, L"Pause" },
};

const DWORD NUM_HELP_CALLOUTS = sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]);




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Application class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    CWaveFileStream m_Stream;        // WAV file XMO

    CXBFont         m_Font;          // A font to render text
    CXBHelp         m_Help;          // Help object
    BOOL            m_bDrawHelp;     // Should we draw help?
    HRESULT         m_hrOpenResult;  // Error code from WMAStream::Initialize()
    HANDLE          m_hWorkerThread; // Worker thread
    DWORD           m_dwPercentCompleted;   // Percentage of file processed
    BOOL            m_bPaused;       // Paused?

    LPDIRECTSOUND8  m_pDSound;       // DirectSound object

public:
    virtual HRESULT Initialize();
    virtual HRESULT Render();
    virtual HRESULT FrameMove();

    CXBoxSample();
};




//-----------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program.
//-----------------------------------------------------------------------------
VOID __cdecl main()
{
    CXBoxSample xbApp;
    if( FAILED( xbApp.Create() ) )
        return;
    xbApp.Run();
}




//-----------------------------------------------------------------------------
// Name: CXBoxSample()
// Desc: Initializes member variables
//-----------------------------------------------------------------------------
CXBoxSample::CXBoxSample()
{
}



//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Performs all initialization needed to run the sample
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    // Create the wav file stream
    m_hrOpenResult = m_Stream.Initialize( "D:\\media\\sounds\\becky_xbadpcm.wav", 
                                          &m_dwPercentCompleted );

    // Create a font
    if( FAILED( m_Font.Create( "Font.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create help
    if( FAILED( m_Help.Create( "Gamepad.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    if( FAILED( DirectSoundCreate( NULL, &m_pDSound, NULL ) ) )
        return E_FAIL;

    // download the standard DirectSound effects image
    DSEFFECTIMAGELOC EffectLoc;
    EffectLoc.dwI3DL2ReverbIndex = GraphI3DL2_I3DL2Reverb;
    EffectLoc.dwCrosstalkIndex   = GraphXTalk_XTalk;
    if( FAILED( XAudioDownloadEffectsImage( "d:\\media\\dsstdfx.bin", 
                                            &EffectLoc, 
                                            XAUDIO_DOWNLOADFX_EXTERNFILE, 
                                            NULL ) ) )
        return E_FAIL;

    m_bDrawHelp = FALSE;
    m_bPaused = FALSE;

    m_dwPercentCompleted = 0;

    if( !FAILED( m_hrOpenResult ) )
    {
        // Create worker thread to process audio
        m_hWorkerThread = CreateThread( NULL, 0, WAVFileStreamThreadProc, &m_Stream, 0, NULL );
        if( m_hWorkerThread == NULL )
            return E_FAIL;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame to update state
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    // Toggle help
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_BACK ) 
    {
        m_bDrawHelp = !m_bDrawHelp;
    }

    // Toggle pause
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_A ] )
    {
        m_bPaused = !m_bPaused;
        m_Stream.Pause( m_bPaused ? DSSTREAMPAUSE_PAUSE : DSSTREAMPAUSE_RESUME );
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: 
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Render()
{
    // Render the scene (which is just the progress bar)
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET, 0xff0000ff, 1.0f, 0L );

    // Process the streaming sound
    DirectSoundDoWork();

    // If didn't open the wav file
    if( FAILED( m_hrOpenResult ) )
    {
        WCHAR strFailure[128];
        wsprintfW( strFailure, L"Failed to load wav; HRESULT: 0x%X", m_hrOpenResult );
        m_Font.DrawText( 64,  50, 0xffffffff, strFailure );
    }
    else
    {
        // Show title, frame rate, and help
        if( m_bDrawHelp )
            m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
        else
        {
            // Draw the text
            m_Font.SetScaleFactors( 1.2f, 1.2f );
            m_Font.DrawText( 48, 36, 0xffffffff, L"FileStream" );
            m_Font.SetScaleFactors( 1.0f, 1.0f );
        }
        m_Font.DrawText( 64, 375, 0xffffff00, L"Progress:" );
        if( m_bPaused )
            m_Font.DrawText( 0xff808080, L" (paused)" );

        // Render a simple progress bar to show the percent completed
        struct BACKGROUNDVERTEX { D3DXVECTOR4 p; D3DCOLOR color; };
        BACKGROUNDVERTEX v[8];
        FLOAT x1 =  64, x2 = x1 + (512*m_dwPercentCompleted)/100, x3 = 64+512;
        FLOAT y1 = 400, y2 = y1 + 20;
        v[0].p = D3DXVECTOR4( x1-0.5f, y1-0.5f, 1.0f, 1.0f );  v[0].color = 0xffffff00;
        v[1].p = D3DXVECTOR4( x2-0.5f, y1-0.5f, 1.0f, 1.0f );  v[1].color = 0xffffff00;
        v[2].p = D3DXVECTOR4( x2-0.5f, y2-0.5f, 1.0f, 1.0f );  v[2].color = 0xffffff00;
        v[3].p = D3DXVECTOR4( x1-0.5f, y2-0.5f, 1.0f, 1.0f );  v[3].color = 0xffffff00;
        v[4].p = D3DXVECTOR4( x2-0.5f, y1-0.5f, 1.0f, 1.0f );  v[4].color = 0xff8080ff;
        v[5].p = D3DXVECTOR4( x3-0.5f, y1-0.5f, 1.0f, 1.0f );  v[5].color = 0xff8080ff;
        v[6].p = D3DXVECTOR4( x3-0.5f, y2-0.5f, 1.0f, 1.0f );  v[6].color = 0xff8080ff;
        v[7].p = D3DXVECTOR4( x2-0.5f, y2-0.5f, 1.0f, 1.0f );  v[7].color = 0xff8080ff;

        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_DISABLE );
        m_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW|D3DFVF_DIFFUSE );
        m_pd3dDevice->DrawPrimitiveUP( D3DPT_QUADLIST, 2, v, sizeof(v[0]) );
    }

    // Draw the on-screen audio level meters
    XBSound_DrawLevelMeters( m_pDSound, 64.0f, 340.0f, 60.0f, 30.0f );

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}

