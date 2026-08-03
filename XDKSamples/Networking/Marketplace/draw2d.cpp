//-----------------------------------------------------------------------------
// File: Draw2D.cpp
//
// Desc: CDraw2D deals with the display of the sprite graphics.
//       the sprites[] array has the center and size of each graphic hardcoded, 
//       so we can display and collide appropriately
//
// Created for the August 2003 SDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#include <xtl.h>
#include <xgraphics.h>
#include <d3d8.h>
#include <stdio.h>
#include "CommonInclude.h"


//-----------------------------------------------------------------------------
// Static objects
//-----------------------------------------------------------------------------
DWORD CDraw2D::m_dwVertexShader = 0L;
DWORD CDraw2D::m_dwPixelShader  = 0L;


// these are the hardcoded center and size of each graphic
struct Sprite
{
    SHORT tx, ty;   // texture size
    SHORT fx, fy;   // file texture size
    SHORT ox, oy;   // origin
} sprites[] = 
{{ 764, 507, 1024, 512, 0, 0},      // marketplace
 { 81, 93, 128, 128, 25, 54},       // podium
 { 143, 129, 256, 256, 82, 63},     // fountain
 { 139, 166, 256, 256, 49, 98},     // plant
 { 127, 176, 128, 256, 44, 114},    // tree
 { 90,  90, 128, 128, 43, 59}};     // girl
    
    
    
//-----------------------------------------------------------------------------
// Name: CDraw2D::CDraw2D()
// Desc: 2D Drawing class constructor
//-----------------------------------------------------------------------------
CDraw2D::CDraw2D()
{
}




//-----------------------------------------------------------------------------
// Name: CDraw2D::~CDraw2D()
// Desc: Destructor
//-----------------------------------------------------------------------------
CDraw2D::~CDraw2D()
{
    Destroy();
}




