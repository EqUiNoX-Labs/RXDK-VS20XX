//-----------------------------------------------------------------------------
// File: Water.cpp
//
// Desc: Four texture stages are used to render the water: wave, reflection,
//       refraction and Fresnel effect:
//
//       Reflection and refraction textures are prepared already before water
//       rendering (See CReflection and CRefraction).
//       Fresnel texture is the most significant component here. It gives the
//       reflection refraction ratio and an approximate normal per-pixel.
//       A bump texture is used to bump the other three textures with different
//       scale and direction.
//
// Hist: 11.14.02 - Created
//       12.10.02 - Optimized and code cleanup
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "WaterDefs.h"
#include "WaterApp.h"
#include "Water.h"


//Constants for loading textures and effects
const CHAR* c_strFresnelTextureFile = "Water\\textures\\fresnel.dds";
const CHAR* c_strBumpmapFile        = "Water\\BumpData\\%d.bum";
const CHAR* c_strWaterPixelShader   = "water\\Shaders\\water.xpu";
const CHAR* c_strWaterVertexShader   = "water\\Shaders\\water.xvu";




//-----------------------------------------------------------------------------
// Name: CWater()
// Desc: Constructor
//-----------------------------------------------------------------------------
CWater::CWater()
{
    m_pIB                  = NULL;
    m_pVB                  = NULL;
    m_fReflectionBumpScale = 0.05f;
    m_fRefractionBumpScale = 0.015f;
    m_fFresnelBumpScale    = 0.5f;
    m_vTexCoordScale       = D3DXVECTOR4( 0.2f,   0.5f,  0.5f, 1 );
    m_fBumpSpeedFactor     = 3.0f;
    m_nCurBumpID           = 0;
    ZeroMemory( m_prgBumpTextures, sizeof(LPDIRECT3DTEXTURE8) * c_nNumWaterTimeDiv );

    D3DXMatrixScaling( &m_matScaleReflection, 0.5, 1, 1 );
}




