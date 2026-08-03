//-----------------------------------------------------------------------------
// Name: VSLights Xbox Sample
// 
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   Shows how to assemble vertex shaders on the fly.


Required files and media
========================
   Copy the media tree to the target machine before running this sample.


Programming Notes
=================
   The AssembleShader function is called to assemble shaders on the fly, as
   the user changes the number of lights that contribute to the scene.

   Unfortunately, the AssembleShader function is too time-consuming for real-
   time use in a shipping game. However, the function may be useful during
   development for rapid prototyping.
