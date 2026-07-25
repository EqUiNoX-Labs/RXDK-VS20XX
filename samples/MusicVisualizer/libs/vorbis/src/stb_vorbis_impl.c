//------------------------------------------------------------------------------
// stb_vorbis implementation TU. Compiled in isolation (no XDK headers) so the
// third-party decoder stays self-contained; the wrapper in vorbis_stream.c calls
// into it through extern declarations. We decode from memory only.
//------------------------------------------------------------------------------
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API

#include "stb_vorbis.c" // resolved via includePaths: ["vendor"]
