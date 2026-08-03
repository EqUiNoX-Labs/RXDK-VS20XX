//-----------------------------------------------------------------------------
// File: XBHelp.cpp
//
// Desc: Support class for rendering a help image, which is an image of an Xbox
//       gamepad, with labelled callouts to each of the gamepad's controls.
//
// Hist: 11.01.00 - New for November XDK release
//       12.15.00 - Changes for December XDK release
//       03.06.01 - Changes for April XDK release
//       04.15.01 - Using packed resources for May XDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <xtl.h>
#include <xgraphics.h>
#include "XBHelp.h"
#include "XBResource.h"




//-----------------------------------------------------------------------------
// Constants for rendering callouts on the help screen. The order of these
// callouts is in agreement with the enum structure in XBHelp.h
//-----------------------------------------------------------------------------
struct XBHELP_CALLOUT_POS
{
    FLOAT fButtonX, fButtonY;
    FLOAT fLine1X, fLine1Y,   fText1X, fText1Y;
    FLOAT fLine2X, fLine2Y,   fText2X, fText2Y;
    DWORD dwFontFlags;
};

static XBHELP_CALLOUT_POS g_vHelpCallouts[] = 
{
    // Left thumbstick
    { 240.0f, 198.0f,
      199.0f, 149.0f,  198.0f, 134.0f,
      199.0f, 149.0f,  198.0f, 134.0f, 
      XBFONT_RIGHT },
    
    // Right thumbstick 
    { 357.0f, 245.0f,
      387.0f, 343.0f,  380.0f, 342.0f,
      387.0f, 343.0f,  380.0f, 342.0f,
      XBFONT_LEFT },

    // D-pad
    { 278.0f, 244.0f,
      278.0f, 343.0f,  285.0f, 342.0f,
      278.0f, 343.0f,  285.0f, 342.0f,
      XBFONT_RIGHT },
    
    // Back button
    { 213.0f, 228.0f,
      179.0f, 228.0f,  177.0f, 216.0f,
      179.0f, 228.0f,  177.0f, 216.0f,
      XBFONT_RIGHT },

    // Start button
    { 228.0f, 244.0f,
      200.0f, 272.0f,  202.0f, 270.0f,
      200.0f, 272.0f,  202.0f, 270.0f,
      XBFONT_RIGHT },

    // X button
    { 376.0f, 198.0f,
      310.0f, 102.0f,  300.0f, 81.0f,
      310.0f, 102.0f,  300.0f, 56.0f,
      XBFONT_LEFT },

    // Y button
    { 398.0f, 177.0f,
      432.0f, 107.0f,  434.0f, 87.0f,
      432.0f, 107.0f,  434.0f, 87.0f,
      XBFONT_LEFT },

    // A button
    { 397.0f, 220.0f,
      452.0f, 202.0f,  454.0f, 189.0f,
      452.0f, 202.0f,  454.0f, 189.0f,
      XBFONT_LEFT },

    // B button
    { 419.0f, 199.0f,
      442.0f, 158.0f,  444.0f, 138.0f,
      442.0f, 158.0f,  444.0f, 138.0f,
      XBFONT_LEFT },

    // White button
    { 401.0f, 255.0f,
      433.0f, 292.0f,  429.0f, 291.0f,
      433.0f, 292.0f,  429.0f, 291.0f,
      XBFONT_LEFT },
    
    // Black button
    { 422.0f, 237.0f,
      448.0f, 249.0f,  450.0f, 240.0f,
      448.0f, 249.0f,  450.0f, 240.0f,
      XBFONT_LEFT },

    // Left trigger
    { 247.0f, 142.0f,
      185.0f,  56.0f,  187.0f,  36.0f,
      185.0f,  56.0f,  187.0f,  36.0f,
      XBFONT_RIGHT },

    // Right trigger
    { 388.0f, 142.0f,
      422.0f,  56.0f,  424.0f,  36.0f,
      422.0f,  56.0f,  424.0f,  36.0f,
      XBFONT_LEFT },

    // Misc callout 1
    {   0.0f,   0.0f,
        0.0f,   0.0f,   48.0f, 419.0f,
        0.0f,   0.0f,   48.0f, 394.0f,
      XBFONT_LEFT },

    // Misc callout 2
    {   0.0f,   0.0f,
        0.0f,   0.0f,  320.0f, 419.0f,
        0.0f,   0.0f,  320.0f, 394.0f,
      XBFONT_CENTER_X },

    // Misc callout 3
    {   0.0f,   0.0f,
        0.0f,   0.0f,  592.0f, 419.0f,
        0.0f,   0.0f,  592.0f, 394.0f,
      XBFONT_RIGHT },
};




//-----------------------------------------------------------------------------
// Name: CXBHelp()
// Desc: Help class constructor
//-----------------------------------------------------------------------------
CXBHelp::CXBHelp()
{
    m_pFont           = NULL;
    m_pGamepadTexture = NULL;
    m_pVB             = NULL;
}




