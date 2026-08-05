//-----------------------------------------------------------------------------
// File: Skinning.cpp
//
// Desc: Contains C and P3 SSE implementations of matrix pallette skinning
//
// Hist: 1.7.03 - Created
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------


#include "skinning.h"


//-----------------------------------------------------------------------------
// Name: Vec3TransformNoAsm
// Desc: FPU vec3 transform
//-----------------------------------------------------------------------------
D3DXVECTOR3* WINAPI Vec3TransformCoordNoAsm( D3DXVECTOR3 *pOut,
                                             const D3DXVECTOR3 *pV,
                                             const D3DXMATRIX *pM )
{
    pOut->x = pV->x * pM->_11 + pV->y * pM->_21 + pV->z * pM->_31 + pM->_41;
    pOut->y = pV->x * pM->_12 + pV->y * pM->_22 + pV->z * pM->_32 + pM->_42;
    pOut->z = pV->x * pM->_13 + pV->y * pM->_23 + pV->z * pM->_33 + pM->_43;
    
    return pOut;
}


//-----------------------------------------------------------------------------
// Name: Vec3TransformNormalNoAsm
// Desc: FPU vec3 normal transform
//-----------------------------------------------------------------------------
D3DXVECTOR3* WINAPI Vec3TransformNormalNoAsm( D3DXVECTOR3 *pOut,
                                              const D3DXVECTOR3 *pV,
                                              const D3DXMATRIX *pM )
{

    pOut->x = pV->x * pM->_11 + pV->y * pM->_21 + pV->z * pM->_31;
    pOut->y = pV->x * pM->_12 + pV->y * pM->_22 + pV->z * pM->_32;
    pOut->z = pV->x * pM->_13 + pV->y * pM->_23 + pV->z * pM->_33;
    
    return pOut;
}


//-----------------------------------------------------------------------------
// Name: Skin_C
// Desc: C matrix pallette skinning routine
//-----------------------------------------------------------------------------
VOID SkinC( const D3DXVECTOR3* pInData,
            DWORD dwNumVerts, DWORD dwNumNormalsPerVert,
            const SkinInfo* pSkinInfo,  
            const D3DXMATRIX* pPallette, DWORD dwNumPalletteMatrices,
            D3DXVECTOR3* pOutData)
{
    D3DXVECTOR3 Temp;
    D3DXVECTOR3 OutData;
    for(UINT i = 0; i < dwNumVerts; i++)
    {
        // transform vec by pallette matrix * weight
        Vec3TransformCoordNoAsm( &Temp, pInData,
            (D3DXMATRIX*)((BYTE*)(pPallette) + pSkinInfo[i].Indices[0]));
        Temp*= pSkinInfo[i].Weights[0];
        OutData = Temp;

        // continue if no more pallettes affect this vec
        if(pSkinInfo[i].Indices[1] == -1)
            goto NORMALS;
        Vec3TransformCoordNoAsm(&Temp, pInData,
            (D3DXMATRIX*)((BYTE*)(pPallette) + pSkinInfo[i].Indices[1]));
        Temp*= pSkinInfo[i].Weights[1];
        OutData += Temp;

        if(pSkinInfo[i].Indices[2] == -1)
            goto NORMALS;
        Vec3TransformCoordNoAsm(&Temp, pInData,
            (D3DXMATRIX*)((BYTE*)(pPallette) + pSkinInfo[i].Indices[2]));
        Temp*= pSkinInfo[i].Weights[2];
        OutData += Temp;

        if(pSkinInfo[i].Indices[3] == -1)
            goto NORMALS;
        Vec3TransformCoordNoAsm(&Temp, pInData,
            (D3DXMATRIX*)((BYTE*)(pPallette) + pSkinInfo[i].Indices[3]));
        Temp*= pSkinInfo[i].Weights[3];
        OutData += Temp;

NORMALS:
        for(UINT j = 0; j < dwNumNormalsPerVert; j++)
        {
            *pOutData = OutData;
            pInData++;
            pOutData++;

            // transform normal by pallette matrix * weight
            Vec3TransformNormalNoAsm(&Temp, pInData,
                (D3DXMATRIX*)((BYTE*)(pPallette) + pSkinInfo[i].Indices[0]));
            Temp*= pSkinInfo[i].Weights[0];
            OutData = Temp;

            // continue if no more pallettes affect this normal
            if(pSkinInfo[i].Indices[1] == -1)
                continue;
            Vec3TransformNormalNoAsm(&Temp, pInData,
                (D3DXMATRIX*)((BYTE*)(pPallette) + pSkinInfo[i].Indices[1]));
            Temp*= pSkinInfo[i].Weights[1];
            OutData += Temp;

            if(pSkinInfo[i].Indices[2] == -1)
                continue;
            Vec3TransformNormalNoAsm(&Temp, pInData,
                (D3DXMATRIX*)((BYTE*)(pPallette) + pSkinInfo[i].Indices[2]));
            Temp*= pSkinInfo[i].Weights[2];
            OutData += Temp;

            if(pSkinInfo[i].Indices[3] == -1)
                continue;
            Vec3TransformNormalNoAsm(&Temp, pInData,
                (D3DXMATRIX*)((BYTE*)(pPallette) + pSkinInfo[i].Indices[3]));
            Temp*= pSkinInfo[i].Weights[3];
            OutData += Temp;
        }

        *pOutData = OutData;
        pInData++;
        pOutData++;
    }
}


