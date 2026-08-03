//-----------------------------------------------------------------------------
// File: Trees.cpp
//
// Desc: Tree rendering with a hierarchy of slice texture level-of-detail 
//       representations.
//
// Hist: 11.11.02 - Cleaned up for December XDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <XBMesh.h>
#include <XBResource.h>
#include <XBUtil.h>
#include <assert.h>
#include <xgraphics.h>
#include "Clip.h"
#include "Terrain.h"
#include "Tree.h"
#include "mipmap.h"




//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_LEFTSTICK,    XBHELP_PLACEMENT_2, L"Move in-out\nand left-right" },
    { XBHELP_RIGHTSTICK,   XBHELP_PLACEMENT_2, L"Rotate and\ntilt view" },
    { XBHELP_DPAD,         XBHELP_PLACEMENT_2, L"Move up\nand down" },
    { XBHELP_WHITE_BUTTON, XBHELP_PLACEMENT_1, L"Wire-frame" },
    { XBHELP_BLACK_BUTTON, XBHELP_PLACEMENT_1, L"Toggle debug" },
    { XBHELP_BACK_BUTTON,  XBHELP_PLACEMENT_1, L"Toggle help" },
    { XBHELP_X_BUTTON,     XBHELP_PLACEMENT_2, L"Toggle\nLOD" },
    { XBHELP_Y_BUTTON,     XBHELP_PLACEMENT_2, L"Toggle culling\ndisplay" },
    { XBHELP_A_BUTTON,     XBHELP_PLACEMENT_1, L"Tree count +" },
    { XBHELP_B_BUTTON,     XBHELP_PLACEMENT_1, L"Tree count -" },
    { XBHELP_MISC_CALLOUT, XBHELP_PLACEMENT_1, L"Use triggers for height" },
};
#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts) / sizeof(XBHELP_CALLOUT))




//-----------------------------------------------------------------------------
// Global parameters and view toggles
//-----------------------------------------------------------------------------
D3DLIGHT8   g_d3dLight; 
D3DXVECTOR3 g_vLightDirection = D3DXVECTOR3(0.1f, 0.95f, 0.2f);
D3DXVECTOR3 g_vMin(-1000.f, 0.f, -1000.f), g_vMax(1000.f, 20.f, 1000.f);    // scale of height field in world coords
D3DXVECTOR2 g_vTerrainTextureScale(150.f, 150.f), g_vTerrainTextureOffset(0.f, 0.f);


// TODO: move all of this level-of-detail stuff down into CTree
BOOL g_bUseIntermediateLOD = TRUE;
BOOL g_bUseLowLOD          = TRUE;

BOOL g_bWireFrame          = FALSE;
BOOL g_bDrawHelp           = FALSE;

BOOL g_bCompressTextures   = FALSE;   // Compress slice textures
BOOL g_bDebugSlice         = FALSE;   // Draw slices with color coding and print out stats
BOOL g_bDebugSliceOpaque   = FALSE;   // Draw slices without using slice texture
BOOL g_bDebugCulling       = FALSE;   // Draw using "from-the-side" projection


enum { DEBUG_NONE, DEBUG_PRINT, DEBUG_SLICE, DEBUG_SLICE_OPAQUE } g_eDebugMode = DEBUG_NONE;


// Tree and slice stats
struct TREESTATS 
{
    DWORD   dwActiveCount;
    DWORD   dwFullGeometryCount;
    DWORD   dwBranchSliceCount;
    DWORD   dwSliceCount;
};
TREESTATS g_TREESTATS;
DWORD     g_dwTotalSliceCount = 0;


#define NTREELIBRARY 1
#define NTREEX 40
#define NTREEZ 40
#define NMAXTREEINSTANCE (NTREEX * NTREEZ)




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
public:
    CXBFont            m_Font;                  // Font for rendering stats and help
    CXBHelp            m_Help;                  // Help class
    CXBPackedResource  m_xprResources;          // Packed texture resources
    LPDIRECT3DTEXTURE8 m_pBlendTexture;         // Render target for level-of-detail transitions
    LPDIRECT3DSURFACE8 m_pBlendDepthBuffer;     // Depth buffer for level-of-detail transitions
    FLOAT              m_fBlendFactor;          // Blend factor that smoothly changes from 0 to 1
    D3DXVECTOR3        m_vFrom, m_vAt, m_vUp;   // Viewing parameters
    D3DXMATRIX         m_matWorld;
    D3DXMATRIX         m_matView;
    D3DXMATRIX         m_matViewInverse;
    D3DXMATRIX         m_matProjection;
    Terrain*           m_pTerrain;              // Current terrain representation
    LPDIRECT3DTEXTURE8 m_pTextureTerrainShadow; // Shadows cast on terrain by trees
    CTree              m_rTreeLibrary[NTREELIBRARY];
    FLOAT              m_fLevelOfDetail;          // Scale factor for level of detail calculation
    D3DXVECTOR3        m_vFromLevelOfDetail[2];   // Position of previous level-of-detail update
    D3DXVECTOR3        m_vAtLevelOfDetail[2];     // View position of previous level-of-detail update
    INT                m_iNumTrees[2];            // Current and previous number of trees
    FLOAT              m_fNumTrees;               // For incrementing/decrementing number of trees
    struct TreeSort
    {
        FLOAT fDist2;             // Squared distance from eye. This field must be first for qsort to work.
        UINT  iTree;              // Index into m_TreeArray array
    };
    TreeSort* m_TreeSortArray[2];      // Model instances, sorted by distance from eye
    
    struct TreeData 
    {
        D3DXVECTOR3 vPosition;    // Position of instance
        FLOAT       fYRot;        // Y rotation of instance
        D3DXMATRIX  mat;          // Tree to world coords
        D3DXMATRIX  matInv;       // World to tree coords
        struct LOD 
        {
            FLOAT fLevelOfDetail;   // Level-of-detail values for this object
            D3DXVECTOR3 vFrom;      // Local eye position determines sorting order for branches, etc.
            DWORD dwDrawFlags;      // Set to TREE_DRAWTRUNK | TREE_DRAWBRANCHES depending on level of detail
            FLOAT fTreeSliceTextureLOD;
            FLOAT fBranchSliceTextureLOD;
        };
        LOD         LOD[2];
        BOOL        bSameLevelOfDetail; // The level-of-detail representation is the same for both rendering passes
        BOOL        bVisible;     // Is tree visible?
        D3DXVECTOR3 vFromFade;    // Current vFrom value in local tree coords
        UINT        iTreeLibrary; // Index into m_rTreeLibrary
    };
    TreeData* m_TreeArray;

    HRESULT UpdateTrees();  // update sorting and level-of-detail parameters
    HRESULT CullTrees();    // cull trees based on current viewing matrices
    HRESULT ShadowTrees();  // render current set of trees onto terrain texture
    HRESULT BlendScreenTexture(LPDIRECT3DTEXTURE8 pTexture, D3DCOLOR colorBlend); // blend texture with backbuffer

