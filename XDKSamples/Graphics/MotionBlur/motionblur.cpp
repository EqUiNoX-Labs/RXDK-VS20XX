//-----------------------------------------------------------------------------
// File: MotionBlur.cpp
//
// Desc: Shows one way of doing motion blur on the Xbox
//
//       For a cool blur experience, zoom in on the rotating plane and set
//       NumPasses = 8 and PosFactor = 0.9
//
// Hist: 11.08.02 - Cleaned up for December XDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <XBMesh.h>
#include <XBResource.h>


//-----------------------------------------------------------------------------
// Help screen definitions
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] =
{
    { XBHELP_LEFTSTICK,    XBHELP_PLACEMENT_1, L"Camera X, Z" },
    { XBHELP_RIGHTSTICK,   XBHELP_PLACEMENT_1, L"Camera Y" },
    { XBHELP_A_BUTTON,     XBHELP_PLACEMENT_2, L"Decrease\npasses" },
    { XBHELP_B_BUTTON,     XBHELP_PLACEMENT_2, L"Increase\npasses" },
    { XBHELP_X_BUTTON,     XBHELP_PLACEMENT_2, L"Decrease\nPosFactor" },
    { XBHELP_Y_BUTTON,     XBHELP_PLACEMENT_2, L"Increase\nPosFactor" },
    { XBHELP_BLACK_BUTTON, XBHELP_PLACEMENT_1, L"Toggle blur" },
    { XBHELP_BACK_BUTTON,  XBHELP_PLACEMENT_1, L"Display\nhelp" },
    { XBHELP_START_BUTTON, XBHELP_PLACEMENT_1, L"Pause" },
};

const DWORD NUM_HELP_CALLOUTS = sizeof(g_HelpCallouts)/sizeof( g_HelpCallouts[0]);




//-----------------------------------------------------------------------------
// Name: class CXBlurObject
// Desc: CXObject with additions to facilitate motion blur.
//-----------------------------------------------------------------------------
class CXBlurObject
{
public:
    CXBMesh*    m_pXBMesh;          // XBG mesh

    D3DXVECTOR3 m_vLastPosition;
    D3DXVECTOR3 m_vLastRotation;
    D3DXVECTOR3 m_vPosStep;
    D3DXVECTOR3 m_vRotStep;
    D3DXVECTOR3 m_vPosSave;
    D3DXVECTOR3 m_vRotSave;

    D3DXVECTOR3 m_vPosition;        // Position
    D3DXVECTOR3 m_vRotation;        // Rotation

    FLOAT       m_fNumSteps;        // # of steps between last & current position
    FLOAT       m_fPosBlend;        // Last position/rotation blend factor

public:
    CXBlurObject()
    {
        m_pXBMesh       = NULL;
        m_vPosition     = D3DXVECTOR3(0.0f,0.0f,0.0f);
        m_vRotation     = D3DXVECTOR3(0.0f,0.0f,0.0f);
        m_vLastPosition = D3DXVECTOR3(0.0f,0.0f,0.0f);
        m_vLastRotation = D3DXVECTOR3(0.0f,0.0f,0.0f);
        m_fNumSteps     = 4.0f;
        m_fPosBlend     = 0.5f;
    }

    // Begin the blue process
    void BeginBlur()
    {
        m_vPosSave  = m_vPosition;
        m_vRotSave  = m_vRotation;
        m_vPosStep  = (m_vPosition-m_vLastPosition)/(m_fNumSteps-1.0f);
        m_vRotStep  = (m_vRotation-m_vLastRotation)/(m_fNumSteps-1.0f);
        m_vPosition = m_vLastPosition;
        m_vRotation = m_vLastRotation;
    }

    void AdvanceBlurStep()
    {
        m_vPosition += m_vPosStep;
        m_vRotation += m_vRotStep;
    }

    // End the blur and adjust lastpos/rot
    void EndBlur()
    {
        m_vLastPosition = m_fPosBlend*m_vLastPosition + (1.0f-m_fPosBlend)*m_vPosition;
        m_vLastRotation = m_fPosBlend*m_vLastRotation + (1.0f-m_fPosBlend)*m_vRotation;
        m_vPosition     = m_vPosSave;
        m_vRotation     = m_vRotSave;
    }

