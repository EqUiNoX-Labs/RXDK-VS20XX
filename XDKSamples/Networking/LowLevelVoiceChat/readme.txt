//-----------------------------------------------------------------------------
// Name: LowLevelVoiceChat Xbox Sample
// 
// Copyright (c) 2000 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   The LowLevelVoiceChat sample demonstrates how a title can use the low-level
voice chat components (Microphone, Encoder, Voice Queue, Decoder, Headphone)
to build a drop-in custom voice engine.  All voice code is contained
inside the VoiceManager and VoiceCommunicator .cpp and .h files.  All
interaction between the title code and the voice subsystem is through the 
public interface of the CVoiceManager class and callback functions specified
when the voice subsystem is initialized.  In this way, the voice code can
be dropped in to any title with little or no changes.
   This sample also demonstrates how to support Voice Through Speakers by
manipulating the mixbins that a remote player's voice is sent to (see the
RecalculateMixBins() function).  In order to support this, the CVoiceManager
class has changed to operate more like XHV - AddChatter() should be called 
whenever a remote player is added to the session, regardless of whether or
not they have a communicator inserted.  This is necessary so that players
can mute and be muted, even if they do not have a communicator.  Then, whenever
a remote player inserts a communicator, ResetChatter() should be called so that
the voice manager can re-synchronize with that player's voice stream.  The 
DoesPlayerHaveVoice() function was removed, because the voice manager no longer
knows which remote chatters have voice or not (before, titles only called
AddChatter() for remote chatters that had a communicator inserted).
   

Required files and media
========================
   Copy the media tree to the target machine before running this sample.


Programming Notes
=================
   This code in this sample was previously the basis for the SimpleVoice 
sample.  With the addition of the Xbox High-Level Voice library (xhv.lib) in
the February 2003 XDK, the SimpleVoice sample was updated to demonstrate the
simplest way to do voice.  The voice engine from SimpleVoice was moved to
this sample, which is now the LowLevelVoiceChat sample.
