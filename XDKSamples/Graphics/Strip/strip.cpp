//-----------------------------------------------------------------------------
// File: Strip.cpp
//
// Desc: Sample to show off tri-stripping performance results. This sample
//       creates a mesh, stripifies it, then displays several copies of it
//       along with performance data.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBPerf.h>
#include <XBStrip.h>
#include <XBUtil.h>
#include <XBHelp.h>




//-----------------------------------------------------------------------------
// Help support
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_NormalHelpCallouts[] =
{
    { XBHELP_A_BUTTON,     XBHELP_PLACEMENT_2, L"Toggle\nmesh type" },
    { XBHELP_LEFTSTICK,    XBHELP_PLACEMENT_2, L"Rotate\nthe model" },
    { XBHELP_RIGHTSTICK,   XBHELP_PLACEMENT_1, L"Zoom in/out" },
    { XBHELP_BACK_BUTTON,  XBHELP_PLACEMENT_2, L"Display\nhelp" },
};

#define MAX_NORMAL_HELP_CALLOUTS 4




//-----------------------------------------------------------------------------
// Global variables and definitions
//-----------------------------------------------------------------------------

// Constants definitions
#define COLOR_WHITE                 0xffffffff
#define COLOR_YELLOW                0xffffff00
#define COLOR_CYAN                  0xff00ffff


// Structure declarations
struct MODELVERT
{
    D3DXVECTOR3 p;
    D3DXVECTOR3 n;
    FLOAT       tu, tv;
};


struct MODELDATA
{
    D3DXMATRIX* pMatrix;
    DWORD       dwNumVertices;
    MODELVERT*  pVertices;
    DWORD       dwNumIndices;
    WORD*       pIndices;
};


enum MESHTYPE { MESHTYPE_ORIGINAL, MESHTYPE_TRISTRIPPED };


struct ROBOTSTATS
{
    BOOL        bValid;

    DWORD       dwNumVertices;
    DWORD       dwCacheHits;
    DWORD       dwPagesCrossed;
    DWORD       dwDegenerateTris;
    
    double      fdAvgTriPerSec;
    double      fdAvgTriPerSec2;
    double      fdMaxTriPerSec;
    double      fdMinTriPerSec;
    DWORD       dwAvgCount;
    DWORD       dwTriCount;
    DWORD       dwIndCount;
    DWORD       dwTime;
    DWORD       dwFrames;
};

struct MESHINFO
{
    D3DPRIMITIVETYPE        dwPrimType;   // primitive type

    DWORD                   dwIndexCount; // index count
    WORD*                   pwIndices;    // index list
    LPDIRECT3DINDEXBUFFER8  pIndexBuffer; // dx8 index buffer

    DWORD                   dwNumVertices; // num verts
    LPDIRECT3DVERTEXBUFFER8 pVertexBuffer; // vbs

    DWORD                   dwPrimitiveCount;

    DWORD                   dwDegenerateTris;
    DWORD                   dwCacheHits;
    DWORD                   dwPagesCrossed;
};


// Render a 3 x 4 grid of robots
static const int C_ROBOTS_X = 4;
static const int C_ROBOTS_Y = 3;




// Mesh data from MODELDATA.CPP
extern DWORD     g_cModelData;
extern MODELDATA g_ModelData[];

#define MAX_LIGHTS                  8

// Camera location
D3DXVECTOR3 g_vEye( 10.0f, -20.0f, 9.0f );
D3DXVECTOR3 g_vAt( 0.0f, 0.0f, 0.0f );
D3DXVECTOR3 g_vUp( 0.0f, 0.0f, 1.0f );




//-----------------------------------------------------------------------------
// Name: class CRobotGeometry
// Desc: Class to render geometry for a Robot
//-----------------------------------------------------------------------------
class CRobotGeometry
{
    D3DMATERIAL8           m_mat;          // material

    DWORD                  m_dwNumMeshes;  // count of VBs to draw
    MESHINFO*              m_pMeshes;      // our list of VBs

    LPDIRECT3DTEXTURE8     m_pTexture1;    // texture1
    LPDIRECT3DCUBETEXTURE8 m_pTexture2;    // texture2

    LPDIRECT3DDEVICE8      m_pd3dDevice;   // d3d device

public:
    DWORD                  m_dwFVF;        // Mesh FVF
    DWORD                  m_dwVertexSize; // Vertex size

