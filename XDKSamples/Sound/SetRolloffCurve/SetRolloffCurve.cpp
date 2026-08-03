//-----------------------------------------------------------------------------
// File: SetRolloffCurve.cpp
//
// Desc: This sample demonstrates how to use the SetRolloffCurve
//       in DirectSound.
//
// Hist: 07.15.02 - New for September XDK release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <stdio.h>
#include <XBApp.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <XBSound.h>
#include "dsound.h"
#include "dsstdfx.h"


//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_BACK_BUTTON,  XBHELP_PLACEMENT_2, L"Display help" },
    { XBHELP_BLACK_BUTTON, XBHELP_PLACEMENT_2, L"Dump text file\nof points" },
    { XBHELP_DPAD,         XBHELP_PLACEMENT_2, L"Add/remove points\nby 1s or 10s" },
    { XBHELP_A_BUTTON,     XBHELP_PLACEMENT_1, L"Shape curve" },
    { XBHELP_B_BUTTON,     XBHELP_PLACEMENT_2, L"Toggle mute\nat max dist." },
    { XBHELP_X_BUTTON,     XBHELP_PLACEMENT_2, L"Change min\ndistance" },
    { XBHELP_Y_BUTTON,     XBHELP_PLACEMENT_2, L"Change max\ndistance" },
    { XBHELP_RIGHTSTICK,   XBHELP_PLACEMENT_2, L"Move distance\nslider" },
    { XBHELP_LEFTSTICK,    XBHELP_PLACEMENT_2, L"Move cursor\nin X/Z" },
    { XBHELP_MISC_CALLOUT, XBHELP_PLACEMENT_2, GLYPH_LEFT_BUTTON GLYPH_RIGHT_BUTTON L": Use with " GLYPH_X_BUTTON L" to change min distance\n"
                                               GLYPH_LEFT_BUTTON GLYPH_RIGHT_BUTTON L": Use with " GLYPH_Y_BUTTON L" to change max distance" },
};

const DWORD NUM_HELP_CALLOUTS = sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]);




//-----------------------------------------------------------------------------
// Global variables and definitions
//-----------------------------------------------------------------------------

struct D3DVERTEX
{
    D3DXVECTOR4 p;           // position
    D3DCOLOR    c;           // color
};
#define D3DFVF_D3DVERTEX (D3DFVF_XYZRHW|D3DFVF_DIFFUSE)

typedef D3DVERTEX D3DVERTEXPOINT[4];


// Constants for rolloff curve
const FLOAT MAX_TOP             = 81.0f;
const FLOAT MAX_BOTTOM          = 349.0f;
const FLOAT MAX_LEFT            = 46.0f;
const FLOAT MAX_RIGHT           = 594.f;
const FLOAT FDX                 = MAX_RIGHT - MAX_LEFT;
const FLOAT FDY                 = MAX_BOTTOM - MAX_TOP;

const FLOAT POINT_SIZE          = 2.0f;
const FLOAT CURSOR_SIZE         = 5.0f;

const FLOAT DISTANCE_FACTOR     = 1.0f;
const FLOAT CURSOR_FACTOR       = 8.0f;
const FLOAT SLIDER_FACTOR       = 10.0f;
const FLOAT POINTS_1_FACTOR     = 1.0f;
const FLOAT POINTS_10_FACTOR    = 10.0f;
const FLOAT POINT_CURVE_FACTOR  = 0.5f;

const FLOAT STICK_TOLERANCE     = 0.2f;
const FLOAT BUTTON_TOLERANCE    = 0.25f;

const DWORD BOX_CURSOR_COLOR    = 0xff00ff00;
const DWORD SLIDER_COLOR        = 0xff8080ff;
const DWORD SLIDER_BAR_COLOR    = 0xffffffff;
const DWORD POINT_COLOR         = 0xffff4040;
const DWORD DIST_COLOR          = 0xffffff00;

const DWORD MAX_POINTS          = 100;




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    // Font and help objects
    CXBFont                 m_Font;                     // Font object
    CXBHelp                 m_Help;                     // Help object

    // Sound objects
    LPDIRECTSOUND8          m_pDSound;                  // DirectSound object
    LPDIRECTSOUNDBUFFER8    m_pDSBuffer;                // DirectSoundBuffer
    CWaveFile               m_pWaveFile;                // Wave file parsers
    BYTE*                   m_prgbSampleData;           // Sample data from wav

    // Rollloff curve objects
    D3DVERTEX*              m_pd3dvCurve;               // Rolloff curve vertex
    DWORD                   m_dwNumPoints;              // Number of points in curve
    FLOAT                   m_fRolloffCurve[MAX_POINTS];// Rolloff point curve array
    FLOAT                   m_fMinDistance;             // Minimum distance
    FLOAT                   m_fMaxDistance;             // Maximum distance
    FLOAT                   m_fPosition;                // Slider position
    BOOL                    m_bMuteAtMax;               // Mute sound at max distance

    // Graph objects
    D3DVERTEXPOINT*         m_pd3dvPoints;              // Graph points
    D3DVERTEX               m_d3dvBox[4];               // Graph box
    D3DVERTEX               m_d3dvCursor[4];            // Graph cursor
    D3DVERTEX               m_d3dvSlider[6];            // Graph slider
    D3DVERTEX               m_d3dvSource[2];            // Graph source
    FLOAT                   m_fX;                       // Graph position
    FLOAT                   m_fY;                       // Graph position
    FLOAT                   m_fsX;                      // Graph position

    // State objects
    BOOL                    m_bDraw;                    // Draw state
    BOOL                    m_bLastAddingPoints;        // Points delta state
    BOOL                    m_bLastAdjustMinMax;        // MinMax delta state
    BOOL                    m_bDrawHelp;                // Draw help flag

