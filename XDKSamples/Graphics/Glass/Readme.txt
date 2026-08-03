//-----------------------------------------------------------------------------
// Name: Glass Xbox Sample
// 
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   The Glass samples shows how to render 3D objects so they look like glass
   that both refracts and refracts light.


Required files and media
========================
   Copy the media tree to the target machine before running this sample.


Programming Notes
=================
   Reflection is nothing new, so this sample is really showing off an
   approximation of refraction. Note that this is a very gross approximation,
   as real refraction would need knowledge of each light ray's entry point and
   exit point into and out of the model. Here we approximate refraction by
   using the surface normal to lookup a perturbed normal in a pre-computed
   cubemap, and using that perturbed normal to lookup a value in our environment
   map.
