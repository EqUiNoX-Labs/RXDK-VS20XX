//-----------------------------------------------------------------------------
// Name: BackgroundMusic Xbox Sample
// 
// Copyright (c) 2000 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


Description
===========
   The BackgroundMusic sample demonstrates how one would play background WMA
music in an Xbox game.  This is done through the CMusicManager class, which 
encapsulates both soundtracks stored on the Xbox hard drive as well as WMA
files that ship with the game.  Note that the same functionality can be
acheived by using the XACT libraries, even if you're not using XACT for the
rest of your game audio.  See the XACTWMAPlayList sample for how to do this.
   The CMusicManager class uses 1 WMA decoder and 2 DirectSound streams to 
perform the playback.  When the decoder hits the end of the currently playing 
track, a new decoder is created to start decoding the next track while the end 
of the previous track is still playing.  It then performs a crossfade between 
the 2 streams to transition to the new track.  The duration of the crossfade 
is determined by the amount of decoded sound data that is buffers.  
   Since the 2 streams share the same audio buffer, the new stream could get 
starved as it is first starting up.  To avoid this, a small number of packets
are reserved for the new stream to use when it first starts playing.  
   The CMusicManager class uses the new WMA decoder which supports 
asynchronous I/O, allowing the decoding to happen on the title's main thread.
It's important to note that opening the file could still block.  If this delay
will cause a problem, then just the process of opening the file could be moved
to a separate thread.
CMusicManager::Process allows for packet processing to occur and can be called
from from the game's main thread, since File I/O inside the WMA decoder is
asynchronous.  However, it is prudent to note that decoding is done
synchronously, which may or may not be an issue depending on your game.
   

Required files and media
========================
   Copy the media tree to the target machine before running this sample.


Programming Notes
=================
   The user is limited to storing 100 soundtracks on their Xbox, which
means that we don't have to worry about using up too much memory when caching
soundtrack information.  Each soundtrack could have up to 500 songs in it,
which means it's not appropriate to attempt to cache every song in memory.

   The intent was to use as little CPU as possible.  As such, we only use
1 WMA decoder at a time, and use DLS envelopes to perform the crossfade.
