//-----------------------------------------------------------------------------
// File: DynamicGammaController.h
//
// Desc: Example code showing how to update the Xbox gamma ramp dynamically.
//
// Hist: 04.01.03 - New for 2003 April XDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef DYNAMIC_GAMMA_CONTROLLER_H
#define DYNAMIC_GAMMA_CONTROLLER_H


//-----------------------------------------------------------------------------
// Name: Luminance pixel shader
// Desc: Transforms RGB to luminance using the luminance transform:
//       lum = 0.3R + 0.59G + 0.11B
// Code: xps.1.1
//
//       def c0, 0.3f, 0.59f, 0.11f, 0.0f ; Luminance transform
//
//       tex t0
//
//       dp3 r0, c0, t0
//       xfc zero, zero, zero, r0, zero, zero, zero 
//-----------------------------------------------------------------------------
DWORD dwLumPixelShaderCode[] = 
{
    0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x0000000c,
    0x00000080, 0x004d961c, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0xc1c80000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x000820c0, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00011101, 0x00000001, 0x00000000, 0x00000000,
    0xfffffff0, 0xffffffff, 0x000001ff
};




//-----------------------------------------------------------------------------
// Name: Inverse gamma pixel shader
// Desc: Transforms RGB by the inverse of the current gamma ramp to compensate
//       for gamma ramp adjustments of HUD elements.
// Code: xps.1.1
//
//       def c0, 1.0f, 0.0f, 0.0f, 0.0f
//       def c1, 0.0f, 1.0f, 0.0f, 0.0f
//       def c2, 0.0f, 0.0f, 1.0f, 0.0f
//
//       tex t0            ; Color texture
//       texreg2ar t1, t0  ; Look up r  
//       texreg2gb t2, t0  ; Look up gb 
// 
//       ; r0 = invgamma(r), invgamma(g), 0, 0
//       xmma discard, discard, r0, t1, c0, t2, c1
// 
//       ; r0 = invgamma(r), invgamma(g), invgamma(b), 0
//       xmma discard, discard, r0, t2, c2, r0, 1-Zero
//
//       ; Output r0.rgb and t0.a
//       xfc zero, zero, zero, r0, zero, zero, t0.a
//-----------------------------------------------------------------------------
DWORD dwInvGammaPixelShaderCode[] = 
{
    0x00000000,
    0xdcd1dad2, 0xdad1dc30, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x0000000c,
    0x00001880, 0x000000ff, 0x00ff0000, 0x000000ff,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x0000ff00, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000c00, 0x00000c00,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0xc9c10000, 0xccc1cac2, 0xcac1cc20,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x000820c0, 0x00000c00, 0x00000c00, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00011103, 0x000041e1, 0x00000000, 0x00000000,
    0xfffff202, 0xffffff1f, 0x000001ff
};




//-----------------------------------------------------------------------------
// Name: struct Histogram
// Desc: Contains the data generated from the luminance texture
//-----------------------------------------------------------------------------
struct Histogram  
{
    FLOAT Lums[256]; // Normalized number of samples present at each luminance level
    FLOAT fMin;      // Smallest luminance value above min/max threshold
    FLOAT fMax;      // Largest luminance value below min/max threshold
    FLOAT fAvg;      // Average luminance value
};




//-----------------------------------------------------------------------------
// Name: class DynamicGammaController
// Desc: Handles luminance sampling and gamma updates
//-----------------------------------------------------------------------------
class DynamicGammaController
{
    DWORD       m_dwLuminancePixelShader; // Luminance pixel shader
    FLOAT       m_fMin;                   // Current min
    FLOAT       m_fMax;                   // Current max
    FLOAT       m_fAvg;                   // Current average
    Histogram   m_Histogram;

    D3DTexture* m_pLuminanceTexture;      // Down sampled luminance texture
    BYTE*       m_pCachedBuffer;          // Cached buffer for luminance texture values

