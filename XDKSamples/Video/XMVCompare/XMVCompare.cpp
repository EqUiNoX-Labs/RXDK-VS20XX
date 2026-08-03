//-----------------------------------------------------------------------------
// File: XMVCompare.cpp
//
// Desc: Plays back two XMV videos for a side-by-side (A/B) comparison of
//       visual quality from different compression settings. See readme.txt for
//       further info.
//
// Hist: 07.30.03 - New for November 2003 XDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#include "XMVCompare.h"


//-----------------------------------------------------------------------------
// Prototypes
//-----------------------------------------------------------------------------
BOOL DebugConsoleHandleCommands();

// global pointer to the app
CXBoxSample* g_pxbApp;

//-----------------------------------------------------------------------------
// Help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_LEFTSTICK,    XBHELP_PLACEMENT_1, L"Move splitter/\nAdvance left\n frame (click)" },
    { XBHELP_RIGHTSTICK,   XBHELP_PLACEMENT_2, L"Advance right frame\n  (click)" },
    { XBHELP_DPAD,         XBHELP_PLACEMENT_2, L"Up/Down scroll rate\nLeft/Right splitter color" },
    { XBHELP_A_BUTTON,     XBHELP_PLACEMENT_2, L"Choose left\n  video file" },
    { XBHELP_B_BUTTON,     XBHELP_PLACEMENT_1, L"Choose right\n  video file" },
    { XBHELP_X_BUTTON,     XBHELP_PLACEMENT_1, L"Play Videos        " },
    { XBHELP_Y_BUTTON,     XBHELP_PLACEMENT_1, L"Toggle text\n  display" },
    { XBHELP_LEFT_BUTTON,  XBHELP_PLACEMENT_2, L"Display left\n  pane fullscreen" },
    { XBHELP_RIGHT_BUTTON, XBHELP_PLACEMENT_1, L"Display right\n  pane fullscreen" },
    { XBHELP_START_BUTTON, XBHELP_PLACEMENT_1, L"Pause" },
    { XBHELP_BACK_BUTTON,  XBHELP_PLACEMENT_1, L"Display help" },
    { XBHELP_BLACK_BUTTON, XBHELP_PLACEMENT_1, L"Next display\n  mode" },
    { XBHELP_WHITE_BUTTON, XBHELP_PLACEMENT_1, L"Previous display\n  mode" },
};

#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts)/sizeof(g_HelpCallouts[0]))




//-----------------------------------------------------------------------------
// Video modes
//-----------------------------------------------------------------------------
typedef struct {
    DWORD dwWidth;
    DWORD dwHeight;
    BOOL  fProgressive;
    BOOL  fWideScreen;
} DISPLAY_MODE;

DISPLAY_MODE g_aDisplayModes[] =
{
//    Width  Height Progressive Widescreen
    {   640,    480,    FALSE,  FALSE },        // 640x480 interlaced 4x3
    {   640,    480,    FALSE,  TRUE },         // 640x480 interlaced 16x9
    {   640,    480,    TRUE,   FALSE },        // 640x480 progressive 4x3
    {   640,    480,    TRUE,   TRUE },         // 640x480 progressive 16x9
    {   720,    480,    FALSE,  FALSE },        // 720x480 interlaced 4x3
    {   720,    480,    FALSE,  TRUE },         // 720x480 interlaced 16x9
    {   720,    480,    TRUE,   FALSE },        // 720x480 progressive 4x3
    {   720,    480,    TRUE,   TRUE },         // 720x480 progressive 16x9
    {  1280,    720,    TRUE,   TRUE },         // 1280x720 progressive 16x9
    {  1920,   1080,    FALSE,  TRUE },         // 1920x1080 interlaced 16x9 
};
#define NUM_MODES ( sizeof( g_aDisplayModes ) / sizeof( g_aDisplayModes[0] ) )

DISPLAY_MODE g_aModes[32];
WORD         g_wNumModes = 0;

DWORD   g_dwSplitterColors[] = {0xff000000, 0xffff0000, 0xff00ff00, 0xff0000ff,
                                0xff888888, 0xffffff};

