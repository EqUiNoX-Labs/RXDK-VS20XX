//-----------------------------------------------------------------------------
// File: HighQualityBumpMapping.cpp
//
// Desc: A sample showing a technique to achieve high quality bump mapping by
//       using a 2D dependant texture lookup for specular lighting that encodes 
//       both the length of the half-angle vector and the specular function.
//
// Hist: 02.01.03 - New for Feb 2003 XDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBUtil.h>
#include <XBHelp.h>
#include <XBResource.h>
#include <xgraphics.h>
#include "BumpMesh.h"




//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_LEFTSTICK,    XBHELP_PLACEMENT_1, L"Rotate scene" },
    { XBHELP_RIGHTSTICK,   XBHELP_PLACEMENT_1, L"Rotate light" },
    { XBHELP_A_BUTTON,     XBHELP_PLACEMENT_1, L"Switch Model" },
    { XBHELP_B_BUTTON,     XBHELP_PLACEMENT_1, L"Switch\nNormalMap" },
    { XBHELP_X_BUTTON,     XBHELP_PLACEMENT_1, L"Toggle Normalization" },
    { XBHELP_DPAD,         XBHELP_PLACEMENT_1, L"Adjust specular\nexponent" },
    { XBHELP_BACK_BUTTON,  XBHELP_PLACEMENT_1, L"Display help" },
};

#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts)/sizeof(g_HelpCallouts[0]))




//-----------------------------------------------------------------------------
// Names of meshes to use.
//-----------------------------------------------------------------------------
CHAR* g_strMeshNames[] = 
{
    "Models\\Torus.xbg",
    "Models\\Sphere.xbg",
    "Models\\Cube.xbg"
};

#define NUM_MESHES (sizeof(g_strMeshNames)/sizeof(g_strMeshNames[0]))




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Application class. The base class provides just about all the
//       functionality we want, so we're just supplying stubs to interface with
//       the non-C++ functions of the app.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    CXBPackedResource       m_xprResource;      // Packed resources for the app
    CXBFont                 m_Font;             // Font class
    CXBHelp                 m_Help;             // Help class
    BOOL                    m_bDrawHelp;        // Whether to draw help

    D3DXVECTOR3             m_vEye;             // Eye location
    D3DXVECTOR3             m_vAt;              // Eye lookat
    D3DXVECTOR3             m_vUp;              // Eye up direction

    D3DXVECTOR3             m_vLightPos;        // Light position.

    FLOAT                   m_fAngleObjYaw;     // Object rotation angles
    FLOAT                   m_fAngleObjPitch;

    LPDIRECT3DTEXTURE8      m_pSpecularTexture;
    LPDIRECT3DTEXTURE8      m_pUnnormSpecularTexture;
    LPDIRECT3DCUBETEXTURE8  m_pNormalizationCubeMap;
    DWORD                   m_dwNumNormalMaps;
    LPDIRECT3DTEXTURE8*     m_pNormalMaps; // Normal map textures.

    DWORD                   m_dwPixelShader;        // Pixel shader handle
    DWORD                   m_dwVertexShader;       // Vertex shader handle

    BumpMesh                m_Mesh[NUM_MESHES];     // Bump mapped meshes.

    DWORD                   m_dwCurrentNormalMap;   // Index of normal map.
    DWORD                   m_dwCurrentMesh;        // Index of mesh.

    FLOAT                   m_fSpecularExponent;

    BOOL                    m_bPerPixelNormalization;

    // Update the specular function map values for a given power.
    VOID UpdateSpecularMap( FLOAT fPower );

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
// Desc: Application constructor. Sets attributes for the app.
//-----------------------------------------------------------------------------
CXBoxSample::CXBoxSample()
            :CXBApplication()
{
    // Allow unlimited frame rate
    m_d3dpp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    // Initialize members
    m_bDrawHelp        = FALSE;
    
    m_vEye             = D3DXVECTOR3( 0.0f, 0.0f,-50.0f );
    m_vAt              = D3DXVECTOR3( 0.0f, 0.0f,  0.0f );
    m_vUp              = D3DXVECTOR3( 0.0f, 1.0f,  0.0f );

    m_vLightPos        = D3DXVECTOR3( 0.0f, 100.0f, -100.0f );

    m_fAngleObjYaw     =  0.0f;
    m_fAngleObjPitch   =  0.0f;

    m_pSpecularTexture = 0;
    m_pNormalizationCubeMap = 0;

    m_dwPixelShader    = 0;
    m_dwVertexShader   = 0;

    m_pNormalMaps        = NULL;
    m_dwNumNormalMaps    = 0;
    m_dwCurrentNormalMap = 0;
    m_dwCurrentMesh      = 0;

    m_fSpecularExponent = 32.0f;

    m_bPerPixelNormalization = true;
}




