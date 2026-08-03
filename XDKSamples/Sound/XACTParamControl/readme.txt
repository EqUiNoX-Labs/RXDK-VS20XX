//-----------------------------------------------------------------------------
// Name: XActParamControl Xbox Sample
// 
// Copyright (c) 2002 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   The XActParamControl sample demonstrates how to use the XACT parameter contols
   to manage sound content. This is done by using several samples of a car engine
   at recorded at different RMPs and then using the parameter controls to adjust
   the volume, pitch, and filter cutoff dynamically in game.

   
Required files and media
========================
   Copy the media tree to the target machine before running this sample.


Programming Notes
=================

Numerous situations arise in which a developer needs to exert additional control over game sounds. For instance, in a driving game, the sound of the engine 
needs to respond to dynamic values such as RPMs and gear. XACT provides several methods for accessing this more dynamic control method.

First of all, the content creator can group sounds into various categories. The developer can then, at run time, manipulate the volume and the pause/resume 
state of specific categories. Volume manipulation via a category (IXACTEngine::SetMasterVolume) is generally reserved for the kinds of game "mixing" controls 
that are often exposed to the end user. Games should typically have relatively few categories, perhaps as simple as the corresponding user-exposed sliders 
often labeled "FX," "Dialog," and "Music." 

Categories should not typically be used for emulating distance attenuation. Also note that categories cannot be used to "amplify" content-authored attenuation. 
Category volume is applied additively with content-authored volumes, so the relative mix of sounds within a single category will remain intact even as that 
category's "global" volume is reduced. The pause/resume state for a category can be manipulated via IXACTEngine::GlobalPause. Again, this is often useful when 
the player switches to a game menu (or otherwise pauses the game). Perhaps everything is paused except for the musical score or the ambience.

Run-time parameter controls (RPCs, also known as sliders) provide for more acute dynamic control of audio behavior. How a specific sound responds to a RPC's 
value is determined by the content author; it might crossfade between two waves, pitch shift the sound up or down, apply filtering, and so on. The RPC is then 
adjusted by the developer in response to game state (IXACTEngine::SetParameterControl). Categories are also exposed here, with the added benefit of sequencing 
parameter sweeps: you can set any parameter to change to an arbitrary value smoothly over a specified amount of time.


NOTE: There is no concept of a "default" RPC value. In a cue instance's initial state, all tracks of a sound would be played without any RPC curves being 
applied. You can apply a value via SetParameterControl right after the cue starts playing, but there's always the chance that the wave might briefly play 
without your RPC settings. To avoid this, use this process to play any cues whose sounds use RPCs:

1) Call IXACTSoundBank::Prepare on the cue, which returns you a cue instance (ppSoundCue). 
2) Call IXACTEngine::SetParameterControl on this cue instance. 
3) Call XACTEngineDoWork. 

The cue instance is now ready to be played (via IXACTSoundBank::Play) with your initial slider values applied.

