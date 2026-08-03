//-----------------------------------------------------------------------------
// Name: FastLoad Xbox Sample
// 
// Copyright (c) 2002 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
    FastLoad asynchronously streams in 53MB levels from the DVD using non-
    buffered DMA and caches them on the hard disk utility region.  It also
    simultaneously streams in and plays a WMA file.

Required files and media
========================
    Copy the media tree to the target machine before running this sample.


Programming Notes
=================
    All IO uses non-buffered DMA.  Both types of asynchronous IO,
    threaded and overlapped, are demonstrated.
	
    The sample's frame rate actually increases during level loads since a 
    level is not being rendered during the load and non-buffered DMA does not
    waste CPU cycles (the IO thread blocks when it has to wait for IO to 
    complete and and non-buffered DMA does not involve the CPU for file
    cache copies). Also note that the frame rate never drops during any IO
    operation, allowing levels to be steamed in while other animations are
    playing.

    Non-buffered DMA reads and writes must be sector aligned, of sector sized
    bytes, and be targeted at DWORD aligned memory buffers.  The levels have a
    DVD sector aligned headers region (cached memory area) and a DVD sector
    aligned data region (write combined memory area).

    Level format:
    <headers> <raw data>

    The headers are read in first, followed by the raw data.
    IDirect3DResource8::Register is used to initialize the raw data into 
    DirectX 8 resources. The sample does not use any of the resource create
    functions, which would waste time copying memory.

    You may notice that the 53MB levels contain several copies of the same
    texture.  The bundler is used to copy the same textures over and over
    again into packed resources, vs. adding approximately 30 2.4MB textures
    to the XDK install size.

    The XCalculateSignature functions are used to detect a corrupt cache.  A
    signature is calculated on the level headers data and written after the
    level is cached (the sample writes the signature last so that if the box
    is turned off during a cache operation, the signature will not be
    present). If the signature is not present, does not match the level
    header, or the cached file or signature file sizes are not correct, the
    cached level is consider corrupt and automatically re-cached.  Since the
    sample does not calculate a signature over the level's raw data region,
    there is a -slight- chance that a level's raw data region will become
    corrupted while the level's file size, headers region, and signature
    remain intact.  In this case, the cached level must be deleted manually.

    The hard disk utility region's cluster size is set to 64k to reduce the
    size of the FAT (which reduces the number of FAT misses in the file 
    cache) by using the UDCLUSTER image builder flag.

    All IO requests packets (IRP) are for 128k size chunks of data.  In order
    to minimize the number of DVD seeks, the WMA data is steamed into 128k
    buffers. If smaller buffers were used, the DVD would be interrupted from
    loading the level to fetch more sound.  Larger buffers would waste memory.

    The terms "system memory" and "video memory" are used throughout the
    source code.  "Video memory" is continuous physically addressed
    write-combining memory that is used for GPU resources. "System memory" is
    virtually addressed cached memory that is used by the CPU.
	
    More detailed explanations on various asynchronous DMA IO "gotcha's" are
    documented next to their relevant sections in the source code under "NOTE:".