#define NUM_SPLITTER_COLORS (sizeof(g_dwSplitterColors) / sizeof(DWORD))

//-----------------------------------------------------------------------------
// Name: SupportsMode
// Desc: Returns TRUE if we can support the given mode, based off the
//       currently plugged in AV pack and video flags
//-----------------------------------------------------------------------------
BOOL SupportsMode( DISPLAY_MODE mode, DWORD dwVideoFlags )
{
    // Need to check for widescreen on 480 modes only - 
    // 720p and 1080i are by definition widescreen.
    if( (mode.dwHeight == 480) &&
        (mode.fWideScreen) && 
        (!(dwVideoFlags & XC_VIDEO_FLAGS_WIDESCREEN)) )
    {
        return FALSE;
    }

    // Explicit check for 480p
    if( (mode.dwHeight == 480) &&
        (mode.fProgressive) &&
        (!(dwVideoFlags & XC_VIDEO_FLAGS_HDTV_480p)) )
    {
        return FALSE;
    }

    // Explicit check for 720p (only 720 mode)
    if( mode.dwHeight == 720 && !(dwVideoFlags & XC_VIDEO_FLAGS_HDTV_720p) )
        return FALSE;

    // Explicit check for 1080i (only 1080 mode)
    if( mode.dwHeight == 1080 && !(dwVideoFlags & XC_VIDEO_FLAGS_HDTV_1080i) )
        return FALSE;

    return TRUE;
}




//-----------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program. Initializes everything, and goes into a
//       message-processing loop. Idle time is used to render the scene.
//-----------------------------------------------------------------------------
void __cdecl main()
{
    CXBoxSample xbApp;
    g_pxbApp = &xbApp;

    if( FAILED( xbApp.Create() ) )
        return;
    
    xbApp.Run();
}




