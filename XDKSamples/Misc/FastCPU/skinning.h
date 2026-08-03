//-----------------------------------------------------------------------------
// File: Skinning.h
//
// Desc: Contains C and P3 SSE implementations of matrix pallette skinning
//       Note that the C implimentation is included for comparison only.
//       Prefer the SSE version on the Xbox
//
// Hist: 1.7.03 - Created
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#ifndef SSE_SKINNING
#define SSE_SKINNING

#include <assert.h>
#include <xtl.h>


//-----------------------------------------------------------------------------
// One SkinInfo struct per vertex
//-----------------------------------------------------------------------------
#pragma pack(  push, 1 ) 
struct SkinInfo
{
    FLOAT Weights[4];
    int Indices[4]; // pallette index * 64,  -1 means no more weights
};
#pragma pack( pop )


//-----------------------------------------------------------------------------
// Name: Vec3TransformNoAsm
// Desc: FPU vec3 transform
//-----------------------------------------------------------------------------
D3DXVECTOR3* WINAPI Vec3TransformCoordNoAsm( D3DXVECTOR3 *pOut,
                                             const D3DXVECTOR3 *pV,
                                             const D3DXMATRIX *pM );


//-----------------------------------------------------------------------------
// Name: Vec3TransformNormalNoAsm
// Desc: FPU vec3 normal transform
//-----------------------------------------------------------------------------
D3DXVECTOR3* WINAPI Vec3TransformNormalNoAsm( D3DXVECTOR3 *pOut,
                                              const D3DXVECTOR3 *pV,
                                              const D3DXMATRIX *pM );


//-----------------------------------------------------------------------------
// Name: Skin_C
// Desc: C matrix pallette skinning routine
//-----------------------------------------------------------------------------
VOID SkinC( const D3DXVECTOR3* pInData,
            DWORD dwNumVerts, DWORD dwNumNormalsPerVert,
            const SkinInfo* pSkinInfo,  
            const D3DXMATRIX* pPallette, DWORD dwNumPalletteMatrices,
            D3DXVECTOR3* pOutData);


//-----------------------------------------------------------------------------
// Name: PreCachePallette
// Desc: moves a matrix pallette into the cache
//-----------------------------------------------------------------------------
VOID PreCachePallette( D3DXMATRIX* pPallette, DWORD dwNumPalletteMatrices );


//-----------------------------------------------------------------------------
// Name: Skin_ASM
// Desc: SSE matrix pallette skinning routine
//-----------------------------------------------------------------------------
VOID SkinASM( const D3DXVECTOR3* pInData,
              DWORD dwNumVerts, DWORD dwNumNormalsPerVert,
              const SkinInfo* pSkinInfo,  
              const D3DXMATRIX* pPallette,
              DWORD dwNumPalletteMatrices, bool bPreCacheMatrices,
              D3DXVECTOR3* pOutData );


#endif // SSE_SKINNING
