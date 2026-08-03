//-----------------------------------------------------------------------------
// Name: Pan3D Xbox Sample
// 
// Copyright (c) 2000 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
    This sample demonstrates how to use a panning algorithm and 
the DirectSOund SetMixbinVolumes API to 3D position a sound source.
It allows the game to use 2D  voices and also utilize N speakers (N>4)
for positioning. Two algorithms, of different CPU overhead are provided.


Required files and media
========================
   Copy the media tree to the target machine before running this sample.


Programming Notes
=================
   Sounds are created using DirectSound and the helper class in the 
XBSound.h/XBSound.cpp files.  The helper class just provides easy access to 
the wav file to get format information and read sample data from the file.
   The sample by default sends sounds to the crosstalk mixbins.  Therefore, 
in order to get sound from those mixbins to the speakers, you must have a 
DSP image loaded that contains at least the crosstalk code.  See the
DownloadScratch() function for how to do this.