//-----------------------------------------------------------------------------
// Name: CXBoxSample()
// Desc: Constructor
//-----------------------------------------------------------------------------
CXBoxSample::CXBoxSample()
            :CXBApplication()
{
    m_dwVideoFlags = XGetVideoFlags();

    // Start out in 640x480 interlaced 4x3
    m_d3dpp.BackBufferWidth  = 640;
    m_d3dpp.BackBufferHeight = 480;
    m_d3dpp.Flags            = D3DPRESENTFLAG_INTERLACED;

    m_d3dpp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_ONE;

    m_dwCurrentMode       = 0;
    m_bDrawHelp           = FALSE;
    m_bPause              = FALSE;
    m_bVideoChanged       = FALSE;
    m_bDisplayText        = TRUE;
    m_fSplitterPosition   = 320.0f;
    m_fSplitterScrollRate = 10.0f;
    m_iCurrentAVideo      = 0;
    m_iCurrentBVideo      = 0;
    m_iTempASelection     = 0;
    m_iTempBSelection     = 0;
    m_pTextureA           = NULL;
    m_pTextureB           = NULL;
    m_iSplitterColorIndex = 0;
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize device-dependant objects.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    // Create a font
    if( FAILED( m_Font.Create( "Font.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create the help
    if( FAILED( m_Help.Create( "Gamepad.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    m_wMaxFilesFound = EnumVideoFiles();

    // Make sure the start of the sample displays two different videos
    if (m_wMaxFilesFound)
        m_iTempASelection = m_iCurrentAVideo = 1;

    PlayVideoWithGetNextFrame( m_szVideoFileNames[m_iCurrentAVideo],
                               m_szVideoFileNames[m_iCurrentBVideo] );
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    // Toggle help
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Toggle pause
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_START )
        m_bPause = !m_bPause;

    // Select video A, but don't play the video yet until X is pressed. The new
    // video will be rendered in red text.
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
    {
        m_iTempASelection++;

        if ( m_iTempASelection >= m_wMaxFilesFound )
            m_iTempASelection = 0;

        m_bPause        = TRUE;
        m_bVideoChanged = TRUE;
        m_bDisplayText  = TRUE;
    }

    // Select Video B, but don't play the video yet until X is pressed. The new
    // video will be rendered in red text.
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_B] )
    {
        m_iTempBSelection++;

        if ( m_iTempBSelection >= m_wMaxFilesFound )
            m_iTempBSelection = 0;

        m_bPause        = TRUE;
        m_bVideoChanged = TRUE;
        m_bDisplayText  = TRUE;
    }

    // Start video
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_X] )
    {
        Play();
    }

    // Toggle text display
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_Y] )
    {
        m_bDisplayText = !m_bDisplayText;
    }

    // Next video mode
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_BLACK] )
    {

        // Find the next mode that can be supported
        do
        {
            m_dwCurrentMode = ( m_dwCurrentMode + 1 ) % NUM_MODES;

        } while( !SupportsMode( g_aDisplayModes[ m_dwCurrentMode ], m_dwVideoFlags ) );

        // Adjust presentation parameters
        m_d3dpp.BackBufferWidth = g_aDisplayModes[ m_dwCurrentMode ].dwWidth;
        m_d3dpp.BackBufferHeight = g_aDisplayModes[ m_dwCurrentMode ].dwHeight;
        m_d3dpp.Flags = g_aDisplayModes[ m_dwCurrentMode ].fProgressive ?
                        D3DPRESENTFLAG_PROGRESSIVE : D3DPRESENTFLAG_INTERLACED;
        m_d3dpp.Flags |= g_aDisplayModes[ m_dwCurrentMode ].fWideScreen ?
                         D3DPRESENTFLAG_WIDESCREEN : 0;

        // Reset the device
        m_pd3dDevice->Reset( &m_d3dpp );

        // Re-create our help object for the new frame buffer size
        m_Help.Destroy();
        m_Help.Create( "Gamepad.xpr" );
    }

    // Previous video mode
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_WHITE] )
    {
        // Find the next mode that can be supported
        do
        {
            m_dwCurrentMode = ( m_dwCurrentMode - 1 ) % NUM_MODES;

        } while( !SupportsMode( g_aDisplayModes[ m_dwCurrentMode ], m_dwVideoFlags ) );

        // Adjust presentation parameters
        m_d3dpp.BackBufferWidth = g_aDisplayModes[ m_dwCurrentMode ].dwWidth;
        m_d3dpp.BackBufferHeight = g_aDisplayModes[ m_dwCurrentMode ].dwHeight;
        m_d3dpp.Flags = g_aDisplayModes[ m_dwCurrentMode ].fProgressive ?
                        D3DPRESENTFLAG_PROGRESSIVE : D3DPRESENTFLAG_INTERLACED;
        m_d3dpp.Flags |= g_aDisplayModes[ m_dwCurrentMode ].fWideScreen ?
                         D3DPRESENTFLAG_WIDESCREEN : 0;

        // Reset the device
        m_pd3dDevice->Reset( &m_d3dpp );


        // Re-create our help object for the new frame buffer size
        m_Help.Destroy();
        m_Help.Create( "Gamepad.xpr" );

    }

    // Increase splitter's scroll rate
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
    {
        m_fSplitterScrollRate += 1.0f;
    }

    // Decrease splitter's scroll rate
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
    {
        m_fSplitterScrollRate -= 1.0f;

        if ( m_fSplitterScrollRate < 1.0f )
            m_fSplitterScrollRate = 1.0f;
    }

    // Select next splitter color    
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
    {
        m_iSplitterColorIndex--;

        if ( m_iSplitterColorIndex < 0 )
            m_iSplitterColorIndex = NUM_SPLITTER_COLORS-1;
    }

    // Select previous splitter color
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
    {
        m_iSplitterColorIndex++;

        if ( m_iSplitterColorIndex > NUM_SPLITTER_COLORS )
            m_iSplitterColorIndex = 0;
    }

    // Display video A or B fullscreen
    if( m_DefaultGamepad.bAnalogButtons[XINPUT_GAMEPAD_LEFT_TRIGGER] )
    {
       m_fSplitterPosition = (FLOAT)m_d3dpp.BackBufferWidth;
    }
    else if( m_DefaultGamepad.bAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER] )
    {
       m_fSplitterPosition = 0.0f;
    }

    // Advance video A one frame
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_LEFT_THUMB)
    {
        PlayVideoFrame(VIDEO_PLAYBACK_A);
    }

    // Advance video B one frame
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_RIGHT_THUMB)
    {
        PlayVideoFrame(VIDEO_PLAYBACK_B);
    }

    // Move the splitter line
    m_fSplitterPosition += m_DefaultGamepad.fX1 * m_fSplitterScrollRate;

    if ( m_fSplitterPosition < 0.0f )
        m_fSplitterPosition = 1.0f;
    else if ( m_fSplitterPosition > (FLOAT)m_d3dpp.BackBufferWidth )
        m_fSplitterPosition = (FLOAT)m_d3dpp.BackBufferWidth - 1.0f;

    // Process any pending commands from the remote debug console
    DebugConsoleHandleCommands();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: EnumVideoFiles()
