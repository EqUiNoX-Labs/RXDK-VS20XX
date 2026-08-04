//-----------------------------------------------------------------------------
// File: FastCPU.cpp
//
// Desc: Implements the FastCPU sample
//
// Hist: 1.7.03 - Created
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <xgraphics.h>

#include "skinning.h"
#include "SSE.h"
#include "P3HardwareTimer.h"


//-----------------------------------------------------------------------------
// Callouts for labeling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_BACK_BUTTON,  XBHELP_PLACEMENT_1, L"Display help" },
    { XBHELP_A_BUTTON, XBHELP_PLACEMENT_1, L"Re-run Tests" },
    { XBHELP_DPAD, XBHELP_PLACEMENT_2, L"Change Event Counters" },
};
#define NUM_HELP_CALLOUTS ( sizeof(g_HelpCallouts) / sizeof(g_HelpCallouts[0]) )


//-----------------------------------------------------------------------------
// Set if there is a verification error
//-----------------------------------------------------------------------------
BOOL g_bVerificationError = FALSE;


//-----------------------------------------------------------------------------
// Global buffer of display strings
//-----------------------------------------------------------------------------
#define OUTPUTBUFFER_SIZE 1000
#define NUM_OUTPUTBUFFER_STRINGS 100
UINT g_uiNumOutputStrings = 0;
WCHAR g_OutputBuffers[NUM_OUTPUTBUFFER_STRINGS][OUTPUTBUFFER_SIZE];



//-----------------------------------------------------------------------------
// Name: PrintLine
// Desc: Outpus a line to a global buffer for display later
//-----------------------------------------------------------------------------
VOID __cdecl PrintLine( const WCHAR* strFormat, ... )
{
    assert( g_uiNumOutputStrings < NUM_OUTPUTBUFFER_STRINGS );
    va_list pArgList;
    va_start( pArgList, strFormat );
    wvsprintfW( g_OutputBuffers[g_uiNumOutputStrings], strFormat, pArgList );
    g_uiNumOutputStrings++;
    va_end( pArgList );
}