//-----------------------------------------------------------------------------
// Name: UpdateSpecularMap()
// Desc: Update the specular power function textures for a new power.
//-----------------------------------------------------------------------------
VOID CXBoxSample::UpdateSpecularMap( FLOAT fPower )
{
    // Update the 2D specular lookup texture.
    D3DSURFACE_DESC desc;
    m_pSpecularTexture->GetLevelDesc( 0, &desc );

    DWORD dwWidth = desc.Width;
    DWORD dwHeight = desc.Height;

    D3DLOCKED_RECT lock;
    m_pSpecularTexture->LockRect( 0, &lock, 0, 0 );

    BYTE* pBits = (BYTE*)lock.pBits;

    Swizzler S( dwWidth, dwHeight, 1 );

    FLOAT fInvHeight = 1.0f / dwHeight;
    FLOAT fInvWidth = 1.0f / dwWidth;

    for( DWORD index = 0; index < dwWidth*dwHeight; index++ )
    {
        DWORD x = S.UnswizzleU( index );
        DWORD y = S.UnswizzleV( index );

        FLOAT fLength = FLOAT(y + 0.5f) * fInvHeight;
        FLOAT fDot = FLOAT(x + 0.5f) * fInvWidth;
        FLOAT fValue = powf(fDot / fLength, fPower);

        if (fValue > 1.0f) fValue = 1.0f;

        // Clamp values for lengths close to zero to avoid artifacts.
        if (y == 0) fValue = 0.0f;

        pBits[index] = BYTE(fValue*255.0f);
    }

    m_pSpecularTexture->UnlockRect(0);

    // Update the 1D un-normalized texture.
    m_pUnnormSpecularTexture->GetLevelDesc( 0, &desc );

    dwWidth = desc.Width;
    dwHeight = desc.Height;

    m_pUnnormSpecularTexture->LockRect( 0, &lock, 0, 0 );

    pBits = (BYTE*)lock.pBits;

    for( DWORD x = 0; x < dwWidth; x++ )
    {
        FLOAT fDot = FLOAT(x + 0.5f) * fInvWidth;
        FLOAT fValue = powf(fDot, fPower);

        if (fValue > 1.0f) fValue = 1.0f;

        pBits[x] = BYTE(fValue*255.0f);
    }

    m_pUnnormSpecularTexture->UnlockRect(0);
}