public:
    HRESULT Initialize();
    HRESULT FrameMove();
    HRESULT Render();

    CXBoxSample();
};




//-----------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program. Initializes everything, and goes into a
//       message-processing loop. Idle time is used to render the scene.
//-----------------------------------------------------------------------------
void __cdecl main()
{
    CXBoxSample xbApp;
    if( FAILED( xbApp.Create() ) )
        return;
    xbApp.Run();
}




//-----------------------------------------------------------------------------
// Name: CXBoxSample()
// Desc: Constructor
//-----------------------------------------------------------------------------
CXBoxSample::CXBoxSample()
            :CXBApplication()
{
#if defined(_DEBUG) || defined(PROFILE)  // Don't vsync when profiling or debugging
    m_d3dpp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;    // Allow unlimited frame rate
#else
    m_d3dpp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_TWO;  // 30Hz
#endif

    m_pTerrain              = NULL;
    m_pTextureTerrainShadow = NULL;
    m_fLevelOfDetail        = 1.0f; // 0.045f;
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize device-dependant objects.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    HRESULT hr;
    m_bPaused = TRUE; // FALSE;

    // Create a font
    if( FAILED( hr = m_Font.Create( "Font.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create the help
    if( FAILED( m_Help.Create( "Gamepad.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Load resources from the packed resource file
    if( FAILED( m_xprResources.Create( "Resource.xpr" ) ) )
        return E_FAIL;

    // Create the texture used for blending level-of-detail transitions
    D3DSURFACE_DESC descBackBuffer;
    m_pBackBuffer->GetDesc( &descBackBuffer );
    hr = m_pd3dDevice->CreateTexture( descBackBuffer.Width, descBackBuffer.Height, 1, 0,
                                      D3DFMT_LIN_A8R8G8B8, D3DPOOL_DEFAULT, &m_pBlendTexture );
    if( FAILED(hr) )
        return hr;
    hr = m_pd3dDevice->CreateDepthStencilSurface( descBackBuffer.Width, descBackBuffer.Height, D3DFMT_LIN_D24S8,
                                                  D3DMULTISAMPLE_NONE, &m_pBlendDepthBuffer );
    if( FAILED(hr) )
        return hr;

    // Set light parameters to mimic the sun
    ZeroMemory( &g_d3dLight, sizeof(D3DLIGHT8) );
    g_d3dLight.Type      = D3DLIGHT_DIRECTIONAL;
    g_d3dLight.Position  = D3DXVECTOR3(10000.0f, 10000.0f, 10000.0f);
    g_d3dLight.Direction = g_vLightDirection;
    g_d3dLight.Ambient   = D3DXCOLOR(0.4f, 0.4f, 0.4f, 1.0f);
    g_d3dLight.Diffuse   = D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f);
    g_d3dLight.Specular  = D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f);
    m_pd3dDevice->SetLight( 0, &g_d3dLight );
    m_pd3dDevice->LightEnable( 0, TRUE );

    // Load the terrain image and create the height field mesh
    m_pTerrain = new Terrain;
    if( m_pTerrain == NULL )
        return E_OUTOFMEMORY;
    LPDIRECT3DTEXTURE8 pTextureTerrain    = m_xprResources.GetTexture( "TerrainTexture" );
    LPDIRECT3DTEXTURE8 pTextureUndergrass = m_xprResources.GetTexture( "undergrass" );
    hr = m_pd3dDevice->CreateTexture( 512, 512, 0, 0, D3DFMT_A8R8G8B8,
                                      D3DPOOL_DEFAULT, &m_pTextureTerrainShadow );
    if( FAILED(hr) )
        return hr;
    UINT widthTerrain = 40, heightTerrain = 40; // Number of samples in terrain mesh
    hr = m_pTerrain->Initialize( pTextureTerrain, pTextureUndergrass, m_pTextureTerrainShadow,
                                 g_vMin, g_vMax, g_vTerrainTextureScale, g_vTerrainTextureOffset,
                                 widthTerrain, heightTerrain );
    if (FAILED(hr))
        return hr;

    // Create the tree library
#define BOUNDSET_XMIN 001
#define BOUNDSET_YMIN 002
#define BOUNDSET_ZMIN 004
#define BOUNDSET_XMAX 010
#define BOUNDSET_YMAX 020
#define BOUNDSET_ZMAX 040
#define BOUNDSET_XZ (BOUNDSET_XMIN|BOUNDSET_XMAX|BOUNDSET_ZMIN|BOUNDSET_ZMAX)
    
    struct TreeLibraryData
    {
        CHAR*  strName;
        DWORD  dwFlags;
        struct Vector { float x, y, z; };
        Vector vScale, vMin, vMax;
    };
    static TreeLibraryData rTreeLibraryData[NTREELIBRARY] =  
    {
        { "tree1",  0,  { 1.0f, 1.0f, 1.0f} },   // Use tree's bbox
    };
    for( UINT iTreeLibrary = 0; iTreeLibrary < NTREELIBRARY; iTreeLibrary++ )
    {
        CTree*           pTreeLibrary     = &m_rTreeLibrary[iTreeLibrary];
        TreeLibraryData* pTreeLibraryData = &rTreeLibraryData[iTreeLibrary];
        if( FAILED( pTreeLibrary->Create( pTreeLibraryData->strName, &m_xprResources ) ) )
            return XBAPPERR_MEDIANOTFOUND;
        // pTreeLibrary->Scale(*(D3DXVECTOR3 *)&pTreeLibraryData->vScale); // scale top-level frames and then get new bounding box
        // Set bounds to be different than actual geometry bounding box
        if( pTreeLibraryData->dwFlags & BOUNDSET_XMIN ) pTreeLibrary->m_vMin.x = pTreeLibraryData->vMin.x;
        if( pTreeLibraryData->dwFlags & BOUNDSET_YMIN ) pTreeLibrary->m_vMin.y = pTreeLibraryData->vMin.y;
        if( pTreeLibraryData->dwFlags & BOUNDSET_ZMIN ) pTreeLibrary->m_vMin.z = pTreeLibraryData->vMin.z;
        if( pTreeLibraryData->dwFlags & BOUNDSET_XMAX ) pTreeLibrary->m_vMax.x = pTreeLibraryData->vMax.x;
        if( pTreeLibraryData->dwFlags & BOUNDSET_YMAX ) pTreeLibrary->m_vMax.y = pTreeLibraryData->vMax.y;
        if( pTreeLibraryData->dwFlags & BOUNDSET_ZMAX ) pTreeLibrary->m_vMax.z = pTreeLibraryData->vMax.z;
        pTreeLibrary->Slice();  // turn geometry into texture by taking slices
    }

    // Position the tree instances
    srand(123456);
#define irand(a) ((rand()*(a))>>15)
#define frand(a) ((float)rand()*(a)/32768.0f)
    m_TreeSortArray[0] = new TreeSort[ NMAXTREEINSTANCE ];
    m_TreeSortArray[1] = new TreeSort[ NMAXTREEINSTANCE ];
    m_TreeArray = new TreeData [ NMAXTREEINSTANCE ];
    D3DXVECTOR3 vTerrainCell = g_vMax - g_vMin;
    vTerrainCell.x /= NTREEX;
    vTerrainCell.z /= NTREEZ;
    for( int iTreeZ = 0; iTreeZ < NTREEZ; iTreeZ++ )
    for( int iTreeX = 0; iTreeX < NTREEX; iTreeX++ )
    {
        int iTree = iTreeZ * NTREEX + iTreeX;
        TreeData* pTree = &m_TreeArray[iTree];
        pTree->iTreeLibrary = irand(NTREELIBRARY);
        CTree* pTreeLibrary = &m_rTreeLibrary[pTree->iTreeLibrary];
        static FLOAT fCenter = 0.75f;   // radius from center of cell for semi-random tree placement
        pTree->vPosition = g_vMin + D3DXVECTOR3( (iTreeX + 0.5f  + (frand(1.0f) - 0.5f) * fCenter) * vTerrainCell.x,
                                                 0.0f,
                                                 (iTreeZ + 0.5f  + (frand(1.0f) - 0.5f) * fCenter) * vTerrainCell.z );
        float fTerrainHeight;
        D3DXVECTOR3 vTerrainNormal;
        m_pTerrain->GetTerrainPoint(pTree->vPosition, &fTerrainHeight, &vTerrainNormal);
        pTree->vPosition.y = fTerrainHeight - pTreeLibrary->m_vMin.y;
        pTree->fYRot = frand(2.f * D3DX_PI);            // Y rotation of instance
        D3DXMATRIX matRotY;
        D3DXMatrixRotationY(&matRotY, pTree->fYRot);
        D3DXMATRIX matTrans;
        D3DXMatrixTranslation(&matTrans, pTree->vPosition.x, pTree->vPosition.y, pTree->vPosition.z);
        pTree->mat = matRotY * matTrans;
        D3DXMatrixInverse(&pTree->matInv, NULL, &pTree->mat);
        pTree->vPosition.y += 0.5f * (pTreeLibrary->m_vMax.y + pTreeLibrary->m_vMin.y); // offset level-of-detail center to center of tree
    }
    // randomize tree ordering so that a subset will appear in random places
    for (int iTree = 0; iTree < NMAXTREEINSTANCE; iTree++)
    {
        TreeData t = m_TreeArray[iTree];
        int jTree = irand(NMAXTREEINSTANCE);
        m_TreeArray[iTree] = m_TreeArray[jTree];
        m_TreeArray[jTree] = t;
    }
    m_fNumTrees = NMAXTREEINSTANCE; // default number of trees to start
    if (m_fNumTrees > NMAXTREEINSTANCE)
        m_fNumTrees = NMAXTREEINSTANCE;
    m_iNumTrees[0] = m_iNumTrees[1] = 0;  // set when UpdateTrees is called

    // Set camera parameters
    m_vFrom = D3DXVECTOR3( 27.1533f, 30.f, -6.41251f);
    m_vAt = m_vFrom + D3DXVECTOR3(-0.97f, -0.1f, 0.2f);
    m_vUp = D3DXVECTOR3( 0.0f, 1.0f , 0.0f);
    D3DXMatrixPerspectiveFovLH( &m_matProjection, D3DX_PI/4, 640.f/480.f, 0.4f, 4000.0f );
    D3DXMatrixLookAtLH( &m_matView, &m_vFrom, &m_vAt, &m_vUp);
    D3DXMatrixInverse(&m_matViewInverse, NULL, &m_matView);
    D3DXMatrixIdentity(&m_matWorld);

    // Set up level-of-detail processing
    m_fBlendFactor = 0.f;
    UpdateTrees();
    UpdateTrees();  // call twice to initialize double-buffered level-of-detail values
    ShadowTrees();  // render current set of tree shadows onto terrain texture
    return hr;
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_BACK )
        g_bDrawHelp = !g_bDrawHelp;

    if (m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_WHITE])
        g_bWireFrame = !g_bWireFrame;

    if (m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_BLACK])
    {
        // advance to the next mode
        switch (g_eDebugMode) {
        case DEBUG_NONE:
            g_eDebugMode = DEBUG_PRINT;
            g_bDebugSlice = FALSE;
            g_bDebugSliceOpaque = FALSE;
            break;
        case DEBUG_PRINT:
            g_eDebugMode = DEBUG_SLICE;
            g_bDebugSlice = TRUE;
            g_bDebugSliceOpaque = FALSE;
            break;
        case DEBUG_SLICE:
            g_eDebugMode = DEBUG_SLICE_OPAQUE;
            g_bDebugSlice = TRUE;
            g_bDebugSliceOpaque = TRUE;
            break;
        case DEBUG_SLICE_OPAQUE:    
        default:
            g_eDebugMode = DEBUG_NONE;
            g_bDebugSlice = FALSE;
            g_bDebugSliceOpaque = FALSE;
            break;
        }
    }

    if (m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_X])
    {
        DWORD v = (g_bUseIntermediateLOD ? 1 : 0) | (g_bUseLowLOD ? 2 : 0);
        v++;
        g_bUseIntermediateLOD = !!(v & 1);
        g_bUseLowLOD = !!(v & 2);
    }

    if (m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_Y])
    {
        g_bDebugCulling = !g_bDebugCulling;
    }
    
    // Scale translation by height above ground plane
    float fTranslateScale = fabsf(m_vFrom.y) + 0.01f;

    // update view position
    static float fOffsetScale = 0.5f;
    float fX1 = m_DefaultGamepad.fX1;
    fX1 *= fX1 * fX1; // fX1 cubed
    float fY1 = m_DefaultGamepad.fY1;
    fY1 *= fY1 * fY1; // fY1 cubed
    D3DXVECTOR3 vOffset(fX1, 0.f, fY1); // screen space offset, X moves left-right, Y moves in-out in depth
    D3DXVec3TransformNormal(&vOffset, &vOffset, &m_matViewInverse);
    D3DXVec3Normalize(&m_vUp, &m_vUp);
    vOffset -= D3DXVec3Dot(&vOffset, &m_vUp) * m_vUp; // don't move up or down with thumb sticks
    D3DXVec3Normalize(&vOffset, &vOffset);
    vOffset *= fTranslateScale * fOffsetScale * m_fElapsedTime;
    m_vFrom += vOffset;
    m_vAt += vOffset;
    
    // move up and down with DPAD or with triggers
    D3DXVECTOR3 vVerticalOffset(0.f, 0.f, 0.f);
    static float fVerticalScaleDPAD = 1.f;
    if (m_DefaultGamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP)
        vVerticalOffset.y += fTranslateScale * fVerticalScaleDPAD * m_fElapsedTime;
    if(m_DefaultGamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
        vVerticalOffset.y -= fTranslateScale * fVerticalScaleDPAD * m_fElapsedTime;
    static float fVerticalScaleTriggers = 1.f;
    float fDelta = fTranslateScale * fVerticalScaleTriggers * (1.f/255.f) * 
        (m_DefaultGamepad.bAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER] 
         - m_DefaultGamepad.bAnalogButtons[XINPUT_GAMEPAD_LEFT_TRIGGER]);
    vVerticalOffset.y += fDelta * m_fElapsedTime;
    m_vFrom += vVerticalOffset;
    m_vAt += vVerticalOffset;

    // update view angle
    static float fAtOffsetScale = 2.f;
    D3DXVECTOR3 vAtOffset(0.f, 0.f, 0.f);
    float fX2 = m_DefaultGamepad.fX2;
    fX2 *= fX2 * fX2; // fX2 cubed
    float fY2 = m_DefaultGamepad.fY2;
    fY2 *= fY2 * fY2; // fY2 cubed
    vAtOffset.x += fAtOffsetScale * fX2 * m_fElapsedTime;
    D3DXVECTOR3 vE = m_vAt - m_vFrom;
    D3DXVec3Normalize(&vE, &vE);
    float fThreshold = 0.99f;
    float fEdotU = D3DXVec3Dot(&vE, &m_vUp);
    if ((fEdotU < -fThreshold && fY2 < 0.f) // near -vUp, but positive movement
        || (fEdotU > fThreshold && fY2 > 0.f)   // near vUp, but negative movement
        || (fEdotU > -fThreshold && fEdotU < fThreshold))       // ordinary case
        vAtOffset.y -= fAtOffsetScale * fY2 * m_fElapsedTime;   // screen-space Y displacement means up-down view turn
    D3DXVec3TransformNormal(&vAtOffset, &vAtOffset, &m_matViewInverse);
    m_vAt += vAtOffset;

    // Check to make sure we're not beneath the ground plane

    // For the projection matrix, we set up a perspective transform (which
    // transforms geometry from 3D view space to 2D viewport space, with
    // a perspective divide making objects smaller in the distance). To build
    // a perspective transform, we need the field of view (1/4 pi is common),
    // the aspect ratio, and the near and far clipping planes (which define at
    // what distances geometry should be no longer be rendered).
    D3DXMatrixPerspectiveFovLH( &m_matProjection, D3DX_PI/4, 640.f/480.f, 0.4f, 4000.0f );

    // Set up our view matrix.
    D3DXMatrixLookAtLH( &m_matView, &m_vFrom, &m_vAt, &m_vUp);
    D3DXMatrixInverse(&m_matViewInverse, NULL, &m_matView);

    // Set world matrix to identity
    D3DXMatrixIdentity(&m_matWorld);

    // change number of trees
    BYTE buttonA = m_DefaultGamepad.bAnalogButtons[XINPUT_GAMEPAD_A];
    BYTE buttonB = m_DefaultGamepad.bAnalogButtons[XINPUT_GAMEPAD_B];
    static int iThreshold = 1;
    if (buttonA >= iThreshold || buttonB >= iThreshold)
    {
        static float fMinTreesPerSecond = 0.5f;
        static float fMaxTreesPerSecond = 100.f;
        float fx = (buttonA - iThreshold) / (255.f - iThreshold);
        fx = (fx <= 0.f) ? 0.f : fMinTreesPerSecond + fMaxTreesPerSecond * fx * fx;
        float fy = (buttonB - iThreshold) / (255.f - iThreshold);
        fy = (fy <= 0.f) ? 0.f : fMinTreesPerSecond + fMaxTreesPerSecond * fy * fy;
        m_fNumTrees += m_fElapsedTime * (fx - fy);
        if (m_fNumTrees < 1) 
            m_fNumTrees = 1;
        if (m_fNumTrees > NMAXTREEINSTANCE) 
            m_fNumTrees = NMAXTREEINSTANCE;
        static float fBlendTreeFactor = 5.f;
        m_fBlendFactor += fBlendTreeFactor * m_fElapsedTime;
    }

    // smoothly go to next level-of-detail representation
    D3DXVECTOR3 vFromDelta = m_vFrom - m_vFromLevelOfDetail[0];
    float fFromDelta = D3DXVec3LengthSq(&vFromDelta);
    static float fFromDeltaThreshold = 1e-4f;
    static float fBlendTimeFactor = 1.f;
    static float fBlendTimeFactorSlow = 0.25f;

    static float fBlendHeight0 = 10.f;
    static float fBlendHeight1 = 200.f;
    float fBlendHeightScale = expf((m_vFrom.z - fBlendHeight0) * logf(3.f) / (fBlendHeight1 - fBlendHeight0));
    static float fBlendHeightScaleMax = 3.f;
    static float fBlendHeightScaleMin = 1.f;
    if (fBlendHeightScale > fBlendHeightScaleMax)
        fBlendHeightScale = fBlendHeightScaleMax;
    if (fBlendHeightScale < fBlendHeightScaleMin)
        fBlendHeightScale = fBlendHeightScaleMin;

    if (fFromDelta > fFromDeltaThreshold)
    {
        // change LODs quickly when moving
        m_fBlendFactor += fBlendHeightScale * fBlendTimeFactor * m_fElapsedTime;
    }
    else
    {
        // change LOD slowly when stopped
        m_fBlendFactor += fBlendHeightScale * fBlendTimeFactorSlow * m_fElapsedTime;
    }
    if (m_fBlendFactor > 1.f)
    {
        m_fBlendFactor = 0.f;   // triggers level-of-detail update

        // update tree level of detail and sorting order
        INT iNumTrees = m_iNumTrees[1];
        UpdateTrees();
        if( iNumTrees != m_iNumTrees[1] )
            ShadowTrees();  // render current set of trees onto terrain texture
    }
    CullTrees();        // update tree visibility
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: CompareDist2()
// Desc: Used by UpdateTrees as an ordering function to sort models from
//   near to far.
//-----------------------------------------------------------------------------
static int __cdecl CompareDist2(const void *arg1, const void *arg2 )
{
    float f1 = *(float *)arg1;
    float f2 = *(float *)arg2;
    if (f1 < f2) 
        return -1;
    else if (f1 > f2) 
        return 1;
    else
        return 0;
}