//-----------------------------------------------------------------------------
// Name: SkinningTest
// Desc: tests CPU and assembly skinning with verification
//-----------------------------------------------------------------------------
#define NUM_VERTS 1000
#define NUM_NORMALS_PER_VERT 1
#define NUM_PALLETTE_MATRICES 50
VOID SkinningTest()
{   
    // create input verts
    D3DXVECTOR3* pInVerts = 
        (D3DXVECTOR3*)VirtualAlloc( NULL, NUM_VERTS * (1+NUM_NORMALS_PER_VERT)
                                        * sizeof(D3DXVECTOR3),
                                    MEM_COMMIT | MEM_NOZERO, PAGE_READWRITE );

    // generate random verts
    for( UINT i = 0; i < NUM_VERTS * (1+NUM_NORMALS_PER_VERT); i++ )
    {
        for( UINT j = 0; j < 3; j++ )
            pInVerts[i][j] = FLOAT(rand())/FLOAT(RAND_MAX) * 1000.0f;
    }


    // create skininfo structures (one per vertex)
    SkinInfo* pSkinInfo =
        (SkinInfo*)VirtualAlloc( NULL, NUM_VERTS * sizeof(SkinInfo),
                                 MEM_COMMIT | MEM_NOZERO, PAGE_READWRITE );

    // generate random skinning data
    for( UINT i = 0; i < NUM_VERTS; i++ )
    {
        UINT NumWeights = 1 + i/(NUM_VERTS/4);
        // RXDK: hoisted out of the for-init (MSVC's old for-scope leaked it past the loop)
        UINT j;
        for( j = 0; j < NumWeights; j++ )
        {
            pSkinInfo[i].Indices[j] = (rand() % NUM_PALLETTE_MATRICES) * 64;
            pSkinInfo[i].Weights[j] = 1.0f / NumWeights;
        }
        for( ; j < 4; j++ )
        {
            pSkinInfo[i].Indices[j] = -1;
        }
    }
    

    // create matrix pallette    
    D3DXMATRIX* pPallette =
        (D3DXMATRIX*)VirtualAlloc( NULL,
                                   NUM_PALLETTE_MATRICES * sizeof(D3DXMATRIX),
                                   MEM_COMMIT | MEM_NOZERO, PAGE_READWRITE );
    // matrix pallet must be 16 byte aligned
    assert( (DWORD(pPallette) & 0xF) == 0 );

    // generate random pallet matrices
    for( UINT i = 0; i < NUM_PALLETTE_MATRICES; i++ )
    {
        D3DXVECTOR3 RotVec( FLOAT(rand())/FLOAT(RAND_MAX/2) - 1.0f,
                            FLOAT(rand())/FLOAT(RAND_MAX/2) - 1.0f,
                            FLOAT(rand())/FLOAT(RAND_MAX/2) - 1.0f );

        D3DXVec3Normalize( &RotVec, &RotVec);
        FLOAT RotRad = FLOAT(rand()/FLOAT(RAND_MAX) * 2*D3DX_PI);
        D3DXQUATERNION  Rot;
        D3DXQuaternionRotationAxis( &Rot, &RotVec, RotRad );

        D3DXVECTOR3 Pos( FLOAT(rand())/FLOAT(RAND_MAX/2) - 1.0f,
                         FLOAT(rand())/FLOAT(RAND_MAX/2) - 1.0f,
                         FLOAT(rand())/FLOAT(RAND_MAX/2) - 1.0f );

        D3DXMatrixAffineTransformation( pPallette + i, 1.0f, NULL, &Rot, &Pos );
    }


    // create output verts in write combining memory
    D3DXVECTOR3* pOutVerts =
        (D3DXVECTOR3*)XPhysicalAlloc( NUM_VERTS * (1+NUM_NORMALS_PER_VERT) *
                                          sizeof(D3DXVECTOR3),
                                      MAXULONG_PTR, 0, 
                                      PAGE_READWRITE | PAGE_WRITECOMBINE );
    

    
    //
    // C TEST
    //
    P3WriteBackInvalidateCache();
    g_pP3Timer->StartTimer( "C Skinning" );
    SkinC( pInVerts, NUM_VERTS, NUM_NORMALS_PER_VERT,
            pSkinInfo, 
            pPallette, NUM_PALLETTE_MATRICES,
            pOutVerts );
    g_pP3Timer->StopTimer( "C Skinning" );

    //
    // ASM Test
    //
    P3WriteBackInvalidateCache();
    g_pP3Timer->StartTimer( "ASM Skinning" );
    SkinASM( pInVerts, NUM_VERTS, NUM_NORMALS_PER_VERT,
              pSkinInfo, 
              pPallette, NUM_PALLETTE_MATRICES, true,
              pOutVerts );
    g_pP3Timer->StopTimer( "ASM Skinning" );

    //
    // Verification
    //
    D3DXVECTOR3* pTestVertsC =
        new D3DXVECTOR3[NUM_VERTS * (1+NUM_NORMALS_PER_VERT)];
    D3DXVECTOR3* pTestVertsASM =
        new D3DXVECTOR3[NUM_VERTS * (1+NUM_NORMALS_PER_VERT)];
    ZeroMemory( pTestVertsC,
                NUM_VERTS * (1+NUM_NORMALS_PER_VERT) *sizeof(D3DXVECTOR3) );
    ZeroMemory( pTestVertsASM,
                NUM_VERTS * (1+NUM_NORMALS_PER_VERT) *sizeof(D3DXVECTOR3) );


    ZeroMemory( pOutVerts,
                NUM_VERTS * (1+NUM_NORMALS_PER_VERT) *sizeof(D3DXVECTOR3) );
    SkinC( pInVerts, NUM_VERTS, NUM_NORMALS_PER_VERT,
            pSkinInfo, 
            pPallette, NUM_PALLETTE_MATRICES,
            pOutVerts );
    memcpy( pTestVertsC, pOutVerts,
            NUM_VERTS * (1+NUM_NORMALS_PER_VERT) * sizeof(D3DXVECTOR3) );

    ZeroMemory( pOutVerts,
                NUM_VERTS * (1+NUM_NORMALS_PER_VERT) *sizeof(D3DXVECTOR3) );
    SkinASM( pInVerts, NUM_VERTS, NUM_NORMALS_PER_VERT,
              pSkinInfo, 
              pPallette, NUM_PALLETTE_MATRICES, true,
              pOutVerts );
    memcpy( pTestVertsASM, pOutVerts,
              NUM_VERTS * (1+NUM_NORMALS_PER_VERT) * sizeof(D3DXVECTOR3) );
    
    for( UINT i = 0; i < NUM_VERTS * (1+NUM_NORMALS_PER_VERT); i++ )
    {
        D3DXVECTOR3 Diff = pTestVertsASM[i] - pTestVertsC[i];
        FLOAT NormDif =
            D3DXVec3Length( &Diff ) / D3DXVec3Length( &pTestVertsC[i] );
        if(fabsf( NormDif ) > 0.01f)  // greater that 1% normalized diference
        {
            g_bVerificationError = TRUE;
        }
    }
    
    
    // free resources
    XPhysicalFree( pOutVerts );
    VirtualFree( pInVerts, 0, MEM_RELEASE );
    VirtualFree( pSkinInfo, 0, MEM_RELEASE );
    VirtualFree( pPallette, 0, MEM_RELEASE );
    delete [] pTestVertsC;
    delete [] pTestVertsASM;
    
}


