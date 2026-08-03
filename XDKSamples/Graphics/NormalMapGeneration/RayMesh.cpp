//-----------------------------------------------------------------------------
// File: RayMesh.cpp
//
// Desc: Subclass of CXBMesh that supports ray triangle intersection testing.
//
// Hist: 08.05.02 - New
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#include "RayMesh.h"




//-----------------------------------------------------------------------------
// Name: MeshVertex
// Desc: Layout of the position and normal within each mesh vertex.
//-----------------------------------------------------------------------------
struct MeshVertex
{
    D3DXVECTOR3 Pos;
    D3DXVECTOR3 Norm;
};



//-----------------------------------------------------------------------------
// Name: Swap
// Desc: Simple templated swap.
//-----------------------------------------------------------------------------
template <class T>
void Swap(T& a, T& b)
{
    T temp = a;
    a = b;
    b = temp;
}



//-----------------------------------------------------------------------------
// Name: RayMesh()
// Desc: Constructor.
//-----------------------------------------------------------------------------
RayMesh::RayMesh()
{
    m_pMeshTree = NULL;
}



//-----------------------------------------------------------------------------
// Name: ~RayMesh()
// Desc: Destructor, free extra data added to mesh.
//-----------------------------------------------------------------------------
RayMesh::~RayMesh()
{
    if (m_pMeshTree)
    {
        // Free buffers.
        for (DWORD frame = 0; frame < m_dwNumFrames; frame++)
        {
            // Free buffers.
            delete[] m_pMeshTree[frame].pNodes;
            delete[] m_pMeshTree[frame].pTriLists;
        }
    }

    delete[] m_pMeshTree;
}



//-----------------------------------------------------------------------------
// Name: CacheMeshVertices()
// Desc: Call CacheFrameVertices for the base frame.
//-----------------------------------------------------------------------------
void RayMesh::CacheVertices()
{
    CacheFrameVertices( m_pMeshFrames );
}



//-----------------------------------------------------------------------------
// Name: CacheFrameVertices()
// Desc: Recursively calls CacheMeshVertices for each frame.
//-----------------------------------------------------------------------------
void RayMesh::CacheFrameVertices( XBMESH_FRAME* pFrame )
{
    // Mark mesh vertices as cacheable.
    if( pFrame->m_MeshData.m_dwNumSubsets ) 
        CacheMeshVertices( &pFrame->m_MeshData );

    // Compute bounds for any child frames
    if( pFrame->m_pChild ) 
    {
        CacheFrameVertices( pFrame->m_pChild  );
    }

    // Compute bounds for any sibling frames
    if( pFrame->m_pNext )  
    {
        CacheFrameVertices( pFrame->m_pNext );
    }
}



//-----------------------------------------------------------------------------
// Name: CacheMeshVertices()
// Desc: Mark the vertex buffer for the mesh data as cacheable, read-only.
//-----------------------------------------------------------------------------
void RayMesh::CacheMeshVertices( XBMESH_DATA* pMesh )
{
    DWORD       dwNumVertices = pMesh->m_dwNumVertices;
    DWORD       dwVertexSize  = pMesh->m_dwVertexSize;
    BYTE*       pVertices;

    pMesh->m_VB.Lock( 0, 0, &pVertices, 0 );

    XPhysicalProtect(pVertices, dwNumVertices*dwVertexSize, PAGE_READONLY);

    pMesh->m_VB.Unlock();
}



