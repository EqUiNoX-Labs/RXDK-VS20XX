//-----------------------------------------------------------------------------
// Name: WaveBank Xbox Sample
// 
// Copyright (c) 2000 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   The WaveBank sample demonstrates how to use the new SetPlayRegion and
SetFormat APIs to implement a bank of wave files.  First, the XACT
tool is used to package all the wav assets into one wave bank (.XWB) file.  
Then, the sample loads that .XWB file and creates a pool of buffers, all 
mapped to the entire region of sample data.  
   When a sound is triggered, a buffer is retrieved from the pool, pointed at
the appropriate section of the wave bank, set with the correct format, and
played.  Triggered sounds can either be explicitly stopped, or periodically
polled to see when they have completed.  At that point, they are returned to
the pool of available buffers
   

Required files and media
========================
   Copy the media tree to the target machine before running this sample.


Programming Notes
=================
   The XACT tool can generate wave banks for in-memory usage, like this sample
illustrates, as well as wave banks that are meant to be streamed off the 
hard drive or DVD (see WaveBankStream sample).
