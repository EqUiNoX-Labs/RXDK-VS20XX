//-----------------------------------------------------------------------------
// File: Fur.cpp
//
// Desc: This is the main file for the fur demo, which shows how to draw fur
//       using vertex shaders (for shell expansion, lighting, and wind) and
//       pixel shaders (for combining slice textures with a hair lighting
//       texture and for fading-in level-of-detail layers.)
//
// Hist: 11.11.02 - Cleaned up for December XDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBUtil.h>
#include <XBHelp.h>
#include <XBMesh.h>
#include "Clip.h"
#include "XBFur.h"
#include "XBFurMesh.h"
#include "d3d8perf.h"
#include <assert.h>




//-----------------------------------------------------------------------------
// Help screen definitions
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_NormalHelpCallouts[] =
{
    { XBHELP_LEFTSTICK,    XBHELP_PLACEMENT_2, L"Rotate/zoom\nthe model"},
    { XBHELP_RIGHTSTICK,   XBHELP_PLACEMENT_2, L"Rotate the model\nand light source"},
    { XBHELP_DPAD,         XBHELP_PLACEMENT_1, L"Model count" },
    { XBHELP_X_BUTTON,     XBHELP_PLACEMENT_2, L"Toggle\nshells" },
    { XBHELP_Y_BUTTON,     XBHELP_PLACEMENT_1, L"Toggle fins" },
    { XBHELP_BLACK_BUTTON, XBHELP_PLACEMENT_2, L"Toggle self-\nshadowing" },
    { XBHELP_BACK_BUTTON,  XBHELP_PLACEMENT_1, L"Toggle help" },
    { XBHELP_MISC_CALLOUT, XBHELP_PLACEMENT_1, L"Right trigger to use blower" },
};
#define MAX_NORMAL_HELP_CALLOUTS (sizeof(g_NormalHelpCallouts)/sizeof(XBHELP_CALLOUT))




bool  g_bDrawFins      = true;
bool  g_bDrawShells    = true;
bool  g_bWind          = false;
bool  g_bLocalLighting = false;
bool  g_bSelfShadow    = false;

DWORD g_dwNumShellsDrawn;


D3DXVECTOR4 g_vWind1;   // Wind origin
D3DXVECTOR4 g_vWind2;   // Wind up vector
D3DXVECTOR4 g_vWind3;   // Wind left vector
FLOAT g_fWindChoose;    // Wind smooth start / stop


D3DXVECTOR3 g_LightPos; // Current light position
D3DXVECTOR3 g_EyePos;   // Current eye position
D3DXVECTOR3 g_vLookAt;




//-----------------------------------------------------------------------------
// Name: class CLightObj
// Desc: Class to draw a body for the light
//-----------------------------------------------------------------------------
class CLightObj
{
    DWORD        m_dwNumVertices;   // # of vertices
    DWORD        m_dwVertexSize;    // Vertex size
    LPDIRECT3DVERTEXBUFFER8 m_pVB;  // Vertex buffer

public:
    D3DXMATRIX  m_matOrientation;   // Orientation matrix

    CLightObj()
    {
        // Clear out relevant fields
        m_pVB = NULL;
        D3DXMatrixIdentity( &m_matOrientation );
    }

    ~CLightObj()
    {
        if( m_pVB )
            m_pVB->Release();
    }

    void BuildCylinder( FLOAT fRadius0, FLOAT fRadius1, FLOAT fLength )
    {
        struct VERTEX { D3DXVECTOR3 p; DWORD color; };

        DWORD dwNumSegments = 40;

        m_dwVertexSize  = sizeof(VERTEX);
        m_dwNumVertices = dwNumSegments*2;
        g_pd3dDevice->CreateVertexBuffer( m_dwNumVertices*m_dwVertexSize,
                                          0, 0, 0, &m_pVB );
        VERTEX *pVertices;
        m_pVB->Lock( 0, 0, (BYTE**)&pVertices, 0 );

        for( DWORD i=0; i<dwNumSegments; i++ )
        {
            FLOAT ss = sinf( 2*D3DX_PI*(float)(i)/(float)(dwNumSegments-1) );
            FLOAT cc = cosf( 2*D3DX_PI*(float)(i)/(float)(dwNumSegments-1) );

            // Top disc
            pVertices->p.x   = -fRadius0*ss;
            pVertices->p.y   = +fRadius0*cc;
            pVertices->p.z   = 0.0f;
            pVertices->color = 0xc0ffffff;
            pVertices++;

            // Bottom disc
            pVertices->p.x   = -fRadius1*ss;
            pVertices->p.y   = +fRadius1*cc;
            pVertices->p.z   = fLength;
            pVertices->color = 0x00ffffff;
            pVertices++;
        }

        m_pVB->Unlock();
    }

