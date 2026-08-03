xof 0303txt 0032
template VertexDuplicationIndices {
 <b8d65549-d7c9-4995-89cf-53a9a8b031e3>
 DWORD nIndices;
 DWORD nOriginalVertices;
 array DWORD indices[nIndices];
}

template FVFData {
 <b6e70a0e-8ef9-4e83-94ad-ecc8b0c04897>
 DWORD dwFVF;
 DWORD nDWords;
 array DWORD data[nDWords];
}

template VertexElement {
 <f752461c-1e23-48f6-b9f8-8350850f336f>
 DWORD Type;
 DWORD Method;
 DWORD Usage;
 DWORD UsageIndex;
}

template DeclData {
 <bf22e553-292c-4781-9fea-62bd554bdd93>
 DWORD nElements;
 array VertexElement Elements[nElements];
 DWORD nDWords;
 array DWORD data[nDWords];
}

template EffectParamFloats {
 <3014b9a0-62f5-478c-9b86-e4ac9f4e418b>
 STRING ParamName;
 DWORD nFloats;
 array FLOAT Floats[nFloats];
}

template EffectParamString {
 <1dbc4c88-94c1-46ee-9076-2c28818c9481>
 STRING ParamName;
 STRING Value;
}

template EffectParamDWord {
 <e13963bc-ae51-4c5d-b00f-cfa3a9d97ce5>
 STRING ParamName;
 DWORD Value;
}

template EffectInstance {
 <e331f7e4-0559-4cc2-8e99-1cec1657928f>
 STRING EffectFilename;
 [...]
}


Mesh {
 10;
 871.066406;435.948639;0.000000;,
 -871.066406;435.948639;0.000000;,
 -871.066406;-640.748657;0.000000;,
 871.066406;-640.748657;0.000000;,
 538.348633;-102.400002;871.066406;,
 538.348633;-102.400002;-871.066406;,
 -538.348633;-102.400002;-871.066406;,
 -538.348633;-102.400002;871.066406;,
 0.000000;768.666382;538.348633;,
 0.000000;768.666382;-538.348633;;
 12;
 3;4,8,7;,
 3;5,6,9;,
 3;0,4,3;,
 3;0,3,5;,
 3;2,7,1;,
 3;2,1,6;,
 3;8,0,9;,
 3;8,9,1;,
 3;8,4,0;,
 3;9,0,5;,
 3;7,8,1;,
 3;6,1,9;;
}