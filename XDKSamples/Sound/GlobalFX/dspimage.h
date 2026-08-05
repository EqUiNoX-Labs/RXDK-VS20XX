
#pragma once

typedef enum _DSP_IMAGE_dspimage_FX_INDICES {
    GraphZeroTempMixBins_ZeroTempMixbins0 = 0,
    UserLowFrequencyOscillator_LowFrequencyOscillator = 1,
    UserStereoEcho_StereoEcho = 2,
    UserDistortion_1_Distortion_1 = 3,
    UserDistortion_2_Distortion_2 = 4,
    UserStereoChorus_StereoChorus = 5,
    UserStereoAmpMod_StereoAmpMod = 6,
    UserStereoFlange_StereoFlange = 7
} DSP_IMAGE_dspimage_FX_INDICES;

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

typedef struct _UserStereoEcho_FX0_StereoEcho_STATE {
    DWORD dwScratchOffset;        // Offset in bytes, of scratch area for this FX
    DWORD dwScratchLength;        // Length in DWORDS, of scratch area for this FX
    DWORD dwYMemoryOffset;        // Offset in DSP WORDS, of Y memory area for this FX
    DWORD dwYMemoryLength;        // Length in DSP WORDS, of Y memory area for this FX
    DWORD dwFlags;                // FX bitfield for various flags. See xgpimage documentation
    DWORD dwInMixbinPtrs[2];      // XRAM offsets in DSP WORDS, of input mixbins
    DWORD dwOutMixbinPtrs[2];     // XRAM offsets in DSP WORDS, of output mixbins
} UserStereoEcho_FX0_StereoEcho_STATE, *LPUserStereoEcho_FX0_StereoEcho_STATE;

typedef const UserStereoEcho_FX0_StereoEcho_STATE *LPCUserStereoEcho_FX0_StereoEcho_STATE;

typedef struct _UserDistortion_1_FX0_Distortion_1_STATE {
    DWORD dwScratchOffset;        // Offset in bytes, of scratch area for this FX
    DWORD dwScratchLength;        // Length in DWORDS, of scratch area for this FX
    DWORD dwYMemoryOffset;        // Offset in DSP WORDS, of Y memory area for this FX
    DWORD dwYMemoryLength;        // Length in DSP WORDS, of Y memory area for this FX
    DWORD dwFlags;                // FX bitfield for various flags. See xgpimage documentation
    DWORD dwInMixbinPtrs[1];      // XRAM offsets in DSP WORDS, of input mixbins
    DWORD dwOutMixbinPtrs[1];     // XRAM offsets in DSP WORDS, of output mixbins
} UserDistortion_1_FX0_Distortion_1_STATE, *LPUserDistortion_1_FX0_Distortion_1_STATE;

typedef const UserDistortion_1_FX0_Distortion_1_STATE *LPCUserDistortion_1_FX0_Distortion_1_STATE;

typedef struct _UserDistortion_2_FX0_Distortion_2_STATE {
    DWORD dwScratchOffset;        // Offset in bytes, of scratch area for this FX
    DWORD dwScratchLength;        // Length in DWORDS, of scratch area for this FX
    DWORD dwYMemoryOffset;        // Offset in DSP WORDS, of Y memory area for this FX
    DWORD dwYMemoryLength;        // Length in DSP WORDS, of Y memory area for this FX
    DWORD dwFlags;                // FX bitfield for various flags. See xgpimage documentation
    DWORD dwInMixbinPtrs[1];      // XRAM offsets in DSP WORDS, of input mixbins
    DWORD dwOutMixbinPtrs[1];     // XRAM offsets in DSP WORDS, of output mixbins
} UserDistortion_2_FX0_Distortion_2_STATE, *LPUserDistortion_2_FX0_Distortion_2_STATE;

typedef const UserDistortion_2_FX0_Distortion_2_STATE *LPCUserDistortion_2_FX0_Distortion_2_STATE;

typedef struct _UserStereoChorus_FX0_StereoChorus_STATE {
    DWORD dwScratchOffset;        // Offset in bytes, of scratch area for this FX
    DWORD dwScratchLength;        // Length in DWORDS, of scratch area for this FX
    DWORD dwYMemoryOffset;        // Offset in DSP WORDS, of Y memory area for this FX
    DWORD dwYMemoryLength;        // Length in DSP WORDS, of Y memory area for this FX
    DWORD dwFlags;                // FX bitfield for various flags. See xgpimage documentation
    DWORD dwInMixbinPtrs[3];      // XRAM offsets in DSP WORDS, of input mixbins
    DWORD dwOutMixbinPtrs[2];     // XRAM offsets in DSP WORDS, of output mixbins
} UserStereoChorus_FX0_StereoChorus_STATE, *LPUserStereoChorus_FX0_StereoChorus_STATE;

typedef const UserStereoChorus_FX0_StereoChorus_STATE *LPCUserStereoChorus_FX0_StereoChorus_STATE;

typedef struct _UserStereoAmpMod_FX0_StereoAmpMod_STATE {
    DWORD dwScratchOffset;        // Offset in bytes, of scratch area for this FX
    DWORD dwScratchLength;        // Length in DWORDS, of scratch area for this FX
    DWORD dwYMemoryOffset;        // Offset in DSP WORDS, of Y memory area for this FX
    DWORD dwYMemoryLength;        // Length in DSP WORDS, of Y memory area for this FX
    DWORD dwFlags;                // FX bitfield for various flags. See xgpimage documentation
    DWORD dwInMixbinPtrs[3];      // XRAM offsets in DSP WORDS, of input mixbins
    DWORD dwOutMixbinPtrs[2];     // XRAM offsets in DSP WORDS, of output mixbins
} UserStereoAmpMod_FX0_StereoAmpMod_STATE, *LPUserStereoAmpMod_FX0_StereoAmpMod_STATE;

typedef const UserStereoAmpMod_FX0_StereoAmpMod_STATE *LPCUserStereoAmpMod_FX0_StereoAmpMod_STATE;

typedef struct _UserStereoFlange_FX0_StereoFlange_STATE {
    DWORD dwScratchOffset;        // Offset in bytes, of scratch area for this FX
    DWORD dwScratchLength;        // Length in DWORDS, of scratch area for this FX
    DWORD dwYMemoryOffset;        // Offset in DSP WORDS, of Y memory area for this FX
    DWORD dwYMemoryLength;        // Length in DSP WORDS, of Y memory area for this FX
    DWORD dwFlags;                // FX bitfield for various flags. See xgpimage documentation
    DWORD dwInMixbinPtrs[3];      // XRAM offsets in DSP WORDS, of input mixbins
    DWORD dwOutMixbinPtrs[2];     // XRAM offsets in DSP WORDS, of output mixbins
} UserStereoFlange_FX0_StereoFlange_STATE, *LPUserStereoFlange_FX0_StereoFlange_STATE;

typedef const UserStereoFlange_FX0_StereoFlange_STATE *LPCUserStereoFlange_FX0_StereoFlange_STATE;
