//-----------------------------------------------------------------------------
// Name: HighQualityBumpMapping
// 
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
  The HighQualityBumpMapping sample shows how to perform high quality per-pixel
  Blinn-Phong lighting with bump mappping, per-pixel normalization of the
  half-angle vector, and an arbitrary specular exponent. Not doing per-pixel 
  normalization of the half-angle vector results in obvious artifacts unless the
  geometry is extrememly finely tessalated. Normalizing the half-angle vector 
  using a normalization cube map and then computing the specular function in the 
  color combiners requires the use of an approximation for non power-of-two 
  exponents and results in artifacts becuase of the limited precision available.


Required files and media
========================
   Copy the media tree to the target machine before running this sample.


Programming Notes
=================
  The technique uses texm3x2tex dependant texture lookup operation to lookup 
  the result of the dot product (N dot H) with per-pixel normalization of the H
  vector. The idea is to lookup in a 2D texture with u = (N dot H) and 
  v = (H dot H/|H|). (H dot H/|H|) is equal to the length of H. We get H/|H| or 
  the normalized H from a normalization cube map lookup. The best results are 
  obtained by using a cube map with V16U16 values and the hemisphere dot 
  mapping mode. The 2D texture encodes (N dot H) to a power normalized for the 
  length of H. The N vector comes from a normal map. The normal map may be an 
  RGB texture, a V16U16 texture for high accuracy, or a V8U8 texture for 
  compactness. Note that using the hemisphere dot mapping mode with a V16U16 
  or V8U8 texture only works when doing tangent space bump mapping.

  The diffuse component of the lighting equation is computed with the color 
  combiners using an approximation to the re-normalization of the L vector and 
  the dot product instruction. The technique used for the specular could be 
  used for the diffuse in a seperate rendering pass, but it rarely results in 
  any improvement in quality.

  Because the technique requires all four texture stages the most likely way to 
  use it is to render the diffuse color texture and diffuse normal texture map 
  in one pass, then add the high-quality specular in a seperate pass.
