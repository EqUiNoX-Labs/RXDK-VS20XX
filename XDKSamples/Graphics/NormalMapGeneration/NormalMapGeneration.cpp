//-----------------------------------------------------------------------------
// File: NormalShootng.cpp
//
// Desc: Example code showing how to compute a normal map by doing ray 
//       intersections with a high resolution object using the normals of a 
//       low resolution object.
//
// Hist: 08.05.02 - New
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <XBMesh.h>
#include <XBResource.h>
#include <XBUtil.h>
#include "NormalMesh.h"
#include "RayMesh.h"


//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_LEFTSTICK,    XBHELP_PLACEMENT_1, L"Move camera" },
    { XBHELP_X_BUTTON,     XBHELP_PLACEMENT_1, L"High-Res Model" },
    { XBHELP_A_BUTTON,     XBHELP_PLACEMENT_1, L"Low-Res\nModel" },
    { XBHELP_B_BUTTON,     XBHELP_PLACEMENT_1, L"Low-Res with\nNormalMap" },
    { XBHELP_WHITE_BUTTON, XBHELP_PLACEMENT_1, L"Recompute\nnormal map" },
    { XBHELP_RIGHT_BUTTON, XBHELP_PLACEMENT_1, L"Zoom in" },
    { XBHELP_LEFT_BUTTON,  XBHELP_PLACEMENT_1, L"Zoom out" },
    { XBHELP_BACK_BUTTON,  XBHELP_PLACEMENT_1, L"Display help" },
};

#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts)/sizeof(g_HelpCallouts[0]))




//-----------------------------------------------------------------------------
// Custom vertex types
//-----------------------------------------------------------------------------
struct MIRRORVERT
{
    D3DVECTOR Pos;
    D3DCOLOR  Color;
};


MIRRORVERT g_MirrorVerts[] =
{
    { { 0.0f, -0.5f, -0.1f }, D3DCOLOR_ARGB(60, 0, 0, 255) },
    { { 0.0f,  0.5f, -0.1f }, D3DCOLOR_ARGB(60, 0, 0, 255) },
    { { 0.0f, -0.5f,  2.0f }, D3DCOLOR_ARGB(60, 0, 0, 255) },
    { { 0.0f,  0.5f,  2.0f }, D3DCOLOR_ARGB(60, 0, 0, 255) },
};




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    CXBPackedResource  m_xprResource;        // Packed resources for the app
    CXBFont            m_Font;               // Font class
    CXBHelp            m_Help;               // Help class
    bool               m_bDrawHelp;          // Whether to draw help

    RayMesh            m_ModelHigh;          // XBG file object to render
    NormalMesh         m_ModelLow;           // XBG file object to render

    bool               m_bDrawHighRes;       // Draw the high-res mesh?
    bool               m_bUseNormalMap;      // Draw using the normal map?

    D3DXVECTOR3        m_vEye;

    HRESULT RenderScene();

