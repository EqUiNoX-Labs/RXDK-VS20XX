//------------------------------------------------------------------------------
// vorbis_stream.c — the streaming wrapper the app uses.
//
// Reads the whole .ogg into RAM (the decoder streams PCM out of it) and pulls
// interleaved shorts on demand. stb_vorbis is called through extern declarations
// so this TU can include the XDK headers (for file I/O) without pulling the
// decoder source in with them.
//------------------------------------------------------------------------------
#include <xtl.h>
#include <stdlib.h>
#include <string.h>
#include "vorbis_stream.h"

// --- minimal stb_vorbis surface (matches stb_vorbis.c) ------------------------
typedef struct stb_vorbis stb_vorbis;
typedef struct {
    unsigned int sample_rate;
    int          channels;
    unsigned int setup_memory_required;
    unsigned int setup_temp_memory_required;
    unsigned int temp_memory_required;
    int          max_frame_size;
} stb_vorbis_info;
// stb_vorbis allocation arena. Passing a heap buffer makes the decoder carve all
// its memory from it instead of alloca()-ing large temporaries down a deep decode
// call chain — which would overflow the modest title stack on the first frame.
// Layout must match stb_vorbis.c's stb_vorbis_alloc { char* buffer; int length; }.
typedef struct { char* alloc_buffer; int alloc_buffer_length_in_bytes; } rxdk_stb_vorbis_alloc;
#define VORBIS_ARENA_BYTES (1024 * 1024)

extern stb_vorbis*     stb_vorbis_open_memory(const unsigned char* data, int len, int* error, void* alloc);
extern stb_vorbis_info stb_vorbis_get_info(stb_vorbis* f);
extern int             stb_vorbis_get_samples_short_interleaved(stb_vorbis* f, int channels, short* buffer, int num_shorts);
extern int             stb_vorbis_seek_start(stb_vorbis* f);
extern void            stb_vorbis_close(stb_vorbis* f);

struct VorbisStream {
    stb_vorbis*          v;
    unsigned char*       fileBuf;
    rxdk_stb_vorbis_alloc arena;
    int                  channels;
    int                  rate;
};

static unsigned char* ReadWholeFile(const char* path, DWORD* outLen)
{
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        return NULL;
    }
    DWORD len = GetFileSize(h, NULL);
    unsigned char* buf = (unsigned char*)malloc(len);
    DWORD got = 0;
    if (buf && ReadFile(h, buf, len, &got, NULL) && got == len) {
        CloseHandle(h);
        *outLen = len;
        return buf;
    }
    CloseHandle(h);
    free(buf);
    return NULL;
}

VorbisStream* VorbisOpenFile(const char* path)
{
    DWORD len = 0;
    unsigned char* ogg = ReadWholeFile(path, &len);
    if (!ogg) {
        return NULL;
    }

    VorbisStream* s = (VorbisStream*)malloc(sizeof(VorbisStream));
    if (!s) {
        free(ogg);
        return NULL;
    }
    memset(s, 0, sizeof(*s));
    s->fileBuf = ogg;
    s->arena.alloc_buffer = (char*)malloc(VORBIS_ARENA_BYTES);
    s->arena.alloc_buffer_length_in_bytes = VORBIS_ARENA_BYTES;
    if (!s->arena.alloc_buffer) {
        free(ogg);
        free(s);
        return NULL;
    }

    int err = 0;
    s->v = stb_vorbis_open_memory(ogg, (int)len, &err, &s->arena);
    if (!s->v) {
        free(s->arena.alloc_buffer);
        free(ogg);
        free(s);
        return NULL;
    }

    stb_vorbis_info info = stb_vorbis_get_info(s->v);
    s->channels = info.channels;
    s->rate = (int)info.sample_rate;
    return s;
}

int VorbisChannels(VorbisStream* s) { return s ? s->channels : 0; }
int VorbisSampleRate(VorbisStream* s) { return s ? s->rate : 0; }

int VorbisReadLooping(VorbisStream* s, short* out, int maxShorts)
{
    if (!s || !s->v || maxShorts <= 0) {
        return 0;
    }
    int got = stb_vorbis_get_samples_short_interleaved(s->v, s->channels, out, maxShorts);
    if (got <= 0) {
        stb_vorbis_seek_start(s->v);
        got = stb_vorbis_get_samples_short_interleaved(s->v, s->channels, out, maxShorts);
    }
    return got > 0 ? got * s->channels : 0;
}

void VorbisClose(VorbisStream* s)
{
    if (!s) {
        return;
    }
    if (s->v) {
        stb_vorbis_close(s->v);
    }
    free(s->arena.alloc_buffer);
    free(s->fileBuf);
    free(s);
}
