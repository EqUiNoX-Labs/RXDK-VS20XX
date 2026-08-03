//-----------------------------------------------------------------------------
// Name: Rumble Xbox Sample
// 
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   This sample shows how to control the vibration motors on Xbox gamepads.
   

Required files and media
========================
   Copy the media tree to the target machine before running this sample.


Programming Notes
=================
   Controlling gamepad vibration is very easy to do, and just amounts to
   setting two integer fields (one per motor) in the gamepad's feedback
   structure.

   Note that "vibration" or "rumble" is very different from the "force
   feedback" that developers may be familiar with on the PC. The rumble
   effect on Xbox gamepads is simply two small DC motors with offset weights
   inside the controller. Although you can play back simple waveforms, like a
   sine pattern, the possible effects are very limited compared to force
   feedback.

