//-----------------------------------------------------------------------------
// Name: Fuzzy Teapot Xbox Sample
// 
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   Shows a quick way of rendering fuzzy objects on the Xbox. This technique
   could also be used for simple grass and fur.


Required files and media
========================
   Copy the media tree to the target machine before running this sample.


Programming Notes
=================
   The technique used in this sample renders concentric shells of an object
   with textures that are cross-sections of fuzz (or fur, or grass, etc.). 
   The concentric shells are constructed in a vertex shader that simply offsets
   the position slightly in the direction of the vertex normal. The textures
   are pre-computed, and can be thought of as a 3D volume texture of the fuzz,
   so that each 2D slice is aligned with the surface of a concentric shell of
   the orginal object.
