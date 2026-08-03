//-----------------------------------------------------------------------------
// File: FastVSConstants.cpp
//
// Desc: The FastVSConstants sample shows how to use the 
//       IDirect3DDevice8::BeginState and  Direct3DDevice8::EndState APIs to 
//       efficiently store calculated vertex shader constant data directly 
//       into the push-buffer.  The sample also illustrates the importance 
//       of pre-fetching data in order to achieve maximum CPU performance.
//
// Hist: 11.12.02 - New for December XDK release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <XBMesh.h>
#include <XBResource.h>
#include <XBUtil.h>
#include <XGMath.h>
#include "Resource.h"  // Bundled resources




//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_A_BUTTON,     XBHELP_PLACEMENT_2, L"Draw\nmethod" },
    { XBHELP_B_BUTTON,     XBHELP_PLACEMENT_2, L"Camera\nrotation" },
    { XBHELP_X_BUTTON,     XBHELP_PLACEMENT_2, L"Object\nrotation" },
    { XBHELP_BACK_BUTTON,  XBHELP_PLACEMENT_2, L"Display\nhelp" },
};

#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]))




//-----------------------------------------------------------------------------
// Number of objects drawn per frame.
//-----------------------------------------------------------------------------
#define NUM_OBJECTS 20000




//-----------------------------------------------------------------------------
// Simple vertex used by the objects.
//-----------------------------------------------------------------------------
struct OBJECTVERTEX
{
    XGVECTOR3 vPosition;
    XGVECTOR3 vNormal;
    XGVECTOR2 vTexCoords;
};




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Application class. The base class provides just about all the
//       functionality we want, so we're just supplying stubs to interface with
//       the non-C++ functions of the app.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    CXBPackedResource m_xprResource;        // Packed resources for the app
    CXBFont           m_Font;               // Font class
    CXBHelp           m_Help;               // Help class
    BOOL              m_bDrawHelp;          // Whether to draw help

    XGVECTOR3         m_vLightDir;          // Light vector

    XGVECTOR3         m_vEye;               // Eye position

    XGMATRIX          m_matView;            // Global transforms
    XGMATRIX          m_matProj;

    XGVECTOR3*        m_vObjectPos;         // Array of object positions
    XGQUATERNION*     m_qObjectQuat;        // Array of object rotations
    XGMATRIX*         m_matObject;          // Array of object transforms
    XGVECTOR4*        m_vObjectLight;       // Array of object light vectors

    D3DVertexBuffer*  m_pVB;                // Geoemetry for objects
    D3DPRIMITIVETYPE  m_PrimitiveType;
    DWORD             m_dwNumPrimitives;
    DWORD             m_dwNumVertices;
    D3DTexture*       m_pTexture;

    DWORD             m_dwSimpleVertexShader; // A simple vertex shader

    BOOL              m_bFast;              // Options
    BOOL              m_bRotateCamera;
    BOOL              m_bRotateObjects;

protected:
    HRESULT Initialize();
    HRESULT Render();
    HRESULT FrameMove();

    VOID RenderObjects();

    VOID MakeObject();

public:
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

    m_bDrawHelp      = false;

    m_bFast          = true;
    m_bRotateCamera  = true;
    m_bRotateObjects = false;
}




