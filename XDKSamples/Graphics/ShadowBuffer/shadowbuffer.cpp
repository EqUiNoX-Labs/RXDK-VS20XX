//-----------------------------------------------------------------------------
// File: ShadowBuffer.cpp
//
// Desc: Illustrates how to do shadow buffering on the Xbox.
//
// Hist: 03.28.00 - Added toggle for drawing backfaces instead of z offset.
//       06.06.01 - Adding start/stop capability
//       07.22.02 - Ported to XBMesh models
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <XBMesh.h>


//-----------------------------------------------------------------------------
// Shadowbuffer types
//-----------------------------------------------------------------------------
enum
{
    SHADOWBUFFERTYPE_D16 = 0,
    SHADOWBUFFERTYPE_D24S8,
    SHADOWBUFFERTYPE_F16,
    SHADOWBUFFERTYPE_F24S8,
};

// Z ranges for all buffer types
FLOAT g_fShadowBufferZRange[4] = { D3DZ_MAX_D16, D3DZ_MAX_D24S8, 
                                   D3DZ_MAX_F16, (FLOAT)D3DZ_MAX_F24S8 };

// Descriptions
WCHAR* g_strShadowBufferDesc[4] = { L"D16", L"D24S8",
                                    L"F16", L"F24S8" };

// Formats
D3DFORMAT g_ShadowBufferFormat[4] = { D3DFMT_LIN_D16, D3DFMT_LIN_D24S8,
                                      D3DFMT_LIN_F16, D3DFMT_LIN_F24S8 };

// Shadow buffer width and height.
const int SHADOWBUFFERWIDTH  = 512;
const int SHADOWBUFFERHEIGHT = 512;




//-----------------------------------------------------------------------------
// Projection frustum
//-----------------------------------------------------------------------------
struct LINEVERTEX
{
    FLOAT x, y, z;
    DWORD color;
};


D3DXVECTOR4 g_vHomogenousFrustum[8] =
{
    D3DXVECTOR4( 1.0f, 1.0f, 0.0f, 1.0f ),
    D3DXVECTOR4( 1.0f, 1.0f, 1.0f, 1.0f ),

    D3DXVECTOR4(-1.0f, 1.0f, 0.0f, 1.0f ),
    D3DXVECTOR4(-1.0f, 1.0f, 1.0f, 1.0f ),

    D3DXVECTOR4(-1.0f,-1.0f, 0.0f, 1.0f ),
    D3DXVECTOR4(-1.0f,-1.0f, 1.0f, 1.0f ),

    D3DXVECTOR4( 1.0f,-1.0f, 0.0f, 1.0f ),
    D3DXVECTOR4( 1.0f,-1.0f, 1.0f, 1.0f ),
};


LINEVERTEX g_vFrustumLines[8] =
{
    { 1.0f, 1.0f, 0.0f, 0xffffffff },
    { 1.0f, 1.0f, 1.0f, 0xffffffff },

    {-1.0f, 1.0f, 0.0f, 0xffffffff },
    {-1.0f, 1.0f, 1.0f, 0xffffffff },

    {-1.0f,-1.0f, 0.0f, 0xffffffff },
    {-1.0f,-1.0f, 1.0f, 0xffffffff },

    { 1.0f,-1.0f, 0.0f, 0xffffffff },
    { 1.0f,-1.0f, 1.0f, 0xffffffff },
};




//-----------------------------------------------------------------------------
// Help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_LEFTSTICK,    XBHELP_PLACEMENT_1, L"Move light" },
    { XBHELP_RIGHTSTICK,   XBHELP_PLACEMENT_1, L"Move camera" },
    { XBHELP_DPAD,         XBHELP_PLACEMENT_2, L"Adjust\nZ offset/slope" },
    { XBHELP_A_BUTTON,     XBHELP_PLACEMENT_2, L"Change\nZ-buffer format" },
    { XBHELP_B_BUTTON,     XBHELP_PLACEMENT_1, L"Toggle\nZ offset/slope" },
    { XBHELP_X_BUTTON,     XBHELP_PLACEMENT_2, L"Toggle\nfrustum" },
    { XBHELP_LEFT_BUTTON,  XBHELP_PLACEMENT_1, L"Zoom Out" },
    { XBHELP_RIGHT_BUTTON, XBHELP_PLACEMENT_1, L"Zoom In" },
    { XBHELP_START_BUTTON, XBHELP_PLACEMENT_1, L"Pause" },
    { XBHELP_BACK_BUTTON,  XBHELP_PLACEMENT_1, L"Display help" },
};

