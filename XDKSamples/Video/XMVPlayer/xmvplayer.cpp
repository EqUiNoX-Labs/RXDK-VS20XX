//-----------------------------------------------------------------------------
// File: XMVPlayer.cpp
//
// Desc: Example use of playing XMV content
//
// Hist: 2.7.03 - Created
//
//          XMV playback has a few principle variations:
//              Playing to a texture or to the overlay planes
//              Using the packet interface to read the file, using packets to copy from
//              memory, or letting XMV do the reading.
//              Using Play() to play the entire movie, or GetNextFrame
//              Unpacking to an RGB or YUV texture
//              Playing full screen or on just part of the screen
//
//          If these could be combined arbitrarily this would give us dozens of combinations.
//          A few of the combinations don't make sense - overlay planes are always YUV,
//          Play() always uses the overlay planes, etc.
//
//          Many of these variations - such as using CreateDecoderForFile versus
//          CreateDecoderForPackets - do not affect other aspects of playback, so the
//          different variations can be mixed without difficulty.
//
//          This sample tries to show all sensible combinations of these possibilities
//
//          When playing to an overlay plane this sample also demonstrates placing other
//          graphics above the movie being played.
//
//          The playback logic is encapsulated in the XMVHelper class, so the actual
//          playback process is pretty simple.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <xgraphics.h>

#include <xmv.h>
#include "XMVHelper.h"




//-----------------------------------------------------------------------------
// Name: g_colorKey
// Desc: Used when color keying is enabled
//-----------------------------------------------------------------------------
const D3DCOLOR g_colorKey = D3DCOLOR_ARGB( 0xFF, 0x3F, 0x00, 0x3F );

