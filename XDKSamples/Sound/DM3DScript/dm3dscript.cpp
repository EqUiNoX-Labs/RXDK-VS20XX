//-----------------------------------------------------------------------------
// File: DM3DScript.cpp
//
// Desc: This sample demonstrates how to use the 3D capabilities of 
//       DirectMusic, moving a sound source and listener in 3D.
//
// Hist: 05.14.01 - New for June XDK Release
//       03.06.02 - Added audio level meters for April 02 XDK release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <XBSound.h>
#include <dsound.h>
#include <dmusici.h>
#include <dmusicfx.h>
#include "myfactory.h"


//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_BACK_BUTTON,  XBHELP_PLACEMENT_1, L"Display help" },
    { XBHELP_A_BUTTON,     XBHELP_PLACEMENT_2, L"Toggle\nsound" },
    { XBHELP_B_BUTTON,     XBHELP_PLACEMENT_2, L"Change\nsound" },
    { XBHELP_X_BUTTON,     XBHELP_PLACEMENT_2, L"Toggle source/\nlistener" },
    { XBHELP_Y_BUTTON,     XBHELP_PLACEMENT_2, L"Toggle\nheadphones" },
    { XBHELP_WHITE_BUTTON, XBHELP_PLACEMENT_2, L"Increase\nvolume" },
    { XBHELP_BLACK_BUTTON, XBHELP_PLACEMENT_2, L"Decrease\nvolume" },
    { XBHELP_RIGHTSTICK,   XBHELP_PLACEMENT_1, L"Move object in Y" },
    { XBHELP_LEFTSTICK,    XBHELP_PLACEMENT_2, L"Move object\nin X/Z" },
};

const DWORD NUM_HELP_CALLOUTS = sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]);




//-----------------------------------------------------------------------------
// Global variables and definitions
//-----------------------------------------------------------------------------

struct D3DVERTEX
{
    D3DXVECTOR3 p;           // position
    D3DCOLOR    c;           // color
};
#define D3DFVF_D3DVERTEX (D3DFVF_XYZ|D3DFVF_DIFFUSE)


// Constants to define our world space
#define XMIN -10
#define XMAX 10
#define ZMIN -10
#define ZMAX 10
#define YMIN 0
#define YMAX 5

// Constants for colors
static const DWORD SOURCE_COLOR   = 0xFFEA1B1B;
static const DWORD LISTENER_COLOR = 0xFF1B1BEA;

