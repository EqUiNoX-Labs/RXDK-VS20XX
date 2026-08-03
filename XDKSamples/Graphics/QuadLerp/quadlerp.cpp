//-----------------------------------------------------------------------------
// File: QuadLerp.cpp
//
// Desc: Demonstrates the optimizing of pixel shaders. The pixel shaders
//       show several implementations of the same pixel shader, with varying
//       degrees of optimization.
//
//       The shaders all implement the same effect, which is to linearly
//       interpolate between four textures based on the r, g, b, and a
//       components of the specular color.
//
//       This sample is a companion to the "Pixel Shader Optimizing" whitepaper.
//
// Hist: 12.15.00 - New for December XDK release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <XBResource.h>
#include <XBUtil.h>




//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_LEFTSTICK,    XBHELP_PLACEMENT_1, L"Adjust specular/\ntexture weights" },
    { XBHELP_RIGHTSTICK,   XBHELP_PLACEMENT_1, L"Rotate object" },
    { XBHELP_DPAD,         XBHELP_PLACEMENT_1, L"Zoom" },
    { XBHELP_X_BUTTON,     XBHELP_PLACEMENT_2, L"Cycle\npshaders" },
    { XBHELP_Y_BUTTON,     XBHELP_PLACEMENT_1, L"Reset" },
    { XBHELP_BACK_BUTTON,  XBHELP_PLACEMENT_1, L"Display\nhelp" },
};

#define NUM_HELP_CALLOUTS ( sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] ) )


// How many different shader variants do we have?
const DWORD NUMSHADERS = 4;
const DWORD NUMVBUFFERS = 4;

const WCHAR* ShaderDescriptions[NUMSHADERS] =
{
    L"unoptimized shader",
    L"optimized shader",
    L"optimized, with clipping",
    L"optimized, no times two",
};



//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
struct CUSTOMVERTEX
{
    D3DXVECTOR3 position;   // The position
    D3DXVECTOR3 normal;     // The vertex normals
    D3DCOLOR    color;      // The diffuse color
    D3DCOLOR    specular;   // Specular
    FLOAT       tu0, tv0;   // Texture 0 coordinates
    FLOAT       tu1, tv1;   // Texture 1 coordinates
    FLOAT       tu2, tv2;   // Texture 2 coordinates
    FLOAT       tu3, tv3;   // Texture 3 coordinates
};

// Our custom FVF, which describes our custom vertex structure
#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_DIFFUSE|D3DFVF_SPECULAR|D3DFVF_TEX4)




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    CXBPackedResource   m_xprResource;        // Packed resources for the app
    CXBFont             m_Font;               // Font class
    CXBHelp             m_Help;               // Help class
    BOOL                m_bDrawHelp;          // Whether to draw help

    // Multi-buffer the vertex buffer to avoid stalling the pipeline
    // Double buffering is not sufficient for this sample - it needs to be
    // quad buffered. In normal apps double or triple buffering would suffice.
    DWORD                   m_NextVBuffer;
    LPDIRECT3DVERTEXBUFFER8 m_pQuadVBs[NUMVBUFFERS];    // Buffers for quad vertices
    LPDIRECT3DTEXTURE8      m_pTexture0;
    LPDIRECT3DTEXTURE8      m_pTexture1;
    LPDIRECT3DTEXTURE8      m_pTexture2;
    LPDIRECT3DTEXTURE8      m_pTexture3;

    DWORD       m_dwCurrentPixelShader;
    DWORD       m_dwPixelShaders[NUMSHADERS];// Handles for the pixel shaders

    D3DXMATRIX  m_matObject;        // Transform matrix for the object
    FLOAT       m_fEyeScale;        // Scale of viewing distance
    FLOAT       m_hShift, m_vShift;

    VOID        UpdateQuadVertices();
    VOID        Reset();
    D3DCOLOR    EvalSpecular(FLOAT u, FLOAT v);

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
#if defined(_DEBUG) || defined(PROFILE)  // Don't vsync when profiling or debugging
    // Allow unlimited frame rate
    m_d3dpp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