    HRESULT Init( D3DDevice*, MESHTYPE );
    HRESULT InitVertexBuffers( MESHTYPE );
    VOID    Release();

    HRESULT SetStates();
    HRESULT RestoreStates();
    HRESULT Render( ROBOTSTATS* = NULL );

    CRobotGeometry();
    ~CRobotGeometry() { Release(); }
};




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Application class. The base class provides just about all the
//       functionality we want, so we're just supplying stubs to interface with
//       the non-C++ functions of the app.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    CXBFont         m_Font;
    CXBHelp         m_Help;
    BOOL            m_bDisplayHelp;

    // The robot object
    ROBOTSTATS      m_Stats;
    CRobotGeometry  m_OriginalRobot;     // The original robot geometry
    CRobotGeometry  m_TriStrippedRobot;  // The tri-stripped version of the robot
    
    CRobotGeometry* m_pRobot;            // Which version of the robot to render
    BOOL            m_bUseTriStrippedMesh;

    HRESULT InitLights();
    VOID    DisplayArgs();

public:
    HRESULT Initialize();
    HRESULT FrameMove();
    HRESULT Render();
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
// Desc: Main app constructor
//-----------------------------------------------------------------------------
CXBoxSample::CXBoxSample()
{
    // Enable anti-aliasing and unlimited frame rates
    m_d3dpp.MultiSampleType = D3DMULTISAMPLE_2_SAMPLES;
    m_d3dpp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    // Initialize derived member variables
    m_bDisplayHelp = FALSE;

    ZeroMemory( &m_Stats, sizeof(m_Stats) );
    m_Stats.bValid          = FALSE;
    m_Stats.dwAvgCount      = 0;
    m_Stats.fdAvgTriPerSec  = 0.0;
    m_Stats.fdAvgTriPerSec2 = 0.0;
    m_Stats.fdMaxTriPerSec  = 0.0;
    m_Stats.fdMinTriPerSec  = 1e99;

    m_bUseTriStrippedMesh = TRUE; // Start with tri-strips
    m_pRobot = &m_TriStrippedRobot;
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize all dependencies and states
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    HRESULT hr;

    if( FAILED( hr = m_Font.Create( "Font.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    if( FAILED( hr = m_Help.Create( "Gamepad.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;
    
    // Disable antialiasing by default
    m_pd3dDevice->SetRenderState( D3DRS_MULTISAMPLEANTIALIAS, FALSE );

    // Display initial wait screen
    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET, 0xff0000ff, 1.0f, 0L );
    m_Font.DrawText( 64,  50, COLOR_WHITE, L"Xbox Tri-stripper demo" );
    m_Font.DrawText( 64,  75, COLOR_CYAN,  L"Calculating tri-strips. Please wait..." );
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    // Initialize robot data
    hr = m_OriginalRobot.Init( m_pd3dDevice, MESHTYPE_ORIGINAL );
    if( FAILED(hr) )
        return hr;
    hr = m_TriStrippedRobot.Init( m_pd3dDevice, MESHTYPE_TRISTRIPPED );
    if( FAILED(hr) )
        return hr;
    
    D3DXMATRIX matView;
    D3DXMatrixLookAtLH( &matView, &g_vEye, &g_vAt, &g_vUp );
    m_pd3dDevice->SetTransform( D3DTS_VIEW, &matView );

    D3DXMATRIX matProj;
    D3DXMatrixPerspectiveFovLH( &matProj, D3DX_PI * 100.0f / 360.0f,
                                640.0f  / 480.0f, 1.0f, 1000.0f );
    m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &matProj );

    // Initialize our lights
    InitLights();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: InitLights()
// Desc: Initialize all lights
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::InitLights()
{
    D3DLIGHT8 light;
    ZeroMemory( &light, sizeof(light) );
    light.Type         =  D3DLIGHT_POINT;
    light.Diffuse.r    =  1.0f;
    light.Diffuse.g    =  1.0f;
    light.Diffuse.b    =  1.0f;
    light.Specular.r   =  0.5f;
    light.Specular.g   =  0.5f;
    light.Specular.b   =  0.5f;
    light.Position.x   =  0.0f;
    light.Position.y   =  -10.0f;
    light.Position.z   =  40.0f;
    light.Range        = 10000.0f;
    light.Attenuation0 = 0.0f;
    light.Attenuation1 = 0.02f;
    light.Attenuation2 = 0.0f;
    m_pd3dDevice->SetLight( 0, &light );
    m_pd3dDevice->LightEnable( 0, TRUE );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    // Handle the A button
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_A] ) 
    {
        // Toggle between tri-stripped mesh and the original mesh
        m_bUseTriStrippedMesh = !m_bUseTriStrippedMesh;

        // Make sure we point to the right robot mesh to render
        if( m_bUseTriStrippedMesh )
            m_pRobot = &m_TriStrippedRobot;
        else
            m_pRobot = &m_OriginalRobot;

        // Clear the stats
        m_Stats.bValid          = FALSE;
        m_Stats.dwAvgCount      = 0;
        m_Stats.fdAvgTriPerSec  = 0.0;
        m_Stats.fdAvgTriPerSec2 = 0.0;
        m_Stats.fdMaxTriPerSec  = 0.0;
        m_Stats.fdMinTriPerSec  = 1e99;
    }

    // Handle the BACK button
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_BACK ) 
    {
        // Toggle help
        m_bDisplayHelp = !m_bDisplayHelp;
    }

    // Handle the thumbsticks
    FLOAT fX  = m_DefaultGamepad.fX1 * 3.0f;
    FLOAT fLY = m_DefaultGamepad.fY1 * 3.0f;
    FLOAT fRY = m_DefaultGamepad.fY2 * 3.0f;

    if( fX || fLY || fRY )
    {
        // In / out
        FLOAT fLength = D3DXVec3LengthSq( &g_vEye );
        D3DXVECTOR3 vEyeNorm = g_vEye / fLength;
        g_vEye += vEyeNorm * fRY * .75f;

        // Up / down
        g_vEye.z += fLY;

        // Left / right
        FLOAT fCos = cosf( fX * D3DX_PI / 180.0f );
        FLOAT fSin = sinf( fX * D3DX_PI / 180.0f );
        g_vEye.x = g_vEye.x * fCos - g_vEye.y * fSin;
        g_vEye.y = g_vEye.x * fSin + g_vEye.y * fCos;

        D3DXMATRIX matView;
        D3DXMatrixLookAtLH( &matView, &g_vEye, &g_vAt, &g_vUp );
        m_pd3dDevice->SetTransform( D3DTS_VIEW, &matView );
    }

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
    static WCHAR s_strStats[MAX_PATH] = L"Running...";
    DWORD       dwStart = GetTickCount();
    DWORD       dwNow   = dwStart;

    // Clear the viewport
    m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0x00000000,
                         1.0f, 0L );

    // Render the robot
    m_pRobot->SetStates();
    
    // Clear the geometry stats
    m_Stats.dwTriCount       = 0;
    m_Stats.dwIndCount       = 0;
    m_Stats.dwNumVertices    = 0;
    m_Stats.dwCacheHits      = 0;
    m_Stats.dwPagesCrossed   = 0;
    m_Stats.dwDegenerateTris = 0;

    // Draw robot a bunch of times
    for( DWORD dwX = 0; dwX < C_ROBOTS_X; dwX++ )
    {
        for( DWORD dwY = 0; dwY < C_ROBOTS_Y; dwY++ )
        {
            D3DXMATRIX matWorld;
            D3DXMatrixTranslation( &matWorld, (dwX - C_ROBOTS_X / 2.0f) * 3.5f, 
                                              0.0f, 
                                              (dwY - C_ROBOTS_Y / 2.0f) * 4.0f );
            m_pd3dDevice->SetTransform( D3DTS_WORLD, &matWorld );

            m_pRobot->Render( &m_Stats );
        }
    }

    m_pRobot->RestoreStates();

    dwNow = GetTickCount();

    // Draw the help or main screen text output
    if( m_bDisplayHelp )
    {
        m_Help.Render( &m_Font, g_NormalHelpCallouts, MAX_NORMAL_HELP_CALLOUTS );
    }
    else
    {
        // Draw stats
        m_Font.DrawText( 64.0f, 75.0f, COLOR_CYAN, s_strStats );

        // Display args
        DisplayArgs();

        // Draw title and frame rate
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 48, 36, 0xffffffff,  L"Strip" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 592, 38, 0xffffff00, m_strFrameRate, XBFONT_RIGHT );
    }

    // Update the timing stats
    FLOAT fElapsedTime = 0.001f * (dwNow-dwStart);
    FLOAT tps = 1e-6f * (m_Stats.dwTriCount)   / fElapsedTime;
    FLOAT sps = 1e-6f * (m_Stats.dwIndCount)   / fElapsedTime;
    swprintf( s_strStats, L"%6.1f MTri/s,   %6.1f MVerts/s", tps, sps );

    m_Stats.dwAvgCount++;
    m_Stats.fdAvgTriPerSec  += tps;
    m_Stats.fdAvgTriPerSec2 += 1e-6f * (m_Stats.dwTriCount - m_Stats.dwDegenerateTris) / fElapsedTime;
    m_Stats.fdMaxTriPerSec   = max(tps, m_Stats.fdMaxTriPerSec);
    m_Stats.fdMinTriPerSec   = min(tps, m_Stats.fdMinTriPerSec);
    m_Stats.dwTime           = dwNow - dwStart;
    m_Stats.dwFrames         = 1;

    m_Stats.bValid           = TRUE;

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: DisplayArgs()
// Desc: Display the various stats and options.
//-----------------------------------------------------------------------------
VOID CXBoxSample::DisplayArgs()
{
    WCHAR str[128];
    FLOAT fX[4]   = { 64.0f, 300.0f, 320.0f, 576.0f };
    FLOAT fY      = 120.0f;
    FLOAT fHeight = (FLOAT)m_Font.GetFontHeight();

    // Draw left-hand column
    fY = 120.0f;

    static const WCHAR* rgMeshType[2] = { L"Original", L"Tri-stripped" };
    swprintf( str, L"%s", rgMeshType[m_bUseTriStrippedMesh] );
    m_Font.DrawText( fX[0], fY, COLOR_WHITE, L"Mesh type:" );
    m_Font.DrawText( fX[1], fY, COLOR_YELLOW, str, XBFONT_RIGHT );
    fY += fHeight;

    swprintf( str, L"%d", m_Stats.dwTriCount );
    m_Font.DrawText( fX[0], fY, COLOR_WHITE, L"# triangles:" );
    if( m_Stats.bValid )
        m_Font.DrawText( fX[1], fY, COLOR_YELLOW, str, XBFONT_RIGHT );
    fY += fHeight;

    swprintf( str, L"%d", m_Stats.dwDegenerateTris );
    m_Font.DrawText( fX[0], fY,  COLOR_WHITE, L"# degenerate:" );
    if( m_Stats.bValid )
        m_Font.DrawText( fX[1], fY, COLOR_YELLOW, str, XBFONT_RIGHT );
    fY += fHeight;

    swprintf( str, L"%d", m_Stats.dwNumVertices );
    m_Font.DrawText( fX[0], fY,  COLOR_WHITE, L"# vertices:" );
    if( m_Stats.bValid )
        m_Font.DrawText( fX[1], fY, COLOR_YELLOW, str, XBFONT_RIGHT );
    fY += fHeight;

    swprintf( str, L"%d", m_Stats.dwIndCount );
    m_Font.DrawText( fX[0], fY,  COLOR_WHITE, L"# indices:" );
    if( m_Stats.bValid )
        m_Font.DrawText( fX[1], fY, COLOR_YELLOW, str, XBFONT_RIGHT );
    fY += fHeight;

    // Draw right-hand column
    fY = 120.0f;

    swprintf( str, L"%8.3f", m_Stats.fdMaxTriPerSec );
    m_Font.DrawText( fX[2], fY,  COLOR_WHITE, L"Max tri/s" );
    if( m_Stats.bValid )
        m_Font.DrawText( fX[3], fY, COLOR_YELLOW, str, XBFONT_RIGHT );
    fY += fHeight;

    swprintf( str, L"%8.3f", m_Stats.fdMinTriPerSec );
    m_Font.DrawText( fX[2], fY,  COLOR_WHITE, L"Min tri/s" );
    if( m_Stats.bValid )
        m_Font.DrawText( fX[3], fY, COLOR_YELLOW, str, XBFONT_RIGHT );
    fY += fHeight;

    swprintf( str, L"%8.3f", m_Stats.fdAvgTriPerSec / m_Stats.dwAvgCount );
    m_Font.DrawText( fX[2], fY,  COLOR_WHITE, L"Avg tri/s" );
    if( m_Stats.bValid )
        m_Font.DrawText( fX[3], fY, COLOR_YELLOW, str, XBFONT_RIGHT );
    fY += fHeight;

    swprintf( str, L"%8.3f", m_Stats.fdAvgTriPerSec2 / m_Stats.dwAvgCount );
    m_Font.DrawText( fX[2], fY,  COLOR_WHITE, L"Real avg tri/s:" );
    if( m_Stats.bValid )
        m_Font.DrawText( fX[3], fY, COLOR_YELLOW, str, XBFONT_RIGHT );
    fY += fHeight;

    swprintf( str, L"%d", m_Stats.dwCacheHits );
    m_Font.DrawText( fX[2], fY,  COLOR_WHITE, L"GPU cache hits:" );
    if( m_Stats.bValid )
        m_Font.DrawText( fX[3], fY, COLOR_YELLOW, str, XBFONT_RIGHT );
    fY += fHeight;

    DWORD vbpages = (m_Stats.dwNumVertices + m_Stats.dwNumVertices * m_pRobot->m_dwVertexSize) / (1024 * 4);
    swprintf( str, L"%d", vbpages );
    m_Font.DrawText( fX[2], fY,  COLOR_WHITE, L"VB pages:" );
    if( m_Stats.bValid )
        m_Font.DrawText( fX[3], fY, COLOR_YELLOW, str, XBFONT_RIGHT );
    fY += fHeight;

    swprintf( str, L"%d", m_Stats.dwPagesCrossed );
    m_Font.DrawText( fX[2], fY,  COLOR_WHITE, L"Pages crossed:" );
    if( m_Stats.bValid )
        m_Font.DrawText( fX[3], fY, COLOR_YELLOW, str, XBFONT_RIGHT );
    fY += fHeight;

    swprintf( str, L"%d ms", m_Stats.dwTime );
    m_Font.DrawText( fX[2], fY,  COLOR_WHITE, L"Time/frame:" );
    if( m_Stats.bValid )
        m_Font.DrawText( fX[3], fY, COLOR_YELLOW, str, XBFONT_RIGHT );
    fY += fHeight;
}