//-----------------------------------------------------------------------------
// Name: g_fullScreenRect and g_partialScreenRect
// Desc: Rectangles to use when playing movies full or partial screen.
//-----------------------------------------------------------------------------
const RECT g_fullScreenRect = { 0, 0, 640, 480 };
const RECT g_partialScreenRect = { 160, 120, 480, 360 };




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    enum READ_METHOD
    {
        READ_FROM_FILE,     // Read from a file using CreateDecoderForFile
        READ_FROM_PACKETS,  // Read from a file using CreateDecoderForPackets
        READ_FROM_MEMORY,   // Read from a block of memory
        READ_METHOD_COUNT   // Number of READ_METHOD enums
    };

    CXBFont         m_Font;                 // Font object
    CXMVPlayer      m_player;               // Movie player object
    READ_METHOD     m_readMethod;           // Controls how the movie data is read.
    BOOL            m_bYUV;                 // Unpack to a YUV or RGB texture - using an overlay requires YUV
    BOOL            m_bUseTextures;         // Render to a texture or an overlay - using Play() requires an overlay
    BOOL            m_bUsePlay;             // Use the Play() or GetNextFrame() method to play the movie
    BOOL            m_bUseColorKey;         // Use a colorkey - requires an overlay
    BOOL            m_bFullScreen;          // Should we play full screen or just partial?

    HRESULT PlayVideoWithPlay( const CHAR* strFilename );
    HRESULT PlayVideoWithGetNextFrame( const CHAR* strFilename );
    BOOL    PlayVideoFrame();           // Play a frame from a video, if one is playing.
    HRESULT OpenMovie( const CHAR* strFilename, D3DFORMAT format, BOOL allocateTextures );

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
    m_bFullScreen =         FALSE;
    m_readMethod =          READ_FROM_PACKETS;
    m_bYUV =                TRUE;
    m_bUseTextures =        TRUE;
    m_bUsePlay =            FALSE;
    m_bUseColorKey =        FALSE;
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
    const char* strMovieName = "D:\\Media\\Videos\\Test.xmv";

    if ( m_player.IsPlaying() )
    {
        // Halt movie playback
        if ( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
            m_player.Destroy();
    }
    else
    {
        // Trigger video playback
        if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
        {
            if ( m_bUsePlay )
            {
                if( FAILED( PlayVideoWithPlay( strMovieName ) ) )
                    OUTPUT_DEBUG_STRING( "Video playback failed!\n" );
            }
            else
            {
                if( FAILED( PlayVideoWithGetNextFrame( strMovieName ) ) )
                    OUTPUT_DEBUG_STRING( "Video playback failed!\n" );
            }
        }

        // Cycle through the different methods of reading movie data
        if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_B] )
        {
            m_readMethod = READ_METHOD( m_readMethod + 1 );
            if ( m_readMethod == READ_METHOD_COUNT )
                m_readMethod = READ_FROM_FILE;
        }

        // Toggle full screen playback
        if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_X] )
            m_bFullScreen = !m_bFullScreen;

        // Switch between playing in a texture and an overlay
        if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_Y] )
            m_bUseTextures = !m_bUseTextures;

        if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_BLACK] )
        {
            // The functionality of the black key varies depending on whether we're
            // unpacking to a texture or an overlay.
            if ( m_bUseTextures)
            {
                // Switch between YUV and RGB textures
                m_bYUV = !m_bYUV;
            }
            else
            {
                // Toggle color keying
                m_bUseColorKey = !m_bUseColorKey;
            }
        }

        // Switch between using Play() and GetNextFrame()
        if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_WHITE] )
            m_bUsePlay = !m_bUsePlay;
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

    // Play a frame from a video.
    BOOL bPlayedFrame = PlayVideoFrame();

    bool doOverlayText = false;
    if ( m_bUseColorKey && bPlayedFrame && !m_bUseTextures )
    {
        doOverlayText = true;
        // If we are using color keys on overlays draw a color key rectangle.
        // The movie will show through everywhere that color is. Then draw
        // a rectangle of another color, and the movie will not show through
        // there.
 
        RECT rect = g_partialScreenRect;
        if ( m_bFullScreen )
            rect = g_fullScreenRect;

        // First draw the rectangle that will let the movie show through.
        D3DDevice::SetVertexDataColor( D3DVSDE_DIFFUSE, g_colorKey );

        D3DDevice::Begin( D3DPT_QUADLIST );
        D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, (float)rect.left, (float)rect.top, 1.0f, 1.0f );
        D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, (float)rect.right, (float)rect.top, 1.0f, 1.0f );
        D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, (float)rect.right, (float)rect.bottom, 1.0f, 1.0f );
        D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, (float)rect.left, (float)rect.bottom, 1.0f, 1.0f );
        D3DDevice::End();


        // Now draw a rectangle where the movie does not show through - the background will
        // be visible here, along with the text that we display later.
        D3DDevice::SetVertexDataColor( D3DVSDE_DIFFUSE, D3DCOLOR_ARGB( 0xFF, 0x40, 0x40, 0x80 ) );

        FLOAT top = 130;
        FLOAT bottom = 160;
        FLOAT left = 170;
        FLOAT right = 470;

        D3DDevice::Begin( D3DPT_QUADLIST );
        D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, left, top, 1.0f, 1.0f );
        D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, right, top, 1.0f, 1.0f );
        D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, right, bottom, 1.0f, 1.0f );
        D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, left, bottom, 1.0f, 1.0f );
        D3DDevice::End();
    }

    // Show text
    m_Font.Begin();
    m_Font.SetScaleFactors( 1.2f, 1.2f );
    m_Font.DrawText( 48, 36, 0xffffffff, L"XMV Player" );
    m_Font.SetScaleFactors( 1.0f, 1.0f );

    if ( doOverlayText )
    {
        // Draw some overlay text on the color keyed area.
        m_Font.DrawText( 320, 145, 0xffffffff, L"Color keyed text block",
                    XBFONT_CENTER_X | XBFONT_CENTER_Y );
    }

    if ( !bPlayedFrame )
    {
        // Buffer to put the output text in.
        WCHAR    message[2000] = L"";
        // Pointer to where the next piece of text should be placed.
        WCHAR*   output = message;

        // swprintf returns the number of characters printed, so it is handy for building up a string in
        // a buffer.
        output += swprintf( output, L"Press " GLYPH_A_BUTTON L" to play movie." );

        output += swprintf( output, L"\n");

        if ( !m_bUsePlay )
        {
            // Full screen doesn't make sense when using the Play() interface - it works, 
            // but it usually isn't appropriate.
            output += swprintf( output, L"\n");
            if ( m_bFullScreen )
                output += swprintf( output, L"Full screen: " GLYPH_X_BUTTON );
            else
                output += swprintf( output, L"Partial screen: " GLYPH_X_BUTTON );
        }

        output += swprintf( output, L"\n");
        switch ( m_readMethod )
        {
            case READ_FROM_FILE:
                output += swprintf( output, L"Use CreateDecoderForFile: " GLYPH_B_BUTTON );
                break;
            case READ_FROM_PACKETS:
                output += swprintf( output, L"Use CreateDecoderForPackets from a file: " GLYPH_B_BUTTON );
                break;
            case READ_FROM_MEMORY:
                output += swprintf( output, L"Use CreateDecoderForPackets from memory: " GLYPH_B_BUTTON );
                break;
        }

        output += swprintf( output, L"\n");
        if ( !m_bUsePlay )
        {
            // Textures only make sense when using GetNextFrame - disable the message otherwise.
            if ( m_bUseTextures )
                output += swprintf( output, L"Play on a texture: " GLYPH_Y_BUTTON );
            else
                output += swprintf( output, L"Play on overlays: " GLYPH_Y_BUTTON );
        }

        output += swprintf( output, L"\n");
        if ( !m_bUsePlay )
        {
            // The YUV option only makes sense when using textures and GetNextFrame, because
            // overlays always use YUV - disable the message otherwise
            if ( m_bUseTextures )
            {
                if ( m_bYUV )
                    output += swprintf( output, L"Use YUV surface: " GLYPH_BLACK_BUTTON );
                else
                    output += swprintf( output, L"Use RGB surface (may be slower): " GLYPH_BLACK_BUTTON );
            }
            else
            {
                if ( m_bUseColorKey )
                    output += swprintf( output, L"Use color key: " GLYPH_BLACK_BUTTON );
                else
                    output += swprintf( output, L"Don't use color key: " GLYPH_BLACK_BUTTON );
            }
        }

        output += swprintf( output, L"\n");
        if ( m_bUsePlay )
            output += swprintf( output, L"Use Play() to show movie: " GLYPH_WHITE_BUTTON );
        else
            output += swprintf( output, L"Use GetNextFrame() to show movie: " GLYPH_WHITE_BUTTON );

        m_Font.DrawText( 320, 240, 0xFFFFFFFF, message,
                        XBFONT_CENTER_X | XBFONT_CENTER_Y );
    }

    m_Font.DrawText( 592, 38, 0xFFFFFFFF, m_strFrameRate, XBFONT_RIGHT );
    m_Font.End();

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: OpenMovie()
// Desc: Open a movie file in one of the three supported ways.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::OpenMovie( const CHAR* strFilename, D3DFORMAT format, BOOL bAllocateTextures )
{
    HRESULT hr = E_FAIL;
    switch ( m_readMethod )
    {
        case READ_FROM_FILE:
            hr = m_player.OpenFile( strFilename, format, m_pd3dDevice, bAllocateTextures );
            break;
        case READ_FROM_PACKETS:
            hr = m_player.OpenFileForPackets( strFilename, format, m_pd3dDevice, bAllocateTextures  );
            break;
        case READ_FROM_MEMORY:
            hr = m_player.OpenMovieFromMemory( strFilename, format, m_pd3dDevice, bAllocateTextures  );
            break;
    }
    return hr;
}




