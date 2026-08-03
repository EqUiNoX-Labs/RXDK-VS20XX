//-----------------------------------------------------------------------------
// File: XBFurMesh.cpp
//
// Desc: Facilitates drawing of 'fuzz' patches (hair, fur, grass, etc.)
//       by extracting fins and computing appropriate levels of detail.
//
// Hist: 11.11.02 - Cleaned up for December XDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <stdio.h>
#include <assert.h>
#include "XBFurMesh.h"
#include "XBUtil.h"
#include "OctoSphere.h"


extern LPDIRECT3DDEVICE8 g_pd3dDevice;


struct FINVERTEX 
{
    D3DXVECTOR3 vPosition;        // Position
    D3DXVECTOR3 vNormal;          // Hair tangent for lighting calculation
    FLOAT       tu, tv;           // Fin texture coordinates
    D3DXVECTOR3 vFaceNormal;      // Face normal for fading fins out as they get perpendicular to eye
};


struct FURMESHVERTEX
{ 
    D3DXVECTOR3 p;
    D3DXVECTOR3 n;
    FLOAT       tu, tv;
};
#define D3DFVF_FURMESHVERTEX (D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_TEX1)




//-----------------------------------------------------------------------------
// Name: Constructor & Destructor
//-----------------------------------------------------------------------------
CXBFurMesh::CXBFurMesh()
{
    m_dwNumVertices = 0L;
    m_pVB           = NULL;
    m_dwNumIndices  = 0L;
    m_pIB           = NULL;
    m_pTexture      = NULL;
    m_fFinDotProductThreshold = 0.0f;
    m_dwNumFinBins  = 0L;
    ZeroMemory( &m_rFinBin,   sizeof(m_rFinBin) );
}


