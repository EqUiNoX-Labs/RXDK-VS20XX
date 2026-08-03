//-----------------------------------------------------------------------------
// File: BumpMesh.cpp
//
// Desc: A subclass of xbmesh that supports texture space bumpmapping.
//
// Hist:  02.01.03 - New for Feb 2003 XDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#include <xtl.h>
#include <xgraphics.h>
#include "BumpMesh.h"




//-----------------------------------------------------------------------------
// Expected input vertex.
//-----------------------------------------------------------------------------
struct MeshVertex
{
    D3DXVECTOR3 pos;
    D3DXVECTOR3 normal;
    D3DXVECTOR2 texcoord;
};




//-----------------------------------------------------------------------------
// Basis vectors for a vertex
//-----------------------------------------------------------------------------
struct TextureSpaceBasis
{
    D3DXVECTOR3 S;
    D3DXVECTOR3 T;
    D3DXVECTOR3 SxT;
};




//-----------------------------------------------------------------------------
// Name: Constructor
// Desc: Zero the basis vector buffer array pointer.
//-----------------------------------------------------------------------------
BumpMesh::BumpMesh() : CXBMesh()
{
    m_pMeshBasisVB = 0;
}




//-----------------------------------------------------------------------------
// Name: Destructor
// Desc: Free any resourced allocated by the BumpMesh.
//-----------------------------------------------------------------------------
BumpMesh::~BumpMesh()
{
    if( m_pMeshBasisVB )
    {
        for( DWORD i = 0; i < m_dwNumFrames; i++ )
            m_pMeshBasisVB[i]->Release();

        delete[] m_pMeshBasisVB;
    }
}




//-----------------------------------------------------------------------------
// Name: BumpMesh::RenderCallback()
// Desc: Set the appropriate basis vector vertex buffer as a source for stream
//       one and set the vertex shader constants for the bumpmap shader.
//-----------------------------------------------------------------------------
VOID BumpMesh::RenderMeshCallback( DWORD dwFrame, XBMESH_FRAME* pFrame,
                                   DWORD dwFlags )
{
    if( m_pMeshBasisVB )
    {
        // Set our VB with the texture space basis as stream 1.
        D3DDevice::SetStreamSource( 1, m_pMeshBasisVB[dwFrame], 
                                    sizeof(TextureSpaceBasis) );
    }

    // Note: when passing matrices to a vertex shader, we transpose them, since
    // the matrix multiplies are done with dot product operations on the matrix 
    // rows.
    D3DXMATRIX mat;
    D3DXMATRIX matWorld, matView, matProj;
    D3DDevice::GetTransform( D3DTS_WORLD,      &matWorld );
    D3DDevice::GetTransform( D3DTS_VIEW,       &matView );
    D3DDevice::GetTransform( D3DTS_PROJECTION, &matProj );

    // Pass the world * view * projection matrix to the vertex shader
    D3DXMatrixMultiply( &mat, &matView, &matProj );
    D3DXMatrixMultiply( &mat, &matWorld, &mat );
    D3DXMatrixTranspose( &mat, &mat );
    D3DDevice::SetVertexShaderConstant( 0, &mat, 4 );

    D3DXMATRIX matViewInverse;
    D3DXMatrixInverse( &matViewInverse, 0, &matView );

    D3DXMATRIX matWorldInverse;
    D3DXMatrixInverse( &matWorldInverse, 0, &matWorld );

    // Pass the local space light position to the vertex shader.
    D3DXVECTOR4 v4LocalLightPos = m_vLightPos;
    D3DXVec4Transform( &v4LocalLightPos, &v4LocalLightPos, &matWorldInverse );
    D3DDevice::SetVertexShaderConstant( 4, &v4LocalLightPos, 1 );

    // Pass the local space view position to the vertex shader.
    D3DXVECTOR4 v4LocalViewPos(0.0f, 0.0f, 0.0f, 1.0f);
    D3DXVec4Transform( &v4LocalViewPos, &v4LocalViewPos, &matViewInverse );
    D3DXVec4Transform( &v4LocalViewPos, &v4LocalViewPos, &matWorldInverse );
    D3DDevice::SetVertexShaderConstant( 5, &v4LocalViewPos, 1 );

    // Pass some constants to the vertex shader.
    D3DXVECTOR4 fConstantValues( 0.5f, 0.5f, 0.5f, 0.5f );
    D3DDevice::SetVertexShaderConstant( 6, fConstantValues, 1 );
}
    



