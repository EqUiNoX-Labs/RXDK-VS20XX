//-----------------------------------------------------------------------------
// File: PackMan.cpp
//
// Desc: Classes to load the package file. 
//
//       The package file contains mesh data and the model descriptions. CPackMan 
//       creates and loads the mesh data to the shared vertex buffer and index
//       buffer which is used by all static models(see CModelList for more info). 
//
//       Models texture and effect resources (See CResourceManager) are referenced
//       by ID or name. The ID and name come from the package file and be stored in
//       the model (CPackModelInfo::FillInfo2ModelList).
//
// Hist: 11.14.00 - Created
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "waterdefs.h"
#include "waterapp.h" 
#include "packman.h"
#include "resman.h"
#include "model.h"

//The package file for the scene
const CHAR* c_strPackageFile = "NonWater\\models\\models.package";




//-----------------------------------------------------------------------------
// Name: CPackModelInfo()
// Desc: Constructor
//-----------------------------------------------------------------------------
CPackModelInfo::CPackModelInfo()
{
    m_pModel[0]         = NULL;
    m_pModel[1]         = NULL;
    m_pModel[2]         = NULL;
    m_dwStartIndex      = 0;
    m_dwBaseVertexIndex = 0;
    ZeroMemory( &ModelDesc, sizeof(ModelDesc) );
}




//-----------------------------------------------------------------------------
// Name: ~CPackModelInfo()
// Desc: Destructor
//-----------------------------------------------------------------------------
CPackModelInfo::~CPackModelInfo()
{
}




