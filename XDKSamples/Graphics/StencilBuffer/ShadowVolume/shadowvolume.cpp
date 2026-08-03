//-----------------------------------------------------------------------------
// File: ShadowVolume.cpp
//
// Desc: Sample code showing how to use stencil buffers to implement shadow
//       volumes. The shadow volumes closed and are drawn using the "zfail"
//       method and depth clamping so that they are fully robust. Two techniques
//       for computing the shadow volume are shown: one using a simple fast CPU
//       based algorithm and one using the GPU. Two techniques for drawing the 
//       shadow volume are shown: the typical two-pass solution and a one-pass 
//       solution.
//
// Perf: In the two-pass case, the geometry must be transformed twice, but the
//       rasterization cost is slightly less. Since rendering shadow volumes
//       tends to be fill-bound (in other words, the transform cost is masked
//       by the fill cost), the two-pass case usually works out to be faster.
//
// Hist: 11.01.00 - New for November XDK release
//       12.15.00 - Changes for December XDK release
//       10.25.02 - Dramatic performance increase by precomputing edge lists
//       01.15.03 - Added one-pass solution using two-sided lighting
//       06.17.03 - Revised for robustness and performance.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <XBMesh.h>
#include <XBUtil.h>
#include "ShadowMesh.h"




//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_LEFTSTICK,    XBHELP_PLACEMENT_1, L"Move airplane" },
    { XBHELP_RIGHTSTICK,   XBHELP_PLACEMENT_1, L"Move light" },
    { XBHELP_A_BUTTON,     XBHELP_PLACEMENT_2, L"Use one-\npass solution" },
    { XBHELP_B_BUTTON,     XBHELP_PLACEMENT_2, L"Toggle 4x\nmultisampling" },
    { XBHELP_X_BUTTON,     XBHELP_PLACEMENT_2, L"GPU silhouette\ngeneration" },
    { XBHELP_Y_BUTTON,     XBHELP_PLACEMENT_2, L"Show\nsilhouette" },
    { XBHELP_BACK_BUTTON,  XBHELP_PLACEMENT_1, L"Display help" },
};

#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts)/sizeof(g_HelpCallouts[0]))




//-----------------------------------------------------------------------------
// External definitions and prototypes
//-----------------------------------------------------------------------------
#define FOG_COLOR 0xff0000ff




