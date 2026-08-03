//-----------------------------------------------------------------------------
// File: OctoSphere.h
//
// Desc: Creates geometry for a sphere
//  
// Hist: 11.11.02 - Cleaned up for December XDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <xtl.h>
 



//-----------------------------------------------------------------------------
// Name: FillOctoSphere()
// Desc: Create a sphere by sampling the faces of an octohedron and
//       renormalizing. Pass NULL pointers to pVertices or pIndices to return
//       needed sizes of buffers.
//
//       dwNumSplits is the # splits on each edge, where 0 = octohedron
//
//-----------------------------------------------------------------------------
HRESULT FillOctoSphere( DWORD dwNumSplits,
                        DWORD* pdwNumVertices, D3DXVECTOR3* pVertices,
                        DWORD* pdwNumIndices, WORD* pIndices );




