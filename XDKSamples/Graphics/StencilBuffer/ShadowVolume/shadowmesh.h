//-----------------------------------------------------------------------------
// File: ShadowMesh.h
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
#include <XBMesh.h>




//-----------------------------------------------------------------------------
// Name: struct SHADOWMESH_EDGE
// Desc: Info for each edge. The vertex order matches that of Face0.
//       Exactly two faces must be adjacent to an edge.
//-----------------------------------------------------------------------------
struct SHADOWMESH_EDGE
{
    WORD  V[2];             // Vert indices for the edge.
    DWORD Face0, Face1;     // Adjacent faces.
};




//-----------------------------------------------------------------------------
// Name: struct SHADOWMESH_FACE
// Desc: Info for each face.
//-----------------------------------------------------------------------------
struct SHADOWMESH_FACE
{
    WORD      V[3];         // Vert indices for the face.
    D3DXPLANE Plane;        // Plane of the face.
};




//-----------------------------------------------------------------------------
// Name: struct XBMESH_CPUSHADOW_DATA
// Desc: Struct with additional data for each mesh frame used to build the
//       shadow volume.
//-----------------------------------------------------------------------------
struct XBMESH_CPUSHADOW_DATA
{
    // Data needed to quickly build the shadow volume.
    DWORD                   m_dwNumFaces;
    SHADOWMESH_FACE*        m_pFaces;

    DWORD                   m_dwNumEdges;
    SHADOWMESH_EDGE*        m_pEdges;

    DWORD                   m_dwNumVertices;
    LPDIRECT3DVERTEXBUFFER8 m_pVB;

    // Temporary data used for storing the current shadow volume.
    BOOL*                   m_pFrontFacing;
    
    DWORD                   m_dwNumTriIndices;
    WORD*                   m_pTriIndices;
    
    DWORD                   m_dwNumQuadIndices;
    WORD*                   m_pQuadIndices;
};




//-----------------------------------------------------------------------------
// Name: struct XBMESH_GPUSHADOW_DATA
// Desc: Struct with additional data for each mesh frame used to build the
//       shadow volume.
//-----------------------------------------------------------------------------
struct XBMESH_GPUSHADOW_DATA
{
    DWORD                   m_dwNumVertices;
    LPDIRECT3DVERTEXBUFFER8 m_pVB;

    DWORD                   m_dwNumQuadIndices;
    WORD*                   m_pQuadIndices;
};




//-----------------------------------------------------------------------------
// Name: class ShadowMesh
// Desc: A subclass of CXBMesh that has extra data to quickly build shadow
//       volumes.
//-----------------------------------------------------------------------------
class ShadowMeshCPU
{
    // Pointer to the original mesh that casts this shadow
    CXBMesh* m_pMesh;

    // Additional data from each XBMESH_FRAME.
    // Basically a parallel array to m_pMeshFrames.
    XBMESH_CPUSHADOW_DATA* m_pMeshShadowData;  

    // Functions to calculate shadow data
    HRESULT CalculateFrameShadowData( XBMESH_FRAME* pFrame );

    HRESULT CalculateMeshShadowData( XBMESH_DATA* pMesh,
                                     XBMESH_CPUSHADOW_DATA* pShadowData );

    VOID    ComputeVolumeFrame( XBMESH_FRAME* pFrame,
                                const D3DXVECTOR4& vLightPos  );

    VOID    ComputeVolumeMesh( XBMESH_DATA* pMesh, 
                               XBMESH_CPUSHADOW_DATA* pShadowData, 
                               const D3DXVECTOR4& vLightPos );

    // Internal rendering functions
    VOID    RenderVolumeFrame( XBMESH_FRAME* pFrame,
                               const D3DXVECTOR4& vLightPos  );

    VOID    RenderVolumeMesh( XBMESH_DATA* pMesh, 
                              XBMESH_CPUSHADOW_DATA* pShadowData, 
                              const D3DXVECTOR4& vLightPos );

public:
    // Build the data needed to quickly compute the shadow volume.
    HRESULT Create( CXBMesh* pMesh );

    // Compute the shadow volume for the mesh.
    VOID    ComputeVolume( const D3DXVECTOR4& vLightPos );

    // Renders the shadow volume for the mesh.
    VOID    RenderVolume( const D3DXVECTOR4& vLightPos );

    ShadowMeshCPU();
    ~ShadowMeshCPU();
};




//-----------------------------------------------------------------------------
// Name: class ShadowMeshGPU
// Desc: A subclass of CXBMesh that has extra data to render shadow volumes
//       entirely using the GPU.
//-----------------------------------------------------------------------------
class ShadowMeshGPU
{
    // Pointer to the original mesh that casts this shadow
    CXBMesh* m_pMesh;

    // Additional data from each XBMESH_FRAME.
    // Basically a parallel array to m_pMeshFrames.
    XBMESH_GPUSHADOW_DATA* m_pMeshShadowData;

    // Functions to calculate shadow data
    HRESULT CalculateFrameShadowData( XBMESH_FRAME* pFrame );

    HRESULT CalculateMeshShadowData( XBMESH_DATA* pMesh,
                                     XBMESH_GPUSHADOW_DATA* pShadowData );

    // Internal rendering functions
    VOID    RenderVolumeFrame( XBMESH_FRAME* pFrame,
                               const D3DXVECTOR4& vLightPos  );

    VOID    RenderVolumeMesh( XBMESH_DATA* pMesh, 
                              XBMESH_GPUSHADOW_DATA* pShadowData, 
                              const D3DXVECTOR4& vLightPos );

public:
    // Build the data needed to compute the shadow volume on the GPU.
    HRESULT Create( CXBMesh* pMesh );

    // Renders the shadow volume for the mesh.
    VOID    RenderVolume( const D3DXVECTOR4& vLightPos );

    ShadowMeshGPU();
    ~ShadowMeshGPU();
};