CXBFurMesh::~CXBFurMesh()
{
    if( m_pVB )
        m_pVB->Release();
    if( m_pIB )
        m_pIB->Release();
    if( m_pTexture )
        m_pTexture->Release();
    CleanFins();
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Keep pointer to object and fill initial arrays from selection.
//       If aIndices==NULL, do whole mesh.
//-----------------------------------------------------------------------------
HRESULT CXBFurMesh::Initialize( DWORD dwFVF, 
                                DWORD dwVertexCount, LPDIRECT3DVERTEXBUFFER8 pVB, 
                                DWORD dwIndexCount, LPDIRECT3DINDEXBUFFER8 pIB )
{
    assert(dwFVF==D3DFVF_FURMESHVERTEX);    // make sure we are using correct FVF type

    m_dwNumVertices = dwVertexCount;
    
    LPDIRECT3DVERTEXBUFFER8 pVBOld = m_pVB;
    m_pVB = pVB;
    m_pVB->AddRef();
    if( pVBOld ) pVBOld->Release();
    
    m_dwNumIndices = dwIndexCount;
    LPDIRECT3DINDEXBUFFER8 pIBOld = m_pIB;
    m_pIB = pIB;
    m_pIB->AddRef();
    if( pIBOld ) pIBOld->Release();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: ExtractFins()
// Desc: Find all the edges of the mesh and create an VB to draw them.
//-----------------------------------------------------------------------------
HRESULT CXBFurMesh::ExtractFins( UINT BinFactor, float fFinDotProductThreshold,
                                 float fEdgeTextureScale )
{
    HRESULT hr = S_OK;
    if (!m_pVB || !m_pIB) 
        return E_FAIL;
    DWORD dwVertexSize    = sizeof(FURMESHVERTEX);
    DWORD dwFinVertexSize = sizeof(FINVERTEX);

    FURMESHVERTEX* rVertex = NULL;
    UINT iFinBin;
    CleanFins();
    m_fFinDotProductThreshold = fFinDotProductThreshold;
    
    // count the number of fins by adding all the edges to a set
    UINT nFin = 0;
    UINT nFinMax = m_dwNumIndices; // 3 * nFace
    DWORD* rFin = new DWORD [nFinMax];
    if (rFin == NULL)
        return E_OUTOFMEMORY;
    WORD* piFace;
    hr = m_pIB->Lock(0, m_dwNumIndices*sizeof(WORD), (BYTE**)&piFace, 0);
    if (FAILED(hr))
        goto e_Exit;
    for (UINT iFace = 0; iFace < m_dwNumIndices/3; iFace++)
    {
        WORD* aiv = piFace + iFace * 3;
        for (UINT iCorner = 0; iCorner < 3; iCorner++)
        {
            WORD iVertex0 = aiv[iCorner];
            WORD iVertex1 = aiv[(iCorner + 1) % 3];
            if (iVertex0 > iVertex1)
            {
                // swap so the iVertex0 is always lower than iVertex1
                WORD iVertexSave = iVertex0;
                iVertex0 = iVertex1;
                iVertex1 = iVertexSave;
            }
            DWORD finKey = (((DWORD)iVertex0) << 16) | iVertex1;
            // find the right position
            UINT i = 0, j, k = nFin;
            while (i != k)
            {
                j = (i + k) >> 1;
                if (rFin[j] < finKey)
                    i = j + 1;
                else if (rFin[j] > finKey)
                    k = j;
                else // (rFin[i] == key)
                    break; // already in table
            }
            if (i == k)
            {
                // move all the slots one up, then add new key
                assert(nFin < nFinMax);
                for (j = nFin; j > i; j--)
                    rFin[j] = rFin[j-1];
                rFin[i] = finKey;
                nFin++;
            }
        }
    }
    m_pIB->Unlock();

    // Create discretized direction buckets by taking vertices from a subdivided octohedron
    hr = FillOctoSphere(BinFactor, &m_dwNumFinBins, NULL, NULL, NULL); // get size of buffers needed
    if (FAILED(hr))
        goto e_Exit;
    m_rFinBin = new FinBin [ m_dwNumFinBins ];
    if (FAILED(hr))
        goto e_Exit;
    {
        // copy direction vectors
        D3DXVECTOR3* rFinDir = new D3DXVECTOR3 [ m_dwNumFinBins ]; // temporary array to hold fin directions
        if (rFinDir == NULL)
        {
            hr = E_OUTOFMEMORY;
            goto e_Exit;
        }
        hr = FillOctoSphere( BinFactor, &m_dwNumFinBins, rFinDir, NULL, NULL );
        if (FAILED(hr))
        {
            delete [] rFinDir;
            goto e_Exit;
        }
        for (iFinBin = 0; iFinBin < m_dwNumFinBins; iFinBin++)
        {
            m_rFinBin[iFinBin].m_vDirection = rFinDir[iFinBin];
            m_rFinBin[iFinBin].m_fDirectionThreshold = 1.0f;    // first pass, use as dot-product threshold
            m_rFinBin[iFinBin].m_dwNumFins = 0;
            m_rFinBin[iFinBin].m_pFinVB = NULL;
//            m_rFinBin[iFinBin].m_pFinIB = NULL;
        }
        delete [] rFinDir;
    }
    // We need the original vertices to calculate fin face normals
    // and to copy over vertex data.
    hr = m_pVB->Lock(0, m_dwNumVertices*dwVertexSize, (BYTE**)&rVertex, 0);
    if (FAILED(hr))
        goto e_Exit;

    // For each fin, find the bin with the smallest dot-product
    // deviation and increment the count for that bin.  Each fin is
    // put into a single fin bin.
    float fBackfaceThreshold = -0.2f; // allow the normals to face slightly backwards
    for (UINT iFin = 0; iFin < nFin; iFin++)
    {
        // calculate face normal
        DWORD finKey = rFin[iFin];
        WORD iVertex0 = (WORD)(finKey >> 16);
        WORD iVertex1 = (WORD)(finKey & 0xffff);
        D3DXVECTOR3 e = rVertex[iVertex1].p - rVertex[iVertex0].p;
        float fEdgeLength = D3DXVec3Length(&e);
        e /= fEdgeLength; // normalize edge vector
        D3DXVECTOR3 n;
        D3DXVec3Normalize(&n, &rVertex[iVertex0].n);
        D3DXVECTOR3 normalFace;
        D3DXVec3Cross(&normalFace, &e, &n);
        D3DXVec3Normalize(&normalFace, &normalFace);
        for (UINT iDir = 0; iDir < 2; iDir++) // add fin as both front-facing and back-facing fin
        {
            if (iDir) // flip the normal
                normalFace *= -1.0f;
            // look for the best bin
            UINT iFinBinBest = (UINT)-1;
            float fDotProductBest = -1.0f;
            for (iFinBin = 0; iFinBin < m_dwNumFinBins; iFinBin++)
            {
                // Backface culling is based on the underlying mesh.  Does
                // at least one of the underlying mesh normals point in
                // the right direction, towards the eye?
                FinBin* pFinBin = &m_rFinBin[iFinBin];
                float fN0DotE = D3DXVec3Dot(&rVertex[iVertex0].n, &pFinBin->m_vDirection);
                float fN1DotE = D3DXVec3Dot(&rVertex[iVertex1].n, &pFinBin->m_vDirection);
                if (fN0DotE < fBackfaceThreshold && fN1DotE < fBackfaceThreshold)
                    continue; // Skip the backfacing fin bin

                // Is this the best bucket?
                float fNDotD = D3DXVec3Dot(&normalFace, &pFinBin->m_vDirection);
                if (fNDotD > fDotProductBest)
                {
                    iFinBinBest = iFinBin;
                    fDotProductBest = fNDotD;
                }
            }
            assert(iFinBinBest != -1);    // we must have found at least one bin
            m_rFinBin[iFinBinBest].m_dwNumFins++;
        }
    }

    // Create the fin bin vertex buffers
    for( iFinBin = 0; iFinBin < m_dwNumFinBins; iFinBin++ )
    {
        FinBin* pFinBin = &m_rFinBin[iFinBin];
        if (pFinBin->m_dwNumFins == 0) continue; // empty bin
        
        // Create the fin vertex buffer
        if( pFinBin->m_pFinVB )
            SAFE_RELEASE( pFinBin->m_pFinVB );
        g_pd3dDevice->CreateVertexBuffer( 4 * pFinBin->m_dwNumFins * dwFinVertexSize,
                                          0, 0, 0, &pFinBin->m_pFinVB );

        // Use the fin count as an index into the fin vertex buffer.  Since the fin dot-product
        // test below is identical to the test above, the fin count will be the same at the end.
        pFinBin->m_dwNumFins = 0;
    }

    // Add the fins to the appropriate fin bin
    for (UINT iFin = 0; iFin < nFin; iFin++)
    {
        // calculate face normal
        DWORD finKey = rFin[iFin];
        WORD iVertex0 = (WORD)(finKey >> 16);
        WORD iVertex1 = (WORD)(finKey & 0xffff);
        D3DXVECTOR3 e = rVertex[iVertex1].p - rVertex[iVertex0].p;
        float fEdgeLength = D3DXVec3Length(&e);
        e /= fEdgeLength; // normalize edge vector
        D3DXVECTOR3 n;
        D3DXVec3Normalize(&n, &rVertex[iVertex0].n);
        D3DXVECTOR3 normalFace;
        D3DXVec3Cross(&normalFace, &e, &n);
        D3DXVec3Normalize(&normalFace, &normalFace);
        for (UINT iDir = 0; iDir < 2; iDir++) // add fin as both front-facing and back-facing fin
        {
            if (iDir) // flip the normal
                normalFace *= -1.0f;
            // look for the best bin
            UINT iFinBinBest = (UINT)-1;
            float fDotProductBest = -1.0f;
            for (iFinBin = 0; iFinBin < m_dwNumFinBins; iFinBin++)
            {
                // Backface culling is based on the underlying mesh.  Does
                // at least one of the underlying mesh normals point in
                // the right direction, towards the eye?
                FinBin* pFinBin = &m_rFinBin[iFinBin];
                float fN0DotE = D3DXVec3Dot(&rVertex[iVertex0].n, &pFinBin->m_vDirection);
                float fN1DotE = D3DXVec3Dot(&rVertex[iVertex1].n, &pFinBin->m_vDirection);
                if (fN0DotE < fBackfaceThreshold && fN1DotE < fBackfaceThreshold)
                    continue; // Skip the backfacing fin bin

                // Is this the best bucket?
                float fNDotD = D3DXVec3Dot(&normalFace, &pFinBin->m_vDirection);
                if (fNDotD > fDotProductBest)
                {
                    iFinBinBest = iFinBin;
                    fDotProductBest = fNDotD;
                }
            }
            assert(iFinBinBest != -1);    // we must have found at least one bin
            FinBin* pFinBinBest = &m_rFinBin[iFinBinBest];
            UINT iFinBest =    pFinBinBest->m_dwNumFins++;    // increment the fin count for the best bin
            // get range of normal deviation for this bin
            float fNDotE = D3DXVec3Dot(&normalFace, &pFinBinBest->m_vDirection);
            if (pFinBinBest->m_fDirectionThreshold > fNDotE)
                pFinBinBest->m_fDirectionThreshold = fNDotE;

            // Copy the base and offset vertices
            FINVERTEX* pFinVertex;
            pFinBinBest->m_pFinVB->Lock( 4 * iFinBest * dwFinVertexSize, 4 * dwFinVertexSize,
                                         (BYTE**)&pFinVertex, 0 );
            float fEps = 1e-5f; // make slight offset from surface
            float u0 = rVertex[iVertex0].tu + rVertex[iVertex0].tv;    // initial point texture coordinate
            float u1 = u0 + fEdgeTextureScale * fEdgeLength;
            
            pFinVertex[0].vPosition   = rVertex[iVertex0].p;
            pFinVertex[0].vNormal     = rVertex[iVertex0].n;
            pFinVertex[0].tu          = u0;
            pFinVertex[0].tv          = fEps; // slight offset
            pFinVertex[0].vFaceNormal = normalFace;
        
            pFinVertex[1].vPosition   = rVertex[iVertex0].p; // extrusion done in fin.vsh, based on tex1.y
            pFinVertex[1].vNormal     = rVertex[iVertex0].n;
            pFinVertex[1].tu          = u0;
            pFinVertex[1].tv          = 1.0f;
            pFinVertex[1].vFaceNormal = normalFace;
        
            pFinVertex[2].vPosition   = rVertex[iVertex1].p; // extrusion done in fin.vsh, based on tex1.y
            pFinVertex[2].vNormal     = rVertex[iVertex1].n;
            pFinVertex[2].tu          = u1;
            pFinVertex[2].tv          = 1.0f;
            pFinVertex[2].vFaceNormal = normalFace;
        
            pFinVertex[3].vPosition   = rVertex[iVertex1].p;
            pFinVertex[3].vNormal     = rVertex[iVertex1].n;
            pFinVertex[3].tu          = u1;
            pFinVertex[3].tv          = fEps; // slight offset
            pFinVertex[3].vFaceNormal = normalFace;
        
            pFinBinBest->m_pFinVB->Unlock();
        }
    }

    // Convert bin thresholds from dot products to angles, add in fade
    // threshold, then convert back to dot product. Since we're in the
    // monotonic region of cosine (0 to pi), this avoids an acos call
    // in the draw routine.
    // Original test: acos(NdotE) > acos(NdotB) + acos(fFinDotProductThreshold)
    // Modified test: NdotE > cos(acos(NdotB) + acos(fFinDotProductThreshold))
    // Runtime test: NdotE > fDirectionThreshold
    float f = acosf(m_fFinDotProductThreshold);
    for (iFinBin = 0; iFinBin < m_dwNumFinBins; iFinBin++)
    {
        FinBin* pFinBin = &m_rFinBin[iFinBin];
        pFinBin->m_fDirectionThreshold = cosf(acosf(pFinBin->m_fDirectionThreshold) + f);
    }
    
e_Exit:
    if (rFin)
        delete [] rFin;
    if (rVertex)
        m_pVB->Unlock();
    if (FAILED(hr))
        CleanFins();
    return hr;
}




//-----------------------------------------------------------------------------
// Name: CleanFins()
// Desc: Clean up fin bins
//-----------------------------------------------------------------------------
HRESULT CXBFurMesh::CleanFins()
{
    for( DWORD i = 0; i < m_dwNumFinBins; i++)
    {
        FinBin* pFinBin = &m_rFinBin[i];
        SAFE_DELETE( pFinBin );
    }

    SAFE_DELETE_ARRAY(m_rFinBin);

    return S_OK;
}




//-----------------------------------------------------------------------------
// Desc: Vertex shader constant registers must match fur.vsh and fin.vsh
//-----------------------------------------------------------------------------
#define VSC_WORLD_VIEW_PROJECTION 0
#define VSC_OFFSET     12
#define VSC_EYE        13
#define VSC_LIGHT      14
#define VSC_HALF       15 // Used only for directional lighting
#define VSC_DIFFUSE    16
#define VSC_AMBIENT    17
#define VSC_SELFSHADOW 18
#define VSC_WIND1      20
#define VSC_WIND2      21
#define VSC_WIND3      22
#define VSC_FINFADE    23

static FLOAT     s_fSelfShadowScale = 255.0f; // Amplification factor for the self shadow term
static D3DXCOLOR s_DiffuseConditioning( 0.800f, 0.800f, 0.800f, 1.0f );    // Banks' diffuse-conditioning term
static D3DXCOLOR s_AmbientConditioning( 0.125f, 0.125f, 0.125f, 1.0f );




//-----------------------------------------------------------------------------
// Name: Begin()
// Desc: Setup texture stage and render state used for drawing both fur and fins.
//-----------------------------------------------------------------------------
void CXBFurMesh::Begin( D3DXVECTOR3* pvEyePos, D3DXVECTOR3* pvLightPos,
                        D3DXMATRIX* pmatViewProjection )
{
    // Set up volume texture state.  The texture is set per-shell below.

    // No apparent perf improvement for ALPHAKILL_ENABLE, but does add pop artifact on LOD switch
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAKILL, D3DTALPHAKILL_DISABLE );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE4X );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_MIPFILTER, D3DTEXF_LINEAR );
    
    g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    g_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE ); // Premultiplied alpha
    g_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
    g_pd3dDevice->GetRenderState( D3DRS_LIGHTING, &m_dwSavedLightingState );
    g_pd3dDevice->SetRenderState( D3DRS_LIGHTING, TRUE );
    g_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, TRUE );
    g_pd3dDevice->SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATER );
    g_pd3dDevice->SetRenderState( D3DRS_ALPHAREF,  0x00000000 );

    // Set up hair lighting texture state
    g_pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAKILL, D3DTALPHAKILL_DISABLE );
    g_pd3dDevice->SetTextureStageState( 1, D3DTSS_TEXCOORDINDEX, 1);
