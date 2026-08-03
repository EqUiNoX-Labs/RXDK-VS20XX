//-----------------------------------------------------------------------------
// Name: XActBasicSound Xbox Sample
// 
// Copyright (c) 2000 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   The XActBasicSound sample demonstrates how to use the basic functionality
of the XACT tool and runtime engine. This sample does not include the graphics
framework. It is designed strictly to sample basic XACT functionality. For more
details on XACT tool and API, look in the XDK documentation.

   To get started with XACT, all you have to do is create the engine via 
XACTEngineCreate, and then call XACTEngineDoWork every frame.  From here, you
can connect the XACT tool on your PC and start auditioning waves and sounds.

   Loading content depends on the type of content:
   * In-memory wave banks: Load the entire wave bank from disk to a buffer in
        memory, and register the buffer with the XACT engine.
   * Streaming wave banks: Create a file handle, and register the file handle
        with the XACT engine.
   * Sound banks: Just like in-memory banks, load the entire sound bank from
        disk to a buffer in memory, and register the buffer with XACT.

   Currently, the XACT GUI tool does not produce header files with #defines
for sound cue indices, so you must look up cue indices by using their friendly
name.  Once you've got a sound cue index, you can call Play() on the sound
bank, passing in the cue index you wish to play.

   PLEASE, e-mail xboxds@xbox.com with your feedback on both the XACT GUI
tool and the runtime engine API.
   

Required files and media
========================
   Copy the media tree to the target machine before running this sample.


Programming Notes
=================
   You can use the XACT tool on your development system to audition waves and
sounds while running the XACTBasicSound sample, because it links with the 
instrumented (i) version of xacteng.lib.
