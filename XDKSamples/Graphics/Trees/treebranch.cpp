//-----------------------------------------------------------------------------
// File: TreeBranch.cpp
//
// Desc: TreeBranch represented as both geometry and as a set of slice textures.
//
// Hist: 11.11.02 - Cleaned up for December XDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <xtl.h>
#include <xgraphics.h>
#include <XBApp.h>
#include "TreeBranch.h"

extern LPDIRECT3DDEVICE8 g_pd3dDevice;




//-----------------------------------------------------------------------------
// Name: RenderCallback()
// Desc: Callback from XBMESH that allows us to set the vertex shader and
//       associated constants.
//-----------------------------------------------------------------------------
BOOL CTreeShaderMesh::RenderCallback( DWORD dwSubset, XBMESH_SUBSET* pSubset, DWORD dwFlags )
{
    g_pd3dDevice->SetVertexShader(m_dwVertexShader);
    D3DXMATRIX matProjection;
    g_pd3dDevice->GetTransform( D3DTS_PROJECTION, &matProjection );
    D3DXMATRIX matView;
    g_pd3dDevice->GetTransform( D3DTS_VIEW, &matView );
    D3DXMATRIX matWorld;
    g_pd3dDevice->GetTransform( D3DTS_WORLD, &matWorld );
    D3DXMATRIX mat = matWorld * matView * matProjection;
    D3DXMATRIX matTranspose;
    D3DXMatrixTranspose(&matTranspose, &mat);
    g_pd3dDevice->SetVertexShaderConstant( TREE_WORLD_VIEW_PROJECTION, &matTranspose, 4 );
    g_pd3dDevice->SetVertexShaderConstant( TREE_SHADOW_CENTER, &m_vShadowCenter, 1 );
    D3DXCOLOR colorOffset;
    D3DXColorModulate(&colorOffset, &m_colorOffset, (D3DXCOLOR *)&pSubset->mtrl.Diffuse);
    g_pd3dDevice->SetVertexShaderConstant( TREE_COLOR_OFFSET, &colorOffset, 1 );
    D3DXCOLOR colorScale;
    D3DXColorModulate(&colorScale, &m_colorScale, (D3DXCOLOR *)&pSubset->mtrl.Diffuse);
    g_pd3dDevice->SetVertexShaderConstant( TREE_COLOR_SCALE, &colorScale, 1 );
    return TRUE;
}




