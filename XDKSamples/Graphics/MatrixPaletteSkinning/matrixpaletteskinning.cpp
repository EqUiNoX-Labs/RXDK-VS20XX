//-----------------------------------------------------------------------------
// File: MatrixPaletteSkinning.cpp
//
// Desc: This sample demonstrates how to perform matrix palette skinning on
//       the Xbox, using vertex shaders
//
// Hist: 11.09.01 - New for December XDK
//       03.28.02 - Added optional bumpmapping.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <XBMesh.h>
#include <xgraphics.h>




//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_BACK_BUTTON,  XBHELP_PLACEMENT_1, L"Display help" },
    { XBHELP_START_BUTTON, XBHELP_PLACEMENT_1, L"Pause" },
    { XBHELP_LEFTSTICK,    XBHELP_PLACEMENT_1, L"Rotate camera" },
    { XBHELP_RIGHTSTICK,   XBHELP_PLACEMENT_1, L"Zoom in/out" },
    { XBHELP_DPAD,         XBHELP_PLACEMENT_2, L"Add/remove\nbones" },
    { XBHELP_A_BUTTON,     XBHELP_PLACEMENT_2, L"Next\nanimation" },
    { XBHELP_B_BUTTON,     XBHELP_PLACEMENT_2, L"Toggle\nbumpmapping" },
};

#define NUM_HELP_CALLOUTS ( sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]) )




//-----------------------------------------------------------------------------
// Constants for how many bones and what constants register they use.
//-----------------------------------------------------------------------------
const int MIN_BONES = 5;
const int MAX_BONES = 30;
const int BONE_REGISTER_BASE = 6;

enum ANIMATE_MODE
{
    ANIMATE_SINE = 0,
    ANIMATE_CIRCLE,
    ANIMATE_MAX,
};




//-----------------------------------------------------------------------------
// Structure for per-vertex bone information.
//-----------------------------------------------------------------------------
#pragma pack(push,1)
struct BoneIndicesAndWeights
{
    SHORT Indices[3];   // Index of the constant registers holding the bone matrix.
    FLOAT Weights[3];   // Weight for the corresponding bone matrix.
};
#pragma pack(pop)




//-----------------------------------------------------------------------------
// Structure for texture space basis.
//-----------------------------------------------------------------------------
struct TextureSpaceBasis
{
    D3DXVECTOR3 S;
    D3DXVECTOR3 T;
    D3DXVECTOR3 SxT;
};




//-----------------------------------------------------------------------------
// Name: class CXBMeshBump
// Desc: Subclass of the mesh class that allows us to set the appropriate bump
//       map for each material subset of the mesh.
//-----------------------------------------------------------------------------
#define XBMESH_SETBUMPTEXTURE 0x80000000

class CXBMeshBump : public CXBMesh
{
    CXBPackedResource* m_pResource;

public:
    // Override of the Create method.
    HRESULT Create( CHAR* strFilename, CXBPackedResource* pResource )
    {
        CXBMesh::Create( strFilename, pResource );

        // Create any bump map textures used by the meshes' subsets.
        m_pResource = pResource;

        return S_OK;
    }