//-----------------------------------------------------------------------------
// Name: CRobotGeometry()
// Desc: CRobotGeometry constructor
//-----------------------------------------------------------------------------
CRobotGeometry::CRobotGeometry()
{
    m_dwNumMeshes = 0;
    m_pMeshes     = NULL;

    m_pTexture1   = NULL;
    m_pTexture2   = NULL;
    m_pd3dDevice  = NULL;
}




//-----------------------------------------------------------------------------
// Name: ~CRobotGeometry()
// Desc: CRobotGeometry destructor
//-----------------------------------------------------------------------------
VOID CRobotGeometry::Release()
{
    for( DWORD i = 0; i < m_dwNumMeshes; i++ )
    {
        SAFE_RELEASE( m_pMeshes[i].pIndexBuffer );
        SAFE_RELEASE( m_pMeshes[i].pVertexBuffer );
    }

    SAFE_DELETE_ARRAY( m_pMeshes );
    m_dwNumMeshes = 0;

    SAFE_RELEASE( m_pTexture1 );
    SAFE_RELEASE( m_pTexture2 );
}




//-----------------------------------------------------------------------------
// Name: Init()
// Desc: Initialize robot dependencies
//-----------------------------------------------------------------------------
HRESULT CRobotGeometry::Init( LPDIRECT3DDEVICE8 pd3dDevice, MESHTYPE meshtype )
{
    HRESULT hr;

    if( pd3dDevice == NULL )
        return E_INVALIDARG;

    m_pd3dDevice = pd3dDevice;

    // Create our vertex buffers and indices
    InitVertexBuffers( meshtype );

    // Set material
    ZeroMemory( &m_mat, sizeof(D3DMATERIAL8) );
    m_mat.Diffuse.r  =  1.0f;
    m_mat.Diffuse.g  =  1.0f;
    m_mat.Diffuse.b  =  1.0f;
    m_mat.Diffuse.a  =  1.0f;
    m_mat.Ambient.r  =  1.0f;
    m_mat.Ambient.g  =  1.0f;
    m_mat.Ambient.b  =  1.0f;
    m_mat.Ambient.a  =  1.0f;
    m_mat.Specular.r =  0.8f;
    m_mat.Specular.g =  0.8f;
    m_mat.Specular.b =  0.8f;
    m_mat.Specular.a =  0.4f;
    m_mat.Power      = 16.0f;

    // Load the texture
    hr = D3DXCreateTextureFromFileA( pd3dDevice, "D:\\Media\\Textures\\Strip_T.dds", 
                                     &m_pTexture1 );
    if( FAILED(hr) )
        return hr;

    // Load the environment map textures
    hr = D3DXCreateCubeTextureFromFileA( pd3dDevice, "D:\\Media\\Textures\\Strip_C.dds", 
                                         &m_pTexture2 );
    if( FAILED(hr) )
        return hr;

    OUTPUT_DEBUG_STRING( "Strip: Successfull CRobotGeometry::Init()\n" );
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the robot mesh 12 times and calc stats
//-----------------------------------------------------------------------------
HRESULT CRobotGeometry::Render( ROBOTSTATS* pStats )
{
    D3DXMATRIX matWorld;
    m_pd3dDevice->GetTransform( D3DTS_WORLD, &matWorld );
    
    // Render each part
    for( DWORD iVB = 0; iVB < m_dwNumMeshes; iVB++ )
    {
        // Set our world transform
        D3DXMATRIX matObject(*g_ModelData[iVB].pMatrix);
        D3DXMatrixMultiply( &matObject, &matObject, &matWorld );
        m_pd3dDevice->SetTransform( D3DTS_WORLD, &matObject );

        // Draw robot
        m_pd3dDevice->SetVertexShader( m_dwFVF );
        m_pd3dDevice->SetStreamSource( 0, m_pMeshes[iVB].pVertexBuffer, m_dwVertexSize );
        m_pd3dDevice->SetIndices( m_pMeshes[iVB].pIndexBuffer, 0 );
        m_pd3dDevice->DrawIndexedPrimitive( m_pMeshes[iVB].dwPrimType, 
                                            0, m_pMeshes[iVB].dwNumVertices, 
                                            0, m_pMeshes[iVB].dwPrimitiveCount );

        // Record stats
        pStats->dwTriCount       += m_pMeshes[iVB].dwPrimitiveCount;
        pStats->dwIndCount       += m_pMeshes[iVB].dwIndexCount;
        pStats->dwNumVertices    += m_pMeshes[iVB].dwNumVertices;
        pStats->dwCacheHits      += m_pMeshes[iVB].dwCacheHits;
        pStats->dwPagesCrossed   += m_pMeshes[iVB].dwPagesCrossed;
        pStats->dwDegenerateTris += m_pMeshes[iVB].dwDegenerateTris;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SetStates()
// Desc: Set states that will be used to render the scene
//-----------------------------------------------------------------------------
HRESULT CRobotGeometry::SetStates()
{
    m_pd3dDevice->SetMaterial( &m_mat );

    m_pd3dDevice->SetRenderState( D3DRS_LIGHTING, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_AMBIENT, D3DCOLOR_COLORVALUE(.5, .5, .5, 1.0f) );
    m_pd3dDevice->SetRenderState( D3DRS_NORMALIZENORMALS, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );

    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SPECULARENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_LOCALVIEWER, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_MULTISAMPLEANTIALIAS, TRUE );

    for( DWORD dwStage = 0; dwStage < 2; dwStage++ )
    {
        m_pd3dDevice->SetTextureStageState( dwStage, D3DTSS_MAXANISOTROPY, 3 );
        m_pd3dDevice->SetTextureStageState( dwStage, D3DTSS_MINFILTER, D3DTEXF_ANISOTROPIC );
        m_pd3dDevice->SetTextureStageState( dwStage, D3DTSS_MAGFILTER, D3DTEXF_ANISOTROPIC );
        m_pd3dDevice->SetTextureStageState( dwStage, D3DTSS_MIPFILTER, D3DTEXF_LINEAR );
    }

    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
    m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );

    m_pd3dDevice->SetTexture( 0, m_pTexture1);

    // Modulate texture 0 with diffuse
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );

    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );

    // Transform incoming camera space reflection vectors to world space
    D3DXMATRIX matTex;

    m_pd3dDevice->GetTransform(D3DTS_VIEW, &matTex);
    matTex.m[3][0] = 0.0f;
    matTex.m[3][1] = 0.0f;
    matTex.m[3][2] = 0.0f;

    D3DXMatrixInverse( &matTex, NULL, &matTex );
    m_pd3dDevice->SetTransform( D3DTS_TEXTURE1, &matTex );

    m_pd3dDevice->SetTexture( 1, m_pTexture2 );

    // Modulate the color of the second argument, using the alpha of the first argument;
    // then add the result to arg one.
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_MODULATEALPHA_ADDCOLOR );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLORARG1, D3DTA_CURRENT );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLORARG2, D3DTA_TEXTURE );

    // Generate camera-space reflection vectors as tex coords
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_TEXCOORDINDEX,
                                           D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR | 1 );

    // Setup the texture transform pipeline for 3d tex coords
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_TEXTURETRANSFORMFLAGS,
                                           D3DTTFF_COUNT3);

    m_pd3dDevice->SetTextureStageState( 2, D3DTSS_COLOROP, D3DTOP_DISABLE );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: RestoreStates()