public:
    // Initialization methods
    HRESULT     InitializeDSBuffer();
    HRESULT     CreateScene();

    // SetRolloffCurve methods
    HRESULT     SetRolloffCurve();
    HRESULT     MoveCursor(FLOAT fdX, FLOAT fdY);
    HRESULT     MoveSlider(FLOAT fsX);
    HRESULT     SetNumPoints(DWORD dwNumPoints);
    HRESULT     SetPoint(DWORD dwIndex);

    // Utility methods
    FLOAT       Lerp(FLOAT fA, FLOAT fB, FLOAT fT)      {return ( ( ( fB - fA ) * fT ) + fA );}
    HRESULT     DownloadEffectsImage( CHAR* strScratchFile );

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
// Desc: Constructor
//-----------------------------------------------------------------------------
CXBoxSample::CXBoxSample()
            :CXBApplication()
{
    // Sound objects
    m_pDSound           = NULL;
    m_pDSBuffer         = NULL;
    m_prgbSampleData    = NULL;

    // Rolloff curve objects
    m_pd3dvCurve        = NULL;
    m_dwNumPoints       = 0;
    m_fMinDistance      = DS3D_DEFAULTMINDISTANCE;
    m_fMaxDistance      = 100.0;
    m_bMuteAtMax        = FALSE;

    // Graph objects
    m_pd3dvPoints       = NULL;

    // State objects
    m_bDraw             = FALSE;
    m_bLastAddingPoints = FALSE;
    m_bLastAdjustMinMax = FALSE;
    m_bDrawHelp         = FALSE;

    // Direct3D objects
    m_d3dpp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_THREE;
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: 
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    HRESULT hr;

    // Create the font object
    if( FAILED( hr = m_Font.Create( "Font.xpr" ) ) )
        return hr;

    // Create the help object
    if( FAILED( hr = m_Help.Create( "Gamepad.xpr" ) ) )
        return hr;

    // Create DirectSound
    if( FAILED( hr = DirectSoundCreate( NULL, &m_pDSound, NULL ) ) )
        return hr;

    // Set 3-D sound processing:
    DirectSoundUseFullHRTF();

    // Download the standard DirectSound effects image
    if( FAILED( hr = DownloadEffectsImage( "D:\\Media\\dsstdfx.bin" ) ) )
        return hr;

    // Initialize DirectSound objects
    if( FAILED( hr = InitializeDSBuffer() ) )
        return hr;

    // Initialize scene
    if( FAILED( hr = CreateScene() ) )
        return hr;

    // Initialize the rolloff curve
    if( FAILED( hr = SetRolloffCurve() ) )
        return hr;

    return S_OK;
}