    DWORD       m_dwInvGammaPixelShader;  // Inverse gamma pixel shader to maintain
    D3DTexture* m_pInvGammaRampLUT;       // constant gamma for HUD elements

public:
    VOID        Initialize();   // Initialize values and resources   
    VOID        Free();         // Free resources, reset gamma ramp to linear

    // In order to dynamically update the gamma ramp based on scene luminance,
    // call the following functions in this order:
    // 1) GetLuminance
    // 2) IsRenderingLuminance (make sure GPU is finished rendering luminance
    //    texture)
    // 3) GetHistogram
    // 4) UpdateGammaValues
    // 5) SetGammaRamp
    // 
    // Note that for performance reasons, the functions may not be called all
    // in the same frame

    // Get luminance texture from source texture
    VOID        GetLuminance( D3DTexture* pTextureSrc, UINT nSuperSampleX,
                                                       UINT nSuperSampleY );

    // Is the GPU still rendering the luminance texture?
    BOOL        IsRenderingLuminance();

    // Calculate histogram from luminance texture
    VOID        GetHistogram();
    
    // Update current gamma ramp min, max, and average
    VOID        UpdateGammaValues( FLOAT fDt );

    // Update RGB gamma ramps
    VOID        SetGammaRamp();
    
    // Draw histogram and gamma ramps in the supplied screen space rectangles
    VOID        DrawHistogram( FLOAT fX1, FLOAT fY1, FLOAT fX2, FLOAT fY2 );
    VOID        DrawGammaRamps( FLOAT fX1, FLOAT fY1, FLOAT fX2, FLOAT fY2 );

    // Sets up the pixel shader and textures required to draw a HUD
    // element with constant gamma
    VOID        SetInvGammaCorrection();

    // Effect adjustments

    // Gamma ramp bias
    FLOAT       m_fBias;             

    // Maximum gamma ramp adjustment for dark scene compensation
    FLOAT       m_fDarkClamp;  

    // Maximum gamma ramp adjustment for bright scene compensation
    FLOAT       m_fLightClamp;       

    // Gamma ramp darkness adjustment with respect to time
    FLOAT       m_fDarkAdjustDt;   

    // Gamma ramp brightness adjustment with respect to time
    FLOAT       m_fLightAdjustDt; 

    // Gamma ramp contrast adjustment with respect to time
    FLOAT       m_fContrastAdjustDt; 

    // Area underneath the dark side of the normalized histogram where
    // luminance values are considered 0.  Used to filter out dark outliners.
    FLOAT       m_fDarkThreshold;    

    // Area underneath the bright side of the normalized histogram where
    // luminance values are considered 0.  Used to filter out bright outliners.
    FLOAT       m_fLightThreshold;   
    
};




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: 
//-----------------------------------------------------------------------------
VOID DynamicGammaController::Initialize()
{
    // Create luminance pixel shader
    D3DDevice::CreatePixelShader( (D3DPIXELSHADERDEF*)dwLumPixelShaderCode,
                                  &m_dwLuminancePixelShader );

    // Create inverse gamma pixel shader
    D3DDevice::CreatePixelShader( (D3DPIXELSHADERDEF*)dwInvGammaPixelShaderCode,
                                  &m_dwInvGammaPixelShader );

    // Set default values
    m_fBias             = 0.8f;
    m_fDarkClamp        = 0.4f;
    m_fLightClamp       = 0.4f;
    m_fDarkAdjustDt     = 0.10f;
    m_fLightAdjustDt    = 0.20f;
    m_fContrastAdjustDt = 0.07f;
    m_fDarkThreshold    = 0.010f;
    m_fLightThreshold   = 0.010f;
    m_fMin              = 0.0f;
    m_fMax              = 1.0f;
    m_fAvg              = 0.5f;

    m_pLuminanceTexture = NULL;
    m_pCachedBuffer     = NULL;
    ZeroMemory( &m_Histogram, sizeof(Histogram));

    // Create the inverse gamma ramp texture, for use with the pixel shader
    // which maps colors through an inverse of the gamma ramp. This is so
    // that when the gamma ramp is applied globally, HUD elements will still
    // appear properly. Note that these textures will be addressed with tex
    // coords ranging from [0..1], so the textures must be swizzled.
    D3DDevice::CreateTexture( 64, 64, 1, 0, D3DFMT_G8B8, 0, &m_pInvGammaRampLUT );
}




