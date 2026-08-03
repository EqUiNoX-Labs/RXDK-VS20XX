//-----------------------------------------------------------------------------
// File: HighDynamicRange.cpp
//
// Desc: Impliments the High Dynamic Range Sample
//
// Copyright (c) 2002 Microsoft Corporation. All rights reserved.
//
// Hist: 9.00.02 - Created 
//       11.00.02 - Added "hot regions" filter and improved blur
//       04.00.03 - Added Accumulation of previous to get "streaky" lights
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
    { XBHELP_LEFTSTICK,   XBHELP_PLACEMENT_1, L"Move camera" },
    { XBHELP_RIGHTSTICK,  XBHELP_PLACEMENT_1, L"Rotate camera" },
    { XBHELP_A_BUTTON,    XBHELP_PLACEMENT_2, L"View original\nand blurred\nimage" },
    { XBHELP_B_BUTTON,    XBHELP_PLACEMENT_2, L"View streaked\nimage" },
    { XBHELP_X_BUTTON,    XBHELP_PLACEMENT_2, L"View original\nand streaked\nimage" },
    { XBHELP_Y_BUTTON,    XBHELP_PLACEMENT_2, L"View blurred\nimage\n" },
    { XBHELP_WHITE_BUTTON,XBHELP_PLACEMENT_2, L"View original\nimage\n" },
    { XBHELP_DPAD,        XBHELP_PLACEMENT_2, L"Change\nbloom scale\n and streak length" },
};
#define NUM_HELP_CALLOUTS ( sizeof(g_HelpCallouts)/sizeof(g_HelpCallouts[0]) )




//-----------------------------------------------------------------------------
// Name: FilterSample
// Desc: A filter sample holds a subpixel offset and a filter value
//       to be multiplied by a source texture to compute an arbitrary
//       filter.  See FilterCopy for more details.
//-----------------------------------------------------------------------------
struct FilterSample
{
    float fValue;               // coefficient
    float fOffsetX, fOffsetY;   // subpixel offsets of supersamples in
                                //   destination coordinates
};




// Range for m_fStreakLength
#define MAX_STREAK      0.95f
#define MIN_STREAK      0.80f




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    CXBFont     m_Font;             // Font object
    CXBHelp     m_Help;             // Help object
    CXBMesh     m_Scene;            // scene
    BOOL        m_bDrawHelp;        // TRUE to draw help screen

    // Viewing parameters
    FLOAT           m_fYaw, m_fPitch;
    D3DXVECTOR3     m_vPosition;
    D3DXMATRIX      m_matWorld;
    D3DXMATRIX      m_matView;
    D3DXMATRIX      m_matProjection;
    
    // The blur filters are multipass and need temporary space.
#define BLUR_COUNT 2
    D3DTexture *m_rpHotImage;           // hot image
    D3DTexture *m_rpImage;              // Other hot image
    D3DTexture *m_rpBlur[BLUR_COUNT];   // bluring textures of decreasing size
    D3DTexture *m_pBlur;                // current blur texture, set by Blur()
    D3DTexture *m_pPreviousBlur;        // Accumulation buffer so hot parts
                                        // of the image show motion blur

    // Overall modes for the sample.
    enum EFFECTMODE
    { 
        EM_ORIGINAL,                // original scene (no effect)
        EM_HOT_BLUR,                // blured hot portions
        EM_HOT_STREAK,              // accumulated hot portions
        EM_ORIGINAL_PLUS_HOT_BLUR,  // original scene + blured hot portions
        EM_ORIGINAL_PLUS_HOT_STREAK,// original scene + accumulation buffer
        EM_MAX
    }
    m_eEffectMode;

    // Light blend intensity scale factor
    FLOAT m_fBloomScale;

    // Controls the length of the motion blur streaks
    FLOAT m_fStreakLength;
    
    // Pixel shader handles
    DWORD m_dwHotBlurPixelShader;      // blur the hot image
    DWORD m_dwExtractHotPixelShader;   // extract hot image
    DWORD m_dwAccumulatePixelShader;   // Blend previous hot with current hot.

    
    // Filtering routine that draws the source texture multiple
    // times, with sub-pixel offsets and filter coefficients.
    HRESULT FilterCopy( LPDIRECT3DTEXTURE8 pTextureDst,
                        LPDIRECT3DTEXTURE8 pTextureSrc,
                        UINT nSample,
                        FilterSample rSample[],
                        UINT nSuperSampleX,
                        UINT nSuperSampleY,
                        RECT *pRectDst = NULL,  // The destination texture is
                                                //   written only within this
                                                //   region.
                        RECT *pRectSrc = NULL );// The source texture is read
                                                //   outside of this region by
                                                //   the halfwidth of the
                                                //   filter.

    // extract hot image with downsamping
    HRESULT ExtractHot( LPDIRECT3DTEXTURE8 pTextureDst,
                        LPDIRECT3DTEXTURE8 pTextureSrc,
                        UINT nSuperSampleX, UINT nSuperSampleY,
                        RECT* pRectDst = NULL,
                        RECT* pRectSrc = NULL );
                       
    // add hot image to previous image with downsamping
    HRESULT Accumulate( LPDIRECT3DTEXTURE8 pTextureDst,
                        LPDIRECT3DTEXTURE8 pTextureSrc1,
                        LPDIRECT3DTEXTURE8 pTextureSrc2,
                        UINT nSuperSampleX, UINT nSuperSampleY,
                        RECT* pRectDst = NULL,
                        RECT* pRectSrc1 = NULL,
                        RECT* pRectSrc2 = NULL );

    // Blur hot texture and set m_pBlur.  Calls FilterCopy with
    // different filter coefficients and offsets
    HRESULT HotBlur( BOOL bAccumulate );
    
    // Demonstrate the inputs to the full high dynamic range effect.
    HRESULT DrawHotBlur( BOOL bAdd ); // draw blurred "hot" texture
    
    // Set D3D Render States
    HRESULT SetRenderStates( );

    // Set D3D Lighting
    HRESULT SetLight( FLOAT fScale );

    HRESULT ClearTexture( LPDIRECT3DTEXTURE8 pTexture );
    
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
    // Allow unlimited frame rate
#if defined(_DEBUG) || defined(PROFILE)  // Don't vsync when profiling or debugging
    m_d3dpp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;    
