//-----------------------------------------------------------------------------
// File: Mipmap.h
//
// Desc: 
//
// Hist: 11.11.02 - Cleaned up for December XDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <xtl.h>
#include <xgraphics.h>




//-----------------------------------------------------------------------------
// Name: class MipmapFilter
// Desc: Filter coefficients and offsets for separable mipmap filtering.
//-----------------------------------------------------------------------------
class MipmapFilter 
{
public:
    UINT    m_nSuperSample;                 // Number of source samples per destination pixel
    FLOAT   m_fHalfWidth;                   // Size of filter support
    UINT    m_nSample;                      // Number of filter samples
    struct Sample 
    {
        FLOAT m_fOffset;                    // Offset in destination pixel coords
        FLOAT m_fValue;                     // Value at offset
    };
    Sample* m_rSample;
    int     m_iSuperMin, m_iSuperMax;       // Range of supersamples
 
public:
#define MIPMAPFILTER_NORMALIZE_ADD      001 // Add (1-sum)/N to each filter coefficient
#define MIPMAPFILTER_NORMALIZE_MULTIPLY 002 // Multiply each value by 1/sum
#define MIPMAPFILTER_STRETCH            010 // Expand filter slightly to make tighter frequency bound
#define MIPMAPFILTER_KEEPZEROS          020 // Filter values that quantize to zero are usually culled, when 255 * f < 0.5
    
    MipmapFilter( UINT nSuperSample = 2,    // Number of source samples per output pixel
                  FLOAT fHalfWidth = 2.f,   // In destination pixel coords, filter is assumed to be zero outside this bound
                  FLOAT (*pfFilter)( FLOAT x, FLOAT fHalfWidth ) = lanczos, // Filter kernel
                  DWORD dwFlags = MIPMAPFILTER_NORMALIZE_ADD );             // Normalization flags
    ~MipmapFilter() { delete [] m_rSample; }

    // Kernel helper functions
    VOID         NormalizeAdd();            // Add (1 - sum)/N to each value
    VOID         NormalizeMultiply();       // Multiply each value by 1/sum
    static FLOAT triangle( float x, float fHalfWidth );
    static FLOAT mitchell( float x, float fHalfWidth );
    static FLOAT sinc( float x );                         // sin(x) / x
    static FLOAT lanczos( float x, float fHalfWidth );    // sinc windowed sinc
    static FLOAT hamming( float x, float fHalfWidth );    // Hamming-windowed sinc
};




//-----------------------------------------------------------------------------
// Name: Decimate()
// Desc: Use filter coefficients to re-sample from higher resolution
//       pTextureSrc to lower resolution pSurfaceDst.
//       The width of Src must be m_nSuperSample * width of Dst and
//       the height of Src must be m_nSuperSample * height of Dst.
//       The scratch texture must be swizzled and have width >= Src and height >= Src
//       If either pTextureScratchY or pTextureScratchX are NULL, temporary
//       textures will be created and then released when done.
//-----------------------------------------------------------------------------
HRESULT Decimate( LPDIRECT3DSURFACE8 pSurfaceDst,
                  LPDIRECT3DTEXTURE8 pTextureSrc,
                  UINT iSourceLevel,    // Index of source mip level in pTextureSrc
                  D3DTEXTUREADDRESS WrapU = D3DTADDRESS_WRAP, 
                  D3DTEXTUREADDRESS WrapV = D3DTADDRESS_WRAP,
                  LPDIRECT3DTEXTURE8 pTextureScratchY = NULL,    // Destination for filtering in Y
                  LPDIRECT3DTEXTURE8 pTextureScratchX = NULL,    // Destination for filtering in X
                  MipmapFilter* pFilter = NULL );




//-----------------------------------------------------------------------------
// Name: GenerateMipmaps()
// Desc: Compute mip maps starting from iSourceLevel
//       The scratch textures must be swizzled and have width >= Src and height >= Src
//       If either pTextureScratchY or pTextureScratchX are NULL, temporary
//       textures will be created and then released when done.
//       The filter must have nSuperSample = 2.
//-----------------------------------------------------------------------------
HRESULT GenerateMipmaps( LPDIRECT3DTEXTURE8 pTexture,
                         UINT iSourceLevel,
                         D3DTEXTUREADDRESS WrapU = D3DTADDRESS_WRAP, 
                         D3DTEXTUREADDRESS WrapV = D3DTADDRESS_WRAP,
                         LPDIRECT3DTEXTURE8 pTextureScratchY = NULL,
                         LPDIRECT3DTEXTURE8 pTextureScratchX = NULL,
                         MipmapFilter* pFilter = NULL );




//-----------------------------------------------------------------------------
// Name: CompressTexture()
// Desc: Create a new texture the same size as the source texture, with the
//       same number of mipmap levels, and then copy the source to the
//       destination, with a format change.  This function handles swizzled and
//       unswizzled textures.
//-----------------------------------------------------------------------------
HRESULT CompressTexture( LPDIRECT3DTEXTURE8* ppTextureDst, D3DFORMAT fmtNew,
                         LPDIRECT3DTEXTURE8 pTextureSrc );