//-----------------------------------------------------------------------------
// Name: Create()
// Desc: Load mesh and create slice textures
//-----------------------------------------------------------------------------
HRESULT CTreeBranch::Create(CHAR *strName, CXBPackedResource *pResource)
{
    m_pMesh = NULL;
    m_pMesh = new CTreeShaderMesh;
    if (!m_pMesh)
        return E_OUTOFMEMORY;
    if( FAILED( m_pMesh->Create( strName, pResource )))
        return XBAPPERR_MEDIANOTFOUND;
#if 1
    // TODO: fix media instead of this
    if (m_pMesh->m_dwNumFrames == 1)
        if (m_pMesh->m_dwNumFrames == 1)
            if (m_pMesh->m_pMeshFrames->m_MeshData.m_dwNumSubsets == 2)
            {
                if (!strcmp(m_pMesh->m_pMeshFrames->m_MeshData.m_pSubsets[1].strTexture, "leaf2"))
                {
                    m_pMesh->m_pMeshFrames->m_MeshData.m_pSubsets[0].mtrl.Diffuse.a = 0.9999f;  // branches, too
                    m_pMesh->m_pMeshFrames->m_MeshData.m_pSubsets[1].mtrl.Diffuse.a = 0.9999f;  // leaves
                }
            }
            else
            {
                m_pMesh->m_pMeshFrames->m_MeshData.m_pSubsets[0].mtrl.Diffuse.a = 0.9999f;  // branches, too
            }
#endif
    m_pMesh->ComputeBoundingBox(&m_vMin, &m_vMax);
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Scale()
// Desc: Scale the mesh
//-----------------------------------------------------------------------------
HRESULT CTreeBranch::Scale(const D3DXVECTOR3 &vScale)
{
    D3DXMATRIX matScale;
    D3DXMatrixScaling(&matScale, vScale.x, vScale.y, vScale.z);
    for (UINT iFrame = 0; iFrame < m_pMesh->m_dwNumFrames; iFrame++)
    {
        XBMESH_FRAME *pFrame = &m_pMesh->m_pMeshFrames[iFrame];
        pFrame->m_matTransform = matScale * pFrame->m_matTransform;
    }
    return m_pMesh->ComputeBoundingBox(&m_vMin, &m_vMax);
}




//-----------------------------------------------------------------------------
// Name: Slice()
// Desc: Compute slice texture from tree branch geometry
//-----------------------------------------------------------------------------
HRESULT CTreeBranch::Slice()
{
    SliceInfo rSliceInfo[3] = 
    {
        { 8, 256, 256, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP, D3DFMT_DXT4,
          { 1.f, 0.f, 0.f }, { 0.f, 1.f, 0.f }, 0.4f, 0.6f, "BranchX" },
        { 8, 256, 256, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP, D3DFMT_DXT4,
          { 0.f, 1.f, 0.f }, { 1.f, 0.f, 0.f }, 0.4f, 0.6f, "BranchY" },
        { 8, 256, 256, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP, D3DFMT_DXT4,
          { 0.f, 0.f, 1.f }, { -1.f, 0.f, 0.f }, 0.4f, 0.6f, "BranchZ" },
    };
    return CSliceObject::Slice(3, rSliceInfo);
}




//-----------------------------------------------------------------------------
// Name: Begin()
// Desc: Prepare for drawing into slice textures or for drawing
//       the full geometry.
//-----------------------------------------------------------------------------
HRESULT CTreeBranch::Begin( BOOL bWorldCoords )
{
    HRESULT hr;
    hr = CSliceObject::Begin(bWorldCoords);
    if (FAILED(hr))
        return hr;
    return SetState();
}




//-----------------------------------------------------------------------------
// Name: SetState()
// Desc: Set render state for drawing into slice textures or for drawing
//       the full geometry.
//-----------------------------------------------------------------------------
HRESULT CTreeBranch::SetState()
{
    // Use global light
    DWORD xx = 0;
    g_pd3dDevice->LightEnable(0, TRUE);
    g_pd3dDevice->SetTextureStageState( xx, D3DTSS_COLOROP, D3DTOP_MODULATE );
    g_pd3dDevice->SetTextureStageState( xx, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    g_pd3dDevice->SetTextureStageState( xx, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
    g_pd3dDevice->SetTextureStageState( xx, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    g_pd3dDevice->SetTextureStageState( xx, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
    g_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    g_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE);
    g_pd3dDevice->SetRenderState( D3DRS_LIGHTING, TRUE);
    g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    g_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, TRUE);     // tree texture uses, effectively, a 1-bit alpha
    g_pd3dDevice->SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATER);
    static DWORD dwAlphaRef = 100;
    g_pd3dDevice->SetRenderState( D3DRS_ALPHAREF, dwAlphaRef );
    g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    return S_OK;
}
    



//-----------------------------------------------------------------------------
// Name: Draw()
// Desc: Draw all that intersects range
//-----------------------------------------------------------------------------
HRESULT CTreeBranch::Draw(const D3DXVECTOR3 &vMin, const D3DXVECTOR3 &vMax)
{
    if (m_pMesh == NULL)
        return E_FAIL;  // not initialized properly
    // draw the whole mesh every time (ignore bounding box)
    static DWORD dwFlags = XBMESH_ALPHAONLY | XBMESH_NOFVF;
    return m_pMesh->Render( dwFlags);
}




//-----------------------------------------------------------------------------
// Name: End()
// Desc: Cleanup
//-----------------------------------------------------------------------------
HRESULT CTreeBranch::End()
{
    return CSliceObject::End();
}




//-----------------------------------------------------------------------------
// Name: 
// Desc: Draw tree level-of-detail representation based on flags
//-----------------------------------------------------------------------------
HRESULT CTreeBranch::DrawLOD(const D3DXVECTOR3 &vFromFade, 
                             const D3DXVECTOR3 &vFromSliceOrder,
                             DWORD dwFlags)
{
    if (m_pMesh == NULL)
        return E_FAIL;  // not initialized properly
    if (dwFlags == 0)
        return CSliceObject::DrawSlices(vFromFade, vFromSliceOrder, 0);

    // draw the whole mesh every time (ignore bounding box)
    DWORD dwMeshFlags = XBMESH_NOFVF;
    if (dwFlags == TREEBRANCH_DRAWLIMBS)
        dwFlags |= XBMESH_OPAQUEONLY;
    else if (dwFlags == TREEBRANCH_DRAWLEAVES)
        dwFlags |= XBMESH_ALPHAONLY;
    // else dwFlags == TREEBRANCH_DRAWFULLGEOMETRY
    return m_pMesh->Render( dwMeshFlags);
}



