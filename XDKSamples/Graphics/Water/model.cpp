//-----------------------------------------------------------------------------
// File: Model.cpp
//
// Desc: Contains the model and model list. The model list is used to manage
//       all the static scene models and render them.
//
//       Eeach model has a pointer to its vertex buffer and index buffer
//       (shared with other models in CPackageManager) with the start
//       position and size.
//   
//       When the model needs texture and effect for rendering, it can find the 
//       resource in CResourceManager (g_pApp->m_pResMan)
//
// Hist: 11.14.00 - Created
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "waterdefs.h"
#include "waterapp.h"
#include "packman.h"
#include "model.h"




//-----------------------------------------------------------------------------
// Name: CModel()
// Desc: Constructor
//-----------------------------------------------------------------------------
CModel::CModel()
{
    m_dwRenderFlag      = 0;
    m_dwVSH             = 0;
    m_dwPSH             = 0;
    m_pEffect           = NULL;
    m_pVB               = NULL;
    m_pIB               = NULL;
    m_dwBaseVertexIndex = 0;
    m_dwNumVertices     = 0;
    m_dwStartIndex      = 0;
    m_dwPrimitiveCount  = 0;

    for( INT i = 0; i < c_nMaxTextureStage; i++ )
    {
        m_rgTexPair[i].dwFCCID = 0;
        m_rgTexPair[i].tex = NULL;
    }
}




