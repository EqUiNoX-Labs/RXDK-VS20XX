//-----------------------------------------------------------------------------
// File: NormalMesh.cpp
//
// Desc: A subclass of xbmesh that supports calculating a normal map for the 
//       mesh by shooting rays from the surface along the normal of the mesh 
//       and intersecting the rays with another mesh where the normal is 
//       sampled.
//
// Hist: 08.05.02 - New
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------

#include <xtl.h>
#include <xgraphics.h>
#include <stdio.h>
#include <XBMesh.h>
#include "DrawTriangle.h"
#include "RayMesh.h"
#include "NormalMesh.h"



//-----------------------------------------------------------------------------
// Name: iRound()
// Desc: Fast float to integer rounding.
//-----------------------------------------------------------------------------
inline int iRound(float f)
{
    int iRetVal;

    __asm
    {
        fld [f]
        fistp [iRetVal]
    }

    return iRetVal;
}



//-----------------------------------------------------------------------------
// Name: NormalMesh::CalculateNormalMap()
// Desc: Calculate the normal map for this mesh using pHighMesh.
//-----------------------------------------------------------------------------
void NormalMesh::CalculateNormalMap( RayMesh* pHighMesh, int iOverSample )
{
    assert(pHighMesh);

    m_pHighMesh = pHighMesh;

    m_iOverSample = 1;
    if (iOverSample > 1)
        m_iOverSample = iOverSample;

    // Render the mesh triangles into texture.
    D3DXMATRIX matIdentity;
    D3DXMatrixIdentity( &matIdentity );

    CalculateFrameNormalMap( m_pMeshFrames, &matIdentity );
}



//-----------------------------------------------------------------------------
// Name: NormalMesh::CalculateFrameNormalMap()
// Desc: Calculate the normal map for this frame and any sub-frames.
//-----------------------------------------------------------------------------
void NormalMesh::CalculateFrameNormalMap( XBMESH_FRAME* pFrame, 
                                          D3DXMATRIX* pmatParent )
{
    // Apply the frame's local transform
    D3DXMATRIX matWorld;
    D3DXMatrixMultiply( &matWorld, &pFrame->m_matTransform, pmatParent );

    // Compute bounds for the mesh data
    if( pFrame->m_MeshData.m_dwNumSubsets ) 
        CalculateMeshNormalMap( &pFrame->m_MeshData, &matWorld );

    // Compute bounds for any child frames
    if( pFrame->m_pChild ) 
        CalculateFrameNormalMap( pFrame->m_pChild, &matWorld  );

    // Compute bounds for any sibling frames
    if( pFrame->m_pNext )  
        CalculateFrameNormalMap( pFrame->m_pNext, pmatParent );
}




//-----------------------------------------------------------------------------
// .BMP header structs so we can save out the data.
//-----------------------------------------------------------------------------
#pragma pack(push,1)
typedef struct tagBITMAPFILEHEADER { 
  WORD    bfType; 
  DWORD   bfSize; 
  WORD    bfReserved1; 
  WORD    bfReserved2; 
  DWORD   bfOffBits; 
} BITMAPFILEHEADER, *PBITMAPFILEHEADER; 

typedef struct tagBITMAPINFOHEADER{
  DWORD  biSize; 
  LONG   biWidth; 
  LONG   biHeight; 
  WORD   biPlanes; 
  WORD   biBitCount; 
  DWORD  biCompression; 
  DWORD  biSizeImage; 
  LONG   biXPelsPerMeter; 
  LONG   biYPelsPerMeter; 
  DWORD  biClrUsed; 
  DWORD  biClrImportant; 
} BITMAPINFOHEADER, *PBITMAPINFOHEADER; 
#pragma pack(pop)