//-----------------------------------------------------------------------------
// Name: UpdateTrees()
// Desc: set level-of-detail and sort the instances by distance from the eye
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::UpdateTrees()
{
    memcpy(m_TreeSortArray[0], m_TreeSortArray[1], sizeof(TreeSort) * m_iNumTrees[1]);
    m_iNumTrees[0] = m_iNumTrees[1];
    m_iNumTrees[1] = (int)floorf(m_fNumTrees);
    m_vFromLevelOfDetail[0] = m_vFromLevelOfDetail[1];
    m_vAtLevelOfDetail[0]   = m_vAtLevelOfDetail[1];
    m_vFromLevelOfDetail[1] = m_vFrom;
    m_vAtLevelOfDetail[1]   = m_vAt;
    
    for( int iTree = 0; iTree < m_iNumTrees[1]; iTree++ )
    {
        TreeData *pTree = &m_TreeArray[iTree];
        
        // save previous LOD factors
        pTree->LOD[0] = pTree->LOD[1];

        // compute distance squared
        D3DXVECTOR3 vEye = pTree->vPosition - m_vFrom;
        m_TreeSortArray[1][iTree].fDist2 = D3DXVec3LengthSq(&vEye);
        m_TreeSortArray[1][iTree].iTree = iTree;

        // compute level of detail based on squared distance
        pTree->LOD[1].fLevelOfDetail = m_fLevelOfDetail * m_TreeSortArray[1][iTree].fDist2;
        D3DXVec3TransformCoord(&pTree->LOD[1].vFrom, &m_vFrom, &pTree->matInv );

        // TODO: move all of this level-of-detail stuff down into CTree
        static float fTreeSliceLODMin     = 6000.0f;
        static float fTreeSliceLODScale   = 1.0f/100000.0f;
        static float fBranchSliceLODMin   = 1000.0f;
        static float fBranchSliceLODScale = 1.0f/30000.0f;
        if( g_bUseLowLOD && pTree->LOD[1].fLevelOfDetail > fTreeSliceLODMin )
        {
            pTree->LOD[1].dwDrawFlags = 0;                     // draw the whole tree as slices
            pTree->LOD[1].fTreeSliceTextureLOD = (pTree->LOD[1].fLevelOfDetail - fTreeSliceLODMin) * fTreeSliceLODScale;
            pTree->bSameLevelOfDetail = (pTree->LOD[0].dwDrawFlags == 0) &&
                                        ((DWORD)(pTree->LOD[1].fTreeSliceTextureLOD) == (DWORD)(pTree->LOD[1].fTreeSliceTextureLOD));
        }
        else if( g_bUseIntermediateLOD && pTree->LOD[1].fLevelOfDetail > fBranchSliceLODMin )
        {
            pTree->LOD[1].dwDrawFlags = TREE_DRAWTRUNK;    // draw the trunk as geometry and the branches as slice textures
            pTree->LOD[1].fBranchSliceTextureLOD = (pTree->LOD[1].fLevelOfDetail - fBranchSliceLODMin) * fBranchSliceLODScale;
            pTree->bSameLevelOfDetail = (pTree->LOD[0].dwDrawFlags == TREE_DRAWTRUNK) &&
                                        ((DWORD)(pTree->LOD[1].fBranchSliceTextureLOD) == (DWORD)(pTree->LOD[1].fBranchSliceTextureLOD));
        }
        else
        {
            pTree->LOD[1].dwDrawFlags = TREE_DRAWFULLGEOMETRY;     // if close enough, draw all the geometry of the tree
            pTree->bSameLevelOfDetail = (pTree->LOD[0].dwDrawFlags == TREE_DRAWFULLGEOMETRY);
        }
    }
    qsort( (void*)m_TreeSortArray[1], m_iNumTrees[1], sizeof(TreeSort), &CompareDist2 );
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: CullTrees()
// Desc: Cull trees based on current transformation matrices
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::CullTrees()
{
    // get Blinn-style clipping matrix for bounding box culling
    D3DXMATRIX matViewProj = m_matView * m_matProjection;
    D3DXMATRIX matViewProjClip;
    BlinnClipMatrix(&matViewProjClip, &matViewProj);

    g_TREESTATS.dwActiveCount = 0;  // keep track of number of visible trees
    
    // cull trees
#if NTREELIBRARY == 1
    CTree *pTreeLibrary = &m_rTreeLibrary[0];
#endif
    int iLOD = m_iNumTrees[0] > m_iNumTrees[1] ? 0 : 1;
    for (int iTree = m_iNumTrees[iLOD] - 1; iTree >= 0; iTree--)
    {
        TreeData *pTree = &m_TreeArray[m_TreeSortArray[iLOD][iTree].iTree];
#if NTREELIBRARY > 1            
        CTree *pTreeLibrary = &m_rTreeLibrary[pTree->iTreeLibrary];
#endif
        // Cull tree if bounding box is outside of current frustum
        D3DXMATRIX matWorldViewProjClip = pTree->mat * matViewProjClip;
        pTree->bVisible = BoundingBoxInFrustum(matWorldViewProjClip, pTreeLibrary->m_vMin, pTreeLibrary->m_vMax);

        if (pTree->bVisible)
            g_TREESTATS.dwActiveCount++;

        // Transform current from vector
        D3DXVec3TransformCoord(&pTree->vFromFade, &m_vFrom, &pTree->matInv );
    }
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: ShadowTrees()
// Desc: render trees onto ground plane
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::ShadowTrees()
{
    HRESULT hr;
    LPDIRECT3DSURFACE8 pSurface = NULL;
    hr = m_pTextureTerrainShadow->GetSurfaceLevel( 0, &pSurface );
    if (FAILED(hr))
        return hr;
    hr = m_pd3dDevice->SetRenderTarget( pSurface, NULL );
    pSurface->Release();
    if (FAILED(hr))
        return hr;

    // start with all the transformations set to identity
    D3DXMATRIX matIdentity;
    D3DXMatrixIdentity(&matIdentity);
    m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &matIdentity );
    m_pd3dDevice->SetTransform( D3DTS_VIEW, &matIdentity );
    m_pd3dDevice->SetTransform( D3DTS_WORLD, &matIdentity );

    // clear to white
    struct BACKGROUNDVERTEX { D3DXVECTOR3 p; };
    BACKGROUNDVERTEX v[4];
    v[0].p = D3DXVECTOR3(-1.f,  1.f, 0.5f);
    v[1].p = D3DXVECTOR3( 1.f,  1.f, 0.5f);
    v[2].p = D3DXVECTOR3(-1.f, -1.f, 0.5f);
    v[3].p = D3DXVECTOR3( 1.f, -1.f, 0.5f);
    m_pd3dDevice->SetTexture( 0, NULL );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TFACTOR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
    m_pd3dDevice->SetRenderState( D3DRS_TEXTUREFACTOR, 0xffffffff );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE ); 
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE ); 
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE ); 
    m_pd3dDevice->SetVertexShader( D3DFVF_XYZ );
    m_pd3dDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, v, sizeof(BACKGROUNDVERTEX)); 

    // draw shadows for each tree
    D3DXMATRIX matProj;
    D3DXMatrixIdentity(&matProj);
    m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &matProj );
    D3DXVECTOR3 vSize = g_vMax - g_vMin;
    D3DXMATRIX matSize(      1.f/vSize.x,               0.f,               0.f, 0.f,
                                     0.f,               0.f,       1.f/vSize.y, 0.f,
                                     0.f,       1.f/vSize.z,               0.f, 0.f,
                       -g_vMin.x/vSize.x, -g_vMin.z/vSize.z, -g_vMin.y/vSize.y, 1.f);   // map to 0,1 range, swap Y and Z
    D3DXMATRIX mat2(  2.f,  0.f, 0.f, 0.f,
                      0.f, -2.f, 0.f, 0.f,
                      0.f,  0.f, 1.f, 0.f,
                     -1.f,  1.f, 0.f, 1.f); // map X and -Y from 0,1 to -1,1 range, leave Z alone
    D3DXMATRIX matView = matSize * mat2;
    m_pd3dDevice->SetTransform( D3DTS_VIEW, &matView );
    D3DXVECTOR3 vFrom(g_d3dLight.Direction.x, g_d3dLight.Direction.y, g_d3dLight.Direction.z);
    // This assumes we're mapping to a planar shadow receiver, and so works only 
    // when the terrain is not too hilly.
    D3DXMATRIX matShadow(1.f,  0.f,  0.f, 0.f,
                        -vFrom.x/vFrom.y,  0.f, -vFrom.z/vFrom.y, 0.f,
                         0.f,  0.f,  1.f, 0.f,
                         0.f,  0.f,  0.f, 1.f);
    UINT iLOD = 1;
    DWORD dwCubeFadeFlags = 0;
    for (int iTree = m_iNumTrees[iLOD] - 1; iTree >= 0; iTree--)
    {
        TreeData *pTree = &m_TreeArray[m_TreeSortArray[iLOD][iTree].iTree];
        D3DXMATRIX mat = pTree->mat * matShadow;
        m_pd3dDevice->SetTransform( D3DTS_WORLD, &mat);
        CTree *pTreeLibrary = &m_rTreeLibrary[pTree->iTreeLibrary];
        pTreeLibrary->BeginDrawCubeSlices();
        pTreeLibrary->SetCubeFade(vFrom, dwCubeFadeFlags);
        static float fShadowFactor = 0.25f;
        for (UINT iDir = 0; iDir < pTreeLibrary->m_nDirection; iDir++)
        {
            pTreeLibrary->m_rfFade[iDir] *= fShadowFactor;      // adjust the fade value to make the shadow less dark
            pTreeLibrary->m_rSliceTexture[iDir].SetLevelOfDetail(0.f);
        }
        pTreeLibrary->DrawCubeSlices(vFrom);
        pTreeLibrary->EndDrawCubeSlices();
    }
    return GenerateMipmaps(m_pTextureTerrainShadow, 0, D3DTADDRESS_CLAMP, D3DTADDRESS_CLAMP);
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, the call is the entry point for 3d
//       rendering. This function sets up render states, clears the
//       viewport, and renders the scene.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Render()
{
    const float fZeroBlend = 0.5f/255.f;    // blend values below this quantize to zero
    
    // reset tree stats
    g_TREESTATS.dwFullGeometryCount = 0;
    g_TREESTATS.dwBranchSliceCount = 0; 
    g_TREESTATS.dwSliceCount = 0;   
    g_dwTotalSliceCount = 0;
    
    // To make smooth level-of-detail transitions, we draw the scene in
    // passes and then blend the results.  The schedule of blending is updated in
    // FrameMove and depends on whether the camera is moving or the rendering
    // load is getting too high.  The typical blending schedule is to make a
    // complete level-of-detail transition every second.  This lets all the
    // objects in the scene transition in a smooth way.
    
    // Each object in the scene has two LODs associated with it, the current
    // and the LOD that is being transitioned to.  Once the new LOD is reached,
    // all the LOD targets are updated.
    
    // First pass: render sky, terrain, and non-LOD-changing non-transparent geometry,
    // then copy current result to temporary texture.
    // Second pass: Render previous LOD to backbuffer
    // Third pass: Render next LOD to temporary texture, then blend with backbuffer.
    for (UINT iPass = 0; iPass < 3; iPass++)
    {
        UINT iLOD = (iPass == 0) ? 0 : iPass - 1;
        if (iPass == 0 || iPass == 1)
            m_pd3dDevice->SetRenderTarget( m_pBackBuffer, m_pDepthBuffer );
        else
        {
            if (m_fBlendFactor < fZeroBlend)
                break;          // No need to render a whole image and then multiply by zero
            LPDIRECT3DSURFACE8 pBlendSurface = NULL;
            m_pBlendTexture->GetSurfaceLevel( 0, &pBlendSurface );
            m_pd3dDevice->SetRenderTarget( pBlendSurface, m_pBlendDepthBuffer );
            pBlendSurface->Release();
        }
        m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &m_matProjection );
        m_pd3dDevice->SetTransform( D3DTS_VIEW, &m_matView );
        m_pd3dDevice->SetTransform( D3DTS_WORLD, &m_matWorld);

        // If culling debug is turned on, render "from-the-side" to show the current projection frustum.
        if (g_bDebugCulling)
        {
            extern HRESULT DebugSetFrustum();   // replaces current projection matrix with offset projection
            DebugSetFrustum();      // replaces current projection matrix with offset projection
        }
        m_pd3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
        if (iPass == 0) // draw sky background
            RenderGradientBackground(D3DXCOLOR(0.3f, 0.3f, 0.4f, 1.f), D3DXCOLOR(0.45f, 0.45f, 0.9f, 1.f));
        m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
        m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE,  FALSE );
        m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, D3DZB_TRUE );
        m_pd3dDevice->SetRenderState( D3DRS_ZWRITEENABLE, TRUE );

        if (g_bWireFrame)
            m_pd3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_WIREFRAME);
        else
            m_pd3dDevice->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);

        if (iPass == 0)
        {
            // draw the ground
            m_pTerrain->DrawTerrain();
        }

        // draw our trees from back to front
