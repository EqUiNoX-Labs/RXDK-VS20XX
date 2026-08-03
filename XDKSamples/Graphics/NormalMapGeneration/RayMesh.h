//-----------------------------------------------------------------------------
// File: RayMesh.h
//
// Desc: Subclass of CXBMesh that supports ray triangle intersection testing.
//
// Hist: 08.05.02 - New
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#ifndef RAYMESH_H
#define RAYMESH_H

#include <xtl.h>
#include <XBMesh.h>



//-----------------------------------------------------------------------------
// Name: class NormalMesh
// Desc: A subclass of CXBMesh that support calculating normal maps from a high
//       resolution model for the mesh.
//-----------------------------------------------------------------------------
class RayMesh : public CXBMesh
{
public:
    RayMesh();
    ~RayMesh();

    // Mark the vertices of the mesh as cacheable by the CPU. If any vertices 
    // are modified by the CPU it will be necessary to flush  the CPU cache. 
    // Caching the vertices improves the performance of the below routines.
    void CacheVertices();

    // Compute number of intersections of a ray with the mesh.
    // Return the position and normal at the nearest intersection point.
    bool RayIntersection( const D3DXVECTOR3& vOrigin, const D3DXVECTOR3& vDir, 
                          float* pfDist, D3DXVECTOR3* pvPos, D3DXVECTOR3* pvNorm );

    // Calculation spatial subdivision information for the mesh in order
    // to accelerate ray intersection tests.
    void CalculateSpatialSubdivision();

private:
    // Internal functions to mark the mesh vertices as cacheable.
    void CacheFrameVertices( XBMESH_FRAME* pFrame );
    void CacheMeshVertices( XBMESH_DATA* pMesh );

    //
    // kDOPTree that is used to spatially subdivide the triangles of the mesh.
    //
    struct kDOPTree
    {
        // 3 = 6-DOP (AABB), 7 = 14-DOP, 13 = 26-DOP
        static const int iNumPlanes = 3;
        static const D3DXVECTOR3 PlaneNorms[13];
        static const int iMaxTrisPerLeaf = 8;

        struct kDOPTri
        {
            WORD v1, v2, v3;
        };

        struct kDOP
        {
            float fMin[iNumPlanes];
            float fMax[iNumPlanes];
        };

        struct kDOPNode
        {
            bool bLeaf;

            kDOP Volume;

            union
            {
                struct
                {
                    kDOPNode* pLeft;
                    kDOPNode* pRight;
                };

                struct
                {
                    int iNumTris;
                    kDOPTri* pTriList;
                };
            };
        };

        kDOPNode* pNodes;         // Storage for nodes.
        kDOPTri* pTriLists;       // Storage for triangle lists.

        struct kDOPQuery
        {
            // Ray.
            const D3DXVECTOR3* pvOrigin;
            const D3DXVECTOR3* pvDir;

            // Cached dot products.
            float fDotProdOrigin[iNumPlanes];
            float fInvDotProdDir[iNumPlanes];
            bool  bParallel[iNumPlanes];

            // Mesh vertices.
            BYTE* pVertices;
            DWORD dwVertexSize;

            // Intersection info.
            float fDist; 
            D3DXVECTOR3 vPos;
            D3DXVECTOR3 vNorm;
        };

        struct kDOPBuildInfo
        {
            // Number of nodes allocated.
            int iNumNodes;
            int iMaxNodes;

            // Number of triangles allocated.
            int iNumTris;
            int iMaxTris;

            // Mesh vertices.
            BYTE* pVertices;
            DWORD dwVertexSize;
        };

        struct kDOPBuildTri
        {
            kDOPTri Tri;
            D3DXVECTOR3 vCentroid;
        };

        static void AddPointTokDOP( kDOP* pDOP, const D3DXVECTOR3& vPos );

        // Create a new node that splits the tri list.
        kDOPNode* SplitTriList( kDOPBuildInfo* pInfo, int iNumTris, kDOPBuildTri* pTriList );

        static bool RaykDOPIntersection( const kDOP* pDOP, const kDOPQuery* pQuery, float* pT );

        // Find ray intersection.
        bool RayIntersection( kDOPNode* pNode, kDOPQuery* pQuery );
    };

    // Internal functions to find intersections of a ray with the mesh.
    bool FrameRayIntersection( XBMESH_FRAME* pFrame, 
                               D3DXMATRIX* pParentMat, 
                               const D3DXVECTOR3& vOrigin, 
                               const D3DXVECTOR3& vDir, float *pfDist, 
                               D3DXVECTOR3* pvPos, D3DXVECTOR3* pvNorm );

    bool MeshRayIntersection( XBMESH_DATA* pMesh, kDOPTree* pMeshTree,
                              D3DXMATRIX* pMat, const D3DXVECTOR3& vOrigin, 
                              const D3DXVECTOR3& vDir, float *pfDist, 
                              D3DXVECTOR3* pvPos, D3DXVECTOR3* pvNorm );

    // Internal functions to compute spatial subdivisions for the mesh.
    void CalculateFrameSpatialSubdivision( XBMESH_FRAME* pFrame );

    void CalculateMeshSpatialSubdivision( XBMESH_DATA* pMesh, 
                                          kDOPTree* pMeshTree );

    // kd-trees (one per frame) that subdivide the traingles of the mesh.   
    kDOPTree* m_pMeshTree;
};



#endif