#endif

    ZeroMemory( m_rpBlur, sizeof(m_rpBlur) );
    m_pBlur = NULL;
    m_dwHotBlurPixelShader = NULL;
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

    // Create Scene
    if( FAILED( m_Scene.Create( "Models\\SnowNite.xbg", NULL ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create pixel shader
    {
        #include "hotblur.inl"
        D3DDevice_CreatePixelShader(&psd, &m_dwHotBlurPixelShader);
    }
    // Create pixel shader
    {
        #include "extracthot.inl"
        D3DDevice_CreatePixelShader(&psd, &m_dwExtractHotPixelShader);
    }
    
    // Create pixel shader
    {
        #include "accumulate.inl"
        D3DDevice_CreatePixelShader(&psd, &m_dwAccumulatePixelShader);
    }

    // Get size of render target
    LPDIRECT3DSURFACE8 pRenderTarget;
    g_pd3dDevice->GetRenderTarget( &pRenderTarget );
    D3DSURFACE_DESC descRenderTarget;
    pRenderTarget->GetDesc( &descRenderTarget );
    UINT Width = descRenderTarget.Width;
    UINT Height = descRenderTarget.Height;
    pRenderTarget->Release();

    // The HighDynamicRange effect is mostly memory bandwidth limited.
    // Therefore it can be made to run slightly faster - and use less
    // memory - by choosing a 16-bit format for the downsampled
    // textures instead of a 32-bit format.
    // The loss of precision does not noticeably affect the results
    // because the visual anomalies in this effect are primarily caused
    // by lack of spatial resolution, not lack of color precision.
    // Also, the algorithm only uses 7 bits anyway, so not very much
    // color precision is lost.
    // It is important to use a 555 format rather than a 565 format
    // because of the visible color distortions that can happen with
    // 565.
    D3DFORMAT Format = D3DFMT_LIN_X1R5G5B5;

    // Create extract hot text
    D3DDevice_CreateTexture( Width >> 1, Height >> 1, 1,
                             D3DUSAGE_RENDERTARGET, Format,
                             0, &m_rpHotImage );

    // Create the accumulation buffer
    D3DDevice_CreateTexture( Width >> 2, Height >> 2, 1,
                             D3DUSAGE_RENDERTARGET, Format,
                             0, &m_pPreviousBlur );

    // Clear Accumulation buffer to Minimum color we can get down to
    ClearTexture( m_pPreviousBlur );

    // Make the size a factor of 2 smaller on each axis 
    D3DDevice_CreateTexture( Width >> 1, Height >> 2, 1,
                             D3DUSAGE_RENDERTARGET, Format,
                             0, &m_rpBlur[0] );

    D3DDevice_CreateTexture( Width >> 2, Height >> 2, 1,
                             D3DUSAGE_RENDERTARGET, Format,
                             0, &m_rpBlur[1] );

    
    // Set camera parameters and initialize camera matrices
    m_fYaw      = 0.0f;
    m_fPitch    = 0.0f;
    m_vPosition = D3DXVECTOR3( 0, 0, 23.0f );

    // Set world matrix (scene geometry needed to be reorientated)
    ZeroMemory( &m_matWorld, sizeof(m_matWorld) );
    m_matWorld._11 = -1.0f;
    m_matWorld._23 = 1.0f;
    m_matWorld._32 = 1.0f;
    m_matWorld._44 = 1.0f;
    m_matWorld._42 = -25.0f;
    D3DDevice_SetTransform( D3DTS_WORLD, &m_matWorld );

    // Set projection 
    D3DXMatrixPerspectiveFovLH( &m_matProjection, D3DX_PI/4,
                                640.f / 480.f, 1.f, 1000.f );
    D3DDevice_SetTransform( D3DTS_PROJECTION, &m_matProjection );
   
    // Set modes
    m_bDrawHelp     = false;
    m_eEffectMode   = EM_ORIGINAL_PLUS_HOT_BLUR;

    // Set bloom scale
    m_fBloomScale = 1.00f;

    m_fStreakLength = 0.95f;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SetLight()
// Desc: Set current D3D Light
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::SetLight( FLOAT fScale )
{
    // Set Light
    D3DLIGHT8 light;
    ZeroMemory( &light, sizeof(D3DLIGHT8) );
    light.Type         = D3DLIGHT_DIRECTIONAL;
    light.Diffuse      = D3DXCOLOR( 0.5f, 0.5f, 0.5f, 1.f ) * fScale;
    light.Direction    = D3DXVECTOR3( -1.f, -1.f, 1.f );
    D3DDevice_SetLight( 0, &light );
    D3DDevice_LightEnable( 0, TRUE );

    // Set Ambient
    D3DDevice_SetRenderState( D3DRS_AMBIENT,
                              XGCOLOR(0.15f, 0.15f, 0.15f, 1.0f) * fScale);

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SetRenderStates()
// Desc: Set current D3D Render States
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::SetRenderStates( )
{   
    // Depth
    D3DDevice_SetRenderState( D3DRS_ZWRITEENABLE, TRUE ); 
    D3DDevice_SetRenderState( D3DRS_ZENABLE, D3DZB_TRUE ); 
    
    // Alpha test and blend
    D3DDevice_SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
    D3DDevice_SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );

    // Lighting
    D3DDevice_SetRenderState( D3DRS_LIGHTING, TRUE );
    D3DDevice_SetRenderState( D3DRS_COLORVERTEX, FALSE );

    // textures and blendops
    D3DDevice_SetTexture( 0, NULL );
    D3DDevice_SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
    D3DDevice_SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_CURRENT );
    D3DDevice_SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
    D3DDevice_SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
    
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
        m_bDrawHelp = !m_bDrawHelp;

    BOOL bClearPrevious = FALSE;
    // Effect modes
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
    {
        m_eEffectMode = EM_ORIGINAL_PLUS_HOT_BLUR;
        bClearPrevious = TRUE;
    }

    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_X] )
    {
        m_eEffectMode = EM_ORIGINAL_PLUS_HOT_STREAK;
        bClearPrevious = TRUE;
    }

    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_Y] )
    {
        m_eEffectMode = EM_HOT_BLUR;
        bClearPrevious = TRUE;
    }

    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_B] )
    {
        m_eEffectMode = EM_HOT_STREAK;
        bClearPrevious = TRUE;
    }

    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_WHITE] )
    {
        m_eEffectMode = EM_ORIGINAL;
        bClearPrevious = TRUE;
    }
    
    // Update light scale
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
    {
        m_fBloomScale += 0.05f;
        bClearPrevious = TRUE;
    }

    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
    {
        m_fBloomScale -= 0.05f;
        bClearPrevious = TRUE;
    }

    if( m_fBloomScale > 1.0f )
        m_fBloomScale = 1.0f;
    if( m_fBloomScale < 0.0f )
        m_fBloomScale = 0.0f;
    
    // Update streak scale
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
    {
        m_fStreakLength += 0.01f;
        bClearPrevious = TRUE;
    }

    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
    {
        m_fStreakLength -= 0.01f;
        bClearPrevious = TRUE;
    }

    if( m_fStreakLength > MAX_STREAK )
        m_fStreakLength = MAX_STREAK;
    if( m_fStreakLength < MIN_STREAK )
        m_fStreakLength = MIN_STREAK;

    // We need to clear the accumulation buffer.  Something big changed!
    // This will cause the screen to "flash" but its OK since you shouldn't
    // be changing these on the fly for the full screen in a real game.
    if( bClearPrevious )
        ClearTexture( m_pPreviousBlur );

    // Camera matrix
    XGMATRIX Camera;
    XGVECTOR3* pLeft     = (XGVECTOR3*)&Camera[4*0];
    XGVECTOR3* pForward  = (XGVECTOR3*)&Camera[4*2];
    XGVECTOR3* pPosition = (XGVECTOR3*)&Camera[4*3];

    // Rotation
    FLOAT fRotationScale = 1.0f * m_fElapsedTime;
    FLOAT fDeltaYaw = fRotationScale * m_DefaultGamepad.fX2;
    FLOAT fDeltaPitch = -fRotationScale * m_DefaultGamepad.fY2;
    m_fYaw   += fDeltaYaw;
    m_fPitch += fDeltaPitch;
    XGMatrixRotationYawPitchRoll( &Camera, m_fYaw, m_fPitch, 0.0f );

    // Translation
    FLOAT fTranslationScale = 10.0f*m_fElapsedTime;
    D3DXVECTOR3 vLeft( pLeft->x, 0.0f, pLeft->z );
    D3DXVec3Normalize( &vLeft, &vLeft );
    D3DXVECTOR3 vForward( pForward->x, 0.0f, pForward->z );
    D3DXVec3Normalize( &vForward, &vForward );
    m_vPosition += vLeft * fTranslationScale * m_DefaultGamepad.fX1;
    m_vPosition += vForward * fTranslationScale * m_DefaultGamepad.fY1;
    m_vPosition.y = 0.0f;
    (*pPosition) = m_vPosition;

    // Update view matrix
    FLOAT fDet;
    XGMatrixInverse( &m_matView, &fDet, &Camera );
    assert(fDet > 0.0f);

    // Set view matrix
    D3DDevice_SetTransform( D3DTS_VIEW, &m_matView );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the scene
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Render()
{
    // Clear the back and zbuffer
    D3DDevice_Clear( 0, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,
                     0x00000000, 1.0f, 0L );

    // Render effects

    // Set render states
    SetRenderStates();

    switch( m_eEffectMode )
    {
        case EM_ORIGINAL:
            SetLight( 0.8f );           // Set light
            m_Scene.Render();           // Render scene
            break;
        case EM_HOT_BLUR:
            SetLight( 0.8f );           // Set light
            m_Scene.Render();           // Render scene
            HotBlur(FALSE);             // Blur the hot values in the backbuffer
            DrawHotBlur( FALSE );       // Draw blurred hot values
            break;
        case EM_HOT_STREAK:
            SetLight( 0.8f );           // Set light
            m_Scene.Render();           // Render scene
            HotBlur(TRUE);              // Blur the hot values in the backbuffer
            DrawHotBlur( FALSE );       // Draw blurred hot values
            break;
        case EM_ORIGINAL_PLUS_HOT_BLUR:
            SetLight( 0.8f );           // Set light
            m_Scene.Render();           // Render scene
            HotBlur(FALSE);             // Blur the hot values in the backbuffer
            DrawHotBlur( TRUE );        // Draw blurred hot values, add to scene
            break;
        case EM_ORIGINAL_PLUS_HOT_STREAK:
            SetLight( 0.8f );           // Set light
            m_Scene.Render();           // Render scene
            HotBlur(TRUE);              // Blur the hot values in the backbuffer
            DrawHotBlur( TRUE );        // Draw blurred hot values, add to scene
            break;
    }

    
    // Show title, effect mode, frame rate, and help
    if( m_bDrawHelp )
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    else
    {
        static WCHAR* strEffectMode[EM_MAX] = 
        {
            L"Original Image\n",
            L"Blurred Hot Image",
            L"Streaked Hot Image",
            L"Original Image\n+ Blurred Hot Image",
            L"Original Image\n+ Streaked Hot Image",
         };
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 48, 36, 0xffffffff,  L"HighDynamicRange" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 592, 38, 0xffffff00, m_strFrameRate, XBFONT_RIGHT );

        WCHAR strBuf[100];
        swprintf( strBuf, L"Bloom Scale: %0.2f", m_fBloomScale );
        m_Font.DrawText( 64, 75, 0xffffff00, strBuf );
        swprintf( strBuf, L"Streak Length: %0.2f", m_fStreakLength );
        m_Font.DrawText( 64, 95, 0xffffff00, strBuf );
        m_Font.DrawText( 64, 120, 0xffffff00, strEffectMode[m_eEffectMode] );
      
        m_Font.End();
    }

    // Present the scene
    D3DDevice_Swap( D3DSWAP_DEFAULT );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: DrawHotBlur()