#if NTREELIBRARY == 1
        CTree *pTreeLibrary = &m_rTreeLibrary[0];
        pTreeLibrary->Begin(TRUE);
#endif
        for (int iTree = m_iNumTrees[iLOD] - 1; iTree >= 0; iTree--)
        {
            TreeData *pTree = &m_TreeArray[m_TreeSortArray[iLOD][iTree].iTree];
            if (!pTree->bVisible)
                continue;

#if NTREELIBRARY > 1            
            CTree *pTreeLibrary = &m_rTreeLibrary[pTree->iTreeLibrary];
#endif
            BOOL bFullGeometryBothPasses = pTree->LOD[0].dwDrawFlags == TREE_DRAWFULLGEOMETRY &&
                                           pTree->LOD[1].dwDrawFlags == TREE_DRAWFULLGEOMETRY;
            if( iPass == 0 )
            {
                // For the pass shared by both blend buffers, draw the
                // tree only if the most-detailed geometry-only
                // version is active for both LODs
                if( !bFullGeometryBothPasses )
                    continue;
            }
            else
            {
                // don't redraw the tree, if already drawn in pass 0
                if( bFullGeometryBothPasses )
                    continue;
            }   
            
            m_pd3dDevice->SetTransform( D3DTS_WORLD, &pTree->mat );
#if NTREELIBRARY > 1            
            pTreeLibrary->Begin(TRUE);
#endif
            // Draw the current level-of-detail representation of the tree
            // TODO: Move all the LOD stuff to the tree class
            if (pTree->LOD[iLOD].dwDrawFlags == 0)
            {
                // set texture level-of-detail for whole tree
                for (UINT iDir = 0; iDir < pTreeLibrary->m_nDirection; iDir++)
                    pTreeLibrary->m_rSliceTexture[iDir].SetLevelOfDetail(pTree->LOD[iLOD].fTreeSliceTextureLOD);
                g_TREESTATS.dwSliceCount++;
            }
            else if (!(pTree->LOD[iLOD].dwDrawFlags & TREE_DRAWBRANCHES))
            {
                // set texture level-of-detail for branches
                for (UINT iDir = 0; iDir < pTreeLibrary->m_TreeBranch.m_nDirection; iDir++)
                    pTreeLibrary->m_TreeBranch.m_rSliceTexture[iDir].SetLevelOfDetail(pTree->LOD[iLOD].fBranchSliceTextureLOD);
                g_TREESTATS.dwBranchSliceCount++;
            }
            else
                g_TREESTATS.dwFullGeometryCount++;
            pTreeLibrary->DrawLOD(pTree->vFromFade, pTree->LOD[iLOD].vFrom, pTree->LOD[iLOD].dwDrawFlags);
#if NTREELIBRARY > 1            
            pTreeLibrary->End();
#endif          
        }
