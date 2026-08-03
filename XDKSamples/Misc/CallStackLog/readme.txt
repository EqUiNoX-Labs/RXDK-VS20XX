//-----------------------------------------------------------------------------
// Name: CallStackLog Xbox Sample
// 
// Copyright (c) 2002 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   The CallStackLog sample demonstrates how to capture the current call stack
   from a program running on the Xbox.  It generates a log file that is meant
   to be used in conjunction with the CallStackLookup sample, which is run on
   the development machine to look up the call stack names and generate a
   friendly printout.


Required files and media
========================
   Copy the media tree to the target machine before running this sample.


Programming Notes
=================
   DmCaptureStackBackTrace is used to collect an array of addresses for
   the current call stack.  DmWalkLoadedModules is used to iterate over
   the modules making up the XBE and determines their base addresses.
   This is the information that will be needed in order to look up
   the function names in the debug information on the development PC.