//-----------------------------------------------------------------------------
// Name: BumpMesh::CalculateTextureSpaceBasis()
// Desc: Allocate the array of pointers to vertex buffers for the basis 
//       vectors for each frame of the mesh and call 
//       CalculateFrameTextureSpaceBasis on the root frame.
//-----------------------------------------------------------------------------
HRESULT BumpMesh::CalculateTextureSpaceBasis()
{
    m_pMeshBasisVB = new LPDIRECT3DVERTEXBUFFER8[m_dwNumFrames];

    if( !m_pMeshBasisVB )
        return E_FAIL;

    memset( m_pMeshBasisVB, 0, 
            sizeof(LPDIRECT3DVERTEXBUFFER8) * m_dwNumFrames );

    // Render the mesh triangles into texture.
    D3DXMATRIX matIdentity;
    D3DXMatrixIdentity( &matIdentity );

    if ( m_pMeshBasisVB )
        CalculateFrameTextureSpaceBasis( m_pMeshFrames, &matIdentity );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: BumpMesh::CalculateFrameTextureSpaceBasis()
// Desc: Call CalculateMeshTextureSpaceBasis for the mesh data associated with
//       this frame and recurse on any child and/or sibling frames.
//-----------------------------------------------------------------------------
VOID BumpMesh::CalculateFrameTextureSpaceBasis( XBMESH_FRAME* pFrame, 
                                                D3DXMATRIX* pmatParent )
{
    // Apply the frame's local transform
    D3DXMATRIX matWorld;
    D3DXMatrixMultiply( &matWorld, &pFrame->m_matTransform, pmatParent );

    // Compute texture space basis for the mesh data
    if( pFrame->m_MeshData.m_dwNumSubsets )
    {
        DWORD index = pFrame - m_pMeshFrames;
        CalculateMeshTextureSpaceBasis( &pFrame->m_MeshData, 
                                        m_pMeshBasisVB+index,
                                        &matWorld );
    }

    // Compute texture space basis for any child frames
    if( pFrame->m_pChild ) 
        CalculateFrameTextureSpaceBasis( pFrame->m_pChild, &matWorld  );

    // Compute texture space basis for any sibling frames
    if( pFrame->m_pNext )  
        CalculateFrameTextureSpaceBasis( pFrame->m_pNext, pmatParent );
}




//-----------------------------------------------------------------------------
// Name: BumpMesh::CalculateMeshTextureSpaceBasis()
// Desc: Calculate the texture space basis vectors for the given mesh data.
//       The texture space basis vectors provide a transform to/from texture
//       space. They are calculated by computing the delta slopes of the 
//       texture co-ordinates (ds/dx, dt/dx, ds/dy, dt/dy, dx/dz, dt/dz) for
//       each triangle and then averaging the slopes at each shared vertex,
//       similar to the way normals would be averaged to make a faceted object
//       appear smooth. The averaging is complicated by the fact that sometimes
//       vertices that should be shared based on their position and normal are
//       not shared becuase of differing texture co-ordinates. This case is 
//       handles by going through the vertices and averaging any that have 
//       positions and normals that are equal to within some tolerance values.
//       Some cases also exist where vertices are shared that shouldn't be.
//       For example: 1) If a texture is mirrored across an object the vertices
//       at the origin of the mirroring may be shared even though their tangent
//       spaces should point in opposite directions. 2) The vertex at the pole 
//       of a sphere may be shared even though each adjacent triangle should 
//       have a different tangent space for the vertex. These cases are not
//       correctly handled by this routine.
//
//-----------------------------------------------------------------------------
VOID BumpMesh::CalculateMeshTextureSpaceBasis( XBMESH_DATA* pMesh, 
                                              LPDIRECT3DVERTEXBUFFER8* pBasisVB,
                                              D3DXMATRIX* pmat )
{
    const FLOAT SMALL_FLOAT = 1e-12f;

    // Create a vertex buffer
    DWORD dwBufferSize = pMesh->m_dwNumVertices * sizeof(TextureSpaceBasis);
    D3DDevice::CreateVertexBuffer( dwBufferSize, 0, 0, 0, pBasisVB );

    // Fill the VB with the basis vectors
    TextureSpaceBasis* pBasis;
    (*pBasisVB)->Lock( 0, 0, (BYTE**)&pBasis, 0 );

    // Clear the basis vectors
    for( DWORD i = 0; i < pMesh->m_dwNumVertices; i++)
    {
        pBasis[i].S = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );
        pBasis[i].T = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );
    }

    // The mesh vertex buffer has to have position, normal, and texture coords.
    assert( pMesh->m_dwFVF == (D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_TEX1) );

    // Lock the vertex buffer.
    MeshVertex* pVertices;
    pMesh->m_VB.Lock( 0, 0, (BYTE**)&pVertices, D3DLOCK_READONLY );

    // Walk through the triangle list and calculate gradiants for each triangle.
    // Sum the results into the S and T components.
    WORD *pIndices;
    pMesh->m_IB.Lock( 0, 0, (BYTE**)&pIndices, D3DLOCK_READONLY );

    DWORD index, ind0 = 0, ind1 = 0, ind2 = 0;

    if ( D3DPT_TRIANGLELIST == pMesh->m_dwPrimType )
    {
        index = 0;
    }
    else if ( D3DPT_TRIANGLESTRIP == pMesh->m_dwPrimType )
    {
        ind0 = 0;
        ind1 = pIndices[0];
        ind2 = pIndices[1];
        index = 2;
    }
    else
    {
        // Unsupported primitive type.
        assert(0);
        return;
    }

    while ( index < pMesh->m_dwNumIndices )
    {
        MeshVertex *pVert0, *pVert1, *pVert2;
        TextureSpaceBasis *pBV0, *pBV1, *pBV2;

        if ( D3DPT_TRIANGLELIST == pMesh->m_dwPrimType )
        {
            pVert0 = pVertices + pIndices[index+0];
            pVert1 = pVertices + pIndices[index+1];
            pVert2 = pVertices + pIndices[index+2];

            pBV0 = pBasis + pIndices[index+0];
            pBV1 = pBasis + pIndices[index+1];
            pBV2 = pBasis + pIndices[index+2];

            index += 3;
        }
        else if ( D3DPT_TRIANGLESTRIP == pMesh->m_dwPrimType )
        {
            ind0 = ind1;
            ind1 = ind2;
            ind2 = pIndices[index];
            
            if ( ind0 != ind1 && ind1 != ind2 && ind2 != ind0 )
            {
                if ( i & 1 )
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
        else
        {
            // Unsupported primitive type.
            assert(0);
            return;
        }

        D3DXVECTOR3 edge01;
        D3DXVECTOR3 edge02;
        D3DXVECTOR3 cp;

        FLOAT ds1 = pVert1->texcoord.x - pVert0->texcoord.x;
        FLOAT dt1 = pVert1->texcoord.y - pVert0->texcoord.y;

        FLOAT ds2 = pVert2->texcoord.x - pVert0->texcoord.x;
        FLOAT dt2 = pVert2->texcoord.y - pVert0->texcoord.y;

        // x, s, t
        edge01 = D3DXVECTOR3( pVert1->pos.x - pVert0->pos.x, ds1, dt1 );
        edge02 = D3DXVECTOR3( pVert2->pos.x - pVert0->pos.x, ds2, dt2 );

        D3DXVec3Cross( &cp, &edge01, &edge02 );
        if ( fabs(cp.x) > SMALL_FLOAT )
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
        if ( fabs(cp.x) > SMALL_FLOAT )
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
        if ( fabs(cp.x) > SMALL_FLOAT )
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

    pMesh->m_IB.Unlock();

    // Paramerters that define if verts are shared.
    const FLOAT fDistMax = 1e-6f;
    const FLOAT fNormAngMin = 0.939693f;  // 20 degrees
    const FLOAT fTanAngMin = 0.939693f;   // 20 degrees

    BOOL* bSharingRejected = new BOOL[pMesh->m_dwNumVertices];

    assert( bSharingRejected );

    memset( bSharingRejected, 0, sizeof(BOOL) * pMesh->m_dwNumVertices );

    // Any verts that share position and normal, but don't share texture
    // co-ordinates should be treated as shared.
    for ( DWORD i = 0; i < pMesh->m_dwNumVertices; i++ )
    {
        if (bSharingRejected[i])
            continue;
            
        D3DXVECTOR3 vS = pBasis[i].S;
        D3DXVECTOR3 vT = pBasis[i].T;

        for ( DWORD j = i+1; j < pMesh->m_dwNumVertices; j++ )
        {
            // Are position and normal shared?
            D3DXVECTOR3 vDelta = pVertices[i].pos - pVertices[j].pos;
            FLOAT fDist = D3DXVec3Length( &vDelta );
            FLOAT fCosA = D3DXVec3Dot( &pVertices[i].normal, 
                                       &pVertices[j].normal );

            // Less than 1 / 1000000 distance, and 20 degrees.
            if ( fDist < fDistMax && fCosA > fNormAngMin )
            {
                // Average S and T vectors for i and j.
                vS += pBasis[j].S;
                vT += pBasis[j].T;
            }
        }

        // Do a check of the existing basis vectors vs. the newly calculated 
        // ones to make sure we really should average the textures spaces.
        // For example at the poles of a sphere we do not want to average the
        // texture spaces.
        BOOL bUseAverage = true;

        D3DXVECTOR3 vSavg, vTavg;
        D3DXVec3Normalize( &vSavg, &vS );
        D3DXVec3Normalize( &vTavg, &vT );

        D3DXVECTOR3 vSi, vTi;
        D3DXVec3Normalize( &vSi, &pBasis[i].S );
        D3DXVec3Normalize( &vTi, &pBasis[i].T );

        FLOAT fDotS = D3DXVec3Dot( &vSi, &vSavg );
        FLOAT fDotT = D3DXVec3Dot( &vTi, &vTavg );

        if (fDotS <= fTanAngMin || fDotT <= fTanAngMin)
        {
            bUseAverage = false;
        }

        for ( DWORD j = i+1; j < pMesh->m_dwNumVertices; j++ )
        {
            // Are position and normal shared?
            D3DXVECTOR3 vDelta = pVertices[i].pos - pVertices[j].pos;
            FLOAT fDist = D3DXVec3Length( &vDelta );
            FLOAT fCosA = D3DXVec3Dot( &pVertices[i].normal, 
                                       &pVertices[j].normal );

            // Less than 1 / 1000000 distance, and 20 degrees.
            if ( fDist < fDistMax && fCosA > fNormAngMin )
            {
                D3DXVECTOR3 vSj, vTj;
                D3DXVec3Normalize( &vSj, &pBasis[j].S );
                D3DXVec3Normalize( &vTj, &pBasis[j].T );

                fDotS = D3DXVec3Dot( &vSj, &vSavg );
                fDotT = D3DXVec3Dot( &vTj, &vTavg );

                if (fDotS <= fTanAngMin || fDotT <= fTanAngMin)
                {
                    bUseAverage = false;
                    break;
                }
            }
        }

        if( bUseAverage )
        {
            // Set the basis vectors for all the verts to the average.
            for ( DWORD j = i+1; j < pMesh->m_dwNumVertices; j++ )
            {
                // Are position and normal shared?
                D3DXVECTOR3 vDelta = pVertices[i].pos - pVertices[j].pos;
                FLOAT fDist = D3DXVec3Length( &vDelta );
                FLOAT fCosA = D3DXVec3Dot( &pVertices[i].normal, 
                                           &pVertices[j].normal );

                // Less than 1 / 1000000 distance, and 10 degrees.
                if ( fDist < fDistMax && fCosA > fNormAngMin )
                {
                    pBasis[j].S = vS;
                    pBasis[j].T = vT;
                }
            }

            pBasis[i].S = vS;
            pBasis[i].T = vT;
        }
        else
        {
            // Prevent the vertices from being considered again.
            for ( DWORD j = i+1; j < pMesh->m_dwNumVertices; j++ )
            {
                // Are position and normal shared?
                D3DXVECTOR3 vDelta = pVertices[i].pos - pVertices[j].pos;
                FLOAT fDist = D3DXVec3Length( &vDelta );
                FLOAT fCosA = D3DXVec3Dot( &pVertices[i].normal, 
                                           &pVertices[j].normal );

                // Less than 1 / 1000000 distance, and 10 degrees.
                if ( fDist < fDistMax && fCosA > fNormAngMin )
                {
                    bSharingRejected[j] = true;
                }
            }

            bSharingRejected[i] = true;
        }
    }

    delete[] bSharingRejected;

    // Calculate the SxT vector
    for ( DWORD i = 0; i < pMesh->m_dwNumVertices; i++ )
    {
        // Normalize the S, T vectors
        D3DXVec3Normalize( &pBasis[i].S, &pBasis[i].S );
        D3DXVec3Normalize( &pBasis[i].T, &pBasis[i].T );

        // Get the cross of the S and T vectors
        D3DXVec3Cross( &pBasis[i].SxT, &pBasis[i].S, &pBasis[i].T );

        D3DXVec3Normalize( &pBasis[i].SxT, &pBasis[i].SxT );

        // Fix the direction of the SxT vector
        if ( D3DXVec3Dot( &pBasis[i].SxT, &pVertices[i].normal ) < 0.0f )
            pBasis[i].SxT = -pBasis[i].SxT;
    }

    pMesh->m_VB.Unlock();
    (*pBasisVB)->Unlock();
}