//-----------------------------------------------------------------------------
// Name: Attach()
// Desc: Create a new CModel and attach the information to it 
//-----------------------------------------------------------------------------
HRESULT CPackModelInfo::FillInfo2ModelList( CModelList*  pModelList, 
                                            const CPackageManager* pPackModelMan )
{
    assert( pModelList );
    assert( pPackModelMan );

    if( ModelDesc.attrAboveWater.BufferInfo.dwNumTotalFaces )
    {
        m_pModel[2] = pModelList->CreateNewModel();
        FillAttribute( m_pModel[2], &ModelDesc.attrNormal );
        m_pModel[2]->m_pVB = pPackModelMan->m_pVB;
        m_pModel[2]->m_pIB = pPackModelMan->m_pIB;
        m_pModel[2]->m_dwBaseVertexIndex = m_dwBaseVertexIndex;
        m_pModel[2]->m_dwRenderFlag = RF_NORMAL;
        FillTexturePair( m_pModel[2] );

        m_pModel[0] = pModelList->CreateNewModel();
        FillAttribute( m_pModel[0], &ModelDesc. attrAboveWater );
        m_pModel[0]->m_pVB = pPackModelMan->m_pVB;
        m_pModel[0]->m_pIB = pPackModelMan->m_pIB;
        m_pModel[0]->m_dwBaseVertexIndex = m_dwBaseVertexIndex;
        m_pModel[0]->m_dwRenderFlag = RF_ABOVE_WATER;
        FillTexturePair( m_pModel[0] );
    }

    if( ModelDesc.attrBelowWater.BufferInfo.dwNumTotalFaces )
    {
        m_pModel[1] = pModelList->CreateNewModel();
        FillAttribute( m_pModel[1], &ModelDesc.attrBelowWater );
        m_pModel[1]->m_pVB = pPackModelMan->m_pVB;
        m_pModel[1]->m_pIB = pPackModelMan->m_pIB;
        m_pModel[1]->m_dwBaseVertexIndex = m_dwBaseVertexIndex;
        m_pModel[1]->m_dwRenderFlag = RF_BELOW_WATER;
        FillTexturePair( m_pModel[1] );
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FillAttribute()
// Desc: Fill the attributes into the model
//-----------------------------------------------------------------------------
HRESULT CPackModelInfo::FillAttribute( CModel* pModels, const Attribute* attr )
{
    // Fill shaders and effect
    pModels->m_dwVSH   = g_pApp->m_pResMan->GetVertexShader( attr->strVertexShader );
    pModels->m_dwPSH   = g_pApp->m_pResMan->GetPixelShader( attr->strPixelShader );
    pModels->m_pEffect = g_pApp->m_pResMan->GetEffectsFile( attr->strEffects );

    // Fill the information about the vertices buffer and indices buffer
    pModels->m_dwNumVertices    = attr->BufferInfo.dwNumTotalVertices;
    pModels->m_dwPrimitiveCount = attr->BufferInfo.dwNumTotalFaces;
    pModels->m_dwStartIndex     = m_dwStartIndex + attr->BufferInfo.dwStartIndex;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FillTexturePair()
// Desc: Fill texture information
//-----------------------------------------------------------------------------
HRESULT CPackModelInfo::FillTexturePair( CModel* pModels )
{
    for( INT i = 0; i < c_nMaxTextureStage; i++ )
    {
        const CHAR* par;
        // Get texture name
        if( !_stricmp( ModelDesc.rgTextures[i].strParameter, "tBaseTex") )
            par = "BsTx";
        else
            par = ModelDesc.rgTextures[i].strParameter;

        if( !par || !par[0] )
            break;

        pModels->m_rgTexPair[i].dwFCCID = GetFCCFromString( par );

        // Find texture and fill it into model
        LPDIRECT3DBASETEXTURE8 tex;
        const CHAR* id = ModelDesc.rgTextures[i].strTextureID;
        tex = g_pApp->m_pResMan->GetTexture( id );
        pModels->m_rgTexPair[i].tex = tex;
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: CPackageManager()
// Desc: Constructor
//-----------------------------------------------------------------------------
CPackageManager::CPackageManager( CModelList*  pList )
{
    m_pVB                = NULL;
    m_pIB                = NULL;
    m_dwNumTotalVertices = 0L;
    m_dwNumTotalFaces    = 0L;
    m_pModelList         = pList;
    m_nNumModel          = 0;
}




//-----------------------------------------------------------------------------
// Name: ~CPackageManager()
// Desc: Destructor
//-----------------------------------------------------------------------------
CPackageManager::~CPackageManager()
{
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Load package, create and fill buffer
//-----------------------------------------------------------------------------
HRESULT CPackageManager::Initialize()
{
    HRESULT hr;
    
    // Load package file into memory
    if( FAILED( hr = Load() ) )
        return hr;
    
    // Fill information into models
    for( INT i = 0; i < m_nNumModel; i++ )
    {
        m_rgModelInfos[i].FillInfo2ModelList( m_pModelList, this );
    }

    return hr;
}




//-----------------------------------------------------------------------------
// Name: Load()
// Desc: Load package
//-----------------------------------------------------------------------------
HRESULT CPackageManager::Load()
{
    HRESULT hr;

    // Find file
    CHAR fullPath[c_nMaxPathLength];
    CHAR file[c_nMaxPathLength];
    sprintf( fullPath, c_strPackageFile, c_strPackDir );
    if ( FAILED( hr = XBUtil_FindMediaFile( file, fullPath ) ) )
        return hr;

    TVertex* pTmpStoredVertices;
    WORD*    pTmpStoredIndices;

   // Open and load
    FILE* fp = fopen( file, "rb" );
    if( !fp )
        return E_FAIL;

    if( FAILED( hr = LoadMeshes( fp, &pTmpStoredVertices, &pTmpStoredIndices ) ) )
    {
        fclose(fp);
        return hr;
    }

    fclose( fp );

    // Create buffers
    if( m_dwNumTotalVertices && m_dwNumTotalFaces )
    {
        if( FAILED( hr = g_pd3dDevice->CreateIndexBuffer( sizeof(WORD) * m_dwNumTotalFaces * 3, 
                                                          0, D3DFMT_INDEX16, 0, &m_pIB ) ) )
            return hr;

        if( FAILED( hr = g_pd3dDevice->CreateVertexBuffer( sizeof(TVertex) * m_dwNumTotalVertices,
                                                           0, 0, 0, &m_pVB ) ) )
            return hr;

        // Fill buffers
        TVertex* pVertices;
        m_pVB->Lock( 0, 0, (BYTE**)&pVertices, 0 );
        memcpy( pVertices, pTmpStoredVertices, sizeof(TVertex) * m_dwNumTotalVertices );
        m_pVB->Unlock();
        
        WORD* pIndices;
        m_pIB->Lock( 0, 0, (BYTE**)&pIndices, 0 );
        memcpy( pIndices, pTmpStoredIndices, sizeof(WORD) * m_dwNumTotalFaces * 3 );
        m_pIB->Unlock();

    }

    SAFE_DELETE( pTmpStoredVertices ); // release temp buffer
    SAFE_DELETE( pTmpStoredIndices );

    return hr;
}




//-----------------------------------------------------------------------------
// Name: LoadMeshes()
// Desc: Loads the vertex buffer and index buffer from the package
//-----------------------------------------------------------------------------
HRESULT CPackageManager::LoadMeshes( FILE* fp, TVertex** ppOutVertices,
                                               WORD** ppOutIndices )
{
    // The total number of models
    fread( &m_nNumModel, sizeof(m_nNumModel), 1, fp );

    m_dwNumTotalVertices = 0;
    m_dwNumTotalFaces    = 0;

    for( INT i = 0; i < m_nNumModel; i++ ) 
    {
        CPackModelInfo& model = m_rgModelInfos[i];
        // Read description
        size_t readCount = fread( &model.ModelDesc, sizeof(model.ModelDesc), 1, fp );
        if( readCount != 1 )
            assert( FALSE );

        model.m_dwStartIndex      = m_dwNumTotalFaces * 3;
        model.m_dwBaseVertexIndex = m_dwNumTotalVertices;

        m_dwNumTotalVertices += model.ModelDesc.dwNumTotalVertices;
        m_dwNumTotalFaces    += model.ModelDesc.dwNumTotalFaces;
        
        assert( model.ModelDesc.dwNumTotalVertices < 65535 );
        assert( model.ModelDesc.dwNumTotalFaces < 65535 );
    }

    TVertex* pVertices = new TVertex[m_dwNumTotalVertices];
    if( pVertices == NULL )
        return E_FAIL;

    WORD* pIndices = new WORD[m_dwNumTotalFaces * 3];
    if( pIndices == NULL )
        return E_FAIL;

    // Read each model
    for( INT i = 0; i < m_nNumModel; i++ ) 
    {
        CPackModelInfo& model = m_rgModelInfos[i];

        // Read vertex buffer
        fread( pVertices + model.m_dwBaseVertexIndex,
               sizeof(TVertex), model.ModelDesc.dwNumTotalVertices,
               fp );

        // Read index buffer
        fread( pIndices + model.m_dwStartIndex,
               sizeof(WORD), model.ModelDesc.dwNumTotalFaces * 3,
               fp );
    }
    
    (*ppOutVertices) = pVertices;
    (*ppOutIndices)  = pIndices;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Cleanup()
// Desc: Frees resources
//-----------------------------------------------------------------------------
HRESULT CPackageManager::Cleanup()
{
    SAFE_RELEASE( m_pVB );
    SAFE_RELEASE( m_pIB );

    return S_OK;
}