//-----------------------------------------------------------------------------
// Name: class ShadowMesh
// Desc: Combines CPU and GPU shadow meshes so that the sample can toggle 
//       between the two methods.  Normally you only do one or the other unless
//       you are trying to dynamically balance CPU/GPU load.
//-----------------------------------------------------------------------------
class ShadowMesh : public ShadowMeshCPU, public ShadowMeshGPU
{
};




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Application class. The base class provides just about all the
//       functionality we want, so we're just supplying stubs to interface with
//       the non-C++ functions of the app.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    CXBPackedResource m_xprResource;         // Packed resources for the app
    CXBFont           m_Font;                // Font class
    CXBHelp           m_Help;                // Help class
    BOOL              m_bDrawHelp;           // Whether to draw help

    CXBMesh*          m_pAirplaneObject;     // Object to render
    ShadowMeshCPU*    m_pAirplaneShadowCPU;  // Shadow-casting object (using CPU to compute shadow volume)
    ShadowMeshGPU*    m_pAirplaneShadowGPU;  // Shadow-casting object (using GPU to compute shadow volume)
    D3DXMATRIX        m_matAirplaneMatrix;

    CXBMesh*          m_pTerrainObject;
    D3DXMATRIX        m_matTerrainMatrix;
    
    BOOL              m_bDrawUsingGPU;       // Use the GPU to create the volume.
    BOOL              m_bUse4xMultiSampling; // Options
    BOOL              m_bDrawSilhouette;
    BOOL              m_bUseOnePass;

    D3DSurface        m_DepthBufferSurface;  // A surface for rendering into the depthbuffer
    
    D3DXVECTOR4       m_vLight;
    
    DWORD             m_dwShadowVolumeVS;
    DWORD             m_dwShadowVolumeGpuVS;
    DWORD             m_dwShadowVolumeTwoSideVS;
    DWORD             m_dwShadowVolumeTwoSideGpuVS;

    HRESULT RenderShadowOnePass();
    HRESULT RenderShadowTwoPass();
    HRESULT DrawShadow();

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
// Desc: Constructor
//-----------------------------------------------------------------------------
CXBoxSample::CXBoxSample()
            :CXBApplication()
{
    // Allow an unlimited framerate
    m_d3dpp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    m_pTerrainObject      = new CXBMesh();
    m_pAirplaneObject     = new CXBMesh();
    m_pAirplaneShadowCPU  = new ShadowMeshCPU();
    m_pAirplaneShadowGPU  = new ShadowMeshGPU();
    
    m_bDrawUsingGPU       = FALSE;
    m_bUseOnePass         = FALSE;
    m_bUse4xMultiSampling = TRUE;
    m_bDrawSilhouette     = FALSE;
    m_bDrawHelp           = FALSE;

    D3DXMatrixIdentity( &m_matTerrainMatrix );
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize scene objects.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    // Create the font
    if( FAILED( m_Font.Create( "Font.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create the help
    if( FAILED( m_Help.Create( "Gamepad.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create the resources
    if( FAILED( m_xprResource.Create( "Resource.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Load an object to cast the shadow
    if( FAILED( m_pAirplaneObject->Create( "Models\\Airplane.xbg", &m_xprResource ) ) )
        return XBAPPERR_MEDIANOTFOUND;
    
    // Generate data for quick shadow rendering for the mesh.
    if( FAILED( m_pAirplaneShadowCPU->Create( m_pAirplaneObject ) ) )
        return E_FAIL;
    if( FAILED( m_pAirplaneShadowGPU->Create( m_pAirplaneObject ) ) )
        return E_FAIL;

    // Load some terrain
    if( FAILED( m_pTerrainObject->Create( "Models\\Terrain.xbg", &m_xprResource ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create a rendertarget surface from the depthbuffer. We'll use this in
    // the one-pass shadow volume method so we can update the stencil buffer
    // by rendering into the blue-channel of this surface
    D3DSURFACE_DESC desc;
    m_pDepthBuffer->GetDesc( &desc );
    XGSetSurfaceHeader( desc.Width, desc.Height, D3DFMT_LIN_A8R8G8B8,
                        &m_DepthBufferSurface, m_pDepthBuffer->Data, desc.Width * 4 );

    // Create vertex shader for the CPU-based shadow algorithms
    {
        DWORD dwDecl[] =
        {
            D3DVSD_STREAM(0),
            D3DVSD_REG(D3DVSDE_POSITION,  D3DVSDT_FLOAT4),
            D3DVSD_END()
        };

        // Create vertex shader for rendering the shadow volume.
        if( FAILED( XBUtil_CreateVertexShader( "Shaders\\ShadowVolume.xvu",
                                               dwDecl, &m_dwShadowVolumeVS ) ) )
            return E_FAIL;

        // Create vertex shader for rendering the shadow volume using two sided
        // rendering tricks.
        if( FAILED( XBUtil_CreateVertexShader( "Shaders\\ShadowVolumeTwoSide.xvu",
                                               dwDecl, &m_dwShadowVolumeTwoSideVS ) ) )
            return E_FAIL;
    }

    // Create vertex shader for the GPU-based shadow algorithms
    {
        DWORD dwDecl[] =
        {
            D3DVSD_STREAM(0),
            D3DVSD_REG(0, D3DVSDT_FLOAT3),
            D3DVSD_REG(1, D3DVSDT_FLOAT4),
            D3DVSD_END()
        };

        // Create vertex shader for rendering the shadow volume using the GPU to 
        // find the silhouette.
        if( FAILED( XBUtil_CreateVertexShader( "Shaders\\ShadowVolumeGPU.xvu",
                                               dwDecl, &m_dwShadowVolumeGpuVS ) ) )
            return E_FAIL;

        // Create vertex shader for rendering the shadow volume using the GPU to 
        // find the silhouette and using two sided rendering tricks.
        if( FAILED( XBUtil_CreateVertexShader( "Shaders\\ShadowVolumeTwoSideGPU.xvu",
                                               dwDecl, &m_dwShadowVolumeTwoSideGpuVS ) ) )
            return E_FAIL;
    }

    // Set the transform matrices
    D3DXVECTOR3 vEyePt    = D3DXVECTOR3( 0.0f, 10.0f, -20.0f );
    D3DXVECTOR3 vLookatPt = D3DXVECTOR3( 0.0f,  0.0f,   0.0f  );
    D3DXVECTOR3 vUpVec    = D3DXVECTOR3( 0.0f,  1.0f,   0.0f  );
    D3DXMATRIX matWorld, matView, matProj;

    D3DXMatrixIdentity( &matWorld );
    D3DXMatrixLookAtLH( &matView, &vEyePt, &vLookatPt, &vUpVec );
    D3DXMatrixPerspectiveFovLH( &matProj, D3DX_PI/4, 4.0f/3.0f, 1.0f, 100.0f );

    m_pd3dDevice->SetTransform( D3DTS_WORLD,      &matWorld );
    m_pd3dDevice->SetTransform( D3DTS_VIEW,       &matView );
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
    // Toggle help
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Whether or not to use one-pass solution
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
        m_bUseOnePass = !m_bUseOnePass;

    // Toggle pure GPU generation of the silhouette.
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_X] )
        m_bDrawUsingGPU = !m_bDrawUsingGPU;

    // Whether or not to draw the silhouette used to build the shadowvolume
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_Y] )
        m_bDrawSilhouette = !m_bDrawSilhouette;

    // Whether or not to use 4x multisampling to increase z-only fill
    if( FALSE == m_bUseOnePass )
    {
        if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_B] )
            m_bUse4xMultiSampling = !m_bUse4xMultiSampling;
    }

    // Setup viewing position from Gamepad
    static FLOAT fRotateX1 = 0.0f;
    static FLOAT fRotateY1 = 0.0f;
    fRotateX1 += m_DefaultGamepad.fX1*m_fElapsedTime*D3DX_PI*0.5f;
    fRotateY1 += m_DefaultGamepad.fY1*m_fElapsedTime*D3DX_PI*0.5f;
    D3DXMatrixRotationYawPitchRoll( &m_matAirplaneMatrix, -fRotateX1, -fRotateY1, 0.0f );

    // Setup light position from Gamepad
    static FLOAT Lx = 0.0f;
    static FLOAT Lz = 0.0f;
    Lx = ( Lx + m_DefaultGamepad.fX2*m_fElapsedTime*12.0f ) * 0.99f;
    Lz = ( Lz + m_DefaultGamepad.fY2*m_fElapsedTime*12.0f ) * 0.99f;
    D3DXVECTOR3 vLight( Lx+0.0f, 1.0f, Lz+0.0f );

    // Move the light
    D3DLIGHT8 light;
    XBUtil_InitLight( light, D3DLIGHT_DIRECTIONAL, -vLight.x, -vLight.y, -vLight.z );
    light.Attenuation0 = 0.9f;
    m_pd3dDevice->SetLight( 0, &light );

    // Save the light positions.
    m_vLight = D3DXVECTOR4( vLight.x, vLight.y, vLight.z, 0.0f );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: RenderShadowTwoPass()
// Desc: Renders the shadow volume using the two-pass technique.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::RenderShadowTwoPass()
{
    // Disable z-buffer writes (note: z-testing still occurs), and enable the
    // stencil-buffer
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE,  FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILENABLE, TRUE );

    // Set up stencil compare function, reference value, and masks.
    // Stencil test passes if ((ref & mask) cmpfn (stencil & mask)) is true.
    // Note: since we set up the stencil-test to always pass, the STENCILFAIL
    // renderstate is really not needed.
    m_pd3dDevice->SetRenderState( D3DRS_STENCILFUNC,      D3DCMP_ALWAYS );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILPASS,      D3DSTENCILOP_KEEP );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILZFAIL,     D3DSTENCILOP_KEEP );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILFAIL,      D3DSTENCILOP_KEEP );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILMASK,      0xffffffff );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILWRITEMASK, 0xffffffff );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILREF,       0x1 );

    // Setup depth clamping.
    m_pd3dDevice->SetRenderState( D3DRS_DEPTHCLIPCONTROL, D3DDCC_CLAMP );

    // Make sure that no pixels get drawn to the frame buffer
    m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, 0 );

    // Turn on multisampling to increase fill-rate performance.  (Note: this
    // subtle trick works because 4x multisampling tells the texture units and
    // pixel shader to only run once for 4 pixels and replicate colors. The 
    // limitation doesn't apply here since we are rendering to z/stencil only,
    // and the hardware still computes accurate z- and stencil values. 
    // Theoretically, we *could* get a 4x fillrate improvement, but after 
    // taking z-compression and memory bandwidth into account, we might 
    // realistically expect a 20-30% gain.  Note that this trick doesn't 
    // really work when already using 2x multisampling to do FSAA, because 2x
    // multisampling uses a different convention for the pixel centers than is
    // used by 1x and 4x.)
    if( m_bUse4xMultiSampling )
        m_pd3dDevice->SetRenderState( D3DRS_MULTISAMPLEMODE, D3DMULTISAMPLEMODE_4X );

    // Now reverse cull order so back sides of shadow volume are written.
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CW );

    // Increment stencil buffer value on z-fail.
    m_pd3dDevice->SetRenderState( D3DRS_STENCILZFAIL, D3DSTENCILOP_INCR );

    m_pd3dDevice->SetTransform( D3DTS_WORLD, &m_matAirplaneMatrix );

    // Draw back faces (CW).
    if( m_bDrawUsingGPU )
    {
        m_pd3dDevice->SetVertexShader( m_dwShadowVolumeGpuVS );
        m_pAirplaneShadowGPU->RenderVolume( m_vLight );
    }
    else
    {
        // Re-compute the volume.
        m_pAirplaneShadowCPU->ComputeVolume( m_vLight );

        m_pd3dDevice->SetVertexShader( m_dwShadowVolumeVS );
        m_pAirplaneShadowCPU->RenderVolume( m_vLight );
    }
    
    // Restore cull order so front sides of shadow volume are written.
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );

    // Decrement stencil buffer value on z-fail.
    m_pd3dDevice->SetRenderState( D3DRS_STENCILZFAIL, D3DSTENCILOP_DECR );

    m_pd3dDevice->SetTransform( D3DTS_WORLD, &m_matAirplaneMatrix );

    // Draw front faces (CCW).
    if( m_bDrawUsingGPU )
    {
        m_pd3dDevice->SetVertexShader( m_dwShadowVolumeGpuVS );
        m_pAirplaneShadowGPU->RenderVolume( m_vLight );
    }
    else
    {
        m_pd3dDevice->SetVertexShader( m_dwShadowVolumeVS );
        m_pAirplaneShadowCPU->RenderVolume( m_vLight );
    }
    
    // Restore render states
    m_pd3dDevice->SetRenderState( D3DRS_DEPTHCLIPCONTROL, D3DDCC_CULLPRIMITIVE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE,     TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILENABLE,    FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL );
    m_pd3dDevice->SetRenderState( D3DRS_MULTISAMPLEMODE,  D3DMULTISAMPLEMODE_1X );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: RenderShadowOnePass()