// Constants for scaling input
#define MOTION_SCALE 10.0f
#define VOLUME_SCALE  5.0f




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    CXBFont                 m_Font;                 // Font object
    CXBHelp                 m_Help;                 // Help object

    LPDIRECTSOUND8            m_pDSound;          // DirectSound object
    IDirectMusicLoader8*      m_pLoader;          // DM Loader
    IDirectMusicPerformance8* m_pPerformance;     // DM Performance
    IDirectMusicAudioPath8*   m_pAudioPath;       // DM AudioPath
    LPDIRECTSOUNDBUFFER8      m_p3DBuffer;        // 3D buffer from audiopath
    IDirectMusicScript8*      m_pScript;          // DM Script
    FLOAT                     m_fVolume;          // Current volume
    BOOL                      m_bHeadphones;      // True if headphones enabled

    // Sound source and listener positions
    D3DXVECTOR3             m_vSourcePosition;      // Source position vector
    D3DXVECTOR3             m_vListenerPosition;    // Listener position vector

    // Transform matrices
    D3DXMATRIX              m_matWorld;             // World transform
    D3DXMATRIX              m_matView;              // View transform
    D3DXMATRIX              m_matProj;              // Projection transform

    // Models for floor, source, and listener
    LPDIRECT3DVERTEXBUFFER8 m_pvbFloor;             // Quad for the floor
    LPDIRECT3DVERTEXBUFFER8 m_pvbSource;            // Quad for the source
    LPDIRECT3DVERTEXBUFFER8 m_pvbListener;          // Quad for the listener
    LPDIRECT3DVERTEXBUFFER8 m_pvbGrid;              // Lines to grid the floor

    D3DCOLOR        m_dwSourceColor;                // Color for sound source
    D3DCOLOR        m_dwListenerColor;              // Color for listener

    BOOL            m_bDrawHelp;                    // Should we draw help?
    BOOL            m_bControlSource;               // Control source (TRUE) or

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
    m_bDrawHelp   = FALSE;
    m_bHeadphones = FALSE;

    // Sounds
    m_fVolume = DSBVOLUME_MAX;

    // Positions
    m_vSourcePosition   = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );
    m_vListenerPosition = D3DXVECTOR3( 0.0f, 0.0f, ZMIN );
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: 
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    // Create a font
    if( FAILED( m_Font.Create( "Font.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create help
    if( FAILED( m_Help.Create( "Gamepad.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create DirectSound object
    if( FAILED( DirectSoundCreate( NULL, &m_pDSound, NULL ) ) )
        return E_FAIL;

    // If the application doesn't care about vertical HRTF positioning,
    // calling DirectSoundUseLightHRTF can save about 60k of memory.
    // DirectSoundUseLightHRTF();
    DirectSoundUseFullHRTF();

    // Download the standard DirectSound effects image
    DSEFFECTIMAGELOC EffectLoc;
    EffectLoc.dwI3DL2ReverbIndex = GraphI3DL2_I3DL2Reverb;
    EffectLoc.dwCrosstalkIndex   = GraphXTalk_XTalk;
    LPDSEFFECTIMAGEDESC pDesc;
    if( FAILED( XAudioDownloadEffectsImage( "d:\\Media\\dmusicfx.bin", &EffectLoc,
                                            XAUDIO_DOWNLOADFX_EXTERNFILE, &pDesc ) ) )
        return E_FAIL;

    // Initialize DMusic
    IDirectMusicHeap* pNormalHeap;
    DirectMusicCreateDefaultHeap( &pNormalHeap );

    IDirectMusicHeap* pPhysicalHeap;
    DirectMusicCreateDefaultPhysicalHeap( &pPhysicalHeap );

    DirectMusicInitializeEx( pNormalHeap, pPhysicalHeap, MyFactory );

    pNormalHeap->Release();
    pPhysicalHeap->Release();

    // Create DirectMusic loader object
    DirectMusicCreateInstance( CLSID_DirectMusicLoader, NULL, 
                               IID_IDirectMusicLoader8, (VOID**)&m_pLoader );

    // Create DirectMusic performance object
    DirectMusicCreateInstance( CLSID_DirectMusicPerformance, NULL,
                               IID_IDirectMusicPerformance8, (VOID**)&m_pPerformance );

    // Initialize the performance with a 3D audiopath.
    m_pPerformance->InitAudioX( DMUS_APATH_DYNAMIC_3D, 64, 128, 0 );

    // Tell DirectMusic where the default search path is
    m_pLoader->SetSearchDirectory( GUID_DirectMusicAllTypes, 
                                   "D:\\Media\\Sounds", FALSE );

    // Get 3D audiopath.
    m_pPerformance->GetDefaultAudioPath( &m_pAudioPath );

    // Max volume for music
    m_pAudioPath->SetVolume( (LONG)m_fVolume, 0 );
    m_pAudioPath->GetObjectInPath( DMUS_PCHANNEL_ALL, DMUS_PATH_BUFFER, 0,
                                   GUID_NULL, 0, GUID_NULL, (VOID **)&m_p3DBuffer );

    if( FAILED( m_pLoader->LoadObjectFromFile( CLSID_DirectMusicScript, IID_IDirectMusicScript8, 
                                               "dm3dscript.spt", (VOID **)&m_pScript ) ) )
        return E_FAIL;

    m_pScript->Init( m_pPerformance, NULL );
    m_pScript->CallRoutine( "Initialize", NULL );
    m_pScript->CallRoutine( "PlaySound", NULL );

    // Set the transform matrices
    D3DXVECTOR3 vEyePt      = D3DXVECTOR3( XMIN, 45.0f,  ZMAX / 2.0f );
    D3DXVECTOR3 vLookatPt   = D3DXVECTOR3( XMIN,  0.0f,  ZMAX / 2.0f );
    D3DXVECTOR3 vUpVec      = D3DXVECTOR3( 0.0f,  0.0f,  1.0f );
    D3DXMatrixIdentity( &m_matWorld );
    D3DXMatrixLookAtLH( &m_matView, &vEyePt, &vLookatPt, &vUpVec );
    D3DXMatrixPerspectiveFovLH( &m_matProj, D3DX_PI/4, 4.0f/3.0f, 1.0f, 10000.0f );

    m_pd3dDevice->SetTransform( D3DTS_WORLD, &m_matWorld );
    m_pd3dDevice->SetTransform( D3DTS_VIEW, &m_matView );
    m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &m_matProj );

    // Create our vertex buffers
    m_pd3dDevice->CreateVertexBuffer( 4 * sizeof( D3DVERTEX ), 0, 0, 0, &m_pvbFloor );
    m_pd3dDevice->CreateVertexBuffer( 4 * sizeof( D3DVERTEX ), 0, 0, 0, &m_pvbSource );
    m_pd3dDevice->CreateVertexBuffer( 4 * sizeof( D3DVERTEX ), 0, 0, 0, &m_pvbListener );
    m_pd3dDevice->CreateVertexBuffer( 2 * ( ( ZMAX - ZMIN + 1 ) + ( XMAX - XMIN + 1 ) ) * sizeof( D3DVERTEX ), 0, 0, 0, &m_pvbGrid );
    
    D3DVERTEX* pVertices;

    // Fill the VB for the floor
    m_pvbFloor->Lock( 0, 0, (BYTE **)&pVertices, 0 );
    pVertices[0].p = D3DXVECTOR3( XMIN, 0.0f, ZMIN ); pVertices[0].c = 0xFF101010;
    pVertices[1].p = D3DXVECTOR3( XMIN, 0.0f, ZMAX ); pVertices[1].c = 0xFF101010;
    pVertices[2].p = D3DXVECTOR3( XMAX, 0.0f, ZMIN ); pVertices[2].c = 0xFF101010;
    pVertices[3].p = D3DXVECTOR3( XMAX, 0.0f, ZMAX ); pVertices[3].c = 0xFF101010;
    m_pvbFloor->Unlock();

    // Fill the VB for the grid
    m_pvbGrid->Lock( 0, 0, (BYTE **)&pVertices, 0 );
    for( int i = ZMIN, j = 0; i <= ZMAX; i++, j++ )
    {
        pVertices[ j * 2 ].p     = D3DXVECTOR3( XMIN, 0, (FLOAT)i ); pVertices[ j * 2 ].c     = 0xFF00A000;
        pVertices[ j * 2 + 1 ].p = D3DXVECTOR3( XMAX, 0, (FLOAT)i ); pVertices[ j * 2 + 1 ].c = 0xFF00A000;
    }
    for( i = XMIN; i <= XMAX; i++, j++ )
    {
        pVertices[ j * 2 ].p     = D3DXVECTOR3( (FLOAT)i, 0, ZMIN ); pVertices[ j * 2 ].c     = 0xFF00A000;
        pVertices[ j * 2 + 1 ].p = D3DXVECTOR3( (FLOAT)i, 0, ZMAX ); pVertices[ j * 2 + 1 ].c = 0xFF00A000;
    }
    m_pvbGrid->Unlock();


    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    D3DVERTEX *     pVertices;
    D3DXVECTOR3     vSourceOld   = m_vSourcePosition;
    D3DXVECTOR3     vListenerOld = m_vListenerPosition;
    D3DXVECTOR3 *   pvControl;
    DWORD           dwPulse = DWORD( ( cosf( m_fAppTime * 6.0f ) + 1.0f ) * 50 );
    D3DCOLOR        cBlend = dwPulse | ( dwPulse << 8 ) | ( dwPulse << 16 );

    // Toggle help
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_BACK ) 
    {
        m_bDrawHelp = !m_bDrawHelp;
    }

    // Increase/Decrease volume
    m_fVolume += ( m_DefaultGamepad.bAnalogButtons[XINPUT_GAMEPAD_WHITE] - 
                   m_DefaultGamepad.bAnalogButtons[XINPUT_GAMEPAD_BLACK] ) *
                 m_fElapsedTime * VOLUME_SCALE;

    // Make sure volume is in the appropriate range
    if( m_fVolume < DSBVOLUME_MIN )
        m_fVolume = DSBVOLUME_MIN;
    else if( m_fVolume > DSBVOLUME_MAX )
        m_fVolume = DSBVOLUME_MAX;

    // Toggle sound on and off
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
    {
        m_pScript->CallRoutine( "TogglePlayback", NULL );
    }

    // Cycle through sounds
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_B] )
    {
        m_pScript->CallRoutine( "NextSegment", NULL );
    }

    // Switch which of source vs. listener we are moving
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_X] )
    {
        m_bControlSource = !m_bControlSource;
    }

    // Toggle headphones
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_Y] )
    {
        m_bHeadphones = !m_bHeadphones;
        m_pDSound->EnableHeadphones( m_bHeadphones );
    }


    // Set up our colors
    m_dwSourceColor   = SOURCE_COLOR   | ( m_bControlSource ? cBlend : 0 );
    m_dwListenerColor = LISTENER_COLOR | ( m_bControlSource ? 0 : cBlend );

    // Point to the appropriate vector
    pvControl = m_bControlSource ? &m_vSourcePosition : &m_vListenerPosition;

    // Move selected object and clamp to the appropriate range
    pvControl->x += m_DefaultGamepad.fX1 * m_fElapsedTime * MOTION_SCALE;
    if( pvControl->x < XMIN )
        pvControl->x = XMIN;
    else if( pvControl->x > XMAX )
        pvControl->x = XMAX;

    pvControl->z += m_DefaultGamepad.fY1 * m_fElapsedTime * MOTION_SCALE;
    if( pvControl->z < ZMIN )
        pvControl->z = ZMIN;
    else if( pvControl->z > ZMAX )
        pvControl->z = ZMAX;

    pvControl->y += m_DefaultGamepad.fY2 * m_fElapsedTime * MOTION_SCALE;
    if( pvControl->y < YMIN )
        pvControl->y = YMIN;
    else if( pvControl->y > YMAX )
        pvControl->y = YMAX;

    //
    // Update source/listener vertex buffers
    //
    m_pvbSource->Lock( 0, 0, (BYTE **)&pVertices, 0 );
    pVertices[0].p = m_vSourcePosition + D3DXVECTOR3( -0.5f, 0.0f, -0.5f ); pVertices[0].c = m_dwSourceColor;
    pVertices[1].p = m_vSourcePosition + D3DXVECTOR3( -0.5f, 0.0f,  0.5f ); pVertices[1].c = m_dwSourceColor;
    pVertices[2].p = m_vSourcePosition + D3DXVECTOR3(  0.5f, 0.0f, -0.5f ); pVertices[2].c = m_dwSourceColor;
    pVertices[3].p = m_vSourcePosition + D3DXVECTOR3(  0.5f, 0.0f,  0.5f ); pVertices[3].c = m_dwSourceColor;
    m_pvbSource->Lock( 0, 0, (BYTE **)&pVertices, 0 );

    m_pvbListener->Lock( 0, 0, (BYTE **)&pVertices, 0 );
    pVertices[0].p = m_vListenerPosition + D3DXVECTOR3( -0.5f, 0.0f, -0.5f ); pVertices[0].c = m_dwListenerColor;
    pVertices[1].p = m_vListenerPosition + D3DXVECTOR3( -0.5f, 0.0f,  0.5f ); pVertices[1].c = m_dwListenerColor;
    pVertices[2].p = m_vListenerPosition + D3DXVECTOR3(  0.5f, 0.0f, -0.5f ); pVertices[2].c = m_dwListenerColor;
    pVertices[3].p = m_vListenerPosition + D3DXVECTOR3(  0.5f, 0.0f,  0.5f ); pVertices[3].c = m_dwListenerColor;
    m_pvbListener->Lock( 0, 0, (BYTE **)&pVertices, 0 );

    m_pAudioPath->SetVolume( (LONG)m_fVolume, 0 );

    // Position the sound and listener in 3D.  We use DS3D_DEFERRED so that 
    // all the changes will be committed at once.  Scale the velocities by 2 
    // so that doppler effect is a bit more noticeable.
    D3DXVECTOR3 vListenerVelocity = 2.0f * ( m_vListenerPosition - vListenerOld ) / m_fElapsedTime;
    D3DXVECTOR3 vSoundVelocity = 2.0f * ( m_vSourcePosition - vSourceOld ) / m_fElapsedTime;

    // Source position/velocity/volume
    m_p3DBuffer->SetPosition( m_vSourcePosition.x, m_vSourcePosition.y, m_vSourcePosition.z, DS3D_DEFERRED );
    m_p3DBuffer->SetVelocity( vSoundVelocity.x, vSoundVelocity.y, vSoundVelocity.z, DS3D_DEFERRED );

    // Listener position/velocity
    m_pDSound->SetPosition( m_vListenerPosition.x, m_vListenerPosition.y, m_vListenerPosition.z, DS3D_DEFERRED  );
    m_pDSound->SetVelocity( vListenerVelocity.x, vListenerVelocity.y, vListenerVelocity.z, DS3D_DEFERRED );

    // Commit position/velocity changes
    m_pDSound->CommitDeferredSettings();

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
    // Pump DirectSound's work queue
    DirectSoundDoWork();

    RenderGradientBackground( 0xff404040, 0xff404080 );

    // Set default render states
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE,          TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_LIGHTING,         FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_DISABLE );
    m_pd3dDevice->SetVertexShader( D3DFVF_D3DVERTEX );

    // Draw the floor
    m_pd3dDevice->SetStreamSource( 0, m_pvbFloor, sizeof( D3DVERTEX ) );
    m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 );

    // Draw the grid
    m_pd3dDevice->SetStreamSource( 0, m_pvbGrid, sizeof( D3DVERTEX ) );
    m_pd3dDevice->DrawPrimitive( D3DPT_LINELIST, 0, 2 * ( ( ZMAX - ZMIN + 1 ) + ( XMAX - XMIN + 1 ) ) );

    // Draw the source
    m_pd3dDevice->SetStreamSource( 0, m_pvbSource, sizeof( D3DVERTEX ) );
    m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 );

    // Draw the listener
    m_pd3dDevice->SetStreamSource( 0, m_pvbListener, sizeof( D3DVERTEX ) );
    m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 );

    // Show title, frame rate, and help
    if( m_bDrawHelp )
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    else
    {
        m_Font.Begin();

        static char* pstrSegment = NULL;
        static LONG  lAllocated = 0;
        WCHAR strBuffer[200];

        // Show title
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText(  48, 36, 0xffffffff, L"DM3DScript" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );

        // Ask the script if a segment is playing
        LONG lPlaying;
        m_pScript->GetVariableNumber( "Playing", &lPlaying, NULL );

        // Ask the script which segment is selected:
        // First, find out how long the string is
        LONG lNeeded;
        m_pScript->GetVariableString( "SegmentName", NULL, 0, &lNeeded, NULL );

        // Then make sure we have enough space and get the value
        if( lNeeded > lAllocated )
        {
            delete[] pstrSegment;
            pstrSegment = new char[lNeeded];
            lAllocated = lNeeded;
        }
        m_pScript->GetVariableString( "SegmentName", pstrSegment, lNeeded, &lNeeded, NULL );

        // Show status
        swprintf( strBuffer, L"<%0.1f, %0.1f, %0.1f>", m_vSourcePosition.x, m_vSourcePosition.y, m_vSourcePosition.z );
        m_Font.DrawText( 80, 95, 0xffffffff, GLYPH_X_BUTTON, XBFONT_RIGHT );
        m_Font.DrawText( 80, 80, 0xffffffff, L"Source: " );
        m_Font.DrawText( m_dwSourceColor, strBuffer );
        
        swprintf( strBuffer, L"<%0.1f, %0.1f, %0.1f>", m_vListenerPosition.x, m_vListenerPosition.y, m_vListenerPosition.z );
        m_Font.DrawText( 80, 110, 0xffffffff, L"Listener: " );
        m_Font.DrawText( m_dwListenerColor, strBuffer );
        
        swprintf( strBuffer, L"%S", pstrSegment );
        m_Font.DrawText( 80, 150, 0xffffffff, GLYPH_B_BUTTON, XBFONT_RIGHT );
        m_Font.DrawText( 80, 150, 0xffffffff, L"Current Sound: " );
        m_Font.DrawText( lPlaying ? 0xffffff00 : 0xff808000, strBuffer );
        m_Font.DrawText( 0xff808000, lPlaying ? L"" : L" (paused)" );
        m_Font.DrawText( 80, 180, 0xffffffff, GLYPH_A_BUTTON, XBFONT_RIGHT );
        m_Font.DrawText( 80, 180, 0xffffffff, L"Pause" );
        
        // Show percentage and volume (rounded to nearest dB)
        FLOAT fPercent = powf( 10, m_fVolume / 2000.0f ) * 100;
        swprintf( strBuffer, L"%ddB (%0.0f%%)", ( LONG(m_fVolume) - 50 ) / 100, fPercent );
        m_Font.DrawText( 62, 220, 0xffffffff, GLYPH_WHITE_BUTTON, XBFONT_RIGHT );
        m_Font.DrawText( 80, 220, 0xffffffff, GLYPH_BLACK_BUTTON, XBFONT_RIGHT );
        m_Font.DrawText( 80, 220, 0xffffffff, L"Volume: " );
        m_Font.DrawText( 0xffffff00, strBuffer );
        
        m_Font.DrawText( 80, 260, 0xffffffff, GLYPH_Y_BUTTON, XBFONT_RIGHT );
        m_Font.DrawText( 80, 260, 0xffffffff, L"Headphones: " );
        if( m_bHeadphones )
            m_Font.DrawText( 0xffffff00, L"enabled");
        else
            m_Font.DrawText( 0xff808000, L"disabled");

        m_Font.DrawText( 80, 300, 0xffffffff, GLYPH_BACK1_BUTTON GLYPH_BACK2_BUTTON, XBFONT_RIGHT );
        m_Font.DrawText( 80, 300, 0xffffffff, L"Help" );

        m_Font.End();
    }

    // Draw the on-screen audio level meters
    XBSound_DrawLevelMeters( m_pDSound, 64.0f, 400.0f, 60.0f, 30.0f );

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}