    void Render()
    {
        // Set state
        g_pd3dDevice->SetRenderState( D3DRS_LIGHTING,     FALSE );
        g_pd3dDevice->SetRenderState( D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_COLOR1 );
        g_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
        g_pd3dDevice->SetRenderState( D3DRS_SRCBLEND,     D3DBLEND_SRCALPHA );
        g_pd3dDevice->SetRenderState( D3DRS_DESTBLEND,    D3DBLEND_ONE );
        g_pd3dDevice->SetRenderState( D3DRS_CULLMODE,     D3DCULL_NONE );
        g_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, FALSE );
        g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 );
        g_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE );
        g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1 );
        g_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE );

        // Draw the light
        g_pd3dDevice->SetTransform( D3DTS_WORLD, &m_matOrientation );
        g_pd3dDevice->SetTexture( 0, NULL );
        g_pd3dDevice->SetVertexShader( D3DFVF_XYZ|D3DFVF_DIFFUSE );
        g_pd3dDevice->SetStreamSource( 0, m_pVB, m_dwVertexSize );
        g_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, m_dwNumVertices-2 );

        // Restore state
        g_pd3dDevice->SetRenderState( D3DRS_CULLMODE,     D3DCULL_CCW );
        g_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );
    }
};




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    CXBFont m_Font;
    CXBHelp m_Help;
    BOOL    m_bDrawHelp;
    
    D3DVECTOR   m_vViewAngle;
    D3DXVECTOR3 m_vCameraPos;       // Camera rest position
    D3DXVECTOR3 m_vEyePos;          // Eye position
    D3DXMATRIX  m_matProjection;    // Projection matrix
    D3DXMATRIX  m_matView;          // View matrix
    
    D3DXVECTOR3 m_vLightPos;
    D3DLIGHT8   m_Light0;           // D3D8 light
    D3DXVECTOR3 m_vLightAngle;
    CLightObj   m_LightObj;         // Light icon

#define TEDDYCOUNT 3
    struct Teddy                    // The teddy model comes in several levels of geometric detail
    {
        D3DXVECTOR3 m_vMin, m_vMax; // Bounding box
        CXBMesh m_Mesh;             // Skin base mesh, plus eyes and nose
        CXBFurMesh m_FurMesh;       // Base mesh for fur + "fins" to get better silhouettes
    };
    Teddy  m_rTeddy[TEDDYCOUNT];

    CXBFur m_Fur;                   // Fur texture

    DWORD  m_dwLoadPhase;           // Keeps track of current loading stage

    FLOAT  m_fLevelOfDetail;        // Scale factor for level of detail calculation

#define NMAXINSTANCE 1000
    struct Instance 
    {
        D3DXVECTOR3 vPosition;      // Position of instance
        FLOAT       fLevelOfDetail; // Level-of-detail value for this object
        UINT        iModel;         // Which model to use for this object
        D3DXMATRIX  matWorld;       // World matrix for instance
    } m_rInstance[NMAXINSTANCE];    // Model instances, sorted by distance from eye
    INT m_iNumInstances;
    
    struct ActiveInstance 
    {
        FLOAT fDist2;       // Squared distance from eye. This field must be first for qsort to work.
        INT   iInstance;    // Index of instance
    } m_rActiveInstance[NMAXINSTANCE];
    INT m_iNumActiveInstances;

    INT  m_iNumSlices;      // Number of slices for the fur texture

    UINT m_iTextureIndex;   // Most recently compressed texture index
    
#define VERTEXSHADER_CONFIGURATIONS 8    // Three bits: wind local_lighting self_shadowing
    DWORD m_rdwFurVS[VERTEXSHADER_CONFIGURATIONS];
    DWORD m_rdwFinVS[VERTEXSHADER_CONFIGURATIONS];

#define PIXELSHADER_CONFIGURATIONS 3    // 0=texture + hairlighting,  1=blend LOD texture + hairlighting, 2=fade texture + lighting
    DWORD m_rdwFurPS[PIXELSHADER_CONFIGURATIONS];
    
    HRESULT LoadModels();         // Load geometric models
    HRESULT ExtractFins();        // Group mesh edges of original model according to view direction
    HRESULT CreateHairTextures(); // Slice particle system hair into textures
    HRESULT UpdateInstances();

public:
    HRESULT Initialize();
    HRESULT FrameMove();
    HRESULT Render();

    CXBoxSample();
};

CXBoxSample g_xbApp;




//-----------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program. Initializes everything, and goes into a
//       message-processing loop. Idle time is used to render the scene.
//-----------------------------------------------------------------------------
void __cdecl main()
{
    if( FAILED( g_xbApp.Create() ) )
        return;
    g_xbApp.Run();
}