//-----------------------------------------------------------------------------
// Name: RayTriangleIntersection
// Desc: Compute the intersecion of a ray (vOrigin, vDir) with a triangle 
//       (v1, v2, v3).  Return true if there is an interseciton and also
//       set t to the distance along the ray and u,v to the barycentric 
//       cooridinates of the intersection.  bCull controls if intersections 
//       with the back side of the triangle are returned.
//-----------------------------------------------------------------------------
bool RayTriangleIntersection( const D3DXVECTOR3& vOrigin, 
                              const D3DXVECTOR3& vDir, 
                              const D3DXVECTOR3& v1, const D3DXVECTOR3& v2, 
                              const D3DXVECTOR3& v3, bool bCull, float& t, 
                              float& u, float& v )
{
    // From "Real-Time Rendering" which references Moller, Tomas and Trumbore, 
    // "Fast, Minimum Storage Ray-Triangle Intersection", Journal of Graphics 
    // Tools, vol. 2, no. 1, pp 21-28, 1997. With some modifications to the 
    // non-culling case by Michael Mounier.

    const float epsilon = 1e-15f;

    D3DXVECTOR3 e1 = v2 - v1;
    D3DXVECTOR3 e2 = v3 - v1;

    //D3DXVECTOR3 p = vDir ^ e2;
    D3DXVECTOR3 p;
    D3DXVec3Cross( &p, &vDir, &e2 );

    //float det = e1 * p;
    float det = D3DXVec3Dot( &e1, &p );

    if (det >= epsilon)
    {
        // Determinate is positive.
        D3DXVECTOR3 s = vOrigin - v1;

        //u = s * p;
        u = D3DXVec3Dot( &s, &p );

        if (u < 0.0f || u > det)
            return false;

        //D3DXVECTOR3 q = s ^ e1;
        D3DXVECTOR3 q;
        D3DXVec3Cross( &q, &s, &e1 );

        //v = vDir * q;
        v = D3DXVec3Dot( &vDir, &q );

        if (v < 0.0f || u + v > det)
            return false;

        //t = e2 * q;
        t = D3DXVec3Dot( &e2, &q );

        if (t < 0.0f)
            return false;
    }
    else if (det <= -epsilon && !bCull)
    {
        // Determinate is negative.
        D3DXVECTOR3 s = vOrigin - v1;

        //u = s * p;
        u = D3DXVec3Dot( &s, &p );

        if (u > 0.0f || u < det)
            return false;

        //D3DXVECTOR3 q = s ^ e1;
        D3DXVECTOR3 q;
        D3DXVec3Cross( &q, &s, &e1 );

        //v = vDir * q;
        v = D3DXVec3Dot( &vDir, &q );

        if (v > 0.0f || u + v < det)
            return false;

        //t = e2 * q;
        t = D3DXVec3Dot( &e2, &q );

        if (t > 0.0f)
            return false;
    }
    else
    {
        // Parallel ray or culled.
        return false;
    }

    float inv_det = 1.0f / det;

    u *= inv_det;
    v *= inv_det;
    t *= inv_det;

    return true;
}



//-----------------------------------------------------------------------------
// Name: RayIntersection
// Desc: Check for ray intersection with base frame.
//-----------------------------------------------------------------------------
bool RayMesh::RayIntersection( const D3DXVECTOR3& vOrigin, 
                               const D3DXVECTOR3& vDir, 
                               float* pfDist, D3DXVECTOR3* pvPos, 
                               D3DXVECTOR3* pvNorm )
{
    D3DXMATRIX matIdentity;
    D3DXMatrixIdentity( &matIdentity );

    *pfDist = FLT_MAX;

    if (!m_pMeshTree)
        return false;

    return FrameRayIntersection( m_pMeshFrames, &matIdentity, vOrigin, vDir, 
                                 pfDist, pvPos, pvNorm );
}