#endif
    m_bDrawHelp     = FALSE;

    m_NextVBuffer   = 0;
    ZeroMemory( m_pQuadVBs, sizeof( m_pQuadVBs ) );

    m_pTexture0     = NULL;
    m_pTexture1     = NULL;
    m_pTexture2     = NULL;
    m_pTexture3     = NULL;

    ZeroMemory( m_dwPixelShaders, sizeof( m_dwPixelShaders ) );

    m_fEyeScale     = .50f;
    Reset();

    D3DXMatrixIdentity( &m_matObject );
}




//-----------------------------------------------------------------------------
// Name: Reset()
// Desc: Reset the texture weighting and pixel shader.
//-----------------------------------------------------------------------------
VOID CXBoxSample::Reset()
{
    m_dwCurrentPixelShader = 0;
    m_hShift        = 0;
    m_vShift        = 0;
}




//-----------------------------------------------------------------------------
// Name: EvalSpecular()
// Desc: This function takes u and v locations (in the -1.0 to 1.0 range),
// offsets them by m_hShift and m_vShift and
// uses these to calculate the specular value for the current point. The idea
// is that when the default values are passed in we get an even blend of the
// four textures, with each texture being used at 100% in one corner. As we
// shift the coordinates towards one corner that texture beings to dominate,
// until it covers the entire quad.
// Think of scrolling around a 3x3 grid. The corner squares of the grid are
// 100% of one texture. The other five squares interpolate between. We start
// out displaying the middle square - interpolating between four textures -
// but we can shift around the 3x3 grid.
//-----------------------------------------------------------------------------
D3DCOLOR CXBoxSample::EvalSpecular(FLOAT u, FLOAT v)
{
    // Given parametric u & v coordinates for a vertex within our model, we compute
    // and return the specular color (used as shading coefficients within our
    // pixel shader) based on the current application state.
    int red = 0, green = 0, blue = 0, alpha = 0;

    // Convert u & v from [0..1] range to [-1..1] range
    u = u * 2.0f - 1.0f;
    v = v * 2.0f - 1.0f;

    // Assume m_hShift and m_vShift go from -1.0 to 1.0, starting at 0.0
    u += m_hShift * 2;
    v += m_vShift * 2;

    // Red is in the top right corner
    if ( u < -1.0 || v < -1.0 )
        red = 0;
    else
    {
        FLOAT xPart = (FLOAT)min(1.0, ( u + 1.0 ) * 0.5 );
        FLOAT yPart = (FLOAT)min(1.0, ( v + 1.0 ) * 0.5 );
        red = int(255 * xPart * yPart + 0.5);
    }

    // Green is in the bottom left corner
    if ( u > 1.0 || v > 1.0 )
        green = 0;
    else
    {
        FLOAT xPart = (FLOAT)min(1.0, ( 1.0 - u ) * 0.5 );
        FLOAT yPart = (FLOAT)min(1.0, ( 1.0 - v ) * 0.5 );
        green = int(255 * xPart * yPart + 0.5);
    }

    // Blue is in the top left corner
    if ( u > 1.0 || v < -1.0 )
        blue = 0;
    else
    {
        FLOAT xPart = (FLOAT)min(1.0, ( 1.0 - u ) * 0.5 );
        FLOAT yPart = (FLOAT)min(1.0, ( v + 1.0 ) * 0.5 );
        blue = int(255 * xPart * yPart + 0.5);
    }

    // Alpha is in the bottom right corner
    if ( u < -1.0 || v > 1.0 )
        alpha = 0;
    else
    {
        FLOAT xPart = (FLOAT)min(1.0, ( u + 1.0 ) * 0.5 );
        FLOAT yPart = (FLOAT)min(1.0, ( 1.0 - v ) * 0.5 );
        alpha = int(255 * xPart * yPart + 0.5);
    }

    red = min(max(red, 0), 255);
    green = min(max(green, 0), 255);
    blue = min(max(blue, 0), 255);
    alpha = min(max(alpha, 0), 255);

    return D3DCOLOR_ARGB(alpha, red, green, blue);
}




