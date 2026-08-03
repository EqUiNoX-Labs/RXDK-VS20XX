//-----------------------------------------------------------------------------
// Name: Visibility Test Xbox Sample
// 
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   This samples shows how to use the visibility testing API on the Xbox.


Required files and media
========================
   Copy the media tree to the target machine before running this sample.


Programming Notes
=================
   Visibility testing is simply drawing an object and then asking the hardware
   how many pixels were drawn. If the object was drawn off-screen, or was
   occluded by another object in the z-buffer, then the number of pixels drawn
   will be zero. Apps would like to detect that case and avoid drawing the
   object in future frames.

   A common use for visibility testing is to disable writes to the frame-
   buffer, then render a bounding volume (or other highly-simplied
   represenation of a mesh) and see how many pixels were drawn. If the object
   would be visible, the app would then render the object for real.

   One caveat of using visibility testing is working around the inherit
   asynchronous nature of the CPU and the GPU. Whereas the CPU issues a draw
   command at time X, the GPU will not finish executing the draw command until
   some later time, Y. In some cases, Y can be much later than X, so apps must
   do visibility testing as early as possible and postpone rendering until
   results are known.