// Desc: Enumerates XMV files from disk
//-----------------------------------------------------------------------------
WORD CXBoxSample::EnumVideoFiles()
{
    WIN32_FIND_DATA     wfd;
    HANDLE              hFind;
    WORD                i = 0;

    ZeroMemory( m_szVideoFileNames, sizeof(m_szVideoFileNames) );

    // enumerate all XMV videos
    hFind = FindFirstFile( "d:\\media\\videos\\*.xmv", &wfd );

    if ( hFind != INVALID_HANDLE_VALUE )
    {
        do
        {
            sprintf( m_szVideoFileNames[i++], "d:\\media\\videos\\%s",
                      wfd.cFileName );

            if ( i > MAX_FILES-1 )
                break;

        } while ( FindNextFile(hFind, &wfd) );

        FindClose( hFind );
    }

    return i;
}


//-----------------------------------------------------------------------------
// Name: IndexFromVideoFilename()
// Desc: Returns the index of the video in the filename array. -1 if not found
//-----------------------------------------------------------------------------
INT CXBoxSample::IndexFromVideoFilename(const CHAR* strFileName)
{
   INT  i; 

   for ( i = 0; i < m_wMaxFilesFound; i++ )
   {
       if ( 0 == strcmp(m_szVideoFileNames[i], strFileName) )
       {
           return i; // found
       }
   }

   return -1; // not found
}




//-----------------------------------------------------------------------------
// Name: SetLeftVideo()
// Desc: Sets the left hand video. Used for remotely setting the video.
//-----------------------------------------------------------------------------
BOOL CXBoxSample::SetLeftVideo(const CHAR* strFileName)
{
    INT iIndex = 0;

    // re-enumerate videos
    m_wMaxFilesFound = EnumVideoFiles();

    // found the desired video
    iIndex = IndexFromVideoFilename(strFileName);

    if ( iIndex >= 0 )
        m_iTempASelection = iIndex;
    else
        return FALSE;

    return TRUE;
}



//-----------------------------------------------------------------------------
// Name: SetRightVideo()
// Desc: Sets the left hand video. Used for remotely setting the video.
//-----------------------------------------------------------------------------
BOOL CXBoxSample::SetRightVideo(const CHAR* strFileName)
{
   INT iIndex = 0;

    // re-enumerate videos
    m_wMaxFilesFound = EnumVideoFiles();

    // found the desired video
    iIndex = IndexFromVideoFilename(strFileName);

    if ( iIndex >= 0 )
        m_iTempBSelection = iIndex;
    else
        return FALSE;

    return TRUE;
}


//-----------------------------------------------------------------------------
// Name: Play()
// Desc: Added to command the sample to play via remote. Same as pressing "X"
//       button.
//-----------------------------------------------------------------------------
VOID CXBoxSample::Play()
{
    if ( m_bVideoChanged )
            m_bVideoChanged = FALSE;

    if ( m_bPause )
        m_bPause = FALSE;

    if ( m_PlayerA.IsPlaying() )
        m_PlayerA.Destroy();

    if ( m_PlayerB.IsPlaying() )
        m_PlayerB.Destroy();

    m_iCurrentAVideo = m_iTempASelection;
    m_iCurrentBVideo = m_iTempBSelection;

    m_iTempASelection = m_iCurrentAVideo;
    m_iTempBSelection = m_iCurrentBVideo;

    if( FAILED( PlayVideoWithGetNextFrame( m_szVideoFileNames[m_iCurrentAVideo],
                                            m_szVideoFileNames[m_iCurrentBVideo] ) ) )
    {
        OUTPUT_DEBUG_STRING( "Video playback failed!\n" );
    }
}



