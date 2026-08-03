//-----------------------------------------------------------------------------
// Name: WaveBankStream Xbox Sample
// 
// Copyright (c) 2002 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   The WaveBankStream sample shows how to use the new XACT wavebank file format
to efficiently stream wave data (of all formats, ADPCM, PCM and with minor code
changes even WMA). It uses non-buffered overlapped I/O to eliminate the need
for background threads while minimizing CPU usage in the context of the render
thread. It shows the optimal way to stream multiple streams, one for each entry
in the wavebank. The streams loop the entire play region right but for 
simplicity the use of a readahead packet to avoid stream starvation when looping
is not shown.

Required files and media
========================
   Copy the media tree to the target machine before running this sample.


Programming Notes
=================
    The sample issues sector aligned read requests to the async file XMO
(XFileMediaObjectAsync). Note that the wavebank file was opened just once and 
each time we want to stream a wave entry, we just instantiate the source file XMO
using the same handle and setting the file offset to the proper wave region. The
wavebank authoring tool (XACT.EXE) creates sector aligned play regions making
efficient streaming alot easier compared to .WAV files. Wavebanks are the 
recommended package for audio assets. They can be authored as in-memory or 
streamed. Note that the XACT wavebank format is different than the old 
wavebundler format that only supported in-memory wavebanks.
