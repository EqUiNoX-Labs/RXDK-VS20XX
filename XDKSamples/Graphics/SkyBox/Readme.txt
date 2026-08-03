//-----------------------------------------------------------------------------
// Name: SkyBox Xbox Sample
// 
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   The SkyBox sample shows a cool artist trick where you can apply dynamic
   time-of-day lighting effects on a sky box.


Required files and media
========================
   Copy the media tree to the target machine before running this sample.


Programming Notes
=================
   The skybox is rendered with luminance-only (aka "gray scale") textures, and
   dynamic coloring is achieved through coloring specially places vertices in
   the skybox geometry.

   Note that we use the term "skybox" loosely here, as the underlying geometry
   could as well be a sphere, a cylinder, etc..

   In order that the programmer does not have to manually tweak vertices,
   the app uses a vertex shader which applies a tex coord value depending on
   the y-value of each skybox vertex. The tex coord then is used to lookup
   actual color values from a texture. 

   Since only one tex-coord is needed for the effect, the other can be used as
   a sliding scale for the time of day. For instance, to get different color
   gradients for a morning sky versus a high-noon sky.
