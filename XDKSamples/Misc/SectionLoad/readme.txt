//-----------------------------------------------------------------------------
// Name: Section Load Sample
// 
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   This sample illustrates loading and unloading of sections.



Programming Notes
=================
   Section loading and unloading is something that will be used
   primarily in large projects where memory restrictions prevent you
   from having all code and data loaded simultaneously. Furthermore,
   flagging a section as NOPRELOAD only works if that section cannot
   be squeezed into a preloaded section. Thus, when I tried to write
   a couple of small routines that were loaded and unloaded, they were
   squeezed in to the end of a preloaded section and the code would
   always execute (as they were always in memory).

   As a result, this sample shows that section loading and unloading
   work properly by doing the following: If the data section is 
   loaded, write access to the data area will work just fine and
   the sample shows the time of the last successful access.
   If the data section is not loaded, accessing the data area will 
   generate an exception.
