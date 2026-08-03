//-----------------------------------------------------------------------------
// File: AntiAlias.cpp
//
// Desc: Demonstrates the various anti-alias modes
//
// Hist: 11.29.01 - Created
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <XBMesh.h>
#include <xgraphics.h>




//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_BACK_BUTTON, XBHELP_PLACEMENT_1, L"Display help" },
    { XBHELP_A_BUTTON,    XBHELP_PLACEMENT_2, L"Change\nantialias\nmode" },
    { XBHELP_X_BUTTON,    XBHELP_PLACEMENT_1, L"Change model" },
    { XBHELP_LEFTSTICK,   XBHELP_PLACEMENT_1, L"Rotate object" },
    { XBHELP_RIGHTSTICK,  XBHELP_PLACEMENT_1, L"Zoom/scale\nbackbuffer" },
};

#define NUM_HELP_CALLOUTS ( sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]) )




#define NEAR_PLANE           1.0f
#define FAR_PLANE         2000.0f
#define EARTH_FAR_PLANE 200000.0f


#define DRAW_TEAPOT        0
#define DRAW_CAR           1
#define DRAW_STARSHIP      2
#define MAX_DRAW_OBJECTS   DRAW_STARSHIP




struct AntiAliasMode
{
    DWORD  dwMultiSampleType;
    WCHAR* strDesc;
    BOOL   bCanScaleBackBuffer;
};

AntiAliasMode g_AntiAliasModes[] =
{
    { D3DMULTISAMPLE_NONE,                                    L"None",                                    TRUE },
    { D3DMULTISAMPLE_NONE,                                    L"Edge Anti-Alias",                         TRUE },
    { D3DMULTISAMPLE_2_SAMPLES_MULTISAMPLE_LINEAR,            L"2x MultiSample Linear Filter",            TRUE },
    { D3DMULTISAMPLE_2_SAMPLES_MULTISAMPLE_QUINCUNX,          L"2x MultiSample Quincunx Filter",          FALSE },
    { D3DMULTISAMPLE_2_SAMPLES_SUPERSAMPLE_HORIZONTAL_LINEAR, L"2x SuperSample Linear Horizontal Filter", TRUE },
    { D3DMULTISAMPLE_2_SAMPLES_SUPERSAMPLE_VERTICAL_LINEAR,   L"2x SuperSample Linear Vertical Filter",   TRUE },
    { D3DMULTISAMPLE_4_SAMPLES_MULTISAMPLE_LINEAR,            L"4x MultiSample Linear Filter",            TRUE },
    { D3DMULTISAMPLE_4_SAMPLES_MULTISAMPLE_GAUSSIAN,          L"4x MultiSample Gaussian Filter",          FALSE },
    { D3DMULTISAMPLE_4_SAMPLES_SUPERSAMPLE_LINEAR,            L"4x SuperSample Linear Filter",            TRUE },
    { D3DMULTISAMPLE_4_SAMPLES_SUPERSAMPLE_GAUSSIAN,          L"4x SuperSample Gaussian Filter",          FALSE },
    { D3DMULTISAMPLE_9_SAMPLES_MULTISAMPLE_GAUSSIAN,          L"9x MultiSample Gaussian Filter",          FALSE },
    { D3DMULTISAMPLE_9_SAMPLES_SUPERSAMPLE_GAUSSIAN,          L"9x SuperSample Gaussian Filter",          FALSE },
};

#define NUM_ANTIALIAS_MODES ( sizeof(g_AntiAliasModes) / sizeof(g_AntiAliasModes[0]) )
#define EDGE                 1




//-----------------------------------------------------------------------------
// Name: class CCarMesh
// Desc: Class to load and render geometry. Most functionality is inherited
//       from the CXBMesh base class.
//-----------------------------------------------------------------------------
class CCarMesh : public CXBMesh
{
public:
    // IDs for the Car mesh's subsets
    enum
    {
        BODY = 0,
        UNDERCARRIAGE,
        MIRRORS,
        WINDOWS,
        WINDOWSTRIM,
        MISC,
    };

