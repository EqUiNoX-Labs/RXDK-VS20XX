//-----------------------------------------------------------------------------
// Name: DM Script Xbox Sample
// 
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   This sample shows how to play an audio script using DirectMusic. The theme
   of the script is a baseball game.


Required files and media
========================
   Copy the media tree to the target machine before running this sample.


Programming Notes
=================
   The sample loads a DirectMusic script using an IDirectMusicLoader8 object,
   and then shows how to call into that script using the
   IDirectMusicScript8::CallRoutine() interface.