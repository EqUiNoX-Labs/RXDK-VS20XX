//-----------------------------------------------------------------------------
// Name: XActCuePlayback demonstrates the general use of XACT to play
//       background music, ambient sound, as well as 3D positioned sound effects
// 
// Copyright (c) 2000 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   This sample loads two different wavebanks (one in-memory, the other streaming)
   and one soundbank.  The in-memory wavebank contains various sound effects.
   In this case, a helicopter rotor sound that loops, a gunshot/weapon sound, and
   a dialogue sample.  The streaming wavebank contains both music as well as longer
   sound effects (to demonstrate that sound effects can be streamed from disk as well).
   
   The sample also maintains information about 3 different sound sources (the helicopter,
   target and bystander) as well as the listener.  The various sound effects are tied to
   the the sound sources, while the music and ambient sounds are not.  Some of these
   sound effects are fire & forget sounds, while others are looping sounds, or sounds
   we may want more control over.  For instance, the bystander screams "yeah!", but
   should the character die in the game, we would want to stop the sound before it
   finished.  Here is a list of all the sources and the sounds tied to them:
   
   Target
   ------
     Explosion                  Fire and forget
     
   Helicopter
   ----------
     Helicopter (rotor sound)   Looping sound
     Gunshot                    Fire and forget
   
   Bystander
   ---------
     Oh Yeah!  (dialogue)       Fire, but maintain control
     
   No Source
   ---------
     BG Music 1/2               Fire
     Ambient sound              Looping background sound
     
   Most of these sounds are "autorelease" sounds (specified by the
   XACT_FLAG_SOUNDCUE_AUTORELEASE flag when calling IXACTSoundBank::Play).
   This means the XACT engine will clean up any memory associated with
   the cue once it is finished playing.  The non-autoreleased sounds
   (Ambient sound, Helicopter rotor sound, and the "Oh Yeah!" dialogue)
   are sounds that we need more control over.  The looping sounds will
   loop forever until we tell them to stop (or an error occurs).  The dialogue
   would usually be considered fire and forget.  However, let's assume that
   the character could die while speaking.  Hence, we would want to be able
   to control the playback of the dialogue, which is why the dialogue is a non-
   autorelease sound.
   
   The sample tracks all instances of cues as they are started and stopped.  For
   autoreleased cues, the sample only maintains a count.  For non-autoreleased
   cues, the sample maintains a queue of IXACTSoundCue pointers, which allows
   the sample to control specific sound cues.
   

Required files and media
========================
   Copy the media tree to the target machine before running this sample.


Programming Notes
=================