//-----------------------------------------------------------------------------
// Name: FrameRayIntersection
// Desc: Recursively check for ray intersections with any sub-frames.
//-----------------------------------------------------------------------------
bool RayMesh::FrameRayIntersection( XBMESH_FRAME* pFrame, 
                                    D3DXMATRIX* pParentMat, 
                                    const D3DXVECTOR3& vOrigin, 
                                    const D3DXVECTOR3& vDir, 
                                    float *pfDist, D3DXVECTOR3* pvPos, 
                                    D3DXVECTOR3* pvNorm )
{
    // Apply the frame's local transform
    D3DXMATRIX matWorld;
    D3DXMatrixMultiply( &matWorld, &pFrame->m_matTransform, pParentMat );

    bool bIntersect = false;

    // Compute intersections for the mesh data
    if( pFrame->m_MeshData.m_dwNumSubsets )
    {
        DWORD index = pFrame - m_pMeshFrames;
        bIntersect |= MeshRayIntersection( &pFrame->m_MeshData, 
                                           &m_pMeshTree[index], &matWorld, 
                                           vOrigin, vDir, pfDist, pvPos, 
                                           pvNorm );
    }

    // Compute intersections for any child frames
    if( pFrame->m_pChild ) 
    {
        bIntersect |= FrameRayIntersection( pFrame->m_pChild, &matWorld, 
                                            vOrigin, vDir, pfDist, pvPos, 
                                            pvNorm );
    }

    // Compute intersections for any sibling frames
    if( pFrame->m_pNext )  
    {
        bIntersect |= FrameRayIntersection( pFrame->m_pNext, pParentMat, 
                                            vOrigin, vDir, pfDist, pvPos, 
                                            pvNorm );
    }

    return bIntersect;
}



//-----------------------------------------------------------------------------
// Name: MeshRayIntersection
// Desc: Check for ray intersection with the mesh data using a kDOP-tree.
//-----------------------------------------------------------------------------
bool RayMesh::MeshRayIntersection( XBMESH_DATA* pMesh, kDOPTree* pMeshTree,
                                   D3DXMATRIX* pMat, const D3DXVECTOR3& vOrigin, 
                                   const D3DXVECTOR3& vDir, float *pfDist, 
                                   D3DXVECTOR3* pvPos, D3DXVECTOR3* pvNorm )
{
    bool bIntersect = false;

    D3DXMATRIX matInverse;
    D3DXMatrixInverse( &matInverse, NULL, pMat );

    D3DXVECTOR3 vLocalOrigin;
    D3DXVec3TransformCoord( &vLocalOrigin, &vOrigin, &matInverse );

    D3DXVECTOR3 vLocalDir;
    D3DXVec3TransformNormal( &vLocalDir, &vDir, &matInverse );

    assert( pMesh->m_dwFVF & D3DFVF_NORMAL );

    // Setup query data.
    kDOPTree::kDOPQuery Query;

    Query.pvOrigin = &vLocalOrigin;
    Query.pvDir = &vLocalDir;

    for ( int k = 0; k < kDOPTree::iNumPlanes; k++ )
    {
        Query.fDotProdOrigin[k] = D3DXVec3Dot( &vLocalOrigin, &kDOPTree::PlaneNorms[k] );
        float fDotProdDir = D3DXVec3Dot( &vLocalDir, &kDOPTree::PlaneNorms[k] );
        Query.fInvDotProdDir[k] = 1.0f / fDotProdDir;
        Query.bParallel[k] = fabs(fDotProdDir) < 1e-30f;
    }

    pMesh->m_VB.Lock( 0, 0, &Query.pVertices, D3DLOCK_READONLY );
    Query.dwVertexSize = pMesh->m_dwVertexSize;

    Query.fDist = FLT_MAX;

    // Find the nearest intersection.
    float dummy;
    if ( pMeshTree->RaykDOPIntersection( &pMeshTree->pNodes[0].Volume, &Query, &dummy ) )
    {
        if ( pMeshTree->RayIntersection( pMeshTree->pNodes, &Query ) )
        {
            // Keep closest intersection.
            if ( Query.fDist < *pfDist )
            {
                *pfDist = Query.fDist;
                *pvPos = Query.vPos;
                *pvNorm = Query.vNorm;

                // Transform to world space.
                D3DXVec3TransformCoord( pvPos, pvPos, pMat );
                D3DXVec3TransformNormal( pvNorm, pvNorm, pMat );
            }

            bIntersect = true;
        }
    }

    pMesh->m_VB.Unlock();

    return bIntersect;
}