    // Render the object
    VOID Render()
    {
        // Set the position and orientation of the object
        D3DXMATRIX matRotate, matTrans, mat;
        D3DXMatrixRotationYawPitchRoll( &matRotate, m_vRotation.y, m_vRotation.x, m_vRotation.z );
        D3DXMatrixTranslation( &matTrans, m_vPosition.x, m_vPosition.y, m_vPosition.z );
        D3DXMatrixMultiply( &mat, &matRotate, &matTrans );
        D3DDevice::SetTransform( D3DTS_WORLD, &mat );

        // Render the object
        m_pXBMesh->Render();
    }
};




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    CXBPackedResource  m_xprResource;
    CXBFont            m_Font;
    CXBHelp            m_Help;
    BOOL               m_bDrawHelp;
    
    D3DXVECTOR3        m_vCameraPos;
    D3DXVECTOR3        m_vCameraRot;

    CXBlurObject       m_Plane1Obj;
    CXBlurObject       m_Plane2Obj;

    CXBMesh            m_SkyBox;
    CXBMesh            m_Airplane;
    CXBMesh            m_Sphere;

    LPDIRECT3DTEXTURE8 m_pBlendTexture;
    LPDIRECT3DTEXTURE8 m_pCompositeTexture;
    LPDIRECT3DSURFACE8 m_pBlendSurface;
    LPDIRECT3DSURFACE8 m_pCompositeSurface;
    LPDIRECT3DSURFACE8 m_pBlendZSurface;

    DWORD              m_bBlur;
    DWORD              m_dwNumPasses;
    FLOAT              m_fPosFactor;

    HRESULT RenderScreenTexture( LPDIRECT3DTEXTURE8 pTexture, DWORD dwTFactor );
    HRESULT AddTexture( LPDIRECT3DTEXTURE8 pTexture );

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
    // Allow unlimited frame rate
    m_d3dpp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    // Initialize our stuff
    m_bDrawHelp   = FALSE;

    m_vCameraPos  = D3DXVECTOR3( 0.0f, 0.0f, -50.0f);
    m_vCameraRot  = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );

    m_bBlur       = TRUE;
    m_dwNumPasses = 4;
    m_fPosFactor  = 0.5f;


