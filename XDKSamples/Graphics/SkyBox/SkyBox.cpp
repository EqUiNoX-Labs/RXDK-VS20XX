//-----------------------------------------------------------------------------
// File: SkyBox.cpp
//
// Desc: Demonstrates a cool artist trick where you can apply dynamic time-of-
//       day lighting effects on a sky box. The skybox is rendered with
//       luminance-only (aka "gray scale") textures, and dynamic coloring is
//       achieved through coloring specially places vertices in the skybox
//       geometry.
//
//       In order that the programmer does not have to manually tweak vertices,
//       the app uses a vertex shader which applies a tex coord value depending
//       on the y-value of each skybox vertex. The tex coord then is used to
//       lookup actual color values from a texture. Since only one tex-coord
//       is needed for the effect, the other can be used as a sliding scale for
//       the time of day. For instance, to get different color gradients for a
//       morning sky versus a high-noon sky.
//
//       Note that we use the term "skybox" loosely here, as the underlying
//       geometry could as well be a sphere, a cylinder, etc..
//
// Hist: 06.16.02 - New for August 2002 XDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBMesh.h>
#include <XBUtil.h>
#include <XBHelp.h>




//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
static XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_Y_BUTTON,       XBHELP_PLACEMENT_2, L"Toggle\nbrightness map" },
    { XBHELP_B_BUTTON,       XBHELP_PLACEMENT_2, L"Toggle color\nmap" },
    { XBHELP_X_BUTTON,       XBHELP_PLACEMENT_2, L"Toggle color\nmap display" },
    { XBHELP_LEFTSTICK,      XBHELP_PLACEMENT_1, L"Rotate camera" },
    { XBHELP_RIGHTSTICK,     XBHELP_PLACEMENT_1, L"Adjust time-of-day" },
    { XBHELP_MISC_CALLOUT_2, XBHELP_PLACEMENT_2, L"Triggers adjust\ncolor blend amount" },
    { XBHELP_START_BUTTON,   XBHELP_PLACEMENT_1, L"Pause" },
    { XBHELP_BACK_BUTTON,    XBHELP_PLACEMENT_1, L"Display help" },
};

#define NUM_HELP_CALLOUTS ( sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]) )




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    CXBPackedResource      m_xprResource;          // Packed resources for the app
    CXBFont                m_Font;                 // Font class
    CXBHelp                m_Help;                 // Help object
    BOOL                   m_bDrawHelp;            // TRUE to draw help screen

    D3DXMATRIX             m_matWorld;             // World matrix
    D3DXMATRIX             m_matView;              // View matrix
    D3DXMATRIX             m_matProj;              // Projection matrix

    D3DXMATRIX             m_matSkyboxView;        // View matrix for rendering skybox

    DWORD                  m_dwSkyBoxVertexShader; // Skybox vertex shader

    LPDIRECT3DTEXTURE8     m_pSkyBoxColorLookupTexture; // Skybox textures
    LPDIRECT3DCUBETEXTURE8 m_pSkyBoxLuminanceCubeMap;

    D3DVertexBuffer*       m_pSkyBoxVB;            // Skybox geometry
    DWORD                  m_dwNumSkyBoxVertices;

    BOOL                   m_bShowColors;          // Apply color to the sky box?
    FLOAT                  m_fColorAmount;         // Color [0..1] to apply to the sky box?
    BOOL                   m_bShowOverlay;         // Display the color we are overlaying?
    BOOL                   m_bShowLuminance;       // Display the luminance map?
    FLOAT                  m_fTimeOfDayAdjust;     // Offset for the time of day?

    VOID    RenderColorTextureAsOverlay();
    VOID    CreateSkyBox();

public:
    HRESULT Initialize();       // Initialize the sample
    HRESULT Render();           // Render the scene
    HRESULT FrameMove();        // Perform per-frame updates

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
// Desc: Constructor for CXBoxSample class
//-----------------------------------------------------------------------------
CXBoxSample::CXBoxSample() 
            :CXBApplication()
{
    m_bDrawHelp        = FALSE;
    m_bShowColors      = TRUE;
    m_bShowOverlay     = TRUE;
    m_bShowLuminance   = TRUE;
    m_fColorAmount     = 1.0f;
    m_fTimeOfDayAdjust = 0.0f;
}




