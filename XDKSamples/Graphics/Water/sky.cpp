//-----------------------------------------------------------------------------
// File: Sky.cpp
//
// Desc: This file contains the sky class. 
//
// Hist: 11.14.02 - Created
//       12.10.02 - Optimized and code cleanup
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "WaterDefs.h"
#include "WaterApp.h"
#include "Sky.h"


// Constants for loading sky resource
const CHAR* c_strSkyMeshFile         = "NonWater\\Models\\Sky.xbg";
const CHAR* c_strSkyTextureFile      = "NonWater\\Textures\\Sky\\Sky.xpr";
const CHAR* c_strSkyVertexShaderFile = "NonWater\\Shaders\\Sky.xvu";
const CHAR* c_strSkyPixelShaderFile  = "NonWater\\Shaders\\Sky.xpu";




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Load sky mesh, texture and effects
//-----------------------------------------------------------------------------
HRESULT CSky::Initialize()
{
    // Load mesh
    if( FAILED( m_SkyMesh.Create( (CHAR*)c_strSkyMeshFile ) ) )
        return E_FAIL;

    // Load texture
    if( FAILED( m_xprTextures.Create( c_strSkyTextureFile ) ) )
        return E_FAIL;
    m_pTexture = m_xprTextures.GetCubemap( 0UL );

    // Create the vertex shader
    DWORD dwDecl[]=
    {
        D3DVSD_STREAM( 0 ),
        D3DVSD_REG( 0, D3DVSDT_FLOAT3 ),
        D3DVSD_END(),
    };

    if( FAILED( XBUtil_CreateVertexShader( c_strSkyVertexShaderFile, dwDecl,
                                           &m_dwVertexShaderHandle ) ) )
        return E_FAIL;

    // Create the pixel shader
    if( FAILED( XBUtil_CreatePixelShader( c_strSkyPixelShaderFile,
                                          &m_dwPixelShaderHandle ) ) )
        return E_FAIL;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Cannot render sky when the flag is RF_BELOW_WATER
//-----------------------------------------------------------------------------
HRESULT CSky::Render( ICullFrustum* pCullFrustumObject )
{
    // Save state
    DWORD dwCullMode, dwFog, dwZWrite;
    g_pd3dDevice->GetRenderState( D3DRS_CULLMODE, &dwCullMode );
    g_pd3dDevice->GetRenderState( D3DRS_FOGENABLE, &dwFog );
    g_pd3dDevice->GetRenderState( D3DRS_ZWRITEENABLE, &dwZWrite );

    // Get center-of-cubemap view matrix
    D3DXMATRIX matView = *(pCullFrustumObject->GetViewMatrix());
    matView._41 = matView._42 = matView._43 = 0.0f;

    // Set WVP matrix
    D3DXMATRIX mat;
    D3DXMatrixMultiply( &mat, &matView, pCullFrustumObject->GetProjMatrix() );
    D3DXMatrixTranspose( &mat, &mat );
    g_pd3dDevice->SetVertexShaderConstant( 0, &mat, 4 );
        
    // Set effect parameters
    if( pCullFrustumObject->IsReflected() )
    {
        D3DXVECTOR4 vFogHelperReflection( 0.0006f, -0.5f, 0, 0 );
        DWORD dwReflectionFogColor = D3DCOLOR_XRGB( 120, 132, 160 );
        g_pd3dDevice->SetVertexShaderConstant( 4, vFogHelperReflection, 1 );
        g_pd3dDevice->SetRenderState( D3DRS_FOGCOLOR, dwReflectionFogColor );
        g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
    }
    else
    {
        D3DXVECTOR4 vFogHelperNormal( 0.0042f,-0.05f, 0, 0 );
        g_pd3dDevice->SetVertexShaderConstant( 4, vFogHelperNormal, 1 );
        g_pd3dDevice->SetRenderState( D3DRS_FOGCOLOR, c_dwFogColor );
        g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CW );
    } 

    // Set state
    g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    g_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE,  FALSE );
    g_pd3dDevice->SetRenderState( D3DRS_FOGENABLE,        TRUE );
    g_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE,     FALSE );

    // There will be visible cracks on sky when set ADDRESSU/V to WRAP mode, 
    // so set it to CLAMP mode.
    g_pd3dDevice->SetTexture( 0, m_pTexture );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP );

    // Set vertex shader
    g_pd3dDevice->SetVertexShader( m_dwVertexShaderHandle );
    D3DXVECTOR4 vec5( 32, 32, 32, 32 );
    g_pd3dDevice->SetVertexShaderConstant( 5, &vec5, 1 ); 
    
    // Set pixel shader
    g_pd3dDevice->SetPixelShader( m_dwPixelShaderHandle );
    D3DXVECTOR4 vec0( 0.7f, 0.7f, 0.7f, 0.7f );
    g_pd3dDevice->SetPixelShaderConstant( 0, &vec0, 1 );

    // Render the sky
    m_SkyMesh.Render( XBMESH_NOMATERIALS|XBMESH_NOTEXTURES|XBMESH_NOFVF );

    // Restore state
    g_pd3dDevice->SetPixelShader( NULL );
    g_pd3dDevice->SetRenderState( D3DRS_CULLMODE,         dwCullMode );
    g_pd3dDevice->SetRenderState( D3DRS_FOGENABLE,        dwFog );
    g_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE,     dwZWrite );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP );

    return S_OK;
}

