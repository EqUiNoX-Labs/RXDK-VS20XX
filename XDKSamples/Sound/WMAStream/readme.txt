//-----------------------------------------------------------------------------
// Name: WMAStream Xbox Sample
// 
// Copyright (c) 2000 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   The WMAStream sample shows how to use the asynchronous WMA decoder XMO.  This
   WMA decoder can be used without maintaining a separate thread for processing
   data packets.
   

Required files and media
========================
   Copy the media tree to the target machine before running this sample.


Programming Notes
=================
   In this sample, we create the asynchronous decoder to process packets
asynchronously, without the need for us to maintain our own thread.  Processing
still needs to occur frequently to keep the stream from starving.