// Desc: Restore all used states
//-----------------------------------------------------------------------------
HRESULT CRobotGeometry::RestoreStates()
{
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_DISABLE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_DISABLE );

    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );

    D3DXMATRIX matIdentity;
    D3DXMatrixIdentity( &matIdentity );
    m_pd3dDevice->SetTransform( D3DTS_TEXTURE1, &matIdentity );

    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_TEXCOORDINDEX, 0 );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE );

    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );

    for( DWORD dwStage = 0; dwStage < 2; dwStage++ )
    {
        m_pd3dDevice->SetTextureStageState( dwStage, D3DTSS_MAXANISOTROPY, 1 );
        m_pd3dDevice->SetTextureStageState( dwStage, D3DTSS_MINFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetTextureStageState( dwStage, D3DTSS_MAGFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetTextureStageState( dwStage, D3DTSS_MIPFILTER, D3DTEXF_NONE );
    }

    m_pd3dDevice->SetRenderState( D3DRS_AMBIENT,          0x00000000 );
    m_pd3dDevice->SetRenderState( D3DRS_NORMALIZENORMALS, FALSE);
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE,          D3DZB_TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SPECULARENABLE,   FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_LOCALVIEWER,      TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_MULTISAMPLEANTIALIAS, FALSE );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: InitVertexBuffers()
// Desc: Create vertex and index buffers based on the mesh type being used
//-----------------------------------------------------------------------------
HRESULT CRobotGeometry::InitVertexBuffers( MESHTYPE meshtype )
{
    OUTPUT_DEBUG_STRING( "Strip: Begin initializing vertex buffers\n" );

    // Release any previously allocated meshes
    for( DWORD i = 0; i < m_dwNumMeshes; i++ )
    {
        SAFE_RELEASE( m_pMeshes[i].pIndexBuffer );
        SAFE_RELEASE( m_pMeshes[i].pVertexBuffer );
    }
    SAFE_DELETE_ARRAY( m_pMeshes );
    m_dwNumMeshes = 0;

    // FVF for creating vertex buffers
    m_dwFVF        = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1;
    m_dwVertexSize = D3DXGetFVFVertexSize(m_dwFVF);

    // Initialize new mesh array
    m_dwNumMeshes = g_cModelData;
    m_pMeshes     = new MESHINFO[m_dwNumMeshes];
    ZeroMemory( m_pMeshes, m_dwNumMeshes * sizeof(MESHINFO) );

    for( i = 0; i < m_dwNumMeshes; i++ )
    {
        MODELDATA* pModelData = &g_ModelData[i];
        MESHINFO*  pMesh      = &m_pMeshes[i];

        if( meshtype == MESHTYPE_TRISTRIPPED )
        {
            DWORD dwNumVertices = pModelData->dwNumVertices;
            DWORD dwStrippedIndexCount;   // Tristrip count
            WORD* pwStrippedIndices;      // Tristrip indices
            WORD* pwVertexPermutation;    // Array for sorting
            
            // Run the tri-list through our tri-stripper
            Stripify( pModelData->dwNumIndices / 3, pModelData->pIndices,
                      &dwStrippedIndexCount, &pwStrippedIndices );

            // Sort the vertices...
            ComputeVertexPermutation( dwStrippedIndexCount, pwStrippedIndices,
                                      dwNumVertices, &pwVertexPermutation );

            // Create a vertex buffer
            m_pd3dDevice->CreateVertexBuffer( dwNumVertices * m_dwVertexSize, 
                                              D3DUSAGE_WRITEONLY, m_dwFVF, 
                                              D3DPOOL_DEFAULT, &pMesh->pVertexBuffer );

            // Lock and fill the vertex buffer, remapping vertices through the
            // vertex permutation array.
            MODELVERT* pVertices;
            pMesh->pVertexBuffer->Lock( 0, 0, (BYTE**)&pVertices, 0 );
            for( DWORD i = 0; i < dwNumVertices; i++ )
                pVertices[i] = pModelData->pVertices[pwVertexPermutation[i]];
            pMesh->pVertexBuffer->Unlock();

            // Free the array allocated by the ComputeVertexPermutation() call
            SAFE_DELETE_ARRAY( pwVertexPermutation );

            // Create an index buffer for using DrawIndexedPrimitive.
            m_pd3dDevice->CreateIndexBuffer( dwStrippedIndexCount * sizeof(WORD),
                                             D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, 
                                             D3DPOOL_DEFAULT, &pMesh->pIndexBuffer );

            // Lock and fill the index buffer
            WORD* pIndices;
            pMesh->pIndexBuffer->Lock( 0, 0, (BYTE**)&pIndices, 0 );
            memcpy( pIndices, pwStrippedIndices, dwStrippedIndexCount * sizeof(WORD) );
            pMesh->pIndexBuffer->Unlock();

            // Free the array allocated by the Stripify() call
            SAFE_DELETE_ARRAY( pwStrippedIndices );

            // Save info for rendering the mesh
            pMesh->dwNumVertices    = pModelData->dwNumVertices;
            pMesh->pwIndices        = pIndices;
            pMesh->dwPrimType       = D3DPT_TRIANGLESTRIP;
            pMesh->dwPrimitiveCount = dwStrippedIndexCount - 2;
            pMesh->dwIndexCount     = dwStrippedIndexCount;
        }
        else // original mesh
        {
            // Create a vertex buffer
            m_pd3dDevice->CreateVertexBuffer( pModelData->dwNumVertices * m_dwVertexSize, 
                                              D3DUSAGE_WRITEONLY, m_dwFVF, 
                                              D3DPOOL_DEFAULT, &pMesh->pVertexBuffer );

            // Lock and fill the vertex buffer
            MODELVERT* pVertices;
            pMesh->pVertexBuffer->Lock( 0, 0, (BYTE**)&pVertices, 0 );
            memcpy( pVertices, pModelData->pVertices, pModelData->dwNumVertices * m_dwVertexSize );
            pMesh->pVertexBuffer->Unlock();

            // Create an index buffer for using DrawIndexedPrimitive.
            m_pd3dDevice->CreateIndexBuffer( pModelData->dwNumIndices * sizeof(WORD),
                                             D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, 
                                             D3DPOOL_DEFAULT, &pMesh->pIndexBuffer );

            // Lock and fill the index buffer
            WORD* pIndices;
            pMesh->pIndexBuffer->Lock( 0, 0, (BYTE**)&pIndices, 0 );
            memcpy( pIndices, pModelData->pIndices, pModelData->dwNumIndices*sizeof(WORD) );
            pMesh->pIndexBuffer->Unlock();

            pMesh->dwNumVertices    = pModelData->dwNumVertices;
            pMesh->pwIndices        = pIndices;
            pMesh->dwPrimType       = D3DPT_TRIANGLELIST;
            pMesh->dwPrimitiveCount = pModelData->dwNumIndices / 3;
            pMesh->dwIndexCount     = pModelData->dwNumIndices;
        }

        // Figure out how many degenerate triangles and cache hits we've got
        XBPerf_CalcCacheHits( pMesh->dwPrimType, m_dwVertexSize,
                              pMesh->pwIndices, pMesh->dwIndexCount,
                              &pMesh->dwDegenerateTris,
                              &pMesh->dwCacheHits,
                              &pMesh->dwPagesCrossed );
    }

    OUTPUT_DEBUG_STRING( "Strip: Finished initializing vertex buffers\n" );
    return S_OK;
}