//-----------------------------------------------------------------------------
// Name: CalculateSpatialSubdivision
// Desc: Calculate a kDOP-tree for the base frame.
//-----------------------------------------------------------------------------
void RayMesh::CalculateSpatialSubdivision()
{
    if (!m_pMeshTree)
    {
        m_pMeshTree = new kDOPTree[m_dwNumFrames];

        if (!m_pMeshTree)
            return;

        memset(m_pMeshTree, 0, sizeof(kDOPTree) * m_dwNumFrames);

        if( m_pMeshFrames )
            CalculateFrameSpatialSubdivision( m_pMeshFrames );
    }
}



//-----------------------------------------------------------------------------
// Name: CalculateFrameSpatialSubdivision
// Desc: Recursively calculate a kDOP-tree for any sub-frames.
//-----------------------------------------------------------------------------
void RayMesh::CalculateFrameSpatialSubdivision( XBMESH_FRAME* pFrame )
{
    // Mark mesh vertices as cacheable.
    if( pFrame->m_MeshData.m_dwNumSubsets )
    {
        DWORD index = pFrame - m_pMeshFrames;
        CalculateMeshSpatialSubdivision( &pFrame->m_MeshData, &m_pMeshTree[index] );
    }

    // Compute bounds for any child frames
    if( pFrame->m_pChild ) 
    {
        CalculateFrameSpatialSubdivision( pFrame->m_pChild  );
    }

    // Compute bounds for any sibling frames
    if( pFrame->m_pNext )  
    {
        CalculateFrameSpatialSubdivision( pFrame->m_pNext );
    }
}



