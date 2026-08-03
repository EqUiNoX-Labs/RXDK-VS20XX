//-----------------------------------------------------------------------------
// File: LensFlare.cpp
//
// Desc: Demonstrates a lens flare technique.  The technique uses visibility
//       testing and a back buffer copy of the alpha channel to achieve it's
//       effect.
//
// Hist: 02.26.02 - Created
//       01.03.03 - Code cleanup
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <xgraphics.h>
#include <xgmath.h>
#include <XBMesh.h>
#include <XBResource.h>




//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_BACK_BUTTON,  XBHELP_PLACEMENT_2, L"Display help" },
    { XBHELP_LEFTSTICK,    XBHELP_PLACEMENT_2, L"Move in-out\nand left-right" },
    { XBHELP_RIGHTSTICK,   XBHELP_PLACEMENT_2, L"Rotate and\ntilt view" },
    { XBHELP_DPAD,         XBHELP_PLACEMENT_2, L"Move up\nand down" },
};

#define NUM_HELP_CALLOUTS ( sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]) )




//-----------------------------------------------------------------------------
// Global message buffer
//-----------------------------------------------------------------------------
WCHAR strMessage[200];




//-----------------------------------------------------------------------------
// Name: struct SVert
// Desc: Vertices used to render light and flare elements
//-----------------------------------------------------------------------------
struct SVert
{
    XGVECTOR3 Pos;
    XGVECTOR2 UV;
};




//-----------------------------------------------------------------------------
// Name: struct SFlareElement
// Desc: Represents a flare element
//-----------------------------------------------------------------------------
struct SFlareElement
{
    D3DTexture* pTexture;   // Alpha texture
    XGVECTOR2   Size;       // Flare size in projection space
    D3DCOLOR    Color;      // Flare color and alpha
    FLOAT       fDistance;  // Distance along element axis
};




//-----------------------------------------------------------------------------
// Name: struct SLightElement
// Desc: Represents the flare light
//-----------------------------------------------------------------------------
struct SLightElement
{
    D3DTexture* pTexture;       // Alpha texture
    XGVECTOR2   BaseSize;       // Size of light center
    XGVECTOR2   FlareOutSize;   // Size of "flare out"
    UINT        uiNumFlareOuts; // Number of flare outs to draw between base and flareout size
    
    D3DCOLOR    Color;          // Light color and alpha
    XGVECTOR2   ViewCenter;     // Center point for element axis in projection space
};




//-----------------------------------------------------------------------------
// Name: class CLensflare
// Desc: encapsulates the lens flare
//-----------------------------------------------------------------------------
class CLensFlare
{
    SFlareElement*   m_pFlareElements;           // Array of flare elements
    UINT             m_uiNumFlareElements;       // Number of flare elements
    XGVECTOR2        m_MaxElementSize;           // Maximum size for an element (clamp)
    
    SLightElement*   m_pLightElement;            // Light element

    D3DTexture*      m_pBackBufferCopy;          // The copy of the back buffer used for flare out

    UINT             m_uiNumFullLightPixels;     // Number of visible pixels for the non-occluded light
   
    FLOAT            m_bPreviouslyFullyVisible;  // Hint not to copy back buffer

    D3DVertexBuffer* m_pQuadVB;                  // VB for light and flare elements

    HRESULT SetNumFullLightPixels();    // Sets the number of full light pixels
    int     RasterPos( FLOAT fNum );    // Determines where a vertex will be rendered

public:
    HRESULT Create( SLightElement* pLightElement,                        // Initialized the lens flare
                    SFlareElement* pFlareElements, UINT uiNumElements,
                    const XGVECTOR2& MaxFlareSize );
    
    HRESULT Render( const XGVECTOR3 WorldLightDirection,                 // Renders the lens flare
                    const XGMATRIX& ViewMatrix ); 
    CLensFlare();    
    ~CLensFlare();
};




//-----------------------------------------------------------------------------
// Name: CLensflare()
// Desc: Constructor
//-----------------------------------------------------------------------------
CLensFlare::CLensFlare()
{
    m_pFlareElements          = NULL;
    m_uiNumFlareElements      = 0;
    m_pLightElement           = NULL;
    m_pBackBufferCopy         = NULL;
    m_uiNumFullLightPixels    = 0;
    m_bPreviouslyFullyVisible = false;
    m_pQuadVB                 = NULL;
}




//-----------------------------------------------------------------------------
// Name: ~CLensflare()
// Desc: Destructor
//-----------------------------------------------------------------------------
CLensFlare::~CLensFlare()
{
    // Free light
    if( m_pLightElement )
        SAFE_RELEASE( m_pLightElement->pTexture );
    delete m_pLightElement;

    // Free flare elements
    for( UINT i = 0; i < m_uiNumFlareElements; i++ )
        SAFE_RELEASE( m_pFlareElements[i].pTexture );
    delete [] m_pFlareElements;

    // Free VB
    SAFE_RELEASE( m_pQuadVB );

    // Free back buffer copy
    SAFE_RELEASE( m_pBackBufferCopy );
}




