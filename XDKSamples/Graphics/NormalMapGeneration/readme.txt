//-----------------------------------------------------------------------------
// Name: NormapMap Generation
// 
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   The NormalMap Generation example shows how to create a normal map for a low 
   resolution mesh that approximates a high resolution mesh.


Required files and media
========================
   Copy the media tree to the target machine before running this sample.


Programming Notes
=================
   The sample requires two meshes, one high-resolution mesh and one lower 
   resolution mesh.  The low-resolution mesh must have a texture with unique
   texture coordinates.  The normals are computed by shooting rays from the
   surface along the normal of the low-resolution mesh and intersecting the 
   rays with the high-resolution mesh.  The normal (and potentially other 
   parameters) are sampled at the intersection point.  The calculated normal 
   maps are saved on the target machine in the "Media\Textures" directory.


TODO
====