#if NTREELIBRARY == 1
        pTreeLibrary->End();
#endif

        m_pd3dDevice->SetTransform( D3DTS_WORLD, &m_matWorld ); // restore world transform

        if( g_bDebugCulling )
        {
            extern HRESULT DebugDrawFrustum();
            DebugDrawFrustum();
        }

        if( iPass == 0 && m_fBlendFactor >= fZeroBlend )
        {
            // Copy backbuffer to temporary texture, and depth buffer to temporary depth buffer
            LPDIRECT3DSURFACE8 pBlendSurface = NULL;
            m_pBlendTexture->GetSurfaceLevel( 0, &pBlendSurface );
            m_pd3dDevice->CopyRects( m_pBackBuffer, NULL, 1, pBlendSurface, NULL );
            pBlendSurface->Release();
            m_pd3dDevice->CopyRects( m_pDepthBuffer, NULL, 1, m_pBlendDepthBuffer, NULL );
        }
        else if( iPass == 2) 
        {
            // Add result to frame buffer, blending in smoothly according to the blend factor
            m_pd3dDevice->SetRenderTarget( m_pBackBuffer, NULL );
            D3DXCOLOR colorBlend(m_fBlendFactor, m_fBlendFactor, m_fBlendFactor, m_fBlendFactor);
            BlendScreenTexture(m_pBlendTexture, colorBlend);
        }
    }

    // show game title or help
    if( g_bDrawHelp )
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 48, 36, 0xffffffff,  L"Trees" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 592, 38, 0xffffff00, m_strFrameRate, XBFONT_RIGHT );
        
        WCHAR buf[100];
        FLOAT x = 64.0f, y = 55.0f, dy = 25.0f;

        m_Font.DrawText( x, y += dy, 0xffffffff, L"Mode: " );
        if( g_bDebugSlice && g_bDebugSliceOpaque )
            m_Font.DrawText( 0xffffff00, L"Debug opaque" );
        else if( g_bDebugSlice )
            m_Font.DrawText( 0xffffff00, L"Debug" );
        else
            m_Font.DrawText( 0xffffff00, L"Normal" );

        m_Font.DrawText( x, y += dy, 0xffffffff, L"Intermediate LOD: " );
        m_Font.DrawText( 0xffffff00, g_bUseIntermediateLOD ? L"On" : L"Off" );

        m_Font.DrawText( x, y += dy, 0xffffffff, L"Low LOD: " );
        m_Font.DrawText( 0xffffff00, g_bUseLowLOD ? L"On" : L"Off" );

        swprintf( buf, L"%d", (int)floorf(m_fNumTrees) );
        m_Font.DrawText( x, y += dy, 0xffffffff, L"Tree count: " );
        m_Font.DrawText( 0xffffff00, buf );

        if( g_eDebugMode != DEBUG_NONE )
        {
            m_Font.DrawText( x, y += dy, 0xffffffff, L"Culling display: " );
            m_Font.DrawText( 0xffffff00, g_bDebugCulling ? L"On" : L"Off" );

            swprintf( buf, L"%d", g_TREESTATS.dwActiveCount );
            m_Font.DrawText(x, y += dy, 0xffffffff, L"Active trees: " );
            m_Font.DrawText( 0xffffff00, buf );
            
            swprintf( buf, L"%d", g_TREESTATS.dwFullGeometryCount );
            m_Font.DrawText(x, y += dy, 0xffffffff, L"Full geometry trees: " );
            m_Font.DrawText( 0xffffff00, buf );
            
            swprintf( buf, L"%d%s", g_TREESTATS.dwBranchSliceCount, g_bUseIntermediateLOD ? L" on" : L" off" );
            m_Font.DrawText(x, y += dy, 0xffffffff, L"Branch slice trees: " );
            m_Font.DrawText( 0xffffff00, buf );
            
            swprintf( buf, L"%d%s", g_TREESTATS.dwSliceCount, g_bUseLowLOD ? L" on" : L" off" );
            m_Font.DrawText(x, y += dy, 0xffffffff, L"Slice trees: " );
            m_Font.DrawText( 0xffffff00, buf );
        }

        m_Font.End();
    }
    
    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: BlendScreenTexture()
