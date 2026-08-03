//-----------------------------------------------------------------------------
// File: PushBufferModel.h
//
// Desc: Some data to be encapsulated in a push buffer
//
// Hist: 01.23.03 - New for Feb XDK
//       02.06.03 - Code cleanup
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef PUSHBUFFERMODEL_H
#define PUSHBUFFERMODEL_H




//------------------------------------------------------------------------------
// Define a triangle.
//------------------------------------------------------------------------------
struct VERTEX
{ 
    FLOAT    x,y,z; 
    D3DCOLOR color; 
};

VERTEX g_Vertices[] =
{
    {-0.6f,-1.0f, 0.0f, 0xffff0000 },
    { 0.0f, 1.0f, 0.0f, 0xff00ff00 },
    { 0.6f,-1.0f, 0.0f, 0xff0000ff },
};




//------------------------------------------------------------------------------
// Define a simple vertex shader
//------------------------------------------------------------------------------
DWORD g_dwVertexShaderDecl[] =
{
    D3DVSD_STREAM( 0 ),
    D3DVSD_REG( 0, D3DVSDT_FLOAT3 ),   // Position
    D3DVSD_REG( 1, D3DVSDT_D3DCOLOR ), // Diffuse color
    D3DVSD_END()
};

DWORD g_dwVertexShaderProgram[] =
{
    0x00072078,
    0x00000000, 0x00ec001b, 0x0836186c, 0x20708800,
    0x00000000, 0x00ec201b, 0x0836186c, 0x20704800,
    0x00000000, 0x00ec401b, 0x0836186c, 0x20702800,
    0x00000000, 0x00ec601b, 0x0836186c, 0x20701800,
    0x00000000, 0x0020021b, 0x0836106c, 0x2070f818,
    0x00000000, 0x0647401b, 0xc4361bff, 0x1078e800,
    0x00000000, 0x0087601b, 0xc400286c, 0x3070e801
};




//------------------------------------------------------------------------------
// Define a simple pixel shader
//------------------------------------------------------------------------------
DWORD g_dwPixelShaderProgram[] = 
{
    0xd4301010,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x000000c0, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0xc4200000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x000000c0, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00011101, 0x00000000, 0x00000000, 0x00000000,
    0xffffffff, 0xffffffff, 0x000001ff
};




//------------------------------------------------------------------------------
// Define a simple fixed function decl
//------------------------------------------------------------------------------
D3DVERTEXATTRIBUTEFORMAT g_FixedFunctionShaderInputs = 
{
    {
        { 0, 0, D3DVSDT_FLOAT3, 0, 0 },
        { 0, 0, D3DVSDT_NONE,   0, 0 },
        { 0, 0, D3DVSDT_NONE,   0, 0 },
        { 0, 3 * sizeof(FLOAT), D3DVSDT_D3DCOLOR, 0, 0 },
        { 0, 0, D3DVSDT_NONE,   0, 0 },
        { 0, 0, D3DVSDT_NONE,   0, 0 },
        { 0, 0, D3DVSDT_NONE,   0, 0 },
        { 0, 0, D3DVSDT_NONE,   0, 0 },
        { 0, 0, D3DVSDT_NONE,   0, 0 },
        { 0, 0, D3DVSDT_NONE,   0, 0 },
        { 0, 0, D3DVSDT_NONE,   0, 0 },
        { 0, 0, D3DVSDT_NONE,   0, 0 },
        { 0, 0, D3DVSDT_NONE,   0, 0 },
        { 0, 0, D3DVSDT_NONE,   0, 0 },
        { 0, 0, D3DVSDT_NONE,   0, 0 },
        { 0, 0, D3DVSDT_NONE,   0, 0 },
    }
};
    



#endif // PUSHBUFFERMODEL_H
