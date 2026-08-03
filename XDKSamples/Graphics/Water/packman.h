//-----------------------------------------------------------------------------
// File: PackMan.h
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
//       The format of package: 
// 
//       Number of all models
//       { Description of each model }
//       { Vertex buffer
//         Indiex buffer }
//       End of File
//
//       Hierarchy structure:
//
//           PACKAGE FILE                     (FillInfo2ModelList)    PROGRAM
//       ---------------------------------------------------------------------------
//       + PackModel0   (ModelDesc)                                  Model List
//       |  |------- [ Normal rendering parameters]      ->             Model0
//       |  |------- [ Reflection rendering parameters]  ->             Model1
//       |  |--------[ Refraction rendering parameters]  ->             Model2
//       + PackModel1                                                   ...
//         ...                                                          Model3
//
// Hist: 11.14.00 - Created
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#pragma once
#include "waterdefs.h"

class CModelList;
class CModel;

typedef CHAR T_CHAR64[64];
typedef CHAR T_CHAR128[128];




//-----------------------------------------------------------------------------
// Name: class CPackModelInfo
// Desc: class to maintain the information of one package model in memory.
//-----------------------------------------------------------------------------
class CPackModelInfo
{
    friend class CPackageManager;

protected:
    // SModelDesc is the structure in the package file
    // It is the description of each model in the package
    
    struct Attribute // One sub model 
    {
        // The models share same vertex and index buffer
        // So they need to know each model's beginning and size
        struct
        {
            DWORD dwStartIndex;
            DWORD dwNumTotalFaces;
            DWORD dwStartVertex;
            DWORD dwNumTotalVertices;
        }  BufferInfo; 

        // The name of the shaders and effect of this model
        T_CHAR64 strVertexShader;
        T_CHAR64 strPixelShader;
        T_CHAR64 strEffects;
    };

    struct SModelDesc // One model
    {
        Attribute   attrNormal; 
        Attribute   attrAboveWater;
        Attribute   attrBelowWater;
        struct TexturePair
        {
            T_CHAR64   strParameter;
            T_CHAR128  strTextureID;
        };

        TexturePair rgTextures[c_nMaxTextureStage];

        DWORD dwNumTotalVertices;
        DWORD dwNumTotalFaces;
    } ModelDesc;

    // One model info includes 3 sub models: all part, above water part 
    // and below water part
    CModel*       m_pModel[3];
    // The start vertex index in the shared vertex buffer
    DWORD         m_dwBaseVertexIndex; 
    // The start index in the shared indices buffer
    DWORD         m_dwStartIndex; 

    // Copy/Fill attribute info into CModel
    HRESULT FillAttribute( CModel* pModels, const Attribute* attr);

    // Copy/Fill texture pair info into CModel
    HRESULT FillTexturePair( CModel* pModels);
public:

      CPackModelInfo();
      virtual ~CPackModelInfo();

     // FillInfo2ModelList the infomation of this class to 3 models in model list
    HRESULT FillInfo2ModelList( CModelList* pModelList,
                                const CPackageManager* pPackModelMan );

};




//-----------------------------------------------------------------------------
// Name: class CPackageManager
// Desc: This class reads the package file into a CPackModelInfo list
//       and creates and loads the shared vertex and index buffers.
//-----------------------------------------------------------------------------
class CPackageManager
{
    friend class CPackModelInfo;

public:
    struct TVertex
    {
        D3DXVECTOR3 pos;
        D3DXVECTOR3 nrm;
        D3DXVECTOR2 tex;
    };

protected:
    // All the models except the sky and water share 
    // the same buffer, which is defined here.
    LPDIRECT3DVERTEXBUFFER8  m_pVB;
    LPDIRECT3DINDEXBUFFER8   m_pIB;

    DWORD                    m_dwNumTotalVertices;
    DWORD                    m_dwNumTotalFaces;

    INT                      m_nNumModel;

    CModelList*              m_pModelList;
    CPackModelInfo           m_rgModelInfos[c_nMaxModelNumber];

protected:
    HRESULT LoadMeshes( FILE* fp, TVertex** ppOutVertices,
                                  WORD** ppOutIndices );

public:
    CPackageManager( CModelList* pList);
    virtual ~CPackageManager();


    HRESULT Initialize();
    HRESULT Load();
    HRESULT Cleanup();
};