//-----------------------------------------------------------------------------
// Name: FtolTest
// Desc: tests C and assembly float to long with verification
//-----------------------------------------------------------------------------
#define NUM_CONVERSIONS 1000
VOID FtolTest()
{
    // volatile values are used to keep the compiler from
    // optimizing out multiple iterations of the same math operation
    volatile int x;
    volatile FLOAT f = FLOAT(-RAND_MAX/2) + FLOAT(rand())
                       + FLOAT(rand()/RAND_MAX);


    //
    // C Test
    //
    g_pP3Timer->StartTimer( "C Ftol" );
    for( UINT i = 0; i < NUM_CONVERSIONS; i+=10 )
    {
        x = int(f);  
        x = int(f);  
        x = int(f);  
        x = int(f);  
        x = int(f);  
        x = int(f);
        x = int(f);
        x = int(f);
        x = int(f);
        x = int(f);
    }
    g_pP3Timer->StopTimer( "C Ftol" );

        
    // 
    // ASM Test
    //
    g_pP3Timer->StartTimer( "ASM Ftol" );
    for( UINT i = 0; i < NUM_CONVERSIONS; i+=10 )
    {
        x = Ftoi_ASM( f );
        x = Ftoi_ASM( f );
        x = Ftoi_ASM( f );
        x = Ftoi_ASM( f );
        x = Ftoi_ASM( f );
        x = Ftoi_ASM( f );
        x = Ftoi_ASM( f );
        x = Ftoi_ASM( f );
        x = Ftoi_ASM( f );
        x = Ftoi_ASM( f );
    }
    g_pP3Timer->StopTimer( "ASM Ftol" );

    //
    // Verification
    //
    for( UINT i = 0; i < NUM_CONVERSIONS; i++ )
    {
        FLOAT fToRound = FLOAT(-RAND_MAX/2) + FLOAT(rand()) 
                         + FLOAT(rand()/RAND_MAX);;
        int intC = int(fToRound);
        int intASM = Ftoi_ASM( fToRound );
        if( intC != intASM )
            g_bVerificationError = TRUE;
    }
}