public:
    HRESULT Initialize();
    HRESULT Render();
    HRESULT FrameMove();

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
    m_bDrawHelp     = false;
    m_bDrawHighRes  = false;
    m_bUseNormalMap = true;
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Called once to initialize stuff specific to the sample.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    // Create the font
    if( FAILED( m_Font.Create( "Font.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create help
    if( FAILED( m_Help.Create( "Gamepad.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create the resources
    if( FAILED( m_xprResource.Create( "Resource.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Load the low-res mesh.
    if( FAILED( m_ModelLow.Create( "Models\\SimpleMesh.xbg", &m_xprResource ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Load the high-res mesh.
    if( FAILED( m_ModelHigh.Create( "Models\\ComplexMesh.xbg", &m_xprResource ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Cache vertex buffers.
    m_ModelHigh.CacheVertices();

    // Set the matrices
    D3DXMATRIX matWorld, matView, matProj;

    D3DXMatrixIdentity( &matWorld );
    m_pd3dDevice->SetTransform( D3DTS_WORLD, &matWorld );

    m_vEye = D3DXVECTOR3( 0.0f, 0.25f, -1.0f );
    D3DXVECTOR3 vAt( 0.0f, 0.0f, 0.0f );
    D3DXVECTOR3 vUp( 0.0f, 1.0f, 0.0f );

    D3DXMatrixLookAtRH( &matView, &m_vEye, &vAt, &vUp );
    m_pd3dDevice->SetTransform( D3DTS_VIEW, &matView );

    D3DXMatrixPerspectiveFovRH( &matProj, D3DX_PI/3, 4.0f/3.0f, 0.01f, 100.0f );
    m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &matProj );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    D3DXMATRIX matView, matRotate;

    // Toggle help
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // High resolution model.
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_X] )
    {
        m_bDrawHighRes = true;
    }
    
    // Low resolution model without normal map.
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
    {
        m_bDrawHighRes = false;
        m_bUseNormalMap = false;
    }
    
    // Low resolution model with normal map.
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_B] )
    {
        m_bDrawHighRes = false;
        m_bUseNormalMap = true;
    }
    
    // Re-compute the normal map.
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_WHITE] )
    {
        // Clear the viewport, zbuffer, and stencil buffer
        m_pd3dDevice->Clear( 0L, NULL, 
                             D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,
                             0x001f1f1f, 1.0f, 0L );

        m_Font.DrawText(  64, 50, 0xffffffff, 
                          L"Calculating Normal Map, Please Wait." );

        m_Font.DrawText(  64, 75, 0xffffffff, 
                          L"Calculation may take several minutes." );

        m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

        DWORD dwStartMS = GetTickCount();

        // Build spatial subdivision to accelerate intersection calculations.
        m_ModelHigh.CalculateSpatialSubdivision();

        DWORD dwMiddleMS = GetTickCount();

        // Create the normal map.
        m_ModelLow.CalculateNormalMap( &m_ModelHigh, 4 );

        DWORD dwStopMS = GetTickCount();

        char buf[80];
        sprintf(buf, "CalculateSpatialSubdivision: %.02f Seconds Elapsed\n", 
                     (dwMiddleMS - dwStartMS) / 1000.0f);
        OutputDebugStringA(buf);
        sprintf(buf, "CalculateNormalMap: %.02f Seconds Elapsed\n", 
                     (dwStopMS - dwMiddleMS) / 1000.0f);
        OutputDebugStringA(buf);
    }

    D3DXVECTOR3 vAt( 0.0f, 0.0f, 0.0f );
    D3DXVECTOR3 vUp( 0.0f, 1.0f, 0.0f );

    // Rotate eye around up axis.
    D3DXMatrixRotationAxis( &matRotate, &vUp, 
                            m_DefaultGamepad.fX1*m_fElapsedTime );

    D3DXVec3TransformCoord( &m_vEye, &m_vEye, &matRotate );

    // Rotate eye points around side axis.
    D3DXVECTOR3 vView = (m_vEye - vAt);
    float dist = D3DXVec3Length( &vView );
    D3DXVec3Normalize( &vView, &vView );

    // Place limits so we don't go over the top or under the bottom.
    FLOAT dot = D3DXVec3Dot( &vView, &vUp );
    if( (dot > -0.99f || m_DefaultGamepad.fY1 < 0.0f) && 
        (dot <  0.99f || m_DefaultGamepad.fY1 > 0.0f) )
    {
        D3DXVECTOR3 axis;
        D3DXVec3Cross( &axis, &vUp, &vView );

        D3DXMatrixRotationAxis( &matRotate, &axis, 
                                m_DefaultGamepad.fY1*m_fElapsedTime );

        D3DXVec3TransformCoord( &m_vEye, &m_vEye, &matRotate );
    }

    // Move in/out.
    float fIn = (m_DefaultGamepad.bAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER] 
                 / 255.0f);

    float fOut = (m_DefaultGamepad.bAnalogButtons[XINPUT_GAMEPAD_LEFT_TRIGGER] 
                  / 255.0f);

    if( fIn > 0.1f && dist > 0.5f )
        m_vEye -= vView * 5.0f * fIn * m_fElapsedTime;

    if( fOut > 0.1f )
        m_vEye += vView * 5.0f * fOut * m_fElapsedTime;

    D3DXMatrixLookAtRH( &matView, &m_vEye, &vAt, &vUp );

    m_pd3dDevice->SetTransform( D3DTS_VIEW, &matView );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: RenderScene()
// Desc: Render the objects that make up the scene.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::RenderScene()
{
    D3DXVECTOR3 vLightDir( 0.0f, 0.7071067f, 0.7071067f );

    D3DXMATRIX matView, matInverse;
    m_pd3dDevice->GetTransform( D3DTS_VIEW, &matView );
    D3DXMatrixInverse( &matInverse, 0, &matView );
    D3DXVec3TransformNormal( &vLightDir, &vLightDir, &matInverse );

    if ( m_bDrawHighRes )
    {
        // Draw the high res mesh.
        m_pd3dDevice->SetRenderState( D3DRS_LIGHTING, TRUE );
        m_pd3dDevice->SetRenderState( D3DRS_AMBIENT, 0x00000000 );

        // Set up the light
        D3DLIGHT8 light;
        XBUtil_InitLight( light, D3DLIGHT_DIRECTIONAL, -vLightDir.x, -vLightDir.y, -vLightDir.z );
        m_pd3dDevice->SetLight( 0, &light );
        m_pd3dDevice->LightEnable( 0, TRUE );
        
        // Default material.
        D3DMATERIAL8 mtrlDefault;
        XBUtil_InitMaterial( mtrlDefault, 1.0f, 1.0f, 1.0f, 1.0f );
        m_pd3dDevice->SetMaterial( &mtrlDefault );

        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG2 );

        m_ModelHigh.Render( XBMESH_NOMATERIALS );
    }
    else
    {
        if ( m_bUseNormalMap )
        {
            m_pd3dDevice->SetRenderState( D3DRS_LIGHTING, FALSE );

            int r = int((vLightDir.x * 0.5f + 0.5f) * 255.0f);
            int g = int((vLightDir.y * 0.5f + 0.5f) * 255.0f);
            int b = int((vLightDir.z * 0.5f + 0.5f) * 255.0f);

            DWORD tfactor = 0xff000000 | (r << 16) | (g << 8) | b;
            m_pd3dDevice->SetRenderState( D3DRS_TEXTUREFACTOR, tfactor );

            m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
            m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_TFACTOR );
            m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_DOTPRODUCT3 );
        }
        else
        {
            m_pd3dDevice->SetRenderState( D3DRS_LIGHTING, TRUE );
            m_pd3dDevice->SetRenderState( D3DRS_AMBIENT, 0x00000000 );

            // Set up the light
            D3DLIGHT8 light;
            XBUtil_InitLight( light, D3DLIGHT_DIRECTIONAL, -vLightDir.x, -vLightDir.y, -vLightDir.z );
            m_pd3dDevice->SetLight( 0, &light );
            m_pd3dDevice->LightEnable( 0, TRUE );

            // Default material.
            D3DMATERIAL8 mtrlDefault;
            XBUtil_InitMaterial( mtrlDefault, 1.0f, 1.0f, 1.0f, 1.0f );
            m_pd3dDevice->SetMaterial( &mtrlDefault );

            m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
            m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
            m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG2 );
        }

        // Draw the low res mesh.
        m_ModelLow.Render( XBMESH_NOMATERIALS );
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Render()
{
    // Clear the viewport, zbuffer, and stencil buffer
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,
                         0x001f1f1f, 1.0f, 0L );

    // Set up misc render states
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CW );

    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_DITHERENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SPECULARENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );

    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );

    // Render the scene.
    RenderScene();

    // Show title, frame rate, and help
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        // Show frame rate
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 48, 36, 0xffffffff,  L"NormalMapGeneration" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 592, 38, 0xffffff00, m_strFrameRate, XBFONT_RIGHT );

        if( m_bDrawHighRes )
        {
            m_Font.DrawText(  64, 75, 0xffffff00, L"High Resolution Model (30,408 tris)" );
        }
        else
        {
            if( m_bUseNormalMap )
            {
                m_Font.DrawText(  64, 75, 0xffffff00, 
                                  L"Low Resolution Model (1,393 tris), Normal Map" );
            }
            else
            {
                m_Font.DrawText(  64, 75, 0xffffff00, 
                                  L"Low Resolution Model (1,393 tris), No Normal Map" );
            }
        }
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}