// Desc: Display the blurred hot values
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::DrawHotBlur( BOOL bAdd )
{
    if( !m_pBlur )
        return S_FALSE;
    LPDIRECT3DTEXTURE8 pTexture = m_pBlur;

    // Get size of backbuffer
    LPDIRECT3DSURFACE8 pRenderTarget;
    D3DDevice_GetRenderTarget( &pRenderTarget );
    D3DSURFACE_DESC descRenderTarget;
    pRenderTarget->GetDesc( &descRenderTarget );
    UINT Width = descRenderTarget.Width;
    UINT Height = descRenderTarget.Height;
    pRenderTarget->Release();

    // Texture coordinates in linear format textures go from 0 to n-1 rather
    // than the 0 to 1 that is used for swizzled textures.
    D3DSURFACE_DESC desc;
    pTexture->GetLevelDesc( 0, &desc );
    struct BACKGROUNDVERTEX { D3DXVECTOR4 p; FLOAT tu, tv; } v[4];
    v[0].p = D3DXVECTOR4( -0.5f,        -0.5f,         1.0f, 1.0f );
    v[0].tu = 0.0f;              v[0].tv = 0.0f;
    v[1].p = D3DXVECTOR4( Width - 0.5f, -0.5f,         1.0f, 1.0f );
    v[1].tu = (float)desc.Width; v[1].tv = 0.0f;
    v[2].p = D3DXVECTOR4( -0.5f,        Height - 0.5f, 1.0f, 1.0f );
    v[2].tu = 0.0f;              v[2].tv = (float)desc.Height;
    v[3].p = D3DXVECTOR4( Width - 0.5f, Height - 0.5f, 1.0f, 1.0f );
    v[3].tu = (float)desc.Width; v[3].tv = (float)desc.Height;

    // Set states
    D3DDevice_SetPixelShader( 0 );
    D3DDevice_SetTexture( 0, pTexture );
    D3DDevice_SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE2X );
    D3DDevice_SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    D3DDevice_SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_TFACTOR );
    D3DDevice_SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
    D3DDevice_SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP );
    D3DDevice_SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP );
    D3DDevice_SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    D3DDevice_SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    D3DDevice_SetTextureStageState( 0, D3DTSS_MAXMIPLEVEL, 0 );
    D3DDevice_SetTextureStageState( 0, D3DTSS_MIPFILTER, D3DTEXF_NONE );
    D3DDevice_SetRenderState( D3DRS_ZENABLE, FALSE ); 
    D3DDevice_SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
    
    XGCOLOR Blend(1.0f, 1.0f, 1.0f, 1.0f);
    Blend*= m_fBloomScale; // adjust blend amount
    D3DDevice_SetRenderState( D3DRS_TEXTUREFACTOR, Blend );
    
    // add if requested
    D3DDevice_SetRenderState( D3DRS_ALPHABLENDENABLE, bAdd );
    D3DDevice_SetRenderState( D3DRS_BLENDOP, D3DBLENDOP_ADD );    
    D3DDevice_SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
    D3DDevice_SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );
    
    // Render the screen-aligned quadrilateral
    D3DDevice_SetVertexShader( D3DFVF_XYZRHW|D3DFVF_TEX1 );
    D3DDevice_DrawVerticesUP( D3DPT_QUADSTRIP,
                              4, v, sizeof(BACKGROUNDVERTEX) );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FilterCopy()