//-----------------------------------------------------------------------------
// Name: ReciprocalTest
// Desc: tests C and assembly reciprocal, SSE reciprocal estimate, and SSE
//       reciprocal estimate with Newton-Raphson.  Also verifies test results
//-----------------------------------------------------------------------------
#define NUM_RECIPROCALS 1000
VOID ReciprocalTest()
{
    // volatile values are used to keep the compiler from
    // optimizing out multiple iterations of the same math operation
    volatile FLOAT x;
    volatile FLOAT f = FLOAT(-RAND_MAX/2) + FLOAT(rand()) 
                       + FLOAT(rand()/RAND_MAX) + 0.5f;

    //
    // C Test
    //
    g_pP3Timer->StartTimer( "C Recip" );
    for( UINT i = 0; i < NUM_RECIPROCALS; i+=10 )
    {
        x = 1.0f/f;
        x = 1.0f/f;
        x = 1.0f/f;
        x = 1.0f/f;
        x = 1.0f/f;
        x = 1.0f/f;
        x = 1.0f/f;
        x = 1.0f/f;
        x = 1.0f/f;
        x = 1.0f/f;
    }
    g_pP3Timer->StopTimer( "C Recip" );

    
    //
    // ASM Test
    //
    g_pP3Timer->StartTimer( "ASM RecipEst" );
    for( UINT i = 0; i < NUM_RECIPROCALS; i+=10 )
    {
        x = ReciprocalEstimate_ASM( f );
        x = ReciprocalEstimate_ASM( f );
        x = ReciprocalEstimate_ASM( f );
        x = ReciprocalEstimate_ASM( f );
        x = ReciprocalEstimate_ASM( f );
        x = ReciprocalEstimate_ASM( f );
        x = ReciprocalEstimate_ASM( f );
        x = ReciprocalEstimate_ASM( f );
        x = ReciprocalEstimate_ASM( f );
        x = ReciprocalEstimate_ASM( f );
    }
    g_pP3Timer->StopTimer( "ASM RecipEst" );
    

    // 
    // ASM NR Test
    //
    g_pP3Timer->StartTimer( "ASM NRRecipEst" );
    for( UINT i = 0; i < NUM_RECIPROCALS; i+=10 )
    {
        x = ReciprocalEstimateNR_ASM( f );
        x = ReciprocalEstimateNR_ASM( f );
        x = ReciprocalEstimateNR_ASM( f );
        x = ReciprocalEstimateNR_ASM( f );
        x = ReciprocalEstimateNR_ASM( f );
        x = ReciprocalEstimateNR_ASM( f );
        x = ReciprocalEstimateNR_ASM( f );
        x = ReciprocalEstimateNR_ASM( f );
        x = ReciprocalEstimateNR_ASM( f );
        x = ReciprocalEstimateNR_ASM( f );
    }
    g_pP3Timer->StopTimer( "ASM NRRecipEst" );


    //
    // Verification
    //
    for( UINT i = 0; i < NUM_RECIPROCALS; i++ )
    {
        FLOAT fToRec = FLOAT(-RAND_MAX/2) + FLOAT(rand()) 
                       + FLOAT(rand()/RAND_MAX) + 0.5f;
        FLOAT fC = 1.0f /fToRec;
        FLOAT fASM = ReciprocalEstimate_ASM( fToRec );
        FLOAT fASM_NR = ReciprocalEstimateNR_ASM( fToRec );

        if( fabsf( (fASM - fC) / fC ) > .001f )
            g_bVerificationError = TRUE;
        if( fabsf( (fASM_NR - fC) / fC) > .000001f )
            g_bVerificationError = TRUE;
    }

}


//-----------------------------------------------------------------------------
// Name: ReciprocalSquareRootTest
// Desc: Tests C and assembly reciprocal square root, SSE reciprocal sqare
//       root estimate, and SSE recirpocal square root estimate with
//       Newton-Raphson. Also verifies test results.
//-----------------------------------------------------------------------------
#define NUM_RECIPROCAL_SQRTS 1000
VOID ReciprocalSquareRootTest()
{
    // volatile values are used to keep the compiler from
    // optimizing out multiple iterations of the same math operation
    volatile FLOAT x;
    volatile FLOAT f = FLOAT(rand()) 
                       + FLOAT(rand()/RAND_MAX) + 0.5f;

    // 
    // C Test
    //
    g_pP3Timer->StartTimer( "C RecipSqrt" );
    for( UINT i = 0; i < NUM_RECIPROCAL_SQRTS; i+=10 )
    {
        x = 1.0f/sqrtf( f );
        x = 1.0f/sqrtf( f );
        x = 1.0f/sqrtf( f );
        x = 1.0f/sqrtf( f );
        x = 1.0f/sqrtf( f );
        x = 1.0f/sqrtf( f );
        x = 1.0f/sqrtf( f );
        x = 1.0f/sqrtf( f );
        x = 1.0f/sqrtf( f );
        x = 1.0f/sqrtf( f );
        
    }
    g_pP3Timer->StopTimer( "C RecipSqrt" );

    
    //
    // ASM Test
    //
    g_pP3Timer->StartTimer( "ASM RecipSqrtEst" );
    for( UINT i = 0; i < NUM_RECIPROCAL_SQRTS; i+=10 )
    {
        x = ReciprocalSqrtEstimate_ASM( f );
        x = ReciprocalSqrtEstimate_ASM( f );
        x = ReciprocalSqrtEstimate_ASM( f );
        x = ReciprocalSqrtEstimate_ASM( f );
        x = ReciprocalSqrtEstimate_ASM( f );
        x = ReciprocalSqrtEstimate_ASM( f );
        x = ReciprocalSqrtEstimate_ASM( f );
        x = ReciprocalSqrtEstimate_ASM( f );
        x = ReciprocalSqrtEstimate_ASM( f );
        x = ReciprocalSqrtEstimate_ASM( f );
    }
    g_pP3Timer->StopTimer( "ASM RecipSqrtEst" );
    

    //
    // ASM NR Test
    //
    g_pP3Timer->StartTimer( "ASM NRRecipSqrtEst" );
    for( UINT i = 0; i < NUM_RECIPROCAL_SQRTS; i+=10 )
    {
        x = ReciprocalSqrtEstimateNR_ASM( f );
        x = ReciprocalSqrtEstimateNR_ASM( f );
        x = ReciprocalSqrtEstimateNR_ASM( f );
        x = ReciprocalSqrtEstimateNR_ASM( f );
        x = ReciprocalSqrtEstimateNR_ASM( f );
        x = ReciprocalSqrtEstimateNR_ASM( f );
        x = ReciprocalSqrtEstimateNR_ASM( f );
        x = ReciprocalSqrtEstimateNR_ASM( f );
        x = ReciprocalSqrtEstimateNR_ASM( f );
        x = ReciprocalSqrtEstimateNR_ASM( f );
    }
    g_pP3Timer->StopTimer( "ASM NRRecipSqrtEst" );


    //
    // Verification
    //
    for( UINT i = 0; i < NUM_RECIPROCAL_SQRTS; i++ )
    {
        FLOAT fToRecSqrt = FLOAT(rand()) 
                           + FLOAT(rand()/RAND_MAX) + 0.5f;
        FLOAT fC = 1.0f /sqrtf( fToRecSqrt );
        FLOAT fASM = ReciprocalSqrtEstimate_ASM( fToRecSqrt );
        FLOAT fASM_NR = ReciprocalSqrtEstimateNR_ASM( fToRecSqrt );

        if( fabsf( (fASM - fC) / fC) > .001f )
            g_bVerificationError = TRUE;
        if( fabsf( (fASM_NR - fC) / fC) > .000001f )
            g_bVerificationError = TRUE;
    }
}