//-----------------------------------------------------------------------------
// Name: RenderSplitter()
// Desc: Renders the splitter bar
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::RenderSplitter()
{
    // render the vertical splitter
    m_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW | D3DFVF_DIFFUSE );

    m_pd3dDevice->Begin(D3DPT_LINELIST);
        m_pd3dDevice->SetVertexDataColor( D3DVSDE_DIFFUSE, g_dwSplitterColors[m_iSplitterColorIndex] );
        m_pd3dDevice->SetVertexData4f(D3DVSDE_VERTEX, m_fSplitterPosition, 0.0f, 0.5f, 1.0f);
        m_pd3dDevice->SetVertexDataColor( D3DVSDE_DIFFUSE, g_dwSplitterColors[m_iSplitterColorIndex] );
        m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX, m_fSplitterPosition, (float)m_d3dpp.BackBufferHeight, 0.5f, 1.0f );
    m_pd3dDevice->End();

    m_pd3dDevice->SetVertexShader(0);

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, the call is the entry point for 3d
//       rendering. This function sets up render states, clears the
//       viewport, and renders the scene.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Render()
{
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,
                        0xff404040, 1.0f, 0L );

    if (!m_bPause)    
        PlayVideoFrame(VIDEO_PLAYBACK_BOTH);

    RenderVideoFrame();

    // Display info and help
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else if ( m_bDisplayText )
    {
        FLOAT fBorderX = 0.10f * g_aDisplayModes[ m_dwCurrentMode ].dwWidth;
        FLOAT fBorderY = 0.10f * g_aDisplayModes[ m_dwCurrentMode ].dwHeight; 

        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( fBorderX, fBorderY, 0xffffffff, L"XMVCompare " );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( g_aDisplayModes[m_dwCurrentMode].dwWidth - fBorderX, fBorderY, 
                         0xffffff00, m_strFrameRate, XBFONT_RIGHT );

        WCHAR sz[100];
        swprintf( sz, L"Video size: %dx%d", m_PlayerA.GetWidth(), m_PlayerA.GetHeight() );
        m_Font.DrawText( fBorderX, fBorderY + 30.0f, 0xffffff00, sz );

        swprintf( sz, L"Video size: %dx%d", m_PlayerB.GetWidth(), m_PlayerB.GetHeight() );
        m_Font.DrawText( g_aDisplayModes[m_dwCurrentMode].dwWidth - fBorderX,
                         fBorderY + 30.0f, 0xffffff00, sz, XBFONT_RIGHT );

        swprintf( sz, L"Mode: %dx%d %s %s", g_aDisplayModes[ m_dwCurrentMode ].dwWidth, 
                                            g_aDisplayModes[ m_dwCurrentMode ].dwHeight, 
                                            g_aDisplayModes[ m_dwCurrentMode ].fProgressive ? L"Progressive" : L"Interlaced", 
                                            g_aDisplayModes[ m_dwCurrentMode ].fWideScreen ? L"16x9" : L"4x3" );
        m_Font.DrawText( fBorderX, fBorderY + 60.0f, 0xffffff00, sz );

        if ( m_bPause )
        {
            swprintf(sz, L"Paused");
            m_Font.DrawText( g_aDisplayModes[m_dwCurrentMode].dwWidth - fBorderX, fBorderY + 60.0f,
                             0xffffff00, sz, XBFONT_RIGHT );
        }

        if (!m_PlayerA.IsPlaying())
        {
            FLOAT       fStringWidth, fStringHeight;

            swprintf(sz, L"Press " GLYPH_X_BUTTON L" to play videos");
            m_Font.GetTextExtent(sz, &fStringWidth, &fStringHeight);
            m_Font.DrawText((g_aDisplayModes[ m_dwCurrentMode ].dwWidth - fStringWidth) / 2,
                            (g_aDisplayModes[ m_dwCurrentMode ].dwHeight - fStringHeight) / 2,
                            0xffffffff, sz);
        }

        // Display the XMV videos' filenames. If the video file selection has changed and
        // the old one is currently being played, the name will be rendered in red.
        // Otherwise, it will be white
        XBUtil_GetWide( m_szVideoFileNames[m_iTempASelection], sz, sizeof(sz) );
        m_Font.DrawText( m_fSplitterPosition - m_Font.GetTextWidth(sz) - 10,
                         g_aDisplayModes[ m_dwCurrentMode ].dwHeight - fBorderY -
                         m_Font.GetFontHeight(),
                         m_iCurrentAVideo == m_iTempASelection ? 0xffffffff : 0xffff0000, sz );

        XBUtil_GetWide( m_szVideoFileNames[m_iTempBSelection], sz, sizeof(sz) );
        m_Font.DrawText( m_fSplitterPosition+10, g_aDisplayModes[ m_dwCurrentMode ].dwHeight -
                         fBorderY - m_Font.GetFontHeight(),
                         m_iCurrentBVideo == m_iTempBSelection ? 0xffffffff : 0xffff0000,  sz );


        m_Font.End();
    }

    RenderSplitter();

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: PlayVideoWithGetNextFrame()
// Desc: Plays specified video file onto a texture.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::PlayVideoWithGetNextFrame( const CHAR* strFilenameA,
                                               const CHAR* strFilenameB)
{
    assert( !m_PlayerA.IsPlaying() );
    assert( !m_PlayerB.IsPlaying() );

    D3DFORMAT format = D3DFMT_YUY2;

    // We can use the file or memory or packet interface - it doesn't matter.
    HRESULT hr = m_PlayerA.OpenMovieFromMemory( strFilenameA, format, m_pd3dDevice, TRUE);

    m_PlayerB.OpenMovieFromMemory( strFilenameB, format, m_pd3dDevice, TRUE );

    return hr;
}