//-----------------------------------------------------------------------------
// Name: CDraw2D::CreateShaders()
// Desc: Creates a vertex shader for rendering sprites
//-----------------------------------------------------------------------------
HRESULT CDraw2D::CreateShaders()
{
    // Create the vertex shader
    if( 0L == m_dwVertexShader )
    {
        // Specify the vertex declaration, used here to manually specify a constant.
        DWORD dwVertexDecl[] =
        {
            D3DVSD_STREAM(0),
            D3DVSD_REG( 0, D3DVSDT_FLOAT4 ),        // Vertex/tex coord combo
            D3DVSD_REG( 3, D3DVSDT_D3DCOLOR ),      // Color
            D3DVSD_END()
        };

        // Microcode for the vertex shader, where input is defined as:
        //    v0.xy = screen space position
        //    v0.zw = tex coords
        //    v3    = diffuse color
        static DWORD dwVertexShaderInstructions[] = 
        {
            0x00032078,
            0x00000000, 0x00200015, 0x0836106c, 0x2070c800, // mov oPos.xy, v0.xy
            0x00000000, 0x002000bf, 0x0836106c, 0x2070c848, // mov oT0.xy, v0.zw
            0x00000000, 0x0020061b, 0x0836106c, 0x2070f819  // mov oD0, v3
        };

        // Create the vertex shader
        if( FAILED( D3DDevice::CreateVertexShader( dwVertexDecl, 
                                                   dwVertexShaderInstructions, 
                                                   &m_dwVertexShader,
                                                   D3DUSAGE_PERSISTENTDIFFUSE ) ) )
        return E_FAIL;

    }

    // Create the pixel shader
    if( 0L == m_dwPixelShader )
    {
        D3DPIXELSHADERDEF psd;
        ZeroMemory( &psd, sizeof(psd) );
        psd.PSCombinerCount = PS_COMBINERCOUNT( 1, 0 );
        psd.PSTextureModes  = PS_TEXTUREMODES( PS_TEXTUREMODES_PROJECT2D, 0, 0, 0 );

        //------------- Stage 0 -------------
        psd.PSRGBInputs[0]    = PS_COMBINERINPUTS( PS_REGISTER_T0|PS_CHANNEL_RGB,   PS_REGISTER_V0|PS_CHANNEL_RGB,   0, 0 );
        psd.PSAlphaInputs[0]  = PS_COMBINERINPUTS( PS_REGISTER_T0|PS_CHANNEL_ALPHA, PS_REGISTER_V0|PS_CHANNEL_ALPHA, 0, 0 );
        psd.PSRGBOutputs[0]   = PS_COMBINEROUTPUTS( PS_REGISTER_R0, 0, 0, PS_COMBINEROUTPUT_AB_MULTIPLY );
        psd.PSAlphaOutputs[0] = PS_COMBINEROUTPUTS( PS_REGISTER_R0, 0, 0, PS_COMBINEROUTPUT_AB_MULTIPLY );

        //------------- Final combiner -------------
        psd.PSFinalCombinerInputsABCD = PS_COMBINERINPUTS( PS_REGISTER_C0, PS_REGISTER_R0, PS_REGISTER_ZERO, PS_REGISTER_C1 );
        psd.PSFinalCombinerInputsEFG  = PS_COMBINERINPUTS( 0, 0, PS_REGISTER_R0|PS_CHANNEL_ALPHA, 0 );

        // Set constants to scale output to an NTSC-safe range
        // NTSC_MAX = 0.8549f => 0x00dadada
        // NTSC_MIN = 0.0625f => 0x00101010
        psd.PSFinalCombinerConstant0 = 0x00dadada;
        psd.PSFinalCombinerConstant1 = 0x00101010;
        psd.PSFinalCombinerConstants = PS_FINALCOMBINERCONSTANTS(0,1,PS_GLOBALFLAGS_NO_TEXMODE_ADJUST);

        // Create the pixel shader, as defined above.
        if( FAILED( D3DDevice::CreatePixelShader( &psd, &m_dwPixelShader ) ) )
            return E_FAIL;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: CDraw2D::Create()
// Desc: Loads the sprites and creates the necessary resources
//-----------------------------------------------------------------------------
HRESULT CDraw2D::Create( const CHAR* strFontResourceFileName )
{
    // Load the resource for the font
    if( FAILED( m_xprResource.Create( strFontResourceFileName ) ) )
        return E_FAIL;

    if( FAILED( CreateShaders() ) )
        return E_FAIL;

    // Determine whether we should save/restore state
    D3DDEVICE_CREATION_PARAMETERS d3dcp;
    D3DDevice::GetCreationParameters( &d3dcp );
    m_bSaveState = (d3dcp.BehaviorFlags&D3DCREATE_PUREDEVICE) ? FALSE : TRUE;

    m_dwNestedBeginCount = 0;
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: CDraw2D::Destroy()
// Desc: Frees the sprite resources
//-----------------------------------------------------------------------------
HRESULT CDraw2D::Destroy()
{
    m_xprResource.Destroy();
    
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: CDraw2D::Begin()
// Desc: Prepares the font vertex buffers for rendering.
//-----------------------------------------------------------------------------
HRESULT CDraw2D::Begin()
{
    // Set state on the first call
    if( 0 == m_dwNestedBeginCount )
    {
        // Save state
        if( m_bSaveState )
        {
            // Note, we are not saving the texture, vertex, or pixel shader,
            //       since it's not worth the performance. We're more interested
            //       in saving state that would cause hard to find problems.
            D3DDevice::GetRenderState( D3DRS_ALPHABLENDENABLE, &m_dwSavedState[0] );
            D3DDevice::GetRenderState( D3DRS_SRCBLEND,         &m_dwSavedState[1] );
            D3DDevice::GetRenderState( D3DRS_DESTBLEND,        &m_dwSavedState[2] );
            D3DDevice::GetRenderState( D3DRS_ALPHATESTENABLE,  &m_dwSavedState[3] );
            D3DDevice::GetRenderState( D3DRS_ALPHAREF,         &m_dwSavedState[4] );
            D3DDevice::GetRenderState( D3DRS_ALPHAFUNC,        &m_dwSavedState[5] );
            D3DDevice::GetRenderState( D3DRS_FILLMODE,         &m_dwSavedState[6] );
            D3DDevice::GetRenderState( D3DRS_CULLMODE,         &m_dwSavedState[7] );
            D3DDevice::GetRenderState( D3DRS_ZENABLE,          &m_dwSavedState[8] );
            D3DDevice::GetRenderState( D3DRS_STENCILENABLE,    &m_dwSavedState[9] );
            D3DDevice::GetRenderState( D3DRS_EDGEANTIALIAS,    &m_dwSavedState[10] );
            D3DDevice::GetTextureStageState( 0, D3DTSS_MINFILTER, &m_dwSavedState[11] );
            D3DDevice::GetTextureStageState( 0, D3DTSS_MAGFILTER, &m_dwSavedState[12] );
        }

        // Set the necessary render states

        D3DDevice::SetVertexShader( m_dwVertexShader );
        D3DDevice::SetPixelShader( m_dwPixelShader );
        D3DDevice::SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
        D3DDevice::SetRenderState( D3DRS_SRCBLEND,         D3DBLEND_SRCALPHA );
        D3DDevice::SetRenderState( D3DRS_DESTBLEND,        D3DBLEND_INVSRCALPHA );
        D3DDevice::SetRenderState( D3DRS_ALPHATESTENABLE,  TRUE );
        D3DDevice::SetRenderState( D3DRS_ALPHAREF,         0x08 );
        D3DDevice::SetRenderState( D3DRS_ALPHAFUNC,        D3DCMP_GREATEREQUAL );
        D3DDevice::SetRenderState( D3DRS_FILLMODE,         D3DFILL_SOLID );
        D3DDevice::SetRenderState( D3DRS_CULLMODE,         D3DCULL_CCW );
        D3DDevice::SetRenderState( D3DRS_ZENABLE,          FALSE );
        D3DDevice::SetRenderState( D3DRS_STENCILENABLE,    FALSE );
        D3DDevice::SetRenderState( D3DRS_EDGEANTIALIAS,    FALSE );
        D3DDevice::SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
        D3DDevice::SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );

        // Begin the drawing of vertices
    }

    // Keep track of the nested begin/end calls.
    m_dwNestedBeginCount++;

    return S_OK;
}




//------------------------------------------------------------------------------
// Name: CDraw2D::DrawSprite()
// Desc: Draws a sprite on the screen at the given position, color, and scale
//       idx is the index of the sprite in the file, one of the GFX_ defines 
//       in constants.h
//------------------------------------------------------------------------------
HRESULT CDraw2D::DrawSprite( FLOAT fOriginX, FLOAT fOriginY, FLOAT fScale, DWORD dwColor, DWORD idx )
{
    FLOAT u, v;
    
    // Set up stuff (i.e. lock the vertex buffer) to prepare for drawing text
    Begin();

    // Set the common color for all the vertices to follow
    D3DDevice::SetVertexDataColor( D3DVSDE_DIFFUSE, dwColor );

    D3DTexture *pTex = m_xprResource.GetTexture( 20UL * idx );
    
    D3DDevice::SetTexture( 0, pTex );
    D3DDevice::Begin( D3DPT_QUADLIST );

    if ( idx > GFX_GIRL_START_IDLE ) idx = GFX_GIRL_START_IDLE;

    // Setup the screen coordinates
    FLOAT left = fOriginX - fScale * (sprites[idx].ox) + 0.5f;
    FLOAT right = fOriginX + fScale * (sprites[idx].tx - sprites[idx].ox) + 0.5f;
    FLOAT top = fOriginY - fScale * (sprites[idx].oy) + 0.5f;
    FLOAT bottom = fOriginY + fScale * (sprites[idx].ty - sprites[idx].oy) + 0.5f;
    u = 1.0f; v = 1.0f;

    // Draw the quad using SetVertexData
    D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, left,  bottom, 0.0f, v );
    D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, left,  top,    0.0f, 0.0f );
    D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, right, top,    u, 0.0f );
    D3DDevice::SetVertexData4f( D3DVSDE_VERTEX, right, bottom, u, v );
    D3DDevice::End();

    // Call End() to complete the begin/end pair for drawing text
    End();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: CDraw2D::End()
// Desc: Called after Begin(), this function triggers the rendering of the
//       vertex buffer contents filled during calls to DrawText().
//-----------------------------------------------------------------------------
HRESULT CDraw2D::End()
{
    // Keep track of nested calls to begin/end.
    if( 0L == m_dwNestedBeginCount )
        return E_FAIL;
    if( --m_dwNestedBeginCount > 0 )
        return S_FALSE;

    // End the drawing of vertex data
    // Restore state
    if( m_bSaveState )
    {
        D3DDevice::SetTexture( 0, NULL );
        D3DDevice::SetPixelShader( NULL );
        D3DDevice::SetRenderState( D3DRS_ALPHABLENDENABLE, m_dwSavedState[0] );
        D3DDevice::SetRenderState( D3DRS_SRCBLEND,         m_dwSavedState[1] );
        D3DDevice::SetRenderState( D3DRS_DESTBLEND,        m_dwSavedState[2] );
        D3DDevice::SetRenderState( D3DRS_ALPHATESTENABLE,  m_dwSavedState[3] );
        D3DDevice::SetRenderState( D3DRS_ALPHAREF,         m_dwSavedState[4] );
        D3DDevice::SetRenderState( D3DRS_ALPHAFUNC,        m_dwSavedState[5] );
        D3DDevice::SetRenderState( D3DRS_FILLMODE,         m_dwSavedState[6] );
        D3DDevice::SetRenderState( D3DRS_CULLMODE,         m_dwSavedState[7] );
        D3DDevice::SetRenderState( D3DRS_ZENABLE,          m_dwSavedState[8] );
        D3DDevice::SetRenderState( D3DRS_STENCILENABLE,    m_dwSavedState[9] );
        D3DDevice::SetRenderState( D3DRS_EDGEANTIALIAS,    m_dwSavedState[10] );
        D3DDevice::SetTextureStageState( 0, D3DTSS_MINFILTER, m_dwSavedState[11] );
        D3DDevice::SetTextureStageState( 0, D3DTSS_MAGFILTER, m_dwSavedState[12] );
    }

    return S_OK;
}

