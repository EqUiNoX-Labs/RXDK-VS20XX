//-----------------------------------------------------------------------------
// File: Octosphere.cpp
//
// Desc: Creates geometry for a sphere
// 
// Hist: 11.11.02 - Cleaned up for December XDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "octosphere.h"




//-----------------------------------------------------------------------------
// Name: OctoSphereIndex()
// Desc: The octohedron is mapped to a square and layed-out in memory as follows:
//
//             DOMAIN                        MEMORY LAYOUT
//               -Y
//   -Z  +a0a1a2a3+a3a2a2a0+ -Z           Z0a0a1a2a3Y0b3b2b1b0
//      c0       /|\       d0              c0 . . . . . . . . .
//      c1     /  |  \     d1              c1 . . . . . . . . .
//      c2   /    |    \   d2              c2 . . . . . . . . .
//      c3 /   +Z |      \ d3              c3 . . . . . . . . .
//    -X +--------+--------+ +X              X0 . . . .Z1 . . . .
//      c3 \      |      / d3              d3 . . . . . . . . .
//      c2   \    |    /   d2              d2 . . . . . . . . .
//      c1     \  |  /     d1              d1 . . . . . . . . .
//      c0       \|/       d0              d0 . . . . . . . . .
//       +b0b1b2b3+b3b2b1b0+ -Z              X1 Y1               
//      -Z       +Y
//
// Where the a0=a0, b2=b2, etc. points are identical.
// The -Z axis is at all four corners.
// For the memory layout, the right column and bottom row are "chopped" off.
//   'b' values are packed next to 'a' values in the top row
//   'd' values are packed below 'c' values in the left column
//   X1 = +X and Y1=+Y are put at the end
//-----------------------------------------------------------------------------
inline int OctoSphereStandardIndex( int i, int j, int M )
{
    return (i + M) * M * 2 + j + M; // default indexing
}


inline int OctoSphereIndex( int i, int j, int M )
{
    if( i == -M )
    {
        if( j == -M || j == M )
            return 0; // -Z
        else if( j > 0 )
            return OctoSphereStandardIndex( i, -j, M ); // flipped 'a' value
        else
            return OctoSphereStandardIndex( i, j, M );
    }
    else if( i == M )
    {
        if( j == -M || j == M )
            return 0; // -Z
        else if( j == 0 )
            return 4*M*M + 1; // +Y
        else if( j < 0 )
            return OctoSphereStandardIndex( -M, -j, M ); // flipped packed 'b' value
        else // if( j > 0 )
            return OctoSphereStandardIndex( -M, j, M );   // packed 'b' value
    }
    else if( j == -M )
    {
        if( i == -M || i == M )
            return 0; // -Z
        else if( i > 0)
            return OctoSphereStandardIndex( -i, j, M ); // flipped 'c' value
        else
            return OctoSphereStandardIndex( i, j, M );
    }
    else if( j == M )
    {
        if( i == -M || i == M )
            return 0; // -Z
        else if( i == 0 )
            return 4*M*M; // +X
        else if( i < 0 )
            return OctoSphereStandardIndex( -i, -M, M ); // flipped packed 'd' value
        else // if (i > 0)
            return OctoSphereStandardIndex( i, -M, M ); // packed 'd' value
    }
    else
        return OctoSphereStandardIndex( i, j, M );
}




//-----------------------------------------------------------------------------
// Name: FillOctoSphere()
// Desc: Create a sphere as a subdivision of an octohedron.
//       Pass in NULL pointers to rVertex or rIndex to return needed sizes of buffers.
//-----------------------------------------------------------------------------
HRESULT FillOctoSphere( DWORD dwNumSplits,                             // Num splits on each edge, 0 = octohedron
                        DWORD* pdwNumVertices, D3DXVECTOR3* pVertices, // Vertices
                        DWORD* pdwNumIndices, WORD* pIndices )         // Triangle indices
{
    int M = dwNumSplits + 1;
    int N = M * 2; // Number of edges along one side of our domain
    if( pdwNumVertices )
        (*pdwNumVertices) = N * N + 2;
    if( pdwNumIndices  )
        (*pdwNumIndices) = 3 * 2 * N * N;

    // Fill in vertices based on faces of octohedron
    if( pVertices )
    {
        // TODO: Add chord -> angle correction to get better shaped triangles
        for( int i = -M; i < M; i++ )
        {
            for( int j = -M; j < M; j++ )
            {
                int i0, j0;

                // Handle special packing of boundaries
                if( i == -M && j > 0 )
                {
                    i0 = M;     // Flip i to handle 'b' boundary
                    j0 = j;
                }
                else if( j == -M && i > 0 )
                {
                    i0 = i;
                    j0 = M;    // Flip j to handle 'd' boundary
                }
                else
                {
                    // Standard case
                    i0 = i;
                    j0 = j;
                }
                int i1 = (i0 < 0) ? -i0 : i0;    // Flip y to positive quadrant
                int j1 = (j0 < 0) ? -j0 : j0;    // Flip x to positive quadrant
                int ix, iy, iz;
                if( i1 <= M - j1 )               // Test against x + y = M line
                {
                    ix = j0;
                    iy = i0;
                    iz = M - i1 - j1;
                }
                else // Flip z
                {
                    // Reflect x and y across x + y = M line
                    ix = M - i1;           // Swap x and y
                    iy = M - j1;
                    if( j0 < 0 ) ix = -ix; // Restore quadrant
                    if( i0 < 0 ) iy = -iy;
                    iz = M - i1 - j1;      // -M + (M - i1) + (M - j1);
                }

                D3DXVECTOR3 vPosition( (float)ix, (float)iy, (float)iz );
                D3DXVec3Normalize( &vPosition, &vPosition );
                *pVertices++ = vPosition;
            }
        }
        // Last two vertices are special cases
        *pVertices++ = D3DXVECTOR3( 1.0f, 0.0f, 0.0f ); // +X
        *pVertices++ = D3DXVECTOR3( 0.0f, 1.0f, 0.0f ); // +Y
    }

    // Fill in triangles, orienting triangle flips by quadrant
    if( pIndices )
    {
        for( int i = -M; i < M; i++ )
        {
            for( int j = -M; j < M; j++ )
            {
                if( (i < 0 && j < 0) || (i >= 0 && j >= 0) )
                {
                    *pIndices++ = (WORD)OctoSphereIndex( i+0, j+0, M );
                    *pIndices++ = (WORD)OctoSphereIndex( i+1, j+0, M );
                    *pIndices++ = (WORD)OctoSphereIndex( i+0, j+1, M );

                    *pIndices++ = (WORD)OctoSphereIndex( i+0, j+1, M );
                    *pIndices++ = (WORD)OctoSphereIndex( i+1, j+0, M );
                    *pIndices++ = (WORD)OctoSphereIndex( i+1, j+1, M );
                }
                else
                {
                    *pIndices++ = (WORD)OctoSphereIndex( i+0, j+0, M );
                    *pIndices++ = (WORD)OctoSphereIndex( i+1, j+0, M );
                    *pIndices++ = (WORD)OctoSphereIndex( i+1, j+1, M );

                    *pIndices++ = (WORD)OctoSphereIndex( i+0, j+0, M );
                    *pIndices++ = (WORD)OctoSphereIndex( i+1, j+1, M );
                    *pIndices++ = (WORD)OctoSphereIndex( i+0, j+1, M );
                }
            }
        }
    }
    return S_OK;
}
