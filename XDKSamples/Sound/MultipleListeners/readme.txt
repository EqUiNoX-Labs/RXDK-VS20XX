//-----------------------------------------------------------------------------
// Name: MultipleListeners Xbox Sample
// 
// Copyright (c) 2003 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   The MultipleListeners sample demonstrates how to create multiple independent
virtual listeners using DirectSound.  There are few concepts and terms that
need to be defined:

"Real" Listener - This is the standard DirectSound 3D Listener.  For simplicity,
    we leave the real listener at the origin.
"Virtual" Listeners - These are the listeners that we actually want to position
    throughout our game world, usually corresponding to players.
"Real" Source - This is the actual source object, positioned in standard 
    world coordinates.
"Virtual" Source - This is a copy of the real source object, but with it's
    position calculated relative to its corresponding virtual listener.
    
   Whenever a real source moves, we must update the position of each
virtual source by re-calculating the position of the real source relative
to each virtual listener.  Whenever a virtual listener moves, we must 
re-calculate the position of EVERY real source relative to that virtual
listener.

   Two modes are presented in the sample - all-listener mode and 
closest-listener mode.  In all-listener mode, one virtual source is created
and positioned relative to each virtual listener.  All the virtual sounds
are then mixed together so that the final speaker output is a mix of what
each virtual listener would be hearing.  There are two main drawbacks to
this approach:
1) It uses more hardware voices, since you must allocate one buffer/stream
per sound source per listener.
2) Since each buffer/stream will have a separate velocity, they will likely
get out of sync due to doppler shifting and create a flange effect.  You can
avoid this by only using all-listener for short, one-shot sounds and/or not
setting velocity on the virtual sounds.
   In closest-listener mode, there is only one virtual source which is
positioned relative to whichever virtual listener is closest to the real
source.  The primary drawback of this approach is that it can result in
discontinuities when a different virtual listener becomes the new closest
listener.  You should experiment with these (and possibly other) techniques 
to determine what sounds best for your game.
   
Required files and media
========================
   Copy the media tree to the target machine before running this sample.


Programming Notes
=================