//-----------------------------------------------------------------------------
// Name: MoviePlayerThread()
// Desc: This function is used in a separate thread so that the main thread can
// do other tasks, such as loading data, or just checking for the user to press A.
// Alternately the movie can be played in the main thread, with a sub-thread
// to check for button presses.
//-----------------------------------------------------------------------------
DWORD __stdcall MoviePlayerThread( void* pMovieData )
{
    assert( pMovieData );
    CXMVPlayer *pPlayer = ( CXMVPlayer* )pMovieData;

    // Play the movie
    // Can also be played in a subrectangle by specifying a rectangle, but that
    // rarely makes sense with the Play() interface.
    pPlayer->Play( XMVFLAG_NONE, 0 );

    return 0;
}




//-----------------------------------------------------------------------------
// Name: SimplePlayVideo()
// Desc: Plays specified video file.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::PlayVideoWithPlay( const CHAR* strFilename )
{
    // When using Play(), which uses overlays, we have to use D3DFMT_YUY2
    D3DFORMAT format = D3DFMT_YUY2;

    // We can use the file or memory or packet interface - it doesn't matter.
    HRESULT hr = OpenMovie( strFilename, format, FALSE );
    if ( FAILED( hr ) )
        return hr;

    // Start the movie stopping thread - this should always succeed.
    HANDLE hThread = CreateThread( 0, 0, &MoviePlayerThread, &m_player, 0, 0 );

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
                m_player.TerminatePlayback();
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

    // Free all movie playback resources.
    m_player.Destroy();

    return hr;
}




