//-----------------------------------------------------------------------------
// File: FastLoad.cpp
//
// Desc: App class and globals
//
// Hist: 03.11.02 - New for May XDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <xgraphics.h>
#include "LevelLoader.h"
#include "WMAInMemory.h"


//-----------------------------------------------------------------------------
// Call outs for labelling the game pad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_BACK_BUTTON,  XBHELP_PLACEMENT_1, L"Display help" },
    { XBHELP_A_BUTTON,     XBHELP_PLACEMENT_2, L"Load next\nlevel" },
    { XBHELP_B_BUTTON,     XBHELP_PLACEMENT_2, L"Format utility\npartition" },
    { XBHELP_X_BUTTON,     XBHELP_PLACEMENT_2, L"Reset min\nFPS" },
    { XBHELP_Y_BUTTON,     XBHELP_PLACEMENT_1, L"Corrupt cache" },
    { XBHELP_BLACK_BUTTON, XBHELP_PLACEMENT_1, L"Reboot" },
    { XBHELP_LEFTSTICK,    XBHELP_PLACEMENT_1, L"Rotate" },
    { XBHELP_RIGHTSTICK,   XBHELP_PLACEMENT_1, L"Zoom" },
    { XBHELP_START_BUTTON, XBHELP_PLACEMENT_1, L"Pause Music" },
    { XBHELP_DPAD,         XBHELP_PLACEMENT_2, L"Switch between\nthreaded and\noverlapped" },
};

#define NUM_HELP_CALLOUTS ( sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]) )




//-----------------------------------------------------------------------------
// Global list of the available levels
//-----------------------------------------------------------------------------
CHAR* g_strLevels[] = 
{
    "Resource0",
    "Resource1",
    "Resource2"
};
#define NUM_LEVELS ( sizeof(g_strLevels) / sizeof(g_strLevels[0]) )




//-----------------------------------------------------------------------------
// Initial contiguous memory allocation size and system memory allocation size
// (should be the same sizes as required by the largest level
//-----------------------------------------------------------------------------
#define INITVIDMEMSIZE ( 54*1024*1024 )
#define INITSYSMEMSIZE ( 2*1024 ) // The XPR header is DVD sector aligned, and
                                  // our levels don't have more than 2k's worth
                                  // of system memory data




//-----------------------------------------------------------------------------
// Name: VERTEX
// Desc: Vertices used to render textured quads
//-----------------------------------------------------------------------------
struct VERTEX
{
    D3DXVECTOR3 pos;
    D3DXVECTOR2 texcoord;
};


//-----------------------------------------------------------------------------
// Name: StartTime
// Desc: Global start timer
//-----------------------------------------------------------------------------
DOUBLE g_dStartTime;




//-----------------------------------------------------------------------------
// Name: CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    // Type of IO 
    enum IOType
    {
        Threaded,   // Using Threaded non-bufferd IO
        Overlapped  // Using Overlapped non-buffered IO
    };

    CXBFont             m_Font;         // Font object
    CXBHelp             m_Help;         // Help object
    CXBPackedResource   m_Resources;    // additional textures

    CLevelLoader*       m_pLevelLoader; // Loads level data
    CWMAFileStream      m_WaveStream;   // WMA steam class

    IDirect3DVertexBuffer8* m_pOneQuadVB;   // Circle quad VB

    BOOL        m_bDrawHelp;                // TRUE to draw help screen

    DWORD       m_dwCurrentLevel;           // Current working level

    BOOL        m_bFirstLoad;               // First load
    DOUBLE      m_dStartTime;               // App start time (including initialize)

    BOOL        m_bUtilityFormatted;        // Utility parition has been formatted
    BOOL        m_bCorruptCache;            // Corrupt the utility partition cache

    FLOAT       m_fCenterRotTheta;          // Rotation of center quad
    BOOL        m_bPausedMusic;             // Music is paused

    BOOL        m_bReboot;                  // Soft reboots to self
    BOOL        m_bRebootToDash;            // Soft reboot to dash (waits for IO to complete)
                                            // Hard Reboots are handled by signature verfication, etc.

    // WVP matrices
    D3DXMATRIX  m_matWorld;
    D3DXMATRIX  m_matView;
    D3DXMATRIX  m_matProjection;

    // Camera parameters
    FLOAT       m_fYaw;
    FLOAT       m_fPitch;
    FLOAT       m_fZoom;

    // Reset min FPS
    BOOL        m_bResetFPSMinMax;
    DOUBLE      m_dResetTime;

    // Level memory
    BYTE*       m_pSysMemData;
    BYTE*       m_pVidMemData;

    // Type of IO we are currently using
    IOType      m_IOType;

