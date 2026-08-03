//-----------------------------------------------------------------------------
// File: Model.h
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
#pragma once
#include "waterdefs.h"
#include <vector>
using namespace std;

const INT c_nMaxID = 64; //The max number of the model ID




//-----------------------------------------------------------------------------
// Name: class CModel
// Desc: Defines a static model.
//       All the static models share same vertices buffer and indices buffer 
//       which is defined in CPackageManager.
//-----------------------------------------------------------------------------
class CModel 
{
    friend class CPackModelInfo;
    friend class CModelList;
protected:
    LPDIRECT3DVERTEXBUFFER8 m_pVB;
    LPDIRECT3DINDEXBUFFER8  m_pIB;

    DWORD                   m_dwVSH;  // vertex shader handle
    DWORD                   m_dwPSH;  // pixel shader handle

    LPD3DXEFFECT            m_pEffect;

    // All the models share the same vertex buffer and index buffer
    // So we need to know where they begins and how many they have.

    // The start  position in vertex buffer
    DWORD                   m_dwBaseVertexIndex; 
    DWORD                   m_dwNumVertices;
    // The start position indices buffer of 
    DWORD                   m_dwStartIndex;
    DWORD                   m_dwPrimitiveCount;

   
    // Indicate the render flag of this model: normal model or 
    // above water part or below water part.  A scene object may 
    // have 3 models: the normal part, the above water part and 
    // the below water part.
    DWORD                   m_dwRenderFlag;

    // A model can have 'cs_nMaxTextureStage'  texture stages
    struct TEXTURE_PAIR
    {
        DWORD dwFCCID;
        LPDIRECT3DBASETEXTURE8 tex;
    } m_rgTexPair[c_nMaxTextureStage];


public:
    CModel();
    ~CModel();

    HRESULT Render( DWORD dwRenderFlag );
};




//-----------------------------------------------------------------------------
// Name: class CModelList
// Desc: This class manage all the static models.
//-----------------------------------------------------------------------------
class CModelList
{
protected:
    vector<CModel *> m_rgpModels;
    
    // A nonuse model in the array can be called a 'slot'.
    // The m_nCurModel indicate the location of the first slot, 
    // See CreateNewModel
    INT    m_nCurModel; 
    INT    m_nRenderOrder[c_nMaxModelNumber];
public:
    CModelList();
    virtual ~CModelList();

    HRESULT Render( DWORD dwRenderFlag = RF_NONE );
    CModel* CreateNewModel();
    HRESULT SortRenderOrder();
    // Compare the render order between two model 
    static int __cdecl CompareRenderStates( const VOID* arg1, 
                                            const VOID* arg2 );
};