//-----------------------------------------------------------------------------
// Name: CXBoxSample()
// Desc: Constructor
//-----------------------------------------------------------------------------
CXBoxSample::CXBoxSample()
            :CXBApplication()
{
#if defined(_DEBUG) || defined(PROFILE)  // Don't vsync when profiling or debugging
    // Allow unlimited frame rate
    m_d3dpp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
#endif
    m_bDrawHelp      = FALSE;

    m_dwLoadPhase    = 0;
    m_fLevelOfDetail = 0.045f;
    m_iNumInstances  = 25;   // start with a bunch of teddy bears
    m_iNumSlices     = 8;   // Increase this number to get finer detail along the length of the fur
    
    for( UINT i = 0; i < VERTEXSHADER_CONFIGURATIONS; i++ )
    {
        m_rdwFurVS[i] = 0;
        m_rdwFinVS[i] = 0;
    }
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize device-dependant objects.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    HRESULT hr;

    // Create a font
    if( FAILED( hr = m_Font.Create( "Font.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Initialize the help system
    if( FAILED( hr = m_Help.Create( "Gamepad.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Set projection transform
    FLOAT fFOV = D3DX_PI/4;
    D3DXMatrixPerspectiveFovLH( &m_matProjection, fFOV, 640.0f/480.0f, 0.4f, 40.0f );
    m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &m_matProjection );

    // Load the fur and fin vertex shaders
    for( UINT iConfig = 0; iConfig < VERTEXSHADER_CONFIGURATIONS; iConfig++ )
    {
        int iWind  = (iConfig >> 2) & 1;
        int iLocal = (iConfig >> 1) & 1;
        int iSelf  = iConfig & 1;
        
        // Load the fur vertex shader
        {
            DWORD vsdecl[] = 
            {
                D3DVSD_STREAM( 0 ),
                D3DVSD_REG( 0, D3DVSDT_FLOAT3 ), // Vertex
                D3DVSD_REG( 1, D3DVSDT_FLOAT3 ), // Normal / hair tangent
                D3DVSD_REG( 2, D3DVSDT_FLOAT2 ), // Texture 0
                D3DVSD_END()
            };
            CHAR name[_MAX_PATH];
            _snprintf( name, _MAX_PATH, "Shaders\\fur_wind%d_local%d_self%d.xvu", iWind, iLocal, iSelf );
            name[_MAX_PATH - 1] = '\0';
            XBUtil_CreateVertexShader( name, vsdecl, &m_rdwFurVS[iConfig] );
            if( !m_rdwFurVS[iConfig] )
            {
                OUTPUT_DEBUG_STRING( "Initialize : error loading \"" );
                OUTPUT_DEBUG_STRING( name );
                OUTPUT_DEBUG_STRING( "\"\n" );
            }
        }
        
        // Load the fin vertex shader
        {
            DWORD vsdecl[] = 
            {
                D3DVSD_STREAM( 0 ),
                D3DVSD_REG( 0, D3DVSDT_FLOAT3 ), // Vertex
                D3DVSD_REG( 1, D3DVSDT_FLOAT3 ), // Normal / hair tangent
                D3DVSD_REG( 2, D3DVSDT_FLOAT2 ), // u,v
                D3DVSD_REG( 6, D3DVSDT_FLOAT3 ), // Fin face normal
                D3DVSD_END()
            };
            CHAR name[_MAX_PATH];
            _snprintf( name, _MAX_PATH, "Shaders\\fin_wind%d_local%d_self%d.xvu", iWind, iLocal, iSelf );
            name[_MAX_PATH - 1] = '\0';
            XBUtil_CreateVertexShader( name, vsdecl, &m_rdwFinVS[iConfig] );
            if( !m_rdwFinVS[iConfig] )
            {
                OUTPUT_DEBUG_STRING( "Initialize : error loading \"" );
                OUTPUT_DEBUG_STRING( name );
                OUTPUT_DEBUG_STRING( "\"\n" );
            }
        }
    }

    // create the fur pixel shaders
#pragma warning(push)
#pragma warning(disable: 4245)    // conversion from int to DWORD
    {
#include "furfade0.inl"
        g_pd3dDevice->CreatePixelShader( &psd, &m_rdwFurPS[0] );
    }
    {
#include "furfade1.inl"
        g_pd3dDevice->CreatePixelShader( &psd, &m_rdwFurPS[1] );
    }
    {
#include "furfade2.inl"
        g_pd3dDevice->CreatePixelShader( &psd, &m_rdwFurPS[2] );
    }
#pragma warning(pop)

    // Enable lighting
    m_pd3dDevice->SetRenderState( D3DRS_AMBIENT,  0x00000000 );
    m_pd3dDevice->SetRenderState( D3DRS_LIGHTING, TRUE );

    // Init the rest of the stuff
    float fRadius = 1.0f;
    m_vCameraPos  = D3DXVECTOR3( 0.0f, fRadius * 0.3f, fRadius * 2.0f );
    m_vLightPos   = D3DXVECTOR3( 0.0f, 0.0f, fRadius * 1.2f );
    m_vLightAngle = D3DXVECTOR3(-0.7f, 0.7f * D3DX_PI, 0.0f );
    m_vViewAngle  = D3DXVECTOR3( 0.0f, 0.8F * D3DX_PI, 0.0f );

    // Init light position and color
    memset( &m_Light0, 0, sizeof(D3DLIGHT8) );
    m_Light0.Type = D3DLIGHT_DIRECTIONAL;
    m_Light0.Position = m_vLightPos;
    D3DXVECTOR3 vLightDir;
    D3DXVec3Normalize( &vLightDir, &m_vLightPos );
    m_Light0.Direction = -vLightDir;
    m_Light0.Diffuse.r = 1.0f;
    m_Light0.Diffuse.g = 1.0f;
    m_Light0.Diffuse.b = 1.0f;
    m_Light0.Specular.r = 1.0f;
    m_Light0.Specular.g = 1.0f;
    m_Light0.Specular.b = 1.0f;
    m_Light0.Range = 1000.0f;
    m_Light0.Attenuation0 = 1.0f;
    m_Light0.Phi = D3DX_PI;
    m_Light0.Theta = D3DX_PI/4.0;
    m_Light0.Falloff = 0.f;
    m_pd3dDevice->LightEnable( 0, TRUE );
    m_pd3dDevice->SetLight( 0, &m_Light0 );

    // Light
    float fLength = 0.2f;
    float fRadius0 = 0.01f;
    float fRadius1 = fLength * sinf(m_Light0.Theta);    // Inner spotlight radius
    m_LightObj.BuildCylinder( fRadius0, fRadius1, fLength );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: CompareDist2()
// Desc: Used by SortInstances as an ordering function to sort models from
//       near to far.
//-----------------------------------------------------------------------------
static int __cdecl CompareDist2( const void* arg1, const void* arg2 )
{
    float f1 = *(float*)arg1;
    float f2 = *(float*)arg2;
    if( f1 < f2 ) 
        return -1;
    else if( f1 > f2 ) 
        return 1;
    else
        return 0;
}




//-----------------------------------------------------------------------------
// Name: UpdateInstances()
// Desc: Arrange the instances in a triangular grid,
//       cull based on current view, and then sort front to back.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::UpdateInstances()
{
    // Get Blinn-style clipping matrix for bounding box culling
    D3DXMATRIX matViewProj = m_matView * m_matProjection;
    D3DXMATRIX matViewProjClip;
    BlinnClipMatrix( &matViewProjClip, &matViewProj );

    FLOAT fScale = 3.0f;
    D3DXVECTOR3 vPosition( 0.0f, 0.0f, 0.0f );
    D3DXVECTOR3 vDir( fScale, 0.0f, 0.0f );
    UINT iStep = 0; // Current position in leg
    UINT nStep2 = 0;  // Number of steps in leg * 2
    m_iNumActiveInstances = 0;
    for( INT iInstance = 0; iInstance < m_iNumInstances; iInstance++ )
    {
        // Set current position
        m_rInstance[iInstance].vPosition = vPosition;

        // Set geometric level of detail
        // Simple scheme: high detail in center, less on outer rings
        if( iInstance == 0 )
            m_rInstance[iInstance].iModel = 0;
        else if( iInstance < 25 )
            m_rInstance[iInstance].iModel = 1;
        else
            m_rInstance[iInstance].iModel = 2;

        // Compute world matrix
        Teddy* pTeddy = &m_rTeddy[m_rInstance[iInstance].iModel];
        D3DXVECTOR3* p = &m_rInstance[iInstance].vPosition;
        D3DXMatrixTranslation(&m_rInstance[iInstance].matWorld, p->x, p->y, p->z);

        // Cull if completely outside view frustum
        D3DXMATRIX matWorldViewProjClip = m_rInstance[iInstance].matWorld * matViewProjClip;
        if( BoundingBoxInFrustum( matWorldViewProjClip, pTeddy->m_vMin, pTeddy->m_vMax ) )
        {
            // Compute distance squared
            D3DXVECTOR3 vEye = m_rInstance[iInstance].vPosition - m_vEyePos;
            FLOAT fDist2 = D3DXVec3LengthSq( &vEye );

            // Compute level of detail based on scaled squared distance
            m_rInstance[iInstance].fLevelOfDetail = m_fLevelOfDetail * fDist2;

            // Add to active list
            m_rActiveInstance[m_iNumActiveInstances].fDist2 = fDist2;
            m_rActiveInstance[m_iNumActiveInstances].iInstance = iInstance;
            m_iNumActiveInstances++;
        }
        
        // Move to next grid position
        vPosition += vDir;
        iStep++;
        if( iStep * 2 > nStep2 )
        {
            iStep = 0;
            nStep2++; // Increase number of steps every two legs
            vDir = D3DXVECTOR3(-vDir.z, 0.f, vDir.x);           // Rotate direction
        }
    }
    qsort( (void*)m_rActiveInstance, m_iNumActiveInstances, sizeof(ActiveInstance), &CompareDist2 );
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: LoadModels()
// Desc: load model files
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::LoadModels()
{
    static CHAR* rstrTeddy[TEDDYCOUNT] = 
    {
        "Models\\Teddy2000.xbg",
        "Models\\Teddy1000.xbg",
        "Models\\Teddy0500.xbg",
    };
    for( UINT iTeddy = 0; iTeddy < TEDDYCOUNT; iTeddy++ )
    {
        Teddy* pTeddy = &m_rTeddy[iTeddy];

        // Load meshes from xbg files
        if( FAILED( pTeddy->m_Mesh.Create( rstrTeddy[iTeddy], NULL ) ) )
            return XBAPPERR_MEDIANOTFOUND;

        // Copy VB and IB pointers from first mesh of input model
        XBMESH_DATA *pMeshData = pTeddy->m_Mesh.GetMesh( 0 );
        pTeddy->m_FurMesh.Initialize( pMeshData->m_dwFVF, pMeshData->m_dwNumVertices, &pMeshData->m_VB,
                                      pMeshData->m_dwNumIndices, &pMeshData->m_IB );
        
        // Calc bounding box
        pTeddy->m_Mesh.ComputeBoundingBox( &pTeddy->m_vMin, &pTeddy->m_vMax );
        
        if( iTeddy == 0 ) // Set lookat to center of most detailed model
            g_vLookAt = 0.5f * ( pTeddy->m_vMin +  pTeddy->m_vMax );  // Center of bb
    }
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: ExtractFins()
// Desc: extract the fins from all the models
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::ExtractFins()
{
    UINT  BinFactor = 5; // Factor that chooses the number of angle bins to discretize.  See XBFurMesh.cpp for more.
    FLOAT fFinDotProductThreshold = 0.75f; // Range of dot products of fin's face normal with eye vector
    FLOAT fFinEdgeTextureScale    = 7.00f; // Scaling term matches scale of texture as applied to underlying model

    for( UINT i = 0; i < TEDDYCOUNT; i++ )
    {
        // Extract fins
        m_rTeddy[i].m_FurMesh.ExtractFins( BinFactor, fFinDotProductThreshold,
                                           fFinEdgeTextureScale );
    }
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: CreateHairTextures()
// Desc: Create a particle system patch of hair and then sample
//       the patch into a slice texture.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::CreateHairTextures()
{
    // Setup hair parameters
    DWORD numfuzz    = 8000;
    DWORD slicexsize = 128;
    DWORD slicezsize = 128;
    DWORD numfuzzlib = 32;
    DWORD numslices  = m_iNumSlices;
    DWORD finWidth   = 256; // We can afford to make the fin texture a little larger,
    DWORD finHeight  = 128; // since there's just one instead of numslices
    m_Fur.m_fXSize = 0.1f;
    m_Fur.m_fYSize = 0.01f;
    m_Fur.m_fZSize = 0.1f;
    m_Fur.m_dwNumSegments  = 4;
    m_Fur.m_fuzzCenter.vel = D3DXVECTOR3( 0.0f, 4.f, 0.5f );
    m_Fur.m_fuzzRandom.vel = D3DXVECTOR3( 0.25f, 0.25f, 0.25f );
    m_Fur.m_fuzzCenter.acc = D3DXVECTOR3( 0.f, 0.f, 0.0f );
    m_Fur.m_fuzzRandom.acc = D3DXVECTOR3( 0.5f, 0.5f, 0.5f );
    m_Fur.m_fuzzRandom.colorBase = D3DXCOLOR( 0.2f, 0.2f, 0.2f, 0.0f );
    m_Fur.m_fuzzCenter.colorBase = D3DXCOLOR( 0.501961f, 0.250980f, 0.1f, 1.0f ) - 0.5f * m_Fur.m_fuzzRandom.colorBase;
    m_Fur.m_fuzzRandom.colorTip  = D3DXCOLOR( 0.1f, 0.1f, 0.1f, 0.1f );
    m_Fur.m_fuzzCenter.colorTip  = D3DXCOLOR( 0.1f, 0.1f, 0.1f, 0.1f );
    
    // Generate hair texture
    m_Fur.InitFuzz( numfuzz, numfuzzlib );
    m_Fur.GenSlices( numslices, slicexsize, slicezsize );
    // Generate fin texture with more image samples and more fuzz to get scaling right
    static FLOAT s_fFinXFraction = 0.25f; // Proportion of fur that is projected in fin texture
    static FLOAT s_fFinZFraction = 0.05f;
    m_Fur.GenFin( finWidth, finHeight, s_fFinXFraction, s_fFinZFraction );
    m_Fur.ComputeLevelOfDetailTextures();
    m_Fur.SetLevelOfDetail(0.f);
    m_Fur.m_fYSize = 0.055f;
    
    // make hair lighting texture
    D3DMATERIAL8 mtrl;
    ZeroMemory( &mtrl, sizeof(mtrl) );
    mtrl.Ambient  = D3DXCOLOR( 0.2f, 0.2f, 0.2f, 1.0f );
    mtrl.Diffuse  = D3DXCOLOR( 0.5f, 0.5f, 0.5f, 1.0f );
    mtrl.Specular = D3DXCOLOR( 1.0f, 1.0f, 1.0f, 1.0f );
    mtrl.Power    = 40.0f;
    m_Fur.SetHairLightingMaterial( &mtrl );
    
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
   int fmBegin = D3DPERF_BeginEvent( D3DCOLOR_ARGB(0xff,0xff,0x7f,0x7f), "FrameMove" );

    // Toggle help
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Load the scene in chunks so we don't wait several
    // seconds before the app starts up
    if( m_dwLoadPhase == 0 )
    {
        // Load the models
        LoadModels();
        m_dwLoadPhase++;
    }
    else if( m_dwLoadPhase == 1 )
    {
       CreateHairTextures();
       m_dwLoadPhase++;
    }
    else if( m_dwLoadPhase == 2 )
    {
       // Compute the fins
       ExtractFins();
       m_dwLoadPhase++;
    }
    else if( m_dwLoadPhase == 3 )
    {
        if( m_Fur.CompressNextTexture( D3DFMT_DXT4, &m_iTextureIndex ) == S_OK )
            m_dwLoadPhase++;
        else 
        {
            int fmEnd = D3DPERF_EndEvent();
            assert( fmBegin == fmEnd );
            return S_OK;
        }
    }

    // Toggle shells
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_X] )
        g_bDrawShells = !g_bDrawShells;

    // Toggle fins
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_Y] )
        g_bDrawFins = !g_bDrawFins;

    // Toggle self-shadowing
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_BLACK] )
        g_bSelfShadow = !g_bSelfShadow; 

    // Set number of models
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_UP )
    {
        if( m_iNumInstances < NMAXINSTANCE )
            m_iNumInstances++;
    }
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN )
    {
        if( m_iNumInstances > 1 )
            m_iNumInstances--;
    }
   
    // Move view
    m_vViewAngle.y -= m_DefaultGamepad.fX1*1.0f*m_fElapsedTime;
    if( m_vViewAngle.y>D3DX_PI*2 )
        m_vViewAngle.y -= D3DX_PI*2;
    if( m_vViewAngle.y<0.0f )
        m_vViewAngle.y += D3DX_PI*2;
    if( !(m_DefaultGamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) )
    {
        m_vViewAngle.x += m_DefaultGamepad.fY2*1.0f*m_fElapsedTime;
        if( m_vViewAngle.x>1.0f )
            m_vViewAngle.x = 1.0f;
        if( m_vViewAngle.x<-1.0f )
            m_vViewAngle.x = -1.0f;
    }

    m_vCameraPos.z -= m_DefaultGamepad.fY1*2.f*m_fElapsedTime;
    if( m_vCameraPos.z<0.2f)
        m_vCameraPos.z = 0.2f;

     // Move the camera around the model and always point right at it
    D3DXMATRIX m, m2;
    D3DXMatrixRotationYawPitchRoll( &m, m_vViewAngle.y, m_vViewAngle.x, m_vViewAngle.z );
    D3DXVec3TransformCoord( &m_vEyePos, &m_vCameraPos, &m );
    m_vEyePos += g_vLookAt;
    D3DXVECTOR3 up( 0.0f, 1.0f, 0.0f );
    D3DXMatrixLookAtLH( &m_matView, &m_vEyePos, &g_vLookAt, &up );
    m_pd3dDevice->SetTransform( D3DTS_VIEW, &m_matView );

    // Move the light around the model
    m_vLightAngle.y -= m_DefaultGamepad.fX2*1.0f*m_fElapsedTime;
    if( m_vLightAngle.y > D3DX_PI*2 )
        m_vLightAngle.y -= D3DX_PI*2;
    if( m_vLightAngle.y < 0.0f )
        m_vLightAngle.y += D3DX_PI*2;
    if( m_DefaultGamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB )
    {
        m_vLightAngle.x -= m_DefaultGamepad.fY2*1.0f*m_fElapsedTime;
        if( m_vLightAngle.x > 1.0f )
            m_vLightAngle.x = 1.0f;
        if( m_vLightAngle.x < -1.0f )
            m_vLightAngle.x = -1.0f;
    }
    D3DXMatrixRotationYawPitchRoll(&m, m_vLightAngle.y, m_vLightAngle.x, m_vLightAngle.z);
    D3DXVECTOR3 lpos;
    D3DXVec3TransformCoord(&lpos, &m_vLightPos, &m);
    D3DXVECTOR3 ldir;
    D3DXVec3Normalize(&ldir, &lpos);
    lpos += g_vLookAt;
    m_Light0.Position = lpos;
    m_Light0.Direction = -ldir;
    m_pd3dDevice->SetLight(0, &m_Light0);

    // Set world-space light and eye positions for vertex shader
    g_LightPos = lpos;
    g_EyePos = m_vEyePos;
    
    // Light looks at g_vLookAt
    D3DXMatrixLookAtLH( &m_LightObj.m_matOrientation, &g_LightPos, &g_vLookAt, &up );
    D3DXMatrixInverse( &m_LightObj.m_matOrientation, NULL, &m_LightObj.m_matOrientation );

    // Set wind parameters
    static float fWindAmplitude = 0.01f;
    static float fWindFrequency = 2.f * D3DX_PI / 0.2f;
    static float fWindZero = -0.25f;
    // static float fPenalty = 1.5f;
    static float fTangentPlaneFraction = 0.9f;
    static float fWindStart = -7.f; // Start the wind gradually
    static float fWindDecay = -5.f; // Stop the wind gradually 
    static float fWindSwirlRadius = 0.1f;
    static float fWindSwirlFrequency = 2.f * D3DX_PI / 0.3f;
    g_vWind1.x = g_LightPos.x;
    g_vWind1.y = g_LightPos.y;
    g_vWind1.z = g_LightPos.z;
    D3DXMATRIX *pmat = &m_LightObj.m_matOrientation;    // Grab left and up out of light matrix
    D3DXVECTOR3 vX(pmat->m[0][0], pmat->m[0][1], pmat->m[0][2]);
    D3DXVECTOR3 vY(pmat->m[1][0], pmat->m[1][1], pmat->m[1][2]);
    *(D3DXVECTOR3*)&g_vWind2 = vY;
    g_vWind2.w = fTangentPlaneFraction;
    *(D3DXVECTOR3*)&g_vWind3 = vX;
    g_vWind3.w = 0.f;
    if( m_DefaultGamepad.bAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER] )
    {
        // blow-dryer
        g_bWind = true;
        float fWind = fWindZero + fWindAmplitude * cosf( m_fTime * fWindFrequency ); 
        g_fWindChoose *= expf( fWindStart * m_fElapsedTime );
        g_vWind1.w = (1.f - g_fWindChoose) * fWind;
        // move source in small swirl around light source position
        float fWindSwirlX = fWindSwirlRadius * cosf( m_fTime * fWindSwirlFrequency );
        float fWindSwirlY = fWindSwirlRadius * sinf( m_fTime * fWindSwirlFrequency );
        *(D3DXVECTOR3 *)&g_vWind1 += fWindSwirlX * vX + fWindSwirlY * vY;
    }
    else
    {
        // turn-off wind
        g_fWindChoose = 1.f;
        g_vWind1.w *= expf( fWindDecay * m_fElapsedTime );

        // wait until wind has died out to turn off the wind vertex shader
        float fWindEpsilon = 1e-3f;
        if( fabsf(g_vWind1.w) < fWindEpsilon )
            g_bWind = false;
    }

    // Position, sort, and cull the instances
    D3DPERF_SetMarker( D3DCOLOR_ARGB(0xff, 0,0,0), "UpdateInstances" );
    UpdateInstances();
    
    int fmEnd = D3DPERF_EndEvent();
    assert( fmBegin == fmEnd );

    return S_OK;
}