//-----------------------------------------------------------------------------
// Name: Create()
// Desc: Initialized the flare.  Takes ownership of the light element and flare
//       elements array
//-----------------------------------------------------------------------------
HRESULT CLensFlare::Create( SLightElement* pLightElement, 
                            SFlareElement* pFlareElements, UINT uiNumFlareElements,
                            const XGVECTOR2& MaxElementSize )
{
    // Light element
    m_pLightElement = pLightElement;

    // Flare elements
    m_pFlareElements =  pFlareElements;
    m_uiNumFlareElements =  uiNumFlareElements;

    // Max element size
    m_MaxElementSize = MaxElementSize;

    // Never been rendered
    m_bPreviouslyFullyVisible = false;

    //-----------------------------------------------
    // Initialize a texture to receive the portion of
    // the pack buffer that the light is rendered to
    //-----------------------------------------------

    // Get back buffer
    D3DSURFACE_DESC SrcDesc;
    IDirect3DSurface8* pSrcSurface;
    g_pd3dDevice->GetBackBuffer( 0, D3DBACKBUFFER_TYPE_MONO, &pSrcSurface );
    pSrcSurface->GetDesc( &SrcDesc );

    // Initialize skybox projection
    XGMATRIX SkyBoxProjection;
    XGMatrixOrthoLH( &SkyBoxProjection, 2.0f, 2.0f * FLOAT(SrcDesc.Height)/FLOAT(SrcDesc.Width), 0.0f, 1.0f );

    // Get texture format and size in screen space
    XGVECTOR3 BaseSize( m_pLightElement->BaseSize.x, m_pLightElement->BaseSize.y, 0.0f );
    XGVec3TransformCoord( &BaseSize, &BaseSize, &SkyBoxProjection);
    UINT uiWidth =  UINT( SrcDesc.Width/2.0f  * BaseSize.x );  // Pixel width in screen space
    UINT uiHeight = UINT( SrcDesc.Height/2.0f * BaseSize.y );  // Pixel height in screen space
    D3DFORMAT Format = SrcDesc.Format;                         // Prefer swizzled format here

    // Create texture
    g_pd3dDevice->CreateTexture( uiWidth, uiHeight, 1, 0, Format, 0, &m_pBackBufferCopy );

    //---------------------------------------
    // Create flare VB
    //---------------------------------------
    UINT uiNumVerts = 8;
    SVert* pVerts;
    g_pd3dDevice->CreateVertexBuffer( uiNumVerts * sizeof(SVert), 0, 0, 0, &m_pQuadVB );

    m_pQuadVB->Lock( 0, 0, (BYTE**)&pVerts, 0 );

    // Verts for swizzled texture
    pVerts[0].Pos = XGVECTOR3( -0.5f, -0.5f, 1 );
    pVerts[1].Pos = XGVECTOR3( -0.5f,  0.5f, 1 );
    pVerts[2].Pos = XGVECTOR3(  0.5f,  0.5f, 1 );
    pVerts[3].Pos = XGVECTOR3(  0.5f, -0.5f, 1 );

    pVerts[0].UV = XGVECTOR2( 0, 1 );
    pVerts[1].UV = XGVECTOR2( 0, 0 );
    pVerts[2].UV = XGVECTOR2( 1, 0 );
    pVerts[3].UV = XGVECTOR2( 1, 1 );

    // Verts for linear texture
    pVerts[4].Pos = XGVECTOR3(-0.5f, -0.5f, 1 );
    pVerts[5].Pos = XGVECTOR3(-0.5f,  0.5f, 1 );
    pVerts[6].Pos = XGVECTOR3( 0.5f,  0.5f, 1 );
    pVerts[7].Pos = XGVECTOR3( 0.5f, -0.5f, 1 );

    pVerts[4].UV = XGVECTOR2( 0, FLOAT(uiWidth) );
    pVerts[5].UV = XGVECTOR2( 0, 0 );
    pVerts[6].UV = XGVECTOR2( FLOAT( uiHeight ), 0 );
    pVerts[7].UV = XGVECTOR2( FLOAT( uiHeight ), FLOAT( uiWidth ) );

    m_pQuadVB->Unlock();

    // Record the number of fully visible light pixels
    SetNumFullLightPixels();

    pSrcSurface->Release();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SetNumFullLightPixels()
// Desc: Determines the number of pixels rendered for a fully visible light
//-----------------------------------------------------------------------------
HRESULT CLensFlare::SetNumFullLightPixels()
{
    XGMATRIX Mat;

    // Get back buffer desc
    D3DSURFACE_DESC SrcDesc;
    IDirect3DSurface8* pSrcSurface;
    g_pd3dDevice->GetBackBuffer( 0, D3DBACKBUFFER_TYPE_MONO, &pSrcSurface );
    pSrcSurface->GetDesc( &SrcDesc );

    // Set world view projection matrices
    XGVECTOR3 Position( 0, 0, 0 );
    XGVECTOR3 Scale( m_pLightElement->BaseSize.x, m_pLightElement->BaseSize.y, 1.0f );
    XGMatrixTransformation( &Mat, NULL, NULL, &Scale, NULL, NULL, &Position );
    g_pd3dDevice->SetTransform( D3DTS_WORLD, &Mat);

    //Identity view matrix
    XGMatrixIdentity(&Mat);
    g_pd3dDevice->SetTransform( D3DTS_VIEW, &Mat );

    // Initialize projection matrix
    XGMatrixOrthoLH( &Mat, 2.0f, 2.0f * FLOAT(SrcDesc.Height)/FLOAT(SrcDesc.Width), 0.0f, 1.0f );
    g_pd3dDevice->SetTransform( D3DTS_PROJECTION, &Mat );

    //------------------------------------------
    // Set render state
    //------------------------------------------
    g_pd3dDevice->SetPixelShader( NULL );
    g_pd3dDevice->SetVertexShader( D3DFVF_XYZ | D3DFVF_TEX1 );

    g_pd3dDevice->SetRenderState( D3DRS_CULLMODE,         D3DCULL_CCW );
    g_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE,  FALSE );

    // Not using Z since we are looking for the number of pixels when the light is fully visible
    g_pd3dDevice->SetRenderState( D3DRS_ZENABLE,          FALSE );


    g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    g_pd3dDevice->SetRenderState( D3DRS_SRCBLEND,         D3DBLEND_SRCALPHA );
    g_pd3dDevice->SetRenderState( D3DRS_DESTBLEND,        D3DBLEND_INVSRCALPHA );

    // Output color is the light color
    // Output alpha is the texture alpha
    g_pd3dDevice->SetTexture( 0, m_pLightElement->pTexture );
    g_pd3dDevice->SetRenderState( D3DRS_TEXTUREFACTOR, m_pLightElement->Color );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TFACTOR );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1 );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_MIPFILTER, D3DTEXF_NONE );
    g_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );
    g_pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );

    g_pd3dDevice->SetStreamSource( 0, m_pQuadVB, sizeof(SVert) );

    // Visibility test
    g_pd3dDevice->BeginVisibilityTest();
    g_pd3dDevice->DrawPrimitive( D3DPT_QUADLIST, 0, 1 );
    g_pd3dDevice->EndVisibilityTest( 0 );

    HRESULT hr = D3DERR_TESTINCOMPLETE;
    while( hr == D3DERR_TESTINCOMPLETE )
    {
        // Get results of vis test
        hr = g_pd3dDevice->GetVisibilityTestResult( 0, &m_uiNumFullLightPixels, NULL );
    }

    pSrcSurface->Release();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: RasterPos()
