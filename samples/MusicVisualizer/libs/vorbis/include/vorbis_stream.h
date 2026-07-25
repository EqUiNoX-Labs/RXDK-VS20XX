//------------------------------------------------------------------------------
// vorbis_stream.h — public interface of the vorbis library.
//
// A tiny streaming Ogg Vorbis decoder wrapper over stb_vorbis: open a .ogg file,
// then pull interleaved 16-bit PCM in chunks (looping at end-of-stream). The app
// feeds those chunks to DirectSound and analyzes them for the visualizer.
//
// Exported to the app via publicIncludePaths; the stb_vorbis source itself stays
// private to the library.
//------------------------------------------------------------------------------
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VorbisStream VorbisStream;

// Open an .ogg from the title filesystem (e.g. "D:\\media\\music.ogg"). Returns
// NULL on failure.
VorbisStream* VorbisOpenFile(const char* path);

int VorbisChannels(VorbisStream* s);
int VorbisSampleRate(VorbisStream* s);

// Decode up to maxShorts interleaved 16-bit samples into out; returns the number
// of shorts written. Loops back to the start at end-of-stream (so it never ends).
int VorbisReadLooping(VorbisStream* s, short* out, int maxShorts);

void VorbisClose(VorbisStream* s);

#ifdef __cplusplus
}
#endif