#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts)/sizeof(g_HelpCallouts[0]))




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    CXBFont            m_Font;
    CXBPackedResource  m_xprResource;
    CXBHelp            m_Help;
    BOOL               m_bDrawHelp;

    BOOL               m_bDrawFrustum;         // Option to draw frustum
    BOOL               m_bUseOffsetSlope;      // Option to use offst/slope or just backface.

    D3DXMATRIX         m_matProj;
    D3DXMATRIX         m_matShadowProj;
    D3DXVECTOR3        m_vCameraPos;
    D3DXVECTOR3        m_vCameraRot;
    D3DXMATRIX         m_matView;

    CXBMesh            m_FloorMesh;
    D3DXMATRIX         m_matFloor;

    CXBMesh            m_ObjectMesh;
    D3DXVECTOR3        m_vObjectPosition;
    D3DXVECTOR3        m_vObjectRotation;
    D3DXMATRIX         m_matObject;
    FLOAT              m_fObjectRadius;

    CXBMesh            m_LightMesh;
    D3DXVECTOR3        m_vLightPosition;
    D3DXMATRIX         m_matLight;

    D3DXMATRIX         m_matTexture;                    // Texture projection matrix

    D3DTexture*        m_pShadowBufferDepthTexture;    // Shadow buffer depth texture
    D3DSurface*        m_pShadowBufferDepthSurface;
    D3DSurface         m_DummyRenderTargetSurface;

    D3DSurface*        m_pRenderTarget;
    D3DSurface*        m_pZBuffer;              // Back buffer depth surface

    D3DSurface         m_FakeTarget;

    DWORD              m_dwShadowBufVS;                // Shadow buffer vertex shader
    DWORD              m_dwShadowBufPS;                // Shadow buffer pixel shader

    DWORD              m_dwShadowBufferType;           // Shadowbuffer type

    FLOAT              m_fZOffset;                     // Shadowbuffer z offset
    FLOAT              m_fZSlopeScale;                 // Shadowbuffer z slope scale

    HRESULT CreateShadowBuffer();
    HRESULT InitPixelShader();

public:
    HRESULT Initialize();
    HRESULT FrameMove();
    HRESULT Render();

    HRESULT DrawShadowBufferedObject( CXBMesh* pMesh, const D3DXMATRIX& matObject );

    CXBoxSample();
};