//------------------------------------------------------------------------------
// Name: InitializeDSBuffer()
// Desc: Initialize the DirectSound buffer
//------------------------------------------------------------------------------
HRESULT CXBoxSample::InitializeDSBuffer()
{
    DSBUFFERDESC    dsbdesc = {0};
    DWORD           dwFileSize;
    HRESULT         hr;

    // Release the buffer if already set
    SAFE_RELEASE( m_pDSBuffer );

    // Open the wave file
    if( FAILED( hr = m_pWaveFile.Open( "D:\\Media\\Sounds\\heli.wav" ) ) )
        return hr;

    // Create the wave format struct
    WAVEFORMATEXTENSIBLE wfx;
    if( FAILED( hr = m_pWaveFile.GetFormat( &wfx ) ) )
        return hr;

    // Initialize buffer descriptor
    dsbdesc.dwSize          = sizeof( DSBUFFERDESC );
    dsbdesc.dwFlags         = DSBCAPS_CTRL3D;
    dsbdesc.dwBufferBytes   = 0;
    dsbdesc.lpwfxFormat     = (WAVEFORMATEX*)&wfx;
    dsbdesc.lpMixBins       = NULL;

    // Mute sound at max distance
    if( TRUE == m_bMuteAtMax )
    {
        dsbdesc.dwFlags |= DSBCAPS_MUTE3DATMAXDISTANCE;
    }

    // Create a sound buffer
    if( FAILED( hr = m_pDSound->CreateSoundBuffer( &dsbdesc, &m_pDSBuffer, NULL ) ) )
        return hr;

    // Get the wave sample size
    m_pWaveFile.GetDuration( &dwFileSize );

    // Allocate space for the sample data
    m_prgbSampleData = new BYTE[ dwFileSize ];
    if( NULL == m_prgbSampleData )
        return E_OUTOFMEMORY;

    // Read sample data from the wave file
    if( FAILED( hr = m_pWaveFile.ReadSample(0, m_prgbSampleData, dwFileSize, &dwFileSize ) ) )
        return hr;

    // Set up values for the new buffer
    if( FAILED( hr = m_pDSBuffer->SetBufferData(m_prgbSampleData, dwFileSize ) ) )
        return hr;

    // Play the buffer
    if( FAILED( hr = m_pDSBuffer->Play( 0, 0, DSBPLAY_LOOPING ) ) )
        return hr;

    // Source Settings
    m_pDSBuffer->SetPosition(0.0f, 0.0f, 0.0f, DS3D_DEFERRED);
    m_pDSBuffer->SetVelocity(0.0f, 0.0f, 0.0f, DS3D_DEFERRED);
    m_pDSBuffer->SetVolume(0);
    m_pDSBuffer->SetHeadroom(0);
    m_pDSBuffer->SetMinDistance(m_fMinDistance, DS3D_DEFERRED);
    m_pDSBuffer->SetMaxDistance(m_fMaxDistance, DS3D_DEFERRED);

    // Listener Settings
    m_pDSound->SetOrientation(1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, DS3D_DEFERRED);
    m_pDSound->SetPosition(0.0f, 0.0f, 0.0f, DS3D_DEFERRED);
    m_pDSound->SetVelocity(0.0f, 0.0f, 0.0f, DS3D_DEFERRED );

    // Commit position/velocity changes
    if( FAILED( hr = m_pDSound->CommitDeferredSettings() ) )
        return hr;

    // Set the rollof curve
    if( FAILED( SetRolloffCurve() ) )
        return hr;

    return S_OK;
}




