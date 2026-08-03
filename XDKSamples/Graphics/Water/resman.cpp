//-----------------------------------------------------------------------------
// File: ResMan.cpp
//
// Desc: Load and manage all the textures, effects, shaders and other resources.
//
//       Models (See CModelList) can find their resources by the ID or name.
//
// Hist: 11.14.00 - Created
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "waterdefs.h"
#include "waterapp.h"
#include "resman.h"

// Constants for loading
const CHAR* c_strPackDir         = "fromMax";
const CHAR* c_strVertexShaders   = "NonWater\\Shaders\\vertexshaders.fx";
const CHAR* c_strPixelShaders    = "NonWater\\Shaders\\pixelshaders.fx";
const CHAR* c_strEffectsLocation = "NonWater\\Shaders\\effects";
const CHAR* c_strTexturesFile    = "NonWater\\Textures\\Textures.xpr";
extern CHAR g_strMediaPath[];// Defined in xbutil.cpp

// Fog vector
D3DXVECTOR4 g_vFog =  D3DXVECTOR4( 1.0f / ( c_FogNearPlane - c_fFogFarPlane ),
                                   -c_fFogFarPlane * 1.0f / ( c_FogNearPlane - c_fFogFarPlane ),
                                   0.0f,
                                   0.0f );


static CXBPackedResource m_xprTextures;




//-----------------------------------------------------------------------------
// Name: CResourceManager()
// Desc: Constructor
//-----------------------------------------------------------------------------
CResourceManager::CResourceManager()
{
    m_pVertexShaderEffect = NULL;
    m_pPixelShaderEffect  = NULL;
    m_nNumVertexShaders   = 0;
    m_nNumPixelShaders    = 0;
    m_nNumEffect          = 0;
}




