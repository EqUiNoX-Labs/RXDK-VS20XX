//-----------------------------------------------------------------------------
// File: Waterapp.cpp
//
// Desc: Inplements the main sample application calss.
//
//       The workflow of the water application is:
//       1) Initialize
//          Create device, buffers; Load all meshes, textures, shaders, effects 
//          and other resource into memory.
//       2) FrameMove
//          Get gamepad input and change camera's position and direction. If the 
//          camera changed, update the reflection and refraction texture by 
//          rendering the non-water scene (static models and sky) into atexture.
//          Change the bumpmap for water and its related matrices.
//       3) Render
//          Render the sky(CSky), static models( managed by CModelList ) and
//          water(CWater).
//       
//
// Hist: 11.14.00 - Created
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "waterdefs.h"
#include "waterapp.h"
#include "nonewater.h"
#include "resman.h"
#include "refraction.h"
#include "water.h"




const CHAR*  c_strHotspotFileName = "hotspots.dat";

//The sun direction
D3DXVECTOR4  g_SunLightDir( -.30f,
                           0.56f,
                           0.77f,
                           0.3f );//The last value is global ambient
CXBoxSample* g_pApp;
 



//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_BACK_BUTTON,  XBHELP_PLACEMENT_1, L"Display help" },
    { XBHELP_A_BUTTON,     XBHELP_PLACEMENT_2, L"Switch\nHotspots" },
    { XBHELP_WHITE_BUTTON, XBHELP_PLACEMENT_1, L"Toggle Info" },
    { XBHELP_BLACK_BUTTON, XBHELP_PLACEMENT_2, L"Toggle\nWireframe" },
    { XBHELP_LEFT_BUTTON,  XBHELP_PLACEMENT_1, L"Move Down" },
    { XBHELP_RIGHT_BUTTON, XBHELP_PLACEMENT_1, L"Move Up" },
    { XBHELP_LEFTSTICK,    XBHELP_PLACEMENT_1, L"Move" },
    { XBHELP_RIGHTSTICK,   XBHELP_PLACEMENT_1, L"Rotate" },
};

#define NUM_HELP_CALLOUTS 8




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
// Desc: Application constructor. Sets attributes for the app.
//-----------------------------------------------------------------------------
CXBoxSample::CXBoxSample()
            :CXBApplication()
{
    // Allow unlimited frame rate
    m_d3dpp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    g_pApp        = this;
    m_bShowHelp   = FALSE;
    m_bShowInfo   = TRUE;
    m_bWireFrame  = FALSE;
    
    m_pResMan     = NULL;
    m_pNonWater   = NULL;
    m_pWater      = NULL;
    m_pReflection = NULL;
    m_pRefraction = NULL;

    m_vViewPos            = D3DXVECTOR3( 0, 1.9f, 0 );
    m_vViewDir            = D3DXVECTOR3( -0.4f, 0, -0.94f );
    m_vViewLeftDir        = D3DXVECTOR3( 0, 0, 0 );
    m_nNumHotspots        = 0;
    m_pCurrentCullFrustum = NULL;
    m_fMaxLookDownAngle   = 0.8f;
    m_fFieldOfView        = D3DX_PI / 3;
    
}




//-----------------------------------------------------------------------------
// Name: ~CXBoxSample
// Desc: Destructor
//-----------------------------------------------------------------------------
CXBoxSample::~CXBoxSample()
{
}




//-----------------------------------------------------------------------------
// Name: Create
// Desc: Override the default create method to set the global 'g_pd3dDevice'
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Create()
{
    HRESULT hr   = CXBApplication::Create();
    assert( m_pd3dDevice );
    g_pd3dDevice = m_pd3dDevice;

    return hr;
}