//-----------------------------------------------------------------------------
// Name: PlayVideoFrame()
// Desc: Plays one frame of video if a movie is currently open and if there is
// a frame available. This function is safe to call at any time.
//-----------------------------------------------------------------------------
BOOL CXBoxSample::PlayVideoFrame(WORD wFlag)
{
    BOOL    bResult = TRUE;

    // Move to the next frame.

    if (wFlag & VIDEO_PLAYBACK_A)
    {
        // See if the movie is over now.
        if ( !m_PlayerA.IsPlaying() )
        {
            // Clean up the movie, then return.
            m_PlayerA.Destroy();
            m_pTextureA = NULL;
            bResult = FALSE;
        }
        else
        {
            m_pTextureA = m_PlayerA.AdvanceFrameForTexturing( m_pd3dDevice );

            // If no texture is ready, return TRUE to indicate that a movie is playing,
            // but don't render anything yet.
            if ( !m_pTextureA )
                bResult = TRUE;
        }
    }

    if (wFlag & VIDEO_PLAYBACK_B)
    {
        if ( !m_PlayerB.IsPlaying() )
        {
            // Clean up the movie, then return.
            m_PlayerB.Destroy();
            m_pTextureB = NULL;
            bResult = FALSE;
        }
        else
        {

            m_pTextureB = m_PlayerB.AdvanceFrameForTexturing( m_pd3dDevice );

            if ( !m_pTextureB )
                bResult = TRUE;
        }
    }

    return bResult;
}




