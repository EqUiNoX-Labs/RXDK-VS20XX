//-----------------------------------------------------------------------------
// Name: CrashDump Xbox Sample
// 
// Copyright (c) 2000 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   The CrashDump sample shows how the Xbox crash dump system works.
   
Instructions
============
   Make sure your Xbox is using the August 2002 or later version of xbdm. If
   you aren't sure, run the August 2002 or later XDKRecovery application to
   update your Xbox.
   
   Make sure you don't have a kernel debugger (such as KD or WinDBG)
   connected to your Xbox. (If you do, the bug check will be caught by
   that debugger, rather than generate a crash dump.)
   
   Build and run the CrashDump sample.
   
   Note that the "Vertical Blank Count" field on the Xbox screen is incrementing.
   
   Press the "A" button on your Xbox game controller to cause the application
   to bug check (by dereferencing a null pointer during a vertical blank interrupt
   callback.)
   
   Note that the "Vertical Blank Count" field is no longer incrementing.
   
   When you press the "A" button, the application will bug check. If you had a
   kernel debugger (such as KD or WinDBG) connected to your Xbox, you would get
   a breakpoint in that debugger that would allow you to debug the bug check.
   
   Since you don't have a kernel debugger connected to your Xbox, xbdm will write
   out a crash dump that you can use to debug the problem using Visual Studio .NET.
   
   It takes about two minutes for the Xbox to write a crash dump and return to the
   XDK Launcher. During that time the Xbox's LEDs will be flashing various red and
   green patterns.
   
   If you happened to have the Visual Studio .NET debugger attached when the crash
   occurred, the VS.NET debugger will eventually notice that the Xbox has crashed,
   and end the debugging session.
   
   After the Xbox finishes writing the crash dump, it will return to the XDK Launcher
   application. When this happens, you should copy the crash dump file from the Xbox
   to the development PC. The crash dump file is located here:
   
     xe:\crashdump.xdmp
   
   To make it easier for Visual Studio .NET to find the symbols for the application,
   you should copy the crash dump to the same directory as your application's PDB file.
   Depending upon where you installed your XDK, for the CrashDump sample the path
   will be something like:
   
    C:\Program Files\Microsoft Xbox SDK\Samples\Xbox\Misc\CrashDump\Debug
    
   To debug the crash dump:
  
   1) Start Visual Studio .NET
   2) Choose "File:Open Project" from the Visual Studio .NET menus.
   3) Open the crashdump.xdmp file.
   4) Press the function key "F5" to begin debugging the crash dump.
   
   The first thing you'll see is a dialog box describing what caused the crash dump.
   Then you can use the standard debugger features (like the stack crawl window) to
   determine what caused the crash.
   
Required files and media
========================
   Copy the media tree to the target machine before running this sample.


Programming Notes
=================