#if ZOCKLER
    g_pd3dDevice->SetTextureStageState( 1, D3DTSS_ADDRESSU, D3DTADDRESS_MIRROR );
    g_pd3dDevice->SetTextureStageState( 1, D3DTSS_ADDRESSV, D3DTADDRESS_MIRROR );
#else
    g_pd3dDevice->SetTextureStageState( 1, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP );
    g_pd3dDevice->SetTextureStageState( 1, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP );
#endif
    g_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_MODULATE2X );
    g_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    g_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLORARG2, D3DTA_CURRENT );
    g_pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1 );
    g_pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAARG1, D3DTA_CURRENT );
    g_pd3dDevice->SetTextureStageState( 1, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetTextureStageState( 1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetTextureStageState( 1, D3DTSS_MIPFILTER, D3DTEXF_LINEAR );

    // Set up coarse level-of-detail texture
    g_pd3dDevice->SetTextureStageState( 2, D3DTSS_ALPHAKILL, D3DTALPHAKILL_DISABLE );
    g_pd3dDevice->SetTextureStageState( 2, D3DTSS_TEXCOORDINDEX, 0);
    g_pd3dDevice->SetTextureStageState( 2, D3DTSS_ADDRESSU,  D3DTADDRESS_WRAP );
    g_pd3dDevice->SetTextureStageState( 2, D3DTSS_ADDRESSV,  D3DTADDRESS_WRAP );
    g_pd3dDevice->SetTextureStageState( 2, D3DTSS_COLOROP,   D3DTOP_DISABLE );
    g_pd3dDevice->SetTextureStageState( 2, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
    g_pd3dDevice->SetTextureStageState( 2, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetTextureStageState( 2, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetTextureStageState( 2, D3DTSS_MIPFILTER, D3DTEXF_LINEAR );

    // Clear out the rest of the texture stages
    g_pd3dDevice->SetTexture( 2, NULL );
    g_pd3dDevice->SetTextureStageState( 2, D3DTSS_COLOROP, D3DTOP_DISABLE );
    g_pd3dDevice->SetTextureStageState( 2, D3DTSS_ALPHAOP, D3DTOP_DISABLE );        
    g_pd3dDevice->SetTexture( 3, NULL );
    g_pd3dDevice->SetTextureStageState( 3, D3DTSS_COLOROP, D3DTOP_DISABLE );
    g_pd3dDevice->SetTextureStageState( 3, D3DTSS_ALPHAOP, D3DTOP_DISABLE );        
    
    // For the Banks' self-shadowing attenuation factor, inside the vertex shader, 
    // we want (1-p)^d, where d = h / NdotL.   Since we have only expp which gives 
    // the result to the power of two, we use 2^(log2(1-p)*d), 
    // Since expp doesn't accept negative exponents, we negate it here and divide 
    // by the factor inside the shader.
    static float fEps = 1e-4f;
    static float fRho = 0.02f;    // attenuation factor, Banks
    FLOAT       fSelfShadowLog2Attenuation = -logf(1.0f-fRho)/logf(2.0f);
    D3DXVECTOR4 vSelfShadowConstants( fEps, 0.0f, 0.0f, 0.0f );

#if 1
    vSelfShadowConstants.x = fEps;    // min value for N dot L
    vSelfShadowConstants.y = s_fSelfShadowScale * fSelfShadowLog2Attenuation;
#else
    float h = s_fSelfShadowScale*(m_vShellOffset.y - m_vShellOffset.x);
    vSelfShadowConstants.x = fEps;    // min value for N dot L
    extern bool g_bSelfShadow;    // DEBUG
    if( !g_bSelfShadow )
        vSelfShadowConstants.y = 0.0f;
    else
        vSelfShadowConstants.y = h * fSelfShadowLog2Attenuation;
#endif
    g_pd3dDevice->SetVertexShaderConstant( VSC_SELFSHADOW, &vSelfShadowConstants, 1 );
    g_pd3dDevice->SetVertexShaderConstant( VSC_DIFFUSE,    &s_DiffuseConditioning, 1 );
    g_pd3dDevice->SetVertexShaderConstant( VSC_AMBIENT,    &s_AmbientConditioning, 1 );
    m_vEyeWorldPos      = (*pvEyePos);
    m_vLightWorldPos    = (*pvLightPos);
    m_matViewProjection = (*pmatViewProjection);

    // Set fin fading constant
    D3DXVECTOR4 vFinFade( -m_fFinDotProductThreshold, 1.0f/(1.0f-m_fFinDotProductThreshold), 0.0f, 0.0f );
    g_pd3dDevice->SetVertexShaderConstant( VSC_FINFADE, &vFinFade, 1 );
}




//-----------------------------------------------------------------------------
// Name: BeginObject()
// Desc: Set matrix state in vertex shader constants
//-----------------------------------------------------------------------------
void CXBFurMesh::BeginObject( D3DXMATRIX* pmatWorld, D3DXMATRIX* pmatWorldInverse )
{
    // set display matrix
    D3DXMATRIX matWorldViewProjection;
    D3DXMatrixMultiply( &matWorldViewProjection, pmatWorld, &m_matViewProjection );
    D3DXMatrixTranspose( &matWorldViewProjection, &matWorldViewProjection );
    g_pd3dDevice->SetVertexShaderConstant( VSC_WORLD_VIEW_PROJECTION, &matWorldViewProjection, 4 );

    // Get direction of eye and light in object coords
    D3DXVec3TransformCoord( &m_vEyeObjectPos, &m_vEyeWorldPos, pmatWorldInverse );
    D3DXVec3TransformCoord( &m_vLightObjectPos, &m_vLightWorldPos, pmatWorldInverse );
#if 0
    D3DXVECTOR3 vAt(0.0f, 0.0f, 0.0f);    // center of object assumed to be at origin
#else
    extern D3DXVECTOR3 g_vLookAt;    // use same origin for all the objects
    D3DXVECTOR3 vAt;
    D3DXVec3TransformCoord( &vAt, &g_vLookAt, pmatWorldInverse );
#endif
    m_vEyeDirection = m_vEyeObjectPos - vAt;
    D3DXVec3Normalize( &m_vEyeDirection, &m_vEyeDirection );
    m_vLightDirection = m_vLightObjectPos - vAt;
    D3DXVec3Normalize( &m_vLightDirection, &m_vLightDirection );
    
    // set vertex shader light and eye positions (or directions for directional lighting) in object coords
    extern bool g_bLocalLighting;
    if( !g_bLocalLighting )
    {
        // directional lighting uses the same eye and light vectors for the whole object
        D3DXVECTOR3 vHalfDirection = (m_vEyeDirection + m_vLightDirection) * 0.5f;
        D3DXVec3Normalize( &vHalfDirection, &vHalfDirection );
        D3DXVECTOR4 vEye( m_vEyeDirection.x, m_vEyeDirection.y, m_vEyeDirection.z, 0.0f );
        D3DXVECTOR4 vLight( m_vLightDirection.x, m_vLightDirection.y, m_vLightDirection.z, 0.0f );
        D3DXVECTOR4 vHalf( vHalfDirection.x, vHalfDirection.y, vHalfDirection.z, 0.0f );
        g_pd3dDevice->SetVertexShaderConstant( VSC_HALF,  &vHalf,  1 );
        g_pd3dDevice->SetVertexShaderConstant( VSC_EYE,   &vEye,   1 );
        g_pd3dDevice->SetVertexShaderConstant( VSC_LIGHT, &vLight, 1 );
    }
    else
    {
        // local lighting computes eye and light vectors per vertex
        D3DXVECTOR4 vEye( m_vEyeObjectPos.x, m_vEyeObjectPos.y, m_vEyeObjectPos.z, 1.0f );
        D3DXVECTOR4 vLight( m_vLightObjectPos.x, m_vLightObjectPos.y, m_vLightObjectPos.z, 1.0f );
        g_pd3dDevice->SetVertexShaderConstant( VSC_EYE,   &vEye,   1 );
        g_pd3dDevice->SetVertexShaderConstant( VSC_LIGHT, &vLight, 1 );
    }
    
    extern D3DXVECTOR4 g_vWind1; // source position in x,y,z, local magnitude in w
    extern D3DXVECTOR4 g_vWind2; // source up, w = out-of-tangent plane fraction
    extern D3DXVECTOR4 g_vWind3; // source left
    g_pd3dDevice->SetVertexShaderConstant( VSC_WIND1, &g_vWind1, 1 );
    g_pd3dDevice->SetVertexShaderConstant( VSC_WIND2, &g_vWind2, 1 );
    g_pd3dDevice->SetVertexShaderConstant( VSC_WIND3, &g_vWind3, 1 );
}




//-----------------------------------------------------------------------------
// Name: DrawFins()
// Desc: Draw the extruded edges
//-----------------------------------------------------------------------------
void CXBFurMesh::DrawFins( CXBFur* pFur, DWORD dwFinVS, float fFinLODFull,
                           float fFinLODCutoff, float fFinExtraNormalScale )
{
    if( !pFur->m_pFinTexture )
        return;
    if( pFur->m_fLevelOfDetail > fFinLODCutoff )
        return;
    
    // Setup texture stage and render state for drawing fins.
    m_vShellOffset   = D3DXVECTOR4( pFur->m_fYSize, pFur->m_fYSize, 0.5f, 0.0f );
    m_vShellOffset.x = fFinExtraNormalScale * pFur->m_fYSize;
    m_vShellOffset.y = fFinExtraNormalScale * pFur->m_fYSize;
    g_pd3dDevice->SetVertexShaderConstant( VSC_OFFSET, &m_vShellOffset, 1 );
    g_pd3dDevice->SetVertexShader( dwFinVS );
    g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    g_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
    
    // Fade fins based on LOD transition range
    if( pFur->m_fLevelOfDetail >= fFinLODFull )
    {
        float f = 1.0f - (pFur->m_fLevelOfDetail - fFinLODFull) / (fFinLODCutoff - fFinLODFull);
        D3DXCOLOR diffuseFade = f * f * s_DiffuseConditioning;
        g_pd3dDevice->SetVertexShaderConstant( VSC_DIFFUSE, &diffuseFade, 1 );
    }
    else
        g_pd3dDevice->SetVertexShaderConstant( VSC_DIFFUSE, &s_DiffuseConditioning, 1 );
    g_pd3dDevice->SetPixelShader( NULL );
    
    // Set volume texture on stage 0
    g_pd3dDevice->SetTexture( 0, pFur->m_pFinTexture );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0 );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE2X );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE ); // Fin fade
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAKILL, D3DTALPHAKILL_DISABLE ); // D3DTALPHAKILL_ENABLE
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU,  D3DTADDRESS_WRAP );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV,  D3DTADDRESS_CLAMP ); // Don't wrap tips of hair
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_MIPFILTER, D3DTEXF_LINEAR );
    
    // Leave stage 1, hair lighting texture
    g_pd3dDevice->SetTexture( 1, pFur->m_pHairLightingTexture);
    
    // Disable stage 2
    g_pd3dDevice->SetTexture( 2, NULL);
    g_pd3dDevice->SetTextureStageState( 2, D3DTSS_COLOROP, D3DTOP_DISABLE );
    g_pd3dDevice->SetTextureStageState( 2, D3DTSS_ALPHAOP, D3DTOP_DISABLE );

    // Draw the active fins
    for( UINT i = 0; i < m_dwNumFinBins; i++ )
    {
        FinBin* pFinBin = &m_rFinBin[i];
        if( pFinBin->m_dwNumFins == 0 ) // Skip empty bins
            continue;

        // Skip fins that are outside the active cone of normals
        float fNDotE = D3DXVec3Dot( &m_vEyeDirection, &pFinBin->m_vDirection );
        if( fNDotE < pFinBin->m_fDirectionThreshold )
            continue;
        
        // Draw the active fins
        g_pd3dDevice->SetStreamSource( 0, pFinBin->m_pFinVB, sizeof(FINVERTEX) );
        g_pd3dDevice->DrawPrimitive( D3DPT_QUADLIST, 0, pFinBin->m_dwNumFins );
    }

    // Cleanup after drawing fins.
    g_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
}




