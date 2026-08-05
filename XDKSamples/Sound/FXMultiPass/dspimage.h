
#pragma once

typedef enum _DSP_IMAGE_dspimage_FX_INDICES {
    GraphZeroTempMixBins_ZeroTempMixbins0 = 0,
    UserLowFrequencyOscillator_LowFrequencyOscillator = 1,
    UserMonoEcho_MonoEcho = 2,
    UserDistortion_Distortion = 3,
    UserMonoAmpMod_MonoAmpMod = 4,
    UserMonoChorus_MonoChorus = 5,
    UserMonoFlange_MonoFlange = 6,
    GraphI3DL2_I3DL2Reverb = 7,
    GraphXTalk_XTalk = 8
} DSP_IMAGE_dspimage_FX_INDICES;

#define DSI3DL2_ENVIRONMENT_GraphI3DL2_I3DL2Reverb -1000, -100, 0.000000, 1.490000, 0.830000, -2602, 0.007000, 200, 0.011000, 100.000000, 100.000000, 5000.000000

typedef struct _GraphZeroTempMixBins_FX0_ZeroTempMixbins0_STATE {
    DWORD dwScratchOffset;        // Offset in bytes, of scratch area for this FX
    DWORD dwScratchLength;        // Length in DWORDS, of scratch area for this FX
    DWORD dwYMemoryOffset;        // Offset in DSP WORDS, of Y memory area for this FX
    DWORD dwYMemoryLength;        // Length in DSP WORDS, of Y memory area for this FX
    DWORD dwFlags;                // FX bitfield for various flags. See xgpimage documentation
    DWORD dwInMixbinPtrs[1];      // XRAM offsets in DSP WORDS, of input mixbins
    DWORD dwOutMixbinPtrs[8];     // XRAM offsets in DSP WORDS, of output mixbins
} GraphZeroTempMixBins_FX0_ZeroTempMixbins0_STATE, *LPGraphZeroTempMixBins_FX0_ZeroTempMixbins0_STATE;

typedef const GraphZeroTempMixBins_FX0_ZeroTempMixbins0_STATE *LPCGraphZeroTempMixBins_FX0_ZeroTempMixbins0_STATE;

typedef struct _UserLowFrequencyOscillator_FX0_LowFrequencyOscillator_STATE {
    DWORD dwScratchOffset;        // Offset in bytes, of scratch area for this FX
    DWORD dwScratchLength;        // Length in DWORDS, of scratch area for this FX
    DWORD dwYMemoryOffset;        // Offset in DSP WORDS, of Y memory area for this FX
    DWORD dwYMemoryLength;        // Length in DSP WORDS, of Y memory area for this FX
    DWORD dwFlags;                // FX bitfield for various flags. See xgpimage documentation
    DWORD dwOutMixbinPtrs[4];     // XRAM offsets in DSP WORDS, of output mixbins
} UserLowFrequencyOscillator_FX0_LowFrequencyOscillator_STATE, *LPUserLowFrequencyOscillator_FX0_LowFrequencyOscillator_STATE;

typedef const UserLowFrequencyOscillator_FX0_LowFrequencyOscillator_STATE *LPCUserLowFrequencyOscillator_FX0_LowFrequencyOscillator_STATE;

typedef struct _UserMonoEcho_FX0_MonoEcho_STATE {
    DWORD dwScratchOffset;        // Offset in bytes, of scratch area for this FX
    DWORD dwScratchLength;        // Length in DWORDS, of scratch area for this FX
    DWORD dwYMemoryOffset;        // Offset in DSP WORDS, of Y memory area for this FX
    DWORD dwYMemoryLength;        // Length in DSP WORDS, of Y memory area for this FX
    DWORD dwFlags;                // FX bitfield for various flags. See xgpimage documentation
    DWORD dwInMixbinPtrs[1];      // XRAM offsets in DSP WORDS, of input mixbins
    DWORD dwOutMixbinPtrs[1];     // XRAM offsets in DSP WORDS, of output mixbins
} UserMonoEcho_FX0_MonoEcho_STATE, *LPUserMonoEcho_FX0_MonoEcho_STATE;

typedef const UserMonoEcho_FX0_MonoEcho_STATE *LPCUserMonoEcho_FX0_MonoEcho_STATE;

typedef struct _UserDistortion_FX0_Distortion_STATE {
    DWORD dwScratchOffset;        // Offset in bytes, of scratch area for this FX
    DWORD dwScratchLength;        // Length in DWORDS, of scratch area for this FX
    DWORD dwYMemoryOffset;        // Offset in DSP WORDS, of Y memory area for this FX
    DWORD dwYMemoryLength;        // Length in DSP WORDS, of Y memory area for this FX
    DWORD dwFlags;                // FX bitfield for various flags. See xgpimage documentation
    DWORD dwInMixbinPtrs[1];      // XRAM offsets in DSP WORDS, of input mixbins
    DWORD dwOutMixbinPtrs[1];     // XRAM offsets in DSP WORDS, of output mixbins
} UserDistortion_FX0_Distortion_STATE, *LPUserDistortion_FX0_Distortion_STATE;

typedef const UserDistortion_FX0_Distortion_STATE *LPCUserDistortion_FX0_Distortion_STATE;