//------------------------------------------------------------------------------
// Name: CreateScene()
// Desc: Initialize the graphic draw environment
//------------------------------------------------------------------------------
HRESULT CXBoxSample::CreateScene()
{
    // The box
    m_d3dvBox[0].p.x        = MAX_LEFT - 1.0f;        
    m_d3dvBox[0].p.y        = MAX_TOP - 1.0f;
    m_d3dvBox[0].p.z        = 0.0f;     
    m_d3dvBox[0].p.w        = 1.0f;     
    m_d3dvBox[0].c          = BOX_CURSOR_COLOR;       

    m_d3dvBox[1].p.x        = MAX_RIGHT + 1.0f;
    m_d3dvBox[1].p.y        = m_d3dvBox[0].p.y;       
    m_d3dvBox[1].p.z        = 0.0f;     
    m_d3dvBox[1].p.w        = 1.0f;
    m_d3dvBox[1].c          = BOX_CURSOR_COLOR;       

    m_d3dvBox[2].p.x        = m_d3dvBox[1].p.x;       
    m_d3dvBox[2].p.y        = MAX_BOTTOM + 1.0f;
    m_d3dvBox[2].p.z        = 0.0f;     
    m_d3dvBox[2].p.w        = 1.0f;     
    m_d3dvBox[2].c          = BOX_CURSOR_COLOR;       

    m_d3dvBox[3].p.x        = m_d3dvBox[0].p.x;       
    m_d3dvBox[3].p.y        = m_d3dvBox[2].p.y;       
    m_d3dvBox[3].p.z        = 0.0f;     
    m_d3dvBox[3].p.w        = 1.0f;     
    m_d3dvBox[3].c          = BOX_CURSOR_COLOR;       

    // Cursor
    m_fX                    = (MAX_RIGHT + MAX_LEFT) / 2.0f;
    m_fY                    = (MAX_BOTTOM + MAX_TOP) / 2.0f;
    m_d3dvCursor[0].p.x     = m_fX;
    m_d3dvCursor[0].p.y     = m_fY - 5.0f;
    m_d3dvCursor[0].p.z     = 0.0f;
    m_d3dvCursor[0].p.w     = 1.0f;
    m_d3dvCursor[0].c       = BOX_CURSOR_COLOR;

    m_d3dvCursor[1].p.x     = m_fX;
    m_d3dvCursor[1].p.y     = m_fY + 5.0f;
    m_d3dvCursor[1].p.z     = 0.0f;
    m_d3dvCursor[1].p.w     = 1.0f;
    m_d3dvCursor[1].c       = BOX_CURSOR_COLOR;

    m_d3dvCursor[2].p.x     = m_fX - 5.0f;
    m_d3dvCursor[2].p.y     = m_fY;
    m_d3dvCursor[2].p.z     = 0.0f;
    m_d3dvCursor[2].p.w     = 1.0f;
    m_d3dvCursor[2].c       = BOX_CURSOR_COLOR;

    m_d3dvCursor[3].p.x     = m_fX + 5.0f;
    m_d3dvCursor[3].p.y     = m_fY;
    m_d3dvCursor[3].p.z     = 0.0f;
    m_d3dvCursor[3].p.w     = 1.0f;
    m_d3dvCursor[3].c       = BOX_CURSOR_COLOR;

    // Slider
    m_d3dvSlider[0].p.x     = m_d3dvBox[0].p.x;
    m_d3dvSlider[0].p.y     = m_d3dvBox[2].p.y + 20.0f;
    m_d3dvSlider[0].p.z     = 0.0f;
    m_d3dvSlider[0].p.w     = 1.0f;
    m_d3dvSlider[0].c       = SLIDER_COLOR;

    m_d3dvSlider[1].p.x     = m_d3dvSlider[0].p.x;
    m_d3dvSlider[1].p.y     = m_d3dvSlider[0].p.y + 40.0f;
    m_d3dvSlider[1].p.z     = 0.0f;
    m_d3dvSlider[1].p.w     = 1.0f;
    m_d3dvSlider[1].c       = SLIDER_COLOR;

    m_d3dvSlider[2].p.x     = m_d3dvBox[1].p.x;
    m_d3dvSlider[2].p.y     = m_d3dvSlider[0].p.y;
    m_d3dvSlider[2].p.z     = 0.0f;
    m_d3dvSlider[2].p.w     = 1.0f;
    m_d3dvSlider[2].c       = SLIDER_COLOR;

    m_d3dvSlider[3].p.x     = m_d3dvSlider[2].p.x;
    m_d3dvSlider[3].p.y     = m_d3dvSlider[1].p.y;
    m_d3dvSlider[3].p.z     = 0.0f;
    m_d3dvSlider[3].p.w     = 1.0f;
    m_d3dvSlider[3].c       = SLIDER_COLOR;

    m_d3dvSlider[4].p.x     = m_d3dvSlider[0].p.x;
    m_d3dvSlider[4].p.y     = (m_d3dvSlider[1].p.y + m_d3dvSlider[0].p.y) / 2.0f;
    m_d3dvSlider[4].p.z     = 0.0f;
    m_d3dvSlider[4].p.w     = 1.0f;
    m_d3dvSlider[4].c       = SLIDER_COLOR;

    m_d3dvSlider[5].p.x     = m_d3dvSlider[2].p.x;
    m_d3dvSlider[5].p.y     = m_d3dvSlider[4].p.y;
    m_d3dvSlider[5].p.z     = 0.0f;
    m_d3dvSlider[5].p.w     = 1.0f;
    m_d3dvSlider[5].c       = SLIDER_COLOR;

    // Slider bar (source)
    m_fsX                   = m_d3dvSlider[0].p.x + 1.0f;
    m_d3dvSource[0].p.x     = m_fsX;
    m_d3dvSource[0].p.y     = m_d3dvSlider[0].p.y + 5.0f;
    m_d3dvSource[0].p.z     = 0.0f;
    m_d3dvSource[0].p.w     = 1.0f;
    m_d3dvSource[0].c       = SLIDER_BAR_COLOR;

    m_d3dvSource[1].p.x     = m_d3dvSource[0].p.x;
    m_d3dvSource[1].p.y     = m_d3dvSlider[1].p.y - 5.0f;
    m_d3dvSource[1].p.z     = 0.0f;
    m_d3dvSource[1].p.w     = 1.0f;
    m_d3dvSource[1].c       = SLIDER_BAR_COLOR;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    HRESULT     hr                  = S_OK;
    FLOAT       fdX                 = 0.0f;
    FLOAT       fdY                 = 0.0f;
    FLOAT       fsX                 = 0.0f;
    FLOAT       fNumPoints          = (FLOAT)m_dwNumPoints;
    BOOL        bAdjustMinMax       = FALSE;
    BOOL        bDraw               = FALSE;
    BOOL        bAddingPoints       = FALSE;

    // Toggle help
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_BACK ) 
    {
        m_bDrawHelp = !m_bDrawHelp;
    }

    // Toggle mute at max distance
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_B ] )
    {
        m_bMuteAtMax = !m_bMuteAtMax;
        if( FAILED( hr = InitializeDSBuffer() ) )
            return hr;
    }

    // Check for point/value file dump
    if( m_DefaultGamepad.bAnalogButtons[ XINPUT_GAMEPAD_BLACK ] )
    {
        FILE* file;

        // Open the log file
        if( ( file = fopen( "D:\\PointValues.log", "w" ) ) == NULL )
        {
            OutputDebugStringA( "Error: Unable to open log file!\n" );
        }
        else
        {
            // Print rolloff curve values to log file
            for( DWORD dwIndex = 0; dwIndex < m_dwNumPoints; dwIndex++ )
            {
                fwprintf( file, L"Point: %d \t Rolloff Value: %.4f\n", dwIndex, m_fRolloffCurve[ dwIndex ] );
            }

            // Close the log file
            fclose( file);
        }
    }

    // Check for change of min distance
    if( m_DefaultGamepad.bAnalogButtons[ XINPUT_GAMEPAD_X ] )
    {
        bAdjustMinMax = TRUE;

        // Calculate new min distance
        if( m_DefaultGamepad.bAnalogButtons[XINPUT_GAMEPAD_LEFT_TRIGGER] )
        {
            m_fMinDistance -= DISTANCE_FACTOR;
        }

        if( m_DefaultGamepad.bAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER] )
        {
            m_fMinDistance += DISTANCE_FACTOR;
        }

        // Validate min distance is in range
        if( m_fMinDistance < DS3D_MINMINDISTANCE )
            m_fMinDistance = DS3D_MINMINDISTANCE;
        if( m_fMinDistance > DS3D_MAXMINDISTANCE )
            m_fMinDistance = DS3D_MAXMINDISTANCE;
        if( m_fMinDistance > m_fMaxDistance )
            m_fMinDistance = m_fMaxDistance;

        // Move slider 
        if( FAILED( hr = MoveSlider( 0.0f ) ) )
            return hr;

        // Reset the new min distance
        if( FAILED( hr = m_pDSBuffer->SetMinDistance( m_fMinDistance, DS3D_DEFERRED ) ) )
            return hr;
    }

    // Check for change of max distance
    if( m_DefaultGamepad.bAnalogButtons[ XINPUT_GAMEPAD_Y ] )
    {
        bAdjustMinMax = TRUE;

        // Calculate new max distance
        if( m_DefaultGamepad.bAnalogButtons[ XINPUT_GAMEPAD_LEFT_TRIGGER ] )
        {
            m_fMaxDistance -= DISTANCE_FACTOR;
        }

        if( m_DefaultGamepad.bAnalogButtons[ XINPUT_GAMEPAD_RIGHT_TRIGGER ] )
        {
            m_fMaxDistance += DISTANCE_FACTOR;
        }

        // Validate max distance is in range
        if( m_fMaxDistance < DS3D_MINMAXDISTANCE )
            m_fMaxDistance = DS3D_MINMAXDISTANCE;
        if( m_fMaxDistance > DS3D_MAXMAXDISTANCE )
            m_fMaxDistance = DS3D_MAXMAXDISTANCE;
        if( m_fMaxDistance < m_fMinDistance )
            m_fMaxDistance = m_fMinDistance;

        // Move slider 
        if( FAILED( hr = MoveSlider( 0.0f ) ) )
            return hr;

        // Reset the new max distance
        if( FAILED( hr = m_pDSBuffer->SetMaxDistance( m_fMaxDistance, DS3D_DEFERRED ) ) )
            return hr;
    }

    // Update cursor
    fdX = m_DefaultGamepad.fX1 * CURSOR_FACTOR;
    fdY = -( m_DefaultGamepad.fY1 * CURSOR_FACTOR );

    // Update slider
    fsX = m_DefaultGamepad.fX2 * SLIDER_FACTOR;

    // Draw
    if( m_DefaultGamepad.bAnalogButtons[XINPUT_GAMEPAD_A] >= BUTTON_TOLERANCE )
    {
        bDraw = TRUE;
    }

    // Add/Remove points
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
    {
        fNumPoints = floorf(fNumPoints) + POINTS_1_FACTOR;

        if( fNumPoints > MAX_POINTS )
            fNumPoints = MAX_POINTS;

        bAddingPoints = TRUE;
    }
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
    {
        fNumPoints = floorf(fNumPoints) - POINTS_1_FACTOR;

        if( fNumPoints < 0.0f )
            fNumPoints = 0.0f;
        
        bAddingPoints = TRUE;
    }
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
    {
        fNumPoints = floorf(fNumPoints) + POINTS_10_FACTOR;

        if( fNumPoints > MAX_POINTS )
            fNumPoints = MAX_POINTS;

        bAddingPoints = TRUE;
    }

    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
    {
        fNumPoints = floorf(fNumPoints) - POINTS_10_FACTOR;

        if( fNumPoints < 0.0f )
            fNumPoints = 0.0f;

        bAddingPoints = TRUE;
    }

    // If finished updating our current state, then 
    // reset the rolloff curve in our DSoundBuffer.
    if( ( m_bDraw && !bDraw )                       || 
        ( m_bLastAddingPoints && !bAddingPoints )   || 
        ( m_bLastAdjustMinMax && !bAdjustMinMax ) )
    {
        SetRolloffCurve();
    }

    // Commit position/velocity changes
    if( bAdjustMinMax )
    {
        m_pDSound->CommitDeferredSettings();
    }

    // Save last state
    m_bLastAddingPoints = bAddingPoints;
    m_bLastAdjustMinMax = bAdjustMinMax;
    m_bDraw             = bDraw;

    // Set number of points
    if( FAILED( hr = SetNumPoints( ( DWORD( fNumPoints ) ) ) ) )
        return hr;

    // Move cursor
    if( FAILED( hr = MoveCursor(fdX, fdY) ) )
        return hr;
    
    // Move slider
    if( FAILED( hr = MoveSlider(fsX) ) )
        return hr;

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
    WCHAR  strBuffer[200];

    // Pump DirectSound's work queue
    DirectSoundDoWork();

    RenderGradientBackground( 0xFF202020, 0xFF404040 );

    if( m_bDrawHelp )
    {
        // Show help
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        // Set default render states
        m_pd3dDevice->SetTexture( 0, NULL );
        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
        m_pd3dDevice->SetRenderState( D3DRS_ZENABLE,          FALSE );
        m_pd3dDevice->SetRenderState( D3DRS_LIGHTING,         FALSE );
        m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

        // Setup for drawing
        m_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW | D3DFVF_DIFFUSE );

        // Draw the box
        m_pd3dDevice->DrawPrimitiveUP( D3DPT_LINELOOP, 3, m_d3dvBox, sizeof(D3DVERTEX) );

        // Draw the curve
        if( m_pd3dvCurve )
            m_pd3dDevice->DrawPrimitiveUP( D3DPT_LINESTRIP, m_dwNumPoints, m_pd3dvCurve, sizeof(D3DVERTEX) );

        // Draw the points
        if( m_pd3dvPoints )
            m_pd3dDevice->DrawPrimitiveUP( D3DPT_QUADLIST, m_dwNumPoints, m_pd3dvPoints, sizeof(D3DVERTEX) );

        // Draw the slider
        m_pd3dDevice->DrawPrimitiveUP( D3DPT_LINELIST, 3, m_d3dvSlider, sizeof(D3DVERTEX) );

        // Draw the source
        m_pd3dDevice->DrawPrimitiveUP( D3DPT_LINELIST, 1, m_d3dvSource, sizeof(D3DVERTEX) );

        // Draw the cursor
        m_pd3dDevice->DrawPrimitiveUP( D3DPT_LINELIST, 2, m_d3dvCursor, sizeof(D3DVERTEX) );

        // Draw title
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 320, 36, 0xffffffff, L"SetRolloffCurve", XBFONT_CENTER_X );
        m_Font.SetScaleFactors( 1.0f, 1.0f );

        // Show number of points
        swprintf( strBuffer, L"Points: %d", m_dwNumPoints );
        m_Font.DrawText( 48, 50, SLIDER_BAR_COLOR, strBuffer );

        // Show mute at max distance status
        swprintf( strBuffer, L"MuteAtMax: %s", m_bMuteAtMax ? L"True" : L"False" );
        m_Font.DrawText( 425, 50, POINT_COLOR, strBuffer );

        // Show min distance
        swprintf( strBuffer, L"MinDist: %.1f", m_fMinDistance );
        m_Font.DrawText( 64, 400, SLIDER_COLOR, strBuffer );

        // Show max distance
        swprintf( strBuffer, L"MaxDist: %.1f", m_fMaxDistance );
        m_Font.DrawText( 425, 400, DIST_COLOR, strBuffer );

        // Show position
        swprintf( strBuffer, L"(%.1f)", m_fPosition );
        m_Font.DrawText( 250, 400, SLIDER_BAR_COLOR, strBuffer );
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SetRolloffCurve()
// Desc: Set the rolloff curve
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::SetRolloffCurve()
{
    HRESULT hr;

    if( m_pd3dvCurve )
    {
        for( DWORD dwIndex = 0; dwIndex < m_dwNumPoints; ++dwIndex )
        {
            m_fRolloffCurve[ dwIndex ] = 1.0f - ( ( m_pd3dvCurve[ dwIndex + 1 ].p.y - MAX_TOP ) / FDY );

            if( m_fRolloffCurve[ dwIndex ] > 1.0f )
                m_fRolloffCurve[ dwIndex ] = 1.0f;
            else if( m_fRolloffCurve[ dwIndex ] < 0.0f )
                m_fRolloffCurve[ dwIndex ] = 0.0f;
        }
        if( FAILED( hr = m_pDSBuffer->SetRolloffCurve( m_fRolloffCurve, m_dwNumPoints, DS3D_IMMEDIATE ) ) )
            return hr;
    }
    else
    {
        if( FAILED( hr = m_pDSBuffer->SetRolloffCurve( NULL, 0, DS3D_IMMEDIATE ) ) )
            return hr;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: MoveCursor()
// Desc: Move the cursor
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::MoveCursor(FLOAT fdX, FLOAT fdY)
{
    HRESULT hr;

    // Update position
    m_fX += fdX;
    m_fY += fdY;

    // Validate boundaries
    if( m_fX >= ( m_d3dvBox[2].p.x - 1.0f ) )
        m_fX = m_d3dvBox[2].p.x - 1.0f;
    else if( m_fX <= ( m_d3dvBox[0].p.x + 1.0f ) )
        m_fX = m_d3dvBox[0].p.x + 1.0f;

    if( m_fY >= ( m_d3dvBox[2].p.y - 1.0f ) )
        m_fY = m_d3dvBox[2].p.y - 1.0f;
    else if( m_fY <= ( m_d3dvBox[0].p.y + 1.0f ) )
        m_fY = m_d3dvBox[0].p.y + 1.0f;

    // Update cursor
    m_d3dvCursor[0].p.x = m_fX;
    m_d3dvCursor[0].p.y = m_fY - CURSOR_SIZE;
    m_d3dvCursor[1].p.x = m_fX;
    m_d3dvCursor[1].p.y = m_fY + CURSOR_SIZE;
    m_d3dvCursor[2].p.x = m_fX - CURSOR_SIZE;
    m_d3dvCursor[2].p.y = m_fY;
    m_d3dvCursor[3].p.x = m_fX + CURSOR_SIZE;
    m_d3dvCursor[3].p.y = m_fY;

    // Draw the curve
    if( m_bDraw && NULL != m_pd3dvCurve && NULL != m_pd3dvPoints )
    {
        // Calculate index
        DWORD dwIndex = ( DWORD )( ( ( m_fX - MAX_LEFT ) / FDX ) * (FLOAT)m_dwNumPoints + POINT_CURVE_FACTOR );

        // Validate index
        if( dwIndex < 1 )
            dwIndex = 1;
        else if( dwIndex > m_dwNumPoints )
            dwIndex = m_dwNumPoints;

        m_pd3dvCurve[dwIndex].p.y = m_fY;

        // Set the point
        if( FAILED( hr = SetPoint( dwIndex - 1 ) ) )
            return hr;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: MoveSlider()
// Desc: Move the slider
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::MoveSlider(FLOAT fsX)
{
    HRESULT hr;

    // Update position
    m_fsX += fsX;

    // Validate boundaries
    if( m_fsX > m_d3dvBox[2].p.x )
        m_fsX = m_d3dvBox[2].p.x;
    else if( m_fsX < m_d3dvBox[0].p.x )
        m_fsX = m_d3dvBox[0].p.x;

    // Update source
    m_d3dvSource[0].p.x   = m_fsX;
    m_d3dvSource[1].p.x   = m_d3dvSource[0].p.x;

    // Calculate position
    m_fPosition = ( m_fMaxDistance - m_fMinDistance ) * ( ( m_fsX - m_d3dvBox[0].p.x ) / ( m_d3dvBox[2].p.x - m_d3dvBox[0].p.x ) ) + m_fMinDistance;

    // Validate position
    if( m_fPosition < m_fMinDistance )
        m_fPosition = m_fMinDistance;
    else if( m_fPosition > m_fMaxDistance )
        m_fPosition = m_fMaxDistance;
    
    // Set the position
    if( FAILED( hr = m_pDSBuffer->SetPosition( m_fPosition, 0.0f, 0.0f, DS3D_IMMEDIATE ) ) )
        return hr;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SetNumPoints()
// Desc: Set new number of points
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::SetNumPoints(DWORD dwNumPoints)
{
    HRESULT     hr = S_OK;
    FLOAT       fIncrement;
    DWORD       dwIndex;

    if((dwNumPoints < 0) || (dwNumPoints == m_dwNumPoints))
    {
        return S_OK;
    }

    // Free current curve
    if( NULL != m_pd3dvCurve )
    {
        SAFE_DELETE_ARRAY( m_pd3dvCurve );
    }

    // Free current points
    if( NULL != m_pd3dvPoints )
    {
        SAFE_DELETE_ARRAY( m_pd3dvPoints );
    }

    m_dwNumPoints = dwNumPoints;
    if( 0 != m_dwNumPoints )
    {
        m_pd3dvCurve        = new D3DVERTEX [ m_dwNumPoints + 1 ];
        m_pd3dvPoints       = new D3DVERTEXPOINT [ m_dwNumPoints ];

        // Handle setting the first point
        m_pd3dvCurve[ 0 ].p.x = MAX_LEFT;
        m_pd3dvCurve[ 0 ].p.y = MAX_TOP;
        m_pd3dvCurve[ 0 ].p.z = 0.0f;
        m_pd3dvCurve[ 0 ].p.w = 1.0f;
        m_pd3dvCurve[ 0 ].c   = DIST_COLOR;

        // Handle setting the last point
        m_pd3dvCurve[ m_dwNumPoints ].p.x = MAX_RIGHT;
        m_pd3dvCurve[ m_dwNumPoints ].p.y = MAX_BOTTOM;
        m_pd3dvCurve[ m_dwNumPoints ].p.z = 0.0f;
        m_pd3dvCurve[ m_dwNumPoints ].p.w = 1.0f;
        m_pd3dvCurve[ m_dwNumPoints ].c   = DIST_COLOR;

        if( FAILED( hr = SetPoint( m_dwNumPoints - 1 ) ) )
            return hr;

        // Calculate our increment
        fIncrement = FDX / m_dwNumPoints;

        // Handle setting the interior points
        for( dwIndex = 1; dwIndex < m_dwNumPoints; ++dwIndex )
        {
            m_pd3dvCurve[ dwIndex ].p.x = ( (FLOAT)dwIndex * fIncrement ) + MAX_LEFT;
            m_pd3dvCurve[ dwIndex ].p.y = Lerp( m_pd3dvCurve[ 0 ].p.y, m_pd3dvCurve[ m_dwNumPoints ].p.y, (FLOAT)dwIndex / (FLOAT)m_dwNumPoints );
            m_pd3dvCurve[ dwIndex ].p.z = 0.0f;
            m_pd3dvCurve[ dwIndex ].p.w = 1.0f;
            m_pd3dvCurve[ dwIndex ].c   = DIST_COLOR;

            if( FAILED( hr = SetPoint( dwIndex - 1 ) ) )
                return hr;
        }
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SetPoint()
// Desc: Set a single point
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::SetPoint(DWORD dwIndex)
{
    m_pd3dvPoints[dwIndex][0].p.x   = m_pd3dvCurve[ dwIndex + 1 ].p.x - POINT_SIZE;
    m_pd3dvPoints[dwIndex][0].p.y   = m_pd3dvCurve[dwIndex+1].p.y - POINT_SIZE;
    m_pd3dvPoints[dwIndex][0].p.z   = 0.0f;
    m_pd3dvPoints[dwIndex][0].p.w   = 1.0f;
    m_pd3dvPoints[dwIndex][0].c     = POINT_COLOR;

    m_pd3dvPoints[dwIndex][1].p.x   = m_pd3dvCurve[dwIndex+1].p.x + POINT_SIZE;
    m_pd3dvPoints[dwIndex][1].p.y   = m_pd3dvCurve[dwIndex+1].p.y - POINT_SIZE;
    m_pd3dvPoints[dwIndex][1].p.z   = 0.0f;
    m_pd3dvPoints[dwIndex][1].p.w   = 1.0f;
    m_pd3dvPoints[dwIndex][1].c     = POINT_COLOR;

    m_pd3dvPoints[dwIndex][2].p.x   = m_pd3dvCurve[dwIndex+1].p.x - POINT_SIZE;
    m_pd3dvPoints[dwIndex][2].p.y   = m_pd3dvCurve[dwIndex+1].p.y + POINT_SIZE;
    m_pd3dvPoints[dwIndex][2].p.z   = 0.0f;
    m_pd3dvPoints[dwIndex][2].p.w   = 1.0f;
    m_pd3dvPoints[dwIndex][2].c     = POINT_COLOR;

    m_pd3dvPoints[dwIndex][3].p.x   = m_pd3dvCurve[dwIndex+1].p.x + POINT_SIZE;
    m_pd3dvPoints[dwIndex][3].p.y   = m_pd3dvCurve[dwIndex+1].p.y + POINT_SIZE;
    m_pd3dvPoints[dwIndex][3].p.z   = 0.0f;
    m_pd3dvPoints[dwIndex][3].p.w   = 1.0f;
    m_pd3dvPoints[dwIndex][3].c     = POINT_COLOR;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: DownloadEffectsImage()
// Desc: Downloads an effects image to the DSP
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::DownloadEffectsImage( CHAR* strScratchFile )
{
    HRESULT             hr = S_OK;
    HANDLE              hFile   = NULL;
    PVOID               pBuffer = NULL;
    LPDSEFFECTIMAGEDESC pDesc;
    DSEFFECTIMAGELOC    EffectLoc;
    DWORD               dwBytesRead;
    DWORD               dwSize;
    DWORD               err;

    // Open scratch image file
    hFile = CreateFile( strScratchFile, GENERIC_READ, 0, NULL,
                        OPEN_EXISTING, 0, NULL );
    if( hFile == INVALID_HANDLE_VALUE )
    {
        err = GetLastError();
        hr = HRESULT_FROM_WIN32(err);
        return hr;
    }

    // Determine the size of the scratch image by seeking to
    // the end of the file.
    dwSize = SetFilePointer( hFile, 0, NULL, FILE_END );
    SetFilePointer( hFile, 0, NULL, FILE_BEGIN );

    // Allocate memory to read the scratch image from disk
    pBuffer = new BYTE[dwSize];
    if (NULL == pBuffer)
    {
        CloseHandle( hFile );
        return E_OUTOFMEMORY;
    }

    // Read the image in
    BOOL bResult = ReadFile( hFile, pBuffer, dwSize, &dwBytesRead, 0 );
    if (!bResult)
    {
        CloseHandle( hFile );
        SAFE_DELETE_ARRAY(pBuffer);
        err = GetLastError();
        hr = HRESULT_FROM_WIN32(err);
        return hr;
    }

    // Call DSound API to download the image..
    EffectLoc.dwI3DL2ReverbIndex = GraphI3DL2_I3DL2Reverb;
    EffectLoc.dwCrosstalkIndex   = GraphXTalk_XTalk;

    hr = m_pDSound->DownloadEffectsImage( pBuffer, dwSize, &EffectLoc, &pDesc );

    // Close file handle
    CloseHandle( hFile );

    // Cleanup memory
    SAFE_DELETE_ARRAY(pBuffer);
    
    return S_OK;
}
