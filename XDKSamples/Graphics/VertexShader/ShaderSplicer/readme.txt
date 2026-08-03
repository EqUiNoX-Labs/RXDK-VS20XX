//-----------------------------------------------------------------------------
// Name: ShaderSplicer Xbox Sample
// 
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   Shows how to splice two vertex shaders together at runtime.


Required files and media
========================
   Copy the media tree to the target machine before running this sample.


Programming Notes
=================
   This sample shows how to use the AssembleShader function (courtesy of
   xgraphics.lib) to shader fragments together at run-time. The final shader is
   represented in ASCII-string form, so it can be built using regular string
   manipulation functions. Unfortunately, assembling shaders is a time-consuming
   process it is not recommended to splice shaders this way in a release build
   of a game. However, during development, the technique may be useful for rapid
   prototyping of shaders.