//-----------------------------------------------------------------------------
// Name: Free()
// Desc: Frees resource required by the controller and resets the gamma ramp
//       back to linear
//-----------------------------------------------------------------------------
VOID DynamicGammaController::Free()
{
    // Free resources
    if( m_pLuminanceTexture )
        m_pLuminanceTexture->Release();
    delete [] m_pCachedBuffer;
    D3DDevice::DeletePixelShader( m_dwLuminancePixelShader );
    D3DDevice::DeletePixelShader( m_dwInvGammaPixelShader );
    m_pInvGammaRampLUT->Release();

    // Set linear gamma ramp
    D3DGAMMARAMP Ramp;
    for( UINT i = 0; i < 256; i++ )
    {
        Ramp.red[i] = Ramp.green[i] = Ramp.blue[i] = BYTE(i);
    }
    D3DDevice::SetGammaRamp( 0, &Ramp );
}




//-----------------------------------------------------------------------------
// Name: Clamp()
// Desc: 
//-----------------------------------------------------------------------------
template<class T>
__forceinline VOID Clamp( T* pVal, T Min, T Max )
{
    if( *pVal < Min )      *pVal = Min;
    else if( *pVal > Max ) *pVal = Max;
}




//-----------------------------------------------------------------------------
// Name: InRenderingLuminance()
// Desc: Used to determine whether the luminance texture is locked by the GPU
//-----------------------------------------------------------------------------
BOOL DynamicGammaController::IsRenderingLuminance()
{
    if( !m_pLuminanceTexture )
        return FALSE;
    return m_pLuminanceTexture->IsBusy();
}