m_d3dpp.AutoDepthStencilFormat = D3DFMT_LIN_D16;
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize device-dependant objects.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    HRESULT hr;

    // Create a font
    if( FAILED( hr = m_Font.Create( "Font.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Initialize the help system
    if( FAILED( hr = m_Help.Create( "Gamepad.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Load the packed resource
    if( FAILED( m_xprResource.Create( "Resource.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Set projection transform
    D3DXMATRIX matProj;
    D3DXMatrixPerspectiveFovLH( &matProj, D3DX_PI/4, 640.0f/480.0f, 0.1f, 100.0f );
    m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &matProj );

    // Create a skyboc
    m_SkyBox.Create( "Models\\Skybox.xbg", &m_xprResource );

    // Initialize the airplanes
    m_Airplane.Create( "Models\\Airplane.xbg", &m_xprResource );

    m_Plane1Obj.m_pXBMesh   = &m_Airplane;
    m_Plane1Obj.m_vPosition = D3DXVECTOR3( 20.0f, 5.0f, 0.0f );
    m_Plane2Obj.m_pXBMesh   = &m_Airplane;
    m_Plane2Obj.m_vPosition = D3DXVECTOR3( 0.0f, 0.0f, -10.0f );

    // Put an object in the middle of the scene to prove zbuffering works
    m_Sphere.Create( "Models\\Sphere.xbg", &m_xprResource );
    D3DMATRIX matTransform = { 4.0f, 0.0f, 0.0f, 0.0f,
                               0.0f, 4.0f, 0.0f, 0.0f,
                               0.0f, 0.0f, 4.0f, 0.0f,
                               0.0f, 5.0f, 0.0f, 1.0f };
    m_Sphere.GetFrame(0)->m_matTransform = matTransform;

    // Create the texture used for blending
    m_pd3dDevice->CreateTexture( 640, 480, 1, 0, D3DFMT_LIN_A8R8G8B8, D3DPOOL_DEFAULT, &m_pBlendTexture );

    // Create the z buffer for blending
//    m_pd3dDevice->CreateDepthStencilSurface( 640, 480, D3DFMT_LIN_D24S8, D3DMULTISAMPLE_NONE , &m_pBlendZSurface );
m_pd3dDevice->CreateDepthStencilSurface( 640, 480, D3DFMT_LIN_D16, D3DMULTISAMPLE_NONE , &m_pBlendZSurface );

    // Create the compositing texture
    m_pd3dDevice->CreateTexture( 640, 480, 1, 0, D3DFMT_LIN_A8R8G8B8, D3DPOOL_DEFAULT, &m_pCompositeTexture );

    // Clear them out
    m_pBlendTexture->GetSurfaceLevel( 0, &m_pBlendSurface );
    m_pd3dDevice->SetRenderTarget( m_pBlendSurface, m_pBlendZSurface );
//    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL, 0, 1.0f, 0L );
    m_pBlendSurface->Release();

    m_pd3dDevice->SetRenderTarget( m_pBackBuffer, m_pDepthBuffer );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    m_Plane1Obj.m_vPosition.x =  20.0f * cosf( m_Plane1Obj.m_vRotation.y );
    m_Plane1Obj.m_vRotation.y += D3DX_PI*m_fElapsedAppTime;
    m_Plane1Obj.m_vPosition.z = -20.0f * sinf( m_Plane1Obj.m_vRotation.y );

    m_Plane2Obj.m_vRotation.x -= 1*D3DX_PI*m_fElapsedAppTime;
    m_Plane2Obj.m_vRotation.y -= 2*D3DX_PI*m_fElapsedAppTime;

    // Toggle help
    if( m_DefaultGamepad.wPressedButtons&XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Adjust # of passes
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_B])
        m_dwNumPasses++;
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_A])
    {
        m_dwNumPasses--;
        if( m_dwNumPasses<2 )
            m_dwNumPasses = 2;
    }
    m_Plane1Obj.m_fNumSteps = (float)m_dwNumPasses;
    m_Plane2Obj.m_fNumSteps = (float)m_dwNumPasses;

    // Adjust position factor
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_Y])
    {
        m_fPosFactor += 0.1f;
        if( m_fPosFactor>0.9f )
            m_fPosFactor = 0.9f;
    }
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_X])
    {
        m_fPosFactor -= 0.1f;
        if( m_fPosFactor<0.0f )
            m_fPosFactor = 0.0f;
    }
    m_Plane1Obj.m_fPosBlend = m_fPosFactor;
    m_Plane2Obj.m_fPosBlend = m_fPosFactor;

    // Toggle blur
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_BLACK])
        m_bBlur = !m_bBlur;

    // Adjust the camera
    D3DXMATRIX matView;
    m_vCameraPos.x += m_DefaultGamepad.fX1*m_fElapsedTime*8.0f;
    m_vCameraPos.y += m_DefaultGamepad.fY2*m_fElapsedTime*8.0f;
    m_vCameraPos.z += m_DefaultGamepad.fY1*m_fElapsedTime*8.0f;

    D3DXMatrixTranslation( &matView, -m_vCameraPos.x, -m_vCameraPos.y, -m_vCameraPos.z);
    m_pd3dDevice->SetTransform( D3DTS_VIEW, &matView );

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
    DWORD i, dwTfactor;

    // Clear the viewport
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL, 
                         0, 1.0f, 0L );

    // Restore state that text clobbers
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE,  FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, D3DZB_TRUE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );

    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );

    // Render the Skybox
    {
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP );
        
        // Center view matrix for skybox and disable zbuffer
        D3DXMATRIX matView, matViewSave;

        D3DXMatrixIdentity(&matView);
        m_pd3dDevice->SetTransform( D3DTS_WORLD,      &matView );

        m_pd3dDevice->GetTransform( D3DTS_VIEW,      &matViewSave );
        matView = matViewSave;
        matView._41 = 0.0f; matView._42 = -0.0f; matView._43 = 0.0f;
        m_pd3dDevice->SetTransform( D3DTS_VIEW,      &matView );
        m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );

        // Render the skybox
        m_SkyBox.Render();

        // Restore the render states
        m_pd3dDevice->SetTransform( D3DTS_VIEW,      &matViewSave );
        m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    }

    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU,  D3DTADDRESS_WRAP );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV,  D3DTADDRESS_WRAP );

    // Render blocker object to prove that z buffering works
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
    m_Sphere.Render();

    if( !m_bBlur )
    {
        m_Plane1Obj.Render();
        m_Plane2Obj.m_vRotation.y += D3DX_PI;
        m_Plane2Obj.Render();
        m_Plane2Obj.m_vRotation.y -= D3DX_PI;
    }
    else
    {
        // Begin the blur for each object
        m_Plane1Obj.BeginBlur();
        m_Plane2Obj.BeginBlur();

        // Clear out the compositing surface
        m_pCompositeTexture->GetSurfaceLevel( 0, &m_pCompositeSurface );
        m_pd3dDevice->SetRenderTarget( m_pCompositeSurface, NULL );
        m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0L );

        // Adjust the tfactor to contribute the right amount of color/alpha
        // per pass
        dwTfactor = 255/m_dwNumPasses;
        dwTfactor |= (dwTfactor<<8);
        dwTfactor |= (dwTfactor<<8);
        dwTfactor |= (dwTfactor<<8);

        // Do the blur. NOTE: make sure m_dwNumPasses matches the m_fNumSteps
        // value in the blur objects
        for( i=0; i<m_dwNumPasses; i++ )
        {
            m_pd3dDevice->SetRenderState( D3DRS_TEXTUREFACTOR, dwTfactor );
            m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
            m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
            m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_TFACTOR );
            m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1 );
            m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR );

            m_pBlendTexture->GetSurfaceLevel( 0, &m_pBlendSurface );
            m_pd3dDevice->SetRenderTarget( m_pBlendSurface, m_pBlendZSurface );
            
            // Clear the blend surface
            m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0L );

            // Copy the screen z buffer
            m_pd3dDevice->CopyRects( m_pDepthBuffer, NULL, 0, m_pBlendZSurface, NULL );

            // Render the blurred objects
            m_Plane1Obj.Render();
            m_Plane2Obj.Render();
            m_pBlendSurface->Release();

            // Advance the objects to the next blur step
            m_Plane1Obj.AdvanceBlurStep();
            m_Plane2Obj.AdvanceBlurStep();

            // Add this layer to the composite surface
            m_pd3dDevice->SetRenderTarget( m_pCompositeSurface, NULL );
            AddTexture( m_pBlendTexture );
        }

        // End the blur for each object
        m_Plane1Obj.EndBlur();
        m_Plane2Obj.EndBlur();

        // Release our compositing surface
        m_pCompositeSurface->Release();

        // Blend the blur texture with the existing screen
        m_pd3dDevice->SetRenderTarget( m_pBackBuffer, m_pDepthBuffer );
        RenderScreenTexture( m_pCompositeTexture, 0 );
    }

    // Show title, frame rate, and help
    if( m_bDrawHelp )
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 48, 36, 0xffffffff,  L"MotionBlur" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 592, 38, 0xffffff00, m_strFrameRate, XBFONT_RIGHT );

        WCHAR str[80];
        swprintf( str, L"%d", m_dwNumPasses );
        m_Font.DrawText( 64, 70, 0xffffffff, L"NumPasses: " );
        m_Font.DrawText( 0xffffff00, str );
        swprintf( str, L"%3.2f", m_fPosFactor );
        m_Font.DrawText( 64, 95, 0xffffffff, L"PosFactor: " );
        m_Font.DrawText( 0xffffff00, str );

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: RenderScreenTexture()
// Desc: This function renders the motion blurred screen texture to the screen.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::RenderScreenTexture( LPDIRECT3DTEXTURE8 pTexture, DWORD dwAlphaRef )
{
    static FLOAT vQuadVertices[4][6] =
    {
        {  -0.5f,  -0.5f, 1.0f, 1.0f,    0.0f,   0.0f },
        { 639.5f,  -0.5f, 1.0f, 1.0f,  640.0f,   0.0f },
        {  -0.5f, 479.5f, 1.0f, 1.0f,    0.0f, 480.0f },
        { 639.5f, 479.5f, 1.0f, 1.0f,  640.0f, 480.0f }
    };

    // Set states
    g_pd3dDevice->SetTexture( 0, pTexture );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU,  D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV,  D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE ); 

    // If using modulate, you need to set alpharef to around 0xc0. Otherwise
    // you will see major streaks behind the objects
    if( dwAlphaRef>0 )
    {
        m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, TRUE );
        m_pd3dDevice->SetRenderState( D3DRS_ALPHAREF,  dwAlphaRef&0xff );
        m_pd3dDevice->SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL );
    }

    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );

    // Draw the textured quad
    m_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW|D3DFVF_TEX1 );
    m_pd3dDevice->DrawPrimitiveUP( D3DPT_QUADSTRIP, 1, vQuadVertices, 6*sizeof(FLOAT) );

    // Reset render states
    m_pd3dDevice->SetTexture( 0, NULL );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE,  FALSE );
    g_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE ); 
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: AddTexture()
// Desc: This function adds the "blend" texture to the "composite" texture.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::AddTexture( LPDIRECT3DTEXTURE8 pTexture )
{
    static FLOAT vQuadVertices[4][6] =
    {
        {  -0.5f,  -0.5f, 1.0f, 1.0f,    0.0f,   0.0f },
        { 639.5f,  -0.5f, 1.0f, 1.0f,  640.0f,   0.0f },
        {  -0.5f, 479.5f, 1.0f, 1.0f,    0.0f, 480.0f },
        { 639.5f, 479.5f, 1.0f, 1.0f,  640.0f, 480.0f }
    };

    // Set states
    g_pd3dDevice->SetTexture( 0, pTexture );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU,  D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV,  D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE ); 

    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND,  D3DBLEND_ONE );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );

    // Draw the textured quad
    m_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW|D3DFVF_TEX1 );
    m_pd3dDevice->DrawPrimitiveUP( D3DPT_QUADSTRIP, 1, vQuadVertices, 6*sizeof(FLOAT) );

    // Reset render states
    m_pd3dDevice->SetTexture( 0, NULL );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE,  FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE ); 

    return S_OK;
}