//-----------------------------------------------------------------------------
// SSE skinning macros
//-----------------------------------------------------------------------------
// RXDK: these macros used to be self-contained `__asm { __asm insn ... }` blocks,
// the MSVC idiom that lets one macro serve both as a statement and inside a
// larger block. Clang scopes an asm block's labels to that ONE block, so the
// skinning loop below -- whose jumps cross between macros -- has to live in a
// single __asm block, and a nested __asm block inside it will not parse. They
// therefore expand to bare instructions now and are only usable INSIDE a block.
// The instructions and their order are untouched.
//
// The names stay function-like on purpose: `OUTVEC(ecx)` is a macro while
// `OUTVEC:` is a label, and the preprocessor only expands the former.

#define MOVVEC( ptr )                                 \
    __asm movss   xmm0, [ptr]           /* store ptr->x */  \
    __asm movss   xmm1, [ptr+4]         /* store ptr->y */  \
    __asm movss   xmm2, [ptr+8]         /* store ptr->z */  \
    __asm shufps  xmm0, xmm0, 0         /* xmm0 = xxxx */   \
    __asm shufps  xmm1, xmm1, 0         /* xmm0 = yyyy */   \
    __asm shufps  xmm2, xmm2, 0         /* xmm0 = zzzz */

#define MOVSCALE( ptr )                                   \
    __asm movaps xmm3, [ptr]             /* xmm3 = ptr->xyzw */

#define TMP_REG edi
#define MULMATRIX43ANDSCALE( ptrMat )                                        \
    __asm add     TMP_REG, ptrMat            /* ptr mat*/                          \
    __asm movaps  xmm4, [TMP_REG]            /* mat->r0 */                         \
    __asm movaps  xmm5, [TMP_REG + 16]       /* mat->r1 */                         \
    __asm movaps  xmm6, [TMP_REG + 32]       /* mat->r2 */                         \
    __asm mulps   xmm4, xmm0                 /* mat->R0 * V->x */                  \
    __asm mulps   xmm5, xmm1                 /* mat->R1 * V->y */                  \
    __asm mulps   xmm6, xmm2                 /* mat->R2 * V->z */                  \
    __asm addps   xmm4, xmm5                 /* mat->R0 * V->x + mat->R1 * V->y */ \
    __asm addps   xmm6, [TMP_REG + 48]       /* mat->R2 * V->z + mat->R3 */        \
    __asm movaps  xmm5, xmm3                 /* xmm5 = scale */                    \
    __asm addps   xmm6, xmm4                 /* + mat->R2 *V->z + mat->R3 */       \
    __asm shufps  xmm5, xmm5, 0x00           /* xmm5 = scale,scale,scale,scale */  \
    __asm shufps  xmm3, xmm3, 0x39           /* xmm3:xyzw = xmm3:wxyz */           \
    __asm mulps   xmm6, xmm5                 /* scale*vec */

