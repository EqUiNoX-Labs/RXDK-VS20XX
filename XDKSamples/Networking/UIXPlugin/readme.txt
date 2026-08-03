//-----------------------------------------------------------------------------
// Name: UIXPlugin
// 
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   The UIXPlugin sample shows how to implement a custom UI plugin that works
   with the UIX drop-in UI library.


Required files and media
========================
   Run copytoxb.bat to copy the required fonts and skin files to the Xbox prior
   to running this sample.


Programming Notes
=================
   The UIX library provides a complete drop-in UI for Xbox Live authentication
   (signing on) and friends list. However, many developers will want to
   customize the UI rendering to better match their game. This sample basically
   exposes the source code for a UI plugin, which can be used as a starting
   point to work with any game. For starters, all games using a custom UI plugin
   will want to tie the plugin into their own font code, and possbily their own
   resource loading and management code as well.

   Be aware that using your own custom UI plugin means you will not pick up any
   bug fixes for the default UI plugin that may appear in later XDK releases.
