//-----------------------------------------------------------------------------
// Name: Ripple Xbox Sample
// 
// Copyright (c) 2000 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   The Ripple sample uses a vertex shader to create a ripple effect.


Required files and media
========================
   Copy the media tree to the target machine before running this sample.


Programming Notes
=================
   The vertex shader used by this sample actually modulates the positions of
   the vertices that it processes. The sine wave used to position the vertices
   is coded in the vertex shader using the first few elements of the Taylor-
   series expansion for the sine function.

   The mesh is drawn in a vertex cache optimal fashion by drawing it in twenty 
   vertex wide swaths.  Twenty is optimal because the post-transform vertex 
   cache is twenty four verts in size, but up to six of the entries can be
   reserved for vertices that are currently being processed. The first twenty
   vertices in a swath are pre-cached using PrimeVertexCache(), then the swath 
   is drawn.  If the vertices were not pre-cached, then the swaths could only be
   about half as big because some of the vertices in the second (and subsequent) 
   row would pushed out of the cache before they could be re-used.
