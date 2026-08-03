//-----------------------------------------------------------------------------
// Name: Voice Loop Back Xbox Sample
// 
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   This sample demonstrates basic usage of the Xbox communicator.


Required files and media
========================
   Copy the media tree to the target machine before running this sample.


Programming Notes
=================
   This sample just monitors the microphone of each connected voice
   communicator and routes the data directly to that communicator's headphone.
   The main functionality is wrapped in the CLoopbackCommunicator class, which
   uses XMOs (Xbox Media Objects) to handle the audio data.