public:
    // Initializes the level loaded with the set of levels
    HRESULT     InitializeLevelLoader();

    // Test function
    VOID        CorruptCache( CHAR** astrLevels, DWORD dwNumLevels );

    // Grabs as much contiguous memory as possible for the level data
    VOID        AllocateLevelMemory();

public:
    virtual HRESULT     Initialize();
    virtual HRESULT     Render();
    virtual HRESULT     FrameMove();
    virtual HRESULT     RebootToDash();

    CXBoxSample();
};




//-----------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program.
//-----------------------------------------------------------------------------
VOID __cdecl main()
{
    // Record app start time
    g_dStartTime = GetTimeInSeconds();

    CXBoxSample xbApp;

    // Mount utility drive
    XMountUtilityDrive( FALSE );

    // Un-comment to get more info about HD, DVD and file system
    //DWORD dwDVDClusterSize  = XGetDiskClusterSize("D:\\");
    //DWORD dwUTILClusterSize = XGetDiskClusterSize("Z:\\");
    //DWORD dwDVDSectorSize   = XGetDiskSectorSize("D:\\");
    //DWORD dwUTILSectorSize  = XGetDiskSectorSize("Z:\\");
    //SIZE_T FileCacheSize    = XGetFileCacheSize();
        
    // Initialize the levels. Do this first to get the most contiguous memory.
    xbApp.AllocateLevelMemory();

    if( FAILED( xbApp.Create() ) )
        return;
    xbApp.Run();
}




//-----------------------------------------------------------------------------
// Name: CXBoxSample()
// Desc: Constructor for CXBoxSample class
//-----------------------------------------------------------------------------
CXBoxSample::CXBoxSample() 
{   
    // Set presentation interval to be immediate for frame rate testing
    m_d3dpp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    m_bDrawHelp    = FALSE;
    m_pOneQuadVB   = NULL;
    m_pLevelLoader = NULL;
    m_pSysMemData  = NULL;
    m_pVidMemData  = NULL;
}




