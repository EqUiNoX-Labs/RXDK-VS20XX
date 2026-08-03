//-----------------------------------------------------------------------------
// File: XBFur.h
//
// Desc: 
//
// Hist: 11.11.02 - Cleaned up for December XDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#pragma once
#include <xtl.h>
//#include "xfvf.h"


#define XBFUR_MAXSLICE_LOG2 5
#define XBFUR_MAXSLICE (1 << XBFUR_MAXSLICE_LOG2)


extern FLOAT g_fOneInch;


// Patch generation

//-----------------------------------------------------------------------------
// Name: struct Fuzz
// Desc: A fuzz is a single hair follicle, blade of grass, etc.
//-----------------------------------------------------------------------------
struct Fuzz
{
    D3DXVECTOR3 vel;           // Velocity
    D3DXVECTOR3 acc;           // Acceleration
    D3DXCOLOR   colorBase;
    D3DXCOLOR   colorTip;
};




//-----------------------------------------------------------------------------
// Name: struct FuzzInst
// Desc: A fuzz instance is a single instance of a fuzz located at x, z on the
//       patch. We create only a limited number of unique fuzzes and index the
//       library with lidx.
//-----------------------------------------------------------------------------
struct FuzzInst
{
    FLOAT x, z;              // Fuzz location
    int   lidx;              // Library index
};




//-----------------------------------------------------------------------------
// Name: class CXBFur
// Desc: A fur patch is a volume that holds fuzzes.
//       xsize and zsize are chosen by the user
//       ysize is calculated using the height of the tallest fuzz
//-----------------------------------------------------------------------------
class CXBFur
{
    friend class CXBFurMesh;
public:
    DWORD m_dwSeed;                // Patch seed
    
    FLOAT     m_fXSize;            // Patch size in world coords
    FLOAT     m_fYSize;
    FLOAT     m_fZSize;

    // Fuzz library
    DWORD     m_dwNumSegments;     // # of segments in highest LOD
    Fuzz      m_fuzzCenter;        // Fuzz constant
    Fuzz      m_fuzzRandom;        // Random offset around center
    DWORD     m_dwNumFuzzLib;      // # of fuzz in the library
    Fuzz*     m_pFuzzLib;          // Fuzz library

    // Fuzz instances
    DWORD     m_dwNumFuzz;         // # of fuzz in this patch
    FuzzInst* m_pFuzz;

    // Patch volume
    DWORD     m_dwNumSlices;       // # of layers in the volume
    DWORD     m_dwSliceSize;       // Width*height
    DWORD     m_dwSliceXSize;      // Width of volume texture slice
    DWORD     m_dwSliceZSize;      // Height of volume texture slice
    LPDIRECT3DTEXTURE8 m_apSliceTexture[XBFUR_MAXSLICE * 2 - 1];    // slices of volume texture
                                   // ... followed by level-of-detail textures  N/2, N/4, N/8, ... 1

    // LOD textures
    DWORD     m_dwNumSlicesLOD;    // Number of slices in current level of detail
    FLOAT     m_fLevelOfDetail;    // Current LOD value
    DWORD     m_iLOD;              // Current integer LOD value
    FLOAT     m_fLODFraction;      // Fraction towards next coarser level-of-detail
    DWORD     m_dwLODMax;          // Maximum LOD index
    LPDIRECT3DTEXTURE8 *m_pSliceTextureLOD; // current level of detail pointer into m_apSliceTexture array

    // Hair lighting texture
    D3DMATERIAL8 m_HairLightingMaterial;
    LPDIRECT3DTEXTURE8 m_pHairLightingTexture;

    // Fin texture
    DWORD     m_finWidth, m_finHeight;          // Size of fin texture
    FLOAT     m_fFinXFraction, m_fFinZFraction; // Portion of hair texture to put into fin
    LPDIRECT3DTEXTURE8 m_pFinTexture;           // Texture projected from the side

    CXBFur();
    ~CXBFur();
    
    VOID    InitFuzz( DWORD nfuzz, DWORD nfuzzlib );
    VOID    GenSlices( DWORD nslices, DWORD slicexsize, DWORD slicezsize );
    VOID    GenFin( DWORD finWidth, DWORD finHeight, float fFinXFraction, float fFinZFraction );
    VOID    GetLinesVertexBuffer( D3DVertexBuffer** ppVB );
    VOID    RenderLines();
    VOID    Save( CHAR* strFilename, int flags );
    VOID    Load( CHAR* strFilename );
    HRESULT SetHairLightingMaterial( D3DMATERIAL8* pMaterial );

    void SetPatchSize( FLOAT x, FLOAT z )
    {
        m_fXSize = x;
        m_fZSize = z;
        InitFuzz( m_dwNumFuzz, m_dwNumFuzzLib );    // Re-init the fuzz. Automatically sets ysize
    };
    
    void SetFVel( FLOAT cx, FLOAT cy, FLOAT cz, FLOAT rx, FLOAT ry, FLOAT rz )
    {
        m_fuzzCenter.vel.x = cx; m_fuzzCenter.vel.y = cy; m_fuzzCenter.vel.z = cz;
        m_fuzzRandom.vel.x = rx; m_fuzzRandom.vel.y = ry; m_fuzzRandom.vel.z = rz;
    };
    
    void SetFAcc( FLOAT cx, FLOAT cy, FLOAT cz, FLOAT rx, FLOAT ry, FLOAT rz )
    {
        m_fuzzCenter.acc.x = cx; m_fuzzCenter.acc.y = cy; m_fuzzCenter.acc.z = cz;
        m_fuzzRandom.acc.x = rx; m_fuzzRandom.acc.y = ry; m_fuzzRandom.acc.z = rz;
    };

    // fLevelOfDetail can range from 0 to log2(NumSlices)
    HRESULT SetLevelOfDetail( FLOAT fLevelOfDetail );
    
    HRESULT ComputeLevelOfDetailTextures();
    
    inline UINT LevelOfDetailCount( UINT iLOD )
    {
        return m_dwNumSlices >> iLOD;
    }
    
    inline UINT LevelOfDetailIndex( UINT iLOD )
    {
        UINT offset = 0;
        for( UINT i = 1; i <= iLOD; i++ )
            offset += LevelOfDetailCount( i-1 );
        return offset;
    }
    
    inline UINT TotalTextureCount()
    {
        UINT TextureCount = 0;
        for( UINT iLOD = 0; m_dwNumSlices >> iLOD; iLOD++ )
            TextureCount += LevelOfDetailCount( iLOD );
        return TextureCount;
    }

    // Compress textures one at a time until all are done.
    // Returns S_OK when all the textures are in fmtNew format.
    // Returns S_FALSE if there are textures still to be done.
    HRESULT CompressNextTexture( D3DFORMAT fmtNew, UINT* pTextureIndex );
};




//-----------------------------------------------------------------------------
// Name: FillHairLightingTexture()
// Desc: 
//-----------------------------------------------------------------------------
HRESULT FillHairLightingTexture( D3DMATERIAL8* pMaterial,
                                 LPDIRECT3DTEXTURE8 pTexture );