#define MULMATRIX33ANDSCALE( ptrMat )                                        \
    __asm add     TMP_REG, ptrMat            /* ptr mat*/                          \
    __asm movaps  xmm4, [TMP_REG]            /* mat->r0 */                         \
    __asm movaps  xmm5, [TMP_REG + 16]       /* mat->r1 */                         \
    __asm movaps  xmm6, [TMP_REG + 32]       /* mat->r2 */                         \
    __asm mulps   xmm4, xmm0                 /* mat->R0 * V->x */                  \
    __asm mulps   xmm5, xmm1                 /* mat->R1 * V->y */                  \
    __asm mulps   xmm6, xmm2                 /* mat->R2 * V->z */                  \
    __asm addps   xmm4, xmm5                 /* mat->R0 * V->x + mat->R1 * V->y */ \
    __asm movaps  xmm5, xmm3                 /* xmm5 = scale */                    \
    __asm addps   xmm6, xmm4                 /* + mat->R2 *V->z */                 \
    __asm shufps  xmm5, xmm5, 0x00           /* xmm5 = scale,scale,scale,scale */  \
    __asm shufps  xmm3, xmm3, 0x39           /* xmm3:xyzw = xmm3:wxyz */           \
    __asm mulps   xmm6, xmm5                 /* scale*vec */

#define STOREVECANDJMPNOWEIGHT( JMP, ptrOffset )                         \
    __asm mov TMP_REG, [ptrOffset]        /* tmp = ptrOffset */                \
    __asm movaps  xmm7, xmm6              /* move vec from xmm6 to xmm7 */     \
    __asm cmp TMP_REG, -1                 /* compater ptrOffet to -1 */        \
    __asm je JMP                          /* if ptrOffset == -1, goto JUMP */

#define ADDVECANDJMPNOWEIGHT( JMP, ptrOffset )                           \
    __asm mov TMP_REG, [ptrOffset]        /* tmp = ptrOffset */                \
    __asm addps  xmm7, xmm6               /* add xmm6 to xmm7 */               \
    __asm cmp TMP_REG, -1                 /* compater ptrOffet to -1 */        \
    __asm je JMP                          /* if ptrOffset == -1, goto JUMP */

#define ADDVEC()                                               \
    __asm addps   xmm7, xmm6         /* add vec from xmm6 to xmm7 */

#define OUTVEC( ptr )                                            \
    __asm movlps  [ptr], xmm7             /* pOutVec->xy = xmm7.xy */  \
    __asm shufps  xmm7, xmm7, 0x0E        /* xmm7:x = xmm7:z */        \
    __asm movss   [ptr + 8], xmm7         /* pOutVec->z = xmm7:x */


//-----------------------------------------------------------------------------
// Name: PreCachePallette
// Desc: moves a matrix pallette into the cache
//-----------------------------------------------------------------------------
VOID PreCachePallette( const D3DXMATRIX* pPallette,
                       DWORD dwNumPalletteMatrices )
{
    assert( pPallette != NULL && dwNumPalletteMatrices != 0 );

    // pallette must be 16 byte aligned
    assert( (DWORD(pPallette) & 0xF) == 0 );

    // compute num bytes offset from pPallette to end
    dwNumPalletteMatrices *= sizeof(D3DXMATRIX);  

    __asm mov eax, pPallette  // store pallette pointer
    __asm mov ebx, pPallette  // store pallette pointer
    __asm add ebx, dwNumPalletteMatrices // compute end pallette pointer
    
    // RXDK: one __asm block -- clang scopes asm labels to a single block, so a
    // jump from inside __asm to a label sitting between separate single-statement
    // __asm lines does not resolve.
    __asm
    {
PRECACHE:
        prefetcht0 [eax]        // prefetch first half of matrix
        prefetcht0 [eax + 16*2] // prefetch second half of matrix
        add eax, 16*4           // increment pallette pointer
        cmp eax, ebx            // finished when at end
        jne PRECACHE
    }
    
    return;
}