//-----------------------------------------------------------------------------
// Name: GetLuminance()
// Desc: Renders a down sampled luminance transformed version of the input
//       texture
//-----------------------------------------------------------------------------
VOID DynamicGammaController::GetLuminance( D3DTexture* pTextureSrc,
                                           UINT nSuperSampleX,
                                           UINT nSuperSampleY )
{
    // Save current render target and depth buffer
    LPDIRECT3DSURFACE8 pRenderTarget, pZBuffer;
    D3DDevice::GetRenderTarget( &pRenderTarget );
    D3DDevice::GetDepthStencilSurface( &pZBuffer );

    // Texture space pixel center == screen space pixel center
    D3DDevice::SetScreenSpaceOffset( -0.5f, -0.5f );

    static UINT LastSuperSampleX = 0;
    static UINT LastSuperSampleY = 0;

    // Get the source texture description
    D3DSURFACE_DESC descSrc;
    pTextureSrc->GetLevelDesc( 0, &descSrc );

    // update destination texture if the source or super sample amounts change
    if( m_pLuminanceTexture == NULL ||
        LastSuperSampleX != nSuperSampleX ||
        LastSuperSampleY != nSuperSampleY )
    {
        if( m_pLuminanceTexture )
            m_pLuminanceTexture->Release();
        D3DDevice::CreateTexture( descSrc.Width / nSuperSampleX,
                                  descSrc.Height / nSuperSampleY,
                                  1, 0, D3DFMT_LIN_L8, 0,
                                  &m_pLuminanceTexture);

        D3DSURFACE_DESC desc;
        m_pLuminanceTexture->GetLevelDesc( 0, &desc );
        delete [] m_pCachedBuffer;
        m_pCachedBuffer = new BYTE[desc.Size];
    }

    // Set destination as render target
    LPDIRECT3DSURFACE8 pSurface = NULL;
    m_pLuminanceTexture->GetSurfaceLevel( 0, &pSurface );
    D3DDevice::SetRenderTarget( pSurface, NULL );  // no depth-buffering
    pSurface->Release();
    
    // Get description of dest
    D3DSURFACE_DESC descDst;
    m_pLuminanceTexture->GetLevelDesc( 0, &descDst );

    //Set render state for filtering
    D3DDevice::SetRenderState( D3DRS_LIGHTING, FALSE );
    D3DDevice::SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
    D3DDevice::SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
    D3DDevice::SetRenderState( D3DRS_ZENABLE, D3DZB_FALSE );
    D3DDevice::SetRenderState( D3DRS_STENCILENABLE, FALSE );
    D3DDevice::SetRenderState( D3DRS_FOGENABLE, FALSE );
    D3DDevice::SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    
    D3DDevice::SetTexture( 0, pTextureSrc);
    D3DDevice::SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_DISABLE );
    D3DDevice::SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
    
    // Pass texture coords without transformation
    D3DDevice::SetTextureStageState( 0, D3DTSS_TEXTURETRANSFORMFLAGS,
                                        D3DTTFF_DISABLE );  
    // Each texture has different tex coords
    D3DDevice::SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 ); 
    D3DDevice::SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP );
    D3DDevice::SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP );
    D3DDevice::SetTextureStageState( 0, D3DTSS_MAXMIPLEVEL, 0 );
    D3DDevice::SetTextureStageState( 0, D3DTSS_MIPFILTER, D3DTEXF_NONE );

    D3DDevice::SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_POINT );
    D3DDevice::SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_POINT );
    D3DDevice::SetTextureStageState( 0, D3DTSS_COLORKEYOP, D3DTCOLORKEYOP_DISABLE );
    D3DDevice::SetTextureStageState( 0, D3DTSS_COLORSIGN, 0 );
    D3DDevice::SetTextureStageState( 0, D3DTSS_ALPHAKILL, D3DTALPHAKILL_DISABLE );
    
    // Set luminance transform pixel shader
    D3DDevice::SetPixelShader( m_dwLuminancePixelShader );

    // For screen-space texture-mapped quadrilateral
    D3DDevice::SetVertexShader( D3DFVF_XYZRHW|D3DFVF_TEX1 );   

    // Prepare quadrilateral vertices
    FLOAT x0 = 0.0f;
    FLOAT y0 = 0.0f;
    FLOAT x1 = (FLOAT)descDst.Width;
    FLOAT y1 = (FLOAT)descDst.Height;
    
    // Draw a quad 
    FLOAT tu0 = 0.0f;
    FLOAT tv0 = 0.0f;
    FLOAT tu1 = (FLOAT)descSrc.Width;
    FLOAT tv1 = (FLOAT)descSrc.Height;

    if( XGIsSwizzledFormat( descSrc.Format ) )
    {
        tu1 = 1.0f;
        tv1 = 1.0f;
    }
    
    struct QUADVERTEX
    {
        FLOAT x, y, z, w;
        FLOAT tu, tv;
    };
    QUADVERTEX aQuad[4] =
    {   // X   Y   Z     W    tu   tv
        { x0, y0, 1.0f, 1.0f, tu0, tv0 },
        { x1, y0, 1.0f, 1.0f, tu1, tv0 },
        { x1, y1, 1.0f, 1.0f, tu1, tv1 },
        { x0, y1, 1.0f, 1.0f, tu0, tv1 },
    };

    D3DDevice::DrawPrimitiveUP( D3DPT_QUADLIST, 1, aQuad, sizeof(QUADVERTEX) ); 
        
    // Restore render target and z buffer
    D3DDevice::SetRenderTarget( pRenderTarget, pZBuffer );
    if( pRenderTarget ) pRenderTarget->Release();
    if( pZBuffer )      pZBuffer->Release();

    // Restore state
    D3DDevice::SetTexture( 0, NULL );
    D3DDevice::SetScreenSpaceOffset( 0.0f, 0.0f );
    D3DDevice::SetPixelShader( NULL );
};