//-----------------------------------------------------------------------------
// Name: DrawShells()
// Desc: Draw the fuzz patch as layers of texture using alpha blending
//-----------------------------------------------------------------------------
void CXBFurMesh::DrawShells( CXBFur* pFur, DWORD dwFurVS, DWORD dwFurPS[3] )
{
     // Restore diffuse conditioning value
    g_pd3dDevice->SetVertexShaderConstant( VSC_DIFFUSE, &s_DiffuseConditioning, 1 );
    
    g_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );

    g_pd3dDevice->SetVertexShader( dwFurVS );
    g_pd3dDevice->SetStreamSource( 0, m_pVB, sizeof(FURMESHVERTEX) );
    if( m_pIB ) g_pd3dDevice->SetIndices( m_pIB, 0 );
    
    // Stage 0 = Volume texture
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1);
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1);
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAKILL, D3DTALPHAKILL_DISABLE ); // D3DTALPHAKILL_ENABLE
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_TEXCOORDINDEX, 0);
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU,  D3DTADDRESS_WRAP );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV,  D3DTADDRESS_WRAP );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_MIPFILTER, D3DTEXF_LINEAR );

    // Stage 1 = Hair lighting texture
    g_pd3dDevice->SetTexture( 1, pFur->m_pHairLightingTexture);
    
    // Stage 2 = LOD texture
    UINT iLOD = pFur->m_iLOD > pFur->m_dwLODMax ? pFur->m_dwLODMax : pFur->m_iLOD;
    float fOffset = pFur->m_fYSize * (float)(1 << iLOD) / (float)pFur->m_dwNumSlices;
    
    for( UINT i=0; i<pFur->m_dwNumSlicesLOD; i++ )
    {
        // Set shell offset
        m_vShellOffset = D3DXVECTOR4( (i+1) * fOffset, pFur->m_fYSize, 0.5f, 0.0f );
        g_pd3dDevice->SetVertexShaderConstant( VSC_OFFSET, &m_vShellOffset, 1 );

        // Set pixel shaders and constants to fade even levels to clear and
        // odd levels to coarser level approximations that include the composited even levels.
        g_pd3dDevice->SetTexture( 0, pFur->m_pSliceTextureLOD[i] );
        
        if( pFur->m_fLODFraction == 0.0f )
        {
            // On a level-of-detail boundary, do not blend between slice textures. 
            g_pd3dDevice->SetPixelShader( dwFurPS[0] );
            g_pd3dDevice->SetTexture( 2, NULL);
            g_pd3dDevice->SetTextureStageState( 2, D3DTSS_COLOROP, D3DTOP_DISABLE );
            g_pd3dDevice->SetTextureStageState( 2, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
        }
        else
        {
            if( i & 1 ) // Odd level
            {
                // fade odd level to coarser texture as fraction goes to 1
                UINT nLOD = pFur->LevelOfDetailCount( pFur->m_iLOD );
                LPDIRECT3DTEXTURE8 pTextureLOD = (pFur->m_pSliceTextureLOD + nLOD)[i>>1];
                g_pd3dDevice->SetPixelShader( dwFurPS[1] );
                g_pd3dDevice->SetTexture( 2, pTextureLOD );
                g_pd3dDevice->SetTextureStageState( 2, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
                g_pd3dDevice->SetTextureStageState( 2, D3DTSS_COLORARG1, D3DTA_TEXTURE);
                g_pd3dDevice->SetTextureStageState( 2, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
                g_pd3dDevice->SetTextureStageState( 2, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
                g_pd3dDevice->SetTextureStageState( 2, D3DTSS_ALPHAKILL, D3DTALPHAKILL_DISABLE ); // D3DTALPHAKILL_ENABLE
                g_pd3dDevice->SetTextureStageState( 2, D3DTSS_TEXCOORDINDEX, 0);    
                g_pd3dDevice->SetTextureStageState( 2, D3DTSS_ADDRESSU,  D3DTADDRESS_WRAP );
                g_pd3dDevice->SetTextureStageState( 2, D3DTSS_ADDRESSV,  D3DTADDRESS_WRAP );
                g_pd3dDevice->SetTextureStageState( 2, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
                g_pd3dDevice->SetTextureStageState( 2, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
                g_pd3dDevice->SetTextureStageState( 2, D3DTSS_MIPFILTER, D3DTEXF_LINEAR );
            }
            else // Even level
            {
                // Fade even level to clear as fraction goes to 1
                g_pd3dDevice->SetPixelShader( dwFurPS[2] );
                g_pd3dDevice->SetTexture( 2, NULL);
                g_pd3dDevice->SetTextureStageState( 2, D3DTSS_COLOROP, D3DTOP_DISABLE );
                g_pd3dDevice->SetTextureStageState( 2, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
            }
            D3DXCOLOR colorFraction( pFur->m_fLODFraction, pFur->m_fLODFraction,
                                     pFur->m_fLODFraction, pFur->m_fLODFraction );
            g_pd3dDevice->SetPixelShaderConstant( 0, &colorFraction, 1) ;
        }

        // Draw the offset mesh
        if( NULL == m_pIB )
        {
            g_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLELIST, 0, m_dwNumIndices/3 );
        }
        else
        {
            static BOOL bReverseOrder = FALSE;
            if( bReverseOrder )
            {
                UINT FaceCount = m_dwNumIndices / 3;
                for( UINT iFace = 0; iFace < FaceCount; iFace++ )
                    g_pd3dDevice->DrawIndexedPrimitive( D3DPT_TRIANGLELIST, 0, m_dwNumVertices,
                                                        (FaceCount - 1 - iFace) * 3, 1 );
            }
            else
                g_pd3dDevice->DrawIndexedPrimitive( D3DPT_TRIANGLELIST, 0, m_dwNumVertices,
                                                    0, m_dwNumIndices/3 );
        }
    }
}




//-----------------------------------------------------------------------------
// Name: EndObject()
// Desc: Cleanup after drawing a furry object
//-----------------------------------------------------------------------------
void CXBFurMesh::EndObject()
{
    // currently does nothing
}




//-----------------------------------------------------------------------------
// Name: End()
// Desc: Cleanup after drawing fins and fur
//-----------------------------------------------------------------------------
void CXBFurMesh::End()
{
    // Cleanup render and texture stage states.
    g_pd3dDevice->SetRenderState( D3DRS_LIGHTING, m_dwSavedLightingState );
    g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    g_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
    g_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    g_pd3dDevice->SetTexture( 0, NULL );
    g_pd3dDevice->SetTexture( 1, NULL );
    g_pd3dDevice->SetTexture( 2, NULL );
    g_pd3dDevice->SetTexture( 3, NULL );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_MIPFILTER, D3DTEXF_NONE );
    g_pd3dDevice->SetTextureStageState( 1, D3DTSS_MIPFILTER, D3DTEXF_NONE );
    g_pd3dDevice->SetTextureStageState( 2, D3DTSS_MIPFILTER, D3DTEXF_NONE );
    g_pd3dDevice->SetTextureStageState( 3, D3DTSS_MIPFILTER, D3DTEXF_NONE );
    g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAKILL, D3DTALPHAKILL_DISABLE );
    g_pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAKILL, D3DTALPHAKILL_DISABLE );
    g_pd3dDevice->SetTextureStageState( 2, D3DTSS_ALPHAKILL, D3DTALPHAKILL_DISABLE );
    g_pd3dDevice->SetTextureStageState( 3, D3DTSS_ALPHAKILL, D3DTALPHAKILL_DISABLE );
    g_pd3dDevice->SetPixelShader( NULL );
}
