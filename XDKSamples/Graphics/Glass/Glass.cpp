//-----------------------------------------------------------------------------
// File: Glass.cpp
//
// Desc: Draws a glass model using special shaders to simulate refraction and
//       reflection.
//
// Hist: 05.28.03 - Ready for August 2003 XDK release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <XBMesh.h>
#include <XBResource.h>
#include <XBUtil.h>
#include <xgraphics.h>




//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_LEFTSTICK,    XBHELP_PLACEMENT_2, L"Move\nmodel" },
    { XBHELP_RIGHTSTICK,   XBHELP_PLACEMENT_2, L"Move\ncamera" },
    { XBHELP_A_BUTTON,     XBHELP_PLACEMENT_2, L"Toggle\nrefraction" },
    { XBHELP_B_BUTTON,     XBHELP_PLACEMENT_2, L"Toggle\nreflection" },
    { XBHELP_BACK_BUTTON,  XBHELP_PLACEMENT_2, L"Display\nhelp" },
};

#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts)/sizeof(g_HelpCallouts[0]))




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    CXBFont                 m_Font;               // Standard font
    CXBPackedResource       m_xprResource;        // Packed resources for the app
    CXBHelp                 m_Help;               // Help object
    BOOL                    m_bDrawHelp;          // Whether to draw help

    D3DXMATRIX              m_matWorld;           // Matrix transforms
    D3DXMATRIX              m_matView;        
    D3DXMATRIX              m_matProj;        

    CXBMesh                 m_SkyBox;             // A skybox of a lobby
    LPDIRECT3DCUBETEXTURE8  m_pEnvmap;

    CXBMesh                 m_Mesh;               // An object to render as glass
    D3DXMATRIX              m_matObjectTransform;

    LPDIRECT3DCUBETEXTURE8  m_pReflectionCubemap; // Cubemaps and shaders for glass effect
    LPDIRECT3DCUBETEXTURE8  m_pRefractionCubemap;
    DWORD                   m_dwGlassVertexShader;
    DWORD                   m_dwGlassPixelShader;

    BOOL                    m_bShowRefraction;    // Rendering options
    BOOL                    m_bShowReflection;

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

    // Init objects
    m_bDrawHelp       = FALSE;
    m_bShowRefraction = TRUE;
    m_bShowReflection = TRUE;

    D3DXMatrixIdentity( &m_matObjectTransform );
}




