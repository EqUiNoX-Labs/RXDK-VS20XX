//-----------------------------------------------------------------------------
// Name: XMVPlayer Xbox Sample
// 
// Copyright (c) 2002 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   The XMVPlayer sample demonstrates various ways of playing XMV movies
   in your game.
   

Required files and media
========================
   Copy the media tree to the target machine before running this sample.


Programming Notes
=================

   XMV playback has a few principle variations:
       Playing to a texture or to the overlay planes
       Using the packet interface to read the file, using packets to copy from
       memory, or letting XMV do the reading.
       Using Play() to play the entire movie, or GetNextFrame
       Unpacking to an RGB or YUV texture
       Playing full screen or on just part of the screen

   If these could be combined arbitrarily this would give us dozens of combinations.
   A few of the combinations don't make sense - overlay planes are always YUV,
   Play() always uses the overlay planes, etc.

   Many of these variations - such as using CreateDecoderForFile versus
   CreateDecoderForPackets - do not affect other aspects of playback, so the
   different variations can be mixed without difficulty.

   This sample tries to show all sensible combinations of these possibilities

   When playing to an overlay plane this sample also demonstrates placing other
   graphics above the movie being played.

   The playback logic is encapsulated in the XMVHelper class, so the actual
   playback process is pretty simple.