//-----------------------------------------------------------------------------
// Name: ~CModel()
// Desc: 
//-----------------------------------------------------------------------------
CModel::~CModel()
{
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: 
//-----------------------------------------------------------------------------
HRESULT CModel::Render( DWORD dwRenderFlag )
{
    HRESULT hr = S_OK;
    
    // Check render flag. One model will be rendered only if its flag 
    // matches the Renderflag (NORMAL, ABOVE_WATER, BELOW_WATER)
    if ( !( dwRenderFlag & m_dwRenderFlag ) )
        return hr;
    
    // Set matrix, buffers and shader
    ICullFrustum* pCullFrustumObject = g_pApp->GetCullFrustumObject();
    D3DXMATRIX mat = *pCullFrustumObject->GetViewProjMultiMatrix();

    D3DXMatrixTranspose( &mat, &mat );
    m_pEffect->SetMatrix( FCC_MTOT,  &mat );
    g_pd3dDevice->SetStreamSource( 0, m_pVB, sizeof(CPackageManager::TVertex) );
    g_pd3dDevice->SetIndices( m_pIB, m_dwBaseVertexIndex );
    g_pd3dDevice->SetPixelShader( m_dwPSH );
    
    //Set texture
    for( INT i = 0; i < c_nMaxTextureStage; i++ )
    {
        DWORD par = m_rgTexPair[i].dwFCCID;
        if ( par == 0 )
            break;
        m_pEffect->SetTexture( par, m_rgTexPair[i].tex );
    }

    UINT uPasses = 1;
    LPD3DXTECHNIQUE pTechnique;
    if ( FAILED( hr = m_pEffect->GetTechnique( 0, &pTechnique ) ) )
        return hr;
    pTechnique->Begin( &uPasses );

    for( DWORD i = 0; i < uPasses; i++ )
    {
        D3DXVECTOR4 vec;

        pTechnique->Pass( i ); 
        g_pd3dDevice->SetVertexShader( m_dwVSH );
        
        // Set constants
        if (dwRenderFlag & RF_NORMAL)
        {
            vec = D3DXVECTOR4( 0.0f, 1.0f, 0.5f, 2.0f );
            g_pd3dDevice->SetVertexShaderConstant(  7, &vec, 1 );
        }
        else if (dwRenderFlag & RF_BELOW_WATER)
        {
            vec = D3DXVECTOR4(0.5f, 0.95f, 1.0f, 0.0f );
            g_pd3dDevice->SetVertexShaderConstant(  4, &vec, 1 );
        }else if (dwRenderFlag & RF_ABOVE_WATER)
        {
            vec = D3DXVECTOR4(0, 0.0f, 0.0f, 0.0f);
            g_pd3dDevice->SetVertexShaderConstant(  4, &vec, 1 );
        }
        // Render
        g_pd3dDevice->DrawIndexedPrimitive( D3DPT_TRIANGLELIST, 0, m_dwNumVertices,
                                            m_dwStartIndex, m_dwPrimitiveCount );
    }
    pTechnique->End();

    return hr;
}




// This is for the sort operation. Sort need a static function to compare
// two models.  It can access the model list through this static pointer.
static CModelList* s_pADModelList = NULL;




//-----------------------------------------------------------------------------
// Name: CModelList()
// Desc: Constructor
//-----------------------------------------------------------------------------
CModelList::CModelList()
{
    m_nCurModel = NULL;
    s_pADModelList = this;
}



//-----------------------------------------------------------------------------
// Name: ~CModelList()
// Desc: Destructor
//-----------------------------------------------------------------------------
CModelList::~CModelList()
{
    for( DWORD i = 0; i < m_rgpModels.size(); i++ )
    {
        SAFE_DELETE( m_rgpModels[i] );
    }
    m_rgpModels.clear();
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: 
//-----------------------------------------------------------------------------
HRESULT CModelList::Render( DWORD dwRenderFlag )
{
    // Render each model
    for( DWORD i = 0; i < m_rgpModels.size(); i++ )
    {
        m_rgpModels[m_nRenderOrder[i]]->Render( dwRenderFlag );
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: CreateNewModel()
// Desc: Called by CPackageManager to add a new model to model list
//-----------------------------------------------------------------------------
CModel* CModelList::CreateNewModel()
{
    CModel* pModel = new CModel;
    assert(pModel);
    m_rgpModels.push_back(pModel);

    return pModel;
}




//-----------------------------------------------------------------------------
// Name: CompareRenderStates()
// Desc: The goal of the sorting is to make faster render speed through less down 
//       the times of change render state.
//-----------------------------------------------------------------------------
int __cdecl CModelList::CompareRenderStates( const VOID* arg1, const VOID* arg2 )
{
    INT idx1 = *( INT * ) arg1;
    INT idx2 = *( INT * ) arg2;
    CModel*  pModel1 = s_pADModelList->m_rgpModels[idx1];
    CModel*  pModel2 = s_pADModelList->m_rgpModels[idx2];

    if ( pModel1->m_dwRenderFlag != pModel2->m_dwRenderFlag )
    {
        if ( pModel1->m_dwRenderFlag > pModel2->m_dwRenderFlag )
            return 1;
        return -1;
    }

    if ( pModel1->m_pEffect != pModel2->m_pEffect )
    {
        if ( pModel1->m_pEffect > pModel2->m_pEffect )
            return 1;
        return -1;
    }

    for( INT i = 0; i < c_nMaxTextureStage; i++ )
    {
        if ( pModel1->m_rgTexPair[i].tex != pModel2->m_rgTexPair[i].tex )
        {
            if ( pModel1->m_rgTexPair[i].tex > pModel2->m_rgTexPair[i].tex )
                return 1;
            return -1;
        }
    }

    // Sort according vertex & index buffer to maximize rendering performance,
    // however, we have only one vertex & index buffer for the time being.
    if ( pModel1->m_pVB != pModel2->m_pVB )
    {
        if ( pModel1->m_pVB > pModel2->m_pVB )
            return 1;
        return -1;
    }
    
    if ( pModel1->m_pIB != pModel2->m_pIB )
    {
        if ( pModel1->m_pIB > pModel2->m_pIB )
            return 1;
        return -1;
    }

    if ( pModel1->m_dwVSH != pModel2->m_dwVSH )
    {
        if ( pModel1->m_dwVSH > pModel2->m_dwVSH )
            return 1;
        return -1;
    }

    if ( pModel1->m_dwPSH != pModel2->m_dwPSH )
    {
        if ( pModel1->m_dwPSH > pModel2->m_dwPSH )
            return 1;
        return -1;
    }

    return 0;
}




//-----------------------------------------------------------------------------
// Name: SortRenderOrder()
// Desc: Sort the render order of models
//-----------------------------------------------------------------------------
HRESULT CModelList::SortRenderOrder()
{
    for( DWORD i = 0; i < m_rgpModels.size(); i++ )
        m_nRenderOrder[i] = i;

    qsort( m_nRenderOrder, m_rgpModels.size(), sizeof(INT), CompareRenderStates );

    return S_OK;
}
