//-----------------------------------------------------------------------------
// File: ShadowMesh.cpp
//
// Desc: Support code for building and rendering shadow volumes. Shadow volumes
//       can be built either on the CPU or on the GPU, and can be rendered
//       using either a one-pass or two-pass technique.
//
// Perf: In the two-pass case, the geometry must be transformed twice, but the
//       rasterization cost is slightly less. Since rendering shadow volumes
//       tends to be fill-bound (in other words, the transform cost is masked
//       by the fill cost), the two-pass case usually works out to be faster.
//
// Hist: 06.17.03 - Revised for robustness and performance.
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <xtl.h>
#include <XBMesh.h>
#include "ShadowMesh.h"




//-----------------------------------------------------------------------------
// Name: BuildShadowMesh()
// Desc: Collapses any vertices that share a postion value into a single 
//       vertex. Builds the face and edge list for the shadow mesh.
//-----------------------------------------------------------------------------
static HRESULT BuildShadowMesh( DWORD            dwNumFaces,    // Number of faces.
                                const WORD*      pMeshIndices,  // Input triangle indices (3*dwNumFaces).
                                const BYTE*      pMeshVertices, // Input vertices.
                                DWORD            dwVertexSize,  // Size of each vertex in bytes.
                                DWORD&           dwTempCount,   // Number of output vertices.
                                D3DXVECTOR3*     pTempVerts,    // Output vertices (up to # of input verts).
                                DWORD&           dwNumEdges,    // Number of output edges.
                                SHADOWMESH_EDGE* pEdges,        // Output edges (up to 3*dwNumFaces).
                                SHADOWMESH_FACE* pFaces )       // Output faces (dwNumFaces).
{
    // Start with zero output verts and edges.
    dwTempCount = 0;
    dwNumEdges  = 0;
    
    // Build triangle and edge lists.
    for( DWORD face = 0; face < dwNumFaces; face++ )
    {
        // Add unique vertex positions.
        for( DWORD i = 0; i < 3; i++ )
        {
            WORD wSrcIndex = pMeshIndices[face*3+i];

            const D3DXVECTOR3& vPos = *(D3DXVECTOR3*)(pMeshVertices + wSrcIndex * dwVertexSize);
            
            // Check if we already have this position.  
            WORD wDstIndex = 0;
            while( wDstIndex < dwTempCount )
            {
                // Check for exact match of position.
                if( vPos == pTempVerts[wDstIndex] )
                    break;

                wDstIndex++;
            }

            if( wDstIndex == dwTempCount )
            {
                // Add vertex.
                pTempVerts[wDstIndex] = vPos;
                dwTempCount++;
            }

            pFaces[face].V[i] = wDstIndex;
        }

        const D3DXVECTOR3& vert0 = pTempVerts[pFaces[face].V[0]];
        const D3DXVECTOR3& vert1 = pTempVerts[pFaces[face].V[1]];
        const D3DXVECTOR3& vert2 = pTempVerts[pFaces[face].V[2]];
    
        // Compute face plane.
        D3DXVECTOR3 vNormal;
        D3DXVec3Cross( &vNormal, &(vert2-vert1), &(vert1-vert0) );
        D3DXVec3Normalize( &vNormal, &vNormal );
        D3DXPlaneFromPointNormal( &pFaces[face].Plane, &vert0, &vNormal );

        // Add edges.
        DWORD i0 = 2;
        for( DWORD i1 = 0; i1 < 3; i1++ )
        {
            WORD index0 = pFaces[face].V[i0];
            WORD index1 = pFaces[face].V[i1];

            // Check if we already have this edge.
            DWORD edge = 0;
            while( edge < dwNumEdges )
            {
                // Does this edge exist from an adjacent triangle.
                // Note: winding order from the adjacent triangle is reversed.
                if( pEdges[edge].V[0] == index1 && pEdges[edge].V[1] == index0 )
                    break;

                edge++;
            }

            if( edge < dwNumEdges )
            {
                // If the mesh is manifold, only two triangles should ever
                // share an edge.
                if( pEdges[edge].Face1 != 0xffffffff )
                    return E_FAIL;

                // Attach to existing edge.
                pEdges[edge].Face1 = face;
            }
            else
            {
                // Add new edge.
                pEdges[edge].V[0]  = index0;
                pEdges[edge].V[1]  = index1;
                pEdges[edge].Face0 = face;
                pEdges[edge].Face1 = 0xffffffff;
                dwNumEdges++;
            }

            i0 = i1;
        }
    }

    // Make sure the mesh in closed (2-manifold).
    for( DWORD edge = 0; edge < dwNumEdges; edge++ )
    {
        // If the mesh is manifold, all edges should be shared by two triangles.
        if( pEdges[edge].Face0 == 0xffffffff || pEdges[edge].Face1 == 0xffffffff )
            return E_FAIL;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: ShadowMeshCPU()
// Desc: Constructor - Initializes the extra data to NULL.
//-----------------------------------------------------------------------------
ShadowMeshCPU::ShadowMeshCPU()
{
    m_pMesh           = NULL;
    m_pMeshShadowData = NULL;
}




//-----------------------------------------------------------------------------
// Name: ~ShadowMeshCPU()
// Desc: Destructor - Frees the additional data if it has been allocated.
//-----------------------------------------------------------------------------
ShadowMeshCPU::~ShadowMeshCPU()
{
    if( m_pMeshShadowData )
    {
        // Free buffers.
        for( DWORD frame = 0; frame < m_pMesh->m_dwNumFrames; frame++ )
        {
            // Free buffers.
            if( m_pMeshShadowData[frame].m_pVB )
                m_pMeshShadowData[frame].m_pVB->Release();

            delete[] m_pMeshShadowData[frame].m_pQuadIndices;
            delete[] m_pMeshShadowData[frame].m_pFaces;
            delete[] m_pMeshShadowData[frame].m_pEdges;
            delete[] m_pMeshShadowData[frame].m_pFrontFacing;
            delete[] m_pMeshShadowData[frame].m_pTriIndices;
            delete[] m_pMeshShadowData[frame].m_pQuadIndices;
        }
    }

    delete[] m_pMeshShadowData; 
}




//-----------------------------------------------------------------------------
// Name: Create()
// Desc: Allocate space and compute the shadow mesh data from the base mesh.
//-----------------------------------------------------------------------------
HRESULT ShadowMeshCPU::Create( CXBMesh* pMesh )
{
    if( NULL == pMesh )
        return E_INVALIDARG;

    m_pMesh = pMesh;

    m_pMeshShadowData = new XBMESH_CPUSHADOW_DATA[m_pMesh->m_dwNumFrames];
    if( NULL == m_pMeshShadowData )
        return E_OUTOFMEMORY;

    ZeroMemory( m_pMeshShadowData, sizeof(XBMESH_CPUSHADOW_DATA) * m_pMesh->m_dwNumFrames );

    if( m_pMesh->m_pMeshFrames )
        return CalculateFrameShadowData( m_pMesh->m_pMeshFrames );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: CalculateFrameShadowData()
// Desc: Recursively calculate shadow mesh data for each frame. 
//-----------------------------------------------------------------------------
HRESULT ShadowMeshCPU::CalculateFrameShadowData( XBMESH_FRAME* pFrame )
{
    HRESULT hr; 

    // Process the mesh data
    if( pFrame->m_MeshData.m_dwNumSubsets ) 
    {
        DWORD index = pFrame - m_pMesh->m_pMeshFrames;
        if( FAILED( hr = CalculateMeshShadowData( &pFrame->m_MeshData, 
                                                  &m_pMeshShadowData[index] ) ) )
            return hr;
    }

    // Process any child frames
    if( pFrame->m_pChild ) 
        if( FAILED( hr = CalculateFrameShadowData( pFrame->m_pChild ) ) )
            return hr;
    
    // Process any sibling frames
    if( pFrame->m_pNext )  
        if( FAILED( hr = CalculateFrameShadowData( pFrame->m_pNext ) ) )
            return hr;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: CalculateMeshShadowData()
// Desc: Calculate shadow mesh data for the actual indices and vertices.
//-----------------------------------------------------------------------------
HRESULT ShadowMeshCPU::CalculateMeshShadowData( XBMESH_DATA* pMesh, 
                                                XBMESH_CPUSHADOW_DATA* pShadowData )
{
    HRESULT hr;

    // We only deal with triangle lists for simplicity.
    if( pMesh->m_dwPrimType != D3DPT_TRIANGLELIST )
        return E_INVALIDARG;

    // Lock index and vertex buffers.
    BYTE* pMeshVertices;
    WORD* pMeshIndices;

    pMesh->m_VB.Lock( 0L, 0L, &pMeshVertices, 0L );
    pMesh->m_IB.Lock( 0L, 0L, (BYTE**)&pMeshIndices, 0L );

    DWORD dwVertexSize = pMesh->m_dwVertexSize;

    // Create mesh shadow data.
    DWORD dwTempCount = 0;
    D3DXVECTOR3* pTempVerts = new D3DXVECTOR3[pMesh->m_dwNumVertices];
    if( NULL == pTempVerts )
        return E_OUTOFMEMORY;

    DWORD dwNumFaces = pMesh->m_dwNumIndices/3;
    DWORD dwNumEdges = 0;

    SHADOWMESH_FACE* pFaces = new SHADOWMESH_FACE[dwNumFaces];
    SHADOWMESH_EDGE* pEdges = new SHADOWMESH_EDGE[dwNumFaces*3];
    if( NULL == pFaces || NULL == pEdges )
        return E_OUTOFMEMORY;

    // Build the data from the shadow mesh.
    if( FAILED( hr = BuildShadowMesh( dwNumFaces, pMeshIndices, 
                                      pMeshVertices, dwVertexSize, 
                                      dwTempCount, pTempVerts, 
                                      dwNumEdges, pEdges, pFaces ) ) )
        return hr;

    // Keep face data.
    pShadowData->m_pFaces = pFaces;
    pShadowData->m_dwNumFaces = dwNumFaces;

    // Re-allocate and copy edge data.
    pShadowData->m_dwNumEdges = dwNumEdges;
    pShadowData->m_pEdges     = new SHADOWMESH_EDGE[dwNumEdges];
    if( NULL == pShadowData->m_pEdges )
        return E_OUTOFMEMORY;

    memcpy( pShadowData->m_pEdges, pEdges, sizeof(SHADOWMESH_EDGE)*pShadowData->m_dwNumEdges );

    delete[] pEdges;

    pShadowData->m_dwNumVertices = dwTempCount*2;

    assert( pShadowData->m_dwNumVertices < 65536 );

    // Create vertex buffer with offset and non-offset versions of each vert.
    hr = D3DDevice_CreateVertexBuffer( dwTempCount * 2 * sizeof(D3DXVECTOR4),
                                       D3DUSAGE_WRITEONLY, 0,
                                       D3DPOOL_DEFAULT, &pShadowData->m_pVB );
    if( FAILED(hr) )
        return hr;

    D3DXVECTOR4* pShadowVertices;
    pShadowData->m_pVB->Lock( 0L, 0L, (BYTE**)&pShadowVertices, 0L );

    for( DWORD i = 0; i < dwTempCount; i++ )
    {
        // Non-offset vert.
        pShadowVertices[i].x = pTempVerts[i].x;
        pShadowVertices[i].y = pTempVerts[i].y;
        pShadowVertices[i].z = pTempVerts[i].z;
        pShadowVertices[i].w = 0.0f;
    }

    for( DWORD i = dwTempCount; i < dwTempCount*2; i++ )
    {
        // Offset vert.
        pShadowVertices[i].x = pTempVerts[i-dwTempCount].x;
        pShadowVertices[i].y = pTempVerts[i-dwTempCount].y;
        pShadowVertices[i].z = pTempVerts[i-dwTempCount].z;
        pShadowVertices[i].w = 1.0f;
    }

    pShadowData->m_pVB->Unlock();

    // Free temp vertices.
    delete[] pTempVerts;

    // Unlock buffers.
    pMesh->m_VB.Unlock();
    pMesh->m_IB.Unlock();

    // Allocate temp data for drawing the mesh.
    pShadowData->m_pFrontFacing = new BOOL[pShadowData->m_dwNumFaces];
    pShadowData->m_pTriIndices  = new WORD[pShadowData->m_dwNumFaces*3];
    pShadowData->m_pQuadIndices = new WORD[pShadowData->m_dwNumEdges*4];

    if( NULL == pShadowData->m_pFrontFacing )
        return E_OUTOFMEMORY;
    if( NULL == pShadowData->m_pTriIndices )
        return E_OUTOFMEMORY;
    if( NULL == pShadowData->m_pQuadIndices )
        return E_OUTOFMEMORY;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: ComputeVolume()
// Desc: Compute the shadow volume for a given light position.
//-----------------------------------------------------------------------------
VOID ShadowMeshCPU::ComputeVolume( const D3DXVECTOR4& vLightPos )
{
    if( m_pMesh->m_pMeshFrames )
        ComputeVolumeFrame( m_pMesh->m_pMeshFrames, vLightPos );
}




//-----------------------------------------------------------------------------
// Name: ComputeVolumeFrame()
// Desc: Recursively compute the shadow volume for each frame.
//-----------------------------------------------------------------------------
VOID ShadowMeshCPU::ComputeVolumeFrame( XBMESH_FRAME* pFrame,
                                        const D3DXVECTOR4& vLightPos  )
{
    // Apply the frame's local transform
    D3DXMATRIX matSavedWorld, matWorld;
    D3DDevice::GetTransform( D3DTS_WORLD, &matSavedWorld );
    D3DXMatrixMultiply( &matWorld, &pFrame->m_matTransform, &matSavedWorld );
    D3DDevice::SetTransform( D3DTS_WORLD, &matWorld );

    // Compute shadow volume for this mesh
    if( pFrame->m_MeshData.m_dwNumSubsets )
    {
        DWORD index = pFrame - m_pMesh->m_pMeshFrames;
        ComputeVolumeMesh( &pFrame->m_MeshData, &m_pMeshShadowData[index], vLightPos );
    }

    // Compute shadow volume for any child frames
    if( pFrame->m_pChild ) 
        ComputeVolumeFrame( pFrame->m_pChild, vLightPos );

    // Restore the transformation matrix
    D3DDevice::SetTransform( D3DTS_WORLD, &matSavedWorld );
    
    // Compute shadow volume for any sibling frames
    if( pFrame->m_pNext )  
        ComputeVolumeFrame( pFrame->m_pNext, vLightPos );
}




//-----------------------------------------------------------------------------
// Name: ComputeVolumeMesh()
// Desc: Compute the shadow volume for the actual indices and vertices.
//-----------------------------------------------------------------------------
VOID ShadowMeshCPU::ComputeVolumeMesh( XBMESH_DATA* pMesh, 
                                       XBMESH_CPUSHADOW_DATA* pShadowData, 
                                       const D3DXVECTOR4& vLightPos )
{
    // Get the world transform.
    D3DXMATRIX matWorld;
    D3DDevice::GetTransform( D3DTS_WORLD, &matWorld );

    // Transform the light vector to be in object space
    D3DXVECTOR4 vLocalLightPos;
    D3DXMATRIX matInverse;
    D3DXMatrixInverse( &matInverse, NULL, &matWorld );
    D3DXVec4Transform( &vLocalLightPos, &vLightPos, &matInverse );

    // The second half of the vertices are offset.
    WORD wOffset = (WORD)(pShadowData->m_dwNumVertices/2);

    DWORD dwNumTriIndices  = 0;
    DWORD dwNumQuadIndices = 0;

    BOOL* bFrontFacing = pShadowData->m_pFrontFacing;
    WORD* pTriIndices  = pShadowData->m_pTriIndices;
    WORD* pQuadIndices = pShadowData->m_pQuadIndices;

    SHADOWMESH_FACE* pFaces = pShadowData->m_pFaces;

    // Determine if the light is in front of or behind each face and
    // add the faces to the draw list.
    for( DWORD face = 0; face < pShadowData->m_dwNumFaces; face++ )
    {
        if( D3DXPlaneDot( &pFaces[face].Plane, &vLocalLightPos ) > 0.0f )
        {
            bFrontFacing[face] = TRUE;

            // Add tri.
            pTriIndices[dwNumTriIndices++] = pFaces[face].V[0];
            pTriIndices[dwNumTriIndices++] = pFaces[face].V[1];
            pTriIndices[dwNumTriIndices++] = pFaces[face].V[2];
        }
        else
        {
            bFrontFacing[face] = FALSE;

            // Add tri with offset.
            pTriIndices[dwNumTriIndices++] = pFaces[face].V[0] + wOffset;
            pTriIndices[dwNumTriIndices++] = pFaces[face].V[1] + wOffset;
            pTriIndices[dwNumTriIndices++] = pFaces[face].V[2] + wOffset;
        }
    }

    SHADOWMESH_EDGE* pEdges = pShadowData->m_pEdges;

    // Determine which edges are potential silhouette edges.
    for( DWORD edge = 0; edge < pShadowData->m_dwNumEdges; edge++ )
    {
        if( bFrontFacing[pEdges[edge].Face0] ^ bFrontFacing[pEdges[edge].Face1] )
        {
            // Potential silhouette edge.
            if( bFrontFacing[pEdges[edge].Face1] )
            {
                // Add quad, reverse edge order since it matches Face0.
                pQuadIndices[dwNumQuadIndices++] = pEdges[edge].V[0];
                pQuadIndices[dwNumQuadIndices++] = pEdges[edge].V[1];
                pQuadIndices[dwNumQuadIndices++] = pEdges[edge].V[1] + wOffset;
                pQuadIndices[dwNumQuadIndices++] = pEdges[edge].V[0] + wOffset;
            }
            else
            {
                // Add quad, edge order matches Face0.
                pQuadIndices[dwNumQuadIndices++] = pEdges[edge].V[1];
                pQuadIndices[dwNumQuadIndices++] = pEdges[edge].V[0];
                pQuadIndices[dwNumQuadIndices++] = pEdges[edge].V[0] + wOffset;
                pQuadIndices[dwNumQuadIndices++] = pEdges[edge].V[1] + wOffset;
            }
        }
    }

    // Save the current data.
    pShadowData->m_dwNumTriIndices  = dwNumTriIndices;
    pShadowData->m_dwNumQuadIndices = dwNumQuadIndices;
}




//-----------------------------------------------------------------------------
// Name: RenderVolume()
// Desc: Render the shadow volume for a given light position using the 
//       previously computed volume.
//-----------------------------------------------------------------------------
VOID ShadowMeshCPU::RenderVolume( const D3DXVECTOR4& vLightPos )
{
    if( m_pMesh->m_pMeshFrames )
        RenderVolumeFrame( m_pMesh->m_pMeshFrames, vLightPos );
}




//-----------------------------------------------------------------------------
// Name: RenderVolumeFrame()
// Desc: Recursively render the shadow volume for each frame.
//-----------------------------------------------------------------------------
VOID ShadowMeshCPU::RenderVolumeFrame( XBMESH_FRAME* pFrame,
                                       const D3DXVECTOR4& vLightPos  )
{
    // Apply the frame's local transform
    D3DXMATRIX matSavedWorld, matWorld;
    D3DDevice::GetTransform( D3DTS_WORLD, &matSavedWorld );
    D3DXMatrixMultiply( &matWorld, &pFrame->m_matTransform, &matSavedWorld );
    D3DDevice::SetTransform( D3DTS_WORLD, &matWorld );

    // Render the mesh data
    if( pFrame->m_MeshData.m_dwNumSubsets )
    {
        DWORD index = pFrame - m_pMesh->m_pMeshFrames;
        RenderVolumeMesh( &pFrame->m_MeshData, 
                          &m_pMeshShadowData[index], vLightPos );
    }

    // Render any child frames
    if( pFrame->m_pChild ) 
        RenderVolumeFrame( pFrame->m_pChild, vLightPos );

    // Restore the transformation matrix
    D3DDevice::SetTransform( D3DTS_WORLD, &matSavedWorld );
    
    // Render any sibling frames
    if( pFrame->m_pNext )  
        RenderVolumeFrame( pFrame->m_pNext, vLightPos );
}




//-----------------------------------------------------------------------------
// Name: RenderVolumeMesh()
// Desc: Render the shadow volume for the actual indices and vertices.
//-----------------------------------------------------------------------------
VOID ShadowMeshCPU::RenderVolumeMesh( XBMESH_DATA* pMesh, 
                                      XBMESH_CPUSHADOW_DATA* pShadowData, 
                                      const D3DXVECTOR4& vLightPos )
{
    // Get the current transforms.
    D3DXMATRIX matWorld, matView, matProjectionViewport;
    D3DDevice::GetTransform( D3DTS_WORLD, &matWorld );
    D3DDevice::GetTransform( D3DTS_VIEW, &matView );
    D3DDevice::GetProjectionViewportMatrix( &matProjectionViewport );

    // Transform the light vector to be in object space
    D3DXVECTOR4 vLocalLightPos;
    D3DXMATRIX matInverse;
    D3DXMatrixInverse( &matInverse, NULL, &matWorld );
    D3DXVec4Transform( &vLocalLightPos, &vLightPos, &matInverse );

    // Compute composite matrix.
    D3DXMATRIX matComposite;
    D3DXMatrixMultiply( &matComposite, &matWorld, &matView );
    D3DXMatrixMultiply( &matComposite, &matComposite, &matProjectionViewport );

    D3DXMatrixTranspose( &matComposite, &matComposite );
    D3DDevice::SetVertexShaderConstant( 0, &matComposite, 4 );
    D3DDevice::SetVertexShaderConstant( 4, &vLocalLightPos, 1 );

    // The distance the volume is protruded.
    static FLOAT fScaleFactors[4] = { 100.0f, 0.0f, 0.0f , 0.0f };
    D3DDevice::SetVertexShaderConstant( 5, fScaleFactors, 1 );

    // Set viewport offsets.
    static FLOAT fViewportOffsets[4] = { 0.53125f, 0.53125f, 0.0f, 0.0f };
    D3DDevice::SetVertexShaderConstant( 95, fViewportOffsets, 1 );

    D3DDevice::SetStreamSource( 0, pShadowData->m_pVB, sizeof(D3DXVECTOR4) );

    // Draw the shadow volume.
    D3DDevice::DrawIndexedVertices( D3DPT_QUADLIST, 
                                    pShadowData->m_dwNumQuadIndices, 
                                    pShadowData->m_pQuadIndices );
                                     
    D3DDevice::DrawIndexedVertices( D3DPT_TRIANGLELIST, 
                                    pShadowData->m_dwNumTriIndices, 
                                    pShadowData->m_pTriIndices );
}




//-----------------------------------------------------------------------------
// Name: ShadowMeshGPU()
// Desc: Constructor - Initializes the extra data to NULL.
//-----------------------------------------------------------------------------
ShadowMeshGPU::ShadowMeshGPU()
{
    m_pMesh           = NULL;
    m_pMeshShadowData = NULL;
}




//-----------------------------------------------------------------------------
// Name: ~ShadowMeshGPU()
// Desc: Destructor - Frees the additional data if it has been allocated.
//-----------------------------------------------------------------------------
ShadowMeshGPU::~ShadowMeshGPU()
{
    if( m_pMeshShadowData )
    {
        for( DWORD frame = 0; frame < m_pMesh->m_dwNumFrames; frame++ )
        {
            // Free buffers.
            if( m_pMeshShadowData[frame].m_pVB )
                m_pMeshShadowData[frame].m_pVB->Release();

            delete[] m_pMeshShadowData[frame].m_pQuadIndices;
        }
    }

    delete[] m_pMeshShadowData; 
}




//-----------------------------------------------------------------------------
// Name: Create()
// Desc: Allocate space and compute the shadow mesh data from the base mesh.
//-----------------------------------------------------------------------------
HRESULT ShadowMeshGPU::Create( CXBMesh* pMesh )
{
    if( NULL == pMesh )
        return E_INVALIDARG;

    m_pMesh = pMesh;

    m_pMeshShadowData = new XBMESH_GPUSHADOW_DATA[m_pMesh->m_dwNumFrames];
    if( NULL == m_pMeshShadowData )
        return E_OUTOFMEMORY;

    ZeroMemory( m_pMeshShadowData, sizeof(XBMESH_GPUSHADOW_DATA) * m_pMesh->m_dwNumFrames );

    if( m_pMesh->m_pMeshFrames )
        return CalculateFrameShadowData( m_pMesh->m_pMeshFrames );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: CalculateFrameShadowData()
// Desc: Recursively calculate shadow mesh data for each frame. 
//-----------------------------------------------------------------------------
HRESULT ShadowMeshGPU::CalculateFrameShadowData( XBMESH_FRAME* pFrame )
{
    HRESULT hr;

    // Process the mesh data
    if( pFrame->m_MeshData.m_dwNumSubsets ) 
    {
        DWORD index = pFrame - m_pMesh->m_pMeshFrames;
        if( FAILED( hr = CalculateMeshShadowData( &pFrame->m_MeshData, 
                                                  &m_pMeshShadowData[index] ) ) )
            return hr;
    }

    // Process any child frames
    if( pFrame->m_pChild ) 
        if( FAILED( hr = CalculateFrameShadowData( pFrame->m_pChild ) ) )
            return hr;
    
    // Process any sibling frames
    if( pFrame->m_pNext )  
        if( FAILED( hr = CalculateFrameShadowData( pFrame->m_pNext ) ) )
            return hr;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: CalculateMeshShadowData()
// Desc: Calculate shadow mesh data for the actual indices and vertices.
//-----------------------------------------------------------------------------
HRESULT ShadowMeshGPU::CalculateMeshShadowData( XBMESH_DATA* pMesh, 
                                                XBMESH_GPUSHADOW_DATA* pShadowData )
{
    HRESULT hr;

    // We only deal with triangle lists for simplicity.
    if( pMesh->m_dwPrimType != D3DPT_TRIANGLELIST )
        return E_INVALIDARG;

    // Lock index and vertex buffers.
    BYTE* pMeshVertices;
    WORD* pMeshIndices;
    pMesh->m_VB.Lock( 0L, 0L, &pMeshVertices, 0L );
    pMesh->m_IB.Lock( 0L, 0L, (BYTE**)&pMeshIndices, 0L );

    DWORD dwVertexSize = pMesh->m_dwVertexSize;

    // Create mesh shadow data.
    DWORD dwTempCount = 0;
    D3DXVECTOR3* pTempVerts = new D3DXVECTOR3[pMesh->m_dwNumVertices];
    if( NULL == pTempVerts )
        return E_OUTOFMEMORY;

    DWORD dwNumFaces = pMesh->m_dwNumIndices/3;
    DWORD dwNumEdges = 0;

    SHADOWMESH_FACE* pFaces = new SHADOWMESH_FACE[dwNumFaces];
    SHADOWMESH_EDGE* pEdges = new SHADOWMESH_EDGE[dwNumFaces*3];
    if( NULL == pFaces || NULL == pEdges )
        return E_OUTOFMEMORY;

    // Build the data from the shadow mesh.
    if( FAILED( hr = BuildShadowMesh( dwNumFaces, pMeshIndices, 
                                      pMeshVertices, dwVertexSize, 
                                      dwTempCount, pTempVerts, 
                                      dwNumEdges, pEdges, pFaces ) ) )
        return hr;

    // Build a vertex buffer containing each vertex duplicated with the normal 
    // of each face it is shared by.
    pShadowData->m_dwNumVertices = dwNumFaces * 3;

    assert( pShadowData->m_dwNumVertices < 65536 );

    struct SHADOWVERTEX { D3DXVECTOR3 Pos; D3DXPLANE Plane; };

    // Create vertex buffer.
    hr = D3DDevice_CreateVertexBuffer( pShadowData->m_dwNumVertices * sizeof(SHADOWVERTEX),
                                       D3DUSAGE_WRITEONLY, 0,
                                       D3DPOOL_DEFAULT, &pShadowData->m_pVB );
    if( FAILED(hr) )
        return hr;

    SHADOWVERTEX* pShadowVertices;
    pShadowData->m_pVB->Lock( 0L, 0L, (BYTE**)&pShadowVertices, 0L );

    for( DWORD face = 0; face < dwNumFaces; face++ )
    {
        // Add triangle vertices with face normal.
        for( DWORD i = 0; i < 3; i++ )
        {
            pShadowVertices[face*3+i].Pos   = pTempVerts[pFaces[face].V[i]];
            pShadowVertices[face*3+i].Plane = pFaces[face].Plane;
        }
    }

    pShadowData->m_pVB->Unlock();

    // Free temp vertices.
    delete[] pTempVerts;

    // Build a quad-list for the edges.
    pShadowData->m_dwNumQuadIndices = 0;
    pShadowData->m_pQuadIndices     = new WORD[dwNumEdges*4];
    if( NULL == pShadowData->m_pQuadIndices )
        return E_OUTOFMEMORY;

    for( DWORD edge = 0; edge < dwNumEdges; edge++ )
    {
        WORD* pQuad = pShadowData->m_pQuadIndices + pShadowData->m_dwNumQuadIndices;

        // Each quad consists of the two vertices of the edge from one face
        // and the two vertices of the edge from the other face.
        WORD face0 = (WORD)pEdges[edge].Face0;
        WORD face1 = (WORD)pEdges[edge].Face1;

        // Which edge of Face0 does this edge correspond to?
        for( WORD i0 =2, i1 = 0; i1 < 3; i1++ )
        {
            if( pEdges[edge].V[0] == pFaces[face0].V[i0] && pEdges[edge].V[1] == pFaces[face0].V[i1] )
            {
                pQuad[0] = face0*3 + i1;
                pQuad[1] = face0*3 + i0;
                break;
            }
            
            i0 = i1;
        }

        // Which edge of Face1 does this edge correspond to?
        for( WORD i0 = 2, i1 = 0; i1 < 3; i1++ )
        {
            // Note that vert order in edge is opposite vert order in face.
            if( pEdges[edge].V[1] == pFaces[face1].V[i0] && pEdges[edge].V[0] == pFaces[face1].V[i1] )
            {
                pQuad[2] = face1*3 + i1;
                pQuad[3] = face1*3 + i0;
                break;
            }
            
            i0 = i1;
        }

        pShadowData->m_dwNumQuadIndices += 4;
    }

    // Discard face and edge data.
    delete[] pFaces;
    delete[] pEdges;

    // Unlock buffers.
    pMesh->m_VB.Unlock();
    pMesh->m_IB.Unlock();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: RenderVolume()
// Desc:  Render the shadow volume for a given light position using the GPU
//        to generate the volume.
//-----------------------------------------------------------------------------
VOID ShadowMeshGPU::RenderVolume( const D3DXVECTOR4& vLightPos )
{
    if( m_pMesh->m_pMeshFrames )
        RenderVolumeFrame( m_pMesh->m_pMeshFrames, vLightPos );
}




//-----------------------------------------------------------------------------
// Name: RenderVolumeFrame()
// Desc: Recursively render the shadow volume for each frame.
//-----------------------------------------------------------------------------
VOID ShadowMeshGPU::RenderVolumeFrame( XBMESH_FRAME* pFrame,
                                       const D3DXVECTOR4& vLightPos  )
{
    // Apply the frame's local transform
    D3DXMATRIX matSavedWorld, matWorld;
    D3DDevice::GetTransform( D3DTS_WORLD, &matSavedWorld );
    D3DXMatrixMultiply( &matWorld, &pFrame->m_matTransform, &matSavedWorld );
    D3DDevice::SetTransform( D3DTS_WORLD, &matWorld );

    // Render the mesh data
    if( pFrame->m_MeshData.m_dwNumSubsets )
    {
        DWORD index = pFrame - m_pMesh->m_pMeshFrames;
        RenderVolumeMesh( &pFrame->m_MeshData, 
                          &m_pMeshShadowData[index], vLightPos );
    }

    // Render any child frames
    if( pFrame->m_pChild ) 
        RenderVolumeFrame( pFrame->m_pChild, vLightPos );

    // Restore the transformation matrix
    D3DDevice::SetTransform( D3DTS_WORLD, &matSavedWorld );
    
    // Render any sibling frames
    if( pFrame->m_pNext )  
        RenderVolumeFrame( pFrame->m_pNext, vLightPos );
}




//-----------------------------------------------------------------------------
// Name: RenderVolumeMesh()
// Desc: Render the shadow volume for the actual indices and vertices.
//-----------------------------------------------------------------------------
VOID ShadowMeshGPU::RenderVolumeMesh( XBMESH_DATA* pMesh, 
                                      XBMESH_GPUSHADOW_DATA* pShadowData, 
                                      const D3DXVECTOR4& vLightPos )
{
    // Get the current transforms.
    D3DXMATRIX matWorld, matView, matProjectionViewport;
    D3DDevice::GetTransform( D3DTS_WORLD, &matWorld );
    D3DDevice::GetTransform( D3DTS_VIEW, &matView );
    D3DDevice::GetProjectionViewportMatrix( &matProjectionViewport );

    // Transform the light vector to be in object space
    D3DXVECTOR4 vLocalLightPos;
    D3DXMATRIX matInverse;
    D3DXMatrixInverse( &matInverse, NULL, &matWorld );
    D3DXVec4Transform( &vLocalLightPos, &vLightPos, &matInverse );

    // Compute composite matrix.
    D3DXMATRIX matComposite;
    D3DXMatrixMultiply( &matComposite, &matWorld, &matView );
    D3DXMatrixMultiply( &matComposite, &matComposite, &matProjectionViewport );

    D3DXMatrixTranspose( &matComposite, &matComposite );
    D3DDevice::SetVertexShaderConstant( 0, &matComposite, 4 );
    D3DDevice::SetVertexShaderConstant( 4, &vLocalLightPos, 1 );

    // The distance the volume is protruded.
    static FLOAT fScaleFactors[4] = { 100.0f, 0.0f, 0.0f , 0.0f };
    D3DDevice::SetVertexShaderConstant( 5, fScaleFactors, 1 );

    // Set viewport offsets.
    static FLOAT fViewportOffsets[4] = { 0.53125f, 0.53125f, 0.0f, 0.0f };
    D3DDevice::SetVertexShaderConstant( 95, fViewportOffsets, 1 );

    D3DDevice::SetStreamSource( 0, pShadowData->m_pVB, 7 * sizeof(FLOAT) );

    // Draw the shadow volume.
    D3DDevice::DrawIndexedVertices( D3DPT_QUADLIST, 
                                    pShadowData->m_dwNumQuadIndices, 
                                    pShadowData->m_pQuadIndices );

    D3DDevice::DrawVertices( D3DPT_TRIANGLELIST, 0, 
                             pShadowData->m_dwNumVertices );
}