typedef struct _UserMonoAmpMod_FX0_MonoAmpMod_STATE {
    DWORD dwScratchOffset;        // Offset in bytes, of scratch area for this FX
    DWORD dwScratchLength;        // Length in DWORDS, of scratch area for this FX
    DWORD dwYMemoryOffset;        // Offset in DSP WORDS, of Y memory area for this FX
    DWORD dwYMemoryLength;        // Length in DSP WORDS, of Y memory area for this FX
    DWORD dwFlags;                // FX bitfield for various flags. See xgpimage documentation
    DWORD dwInMixbinPtrs[2];      // XRAM offsets in DSP WORDS, of input mixbins
    DWORD dwOutMixbinPtrs[1];     // XRAM offsets in DSP WORDS, of output mixbins
} UserMonoAmpMod_FX0_MonoAmpMod_STATE, *LPUserMonoAmpMod_FX0_MonoAmpMod_STATE;

typedef const UserMonoAmpMod_FX0_MonoAmpMod_STATE *LPCUserMonoAmpMod_FX0_MonoAmpMod_STATE;

typedef struct _UserMonoChorus_FX0_MonoChorus_STATE {
    DWORD dwScratchOffset;        // Offset in bytes, of scratch area for this FX
    DWORD dwScratchLength;        // Length in DWORDS, of scratch area for this FX
    DWORD dwYMemoryOffset;        // Offset in DSP WORDS, of Y memory area for this FX
    DWORD dwYMemoryLength;        // Length in DSP WORDS, of Y memory area for this FX
    DWORD dwFlags;                // FX bitfield for various flags. See xgpimage documentation
    DWORD dwInMixbinPtrs[2];      // XRAM offsets in DSP WORDS, of input mixbins
    DWORD dwOutMixbinPtrs[1];     // XRAM offsets in DSP WORDS, of output mixbins
} UserMonoChorus_FX0_MonoChorus_STATE, *LPUserMonoChorus_FX0_MonoChorus_STATE;

typedef const UserMonoChorus_FX0_MonoChorus_STATE *LPCUserMonoChorus_FX0_MonoChorus_STATE;

typedef struct _UserMonoFlange_FX0_MonoFlange_STATE {
    DWORD dwScratchOffset;        // Offset in bytes, of scratch area for this FX
    DWORD dwScratchLength;        // Length in DWORDS, of scratch area for this FX
    DWORD dwYMemoryOffset;        // Offset in DSP WORDS, of Y memory area for this FX
    DWORD dwYMemoryLength;        // Length in DSP WORDS, of Y memory area for this FX
    DWORD dwFlags;                // FX bitfield for various flags. See xgpimage documentation
    DWORD dwInMixbinPtrs[2];      // XRAM offsets in DSP WORDS, of input mixbins
    DWORD dwOutMixbinPtrs[1];     // XRAM offsets in DSP WORDS, of output mixbins
} UserMonoFlange_FX0_MonoFlange_STATE, *LPUserMonoFlange_FX0_MonoFlange_STATE;

typedef const UserMonoFlange_FX0_MonoFlange_STATE *LPCUserMonoFlange_FX0_MonoFlange_STATE;

typedef struct _GraphI3DL2_FX0_I3DL2Reverb_STATE {
    DWORD dwScratchOffset;        // Offset in bytes, of scratch area for this FX
    DWORD dwScratchLength;        // Length in DWORDS, of scratch area for this FX
    DWORD dwYMemoryOffset;        // Offset in DSP WORDS, of Y memory area for this FX
    DWORD dwYMemoryLength;        // Length in DSP WORDS, of Y memory area for this FX
    DWORD dwFlags;                // FX bitfield for various flags. See xgpimage documentation
    DWORD dwInMixbinPtrs[2];      // XRAM offsets in DSP WORDS, of input mixbins
    DWORD dwOutMixbinPtrs[35];     // XRAM offsets in DSP WORDS, of output mixbins
} GraphI3DL2_FX0_I3DL2Reverb_STATE, *LPGraphI3DL2_FX0_I3DL2Reverb_STATE;

typedef const GraphI3DL2_FX0_I3DL2Reverb_STATE *LPCGraphI3DL2_FX0_I3DL2Reverb_STATE;

typedef struct _GraphXTalk_FX0_XTalk_STATE {
    DWORD dwScratchOffset;        // Offset in bytes, of scratch area for this FX
    DWORD dwScratchLength;        // Length in DWORDS, of scratch area for this FX
    DWORD dwYMemoryOffset;        // Offset in DSP WORDS, of Y memory area for this FX
    DWORD dwYMemoryLength;        // Length in DSP WORDS, of Y memory area for this FX
    DWORD dwFlags;                // FX bitfield for various flags. See xgpimage documentation
    DWORD dwInMixbinPtrs[4];      // XRAM offsets in DSP WORDS, of input mixbins
    DWORD dwOutMixbinPtrs[4];     // XRAM offsets in DSP WORDS, of output mixbins
} GraphXTalk_FX0_XTalk_STATE, *LPGraphXTalk_FX0_XTalk_STATE;

typedef const GraphXTalk_FX0_XTalk_STATE *LPCGraphXTalk_FX0_XTalk_STATE;