//-----------------------------------------------------------------------------
// Name: CalculateMeshSpatialSubdivision
// Desc: Calculate a kDOP-tree for the mesh data.
//-----------------------------------------------------------------------------
void RayMesh::CalculateMeshSpatialSubdivision( XBMESH_DATA* pMesh, 
                                               kDOPTree* pMeshTree )
{
    DWORD            dwNumSubsets  =  pMesh->m_dwNumSubsets;
    XBMESH_SUBSET*   pSubsets      = &pMesh->m_pSubsets[0];
    D3DPRIMITIVETYPE dwPrimType    =  pMesh->m_dwPrimType;

    assert( pMesh->m_dwFVF & D3DFVF_NORMAL );

    WORD* pIndices;
    pMesh->m_IB.Lock( 0, 0, (BYTE**)&pIndices, D3DLOCK_READONLY );

    int iNumTris = 0;

    // Count the number of triangles.
    for( DWORD i = 0; i < dwNumSubsets; i++ )
    {
        WORD wOffset = (WORD)pSubsets[i].dwIndexStart;

        if ( D3DPT_TRIANGLESTRIP == dwPrimType )
        {
            // Unstrip the indices.
            WORD ind0 = 0;
            WORD ind1 = pIndices[0+wOffset];
            WORD ind2 = pIndices[1+wOffset];

            for ( DWORD src = 2; src < pSubsets[i].dwIndexCount; src++ )
            {
                ind0 = ind1;
                ind1 = ind2;
                ind2 = pIndices[src+wOffset];
                
                if (ind0 != ind1 && ind1 != ind2 && ind2 != ind0)
                {
                    iNumTris++;
                }
            }
        }
        else if ( D3DPT_TRIANGLELIST == dwPrimType )
        {
            iNumTris += pSubsets[i].dwIndexCount/3;
        }
    }

    kDOPTree::kDOPBuildTri* pTriList = new kDOPTree::kDOPBuildTri[iNumTris];
    iNumTris = 0;

    // Build the triangle list.
    for( DWORD i = 0; i < dwNumSubsets; i++ )
    {
        WORD wOffset = (WORD)pSubsets[i].dwIndexStart;

        if ( D3DPT_TRIANGLESTRIP == dwPrimType )
        {
            // Unstrip the indices.
            WORD ind0 = 0;
            WORD ind1 = pIndices[0+wOffset];
            WORD ind2 = pIndices[1+wOffset];

            for ( DWORD src = 2; src < pSubsets[i].dwIndexCount; src++ )
            {
                ind0 = ind1;
                ind1 = ind2;
                ind2 = pIndices[src+wOffset];
                
                if (ind0 != ind1 && ind1 != ind2 && ind2 != ind0)
                {
                    if (src & 1)
                    {
                        pTriList[iNumTris].Tri.v1 = ind2;
                        pTriList[iNumTris].Tri.v2 = ind1;
                        pTriList[iNumTris].Tri.v3 = ind0;
                    }
                    else
                    {
                        pTriList[iNumTris].Tri.v1 = ind0;
                        pTriList[iNumTris].Tri.v2 = ind1;
                        pTriList[iNumTris].Tri.v3 = ind2;
                    }

                    iNumTris++;
                }
            }
        }
        else if ( D3DPT_TRIANGLELIST == dwPrimType )
        {
            DWORD dwNumTriangles = pSubsets[i].dwIndexCount/3;

            for ( DWORD tri = 0; tri < dwNumTriangles; tri++ )
            {
                pTriList[iNumTris].Tri.v1 = pIndices[tri*3+0+wOffset];
                pTriList[iNumTris].Tri.v2 = pIndices[tri*3+1+wOffset];
                pTriList[iNumTris].Tri.v3 = pIndices[tri*3+2+wOffset];
                iNumTris++;
            }
        }
    }

    pMesh->m_IB.Unlock();

    // Setup build info.
    kDOPTree::kDOPBuildInfo Info;

    Info.iNumNodes = 0;
    Info.iMaxNodes = iNumTris*4;
    Info.iNumTris = 0;
    Info.iMaxTris = iNumTris;

    pMesh->m_VB.Lock( 0, 0, &Info.pVertices, D3DLOCK_READONLY );
    Info.dwVertexSize = pMesh->m_dwVertexSize;

    // Compute triangle centriods.
    for (int i = 0; i < iNumTris; i++)
    {
        MeshVertex *pV1, *pV2, *pV3;
        
        pV1 = (MeshVertex*)(Info.pVertices + pTriList[i].Tri.v1 * 
                                             Info.dwVertexSize);

        pV2 = (MeshVertex*)(Info.pVertices + pTriList[i].Tri.v2 * 
                                             Info.dwVertexSize);

        pV3 = (MeshVertex*)(Info.pVertices + pTriList[i].Tri.v3 * 
                                             Info.dwVertexSize);

        pTriList[i].vCentroid = (pV1->Pos + pV2->Pos + pV3->Pos) * (1.0f/3.0f);
    }


    // Initialize the tree.
    pMeshTree->pNodes = new kDOPTree::kDOPNode[Info.iMaxNodes];
    pMeshTree->pTriLists = new kDOPTree::kDOPTri[Info.iMaxTris];

    // Build the tree.
    pMeshTree->SplitTriList( &Info, iNumTris, pTriList );

    delete[] pTriList;

    pMesh->m_VB.Unlock();
}



//-----------------------------------------------------------------------------
// Plane normals for the kDOP-tree.
//-----------------------------------------------------------------------------
const D3DXVECTOR3 RayMesh::kDOPTree::PlaneNorms[] =
{
    // Axes. Slabs for a 6-DOP.
    D3DXVECTOR3( 1.0f, 0.0f, 0.0f ),
    D3DXVECTOR3( 0.0f, 1.0f, 0.0f ),
    D3DXVECTOR3( 0.0f, 0.0f, 1.0f ),

    // Corners. Additional slabs for a 14-DOP.
    D3DXVECTOR3( 0.57735f,  0.57735f,  0.57735f ),
    D3DXVECTOR3( 0.57735f, -0.57735f,  0.57735f ),
    D3DXVECTOR3( 0.57735f,  0.57735f, -0.57735f ),
    D3DXVECTOR3( 0.57735f, -0.57735f, -0.57735f ),

    // Edges. Additional slabs for a 26-DOP.
    D3DXVECTOR3( 0.707107f,  0.707107f,  0.0f      ),
    D3DXVECTOR3( 0.707107f,  0.0f,       0.707107f ),
    D3DXVECTOR3( 0.0f,       0.707107f,  0.707107f ),
    D3DXVECTOR3( 0.707107f, -0.707107f,  0.0f      ),
    D3DXVECTOR3( 0.707107f,  0.0f,      -0.707107f ),
    D3DXVECTOR3( 0.0f,       0.707107f, -0.707107f ),
};