//-----------------------------------------------------------------------------
// Name: ~CXBHelp()
// Desc: Help class destructor
//-----------------------------------------------------------------------------
CXBHelp::~CXBHelp()
{
    Destroy();
}




//-----------------------------------------------------------------------------
// Name: Create()
// Desc: Creates the help class' internal objects
//-----------------------------------------------------------------------------
HRESULT CXBHelp::Create( CHAR* strResource )
{
    // Create the gamepad resource
    if( FAILED( m_xprResource.Create( strResource ) ) )
        return E_FAIL;

    // Get access to the gamepad texture
    m_pGamepadTexture = m_xprResource.GetTexture( 0UL );

    D3DSURFACE_DESC desc;
    m_pGamepadTexture->GetLevelDesc( 0, &desc );
    FLOAT hw = (FLOAT)(desc.Width/2);
    FLOAT hh = (FLOAT)(desc.Height/2);

    // Calculate a scale factor based on the video mode
    D3DDISPLAYMODE mode;
    D3DDevice::GetDisplayMode( &mode );
    m_fXScale = mode.Width / 640.0f;
    m_fYScale = mode.Height / 480.0f;

    // Calculate vertex positions
    FLOAT fLeft     = m_fXScale * 0.0f   - 0.5f;
    FLOAT fTop      = m_fYScale * 0.0f   - 0.5f;
    FLOAT fRight    = m_fXScale * 640.0f - 0.5f;
    FLOAT fBottom   = m_fYScale * 480.0f - 0.5f;

    // Create a vertex buffer for rendering the help screen
    struct VERTEX { D3DXVECTOR4 p; FLOAT tu, tv; };
    D3DDevice::CreateVertexBuffer( 4*sizeof(VERTEX), D3DUSAGE_WRITEONLY, 
                                   0L, D3DPOOL_DEFAULT, &m_pVB );
    VERTEX* v;
    m_pVB->Lock( 0, 0, (BYTE**)&v, 0L );
    v[0].p = D3DXVECTOR4(  fLeft,    fTop, 0, 0 );  v[0].tu = hw-320; v[0].tv = hh-240;
    v[1].p = D3DXVECTOR4( fRight,    fTop, 0, 0 );  v[1].tu = hw+320; v[1].tv = hh-240;
    v[2].p = D3DXVECTOR4( fRight, fBottom, 0, 0 );  v[2].tu = hw+320; v[2].tv = hh+240;
    v[3].p = D3DXVECTOR4(   fTop, fBottom, 0, 0 );  v[3].tu = hw-320; v[3].tv = hh+240;
    m_pVB->Unlock();

    // Create a texture for drawing the callout lines
    D3DDevice::CreateTexture( 4, 4, 1, 0, D3DFMT_A8R8G8B8, 0, &m_pLineTexture );
    D3DLOCKED_RECT lock;
    m_pLineTexture->LockRect( 0, &lock, 0, 0 );
    DWORD* pBits = (DWORD*)lock.pBits;
    pBits[0]  = 0xff101010; pBits[1]  = 0xffeaea10; pBits[4]  = 0xffeaea10; pBits[5]  = 0xff101010; 
    pBits[2]  = 0xff101010; pBits[3]  = 0xffeaeaea; pBits[6]  = 0xffeaeaea; pBits[7]  = 0xff101010; 
    pBits[8]  = 0xff101010; pBits[9]  = 0xffeaeaea; pBits[12] = 0xffeaeaea; pBits[13] = 0xff101010; 
    pBits[10] = 0xff101010; pBits[11] = 0xffeaeaea; pBits[14] = 0xffeaeaea; pBits[15] = 0xff101010; 
    m_pLineTexture->UnlockRect( 0 );

#if 0 // NOT YET IMPLEMENTED
    // If the resources are there, create a font for the help screen
    m_pFont = new CXBFont();
    if( FAILED( m_pFont->Create( m_xprResource.GetTexture( 20UL ), m_xprResource.GetData( 40UL ) ) ) )
    {
        delete m_pFont;
        m_pFont = NULL;
    }
#endif

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Destroy()
// Desc: Destroys the help class' internal objects/
//-----------------------------------------------------------------------------
HRESULT CXBHelp::Destroy()
{
    SAFE_RELEASE( m_pVB );
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the gamepad help image, and it's labelled callouts.
//-----------------------------------------------------------------------------
HRESULT CXBHelp::Render( XBHELP_CALLOUT* tags, DWORD dwNumCallouts )
{
    return Render( m_pFont, tags, dwNumCallouts );
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the gamepad help image, and it's labelled callouts.
//-----------------------------------------------------------------------------
HRESULT CXBHelp::Render( CXBFont* pFont, XBHELP_CALLOUT* tags, 
                         DWORD dwNumCallouts )
{
    // Set state to render the gamepad image
    D3DDevice::SetTexture( 0, m_pGamepadTexture );
    D3DDevice::SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
    D3DDevice::SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    D3DDevice::SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
    D3DDevice::SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE );
    D3DDevice::SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
    D3DDevice::SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
    D3DDevice::SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );
    D3DDevice::SetTextureStageState( 1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
    D3DDevice::SetTextureStageState( 0, D3DTSS_ADDRESSU,  D3DTADDRESS_CLAMP );
    D3DDevice::SetTextureStageState( 0, D3DTSS_ADDRESSV,  D3DTADDRESS_CLAMP );
    D3DDevice::SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    D3DDevice::SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    D3DDevice::SetRenderState( D3DRS_ZENABLE,      FALSE );
    D3DDevice::SetRenderState( D3DRS_FOGENABLE,    FALSE );
    D3DDevice::SetRenderState( D3DRS_FOGTABLEMODE, D3DFOG_NONE );
    D3DDevice::SetRenderState( D3DRS_FILLMODE,     D3DFILL_SOLID );
    D3DDevice::SetRenderState( D3DRS_CULLMODE,     D3DCULL_CCW );
    D3DDevice::SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    D3DDevice::SetRenderState( D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA );
    D3DDevice::SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
    D3DDevice::SetVertexShader( D3DFVF_XYZRHW|D3DFVF_TEX1 );

    // Render the gamepad image
    D3DDevice::SetStreamSource( 0, m_pVB, 6*sizeof(FLOAT) );
    D3DDevice::DrawPrimitive( D3DPT_QUADLIST, 0, 1 );

    // Set state to draw the lines
    D3DDevice::SetTexture( 0, m_pLineTexture );
    D3DDevice::SetVertexShader( D3DFVF_XYZRHW );
    D3DDevice::Begin( D3DPT_QUADLIST );

    // Declared outside of the for loop to allow the two for loops to compile
    // either with conforming or non-conforming for loop scoping.
    DWORD i;
    for( i=0; i<dwNumCallouts; i++ )
    {
        // Determine the line start and end positions
        FLOAT line1x = g_vHelpCallouts[tags[i].wControl].fButtonX * m_fXScale;
        FLOAT line1y = g_vHelpCallouts[tags[i].wControl].fButtonY * m_fYScale;
        FLOAT line2x = ( tags[i].wPlacement ? g_vHelpCallouts[tags[i].wControl].fLine2X : g_vHelpCallouts[tags[i].wControl].fLine1X ) * m_fXScale;
        FLOAT line2y = ( tags[i].wPlacement ? g_vHelpCallouts[tags[i].wControl].fLine2Y : g_vHelpCallouts[tags[i].wControl].fLine1Y ) * m_fYScale;

        D3DXVECTOR3 vc ( line2y-line1y, -line2x+line1x, 0.0f );
        D3DXVec3Normalize( &vc, &vc );
        vc.x *= 2;
        vc.y *= 2;

        // Draw the callout line
        D3DDevice::SetVertexData2f( D3DVSDE_TEXCOORD0, 0.0f, 0.0f );
        D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, line1x-vc.x, line1y-vc.y, 0.0f, 0.0f );
        D3DDevice::SetVertexData2f( D3DVSDE_TEXCOORD0, 1.0f, 0.0f );
        D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, line1x+vc.x, line1y+vc.y, 0.0f, 0.0f );
        D3DDevice::SetVertexData2f( D3DVSDE_TEXCOORD0, 1.0f, 1.0f );
        D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, line2x+vc.x, line2y+vc.y, 0.0f, 0.0f );
        D3DDevice::SetVertexData2f( D3DVSDE_TEXCOORD0, 0.0f, 1.0f );
        D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, line2x-vc.x, line2y-vc.y, 0.0f, 0.0f );
    }

    // Finish drawing the lines
    D3DDevice::End();

    // Prepare font for rendering
    pFont->Begin();

    // Render the callouts
    for( i=0; i<dwNumCallouts; i++ )
    {
        // Determine the text position
        FLOAT textx = ( tags[i].wPlacement ? g_vHelpCallouts[tags[i].wControl].fText2X : g_vHelpCallouts[tags[i].wControl].fText1X ) * m_fXScale;
        FLOAT texty = ( tags[i].wPlacement ? g_vHelpCallouts[tags[i].wControl].fText2Y : g_vHelpCallouts[tags[i].wControl].fText1Y ) * m_fYScale;

        // Draw the callout text
        pFont->DrawText( textx, texty, 0xffffffff, tags[i].strText, g_vHelpCallouts[tags[i].wControl].dwFontFlags );
    }

    // Flush the text drawing
    pFont->End();

    return S_OK;
}



