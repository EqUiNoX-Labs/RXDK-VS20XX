//-----------------------------------------------------------------------------
// File: ResMan.h
//
// Desc: Load and manage all the textures, effects, shaders and other resources.
//
//       Models (See CModelList) can find their resources by the ID or name.
//
// Hist: 11.14.00 - Created
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#pragma once
#include "waterdefs.h"


       
       
//-----------------------------------------------------------------------------
// Name: class CResourceManager
// Desc: This class loads and manages all resource including textures,
//       effects and shaders.
//-----------------------------------------------------------------------------
class CResourceManager
{
    friend class CPackageManager;
protected:
    // Description of vertexShader resource
    struct TVShader
    {
        DWORD dwFCCShaderName;
        DWORD dwShaderHandle;
    } m_rgVertexShaders[c_nMaxVShader];
    // Description of pixelShader resource
    struct TPShader
    {
        DWORD dwFCCShaderName;
        DWORD dwShaderHandle;
    } m_rgPixelShaders[c_nMaxPShader];
    // Description of effect resource
    // These effects include only "'fixed function" effects, which have no 
    // shaders.
    struct TEffect
    {
        CHAR strEffectName[c_nMaxPathLength];
        LPD3DXEFFECT m_pEffect; 
    } m_rgEffects[c_nMaxEffect];
    INT  m_nNumEffect;

    // Theese effects include some vertex shaders
    LPD3DXEFFECT   m_pVertexShaderEffect;
    INT            m_nNumVertexShaders;
    
    // These effects include some pixel shaders
    LPD3DXEFFECT   m_pPixelShaderEffect;
    INT            m_nNumPixelShaders;


protected:
    HRESULT LoadAllVertexShaders( const CHAR* strPath );
    HRESULT LoadOneVertexShader( DWORD dwFCCPath );

    HRESULT LoadAllPixelShaders( const CHAR* strPath );
    HRESULT LoadOnePixelShader( DWORD dwFCCPath  );

    HRESULT LoadAllEffects( const CHAR* strPath );
    HRESULT LoadOneEffect( const CHAR* strPath );

public:
    CResourceManager();
    virtual ~CResourceManager();

    // Get resource by its ID
    DWORD GetVertexShader( const CHAR* strID );
    DWORD GetPixelShader( const CHAR* strID );
    LPD3DXEFFECT GetEffectsFile( const CHAR* strID );
    LPDIRECT3DBASETEXTURE8 GetTexture( const CHAR* strID );

    HRESULT Initialize();
    HRESULT Cleanup();
};
