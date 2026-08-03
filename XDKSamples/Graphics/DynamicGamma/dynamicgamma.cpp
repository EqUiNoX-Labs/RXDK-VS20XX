//-----------------------------------------------------------------------------
// File: DynamicGamma.cpp
//
// Desc: Example code showing how to use control the Xbox gamma ramp
//       dynamically
//
// Hist: 04.01.03 - New for the April 2003 XDK Release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBMesh.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <XBResource.h>
#include <XBUtil.h>
#include <xgraphics.h>

// Dynamic gamma controller
#include "DynamicGammaController.h"




//-----------------------------------------------------------------------------
// Callouts for labeling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_RIGHTSTICK,   XBHELP_PLACEMENT_1, L"Move light" },
    { XBHELP_LEFTSTICK,    XBHELP_PLACEMENT_1, L"Move camera" },
    { XBHELP_WHITE_BUTTON, XBHELP_PLACEMENT_2, L"Enable dynamic\ngamma"  },
    { XBHELP_Y_BUTTON,     XBHELP_PLACEMENT_2, L"Turn on\nCrypt light"},
    { XBHELP_DPAD,         XBHELP_PLACEMENT_2, L"Change\nparameters" },
    { XBHELP_BACK_BUTTON,  XBHELP_PLACEMENT_1, L"Display help" },
};
#define NUM_HELP_CALLOUTS ( sizeof(g_HelpCallouts) / sizeof(XBHELP_CALLOUT ) )




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    CXBFont             m_Font;           // System font
    CXBHelp             m_Help;           // Application help
    BOOL                m_bDrawHelp;      // Whether to display help

    CXBPackedResource   m_xprResource;    // Packed resources (textures)
    
    CXBMesh             m_SkyBoxObject;   // The skybox geometry
    D3DXMATRIX          m_matSkyBox;      // Matrix to orient skybox

    CXBMesh             m_TerrainObject;  // The terrain geometry

    D3DTexture*         m_pHUDTexture;    // The HUD

    D3DXMATRIX          m_matWorld;       // World, view, and
    D3DXMATRIX          m_matView;        // projection matrices
    D3DXMATRIX          m_matProj;

    D3DXVECTOR3         m_FlashLightDir;  // Crypt light direction
    BOOL                m_bCryptLight;    // Toggle crypt light
    
    enum ADJUST 
    {   
        ADJUST_BIAS,
        ADJUST_DARKCLAMP,
        ADJUST_LIGHTCLAMP,
        ADJUST_DARKDT,
        ADJUST_LIGHTDT,
        ADJUST_CONTRASTDT,
        ADJUST_DARKTHRESHOLD,
        ADJUST_LIGHTTHRESHOLD,
        MAX_ADJUST,
    };
    ADJUST                 m_Adjust;           // Current parameter for adjustment

    DynamicGammaController m_DynamicGammaCtrl; // The dynamic gamma controller
    BOOL                   m_bUseDynamicGamma; // Toggle effect

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
    // Allow unlimited frame rate
    m_d3dpp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
}