//-----------------------------------------------------------------------------
// Name: XBUtil_CreateRefractionCubeMap()
// Desc: Creates a cubemap and fills it with refracted RGBA vectors. Note that
//       the term "refraction" is used loosely here, as this is a gross
//       approximation. The real refraction equation is Snell's law, but that
//       would need to occur on the lightray as it goes into an object, and
//       out. We just try it on one surface-lightray-interaction and the result
//       looks convincing enough.
//-----------------------------------------------------------------------------
HRESULT XBUtil_CreateRefractionCubeMap( FLOAT fIndexOfRefraction, DWORD dwSize, 
                                        LPDIRECT3DCUBETEXTURE8* ppCubeMap )
{
    HRESULT hr;

    // Create the cube map.
    if( FAILED( hr = D3DDevice_CreateCubeTexture( dwSize, 1, 0, D3DFMT_A8R8G8B8, 
                                                    D3DPOOL_DEFAULT, ppCubeMap ) ) )
        return E_FAIL;
    
    // Allocate temp space for swizzling the cubemap surfaces
    DWORD* pSourceBits = new DWORD[ dwSize * dwSize ];

    // Fill all six sides of the cubemap
    for( DWORD i=0; i<6; i++ )
    {
        // Lock the i'th cubemap surface
        LPDIRECT3DSURFACE8 pCubeMapFace;
        (*ppCubeMap)->GetCubeMapSurface( (D3DCUBEMAP_FACES)i, 0, &pCubeMapFace );

        // Write the RGBA-encoded normals to the surface pixels
        DWORD*       pPixel = (DWORD*)pSourceBits;
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

                // Convert the normal to spherical coordinates
                D3DXVec3Normalize( &n, &n );
                FLOAT fTheta = atan2f( n.y, n.x ); // Returns range [-pi, +pi]
                FLOAT fPhi   = acosf( n.z );       // Returns range [0, pi]

                // Refract fPhi
                FLOAT fPhiRefracted = D3DX_PI + asinf( sinf(fPhi) / fIndexOfRefraction );

                // Convert the spherical coordinates back to a normal
                n.x = -sinf( fPhiRefracted ) * cosf( fTheta );
                n.y = -sinf( fPhiRefracted ) * sinf( fTheta );
                n.z = -cosf( fPhiRefracted );

                *pPixel++ = XBUtil_VectorToRGBA( &n );
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
    SAFE_DELETE_ARRAY( pSourceBits );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: XBUtil_CreateReflectionCubeMap()
// Desc: Creates a cubemap and fills it with normalized RGBA vectors
//-----------------------------------------------------------------------------
HRESULT XBUtil_CreateReflectionCubeMap( DWORD dwSize, 
                                        LPDIRECT3DCUBETEXTURE8* ppCubeMap )
{
    HRESULT hr;

    // Create the cube map.
    if( FAILED( hr = D3DDevice_CreateCubeTexture( dwSize, 1, 0, D3DFMT_A8R8G8B8, 
                                                  D3DPOOL_DEFAULT, ppCubeMap ) ) )
        return E_FAIL;
    
    // Allocate temp space for swizzling the cubemap surfaces
    DWORD* pSourceBits = new DWORD[ dwSize * dwSize ];

    // Fill all six sides of the cubemap
    for( DWORD i=0; i<6; i++ )
    {
        // Lock the i'th cubemap surface
        LPDIRECT3DSURFACE8 pCubeMapFace;
        (*ppCubeMap)->GetCubeMapSurface( (D3DCUBEMAP_FACES)i, 0, &pCubeMapFace );

        // Write the RGBA-encoded normals to the surface pixels
        DWORD*       pPixel = (DWORD*)pSourceBits;
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

                // Write out the reflected vector for the normal
                D3DXVec3Normalize( &n, &n );
                D3DXVECTOR3 vEye( 0, 0, -1 );
                D3DXVECTOR3 r = 2 * n * D3DXVec3Dot( &n, &vEye ) - vEye;
                *pPixel++ = XBUtil_VectorToRGBA( &r, 1.0f - fabsf(n.z) );
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
    SAFE_DELETE_ARRAY( pSourceBits );

    return S_OK;
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

    // Get access to the envmap texture
    m_pEnvmap = m_xprResource.GetCubemap( "Envmap" );

    // Create the refraction cubemap
    XBUtil_CreateRefractionCubeMap( 5.0f, 256, &m_pRefractionCubemap );

    // Create the reflection cubemap
    XBUtil_CreateReflectionCubeMap( 256, &m_pReflectionCubemap );

    // Load the skybox mesh
    if( FAILED( m_SkyBox.Create( "Models\\Lobby.xbg", &m_xprResource ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Load a mesh for a glass model
    if( FAILED( m_Mesh.Create( "Models\\Teapot.xbg" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Init the transforms
    D3DXVECTOR3 vEyePt    = D3DXVECTOR3( 0.0f, 0.0f,-7.0f );
    D3DXVECTOR3 vLookatPt = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );
    D3DXVECTOR3 vUpVec    = D3DXVECTOR3( 0.0f, 1.0f, 0.0f );
    D3DXMatrixIdentity( &m_matWorld );
    D3DXMatrixLookAtLH( &m_matView, &vEyePt, &vLookatPt, &vUpVec );
    D3DXMatrixPerspectiveFovLH( &m_matProj, D3DX_PI/4, 4.0f/3.0f, 1.0f, 500.0f );
    m_pd3dDevice->SetTransform( D3DTS_WORLD,      &m_matWorld );
    m_pd3dDevice->SetTransform( D3DTS_VIEW,       &m_matView );
    m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &m_matProj );

    // Create the glass vertex shader
    DWORD dwVertexDecl[MAX_FVF_DECL_SIZE];
    XBUtil_DeclaratorFromFVF( m_Mesh.GetMesh(0)->m_dwFVF, dwVertexDecl );

    if( FAILED( XBUtil_CreateVertexShader( "Shaders\\Glass.xvu", dwVertexDecl,
                                           &m_dwGlassVertexShader ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create the glass pixel shader
    if( FAILED( XBUtil_CreatePixelShader( "Shaders\\Glass.xpu", 
                                          &m_dwGlassPixelShader ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    // Toggle help and options
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
        m_bShowRefraction = !m_bShowRefraction;
    
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_B] )
        m_bShowReflection = !m_bShowReflection;
    
    // Adjust view with right thumbstick
    static FLOAT fTheta = 0.0f;
    static FLOAT fPhi   = 0.0f;
    fTheta += m_DefaultGamepad.fX2*m_fElapsedTime*D3DX_PI*0.5f;
    fPhi   += m_DefaultGamepad.fY2*m_fElapsedTime*D3DX_PI*0.5f;
    fPhi = min( 1.5f, max( -1.5f, fPhi ) );
    FLOAT x = 7.0f * cos( fTheta ) * cos( fPhi );
    FLOAT y = 7.0f * sin( fPhi );
    FLOAT z = 7.0f * sin( fTheta ) * cos( fPhi );
    D3DXVECTOR3 vEyePt    = D3DXVECTOR3( x, y, z );
    D3DXVECTOR3 vLookatPt = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );
    D3DXVECTOR3 vUpVec    = D3DXVECTOR3( 0.0f, 1.0f, 0.0f );
    D3DXMatrixLookAtLH( &m_matView, &vEyePt, &vLookatPt, &vUpVec );
    m_pd3dDevice->SetTransform( D3DTS_VIEW, &m_matView );

    // Perform object rotation with left thumbstick
    D3DXVECTOR3 vXVec, vYVec, vZVec;
    vXVec.x = m_matView._11; vXVec.y = m_matView._21; vXVec.z = m_matView._31; 
    vYVec.x = m_matView._12; vYVec.y = m_matView._22; vYVec.z = m_matView._32; 
    vZVec.x = m_matView._13; vZVec.y = m_matView._23; vZVec.z = m_matView._33; 

    D3DXMATRIX matRotate, matRotate1, matRotate2;
    FLOAT fXRotate1 = m_DefaultGamepad.fX1*m_fElapsedTime*D3DX_PI*0.5f;
    FLOAT fYRotate1 = m_DefaultGamepad.fY1*m_fElapsedTime*D3DX_PI*0.5f;
    D3DXMatrixRotationAxis( &matRotate1, &vYVec, -fXRotate1 );
    D3DXMatrixRotationAxis( &matRotate2, &vXVec, +fYRotate1 );
    D3DXMatrixMultiply( &matRotate, &matRotate1, &matRotate2 );
    D3DXMatrixMultiply( &m_matObjectTransform, &m_matObjectTransform, &matRotate );
    m_pd3dDevice->SetTransform( D3DTS_WORLD, &m_matObjectTransform );

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
    // Clear the scene
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL, 0x00000000, 1.0f, 0L );

    // Render the Skybox
    {
        // Set states
        m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE );
        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE ); 
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU,  D3DTADDRESS_CLAMP );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV,  D3DTADDRESS_CLAMP );

        D3DXMATRIX m_matIdentity;
        D3DXMatrixIdentity( &m_matIdentity );

        // Center view matrix, and set FOV to 90 degrees
        D3DXMATRIX matSkyBoxView, matProj;
        matSkyBoxView = m_matView; matSkyBoxView._41 = matSkyBoxView._42 = matSkyBoxView._43 = 0.0f;
        D3DXMatrixPerspectiveFovLH( &matProj, D3DX_PI/2, 1.0f, 0.5f, 10000.0f );
        m_pd3dDevice->SetTransform( D3DTS_WORLD,      &m_matIdentity );
        m_pd3dDevice->SetTransform( D3DTS_VIEW,       &matSkyBoxView );
        m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &matProj );

        // Render the skybox
        m_SkyBox.Render();

        // Restore the render states
        m_pd3dDevice->SetTransform( D3DTS_WORLD,      &m_matWorld );
        m_pd3dDevice->SetTransform( D3DTS_VIEW,       &m_matView );
        m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &m_matProj );
        m_pd3dDevice->SetRenderState( D3DRS_ZENABLE,  TRUE );
    }
    
    // Pass in matrix transforms to the glass vertex shader
    D3DXMATRIX matWorld = m_matObjectTransform;
    D3DXMATRIX matView  = m_matView;
    D3DXMATRIX matProj  = m_matProj;

    D3DXMATRIX matWV, matWVP;
    D3DXMatrixMultiply( &matWV, &matWorld, &matView );
    D3DXMatrixMultiply( &matWVP, &matWV, &matProj );
    D3DXMatrixTranspose( &matWVP, &matWVP );
    D3DDevice::SetVertexShaderConstant( 0, &matWVP, 4 );

    matWorld._41 = matWorld._42 = matWorld._43 = 0.0f;
    matView._41  = matView._42  = matView._43  = 0.0f;
    D3DXMatrixMultiply( &matWV, &matWorld, &matView );
    D3DXMatrixTranspose( &matWV, &matWV );
    D3DDevice::SetVertexShaderConstant( 4, &matWV, 4 );

    D3DXMATRIX matInvView;
    D3DXMatrixInverse( &matInvView, NULL, &matView );
    D3DXMatrixTranspose( &matInvView, &matInvView );
    m_pd3dDevice->SetVertexShaderConstant( 20, &matInvView, 4 );

    // Set common states for the two passes of rendering the object
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 2, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 2, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 3, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 3, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 3, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 3, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP );

    // Render the refraction pass (Note: some states are redundant for clarity)
    if( m_bShowRefraction )
    {
        m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE,  FALSE ); 
        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE ); 

        // Use the glass vertex shader
        m_pd3dDevice->SetVertexShader( m_dwGlassVertexShader );

        // Use the refraction pixel shader
        m_pd3dDevice->SetTexture( 0, m_pRefractionCubemap );
        m_pd3dDevice->SetTexture( 3, m_pEnvmap );
        m_pd3dDevice->SetPixelShader( m_dwGlassPixelShader );

        // Draw the geometry
        m_Mesh.Render( XBMESH_NOFVF | XBMESH_NOMATERIALS | XBMESH_NOTEXTURES );
    }

    // Render the reflection pass (Note: some states are redundant for clarity)
    if( m_bShowReflection )
    {
        m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE,  TRUE ); 
        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE ); 
        m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA ); 
        m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA ); 

        // Use the glass vertex shader
        m_pd3dDevice->SetVertexShader( m_dwGlassVertexShader );

        // Use the reflection pixel shader
        m_pd3dDevice->SetTexture( 0, m_pReflectionCubemap );
        m_pd3dDevice->SetTexture( 3, m_pEnvmap );
        m_pd3dDevice->SetPixelShader( m_dwGlassPixelShader );

        // Draw the geometry
        m_Mesh.Render( XBMESH_NOFVF | XBMESH_NOMATERIALS | XBMESH_NOTEXTURES );
    }

    // Restore state
    m_pd3dDevice->SetPixelShader( NULL );
    m_pd3dDevice->SetTexture( 0, NULL );
    m_pd3dDevice->SetTexture( 3, NULL );

    // Show title, frame rate, and help
    if( m_bDrawHelp )
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    else
    {
        m_Font.Begin();
        m_Font.DrawText(  64,  50, 0xffffffff, L"Glass" );
        m_Font.DrawText( 450,  50, 0xffffff00, m_strFrameRate );

        m_Font.DrawText(  64,  80, 0xffffffff, GLYPH_A_BUTTON L"Refraction:" );
        m_Font.DrawText( 204,  80, 0xffffff00, m_bShowRefraction ? L"On" : L"Off" );

        m_Font.DrawText(  64, 105, 0xffffffff, GLYPH_B_BUTTON L"Reflection:" );
        m_Font.DrawText( 204, 105, 0xffffff00, m_bShowReflection ? L"On" : L"Off" );

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}




