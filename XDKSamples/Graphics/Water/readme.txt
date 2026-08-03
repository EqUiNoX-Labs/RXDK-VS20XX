//-----------------------------------------------------------------------------
// Name: Water Xbox Sample
// 
// Copyright (c) 2002 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   This sample demonstrates an Ocean water effect.
   

Required files and media
========================
   Copy the media tree to the target machine before running this sample.


Programming Notes
=================
   This sample shows how to do a very realistic ocean water effect using primarily
the pixel shaders.  The water has fresnel reflection, reflection distortion and 
refraction.  The App code is fairly simple.  The water sample draws the scene, 
the scene reflection and the scene refraction passes and then uses them to draw 
the water pass.