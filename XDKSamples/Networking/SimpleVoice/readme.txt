//-----------------------------------------------------------------------------
// Name: SimpleVoice Xbox Sample
// 
// Copyright (c) 2000 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   The SimpleVoice sample demonstrates how a title can use the sample voice
code provided as a "black box" voice subsystem.  All voice code is contained
inside the VoiceManager and VoiceCommunicator .cpp and .h files.  All
interaction between the title code and the voice subsystem is through the 
public interface of the CVoiceManager class and callback functions specified
when the voice subsystem is initialized.  In this way, the voice code can
be dropped in to any title with little or no changes.
   In August 2003, the sample was updated to show more of the complicated
issues that arise in voice.  It now supports live signon through UIX, and
demonstrates how to retrieve, update, and use mutelists.  It also supports
playback of voice through speakers, which was added to the June 2003 TCRs.
   The UI is meant only for informational and testing purposes, and is not
intended to be a recommended or reference UI model.
   

Required files and media
========================
   Copy the media tree to the target machine before running this sample.
   Run copytoxb.bat to copy the required fonts and skin files to the Xbox prior
   to running this sample.

Programming Notes
=================
   This sample updates the VoiceChat sample.  In the future, you should look
to this sample for details on voice implementation, as the VoiceChat sample
will be deprecated over time.

   Voice Through Speakers can be fairly complicated to implement.  The 
following is a guideline of how it should be implemented:
1) If a player who would normally be able to use voice (i.e., not a guest
    and not voice-banned) does not have a communicator plugged in, voice
    that player is supposed to hear should be sent through the speakers
2) If a player who would normally be able to use voice has a communicator
    plugged in, voice that player is supposed to hear should be sent to
    his/her headset.
3) Provide a UI option that allows the player to override this default
    behavior.
4) If any player on the console is voice-banned, NO voice may be played
    through the speakers, overriding #1 above.
5) If one player on the console has voice played through speakers, respect
    his/her mute settings.
6) If multiple players on the console have voice played through speakers,
    play the union of all voice that those players should hear.  That is,
    if a remote talker should be heard by any of the local players that
    are in voice through speakers mode, then they should be played through
    the speakers.