// Desc: determines where a pixel will be rendered
//-----------------------------------------------------------------------------
int CLensFlare::RasterPos( FLOAT fNum )
{
    DOUBLE dFraction, dIntegerPortion;
    dFraction = modf( fNum, &dIntegerPortion );
    if( dFraction < 0.5 + 1.0 / 16.0 )
        return int( dIntegerPortion );
    else
        return int( dIntegerPortion + 1 );
};




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: renders the lens flare
//-----------------------------------------------------------------------------
HRESULT CLensFlare::Render( const XGVECTOR3 WorldLightDirection, const XGMATRIX& ViewMatrix )
{
    // Get back buffer and desc
    D3DSURFACE_DESC SrcDesc;
    IDirect3DSurface8* pSrcSurface;
    g_pd3dDevice->GetBackBuffer( 0, D3DBACKBUFFER_TYPE_MONO, &pSrcSurface );
    pSrcSurface->GetDesc( &SrcDesc );

    // Get src light texture and desc
    D3DSURFACE_DESC DstDesc;
    IDirect3DSurface8* pDstSurface;
    m_pBackBufferCopy->GetSurfaceLevel( 0, &pDstSurface );
    pDstSurface->GetDesc( &DstDesc );

    //--------------------------------------
    // Get light rect in screen space
    //--------------------------------------

    // Get view space light direction
    XGVECTOR3 ViewLightDirection;
    XGVec3TransformNormal( &ViewLightDirection, &WorldLightDirection, &ViewMatrix );

    // Scale to skybox size
    XGVECTOR3 ViewLightPosition;
    XGVec3Normalize( &ViewLightPosition, &ViewLightDirection );
    ViewLightPosition *= sqrtf( 1.0*1.0f + 1.0f*1.0f + 1.0f*1.0f );

    // World matrix
    XGMATRIX World;
    XGVECTOR3 Position( ViewLightPosition.x, ViewLightPosition.y, 0 );
    XGVECTOR3 Scale( m_pLightElement->BaseSize.x, m_pLightElement->BaseSize.y, 1.0f );
    XGMatrixTransformation( &World, NULL, NULL, &Scale, NULL, NULL, &Position );

    // View matrix is identity
    XGMATRIX View;
    XGMatrixIdentity( &View );
    g_pd3dDevice->SetTransform( D3DTS_VIEW, &View );

    // Skybox projection
    XGMATRIX SkyBoxProjection;
    XGMatrixOrthoLH( &SkyBoxProjection, 2.0f, 2.0f * FLOAT(SrcDesc.Height)/FLOAT(SrcDesc.Width), 0.0f, 1.0f );
    g_pd3dDevice->SetTransform( D3DTS_PROJECTION, &SkyBoxProjection );

    // Clip light if it is behind us
    XGVECTOR3 ProjectedLightPos;
    XGVec3TransformCoord( &ProjectedLightPos, &ViewLightPosition, &SkyBoxProjection );

    // Immediately clip if the light is behind us
    if( ProjectedLightPos.z <= 0 )
    {
        swprintf( strMessage, L"Frustum Clipped\n" );
        pSrcSurface->Release();
        pDstSurface->Release();
        return S_OK;
    }

    // Viewport matrix 
    XGMATRIX ViewportMat;
    D3DVIEWPORT8 Viewport;
    ZeroMemory(&ViewportMat, sizeof( ViewportMat ) );
    g_pd3dDevice->GetViewport(&Viewport);
    ViewportMat[0*4 + 0] = FLOAT( Viewport.Width )/2.0f;
    ViewportMat[1*4 + 1] = -FLOAT( Viewport.Height )/2.0f;
    ViewportMat[2*4 + 2] = Viewport.MaxZ - Viewport.MinZ;
    ViewportMat[3*4 + 0] = FLOAT( Viewport.X ) + FLOAT( Viewport.Width )/2.0f;
    ViewportMat[3*4 + 1] = FLOAT( Viewport.Y ) + FLOAT( Viewport.Height )/2.0f;
    ViewportMat[3*4 + 2] = Viewport.MinZ;
    ViewportMat[3*4 + 3] = 1.0f;

    // Get WVPS
    XGMATRIX WVPS = World * SkyBoxProjection * ViewportMat;

    // Transform quad corner points into screen space so we can copy the back buffer
    XGVECTOR3 Quad[2];
    Quad[0] = XGVECTOR3( -0.5f, -0.5f, 1 );
    Quad[1] = XGVECTOR3(  0.5f,  0.5f, 1 );

    // Get quad corner point in screen space
    const XGVECTOR4 ScreenSpaceOffset( 0.5f + 1.0f/32.0f, 0.5f + 1.0f/32.0f, 0.0f, 0.0f );
    XGVECTOR4 ScreenLightPosVerts[2];
    XGVec3Transform( &ScreenLightPosVerts[0], &Quad[0], &WVPS );
    XGVec3Transform( &ScreenLightPosVerts[1], &Quad[1], &WVPS );
    ScreenLightPosVerts[0] += ScreenSpaceOffset;
    ScreenLightPosVerts[1] += ScreenSpaceOffset;

    //----------------------------------------------
    // Compute source rectangle for back buffer copy
    //----------------------------------------------

    // Get raster portions of screen space coordinates
    int SrcX0 = RasterPos( ScreenLightPosVerts[0].x );
    int SrcX1 = RasterPos( ScreenLightPosVerts[1].x );
    int SrcY0 = RasterPos( ScreenLightPosVerts[1].y );
    int SrcY1 = RasterPos( ScreenLightPosVerts[0].y );

    // If light is not visible, return
    if( SrcX1 <= 0 || SrcX0 >= int( SrcDesc.Width ) ||
        SrcY1 <= 0 || SrcY0 >= int( SrcDesc.Height ) )
    {
       swprintf( strMessage, L"Frustum Clipped\n" );
       pSrcSurface->Release();
       pDstSurface->Release();
       return S_OK;
    }

    // Clamp the src rectangle and offset the dst rectangle if needed
    int DstX0 = 0;
    int DstY0 = 0;

    if( SrcX0 < 0 )
    {
        DstX0 = -SrcX0;
        SrcX0 = 0;
    }
    if( SrcX1 > int( SrcDesc.Width ) )
    {
        SrcX1 = int( SrcDesc.Width );
    }
    if( SrcY0 < 0 )
    {
        DstY0 = -SrcY0;
        SrcY0 = 0;
    }
    if( SrcY1 > int( SrcDesc.Height ) )
    {
        SrcY1 = int( SrcDesc.Height );
    }

    //--------------------------------------
    // Set general render state
    //--------------------------------------
    g_pd3dDevice->SetPixelShader( NULL );
    g_pd3dDevice->SetVertexShader( D3DFVF_XYZ| D3DFVF_TEX1 );
    g_pd3dDevice->SetStreamSource( 0, m_pQuadVB, sizeof(SVert) );

    g_pd3dDevice->SetRenderState( D3DRS_CULLMODE,  D3DCULL_CCW );
    g_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE,  FALSE );

    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_MIPFILTER, D3DTEXF_NONE );

    g_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_DISABLE );
    g_pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_DISABLE );

    //--------------------------------------------------
    // Compute visibility ratio and fill back buffer alpha
    //--------------------------------------------------

    // Set world matrix
    Position = XGVECTOR3( ViewLightPosition.x, ViewLightPosition.y, 0 );
    Scale = XGVECTOR3( m_pLightElement->BaseSize.x, m_pLightElement->BaseSize.y, 1.0f );
    XGMatrixTransformation( &World, NULL, NULL, &Scale, NULL, NULL, &Position );
    g_pd3dDevice->SetTransform( D3DTS_WORLD, &World );

    // Set light texture   
    g_pd3dDevice->SetTexture( 0, m_pLightElement->pTexture );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TFACTOR );
    g_pd3dDevice->SetRenderState( D3DRS_TEXTUREFACTOR, m_pLightElement->Color );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1 );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );

    // Clear the alpha channel.  NOTE: this does not have to be done if alpha values are all 0.0f
    g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE,   FALSE );
    g_pd3dDevice->SetRenderState( D3DRS_ZENABLE,            D3DZB_FALSE );
    g_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE,   D3DCOLORWRITEENABLE_ALPHA );
    g_pd3dDevice->SetRenderState( D3DRS_TEXTUREFACTOR,      0x0000000 );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR );
    g_pd3dDevice->DrawPrimitive( D3DPT_QUADLIST, 0, 1 );

    // Render light 
    g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    g_pd3dDevice->SetRenderState( D3DRS_SRCBLEND,         D3DBLEND_SRCALPHA );
    g_pd3dDevice->SetRenderState( D3DRS_DESTBLEND,        D3DBLEND_INVSRCALPHA );

    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
    g_pd3dDevice->SetRenderState( D3DRS_ZENABLE,          D3DZB_TRUE );
    g_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALL );
    g_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE,     TRUE );
    g_pd3dDevice->SetRenderState( D3DRS_TEXTUREFACTOR,    m_pLightElement->Color );

    g_pd3dDevice->BeginVisibilityTest();
    g_pd3dDevice->DrawPrimitive( D3DPT_QUADLIST, 0, 1 );
    g_pd3dDevice->EndVisibilityTest( 0 );

    // Get visibility test results
    UINT uiNumVisibleLightPixels;
    HRESULT hr = D3DERR_TESTINCOMPLETE;
    while ( hr == D3DERR_TESTINCOMPLETE )
    {
        // Get results of vis test
        hr = g_pd3dDevice->GetVisibilityTestResult( 0, &uiNumVisibleLightPixels, NULL );
    }

    FLOAT fVisRatio = FLOAT(uiNumVisibleLightPixels)/FLOAT(m_uiNumFullLightPixels);

    // Clamp visibility ratio
    if( fVisRatio > 1.0f )
        fVisRatio = 1.0f;
    if( fVisRatio < 0.0f )
        fVisRatio = 0.0f;

    // If the light is not visible, no effect
    if( fVisRatio == 0.0f )
    {
        swprintf( strMessage, L"Visibility Clipped\n" );
        pSrcSurface->Release();
        pDstSurface->Release();
        return S_OK;
    }

    //----------------------------------------------------
    // Copy back buffer portion that light was rendered too
    //----------------------------------------------------

    // We use the last back buffer copy if the visibility remains 100%
    if( fVisRatio == 1.0f && m_bPreviouslyFullyVisible )
    {
        swprintf( strMessage, L"Full Visibility Optimization\n" );
    }
    else
    {
        //------------------------------------
        // Copy back buffer 
        //------------------------------------
        swprintf( strMessage, L"Full Effect\n" );

        RECT SrcRect;
        SrcRect.left   = UINT( SrcX0 );
        SrcRect.right  = UINT( SrcX1 );
        SrcRect.top    = UINT( SrcY0 );
        SrcRect.bottom = UINT( SrcY1 );

        POINT DstPoint;
        DstPoint.x = UINT( DstX0 );
        DstPoint.y = UINT( DstY0 );

        g_pd3dDevice->CopyRects( pSrcSurface, &SrcRect, 1, pDstSurface, &DstPoint );
    }

    m_bPreviouslyFullyVisible = ( fVisRatio == 1.0f );

    //----------------------------------------
    // Compute flare parameters
    //----------------------------------------
    const FLOAT fInvisAlpha = 1.0f/255.0f;  // if alpha is below this, the element if not rendered
    const FLOAT fInvisSize  = 0.00001f;     // if size is below this, the element is not rendered

    // Compute distance ratio
    FLOAT fMaxDistX = m_pLightElement->ViewCenter.x < 0 ? 1.0f - m_pLightElement->ViewCenter.x : m_pLightElement->ViewCenter.x - -1.0f;
    FLOAT fMaxDistY = m_pLightElement->ViewCenter.y < 0 ? 1.0f - m_pLightElement->ViewCenter.y : m_pLightElement->ViewCenter.y - -1.0f;
    XGVECTOR2 FlareVec(ViewLightPosition.x - m_pLightElement->ViewCenter.x, ViewLightPosition.y - m_pLightElement->ViewCenter.y);
    XGVECTOR2 vDistance( fMaxDistX, fMaxDistY );
    FLOAT fMaxFlareDistance = XGVec2Length( &vDistance );
    FLOAT fDistanceRatio = ( fMaxFlareDistance - XGVec2Length( &FlareVec ) ) / fMaxFlareDistance;

    // Calculate light intensity
    XGVECTOR3 NormLightDirection;
    XGVECTOR3 NormViewForward(0, 0, 1.0f);
    XGVec3Normalize( &NormLightDirection, &ViewLightDirection );
    FLOAT fLightIntensity = XGVec3Dot( &NormLightDirection, &NormViewForward );

    //----------------------------------------
    // Render light "flare out"
    //-----------------------------------------

    // Set render state
    g_pd3dDevice->SetRenderState( D3DRS_ZENABLE,          D3DZB_FALSE );
    g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );                    // FALSE: shows back buffer copy
    g_pd3dDevice->SetRenderState( D3DRS_SRCBLEND,         D3DBLEND_SRCALPHA );
    g_pd3dDevice->SetRenderState( D3DRS_DESTBLEND,        D3DBLEND_INVSRCALPHA );
    g_pd3dDevice->SetTexture( 0, m_pBackBufferCopy );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TFACTOR );        // D3DTA_TEXTURE | D3DTA_ALPHAREPLICATE: shows back buffer alpha
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 );   
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR );

    for( UINT i = 0; i < m_pLightElement->uiNumFlareOuts; i++ )
    {
        FLOAT fScale = (i + 1) / FLOAT( m_pLightElement->uiNumFlareOuts );

        XGVECTOR3 FlarePosition( ViewLightPosition.x, ViewLightPosition.y, 0 );

        XGVECTOR2 FlareOutSize = m_pLightElement->BaseSize + ( m_pLightElement->FlareOutSize - m_pLightElement->BaseSize ) * fScale;
        Scale = XGVECTOR3( FlareOutSize.x, FlareOutSize.y, 1.0f );
        Scale*= powf( fLightIntensity, 2 );

        XGMatrixTransformation( &World, NULL, NULL, &Scale, NULL, NULL, &Position );
        g_pd3dDevice->SetTransform( D3DTS_WORLD, &World );

        XGCOLOR Color( m_pLightElement->Color );
        FLOAT fAlphaScale = FLOAT( m_pLightElement->uiNumFlareOuts - i ) / FLOAT( m_pLightElement->uiNumFlareOuts );

        Color.a *= fAlphaScale * powf( fLightIntensity, 4 );

        g_pd3dDevice->SetRenderState( D3DRS_TEXTUREFACTOR, Color );

        g_pd3dDevice->DrawPrimitive( D3DPT_QUADLIST, 4, 1 );
    }

    //----------------------------------------
    // Render flare elements
    //-----------------------------------------
    FLOAT fAlphaMult = fDistanceRatio * fVisRatio - powf(fLightIntensity, 50);

    // Skip rendering flares if they will be invisible
    if( fAlphaMult < fInvisAlpha )
    {
        pSrcSurface->Release();
        pDstSurface->Release();
        return S_OK;
    }
    FLOAT fSizeMult = fDistanceRatio;
    if( fSizeMult < fInvisSize )
    {
        pSrcSurface->Release();
        pDstSurface->Release();
        return S_OK;
    }

    // Flare element render states
    g_pd3dDevice->SetRenderState( D3DRS_ZENABLE,          FALSE );
    g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    g_pd3dDevice->SetRenderState( D3DRS_SRCBLEND,         D3DBLEND_SRCALPHA );
    g_pd3dDevice->SetRenderState( D3DRS_DESTBLEND,        D3DBLEND_ONE );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TFACTOR);
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_TFACTOR );

    for( UINT i = 0; i < m_uiNumFlareElements; i++ )
    {
        g_pd3dDevice->SetTexture( 0, m_pFlareElements[i].pTexture );

        XGCOLOR Color( m_pFlareElements[i].Color );
        Color.a = Color.a * fAlphaMult;

        // Skip if alpha is too small
        if( Color.a < fInvisAlpha )
            continue;

        g_pd3dDevice->SetRenderState( D3DRS_TEXTUREFACTOR, Color);

        // Set world view projection matrices
        Position = XGVECTOR3( FlareVec.x, FlareVec.y, 0.0f );
        Position *= m_pFlareElements[i].fDistance;

        XGVECTOR2 Size( m_pFlareElements[i].Size.x, m_pFlareElements[i].Size.y );

        Size *= fSizeMult;

        // Clamp to max size
        if( Size.x > m_MaxElementSize.x )
            Size *= m_MaxElementSize.x/Size.x;
        if( Size.y > m_MaxElementSize.y )
            Size *= m_MaxElementSize.y/Size.y;

        // Skip if size is too small
        if( Size.x < fInvisSize || Size.y < fInvisSize )
            continue;

        Scale = XGVECTOR3( Size.x, Size.y, 1.0f);
        XGMatrixTransformation( &World, NULL, NULL, &Scale, NULL, NULL, &Position );
        g_pd3dDevice->SetTransform( D3DTS_WORLD, &World);

        g_pd3dDevice->DrawPrimitive( D3DPT_QUADLIST,0, 1 );
    }

    // Cleanup
    pSrcSurface->Release();
    pDstSurface->Release();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    CXBFont     m_Font;             // Font object
    CXBHelp     m_Help;             // Help object
    BOOL        m_bDrawHelp;        // TRUE to draw help screen

    // WVP matrices
    XGMATRIX    m_matWorld;
    XGMATRIX    m_matView;
    XGMATRIX    m_matProjection;

    // Camera data
    FLOAT       m_fYaw;
    FLOAT       m_fPitch;
    XGVECTOR3   m_Position;
    XGVECTOR3   m_Forward;

    // Meshes
    CXBMesh     m_Leaves;
    CXBMesh     m_Branches;
    CXBMesh     m_Dirt;

    // Packed resources
    CXBPackedResource m_xprResource;

    // Lens flare
    CLensFlare m_LensFlare;

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

    // Set presentation interval to be immediate for better frame rate testing
    m_d3dpp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
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

    // Get packed resources
    if( FAILED( m_xprResource.Create( "Resource.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create the scene
    if( FAILED( m_Dirt.Create( "Models\\Dirt.xbg", &m_xprResource ) ) )
        return XBAPPERR_MEDIANOTFOUND;
    m_Dirt.GetMesh(0)->m_pSubsets[0].pTexture = m_xprResource.GetTexture( "stonehengeground512" );

    if( FAILED( m_Branches.Create( "Models\\Branches.xbg", &m_xprResource ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    if( FAILED( m_Leaves.Create( "Models\\Leaves.xbg", &m_xprResource ) ) )
        return XBAPPERR_MEDIANOTFOUND;
    m_Leaves.GetMesh(0)->m_pSubsets[0].pTexture = m_xprResource.GetTexture( "leaf" );

    // Initialize camera 
    m_fYaw     = 0.0f;
    m_fPitch   = 0.0f;
    m_Position = XGVECTOR3( 0, 0, 0 );
    XGMatrixIdentity( &m_matView );

    // Initialize projection matrix
    XGMatrixPerspectiveFovLH( &m_matProjection, D3DX_PI/4, 640.f/480.f, 1.0f, 5000.0f );

    // Initialize world matrix 
    XGMatrixTranslation( &m_matWorld, 0, -13, 0 );

    //--------------------------------------------
    // Initialize flare light and elements
    //--------------------------------------------

    SLightElement* pLightElement = new SLightElement;

    pLightElement->BaseSize = XGVECTOR2( 0.1f, 0.1f );
    pLightElement->FlareOutSize = XGVECTOR2( 1.2f, 1.2f );
    pLightElement->uiNumFlareOuts = 10;
    pLightElement->pTexture = m_xprResource.GetTexture( "sun" );
    pLightElement->Color = XGCOLOR( 247.0f/255.0f, 247.0f/255.0f, 230.0f / 255.0f, 0.45f );
    pLightElement->ViewCenter = XGVECTOR2( 0, 0 );

    SFlareElement* pFlareElements = new SFlareElement[9];

    pFlareElements[0].Size      = XGVECTOR2( 0.15f, 0.15f );
    pFlareElements[0].fDistance = 0.75f;
    pFlareElements[0].pTexture  = m_xprResource.GetTexture( "flare1" );
    pFlareElements[0].Color     = D3DCOLOR_RGBA( 6, 16, 30, 255 );

    pFlareElements[1].Size      = XGVECTOR2( 0.3f, 0.3f );
    pFlareElements[1].fDistance = 0.51f;
    pFlareElements[1].pTexture  = m_xprResource.GetTexture( "flare2" );
    pFlareElements[1].Color     = D3DCOLOR_RGBA( 6, 16, 30, 255 );

    pFlareElements[2].Size      = XGVECTOR2( 0.15f, 0.15f );
    pFlareElements[2].fDistance = 0.42f;
    pFlareElements[2].pTexture  = m_xprResource.GetTexture( "flare3" );
    pFlareElements[2].Color     = D3DCOLOR_RGBA( 81, 40, 10, 255 );

    pFlareElements[3].Size      = XGVECTOR2( 0.2f, 0.2f );
    pFlareElements[3].fDistance = 0.33f;
    pFlareElements[3].pTexture  = m_xprResource.GetTexture( "flare1" );
    pFlareElements[3].Color     = D3DCOLOR_RGBA( 81, 40, 10, 255 );

    pFlareElements[4].Size      = XGVECTOR2( 0.25f, 0.25f );
    pFlareElements[4].fDistance = 0.11f;
    pFlareElements[4].pTexture  = m_xprResource.GetTexture( "flare2" );
    pFlareElements[4].Color     = D3DCOLOR_RGBA( 81, 40, 10, 255 );

    pFlareElements[5].Size      = XGVECTOR2( 0.3f, 0.3f );
    pFlareElements[5].fDistance = -0.22f;
    pFlareElements[5].pTexture  = m_xprResource.GetTexture( "flare3" );
    pFlareElements[5].Color     = D3DCOLOR_RGBA( 81, 40, 10, 255 );

    pFlareElements[6].Size      = XGVECTOR2( 0.15f, 0.15f );
    pFlareElements[6].fDistance = -0.45f;
    pFlareElements[6].pTexture  = m_xprResource.GetTexture( "flare4" );
    pFlareElements[6].Color     = D3DCOLOR_RGBA( 41, 61, 10, 255 );

    pFlareElements[7].Size      = XGVECTOR2( 0.4f, 0.4f );
    pFlareElements[7].fDistance = -0.6f;
    pFlareElements[7].pTexture  = m_xprResource.GetTexture( "flare2" );
    pFlareElements[7].Color     = D3DCOLOR_RGBA( 41, 61, 10, 255 );

    pFlareElements[8].Size      = XGVECTOR2( 0.2f, 0.2f );
    pFlareElements[8].fDistance = -0.8f;
    pFlareElements[8].pTexture  = m_xprResource.GetTexture( "flare3" );
    pFlareElements[8].Color     = D3DCOLOR_RGBA(41, 61, 10, 255);

    m_LensFlare.Create( pLightElement, pFlareElements, 9, XGVECTOR2( 0.3f, 0.3f ) );

    // Initialize message
    strMessage[0] = L'\0';

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

    // Camera matrix
    XGMATRIX Camera;
    XGVECTOR3* pLeft     = (XGVECTOR3*)&Camera[4*0];
    XGVECTOR3* pForward  = (XGVECTOR3*)&Camera[4*2];
    XGVECTOR3* pPosition = (XGVECTOR3*)&Camera[4*3];

    // Rotation
    FLOAT fRotationScale = 1.0f * m_fElapsedTime;
    m_fYaw += fRotationScale * m_DefaultGamepad.fX2;
    m_fPitch += fRotationScale * m_DefaultGamepad.fY2;
    XGMatrixRotationYawPitchRoll( &Camera, m_fYaw, m_fPitch, 0.0f );

    // Translation
    FLOAT fTranslationScale = 10.0f*m_fElapsedTime;
    XGVECTOR3 Left = *pLeft;
    Left.y = 0.0f;
    XGVec3Normalize( &Left, &Left );
    XGVECTOR3 Forward = *pForward;
    m_Forward = Forward;
    Forward.y = 0.0f;
    XGVec3Normalize( &Forward, &Forward );
    m_Position += Left * fTranslationScale * m_DefaultGamepad.fX1;
    m_Position += Forward * fTranslationScale * m_DefaultGamepad.fY1;
    if( m_DefaultGamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP )
        m_Position.y += fTranslationScale; 
    if( m_DefaultGamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN )
        m_Position.y -= fTranslationScale;
    *pPosition = m_Position;

    // Update view matrix
    FLOAT fDet;
    XGMatrixInverse( &m_matView, &fDet, &Camera );
    assert(fDet > 0.0f);

    // Clear message
    strMessage[0] = L'\0';

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the scene
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Render()
{
    // Draw a gradient filled background
    RenderGradientBackground( 0xffc0c8b0, 0xff6ac0de );

    // Set world view projection matrices
    m_pd3dDevice->SetTransform( D3DTS_WORLD,      &m_matWorld );
    m_pd3dDevice->SetTransform( D3DTS_VIEW,       &m_matView );
    m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &m_matProjection );

    // Set general state
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, D3DZB_TRUE ); 
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE ); 
    m_pd3dDevice->SetRenderState( D3DRS_SPECULARMATERIALSOURCE, D3DMCS_MATERIAL );
    m_pd3dDevice->SetRenderState( D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_MATERIAL );
    m_pd3dDevice->SetRenderState( D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL );
    m_pd3dDevice->SetRenderState( D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL );
    m_pd3dDevice->SetRenderState( D3DRS_COLORVERTEX, FALSE );

    // Set ambient
    DWORD dwAmbient = XGCOLOR( 199.0f/255.0f, 245.0f/255.0f, 244.0f/255.0f, 0.0f );
    m_pd3dDevice->SetRenderState( D3DRS_AMBIENT, dwAmbient );

    // Light direction
    XGVECTOR3 vLightDir( 0.0f, 0.5f, 2.0f );

#if 0
    // Set directional light
    D3DLIGHT8 Light;
    ZeroMemory( &Light, sizeof(Light) );
    Light.Type      = D3DLIGHT_DIRECTIONAL;
    Light.Direction = -vLightDir;
    Light.Diffuse   = XGCOLOR( 0.5f, 0.5f, 0.5f, 0.0f );
    m_pd3dDevice->SetLight( 0, &Light );
    m_pd3dDevice->LightEnable( 0, TRUE );
#endif

    // Draw dirt
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_CURRENT );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
    m_Dirt.Render(0);

    // Draw branches
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_CURRENT );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
    m_Branches.Render(0);

    // Draw leaves
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_CURRENT );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, TRUE );     // tree texture uses, effectively, a 1-bit alpha
    m_pd3dDevice->SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_LESS );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHAREF, 100 );
    m_Leaves.Render( 0 );

    // Draw lens flare 
    m_LensFlare.Render( vLightDir, m_matView );

    // Show title, frame rate, and help
    if( m_bDrawHelp )
    {
        m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 48, 36, 0xffffffff,  L"Lensflare" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 592, 38, 0xffffff00, m_strFrameRate, XBFONT_RIGHT );
        m_Font.DrawText( 64,  480 - 75,   0xffffff00, strMessage );
        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}