//-----------------------------------------------------------------------------
// Name: AllocateLevelMemory()
// Desc: gets the video and system memory for the level
//-----------------------------------------------------------------------------
VOID CXBoxSample::AllocateLevelMemory()
{
    // Allocate system memory buffer
    assert( NULL == m_pSysMemData );
    m_pSysMemData = (BYTE*)malloc( INITSYSMEMSIZE );
    assert( m_pSysMemData );

    // Allocate initial video memory buffer
    assert( NULL == m_pVidMemData );
    m_pVidMemData = (BYTE*)D3D_AllocContiguousMemory( INITVIDMEMSIZE,
                                                      D3DTEXTURE_ALIGNMENT );
    assert( m_pVidMemData );
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Performs initialization
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    m_bFirstLoad = TRUE;
    m_bUtilityFormatted = FALSE;
    m_bCorruptCache = FALSE;
    m_bPausedMusic = FALSE;
    m_bRebootToDash = FALSE;
    m_bReboot = FALSE;

    // Create a font
    if( FAILED( m_Font.Create( "Font.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create help
    if( FAILED( m_Help.Create( "Gamepad.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create resource
    if( FAILED( m_Resources.Create( "resource.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create wave stream
    if( FAILED( m_WaveStream.Initialize( "D:\\media\\sounds\\becky.wma" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Make sure media directory is on the HD utility section
    CreateDirectory( "Z:\\media", NULL );

    // Create center quad
    VERTEX* pVerts;
    g_pd3dDevice->CreateVertexBuffer( 4*sizeof(VERTEX), 0,0,0, &m_pOneQuadVB );
    m_pOneQuadVB->Lock( 0, 0, (BYTE**)&pVerts, 0 );
    FLOAT fSize = 3.0f;
    pVerts[0].pos = D3DXVECTOR3( -fSize, -fSize, 1.5 );
    pVerts[1].pos = D3DXVECTOR3( -fSize,  fSize, 1.5 );
    pVerts[2].pos = D3DXVECTOR3(  fSize,  fSize, 1.5 );
    pVerts[3].pos = D3DXVECTOR3(  fSize, -fSize, 1.5 );
    pVerts[0].texcoord = D3DXVECTOR2( 0, 1 );
    pVerts[1].texcoord = D3DXVECTOR2( 0, 0 );
    pVerts[2].texcoord = D3DXVECTOR2( 1, 0 );
    pVerts[3].texcoord = D3DXVECTOR2( 1, 1 );
    m_pOneQuadVB->Unlock();

    // Initialize projection matrix
    D3DXMatrixPerspectiveFovLH( &m_matProjection, D3DX_PI/4, 640.f/480.f, 1.0f, 5000.0f );

    // Initialize world matrix 
    D3DXMatrixIdentity( &m_matWorld );

    // Initialize camera
    m_fYaw   = 0.0f;
    m_fPitch = D3DX_PI/10.0f;
    m_fZoom  = 60.0f;

    // Initialize center quad rotation
    m_fCenterRotTheta = 0.0f;

    // Initialize view matrix 
    D3DXMatrixIdentity( &m_matView );

    // Start with threaded IO
    m_IOType = Threaded;
    if( FAILED( InitializeLevelLoader() ) )
        return E_FAIL;
    
    // Record start time
    m_dStartTime = GetTimeInSeconds() - g_dStartTime;

    // Start recording lowest frame rate after load
    m_dResetTime = GetTimeInSeconds();
    m_bResetFPSMinMax = TRUE;

    // Stream in level first level
    m_dwCurrentLevel = 0;
    m_pLevelLoader->AsyncStreamLevel( m_dwCurrentLevel );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: RebootToDash()
// Desc: Sets m_bRebootToDash to true, which causes a reboot to the dash
//       when IO is idle
// Note: Reboots should not be allowed when IO is pending
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::RebootToDash()
{
    m_bRebootToDash = TRUE;

    return S_OK;
}
            
    


//-----------------------------------------------------------------------------
// Name: InitializeLevelLoader
// Desc: Creates the level loader based off of the requested IO type
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::InitializeLevelLoader()
{
    // Delete the current loaded
    SAFE_DELETE( m_pLevelLoader );

    // Create new loaded bases on requested type
    if( m_IOType == Threaded )
        m_pLevelLoader = new CThreadedLevelLoader;
    else
        m_pLevelLoader = new COverlappedLevelLoader;

    // Initialize the loaded with our set of levels
    return m_pLevelLoader->Initialize( g_strLevels, NUM_LEVELS,
                                       m_pSysMemData, INITSYSMEMSIZE,
                                       m_pVidMemData, INITVIDMEMSIZE ); 
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Performs per-frame updates
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    // Process the streaming sound
    DirectSoundDoWork();

    // Toggle help
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_BACK ) 
    {
        m_bDrawHelp = !m_bDrawHelp;
    }

    // Reset min FPS
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_X] )
    {
         m_bResetFPSMinMax = TRUE;
         m_dResetTime = GetTimeInSeconds();
    }

    // Move camera
    FLOAT fRotationScale =  1.0f * m_fElapsedTime;
    FLOAT fZoomScale     = 10.0f * m_fElapsedTime;
    m_fYaw   += fRotationScale * m_DefaultGamepad.fX1;
    m_fPitch += fRotationScale * m_DefaultGamepad.fY1;
    m_fZoom  += fZoomScale * m_DefaultGamepad.fY2;
    D3DXMATRIX matRotate, matTrans, matCamera;
    D3DXMatrixRotationYawPitchRoll( &matRotate, m_fYaw, m_fPitch, 0.0f );
    D3DXMatrixTranslation( &matTrans, 0, 0, -m_fZoom );
    matCamera = matTrans * matRotate;

    // Set view
    FLOAT fDet;
    D3DXMatrixInverse( &m_matView, &fDet, &matCamera );
    assert(fDet > 0.0f);

    // Update center quad spin
    m_fCenterRotTheta += 0.25f * m_fElapsedTime;

    // Update level streaming state
    // Does nothing when IO is idle or we are using Threaded IO
    m_pLevelLoader->Update();
    
    // Don't draw stats if we are drawing help
    if( !m_bDrawHelp )
    {
        // Wait for idle IO before reboot
        if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_BLACK] )
            m_bReboot = TRUE;

        // Clear, corrupt, and advance to next level only happen when idle
        if( m_pLevelLoader->IsIdle() )
        {
            if( m_bRebootToDash )
            {
                LD_LAUNCH_DASHBOARD LaunchData = { XLD_LAUNCH_DASHBOARD_MAIN_MENU };
                XLaunchNewImage( NULL, (LAUNCH_DATA*)&LaunchData );
            }

            // Reset with a new utility partition
            if( m_bReboot )
            {
                // Find the first xbe on the DVD (or dev HD partition)
                WIN32_FIND_DATA FindData;
                HANDLE hFind = FindFirstFile( "D:\\*.xbe" , &FindData );
                assert( hFind != INVALID_HANDLE_VALUE );            
                FindClose( hFind );

                // Launch it
                // Note: Reboots should not be allowed when IO is pending
                char strBuffer[MAX_PATH];
                sprintf( strBuffer, "D:\\%s", FindData.cFileName ); 
                XLaunchNewImage( strBuffer, NULL );
                assert( FALSE );
            }

            // Format utility partition
            if( !m_bUtilityFormatted )
            {
                if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_B] )
                {
                    XFormatUtilityDrive();

                    // Create media directory on the utility drive
                    CreateDirectory( "Z:\\media", NULL );

                    // Cache is now cleared
                    m_bUtilityFormatted = TRUE;

                    // Refresh level states
                    m_pLevelLoader->RefreshLevelStates();

                    // Reset min FPS since this op was synchronous
                    m_bResetFPSMinMax = TRUE;
                    m_dResetTime = GetTimeInSeconds();
                }
            }

            // Corrupt cache 
            if( !m_bCorruptCache && !m_bUtilityFormatted )
            {
                if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_Y] )
                {
                    CorruptCache( g_strLevels, NUM_LEVELS );

                    // Cache in now corrupt
                    m_bCorruptCache = TRUE;

                    // Reset min FPS since this op was not synchronous
                    // NOTE: We don't tell the level loader that anything was
                    //       corrupted. (That would be cheating!)

                    // Reset min FPS since this op was synchronous
                    m_bResetFPSMinMax = TRUE;
                    m_dResetTime = GetTimeInSeconds();
                }
            }

            // Change IO type
            if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT ||
                m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
            {
                if( m_IOType == Threaded )
                    m_IOType = Overlapped;
                else
                    m_IOType = Threaded;

                // Re-initialize level loaded
                if( FAILED( InitializeLevelLoader() ) )
                    return E_FAIL;

                // Reset min FPS since this op is synchronous
                m_dResetTime = GetTimeInSeconds();
                m_bResetFPSMinMax = TRUE;

                // Clear the corrupt and clear cache warnings
                m_bUtilityFormatted = FALSE;
                m_bCorruptCache = FALSE;

                // Stream in level
                m_pLevelLoader->AsyncStreamLevel( m_dwCurrentLevel );
            }

            // Advance to next level 
            if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
            {
                m_dwCurrentLevel = (m_dwCurrentLevel + 1) % NUM_LEVELS;
                
                // Clear the corrupt and clear cache messages
                m_bUtilityFormatted = FALSE;
                m_bCorruptCache = FALSE;

                // Stream in level
                m_pLevelLoader->AsyncStreamLevel( m_dwCurrentLevel );
                m_bFirstLoad = FALSE;
            }
        }

       // Pause the music
       if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_START )
       {
           m_bPausedMusic = ! m_bPausedMusic;
           m_WaveStream.Pause( m_bPausedMusic );
       }
    }
        
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the scene
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Render()
{
    // Draw a gradient filled background
    RenderGradientBackground( 0xff202020, 0xff202060 );

    // Set world view projection matrices
    m_pd3dDevice->SetTransform( D3DTS_WORLD,      &m_matWorld );
    m_pd3dDevice->SetTransform( D3DTS_VIEW,       &m_matView );
    m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &m_matProjection );

    // Draw xyz vectors (helps you from getting lost)
    FLOAT fScale = 1.0f;
    D3DXVECTOR3 XYZVertices[4];
    XYZVertices[0] = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );
    XYZVertices[1] = D3DXVECTOR3( 1.0f, 0.0f, 0.0f ) * fScale;
    XYZVertices[2] = D3DXVECTOR3( 0.0f, 1.0f, 0.0f ) * fScale;
    XYZVertices[3] = D3DXVECTOR3( 0.0f, 0.0f, 1.0f ) * fScale;

    WORD XYZIndices[6] = { 0,1,0,2,0,3 };

    m_pd3dDevice->SetRenderState( D3DRS_COLORVERTEX, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_AMBIENT, 0xffffffff );

    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );

    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, D3DZB_TRUE ); 

    m_pd3dDevice->SetPixelShader( NULL );
    m_pd3dDevice->SetVertexShader( D3DFVF_XYZ );

    m_pd3dDevice->SetTexture( 0, NULL );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TFACTOR );

    m_pd3dDevice->SetRenderState( D3DRS_TEXTUREFACTOR, D3DCOLOR_ARGB( 0, 0xff,0,0 ) );
    m_pd3dDevice->DrawIndexedVerticesUP( D3DPT_LINELIST, 2, XYZIndices + 0,
                                         XYZVertices, sizeof(D3DXVECTOR3) );
    m_pd3dDevice->SetRenderState( D3DRS_TEXTUREFACTOR, D3DCOLOR_ARGB( 0,0,0xff,0 ) );
    m_pd3dDevice->DrawIndexedVerticesUP( D3DPT_LINELIST, 2, XYZIndices + 2,
                                         XYZVertices, sizeof(D3DXVECTOR3) );
    m_pd3dDevice->SetRenderState( D3DRS_TEXTUREFACTOR, D3DCOLOR_ARGB( 0,0,0,0xff ) );
    m_pd3dDevice->DrawIndexedVerticesUP( D3DPT_LINELIST, 2, XYZIndices + 4, 
                                         XYZVertices, sizeof(D3DXVECTOR3) );

    // Draw animated center quad
    m_pd3dDevice->SetPixelShader( NULL );
    m_pd3dDevice->SetVertexShader( D3DFVF_XYZ | D3DFVF_TEX1 );

    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE,    D3DZB_TRUE ); 
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE,   D3DCULL_NONE );

    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );

    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );

    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );

    // Set rotation matrix
    D3DXMATRIX Mat;
    D3DXMatrixRotationZ( &Mat, m_fCenterRotTheta );
    m_pd3dDevice->SetTransform( D3DTS_WORLD, &Mat );

    m_pd3dDevice->SetStreamSource( 0, m_pOneQuadVB, sizeof(VERTEX) );

    D3DTexture* pTexture = m_Resources.GetTexture( "JustTurnItOn" );
    m_pd3dDevice->SetTexture( 0, pTexture );
    m_pd3dDevice->DrawPrimitive( D3DPT_QUADLIST, 0, 1 );  

    // Return world to identity
    m_pd3dDevice->SetTransform( D3DTS_WORLD, &m_matWorld );

    // Display level (if it is loaded)
    if( m_pLevelLoader->IsCurrentLoaded() )
    {
        DWORD dwNumResources = m_pLevelLoader->GetNumResourceTags();

        // Vertex buffer is last resource in all levels
        DWORD dwVBOffset = m_pLevelLoader->GetResourceTagOffset(dwNumResources-1);
        D3DVertexBuffer* pBuffer = m_pLevelLoader->GetVertexBuffer( dwVBOffset );
        g_pd3dDevice->SetStreamSource( 0, pBuffer, sizeof(VERTEX) );
        
        // One texture per quad
        for( DWORD i = 0; i < dwNumResources-1; i++ )
        {
            D3DTexture* pTexture = m_pLevelLoader->GetTexture( m_pLevelLoader->GetResourceTagOffset(i) );
            m_pd3dDevice->SetTexture( 0, pTexture );
            g_pd3dDevice->DrawPrimitive( D3DPT_QUADLIST, i*4, 1 );   
        }
   }

    // Draw help
    if( m_bDrawHelp )
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    else // Draw stats
    {
        WCHAR strBuffer[100];

        m_Font.Begin();

        // Show level name
        swprintf( strBuffer, L"Current level: %S", g_strLevels[m_dwCurrentLevel] );
        m_Font.DrawText( 65, 100, 0xff00cc00, strBuffer);

        // Show level size
        swprintf( strBuffer, L"Level size: %.2f MB", FLOAT(m_pLevelLoader->GetCurrentDVDFileSize()) / (1024.0f * 1024.0f) );
        m_Font.DrawText( 65, 125, 0xff00cc00, strBuffer );

        // Show pause for reboot
        if( m_bReboot || m_bRebootToDash )
            m_Font.DrawText( 320, 340, 0xffffff00, L"Waiting for IO completion before soft reboot",
                                                   XBFONT_CENTER_X | XBFONT_CENTER_Y );

        // Show loading
        if( m_pLevelLoader->IsLoading() )
        {
            if( m_pLevelLoader->WasCurrentPrecached() && !m_pLevelLoader->WasCurrentCacheCorrupted() )
                swprintf( strBuffer, L"Loading cached level from hard disk" );
            else
                swprintf( strBuffer, L"Loading level from DVD" );

            m_Font.DrawText( 65, 150, 0xff00cc00, strBuffer );
        }
        // Show load times
        else if( m_pLevelLoader->IsCurrentLoaded() )
        {
           if( m_bFirstLoad )
           {
               if( m_pLevelLoader->WasCurrentPrecached() && !m_pLevelLoader->WasCurrentCacheCorrupted() )
                    swprintf( strBuffer, L"Hard disk load time"
                                         L"(including start up): %.2f seconds",
                                         m_dStartTime + m_pLevelLoader->GetLoadTime() );
               else
                   swprintf( strBuffer, L"DVD load time"
                                        L"(including start up): %.2f seconds",
                                        m_dStartTime + m_pLevelLoader->GetLoadTime() );
           }
           else
           {
               if( m_pLevelLoader->WasCurrentPrecached() && !m_pLevelLoader->WasCurrentCacheCorrupted() )
                    swprintf( strBuffer, L"Hard disk load time: %.2f seconds",
                                         m_pLevelLoader->GetLoadTime() );
               else
                    swprintf( strBuffer, L"DVD load time: %.2f seconds",
                                         m_pLevelLoader->GetLoadTime() );
           }

           m_Font.DrawText( 65, 150, 0xff00cc00, strBuffer );
        }

        // Show caching
        if( m_pLevelLoader->IsCacheing() )
        {
           swprintf( strBuffer, L"Caching level to hard disk" );
           m_Font.DrawText( 65, 175, 0xff00cc00, strBuffer );
        }
        // Show cache times
        else if( m_pLevelLoader->IsIdle() )
        {
            if( !m_pLevelLoader->WasCurrentPrecached() || m_pLevelLoader->WasCurrentCacheCorrupted() )
            {
                swprintf( strBuffer, L"Cache time: %.2f seconds",
                                     m_pLevelLoader->GetCacheTime() );
                m_Font.DrawText( 65, 175, 0xff00cc00, strBuffer );
            }
        }

        // Show cleared cache
        if( m_bUtilityFormatted )
        {
            swprintf( strBuffer, L"Utility Drive Formatted" );
            m_Font.DrawText( 65, 225, 0xff00cc00, strBuffer );
        }
        // Show corrupted cache
        else if( m_bCorruptCache )
        {
            swprintf( strBuffer, L"Cache corrupted" );
            m_Font.DrawText( 65, 225, 0xffffff00, strBuffer );
        }

        // Show corrupted level
        if( m_pLevelLoader->IsCurrentOpen() ) 
        {
            if( m_pLevelLoader->WasCurrentCacheCorrupted() )
            {
                swprintf( strBuffer, L"Cached level was corrupted" );
                m_Font.DrawText( 65, 275, 0xffffff00, strBuffer );
            }
        }

        // Show paused music
        if( m_bPausedMusic ) 
        {
            swprintf( strBuffer, L"Music paused" );
            m_Font.DrawText( 65, 300, 0xff00cc00, strBuffer );
        }

        // Show IO type
        if( m_IOType == Threaded )
            swprintf( strBuffer, L"Using Threaded IO" );
        else
            swprintf( strBuffer, L"Using Overlapped IO" );

        m_Font.DrawText( 65, 375, 0xff00cc00, strBuffer );

        // Show worst FPS
        static DOUBLE dLastTime = 0.0f;
        static DOUBLE dInstFPS  = 0.0f;
        static DOUBLE dWorstFPS = 0.0f;
        DOUBLE dCurrentTime = GetTimeInSeconds();
        dInstFPS  = 1.0 / (dCurrentTime - dLastTime);
        dLastTime = dCurrentTime;

        if( m_bResetFPSMinMax )
        {
            dWorstFPS = DBL_MAX;

            DOUBLE dDelta = GetTimeInSeconds() - m_dResetTime;

            // 1.5 second count down
            if( dDelta > 1.5 )
                m_bResetFPSMinMax = FALSE;

            swprintf( strBuffer, L"Reset %u", UINT(ceil( 3.0 - 2.0*dDelta )) );
        }
        else
        {
            if( dInstFPS < dWorstFPS )
                dWorstFPS = dInstFPS;

            swprintf( strBuffer, L"Single Worst Frame Rate: %.2f fps", dWorstFPS );
        }
        m_Font.DrawText( 65, 400, 0xff00cc00, strBuffer );

        
        // Show title name and average frame rate
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 48, 36, 0xffffffff, L"FastLoad" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 592, 39, 0xffffff00, m_strFrameRate, XBFONT_RIGHT );
        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: CorruptCache()