//-----------------------------------------------------------------------------
// Name: GetHistogram()
// Desc: Builds a histogram from the current down sampled luminance texture
//-----------------------------------------------------------------------------
VOID DynamicGammaController::GetHistogram()
{
    ZeroMemory( &m_Histogram, sizeof(Histogram) );

    D3DSURFACE_DESC desc;
    m_pLuminanceTexture->GetLevelDesc( 0, &desc );

    // Get sample values and copy them into cached buffer for faster reads
    D3DLOCKED_RECT Rect;
    m_pLuminanceTexture->LockRect( 0, &Rect, NULL, D3DLOCK_READONLY|D3DLOCK_NOOVERWRITE );
    memcpy( m_pCachedBuffer, Rect.pBits, desc.Size );
    m_pLuminanceTexture->UnlockRect( 0 );

    for( UINT i = 0; i < desc.Height; i++ )
    {
        for( UINT j = 0; j < desc.Width; j++ )
        {
            BYTE index = m_pCachedBuffer[i*Rect.Pitch + j];
            m_Histogram.Lums[index] += 1.0f;
        }
    }

    // Normalize histogram
    FLOAT fSum = 0.0f;
    for( UINT i = 0; i < 256; i++ )
        fSum += m_Histogram.Lums[i];
    if( fSum != 0.0f )
    {
        FLOAT fInvSum = 1.0f/fSum;
        for( UINT i = 0; i < 256; i++ )
            m_Histogram.Lums[i] *= fInvSum;
    }

    // Compute current min, max, and average
    FLOAT fMinArea;
    FLOAT fArea;
    
    fMinArea = m_fDarkThreshold;
    fArea = 0.0f;
    for( int i = 0; i < 256; i++ )
    {
        fArea += m_Histogram.Lums[i];
        if( fArea >= fMinArea )
        {
            m_Histogram.fMin = FLOAT(i)/255.0f;
            break;
        }
    }
    
    fMinArea = m_fLightThreshold;
    fArea = 0.0f;
    for( int i = 255; i > -1; i-- )
    {
        fArea += m_Histogram.Lums[i];
        if( fArea >= fMinArea )
        {
            m_Histogram.fMax = FLOAT(i)/255.0f;
            break;
        }
    }

    m_Histogram.fAvg = 0.0f;
    for( int i = 0; i < 256; i++ )
    {
        m_Histogram.fAvg += FLOAT(i) * m_Histogram.Lums[i];
    }
    m_Histogram.fAvg /= 255.0f;
}




//-----------------------------------------------------------------------------
// Name: UpdateGammaValues()
// Desc: Updates the gamma ramp parameters over time
//-----------------------------------------------------------------------------
VOID DynamicGammaController::UpdateGammaValues( FLOAT fDt )
{
    const FLOAT fNoChangeLumThreshold = 0.01f;
    const FLOAT fNoChangeContThreshold = 0.01f;

    // Update if change over dt
    if( m_Histogram.fMin < m_fMin - fNoChangeLumThreshold )
        m_fMin -= m_fDarkAdjustDt * fDt;
    else if( m_Histogram.fMin > m_fMin + fNoChangeLumThreshold )
        m_fMin += m_fLightAdjustDt * fDt; 

    if( m_Histogram.fMax < m_fMax - fNoChangeLumThreshold )
        m_fMax -= m_fDarkAdjustDt * fDt; 
    else if( m_Histogram.fMax > m_fMax + fNoChangeLumThreshold )
        m_fMax += m_fLightAdjustDt * fDt;  

    if( m_Histogram.fAvg < m_fAvg - fNoChangeContThreshold )
        m_fAvg -= m_fContrastAdjustDt * fDt;
    else if( m_Histogram.fAvg > m_fAvg + fNoChangeContThreshold )
        m_fAvg += m_fContrastAdjustDt * fDt;

    Clamp( &m_fMin, 0.0f, 1.0f );
    Clamp( &m_fMax, m_fDarkClamp, 1.0f );
    Clamp( &m_fMin, 0.0f, m_fLightClamp );
    Clamp( &m_fAvg, m_fMin, m_fMax );
}