//-----------------------------------------------------------------------------
// Name: SquareRootTest
// Desc: Tests C and assembly square root, SSE squre root estiamate, and SSE
//       square root estimate with Newton-Raphson.  Also verifies test results.
//-----------------------------------------------------------------------------
#define NUM_SQRTS 1000
VOID SquareRootTest()
{
    // volatile values are used to keep the compiler from
    // optimizing out multiple iterations of the same math operation
    volatile FLOAT x;
    volatile FLOAT f = FLOAT(rand()) + FLOAT(rand()/RAND_MAX);

    // 
    // C Test
    //
    g_pP3Timer->StartTimer( "C Sqrt" );
    for( UINT i = 0; i < NUM_SQRTS; i+=10 )
    {
        x = sqrtf( f );
        x = sqrtf( f );
        x = sqrtf( f );
        x = sqrtf( f );
        x = sqrtf( f );
        x = sqrtf( f );
        x = sqrtf( f );
        x = sqrtf( f );
        x = sqrtf( f );
        x = sqrtf( f );
    }
    g_pP3Timer->StopTimer( "C Sqrt" );

    
    //
    // ASM Test
    //
    g_pP3Timer->StartTimer( "ASM SqrtEst" );
    for( UINT i = 0; i < NUM_SQRTS; i+=10 )
    {
        x = SqrtEstimate_ASM( f );
        x = SqrtEstimate_ASM( f );
        x = SqrtEstimate_ASM( f );
        x = SqrtEstimate_ASM( f );
        x = SqrtEstimate_ASM( f );
        x = SqrtEstimate_ASM( f );
        x = SqrtEstimate_ASM( f );
        x = SqrtEstimate_ASM( f );
        x = SqrtEstimate_ASM( f );
        x = SqrtEstimate_ASM( f );
    }
    g_pP3Timer->StopTimer( "ASM SqrtEst" );
    

    //
    // ASM NR Test
    //
    g_pP3Timer->StartTimer( "ASM NRSqrtEst" );
    for( UINT i = 0; i < NUM_SQRTS; i+=10 )
    {
        x = SqrtEstimateNR_ASM( f );
        x = SqrtEstimateNR_ASM( f );
        x = SqrtEstimateNR_ASM( f );
        x = SqrtEstimateNR_ASM( f );
        x = SqrtEstimateNR_ASM( f );
        x = SqrtEstimateNR_ASM( f );
        x = SqrtEstimateNR_ASM( f );
        x = SqrtEstimateNR_ASM( f );
        x = SqrtEstimateNR_ASM( f );
        x = SqrtEstimateNR_ASM( f );
    }
    g_pP3Timer->StopTimer( "ASM NRSqrtEst" );

    //
    // Verification
    //
    for( UINT i = 0; i < NUM_SQRTS; i++ )
    {
        // explicitly test 0
        FLOAT fToSqrt = FLOAT(rand()) + FLOAT(rand()/RAND_MAX);
        if( i == 0)
            fToSqrt = 0.0f;
        FLOAT fC = sqrtf( fToSqrt );
        FLOAT fASM = SqrtEstimate_ASM( fToSqrt );
        FLOAT fASM_NR = SqrtEstimateNR_ASM( fToSqrt );

        if( fabsf( (fASM - fC) / fC ) > .001f )
            g_bVerificationError = TRUE;
        if( fabsf( (fASM_NR - fC) / fC ) > .000001f )
            g_bVerificationError = TRUE;
    }
}