// Desc: corrupts cached files for testing purposes
//-----------------------------------------------------------------------------
#define CORRUPTION_BUF_SIZE ( 2*1024 )
VOID CXBoxSample::CorruptCache( CHAR** astrLevels, DWORD dwNumLevels )
{
    // Set random seed
    srand( GetTickCount() );

    // Create a buffer full of junk
    BYTE aJunk[CORRUPTION_BUF_SIZE];
    for( DWORD j = 0; j < (CORRUPTION_BUF_SIZE); j++ )
    {
        aJunk[j] = BYTE(rand());
    }

    // Corrupt each cached level
    for( DWORD i = 0; i < dwNumLevels; i++ )
    {
        char strBuffer[MAX_PATH];
        
        // SIG File
        sprintf( strBuffer, "Z:\\media\\%s.sig", astrLevels[i] );

        // Open sig file for reading and writing 
        HANDLE hSigFile = CreateFile( strBuffer, GENERIC_READ | GENERIC_WRITE,
                                      FILE_SHARE_READ | FILE_SHARE_WRITE,
                                      NULL, OPEN_EXISTING, 
                                      FILE_FLAG_SEQUENTIAL_SCAN, NULL );

        // CACHED file
        sprintf( strBuffer, "Z:\\media\\%s.xpr", astrLevels[i] );
        
        // Open cached file for reading and writing 
        HANDLE hHDFile = CreateFile( strBuffer, GENERIC_WRITE | GENERIC_READ, 
                                     FILE_SHARE_READ | FILE_SHARE_WRITE,
                                     NULL, OPEN_EXISTING,
                                     FILE_FLAG_SEQUENTIAL_SCAN, NULL );
        
        // Determine what to corrupt randomly.
        // NOTE: In the unlikely event of
        //       1) the level data is corrupted 
        //       2) the signature and signature header are not corrupted 
        //       3) level data header is not corrupted
        //       4) all cached file sizes are correct
        //       5) the signature exists
        //       the level will not be detected as corrupted even though its
        //       data is  corrupt.  This is the price we pay for not
        //       calculating a signature over the texture memory portion of
        //       the level. If this happens, you can always reset the app and
        //       mount a new fast formated utility partition or simply clear
        //       the cache.
                
        BOOL bCorruptSigData  = (rand() % 3);
        BOOL bCorruptFileData = (rand() % 2) || !bCorruptSigData;

        DWORD dwNumBytesWritten;

        // Corrupt sig
        if( bCorruptSigData && hSigFile != INVALID_HANDLE_VALUE )
        {
            SetFilePointer( hSigFile, 0, NULL, FILE_BEGIN );
            
            WriteFile( hSigFile, aJunk,
                       GetFileSize(hSigFile, NULL) < CORRUPTION_BUF_SIZE ?
                       GetFileSize(hSigFile, NULL) : CORRUPTION_BUF_SIZE,
                       &dwNumBytesWritten, NULL );
        }

        // Corrupt file data
        if( bCorruptFileData && hHDFile != INVALID_HANDLE_VALUE )
        {
            SetFilePointer( hHDFile, 0, NULL, FILE_BEGIN );
            DWORD dwWritePos = 0;
            BOOL  bCorrupt   = TRUE;
            
            // If you un-comment this line, there is a chance that
            // the level video memory data will be corrupted, but the 
            // level system memory data will not be corrupted
            // (CORRUPTION_BUF_SIZE == level system memory size so the
            // system memory data is always corrupted if the first bCorrupt
            // is TRUE).  As explained above, the odds of this happening
            // under normal circumstances (i.e. you are not trying to 
            // corrupt your cache) are small.
            // bCorrupt = (rand() % 2);

            while( dwWritePos  + CORRUPTION_BUF_SIZE < GetFileSize( hHDFile, NULL ) )
            {
                if( bCorrupt )
                {
                    WriteFile( hHDFile, aJunk, CORRUPTION_BUF_SIZE,
                               &dwNumBytesWritten, NULL );
                }
                dwWritePos = SetFilePointer( hHDFile, ((rand()%100)*CORRUPTION_BUF_SIZE),
                                            NULL, FILE_CURRENT );
                bCorrupt = (rand() % 2);
            }
        }

        // Clean up handles
        if( hSigFile != INVALID_HANDLE_VALUE )
            CloseHandle( hSigFile );
        if( hHDFile != INVALID_HANDLE_VALUE )
            CloseHandle( hHDFile );
    }
}




