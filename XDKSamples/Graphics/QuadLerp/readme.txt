//-----------------------------------------------------------------------------
// Name: QuadLerp experimental code
// 
// Copyright (c) 2000 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   QuadLerp does a linear interpolation between four textures based on the
   specular color.

   Demonstrates the optimizing of pixel shaders. The pixel shaders
   show several implementations of the same pixel shader, with varying
   degrees of optimization.

   The shaders all implement the same effect, which is to linearly
   interpolate between four textures based on the r, g, b, and a
   components of the specular color.

   This sample is a companion to the "Pixel Shader Optimizing" whitepaper.
   
   The left thumbstick can be used to increase the emphasis on a particular
   texture.

   Pixel shaders 2 and 3 have slightly different behavior. Pixel shaders 0 and
   1 are identical, except that pixel shader 1 runs slightly faster.
   If you do a PIX capture of these two shaders you will see that the ideal
   fill rate for pixel shader one is double the ideal fill rate for pixel
   shader two.

   If the textures are setup correctly - with mip-mapping and texture compression -
   so that texture fetch bandwidth doesn't interfere, then the optimized pixel
   shaders run almost twice as fast as the unoptimized version.