// Desc: Filter the source texture by rendering into the destination texture
//       with subpixel offsets. Does 4 filter coefficients at a time, using
//       all the stages of the pixel shader.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FilterCopy( LPDIRECT3DTEXTURE8 pTextureDst,
                                 LPDIRECT3DTEXTURE8 pTextureSrc,
                                 UINT nSample, FilterSample rSample[],
                                 UINT nSuperSampleX, UINT nSuperSampleY,
                                 RECT* pRectDst, RECT* pRectSrc
                                 )
{
    // Texture space pixel center == screen space pixel center
    D3DDevice_SetScreenSpaceOffset( -0.5f, -0.5f );

    // Save current render target and depth buffer
    LPDIRECT3DSURFACE8 pRenderTarget, pZBuffer;
    D3DDevice_GetRenderTarget( &pRenderTarget );
    D3DDevice_GetDepthStencilSurface( &pZBuffer );

    // Set destination as render target
    LPDIRECT3DSURFACE8 pSurface = NULL;
    pTextureDst->GetSurfaceLevel( 0, &pSurface );
    D3DDevice_SetRenderTarget( pSurface, NULL );  // no depth-buffering
    pSurface->Release();

    // Get descriptions of source and destination
    D3DSURFACE_DESC descSrc;
    pTextureSrc->GetLevelDesc( 0, &descSrc );
    D3DSURFACE_DESC descDst;
    pTextureDst->GetLevelDesc( 0, &descDst );

    // Setup rectangles if not specified on input
    RECT rectSrc = { 0, 0, descSrc.Width, descSrc.Height };
    if( pRectSrc == NULL ) pRectSrc = &rectSrc;
    RECT rectDst = { 0, 0, descDst.Width, descDst.Height };
    if( pRectDst == NULL )
    {
        // If the destination rectangle is not specified,
        // we change it to match the source rectangle
        rectDst.right = (pRectSrc->right - pRectSrc->left) / nSuperSampleX;
        rectDst.bottom = (pRectSrc->bottom - pRectSrc->top) / nSuperSampleY;
        pRectDst = &rectDst;
    }
    assert( (pRectDst->right - pRectDst->left) ==
            (pRectSrc->right - pRectDst->left) / (INT)nSuperSampleX );
    assert( (pRectDst->bottom - pRectDst->top) ==
            (pRectSrc->bottom - pRectDst->top) / (INT)nSuperSampleY );
    
    //Set render state for filtering
    D3DDevice_SetRenderState( D3DRS_LIGHTING, FALSE );
    D3DDevice_SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
    D3DDevice_SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
    D3DDevice_SetRenderState( D3DRS_ZENABLE, D3DZB_FALSE );
    D3DDevice_SetRenderState( D3DRS_STENCILENABLE, FALSE );
    D3DDevice_SetRenderState( D3DRS_FOGENABLE, FALSE );
    // On first rendering, copy new value over current render target contents
    D3DDevice_SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    // Setup subsequent renderings to add to previous value
    D3DDevice_SetRenderState( D3DRS_BLENDOP,   D3DBLENDOP_ADD );    
    D3DDevice_SetRenderState( D3DRS_SRCBLEND,  D3DBLEND_ONE );
    D3DDevice_SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );

    // Set texture state
    for( UINT xx = 0; xx < 4; xx++)
    {
        // Use our source texture for all four stages
        D3DDevice_SetTexture( xx, pTextureSrc);  
        D3DDevice_SetTextureStageState( xx, D3DTSS_COLOROP, D3DTOP_DISABLE );
        D3DDevice_SetTextureStageState( xx, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
        
        // Pass texture coords without transformation
        D3DDevice_SetTextureStageState( xx, D3DTSS_TEXTURETRANSFORMFLAGS,
                                            D3DTTFF_DISABLE );  
        // Each texture has different tex coords
        D3DDevice_SetTextureStageState( xx, D3DTSS_TEXCOORDINDEX, xx ); 
        D3DDevice_SetTextureStageState( xx, D3DTSS_ADDRESSU,
                                            D3DTADDRESS_CLAMP );
        D3DDevice_SetTextureStageState( xx, D3DTSS_ADDRESSV,
                                            D3DTADDRESS_CLAMP );
        D3DDevice_SetTextureStageState( xx, D3DTSS_MAXMIPLEVEL, 0 );
        D3DDevice_SetTextureStageState( xx, D3DTSS_MIPFILTER, D3DTEXF_NONE );
        D3DDevice_SetTextureStageState( xx, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
        D3DDevice_SetTextureStageState( xx, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
        D3DDevice_SetTextureStageState( xx, D3DTSS_COLORKEYOP,
                                            D3DTCOLORKEYOP_DISABLE );
        D3DDevice_SetTextureStageState( xx, D3DTSS_COLORSIGN, 0 );
        D3DDevice_SetTextureStageState( xx, D3DTSS_ALPHAKILL,
                                            D3DTALPHAKILL_DISABLE );
    }
    
    // Use hot blur pixel shader
    D3DDevice_SetPixelShader( m_dwHotBlurPixelShader );       

    // For screen-space texture-mapped quadrilateral
    g_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW|D3DFVF_TEX4 );   

    // Prepare quadrilateral vertices
    float x0 = (float)pRectDst->left;
    float y0 = (float)pRectDst->top;
    float x1 = (float)pRectDst->right;
    float y1 = (float)pRectDst->bottom;
    struct Quad
    {
        float x, y, z, w1;
        struct uv
        {
            float u, v;
        }
        tex[4];   // each texture has different offset
    } 
    aQuad[4] =
    { //  X   Y     Z   1/W     u0  v0      u1  v1      u2  v2      u3  v3
        {x0, y0, 1.0f, 1.0f, }, // texture coords are set below
        {x1, y0, 1.0f, 1.0f, },
        {x0, y1, 1.0f, 1.0f, },
        {x1, y1, 1.0f, 1.0f, }
    };

    // Set rendering to just the destination rect
    g_pd3dDevice->SetScissors( 1, FALSE, (D3DRECT *)pRectDst );

    // Draw a quad for each block of 4 filter coefficients
    float fOffsetScaleU = (float)nSuperSampleX; // offset for supersample 
    float fOffsetScaleV = (float)nSuperSampleY;
    float u0 = (float)pRectSrc->left;
    float v0 = (float)pRectSrc->top;
    float u1 = (float)pRectSrc->right;
    float v1 = (float)pRectSrc->bottom;


    if( XGIsSwizzledFormat( descSrc.Format ) )
    {
        float fWidthScale = 1.f / (float)descSrc.Width;
        float fHeightScale = 1.f / (float)descSrc.Height;
        fOffsetScaleU *= fWidthScale;
        fOffsetScaleV *= fHeightScale;
        u0 *= fWidthScale;
        v0 *= fHeightScale;
        u1 *= fWidthScale;
        v1 *= fHeightScale;
    }
    
    xx = 0; // current texture stage
    D3DCOLOR rColor[4];
    DWORD rPSInput[4];
    for( UINT iSample = 0; iSample < nSample; iSample++ )
    {
        // Set filter coefficients
        float fValue = rSample[iSample].fValue;
        if( fValue < 0.f )
        {
            rColor[xx] = D3DXCOLOR( -fValue, -fValue, -fValue, -fValue );
            rPSInput[xx] = PS_INPUTMAPPING_SIGNED_NEGATE |
                           ((xx % 2) ? PS_REGISTER_C1 : PS_REGISTER_C0);
        }
        else
        {
            rColor[xx] = D3DXCOLOR( fValue, fValue, fValue, fValue );
            rPSInput[xx] = PS_INPUTMAPPING_SIGNED_IDENTITY |
                           ((xx % 2) ? PS_REGISTER_C1 : PS_REGISTER_C0);
        }

        // Align supersamples with center of destination pixels
        float fOffsetX = rSample[iSample].fOffsetX;// * fOffsetScaleU;
        float fOffsetY = rSample[iSample].fOffsetY;// * fOffsetScaleV;
        aQuad[0].tex[xx].u = u0 + fOffsetX;
        aQuad[0].tex[xx].v = v0 + fOffsetY;
        aQuad[1].tex[xx].u = u1 + fOffsetX;
        aQuad[1].tex[xx].v = v0 + fOffsetY;
        aQuad[2].tex[xx].u = u0 + fOffsetX;
        aQuad[2].tex[xx].v = v1 + fOffsetY;
        aQuad[3].tex[xx].u = u1 + fOffsetX;
        aQuad[3].tex[xx].v = v1 + fOffsetY;
        
        xx++; // Go to next stage
        if( xx == 4 || iSample == nSample - 1 ) // max texture stages or last sample
        {
            // Zero out unused texture stage coefficients 
            // (Only for last filter sample, when number of samples is not divisible by 4)
            for( ; xx < 4; xx++)
            {
                g_pd3dDevice->SetTexture( xx, NULL );
                rColor[xx] = 0;
                rPSInput[xx] = PS_INPUTMAPPING_UNSIGNED_IDENTITY | PS_REGISTER_ZERO;
            }
        
            // Set coefficients
            g_pd3dDevice->SetRenderState( D3DRS_PSCONSTANT0_0, rColor[0] );
            g_pd3dDevice->SetRenderState( D3DRS_PSCONSTANT1_0, rColor[1] );
            g_pd3dDevice->SetRenderState( D3DRS_PSCONSTANT0_1, rColor[2] );
            g_pd3dDevice->SetRenderState( D3DRS_PSCONSTANT1_1, rColor[3] );

            // Remap coefficients to proper sign
            g_pd3dDevice->SetRenderState(
                D3DRS_PSRGBINPUTS0,
                PS_COMBINERINPUTS( rPSInput[0] | PS_CHANNEL_RGB,
                                   PS_REGISTER_T0 | PS_CHANNEL_RGB | 
                                      PS_INPUTMAPPING_SIGNED_IDENTITY,
                                   rPSInput[1] | PS_CHANNEL_RGB,
                                   PS_REGISTER_T1 | PS_CHANNEL_RGB |
                                       PS_INPUTMAPPING_SIGNED_IDENTITY ) );
            g_pd3dDevice->SetRenderState(
                D3DRS_PSALPHAINPUTS0,
                PS_COMBINERINPUTS( rPSInput[0] | PS_CHANNEL_ALPHA,
                                   PS_REGISTER_T0 | PS_CHANNEL_ALPHA |
                                      PS_INPUTMAPPING_SIGNED_IDENTITY,
                                   rPSInput[1] | PS_CHANNEL_ALPHA,
                                   PS_REGISTER_T1 | PS_CHANNEL_ALPHA |
                                      PS_INPUTMAPPING_SIGNED_IDENTITY ) );
            g_pd3dDevice->SetRenderState(
                D3DRS_PSRGBINPUTS1,
                PS_COMBINERINPUTS( rPSInput[2] | PS_CHANNEL_RGB,
                                   PS_REGISTER_T2 | PS_CHANNEL_RGB |
                                       PS_INPUTMAPPING_SIGNED_IDENTITY,
                                   rPSInput[3] | PS_CHANNEL_RGB,
                                   PS_REGISTER_T3 | PS_CHANNEL_RGB |
                                       PS_INPUTMAPPING_SIGNED_IDENTITY ) );
            g_pd3dDevice->SetRenderState(
                D3DRS_PSALPHAINPUTS1,
                PS_COMBINERINPUTS( rPSInput[2] | PS_CHANNEL_ALPHA,
                                   PS_REGISTER_T2 | PS_CHANNEL_ALPHA |
                                       PS_INPUTMAPPING_SIGNED_IDENTITY,
                                   rPSInput[3] | PS_CHANNEL_ALPHA,
                                   PS_REGISTER_T3 | PS_CHANNEL_ALPHA |
                                       PS_INPUTMAPPING_SIGNED_IDENTITY ) );
            
            // Draw the quad to filter the coefficients so far
            // One quad blends 4 textures
            g_pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP,
                                           2, aQuad, sizeof(Quad) ); 
             // On subsequent renderings, add to what's in the render target 
            g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
            xx = 0;
        }
    }

    // Clear texture stages
    for( xx=0; xx<4; xx++ )
    {
        g_pd3dDevice->SetTexture( xx, NULL );
        g_pd3dDevice->SetTextureStageState( xx, D3DTSS_COLOROP,
                                                D3DTOP_DISABLE );
        g_pd3dDevice->SetTextureStageState( xx, D3DTSS_ALPHAOP,
                                                D3DTOP_DISABLE );
        g_pd3dDevice->SetTextureStageState( xx, D3DTSS_MIPMAPLODBIAS, 0 );
    }

    // Restore render target and zbuffer
    g_pd3dDevice->SetRenderTarget( pRenderTarget, pZBuffer );
    if( pRenderTarget ) pRenderTarget->Release();
    if( pZBuffer )      pZBuffer->Release();

    D3DDevice_SetScreenSpaceOffset( 0.0f, 0.0f );

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: ExtractHot()
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::ExtractHot( LPDIRECT3DTEXTURE8 pTextureDst,
                                 LPDIRECT3DTEXTURE8 pTextureSrc,
                                 UINT nSuperSampleX, UINT nSuperSampleY,
                                 RECT* pRectDst, RECT* pRectSrc )
{
    // Texture space pixel center == screen space pixel center
    D3DDevice_SetScreenSpaceOffset( -0.5f, -0.5f );

    // Save current render target and depth buffer
    LPDIRECT3DSURFACE8 pRenderTarget, pZBuffer;
    D3DDevice_GetRenderTarget( &pRenderTarget );
    D3DDevice_GetDepthStencilSurface( &pZBuffer );

    // Set destination as render target
    LPDIRECT3DSURFACE8 pSurface = NULL;
    pTextureDst->GetSurfaceLevel( 0, &pSurface );
    D3DDevice_SetRenderTarget( pSurface, NULL );  // no depth-buffering
    pSurface->Release();

    // Get descriptions of source and destination
    D3DSURFACE_DESC descSrc;
    pTextureSrc->GetLevelDesc( 0, &descSrc );
    D3DSURFACE_DESC descDst;
    pTextureDst->GetLevelDesc( 0, &descDst );

    // Setup rectangles if not specified on input
    RECT rectSrc = { 0, 0, descSrc.Width, descSrc.Height };
    if( pRectSrc == NULL ) pRectSrc = &rectSrc;
    RECT rectDst = { 0, 0, descDst.Width, descDst.Height };
    if( pRectDst == NULL )
    {
        // If the destination rectangle is not specified,
        // we change it to match the source rectangle
        rectDst.right = (pRectSrc->right - pRectSrc->left) / nSuperSampleX;
        rectDst.bottom = (pRectSrc->bottom - pRectSrc->top) / nSuperSampleY;
        pRectDst = &rectDst;
    }
    assert( (pRectDst->right - pRectDst->left) ==
            (pRectSrc->right - pRectDst->left) / (INT)nSuperSampleX );
    assert( (pRectDst->bottom - pRectDst->top) ==
            (pRectSrc->bottom - pRectDst->top) / (INT)nSuperSampleY );
    
    //Set render state for filtering
    D3DDevice_SetRenderState( D3DRS_LIGHTING, FALSE );
    D3DDevice_SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
    D3DDevice_SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
    D3DDevice_SetRenderState( D3DRS_ZENABLE, D3DZB_FALSE );
    D3DDevice_SetRenderState( D3DRS_STENCILENABLE, FALSE );
    D3DDevice_SetRenderState( D3DRS_FOGENABLE, FALSE );
    D3DDevice_SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    D3DDevice_SetRenderState( D3DRS_BLENDOP,   D3DBLENDOP_ADD );    
    D3DDevice_SetRenderState( D3DRS_SRCBLEND,  D3DBLEND_ONE );
    D3DDevice_SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );

    D3DDevice_SetTexture( 0, pTextureSrc);
    D3DDevice_SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_DISABLE );
    D3DDevice_SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
    
    // Pass texture coords without transformation
    D3DDevice_SetTextureStageState( 0, D3DTSS_TEXTURETRANSFORMFLAGS,
                                        D3DTTFF_DISABLE );  
    // Each texture has different tex coords
    D3DDevice_SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 ); 
    D3DDevice_SetTextureStageState( 0, D3DTSS_ADDRESSU,
                                       D3DTADDRESS_CLAMP );
    D3DDevice_SetTextureStageState( 0, D3DTSS_ADDRESSV,
                                       D3DTADDRESS_CLAMP );
    D3DDevice_SetTextureStageState( 0, D3DTSS_MAXMIPLEVEL, 0 );
    D3DDevice_SetTextureStageState( 0, D3DTSS_MIPFILTER, D3DTEXF_NONE );
    D3DDevice_SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_GAUSSIANCUBIC );
    D3DDevice_SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_GAUSSIANCUBIC );
    D3DDevice_SetTextureStageState( 0, D3DTSS_COLORKEYOP,
                                       D3DTCOLORKEYOP_DISABLE );
    D3DDevice_SetTextureStageState( 0, D3DTSS_COLORSIGN, 0 );
    D3DDevice_SetTextureStageState( 0, D3DTSS_ALPHAKILL,
                                       D3DTALPHAKILL_DISABLE );
    
    // Use extract hot pixel shader
    D3DDevice_SetPixelShader( m_dwExtractHotPixelShader );       

    // For screen-space texture-mapped quadrilateral
    g_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW|D3DFVF_TEX1 );   

    // Prepare quadrilateral vertices
    float x0 = (float)pRectDst->left;
    float y0 = (float)pRectDst->top;
    float x1 = (float)pRectDst->right;
    float y1 = (float)pRectDst->bottom;
    struct Quad
    {
        float x, y, z, w1;
        struct uv
        {
            float u, v;
        }
        tex;  
    } 
    aQuad[4] =
    { //  X   Y     Z   1/W     u0  v0      u1  v1      u2  v2      u3  v3
        {x0, y0, 1.0f, 1.0f, }, // texture coords are set below
        {x1, y0, 1.0f, 1.0f, },
        {x0, y1, 1.0f, 1.0f, },
        {x1, y1, 1.0f, 1.0f, }
    };

    // Set rendering to just the destination rect
    g_pd3dDevice->SetScissors( 1, FALSE, (D3DRECT *)pRectDst );

  
    // Draw a quad for each block of 4 filter coefficients
    float u0 = (float)pRectSrc->left;
    float v0 = (float)pRectSrc->top;
    float u1 = (float)pRectSrc->right;
    float v1 = (float)pRectSrc->bottom;


    if( XGIsSwizzledFormat( descSrc.Format ) )
    {
        float fWidthScale = 1.f / (float)descSrc.Width;
        float fHeightScale = 1.f / (float)descSrc.Height;
        u0 *= fWidthScale;
        v0 *= fHeightScale;
        u1 *= fWidthScale;
        v1 *= fHeightScale;
    }
    
    aQuad[0].tex.u = u0;
    aQuad[0].tex.v = v0;
    aQuad[1].tex.u = u1;
    aQuad[1].tex.v = v0;
    aQuad[2].tex.u = u0;
    aQuad[2].tex.v = v1;
    aQuad[3].tex.u = u1;
    aQuad[3].tex.v = v1;

    // Draw the quad
    g_pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP,
                                   2, aQuad, sizeof(Quad) ); 

    g_pd3dDevice->SetTexture( 0, NULL );
        

    // Restore render target and zbuffer
    g_pd3dDevice->SetRenderTarget( pRenderTarget, pZBuffer );
    if( pRenderTarget ) pRenderTarget->Release();
    if( pZBuffer )      pZBuffer->Release();

    D3DDevice_SetScreenSpaceOffset( 0.0f, 0.0f );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Accumulate()