//-----------------------------------------------------------------------------
// Name: SetGammaRamp()
// Desc: Sets the gamma ramp and updates the inverse gamma textures
//-----------------------------------------------------------------------------
VOID DynamicGammaController::SetGammaRamp()
{
    D3DGAMMARAMP Ramp;

    // Compute normalized distance of fAvg [0,1] between min and max
    FLOAT Dist = (m_fAvg - m_fMin)/(m_fMax - m_fMin);

    // Set curve based on dist
    FLOAT P = 1.0f + Dist*m_fBias;
    
    for( UINT i = 0; i < 256; i++ )
    {
        FLOAT x = i/255.0f;
        FLOAT y;
        if( x <= m_fMin )
            y = 0.0f;
        else if( x >= m_fMax )
            y = 1.0f;
        else
        {
            y = powf( (x - m_fMin) / (m_fMax - m_fMin), P );
            Clamp( &y, 0.0f, 1.0f );
        }
        Ramp.red[i] = Ramp.green[i] = Ramp.blue[i] = BYTE(y * 255.0f + 0.5f);
    }

    D3DDevice::SetGammaRamp( 0, &Ramp );

    // Update inverse gamma ramp textures
    BYTE  InvGammaRamp[256];
    FLOAT InvP = 1.0f/P;
    for( UINT i = 0; i < 256; i++ )
    {
        FLOAT y = i/255.0f;
        FLOAT x = (m_fMax - m_fMin) * powf( y, InvP ) + m_fMin;
        Clamp( &x, 0.0f, 1.0f );
        InvGammaRamp[i] = (BYTE)(x * 255.0f + 0.5f);
    }

    // Copy the gamma ramp to the textures.
    D3DLOCKED_RECT lock;
    static WORD data[64][64];

    // Create the LUT texture
    WORD* pBitsGB = &data[0][0];
    for( DWORD y=0; y<64; y++ )
    {
        WORD b = InvGammaRamp[y*4];

        for( DWORD x=0; x<64; x++ )
        {
            WORD g = InvGammaRamp[x*4];

            *pBitsGB++ = (g<<8)|(b<<0);
        }
    }
    m_pInvGammaRampLUT->LockRect( 0, &lock, NULL, 0);
    XGSwizzleRect( data, 0, NULL, lock.pBits, 64, 64, NULL, sizeof(WORD) );
    m_pInvGammaRampLUT->UnlockRect( 0 );
}