    // Prerender callback (called for each subset).
    virtual BOOL RenderCallback( DWORD dwSubset, XBMESH_SUBSET* pSubset, DWORD dwFlags )
    {
        if( dwFlags & XBMESH_SETBUMPTEXTURE )
        {
            // Set corresponding normal map.
            CHAR strBumpTexture[64];

            strcpy( strBumpTexture, pSubset->strTexture );
            strBumpTexture[2] = 'B';

            if( m_pResource )
            {
                LPDIRECT3DTEXTURE8 pTexture = m_pResource->GetTexture( strBumpTexture );
                D3DDevice::SetTexture( 1, pTexture );
            }
        }

        return TRUE;
    }
};




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

    BOOL        m_bBumpMapped;      // Draw bump-mapped.

    // Information for each bone.
    INT                     m_iNumberOfBones;
    FLOAT                   m_fBoneInfluenceRadius;
    D3DXVECTOR3             m_vBoneCenter[MAX_BONES];
    D3DXMATRIX              m_matBoneTransforms[MAX_BONES];

    CXBPackedResource       m_xprResource;          // Snake textures
    CXBMeshBump             m_Snake;                // Snake model

    DWORD                   m_dwDeformShader;       // Vertex shader handle
    DWORD                   m_dwPixelShader;        // Pixel shader handle

    DWORD                   m_dwDeformBumpShader;   // Vertex shader handle
    DWORD                   m_dwBumpPixelShader;    // Pixel shader handle

    ANIMATE_MODE            m_Mode;                 // Animation mode

    D3DXVECTOR3             m_vViewAngle;           // View angle
    D3DXVECTOR3             m_vCameraPos;           // Camera position

    LPDIRECT3DVERTEXBUFFER8 m_pBoneVB;              // Vertex buffer w/ bones
    LPDIRECT3DVERTEXBUFFER8 m_pBasisVB;             // Vertex buffer w/ basis vectors

    LPDIRECT3DCUBETEXTURE8  m_pNormalizationCubemap;

    VOID    SetupBones( XBMESH_DATA& MeshData );            // Sets up the bones
    HRESULT CreateMeshWeights( XBMESH_DATA& MeshData );     // Sets up weights
    HRESULT CreateMeshBasisVectors( XBMESH_DATA& MeshData );
    VOID    UpdateBoneTransforms();                         // Animates bones

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
    m_bDrawHelp      = FALSE;
    m_bBumpMapped    = FALSE;
    m_iNumberOfBones = 20;
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

    // Create the resources
    if( FAILED( m_xprResource.Create( "Resource.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Load the snake.
    if( FAILED( m_Snake.Create( "Models\\Snake.xbg", &m_xprResource ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create vertex shader.
    DWORD dwVertexDecl[] =
    {
        D3DVSD_STREAM( 0 ),
        D3DVSD_REG( 0, D3DVSDT_FLOAT3 ),  // v0 = Position
        D3DVSD_REG( 1, D3DVSDT_FLOAT3 ),  // v1 = Normal
        D3DVSD_REG( 2, D3DVSDT_FLOAT2 ),  // v2 = Tex coords

        D3DVSD_STREAM( 1 ),
        D3DVSD_REG( 3, D3DVSDT_SHORT3 ),  // Bone matrix indices
        D3DVSD_REG( 4, D3DVSDT_FLOAT3 ),  // Bone weights
        
        D3DVSD_END()
    };

    if( FAILED( XBUtil_CreateVertexShader( "Shaders\\MatrixPaletteSkinning.xvu", 
                                           dwVertexDecl, &m_dwDeformShader ) ) )
        return E_FAIL;

    // Create bump-mapping vertex shader.
    DWORD dwBumpVertexDecl[] =
    {
        D3DVSD_STREAM( 0 ),
        D3DVSD_REG( 0, D3DVSDT_FLOAT3 ),  // v0 = Poistion
        D3DVSD_REG( 1, D3DVSDT_FLOAT3 ),  // v1 = Normal
        D3DVSD_REG( 2, D3DVSDT_FLOAT2 ),  // v2 = Tex coords

        D3DVSD_STREAM( 1 ),
        D3DVSD_REG( 3, D3DVSDT_SHORT3 ),  // Bone matrix indices
        D3DVSD_REG( 4, D3DVSDT_FLOAT3 ),  // Bone weights
        
        D3DVSD_STREAM( 2 ),
        D3DVSD_REG( 5, D3DVSDT_FLOAT3 ),  // S basis vector
        D3DVSD_REG( 6, D3DVSDT_FLOAT3 ),  // T basis vector
        D3DVSD_REG( 7, D3DVSDT_FLOAT3 ),  // SxT basis vector

        D3DVSD_END()
    };

    // Load deforming vertex shader.
    if( FAILED( XBUtil_CreateVertexShader( "Shaders\\BumpMatrixPaletteSkinning.xvu", 
                                           dwBumpVertexDecl, &m_dwDeformBumpShader ) ) )
        return E_FAIL;

    // Create normal pixel shader.
    if( FAILED( XBUtil_CreatePixelShader( "Shaders\\DiffuseAmbientSpecular.xpu", &m_dwPixelShader ) ) )
        return E_FAIL;

    // Create bump-mapped pixel shader.
    if( FAILED( XBUtil_CreatePixelShader( "Shaders\\Dot3BumpMap.xpu", &m_dwBumpPixelShader ) ) )
        return E_FAIL;

    // Create vector normalization cube map.
    if( FAILED( XBUtil_CreateNormalizationCubeMap( 64, &m_pNormalizationCubemap ) ) )
        return E_FAIL;

    // Setup bones.
    SetupBones( m_Snake.m_pMeshFrames[0].m_MeshData );

    // Create mesh weights.
    CreateMeshWeights( m_Snake.m_pMeshFrames[0].m_MeshData );

    // Create mesh basis vectors.
    CreateMeshBasisVectors( m_Snake.m_pMeshFrames[0].m_MeshData );

    m_Mode       = ANIMATE_SINE;

    m_vCameraPos = D3DXVECTOR3( 0.0f, 0.0f, 3.0f );
    m_vViewAngle = D3DXVECTOR3( -0.5f, 3.0f, 0.0f );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SetupBones()
// Desc: Define the position and influence of the bones.
//-----------------------------------------------------------------------------
void CXBoxSample::SetupBones( XBMESH_DATA& MeshData )
{
    // Get the bounding box of the mesh.
    D3DXVECTOR3 vMin, vMax;
    D3DXMATRIX  matIdentity;
    D3DXMatrixIdentity( &matIdentity );
    m_Snake.ComputeMeshBoundingBox( &MeshData, &matIdentity, &vMin, &vMax );

    // Space bones evenly along z, going through the middle of the snake
    FLOAT x      = ( vMin.x + vMax.x ) * 0.5f;
    FLOAT y      = ( vMin.y + vMax.y ) * 0.5f;
    FLOAT z_step = ( vMax.z - vMin.z ) / m_iNumberOfBones;
    FLOAT z      = vMin.z + 0.5f * z_step;

    for( int i = 0; i < m_iNumberOfBones; i++ )
    {
        m_vBoneCenter[i].x = x;
        m_vBoneCenter[i].y = y;
        m_vBoneCenter[i].z = z;

        D3DXMatrixIdentity( &m_matBoneTransforms[i] );

        z += z_step;
    }

    // The z distance that the bone influences.
    m_fBoneInfluenceRadius = z_step * 1.0f;
}




//-----------------------------------------------------------------------------
// Name: CreateMeshWeights()
// Desc: Create a vertex buffer to hold the bone matrix indices and the 
//       weights.  We'll walk the list of bones, and determine which vertices
//       each bone influences (we'll only keep the top 3)
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::CreateMeshWeights( XBMESH_DATA& MeshData )
{
    // Create a vertex buffer to hold the indices and weights ..
    m_pd3dDevice->CreateVertexBuffer( MeshData.m_dwNumVertices * sizeof(BoneIndicesAndWeights),
                                      0, 0, 0, &m_pBoneVB );

    // .. and lock it so we can fill it in.
    BoneIndicesAndWeights* pVertexIndicesAndWeights;
    m_pBoneVB->Lock( 0, 0, (BYTE**)&pVertexIndicesAndWeights, 0 );

    // Initialize indices and weights.
    for( DWORD v = 0; v < MeshData.m_dwNumVertices; v++ )
    {
        for( int j = 0; j < 3; j++ )
        {
            pVertexIndicesAndWeights[v].Indices[j] = 0;
            pVertexIndicesAndWeights[v].Weights[j] = 0.0f;
        }
    }

    BYTE* pVertices;
    MeshData.m_VB.Lock( 0, 0, &pVertices, 0 );

    // Evenly space bones long z.
    for( SHORT i = 0; i < m_iNumberOfBones; i++ )
    {
        FLOAT z = m_vBoneCenter[i].z;

        // Calculate influence of bones on each vertex.
        for( DWORD v = 0; v < MeshData.m_dwNumVertices; v++ )
        {
            D3DXVECTOR3* pVert = (D3DXVECTOR3*)( pVertices + v * MeshData.m_dwVertexSize );

            FLOAT weight = 1.0f - fabsf( pVert->z - z ) / m_fBoneInfluenceRadius;
            
            // Weight = min( 1 - distance / radius, 0 ).
            if( weight > 0.0f )
            {
                // Keep only the nearest 3.
                FLOAT min_weight = weight;
                int min_j = -1;

                for( int j = 0; j < 3; j++ )
                {
                    if( pVertexIndicesAndWeights[v].Weights[j] < min_weight )
                    {
                        min_weight = pVertexIndicesAndWeights[v].Weights[j];
                        min_j = j;
                    }
                }

                if( min_j != -1 )
                {
                    pVertexIndicesAndWeights[v].Indices[min_j] = BONE_REGISTER_BASE + i * 3;
                    pVertexIndicesAndWeights[v].Weights[min_j] = weight;
                }
            }   
        }
    }

    MeshData.m_VB.Unlock();

    // Normalize weights.
    for( DWORD v = 0; v < MeshData.m_dwNumVertices; v++ )
    {
        FLOAT fTotalWeight = 0.0f;

        for( int j = 0; j < 3; j++ )
        {
            fTotalWeight += pVertexIndicesAndWeights[v].Weights[j];
        }

        if( fTotalWeight > 0.0f )
        {
            FLOAT fInvTotalWeight = 1.0f / fTotalWeight;

            for( int j = 0; j < 3; j++ )
            {
                pVertexIndicesAndWeights[v].Weights[j] *= fInvTotalWeight;
            }
        }
    }

    m_pBoneVB->Unlock();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: CreateMeshBasisVectors()
// Desc: 
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::CreateMeshBasisVectors( XBMESH_DATA& MeshData )
{
    const FLOAT SMALL_FLOAT = 1e-12f;

    // Create a vertex buffer
    m_pd3dDevice->CreateVertexBuffer( MeshData.m_dwNumVertices * sizeof(TextureSpaceBasis), 
                                      0, 0, 0, &m_pBasisVB );

    // Fill the VB with the basis vectors
    TextureSpaceBasis* pBasis;
    m_pBasisVB->Lock( 0, 0, (BYTE**)&pBasis, 0 );

    // Clear the basis vectors
    for( DWORD i = 0; i < MeshData.m_dwNumVertices; i++)
    {
        pBasis[i].S = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );
        pBasis[i].T = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );
    }

    // The mesh vertex buffer has to have position, normal, and texture coords.
    assert( MeshData.m_dwFVF == (D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_TEX1) );

    // The mesh type must be either a triangle list or a trianglestrip
    assert( D3DPT_TRIANGLELIST == MeshData.m_dwPrimType || D3DPT_TRIANGLESTRIP == MeshData.m_dwPrimType );

    // Structure of the base vertices.
    struct MeshVertex
    {
        D3DXVECTOR3 pos;
        D3DXVECTOR3 norm;
        FLOAT       tu, tv;
    };

    // Lock the vertex buffer.
    MeshVertex* pVertices;
    MeshData.m_VB.Lock( 0, 0, (BYTE**)&pVertices, D3DLOCK_READONLY );

    // Clear the basis vectors
    // RXDK: hoisted out of the for-init (MSVC's old for-scope leaked it past the loop)
    DWORD i;
    for( i = 0; i < MeshData.m_dwNumVertices; i++ )
    {
        pBasis[i].S = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
        pBasis[i].T = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
    }

    // Walk through the triangle list and calculate gradiants for each triangle.
    // Sum the results into the S and T components.
    WORD* pIndices;
    MeshData.m_IB.Lock( 0, 0, (BYTE**)&pIndices, D3DLOCK_READONLY );

    DWORD ind0 = 0;
    DWORD ind1 = pIndices[0];
    DWORD ind2 = pIndices[1];
    DWORD index = 0;

    if( D3DPT_TRIANGLESTRIP == MeshData.m_dwPrimType )
    {
        index = 2;
    }

    while( index < MeshData.m_dwNumIndices )
    {
        MeshVertex        *pVert0, *pVert1, *pVert2;
        TextureSpaceBasis *pBV0,   *pBV1,   *pBV2;

        if( D3DPT_TRIANGLELIST == MeshData.m_dwPrimType )
        {
            pVert0 = pVertices + pIndices[index+0];
            pVert1 = pVertices + pIndices[index+1];
            pVert2 = pVertices + pIndices[index+2];

            pBV0 = pBasis + pIndices[index+0];
            pBV1 = pBasis + pIndices[index+1];
            pBV2 = pBasis + pIndices[index+2];

            index += 3;
        }
        else // ( D3DPT_TRIANGLESTRIP == MeshData.m_dwPrimType )
        {
            ind0 = ind1;
            ind1 = ind2;
            ind2 = pIndices[index];
            
            if( ind0 != ind1 && ind1 != ind2 && ind2 != ind0 )
            {
                if( i & 1 )
                {
                    pVert0 = pVertices + ind2;
                    pVert1 = pVertices + ind1;
                    pVert2 = pVertices + ind0;

                    pBV0 = pBasis + ind2;
                    pBV1 = pBasis + ind1;
                    pBV2 = pBasis + ind0;
                }
                else
                {
                    pVert0 = pVertices + ind0;
                    pVert1 = pVertices + ind1;
                    pVert2 = pVertices + ind2;

                    pBV0 = pBasis + ind0;
                    pBV1 = pBasis + ind1;
                    pBV2 = pBasis + ind2;
                }

                index++;
            }
            else
            {
                // Degenerate triangle.
                index++;
                continue;
            }
        }

        D3DXVECTOR3 edge01;
        D3DXVECTOR3 edge02;
        D3DXVECTOR3 cp;

        FLOAT ds1 = pVert1->tu - pVert0->tu;
        FLOAT dt1 = pVert1->tv - pVert0->tv;

        FLOAT ds2 = pVert2->tu - pVert0->tu;
        FLOAT dt2 = pVert2->tv - pVert0->tv;

        // x, s, t
        edge01 = D3DXVECTOR3( pVert1->pos.x - pVert0->pos.x, ds1, dt1 );
        edge02 = D3DXVECTOR3( pVert2->pos.x - pVert0->pos.x, ds2, dt2 );

        D3DXVec3Cross( &cp, &edge01, &edge02 );
        if( fabs(cp.x) > SMALL_FLOAT )
        {
            FLOAT dsdx = -cp.y / cp.x;
            FLOAT dtdx = -cp.z / cp.x;

            pBV0->S.x += dsdx;
            pBV0->T.x += dtdx;

            pBV1->S.x += dsdx;
            pBV1->T.x += dtdx;

            pBV2->S.x += dsdx;
            pBV2->T.x += dtdx;
        }

        // y, s, t
        edge01 = D3DXVECTOR3( pVert1->pos.y - pVert0->pos.y, ds1, dt1 );
        edge02 = D3DXVECTOR3( pVert2->pos.y - pVert0->pos.y, ds2, dt2 );

        D3DXVec3Cross( &cp, &edge01, &edge02 );
        if( fabs(cp.x) > SMALL_FLOAT )
        {
            FLOAT dsdx = -cp.y / cp.x;
            FLOAT dtdx = -cp.z / cp.x;

            pBV0->S.y += dsdx;
            pBV0->T.y += dtdx;

            pBV1->S.y += dsdx;
            pBV1->T.y += dtdx;

            pBV2->S.y += dsdx;
            pBV2->T.y += dtdx;
        }

        // z, s, t
        edge01 = D3DXVECTOR3( pVert1->pos.z - pVert0->pos.z, ds1, dt1 );
        edge02 = D3DXVECTOR3( pVert2->pos.z - pVert0->pos.z, ds2, dt2 );

        D3DXVec3Cross( &cp, &edge01, &edge02 );
        if( fabs(cp.x) > SMALL_FLOAT )
        {
            FLOAT dsdx = -cp.y / cp.x;
            FLOAT dtdx = -cp.z / cp.x;

            pBV0->S.z += dsdx;
            pBV0->T.z += dtdx;

            pBV1->S.z += dsdx;
            pBV1->T.z += dtdx;

            pBV2->S.z += dsdx;
            pBV2->T.z += dtdx;
        }
    }

    MeshData.m_IB.Unlock();

    // Calculate the SxT vector
    for( DWORD i = 0; i < MeshData.m_dwNumVertices; i++ )
    {
        // Normalize the S, T vectors
        D3DXVec3Normalize( &pBasis[i].S, &pBasis[i].S );
        D3DXVec3Normalize( &pBasis[i].T, &pBasis[i].T );

        // Get the cross of the S and T vectors
        D3DXVec3Cross( &pBasis[i].SxT, &pBasis[i].S, &pBasis[i].T );

        // Get the direction of the SxT vector
        if( D3DXVec3Dot( &pBasis[i].SxT, &pVertices[i].norm ) < 0.0f )
            pBasis[i].SxT = -pBasis[i].SxT;
    }

    MeshData.m_VB.Unlock();
    m_pBasisVB->Unlock();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: UpdateBoneTransforms()
// Desc: Offset the bones using a time based sine function.
//-----------------------------------------------------------------------------
void CXBoxSample::UpdateBoneTransforms()
{
    switch( m_Mode )
    {
        case ANIMATE_SINE:
            // Update transform for each bone.
            for( int i = 0; i < m_iNumberOfBones; i++ )
            {
                // x = f(z+t) = sin(z+t) * 0.2
                // tangent = f'(z+t) = cos(z+t) * 0.2

                FLOAT t = 10.0f * i / m_iNumberOfBones - m_fAppTime * 2.0f;
                FLOAT angle = atanf( cosf( t ) * 0.2f );
                
                // Translate the bone back to the origin
                D3DXMatrixTranslation( &m_matBoneTransforms[i], -m_vBoneCenter[i].x,
                                                                -m_vBoneCenter[i].y,
                                                                -m_vBoneCenter[i].z );

                // Rotate to the tangent angle
                D3DXMATRIX mat;
                D3DXMatrixRotationY( &mat, angle );
                D3DXMatrixMultiply( &m_matBoneTransforms[i], &m_matBoneTransforms[i], &mat );

                // Then translate back to bone position + sine wave
                D3DXMatrixTranslation( &mat, m_vBoneCenter[i].x + sinf( t ) * 0.2f, 
                                             m_vBoneCenter[i].y,
                                             m_vBoneCenter[i].z );
                D3DXMatrixMultiply( &m_matBoneTransforms[i], &m_matBoneTransforms[i], &mat );
            }
            break;

        case ANIMATE_CIRCLE:
            for( int i = 0; i < m_iNumberOfBones; i++ )
            {
                // x = cos( z + time )
                // z = -sin( z + time )
                // tan = z + time

                FLOAT t = -5.0f * i / m_iNumberOfBones + m_fAppTime;
                FLOAT angle = t;

                // Translate to origin
                D3DXMatrixTranslation( &m_matBoneTransforms[i], -m_vBoneCenter[i].x,
                                                                -m_vBoneCenter[i].y,
                                                                -m_vBoneCenter[i].z );

                // Rotate bone
                D3DXMATRIX mat;
                D3DXMatrixRotationY( &mat, angle );
                D3DXMatrixMultiply( &m_matBoneTransforms[i], &m_matBoneTransforms[i], &mat );

                // Translate to position on unit circle
                D3DXMatrixTranslation( &mat, cosf( t ),
                                             m_vBoneCenter[i].y,
                                             -sinf( t ) );
                D3DXMatrixMultiply( &m_matBoneTransforms[i], &m_matBoneTransforms[i], &mat );
            }
            break;
    }

    // Send bone transforms to the vertex shader.
    for( int i = 0; i < m_iNumberOfBones; i++ )
    {
        D3DXMATRIX mat;
        D3DXMatrixTranspose( &mat, &m_matBoneTransforms[i] );
        m_pd3dDevice->SetVertexShaderConstant( BONE_REGISTER_BASE + i * 3, &mat, 3 );
    }
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

    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_A ] )
    {
        m_Mode = ANIMATE_MODE( ( m_Mode + 1 ) % ANIMATE_MAX );
    }

    if( m_DefaultGamepad.bPressedAnalogButtons[ XINPUT_GAMEPAD_B ] )
        m_bBumpMapped = !m_bBumpMapped;

    // Set the matrices
    D3DXMATRIX matWorld;
    D3DXMatrixIdentity( &matWorld );
    m_pd3dDevice->SetTransform( D3DTS_WORLD, &matWorld );

    // Update camera angle and position
    m_vViewAngle.y -= m_DefaultGamepad.fX1 * m_fElapsedTime;
    if( m_vViewAngle.y > 2 * D3DX_PI )
        m_vViewAngle.y -= 2 * D3DX_PI;
    else if( m_vViewAngle.y < 0.0f )
        m_vViewAngle.y += 2 * D3DX_PI;

    m_vViewAngle.x -= m_DefaultGamepad.fY1 * m_fElapsedTime;
    if( m_vViewAngle.x > 1.0f )
        m_vViewAngle.x = 1.0f;
    else if( m_vViewAngle.x < -1.0f )
        m_vViewAngle.x = -1.0f;

    m_vCameraPos.z -= m_DefaultGamepad.fY2 * m_fElapsedTime;
    if( m_vCameraPos.z < 0.2f )
        m_vCameraPos.z = 0.2f;

    // Calculate eye position based off view angle and camera zoom
    D3DXVECTOR3 vEyePosition;
    D3DXMATRIX m;
    D3DXMatrixRotationYawPitchRoll( &m, m_vViewAngle.y, m_vViewAngle.x, m_vViewAngle.z );
    D3DXVec3TransformCoord( &vEyePosition, &m_vCameraPos, &m );

    // Set up a view transform to look at the origin
    D3DXMATRIX  matView;
    D3DXVECTOR3 vLookAt( 0.0f, 0.0f, 0.0f );
    D3DXVECTOR3 vUp( 0.0f, 1.0f, 0.0f );
    D3DXMatrixLookAtLH( &matView, &vEyePosition, &vLookAt, &vUp );
    m_pd3dDevice->SetTransform( D3DTS_VIEW, &matView );

    // Projection transform
    D3DXMATRIX matProjection;
    D3DXMatrixPerspectiveFovLH( &matProjection, D3DX_PI/4, 4.0f/3.0f, 0.1f, 500.0f );
    m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &matProjection );

    // World * View * Projection composite transformation
    D3DXMATRIX matComposite;
    D3DXMatrixMultiply( &matComposite, &matWorld, &matView );
    D3DXMatrixMultiply( &matComposite, &matComposite, &matProjection );
    D3DXMatrixTranspose( &matComposite, &matComposite );
    m_pd3dDevice->SetVertexShaderConstant( 0, &matComposite, 4 );

    // Adjust number of bones
    int iOldBones = m_iNumberOfBones;
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
    {
        if( ++m_iNumberOfBones > MAX_BONES )
            m_iNumberOfBones = MAX_BONES;
    }
    else if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
    {
        if( --m_iNumberOfBones < MIN_BONES )
            m_iNumberOfBones = MIN_BONES;
    }

    if( iOldBones != m_iNumberOfBones )
    {
        // Setup bones.
        SetupBones( m_Snake.m_pMeshFrames[0].m_MeshData );

        // Create mesh weights.
        CreateMeshWeights( m_Snake.m_pMeshFrames[0].m_MeshData );
    }

    // Animate bones based off current time
    UpdateBoneTransforms();

    // Set light direction
    D3DXVECTOR4 vLightDir( 0.7071067f, 0.7071067f, 0.0f, 0.0f );
    m_pd3dDevice->SetVertexShaderConstant( 4, &vLightDir, 1 );

    // Get eye position in local space.
    D3DXVECTOR4 vLocalEyePos( 0.0f, 0.0f, 0.0f, 1.0f );

    D3DXMATRIX matViewInverse, matWorldInverse;
    D3DXMatrixInverse( &matViewInverse, 0, &matView );
    D3DXMatrixInverse( &matWorldInverse, 0, &matWorld );
    D3DXVec4Transform( &vLocalEyePos, &vLocalEyePos, &matViewInverse );
    D3DXVec4Transform( &vLocalEyePos, &vLocalEyePos, &matWorldInverse );

    // Put specular exponent in w.
    vLocalEyePos.w = 64.0f;

    m_pd3dDevice->SetVertexShaderConstant( 5, &vLocalEyePos, 1 );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the scene
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Render()
{
    // Draw a gradient filled background
    RenderGradientBackground( 0xff404040, 0xff404080 );

    // Set up render and texture states
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );

    // Draw the snake.
    if( m_bBumpMapped )
    {
        m_pd3dDevice->SetVertexShader( m_dwDeformBumpShader );
        m_pd3dDevice->SetStreamSource( 1, m_pBoneVB,  sizeof(BoneIndicesAndWeights) );
        m_pd3dDevice->SetStreamSource( 2, m_pBasisVB, sizeof(TextureSpaceBasis) );

        m_pd3dDevice->SetPixelShader( m_dwBumpPixelShader );

        m_pd3dDevice->SetTexture( 2, m_pNormalizationCubemap );
        m_pd3dDevice->SetTexture( 3, m_pNormalizationCubemap );

        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU,  D3DTADDRESS_WRAP );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV,  D3DTADDRESS_WRAP );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MIPFILTER, D3DTEXF_LINEAR );

        m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ADDRESSU,  D3DTADDRESS_WRAP );
        m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ADDRESSV,  D3DTADDRESS_WRAP );
        m_pd3dDevice->SetTextureStageState( 1, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetTextureStageState( 1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetTextureStageState( 1, D3DTSS_MIPFILTER, D3DTEXF_LINEAR );

        m_pd3dDevice->SetTextureStageState( 2, D3DTSS_ADDRESSU,  D3DTADDRESS_CLAMP );
        m_pd3dDevice->SetTextureStageState( 2, D3DTSS_ADDRESSV,  D3DTADDRESS_CLAMP );
        m_pd3dDevice->SetTextureStageState( 2, D3DTSS_ADDRESSW,  D3DTADDRESS_CLAMP );
        m_pd3dDevice->SetTextureStageState( 2, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetTextureStageState( 2, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetTextureStageState( 2, D3DTSS_MIPFILTER, D3DTEXF_NONE );

        m_pd3dDevice->SetTextureStageState( 3, D3DTSS_ADDRESSU,  D3DTADDRESS_CLAMP );
        m_pd3dDevice->SetTextureStageState( 3, D3DTSS_ADDRESSV,  D3DTADDRESS_CLAMP );
        m_pd3dDevice->SetTextureStageState( 3, D3DTSS_ADDRESSW,  D3DTADDRESS_CLAMP );
        m_pd3dDevice->SetTextureStageState( 3, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetTextureStageState( 3, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetTextureStageState( 3, D3DTSS_MIPFILTER, D3DTEXF_NONE );

        m_Snake.Render( XBMESH_NOFVF | XBMESH_NOMATERIALS | XBMESH_SETBUMPTEXTURE );

        m_pd3dDevice->SetPixelShader( NULL );

        m_pd3dDevice->SetTexture( 1, NULL );
        m_pd3dDevice->SetTexture( 2, NULL );
        m_pd3dDevice->SetTexture( 3, NULL );
    }
    else
    {
        m_pd3dDevice->SetVertexShader( m_dwDeformShader );
        m_pd3dDevice->SetStreamSource( 1, m_pBoneVB, sizeof(BoneIndicesAndWeights) );

        m_pd3dDevice->SetPixelShader( m_dwPixelShader );

        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU,  D3DTADDRESS_WRAP );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV,  D3DTADDRESS_WRAP );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MIPFILTER, D3DTEXF_LINEAR );

        m_Snake.Render( XBMESH_NOFVF | XBMESH_NOMATERIALS );

        m_pd3dDevice->SetPixelShader( NULL );
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
        m_Font.DrawText( 48, 36, 0xffffffff,  L"MatrixPaletteSkinning" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 592, 38, 0xffffff00, m_strFrameRate, XBFONT_RIGHT );

        WCHAR str[100];
        swprintf( str, L"%d", m_iNumberOfBones );
        m_Font.DrawText( 64, 80, 0xffffffff, L"Number of bones: " );
        m_Font.DrawText( 0xffffff00, str );
        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}