//-----------------------------------------------------------------------------
// Name: ~CWater()
// Desc: Destructor
//-----------------------------------------------------------------------------
CWater::~CWater()
{
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize the class
//-----------------------------------------------------------------------------
HRESULT CWater::Initialize()
{
    HRESULT hr;

    CHAR strFile[c_nMaxPathLength];
    XBUtil_FindMediaFile( strFile, c_strFresnelTextureFile );

    if( FAILED( hr = D3DXCreateTextureFromFileEx( g_pd3dDevice,
        strFile,
        D3DX_DEFAULT,
        D3DX_DEFAULT,
        1,
        0,
        D3DFMT_A8R8G8B8,
        D3DPOOL_DEFAULT,
        D3DX_DEFAULT,
        D3DX_DEFAULT,
        NULL,
        NULL, 
        NULL, 
        &m_pTexFresnel ) ) )
        return hr;

    if( FAILED( hr = LoadBumpTextures() ) )
        return hr;

    if( FAILED( hr = InitWaterPlaneMesh() ) )
        return hr;

    if( FAILED( hr = CreateShaders() ) )
        return hr;

    return hr;
}




//-----------------------------------------------------------------------------
// Name: LoadBumpTexture()
// Desc: Load Bump Textures. There are 17 frames. Each has 3 level mipmaps
//-----------------------------------------------------------------------------
HRESULT CWater::LoadBumpTextures()
{
    HRESULT hr = S_OK;

    for( INT i = 0; i < c_nNumWaterTimeDiv; i++ )
    {
        INT height = 128, width = 128;

        // Find file name
        CHAR strName[c_nMaxPathLength];
        CHAR strBumpFile[c_nMaxPathLength];
        sprintf( strName, c_strBumpmapFile, i ); 
        XBUtil_FindMediaFile( strBumpFile, strName );
        
        // Open and read file
        FILE* fp = fopen( strBumpFile, "rb");
        if( !fp )
            return E_FAIL;
      
        // Create texture
        if( FAILED( hr = g_pd3dDevice->CreateTexture( width, height, 3,
                                                      NULL, D3DFMT_V8U8, D3DPOOL_MANAGED,
                                                      &m_prgBumpTextures[i] ) ) )
            return hr;
       
        // Lock first level
        D3DLOCKED_RECT lr; 
        D3DSURFACE_DESC desc;
        m_prgBumpTextures[i]->GetLevelDesc( 0, &desc );
        m_prgBumpTextures[i]->LockRect( NULL, &lr, NULL, NULL );

        fread( lr.pBits, sizeof(WORD), height * width, fp );

        XBUtil_SwizzleTexture2D( &lr, &desc );
        m_prgBumpTextures[i]->UnlockRect( 0 );


        // Load mipmaps
        INT level = 0;
        while( ++level < 3 )
        {
            width >>= 1;
            height >>= 1;
                        
            m_prgBumpTextures[i]->GetLevelDesc( level, &desc );
            m_prgBumpTextures[i]->LockRect( level, &lr, NULL,NULL );

            fread( lr.pBits, sizeof(WORD), width * height, fp );

            XBUtil_SwizzleTexture2D( &lr, &desc );
            m_prgBumpTextures[i]->UnlockRect( level );
        }
        fclose(fp);
    }

    return hr;
}




//-----------------------------------------------------------------------------
// Name: InitWaterPlaneMesh()
// Desc: The water plane mesh is created here
//-----------------------------------------------------------------------------
HRESULT CWater::InitWaterPlaneMesh()
{
    HRESULT hr;

    if( FAILED( hr = g_pd3dDevice->CreateIndexBuffer( sizeof(WORD) * c_dwMaxIndexBufferSize, 
                                                      0, D3DFMT_INDEX16, 0, &m_pIB ) ) )
        return hr;

    if( FAILED( hr = g_pd3dDevice->CreateVertexBuffer( sizeof(D3DXVECTOR3) * c_dwMaxVertices,
                                                       0, 0, 0, &m_pVB) ) )
        return hr;

    D3DXVECTOR3* pVertices;
    WORD*        pIndices;
    m_pVB->Lock( 0, 0, (BYTE**)&pVertices, 0 );
    m_pIB->Lock( 0, 0, (BYTE**)&pIndices, 0 );

    FLOAT fFieldOfView = g_pApp->GetFieldOfView();
    FLOAT fAspect      = g_pApp->GetAspect();
    FLOAT fFarZ        = c_fFarPlane / 4;
    FLOAT fAngleStep   = g_pApp->GetFieldOfView() / 25;
    WORD  wColumn      = 29;

    FLOAT startAngle = fAngleStep * 3;
    FLOAT t1 = tanf( fFieldOfView / 2 );
    FLOAT a  = t1 * fAspect;
    FLOAT b  = sqrtf( a * a + 1 );

    // Create vertices
    FLOAT x;
    FLOAT z = 0.0f;
    FLOAT angle;
    
    WORD nMaxRow = 0;
    for( angle = -startAngle; angle < D3DX_PI / 2 - fAngleStep / 3; angle += fAngleStep )
    {
        nMaxRow++;
        z = tanf( angle );
        x = sqrtf( ( a * a ) / ( b * b - a * a ) * ( z * z + 1 ) );
        for( WORD i = 0; i < wColumn; i++ ) 
        {
            pVertices->x = - x + x / ( wColumn >> 1 ) * i;
            pVertices->y = 0;
            pVertices->z = z;
            pVertices++;
        }
    }

    if( z < fFarZ )
    {
        z = fFarZ;
        nMaxRow++;
        x = sqrtf( ( a * a ) / ( b * b - a * a ) * ( z * z + 1 ) );
        for( WORD i = 0; i < wColumn; i++ )
        {
            pVertices->x = -x + x / ( wColumn >> 1 ) * i;
            pVertices->y = 0;
            pVertices->z = z;
            pVertices++;
        }
    }
    m_dwNumVertices = wColumn * nMaxRow;

    // Create indices
    m_dwNumIndices = ( nMaxRow - 1 ) * ( wColumn - 1 ) * 4;

    WORD* pwIndices = pIndices;
    for( WORD i = 0; i < ( nMaxRow - 1 ); i++ )
    {
        WORD raw0 = i * wColumn;
        WORD raw1 = raw0 + wColumn;
        for( WORD j = 0; j < ( wColumn - 1 ); j++ )
        {
            *pwIndices++ = ( raw0 + j + 0);
            *pwIndices++ = ( raw1 + j + 0 );
            *pwIndices++ = ( raw1 + j + 1 );
            *pwIndices++ = ( raw0 + j + 1 );
        }
    }

    m_pVB->Unlock();
    m_pIB->Unlock();
    return hr;
}




//-----------------------------------------------------------------------------
// Name: CreateShaders()
// Desc: Load pixel shader and vertex shader
//-----------------------------------------------------------------------------
HRESULT CWater::CreateShaders()
{
    // Create the vertex shader
    DWORD dwDecl[]=
    {
        D3DVSD_STREAM(0),
        D3DVSD_REG(0, D3DVSDT_FLOAT3),
        D3DVSD_END(),
    };
    if( FAILED( XBUtil_CreateVertexShader( c_strWaterVertexShader, dwDecl,
                                            &m_dwVertexShaderHandle ) ) )
        return E_FAIL;

    // Create the pixel shader
    if( FAILED( XBUtil_CreatePixelShader( c_strWaterPixelShader,
                                          &m_dwPixelShaderHandle ) ) )
        return E_FAIL;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: MoveWater()
// Desc: Caculate the bump texture matrices 
//-----------------------------------------------------------------------------
HRESULT CWater::MoveWater()
{
    D3DXMATRIX matReflectionScale;
    D3DXMATRIX matRefractionScale;
    D3DXMATRIX matRotate;
    D3DXMATRIX matFresnelScale;
    
    D3DXMatrixScaling( &matReflectionScale, m_fReflectionBumpScale, m_fReflectionBumpScale, m_fReflectionBumpScale );
    D3DXMatrixScaling( &matRefractionScale, m_fRefractionBumpScale, m_fRefractionBumpScale, m_fRefractionBumpScale );
    D3DXMatrixScaling( &matFresnelScale, m_fFresnelBumpScale, m_fFresnelBumpScale, m_fFresnelBumpScale );

    D3DXVECTOR3 vDir = g_pApp->GetViewDirection();
    FLOAT fRotateAngle = ( FLOAT )atan2( vDir.z, vDir.x );
    D3DXMatrixRotationZ( &matRotate, fRotateAngle - D3DX_PI / 2 );

    D3DXMatrixMultiply( &m_matReflectionBump, &matReflectionScale, &matRotate );
    D3DXMatrixMultiply( &m_matReflectionBump, &m_matReflectionBump, &m_matScaleReflection );
    D3DXMatrixMultiply( &m_matRefractionBump, &matRefractionScale, &matRotate );
    D3DXMatrixMultiply( &m_matFresnelBump, &matFresnelScale, &matRotate );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Done every frame
//-----------------------------------------------------------------------------
HRESULT CWater::FrameMove()
{
    MoveWater();
    
    AlterBumpTextureIndex(); 

    // Calculate world matrix    
    D3DXMATRIX matScale; 
    D3DXMATRIX matRotate; 
    D3DXMATRIX matOffset;

    FLOAT fYScale = g_pApp->GetViewPosition().y;
    if( fYScale < 1e-10 )
        fYScale = 1e-10f;

    D3DXMatrixScaling( &matScale, fYScale, fYScale, fYScale );
    D3DXMatrixTranslation( &matOffset, g_pApp->GetViewPosition().x, 0, g_pApp->GetViewPosition().z );
    D3DXMatrixRotationY( &matRotate, atan2f( g_pApp->GetViewDirection().x, g_pApp->GetViewDirection().z ) );

    D3DXMatrixMultiply( &m_matWorld, &matScale, &matRotate );
    D3DXMatrixMultiply( &m_matWorld, &m_matWorld, &matOffset );
    D3DXMatrixTranspose( &m_matWorld, &m_matWorld);

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: AlterBumpTextureIndex()
// Desc: AlterBumpTextureIndex selects the current bump texture ID by current time
//-----------------------------------------------------------------------------
HRESULT CWater::AlterBumpTextureIndex()
{
    static FLOAT tTime = 0;

    tTime += m_fBumpSpeedFactor * g_pApp->GetElapsedTime();
    if( tTime >= c_fWaterTime )
    {
        tTime -= ( (INT) tTime / c_fWaterTime ) * c_fWaterTime;
    }
    m_nCurBumpID = (INT)( tTime / c_WaterTimeSpan );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the water effect
//-----------------------------------------------------------------------------
HRESULT CWater::Render()
{
    // Calculate and set paraqmeters
    // The sunlight dir is in D3D world transform, it must be converted to 
    // the space of the fresnel map.
    D3DXVECTOR4 vLightReflectDir;
    vLightReflectDir.x = -g_SunLightDir.x;
    vLightReflectDir.y = g_SunLightDir.z;
    vLightReflectDir.z = g_SunLightDir.y;
    D3DXVec4Scale( &vLightReflectDir, &vLightReflectDir, 0.5f );
    vLightReflectDir.x += 0.5f;
    vLightReflectDir.y += 0.5f;
    vLightReflectDir.z += 0.5f;
    vLightReflectDir.w = 0.497f;

    D3DXVECTOR4 vLightDir; 
    vLightDir = g_SunLightDir;
    vLightDir.w = 32;

    D3DXVECTOR3& vEyePos = g_pApp->GetViewPosition();
    D3DXVECTOR4  v4EyePos( vEyePos.x, vEyePos.y, vEyePos.z, 0.5f );

    D3DXMATRIX mat = *(g_pApp->GetViewProjMultiMatrix());
    D3DXMatrixTranspose(&mat, &mat);

    // Channel of bump texture value (V,U) MUST be signed. But this setting is only 
    // required in the bump map.  It must be restored to the default value after
    // draw primitives.
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORSIGN, D3DTSIGN_RSIGNED|D3DTSIGN_GSIGNED|D3DTSIGN_BSIGNED );

    // Set state
    g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    g_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    g_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
    g_pd3dDevice->SetRenderState( D3DRS_SPECULARENABLE, TRUE );
    g_pd3dDevice->SetRenderState( D3DRS_CULLMODE,  D3DCULL_CCW );
    g_pd3dDevice->SetRenderState( D3DRS_FOGENABLE, TRUE );
    g_pd3dDevice->SetRenderState( D3DRS_FOGCOLOR, c_dwFogColor );

    g_pd3dDevice->SetTexture( 0, m_prgBumpTextures[m_nCurBumpID] );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU,  D3DTADDRESS_WRAP );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV,  D3DTADDRESS_WRAP );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_MIPFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAXANISOTROPY,4 );

    g_pd3dDevice->SetTexture( 1, g_pApp->m_pReflection->GetTexture() );
    g_pd3dDevice->SetTextureStageState( 1, D3DTSS_ADDRESSU,  D3DTADDRESS_CLAMP );
    g_pd3dDevice->SetTextureStageState( 1, D3DTSS_ADDRESSV,  D3DTADDRESS_CLAMP );
    g_pd3dDevice->SetTextureStageState( 1, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetTextureStageState( 1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetTextureStageState( 1, D3DTSS_BUMPENVMAT00, FtoDW( m_matReflectionBump._11 ) );
    g_pd3dDevice->SetTextureStageState( 1, D3DTSS_BUMPENVMAT01, FtoDW( m_matReflectionBump._12 ) );
    g_pd3dDevice->SetTextureStageState( 1, D3DTSS_BUMPENVMAT10, FtoDW( m_matReflectionBump._21 ) );
    g_pd3dDevice->SetTextureStageState( 1, D3DTSS_BUMPENVMAT11, FtoDW( m_matReflectionBump._22 ) );

    g_pd3dDevice->SetTexture( 2, g_pApp->m_pRefraction->GetTexture() );
    g_pd3dDevice->SetTextureStageState( 2, D3DTSS_ADDRESSU,  D3DTADDRESS_CLAMP );
    g_pd3dDevice->SetTextureStageState( 2, D3DTSS_ADDRESSV,  D3DTADDRESS_CLAMP );
    g_pd3dDevice->SetTextureStageState( 2, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetTextureStageState( 2, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetTextureStageState( 2, D3DTSS_BUMPENVMAT00, FtoDW( -m_matRefractionBump._11 ) );
    g_pd3dDevice->SetTextureStageState( 2, D3DTSS_BUMPENVMAT01, FtoDW( -m_matRefractionBump._12 ) );
    g_pd3dDevice->SetTextureStageState( 2, D3DTSS_BUMPENVMAT10, FtoDW( -m_matRefractionBump._21 ) );
    g_pd3dDevice->SetTextureStageState( 2, D3DTSS_BUMPENVMAT11, FtoDW( -m_matRefractionBump._22 ) );

    g_pd3dDevice->SetTexture( 3, m_pTexFresnel );
    g_pd3dDevice->SetTextureStageState( 3, D3DTSS_ADDRESSU,  D3DTADDRESS_CLAMP );
    g_pd3dDevice->SetTextureStageState( 3, D3DTSS_ADDRESSV,  D3DTADDRESS_CLAMP );
    g_pd3dDevice->SetTextureStageState( 3, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetTextureStageState( 3, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetTextureStageState( 3, D3DTSS_BUMPENVMAT00, FtoDW( m_matFresnelBump._11 ) );
    g_pd3dDevice->SetTextureStageState( 3, D3DTSS_BUMPENVMAT01, FtoDW( m_matFresnelBump._12 ) );
    g_pd3dDevice->SetTextureStageState( 3, D3DTSS_BUMPENVMAT10, FtoDW( m_matFresnelBump._21 ) );
    g_pd3dDevice->SetTextureStageState( 3, D3DTSS_BUMPENVMAT11, FtoDW( m_matFresnelBump._22 ) );

    static D3DXVECTOR4 c15(1.0f,-1.0f,-1.0f,0.5f);
    g_pd3dDevice->SetVertexShader( m_dwVertexShaderHandle );
    g_pd3dDevice->SetVertexShaderConstant(  0, &mat, 4 );
    g_pd3dDevice->SetVertexShaderConstant(  4, &m_matWorld, 4 );
    g_pd3dDevice->SetVertexShaderConstant( 15, &c15, 1 );
    g_pd3dDevice->SetVertexShaderConstant( 16, &m_vTexCoordScale, 1 );
    g_pd3dDevice->SetVertexShaderConstant( 17, &v4EyePos, 1 );
    g_pd3dDevice->SetVertexShaderConstant( 18, (D3DXVECTOR4*)&g_vFog, 1 );
    g_pd3dDevice->SetVertexShaderConstant( 94, &vLightReflectDir, 1 );
    g_pd3dDevice->SetVertexShaderConstant( 95, &vLightDir, 1 );

    g_pd3dDevice->SetPixelShader( m_dwPixelShaderHandle );

    // Draw the water
    g_pd3dDevice->SetStreamSource( 0, m_pVB, sizeof(D3DXVECTOR3) );
    g_pd3dDevice->SetIndices( m_pIB, 0 );
    g_pd3dDevice->DrawIndexedPrimitive( D3DPT_QUADLIST, 0, m_dwNumVertices, 0, m_dwNumIndices/4 );

    // Restore defaut
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORSIGN, D3DTSIGN_RUNSIGNED|D3DTSIGN_GUNSIGNED|D3DTSIGN_BUNSIGNED );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Cleanup()
// Desc: Free resources
//-----------------------------------------------------------------------------
HRESULT CWater::Cleanup()
{
    g_pd3dDevice->DeletePixelShader( m_dwPixelShaderHandle );
    
    SAFE_RELEASE( m_pVB );
    SAFE_RELEASE( m_pIB );

    ReleaseBump();
    
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: ReleaseBump()
// Desc: Free bump map resources
//-----------------------------------------------------------------------------
HRESULT CWater::ReleaseBump()
{
    for( INT i = 0; i < c_nNumWaterTimeDiv; i++ )
    {
        SAFE_RELEASE( m_prgBumpTextures[i] );
    }

    return S_OK;
}