//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: 
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

    // Load the geometry models
    if( FAILED( m_SkyBoxObject.Create( "Models\\CryptSkyBox.xbg", &m_xprResource ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    if( FAILED( m_TerrainObject.Create( "Models\\Crypt.xbg", &m_xprResource ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    m_pHUDTexture = m_xprResource.GetTexture( "Hud.tga" );

    // Set the matrices
    D3DXMatrixIdentity( &m_matWorld );
    D3DXMatrixPerspectiveFovLH( &m_matProj, D3DX_PI/3, 4.0f/3.0f, 0.2f, 75.0f );
    m_pd3dDevice->SetTransform( D3DTS_WORLD,      &m_matWorld );
    m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &m_matProj );

    // Init flags
    m_bDrawHelp        = FALSE;
    m_bCryptLight      = FALSE;
    m_bUseDynamicGamma = TRUE;
    m_Adjust           = ADJUST_BIAS;
    
    // Initialize dynamic gamma 
    m_DynamicGammaCtrl.Initialize();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    // Toggle flags
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_WHITE] )
        m_bUseDynamicGamma = !m_bUseDynamicGamma;

    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_Y] )
        m_bCryptLight = !m_bCryptLight;

    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
    {
        if( m_Adjust == ADJUST(MAX_ADJUST - 1) )
            m_Adjust = ADJUST( 0 );
        else
            m_Adjust = ADJUST( m_Adjust + 1 );
        
    }
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
    {
        if( m_Adjust == ADJUST( 0 ) )
            m_Adjust = ADJUST( MAX_ADJUST - 1);
        else
            m_Adjust = ADJUST( m_Adjust - 1 );
    
    }
    

    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Adjust dynamic gamma parameters
    switch( m_Adjust )
    {
        case ADJUST_BIAS:
        {
            if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
                m_DynamicGammaCtrl.m_fBias += 0.1f;
            if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
                m_DynamicGammaCtrl.m_fBias -= 0.1f;
            break;
        }

        case ADJUST_DARKCLAMP:
        {
            if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
                m_DynamicGammaCtrl.m_fDarkClamp += 0.10f;
            if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
                m_DynamicGammaCtrl.m_fDarkClamp -= 0.10f;

            Clamp( &m_DynamicGammaCtrl.m_fDarkClamp, 0.0f, 1.0f );
            break;
        }

        case ADJUST_LIGHTCLAMP:
        {
            if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
                m_DynamicGammaCtrl.m_fLightClamp += 0.10f;
            if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
                m_DynamicGammaCtrl.m_fLightClamp -= 0.10f;

            Clamp( &m_DynamicGammaCtrl.m_fLightClamp, 0.0f, 1.0f );
            break;
        }

        case ADJUST_DARKDT:
        {
            if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
                m_DynamicGammaCtrl.m_fDarkAdjustDt += 0.01f;
            if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
                m_DynamicGammaCtrl.m_fDarkAdjustDt -= 0.01f;

            Clamp( &m_DynamicGammaCtrl.m_fDarkAdjustDt, 0.0f, 1.0f );
            break;
        }
        
        case ADJUST_LIGHTDT:
        {
            if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
                m_DynamicGammaCtrl.m_fLightAdjustDt += 0.01f;
            if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
                m_DynamicGammaCtrl.m_fLightAdjustDt -= 0.01f;

            Clamp( &m_DynamicGammaCtrl.m_fLightAdjustDt, 0.0f, 1.0f );
            break;
        }

        case ADJUST_CONTRASTDT:
        {
            if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
                m_DynamicGammaCtrl.m_fContrastAdjustDt += 0.01f;
            if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
                m_DynamicGammaCtrl.m_fContrastAdjustDt -= 0.01f;

            Clamp( &m_DynamicGammaCtrl.m_fContrastAdjustDt, 0.0f, 1.0f );
            break;
        }

        case ADJUST_DARKTHRESHOLD:
        {
            if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
                m_DynamicGammaCtrl.m_fDarkThreshold += 0.01f;
            if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
                m_DynamicGammaCtrl.m_fDarkThreshold -= 0.01f;

            Clamp( &m_DynamicGammaCtrl.m_fDarkThreshold, 0.0f, 1.0f );
            break;
        }
        case ADJUST_LIGHTTHRESHOLD:
        {
            if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
                m_DynamicGammaCtrl.m_fLightThreshold += 0.01f;
            if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
                m_DynamicGammaCtrl.m_fLightThreshold -= 0.01f;

            Clamp( &m_DynamicGammaCtrl.m_fLightThreshold, 0.0f, 1.0f );
            break;
        }
    }

        
    // Rotate and position the camera with the gamepad
    static FLOAT fViewAngle =  6.299f;
    static FLOAT fCameraX   =  0.0f;
    static FLOAT fCameraY   =  3.00f;
    static FLOAT fCameraZ   =  -18.97f;
    fViewAngle += 2.0f * m_DefaultGamepad.fX1 * m_fElapsedTime;
    fCameraZ   += 4.0f * m_DefaultGamepad.fY1 * m_fElapsedTime * cosf( fViewAngle );
    Clamp( &fCameraZ, -20.0f, -1.0f );
    
    // Set the view transform
    D3DXVECTOR3 from = D3DXVECTOR3( fCameraX, fCameraY, fCameraZ );
    D3DXVECTOR3 at   = D3DXVECTOR3( sinf(fViewAngle), 0.0f, cosf(fViewAngle) ) + from;
    D3DXVECTOR3 up   = D3DXVECTOR3( 0.0f, 1.0f, 0.0f );
    D3DXMatrixLookAtLH( &m_matView, &from, &at, &up );
    m_pd3dDevice->SetTransform( D3DTS_VIEW, &m_matView );
    
    // Animate the door
    static FLOAT m_fRot = 0.0f;
    if( fCameraZ < -6.0f && fCameraZ > -14.0f )
        m_fRot += 2.5f * m_fElapsedTime;
    else
        m_fRot -= 2.5f * m_fElapsedTime;
    Clamp( &m_fRot, 0.0f, D3DX_PI/2.0f );
    D3DXMATRIX matTransOrg, matRot, matTransWorld;
    D3DXMatrixTranslation( &matTransOrg, 1.0f, 0.0f, 9.45f );
    D3DXMatrixTranslation( &matTransWorld, -1.0f, 0.0f, -9.45f );
    D3DXMatrixRotationY( &matRot, m_fRot );
    m_TerrainObject.GetFrame(6)->m_matTransform = matTransOrg * matRot * matTransWorld;

    // Set the skybox view transform (which retains the view orientation,
    // but not the translation)
    m_matSkyBox     = m_matView;
    m_matSkyBox._41 = 0.0f; 
    m_matSkyBox._42 = 0.0f; 
    m_matSkyBox._43 = 0.0f;

    // Position and orient the light
    static FLOAT fLightPhi   = +4.67f;
    static FLOAT fLightTheta = -.4f;
    if( m_bCryptLight )
    {
        fLightPhi   += +3.0f * m_DefaultGamepad.fX2 * m_fElapsedTime;
        fLightTheta += +3.0f * m_DefaultGamepad.fY2 * m_fElapsedTime;
        fLightTheta = min( +D3DX_PI/2, max( fLightTheta, -D3DX_PI/2 ) );
    }
    m_FlashLightDir.x = +cosf( fLightPhi );
    m_FlashLightDir.y = +sinf( fLightTheta );
    m_FlashLightDir.z = -sinf( fLightPhi );
    D3DXVec3Normalize( &m_FlashLightDir, &m_FlashLightDir );
    
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Render()
{
    // Clear the viewport
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,
                         0x00000000, 1.0f, 0L );

    // Render the skybox
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MIPFILTER, D3DTEXF_NONE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE,  FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_LIGHTING, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE,  FALSE );
    m_pd3dDevice->SetTransform( D3DTS_WORLD, &m_matWorld );
    m_pd3dDevice->SetTransform( D3DTS_VIEW,  &m_matSkyBox );
    m_SkyBoxObject.Render();

    // Set default state
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE,  TRUE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetTransform( D3DTS_VIEW, &m_matView );

    // Draw terrain
    D3DLIGHT8 Light;
    m_pd3dDevice->SetRenderState( D3DRS_LIGHTING, TRUE );
    m_pd3dDevice->LightEnable( 0, m_bCryptLight );

    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );

    ZeroMemory( &Light, sizeof(D3DLIGHT8) );
    Light.Type         = D3DLIGHT_SPOT;
    Light.Falloff      = 1.0f;
    Light.Theta        = D3DX_PI/2.0f;
    Light.Phi          = D3DX_PI/1.5f;
    Light.Diffuse      = D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f );
    Light.Position     = D3DXVECTOR3( 0.0f, 4.0f, 0.0f );
    Light.Direction    = m_FlashLightDir;
    Light.Range        = 10.0f;
    Light.Attenuation0 = 0.0f;
    Light.Attenuation1 = 0.0f;
    Light.Attenuation2 = 0.2f;
    m_pd3dDevice->SetLight( 0, &Light);
    
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetRenderState( D3DRS_AMBIENT, 0x10101010 );
    m_TerrainObject.Render();

    // Current stage in dynamic gamma calculation and update.
    // Note that we don't get luminance and calculate gamma in the same
    // frame to avoid the lock stall when reading the luminance texture.
    enum STAGE
    {
        STAGE_GET_LUM,
        STAGE_GET_HIST,
        STAGE_SET_GAMMA,
        MAX_STAGE
    };

    static STAGE CurrentStage = STAGE_GET_LUM;
    FLOAT fCurrent = timeGetTime()/1000.0f;
    static FLOAT fLast = fCurrent;
    
    switch( CurrentStage )
    {
        case STAGE_GET_LUM:
        {
            // Make D3DTexture wrapper around current render target
            D3DSURFACE_DESC desc;
            m_pBackBuffer->GetDesc( &desc );
            D3DTexture RenderTargetTexture;
            ZeroMemory( &RenderTargetTexture, sizeof(RenderTargetTexture) );
            XGSetTextureHeader( desc.Width, desc.Height, 1, 0, desc.Format, 0,
                                &RenderTargetTexture, m_pBackBuffer->Data,
                                desc.Width * 4 );

            // Get luminance texture
            m_DynamicGammaCtrl.GetLuminance( &RenderTargetTexture, 16, 16 );
            
            // Advance to the next stage
            CurrentStage = STAGE_GET_HIST;
            break;
        }
    
        case STAGE_GET_HIST:
        {
            // Calculate histogram from Luminance texture
            if( FALSE == m_DynamicGammaCtrl.IsRenderingLuminance() )
            {
                m_DynamicGammaCtrl.GetHistogram();
                FLOAT dt = fCurrent - fLast;
                fLast = fCurrent;

                // Update gamma values
                m_DynamicGammaCtrl.UpdateGammaValues( dt );

                // Advance to the next stage
                CurrentStage = STAGE_SET_GAMMA;
            }
            break;
        }

        case STAGE_SET_GAMMA:
        {
            // Update gamma ramps
            if( TRUE == m_bUseDynamicGamma && FALSE == m_bDrawHelp )
            {
                m_DynamicGammaCtrl.SetGammaRamp();
            }
            else // If not using the effect, reset the gamma ramp
            {
                D3DGAMMARAMP Ramp;
                for( UINT i = 0; i < 256; i++ )
                {
                    Ramp.red[i] = Ramp.green[i] = Ramp.blue[i] = BYTE(i);
                }
                m_pd3dDevice->SetGammaRamp( 0, &Ramp );
            }

            // Advance to the next stage
            CurrentStage = STAGE_GET_LUM;
            break;
        }
    }
    
    // Draw screenspace elements (unless help is shown)
    if( FALSE == m_bDrawHelp )
    {
        // Draw HUD
        struct HudVerts
        {
            D3DXVECTOR4 pos;
            D3DXVECTOR2 tex;
        };
        
        FLOAT fHUDWidth  = 125.0f;
        FLOAT fHUDHeight = 125.0f;
        FLOAT fHUDX      = 640/2.0f - fHUDWidth/2.0f;
        FLOAT fHUDY      = 480.0f - fHUDHeight - 35.0f;
        HudVerts Verts[4];
        Verts[0].pos = D3DXVECTOR4( fHUDX, fHUDY, 0.0f, 1.0f );
        Verts[0].tex = D3DXVECTOR2( 0.0f, 0.0f );
        Verts[1].pos = D3DXVECTOR4( fHUDX + fHUDWidth, fHUDY, 0.0f, 1.0f );
        Verts[1].tex = D3DXVECTOR2( 1.0f, 0.0f );
        Verts[2].pos = D3DXVECTOR4( fHUDX + fHUDWidth, fHUDY + fHUDHeight, 0.0f, 1.0f );
        Verts[2].tex = D3DXVECTOR2( 1.0f, 1.0f );
        Verts[3].pos = D3DXVECTOR4( fHUDX, fHUDY + fHUDHeight, 0.0f, 1.0f );
        Verts[3].tex = D3DXVECTOR2( 0.0f, 1.0f );

        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAKILL, D3DTALPHAKILL_ENABLE );
        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
        m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
        m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
        m_pd3dDevice->SetRenderState( D3DRS_LIGHTING,  FALSE );
        m_pd3dDevice->SetRenderState( D3DRS_ZENABLE,   FALSE );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MIPFILTER, D3DTEXF_NONE );
        m_pd3dDevice->SetTexture( 0, m_pHUDTexture );
        m_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW | D3DFVF_TEX1 );
        
        if( TRUE == m_bUseDynamicGamma )
        {
            m_DynamicGammaCtrl.SetInvGammaCorrection();
        }
        else
        {
            m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 );
            m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
            m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1 );
            m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
            m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );
            m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
            
        }
        m_pd3dDevice->DrawVerticesUP( D3DPT_QUADLIST, 4, Verts, sizeof(Verts[0]) );

        // Restore state
        m_pd3dDevice->SetPixelShader( NULL );
        m_pd3dDevice->SetTexture( 0, NULL );
        m_pd3dDevice->SetTexture( 1, NULL );
        m_pd3dDevice->SetTexture( 2, NULL );
        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAKILL, D3DTALPHAKILL_DISABLE );

        // Draw histogram
        m_DynamicGammaCtrl.DrawHistogram(  48, 75,       48+80, 75+75 );

        // Draw gamma ramp
        m_DynamicGammaCtrl.DrawGammaRamps( 48, 75+75+60, 48+80, 75+75+60+75 );

        // Reset some render states
        m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, D3DZB_TRUE );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    }
    
    // Show title, frame rate, state, and help
    if( m_bDrawHelp )
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 48, 36, 0xffffffff,  L"Dynamic Gamma" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 592, 38, 0xffffff00, m_strFrameRate, XBFONT_RIGHT );

        m_Font.SetScaleFactors( 0.8f, 0.8f );
        m_Font.DrawText( 48, 150,    0xffffffff,  L"Luminance", XBFONT_LEFT);
        m_Font.DrawText( 48, 150+17, 0xffffffff,  L"Histogram", XBFONT_LEFT);
        m_Font.DrawText( 48, 285,    0xffffffff,  L"Gamma", XBFONT_LEFT);
        m_Font.DrawText( 48, 285+17, 0xffffffff,  L"Ramp", XBFONT_LEFT);

        if( m_bUseDynamicGamma )
        {   
            WCHAR strBuffer[200];
            DWORD dwColor;

            dwColor = (m_Adjust == ADJUST_BIAS) ? 0xff00ff00 : 0xffffffff;
            m_Font.DrawText( 592-170, 70+(0*18), dwColor, L"Bias", XBFONT_LEFT );
            swprintf( strBuffer, L"%2.2f", m_DynamicGammaCtrl.m_fBias );
            m_Font.DrawText( 592, 70+(0*18), dwColor, strBuffer, XBFONT_RIGHT );

            dwColor = (m_Adjust == ADJUST_DARKCLAMP) ? 0xff00ff00 : 0xffffffff;
            m_Font.DrawText( 592-170, 70+(1*18), dwColor, L"Dark Clamp", XBFONT_LEFT );
            swprintf( strBuffer, L"%2.2f", m_DynamicGammaCtrl.m_fDarkClamp );
            m_Font.DrawText( 592, 70+(1*18), dwColor, strBuffer, XBFONT_RIGHT );

            dwColor = (m_Adjust == ADJUST_LIGHTCLAMP) ? 0xff00ff00 : 0xffffffff;
            m_Font.DrawText( 592-170, 70+(2*18), dwColor, L"Light Clamp", XBFONT_LEFT );
            swprintf( strBuffer, L"%2.2f", m_DynamicGammaCtrl.m_fLightClamp );
            m_Font.DrawText( 592, 70+(2*18), dwColor, strBuffer, XBFONT_RIGHT );

            dwColor = (m_Adjust == ADJUST_DARKDT) ? 0xff00ff00 : 0xffffffff;
            m_Font.DrawText( 592-170, 70+(3*18), dwColor, L"Dark Adj dt", XBFONT_LEFT );
            swprintf( strBuffer, L"%2.2f", m_DynamicGammaCtrl.m_fDarkAdjustDt );
            m_Font.DrawText( 592, 70+(3*18), dwColor, strBuffer, XBFONT_RIGHT );

            dwColor = (m_Adjust == ADJUST_LIGHTDT) ? 0xff00ff00 : 0xffffffff;
            m_Font.DrawText( 592-170, 70+(4*18), dwColor, L"Light Adj dt", XBFONT_LEFT );
            swprintf( strBuffer, L"%2.2f", m_DynamicGammaCtrl.m_fLightAdjustDt );
            m_Font.DrawText( 592, 70+(4*18), dwColor, strBuffer, XBFONT_RIGHT );

            dwColor = (m_Adjust == ADJUST_CONTRASTDT) ? 0xff00ff00 : 0xffffffff;
            m_Font.DrawText( 592-170, 70+(5*18), dwColor, L"Contrast Adj dt", XBFONT_LEFT );
            swprintf( strBuffer, L"%2.2f", m_DynamicGammaCtrl.m_fContrastAdjustDt );
            m_Font.DrawText( 592, 70+(5*18), dwColor, strBuffer, XBFONT_RIGHT );

            dwColor = (m_Adjust == ADJUST_DARKTHRESHOLD) ? 0xff00ff00 : 0xffffffff;
            m_Font.DrawText( 592-170, 70+(6*18), dwColor, L"Dark Threshold", XBFONT_LEFT );
            swprintf( strBuffer, L"%2.2f", m_DynamicGammaCtrl.m_fDarkThreshold );
            m_Font.DrawText( 592, 70+(6*18), dwColor, strBuffer, XBFONT_RIGHT );

            dwColor = (m_Adjust == ADJUST_LIGHTTHRESHOLD) ? 0xff00ff00 : 0xffffffff;
            m_Font.DrawText( 592-170, 70+(7*18), dwColor, L"Light Threshold", XBFONT_LEFT );
            swprintf( strBuffer, L"%2.2f", m_DynamicGammaCtrl.m_fLightThreshold );
            m_Font.DrawText( 592, 70+(7*18), dwColor, strBuffer, XBFONT_RIGHT );
        }
        else
        {
            m_Font.DrawText( 592-170, 70+(0*18), 0xffffffff, L"Linear Gamma", XBFONT_LEFT );
        }

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}