//-----------------------------------------------------------------------------
// Name: TexturePlayVideo()
// Desc: Plays specified video file on a texture.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::PlayVideoWithGetNextFrame( const CHAR* strFilename )
{
    assert( !m_player.IsPlaying() );

    // If we're not currently playing a movie then start playing one.
    D3DFORMAT format = D3DFMT_YUY2;
    if ( m_bUseTextures )
    {
        if ( !m_bYUV )
        {
            // The only non-YUV formats allowed are D3DFMT_LIN_A8R8G8B8 and D3DFMT_LIN_X8R8G8B8
            format = D3DFMT_LIN_A8R8G8B8;
        }
    }
    else
    {
        // If we're using overlays ( !m_bUseTextures ) then we need to use YUY2.
        // We also need to enable overlays. If we do it through m_player then
        // we are guaranteed that they will be disabled later.
        m_player.EnableOverlays( m_pd3dDevice );
    }

    // We can use the file or memory or packet interface - it doesn't matter.
    HRESULT hr = OpenMovie( strFilename, format, TRUE );

    return hr;
}




//-----------------------------------------------------------------------------
// Name: PlayVideoFrame()
// Desc: Plays one frame of video if a movie is currently open and if there is
// a frame available. This function is safe to call at any time.
//-----------------------------------------------------------------------------
BOOL CXBoxSample::PlayVideoFrame()
{
    if ( !m_player.IsPlaying() )
        return FALSE;

    const FLOAT fMovieWidth = FLOAT( m_player.GetWidth() );
    const FLOAT fMovieHeight = FLOAT( m_player.GetHeight() );

    // Move to the next frame.
    LPDIRECT3DTEXTURE8 pTexture = 0;
    if ( m_bUseTextures )
        pTexture = m_player.AdvanceFrameForTexturing( m_pd3dDevice );
    else
        pTexture = m_player.AdvanceFrameForOverlays( m_pd3dDevice );

    // See if the movie is over now.
    if ( !m_player.IsPlaying() )
    {
        // Clean up the movie, then return.
        m_player.Destroy();
        return FALSE;
    }

    // If no texture is ready, return TRUE to indicate that a movie is playing,
    // but don't render anything yet.
    if ( !pTexture )
        return TRUE;

    if ( m_bUseTextures )
    {

        // Have the texture start small and scale up, just to prove it's on
        // a texture.
        const DWORD FRAMES_TO_EASE_IN = 40;
        FLOAT fRatio = 1.0;
        if ( m_player.GetCurrentFrame() < FRAMES_TO_EASE_IN )
            fRatio = FLOAT( m_player.GetCurrentFrame() ) / FRAMES_TO_EASE_IN;

        const FLOAT fSizeY    = ( m_bFullScreen ? 480.0f : 240.0f) * fRatio;
        const FLOAT fOriginX =  320.0f - ( fSizeY * .5f * fMovieWidth / fMovieHeight );
        const FLOAT fOriginY = 240.0f - fSizeY * .5f;

        // Draw the texture.
        m_pd3dDevice->SetRenderState( D3DRS_FILLMODE,         D3DFILL_SOLID );
        m_pd3dDevice->SetRenderState( D3DRS_CULLMODE,         D3DCULL_CCW );
        m_pd3dDevice->SetRenderState( D3DRS_ZENABLE,          FALSE );

        // Draw the texture as a quad.
        m_pd3dDevice->SetTexture( 0, pTexture );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );

        // Wrapping isn't allowed on linear textures.
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU,  D3DTADDRESS_CLAMP );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV,  D3DTADDRESS_CLAMP );

        // If we're unpacking to a YUV surface we have to tell the hardware that we
        // are rendering from a YUV surface.
        if ( m_bYUV )
            m_pd3dDevice->SetRenderState( D3DRS_YUVENABLE, TRUE );

        FLOAT fLeft   = fOriginX;
        FLOAT fRight  = fOriginX + ( fSizeY * fMovieWidth) / fMovieHeight;
        FLOAT fTop    = fOriginY;
        FLOAT fBottom = fOriginY + fSizeY;

        // On linear textures the texture coordinate range is from 0,0 to width,height, instead
        // of 0,0 to 1,1.
        m_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW|D3DFVF_TEX1 );
        m_pd3dDevice->Begin( D3DPT_QUADLIST );
        m_pd3dDevice->SetVertexData2f( D3DVSDE_TEXCOORD0, 0, fMovieHeight );
        m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX, fLeft,  fBottom, 0.0f, 1.0f );
        m_pd3dDevice->SetVertexData2f( D3DVSDE_TEXCOORD0, 0, 0 );
        m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX, fLeft,  fTop,    0.0f, 1.0f );
        m_pd3dDevice->SetVertexData2f( D3DVSDE_TEXCOORD0, fMovieWidth, 0 );
        m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX, fRight, fTop,    0.0f, 1.0f );
        m_pd3dDevice->SetVertexData2f( D3DVSDE_TEXCOORD0, fMovieWidth, fMovieHeight );
        m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX, fRight, fBottom, 0.0f, 1.0f );
        m_pd3dDevice->End();

        // If we switched to YUV texturing then we need to switch back.
        if ( m_bYUV )
            m_pd3dDevice->SetRenderState( D3DRS_YUVENABLE, FALSE );
    }
    else
    {
        // Setup the source and destination rectangles.
        RECT srcRect = { 0, 0, m_player.GetWidth(), m_player.GetHeight() };
        RECT dstRect = g_partialScreenRect;
        if ( m_bFullScreen )
            dstRect = g_fullScreenRect;

        // Get the surface from the texture.
        IDirect3DSurface8* pSurface;
        pTexture->GetSurfaceLevel( 0, &pSurface );

        // Display the frame of data as an overlay, enabling color keying if requested.
        if ( m_bUseColorKey )
            m_pd3dDevice->UpdateOverlay( pSurface, &srcRect, &dstRect, TRUE, g_colorKey );
        else
            m_pd3dDevice->UpdateOverlay( pSurface, &srcRect, &dstRect, FALSE, 0 );
    }

    return TRUE;
}
