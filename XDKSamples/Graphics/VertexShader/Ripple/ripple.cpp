//-----------------------------------------------------------------------------
// File: Ripple.cpp
//
// Desc: Uses a vertex shader to create a ripple effect in a mesh of vertices.
//       Draws the mesh in a vertex cache optimal matter.
//
// Hist: 11.01.00 - New for November XDK release
//       12.15.00 - Changes for December XDK release
//       03.28.00 - Added vertex cache optimizations
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <XBUtil.h>


// Mesh size (in vetices) for the grid
#define MESH_SIZE 128

// Swath width (int vertices) to draw for vertex caching.
#define SWATH_WIDTH 20




//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_LEFTSTICK,    XBHELP_PLACEMENT_2, L"Move ripple\norigin" },
    { XBHELP_BACK_BUTTON,  XBHELP_PLACEMENT_2, L"Display\nhelp" },
    { XBHELP_START_BUTTON, XBHELP_PLACEMENT_1, L"Pause" },
};

#define NUM_HELP_CALLOUTS 3




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    CXBFont      m_Font;
    CXBHelp      m_Help;
    BOOL         m_bDrawHelp;

    // Scene
    LPDIRECT3DVERTEXBUFFER8 m_pVB;
    LPDIRECT3DINDEXBUFFER8  m_pIB;
    DWORD        m_dwNumVertices;
    DWORD        m_dwNumIndices;
    DWORD        m_dwShader;

    // Transforms
    D3DXMATRIX   m_matPosition;
    D3DXMATRIX   m_matView;
    D3DXMATRIX   m_matProj;

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
    CXBoxSample d3dApp;
    if( FAILED( d3dApp.Create() ) )
        return;
    d3dApp.Run();
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

    m_bDrawHelp        = FALSE;

    m_pIB              = NULL;
    m_pVB              = NULL;
    m_dwNumIndices     = 4 * (MESH_SIZE-1) * (MESH_SIZE-1);
    m_dwNumVertices    = MESH_SIZE * MESH_SIZE;
    m_dwShader         = 0;

    // Setup the view matrix
    D3DXVECTOR3 vEye = D3DXVECTOR3( 3.0f, 3.0f, 5.0f );
    D3DXVECTOR3 vAt  = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );
    D3DXVECTOR3 vUp  = D3DXVECTOR3( 0.0f, 1.0f, 0.0f );
    D3DXMatrixLookAtRH( &m_matView, &vEye, &vAt, &vUp );

    // Set the position matrix
    D3DXMatrixInverse( &m_matPosition, NULL, &m_matView );
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize scene objects.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    HRESULT hr;

    // Create the font
    if( FAILED( m_Font.Create( "Font.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create the help
    if( FAILED( m_Help.Create( "Gamepad.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create index buffer
    {
        WORD* pIndices;

        if( FAILED( hr = m_pd3dDevice->CreateIndexBuffer( m_dwNumIndices*sizeof(WORD),
                                                          0, D3DFMT_INDEX16,
                                                          D3DPOOL_DEFAULT, &m_pIB ) ) )
            return hr;

        m_pIB->Lock( 0, m_dwNumIndices*sizeof(WORD), (BYTE**)&pIndices, 0 );

        DWORD swath_start = 0;
        while (swath_start < MESH_SIZE-1)
        {
            // Swath width in quads.
            DWORD swath_width = SWATH_WIDTH-1;

            if (swath_start+swath_width >= MESH_SIZE)
                swath_width = MESH_SIZE-1-swath_start;

            for( DWORD y = 0; y < MESH_SIZE-1; y++ )
            {
                for( DWORD x = swath_start; x < swath_start+swath_width; x++ )
                {
                    *pIndices++ = (WORD)( (x+0) + (y+1) * MESH_SIZE );
                    *pIndices++ = (WORD)( (x+0) + (y+0) * MESH_SIZE );
                    *pIndices++ = (WORD)( (x+1) + (y+0) * MESH_SIZE );
                    *pIndices++ = (WORD)( (x+1) + (y+1) * MESH_SIZE );
                }
            }

            swath_start += swath_width;
        }

        m_pIB->Unlock();
    }

    // Create vertex buffer
    {
        D3DXVECTOR2* pVertices;

        if( FAILED( hr = m_pd3dDevice->CreateVertexBuffer( m_dwNumVertices*sizeof(D3DXVECTOR2),
                                                           0, 0, D3DPOOL_DEFAULT, &m_pVB ) ) )
            return hr;

        m_pVB->Lock( 0, m_dwNumVertices*sizeof(D3DXVECTOR2), (BYTE**)&pVertices, 0 );

        for( DWORD y=0; y<MESH_SIZE; y++ )
            for( DWORD x=0; x<MESH_SIZE; x++ )
                *pVertices++ = D3DXVECTOR2( 4 * ((FLOAT)x / (FLOAT)(MESH_SIZE-1) - 0.5f),
                                            4 * ((FLOAT)y / (FLOAT)(MESH_SIZE-1) - 0.5f) );
        m_pVB->Unlock();
    }

    // Create vertex shader
    {
        DWORD dwDecl[] =
        {
            D3DVSD_STREAM(0),
            D3DVSD_REG(D3DVSDE_POSITION,  D3DVSDT_FLOAT2),
            D3DVSD_END()
        };

        if( FAILED( XBUtil_CreateVertexShader( "Shaders\\Ripple.xvu",
                                               dwDecl, &m_dwShader ) ) )
            return XBAPPERR_MEDIANOTFOUND;
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

    static FLOAT fFrequency = 15.0f;
    static FLOAT fXOffset   =  0.0f;
    static FLOAT fYOffset   =  0.0f;

    // Move the origin around with the gamepad control
    fXOffset -= m_DefaultGamepad.fX1 * m_fElapsedTime;
    fYOffset += m_DefaultGamepad.fY1 * m_fElapsedTime;

    // Navigation
    static D3DXVECTOR3 vVelocity( 0.0f, 0.0f, 0.0f );
    static D3DXVECTOR3 vAngularVelocity( 0.0f, 0.0f, 0.0f );
    static FLOAT       fSpeed           = 5.0f;
    static FLOAT       fAngularSpeed    = 1.0f;

    // Process input
    D3DXVECTOR3 vT( 0.0f, 0.0f, 0.0f );
    D3DXVECTOR3 vR( 0.0f, 0.0f, 0.0f );

    vVelocity        = 0.1f*vT + 0.9f*vVelocity;
    vAngularVelocity = 0.1f*vR + 0.9f*vAngularVelocity;

    // Update position and view matrices
    D3DXMATRIX     matT, matR;
    D3DXQUATERNION qR;

    vT = m_fElapsedAppTime * vVelocity * fSpeed;
    vR = m_fElapsedAppTime * vAngularVelocity * fAngularSpeed;

    D3DXMatrixTranslation( &matT, vT.x, vT.y, vT.z);
    D3DXMatrixMultiply( &m_matPosition, &matT, &m_matPosition );

    D3DXQuaternionRotationYawPitchRoll( &qR, vR.y, vR.x, vR.z );
    D3DXMatrixRotationQuaternion( &matR, &qR );

    // Build the matrices
    D3DXMatrixMultiply( &m_matPosition, &matR, &m_matPosition );
    D3DXMatrixInverse( &m_matView, NULL, &m_matPosition );
    D3DXMatrixPerspectiveFovRH( &m_matProj, D3DX_PI/4, 4.0f/3.0f, 0.1f, 100.0f );

    // Set up the vertex shader constants
    {
        D3DXMATRIX mat;
        D3DXMatrixMultiply( &mat, &m_matView, &m_matProj );
        D3DXMatrixTranspose( &mat, &mat );

        D3DXVECTOR4 vA( sinf(m_fAppTime)*15.0f, 0.0f, 0.5f, 1.0f );
        D3DXVECTOR4 vB( -m_fAppTime, fFrequency, fXOffset, fYOffset );
        D3DXVECTOR4 vD( D3DX_PI, 1.0f/(2.0f*D3DX_PI), 2.0f*D3DX_PI, 0.05f );

        // Series coefficients for sin and cos
        D3DXVECTOR4 vSin( 6.274509804000f, -40.993464050000f, 76.967320260000f, -53.542483660000f );
        D3DXVECTOR4 vCos( 1.000000000000f, -19.555555555556f, 60.444444444444f, -56.888888888889f );
        D3DXVECTOR4 vMisc( 0.5f, 0.5f/D3DX_PI, D3DX_PI, 1.0f);

        m_pd3dDevice->SetVertexShaderConstant(  0, &mat,   4 );
        m_pd3dDevice->SetVertexShaderConstant(  4, &vA,    1 );
        m_pd3dDevice->SetVertexShaderConstant(  5, &vB,    1 );
        m_pd3dDevice->SetVertexShaderConstant(  7, &vD,    1 );
        m_pd3dDevice->SetVertexShaderConstant( 10, &vSin,  1 );
        m_pd3dDevice->SetVertexShaderConstant( 11, &vCos,  1 );
        m_pd3dDevice->SetVertexShaderConstant( 12, &vMisc, 1 );
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: App_Render()
// Desc: Called once per frame, the call is the entry point for 3d
//       rendering. This function sets up render states, clears the
//       viewport, and renders the scene.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Render()
{
    // Draw a gradient filled background
    RenderGradientBackground( 0xff000000, 0xff0000ff );

    // Setup render states
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE,  FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_LIGHTING,         FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE,         D3DCULL_NONE );

    // Render the vertices using the vertex shader
    m_pd3dDevice->SetVertexShader( m_dwShader );
    m_pd3dDevice->SetStreamSource( 0, m_pVB, sizeof(D3DXVECTOR2) );
    m_pd3dDevice->SetIndices( m_pIB, 0 );

    // We have to have a begin/end when changing between the fixed function 
    // pipline and a vertex shader (or between vertex shaders) becuase the 
    // post-transform cache will be invalidated at the next begin resulting 
    // the vertices cached by PrimeVertexCache being flushed.
    m_pd3dDevice->Begin(D3DPT_QUADLIST);
    m_pd3dDevice->End();

    DWORD swath_start = 0;
    while( swath_start < MESH_SIZE-1 )
    {
        // Swath width in quads.
        DWORD swath_width = SWATH_WIDTH-1;

        if (swath_start+swath_width >= MESH_SIZE)
            swath_width = MESH_SIZE-1-swath_start;

        // Prime the vertex cache.
        WORD CacheIndices[SWATH_WIDTH];

        for( DWORD x = swath_start; x <= swath_start+swath_width; x++ )
        {
            CacheIndices[x-swath_start] = WORD(x);
        }

        // Get the first row of vertices in the swath into the cache.
        m_pd3dDevice->PrimeVertexCache(swath_width+1, CacheIndices);

        // Draw the swath
        m_pd3dDevice->DrawIndexedVertices( D3DPT_QUADLIST, 4*swath_width*(MESH_SIZE-1),
                                           D3D__IndexData + 4*swath_start*(MESH_SIZE-1) );

        swath_start += swath_width;
    }

    // Show title, frame rate, and help
    if( m_bDrawHelp )
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 48, 36, 0xffffffff,  L"Ripple" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 592, 38, 0xffffff00, m_strFrameRate, XBFONT_RIGHT );
        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}