//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, the call is the entry point for 3d
//       rendering. This function sets up render states, clears the
//       viewport, and renders the scene.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Render()
{
    // Clear the viewport
    RenderGradientBackground( 0xff1a1a1a, 0xff4d4d66 );

    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, D3DZB_TRUE );
    m_pd3dDevice->SetTransform( D3DTS_WORLD, &m_LightObj.m_matOrientation );

    // If the light is behind the models, draw it first. Else, draw it later.
    D3DXVECTOR3 L(g_LightPos - g_vLookAt);
    D3DXVECTOR3 E(g_EyePos - g_vLookAt);
    BOOL bLightIsBehind = (D3DXVec3Dot(&L, &E) < 0.0f);
    if( bLightIsBehind ) 
        m_LightObj.Render(); // Draw the light icon behind the fur

    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, D3DZB_TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CCW );
    m_pd3dDevice->SetRenderState( D3DRS_LIGHTING, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL );
    m_pd3dDevice->SetRenderState( D3DRS_NORMALIZENORMALS, TRUE ); // This is 

    // Render the base meshes from near to far to initialize the skin and zbuffer
    if( m_dwLoadPhase > 0 )
    {
        for( INT iActiveInstance = 0; iActiveInstance < m_iNumActiveInstances; iActiveInstance++ )       // near to far
        {
            int bmBegin = D3DPERF_BeginEvent( D3DCOLOR_ARGB(0xff,0xff,0xff,0x7f), "Bear Mesh %d", iActiveInstance );

            INT iInstance = m_rActiveInstance[iActiveInstance].iInstance;
            UINT iTeddy = m_rInstance[iInstance].iModel;
            Teddy* pTeddy = &m_rTeddy[iTeddy];
            m_pd3dDevice->SetTransform( D3DTS_WORLD, &m_rInstance[iInstance].matWorld );  // Set world transformation
            pTeddy->m_Mesh.Render();    // Draws skin, nose, and ears

            int bmEnd = D3DPERF_EndEvent();
            assert( bmBegin == bmEnd );
        }
    }

    // Render the fur in the opposite order, from far to near, so that alpha-blending works
    g_dwNumShellsDrawn = 0;
    if( m_dwLoadPhase>1 && (g_bDrawFins || g_bDrawShells) )
    {
        // Pick vertex shaders depending on current settings
        UINT iConfig = (g_bWind ? (1 << 2) : 0) | (g_bLocalLighting ? (1 << 1) : 0) | (g_bSelfShadow ? 1 : 0);
        DWORD dwFurVS = m_rdwFurVS[iConfig];
        DWORD dwFinVS = m_rdwFinVS[iConfig];
        static float fFinLODFull          = 3.50f;   // Fade out between fFinFull and fFinCutoff
        static float fFinLODCutoff        = 4.00f;   // LOD's above this value don't get fins
        static float fFinExtraNormalScale = 1.25f;
        D3DXMATRIX matViewProjection;
        D3DXMatrixMultiply( &matViewProjection, &m_matView, &m_matProjection );
        for( INT iActiveInstance = m_iNumActiveInstances - 1; iActiveInstance >= 0; iActiveInstance-- )      // Far to near
        {
            int bfBegin = D3DPERF_BeginEvent( D3DCOLOR_ARGB(0xff,0xff,0x7f,0x7f), "Bear Fur %d", iActiveInstance );
            
            INT iInstance = m_rActiveInstance[iActiveInstance].iInstance;
            UINT iTeddy = m_rInstance[iInstance].iModel;
            Teddy* pTeddy = &m_rTeddy[iTeddy];
            pTeddy->m_FurMesh.Begin(& g_EyePos, &g_LightPos, &matViewProjection );
            D3DXMATRIX *pmatWorld = &m_rInstance[iInstance].matWorld;
            D3DXMATRIX matWorldInverse;
            D3DXMatrixInverse( &matWorldInverse, NULL, pmatWorld );
            m_Fur.SetLevelOfDetail( m_rInstance[iInstance].fLevelOfDetail );
            pTeddy->m_FurMesh.BeginObject( pmatWorld, &matWorldInverse );
            
            int dfBegin = D3DPERF_BeginEvent( D3DCOLOR_ARGB(0xff,0x7f,0x0,0x0), "DrawFins" );
            if( m_dwLoadPhase > 2 && g_bDrawFins ) 
                pTeddy->m_FurMesh.DrawFins(&m_Fur, dwFinVS, fFinLODFull, fFinLODCutoff, fFinExtraNormalScale );
            int dfEnd = D3DPERF_EndEvent();
            assert( dfBegin == dfEnd );
            
            int dsBegin = D3DPERF_BeginEvent( 0, "DrawShells" ); // Inherit parent's color
            if( g_bDrawShells )
            {
                pTeddy->m_FurMesh.DrawShells( &m_Fur, dwFurVS, m_rdwFurPS );
                g_dwNumShellsDrawn += m_Fur.m_dwNumSlicesLOD;
            }
            int dsEnd = D3DPERF_EndEvent();
            assert( dsBegin == dsEnd );

            pTeddy->m_FurMesh.EndObject();
            pTeddy->m_FurMesh.End();
        
            int bfEnd = D3DPERF_EndEvent();
            assert( bfBegin == bfEnd );
        }
    }

    // Reset state
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
    
    // If we didn't draw the light above, draw it now
    if( !bLightIsBehind )  
        m_LightObj.Render();

    if( m_bDrawHelp )
    {
        // Show help
        m_Help.Render( &m_Font, g_NormalHelpCallouts, MAX_NORMAL_HELP_CALLOUTS );
    }
    else
    {
        // Show title and frame rate
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 48, 36, 0xffffffff,  L"Fur" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        if( m_dwLoadPhase > 3 ) 
            m_Font.DrawText( 592, 38, 0xffffff00, m_strFrameRate, XBFONT_RIGHT );

        // Show shell and instance stats
        WCHAR buf[100];

        swprintf( buf, L"Shells Drawn %d", g_dwNumShellsDrawn );
        m_Font.DrawText( 576, 100, 0xffffff00, buf, XBFONT_RIGHT );

        swprintf( buf, L"Model Count %d", m_iNumInstances );
        m_Font.DrawText( 576, 125, 0xffffff00, buf, XBFONT_RIGHT );

        swprintf( buf, L"Active Count %d", m_iNumActiveInstances );
        m_Font.DrawText( 576, 150, 0xffffff00, buf, XBFONT_RIGHT );

        // Show status
        if( m_dwLoadPhase == 0 )
            m_Font.DrawText( 280, 50, 0xff00ffff, L"Loading models", XBFONT_CENTER_X );
        else if( m_dwLoadPhase == 1 )
            m_Font.DrawText( 280, 50, 0xff00ffff, L"Generating hair", XBFONT_CENTER_X) ;
        else if( m_dwLoadPhase == 2 )
            m_Font.DrawText( 280, 50, 0xff00ffff, L"Extracting fins", XBFONT_CENTER_X );
        else if( m_dwLoadPhase == 3 )
        {
            swprintf( buf, L"Compressing texture %d of %d", m_iTextureIndex + 1, m_Fur.TotalTextureCount() );
            m_Font.DrawText( 280, 50, 0xff00ffff, buf, XBFONT_CENTER_X );
        }
        
        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}