//-----------------------------------------------------------------------------
// Name: NormalMesh::CalculateMeshNormalMap()
// Desc: Calculate the normal map for the actual mesh data.
//-----------------------------------------------------------------------------
void NormalMesh::CalculateMeshNormalMap( XBMESH_DATA* pMesh, D3DXMATRIX* pmat )
{
    DWORD            dwNumSubsets  =  pMesh->m_dwNumSubsets;
    XBMESH_SUBSET*   pSubsets      = &pMesh->m_pSubsets[0];
    D3DPRIMITIVETYPE dwPrimType    =  pMesh->m_dwPrimType;
    DWORD            dwVertexSize  = pMesh->m_dwVertexSize;

    BYTE*       pVertices;
    WORD*       pIndices;

    assert( pMesh->m_dwFVF & D3DFVF_NORMAL );
    assert( pMesh->m_dwFVF & D3DFVF_TEX1 );

    struct MeshVertex
    {
        D3DXVECTOR3 pos;
        D3DXVECTOR3 norm;
        float u, v;
    };

    pMesh->m_VB.Lock( 0, 0, &pVertices, 0 );
    pMesh->m_IB.Lock( 0, 0, (BYTE**)&pIndices, 0 );
    
    bool bNewTexture = true;
    int iWidth = 0;
    int iHeight = 0;
    int iTexWidth = 0;
    int iTexHeight = 0;
    D3DXVECTOR3* pNormals = 0;

    for( DWORD i = 0; i < dwNumSubsets; i++ )
    {
        // Skip over any subset without a texture.
        if ( pSubsets[i].pTexture == 0 )
            continue;

        if ( bNewTexture )
        {
            // Use this subsets existing texture as a target.
            D3DSURFACE_DESC desc;
            pSubsets[i].pTexture->GetLevelDesc( 0, &desc );
            
            iTexWidth = desc.Width;
            iTexHeight = desc.Height;

            iWidth = desc.Width * m_iOverSample;
            iHeight = desc.Height * m_iOverSample;

            pNormals = new D3DXVECTOR3[iWidth * iHeight];

            memset( pNormals, 0, sizeof(D3DXVECTOR3) * iWidth * iHeight );
            
            bNewTexture = false;
        }
        
        float fWidth = float(iWidth);
        float fHeight = float(iHeight);
        
        WORD wOffset = (WORD)pSubsets[i].dwIndexStart;

        if ( D3DPT_TRIANGLESTRIP == dwPrimType )
        {
            // Unstrip the indices.
            WORD ind0 = 0;
            WORD ind1 = pIndices[0+wOffset];
            WORD ind2 = pIndices[1+wOffset];

            for ( DWORD src = 2; src < pSubsets[i].dwIndexCount; src++ )
            {
                ind0 = ind1;
                ind1 = ind2;
                ind2 = pIndices[src+wOffset];
                
                if (ind0 != ind1 && ind1 != ind2 && ind2 != ind0)
                {
                    MeshVertex *pV[3];

                    if (src & 1)
                    {
                        pV[0] = (MeshVertex*)(pVertices + ind2 * dwVertexSize);
                        pV[1] = (MeshVertex*)(pVertices + ind1 * dwVertexSize);
                        pV[2] = (MeshVertex*)(pVertices + ind0 * dwVertexSize);
                    }
                    else
                    {
                        pV[0] = (MeshVertex*)(pVertices + ind0 * dwVertexSize);
                        pV[1] = (MeshVertex*)(pVertices + ind1 * dwVertexSize);
                        pV[2] = (MeshVertex*)(pVertices + ind2 * dwVertexSize);
                    }

                    SourceVert sv[3];

                    for (int j = 0; j < 3; j++)
                    {
                        sv[j].pos = pV[j]->pos;
                        sv[j].norm = pV[j]->norm;
                        sv[j].u = pV[j]->u * fWidth;
                        sv[j].v = pV[j]->v * fHeight;
                    }

                    DrawTriangle( pNormals, iWidth, &sv[0], &sv[1], &sv[2], 
                                  m_pHighMesh );
                }
            }
        }
        else if ( D3DPT_TRIANGLELIST == dwPrimType )
        {
            DWORD dwNumTriangles = pSubsets[i].dwIndexCount/3;

            for ( DWORD tri = 0; tri < dwNumTriangles; tri++ )
            {
                MeshVertex *pV[3];

                pV[0] = (MeshVertex*)(pVertices + pIndices[tri*3+0+wOffset] * 
                                                  dwVertexSize);

                pV[1] = (MeshVertex*)(pVertices + pIndices[tri*3+1+wOffset] * 
                                                  dwVertexSize);

                pV[2] = (MeshVertex*)(pVertices + pIndices[tri*3+2+wOffset] * 
                                                  dwVertexSize);

                SourceVert sv[3];

                for (int j = 0; j < 3; j++)
                {
                    sv[j].pos = pV[j]->pos;
                    sv[j].norm = pV[j]->norm;
                    sv[j].u = pV[j]->u * fWidth;
                    sv[j].v = pV[j]->v * fHeight;
                }

                DrawTriangle( pNormals, iWidth, &sv[0], &sv[1], &sv[2], 
                              m_pHighMesh );
            }
        }
        
        if ( i+1 == dwNumSubsets || pSubsets[i].pTexture != pSubsets[i+1].pTexture )
            bNewTexture = true;

        if ( bNewTexture )
        {
            // Normalize the normals.
            for ( int y = 0; y < iHeight; y++ )
            {
                for ( int x = 0; x < iWidth; x++ )
                {
                    D3DXVec3Normalize( &pNormals[y*iWidth+x], 
                                       &pNormals[y*iWidth+x] );
                }
            }

            // Downsample 
            if (m_iOverSample > 1)
            {
                D3DXVECTOR3* pDestNormals = new D3DXVECTOR3[iTexWidth * iTexHeight];
                memset( pDestNormals, 0, sizeof(D3DXVECTOR3) * iTexWidth * iTexHeight );
                
                for ( int y = 0; y < iTexWidth; y++ )
                {
                    for ( int x = 0; x < iTexHeight; x++ )
                    {
                        D3DXVECTOR3 vNorm(0.0f, 0.0f, 0.0f);
                        
                        for ( int oy = 0; oy < m_iOverSample; oy++ )
                        {
                            for ( int ox = 0; ox < m_iOverSample; ox++ )
                            {
                                vNorm += pNormals[(y * m_iOverSample + oy) * iWidth + (x * m_iOverSample + ox)];
                            }
                        }
                        
                        D3DXVec3Normalize( &pDestNormals[y*iTexWidth+x], &vNorm );
                    }
                }
                
                delete[] pNormals;

                pNormals = pDestNormals;
                iWidth = iTexWidth;
                iHeight = iTexHeight;
            }
                        
            // Fill in any gaps in the texture by averaging adjacent pixels.
            bool bGapFilled = true;

            while (bGapFilled)
            {
                bGapFilled = false;

                for ( int y = 0; y < iHeight; y++ )
                {
                    for ( int x = 0; x < iWidth; x++ )
                    {
                        if (pNormals[y*iWidth+x] == D3DXVECTOR3(0.0f, 0.0f, 0.0f))
                        {
                            bGapFilled = true;
                            
                            if (y > 0)
                                pNormals[y*iWidth+x] += pNormals[(y-1)*iWidth+x];

                            if (x > 0)
                                pNormals[y*iWidth+x] += pNormals[y*iWidth+(x-1)];

                            if (y < iHeight-1)
                                pNormals[y*iWidth+x] += pNormals[(y+1)*iWidth+x];

                            if (x < iWidth-1)
                                pNormals[y*iWidth+x] += pNormals[y*iWidth+(x+1)];

                            if (y > 0 && x > 0)
                            {
                                pNormals[y*iWidth+x] += pNormals[(y-1)*iWidth+x] * 
                                                        0.707107f;
                            }

                            if (y > 0 && x < iWidth-1)
                            {
                                pNormals[y*iWidth+x] += pNormals[(y-1)*iWidth+x] * 
                                                        0.707107f;
                            }

                            if (y < iHeight-1 && x > 0)
                            {
                                pNormals[y*iWidth+x] += pNormals[(y+1)*iWidth+(x-1)]
                                                        * 0.707107f;
                            }

                            if (y < iHeight-1 && x < iWidth-1)
                            {
                                pNormals[y*iWidth+x] += pNormals[(y+1)*iWidth+(x+1)]
                                                        * 0.707107f;
                            }

                            D3DXVec3Normalize( &pNormals[y*iWidth+x], 
                                               &pNormals[y*iWidth+x] );
                        }
                    }
                }
            }

            // Convert to RGB.
            DWORD *pBits = new DWORD[iTexWidth*iTexHeight];

            for ( int y = 0; y < iTexHeight; y++ )
            {
                for ( int x = 0; x < iTexWidth; x++ )
                {
                    D3DXVECTOR3& vNorm = pNormals[y*iTexWidth+x];

                    int r = iRound((vNorm.x * 0.5f + 0.5f) * 255.0f);
                    int g = iRound((vNorm.y * 0.5f + 0.5f) * 255.0f);
                    int b = iRound((vNorm.z * 0.5f + 0.5f) * 255.0f);

                    pBits[y*iTexWidth+x] = 0xff000000 | (r << 16) | (g << 8) | b;
                }
            }

            {
                // Save the computed texture.
                char str_name[256];
                strcpy( str_name, "D:\\Media\\Textures\\" );
                strcat( str_name, pSubsets[i].strTexture );

                BITMAPFILEHEADER bf;
                BITMAPINFOHEADER bi;

                bf.bfType = 'MB';
                bf.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + 
                            iTexWidth * iTexHeight * 4;
                bf.bfReserved1 = 0;
                bf.bfReserved2 = 0;
                bf.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

                bi.biSize = sizeof(BITMAPINFOHEADER);
                bi.biWidth = iTexWidth;
                bi.biHeight = -iTexHeight;
                bi.biPlanes = 1;
                bi.biBitCount = 32;
                bi.biCompression = 0;
                bi.biSizeImage = iTexWidth * iTexHeight * 4;
                bi.biXPelsPerMeter = 2834; 
                bi.biYPelsPerMeter = 2834;
                bi.biClrUsed = 0;
                bi.biClrImportant = 0;

                FILE *fp = fopen( str_name, "wb" );

                if (fp)
                {
                    fwrite( &bf, sizeof(BITMAPFILEHEADER), 1, fp );
                    fwrite( &bi, sizeof(BITMAPINFOHEADER), 1, fp );
                    fwrite( pBits, iTexWidth*iTexHeight, 4, fp );

                    fclose(fp);
                }
            }

            // Swizzle the texture.
            D3DLOCKED_RECT rect;
            pSubsets[i].pTexture->LockRect( 0, &rect, 0, 0 );

            XGSwizzleRect( pBits, 0, 0, rect.pBits, iTexWidth, iTexHeight, 0, 
                        sizeof(DWORD) );

            pSubsets[i].pTexture->UnlockRect( 0 );

            delete[] pBits;
            delete[] pNormals;
        }
    }

    pMesh->m_IB.Unlock();
    pMesh->m_VB.Unlock();
}
