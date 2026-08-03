//-----------------------------------------------------------------------------
// File: XACTParamControl.cpp
//
// Desc: XACTParamControl is a sample that demonstrates how to use XACT and
//       the parameter control sliders feature.
//
// Hist: 11.1.02 - New for the December 2002 XDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <XBSound.h>
#include <xact.h>
#include <dsstdfx.h>
#include <assert.h>
#include "XactSounds.h"


//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_rgHelpCallouts[] = 
{
    {XBHELP_BACK_BUTTON,    XBHELP_PLACEMENT_1, L"Display Help"},
    {XBHELP_RIGHT_BUTTON,   XBHELP_PLACEMENT_1, L"Acceleration"},
    {XBHELP_A_BUTTON,       XBHELP_PLACEMENT_1, L"Brake / Reset"},
};
const DWORD k_dwNumHelpCallouts = sizeof(g_rgHelpCallouts) / sizeof(g_rgHelpCallouts[0]);


// Constants for scaling input
static const DWORD GEAR_MIN             = 0;        // Minimum gear number
static const DWORD GEAR_MAX             = 4;        // Maximum gear number
static const FLOAT RPM_MIN              = 1000.0f;  // Minimum RPMs value
static const FLOAT RPM_REDLINE          = 8500.0f;  // Maximum RPMs value before redline
static const FLOAT RPM_REDLINE_MAX      = 10000.0f; // Maximum ROMs value
static const DWORD RPM_REDLINE_FACTOR   = 180;      // Scale factor after hitting redline


// Gear shift attributes table. This table is used to manage
// the audio asserts through the up and down shift algorithms.
typedef struct _SHIFT_TABLE
{
    DWORD dwAccel;          // Rate of RPM acceleration
    DWORD dwDecel;          // Rate of RPM deceleration
    DWORD dwUpShiftAt;      // Upshift when we hit this RPM
    DWORD dwUpShiftTo;      // Decrease down to this RPM on Upshift
    DWORD dwDownShiftAt;    // Downshift when we hit this RPM
    DWORD dwDownShiftTo;    // Increase to this RPM on Downshift
    DWORD dwInShiftFactor;  // Shift factor
} SHIFT_TABLE;


//
// Shift Example using the table
//
// You are accelerating in first gear:
//      Accelerate  at m_fElapsedTime * 1300 * m_fAcceleration
//      Upshift     at 5500 RPMs
//      Upshift     to 2000 RPMs
// 
// You are decelerating in third gear:
//      Decelerate  at m_fElapsedTime * 1200;
//      Downshift   at 3500 RPMs
//      Downshift   to 7500 RPMs
//
static SHIFT_TABLE g_aShiftTable[] =
{
    //  dwAccel dwDecel dwUpShiftAt dwUpShiftTo dwDownShiftAt dwDownShiftTo dwInShiftFactor
    {   1300,   1500,   5500,       2000,       2500,         5500,          6000 },    // GEAR 1
    {   1200,   1400,   6000,       3000,       3000,         6500,          9000 },    // GEAR 2
    {   1100,   1200,   7000,       3500,       3500,         7500,         10500 },    // GEAR 3
    {   1000,   1100,   7500,       4500,       4500,         8000,         12000 },    // GEAR 4
    {    900,   1000,   8000,       5000,       5000,         9000,         15000 },    // GEAR 5
};


// Variable Declarations
struct BACKGROUNDVERTEX 
{
    D3DXVECTOR4 d3dxVector; 
    D3DCOLOR    d3dColor;
};




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
public:
    // CTORs and DTORs
    CXBoxSample();

    // Inherited application methods
    virtual HRESULT     Initialize();
    virtual HRESULT     Render();
    virtual HRESULT     FrameMove();

    // Utility Methods
    HRESULT             AdjustParamControl( FLOAT fStartValue, CHAR* pszFriendlyName );
    HRESULT             ShiftGearSounds( BOOL bUpshift );
    HRESULT             ResetEngineState();