//-----------------------------------------------------------------------------
// Name: CreateSkyBox()
// Desc: Our skybox is actually an inside-out unit sphere. We don't need
//       normals since it won't be lit, and we can derive cubemap texcoords
//       from the vertex position in our shader.
//-----------------------------------------------------------------------------
VOID CXBoxSample::CreateSkyBox()
{
    // Note: Adjust the tesselation of the sphere to fit your needs. These are
    // very lightweight vertices, and we'll be fillbound when rendering them
    // anyways.
    const DWORD dwNumSphereRings    = 24;
    const DWORD dwNumSphereSegments = 24;

    // Establish constants used in sphere generation
    FLOAT fDeltaRingAngle = ( D3DX_PI / dwNumSphereRings );
    FLOAT fDeltaSegAngle  = ( 2.0f * D3DX_PI / dwNumSphereSegments );

    m_dwNumSkyBoxVertices = dwNumSphereRings*(dwNumSphereSegments+1)*2;

    // Create the vertex buffer and fill it
    m_pd3dDevice->CreateVertexBuffer( m_dwNumSkyBoxVertices*sizeof(D3DXVECTOR3),
                                      D3DUSAGE_WRITEONLY, 0L,
                                      D3DPOOL_MANAGED, &m_pSkyBoxVB );

    D3DXVECTOR3* pVertices;
    m_pSkyBoxVB->Lock( 0, 0, (BYTE**)&pVertices, 0 );

    // Generate the group of rings for the sphere
    for( DWORD ring = 0; ring < dwNumSphereRings; ring++ )
    {
        FLOAT r0 = sinf( (ring+0) * fDeltaRingAngle );
        FLOAT r1 = sinf( (ring+1) * fDeltaRingAngle );
        FLOAT y0 = cosf( (ring+0) * fDeltaRingAngle );
        FLOAT y1 = cosf( (ring+1) * fDeltaRingAngle );

        // Generate the group of segments for the current ring
        for( DWORD seg = 0; seg < (dwNumSphereSegments+1); seg++ )
        {
            FLOAT x0 =  r0 * sinf( seg * fDeltaSegAngle );
            FLOAT z0 =  r0 * cosf( seg * fDeltaSegAngle );
            FLOAT x1 =  r1 * sinf( seg * fDeltaSegAngle );
            FLOAT z1 =  r1 * cosf( seg * fDeltaSegAngle );

            // Add two vertices to the strip which makes up the sphere
            *pVertices++ = D3DXVECTOR3(x1,y1,z1);
            *pVertices++ = D3DXVECTOR3(x0,y0,z0);
        }
    }

    m_pSkyBoxVB->Unlock();
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Performs initialization
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    // Create a font
    if( FAILED( m_Font.Create( "Font.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create help
    if( FAILED( m_Help.Create( "Gamepad.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create the resources
    if( FAILED( m_xprResource.Create( "Resource.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Gain access to the textures. One texture is a cubemap of a gray-scale
    // sky, and the other is a lookup table for the time-of-day lighting
    m_pSkyBoxLuminanceCubeMap   = m_xprResource.GetCubemap( "SkyBoxLuminanceCubeMap" );
    m_pSkyBoxColorLookupTexture = m_xprResource.GetTexture( "SkyBoxColorLookupTable" );

    // Set up transforms
    D3DXMatrixIdentity( &m_matWorld );
    D3DXMatrixIdentity( &m_matView );
    D3DXMatrixPerspectiveFovLH( &m_matProj, D3DX_PI/4, 4.0f/3.0f, 0.1f, 100.0f );
    m_pd3dDevice->SetTransform( D3DTS_WORLD,      &m_matWorld );
    m_pd3dDevice->SetTransform( D3DTS_VIEW,       &m_matView );
    m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &m_matProj );

    // Create the sky box geometry
    CreateSkyBox();

    // Create vertex shader for the skybox. This vertex shader draws the skybox,
    // using each vertex's y-position to compute a tex coord used to look up a
    // value from the time-of-day color lookup texture.
    DWORD dwSkyBoxVertexDecl[] =
    {
        D3DVSD_STREAM( 0 ),
        D3DVSD_REG( 0, D3DVSDT_FLOAT3 ),   // v0 = Position
        D3DVSD_END()
    };

    if( FAILED( XBUtil_CreateVertexShader( "Shaders\\SkyBox.xvu",
                                           dwSkyBoxVertexDecl, &m_dwSkyBoxVertexShader ) ) )
        return XBAPPERR_MEDIANOTFOUND;

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

    static D3DXVECTOR3 m_vViewAngle( 0.0f, 0.0f, 0.0f );

    // Move the camera
    m_vViewAngle.y += m_DefaultGamepad.fX1*1.0f*m_fElapsedTime;

    if( m_vViewAngle.y > D3DX_PI*2 )
        m_vViewAngle.y -= D3DX_PI*2;
    if( m_vViewAngle.y < 0.0f )
        m_vViewAngle.y += D3DX_PI*2;

    m_vViewAngle.x += m_DefaultGamepad.fY1*1.0f*m_fElapsedTime;
    if( m_vViewAngle.x > D3DX_PI/4 )
        m_vViewAngle.x = D3DX_PI/4;
    if( m_vViewAngle.x < -D3DX_PI/4 )
        m_vViewAngle.x = -D3DX_PI/4;

    // Use the lookat direction to get the skybox view matrix
    D3DXMATRIX  matRotate;
    D3DXMatrixRotationYawPitchRoll( &matRotate, m_vViewAngle.y, m_vViewAngle.x, m_vViewAngle.z );
    const D3DXVECTOR3 vEyePos( 0.0f, 0.0f, 0.0f );
          D3DXVECTOR3 vLookAt( 0.0f, 0.0f, 1.0f );
    const D3DXVECTOR3 vUp    ( 0.0f, 1.0f, 0.0f );
    D3DXVec3TransformCoord( &vLookAt, &vLookAt, &matRotate );
    D3DXMatrixLookAtLH( &m_matSkyboxView, &vEyePos, &vLookAt, &vUp );

    // Toggle showing the colors
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_B] )
        m_bShowColors = !m_bShowColors;

    // Toggle showing luminance
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_Y] )
        m_bShowLuminance = !m_bShowLuminance;

    // Toggle showing the overlay
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_X] )
        m_bShowOverlay = !m_bShowOverlay;

    // Adjust the chrominance intensity. This is just handy for visualization
    // to allow smoothly going from no color blended in to full color.
    const FLOAT fTriggerScale = (2.0f / 255.0f);
    m_fColorAmount += m_DefaultGamepad.bLastAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER] * fTriggerScale;
    m_fColorAmount -= m_DefaultGamepad.bLastAnalogButtons[XINPUT_GAMEPAD_LEFT_TRIGGER] * fTriggerScale;
    if( m_fColorAmount < 0.0f )
        m_fColorAmount = 0.0f;
    if( m_fColorAmount > 1.0f )
        m_fColorAmount = 1.0f;

    // Adjust the time of day
    m_fTimeOfDayAdjust += m_DefaultGamepad.fX2 * m_fElapsedTime;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: RenderColorTextureAsOverlay()
// Desc: Renders the overlay
//-----------------------------------------------------------------------------
VOID CXBoxSample::RenderColorTextureAsOverlay()
{
    const FLOAT fSize    = 128.0f;
    const FLOAT fOriginX =  50.0f;
    const FLOAT fOriginY = 440.0f - fSize;

    FLOAT tv = m_fAppTime / 25 + m_fTimeOfDayAdjust;
    tv -= floorf(tv);

    // Draw the sky texture as a overlay.
    m_pd3dDevice->SetRenderState( D3DRS_FILLMODE,         D3DFILL_SOLID );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE,         D3DCULL_CCW );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE,          FALSE );

    // Draw the overlay quad
    m_pd3dDevice->SetTexture( 0, m_pSkyBoxColorLookupTexture );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );

    FLOAT left   = fOriginX;
    FLOAT right  = fOriginX + fSize;
    FLOAT top    = fOriginY;
    FLOAT bottom = fOriginY + fSize;

    m_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW|D3DFVF_TEX1 );
    m_pd3dDevice->Begin( D3DPT_QUADLIST );
    m_pd3dDevice->SetVertexData2f( D3DVSDE_TEXCOORD0, 0, 1 );
    m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX, left,  bottom, 0.0f, 1.0f );
    m_pd3dDevice->SetVertexData2f( D3DVSDE_TEXCOORD0, 0, 0 );
    m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX, left,  top,    0.0f, 1.0f );
    m_pd3dDevice->SetVertexData2f( D3DVSDE_TEXCOORD0, 1, 0 );
    m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX, right, top,    0.0f, 1.0f );
    m_pd3dDevice->SetVertexData2f( D3DVSDE_TEXCOORD0, 1, 1 );
    m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX, right, bottom, 0.0f, 1.0f );
    m_pd3dDevice->End();

    // Draw an indicator showing what portion of the texture we are reading from.
    // Draw it in white.
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE );

    left   = fOriginX + fSize * tv - 2;
    right  = fOriginX + fSize * tv + 2;
    top    = fOriginY - 1;
    bottom = fOriginY + fSize + 1;

    m_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW|D3DFVF_DIFFUSE );
    m_pd3dDevice->Begin( D3DPT_LINELOOP );
    m_pd3dDevice->SetVertexDataColor( D3DVSDE_DIFFUSE, 0xffffffff );
    m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX, left,  bottom, 0.0f, 1.0f );
    m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX, left,  top,    0.0f, 1.0f );
    m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX, right, top,    0.0f, 1.0f );
    m_pd3dDevice->SetVertexData4f( D3DVSDE_VERTEX, right, bottom, 0.0f, 1.0f );
    m_pd3dDevice->End();
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the scene
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Render()
{
    // Clear the depthbuffer
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,
                         0x00000000, 1.0f, 0L );

    // Set state for rendering the skybox
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE,  FALSE );

    // Lerp the color texture with pure white to get some arbitrary mixture of the two.
    m_pd3dDevice->SetTexture( 0, m_pSkyBoxColorLookupTexture );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_LERP );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG0, D3DTA_TFACTOR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE ); // White
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU,  D3DTADDRESS_WRAP );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV,  D3DTADDRESS_WRAP );
    int BlendFactor = m_bShowColors ? (int)( m_fColorAmount * 255.0f + 0.5f ) : 0;
    m_pd3dDevice->SetRenderState( D3DRS_TEXTUREFACTOR, 0x01010101 * BlendFactor );

    // Blend in the luminance map. All of the detail comes from this.
    m_pd3dDevice->SetTexture( 1, m_bShowLuminance ? m_pSkyBoxLuminanceCubeMap : NULL );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_MODULATE );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLORARG2, D3DTA_CURRENT );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP );

    // Pass the transform set to the vertex shader
    D3DXMATRIX matViewT, matProjT;
    D3DXMatrixTranspose( &matViewT, &m_matSkyboxView );
    D3DXMatrixTranspose( &matProjT, &m_matProj );
    m_pd3dDevice->SetVertexShaderConstant( 20, &matViewT, 4 );
    m_pd3dDevice->SetVertexShaderConstant( 24, &matProjT, 4 );

    // Pass a "time-of-day" value to the vertex shader, where:
    //    c10.x is a time value for which column of the color texture to use.
    //    c10.y is a multiplier to move the vertex y-coordinates to [0.5f..-0.5f] range.
    //    c10.z is an offset to move the vertex y-coordinates from to [1.0f..0.0f] range.
    //    c10.w is 1.0f
    FLOAT tv = m_fAppTime / 25 + m_fTimeOfDayAdjust;
    tv -= floorf(tv);
    D3DXVECTOR4 c10( tv, -0.5f, 0.5f, 1.0f );
    m_pd3dDevice->SetVertexShaderConstant( 10, &c10, 1 );

    // Render the skybox
    m_pd3dDevice->SetVertexShader( m_dwSkyBoxVertexShader );
    m_pd3dDevice->SetStreamSource( 0, m_pSkyBoxVB, sizeof(D3DXVECTOR3) );
    m_pd3dDevice->DrawVertices( D3DPT_TRIANGLESTRIP, 0, m_dwNumSkyBoxVertices );
    
    // Restore state
    m_pd3dDevice->SetTexture( 0, NULL );
    m_pd3dDevice->SetTexture( 1, NULL );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );

    // Show the color texture as an overlay
    if( m_bShowOverlay )
        RenderColorTextureAsOverlay();

    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        // Show title, frame rate
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 48, 36, 0xffffffff, L"SkyBox" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 592, 38, 0xffffff00, m_strFrameRate, XBFONT_RIGHT );

        m_Font.DrawText( 64, 75, 0xffffffff, L"Luminance Texture: " );
        m_Font.DrawText( 0xffffff00, m_bShowLuminance ? L"On" : L"Off" );

        m_Font.DrawText( 64, 100, 0xffffffff, L"Color Texture: " );
        m_Font.DrawText( 0xffffff00, m_bShowColors ? L"On" : L"Off" );

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}