//-----------------------------------------------------------------------------
// Name: DrawHistogram()
// Desc: Draws the current histogram.  Used for debugging
//-----------------------------------------------------------------------------
VOID DynamicGammaController::DrawHistogram( FLOAT fX1, FLOAT fY1,
                                            FLOAT fX2, FLOAT fY2 )
{
    FLOAT dx = fX2 - fX1;
    FLOAT dy = fY2 - fY1;

    D3DDevice::SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
    D3DDevice::SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TFACTOR );
    D3DDevice::SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
    D3DDevice::SetRenderState( D3DRS_TEXTUREFACTOR,    0x00000000 );
    D3DDevice::SetRenderState( D3DRS_ZENABLE,          D3DZB_FALSE );
    D3DDevice::SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    D3DDevice::SetVertexShader( D3DFVF_XYZRHW );

    // Draw black area
    FLOAT Quads[4][4] = 
    {
        { fX1, fY1, 0.0f, 0.0f },
        { fX2, fY1, 0.0f, 0.0f },
        { fX2, fY2, 0.0f, 0.0f },
        { fX1, fY2, 0.0f, 0.0f },
    };
    D3DDevice::DrawVerticesUP( D3DPT_QUADLIST, 4, Quads, sizeof(Quads[0]) );

    D3DXVECTOR4 Zones[3];

    for( UINT i = 0; i < 3; i++ )
    {
        FLOAT fVal;
        if( i == 0 )
        {
            fVal = m_Histogram.fMin;
            D3DDevice::SetRenderState( D3DRS_TEXTUREFACTOR, 0x00ffffff );
        }
        else if( i == 1 )
        {
            fVal = m_Histogram.fMax;
            D3DDevice::SetRenderState( D3DRS_TEXTUREFACTOR, 0x00ffffff );
        }
        else
        {
            fVal = m_Histogram.fAvg;
            D3DDevice::SetRenderState( D3DRS_TEXTUREFACTOR, 0x00aaaaaa );
        }

        Zones[0].x = fX1 + dx * ( 0.9f*fVal + 0.1f/2.0f );
        Zones[0].y = fY2 - dy * ( 0.1f/2.0f );
        Zones[0].z = 0.0f;
        Zones[0].w = 0.0f;
        Zones[1].x = fX1 + dx * ( 0.9f*fVal + 0.1f/2.0f + 0.1f/2.0f );
        Zones[1].y = fY2;
        Zones[1].z = 0.0f;
        Zones[1].w = 0.0f;
        Zones[2].x = fX1 + dx * ( 0.9f*fVal + 0.1f/2.0f - 0.1f/2.0f );
        Zones[2].y = fY2;
        Zones[2].z = 0.0f;
        Zones[2].w = 0.0f;

        D3DDevice::DrawVerticesUP( D3DPT_TRIANGLELIST, 3, Zones, sizeof(D3DXVECTOR4) );
    }

    // Draw histogram
    D3DXVECTOR4 Lines[256 * 2];
    D3DDevice::SetRenderState( D3DRS_TEXTUREFACTOR, 0x00ff0000 );

    FLOAT fMaxLum = 0;
    for( UINT i = 0; i < 256; i++ )
    {
        fMaxLum = (m_Histogram.Lums[i] > fMaxLum) ? m_Histogram.Lums[i] : fMaxLum;
    }

    if( fMaxLum == 0.0f )
        fMaxLum = 1.0f;
    FLOAT fInvMaxLum = 1.0f/fMaxLum;
    
    for( UINT i = 0; i < 256; i++ )
    {
        Lines[2*i+0].x = fX1 + dx * ( i*0.9f/255.0f + 0.1f/2.0f );
        Lines[2*i+0].y = fY2 - dy * ( 0.1f/2.0f );
        Lines[2*i+0].z = 0.0f;
        Lines[2*i+0].w = 0.0f;
        Lines[2*i+1].x = fX1 + dx * ( i*0.9f/255.0f + 0.1f/2.0f );
        Lines[2*i+1].y = fY2 - dy * ( 0.1f/2.0f + 0.9f*m_Histogram.Lums[i]*fInvMaxLum );
        Lines[2*i+1].z = 0.0f;
        Lines[2*i+1].w = 0.0f;
    }
    D3DDevice::DrawVerticesUP( D3DPT_LINELIST, 256 * 2, Lines, sizeof(Lines[0]) );

    // Restore state
    D3DDevice::SetRenderState( D3DRS_ZENABLE, D3DZB_TRUE );
    D3DDevice::SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
}