//-----------------------------------------------------------------------------
// Name: Skin_ASM
// Desc: SSE matrix pallette skinning routine
//-----------------------------------------------------------------------------
VOID SkinASM( const D3DXVECTOR3* pInData,
              DWORD dwNumVerts, DWORD dwNumNormalsPerVert,
              const SkinInfo* pSkinInfo,  
              const D3DXMATRIX* pPallette,
              DWORD dwNumPalletteMatrices, bool bPreCacheMatrices,
              D3DXVECTOR3* pOutData )
{
    assert( pInData != NULL && pSkinInfo != NULL && dwNumVerts != 0 );
    assert( pPallette != NULL && dwNumPalletteMatrices != 0 ); 
    assert( pOutData != NULL); 

    // pallette must be 16 byte aligned
    assert( (DWORD(pPallette) & 0xF) == 0 ); 

    // skininfo must be 16 byte aligned
    assert( (DWORD(pSkinInfo) & 0xF) == 0 );

    // out data must be in write combining to avoid cache thrashing
    assert( XQueryMemoryProtect(pOutData) & PAGE_WRITECOMBINE );  


    // precache matrix pallette if requested
    if( bPreCacheMatrices )
    {
        PreCachePallette( pPallette, dwNumPalletteMatrices );
    }
    
    const SkinInfo* pEnd = pSkinInfo + dwNumVerts;   // compute end pointer
    
    // RXDK: one __asm block, not a statement per instruction with C labels
    // between. Clang scopes an asm block's labels to that block, so BEGIN /
    // OUTVEC / NORMALS / OUTNORM / CONTINUE have to be defined in the same block
    // as the jumps that reach them. The instruction stream is unchanged -- this
    // is the same code in one pair of braces.
    //
    // `SIZE SkinInfo` became 32 for the same reason `SIZE D3DXVECTOR3` became
    // 12: clang's asm parser takes an expression there, not a type. SkinInfo is
    // FLOAT Weights[4] + int Indices[4], and the loop's own [edx+16]/[edx+20]/
    // [edx+24]/[edx+28] index reads confirm the layout.
    __asm
    {
        mov     ebx, pInData                   // store in data
        mov     ecx, pOutData                  // store out data
        mov     edx, pSkinInfo                 // store pSkinInfo
        mov     esi, pPallette                 // store pallette

    BEGIN:
        prefetchnta [ebx + 32*3]               // prefetch vert info
        prefetchnta [edx + 32*3]               // prefetch skin info

        MOVVEC( ebx )                          // store vec

        mov edi, [edx + 16]                    // set edi to pallette offset

        MOVSCALE( edx )                        // store scale

        MULMATRIX43ANDSCALE( esi )             // mul matrix * vec * scale

        STOREVECANDJMPNOWEIGHT(OUTVEC, edx + 20)  // store vec in reg, get next
                                               // pallette offset and skip if
                                               // offset == -1
        MULMATRIX43ANDSCALE( esi )             // mul matrix * vec * scale

        ADDVECANDJMPNOWEIGHT(OUTVEC, edx + 24) // add vec in reg, get next
                                               // pallette offset and skip if
                                               // offset == -1
        MULMATRIX43ANDSCALE( esi )             // mul matrix * vec * scale

        ADDVECANDJMPNOWEIGHT(OUTVEC, edx + 28) // add vec in reg, get next
                                               // pallette offset and skip if
                                               // offset == -1
        MULMATRIX43ANDSCALE( esi )             // mul matrix * vec * scale
        ADDVEC()                               // add vec

    OUTVEC:
        add   ebx, 12                          // increment invec pointer
        OUTVEC( ecx )                          // store vec
        mov   eax, dwNumNormalsPerVert         // mov num normals to eax
        add   ecx, 12                          // increment outvec pointer

        cmp   eax, 0                           // compare num normals and 0
        je CONTINUE                            // if no normals to compute,
                                               //  continue

    NORMALS:
        MOVVEC( ebx )                          // store vec

        mov edi, [edx + 16]                    // set edi to pallette offset

        MOVSCALE( edx )                        // store scale

        MULMATRIX33ANDSCALE( esi )             // mul matrix * vec * scale

        STOREVECANDJMPNOWEIGHT(OUTNORM, edx + 20) // store vec in reg, get next
                                               // pallette offset and skip if
                                               // offset == -1
        MULMATRIX33ANDSCALE( esi )             // mul matrix * vec * scale

        ADDVECANDJMPNOWEIGHT(OUTNORM, edx + 24)   // add vec in reg, get next
                                               // pallette offset and skip if
                                               // offset == -1
        MULMATRIX33ANDSCALE( esi )             // mul matrix * vec * scale

        ADDVECANDJMPNOWEIGHT(OUTNORM, edx + 28)   // add vec in reg, get next
                                               // pallette offset and skip if
                                               // offset == -1
        MULMATRIX43ANDSCALE( esi )             // mul matrix * vec * scale
        ADDVEC()                               // add vec

    OUTNORM:
        add   ebx, 12                          // increment invec pointer
        OUTVEC( ecx )                          // store outvec

        sub   eax, 1                           // subtract 1 from number of
                                               // normals left to compute
        add   ecx, 12                          // increment outvec pointer

        cmp   eax, 0                           // compare num normals and 0
        jne NORMALS                            // if no more normals to
                                               // compute, continue

    CONTINUE:
        add   edx, 32                          // increment skininfo pointer
        cmp edx, pEnd                          // compare eax and pEnd
        jne BEGIN                              // jump to end if finished
    }

    return;
}