    HRESULT RenderSubset( DWORD dwSubsetID )
    {
        XBMESH_DATA* pMesh = GetMesh(0);
        XBMESH_SUBSET*   pSubset         = &pMesh->m_pSubsets[dwSubsetID];
        D3DVertexBuffer* pVB             = &pMesh->m_VB;
        D3DIndexBuffer*  pIB             = &pMesh->m_IB;
        DWORD            dwVertexSize    = pMesh->m_dwVertexSize;
        D3DPRIMITIVETYPE dwPrimType      = pMesh->m_dwPrimType;
        DWORD            dwNumPrimitives = ( D3DPT_TRIANGLESTRIP == dwPrimType ) ? 
                                        pSubset->dwIndexCount-2 : pSubset->dwIndexCount/3;

        // Set the vertex stream
        D3DDevice::SetStreamSource( 0, pVB, dwVertexSize );
        D3DDevice::SetIndices( pIB, 0 );

        // Draw the mesh subset
        D3DDevice::DrawIndexedPrimitive( dwPrimType, 0, pSubset->dwIndexCount,
                                        pSubset->dwIndexStart, dwNumPrimitives );

        return S_OK;
    }
};




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    CXBPackedResource  m_xprResource;           // Packed resources for the app
    CXBFont            m_Font;                  // Font object
    CXBHelp            m_Help;                  // Help object
    BOOL               m_bDrawHelp;             // TRUE to draw help screen

    LPDIRECT3DTEXTURE8 m_pSphereMapTexture;     // Texture for Sphere mapping

    CXBMesh            m_Teapot;                // Teapot model
    CXBMesh            m_Starship;              // Starship model
    CCarMesh           m_Car;                   // Car model
    DWORD              m_dwDrawObject;          // Which model object to draw.

    D3DXMATRIX         m_matWorld;              // Transform matrices
    D3DXMATRIX         m_matView;
    D3DXMATRIX         m_matProj;

    D3DXVECTOR3        m_vCameraPosition;       // Camera position and look vector
    D3DXVECTOR3        m_vLookPosition;

    FLOAT              m_fXObjRotate;           // Rotation values for the draw object
    FLOAT              m_fYObjRotate;

    FLOAT              m_fBackBufferScale;      // Scale factor for the backBuffer

    DWORD              m_dwFresnelVertexShader; // Handle to the Fresnel Vertex Shader
    DWORD              m_dwCarBodyPixelShader;  // Handle to Pixel Shader for car body
    DWORD              m_dwCarGlassPixelShader; // Handle to Pixel Shader for car Windows

    DWORD              m_dwAntiAliasMode;       // Which anti-alias mode to use.

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
// Desc: Constructor for CXBoxSample class
//-----------------------------------------------------------------------------
CXBoxSample::CXBoxSample() 
            :CXBApplication()
{
    m_bDrawHelp = FALSE;

    // Do not wait for VSync.  
    // Show changes in frame rate from different antiAlias Modes.
    m_d3dpp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    // To prevent messy fonts with backBufferScale, use 2 back buffers
    m_d3dpp.BackBufferCount = 2; 
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Peforms initialization
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    // Create a font
    if( FAILED( m_Font.Create( "Font.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create help
    if( FAILED( m_Help.Create( "Gamepad.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Load packed resources
    if( FAILED( m_xprResource.Create( "Resource.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Load the spheremap texture
    m_pSphereMapTexture = m_xprResource.GetTexture( "SphereMap" );

    // Create a mesh (vertex and index buffers) for the teapot
    if( FAILED( m_Teapot.Create( "Models\\Teapot.xbg" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create a mesh (vertex and index buffers) for the car
    if( FAILED( m_Car.Create( "Models\\GenericSedan.xbg" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create a mesh (vertex and index buffers) for the Starship
    if( FAILED( m_Starship.Create( "Models\\Starship.xbg" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Set the transform matrices
    D3DXMatrixIdentity( &m_matWorld );
    D3DXMatrixIdentity( &m_matView );
    D3DXMatrixPerspectiveFovLH( &m_matProj, D3DX_PI/4.0f, 4.0f/3.0f, 
                                NEAR_PLANE, FAR_PLANE );

    // Setup the camera look and position vectors
    m_vLookPosition.x = 
    m_vLookPosition.y = 
    m_vLookPosition.z = 0.f;

    m_vCameraPosition.x = 5.f;
    m_vCameraPosition.y = m_vCameraPosition.z = 0.f;

    // Set initial rotation of object
    m_fXObjRotate = m_fYObjRotate = 0.f;

    // Set initial BackBuffer Scale
    m_fBackBufferScale = 1.0f;

    // Set which object to draw
    m_dwDrawObject = DRAW_TEAPOT;

    // Set initial Anti-Alias mode to none
    m_dwAntiAliasMode = 0;

    // Create vertex shaders
    {
        DWORD dwDecl[] =
        {
            D3DVSD_STREAM( 0 ),
            D3DVSD_REG( 0, D3DVSDT_FLOAT3 ),
            D3DVSD_REG( 1, D3DVSDT_FLOAT3 ),
            D3DVSD_REG( 2, D3DVSDT_FLOAT2 ),
            D3DVSD_END()
        };

        if( FAILED( XBUtil_CreateVertexShader( "Shaders\\Fresnel.xvu",
                                               dwDecl, &m_dwFresnelVertexShader ) ) )
            return XBAPPERR_MEDIANOTFOUND;
    }

    // Create the pixel shaders
    if( FAILED( XBUtil_CreatePixelShader( "Shaders\\CarOpaque.xpu",
                                          &m_dwCarBodyPixelShader ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    if( FAILED( XBUtil_CreatePixelShader( "Shaders\\CarTransparent.xpu",
                                          &m_dwCarGlassPixelShader ) ) )
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

    //Toggle Anti-Alias mode
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
    {
        if( ++m_dwAntiAliasMode >= NUM_ANTIALIAS_MODES )
            m_dwAntiAliasMode = 0;

        // Just reset the MultiSampleType in m_d3dpp.
        m_d3dpp.MultiSampleType = g_AntiAliasModes[m_dwAntiAliasMode].dwMultiSampleType;

        // Persist the display to avoid the screen flash during device reset
        m_pd3dDevice->PersistDisplay();

        // Reset the device - Just changes the screen resolution and 
        // Backbuffer size on Xbox.
        m_pd3dDevice->Reset( &m_d3dpp );
    }

    //Toggle Object
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_X] )
    {
        if( ++m_dwDrawObject > MAX_DRAW_OBJECTS )
            m_dwDrawObject = 0;

        switch( m_dwDrawObject )
        {
            case DRAW_TEAPOT:
                m_vCameraPosition.x = m_Teapot.ComputeRadius() * 3.0f;
                break;

            case DRAW_CAR:
                m_vCameraPosition.x = m_Car.ComputeRadius() * 3.0f;
                break;

            case DRAW_STARSHIP:
                m_vCameraPosition.x = m_Starship.ComputeRadius() * 3.0f;
                break;
        }
    }

    // Setup the projection matrix
    D3DXMatrixPerspectiveFovLH( &m_matProj, D3DX_PI/4.0f, 4.0f/3.0f, 
                                NEAR_PLANE, FAR_PLANE );

    // Setup the view Matrix
    D3DXVECTOR3 vUp( 0.0f, 1.0f, 0.0f );
    D3DXMatrixLookAtLH( &m_matView, &m_vCameraPosition, &m_vLookPosition, &vUp );

    // Check if we should rotate the object
    m_fXObjRotate += m_DefaultGamepad.fX1 * D3DX_PI * m_fElapsedTime;
    m_fYObjRotate += m_DefaultGamepad.fY1 * D3DX_PI * m_fElapsedTime;

    // Check if we should move the camera in or out.
    FLOAT fCameraZoomFactor = 1.0f;
    if( m_dwDrawObject == DRAW_STARSHIP )
        fCameraZoomFactor = 20.0f;

    m_vCameraPosition.x += m_DefaultGamepad.fY2 * fCameraZoomFactor * m_fElapsedTime;

    // Check if we should scale the back buffer at all.
    FLOAT fBackbufferScaleFactor = 0.2f;
    m_fBackBufferScale += m_DefaultGamepad.fX2 * fBackbufferScaleFactor * m_fElapsedTime;
    if( m_fBackBufferScale > 1.0f )
        m_fBackBufferScale = 1.0f;

    if( m_fBackBufferScale < 0.5f )
        m_fBackBufferScale = 0.5f;

    // Setup object matrix
    D3DXMATRIX matObjRotate;
    D3DXMatrixRotationYawPitchRoll( &matObjRotate, -m_fXObjRotate, -m_fYObjRotate, 0.0f );
    
    D3DXMatrixIdentity( &m_matWorld );
    D3DXMatrixMultiply( &m_matWorld, &m_matWorld, &matObjRotate );

    // Setup the Fresnel rotate matrix
    D3DXMATRIX matFresnelRotate;

    FLOAT fDeterminant = 0.f;
    D3DXMatrixInverse( &matFresnelRotate, &fDeterminant, &matObjRotate );

    // Setup the vertex shader constants
    {
        // Create the World/view/projection matrix concatenation
        D3DXMATRIX mat;
        D3DXMatrixMultiply( &mat, &m_matWorld, &m_matView );
        D3DXMatrixMultiply( &mat, &mat, &m_matProj );
        D3DXMatrixTranspose( &mat, &mat );

        // Rotate the camera normal into the Object's rotational frame.
        // Saves us having to transform all of the vectors on the object in the shader!
        D3DXVECTOR3 vCameraNormal( 1.0f, 0.0f, 0.0f );
        D3DXVECTOR3 vRotatedNormal;
        D3DXVec3TransformNormal( &vRotatedNormal, &vCameraNormal, &matFresnelRotate );

        m_pd3dDevice->SetVertexShaderConstant( 0, &mat, 4 );
        m_pd3dDevice->SetVertexShaderConstant( 4, &vRotatedNormal, 1 );

        // Generate sphere map texture coords from the position
        D3DXMATRIX matTexture;
        D3DXMatrixIdentity( &matTexture );
        matTexture._11 = 0.5f; matTexture._12 = 0.0f;
        matTexture._21 = 0.0f; matTexture._22 =-0.5f;
        matTexture._14 = 0.5f; matTexture._24 = 0.5f;

        D3DXMatrixMultiply( &matTexture, &matTexture, &mat );
        m_pd3dDevice->SetVertexShaderConstant( 15, &matTexture, 4 );
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the scene
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Render()
{
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

    // Draw a gradient filled background
    RenderGradientBackground( 0xff4040C0, 0xff404040 );

    // Setup render state
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_MINFILTER, D3DTEXF_LINEAR );

    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );

    m_pd3dDevice->SetRenderState( D3DRS_LIGHTING, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );

    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_DITHERENABLE, TRUE );

    // Turn on the anti-alias mode
    m_pd3dDevice->SetRenderState( D3DRS_MULTISAMPLEANTIALIAS, TRUE );

    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );

    if( !m_bDrawHelp )
    {
        // Draw the Object desired
        switch( m_dwDrawObject )
        {
            case DRAW_TEAPOT:
            {
                // If we are in edge anti-alias mode, set the render states
                // Done somewhat redundantly here for clarity.
                if( m_dwAntiAliasMode == EDGE )
                {
                    //--------------------------------------------------------------------------------
                    // NOTE:  THIS IS VERY IMPORTANT
                    //
                    // Edge anti-aliasing works by drawing an alpha edge to each triangle and
                    // line which is rendered.  For objects which are drawn WITHOUT zBuffer
                    // writes and in alpha mode Src Alpha, Dest One, like the teapot, this works 
                    // great. For any other kind of object, if you do not draw the object 
                    // un-aliased first, you will get a wire frame set of artifacts because the 
                    // alpha write to the zBuffer as well as the normal triangles.  The alphaed 
                    // edges take on the color behind them which is the background of the 
                    // backbuffer.  You need the background color to be the actual object color for
                    // that location.  This is still a big win over the normal anti-aliasing modes 
                    // because you don't need a giant backbuffer to get the aliasing you want.  
                    // Also, you can pick and choose what objects to anti-alias!
                    //
                    //--------------------------------------------------------------------------------
                }

                // Set vertex shader constants for this object.
                static D3DXVECTOR4 vForceColor( 0.45f, 0.45f, 0.45f, 1.00f );
                static D3DXVECTOR4 vConstants(  1.00f, 0.50f, 3.00f, 0.15f );
                static D3DXVECTOR4 vConstants1( 1.00f, 0.00f, 0.00f, 0.00f );
                m_pd3dDevice->SetVertexShaderConstant( 5, &vForceColor, 1 );
                m_pd3dDevice->SetVertexShaderConstant( 6, &vConstants,  1 );
                m_pd3dDevice->SetVertexShaderConstant( 7, &vConstants1, 1 );

                // Set the vertex Shader
                m_pd3dDevice->SetVertexShader( m_dwFresnelVertexShader );

                // Adds in the environment map
                m_pd3dDevice->SetTexture( 0, m_pSphereMapTexture );
                m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
                m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
                m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
    
                // Draw the teapot (always in just one pass)
                m_Teapot.Render( XBMESH_NOTEXTURES | XBMESH_NOFVF );

                break;
            }

            case DRAW_STARSHIP:
            {
                // Set render state changes for the Starship
                m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
                m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
                m_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
    
                // Set vertex shader constants for this object.
                static D3DXVECTOR4 vForceColor( 0.70f, 0.73f, 0.70f, 1.00f );
                static D3DXVECTOR4 vConstants(  1.00f, 0.50f, 1.50f, 0.15f );
                static D3DXVECTOR4 vConstants1( 0.35f, 0.00f, 0.00f, 0.00f );
                m_pd3dDevice->SetVertexShaderConstant( 5, &vForceColor, 1 );
                m_pd3dDevice->SetVertexShaderConstant( 6, &vConstants,  1 );
                m_pd3dDevice->SetVertexShaderConstant( 7, &vConstants1, 1 );
    
                // Set the Vertex and Pixel Shader
                m_pd3dDevice->SetVertexShader( m_dwFresnelVertexShader );
                m_pd3dDevice->SetPixelShader( m_dwCarBodyPixelShader );
    
                // Adds in the environment map
                m_pd3dDevice->SetTexture( 0, m_pSphereMapTexture );
                m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
                m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
                m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );

                // Draw the starship
                m_Starship.Render( XBMESH_NOTEXTURES | XBMESH_NOFVF );
    
                // If we are in edge anti-alias mode, set the render states
                // Done somewhat redundantly here for clarity.
                if( m_dwAntiAliasMode == EDGE )
                {
                    //--------------------------------------------------------------------------------
                    // NOTE:  THIS IS VERY IMPORTANT
                    //
                    // Edge anti-aliasing works by drawing an alpha edge to each triangle and
                    // line which is rendered.  For objects which are drawn WITHOUT zBuffer
                    // writes and in alpha mode Src Alpha, Dest One, like the teapot, this works 
                    // great. For any other kind of object, if you do not draw the object 
                    // un-aliased first, you will get a wire frame set of artifacts because the 
                    // alphas write to the zbuffer as well as the normal triangles.  The alphaed 
                    // edges take on the color behind them which is the background of the 
                    // backbuffer.  You need the background color to be the actual object color for
                    // that location.  This is still a big win over the normal anit-aliasing modes 
                    // because you don't need a giant backbuffer to get the aliasing you want.  
                    // Also, you can pick and choose what objects to anti-alias!
                    //
                    // To see what happens if you don't do this, comment out the m_Starship.Render
                    // call directly above the if( m_dwAntiAlias == EDGE ) statement.
                    //--------------------------------------------------------------------------------

                    // Set render states
                    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
                    m_pd3dDevice->SetRenderState( D3DRS_MULTISAMPLEANTIALIAS, FALSE );
                    m_pd3dDevice->SetRenderState( D3DRS_EDGEANTIALIAS, TRUE );

                    // Draw the starship (2nd pass with edge-antialiasing enabled)
                    m_Starship.Render( XBMESH_NOTEXTURES | XBMESH_NOFVF | XBMESH_NOMATERIALS );

                    //Reset the render states.
                    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
                    m_pd3dDevice->SetRenderState( D3DRS_MULTISAMPLEANTIALIAS, TRUE );
                    m_pd3dDevice->SetRenderState( D3DRS_EDGEANTIALIAS, FALSE );
                }

                // Restore state
                m_pd3dDevice->SetPixelShader( NULL );
                break;
            }

            case DRAW_CAR:
            {
                // Set render state changes for the car
                m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
                m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
                m_pd3dDevice->SetRenderState( D3DRS_ZFUNC, D3DCMP_LESSEQUAL );
    
                // Set the Vertex and Pixel Shader
                m_pd3dDevice->SetVertexShader( m_dwFresnelVertexShader );
                m_pd3dDevice->SetPixelShader( m_dwCarBodyPixelShader );
    
                // Adds in the environment map
                m_pd3dDevice->SetTexture( 0, m_pSphereMapTexture );
                m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
                m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
                m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );

                // Draw the car body
                {
                    static D3DXVECTOR4 vForceColor( 0.70f, 0.70f, 0.30f, 1.00f );
                    static D3DXVECTOR4 vConstants(  1.00f, 0.50f, 1.50f, 0.15f );
                    static D3DXVECTOR4 vConstants1( 0.35f, 0.00f, 0.00f, 0.00f );
                    m_pd3dDevice->SetVertexShaderConstant( 5, &vForceColor, 1 );
                    m_pd3dDevice->SetVertexShaderConstant( 6, &vConstants,  1 );
                    m_pd3dDevice->SetVertexShaderConstant( 7, &vConstants1, 1 );
    
                    // Draw the body of the car
                    m_Car.RenderSubset( CCarMesh::BODY );
    
                    // If we are in edge anti-alias mode, set the render states
                    // Done somewhat redundantly here for clarity.
                    if( m_dwAntiAliasMode == EDGE )
                    {
                        //--------------------------------------------------------------------------------
                        // NOTE:  THIS IS VERY IMPORTANT
                        //
                        // Edge anti-aliasing works by drawing an alpha edge to each triangle and
                        // line which is rendered.  For objects which are drawn WITHOUT zBuffer
                        // writes and in alpha mode Src Alpha, Dest One, like the teapot, this works 
                        // great. For any other kind of object, if you do not draw the object 
                        // un-aliased first, you will get a wire frame set of artifacts because the 
                        // alphas write to the zbuffer as well as the normal triangles.  The alphaed 
                        // edges take on the color behind them which is the background of the 
                        // backbuffer.  You need the background color to be the actual object color for
                        // that location.  This is still a big win over the normal anti-aliasing modes 
                        // because you don't need a giant backbuffer to get the aliasing you want.  
                        // Also, you can pick and choose what objects to anti-alias!
                        //
                        // To see what happens if you don't do this, comment out the m_Car.RenderSubset
                        // call directly above the if( m_dwAntiAlias == EDGE ) statement.
                        //--------------------------------------------------------------------------------

                        // Set render state
                        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
                        m_pd3dDevice->SetRenderState( D3DRS_MULTISAMPLEANTIALIAS, FALSE );
                        m_pd3dDevice->SetRenderState( D3DRS_EDGEANTIALIAS, TRUE );

                        // Draw car body (2nd pass with edge-antialiasing enabled)
                        m_Car.RenderSubset( CCarMesh::BODY );

                        // Restore render state
                        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
                        m_pd3dDevice->SetRenderState( D3DRS_MULTISAMPLEANTIALIAS, TRUE );
                        m_pd3dDevice->SetRenderState( D3DRS_EDGEANTIALIAS, FALSE );
                    }
                }

                // Draw the underside of the car - Much darker color
                {
                    static D3DXVECTOR4 vForceColor( 0.00f, 0.03f, 0.15f, 1.00f );
                    static D3DXVECTOR4 vConstants(  1.00f, 0.50f, 1.50f, 0.15f );
                    static D3DXVECTOR4 vConstants1( 0.25f, 0.00f, 0.00f, 0.00f );
                    m_pd3dDevice->SetVertexShaderConstant( 5, &vForceColor, 1 );
                    m_pd3dDevice->SetVertexShaderConstant( 6, &vConstants,  1 );
                    m_pd3dDevice->SetVertexShaderConstant( 7, &vConstants1, 1 );

                    m_Car.RenderSubset( CCarMesh::UNDERCARRIAGE );

                    // If we are in edge anti-alias mode, set the render states
                    // Done somewhat redundantly here for clarity.
                    if( m_dwAntiAliasMode == EDGE )
                    {
                        // Set render state
                        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
                        m_pd3dDevice->SetRenderState( D3DRS_MULTISAMPLEANTIALIAS, FALSE );
                        m_pd3dDevice->SetRenderState( D3DRS_EDGEANTIALIAS, TRUE );

                        // Draw the undercarriage (2nd pass with edge antialiasing enabled)
                        m_Car.RenderSubset( CCarMesh::UNDERCARRIAGE );

                        // Restore render state
                        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
                        m_pd3dDevice->SetRenderState( D3DRS_MULTISAMPLEANTIALIAS, TRUE );
                        m_pd3dDevice->SetRenderState( D3DRS_EDGEANTIALIAS, FALSE );
                    }
                }

                // Draw the Window seals of the car - Black.  No reflection
                {
                    static D3DXVECTOR4 vForceColor( 0.00f, 0.00f, 0.00f, 0.00f );
                    static D3DXVECTOR4 vConstants(  1.00f, 0.50f, 1.50f, 0.15f );
                    static D3DXVECTOR4 vConstants1( 0.15f, 0.00f, 0.00f, 0.00f );
                    m_pd3dDevice->SetVertexShaderConstant( 5, &vForceColor, 1 );
                    m_pd3dDevice->SetVertexShaderConstant( 6, &vConstants,  1 );
                    m_pd3dDevice->SetVertexShaderConstant( 7, &vConstants1, 1 );
    
                    // Draw the misc car pieces 
                    m_Car.RenderSubset( CCarMesh::WINDOWSTRIM );
                    m_Car.RenderSubset( CCarMesh::MISC );

                    // If we are in edge anti-alias mode, set the render states
                    // Done somewhat redundantly here for clarity.
                    if( m_dwAntiAliasMode == EDGE )
                    {
                        // Set render state
                        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
                        m_pd3dDevice->SetRenderState( D3DRS_MULTISAMPLEANTIALIAS, FALSE );
                        m_pd3dDevice->SetRenderState( D3DRS_EDGEANTIALIAS, TRUE );

                        // Draw the misc car pieces (2nd pass with edge antialiasing enabled)
                        m_Car.RenderSubset( CCarMesh::WINDOWSTRIM );
                        m_Car.RenderSubset( CCarMesh::MISC );

                        // Restore render state
                        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
                        m_pd3dDevice->SetRenderState( D3DRS_MULTISAMPLEANTIALIAS, TRUE );
                        m_pd3dDevice->SetRenderState( D3DRS_EDGEANTIALIAS, FALSE );
                    }
                }

                // Draw the Windows and mirrors of the car - Not drawn above so transparent
                // There is almost no difference between drawing the windows Edge anti-aliased
                // and drawing them without the edge anti-aliasing.  This is because the "edges"
                // of the windows were already drawn above with edge anti-aliasing on.  Since
                // the zbuffer values for the "edges" are already written, the other edges do
                // not contribute much to the final appearance.  Games should check what parts
                // of an object need aliasing to help performance.

                // Set the Pixel Shader for the windows and mirrors
                m_pd3dDevice->SetPixelShader( m_dwCarGlassPixelShader );
    
                // Draw mirros (mostly reflective)
                {
                    static D3DXVECTOR4 vConstants(  1.00f, 0.50f, 1.50f, 0.85f );
                    static D3DXVECTOR4 vConstants1( 1.00f, 0.00f, 0.00f, 0.00f );
                    static D3DXVECTOR4 vForceColor( 0.75f, 0.75f, 0.75f, 1.00f );
                    m_pd3dDevice->SetVertexShaderConstant( 5, &vForceColor, 1 );
                    m_pd3dDevice->SetVertexShaderConstant( 6, &vConstants,  1 );
                    m_pd3dDevice->SetVertexShaderConstant( 7, &vConstants1, 1 );

                    m_Car.RenderSubset( CCarMesh::MIRRORS );
                }

                // Draw windows (mostly transparent)
                {
                    static D3DXVECTOR4 vConstants(  1.00f, 0.50f, 1.50f, 0.15f );
                    static D3DXVECTOR4 vConstants1( 0.75f, 0.00f, 0.00f, 0.00f );
                    static D3DXVECTOR4 vForceColor( 0.75f, 0.75f, 0.75f, 1.00f );
                    m_pd3dDevice->SetVertexShaderConstant( 5, &vForceColor, 1 );
                    m_pd3dDevice->SetVertexShaderConstant( 6, &vConstants,  1 );
                    m_pd3dDevice->SetVertexShaderConstant( 7, &vConstants1, 1 );

                    m_Car.RenderSubset( CCarMesh::WINDOWS );
                }

                // Restore state
                m_pd3dDevice->SetPixelShader( NULL );
            }
            break;
        }
    }

    // Turn off the anti-alias mode so that the fonts look better
    m_pd3dDevice->SetRenderState( D3DRS_MULTISAMPLEANTIALIAS, FALSE );

    // Instead of calling Present(), we call Swap() to swap our scaled-down
    // backbuffer to the next buffer in the chain, and then write UI and other
    // visual elements that we want drawn at full resolution.
    m_pd3dDevice->Swap( D3DSWAP_COPY );

    // Between the D3DSWAP_COPY and D3DSWAP_FINISH calls, draw text and any
    // other screen space elements that we don't want affected by the viewport
    // scale.
    if( m_bDrawHelp )
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 48, 36, 0xffffffff,  L"AntiAlias" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 592, 38, 0xffffff00, m_strFrameRate, XBFONT_RIGHT );

        m_Font.DrawText( 64, 70, 0xffffffff, L"AntiAlias Mode:" );
        m_Font.DrawText( 80, 90, 0xffffff00, g_AntiAliasModes[m_dwAntiAliasMode].strDesc );

        m_Font.DrawText(  64, 120, 0xffffffff, L"BackBufferScale: " );
        if( g_AntiAliasModes[m_dwAntiAliasMode].bCanScaleBackBuffer )
        {
            WCHAR strBuffer[80];
            swprintf( strBuffer, L"%.2f", m_fBackBufferScale );
            m_Font.DrawText( 80, 140, 0xffffff00, strBuffer );
        }
        else
        {
            m_Font.DrawText( 80, 140, 0xffffff00, L"N/A" );
        }

        m_Font.End();
    }

    // After drawing non-scaled UI elements, finish the swap
    m_pd3dDevice->Swap( D3DSWAP_FINISH );

    // Scale the backbuffer, if the mode allows it
    if( g_AntiAliasModes[m_dwAntiAliasMode].bCanScaleBackBuffer )
        m_pd3dDevice->SetBackBufferScale( m_fBackBufferScale, m_fBackBufferScale );

    return S_OK;
}

