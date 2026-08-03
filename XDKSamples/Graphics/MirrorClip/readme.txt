//-----------------------------------------------------------------------------
// Name: MirrorClip Xbox Sample
// 
// Copyright (c) 2000 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   The MirrorClip sample illustrates an innovative way of clipping a scene to
   achieve a mirror effect.


Required files and media
========================
   Copy the media tree to the target machine before running this sample.


Programming Notes
=================
   Since user clipplanes are not available on Xbox, this sample shows how to
   use the view frustum and a stencilbuffer to clip to a mirror plane.