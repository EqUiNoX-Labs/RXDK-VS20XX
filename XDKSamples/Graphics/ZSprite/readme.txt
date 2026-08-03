//-----------------------------------------------------------------------------
// Name: ZSprite Xbox Sample
// 
// Copyright (c) 2000 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   The ZSprite sample demonstrates how to render a sprite with depth
information.  This is done by having one texture contain the color data for
the sprite (a normal 2d texture), and having another texture contain the depth
information.  

   The z value will be determined by the division of two dot products.  The 
numerator is a set of texture coordinates dotted with the vector 
< Ztex, Wtex, 1.0 >, and the denominator is a second set of texture coordinates
dotted with the same vector < Ztex, Wtex, 1.0 >.  Ztex is the upper 16 bits
resulting from the texture lookup, and Wtex is the lower 16 bits, both scaled
from 0.0 to 1.0.  This texture coordinates used for the dot product are
< 1.0, 0.0, 0.0 > for the numerator and < 0.0, 0.0, 1/D3DZ_MAX_D24S8 >. The
division then scales the Ztex value to 24 bits for a 24-bit depth buffer.
   

Required files and media
========================
   Copy the media tree to the target machine before running this sample.


Programming Notes
=================
   Z-sprites can be rendered in one pass, using all 4 texture stages: one for
the image data, one for the depth texture, and the remaining two for the DotZW
operation. Unfortunately, the fillrate with this setup is not good. Whereas the
hardware can optimally render 933MPix/s, with z-sprites you only get about
254MPix/s. To draw a full screen (640x480) z-sprite with no optimizations can
take several milliseconds.

   So, optimization is needed. The first optimization is to make the z-sprite
tightly bound around the image data. Any extra pixels is wasted fill. Second,
enable ALPHAKILL, since most pixels in a z-sprite are empty.

   Tiling also helps, as it does for any textures that are used as render
targets.

   Remarkably, a good performance gain can be realized by rendering z-sprites
in two passes. In the first pass, ignore the image data and just deal with the
z-texture. When the z-test fails, write a 1 into the stenicl. During the second
pass, draw the image texture as masked by the stencil. The use of only 3 texture
stages in the first pass actually frees up enough fill to afford the 2nd pass
and have some fill left over.

   Also note the effect of using 16-bit z-textures. In the one-pass method, the
framerate is worse, but in the two-pass rendering method, 16-bit z-textures are
faster.

   Finally, this sample originally used swizzled textures. It was switched to
use linear textures which make more sense for a 640x480 screen. However, if you
have need for only 256x256 (etc) textures, you could used swizzled rendertargets.
In fact, you could even use compressed textures. The only limit is that your
z-buffer must always be linear if you are going to render into it.


