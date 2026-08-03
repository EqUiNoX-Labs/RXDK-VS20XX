//-----------------------------------------------------------------------------
// File: SimpleXMV.cpp
//
// Desc: Example use of playing XMV content
//
// Hist: 08.19.02 - Created
//       01.15.03 - Simplified code to use CreateDecoderForFile
//       02.13.03 - Updated to remove obsolete struct and allow halting
//                  movie playback.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <xgraphics.h>
#include <xmv.h>




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    CXBFont         m_Font;             // Font object

    HRESULT PlayVideo( const CHAR* strFilename );

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
// Name: CXBoxSample
// Desc: Constructor for CXBoxSample class
//-----------------------------------------------------------------------------
CXBoxSample::CXBoxSample() 
            :CXBApplication()
{
}




//-----------------------------------------------------------------------------
// Name: Initialize
// Desc: Performs initialization
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    // Overwrite default presentation parameters
    m_d3dpp.BackBufferWidth        = 640;
    m_d3dpp.BackBufferHeight       = 480;
    m_d3dpp.BackBufferFormat       = D3DFMT_X8R8G8B8;
    m_d3dpp.BackBufferCount        = 1;
    m_d3dpp.EnableAutoDepthStencil = TRUE;
    m_d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;

    // Create a font
    if( FAILED( m_Font.Create( "Font.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FrameMove
// Desc: Performs per-frame updates
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    // Trigger video playback
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
    {
        if( FAILED( PlayVideo( "D:\\Media\\Videos\\Test.xmv" ) ) )
            OUTPUT_DEBUG_STRING( "Video playback failed!\n" );
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render
// Desc: Renders the scene
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Render()
{
    // Draw a gradient filled background
    RenderGradientBackground( 0xff404040, 0xff404080 );

    // Show text
    m_Font.Begin();
    m_Font.SetScaleFactors( 1.2f, 1.2f );
    m_Font.DrawText( 48, 36, 0xffffffff, L"SimpleXMV" );
    m_Font.SetScaleFactors( 1.0f, 1.0f );
    m_Font.DrawText( 320, 240, 0xffffffff, L"Press " GLYPH_A_BUTTON L" to Play Movie", 
                     XBFONT_CENTER_X | XBFONT_CENTER_Y );
    m_Font.End();

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: MoviePlayerThread()
// Desc: This function is used in a separate thread so that the main thread can
// do other tasks, such as loading data, or just checking for the user to press A.
//-----------------------------------------------------------------------------
DWORD __stdcall MoviePlayerThread( void* pMovieData )
{
    assert( pMovieData );
    XMVDecoder *pDecoder = ( XMVDecoder* )pMovieData;

    // Play the movie
    XMVDecoder_Play( pDecoder, XMVFLAG_NONE, NULL );

    return 0;
}




//-----------------------------------------------------------------------------
// Name: PlayVideo()
// Desc: Plays specified video file.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::PlayVideo( const CHAR* strFilename )
{
    // Greatly simplified function for playing an XMV
    XMVDecoder* pDecoder = NULL;
    HRESULT hr = XMVDecoder_CreateDecoderForFile( 0, strFilename, &pDecoder );
    if( FAILED( hr ) )  
        return hr;

    // Play the video
    XMVVIDEO_DESC VideoDescriptor;
    XMVDecoder_GetVideoDescriptor( pDecoder, &VideoDescriptor );

    // Enable audio if there is any.
    if( VideoDescriptor.AudioStreamCount )
    {
        XMVDecoder_EnableAudioStream( pDecoder, 0, 0, NULL, NULL );
    }

    // Start the movie playing thread - this should always succeed.
    HANDLE hThread = CreateThread( 0, 0, &MoviePlayerThread, pDecoder, 0, 0 );

    // Loop waiting for the user to press A or the movie to exit.
    // Resource loading or other activity can be placed here.
    for ( ;; )
    {
        // Wait a little while, or until the movie thread exits.
        // Can do useful work here.
        DWORD waitResult = WaitForSingleObject( hThread, 1000 / 60 );

        // WAIT_OBJECT_0 means the thread exited and we should exit.
        if ( waitResult == WAIT_OBJECT_0 )
            break;

        // Refresh the input data.
        XBInput_GetInput( g_Gamepads );

        // See if the user has pressed A on any of the controllers.
        for ( int i = 0; i < ( sizeof( g_Gamepads ) / sizeof( g_Gamepads[0] ) ); ++i )
        {
            if( g_Gamepads[i].bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
            {
                // If the user presses A, terminate the currently playing movie.
                // This may take a few hundred milliseconds.
                pDecoder->TerminatePlayback();
                goto exit;
            }
        }
    }

exit:
    // We have to make sure the thread is terminated *before* we close the movie
    // decoder, to make sure it has stopped referencing the movie player object.

    // Wait for the thread to terminate.
    WaitForSingleObject( hThread, INFINITE );

    // Clean up our thread handles to free all thread resources.
    // This has to be done after we finish waiting on the handle.
    CloseHandle( hThread );

    // Free all of the movie data.
    XMVDecoder_CloseDecoder( pDecoder );

    return hr;
}