//-----------------------------------------------------------------------------
// Name: Initialize
// Desc: This creates all objects include the water, non-water and others.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    HRESULT hr;

    // Create font
    if( FAILED( hr = m_Font.Create( "Font.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create help
    if( FAILED( m_Help.Create( "Gamepad.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;    

    FILE* fp;
    CHAR strFile[c_nMaxPathLength];

    // Load hotspots file
    XBUtil_FindMediaFile( strFile,c_strHotspotFileName );
    fp = fopen( strFile, "rb" );
    if( fp )
    {
        m_nNumHotspots= fread( m_rgHotSpots, 
                               sizeof(HOTSpots), c_nMaxNumHotspots,
                               fp );
        fclose( fp );
    }
    else
    {
        return E_FAIL;
    }

    //Get project matrix
    D3DDISPLAYMODE DisplayMode;
    g_pd3dDevice->GetDisplayMode( &DisplayMode );
    m_fAspect = ( DisplayMode.Width / (float)DisplayMode.Height );
    D3DXMatrixPerspectiveFovLH( &m_matProj, 
        m_fFieldOfView, 
        m_fAspect,
        c_fNearPlane,
        c_fFarPlane );

    //Init objects
    m_pResMan = new CResourceManager();
    assert( m_pResMan );
    if( FAILED( hr = m_pResMan->Initialize() ) )
        return hr;

    m_pNonWater = new CNonWater(); 
    assert( m_pNonWater );
    if( FAILED( hr = m_pNonWater->Initialize() ) )
        return hr;

    m_pReflection = new CReflection();
    assert( m_pReflection );
    m_pReflection->Initialize();

    m_pRefraction = new CRefraction();
    assert( m_pRefraction );
    m_pRefraction->Initialize();

    m_pWater = new CWater();
    assert( m_pWater );
    if( FAILED( hr = m_pWater->Initialize() ) )
        return hr;

    GoToHotspot( 0 );

    return hr;
}




//-----------------------------------------------------------------------------
// Name: FrameMove
// Desc: Called once per frame, the call is the entry point for animating
//        the scene.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    D3DVIEWPORT8 vp;

    CameraMove();

    if( !m_bShowHelp )
    {
        // The render to surface operation changes the viewport
        // because the size of surface is not (640,480)
        // So we need to save the viewport before it.
        m_pd3dDevice->GetViewport( &vp );
    
        SetCullFrustumObject( m_pRefraction );
        m_pRefraction->FrameMoveAndUpdateTexture(); 
        SetCullFrustumObject( NULL );
    
        SetCullFrustumObject( m_pReflection );
        m_pReflection->FrameMoveAndUpdateTexture();
        SetCullFrustumObject( NULL );
        
        m_pd3dDevice->SetViewport( &vp );
    
        m_pWater->FrameMove(); 
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: CameraMove
// Desc: Called by FrameMove to change the camera position when user presses
//        the Gamepad
//-----------------------------------------------------------------------------
void CXBoxSample::CameraMove()
{
    static INT nCurHotSpot = 1;
    static BOOL bFirstFrame = TRUE;
    D3DXVECTOR3 vUp( 0, 1.0f, 0 );

    m_bCameraChanged = FALSE;
    if( bFirstFrame )
    {
        bFirstFrame = FALSE;
        m_bCameraChanged = TRUE;
    }

    // Gamepad Input
    XBGAMEPAD* pPad = &m_DefaultGamepad;

    // Show/hide help
    if ( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_BACK )
    {
        m_bShowHelp = !m_bShowHelp;
        m_bCameraChanged = TRUE;
    }

    // Goto next hot spot
    if ( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_A] )
        nCurHotSpot = GoToHotspot ( nCurHotSpot ) + 1;

    // Show/hide information
    if ( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_WHITE] )
        m_bShowInfo = (m_bShowInfo) ? FALSE: TRUE;

    // Switch between show wireframe and fill solid
    if ( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_BLACK] )
    {
        m_bWireFrame = (m_bWireFrame) ? FALSE: TRUE;
        m_bCameraChanged = TRUE;                        //  refresh textures
        g_pd3dDevice->SetRenderState( D3DRS_FILLMODE, 
            m_bWireFrame? D3DFILL_WIREFRAME : D3DFILL_SOLID );
    }

    FLOAT speed = 0.5f;

    // Move left or right 
    if( pPad->fX1 != 0.0f )
    {
        m_vViewPos -= m_vViewLeftDir * speed * pPad->fX1 * 0.1f;
        m_bCameraChanged = TRUE;
    }

    // Move up or down 
    if( pPad->fY1 != 0.0f  )
    {
        m_vViewPos += m_vViewDir * speed * pPad->fY1 * 0.1f;
        m_bCameraChanged = TRUE;
    }

    // Move Up
    if( pPad->bAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER] )
    {
        m_vViewPos += vUp * 0.1f * 
            pPad->bAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER] / 512;
        m_bCameraChanged = TRUE;
    }

    // Move Down
    if( pPad->bAnalogButtons[XINPUT_GAMEPAD_LEFT_TRIGGER] )
    {
        m_vViewPos -= vUp  * 0.1f *
            pPad->bAnalogButtons[XINPUT_GAMEPAD_LEFT_TRIGGER] / 512;
        m_bCameraChanged = TRUE;
    }

    if( pPad->fY2 != 0.0f || pPad->fX2 != 0.0f )
    {
        D3DXMATRIX mat;
        D3DXMatrixRotationAxis( &mat, &m_vViewLeftDir, pPad->fY2 / 80.0f );
        D3DXVec3TransformNormal( &m_vViewDir, &m_vViewDir, &mat );
        D3DXMatrixRotationY( &mat, pPad->fX2 / 80.0f );

        if( m_vViewDir.y > m_fMaxLookDownAngle )
        {
            m_vViewDir.y = m_fMaxLookDownAngle;
        }
        else 
        {
            if( m_vViewDir.y < -m_fMaxLookDownAngle )
                m_vViewDir.y = -m_fMaxLookDownAngle;
        }

        D3DXVec3TransformNormal( &m_vViewDir, &m_vViewDir, &mat );
        m_bCameraChanged = TRUE;
    } 

    // Update vectors and matrices
    D3DXVec3Cross( &m_vViewLeftDir, &m_vViewDir, &vUp );
    D3DXVec3Normalize( &m_vViewLeftDir, &m_vViewLeftDir );

    // No dive into water 
    if( m_vViewPos.y < 0.5f ) 
        m_vViewPos.y = 0.5f;

    D3DXVec3Normalize( &m_vViewDir, &m_vViewDir );
    D3DXVECTOR3 vAt = m_vViewPos + m_vViewDir;

    D3DXMatrixLookAtLH( &m_matView, &m_vViewPos, &vAt, &vUp );
    D3DXMatrixMultiply( &m_matViewProj, &m_matView, &m_matProj );
}




//-----------------------------------------------------------------------------
// Name: GoToHotspot
// Desc: Change the camera position to a hotspot.
//-----------------------------------------------------------------------------
INT CXBoxSample::GoToHotspot( INT nID )
{
    nID %= m_nNumHotspots;
    HOTSpots & hs = m_rgHotSpots[nID];
    m_vViewPos = hs.pos;
    m_vViewDir = hs.dir;
    m_bCameraChanged = TRUE;
    return nID;
}




//-----------------------------------------------------------------------------
// Name: SetCullFrustumObject
// Desc: Set Current CullFrustum Object
//-----------------------------------------------------------------------------
void CXBoxSample::SetCullFrustumObject( ICullFrustum*  pCullObject )
{
    m_pCurrentCullFrustum = pCullObject;
}




//-----------------------------------------------------------------------------
// Name: GetCullFrustumObject
// Desc: Get Current CullFrustum Object
//-----------------------------------------------------------------------------
ICullFrustum*  CXBoxSample::GetCullFrustumObject()
{
    if( m_pCurrentCullFrustum )
        return m_pCurrentCullFrustum;

    return this;
}




//-----------------------------------------------------------------------------
// Name: Render
// Desc: Called once per frame, the call is the entry point for 3d
//        rendering. This function sets up render states, clears the
//        viewport, and renders the scene.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Render()
{
    if( m_bWireFrame )
    {
        g_pd3dDevice->Clear( 0,
            NULL,
            D3DCLEAR_ZBUFFER | D3DCLEAR_TARGET,
            0,
            1.0f,
            0L );

        g_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_WIREFRAME );
    }
    else
    {
        // Clear Z buffer only in solid mode
        g_pd3dDevice->Clear( 0, NULL,  D3DCLEAR_ZBUFFER, 0, 1.0f, 0L );
        g_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
    }
    
    
    // Render
    if( !m_bShowHelp )
    {
        m_pNonWater->Render( RF_NORMAL );
        m_pWater->Render();
    }

    if( m_bShowHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );

        // The Z buffer will be disable when call DrawText
        D3DDevice::SetRenderState( D3DRS_ZENABLE, TRUE );
    }
    else if( m_bShowInfo ) // Show infomation 
    {
        FLOAT yAt = 38.0f;
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );

        m_Font.DrawText( 50.0f, yAt, 0xffffffff, L"Water" );
        m_Font.DrawText( 592.0f, yAt, 0xffffff00, m_strFrameRate, XBFONT_RIGHT );
        yAt += 30;
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.End();

        // The Z buffer will be disable when call DrawText
        D3DDevice::SetRenderState( D3DRS_ZENABLE, TRUE );
    }

    g_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Cleanup
