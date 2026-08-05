// Xbox Shader Assembler 1.00.5849.1
// Generated from extracthot.psh -- regenerate with: xsasm extracthot.psh out.xpu
D3DPIXELSHADERDEF psd;
ZeroMemory(&psd, sizeof(psd));
psd.PSAlphaInputs[0]=0x58301010;
psd.PSFinalCombinerInputsABCD=0x0000000c;
psd.PSFinalCombinerInputsEFG=0x00001c80;
psd.PSAlphaOutputs[0]=0x000000c0;
psd.PSRGBInputs[0]=0x48200000;
psd.PSRGBOutputs[0]=0x000000c0;
psd.PSCombinerCount=0x00011101;
psd.PSTextureModes=0x00000001;
psd.PSC0Mapping=0xffffffff;
psd.PSC1Mapping=0xffffffff;
psd.PSFinalCombinerConstants=0x000001ff;