//-----------------------------------------------------------------------------
// Name: WriteCombiningReadTest
// Desc: Tests 32, 64, and 128 bit reads from write combing memory
//-----------------------------------------------------------------------------
#define NUM_BYTES 50 * 1024
VOID WriteCombiningReadTest()
{   
    // allocate a chunk of write combine policy memory.
    BYTE* pSrc = (BYTE*)XPhysicalAlloc( NUM_BYTES,
                                        MAXULONG_PTR, 0,
                                        PAGE_READWRITE | PAGE_WRITECOMBINE  );

    //
    // 32 Bit Test
    //
    g_pP3Timer->StartTimer( "32 Bit WC Read" );
    __asm mov eax, pSrc
    __asm mov ecx, pSrc
    __asm add ecx, NUM_BYTES
    // RXDK: one __asm block -- clang scopes asm labels to a single block.
    __asm
    {
READ32:
        movd mm0, dword ptr[eax]
        movd mm1, dword ptr[eax+4]
        movd mm2, dword ptr[eax+8]
        movd mm3, dword ptr[eax+12]
        movd mm4, dword ptr[eax+16]
        movd mm5, dword ptr[eax+20]
        movd mm6, dword ptr[eax+24]
        movd mm7, dword ptr[eax+28]
        add eax, 32
        cmp eax, ecx
        jne READ32
        emms
    }
    g_pP3Timer->StopTimer( "32 Bit WC Read" );
   

    //
    // 64 Bit Test
    //
    g_pP3Timer->StartTimer( "64 Bit WC Read" );
    __asm mov eax, pSrc
    __asm mov ecx, pSrc
    __asm add ecx, NUM_BYTES
    // RXDK: one __asm block -- clang scopes asm labels to a single block.
    __asm
    {
READ64:
        movq mm0, qword ptr[eax]
        movq mm1, qword ptr[eax+8]
        movq mm2, qword ptr[eax+16]
        movq mm3, qword ptr[eax+24]
        movq mm4, qword ptr[eax+32]
        movq mm5, qword ptr[eax+40]
        movq mm6, qword ptr[eax+48]
        movq mm7, qword ptr[eax+56]
        add eax, 64
        cmp eax, ecx
        jne READ64
        emms
    }
    g_pP3Timer->StopTimer( "64 Bit WC Read" );


    //
    // 128 Bit Test
    //
    P3WriteBackInvalidateCache();
    g_pP3Timer->StartTimer( "128 Bit WC Read" );
    __asm mov eax, pSrc
    __asm mov ecx, pSrc
    __asm add ecx, NUM_BYTES
    // RXDK: one __asm block -- clang scopes asm labels to a single block.
    __asm
    {
READ128:
        // RXDK: movaps moves 128 bits -- MSVC tolerated the qword annotation, clang does not.
        movaps xmm0, xmmword ptr[eax]
        movaps xmm1, xmmword ptr[eax+16]
        movaps xmm2, xmmword ptr[eax+32]
        movaps xmm3, xmmword ptr[eax+48]
        movaps xmm4, xmmword ptr[eax+64]
        movaps xmm5, xmmword ptr[eax+80]
        movaps xmm6, xmmword ptr[eax+96]
        movaps xmm7, xmmword ptr[eax+112]
        add eax, 128
        cmp eax, ecx
        jne READ128
    }
    g_pP3Timer->StopTimer( "128 Bit WC Read" );

    // free resources
    XPhysicalFree( pSrc );
}

   
//-----------------------------------------------------------------------------
// Name: DoTests
// Desc: Runs the various test and outputs the results to the debug channel
//-----------------------------------------------------------------------------
VOID DoTests( P3Event Events[2] )
{
    g_uiNumOutputStrings = 0;
    g_bVerificationError = FALSE;

    // set counter control
    g_pP3Timer->SetEvents( Events[0], Events[1] );

    // maintain rand across test
    srand(0);

    g_pP3Timer->StartTiming( TRUE );

    // run tests
    SkinningTest();
    FtolTest();
    ReciprocalTest();
    ReciprocalSquareRootTest();
    SquareRootTest();
    WriteCombiningReadTest();

    g_pP3Timer->StopTiming( );

    // report verification errors
    if(g_bVerificationError)
    {
        PrintLine( L"Verification Error!" );
    }

    // output stats
    else
    {
        const WCHAR* strHeaderFormat = L"%18.18S %7.7S %6.6S %12.12S %12.12S";
        const WCHAR* strDataFormat0 = L"%18S %7I64i        %12I64i %12I64i";
        const WCHAR* strDataFormat1 = L"%18S %7I64i %6.2f %12I64i %12I64i";

        PrintLine( strHeaderFormat, "Operation", "Cycles", "Ratio",
            g_pP3Timer->GetCounterName( 0 ), g_pP3Timer->GetCounterName( 1 ) );
        PrintLine( strHeaderFormat, 
                   "------------------------------",
                   "------------------------------",
                   "------------------------------",
                   "------------------------------",
                   "------------------------------" );
        for( UINT i = 0; i < 4; i+=2 )
        {
            const P3EventSample* pReport1 = g_pP3Timer->GetReport( i );
            PrintLine( strDataFormat0,
                       pReport1->szName,
                       pReport1->Cycles,
                       pReport1->Counters[0], pReport1->Counters[1] );

            const P3EventSample* pReport2 = g_pP3Timer->GetReport( i+1 );
            PrintLine( strDataFormat1,
                       pReport2->szName, pReport2->Cycles,
                       FLOAT(pReport1->Cycles)/FLOAT(pReport2->Cycles),
                       pReport2->Counters[0], pReport2->Counters[1] );
            PrintLine( L"" );
        }

        for( UINT i = 4; i < 13; i+=3 )
        {
            const P3EventSample* pReport1 = g_pP3Timer->GetReport( i );
            PrintLine( strDataFormat0,
                       pReport1->szName,
                       pReport1->Cycles,
                       pReport1->Counters[0], pReport1->Counters[1] );

            const P3EventSample* pReport2 = g_pP3Timer->GetReport( i+1 );
            PrintLine(strDataFormat1,
                pReport2->szName,
                pReport2->Cycles,
                FLOAT(pReport1->Cycles)/FLOAT(pReport2->Cycles),
                pReport2->Counters[0], pReport2->Counters[1]);
            
            const P3EventSample* pReport3 = g_pP3Timer->GetReport( i+2 );
            PrintLine( strDataFormat1,
                       pReport3->szName,
                       pReport3->Cycles,
                       FLOAT(pReport1->Cycles)/FLOAT(pReport3->Cycles),
                       pReport3->Counters[0], pReport3->Counters[1]);
            PrintLine( L"" );
        }
            

        for( UINT i = 13; i < 16; i+=3 )
        {
            const P3EventSample* pReport1 = g_pP3Timer->GetReport( i );
            PrintLine( strDataFormat0,
                       pReport1->szName,
                       pReport1->Cycles,
                       pReport1->Counters[0],
                       pReport1->Counters[1] );

            const P3EventSample* pReport2 = g_pP3Timer->GetReport( i+1 );
            PrintLine( strDataFormat1,
                       pReport2->szName, 
                       pReport2->Cycles,
                       FLOAT(pReport1->Cycles)/FLOAT(pReport2->Cycles),
                       pReport2->Counters[0], pReport2->Counters[1] );
            
            const P3EventSample* pReport3 = g_pP3Timer->GetReport( i+2 );
            PrintLine( strDataFormat1,
                       pReport3->szName, pReport3->Cycles,
                       FLOAT(pReport1->Cycles)/FLOAT(pReport3->Cycles),
                       pReport3->Counters[0], pReport3->Counters[1] );
            PrintLine( L"" );
        }
    }
}