// Desc: This function renders the level-of-detail blend texture to
//       the screen with a specified blending factor.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::BlendScreenTexture( LPDIRECT3DTEXTURE8 pTexture, D3DCOLOR colorBlend )
{
    // Texture coordinates in linear format textures go from 0 to n-1 rather
    // than the 0 to 1 that is used for swizzled textures.
    D3DSURFACE_DESC desc;
    pTexture->GetLevelDesc(0, &desc);
    struct BACKGROUNDVERTEX { D3DXVECTOR4 p; FLOAT tu, tv; } v[4];
    v[0].p = D3DXVECTOR4( -0.5f,             -0.5f,              1.0f, 1.0f ); v[0].tu = 0.0f;              v[0].tv = 0.0f;
    v[1].p = D3DXVECTOR4( desc.Width - 0.5f, -0.5f,              1.0f, 1.0f ); v[1].tu = (float)desc.Width; v[1].tv = 0.0f;
    v[2].p = D3DXVECTOR4( -0.5f,             desc.Height - 0.5f, 1.0f, 1.0f ); v[2].tu = 0.0f;              v[2].tv = (float)desc.Height;
    v[3].p = D3DXVECTOR4( desc.Width - 0.5f, desc.Height - 0.5f, 1.0f, 1.0f ); v[3].tu = (float)desc.Width; v[3].tv = (float)desc.Height;
    
    // Set states
    m_pd3dDevice->SetPixelShader( NULL );
    m_pd3dDevice->SetTexture( 0, pTexture );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TFACTOR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU,  D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV,  D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetRenderState( D3DRS_TEXTUREFACTOR, colorBlend );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE ); 
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHAREF, 0 );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATER );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID);

    // Render the screen-aligned quadrilateral
    m_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW|D3DFVF_TEX1 );
    m_pd3dDevice->DrawPrimitiveUP( D3DPT_QUADSTRIP, 1, v, sizeof(BACKGROUNDVERTEX) );

    // Reset render states
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetTexture( 0, NULL );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP );
    
    return S_OK;
}