//-----------------------------------------------------------------------------
// Name: PrintQuads()
// Desc: Prints out some precalculated level data
//-----------------------------------------------------------------------------
VOID PrintQuads( DWORD dwNumQuads )
{
    // Open out.txt
    FILE* file = fopen( "t:\\out.txt", "wt" );

    // Create vert array
    DWORD dwNumVerts = dwNumQuads * 4;
    VERTEX* pVerts = new VERTEX[dwNumVerts];

    // Make some "interesting" patterns for quads
    D3DXVECTOR3 vQuad[4];
    const FLOAT fRadiusMult = 1.0f;

    FLOAT fRadius = fRadiusMult * dwNumQuads;
    FLOAT fSize = (2*D3DX_PI * fRadius) / FLOAT(dwNumQuads) / 3.0f;
    
    vQuad[0] = D3DXVECTOR3(-fSize, -fSize, fRadius + 1 );
    vQuad[1] = D3DXVECTOR3(-fSize,  fSize, fRadius + 1 );
    vQuad[2] = D3DXVECTOR3( fSize,  fSize, fRadius + 1 );
    vQuad[3] = D3DXVECTOR3( fSize, -fSize, fRadius + 1 );
    
    for( DWORD i = 0; i < dwNumQuads; i++ )
    {
        FLOAT fAngle = 2*D3DX_PI * i / FLOAT(dwNumQuads);
        D3DXMATRIX mat;
        D3DXMatrixRotationY( &mat, fAngle );
        for( DWORD j = 0; j < 4; j++ )
        {
            D3DXVec3TransformCoord( &pVerts[4*i + j].pos, &vQuad[j], &mat );
        }
        pVerts[4*i + 0].texcoord = D3DXVECTOR2( 0.0f, 1.0f );
        pVerts[4*i + 1].texcoord = D3DXVECTOR2( 0.0f, 0.0f );
        pVerts[4*i + 2].texcoord = D3DXVECTOR2( 1.0f, 0.0f );
        pVerts[4*i + 3].texcoord = D3DXVECTOR2( 1.0f, 1.0f );
    }

    for( i = 0; i < dwNumQuads*4; i++ )
    {
        fprintf( file, "%9.4f %9.4f %9.4f %9.4f %9.4f\n",
                       pVerts[i].pos.x, pVerts[i].pos.y, pVerts[i].pos.z,
                       pVerts[i].texcoord.x, pVerts[i].texcoord.y );
    }
    fprintf( file, "\n\n" );

    vQuad[0] = D3DXVECTOR3(-fSize, -fSize, 2 );
    vQuad[1] = D3DXVECTOR3(-fSize,  fSize, 2 );
    vQuad[2] = D3DXVECTOR3( fSize,  fSize, 2 );
    vQuad[3] = D3DXVECTOR3( fSize, -fSize, 2 );

    D3DXVECTOR3 Start( fSize*dwNumQuads, fSize*dwNumQuads, fSize*dwNumQuads );
    
    for( i = 0; i < dwNumQuads; i++ )
    {
        D3DXVECTOR3 vOffset = Start/2.0f - (Start/FLOAT(dwNumQuads))*FLOAT(i);

        for( DWORD j = 0; j < 4; j++ )
        {
            pVerts[4*i + j].pos = vQuad[j] + vOffset;
        }
        pVerts[4*i + 0].texcoord = D3DXVECTOR2( 0.0f, 1.0f );
        pVerts[4*i + 1].texcoord = D3DXVECTOR2( 0.0f, 0.0f );
        pVerts[4*i + 2].texcoord = D3DXVECTOR2( 1.0f, 0.0f );
        pVerts[4*i + 3].texcoord = D3DXVECTOR2( 1.0f, 1.0f );
    }

    for(i = 0; i < dwNumQuads*4; i++)
    {
        fprintf( file, "%9.4f %9.4f %9.4f %9.4f %9.4f\n",
                       pVerts[i].pos.x, pVerts[i].pos.y, pVerts[i].pos.z,
                       pVerts[i].texcoord.x, pVerts[i].texcoord.y );
    }
    fprintf( file, "\n\n" );
  
    vQuad[0] = D3DXVECTOR3(-fSize, -fSize, 2 );
    vQuad[1] = D3DXVECTOR3(-fSize,  fSize, 2 );
    vQuad[2] = D3DXVECTOR3( fSize,  fSize, 2 );
    vQuad[3] = D3DXVECTOR3( fSize, -fSize, 2 );

    Start = D3DXVECTOR3( 2.3f*fSize*dwNumQuads, 0.0f, 0.0f );
    
    for( i = 0; i < dwNumQuads; i++)
    {
        D3DXVECTOR3 vOffset = Start/2.0f - (Start/FLOAT(dwNumQuads))*FLOAT(i + 0.5);

        for( DWORD j = 0; j < 4; j++ )
        {
            pVerts[4*i + j].pos = vQuad[j] + vOffset;
        }
        pVerts[4*i + 0].texcoord = D3DXVECTOR2( 0.0f, 1.0f );
        pVerts[4*i + 1].texcoord = D3DXVECTOR2( 0.0f, 0.0f );
        pVerts[4*i + 2].texcoord = D3DXVECTOR2( 1.0f, 0.0f );
        pVerts[4*i + 3].texcoord = D3DXVECTOR2( 1.0f, 1.0f );
    }
 
    for( i = 0; i < dwNumQuads*4; i++ )
    {
        fprintf( file, "%9.4f %9.4f %9.4f %9.4f %9.4f\n",
                       pVerts[i].pos.x, pVerts[i].pos.y, pVerts[i].pos.z,
                       pVerts[i].texcoord.x, pVerts[i].texcoord.y );
    }
    fprintf( file, "\n\n" );

    // Close the file
    fclose( file );
 
    // Clean up
    delete[] pVerts;
}