//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    CXBFont     m_Font;             // Font object
    CXBFont     m_ConsoleFont;      // Console font object
    CXBHelp     m_Help;             // Help object
    BOOL        m_bDrawHelp;        // TRUE to draw help screen

public:
    virtual HRESULT Initialize();
    virtual HRESULT Render();
    virtual HRESULT FrameMove();

    CXBoxSample();
};


//-----------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program.
//-----------------------------------------------------------------------------
VOID __cdecl main()
{
    CXBoxSample xbApp;
    if( FAILED( xbApp.Create() ) )
        return;
    xbApp.Run();
}


//-----------------------------------------------------------------------------
// Name: CXBoxSample()
// Desc: Constructor for CXBoxSample class
//-----------------------------------------------------------------------------
CXBoxSample::CXBoxSample() 
            :CXBApplication()
{
    m_bDrawHelp = FALSE;
}


//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Performs initialization
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    // Create main font
    if( FAILED( m_Font.Create( "Font.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create console font
    if( FAILED( m_ConsoleFont.Create( "ConsoleFont.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create help
    if( FAILED( m_Help.Create( "Gamepad.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    InitP3HardwareTimer();

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Performs per-frame updates
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_BACK ) 
    {
        m_bDrawHelp = !m_bDrawHelp;
        return S_OK;
    }

    // counter events
    static P3Event Events[2] = { INST_RETIRED, RESOURCE_STALLS };

    // run the test if requested or an event control is set
    BOOL bTest = FALSE;
    

    // run the test on startup
    static BOOL bFirstTest = FALSE;
    if( !bFirstTest )
    {
        bTest = TRUE;
        bFirstTest = TRUE;
    }

    // re-run tests
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
    {
        bTest = TRUE;
    }

    // change counter controls
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT )
    {
        do{
            if( Events[0] == 0 )
                Events[0] = (P3Event)(P3EVENT_MAX - 1);
            else
                Events[0] = (P3Event)(Events[0] - 1);
        }
        while( g_P3EventInfos[Events[0]].CountersAllowed != 0x00  &&
               g_P3EventInfos[Events[0]].CountersAllowed != 0x10 );

        bTest = TRUE;
    }
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT )
    {
        do
        {
            if( Events[0] == P3EVENT_MAX - 1)
                Events[0] = (P3Event)(0);
            else
                Events[0] = (P3Event)(Events[0] + 1);
        }
        while( g_P3EventInfos[Events[0]].CountersAllowed != 0x00  &&
               g_P3EventInfos[Events[0]].CountersAllowed != 0x10 );

        bTest = TRUE;
    }
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
    {
        do
        {
            if( Events[1] == 0 )
                Events[1] = (P3Event)(P3EVENT_MAX - 1);
            else
                Events[1] = (P3Event)(Events[1] - 1);
        }
        while( g_P3EventInfos[Events[1]].CountersAllowed != 0x01  &&
               g_P3EventInfos[Events[1]].CountersAllowed != 0x10 );

        bTest = TRUE;
        
    }
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
    {
        do
        {
            if( Events[1] == P3EVENT_MAX - 1 )
                Events[1] = (P3Event)(0);
            else
                Events[1] = (P3Event)(Events[1] + 1);
        }
        while( g_P3EventInfos[Events[1]].CountersAllowed != 0x01  &&
               g_P3EventInfos[Events[1]].CountersAllowed != 0x10 );

        bTest = TRUE;
    }

    if( bTest )
    {
        // wait until the GPU is idle to maintain test consistency
        D3DDevice::BlockUntilIdle();
        DoTests( Events );
    }

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Renders the scene
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Render()
{
    D3DDevice::Clear( 0, NULL,
                      D3DCLEAR_TARGET,
                      0x00000000, 1.0f, 0L );

    // Show title, frame rate, and help
    if( m_bDrawHelp )
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );

    // render test results
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.5f, 1.5f );
        m_Font.DrawText( 48, 36, 0xffffffff, L"FastCPU" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.End();

        m_ConsoleFont.Begin();
        m_ConsoleFont.SetScaleFactors( 1.0f, 1.3f );
        FLOAT uiY = 73;
        for( UINT i = 0; i < g_uiNumOutputStrings; i++ )
        {
            m_ConsoleFont.DrawText( 48, uiY,
                                    0xff00ee00,
                                    g_OutputBuffers[i], XBFONT_LEFT  );
            uiY += 16.0f;
        }
        m_ConsoleFont.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}