// Desc: Combines the two source textures passed in using the accumulate
//       pixel shader.  
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Accumulate( LPDIRECT3DTEXTURE8 pTextureDst,
                                 LPDIRECT3DTEXTURE8 pTextureSrc1,
                                 LPDIRECT3DTEXTURE8 pTextureSrc2,
                                 UINT nSuperSampleX, UINT nSuperSampleY,
                                 RECT* pRectDst, RECT* pRectSrc1,
                                 RECT* pRectSrc2 )
{
    // Texture space pixel center == screen space pixel center
    D3DDevice_SetScreenSpaceOffset( -0.5f, -0.5f );

    // Save current render target and depth buffer
    LPDIRECT3DSURFACE8 pRenderTarget, pZBuffer;
    D3DDevice_GetRenderTarget( &pRenderTarget );
    D3DDevice_GetDepthStencilSurface( &pZBuffer );

    // Set destination as render target
    LPDIRECT3DSURFACE8 pSurface = NULL;
    pTextureDst->GetSurfaceLevel( 0, &pSurface );
    D3DDevice_SetRenderTarget( pSurface, NULL );  // no depth-buffering
    pSurface->Release();

    // Get descriptions of sources and destination
    D3DSURFACE_DESC descSrc1;
    pTextureSrc1->GetLevelDesc( 0, &descSrc1 );
    D3DSURFACE_DESC descSrc2;
    pTextureSrc2->GetLevelDesc( 0, &descSrc2 );
    D3DSURFACE_DESC descDst;
    pTextureDst->GetLevelDesc( 0, &descDst );

    // Setup rectangles if not specified on input
    RECT rectSrc1 = { 0, 0, descSrc1.Width, descSrc1.Height };
    if( pRectSrc1 == NULL ) pRectSrc1 = &rectSrc1;
    RECT rectSrc2 = { 0, 0, descSrc2.Width, descSrc2.Height };
    if( pRectSrc2 == NULL ) pRectSrc2 = &rectSrc2;
    RECT rectDst = { 0, 0, descDst.Width, descDst.Height };
    if( pRectDst == NULL )
    {
        // If the destination rectangle is not specified,
        // we change it to match the source rectangle
        rectDst.right = (pRectSrc2->right - pRectSrc2->left) / nSuperSampleX;
        rectDst.bottom = (pRectSrc2->bottom - pRectSrc2->top) / nSuperSampleY;
        pRectDst = &rectDst;
    }
    assert( (pRectDst->right - pRectDst->left) ==
            (pRectSrc2->right - pRectDst->left) / (INT)nSuperSampleX );
    assert( (pRectDst->bottom - pRectDst->top) ==
            (pRectSrc2->bottom - pRectDst->top) / (INT)nSuperSampleY );
    
    //Set render state for filtering
    D3DDevice_SetRenderState( D3DRS_LIGHTING, FALSE );
    D3DDevice_SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
    D3DDevice_SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
    D3DDevice_SetRenderState( D3DRS_ZENABLE, D3DZB_FALSE );
    D3DDevice_SetRenderState( D3DRS_STENCILENABLE, FALSE );
    D3DDevice_SetRenderState( D3DRS_FOGENABLE, FALSE );
    D3DDevice_SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    D3DDevice_SetRenderState( D3DRS_BLENDOP,   D3DBLENDOP_ADD );    
    D3DDevice_SetRenderState( D3DRS_SRCBLEND,  D3DBLEND_ONE );
    D3DDevice_SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );

    D3DDevice_SetTexture( 0, pTextureSrc1);
    D3DDevice_SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_DISABLE );
    D3DDevice_SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
    
    // Pass texture coords without transformation
    D3DDevice_SetTextureStageState( 0, D3DTSS_TEXTURETRANSFORMFLAGS,
                                        D3DTTFF_DISABLE );  
    // Each texture has different tex coords
    D3DDevice_SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 ); 
    D3DDevice_SetTextureStageState( 0, D3DTSS_ADDRESSU,
                                       D3DTADDRESS_CLAMP );
    D3DDevice_SetTextureStageState( 0, D3DTSS_ADDRESSV,
                                       D3DTADDRESS_CLAMP );
    D3DDevice_SetTextureStageState( 0, D3DTSS_MAXMIPLEVEL, 0 );
    D3DDevice_SetTextureStageState( 0, D3DTSS_MIPFILTER, D3DTEXF_NONE );
    D3DDevice_SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    D3DDevice_SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    D3DDevice_SetTextureStageState( 0, D3DTSS_COLORKEYOP,
                                       D3DTCOLORKEYOP_DISABLE );
    D3DDevice_SetTextureStageState( 0, D3DTSS_COLORSIGN, 0 );
    D3DDevice_SetTextureStageState( 0, D3DTSS_ALPHAKILL,
                                       D3DTALPHAKILL_DISABLE );
    
    D3DDevice_SetTexture( 1, pTextureSrc2);
    D3DDevice_SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
    D3DDevice_SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
    
    // Pass texture coords without transformation
    D3DDevice_SetTextureStageState( 1, D3DTSS_TEXTURETRANSFORMFLAGS,
                                        D3DTTFF_DISABLE );  
    // Each texture has different tex coords
    D3DDevice_SetTextureStageState( 1, D3DTSS_TEXCOORDINDEX, 0 ); 
    D3DDevice_SetTextureStageState( 1, D3DTSS_ADDRESSU,
                                       D3DTADDRESS_CLAMP );
    D3DDevice_SetTextureStageState( 1, D3DTSS_ADDRESSV,
                                       D3DTADDRESS_CLAMP );
    D3DDevice_SetTextureStageState( 1, D3DTSS_MAXMIPLEVEL, 0 );
    D3DDevice_SetTextureStageState( 1, D3DTSS_MIPFILTER, D3DTEXF_NONE );
    D3DDevice_SetTextureStageState( 1, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    D3DDevice_SetTextureStageState( 1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    D3DDevice_SetTextureStageState( 1, D3DTSS_COLORKEYOP,
                                       D3DTCOLORKEYOP_DISABLE );
    D3DDevice_SetTextureStageState( 1, D3DTSS_COLORSIGN, 0 );
    D3DDevice_SetTextureStageState( 1, D3DTSS_ALPHAKILL,
                                       D3DTALPHAKILL_DISABLE );

    // Use accumulation pixel shader
    D3DDevice_SetPixelShader( m_dwAccumulatePixelShader );

    // Set the PixelShader constant for streak length
    D3DXVECTOR4 vStreakLength( m_fStreakLength, m_fStreakLength, 
                               m_fStreakLength, m_fStreakLength );

    D3DDevice_SetPixelShaderConstant( 0, vStreakLength, 1 );

    // For screen-space texture-mapped quadrilateral
    g_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW|D3DFVF_TEX2 );   

    // Prepare quadrilateral vertices
    float x0 = (float)pRectDst->left;
    float y0 = (float)pRectDst->top;
    float x1 = (float)pRectDst->right;
    float y1 = (float)pRectDst->bottom;
    struct Quad
    {
        float x, y, z, w1;
        struct uv
        {
            float u0, v0;
            float u1, v1;
        }
        tex;  
    } 
    aQuad[4] =
    { //  X   Y     Z   1/W     u0  v0      u1  v1      u2  v2      u3  v3
        {x0, y0, 1.0f, 1.0f, }, // texture coords are set below
        {x1, y0, 1.0f, 1.0f, },
        {x0, y1, 1.0f, 1.0f, },
        {x1, y1, 1.0f, 1.0f, }
    };

    // Set rendering to just the destination rect
    g_pd3dDevice->SetScissors( 1, FALSE, (D3DRECT *)pRectDst );

  
    // Draw a quad for each block of 4 filter coefficients
    float u0 = (float)pRectSrc1->left;
    float v0 = (float)pRectSrc1->top;
    float u1 = (float)pRectSrc1->right;
    float v1 = (float)pRectSrc1->bottom;

    if( XGIsSwizzledFormat( descSrc1.Format ) )
    {
        float fWidthScale = 1.f / (float)descSrc1.Width;
        float fHeightScale = 1.f / (float)descSrc1.Height;
        u0 *= fWidthScale;
        v0 *= fHeightScale;
        u1 *= fWidthScale;
        v1 *= fHeightScale;
    }
    
    aQuad[0].tex.u0 = u0;
    aQuad[0].tex.v0 = v0;
    aQuad[1].tex.u0 = u1;
    aQuad[1].tex.v0 = v0;
    aQuad[2].tex.u0 = u0;
    aQuad[2].tex.v0 = v1;
    aQuad[3].tex.u0 = u1;
    aQuad[3].tex.v0 = v1;

    u0 = (float)pRectSrc2->left;
    v0 = (float)pRectSrc2->top;
    u1 = (float)pRectSrc2->right;
    v1 = (float)pRectSrc2->bottom;

    if( XGIsSwizzledFormat( descSrc2.Format ) )
    {
        float fWidthScale = 1.f / (float)descSrc2.Width;
        float fHeightScale = 1.f / (float)descSrc2.Height;
        u0 *= fWidthScale;
        v0 *= fHeightScale;
        u1 *= fWidthScale;
        v1 *= fHeightScale;
    }
    aQuad[0].tex.u1 = u0;
    aQuad[0].tex.v1 = v0;
    aQuad[1].tex.u1 = u1;
    aQuad[1].tex.v1 = v0;
    aQuad[2].tex.u1 = u0;
    aQuad[2].tex.v1 = v1;
    aQuad[3].tex.u1 = u1;
    aQuad[3].tex.v1 = v1;

    // Draw the quad
    g_pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP,
                                   2, aQuad, sizeof(Quad) ); 

    g_pd3dDevice->SetTexture( 0, NULL );
    g_pd3dDevice->SetTexture( 1, NULL );


    // Restore render target and zbuffer
    g_pd3dDevice->SetRenderTarget( pRenderTarget, pZBuffer );
    if( pRenderTarget ) pRenderTarget->Release();
    if( pZBuffer )      pZBuffer->Release();

    D3DDevice_SetScreenSpaceOffset( 0.0f, 0.0f );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: HotBlur()
