//-----------------------------------------------------------------------------
// Name: HeatShimmer Xbox Sample
// 
// Copyright (c) 2002 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   The HeatShimmer sample uses a combination of a distortion texture and the
   depth buffer.  The distortion is applied per-pixel using screen space
   quads centered on the horizon.  The amount of distortion follows the formula
   below.

   Distortion (In Pixels) =
   Distortion Texture Value * (24 bit Z >> 16)^4 * Distortion Scale

Required files and media
========================
   Copy the media tree to the target machine before running this sample.


Programming Notes
=================
   The effect is a post processing technique, which makes it easy to "drop
   in" to an existing engine.  The first pass renders the (24 bit Z >> 16)^4
   into an off-screen buffer.  The second pass uses the existing color buffer 
   and new "Distortion * Z" texture to create the final distorted scene.
   The quads that are used to render the distorted color buffer stay centered
   on the horizon and fade the distortion to zero at the top and bottom of 
   the “distortion window”.