//-----------------------------------------------------------------------------
// Name: 
// Desc: 
//-----------------------------------------------------------------------------
void RayMesh::kDOPTree::AddPointTokDOP( kDOP* pDOP, const D3DXVECTOR3& vPos )
{
    for ( int k = 0; k < iNumPlanes; k++ )
    {
        float dot = D3DXVec3Dot( &vPos, &PlaneNorms[k] );

        if ( dot < pDOP->fMin[k] )
            pDOP->fMin[k] = dot;

        if ( dot > pDOP->fMax[k] )
            pDOP->fMax[k] = dot;
    }
}



//-----------------------------------------------------------------------------
// Name: kDOPTree::SplitTriList()
// Desc: Recursively subdiviced a triangle list to build a kDOP-tree.
//-----------------------------------------------------------------------------
RayMesh::kDOPTree::kDOPNode* RayMesh::kDOPTree::SplitTriList( 
                                                        kDOPBuildInfo* pInfo, 
                                                        int iNumTris, 
                                                        kDOPBuildTri* pTriList )
{
    // New node.
    kDOPNode* pNode = pNodes + pInfo->iNumNodes++;
    
    assert(pInfo->iNumNodes < pInfo->iMaxNodes);

    // Compute the bounding volume for this node.
    for ( int k = 0; k < iNumPlanes; k++ )
    {
        pNode->Volume.fMin[k] = FLT_MAX;
        pNode->Volume.fMax[k] = -FLT_MAX;
    }

    for (int i = 0; i < iNumTris; i++)
    {
        MeshVertex *pV1, *pV2, *pV3;
        
        pV1 = (MeshVertex*)(pInfo->pVertices + pTriList[i].Tri.v1 * 
                                               pInfo->dwVertexSize);

        pV2 = (MeshVertex*)(pInfo->pVertices + pTriList[i].Tri.v2 * 
                                               pInfo->dwVertexSize);

        pV3 = (MeshVertex*)(pInfo->pVertices + pTriList[i].Tri.v3 * 
                                               pInfo->dwVertexSize);

        AddPointTokDOP( &pNode->Volume, pV1->Pos );
        AddPointTokDOP( &pNode->Volume, pV2->Pos );
        AddPointTokDOP( &pNode->Volume, pV3->Pos );
    }

    if ( iNumTris <= iMaxTrisPerLeaf )
    {
        // Make aleaf node.
        pNode->bLeaf = true;

        pNode->iNumTris = iNumTris;
        pNode->pTriList = pTriLists + pInfo->iNumTris;
        pInfo->iNumTris += iNumTris;

        assert(pInfo->iNumTris <= pInfo->iMaxTris);

        for ( int i = 0; i < iNumTris; i++ )
            pNode->pTriList[i] = pTriList[i].Tri;
    }
    else
    {
        // Non leaf node.
        pNode->bLeaf = false;

        int iBestPlane = -1;
        float fBestMean = 0.0f;
        float fBestVariance = 0.0f;

        // Determine how to split using the splatter algorithm.
        for ( int k = 0; k < iNumPlanes; k++ )
        {
            float fMean = 0.0f;

            // Compute mean.
            for ( int i = 0; i < iNumTris; i++ )
            {
                fMean += D3DXVec3Dot( &pTriList[i].vCentroid, &PlaneNorms[k] );
            }

            fMean /= float(iNumTris);

            float fVariance = 0.0f;

            // Compute variance.
            for ( int i = 0; i < iNumTris; i++ )
            {
                float dot = D3DXVec3Dot( &pTriList[i].vCentroid, &PlaneNorms[k] );
                fVariance += (dot - fMean) * (dot - fMean);
            }

            fVariance /= float(iNumTris);

            // Is this the best plane?
            if ( fVariance >= fBestVariance )
            {
                iBestPlane = k;
                fBestVariance = fVariance;
                fBestMean = fMean;
            }
        }

        assert( iBestPlane != -1 );

        // Partiton at the median.
        int iLeft = -1;
        int iRight = iNumTris;
        for(;;)
        {
            float dot;

            do
                dot = D3DXVec3Dot( &pTriList[++iLeft].vCentroid, &PlaneNorms[iBestPlane] );
            while (dot < fBestMean);

            do
                dot = D3DXVec3Dot( &pTriList[--iRight].vCentroid, &PlaneNorms[iBestPlane] );
            while (dot >= fBestMean && iRight > 0);

            if (iLeft >= iRight)
                break;

            Swap( pTriList[iLeft], pTriList[iRight] );
        }

        assert( iLeft > 0 );

        pNode->pLeft = SplitTriList( pInfo, iLeft, pTriList );
        pNode->pRight = SplitTriList( pInfo, iNumTris-iLeft, pTriList+iLeft );
    }

    return pNode;
}