// Desc: Renders the shadow volume using the one-pass technique.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::RenderShadowOnePass()
{
    // Disable z-buffer writes (note: z-testing still occurs)
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE,  FALSE );

    // Write to the blue channel only (which is the stencil value when the
    // zstencil surface is set as the render target)
    m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_BLUE );
    
    // Use 1:1 alpha blending with add signed blend op
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE,  FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND,  D3DBLEND_ONE );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );
    m_pd3dDevice->SetRenderState( D3DRS_BLENDOP,   D3DBLENDOP_ADDSIGNED );

    // Render both front and back sides in the same call using two-sided lighting
    m_pd3dDevice->SetRenderState( D3DRS_TWOSIDEDLIGHTING, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE,         D3DCULL_NONE );

    // Setup depth clamping.
    m_pd3dDevice->SetRenderState( D3DRS_DEPTHCLIPCONTROL, D3DDCC_CLAMP );

    // Set front color to "subtract 1 blue" and back color to "add 1 blue" 
    static FLOAT FrontSideColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    static FLOAT BackSideColor[]  = { 1.0f/255.0f, 1.0f/255.0f, 1.0f/255.0f, 1.0f/255.0f };
    m_pd3dDevice->SetVertexShaderConstant( 6, FrontSideColor, 1 );
    m_pd3dDevice->SetVertexShaderConstant( 7, BackSideColor, 1 );

    // Render the shadow volume into the zstencil surface
    m_pd3dDevice->SetRenderTarget( &m_DepthBufferSurface, m_pDepthBuffer );

    m_pd3dDevice->SetTransform( D3DTS_WORLD, &m_matAirplaneMatrix );

    if( m_bDrawUsingGPU )
    {
        m_pd3dDevice->SetVertexShader( m_dwShadowVolumeTwoSideGpuVS );
        m_pAirplaneShadowGPU->RenderVolume( m_vLight );
    }
    else
    {
        // Re-compute the volume.
        m_pAirplaneShadowCPU->ComputeVolume( m_vLight );

        m_pd3dDevice->SetVertexShader( m_dwShadowVolumeTwoSideVS );
        m_pAirplaneShadowCPU->RenderVolume( m_vLight );
    }

    // Restore state
    m_pd3dDevice->SetRenderTarget( m_pBackBuffer, m_pDepthBuffer );
    m_pd3dDevice->SetRenderState( D3DRS_DEPTHCLIPCONTROL, D3DDCC_CULLPRIMITIVE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE,     TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL );
    m_pd3dDevice->SetRenderState( D3DRS_BLENDOP,          D3DBLENDOP_ADD );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE,         D3DCULL_CCW );
    m_pd3dDevice->SetRenderState( D3DRS_TWOSIDEDLIGHTING, FALSE );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: DrawShadow()