//-----------------------------------------------------------------------------
// Name: DrawGammaRamps()
// Desc: Draws the current gamma ramps and inverse gamma. Used for debugging
//-----------------------------------------------------------------------------
VOID DynamicGammaController::DrawGammaRamps( FLOAT fX1, FLOAT fY1,
                                             FLOAT fX2, FLOAT fY2 )
{
    // Draw black area
    D3DDevice::SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 );
    D3DDevice::SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TFACTOR );
    D3DDevice::SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
    D3DDevice::SetRenderState( D3DRS_TEXTUREFACTOR, 0x00000000 );
    D3DDevice::SetRenderState( D3DRS_ZENABLE, D3DZB_FALSE );
    D3DDevice::SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    D3DDevice::SetVertexShader( D3DFVF_XYZRHW );

    FLOAT Quads[4][4] = 
    {
        { fX1, fY1, 0.0f, 0.0f },
        { fX2, fY1, 0.0f, 0.0f },
        { fX2, fY2, 0.0f, 0.0f },
        { fX1, fY2, 0.0f, 0.0f },
    };
    D3DDevice::DrawVerticesUP( D3DPT_QUADLIST, 4, Quads, sizeof(Quads[0]) );

    // Draw gamma ramp
    D3DGAMMARAMP Ramp;
    D3DDevice::GetGammaRamp( &Ramp );

    D3DXVECTOR4 Lines[256];
    D3DDevice::SetRenderState( D3DRS_TEXTUREFACTOR, 0x00ffffff );
    FLOAT dx = (fX2 - fX1) / 255.0f;
    FLOAT dy = (fY2 - fY1) / 255.0f;

    // Draw red channel of gamma ramp
    D3DDevice::SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED );
    for( UINT i = 0; i < 256; i++ )
    {
        Lines[i].x = fX1 + dx * i;
        Lines[i].y = fY2 - dy * Ramp.red[i];
        Lines[i].z = 0.0f;
        Lines[i].w = 0.0f;
    }
    D3DDevice::DrawVerticesUP( D3DPT_LINESTRIP, 256, Lines, sizeof(Lines[0]) );

    // Draw green channel of gamma ramp
    D3DDevice::SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_GREEN );
    for( UINT i = 0; i < 256; i++ )
    {
        Lines[i].y = fY2 - dy * Ramp.green[i];
    }
    D3DDevice::DrawVerticesUP( D3DPT_LINESTRIP, 256, Lines, sizeof(Lines[0]) );

    // Draw blue channel of gamma ramp
    D3DDevice::SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_BLUE );
    for( UINT i = 0; i < 256; i++ )
    {
        Lines[i].y = fY2 - dy * Ramp.blue[i];
    }
    D3DDevice::DrawVerticesUP( D3DPT_LINESTRIP, 256, Lines, sizeof(Lines[0]) );

    // Restore state
    D3DDevice::SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL );
    D3DDevice::SetRenderState( D3DRS_ZENABLE, D3DZB_TRUE );
    D3DDevice::SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
}




//-----------------------------------------------------------------------------
// Name: SetInvGammaCorrection()
// Desc: Sets the pixel shader, textures, and state required to draw objects
//       (for example, UI elements) with linear gamma. (In essence, the pixel
//       shader undoes (or compensates for) the dynamic gamma.) See the inv
//       gamma pixel shader code for details.
//-----------------------------------------------------------------------------
VOID DynamicGammaController::SetInvGammaCorrection()
{
    // Setup inverse gamma textures
    D3DDevice::SetTextureStageState( 1, D3DTSS_MIPFILTER, D3DTEXF_NONE );
    D3DDevice::SetTextureStageState( 1, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    D3DDevice::SetTextureStageState( 1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    D3DDevice::SetTextureStageState( 2, D3DTSS_MIPFILTER, D3DTEXF_NONE );
    D3DDevice::SetTextureStageState( 2, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    D3DDevice::SetTextureStageState( 2, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    D3DDevice::SetTextureStageState( 1, D3DTSS_ADDRESSU,  D3DTADDRESS_CLAMP );
    D3DDevice::SetTextureStageState( 1, D3DTSS_ADDRESSV,  D3DTADDRESS_CLAMP );
    D3DDevice::SetTextureStageState( 2, D3DTSS_ADDRESSU,  D3DTADDRESS_CLAMP );
    D3DDevice::SetTextureStageState( 2, D3DTSS_ADDRESSV,  D3DTADDRESS_CLAMP );
    D3DDevice::SetTexture( 1, m_pInvGammaRampLUT );
    D3DDevice::SetTexture( 2, m_pInvGammaRampLUT );

    // Set inverse gamma pixel shader
    D3DDevice::SetPixelShader( m_dwInvGammaPixelShader );
}
    



#endif // DYNAMIC_GAMMA_CONTROLLER_H