//-----------------------------------------------------------------------------
// Name: ~CResourceManager()
// Desc: Destructor
//-----------------------------------------------------------------------------
CResourceManager::~CResourceManager()
{
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Load shaders, effects, textures
//-----------------------------------------------------------------------------
HRESULT CResourceManager::Initialize()
{
    HRESULT hr;

    if( FAILED( hr = LoadAllVertexShaders( c_strVertexShaders ) ) )
        return hr;
    if( FAILED( hr = LoadAllPixelShaders( c_strPixelShaders ) ) )
        return hr;
    if( FAILED( hr = LoadAllEffects( c_strEffectsLocation ) ) )
        return hr;

    for( INT i = 0; i < m_nNumEffect; i++ )
    {
        TEffect & e = m_rgEffects[i];
        e.m_pEffect->SetVector( FCC_REFF, &g_vFog );
        e.m_pEffect->SetVector( FCC_LTDR, &g_SunLightDir );
    }

    // Load the textures
    if( FAILED( m_xprTextures.Create( c_strTexturesFile ) ) )
        return E_FAIL;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: LoadAllVertexShaders()
// Desc: Loads all vertex shaders
//-----------------------------------------------------------------------------
HRESULT CResourceManager::LoadAllVertexShaders( const CHAR* strPath )
{
    HRESULT hr = S_OK;
    CHAR strFile[c_nMaxPathLength];

    // Load the effect, this effect include some shaders
    if( FAILED( hr = XBUtil_FindMediaFile( strFile, strPath ) ) )
        return hr;
    if( FAILED( hr = CreateEffectFromFile( g_pd3dDevice, strFile, 
                                           &m_pVertexShaderEffect ) ) )
        return hr;

    D3DXEFFECT_DESC ed;
    m_pVertexShaderEffect->GetDesc( &ed );

    for( DWORD i = 0; i < ed.Parameters; i++ ) // each shader
    {
        D3DXPARAMETER_DESC desc;
        m_pVertexShaderEffect->GetParameterDesc( i, &desc );
        if ( desc.Type == D3DXPT_VERTEXSHADER ) 
        {
             if ( FAILED( hr = LoadOneVertexShader( desc.Name ) ) )
                 return hr;
        }
    }

    return hr;
}




//-----------------------------------------------------------------------------
// Name: LoadAllPixelShaders()
// Desc: Load the effect, this effect include some shaders
//-----------------------------------------------------------------------------
HRESULT CResourceManager::LoadAllPixelShaders( const CHAR* strPath )
{
    HRESULT hr = S_OK;
    CHAR strFile[c_nMaxPathLength];

    XBUtil_FindMediaFile( strFile, strPath);
    if( FAILED( hr = CreateEffectFromFile( g_pd3dDevice,
                                           strFile, 
                                           &m_pPixelShaderEffect ) ) )
        return hr;

    D3DXEFFECT_DESC ed;
    m_pPixelShaderEffect->GetDesc( &ed );

    for( DWORD i = 0; i < ed.Parameters; i++ )  // each shader
    {
        D3DXPARAMETER_DESC desc;
        m_pPixelShaderEffect->GetParameterDesc(i, &desc );
        if( desc.Type == D3DXPT_PIXELSHADER ) 
        {
             if( FAILED( hr = LoadOnePixelShader( desc.Name ) ) )
                 return hr;
        }
    }

    return hr;
}




//-----------------------------------------------------------------------------
// Name: LoadAllEffects()
// Desc: Load the effects
//-----------------------------------------------------------------------------
HRESULT CResourceManager::LoadAllEffects( const CHAR* strPath )
{
    HRESULT hr = S_OK;
    CHAR strFile[c_nMaxPathLength];
    WIN32_FIND_DATA FindFileData;

    sprintf( strFile, "%s%s\\*.fx", g_strMediaPath, strPath );
    HANDLE hFind = FindFirstFile( strFile, &FindFileData );

    if( hFind != INVALID_HANDLE_VALUE )
    {
        if( FindFileData.dwFileAttributes != FILE_ATTRIBUTE_DIRECTORY )
        {
            if( FAILED( hr = LoadOneEffect( FindFileData.cFileName ) ) )
                return hr;
        }

        while( FindNextFile ( hFind, &FindFileData ) ) 
        {
            if( FindFileData.dwFileAttributes != FILE_ATTRIBUTE_DIRECTORY )
            {
                if( FAILED( hr = LoadOneEffect( FindFileData.cFileName ) ) )
                    return hr;
            }
        }
    };
    FindClose( hFind );

    return hr;
}




//-----------------------------------------------------------------------------
// Name: LoadOneVertexShader()
// Desc: Load a single vertex shader
//-----------------------------------------------------------------------------
HRESULT CResourceManager::LoadOneVertexShader( DWORD dwFCCPath )
{
    TVShader & vs = m_rgVertexShaders[m_nNumVertexShaders++];
 
    vs.dwFCCShaderName = dwFCCPath;
    // Get shader handle by its FCC name
    m_pVertexShaderEffect->GetVertexShader( dwFCCPath, &vs.dwShaderHandle );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: LoadOnePixelShader()
// Desc: Load a single pixel shader
//-----------------------------------------------------------------------------
HRESULT CResourceManager::LoadOnePixelShader( DWORD dwFCCPath )
{
    TPShader & ps = m_rgPixelShaders[m_nNumPixelShaders++];
 
    ps.dwFCCShaderName = dwFCCPath;
    // Get shader handle by its FCC name
    m_pPixelShaderEffect->GetPixelShader( dwFCCPath, &ps.dwShaderHandle );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: LoadOneEffect()
// Desc: Load a single effect
//-----------------------------------------------------------------------------
HRESULT CResourceManager::LoadOneEffect( const CHAR* strPath )
{
    HRESULT hr;
    CHAR strLongRelativePath[c_nMaxPathLength];

    sprintf( strLongRelativePath, "%s\\%s", c_strEffectsLocation, strPath );
    TEffect & e = m_rgEffects[m_nNumEffect++];
    strcpy(e.strEffectName , strPath );

    CHAR strFile[c_nMaxPathLength];
    XBUtil_FindMediaFile(strFile,strLongRelativePath);
    if( FAILED(  hr = CreateEffectFromFile( g_pd3dDevice, strFile, 
                                            &e.m_pEffect) ) )
        return hr;

    return hr;
}




//-----------------------------------------------------------------------------
// Name: GetVertexShader()
// Desc: Get vertex shader by its name. the name is from the package, 
//        e.g: "normal.c"
//-----------------------------------------------------------------------------
DWORD CResourceManager::GetVertexShader( const CHAR* strID )
{
    for( INT i = 0; i < m_nNumVertexShaders; i++ )
    {
        TVShader & v = m_rgVertexShaders[i];
        if( ConvertStringToFCC( strID ) == v.dwFCCShaderName )  
        {
            return v.dwShaderHandle;
        }
    }

    return 0;
}




//-----------------------------------------------------------------------------
// Name: GetPixelShader()
// Desc: Get pixel shader by its name. the name is from the package, 
//        e.g: "normal.c"
//-----------------------------------------------------------------------------
DWORD CResourceManager::GetPixelShader( const CHAR* strID )
{
    for( INT i = 0; i < m_nNumPixelShaders; i++ )
    {
        TPShader & p = m_rgPixelShaders[i];
        if( ConvertStringToFCC( strID ) == p.dwFCCShaderName )
        {
            return p.dwShaderHandle;
        }
    }

    return 0;
}




//-----------------------------------------------------------------------------
// Name: GetEffectsFile()
// Desc: Get effect by its name. the name from the package, 
//        e.g: "normal.c"
//-----------------------------------------------------------------------------
LPD3DXEFFECT CResourceManager::GetEffectsFile( const CHAR* strID )
{
    for( INT i = 0; i < m_nNumEffect; i++ ) 
    {
        TEffect & e = m_rgEffects[i];
        if( !_stricmp( strID ,e.strEffectName ) )
        {
            return e.m_pEffect;
        }
    }

    return NULL;
}




//-----------------------------------------------------------------------------
// Name: GetTexture()
// Desc: Get a texture by its file name
//-----------------------------------------------------------------------------
LPDIRECT3DBASETEXTURE8 CResourceManager::GetTexture( const CHAR* strID )
{
    return m_xprTextures.GetTexture( strID );
}




//-----------------------------------------------------------------------------
// Name: Cleanup()
// Desc: Frees resources
//----------------------------------------------------------------------------- 
HRESULT CResourceManager::Cleanup()
{
    // Should release XPR here

    for( ; m_nNumEffect > 0; ) 
    {
        TEffect & e = m_rgEffects[--m_nNumEffect];
        SAFE_RELEASE( e.m_pEffect );
    }

    SAFE_RELEASE( m_pVertexShaderEffect );
    SAFE_RELEASE( m_pPixelShaderEffect );

    return S_OK;
}