//-----------------------------------------------------------------------------
// Name: UpdateQuadVertices()
// Desc: Move to the next vertex buffer and fill it in, based on the
// current values for m_hShift and m_vShift. Only the specular data
// is changed, but the entire buffer is rewritten, for simplicity.
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateQuadVertices()
{
    // Since we are locking a vertex buffer and writing to it we do
    // multi-buffering (currently quad buffering) to avoid stalls.
    // This ensures that we don't try locking a buffer until the GPU
    // is long since finished with it.

    // Get the index of the next buffer.
    ++m_NextVBuffer;
    if ( m_NextVBuffer >= NUMVBUFFERS )
        m_NextVBuffer = 0;
    CUSTOMVERTEX* v;

    // Then lock it.
    m_pQuadVBs[m_NextVBuffer]->Lock( 0, 0, (BYTE**)&v, 0 );

    for( DWORD i = 0; i < 4; i++ )
    {
        // Convert the vertex number into x/y coordinates of a square.
        FLOAT x = i<2 ? 1.0f : -1.0f;
        FLOAT y = i%2 ? 1.0f : -1.0f;

        v[i].position = D3DXVECTOR3( x, y, 0.0f );
        v[i].normal   = D3DXVECTOR3( 0.0f, 0.0f, -1.0f );
        v[i].tu0      = v[i].tu1 = v[i].tu2 = v[i].tu3 = (1+x)/2;
        v[i].tv0      = v[i].tv1 = v[i].tv2 = v[i].tv3 = (1-y)/2;

        // Calculate the appropriate specular value.
        v[i].specular = EvalSpecular( v[i].tu0, v[i].tv0 );

        // Always use a 50% grey color
        v[i].color = 0x7f7f7f7f;
    }
    m_pQuadVBs[m_NextVBuffer]->Unlock();
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

    // Load the textures
    m_pTexture0 = m_xprResource.GetTexture( "Texture0" );
    m_pTexture1 = m_xprResource.GetTexture( "Texture1" );
    m_pTexture2 = m_xprResource.GetTexture( "Texture2" );
    m_pTexture3 = m_xprResource.GetTexture( "Texture3" );
    
    // Allocate space for some quad vertex buffers
    for( DWORD i = 0; i < NUMVBUFFERS; ++i)
    {
        if( FAILED( m_pd3dDevice->CreateVertexBuffer( 4*sizeof(CUSTOMVERTEX),
                                                    0, D3DFVF_CUSTOMVERTEX,
                                                    D3DPOOL_DEFAULT, &m_pQuadVBs[i] ) ) )
            return E_FAIL;
    }

    UpdateQuadVertices();

    // Create the file-based pixel shaders. These shaders use the specular value to
    // linearly interpolate between four textures
    // This code includes the pixel shaders that are compiled earlier on in the
    // build process.

    {
#include "pshader0.inl"
        g_pd3dDevice->CreatePixelShader( &psd, &m_dwPixelShaders[0] );
    }
    {
#include "pshader1.inl"
        g_pd3dDevice->CreatePixelShader( &psd, &m_dwPixelShaders[1] );
    }
    {
#include "pshader2.inl"
        g_pd3dDevice->CreatePixelShader( &psd, &m_dwPixelShaders[2] );
    }
    {
#include "pshader3.inl"
        g_pd3dDevice->CreatePixelShader( &psd, &m_dwPixelShaders[3] );
    }

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

    // The left thumb stick is used to shift the weighting of the specular
    // color to make it more red, green, blue, or alpha. This emphasizes one
    // image over another. These constants affect how quickly it changes,
    // and what the maximum adjustment is.
    // It is safe to adjust the speed, but the maximum shift should be left
    // at 1.
    const int kShiftSpeed = 2;
    const int kMaximumShift = 1;
    m_hShift += m_DefaultGamepad.fX1 * m_fElapsedTime * kShiftSpeed;
    if (m_hShift < -kMaximumShift)
        m_hShift = -kMaximumShift;
    if (m_hShift > kMaximumShift)
        m_hShift = kMaximumShift;

    m_vShift -= m_DefaultGamepad.fY1 * m_fElapsedTime * kShiftSpeed;
    if (m_vShift < -kMaximumShift)
        m_vShift = -kMaximumShift;
    if (m_vShift > kMaximumShift)
        m_vShift = kMaximumShift;
    // Update the specular color in the vertex buffer to use the new shift amounts.
    UpdateQuadVertices();

    // Perform object rotation
    D3DXMATRIX matRotate;
    FLOAT fXRotate1 = m_DefaultGamepad.fX2*m_fElapsedTime*D3DX_PI*0.5f;
    FLOAT fYRotate1 = m_DefaultGamepad.fY2*m_fElapsedTime*D3DX_PI*0.5f;
    D3DXMatrixRotationYawPitchRoll( &matRotate, -fXRotate1, -fYRotate1, 0.0f );
    D3DXMatrixMultiply( &m_matObject, &m_matObject, &matRotate );

    // Zoom
    if( m_DefaultGamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP )
        m_fEyeScale = m_fEyeScale/1.01f;
    if( m_DefaultGamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN )
        m_fEyeScale *= 1.01f;

    // Toggle pixel shaders
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_X] )
    {
        ++m_dwCurrentPixelShader;
        if( m_dwCurrentPixelShader >= NUMSHADERS )
            m_dwCurrentPixelShader = 0;
    }

    // Reset texture weighting and pixel shader.
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_Y] )
    {
        Reset();
    }

    // Set transforms
    const D3DXVECTOR3 vEyePos( 0.0f * m_fEyeScale, 3.0f * m_fEyeScale, -5.0f * m_fEyeScale );
    const D3DXVECTOR3 vLookAt( 0.0f, 0.0f, 0.0f );
    const D3DXVECTOR3 vUp    ( 0.0f, 1.0f, 0.0f );
    D3DXMATRIX matView;
    D3DXMatrixLookAtLH( &matView, &vEyePos, &vLookAt, &vUp );

    D3DXMATRIX matProj;
    D3DXMatrixPerspectiveFovLH( &matProj, D3DX_PI/4, 1.0f, 1.0f, 100.0f );
    m_pd3dDevice->SetTransform( D3DTS_WORLD, &m_matObject );
    m_pd3dDevice->SetTransform( D3DTS_VIEW, &matView );
    m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &matProj );

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
    // Draw a gradient filled background
    RenderGradientBackground( 0xff0000ff, 0xff000000 );

    // Set default state
    for( DWORD i = 0; i < 4; ++i )
    {
        m_pd3dDevice->SetTextureStageState( i, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetTextureStageState( i, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetTextureStageState( i, D3DTSS_ADDRESSU,    D3DTADDRESS_CLAMP );
        m_pd3dDevice->SetTextureStageState( i, D3DTSS_ADDRESSV,    D3DTADDRESS_CLAMP );
    }
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    m_pd3dDevice->SetRenderState( D3DRS_LIGHTING,        FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE,         FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE,    FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_SPECULARENABLE, TRUE );
    
    m_pd3dDevice->SetTexture( 0, m_pTexture0 );
    m_pd3dDevice->SetTexture( 1, m_pTexture1 );
    m_pd3dDevice->SetTexture( 2, m_pTexture2 );
    m_pd3dDevice->SetTexture( 3, m_pTexture3 );

    // Specify pixel shader
    m_pd3dDevice->SetPixelShader( m_dwPixelShaders[m_dwCurrentPixelShader] );

    // Render the quad
    m_pd3dDevice->SetStreamSource( 0, m_pQuadVBs[m_NextVBuffer] , sizeof(CUSTOMVERTEX) );
    m_pd3dDevice->SetVertexShader( D3DFVF_CUSTOMVERTEX );
    m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2 );

    // Restore states
    m_pd3dDevice->SetPixelShader( NULL );
    m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
    // Clear the textures so that future renders aren't slowed down by having excessive
    // texture fetches.
    m_pd3dDevice->SetTexture( 0, 0 );
    m_pd3dDevice->SetTexture( 1, 0 );
    m_pd3dDevice->SetTexture( 2, 0 );
    m_pd3dDevice->SetTexture( 3, 0 );

    // Show title, frame rate, and help
    if( m_bDrawHelp )
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 48, 36, 0xffffffff,  L"QuadLerp" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 592, 38, 0xffffff00, m_strFrameRate, XBFONT_RIGHT );

        const DWORD bufSize = 1000;
        WCHAR    buffer[bufSize];
        // Print a description of the shader, checking against buffer overrun.
        _snwprintf( buffer, bufSize, L"Using shader %d - %s", m_dwCurrentPixelShader, ShaderDescriptions[m_dwCurrentPixelShader] );
        // Ensure null termination.
        buffer[bufSize-1] = 0;
        m_Font.DrawText( 48, 60, 0xffffffff, buffer );

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}