//-----------------------------------------------------------------------------
// Name: kDOPTree::RaykDOPIntersection
// Desc: Check for instersection of a ray with a kDOP.
//-----------------------------------------------------------------------------
bool RayMesh::kDOPTree::RaykDOPIntersection( const kDOP* pDOP, 
                                             const kDOPQuery* pQuery, 
                                             float* pT )
{
    float t_min = -FLT_MAX;
    float t_max = FLT_MAX;

#if (_XBOX)

    const float fZero = 0.0f;

    __asm
    {
        movss   xmm2,[t_min]
        movss   xmm3,[t_max]

        mov     esi,[pDOP]
        mov     ebx,[pQuery]

        mov     ecx,0
        mov     edx,[iNumPlanes]

        ALIGN   16

kLoop:
        // Loop for each plane.
        movss   xmm0,[esi+ecx*4]kDOP.fMin
        movss   xmm1,[esi+ecx*4]kDOP.fMax

        mov     al,[ebx+ecx]kDOPQuery.bParallel
        test    al,al
        jz      NonParallelRay

        // Parallel to the slab.
        movss   xmm6,[ebx+ecx*4]kDOPQuery.fDotProdOrigin

        comiss  xmm6,xmm0
        jb      EXIT_FALSE

        comiss  xmm6,xmm1
        ja      EXIT_FALSE

        jmp     EndOfLoop

        ALIGN   16

NonParallelRay:
        movss   xmm6,[ebx+ecx*4]kDOPQuery.fDotProdOrigin

        subss   xmm0,xmm6
        subss   xmm1,xmm6

        movss   xmm5,[ebx+ecx*4]kDOPQuery.fInvDotProdDir

        mulss   xmm0,xmm5       // t1
        mulss   xmm1,xmm5       // t2
        
        movss   xmm4,xmm0
        minss   xmm0,xmm1       // min (t1,t2)
        maxss   xmm1,xmm4       // max (t1,t2)
        
        maxss   xmm2,xmm0
        minss   xmm3,xmm1

        comiss  xmm2,xmm3
        ja      EXIT_FALSE

        comiss  xmm3,[fZero]
        jb      EXIT_FALSE

EndOfLoop:
        inc     ecx
        cmp     ecx,edx
        jl      kLoop

        mov     eax,[pT]
        movss   [eax],xmm2
    }

    return true;

EXIT_FALSE:
    return false;

#else

    for ( int k = 0; k < iNumPlanes; k++ )
    {
        if ( !pQuery->bParallel[k] )
        {
            // Ray intersectons with the slab (k).
            float t1 = (pDOP->fMin[k] - pQuery->fDotProdOrigin[k]) * pQuery->fInvDotProdDir[k];
            float t2 = (pDOP->fMax[k] - pQuery->fDotProdOrigin[k]) * pQuery->fInvDotProdDir[k];

            // Compute the max of min(t1,t2) and the min of max(t1,t2)
            if ( t1 > t2 ) Swap(t1, t2);
            if ( t1 > t_min ) t_min = t1;
            if ( t2 < t_max ) t_max = t2;

            if ( t_min > t_max ) return false;
            if ( t_max < 0.0f ) return false;
        }
        else
        {
            // Parallel to the slab.
            if ( pQuery->fDotProdOrigin[k] < pDOP->fMin[k] || pQuery->fDotProdOrigin[k] > pDOP->fMax[k] )
                return false;
        }
    }

    *pT = t_min;

    return true;

#endif
}