//-----------------------------------------------------------------------------
// Name: RenderVideoFrame()
// Desc: Render video frame A onto the left portion of the split screen and
//       video frame B onto the right portion of the split screen.
//-----------------------------------------------------------------------------
BOOL CXBoxSample::RenderVideoFrame()
{
    FLOAT fMovieWidth = FLOAT( m_PlayerA.GetWidth() );
    FLOAT fMovieHeight = FLOAT( m_PlayerA.GetHeight() );

    const FLOAT fBackBufferWidth = FLOAT( m_d3dpp.BackBufferWidth );
    const FLOAT fBackBufferHeight = FLOAT( m_d3dpp.BackBufferHeight );


    FLOAT fLeft   = 0.0f;
    FLOAT fRight  = m_fSplitterPosition;
    FLOAT fTop    = 0.0f;
    FLOAT fBottom = fMovieHeight;

    if (!m_pTextureA || !m_pTextureB)
        return false;

    // Draw the texture.
    m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE,  FALSE );

    // Draw the texture as a quad.
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );

    // Wrapping isn't allowed on linear textures.
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU,  D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV,  D3DTADDRESS_CLAMP );

    m_pd3dDevice->SetRenderState( D3DRS_YUVENABLE, TRUE );


    { // Render video A into the left pane
        m_pd3dDevice->SetTexture( 0, m_pTextureA );

        // On linear textures the texture coordinate range is from 0,0 to width,height, instead
        // of 0,0 to 1,1.
        m_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW|D3DFVF_TEX1 );
        m_pd3dDevice->Begin( D3DPT_QUADLIST );
            m_pd3dDevice->SetVertexData2f( D3DVSDE_TEXCOORD0, 0.0f, floorf(fMovieHeight) );
            m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX,    0.0f-0.5f, floorf(fBackBufferHeight)-0.5f, 0.0f, 1.0f );
            m_pd3dDevice->SetVertexData2f( D3DVSDE_TEXCOORD0, 0.0f, 0.0f );
            m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX,    0.0f-0.5f, 0.0f-0.5f, 0.0f, 1.0f );
            m_pd3dDevice->SetVertexData2f( D3DVSDE_TEXCOORD0, floorf(fMovieWidth*(m_fSplitterPosition/fBackBufferWidth)), 0 );
            m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX,    floorf(m_fSplitterPosition)-0.5f, 0.0f-0.5f, 0.0f, 1.0f );
            m_pd3dDevice->SetVertexData2f( D3DVSDE_TEXCOORD0, floorf(fMovieWidth*(m_fSplitterPosition/fBackBufferWidth)), floorf(fMovieHeight) );
            m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX,    floorf(m_fSplitterPosition)-0.5f, floorf(fBackBufferHeight)-0.5f, 0.0f, 1.0f );
        m_pd3dDevice->End();
    }

    { // Render video B into the right pane
        m_pd3dDevice->SetTexture( 0, m_pTextureB );

        fMovieWidth  = FLOAT( m_PlayerB.GetWidth() );
        fMovieHeight = FLOAT( m_PlayerB.GetHeight() );
        fLeft        = m_fSplitterPosition;
        fRight       = fMovieWidth;
        fTop         = 0.0f;
        fBottom      = fMovieHeight;

        // On linear textures the texture coordinate range is from 0,0 to width,height, instead
        // of 0,0 to 1,1.
        m_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW|D3DFVF_TEX1 );
        m_pd3dDevice->Begin( D3DPT_QUADLIST );
            m_pd3dDevice->SetVertexData2f( D3DVSDE_TEXCOORD0, floorf(fMovieWidth*(m_fSplitterPosition/fBackBufferWidth)), floorf(fMovieHeight) );
            m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX,    floorf(m_fSplitterPosition)-0.5f, floorf(fBackBufferHeight)-0.5f, 0.0f, 1.0f );
            m_pd3dDevice->SetVertexData2f( D3DVSDE_TEXCOORD0, floorf(fMovieWidth*(m_fSplitterPosition/fBackBufferWidth)), 0 );
            m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX,    floorf(m_fSplitterPosition)-0.5f, 0.0f-0.5f, 0.0f, 1.0f );
            m_pd3dDevice->SetVertexData2f( D3DVSDE_TEXCOORD0, floorf(fMovieWidth), 0 );
            m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX,    floorf(fBackBufferWidth)-0.5f, 0.0f-0.5f, 0.0f, 1.0f );
            m_pd3dDevice->SetVertexData2f( D3DVSDE_TEXCOORD0, floorf(fMovieWidth), floorf(fMovieHeight) );
            m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX,    floorf(fBackBufferWidth)-0.5f, floorf(fBackBufferHeight)-0.5f, 0.0f, 1.0f );
        m_pd3dDevice->End();

        // If we switched to YUV texturing then we need to switch back.
        m_pd3dDevice->SetRenderState( D3DRS_YUVENABLE, FALSE );
        m_pd3dDevice->SetTexture( 0, NULL);
    }

    return TRUE;
}