private:
    // DirectSound objects
    LPDIRECTSOUND8              m_pDSound;              // DirectSound object

    // XACT engine objects
    IXACTEngine*                m_pXACT;                // XACT Engine instance

    // In-memory wavebank objects
    BYTE*                       m_pbWaveBank;           // Wave Bank data
    PXACTWAVEBANK               m_pWaveBank;            // XACT Wave Bank

    // Soundbank objects
    BYTE*                       m_pbSoundBank;          // Sound Bank data
    IXACTSoundBank*             m_pSoundBank;           // XACT Sound Bank

    // Sound source objects
    IXACTSoundSource*           m_pSoundSource;         // XACT Sound Source
    PXACTSOUNDCUE               m_pSoundCue;            // Handle to a playing cue

    // Drawing objects
    CXBFont                     m_Font;                 // Font object
    CXBHelp                     m_Help;                 // Help object
    BOOL                        m_bDrawHelp;            // Should we draw help?

    // Engine objects
    FLOAT                       m_fAcceleration;        // Acceleration
    DWORD                       m_dwGear;               // Gear
    FLOAT                       m_fRPMs;                // RPMs
    BOOL                        m_bInShift;             // Engine in shift
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
// Desc: Constructor for CXBoxSample class
//-----------------------------------------------------------------------------
CXBoxSample::CXBoxSample() : CXBApplication()
{
    // DirectSound objects
    m_pDSound           = NULL;

    // XACT engine objects
    m_pXACT             = NULL;

    // In-memory wavebank objects
    m_pbWaveBank        = NULL;
    m_pWaveBank         = NULL;

    // Soundbank objects
    m_pbSoundBank       = NULL;
    m_pSoundBank        = NULL;

    // Sound source objects
    m_pSoundSource      = NULL;
    m_pSoundCue         = NULL;

    // Drawing objects
    m_bDrawHelp         = FALSE;

    // Engine objects
    m_fAcceleration     = 0.0f;
    m_dwGear            = GEAR_MIN;
    m_fRPMs             = RPM_MIN;
    m_bInShift          = FALSE;
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Performs initialization
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    // Create a font
    if( FAILED( m_Font.Create( "Font.xpr" ) ) )
    {
        return XBAPPERR_MEDIANOTFOUND;
    }

    // Create help
    if( FAILED( m_Help.Create( "Gamepad.xpr" ) ) )
    {
        return XBAPPERR_MEDIANOTFOUND;
    }

    // There are 2 options for 3-D sound processing:
    // 1) DirectSoundUseFullHRTF - full hardware HRTF-based processing
    // 2) DirectSoundUseLightHRTF - hardware HRTF processing, but without
    //      any vertical component (azimuth only).  Saves ~60k of memory
    DirectSoundUseFullHRTF();

   // Create the XACT runtime engine
    XACT_RUNTIME_PARAMETERS xrParams;
    xrParams.dwMax2DHwVoices        = 128;
    xrParams.dwMax3DHwVoices        = 32;
    xrParams.dwMaxConcurrentStreams = 16;
    xrParams.dwMaxNotifications     = 0;
    if( FAILED( XACTEngineCreate( &xrParams, &m_pXACT ) ) )
    {
        return E_FAIL;
    }

    // Load the in memory wave bank
    DWORD dwFileSize;
    if( FAILED( XBUtil_LoadFile( "D:\\media\\sounds\\XactSounds_memory.xwb", (VOID **)&m_pbWaveBank, &dwFileSize ) ) )
    {
        return XBAPPERR_MEDIANOTFOUND;
    }

    // Register the in memory wave bank with XACT
    if( FAILED( m_pXACT->RegisterWaveBank( m_pbWaveBank, dwFileSize, &m_pWaveBank ) ) )
    {
        return E_FAIL;
    }

    // Load the sound bank
    if( FAILED( XBUtil_LoadFile( "D:\\media\\sounds\\XactSounds.xsb", (VOID **)&m_pbSoundBank, &dwFileSize ) ) )
    {
        return XBAPPERR_MEDIANOTFOUND;
    }

    // Register the sound bank with XACT
    if( FAILED( m_pXACT->CreateSoundBank( m_pbSoundBank, dwFileSize, &m_pSoundBank ) ) )
    {
        return E_FAIL;
    }

    // Download the standard DirectSound effects image
    DSEFFECTIMAGELOC EffectLoc;
    EffectLoc.dwI3DL2ReverbIndex = GraphI3DL2_I3DL2Reverb;
    EffectLoc.dwCrosstalkIndex   = GraphXTalk_XTalk;
    if( FAILED( XAudioDownloadEffectsImage( "d:\\media\\dsstdfx.bin", 
                                            &EffectLoc, 
                                            XAUDIO_DOWNLOADFX_EXTERNFILE, 
                                            NULL ) ) )
        return E_FAIL;

    // Create a sound source to play our cues on
    if( FAILED( m_pXACT->CreateSoundSource( XACT_FLAG_SOUNDSOURCE_2D, &m_pSoundSource ) ) )
    {
        return E_FAIL;
    }

    // Start engine sound playing
    if( FAILED( ResetEngineState() ) )
    {
        return E_FAIL;
    }

    // Initialize DSound for our audio level meters
    if( FAILED( DirectSoundCreate( NULL, &m_pDSound, NULL ) ) )
    {
        return E_FAIL;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Performs per-frame updates
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    // Toggle help
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_BACK ) 
    {
        m_bDrawHelp = !m_bDrawHelp;
    }

    // Reset the engine sound play state
    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_A ] ) 
    {
        ResetEngineState();
        return S_OK;
    }

    // Get acceleration input and convert to value
    // between 0 and 1.
    m_fAcceleration = ( FLOAT )( m_DefaultGamepad.bAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER] / 255.0f );

    // Calculate RPM and GEAR values
    if ( m_fAcceleration )
    {
        // ACCELERATION CASE
        if ( !m_bInShift )
        {
            if ( RPM_REDLINE > m_fRPMs )
            {
                // Normal acceleration case, factor the RPMs based 
                // on time and acceleration value from the table.
                m_fRPMs += m_fElapsedTime * g_aShiftTable[m_dwGear].dwAccel * m_fAcceleration;
            }
            else
            {
                // Redline acceleration case, factor the RPMs based
                // on time and the redline acceleration factor.
                m_fRPMs += m_fElapsedTime * RPM_REDLINE_FACTOR;
            }

            // If we have rearched the upshift grear threshold, then 
            // put the engine in shift mode by shifting the gear sounds.
            if( ( m_fRPMs > g_aShiftTable[m_dwGear].dwUpShiftAt ) && ( m_dwGear != GEAR_MAX ) )
            {
                ShiftGearSounds( TRUE );
            }
        }
        else
        {
            // If we have not reached the upshift TO value, then 
            // adjust the RPMs until we hit this threshold.
            if ( m_fRPMs > g_aShiftTable[m_dwGear].dwUpShiftTo )
            {
                m_fRPMs -= m_fElapsedTime * g_aShiftTable[m_dwGear].dwInShiftFactor;
            }
            else
            {
                // We hit the upshift threshold value, so take
                // the engine out of shift mode.
                m_bInShift = FALSE;
            }
        }
    }
    else
    {
        // DECELERATION CASE
        if ( !m_bInShift )
        {
            // Normal deceleration case, factor the RPMs based 
            // on time and deceleration value from the table.
            m_fRPMs -= m_fElapsedTime * g_aShiftTable[m_dwGear].dwDecel;

            // If we have rearched the downshift grear threshold, then 
            // put the engine in shift mode by shifting the gear sounds.
            if( ( m_fRPMs < g_aShiftTable[m_dwGear].dwDownShiftAt ) && ( m_dwGear != GEAR_MIN ) )
            {
                ShiftGearSounds( FALSE );
            }
        }
        else
        {
            // If we have not reached the downshift TO value, then 
            // adjust the RPMs until we hit this threshold.
            if ( m_fRPMs < g_aShiftTable[m_dwGear].dwDownShiftTo )
            {
                m_fRPMs += m_fElapsedTime * g_aShiftTable[m_dwGear].dwInShiftFactor;
            }
            else
            {
                // We hit the downshift threshold value, so take
                // the engine out of shift mode.
                m_bInShift = FALSE;
            }
        }
    }

    // Check min, max, and redline ranges for RPM
    if( RPM_MIN > m_fRPMs )
    {
        m_fRPMs = RPM_MIN;
    }
    if( RPM_REDLINE_MAX < m_fRPMs )
    {
        m_fRPMs = RPM_REDLINE_MAX;
    }

    // Set the RPM parameter control values with XACT
    AdjustParamControl( ( 1.0f / RPM_REDLINE_MAX ) * m_fRPMs , "RPM" );

    // Pump XACT's work queue
    XACTEngineDoWork();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the scene
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Render()
{
    BACKGROUNDVERTEX    bgVertex[4];
    FLOAT               x1;
    FLOAT               x2;
    FLOAT               y1;
    FLOAT               y2;
    WCHAR               wszBuff[200];

    // Clear the viewport
    RenderGradientBackground( 0xFF404040, 0xFF606060 );

    // Show title, frame rate, and help
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_rgHelpCallouts, k_dwNumHelpCallouts );
    }
    else
    {
        m_Font.DrawText( 64,  50, 0xFF00FF00, L"XACT Runtime Parameter Control" );
        m_Font.DrawText( 450, 50, 0xFFFFFF00, m_strFrameRate );
        m_Font.DrawText( 64, 150, 0xFFFFFF00, L"Acceleration:" );

        x1 = 200;
        x2 = x1 + ( 340 * m_fAcceleration );
        y1 = 150;
        y2 = y1 + 20;

        bgVertex[0].d3dxVector = D3DXVECTOR4( x1 - 0.5f, y1 - 0.5f, 1.0f, 1.0f );  
        bgVertex[0].d3dColor = 0xffffffff;

        bgVertex[1].d3dxVector = D3DXVECTOR4( x2 - 0.5f, y1 - 0.5f, 1.0f, 1.0f );  
        bgVertex[1].d3dColor = 0xffffffff;

        bgVertex[2].d3dxVector = D3DXVECTOR4( x1 - 0.5f, y2 - 0.5f, 1.0f, 1.0f );  
        bgVertex[2].d3dColor = 0xffff0000;

        bgVertex[3].d3dxVector = D3DXVECTOR4( x2 - 0.5f, y2 - 0.5f, 1.0f, 1.0f );  
        bgVertex[3].d3dColor = 0xffff0000;

        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_DISABLE );
        m_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW|D3DFVF_DIFFUSE );
        m_pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, bgVertex, sizeof( bgVertex[0] ) );

        // Show current engine RPMs
        swprintf( wszBuff, L"RPMs: %0.f", m_fRPMs );
        m_Font.DrawText( 64, 200, 0xFFFFFF00, wszBuff );

        // Show current engine gear
        swprintf( wszBuff, L"Gear: %d", m_dwGear + 1 );
        m_Font.DrawText( 64, 250, 0xFFFFFF00, wszBuff );

        // Show when in shift mode
        if ( m_bInShift )
        {
            swprintf( wszBuff, L"IN SHIFT" );
            m_Font.DrawText( 64, 300, 0xFF0000FF, wszBuff );
        }

        // Show when engine hits redline
        if ( m_fRPMs >= RPM_REDLINE )
        {
            swprintf( wszBuff, L"REDLINE!!!" );
            m_Font.DrawText( 64, 350, 0xFFFF0000, wszBuff );
        }

        // Update the on-screen audio level meters
        XBSound_DrawLevelMeters( m_pDSound, 64.0f, 400.0f, 60.0f, 30.0f );
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: AdjustParamControl()
// Desc: Sets the parameter control(s) for the XACT engine. For the settings to 
//       take effect immediately, XACTEngineDoWork() must be called after 
//       calling SetParameterControl().
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::AdjustParamControl( FLOAT fStartValue, CHAR* pszFriendlyName )
{
    XACT_PARAMETER_ENTRY        xpe;
    XACT_PARAMETER_CONTROL_DESC xpcd;

    // Initialize the parameter control structs
    xpcd.dwEntryCount = 1;
    xpcd.paEntries    = &xpe;

    // Set the parameter contol data
    xpe.wScope = eXACTParameterControlScope_SoundCueInstance;   // Indicates the scope of the control
    xpe.wFlags = 0;                                             // Parameter control attribute flags
    xpe.wCategoryIndex = 0;                                     // Not currently supported
    xpe.wStepCount = 0;                                         // Number of steps applied to the control
    xpe.flStepSize = 0;                                         // Indicates the amount to adjust the control per step
    xpe.flStartValue = fStartValue;                             // Indicates the starting value of the control
    xpe.u.pSoundCue = m_pSoundCue;                              // A pointer to a valid sound cue instance
    xpe.pszFriendlyName = pszFriendlyName;                      // String value representing the friendly name of the control

    // Set the parameter control values with XACT
    if ( FAILED( m_pXACT->SetParameterControl( &xpcd ) ) )
    {
        return E_FAIL;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: ShiftGearSounds()
// Desc: Shift the gear sounds from one RPM limit to the next
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::ShiftGearSounds( BOOL bUpshift )
{
    // Shift the engine to the new gear
    if( bUpshift )
        m_dwGear++;
    else
        m_dwGear -= 1;

    // Assert the gear is in range
    assert( ( GEAR_MIN <= m_dwGear ) && ( GEAR_MAX >= m_dwGear ) );

    // Play new engine sound based on the new gear
    m_pSoundBank->Play( m_dwGear, m_pSoundSource, XACT_FLAG_SOUNDCUE_AUTORELEASE, &m_pSoundCue );
    m_bInShift = TRUE;
    
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: ResetEngineState()
// Desc: Reset the engine state back to idle condition
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::ResetEngineState()
{
    // Reset all engine objects
    m_fAcceleration     = 0.0f;
    m_dwGear            = GEAR_MIN;
    m_fRPMs             = RPM_MIN;
    m_bInShift          = FALSE;

    // Prepare our engine sound cue We are not actually priming
    // a stream cue, but we use Prepare() to get back a SoundCue
    // handle so we can prime the Parameter Control value.
    m_pSoundBank->Prepare( m_dwGear, XACT_FLAG_SOUNDCUE_PRIME, &m_pSoundCue );

    // Set initial RPM parameter control value with XACT
    AdjustParamControl( 1, "RPM" );

    // Pump XACT's work queue to prime the starting sound cue parameters
    XACTEngineDoWork();

    // Play the initial IDLE sound
    m_pSoundBank->Play( m_dwGear, m_pSoundSource, XACT_FLAG_SOUNDCUE_PREPARED | XACT_FLAG_SOUNDCUE_AUTORELEASE, &m_pSoundCue );

    return S_OK;
}