//-----------------------------------------------------------------------------
// Name: kDOPTree::RayIntersection()
// Desc: Find ray intersection using a kDOP-tree.
//-----------------------------------------------------------------------------
bool RayMesh::kDOPTree::RayIntersection( kDOPNode* pNode, kDOPQuery* pQuery )
{
    // Leaf node.
    bool bIntersection = false;

    if ( pNode->bLeaf )
    {
        // Check triangles in the leaf.
        for (int i = 0; i < pNode->iNumTris; i++)
        {
            MeshVertex* pV1 = (MeshVertex*)(pQuery->pVertices + 
                                            pNode->pTriList[i].v1 * 
                                            pQuery->dwVertexSize);

            MeshVertex* pV2 = (MeshVertex*)(pQuery->pVertices + 
                                            pNode->pTriList[i].v2 * 
                                            pQuery->dwVertexSize);

            MeshVertex* pV3 = (MeshVertex*)(pQuery->pVertices + 
                                            pNode->pTriList[i].v3 * 
                                            pQuery->dwVertexSize);

            float t, u ,v;

            if (RayTriangleIntersection( *pQuery->pvOrigin, *pQuery->pvDir, 
                                         pV1->Pos, pV2->Pos, pV3->Pos,
                                         false, t, u, v ) )
            {
                if ( t < pQuery->fDist )
                {
                    pQuery->fDist = t;

                    // Make sure the point is inside the triangle.
                    assert( u >= 0.0f );
                    assert( v >= 0.0f );
                    assert( u+v < 1.0f );

                    pQuery->vPos = (1.0f - u - v) * pV1->Pos + 
                                   u * pV2->Pos + v * pV3->Pos;

                    pQuery->vNorm = (1.0f - u - v) * pV1->Norm + 
                                    u * pV2->Norm + v * pV3->Norm;
                }

                bIntersection = true;
            }

        }
    }
    else
    {
        // Interior node.
        float t1, t2;
        kDOPNode *pNode1 = 0;
        kDOPNode *pNode2 = 0;

        if ( RaykDOPIntersection( &pNode->pLeft->Volume, pQuery, &t1 ) )
        {
            pNode1 = pNode->pLeft;
        }

        if ( RaykDOPIntersection( &pNode->pRight->Volume, pQuery, &t2 ) )
        {
            pNode2 = pNode->pRight;
        }

        // Order nearest to fartherst.
        if ( pNode1 && pNode2 && t2 < t1 )
        {
            Swap( pNode1, pNode2 );
            Swap( t1, t2 );
        }

        if ( pNode1 )
        {
            // Stop if we already have a closer intersection.
            if ( pQuery->fDist <= t1 )
                return bIntersection;

            bIntersection = RayIntersection( pNode1, pQuery );
        }

        if ( pNode2 )
        {
            // Stop if we already have a closer intersection.
            if ( pQuery->fDist <= t2 )
                return bIntersection;

            bIntersection |= RayIntersection( pNode2, pQuery );
        }

    }

    return bIntersection;
}