//-----------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program. Initializes everything, and goes into a
//       message-processing loop. Idle time is used to render the scene.
//-----------------------------------------------------------------------------
void __cdecl main()
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

    m_bDrawHelp       = FALSE;
    m_bDrawFrustum    = TRUE;
    m_bUseOffsetSlope = FALSE;

    m_fZOffset        = 4.0f;
    m_fZSlopeScale    = 2.0f;

    m_vCameraPos      = D3DXVECTOR3( 0.0f, 20.0f, -30.0f );
    m_vCameraRot      = D3DXVECTOR3( 0.0f,  0.0f,   0.0f );

    m_dwShadowBufferType        = SHADOWBUFFERTYPE_D16;
    m_pShadowBufferDepthTexture = NULL;
    m_pShadowBufferDepthSurface = NULL;
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize device-dependant objects.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    // Create a font
    if( FAILED( m_Font.Create( "Font.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create the help
    if( FAILED( m_Help.Create( "Gamepad.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create our vertex shader
    DWORD dwDecl[] =
    {
        D3DVSD_STREAM(0),
        D3DVSD_REG(0, D3DVSDT_FLOAT3), // v0 = XYZ
        D3DVSD_REG(1, D3DVSDT_FLOAT3), // v1 = normals
        D3DVSD_REG(2, D3DVSDT_FLOAT2), // v2 = TEX1
        D3DVSD_END()
    };

    if( FAILED( XBUtil_CreateVertexShader( "Shaders\\VShader.xvu", dwDecl, &m_dwShadowBufVS ) ) )
        return E_FAIL;

    if( FAILED( XBUtil_CreatePixelShader( "Shaders\\ShadwBuf.xpu", &m_dwShadowBufPS ) ) )
        return E_FAIL;

    // Set projection transform
    D3DXMatrixPerspectiveFovLH( &m_matProj, D3DX_PI/4, 640.0f/480.0f, 1.0f, 1000.0f );
    m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &m_matProj );

    // Create the resources
    if( FAILED( m_xprResource.Create( "Resource.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Floor
    m_FloorMesh.Create( "Models\\Floor.xbg", &m_xprResource );
    D3DXMatrixIdentity( &m_matFloor );

    // Object
    m_ObjectMesh.Create( "Models\\Airplane.xbg", &m_xprResource );
    m_fObjectRadius   = m_ObjectMesh.ComputeRadius();
    m_vObjectPosition = D3DXVECTOR3( 0.0f, 4.0f, 0.0f );
    m_vObjectRotation = D3DXVECTOR3( -1.5708f, 0.0f, 0.0f );

    // Light
    m_LightMesh.Create( "Models\\Light.xbg", &m_xprResource );
    m_vLightPosition = D3DXVECTOR3( 5.0f, 10.0f, 0.0f );

    // Create shadow buffer
    CreateShadowBuffer();

    // Setup dummy color buffer (bad things will happen if you write to it).
    XGSetSurfaceHeader( SHADOWBUFFERWIDTH, SHADOWBUFFERHEIGHT, D3DFMT_LIN_R5G6B5,
                        &m_DummyRenderTargetSurface, 0, 0 );

    // Get original color and z-buffer.
    m_pd3dDevice->GetDepthStencilSurface( &m_pZBuffer );
    m_pd3dDevice->GetRenderTarget( &m_pRenderTarget );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: CreateShadowBuffer()
// Desc: Creates the shadow buffer's texture and sets a tile for it
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::CreateShadowBuffer()
{
    // Release existing objects
    m_pd3dDevice->SetTile( 2, NULL );
    if( m_pShadowBufferDepthTexture )
        m_pShadowBufferDepthTexture->Release();
    if( m_pShadowBufferDepthSurface )
        m_pShadowBufferDepthSurface->Release();

    // Create the shadow buffer texture
    m_pd3dDevice->CreateTexture( SHADOWBUFFERWIDTH, SHADOWBUFFERHEIGHT, 1, 0, 
                                 g_ShadowBufferFormat[m_dwShadowBufferType], 0, 
                                 &m_pShadowBufferDepthTexture );

    // Get a surface ptr to the texture (so we can set it as a depth buffer)
    m_pShadowBufferDepthTexture->GetSurfaceLevel( 0, &m_pShadowBufferDepthSurface );

    // Setup a tile for faster rendering
    static D3DTILE tile = {0};
    tile.Flags   = D3DTILE_FLAGS_ZBUFFER | ( XGBytesPerPixelFromFormat(g_ShadowBufferFormat[m_dwShadowBufferType]) == 4 ? D3DTILE_FLAGS_Z32BITS : D3DTILE_FLAGS_Z16BITS );
    tile.Pitch   = SHADOWBUFFERWIDTH * XGBytesPerPixelFromFormat(g_ShadowBufferFormat[m_dwShadowBufferType]);
    tile.Size    = SHADOWBUFFERWIDTH * SHADOWBUFFERHEIGHT * XGBytesPerPixelFromFormat(g_ShadowBufferFormat[m_dwShadowBufferType]);
    tile.pMemory = (VOID*)(0x80000000 | m_pShadowBufferDepthSurface->Data);
    m_pd3dDevice->SetTile( 2, &tile );

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

    // Toggle frustum
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_X] )
        m_bDrawFrustum = !m_bDrawFrustum;

    // Toggle using z-offset or drawing just back faces.
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_B] )
        m_bUseOffsetSlope = !m_bUseOffsetSlope;

    // Rotate the object
    m_vObjectRotation.x = 0.0f;
    m_vObjectRotation.y += 1.57f*m_fElapsedAppTime;

    // Check for buffer change
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
    {
        // Advance shadow buffer type
        switch( m_dwShadowBufferType )
        {
            case SHADOWBUFFERTYPE_D16:
                m_dwShadowBufferType = SHADOWBUFFERTYPE_D24S8;
                break;
            case SHADOWBUFFERTYPE_D24S8:
                m_dwShadowBufferType = SHADOWBUFFERTYPE_F16;
                break;
            case SHADOWBUFFERTYPE_F16:
                m_dwShadowBufferType = SHADOWBUFFERTYPE_F24S8;
                break;
            case SHADOWBUFFERTYPE_F24S8:
                m_dwShadowBufferType = SHADOWBUFFERTYPE_D16;
                break;
        }

        // Re-create the shadow buffer
        CreateShadowBuffer();
    }

    // Adjust z offset
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
        m_fZOffset += 0.5f;
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
        m_fZOffset -= 0.5f;

    // Adjust z offset slope scale
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
        m_fZSlopeScale += 0.1f;
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
        m_fZSlopeScale -= 0.1f;

    // Adjust light position
    m_vLightPosition.x += m_DefaultGamepad.fX1*m_fElapsedTime*8.0f;
    m_vLightPosition.z += m_DefaultGamepad.fY1*m_fElapsedTime*8.0f;

    // Adjust camera position
    static D3DXVECTOR3 vAt( 0.0f, 4.0f, 0.0f );
    static D3DXVECTOR3 vUp( 0.0f, 1.0f, 0.0f );

    // Rotate camera around z axis.
    D3DXMATRIX matRotate;
    D3DXMatrixRotationAxis( &matRotate, &vUp, m_DefaultGamepad.fX2*m_fElapsedTime );
    D3DXVec3TransformCoord( &m_vCameraPos, &m_vCameraPos, &matRotate );

    // Rotate camera points around side axis.
    D3DXVECTOR3 vView = (m_vCameraPos - vAt);
    D3DXVec3Normalize( &vView, &vView );

    // Place limits so we don't go over the top or under the bottom.
    FLOAT dot = D3DXVec3Dot( &vView, &vUp );
    if( (dot > -0.0f || m_DefaultGamepad.fY2 < 0.0f) && (dot < 0.9f || m_DefaultGamepad.fY2 > 0.0f) )
    {
        D3DXVECTOR3 vAxis;
        D3DXVec3Cross( &vAxis, &vUp, &vView );
        D3DXMatrixRotationAxis( &matRotate, &vAxis, m_DefaultGamepad.fY2*m_fElapsedTime );
        D3DXVec3TransformCoord( &m_vCameraPos, &m_vCameraPos, &matRotate );
    }

    // In/out based on triggers.
    FLOAT fZoomIn  = ( m_DefaultGamepad.bAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER] / 255.0f );
    FLOAT fZoomOut = ( m_DefaultGamepad.bAnalogButtons[XINPUT_GAMEPAD_LEFT_TRIGGER]  / 255.0f );

    if( fZoomIn > 0.1f && D3DXVec3Length(&m_vCameraPos) > 10.0f )
        m_vCameraPos -= vView * 30.0f * fZoomIn * m_fElapsedTime;

    if( fZoomOut > 0.1f )
        m_vCameraPos += vView * 30.0f * fZoomOut * m_fElapsedTime;

    D3DXMatrixLookAtLH( &m_matView, &m_vCameraPos, &vAt, &vUp );

    // Light orientation (looks at object)
    D3DXMatrixLookAtLH( &m_matLight, &m_vLightPosition, &m_vObjectPosition, &vUp );
    D3DXMatrixInverse( &m_matLight, NULL, &m_matLight );

    // Generate the texture transform matrix
    //
    // Note: if you are using multiple lights & shadow buffers, this needs
    // to be calculated for each light.
    //
    // We are starting with worldspace coordinates, so we need to
    // transform from worldspace to lightspace.
    //
    // We go from 3D worldspace to 3D lightspace by multiplying by the
    // inverse of the light matrix.
    // You will probably notice that we are doing two inverses in a row (see previous
    // line of code). I did this to clearly illustrate the steps in generating the
    // texture matrix. The previous line would not always be available.
    D3DXMatrixInverse( &m_matTexture, NULL, &m_matLight );

    // Find a projection that will fit all the objects we want to shadow into the
    // view frustum of the light.
    D3DXVECTOR3 vDistance( m_vObjectPosition - m_vLightPosition );
    FLOAT fDist   = D3DXVec3Length( &vDistance );
    FLOAT fRadius = m_fObjectRadius;
    FLOAT fNear   = fDist - fRadius;
    FLOAT fFar    = fNear + 100.0f;
    FLOAT fAngle  = 2.0f * asinf(fRadius / fDist);
    
    D3DXMatrixPerspectiveFovLH( &m_matShadowProj, fAngle, 1.0f, fNear, fFar );

    // Combine the light orientation matrix with the shadowbuffer projection 
    // matrix.  This projects our light space position onto the shadowbuffer 
    // the same way a projection matrix projects a cameraspace coordinate onto 
    // the screen.
    D3DXMatrixMultiply( &m_matTexture, &m_matTexture, &m_matShadowProj );

    // Finally, we scale and offset by SHADOWBUFFERWIDTH/2, SHADOWBUFFERHEIGHT/2
    // to move from [-1,+1] space to [0:0, SHADOWBUFFERWIDTH:SHADOWBUFFERHEIGHT]
    // texture space we also need to scale z by the zbuffer range.  An additional
    // half texel offset is necessary because of the differences between texture
    // addressing and pixel addressing.
    D3DXMATRIX  mat;
    D3DXMatrixIdentity( &mat );

    // Scale
    mat._11 = +SHADOWBUFFERWIDTH  * 0.5f;
    mat._22 = -SHADOWBUFFERHEIGHT * 0.5f;
    mat._33 = g_fShadowBufferZRange[m_dwShadowBufferType];

    // Offset
    mat._41 = SHADOWBUFFERWIDTH  * 0.5f + 0.5f;
    mat._42 = SHADOWBUFFERHEIGHT * 0.5f + 0.5f;

    D3DXMatrixMultiply( &m_matTexture, &m_matTexture, &mat );

    // m_TextureMat now holds the appropriate transformation matrix
    // for shadowmapping on the Xbox GPU.

    // Calculate the frustum lines
    D3DXMATRIX matInvTexProj;
    D3DXMatrixInverse( &matInvTexProj, NULL, &m_matShadowProj );

    for( int i = 0; i < 8; i++ )
    {
        D3DXVECTOR4 vT = g_vHomogenousFrustum[i];

        D3DXVec4Transform( &vT, &vT, &matInvTexProj );
        D3DXVec4Transform( &vT, &vT, &m_matLight );

        g_vFrustumLines[i].x = vT.x / vT.w;
        g_vFrustumLines[i].y = vT.y / vT.w;
        g_vFrustumLines[i].z = vT.z / vT.w;
    }

    // Compute the object matrix
    D3DXMATRIX matTrans;
    D3DXMatrixTranslation( &matTrans, m_vObjectPosition.x, m_vObjectPosition.y, m_vObjectPosition.z );
    D3DXMatrixRotationYawPitchRoll( &matRotate, m_vObjectRotation.y, m_vObjectRotation.x, m_vObjectRotation.z );
    D3DXMatrixMultiply( &m_matObject, &matRotate, &matTrans );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: DrawShadowBufferedObject()
// Desc: Displays a shadowbuffered object. 
//       The two matrices set up here are the World*View*Projection matrix
//       that transforms the objects points on to the screen, and the
//       World*Texture matrix that transforms the objects points into
//       shadowbuffer space.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::DrawShadowBufferedObject( CXBMesh* pMesh, const D3DXMATRIX& matObject )
{
    D3DXMATRIX mat, matWVP, matWT;
    D3DXMATRIX matTrans, matRotate;

    // WVP matrix
    D3DXMatrixMultiply( &mat, &matObject, &m_matView );
    D3DXMatrixMultiply( &matWVP, &mat, &m_matProj );
    D3DXMatrixTranspose( &matWVP, &matWVP );
    m_pd3dDevice->SetVertexShaderConstant( 0, &matWVP, 4 );

    // WT matrix
    D3DXMatrixMultiply( &matWT, &matObject, &m_matTexture );
    D3DXMatrixTranspose( &matWT, &matWT);
    m_pd3dDevice->SetVertexShaderConstant( 4, &matWT, 4 );

    // Light position
    D3DXVECTOR4 v4LocalLightPos;
    D3DXMatrixInverse( &mat, NULL, &matObject );
    D3DXVec3Transform( &v4LocalLightPos, &m_vLightPosition, &mat );
    m_pd3dDevice->SetVertexShaderConstant( 8, &v4LocalLightPos, 1 );

    // Ambient color
    FLOAT fAmbient[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
    m_pd3dDevice->SetPixelShaderConstant( 0, fAmbient, 1 );

    m_pd3dDevice->SetVertexShader( m_dwShadowBufVS );
    pMesh->Render( XBMESH_NOFVF );

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
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU,  D3DTADDRESS_WRAP );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV,  D3DTADDRESS_WRAP );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MIPFILTER, D3DTEXF_NONE );

    // First, render the scene into the shadow buffer from the viewpoint of
    // the light
    {
        static D3DVIEWPORT8 viewport = { 0, 0, SHADOWBUFFERWIDTH, SHADOWBUFFERHEIGHT, 0.0f, 1.0f };
        m_pd3dDevice->SetRenderTarget( &m_DummyRenderTargetSurface, m_pShadowBufferDepthSurface );
        m_pd3dDevice->SetViewport( &viewport );
        m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_ZBUFFER, 0x00000000, 1.0f, 0L ); 

        // Disable color writes. This is very important since we are using a dummy
        // render target
        m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE, 0 );

        // Turn on 4x multisample, which increase fill performance for z-only rendering
        // m_pd3dDevice->SetRenderState( D3DRS_MULTISAMPLEMODE, D3DMULTISAMPLEMODE_4X );

        // Set matrices to render from light's point-of-view
        D3DXMATRIX matView;
        D3DXMatrixInverse( &matView, NULL, &m_matLight );
        m_pd3dDevice->SetTransform( D3DTS_VIEW,       &matView );
        m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &m_matShadowProj );
        
        if( m_bUseOffsetSlope )
        {
            // Turn on z-offset.
            m_pd3dDevice->SetRenderState( D3DRS_SOLIDOFFSETENABLE,        TRUE );
            m_pd3dDevice->SetRenderState( D3DRS_POLYGONOFFSETZOFFSET,     FtoDW(m_fZOffset) );
            m_pd3dDevice->SetRenderState( D3DRS_POLYGONOFFSETZSLOPESCALE, FtoDW(m_fZSlopeScale) );

            // Disable culling so all triangles cause shadows
            m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
        }
        else
        {
            // Only backfaces cast shadows and rely on frontface/backface seperation
            // to  eliminate any artifacts.
            m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CW );
        }

        // Render our scene into the shadowbuffer
        m_pd3dDevice->SetTransform( D3DTS_WORLD, &m_matFloor );
        m_FloorMesh.Render();

        m_pd3dDevice->SetTransform( D3DTS_WORLD, &m_matObject );
        m_ObjectMesh.Render();

        // Restore important state
        m_pd3dDevice->SetRenderTarget( m_pRenderTarget, m_pZBuffer );
        // m_pd3dDevice->SetRenderState( D3DRS_MULTISAMPLEMODE, D3DMULTISAMPLEMODE_1X );
        m_pd3dDevice->SetRenderState( D3DRS_COLORWRITEENABLE,  D3DCOLORWRITEENABLE_ALL );
        m_pd3dDevice->SetRenderState( D3DRS_SOLIDOFFSETENABLE, FALSE );
        m_pd3dDevice->SetRenderState( D3DRS_CULLMODE,          D3DCULL_CCW );
    }

    // Now render the scene from the point of view of the camera with shadow
    // compare functionality enabled
    {
        // Clear the main view
        m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,
                            0xff400000, 1.0f, 0L );
        m_pd3dDevice->SetTransform( D3DTS_VIEW,       &m_matView );
        m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &m_matProj );

        // Enable the shadow comparison function
        m_pd3dDevice->SetRenderState( D3DRS_SHADOWFUNC, D3DCMP_GREATEREQUAL );

        // Set shadowbuffer texture
        m_pd3dDevice->SetTexture( 1, m_pShadowBufferDepthTexture );
        m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ADDRESSU, D3DTADDRESS_BORDER );
        m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ADDRESSV, D3DTADDRESS_BORDER );
        m_pd3dDevice->SetTextureStageState( 1, D3DTSS_BORDERCOLOR, 0xffffffff );
        m_pd3dDevice->SetTextureStageState( 1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetTextureStageState( 1, D3DTSS_MINFILTER, D3DTEXF_LINEAR );

        // Set the shadowbuffer pixel shader
        m_pd3dDevice->SetPixelShader( m_dwShadowBufPS );

        // Render the objects in the scene 
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );

        // Display the floor (in shadow)
        DrawShadowBufferedObject( &m_FloorMesh, m_matFloor );

        // Display the object (in shadow)
        DrawShadowBufferedObject( &m_ObjectMesh, m_matObject );

        // Reset shadowbuffer state
        m_pd3dDevice->SetPixelShader( NULL );
        m_pd3dDevice->SetTexture( 1, NULL );
        m_pd3dDevice->SetRenderState( D3DRS_SHADOWFUNC, D3DCMP_NEVER );
    }

    // Finally draw all objects not in shadow
    {
        // Draw the light object (not in shadow)
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE );
        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
        m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
        m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_ONE );
        m_pd3dDevice->SetRenderState( D3DRS_LIGHTING, FALSE );
        m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
        m_pd3dDevice->SetTransform( D3DTS_WORLD, &m_matLight );
        m_LightMesh.Render();
        m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_DISABLE );

        // Draw light frustum (not in shadow)
        if( m_bDrawFrustum )
        {
            m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 );
            m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE );
            m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
            m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );

            D3DXMATRIX matIdentity;
            D3DXMatrixIdentity( &matIdentity );
            m_pd3dDevice->SetTransform( D3DTS_WORLD, &matIdentity );
            m_pd3dDevice->SetVertexShader( D3DFVF_XYZ|D3DFVF_DIFFUSE );
            m_pd3dDevice->DrawPrimitiveUP( D3DPT_LINELIST, 4, g_vFrustumLines, 
                                        sizeof(g_vFrustumLines[0]) );
            m_pd3dDevice->DrawVerticesUP( D3DPT_LINELOOP, 4, g_vFrustumLines, 
                                        sizeof(g_vFrustumLines[0])*2 );
            m_pd3dDevice->DrawVerticesUP( D3DPT_LINELOOP, 4, g_vFrustumLines+1, 
                                        sizeof(g_vFrustumLines[0])*2 );
        }
    }

    // Show title, frame rate, and help
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 48, 36, 0xffffffff,  L"ShadowBuffer" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 592, 38, 0xffffff00, m_strFrameRate, XBFONT_RIGHT );

        // Show buffer description
        m_Font.DrawText( 64, 75, 0xffffffff, L"Type: " );
        m_Font.DrawText( 0xffffff00, g_strShadowBufferDesc[m_dwShadowBufferType] );

        if( m_bUseOffsetSlope )
        {
            WCHAR strBuffer1[20];
            WCHAR strBuffer2[20];
            swprintf( strBuffer1, L"%.01f", m_fZOffset );
            swprintf( strBuffer2, L"%.01f", m_fZSlopeScale );
            m_Font.DrawText( 64, 100, 0xffffffff, L"ZOffset: " );
            m_Font.DrawText( 0xffffff00, strBuffer1 );
            m_Font.DrawText( 0xffffffff, L", ZOffset Slope Scale: " );
            m_Font.DrawText( 0xffffff00, strBuffer2 );
        }
        else
        {
            m_Font.DrawText( 64, 100, 0xffffffff, L"ZOffset: " );
            m_Font.DrawText( 0xffffff00, L"(not used)" );
            m_Font.DrawText( 0xffffffff, L", ZOffset Slope Scale: " );
            m_Font.DrawText( 0xffffff00, L"(not used)" );
        }

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}