//-----------------------------------------------------------------------------
// Name: MakeObject()
// Desc: Create a vertex buffer containing the triangles for a tetrahedron.
//-----------------------------------------------------------------------------
VOID CXBoxSample::MakeObject()
{
    // Make a tetrahedron.
    m_PrimitiveType   = D3DPT_TRIANGLELIST;
    m_dwNumPrimitives = 4;
    m_dwNumVertices   = 3 * 4;
    
    DWORD dwVertexBufferSize = sizeof(OBJECTVERTEX) * m_dwNumVertices;
    m_pd3dDevice->CreateVertexBuffer( dwVertexBufferSize, 
                                      D3DUSAGE_WRITEONLY, 
                                      0, 
                                      0, 
                                      &m_pVB );

    // Fill vertex buffer
    OBJECTVERTEX* pVerts = NULL;
    m_pVB->Lock( 0, 0, (BYTE**)&pVerts, 0 );

    XGVECTOR3 Up(     0.0f,    1.0f,    0.0f   );
    XGVECTOR3 One(    0.945f, -0.332f,  0.0f   );
    XGVECTOR3 Two(   -0.471f, -0.332f,  0.816f );
    XGVECTOR3 Three( -0.471f, -0.332f, -0.816f );
    XGVECTOR3 vNormal;

    // Bottom
    pVerts[0].vPosition  = XGVECTOR3( One.x, One.y, One.z );
    pVerts[1].vPosition  = XGVECTOR3( Two.x, Two.y, Two.z );
    pVerts[2].vPosition  = XGVECTOR3( Three.x, Three.y, Three.z );
    
    pVerts[0].vNormal    = XGVECTOR3( 0.0f, -1.0f, 0.0f );
    pVerts[1].vNormal    = XGVECTOR3( 0.0f, -1.0f, 0.0f );
    pVerts[2].vNormal    = XGVECTOR3( 0.0f, -1.0f, 0.0f );
    
    pVerts[0].vTexCoords = XGVECTOR2( 0.5f, 0.0f );
    pVerts[1].vTexCoords = XGVECTOR2( 0.0f, 1.0f );
    pVerts[2].vTexCoords = XGVECTOR2( 1.0f, 1.0f );

    // Side 1
    pVerts[3].vPosition  = XGVECTOR3( Up.x, Up.y, Up.z );
    pVerts[4].vPosition  = XGVECTOR3( Two.x, Two.y, Two.z );
    pVerts[5].vPosition  = XGVECTOR3( One.x, One.y, One.z );

    const D3DXVECTOR3 v4v3( pVerts[4].vPosition - pVerts[3].vPosition );
    const D3DXVECTOR3 v5v3( pVerts[5].vPosition - pVerts[3].vPosition );
    XGVec3Cross( &vNormal, &v4v3, &v5v3 );
    XGVec3Normalize( &vNormal, &vNormal );
    pVerts[3].vNormal    = vNormal;
    pVerts[4].vNormal    = vNormal;
    pVerts[5].vNormal    = vNormal;
    
    pVerts[3].vTexCoords = XGVECTOR2( 0.5f, 0.0f );
    pVerts[4].vTexCoords = XGVECTOR2( 0.0f, 1.0f );
    pVerts[5].vTexCoords = XGVECTOR2( 1.0f, 1.0f );

    // Side 2
    pVerts[6].vPosition  = XGVECTOR3( Up.x, Up.y, Up.z );
    pVerts[7].vPosition  = XGVECTOR3( Three.x, Three.y, Three.z );
    pVerts[8].vPosition  = XGVECTOR3( Two.x, Two.y, Two.z );

    const D3DXVECTOR3 v7v6( pVerts[7].vPosition - pVerts[6].vPosition );
    const D3DXVECTOR3 v8v6( pVerts[8].vPosition - pVerts[6].vPosition );
    XGVec3Cross( &vNormal, &v7v6, &v8v6 );
    XGVec3Normalize( &vNormal, &vNormal );
    pVerts[6].vNormal    = vNormal;
    pVerts[7].vNormal    = vNormal;
    pVerts[8].vNormal    = vNormal;

    pVerts[6].vTexCoords = XGVECTOR2( 0.5f, 0.0f );
    pVerts[7].vTexCoords = XGVECTOR2( 0.0f, 1.0f );
    pVerts[8].vTexCoords = XGVECTOR2( 1.0f, 1.0f );

    // Side 3
    pVerts[ 9].vPosition  = XGVECTOR3( Up.x, Up.y, Up.z );
    pVerts[10].vPosition  = XGVECTOR3( One.x, One.y, One.z );
    pVerts[11].vPosition  = XGVECTOR3( Three.x, Three.y, Three.z );

    const D3DXVECTOR3 v10v9( pVerts[10].vPosition - pVerts[9].vPosition );
    const D3DXVECTOR3 v11v9( pVerts[11].vPosition - pVerts[9].vPosition );
    XGVec3Cross( &vNormal, &v10v9, &v11v9 );
    XGVec3Normalize( &vNormal, &vNormal );
    pVerts[ 9].vNormal    = vNormal;
    pVerts[10].vNormal    = vNormal;
    pVerts[11].vNormal    = vNormal;

    pVerts[ 9].vTexCoords = XGVECTOR2( 0.5f, 0.0f );
    pVerts[10].vTexCoords = XGVECTOR2( 0.0f, 1.0f );
    pVerts[11].vTexCoords = XGVECTOR2( 1.0f, 1.0f );

    m_pVB->Unlock();
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: This creates all device-dependent managed objects, such as managed
//       textures and managed vertex buffers.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    HRESULT hr;

    // create the fonts
    if( FAILED( m_Font.Create( "Font.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create help
    if( FAILED( m_Help.Create( "Gamepad.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create the resources
    if( FAILED( m_xprResource.Create( "Resource.xpr", resource_NUM_RESOURCES ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create vertex shaders
    DWORD dwSimpleVertexDecl[] =
    {
        D3DVSD_STREAM( 0 ),              // This data comes from stream 0
        D3DVSD_REG( 0, D3DVSDT_FLOAT3 ), // v0 = Position
        D3DVSD_REG( 1, D3DVSDT_FLOAT3 ), // v1 = Normal
        D3DVSD_REG( 2, D3DVSDT_FLOAT2 ), // v2 = Tex coords
        D3DVSD_END()
    };

    hr = XBUtil_CreateVertexShader( "Shaders\\Simple.xvu",
                                    dwSimpleVertexDecl, 
                                    &m_dwSimpleVertexShader );
    if( FAILED(hr) )
        return hr;

    // Get a pointer to the texture.
    m_pTexture = m_xprResource.GetTexture( resource_DefaultTexture_OFFSET );

    // Create index and vertex buffers for the object.
    MakeObject();

    // Set up matrices
    m_vEye = XGVECTOR3( 0.0f, 0.0f, -40.0f );
    XGVECTOR3 vAt( 0.0f, 0.0f, 0.0f );
    XGVECTOR3 vUp( 0.0f, 1.0f, 0.0f );
    XGMatrixLookAtLH( &m_matView, &m_vEye, &vAt, &vUp );

    XGMatrixPerspectiveFovLH( &m_matProj, XG_PI/2, 640.0f/480.0f, 
                              1.0f, 1000.0f );

    m_pd3dDevice->SetTransform( D3DTS_VIEW,       &m_matView );
    m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &m_matProj );

    // Light direction.
    m_vLightDir = XGVECTOR3( 0.0f, 0.707107f, -0.707107f );

    // Allocate memory for object state.
    m_vObjectPos   = new XGVECTOR3[NUM_OBJECTS];
    m_qObjectQuat  = new XGQUATERNION[NUM_OBJECTS];
    m_matObject    = (XGMATRIX*)_aligned_malloc( sizeof(XGMATRIX) * NUM_OBJECTS, 16 );
    m_vObjectLight = (XGVECTOR4*)_aligned_malloc( sizeof(XGVECTOR4) * NUM_OBJECTS, 16 );

    // Randomly position and orient objects.
    for( INT i = 0; i < NUM_OBJECTS; i++ )
    {
        m_vObjectPos[i].z = -30.0f + ( rand() / FLOAT(RAND_MAX) ) * 60.0f;
        m_vObjectPos[i].x = -30.0f + ( rand() / FLOAT(RAND_MAX) ) * 60.0f;
        m_vObjectPos[i].y = -30.0f + ( rand() / FLOAT(RAND_MAX) ) * 60.0f;

        FLOAT yaw   = ( rand() / FLOAT(RAND_MAX) ) * 2.0f * XG_PI;
        FLOAT pitch = ( rand() / FLOAT(RAND_MAX) ) * 2.0f * XG_PI;
        FLOAT roll  = ( rand() / FLOAT(RAND_MAX) ) * 2.0f * XG_PI;

        XGQuaternionRotationYawPitchRoll( &m_qObjectQuat[i], yaw, pitch, roll );

        // Build matrix.
        XGMATRIX matTemp;
        XGMatrixRotationQuaternion( &matTemp, &m_qObjectQuat[i] );

        matTemp._41 = m_vObjectPos[i].x;
        matTemp._42 = m_vObjectPos[i].y;
        matTemp._43 = m_vObjectPos[i].z;

        XGMatrixTranspose( &m_matObject[i], &matTemp );

        // Set local space light direction.
        XGVec3TransformNormal( (XGVECTOR3*)&m_vObjectLight[i], 
                               &m_vLightDir, 
                               &m_matObject[i] );
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

    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
        m_bFast = !m_bFast;

    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_B] )
        m_bRotateCamera = !m_bRotateCamera;

    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_X] )
        m_bRotateObjects = !m_bRotateObjects;

    if( m_bRotateObjects )
    {
        XGQUATERNION qRotation;
        XGVECTOR3    vRotationAxis( 0.0f, 1.0f, 0.0f );
        XGQuaternionRotationAxis( &qRotation, &vRotationAxis, 0.02f );

        // Rotate objects around the y axis..
        for( INT i = 0; i < NUM_OBJECTS; i++ )
        {
            m_qObjectQuat[i] *= qRotation;

            // Update matrix and light vector
            XGMATRIX matTemp;

            XGMatrixRotationQuaternion( &matTemp, &m_qObjectQuat[i] );

            matTemp._41 = m_vObjectPos[i].x;
            matTemp._42 = m_vObjectPos[i].y;
            matTemp._43 = m_vObjectPos[i].z;

            XGMatrixTranspose( &m_matObject[i], &matTemp );

            XGVec3TransformNormal( (XGVECTOR3*)&m_vObjectLight[i], 
                                   &m_vLightDir, 
                                   &m_matObject[i] );
        }
    }

    if( m_bRotateCamera )
    {
        XGVECTOR3 vAt( 0.0f, 0.0f, 0.0f );
        XGVECTOR3 vUp( 0.0f, 1.0f, 0.0f );

        // Rotate eye around up axis.
        XGMATRIX matRotate;
        XGMatrixRotationAxis( &matRotate, &vUp, m_fElapsedTime * 0.15f );
        XGVec3TransformCoord( &m_vEye, &m_vEye, &matRotate );

        XGMatrixLookAtLH( &m_matView, &m_vEye, &vAt, &vUp );

        m_pd3dDevice->SetTransform( D3DTS_VIEW, &m_matView );
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: RenderObjects()
// Desc: Draw a whole bunch of objects. For each object, the 
//       world-view-projection-viewport matrix is computed from the world 
//       matrix for the object and from the view-projection-viewport matrix
//       and sent to the GPU in some vertex shader constants. State that is 
//       common to all the objects is set outside the loop for efficiency.
//-----------------------------------------------------------------------------
VOID CXBoxSample::RenderObjects()
{
    XGMATRIX matProjectionViewport;
    
    // Get the projection viewport matrix.
    // The viewport matrix includes the viewport x,y scale and the 
    // appropriate z scale.
    m_pd3dDevice->GetProjectionViewportMatrix( &matProjectionViewport );

    // Composite the view and projection-viewport matrices.
    XGMATRIX matViewProjection;
    XGMatrixMultiply( &matViewProjection, &m_matView, &matProjectionViewport );

    // Keep the transposed view projection matrix around so that we can
    // avoid the transpose for each object.
    XGMatrixTranspose( &matViewProjection, &matViewProjection );

    // Set any state that is shared by all the objects outside of the
    // loop that draws the objects.
    m_pd3dDevice->SetVertexShader( m_dwSimpleVertexShader );
    m_pd3dDevice->SetStreamSource( 0, m_pVB, sizeof(OBJECTVERTEX) );

    // Set viewport offsets for the screenspace vertex shader.
    FLOAT fViewportOffsets[4] = { 0.53125f, 0.53125f, 0.0f, 0.0f };
    m_pd3dDevice->SetVertexShaderConstant( 95, fViewportOffsets, 1 );

    // Setup the texture and color combiners.
    m_pd3dDevice->SetTexture( 0, m_pTexture );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MIPFILTER, D3DTEXF_LINEAR );

    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );

    if( m_bFast )
    {
        for( INT i = 0; i < NUM_OBJECTS; i++ )
        {
            // Set vertex shader constansts by directly writing the calculated
            // values into the push-buffer.
            DWORD* pPush;

            // Reserve space for 23 DWORDS (3 command + 20 data)
            m_pd3dDevice->BeginState( 23, &pPush );

            // Begin the vertex shader constant load command.
            pPush[0] = D3DPUSH_ENCODE( D3DPUSH_SET_TRANSFORM_CONSTANT_LOAD, 1 );

            // Specify the starting register (physical registers are offset by
            // 96 from the D3D logical register).
            pPush[1] = 96 + 0;

            // Specify the number of DWORDS to load. 20 DWORDS for 5 constants.
            pPush[2] = D3DPUSH_ENCODE( D3DPUSH_SET_TRANSFORM_CONSTANT, 20 );

            // Compute matViewProjection * m_ObjectMat[i].
            // Since the matrices are already transposed the multiplication
            // order is reversed.
            XGMATRIX*  pMat   = &m_matObject[i];
            XGVECTOR4* pLight = &m_vObjectLight[i];

            __asm
            {
                lea     eax, matViewProjection
                mov     ecx, pMat
                mov     edx, pLight

                // prefetch matrix data (two ahead is sufficient)
                prefetchnta [ecx+128]
                prefetchnta [ecx+128+32]

                // prefetch light data.
                prefetchnta [edx+32]

                // SSE 4x4 matrix multiply.
                // eax points to the left side matrix and ecx points to the 
                // right side matrix.
                // The result will be in xmm0, xmm1, xmm2, xmm3
                movaps  xmm0, [eax]
                shufps  xmm0, xmm0, 0         
                mulps   xmm0, [ecx]

                movaps  xmm1, [eax]
                shufps  xmm1, xmm1, 55h
                mulps   xmm1, [ecx+16]

                movaps  xmm2, [eax]
                shufps  xmm2, xmm2, 0AAh
                mulps   xmm2, [ecx+32]

                addps   xmm0, xmm1

                movaps  xmm3, [eax]
                shufps  xmm3, xmm3, 0FFh
                mulps   xmm3, [ecx+48]

                addps   xmm0, xmm2

                movaps  xmm1, [eax+16]
                shufps  xmm1, xmm1, 0
                mulps   xmm1, [ecx]

                addps   xmm0, xmm3

                movaps  xmm2, [eax+16]
                shufps  xmm2, xmm2, 55h
                mulps   xmm2, [ecx+16]

                movaps  xmm3, [eax+16]
                shufps  xmm3, xmm3, 0AAh
                mulps   xmm3, [ecx+32]

                addps   xmm1, xmm2

                movaps  xmm4, [eax+16]
                shufps  xmm4, xmm4, 0FFh
                mulps   xmm4, [ecx+48]

                addps   xmm1, xmm3

                movaps  xmm2, [eax+32]
                shufps  xmm2, xmm2, 0
                mulps   xmm2, [ecx]

                addps   xmm1, xmm4

                movaps  xmm3, [eax+32]
                shufps  xmm3, xmm3, 55h
                mulps   xmm3, [ecx+16]

                movaps  xmm4, [eax+32]
                shufps  xmm4, xmm4, 0AAh
                mulps   xmm4, [ecx+32]

                addps   xmm2, xmm3

                movaps  xmm5, [eax+32]
                shufps  xmm5, xmm5, 0FFh
                mulps   xmm5, [ecx+48]

                addps   xmm2, xmm4

                movaps  xmm3, [eax+48]
                shufps  xmm3, xmm3, 0
                mulps   xmm3, [ecx]

                addps   xmm2, xmm5

                movaps  xmm4, [eax+48]
                shufps  xmm4, xmm4, 55h
                mulps   xmm4, [ecx+16]

                movaps  xmm5, [eax+48]
                shufps  xmm5, xmm5, 0AAh
                mulps   xmm5, [ecx+32]

                addps   xmm3, xmm4

                movaps  xmm6, [eax+48]
                shufps  xmm6, xmm6, 0FFh
                mulps   xmm6, [ecx+48]

                addps   xmm3, xmm5

                addps   xmm3, xmm6

                mov     eax, pPush
                movaps  xmm4, [edx]     // Load the light vector.

                // Store the matrix and light vector into the push-buffer.
                movups  [eax+12],    xmm0
                movups  [eax+12+16], xmm1
                movups  [eax+12+32], xmm2
                movups  [eax+12+48], xmm3
                movups  [eax+12+64], xmm4
            }

            // The EndState parameter must always point to exactly the end of 
            // the data written, otherwise random Graphics Hardware Errors 
            // will result:
            m_pd3dDevice->EndState( pPush + 23 );

            // Draw the object.
            m_pd3dDevice->DrawVertices( m_PrimitiveType, 0, m_dwNumVertices );
        }
    }
    else
    {
        for( INT i = 0; i < NUM_OBJECTS; i++ )
        {
            // Compute the composite matrix and set it.
            // Since the matrices are already transposed the multiplication
            // order is reversed. And we do not have to transpose the result.
            XGMATRIX matComposite;
            XGMatrixMultiply( &matComposite, &matViewProjection, 
                                             &m_matObject[i] );

            m_pd3dDevice->SetVertexShaderConstant( 0, &matComposite, 4 );
            m_pd3dDevice->SetVertexShaderConstant( 4, &m_vObjectLight[i], 1 );

            // Draw the object.
            m_pd3dDevice->DrawVertices( m_PrimitiveType, 0, m_dwNumVertices );
        }
    }
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, the call is the entry point for 3d
//       rendering. This function sets up the default render states, clears the
//       viewport, renders the objects, and calls present.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Render()
{
    // Draw a gradient filled background
    RenderGradientBackground( 0xff000000, 0xff0000ff );

    // Set default render states
    m_pd3dDevice->SetRenderState( D3DRS_DITHERENABLE,     TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE,  FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE,          TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_LIGHTING,         FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE,         D3DCULL_CCW );

    // Draw the objects.
    RenderObjects();

    // Show title, frame rate, and help
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 48, 36, 0xffffffff,  L"FastVSConstants" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 592, 38, 0xffffff00, m_strFrameRate, XBFONT_RIGHT );

        static FLOAT fLastTime = 0.0f;
        static FLOAT fCPS      = 0.0f;
        static DWORD dwFrames  = 0L;
        static WCHAR strBuffer[50];

        dwFrames++;

        if( m_fTime - fLastTime > 1.0f )
        {
            fCPS      = (dwFrames * NUM_OBJECTS) / ( m_fTime - fLastTime );
            fLastTime = m_fTime;
            dwFrames  = 0;
            swprintf( strBuffer, L"%.1f Objects per Second", fCPS );
        }

        m_Font.DrawText( 64, 75, 0xffffffff, strBuffer );

        if( m_bFast )
            m_Font.DrawText( 64, 100, 0xffffffff, L"Fast Drawing" );
        else
            m_Font.DrawText( 64, 100, 0xffffffff, L"Slow Drawing" );
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}