// Desc: Blur backbuffer and set m_pBlur to the current blur texture.  Calls
//       FilterCopy with different filter coefficients and offsets
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::HotBlur( BOOL bAccumulate )
{
    // Make D3DTexture wrapper around current render target
    LPDIRECT3DSURFACE8 pRenderTarget;
    g_pd3dDevice->GetRenderTarget( &pRenderTarget );
    if( pRenderTarget == NULL )
        return E_FAIL;

    D3DSURFACE_DESC descRenderTarget;
    pRenderTarget->GetDesc( &descRenderTarget );
    D3DTexture RenderTargetTexture;
    ZeroMemory( &RenderTargetTexture, sizeof(RenderTargetTexture) );
    XGSetTextureHeader( descRenderTarget.Width, descRenderTarget.Height,
                        1, 0, descRenderTarget.Format, 0,
                        &RenderTargetTexture, pRenderTarget->Data,
                        descRenderTarget.Width * 4 );
    pRenderTarget->Release();
    
    // Filters align to blurriest point in supersamples, on the pixel centers
    //   This takes advantage of the bilinear filtering in the texture map lookup.
    FilterSample YFilter[] =        // 1221 4-tap filter in Y
    {
        { 2.0f/6.f,  0.0f,  1.0f },
        { 1.0f/6.f,  0.0f,  3.0f },
        { 2.0f/6.f,  0.0f, -1.0f },
        { 1.0f/6.f,  0.0f, -3.0f },
    };
    FilterSample XFilter[] =        // 1221 4-tap filter in X
    {
        { 2.0f/6.f,  1.0f, 0.0f },
        { 1.0f/6.f,  3.0f, 0.0f },
        { 2.0f/6.f, -1.0f, 0.0f },
        { 1.0f/6.f, -3.0f, 0.0f },
    };

    FilterSample XYFilter[] =        // Pass filter
    {
        { 1.0f, 0.0f, 0.0f },
    };


    D3DTexture *pTextureSrc;
    D3DTexture *pTextureDst;

    // extract "hot" portion of the image with downsampling
    pTextureSrc = &RenderTargetTexture; // source is backbuffer  
    pTextureDst = m_rpHotImage;    // destination is blur texture
    ExtractHot( pTextureDst, pTextureSrc, 2, 2 );

    // 2 passes: Vertical gaussian (1221) followed by
    // horizontal gaussian (1221), with 2x2 downsampling
    
    pTextureSrc = pTextureDst;    // source is next blur texture
    pTextureDst = m_rpBlur[0];    // destination is blur texture
    FilterCopy(pTextureDst, pTextureSrc, 4, YFilter, 1, 2);

        
    pTextureSrc = pTextureDst;  // source is previous blur texture
    pTextureDst = m_rpBlur[1];  // destination is next blur texture
    FilterCopy(pTextureDst, pTextureSrc, 4, XFilter, 2, 1);

    // The only difference between normal high dynamic range
    // and adding motion blur is right here.
    if( bAccumulate )
    {
        // Blend the previous and current frames.
        Accumulate( m_pBlur, m_pPreviousBlur, pTextureDst, 1, 1 );

        // Copy the result to the previous for next frame.
        LPDIRECT3DSURFACE8 pSurfaceSrc = NULL;
        m_pBlur->GetSurfaceLevel( 0, &pSurfaceSrc );

        LPDIRECT3DSURFACE8 pSurfaceDst = NULL;
        m_pPreviousBlur->GetSurfaceLevel( 0, &pSurfaceDst );

        D3DDevice_CopyRects( pSurfaceSrc, NULL, 0, pSurfaceDst, NULL );
        pSurfaceSrc->Release();
        pSurfaceDst->Release();
    }
    else
        m_pBlur = pTextureDst;
    
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: ClearTexture()
// Desc: Sets texture as render target and clears it to the smallest color
//       value that the accumulation buffer can get down to.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::ClearTexture( LPDIRECT3DTEXTURE8 pTexture )
{
    // Save current render target and depth buffer
    LPDIRECT3DSURFACE8 pRenderTarget, pZBuffer;
    D3DDevice_GetRenderTarget( &pRenderTarget );
    D3DDevice_GetDepthStencilSurface( &pZBuffer );

    // Set destination as render target
    LPDIRECT3DSURFACE8 pSurface = NULL;
    pTexture->GetSurfaceLevel( 0, &pSurface );
    D3DDevice_SetRenderTarget( pSurface, NULL );  // no depth-buffering
    pSurface->Release();

    // Clear the back and zbuffer
    D3DDevice_Clear( 0, NULL, D3DCLEAR_TARGET, 0x00121212, 1.0f, 0L );

    // Restore render target and zbuffer
    g_pd3dDevice->SetRenderTarget( pRenderTarget, pZBuffer );
    if( pRenderTarget ) pRenderTarget->Release();
    if( pZBuffer )      pZBuffer->Release();
    
    return S_OK;
}

