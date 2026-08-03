//-----------------------------------------------------------------------------
// Name: FastVSConstants
// 
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   The FastVSConstants sample shows how to use the IDirect3DDevice8::BeginState 
   and  Direct3DDevice8::EndState APIs to efficiently store calculated vertex 
   shader constant data directly into the push-buffer.  It also illustrates the 
   importance of pre-fetching data in order to achieve maximum CPU performance.

Required files and media
========================
   Copy the media tree to the target machine before running this sample.


Programming Notes
=================
   The sample draws 20,000 simple objects (tetrahedrons) per frame. The data 
   needed to draw each object is prefetched to avoid cache misses. The "A" 
   button toggles between the simple drawing loop and the optimized one. The 
   optimized version is approximately 68% faster the non-optimized version. 
   Most of the speed increase (~90%) is due to pre-fetching the data needed to 
   draw each object. The rest (~10%) can be attributed to inlining the matrix 
   multiply and writing the vertex shader constants directly into the 
   push-buffer. Avoiding cache misses while writing data into write-combined 
   memory such as the push-buffer is doubly important because a cache miss can 
   interrupt the write combining resulting in additional bus cycles. The frame 
   rate is 100% CPU bound when using the non-optimized drawing loop. When using 
   the optimized drawing loop the frame rate is GPU bound with the CPU usage at 
   about 90%.

TODO
====