//-----------------------------------------------------------------------------
// Name: XBUtil_CreateNormalizationCubeMapHILO()
// Desc: Creates a cubemap and fills it with normalized U16V16 vectors in HILO
//       hemisphere format.
//       The map is only really useful when doing tangent space bump mapping.
//-----------------------------------------------------------------------------
HRESULT XBUtil_CreateNormalizationCubeMapHILO( DWORD dwSize, 
                                               LPDIRECT3DCUBETEXTURE8* ppCubeMap )
{
    HRESULT hr;

    // Create the cube map
    if( FAILED( hr = D3DDevice::CreateCubeTexture( dwSize, 1, 0, D3DFMT_V16U16, 
                                                   D3DPOOL_DEFAULT, ppCubeMap ) ) )
        return E_FAIL;
    
    // Allocate temp space for swizzling the cubemap surfaces
    DWORD* pSourceBits = new DWORD[ dwSize * dwSize ];
    assert( pSourceBits );

    // Fill all six sides of the cubemap
    for( DWORD i=0; i<6; i++ )
    {
        // Lock the i'th cubemap surface
        LPDIRECT3DSURFACE8 pCubeMapFace;
        (*ppCubeMap)->GetCubeMapSurface( (D3DCUBEMAP_FACES)i, 0, &pCubeMapFace );

        // Write the HILO hemisphere encoded normals to the surface pixels
        DWORD*      pPixel = pSourceBits;
        D3DXVECTOR3 n;
        FLOAT       w, h;

        for( DWORD y = 0; y < dwSize; y++ )
        {
            h  = (FLOAT)y / (FLOAT)(dwSize-1);  // 0 to 1
            h  = ( h * 2.0f ) - 1.0f;           // -1 to 1
            
            for( DWORD x = 0; x < dwSize; x++ )
            {
                w = (FLOAT)x / (FLOAT)(dwSize-1);   // 0 to 1
                w = ( w * 2.0f ) - 1.0f;            // -1 to 1

                // Calc the normal for this texel
                switch( i )
                {
                    case D3DCUBEMAP_FACE_POSITIVE_X:    // +x
                        n.x = +1.0;
                        n.y = -h;
                        n.z = -w;
                        break;
                        
                    case D3DCUBEMAP_FACE_NEGATIVE_X:    // -x
                        n.x = -1.0;
                        n.y = -h;
                        n.z = +w;
                        break;
                        
                    case D3DCUBEMAP_FACE_POSITIVE_Y:    // y
                        n.x = +w;
                        n.y = +1.0;
                        n.z = +h;
                        break;
                        
                    case D3DCUBEMAP_FACE_NEGATIVE_Y:    // -y
                        n.x = +w;
                        n.y = -1.0;
                        n.z = -h;
                        break;
                        
                    case D3DCUBEMAP_FACE_POSITIVE_Z:    // +z
                        n.x = +w;
                        n.y = -h;
                        n.z = +1.0;
                        break;
                        
                    case D3DCUBEMAP_FACE_NEGATIVE_Z:    // -z
                        n.x = -w;
                        n.y = -h;
                        n.z = -1.0;
                        break;
                }

                // Store the normal as an RGBA color
                D3DXVec3Normalize( &n, &n );

                SHORT U = SHORT(n.x * 32767);
                SHORT V = SHORT(n.y * 32767);
                *pPixel++ = (U << 16) | (V & 0x0000ffff);
            }
        }
        
        // Swizzle the result into the cubemap face surface
        D3DLOCKED_RECT lock;
        pCubeMapFace->LockRect( &lock, 0, 0L );
        XGSwizzleRect( pSourceBits, 0, NULL, lock.pBits, dwSize, dwSize,
                       NULL, sizeof(DWORD) );
        pCubeMapFace->UnlockRect();

        // Release the cubemap face
        pCubeMapFace->Release();
    }

    // Free temp space
    delete[] pSourceBits;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: This creates all device-dependant display objects.
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

    // Get pointers to the normals maps.
    XBRESOURCE* pResourceTags;
    m_xprResource.GetResourceTags( &m_dwNumNormalMaps, &pResourceTags );

    m_pNormalMaps = new LPDIRECT3DTEXTURE8[m_dwNumNormalMaps];
    for( DWORD i = 0; i < m_dwNumNormalMaps; i++ )
    {
        m_pNormalMaps[i] = m_xprResource.GetTexture( pResourceTags[i].dwOffset );
    }

    // Create the normalization cube map.
    XBUtil_CreateNormalizationCubeMapHILO( 64, &m_pNormalizationCubeMap );

    // Create the 1D un-normalized specular lookup texture.
    if( FAILED( m_pd3dDevice->CreateTexture( 512, 1, 1, 0,
                                             D3DFMT_L8, D3DPOOL_MANAGED,
                                             &m_pUnnormSpecularTexture ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create the 2D normalized specular lookup texture.
    if( FAILED( m_pd3dDevice->CreateTexture( 512, 512, 1, 0,
                                             D3DFMT_L8, D3DPOOL_MANAGED,
                                             &m_pSpecularTexture ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Update specular maps.
    UpdateSpecularMap( m_fSpecularExponent );

    // Define the shader declaration. Vertex components come in two streams:
    // the first with the typical vertex components (position, normal, and
    // texcoords) and the second with vertex's basis vectors.
    DWORD dwShaderVertexDecl[] =
    {
        D3DVSD_STREAM( 0 ),
        D3DVSD_REG( 0, D3DVSDT_FLOAT3 ),    // Position
        D3DVSD_REG( 1, D3DVSDT_FLOAT3 ),    // Normal
        D3DVSD_REG( 2, D3DVSDT_FLOAT2 ),    // Base texture coordinate
        D3DVSD_STREAM( 1 ),
        D3DVSD_REG( 3, D3DVSDT_FLOAT3 ),    // S basis vector
        D3DVSD_REG( 4, D3DVSDT_FLOAT3 ),    // T basis vector
        D3DVSD_REG( 5, D3DVSDT_FLOAT3 ),    // SxT basis vector
        D3DVSD_END()
    };

    // Init vertex shader.
    if( FAILED( XBUtil_CreateVertexShader( "Shaders\\HQBumpShader.xvu",
                                           dwShaderVertexDecl, 
                                           &m_dwVertexShader ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Init pixel shader.
    if( FAILED( XBUtil_CreatePixelShader( "Shaders\\HQBumpShader.xpu",
                                           &m_dwPixelShader ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    for( DWORD i = 0; i < NUM_MESHES; i++ )
    {
        // Load an object to cast the shadow
        if( FAILED( m_Mesh[i].Create( g_strMeshNames[i], &m_xprResource ) ) )
            return XBAPPERR_MEDIANOTFOUND;

        if( FAILED( m_Mesh[i].CalculateTextureSpaceBasis() ) )
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

    // Move scene
    m_fAngleObjYaw   += -m_DefaultGamepad.fX1 * m_fElapsedTime * 100.0f;
    m_fAngleObjPitch += m_DefaultGamepad.fY1 * m_fElapsedTime * 100.0f;

    // Switch the mesh being displayed
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_A] ) 
    {
        if( ++m_dwCurrentMesh >= NUM_MESHES )
            m_dwCurrentMesh = 0;
    }

    // Switch the normal map being displayed.
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_B] ) 
    {
        if( ++m_dwCurrentNormalMap >= m_dwNumNormalMaps )
            m_dwCurrentNormalMap = 0;
    }
    
    // Togggle per-pixel normalization.
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_X] )
    {
        m_bPerPixelNormalization = !m_bPerPixelNormalization;
    }

    BOOL bSpecularUpdated = false;
    if( (m_DefaultGamepad.wLastButtons & XINPUT_GAMEPAD_DPAD_UP) &&
        m_fSpecularExponent < 1000.0f )
    {
        m_fSpecularExponent += INT(m_fSpecularExponent/16.0f) + 1.0f;
        bSpecularUpdated = true;
    }
    else if( (m_DefaultGamepad.wLastButtons & XINPUT_GAMEPAD_DPAD_DOWN) && 
             m_fSpecularExponent > 1.0f )
    {
        m_fSpecularExponent -= INT(m_fSpecularExponent/16.0f) + 1.0f;
        bSpecularUpdated = true;
    }

    if( bSpecularUpdated )
    {
        // Update specular maps.
        UpdateSpecularMap( m_fSpecularExponent );
    }

    D3DXMATRIX  matRotate;
    D3DXVECTOR3 vNormal( 0.0f, 0.0f, -1.0f );
    D3DXVECTOR3 vUp( 0.0f, 1.0f, 0.0f );
    D3DXVECTOR3 vTemp;

    // Rotate light around up axis.
    D3DXMatrixRotationAxis( &matRotate, &vUp, 
                            -m_DefaultGamepad.fX2 * m_fElapsedTime * 2.0f );
    D3DXVec3TransformCoord( &m_vLightPos, &m_vLightPos, &matRotate );

    // Rotate light around side axis.
    D3DXVECTOR3 axis( 1.0f, 0.0f, 0.0f );
    D3DXMatrixRotationAxis( &matRotate, &axis, 
                            m_DefaultGamepad.fY2 * m_fElapsedTime * 2.0f );
    D3DXVec3TransformCoord( &m_vLightPos, &m_vLightPos, &matRotate );

    // Copy the light direction to the mesh
    m_Mesh[m_dwCurrentMesh].m_vLightPos = m_vLightPos;

    // Set the matrices
    D3DXMATRIX matWorld, matView, matProj;
    D3DXMatrixRotationYawPitchRoll( &matWorld, 
                                    m_fAngleObjYaw * D3DX_PI / 180.0f,
                                    m_fAngleObjPitch * D3DX_PI / 180.0f, 0.0f );
    D3DXMatrixLookAtLH( &matView, &m_vEye, &m_vAt, &m_vUp );
    D3DXMatrixPerspectiveFovLH( &matProj, D3DX_PI/4, 640.0f/480.0f, 1.0f, 
                                10000.0f );
    
    g_pd3dDevice->SetTransform( D3DTS_WORLD,      &matWorld );
    g_pd3dDevice->SetTransform( D3DTS_VIEW,       &matView );
    g_pd3dDevice->SetTransform( D3DTS_PROJECTION, &matProj );

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
    // Clear the viewport
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL, 
                         0x001f1f7f, 1.0f, 0L );

    // Set default states
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_LIGHTING, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE,  TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );

    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 2, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 2, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 2, D3DTSS_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 3, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 3, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 3, D3DTSS_MIPFILTER, D3DTEXF_LINEAR );

    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 2, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 2, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 3, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 3, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP );

    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLORSIGN, D3DTSIGN_ASIGNED |
                                                             D3DTSIGN_RSIGNED |
                                                             D3DTSIGN_GSIGNED |
                                                             D3DTSIGN_BSIGNED );

    // Set states for drawing the bump effect
    m_pd3dDevice->SetVertexShader( m_dwVertexShader );
    m_pd3dDevice->SetPixelShader( m_dwPixelShader );

    m_pd3dDevice->SetTexture( 0, m_pNormalMaps[m_dwCurrentNormalMap] );
    m_pd3dDevice->SetTexture( 1, m_pNormalizationCubeMap );

    if( m_bPerPixelNormalization )
        m_pd3dDevice->SetTexture( 3, m_pSpecularTexture );
    else
        m_pd3dDevice->SetTexture( 3, m_pUnnormSpecularTexture );

    m_Mesh[m_dwCurrentMesh].Render( XBMESH_NOFVF | XBMESH_NOTEXTURES | XBMESH_NOMATERIALS );

    // Restore the state
    m_pd3dDevice->SetVertexShader( D3DFVF_XYZ );
    m_pd3dDevice->SetPixelShader( NULL );
    m_pd3dDevice->SetTexture( 0, NULL );
    m_pd3dDevice->SetTexture( 1, NULL );
    m_pd3dDevice->SetTexture( 3, NULL );

    // Show title, frame rate, and help
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();

        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 48, 36, 0xffffffff,  L"HighQualityBumpMapping" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 592, 38, 0xffffff00, m_strFrameRate, XBFONT_RIGHT );

        WCHAR str[100];
        swprintf( str, L"%.3f", m_fSpecularExponent );
        m_Font.DrawText( 64, 80, 0xffffffff, L"Specular Exponent: " );
        m_Font.DrawText( 0xffffff00, str );

        m_Font.DrawText( 64, 100, 0xffffffff, L"Normalization: " );
        if( m_bPerPixelNormalization )
            m_Font.DrawText( 0xffffff00, L"Per-Pixel" );
        else
            m_Font.DrawText( 0xffffff00, L"Per-Vertex" );

        m_Font.End();
    }
    
    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}