// Desc: Clear all the objects and release interfaces
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Cleanup()
{
    m_pRefraction->Cleanup();
    SAFE_DELETE( m_pRefraction ); 

    m_pReflection->Cleanup();
    SAFE_DELETE( m_pReflection );

    m_pNonWater->Cleanup();
    SAFE_DELETE( m_pNonWater );

    m_pResMan->Cleanup();
    SAFE_DELETE( m_pResMan );

    m_pWater->Cleanup();
    SAFE_DELETE( m_pWater );

    return S_OK;
}




//-----------------------------------------------------------------------------
//  Utility Functions used by project
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// Name: GetFCCFromString
// Desc: Get the FCC DWORD of the input string.
//        The FCC value is used to set shader parameters
//-----------------------------------------------------------------------------
inline DWORD GetFCCFromString( const CHAR* str )
{
    return MAKEFOURCC( str[0], str[1], str[2], str[3] );
}




//-----------------------------------------------------------------------------
// Name: ConvertStringToFCC
// Desc: When a string is more than 4 chars, convert it to 4 chars and return 
//        its FCC DWORD
//-----------------------------------------------------------------------------
DWORD ConvertStringToFCC( const CHAR* str )
{
    if( !_stricmp( str, "normal" ) )
        return GetFCCFromString( "NORM" );

    if( !_stricmp( str, "reflection" ) )
        return GetFCCFromString( "RFLE" );

    if( !_stricmp ( str, "refraction" ) )
        return GetFCCFromString( "RFRA" );

    if( !_stricmp( str, "refractionAndWriteAlpha" ) )
        return GetFCCFromString( "RFWA" );

    assert(FALSE ); // Unreachable

    return 0;
}




//-----------------------------------------------------------------------------
// Name: CreateEffectFromFile
// Desc: Create effect directly from file
//-----------------------------------------------------------------------------
HRESULT CreateEffectFromFile( LPDIRECT3DDEVICE8 pDevice,
                              const CHAR* pSrcFile,
                              LPD3DXEFFECT* ppEffect)
{
    LPD3DXBUFFER ptmpBuffer,pbufErr;
    HRESULT hr = D3DXCompileEffectFromFileA( pSrcFile, &ptmpBuffer, &pbufErr );
    if( hr != S_OK )
        return hr;

    return D3DXCreateEffect( pDevice,
                            ptmpBuffer->GetBufferPointer(),
                            ptmpBuffer->GetBufferSize(), 
                            0, 
                            ppEffect );
}