// Desc: Draws a big gray polygon over scene according to the mask in the
//       stencil buffer.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::DrawShadow()
{
    DWORD dwStencilRefValue = m_bUseOnePass ? 0x80 : 0x00;

    // Set up stencil test
    m_pd3dDevice->SetRenderState( D3DRS_STENCILENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILREF,    dwStencilRefValue );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILFUNC,   D3DCMP_LESS );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILFAIL,   D3DSTENCILOP_KEEP );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILPASS,   D3DSTENCILOP_KEEP );
    
    // Set renderstates (disable z-buffering and turn on alphablending)
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE,          FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND,  D3DBLEND_ZERO );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_SRCALPHA );

    // Set the hardware to draw black, alpha-blended pixels
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TFACTOR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );
    m_pd3dDevice->SetRenderState( D3DRS_TEXTUREFACTOR, 0x7f000000 );

    // Draw the big, darkening square
    static FLOAT v[4][4] = 
    {
        {   0 - 0.5f,   0 - 0.5f, 0.0f, 1.0f },
        { 640 - 0.5f,   0 - 0.5f, 0.0f, 1.0f },
        { 640 - 0.5f, 480 - 0.5f, 0.0f, 1.0f },
        {   0 - 0.5f, 480 - 0.5f, 0.0f, 1.0f },
    };

    m_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW );
    m_pd3dDevice->DrawPrimitiveUP( D3DPT_QUADLIST, 1, v, sizeof(v[0]) );

    // Restore render states
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE,         TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILENABLE,   FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );

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
    // Clear the viewport, zbuffer, and stencil buffer
    DWORD dwStencilClearValue = m_bUseOnePass ? 0x80 : 0x00;

    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,
                         FOG_COLOR, 1.0f, dwStencilClearValue );

    // Set state
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE,        TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_DITHERENABLE,   TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SPECULARENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->LightEnable( 0, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_LIGHTING, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_AMBIENT,  0x00303030 );

    // Turn on fog
    FLOAT fFogStart =  30.0f;
    FLOAT fFogEnd   =  80.0f;
    m_pd3dDevice->SetRenderState( D3DRS_FOGENABLE,      TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_FOGCOLOR,       FOG_COLOR );
    m_pd3dDevice->SetRenderState( D3DRS_FOGTABLEMODE,   D3DFOG_LINEAR );
    m_pd3dDevice->SetRenderState( D3DRS_RANGEFOGENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_FOGSTART,       FtoDW(fFogStart) );
    m_pd3dDevice->SetRenderState( D3DRS_FOGEND,         FtoDW(fFogEnd) );

    // Draw the terrain
    m_pd3dDevice->SetTransform( D3DTS_WORLD, &m_matTerrainMatrix );
    m_pTerrainObject->Render();

    // Draw the airplane
    m_pd3dDevice->SetTransform( D3DTS_WORLD, &m_matAirplaneMatrix );
    m_pAirplaneObject->Render();

    // Turn off fog
    m_pd3dDevice->SetRenderState( D3DRS_FOGENABLE,    FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_FOGTABLEMODE, D3DFOG_NONE );

    // Render the shadow
    if( m_bUseOnePass )
    {
        // Render the shadow volume into the alpha channel as a
        // two-sided object in one pass
        RenderShadowOnePass();
    }
    else
    {
        // Render the shadow volume into the stencil buffer in two
        // separate passes for the front and back sides
        RenderShadowTwoPass();
    }

    // Draw a shadow using the stencil buffer as a mask
    DrawShadow();

    // For debuging, draw the silhouette used to build the shadow volume 
    if( m_bDrawSilhouette )
    {
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TFACTOR );
        m_pd3dDevice->SetRenderState( D3DRS_TEXTUREFACTOR, 0xffffffff );
        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
        m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );

        m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_WIREFRAME );
        
        m_pd3dDevice->SetTransform( D3DTS_WORLD, &m_matAirplaneMatrix );

        // Draw the volume.
        if( m_bDrawUsingGPU )
        {
            m_pd3dDevice->SetVertexShader( m_dwShadowVolumeGpuVS );
            m_pAirplaneShadowGPU->RenderVolume( m_vLight );
        }
        else
        {
            // Re-compute the volume.
            m_pAirplaneShadowCPU->ComputeVolume( m_vLight );

            m_pd3dDevice->SetVertexShader( m_dwShadowVolumeVS );
            m_pAirplaneShadowCPU->RenderVolume( m_vLight );
        }

        m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
    }

    // Show title, frame rate, and help
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 48, 36, 0xffffffff,  L"ShadowVolume" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 592, 38, 0xffffff00, m_strFrameRate, XBFONT_RIGHT );
        
        m_Font.DrawText( 64, 75, 0xffffffff, L"Computing silhouette: " );
        m_Font.DrawText( 0xffffff00, m_bDrawUsingGPU ? L"On GPU" : L"On CPU" );
        
        m_Font.DrawText( 64, 100, 0xffffffff, L"Rendering technique: " );
        m_Font.DrawText( 0xffffff00, m_bUseOnePass ? L"One pass (slower)" : L"Two passes" );

        if( FALSE == m_bUseOnePass )
        {
            m_Font.DrawText( 64, 125, 0xffffffff, L"Multisampling: " );
            m_Font.DrawText( 0xffffff00, m_bUse4xMultiSampling ? L"4x" : L"1x (slower)" );
        }

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}




