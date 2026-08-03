//-----------------------------------------------------------------------------
// Name: Debug Mouse Xbox Sample
// 
// Copyright (c) 2003 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   This sample demonstrates how to use the Debug Mouse
   

Required files and media
========================
   Copy the media tree to the target machine before running this sample.


Programming Notes
=================
   This sample shows how to use the Input API to use a mouse plugged into
the Xbox using the Debug Keyboard adapter.  You must use a USB mouse and the
Xbox supports many different kinds of both wireless and wired mice.  The API
allows you to poll the mouse position, the mouse wheel and the button states
for the left, right, wheel (or center button) and the two extra buttons on
the sides of most new Intellimouse mice.
   You can plug in up to four mice (One in each port) and the sample shows
how to handle plugging in and unplugging the mice from all of the ports.  It
doesn't try to handle moving or clicking four mice at once!!

