//-----------------------------------------------------------------------------
// File: FocusBlur.cpp
//
// Desc: Two pass focus blur effect.  First, use the backbuffer as a texture
//       and draw to a separate blur texture with a blur pixel shader. For
//       computing the blurred back buffer, this sample shows several different
//       filters of varying visual quality and performance. Second, use the
//       depthbuffer as a texture to choose a range of z values to show in
//       sharp focus vs blurry focus.
//
// Copyright (c) 2001-2002 Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <XBUtil.h>
#include "Resource.h" // Bundled resources




//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_LEFTSTICK,    XBHELP_PLACEMENT_1, L"Move camera" },
    { XBHELP_RIGHTSTICK,   XBHELP_PLACEMENT_1, L"Rotate camera" },
    { XBHELP_BACK_BUTTON,  XBHELP_PLACEMENT_1, L"Help" },
    { XBHELP_A_BUTTON,     XBHELP_PLACEMENT_2, L"Next\nFilter" },
    { XBHELP_B_BUTTON,     XBHELP_PLACEMENT_2, L"Previous\nFilter" },
    { XBHELP_X_BUTTON,     XBHELP_PLACEMENT_2, L"Toggle\nEffect" },
    { XBHELP_Y_BUTTON,     XBHELP_PLACEMENT_2, L"Toggle\nDepth Mode" },
    { XBHELP_WHITE_BUTTON, XBHELP_PLACEMENT_2, L"Show Blur\nTexture" },
    { XBHELP_BLACK_BUTTON, XBHELP_PLACEMENT_2, L"Show Depth\nBuffer" },
    { XBHELP_LEFT_BUTTON,  XBHELP_PLACEMENT_1, L"Inc. Focus" },
    { XBHELP_RIGHT_BUTTON, XBHELP_PLACEMENT_1, L"Dec. Focus" },
};

#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts)/sizeof(XBHELP_CALLOUT))




//-----------------------------------------------------------------------------
// Vertex type for the cylinder objects in the simple scene.
//-----------------------------------------------------------------------------
struct CUSTOMVERTEX
{
    D3DXVECTOR3 position;   // The position
    D3DXVECTOR3 normal;     // The vertex normals
    float tu, tv;           // texture coords
};

#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_TEX1)




//-----------------------------------------------------------------------------
// A filter sample holds a subpixel offset and a filter value
// to be multiplied by a source texture to compute an arbitrary
// filter.  See FilterCopy for more details.
//-----------------------------------------------------------------------------
struct FilterSample 
{
    FLOAT fValue;               // Coefficient
    FLOAT fOffsetX, fOffsetY;   // Subpixel offsets of supersamples in destination coordinates
};




//-----------------------------------------------------------------------------
// The depth-mapping pixel shader attempts to do higher precision
// math with eight-bit color registers.  The _x4 instruction modifier
// is used twice to get a 16x range of values.
//-----------------------------------------------------------------------------
FLOAT g_fPixelShaderScale = 16.0f;   // to get into the right range, we scale up the value in the pixel shader




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    CXBPackedResource  m_xprResource;        // Packed resources for the app
    CXBFont      m_Font;
    CXBHelp      m_Help;
    BOOL         m_bDrawHelp;

    DWORD        m_dwBackBufferWidth;        // Details about the backbuffer
    DWORD        m_dwBackBufferHeight;
    D3DFORMAT    m_dwBackBufferFormat;
    D3DTexture   m_BackBufferTexture;

    // To demonstrate the focus blur effect, this sample draws a simple
    // scene with texture-mapped cylinders.
    struct Object 
    {                         
        D3DXMATRIX   m_matWorld;
        D3DMATERIAL8 m_material;
        D3DTexture*  m_pTexture;
    };
    Object*          m_pObjects;             // Put a few objects around in the scene
    DWORD            m_dwObjectCount;        // Number of objects
    D3DVertexBuffer* m_pObjectVB;            // Buffer for cylinder geometry

    #define OBJECT_TEXTURE_COUNT 3           // Textures for the objects
    D3DTexture* m_pObjectTextures[OBJECT_TEXTURE_COUNT];
    
    // Current viewing parameters
    D3DXVECTOR3     m_vFrom, m_vAt, m_vUp;  // Viewing parameters
    D3DXMATRIX      m_matWorld;
    D3DXMATRIX      m_matView;
    D3DXMATRIX      m_matViewInverse;
    D3DXMATRIX      m_matProjection;
    
    // Texture space for the multipass blur filters. These textures are of 
    // decreasing size and are used for blurring the backbuffer
    #define BLUR_TEXTURE_COUNT 5
    D3DTexture* m_pBlurTextures[BLUR_TEXTURE_COUNT]; 
    D3DTexture* m_pBlur;     // Current blur texture, set by Blur function

    // Techniques for mapping the z-buffer to the in-focus range
    enum DEPTHMODE 
    {
        DM_RAW,         // Show the raw z-buffer for demonstration purposes
        DM_RANGE,       // Use arithmetic in the pixel shader to map z 
        DM_LOOKUP,      // Use a dependent-texture read to map z
        _DM_MAX
    } m_eDepthMode;
    
    // Overall modes for the sample.
    enum EFFECTMODE 
    {
        EM_NOEFFECT,              // Draw the basic scene, with no focus effect
        EM_DEBUG_SHOWBLURTEXTURE, // Show the blurred back-buffer texture
        EM_DEBUG_SHOWDEPTHBUFFER, // Show the mapping of z-values to in-focus areas
        EM_SHOWFOCUSEFFECT,       // The full focus effect
        _EM_MAX 
    } m_eEffectMode;
    
    // Enumeration of blur filters available in this sample to compare
    // the speed and quality of different types of blur filters for
    // the out-of-focus parts of the scene.
    enum FILTERMODE 
    {
        FM_VERT2_HORIZ2,
        FM_HORIZ2_VERT2,
        FM_VERT2_HORIZ,
        FM_HORIZ2_VERT,
        FM_IDENTITY,
        FM_BOX,
        FM_VERT,
        FM_HORIZ, 
        FM_BOX2,
        FM_VERT2,
        FM_HORIZ2,
        FM_BOX2_BOX2,
        FM_VERT2_HORIZ2_BOX2,
        FM_BOX2_BOX2_BOX2,
        FM_VERT2_HORIZ2_VERT2,
        FM_HORIZ2_VERT2_HORIZ2,
        FM_VERT2_HORIZ2_VERT2_HORIZ2,
        FM_HORIZ2_VERT2_HORIZ2_VERT2,
        FM_BOX2_BOX2_BOX2_BOX2,
        _FM_MAX
    } m_eFilterMode;    // Keep two indices to visually compare the blur filters
    
    WCHAR* m_strFilterName;            // Name of filter
    WCHAR* m_strFilterDescription;     // Description of filter

    // Constants to choose the focus range. The lookup texture maps z
    // to focus values when DM_LOOKUP is active (for
    // FocusLookupPixelShader or DepthLookupPixelShader).
    FLOAT m_fDepth0,    m_fDepth1;      // Range of depths to map
    FLOAT m_fFraction0, m_fFraction1;   // Fractions of (at-from) vector to use for setting focus depths
    D3DTexture* m_pTextureFocusRange;   // Lookup table for range of z values to use

    // Pixel shader handles
    DWORD m_dwBlurPixelShader;        // Blur the back-buffer
    DWORD m_dwDepthPixelShader;       // Use pixel shader arithmetic to map z to focus value
    DWORD m_dwDepthLookupPixelShader; // Use a lookup texture to map z to the focus value
    DWORD m_dwFocusPixelShader;       // Blend in-focus back-buffer w/blur texture based on DepthPixelShader focus value
    DWORD m_dwFocusLookupPixelShader; // Blend in-focus back-buffer w/blur texture based on DepthLookupPixelShader focus value

    // Main filtering routine that draws the source texture multiple
    // times, with sub-pixel offsets and filter coefficients.
    HRESULT FilterCopy( LPDIRECT3DTEXTURE8 pTextureDst, LPDIRECT3DTEXTURE8 pTextureSrc,
                        DWORD dwNumSamples, FilterSample rSample[],
                        DWORD dwSuperSampleX, DWORD dwSuperSampleY );
    
    // Blur backbuffer and set m_pBlur.  Calls FilterCopy with
    // different filter coefficients and offsets, based on the current
    // FILTERMODE setting.
    HRESULT Blur();
    
    // The z-values from the depth buffer are mapped to a focus range
    // based on the current range of depths m_fDepth0 and m_fDepth1,
    // which are set based on a fraction (m_fFraction0, m_fFraction1)
    // of the distance of the near and far z-clip planes.
    HRESULT FillFocusRangeTexture( bool bRamp );    // Fill texture using current focus mapping 
    HRESULT CalculateFocusDepths();                 // Use fractions along viewing vector to set focus depths
    HRESULT CalculateDepth( float* pfDepth, const D3DXVECTOR3 &vPosition ); // Calculate the depth value of the 3D point
    HRESULT CalculateDepthMapping( float fDepth0, float fDepth1,            // Compute constants for pixel shader arithmetic
                                   float* pfAlphaOffset, float* pfAlphaSlope, 
                                   float* pfBlueOffset, float* pfBlueSlope );
    
    // For demonstrating the inputs to the full focus effect, the sample
    // can draw just the blurred texture or the z-buffer as a texture.
    HRESULT DebugDrawBlur();            // Draw blurred texture
    HRESULT DebugDrawDepthRaw();        // Draw raw z values as rgb
    HRESULT DebugDrawDepthRange();      // Map z to focus values using pixel shader arithmetic
    HRESULT DebugDrawDepthLookup();     // Map z to focus values using a lookup texture
    
    // Full focus blur effect, using either of the two z-to-focus mapping
    // techniques.
    HRESULT DrawFocusEffectUsingRange();  // Use pixel shader arithmetic for mapping z to focus value
    HRESULT DrawFocusEffectUsingLookup(); // Map z through lookup texture to focus value

    // Set current transformation matrices based on current view
    // position and orientation.
    HRESULT SetCameraTransformations();

public:

    // Overrides of XbApp framework functions
    HRESULT Initialize();
    HRESULT Render();
    HRESULT FrameMove();

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
// Desc: Constructor
//-----------------------------------------------------------------------------
CXBoxSample::CXBoxSample()
            :CXBApplication()
{
    m_d3dpp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;    // Allow unlimited frame rate

    m_bDrawHelp     = false;
    m_eDepthMode    = DM_RANGE;
    m_eEffectMode   = EM_SHOWFOCUSEFFECT;
    m_eFilterMode   = (FILTERMODE)0;
    m_strFilterName = NULL;
    m_strFilterDescription = NULL;
    ZeroMemory( m_pObjectTextures, sizeof(m_pObjectTextures) );
    m_pObjectVB      = NULL;
    ZeroMemory( m_pBlurTextures, sizeof(m_pBlurTextures) );
    m_pBlur = NULL;
    m_pTextureFocusRange = NULL;
    m_dwBlurPixelShader        = 0;
    m_dwDepthPixelShader       = 0;
    m_dwDepthLookupPixelShader = 0;
    m_dwFocusPixelShader       = 0;
    m_dwFocusLookupPixelShader = 0;
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Initialize scene objects.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    // Create the font
    if( FAILED( m_Font.Create( "Font.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create the help
    if( FAILED( m_Help.Create( "Gamepad.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create the resources
    if( FAILED( m_xprResource.Create( "Resource.xpr", resource_NUM_RESOURCES ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Load the textures
    m_pObjectTextures[0] = m_xprResource.GetTexture( resource_Texture0_OFFSET );
    m_pObjectTextures[1] = m_xprResource.GetTexture( resource_Texture1_OFFSET );
    m_pObjectTextures[2] = m_xprResource.GetTexture( resource_Checker_OFFSET );
    
    // Get size of render target
    D3DSURFACE_DESC desc;
    m_pBackBuffer->GetDesc( &desc );
    m_dwBackBufferWidth  = desc.Width;
    m_dwBackBufferHeight = desc.Height;
    m_dwBackBufferFormat = desc.Format;

    // Make D3DTexture wrapper for the backbuffer
    ZeroMemory( &m_BackBufferTexture, sizeof(D3DTexture) );
    XGSetTextureHeader( m_dwBackBufferWidth, m_dwBackBufferHeight, 1, 0, 
                        m_dwBackBufferFormat, 0, &m_BackBufferTexture, 
                        m_pBackBuffer->Data, 
                        m_dwBackBufferWidth * XGBytesPerPixelFromFormat(m_dwBackBufferFormat) );

    // Create the blur textures
    for( DWORD i = 0; i < BLUR_TEXTURE_COUNT; i++ )
    {
        // Make the size a factor of 2 smaller each time
        m_pd3dDevice->CreateTexture( m_dwBackBufferWidth >> i, m_dwBackBufferHeight >> i, 1, 
                                     D3DUSAGE_RENDERTARGET, m_dwBackBufferFormat, 0, 
                                     &m_pBlurTextures[i] );
    }

    // Create the pixel shaders
#pragma warning( push )
#pragma warning( disable : 4245 )   // ignore conversion of int to DWORD
    {
#include "blur.inl"
        m_pd3dDevice->CreatePixelShader( &psd, &m_dwBlurPixelShader );
    }
    {
#include "depth.inl"
        m_pd3dDevice->CreatePixelShader( &psd, &m_dwDepthPixelShader );
    }
    {
#include "depthlookup.inl"
        m_pd3dDevice->CreatePixelShader( &psd, &m_dwDepthLookupPixelShader );
    }
    {
#include "focus.inl"
        m_pd3dDevice->CreatePixelShader( &psd, &m_dwFocusPixelShader );
    }
    {
#include "focuslookup.inl"
        m_pd3dDevice->CreatePixelShader( &psd, &m_dwFocusLookupPixelShader );
    }
#pragma warning( pop )
    
    // Create geometry for a cylinder
#define NSAMPLE 25
    if( FAILED( m_pd3dDevice->CreateVertexBuffer( NSAMPLE*2*sizeof(CUSTOMVERTEX),
                                                  0, 0, 0, &m_pObjectVB ) ) )
        return E_FAIL;
    // Fill the vertex buffer. We are setting the tu and tv texture
    // coordinates, which range from 0.0 to 1.0
    CUSTOMVERTEX* v;
    if( FAILED( m_pObjectVB->Lock( 0, 0, (BYTE**)&v, 0 ) ) )
        return E_FAIL;
    for( DWORD i=0; i<NSAMPLE; i++ )
    {
        FLOAT theta = (2*D3DX_PI*i)/(NSAMPLE-1);

        v[2*i+0].position = D3DXVECTOR3( sinf(theta),-1.2f, cosf(theta) );
        v[2*i+0].normal   = D3DXVECTOR3( sinf(theta), 0.0f, cosf(theta) );
        v[2*i+0].tu = ((FLOAT)i*2.0f)/(NSAMPLE-1);
        v[2*i+0].tv = 1.0f;
        
        v[2*i+1].position = D3DXVECTOR3( sinf(theta), 1.2f, cosf(theta) );
        v[2*i+1].normal   = D3DXVECTOR3( sinf(theta), 0.0f, cosf(theta) );
        v[2*i+1].tu = ((FLOAT)i*2.0f)/(NSAMPLE-1);
        v[2*i+1].tv = 0.0f;
    }
    m_pObjectVB->Unlock();

    // Position the objects
    m_dwObjectCount = 100;
    m_pObjects = new Object[m_dwObjectCount];
    if( m_pObjects == NULL )
        return E_OUTOFMEMORY;

#define irand(a) ((rand()*(a))>>15)
#define frand(a) ((float)rand()*(a)/32768.0f)

    float fScale = 100.0f;
    srand( 123456 );
    for( DWORD i = 0; i < m_dwObjectCount; i++ )
    {
        D3DXMatrixTranslation( &m_pObjects[i].m_matWorld, frand(fScale), 0.0f, frand(fScale)  );
        XBUtil_InitMaterial( m_pObjects[i].m_material, frand(1.0f), frand(1.0f), frand(1.0f), 1.0f );
        m_pObjects[i].m_pTexture = m_pObjectTextures[irand(OBJECT_TEXTURE_COUNT)];
    }
    
    // Set camera parameters and initialize camera matrices
    m_vAt.x = m_pObjects[0].m_matWorld._41;
    m_vAt.y = m_pObjects[0].m_matWorld._42;
    m_vAt.z = m_pObjects[0].m_matWorld._43;
    m_vFrom = m_vAt - D3DXVECTOR3(8.0f, 0.0f, 0.0f);
    m_vUp = D3DXVECTOR3( 0.0f, 1.0f , 0.0f);
    SetCameraTransformations();

    // Set focus range to be around the object we're looking at
    m_fFraction0 = 0.9f;
    m_fFraction1 = 1.1f;
    CalculateFocusDepths();

    // Create and fill the focus range texture
    FillFocusRangeTexture( false );

    // Setup the light
    D3DLIGHT8 light;
    ZeroMemory( &light, sizeof(D3DLIGHT8) );
    light.Type         = D3DLIGHT_DIRECTIONAL;
    light.Ambient      = D3DXCOLOR(0.3f, 0.3f, 0.3f, 1.0f);
    light.Diffuse      = D3DXCOLOR(1.0f, 1.0f, 1.0f, 1.0f);
    light.Direction    = D3DXVECTOR3( 1.0f, 1.0f, 1.0f );
    light.Range        = 1000.0f;
    light.Attenuation0 = 1.0f;
    m_pd3dDevice->SetLight( 0, &light );
    m_pd3dDevice->LightEnable( 0, TRUE );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: SetCameraTransformations()
// Desc: Calculate camera matrices and set transformation state
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::SetCameraTransformations()
{
    // Set world matrix to identity
    D3DXMatrixIdentity( &m_matWorld );
    m_pd3dDevice->SetTransform( D3DTS_WORLD, &m_matWorld );
    
    // Set our view matrix.
    D3DXMatrixLookAtLH( &m_matView, &m_vFrom, &m_vAt, &m_vUp );
    D3DXMatrixInverse( &m_matViewInverse, NULL, &m_matView );
    m_pd3dDevice->SetTransform( D3DTS_VIEW, &m_matView );

    // Set projection 
    D3DXMatrixPerspectiveFovLH( &m_matProjection, D3DX_PI/4, 640.0f / 480.0f, 1.0f, 50.0f );
    m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &m_matProjection );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    bool bUpdateDepth = false;
    
    // Toggle help
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Toggle focus effect
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_X] )
    {
        if( m_eEffectMode != EM_NOEFFECT )
            m_eEffectMode = EM_NOEFFECT;
        else
            m_eEffectMode = EM_SHOWFOCUSEFFECT;
    }

    // Toggle display of blur texture
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_WHITE] )
    {
        if( m_eEffectMode != EM_DEBUG_SHOWBLURTEXTURE )
            m_eEffectMode = EM_DEBUG_SHOWBLURTEXTURE;
        else
            m_eEffectMode = EM_SHOWFOCUSEFFECT;
    }

    // Toggle display of depthbuffer texture
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_BLACK] )
    {
        if( m_eEffectMode != EM_DEBUG_SHOWDEPTHBUFFER )
            m_eEffectMode = EM_DEBUG_SHOWDEPTHBUFFER;
        else
            m_eEffectMode = EM_SHOWFOCUSEFFECT;

        // Is this needed here?
        bUpdateDepth = true;
    }

    // Change depth mode only in the focus effect mode and the debug show depth texture mode
    if( m_eEffectMode == EM_DEBUG_SHOWDEPTHBUFFER || m_eEffectMode == EM_SHOWFOCUSEFFECT )
    {
        if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_Y] )
        {
            int iDepthMode = (int)m_eDepthMode + 1;
            if( iDepthMode >= (int)_DM_MAX )
                iDepthMode = 0;
            m_eDepthMode = (DEPTHMODE)iDepthMode;
            bUpdateDepth = true;
        }
    }

    // Change depth mode only in the focus effect mode and the debug show blur texture mode
    if( m_eEffectMode == EM_DEBUG_SHOWBLURTEXTURE || m_eEffectMode == EM_SHOWFOCUSEFFECT )
    {
        // Go to next filter mode
        if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
        {
            int iFilterMode = (int)m_eFilterMode + 1;
            if( iFilterMode >= (int)_FM_MAX )
                iFilterMode = 0;
            m_eFilterMode = (FILTERMODE)iFilterMode;
        }

        // Go to previous filter mode
        if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_B] )
        {
            int iFilterMode = (int)m_eFilterMode - 1;
            if( iFilterMode < 0 )
                iFilterMode = (int)_FM_MAX - 1;
            m_eFilterMode = (FILTERMODE)iFilterMode;
        }

        // Name and description is set by Blur()
        m_strFilterName        = NULL;
        m_strFilterDescription = NULL;
    }

    // update view position
    static float fOffsetScale = 3.0f;
    float fX1 = m_DefaultGamepad.fX1;
    fX1 *= fX1 * fX1; // fX1 cubed
    float fY1 = m_DefaultGamepad.fY1;
    fY1 *= fY1 * fY1; // fY1 cubed
    D3DXVECTOR3 vOffset(fX1, 0.0f, fY1); // screen space offset, X moves left-right, Y moves in-out in depth
    D3DXVec3TransformNormal( &vOffset, &vOffset, &m_matViewInverse );
    D3DXVec3Normalize( &m_vUp, &m_vUp );
    vOffset -= D3DXVec3Dot( &vOffset, &m_vUp ) * m_vUp; // don't move up or down with thumb sticks
    D3DXVec3Normalize( &vOffset, &vOffset );
    vOffset *= fOffsetScale * m_fElapsedTime;
    m_vFrom += vOffset;
    m_vAt += vOffset;

    // update view angle
    static float fAtOffsetScale = 8.0f;
    D3DXVECTOR3 vAtOffset( 0.0f, 0.0f, 0.0f );
    float fX2 = m_DefaultGamepad.fX2;
    fX2 *= fX2 * fX2; // fX2 cubed
    float fY2 = m_DefaultGamepad.fY2;
    fY2 *= fY2 * fY2; // fY2 cubed
    vAtOffset.x += fAtOffsetScale * fX2 * m_fElapsedTime;
    D3DXVECTOR3 vE = m_vAt - m_vFrom;
    D3DXVec3Normalize(&vE, &vE);
    float fThreshold = 0.99f;
    float fEdotU = D3DXVec3Dot(&vE, &m_vUp);
    if( ( fEdotU < -fThreshold && fY2 < 0.0f ) || // near -vUp, but positive movement
        ( fEdotU > fThreshold && fY2 > 0.0f ) ||   // near vUp, but negative movement
        ( fEdotU > -fThreshold && fEdotU < fThreshold ) )       // ordinary case
        vAtOffset.y -= fAtOffsetScale * fY2 * m_fElapsedTime;   // screen-space Y displacement means up-down view turn
    D3DXVec3TransformNormal( &vAtOffset, &vAtOffset, &m_matViewInverse );
    m_vAt += vAtOffset;

    // Set focus depths
    int delta = m_DefaultGamepad.bAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER] -
                m_DefaultGamepad.bAnalogButtons[XINPUT_GAMEPAD_LEFT_TRIGGER];
    if( delta )
    {
        static float fDeltaScale = 0.001f;
        float fScale = 1.0f + m_fElapsedTime * delta * fDeltaScale;
        m_fFraction0 *= fScale;
        m_fFraction1 *= fScale;
        if( m_fFraction0 <  0.1f ) m_fFraction0 =  0.1f;
        if( m_fFraction0 > 10.0f ) m_fFraction0 = 10.0f;
        if( m_fFraction1 <  0.1f ) m_fFraction1 =  0.1f;
        if( m_fFraction1 > 10.0f ) m_fFraction1 = 10.0f;
        bUpdateDepth = true;
    }

    if( bUpdateDepth )
    {
        if( m_eDepthMode == DM_RAW && m_eEffectMode == EM_SHOWFOCUSEFFECT )
        {
            // Skip raw depths when in focus effect mode
            m_eDepthMode = DM_RANGE;
        }

        // Set focus depths from m_vFrom, m_vAt, m_fFraction0, and m_fFraction1
        CalculateFocusDepths();

        // Fill the lookup texture, if needed
        if( m_eDepthMode == DM_LOOKUP )
            FillFocusRangeTexture( false );
    }

    SetCameraTransformations();

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
    // Draw a gradient filled background
    RenderGradientBackground( 0xff0000ff, 0xff000000 );

    // Set default state
    m_pd3dDevice->SetTexture( 0, NULL );
    m_pd3dDevice->SetTexture( 1, NULL );
    m_pd3dDevice->SetTexture( 2, NULL );
    m_pd3dDevice->SetTexture( 3, NULL );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MIPFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU,  D3DTADDRESS_WRAP );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV,  D3DTADDRESS_WRAP );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    m_pd3dDevice->SetRenderState( D3DRS_LIGHTING,        TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE,         TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_MATERIAL );
    m_pd3dDevice->SetRenderState( D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL );

    // Render some geometry
    m_pd3dDevice->SetStreamSource( 0, m_pObjectVB, sizeof(CUSTOMVERTEX) );
    m_pd3dDevice->SetVertexShader( D3DFVF_CUSTOMVERTEX );
    
    for( DWORD i = 0; i < m_dwObjectCount; i++ )
    {
        m_pd3dDevice->SetTransform( D3DTS_WORLD, &m_pObjects[i].m_matWorld );
        m_pd3dDevice->SetMaterial( &m_pObjects[i].m_material );
        m_pd3dDevice->SetTexture( 0, m_pObjects[i].m_pTexture );
       
        m_pd3dDevice->DrawPrimitive( D3DPT_TRIANGLESTRIP, 0, 2*NSAMPLE-2 );
    }

    // Draw the current effect
    switch( m_eEffectMode )
    {
        case EM_NOEFFECT:
            break;

        case EM_DEBUG_SHOWBLURTEXTURE:
            Blur();                 // blur back buffer
            DebugDrawBlur();             // display blurred texture
            break;
        
        case EM_DEBUG_SHOWDEPTHBUFFER:
            switch( m_eDepthMode )
            {
                case DM_RAW:
                    DebugDrawDepthRaw();         // raw depths
                    break;
                case DM_RANGE:
                    DebugDrawDepthRange();       // depths mapped with range in pixel shader
                    break;
                case DM_LOOKUP:
                    DebugDrawDepthLookup();      // depths mapped through lookup table texture
                    break;
            }
            break;

        case EM_SHOWFOCUSEFFECT:
            Blur();                 // blur back buffer
            switch( m_eDepthMode )
            {
                case DM_RAW:    // this should not happen, but just in case, fall through to range depth mapping
                case DM_RANGE:
                    DrawFocusEffectUsingRange();       // blur the backbuffer into a texture, then use current depth range to choose between sharp or blurred focus
                    break;

                case DM_LOOKUP:
                    DrawFocusEffectUsingLookup();      // blur the backbuffer into a texture, then use current depth mapped through a lookup table to choose between sharp or blurred focus
                    break;
            }
            break;
    }

    m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID );
    m_pd3dDevice->SetRenderState( D3DRS_LIGHTING, FALSE );

    // Show title, frame rate, and help
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        static WCHAR* strDepthMode[] = 
        {
            L"Raw Depth",
            L"Depth Range",
            L"Depth Lookup",
        };
        static WCHAR* strDepthModeDescription[] = 
        {
            L"Z values are draw in false color.",
            
            L"Z values are mapped to focus value with\n"
            L"pixel shader arithmetic. Use triggers to change\n"
            L"focus range.",
            
            L"Z values are mapped to focus values using\n"
            L"a lookup texture. Use triggers to change\n"
            L"focus range.",
        };
        WCHAR str[200];
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 48, 36, 0xffffffff,  L"FocusBlur" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 592, 38, 0xffffff00, m_strFrameRate, XBFONT_RIGHT );
        
        if( m_eEffectMode == EM_NOEFFECT )
        {
            m_Font.DrawText(  64,  90, 0xffffffff, L"Mode:" );
            m_Font.DrawText( 200,  90, 0xffffff00, L"(No effect)" );
        }
        else if( m_eEffectMode == EM_SHOWFOCUSEFFECT )
        {
            m_Font.DrawText(  64,  90, 0xffffffff, L"Mode:" );
            m_Font.DrawText( 200,  90, 0xffffff00, L"Focus effect" );
            m_Font.DrawText(  64, 115, 0xffffffff, L"Depth mode:" );
            m_Font.DrawText( 200, 115, 0xffffff00, strDepthMode[m_eDepthMode] );
            m_Font.DrawText(  64, 140, 0xffffffff, L"Filter:" );
            m_Font.DrawText( 200, 140, 0xffffff00, m_strFilterName );
        }
        else if( m_eEffectMode == EM_DEBUG_SHOWBLURTEXTURE )
        {
            m_Font.DrawText(  64,  90, 0xffffffff, L"Mode:" );
            m_Font.DrawText( 200,  90, 0xffffff00, L"(Debug) Show blur texture" );
            m_Font.DrawText(  64, 140, 0xffffffff, L"Filter:" );
            m_Font.DrawText( 200, 140, 0xffffff00, m_strFilterName );
            m_Font.DrawText(  64, 300, 0xffffffff, L"Filter description:" );
            m_Font.DrawText(  64, 325, 0xffaaaa10, m_strFilterDescription );
            m_Font.DrawText(  64, 400, 0xffffffff, L"Use " GLYPH_A_BUTTON L" and " GLYPH_B_BUTTON L" to choose blur filter." );
        }
        else if( m_eEffectMode == EM_DEBUG_SHOWDEPTHBUFFER )
        {
            m_Font.DrawText(  64,  90, 0xffffffff, L"Mode:" );
            m_Font.DrawText( 200,  90, 0xffffff00, L"(Debug) Show depth buffer" );
            m_Font.DrawText(  64, 115, 0xffffffff, L"Depth mode:" );
            m_Font.DrawText( 200, 115, 0xffffff00, strDepthMode[m_eDepthMode] );
            m_Font.DrawText(  64, 300, 0xffffffff, L"Depth mode description:" );
            m_Font.DrawText(  64, 325, 0xffaaaa10, strDepthModeDescription[m_eDepthMode] );
            m_Font.DrawText(  64, 400, 0xffffffff, L"Use " GLYPH_Y_BUTTON L" to choose depth mode." );

            static bool bDebugDepth = false;    // set this in the debugger to see depth mapping values
            if( bDebugDepth )
            {
                float fAlphaOffset, fAlphaSlope, fBlueOffset, fBlueSlope;
                CalculateDepthMapping(m_fDepth0, m_fDepth1, &fAlphaOffset, &fAlphaSlope, &fBlueOffset, &fBlueSlope);
                swprintf( str, L"fAlphaOffset %f 0x%02x", fAlphaOffset, (int)(fAlphaOffset * 255 + 0.5f));
                m_Font.DrawText( 64, 140, 0xff00ff00, str);
                swprintf( str, L"fAlphaSlope %f 0x%02x", fAlphaSlope, (int)(fAlphaSlope * 255 + 0.5f));
                m_Font.DrawText( 64, 170, 0xff00ff00, str);
                swprintf( str, L"fBlueOffset %f 0x%02x", fBlueOffset, (int)(fBlueOffset * 255 + 0.5f));
                m_Font.DrawText( 64, 200, 0xff00ff00, str);
                swprintf( str, L"fBlueSlope %f 0x%02x", fBlueSlope, (int)(fBlueSlope * 255 + 0.5f));
                m_Font.DrawText( 64, 230, 0xff00ff00, str);
                swprintf( str, L"m_fDepth0 %f 0x%x", m_fDepth0, (int)(m_fDepth0 * 65536 + 0.5f));
                m_Font.DrawText( 64, 260, 0xff00ff00, str);
                swprintf( str, L"m_fDepth1 %f 0x%x", m_fDepth1, (int)(m_fDepth1 * 65536 + 0.5f));
                m_Font.DrawText( 64, 290, 0xff00ff00, str);
            }
        }
        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FilterCopy()
// Desc: Filter the source texture by rendering into the destination texture
//       with subpixel offsets. Does 4 filter coefficients at a time, using all
//       the stages of the pixel shader.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FilterCopy( LPDIRECT3DTEXTURE8 pTextureDst,
                                 LPDIRECT3DTEXTURE8 pTextureSrc,
                                 DWORD dwNumSamples, FilterSample rSample[],
                                 DWORD dwSuperSampleX, DWORD dwSuperSampleY )
{
    // Set destination as render target, with no-depth buffer
    LPDIRECT3DSURFACE8 pSurface;
    pTextureDst->GetSurfaceLevel( 0, &pSurface );
    m_pd3dDevice->SetRenderTarget( pSurface, NULL );
    pSurface->Release();

    // Get descriptions of source and destination
    D3DSURFACE_DESC descSrc;
    pTextureSrc->GetLevelDesc( 0, &descSrc );

    // Set render state for filtering
    m_pd3dDevice->SetRenderState( D3DRS_LIGHTING,         FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_FILLMODE,         D3DFILL_SOLID );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE,  FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE,          D3DZB_FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_STENCILENABLE,    FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_FOGENABLE,        FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );          // On first rendering, copy new value over current render target contents
    m_pd3dDevice->SetRenderState( D3DRS_BLENDOP,          D3DBLENDOP_ADD ); // Setup subsequent renderings to add to previous value
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND,         D3DBLEND_ONE );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND,        D3DBLEND_ONE );

    // Set texture state
    DWORD xx; // texture stage index
    for( xx = 0; xx < 4; xx++ )
    {
        m_pd3dDevice->SetTexture( xx, pTextureSrc );  // use our source texture for all four stages
        m_pd3dDevice->SetTextureStageState( xx, D3DTSS_COLOROP, D3DTOP_DISABLE );
        m_pd3dDevice->SetTextureStageState( xx, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
        m_pd3dDevice->SetTextureStageState( xx, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE );  // pass texture coords without transformation
        m_pd3dDevice->SetTextureStageState( xx, D3DTSS_TEXCOORDINDEX, xx ); // each texture has different tex coords
        m_pd3dDevice->SetTextureStageState( xx, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP );
        m_pd3dDevice->SetTextureStageState( xx, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP );
        m_pd3dDevice->SetTextureStageState( xx, D3DTSS_MAXMIPLEVEL, 0 );
        m_pd3dDevice->SetTextureStageState( xx, D3DTSS_MIPFILTER, D3DTEXF_POINT );
        m_pd3dDevice->SetTextureStageState( xx, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetTextureStageState( xx, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetTextureStageState( xx, D3DTSS_COLORKEYOP, D3DTCOLORKEYOP_DISABLE );
        m_pd3dDevice->SetTextureStageState( xx, D3DTSS_COLORSIGN, 0 );
        m_pd3dDevice->SetTextureStageState( xx, D3DTSS_ALPHAKILL, D3DTALPHAKILL_DISABLE );
    }
    
    m_pd3dDevice->SetPixelShader( m_dwBlurPixelShader );          // use blur pixel shader
    m_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW|D3DFVF_TEX4 );   // for screen-space texture-mapped quadrilateral

    // Prepare quadrilateral vertices
    float x0 = -0.5f;
    float y0 = -0.5f;
    float x1 = (float)( descSrc.Width  / dwSuperSampleX ) - 0.5f;
    float y1 = (float)( descSrc.Height / dwSuperSampleY ) - 0.5f;
    struct QUAD
    {
        float x, y, z, w1;
        struct uv 
        {
            float u, v;
        } tex[4];   // each texture has different offset
    };
    
    QUAD aQuad[4] = 
    {
        { x0, y0, 1.0f, 1.0f, }, // texture coords are set below
        { x1, y0, 1.0f, 1.0f, },
        { x0, y1, 1.0f, 1.0f, },
        { x1, y1, 1.0f, 1.0f, }
    };

    // Draw a quad for each block of 4 filter coefficients
    xx = 0; // current texture stage
    FLOAT fOffsetScaleX, fOffsetScaleY; // convert destination coords to source texture coords
    FLOAT u0, v0, u1, v1;   // base source rectangle.
    if( XGIsSwizzledFormat(descSrc.Format) )
    {
        FLOAT fWidthScale  = 1.0f / (FLOAT)descSrc.Width;
        FLOAT fHeightScale = 1.0f / (FLOAT)descSrc.Height;
        fOffsetScaleX = (FLOAT)dwSuperSampleX * fWidthScale;
        fOffsetScaleY = (FLOAT)dwSuperSampleY * fHeightScale;
        u0 = 0.0f;
        v0 = 0.0f;
        u1 = (FLOAT)descSrc.Width * fWidthScale;
        v1 = (FLOAT)descSrc.Height * fHeightScale;
    }
    else
    {
        fOffsetScaleX = (FLOAT)dwSuperSampleX;
        fOffsetScaleY = (FLOAT)dwSuperSampleY;
        u0 = 0.0f;
        v0 = 0.0f;
        u1 = (FLOAT)descSrc.Width;
        v1 = (FLOAT)descSrc.Height;
    }
    D3DCOLOR rColor[4];
    DWORD rPSInput[4];
    for( DWORD dwSample = 0; dwSample < dwNumSamples; dwSample++ )
    {
        // Set filter coefficients
        FLOAT fValue = rSample[dwSample].fValue;
//      float rf[4] = {fValue, fValue, fValue, fValue};
//      m_pd3dDevice->SetPixelShaderConstant(xx, rf, 1);            // positive coeff
  
        if( fValue < 0.0f )
        {
            rColor[xx] = D3DXCOLOR(-fValue, -fValue, -fValue, -fValue);
            rPSInput[xx] = PS_INPUTMAPPING_SIGNED_NEGATE | ((xx % 2) ? PS_REGISTER_C1 : PS_REGISTER_C0);
        }
        else
        {
            rColor[xx] = D3DXCOLOR(fValue, fValue, fValue, fValue);
            rPSInput[xx] = PS_INPUTMAPPING_SIGNED_IDENTITY | ((xx % 2) ? PS_REGISTER_C1 : PS_REGISTER_C0);
        }

        // Align supersamples with center of destination pixels
        FLOAT fOffsetX = rSample[dwSample].fOffsetX * fOffsetScaleX;
        FLOAT fOffsetY = rSample[dwSample].fOffsetY * fOffsetScaleY;
        aQuad[0].tex[xx].u = u0 + fOffsetX;
        aQuad[0].tex[xx].v = v0 + fOffsetY;
        aQuad[1].tex[xx].u = u1 + fOffsetX;
        aQuad[1].tex[xx].v = v0 + fOffsetY;
        aQuad[2].tex[xx].u = u0 + fOffsetX;
        aQuad[2].tex[xx].v = v1 + fOffsetY;
        aQuad[3].tex[xx].u = u1 + fOffsetX;
        aQuad[3].tex[xx].v = v1 + fOffsetY;
        
        xx++; // Go to next stage
        if( xx == 4 || dwSample == dwNumSamples - 1 )  // max texture stages or last sample
        {
            // zero out unused texture stage coefficients 
            // (only for last filter sample, when number of samples is not divisible by 4)
            for( ; xx < 4; xx++ )
            {
                m_pd3dDevice->SetTexture( xx, NULL );
                rColor[xx] = 0;
                rPSInput[xx] = PS_INPUTMAPPING_UNSIGNED_IDENTITY | PS_REGISTER_ZERO;
            }
        
            // Set coefficients
            m_pd3dDevice->SetRenderState( D3DRS_PSCONSTANT0_0, rColor[0] );
            m_pd3dDevice->SetRenderState( D3DRS_PSCONSTANT1_0, rColor[1] );
            m_pd3dDevice->SetRenderState( D3DRS_PSCONSTANT0_1, rColor[2] );
            m_pd3dDevice->SetRenderState( D3DRS_PSCONSTANT1_1, rColor[3] );

            // Remap coefficients to proper sign
            m_pd3dDevice->SetRenderState( D3DRS_PSRGBINPUTS0,
                                          PS_COMBINERINPUTS( rPSInput[0] | PS_CHANNEL_RGB,   PS_REGISTER_T0 | PS_CHANNEL_RGB   | PS_INPUTMAPPING_SIGNED_IDENTITY,
                                                             rPSInput[1] | PS_CHANNEL_RGB,   PS_REGISTER_T1 | PS_CHANNEL_RGB   | PS_INPUTMAPPING_SIGNED_IDENTITY ) );
            m_pd3dDevice->SetRenderState( D3DRS_PSALPHAINPUTS0,
                                          PS_COMBINERINPUTS( rPSInput[0] | PS_CHANNEL_ALPHA, PS_REGISTER_T0 | PS_CHANNEL_ALPHA | PS_INPUTMAPPING_SIGNED_IDENTITY,
                                                             rPSInput[1] | PS_CHANNEL_ALPHA, PS_REGISTER_T1 | PS_CHANNEL_ALPHA | PS_INPUTMAPPING_SIGNED_IDENTITY ) );
            m_pd3dDevice->SetRenderState( D3DRS_PSRGBINPUTS1,
                                          PS_COMBINERINPUTS( rPSInput[2] | PS_CHANNEL_RGB,   PS_REGISTER_T2 | PS_CHANNEL_RGB   | PS_INPUTMAPPING_SIGNED_IDENTITY,
                                                             rPSInput[3] | PS_CHANNEL_RGB,   PS_REGISTER_T3 | PS_CHANNEL_RGB   | PS_INPUTMAPPING_SIGNED_IDENTITY ) );
            m_pd3dDevice->SetRenderState( D3DRS_PSALPHAINPUTS1,
                                          PS_COMBINERINPUTS( rPSInput[2] | PS_CHANNEL_ALPHA, PS_REGISTER_T2 | PS_CHANNEL_ALPHA | PS_INPUTMAPPING_SIGNED_IDENTITY,
                                                             rPSInput[3] | PS_CHANNEL_ALPHA, PS_REGISTER_T3 | PS_CHANNEL_ALPHA | PS_INPUTMAPPING_SIGNED_IDENTITY ) );
            
            // Draw the quad to filter the coefficients so far
            m_pd3dDevice->DrawPrimitiveUP( D3DPT_TRIANGLESTRIP, 2, aQuad, sizeof(QUAD) ); // one quad blends 4 textures
            m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE ); // on subsequent renderings, add to what's in the render target 
            xx = 0;
        }
    }

    // Clear texture stages
    for( xx=0; xx<4; xx++ )
    {
        m_pd3dDevice->SetTexture( xx, NULL );
        m_pd3dDevice->SetTextureStageState( xx, D3DTSS_COLOROP, D3DTOP_DISABLE );
        m_pd3dDevice->SetTextureStageState( xx, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
        m_pd3dDevice->SetTextureStageState( xx, D3DTSS_MIPMAPLODBIAS, 0 );
    }

    // Restore render target, zbuffer, and state
    m_pd3dDevice->SetPixelShader( NULL );
    m_pd3dDevice->SetRenderTarget( m_pBackBuffer, m_pDepthBuffer );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Blur()
// Desc: Blur backbuffer and set m_pBlur to the current blur texture.  Calls
//       FilterCopy() with different filter coefficients and offsets, based on
//       the current FILTERMODE setting.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Blur()
{
    XGSetTextureHeader( m_dwBackBufferWidth, m_dwBackBufferHeight, 1, 0, 
                        m_dwBackBufferFormat, 0, &m_BackBufferTexture, 
                        m_pBackBuffer->Data, 
                        m_dwBackBufferWidth * XGBytesPerPixelFromFormat(m_dwBackBufferFormat) );

    // Filters align to blurriest point in supersamples, on the 0.5 boundaries.
    // This takes advantage of the bilinear filtering in the texture map lookup.
    static FilterSample BoxFilter[] =     // for 2x2 downsampling
    {
        { 0.25f, -0.5f, -0.5f },
        { 0.25f,  0.5f, -0.5f },
        { 0.25f, -0.5f,  0.5f },
        { 0.25f,  0.5f,  0.5f },
    };
    static FilterSample YFilter[] =       // 1221 4-tap filter in Y
    {
        { 1.0f/6.0f, 0.0f, -1.5f },
        { 2.0f/6.0f, 0.0f, -0.5f },
        { 2.0f/6.0f, 0.0f,  0.5f },
        { 1.0f/6.0f, 0.0f,  1.5f },
    };
    static FilterSample XFilter[] =       // 1221 4-tap filter in X
    {
        { 1.0f/6.0f, -1.5f, 0.0f },
        { 2.0f/6.0f, -0.5f, 0.0f },
        { 2.0f/6.0f,  0.5f, 0.0f },
        { 1.0f/6.0f,  1.5f, 0.0f },
    };
    static FilterSample Y141Filter[] =    // 141 3-tap filter in Y
    {
        { 1.0f/6.0f, 0.0f, -1.0f },
        { 4.0f/6.0f, 0.0f,  0.0f },
        { 1.0f/6.0f, 0.0f,  1.0f },
    };
    static FilterSample X141Filter[] =        // 141 3-tap filter in X
    {
        { 1.0f/6.0f, -1.0f, 0.0f },
        { 4.0f/6.0f,  0.0f, 0.0f },
        { 1.0f/6.0f,  1.0f, 0.0f },
    };
    static FilterSample IdentityFilter[] = // No filtering
    {
        { 1.0f, 0.0f, 0.0f },
    };

    switch( m_eFilterMode )
    {
        case FM_IDENTITY:
        {
            m_strFilterName        = L"IDENTITY";
            m_strFilterDescription = L"Identity filter.";

            // Blur from the backbuffer to the blur texture
            D3DTexture* pTextureSrc = &m_BackBufferTexture;
            D3DTexture* pTextureDst = m_pBlurTextures[0];
            FilterCopy( pTextureDst, pTextureSrc, 1, IdentityFilter, 1, 1 );
            
            m_pBlur = pTextureDst;
            break;
        }

        case FM_BOX:
        {
            m_strFilterName        = L"BOX";
            m_strFilterDescription = L"2x2 box filter, no decimation";

            // Blur from the backbuffer to the blur texture
            D3DTexture* pTextureSrc = &m_BackBufferTexture;
            D3DTexture* pTextureDst = m_pBlurTextures[0];
            FilterCopy( pTextureDst, pTextureSrc, 4, BoxFilter, 1, 1 );
            
            m_pBlur = pTextureDst;
            break;
        }

        case FM_VERT:
        {
            m_strFilterName        = L"VERT";
            m_strFilterDescription = L"Vertical Gaussian (1221), no decimation";

            // Blur from the backbuffer to the blur texture
            D3DTexture* pTextureSrc = &m_BackBufferTexture;
            D3DTexture* pTextureDst = m_pBlurTextures[0];
            FilterCopy( pTextureDst, pTextureSrc, 4, YFilter, 1, 1 );
            
            m_pBlur = pTextureDst;
            break;
        }

        case FM_HORIZ:
        {
            m_strFilterName        = L"HORIZ";
            m_strFilterDescription = L"Horizontal Gaussian (1221), no decimation";

            // Blur from the backbuffer to the blur texture
            D3DTexture* pTextureSrc = &m_BackBufferTexture;
            D3DTexture* pTextureDst = m_pBlurTextures[0];
            FilterCopy( pTextureDst, pTextureSrc, 4, XFilter, 1, 1 );
            
            m_pBlur = pTextureDst;
            break;
        }

        case FM_BOX2:
        {
            m_strFilterName        = L"BOX2";
            m_strFilterDescription = L"2x2 box filter, 2x2 downsampling";

            // Blur from the backbuffer to the 1/2 sized blur texture
            D3DTexture* pTextureSrc = &m_BackBufferTexture;
            D3DTexture *pTextureDst = m_pBlurTextures[1];
            FilterCopy( pTextureDst, pTextureSrc, 4, BoxFilter, 2, 2 );
            
            m_pBlur = pTextureDst;
            break;
        }

        case FM_VERT2:
        {
            m_strFilterName        = L"VERT2";
            m_strFilterDescription = L"Vertical Gaussian (1221), 2x2 downsampling";

            // Blur from the backbuffer to the 1/2 sized blur texture
            D3DTexture* pTextureSrc = &m_BackBufferTexture;
            D3DTexture* pTextureDst = m_pBlurTextures[1];
            FilterCopy( pTextureDst, pTextureSrc, 4, YFilter, 2, 2 );
            
            m_pBlur = pTextureDst;
            break;
        }

        case FM_HORIZ2:
        {
            m_strFilterName        = L"HORIZ2";
            m_strFilterDescription = L"Horizontal Gaussian (1221), 2x2 downsampling";

            // Blur from the backbuffer to the 1/2 sized blur texture
            D3DTexture* pTextureSrc = &m_BackBufferTexture;
            D3DTexture *pTextureDst = m_pBlurTextures[1];
            FilterCopy( pTextureDst, pTextureSrc, 4, XFilter, 2, 2 );
            
            m_pBlur = pTextureDst;
            break;
        }
        
        case FM_VERT2_HORIZ2:
        {
            m_strFilterName        = L"VERT2 x HORIZ2";
            m_strFilterDescription = L"2 passes: Vertical Gaussian (1221) followed by\n"
                                     L"horizontal Gaussian (1221), with 2x2 downsampling";

            // Blur from the backbuffer to the 1/2 sized blur texture
            D3DTexture* pTextureSrc = &m_BackBufferTexture;
            D3DTexture* pTextureDst = m_pBlurTextures[1];
            FilterCopy( pTextureDst, pTextureSrc, 4, YFilter, 2, 2 );
            
            // Blur from the previous blur texture to the next blur texture
            pTextureSrc = m_pBlurTextures[1];
            pTextureDst = m_pBlurTextures[2];
            FilterCopy( pTextureDst, pTextureSrc, 4, XFilter, 2, 2 );
            
            m_pBlur = pTextureDst;
            break;
        }

        case FM_HORIZ2_VERT2:
        {
            m_strFilterName        = L"HORIZ2 x VERT2";
            m_strFilterDescription = L"2 passes: Horizontal Gaussian (1221) followed by\n"
                                     L"vertical Gaussian (1221), with 2x2 downsampling";

            // Blur from the backbuffer to the 1/2 sized blur texture
            D3DTexture* pTextureSrc = &m_BackBufferTexture;
            D3DTexture* pTextureDst = m_pBlurTextures[1];
            FilterCopy( pTextureDst, pTextureSrc, 4, XFilter, 2, 2 );
            
            // Blur from the previous blur texture to the next blur texture
            pTextureSrc = m_pBlurTextures[1];
            pTextureDst = m_pBlurTextures[2];
            FilterCopy( pTextureDst, pTextureSrc, 4, YFilter, 2, 2 );
            
            m_pBlur = pTextureDst;
            break;
        }

        case FM_VERT2_HORIZ:
        {
            m_strFilterName        = L"VERT2 x HORIZ";
            m_strFilterDescription = L"2 passes: Vertical Gaussian (1221) followed by\n"
                                     L"narrow horizontal Gaussian (141), with 2x2\n"
                                     L"downsampling";

            // Blur from the backbuffer to the 1/2 sized blur texture
            D3DTexture* pTextureSrc = &m_BackBufferTexture;
            D3DTexture* pTextureDst = m_pBlurTextures[1];
            FilterCopy( pTextureDst, pTextureSrc, 4, YFilter, 2, 2 );
            
            // Blur from the previous blur texture to the next blur texture
            pTextureSrc = m_pBlurTextures[1];
            pTextureDst = m_pBlurTextures[2];
            FilterCopy( pTextureDst, pTextureSrc, 3, X141Filter, 2, 2 );
            
            m_pBlur = pTextureDst;
            break;
        }

        case FM_HORIZ2_VERT:
        {
            m_strFilterName        = L"HORIZ2 x VERT";
            m_strFilterDescription = L"2 passes: Horizontal Gaussian (1221) followed by\n"
                                     L"narrow vertical Gaussian (141), with 2x2\n"
                                     L"downsampling";

            // Blur from the backbuffer to the 1/2 sized blur texture
            D3DTexture* pTextureSrc = &m_BackBufferTexture;
            D3DTexture* pTextureDst = m_pBlurTextures[1];
            FilterCopy( pTextureDst, pTextureSrc, 4, XFilter, 2, 2 );

            // Blur from the previous blur texture to the next blur texture
            pTextureSrc = m_pBlurTextures[1];
            pTextureDst = m_pBlurTextures[2];  // destination is next blur texture
            FilterCopy( pTextureDst, pTextureSrc, 3, Y141Filter, 2, 2 );
            
            m_pBlur = pTextureDst;
            break;
        }

        case FM_BOX2_BOX2:
        {
            m_strFilterName        = L"BOX2 x BOX2";
            m_strFilterDescription = L"2 passes: Box filter followed by box filter,\n"
                                     L"with 2x2 downsampling";

            // Blur from the backbuffer to the 1/2 sized blur texture
            D3DTexture* pTextureSrc = &m_BackBufferTexture;
            D3DTexture* pTextureDst = m_pBlurTextures[1];
            FilterCopy( pTextureDst, pTextureSrc, 4, BoxFilter, 2, 2 );

            // Blur from the previous blur texture to the next blur texture
            pTextureSrc = m_pBlurTextures[1];
            pTextureDst = m_pBlurTextures[2];
            FilterCopy( pTextureDst, pTextureSrc, 4, BoxFilter, 2, 2 );
            
            m_pBlur = pTextureDst;
            break;
        }

        case FM_VERT2_HORIZ2_BOX2:
        {
            m_strFilterName        = L"VERT2 x HORIZ2 x BOX2";
            m_strFilterDescription = L"3 passes: Vertical Gaussian (1441) followed by\n"
                                     L"horizontal Gaussian (1441) followed by box filter,\n"
                                     L"with 2x2 downsampling";

            // Blur from the backbuffer to the 1/2 sized blur texture
            D3DTexture* pTextureSrc = &m_BackBufferTexture;
            D3DTexture* pTextureDst = m_pBlurTextures[1];
            FilterCopy( pTextureDst, pTextureSrc, 4, YFilter, 2, 2 );

            // Blur from the previous blur texture to the next blur texture
            pTextureSrc = m_pBlurTextures[1];
            pTextureDst = m_pBlurTextures[2];
            FilterCopy( pTextureDst, pTextureSrc, 4, XFilter, 2, 2 );

            // Blur from the previous blur texture to the next blur texture
            pTextureSrc = m_pBlurTextures[2];
            pTextureDst = m_pBlurTextures[3];
            FilterCopy( pTextureDst, pTextureSrc, 4, BoxFilter, 2, 2 );
            
            m_pBlur = pTextureDst;
            break;
        }

        case FM_BOX2_BOX2_BOX2:
        {
            m_strFilterName        = L"BOX2 x BOX2 x BOX2";
            m_strFilterDescription = L"3 passes: Box filter followed by box filter\n"
                                     L"followed by box filter, with 2x2 downsampling";

            // Blur from the backbuffer to the 1/2 sized blur texture
            D3DTexture* pTextureSrc = &m_BackBufferTexture;
            D3DTexture* pTextureDst = m_pBlurTextures[1];
            FilterCopy( pTextureDst, pTextureSrc, 4, BoxFilter, 2, 2 );

            // Blur from the previous blur texture to the next blur texture
            pTextureSrc = m_pBlurTextures[1];
            pTextureDst = m_pBlurTextures[2];
            FilterCopy( pTextureDst, pTextureSrc, 4, BoxFilter, 2, 2 );

            // Blur from the previous blur texture to the next blur texture
            pTextureSrc = m_pBlurTextures[2];
            pTextureDst = m_pBlurTextures[3];
            FilterCopy( pTextureDst, pTextureSrc, 4, BoxFilter, 2, 2 );
            
            m_pBlur = pTextureDst;
            break;
        }

        case FM_VERT2_HORIZ2_VERT2:
        {
            m_strFilterName        = L"VERT2 x HORIZ2 x VERT2";
            m_strFilterDescription = L"3 passes: Vertical Gaussian (1441) then horizontal\n"
                                     L"Gaussian (1441) then vertical Gaussian (1441)\n"
                                     L"with 2x2 downsampling";

            // Blur from the backbuffer to the 1/2 sized blur texture
            D3DTexture* pTextureSrc = &m_BackBufferTexture;
            D3DTexture* pTextureDst = m_pBlurTextures[1];
            FilterCopy( pTextureDst, pTextureSrc, 4, YFilter, 2, 2 );

            // Blur from the previous blur texture to the next blur texture
            pTextureSrc = m_pBlurTextures[1];
            pTextureDst = m_pBlurTextures[2];
            FilterCopy( pTextureDst, pTextureSrc, 4, XFilter, 2, 2 );

            // Blur from the previous blur texture to the next blur texture
            pTextureSrc = m_pBlurTextures[2];
            pTextureDst = m_pBlurTextures[3];
            FilterCopy( pTextureDst, pTextureSrc, 4, YFilter, 2, 2 );
            
            m_pBlur = pTextureDst;
            break;
        }

        case FM_HORIZ2_VERT2_HORIZ2:
        {
            m_strFilterName        = L"HORIZ2 x VERT2 x HORIZ2";
            m_strFilterDescription = L"3 passes: Horizontal Gaussian (1441) then vertical\n"
                                     L"Gaussian (1441) then horizontal Gaussian (1441)\n"
                                     L"with 2x2 downsampling";

            // Blur from the backbuffer to the 1/2 sized blur texture
            D3DTexture* pTextureSrc = &m_BackBufferTexture;
            D3DTexture* pTextureDst = m_pBlurTextures[1];
            FilterCopy( pTextureDst, pTextureSrc, 4, XFilter, 2, 2 );

            // Blur from the previous blur texture to the next blur texture
            pTextureSrc = m_pBlurTextures[1];
            pTextureDst = m_pBlurTextures[2];
            FilterCopy( pTextureDst, pTextureSrc, 4, YFilter, 2, 2 );

            // Blur from the previous blur texture to the next blur texture
            pTextureSrc = m_pBlurTextures[2];
            pTextureDst = m_pBlurTextures[3];
            FilterCopy( pTextureDst, pTextureSrc, 4, XFilter, 2, 2 );
            
            m_pBlur = pTextureDst;
            break;
        }

        case FM_VERT2_HORIZ2_VERT2_HORIZ2:
        {
            m_strFilterName        = L"VERT2 x HORIZ2 x VERT2 x HORIZ2";
            m_strFilterDescription = L"4 passes, alternating vertical Gaussian (1441)\n"
                                     L"then horizontal Gaussian (1441), with 2x2\n"
                                     L"downsampling";

            // Blur from the backbuffer to the 1/2 sized blur texture
            D3DTexture* pTextureSrc = &m_BackBufferTexture;
            D3DTexture* pTextureDst = m_pBlurTextures[1];
            FilterCopy( pTextureDst, pTextureSrc, 4, YFilter, 2, 2 );

            // Blur from the previous blur texture to the next blur texture
            pTextureSrc = m_pBlurTextures[1];
            pTextureDst = m_pBlurTextures[2];
            FilterCopy( pTextureDst, pTextureSrc, 4, XFilter, 2, 2 );

            // Blur from the previous blur texture to the next blur texture
            pTextureSrc = m_pBlurTextures[2];
            pTextureDst = m_pBlurTextures[3];
            FilterCopy( pTextureDst, pTextureSrc, 4, YFilter, 2, 2 );

            // Blur from the previous blur texture to the next blur texture
            pTextureSrc = m_pBlurTextures[3];
            pTextureDst = m_pBlurTextures[4];
            FilterCopy( pTextureDst, pTextureSrc, 4, XFilter, 2, 2 );
            
            m_pBlur = pTextureDst;
            break;
        }

        case FM_HORIZ2_VERT2_HORIZ2_VERT2:
        {
            m_strFilterName        = L"HORIZ2 x VERT2 x HORIZ2 x VERT2";
            m_strFilterDescription = L"4 passes, alternating horizontal Gaussian (1441)\n"
                                     L"then vertical Gaussian (1441), with 2x2\n"
                                     L"downsampling";

            // Blur from the backbuffer to the 1/2 sized blur texture
            D3DTexture* pTextureSrc = &m_BackBufferTexture;
            D3DTexture* pTextureDst = m_pBlurTextures[1];
            FilterCopy( pTextureDst, pTextureSrc, 4, XFilter, 2, 2 );

            // Blur from the previous blur texture to the next blur texture
            pTextureSrc = m_pBlurTextures[1];
            pTextureDst = m_pBlurTextures[2];
            FilterCopy( pTextureDst, pTextureSrc, 4, YFilter, 2, 2 );

            // Blur from the previous blur texture to the next blur texture
            pTextureSrc = m_pBlurTextures[2];
            pTextureDst = m_pBlurTextures[3];
            FilterCopy( pTextureDst, pTextureSrc, 4, XFilter, 2, 2 );

            // Blur from the previous blur texture to the next blur texture
            pTextureSrc = m_pBlurTextures[3];
            pTextureDst = m_pBlurTextures[4];
            FilterCopy( pTextureDst, pTextureSrc, 4, YFilter, 2, 2 );
            
            m_pBlur = pTextureDst;
            break;
        }

        case FM_BOX2_BOX2_BOX2_BOX2:
        {
            m_strFilterName        = L"BOX2 x BOX2 x BOX2 x BOX2";
            m_strFilterDescription = L"4 passes of box filter with 2x2 downsampling";

            // Blur from the backbuffer to the 1/2 sized blur texture
            D3DTexture* pTextureSrc = &m_BackBufferTexture;
            D3DTexture* pTextureDst = m_pBlurTextures[1];
            FilterCopy( pTextureDst, pTextureSrc, 4, BoxFilter, 2, 2 );

            // Blur from the previous blur texture to the next blur texture
            pTextureSrc = m_pBlurTextures[1];
            pTextureDst = m_pBlurTextures[2];
            FilterCopy( pTextureDst, pTextureSrc, 4, BoxFilter, 2, 2 );

            // Blur from the previous blur texture to the next blur texture
            pTextureSrc = m_pBlurTextures[2];
            pTextureDst = m_pBlurTextures[3];
            FilterCopy( pTextureDst, pTextureSrc, 4, BoxFilter, 2, 2 );

            // Blur from the previous blur texture to the next blur texture
            pTextureSrc = m_pBlurTextures[3];
            pTextureDst = m_pBlurTextures[4];
            FilterCopy( pTextureDst, pTextureSrc, 4, BoxFilter, 2, 2 );
            
            m_pBlur = pTextureDst;
            break;
        }

        default:
            m_pBlur = NULL;
            break;
    }

    return S_OK;
}
    



//-----------------------------------------------------------------------------
// Name: DrawBlur()
// Desc: Display the blurry texture
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::DebugDrawBlur()
{
    if( !m_pBlur )
        return S_FALSE;

    // Texture coordinates in linear format textures go from 0 to n-1 rather
    // than the 0 to 1 that is used for swizzled textures.
    D3DSURFACE_DESC desc;
    m_pBlur->GetLevelDesc( 0, &desc );
    struct BACKGROUNDVERTEX { D3DXVECTOR4 p; FLOAT tu, tv; } v[4];
    v[0].p = D3DXVECTOR4( -0.5f,                      -0.5f,                       1.0f, 1.0f );
    v[1].p = D3DXVECTOR4( m_dwBackBufferWidth - 0.5f, -0.5f,                       1.0f, 1.0f );
    v[2].p = D3DXVECTOR4( -0.5f,                      m_dwBackBufferHeight - 0.5f, 1.0f, 1.0f );
    v[3].p = D3DXVECTOR4( m_dwBackBufferWidth - 0.5f, m_dwBackBufferHeight - 0.5f, 1.0f, 1.0f );
    v[0].tu = 0.0f;              v[0].tv = 0.0f;
    v[1].tu = (float)desc.Width; v[1].tv = 0.0f;
    v[2].tu = 0.0f;              v[2].tv = (float)desc.Height;
    v[3].tu = (float)desc.Width; v[3].tv = (float)desc.Height;
    
    // Set states
    m_pd3dDevice->SetTexture( 0, m_pBlur );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU,  D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV,  D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE,          FALSE ); 
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE,  FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_FILLMODE,         D3DFILL_SOLID );

    // Render the screen-aligned quadrilateral
    m_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW|D3DFVF_TEX1 );
    m_pd3dDevice->DrawPrimitiveUP( D3DPT_QUADSTRIP, 1, v, sizeof(BACKGROUNDVERTEX) );

    // Reset render states
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetTexture( 0, NULL );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: DrawDepthRaw()
// Desc: Display the depth buffer as color values, with the most significant
//       bits in red and green
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::DebugDrawDepthRaw()
{
    // Make a D3DTexture wrapper around the depth buffer surface
    // Note: override the format to put the most significant bits in the red and green channels
    D3DTexture ZBufferTexture;
    XGSetTextureHeader( m_dwBackBufferWidth, m_dwBackBufferHeight, 1, 0, 
                        D3DFMT_LIN_R8G8B8A8, 0, &ZBufferTexture, m_pDepthBuffer->Data,
                        m_dwBackBufferWidth * 4 );

    struct BACKGROUNDVERTEX { D3DXVECTOR4 p; FLOAT tu, tv; } v[4];
    v[0].p = D3DXVECTOR4(         -0.5f,              -0.5f,                       1.0f, 1.0f ); v[0].tu = 0.0f;                       v[0].tv = 0.0f;
    v[1].p = D3DXVECTOR4( m_dwBackBufferWidth - 0.5f, -0.5f,                       1.0f, 1.0f ); v[1].tu = (float)m_dwBackBufferWidth; v[1].tv = 0.0f;
    v[2].p = D3DXVECTOR4(         -0.5f,              m_dwBackBufferHeight - 0.5f, 1.0f, 1.0f ); v[2].tu = 0.0f;                       v[2].tv = (float)m_dwBackBufferHeight;
    v[3].p = D3DXVECTOR4( m_dwBackBufferWidth - 0.5f, m_dwBackBufferHeight - 0.5f, 1.0f, 1.0f ); v[3].tu = (float)m_dwBackBufferWidth; v[3].tv = (float)m_dwBackBufferHeight;
    
    // Set states
    m_pd3dDevice->SetTexture( 0, &ZBufferTexture );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE|D3DTA_COMPLEMENT );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE,          FALSE ); 
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE,  FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_FILLMODE,         D3DFILL_SOLID );

    // Render the screen-aligned quadrilateral
    m_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW|D3DFVF_TEX1 );
    m_pd3dDevice->DrawPrimitiveUP( D3DPT_QUADSTRIP, 1, v, sizeof(BACKGROUNDVERTEX) );

    // Reset render states
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetTexture( 0, NULL );
    m_pd3dDevice->SetTexture( 1, NULL );

    return S_OK;
}




//////////////////////////////////////////////////////////////////////
// For mapping from the depth buffer to blend values using
// a texture map lookup. See media\shaders\depthlookup.psh
//
// This is more general than computing the range as in
// media\shaders\depth.psh, since the ramp can be filled in
// arbitrarily, but may be more expensive due to the extra texture
// lookup.
//
float FUnitMap(float fAlpha, float fBlue, float fAlphaOffset, float fAlphaSlope, float fBlueOffset, float fBlueSlope)
{
    //return g_fPixelShaderScale * fAlphaSlope * (fAlpha - fAlphaOffset) + fBlueSlope * fBlue + fBlueOffset - 0.5f;
    return g_fPixelShaderScale * fAlphaSlope * (fAlpha - fAlphaOffset) + fBlueSlope * (fBlue - fBlueOffset);
}

float FQuantizedDepth(float fDepth, float *pfAlpha, float *pfBlue)
{
    float fDepth16 = fDepth * (float)(1 << 16);
    DWORD dwDepth16 = (DWORD)(fDepth16 /*+ 0.5f*/);
    *pfAlpha = (dwDepth16 >> 8) * (1.0f / 255.0f);
    *pfBlue = (dwDepth16 & 0xff) * (1.0f / 255.0f);
    return (float)dwDepth16 / (float)(1 << 16);
}




HRESULT CXBoxSample::FillFocusRangeTexture( bool bRamp )
{
    HRESULT hr;
    static const DWORD Width = 256;
    static const DWORD Height = 64;
    
    // Create the focus range texture
    if( m_pTextureFocusRange )
        m_pTextureFocusRange->Release();
    m_pd3dDevice->CreateTexture( Width, Height, 1, 0, D3DFMT_A8, 0, &m_pTextureFocusRange);
    
    // Fill the focus range texture
    D3DLOCKED_RECT lockedRect;
    hr = m_pTextureFocusRange->LockRect( 0, &lockedRect, NULL, 0L );
    if( FAILED(hr) )
        return hr;
    
    DWORD dwPixelStride = 1;
    Swizzler s(Width, Height, 0);
    s.SetV(s.SwizzleV(0));
    s.SetU(s.SwizzleU(0));
    if( bRamp )
    {
        for( DWORD j = 0; j < Height; j++ )
        {
            for( DWORD i = 0; i < Width; i++ )
            {
                BYTE *p = (BYTE *)lockedRect.pBits + dwPixelStride * s.Get2D();
                *p = (BYTE)i;
                s.IncU();
            }
            s.IncV();
        }
    }
    else
    {
        float fAlphaOffset, fAlphaSlope, fBlueOffset, fBlueSlope;
        CalculateDepthMapping( m_fDepth0, m_fDepth1, &fAlphaOffset, &fAlphaSlope, &fBlueOffset, &fBlueSlope );
        for( DWORD i = 0; i < Width; i++ )
        {
            for( DWORD j = 0; j < Height; j++ )
            {
                BYTE *p = (BYTE *)lockedRect.pBits + dwPixelStride * s.Get2D();
                float fAlpha = (float)i / (Width - 1);
                float fBlue  = (float)j / (Height - 1);
                float fUnit  = 2.0f * (FUnitMap(fAlpha, fBlue, fAlphaOffset, fAlphaSlope, fBlueOffset, fBlueSlope) - 0.5f);
                float fMap   = 1.0f - fUnit * fUnit;
                if( fMap < 0.0f ) fMap = 0.0f;
                if( fMap > 1.0f ) fMap = 1.0f;
                *p = (BYTE)(255 * fMap + 0.5f);
                s.IncV();
            }
            s.IncU();
        }
    }
    
    m_pTextureFocusRange->UnlockRect(0);

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: CalculateFocusDepths()
// Desc: Use fractions along viewing vector to set focus depths
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::CalculateFocusDepths()
{
    D3DXVECTOR3 vEye = m_vAt - m_vFrom;
    D3DXVECTOR3 v0   = m_vFrom + m_fFraction0 * vEye;
    D3DXVECTOR3 v1   = m_vFrom + m_fFraction1 * vEye;
    
    CalculateDepth( &m_fDepth0, v0 );
    CalculateDepth( &m_fDepth1, v1 );
    
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: CalculateDepth()
// Desc: Transform a point through our world, view, and projection 
//       matrices to obtain a depth value.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::CalculateDepth( float* pfDepth, const D3DXVECTOR3& vPosition )
{
    D3DXVECTOR4 v( vPosition.x, vPosition.y, vPosition.z, 1.0f );
    D3DXVec4Transform( &v, &v, &m_matWorld );
    D3DXVec4Transform( &v, &v, &m_matView );
    D3DXVec4Transform( &v, &v, &m_matProjection );
    
    (*pfDepth) = v.z / v.w;
    
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: CalculateDepthMapping()
// Desc: Calculate offsets and slope to map given z range to 0,1 in
//       the depth and focus pixel shaders.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::CalculateDepthMapping( float fDepth0, float fDepth1,
                                            float* pfAlphaOffset, float* pfAlphaSlope, 
                                            float* pfBlueOffset, float* pfBlueSlope )
{
    // Check range of args
    if( fDepth0 < 0.0f ) fDepth0 = 0.0f;
    if( fDepth0 > 1.0f ) fDepth0 = 1.0f;
    if( fDepth1 < 0.0f ) fDepth1 = 0.0f;
    if( fDepth1 > 1.0f ) fDepth1 = 1.0f;

    if( fDepth1 < fDepth0 )
    {
        // Swap depth to make fDepth0 <= fDepth1
        float t = fDepth1;
        fDepth1 = fDepth0;
        fDepth0 = t;
    }
    
    // Calculate quantized values
    float fAlpha0, fBlue0;
    float fQuantizedDepth0 = FQuantizedDepth(fDepth0, &fAlpha0, &fBlue0);
    float fAlpha1, fBlue1;
    float fQuantizedDepth1 = FQuantizedDepth(fDepth1, &fAlpha1, &fBlue1);

    // Calculate offset and slopes
    float fScale = 1.0f / (fQuantizedDepth1 - fQuantizedDepth0);
    if( fScale > g_fPixelShaderScale )
    {
        fScale = g_fPixelShaderScale; // This is the steepest slope we can handle
        fDepth0 = 0.5f * (fDepth0 + fDepth1) - 0.5f / fScale; // Move start so that peak is in middle of fDepth0 and fDepth1
        fDepth1 = fDepth0 + 1.0f / fScale;
        fQuantizedDepth0 = FQuantizedDepth(fDepth0, &fAlpha0, &fBlue0);
        fQuantizedDepth1 = FQuantizedDepth(fDepth1, &fAlpha1, &fBlue1);
    }
    
    (*pfAlphaOffset) = fAlpha0;
    (*pfAlphaSlope)  = fScale / g_fPixelShaderScale;
    (*pfBlueSlope)   = fScale * (1.0f/255.0f); // blue ramp adds more levels to the ramp

    // Align peak of map to center by calculating the quantized alpha value
//    *pfBlueOffset = 0.5f;   // zero biased up by 0.5f
//    float fZeroDesired = (fQuantizedDepth0 - fDepth0) / (fDepth1 - fDepth0);
//    float fZero = FUnitMap(fAlpha0, fBlue0, *pfAlphaOffset, *pfAlphaSlope, *pfBlueOffset, *pfBlueSlope);
//    float fOneDesired = (fQuantizedDepth1 - fDepth0) / (fDepth1 - fDepth0);
//    float fOne = FUnitMap(fAlpha1, fBlue1, *pfAlphaOffset, *pfAlphaSlope, *pfBlueOffset, *pfBlueSlope);
//    *pfBlueOffset = 0.5f * (fZeroDesired-fZero + fOneDesired-fOne) + 0.5f;  // biased up by 0.5f
    (*pfBlueOffset) = fBlue0;
    
    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: DrawDepthRange()
// Desc: Display the depth buffer mapped to focus values using pixel shader
//       arithmetic.  See media/shaders/depth.psh for more details.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::DebugDrawDepthRange()
{
    // Make a D3DTexture wrapper around the depth buffer surface
    D3DTexture ZBufferTexture;
    XGSetTextureHeader( m_dwBackBufferWidth, m_dwBackBufferHeight, 1, 0,
                        D3DFMT_LIN_A8B8G8R8, 0, &ZBufferTexture, m_pDepthBuffer->Data,
                        m_dwBackBufferWidth * 4);

    struct BACKGROUNDVERTEX { D3DXVECTOR4 p; FLOAT tu, tv; } v[4];
    v[0].p = D3DXVECTOR4( -0.5f,                      -0.5f,                       1.0f, 1.0f ); v[0].tu = 0.0f;                       v[0].tv = 0.0f;
    v[1].p = D3DXVECTOR4( m_dwBackBufferWidth - 0.5f, -0.5f,                       1.0f, 1.0f ); v[1].tu = (float)m_dwBackBufferWidth; v[1].tv = 0.0f;
    v[2].p = D3DXVECTOR4( -0.5f,                      m_dwBackBufferHeight - 0.5f, 1.0f, 1.0f ); v[2].tu = 0.0f;                       v[2].tv = (float)m_dwBackBufferHeight;
    v[3].p = D3DXVECTOR4( m_dwBackBufferWidth - 0.5f, m_dwBackBufferHeight - 0.5f, 1.0f, 1.0f ); v[3].tu = (float)m_dwBackBufferWidth; v[3].tv = (float)m_dwBackBufferHeight;
    
    // Set pixel shader states
    m_pd3dDevice->SetPixelShader( m_dwDepthPixelShader );
    float fAlphaOffset, fAlphaSlope, fBlueOffset, fBlueSlope;
    CalculateDepthMapping(m_fDepth0, m_fDepth1, &fAlphaOffset, &fAlphaSlope, &fBlueOffset, &fBlueSlope);
    float Constants[] = 
    {
        0.0f, 0.0f, fBlueOffset, fAlphaOffset,        // offset
        0.0f, 0.0f, fBlueSlope, 0.0f,                 // 1x
        0.0f, 0.0f, 0.0f, 0.0f,                       // 4x
        0.0f, 0.0f, 0.0f, fAlphaSlope,                // 16x
    };
    m_pd3dDevice->SetPixelShaderConstant( 0, Constants, 4 );

    m_pd3dDevice->SetTexture( 0, &ZBufferTexture );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP );

    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE,          FALSE ); 
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE,  FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_FILLMODE,         D3DFILL_SOLID );

    // Render the screen-aligned quadrilateral
    m_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW|D3DFVF_TEX1 );
    m_pd3dDevice->DrawPrimitiveUP( D3DPT_QUADSTRIP, 1, v, sizeof(BACKGROUNDVERTEX) );

    // Reset render states
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE,  FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE,          TRUE );
    m_pd3dDevice->SetPixelShader( NULL );
    m_pd3dDevice->SetTexture( 0, NULL );
    m_pd3dDevice->SetTexture( 1, NULL );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: DrawDepthLookup()
// Desc: Display the depth buffer mapped through the lookup texture. This
//       function is for demonstrating the range of z values mapped to
//       focus values.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::DebugDrawDepthLookup()
{
    // Make a D3DTexture wrapper around the depth buffer surface
    D3DTexture ZBufferTexture;
    XGSetTextureHeader( m_dwBackBufferWidth, m_dwBackBufferHeight, 1, 0, 
                        D3DFMT_LIN_A8R8G8B8, 0, &ZBufferTexture,
                        m_pDepthBuffer->Data, m_dwBackBufferWidth * 4 );

    struct BACKGROUNDVERTEX { D3DXVECTOR4 p; FLOAT tu, tv; } v[4];
    v[0].p = D3DXVECTOR4( -0.5f,                      -0.5f,                       1.0f, 1.0f ); v[0].tu = 0.0f;                       v[0].tv = 0.0f;
    v[1].p = D3DXVECTOR4( m_dwBackBufferWidth - 0.5f, -0.5f,                       1.0f, 1.0f ); v[1].tu = (float)m_dwBackBufferWidth; v[1].tv = 0.0f;
    v[2].p = D3DXVECTOR4( -0.5f,                      m_dwBackBufferHeight - 0.5f, 1.0f, 1.0f ); v[2].tu = 0.0f;                       v[2].tv = (float)m_dwBackBufferHeight;
    v[3].p = D3DXVECTOR4( m_dwBackBufferWidth - 0.5f, m_dwBackBufferHeight - 0.5f, 1.0f, 1.0f ); v[3].tu = (float)m_dwBackBufferWidth; v[3].tv = (float)m_dwBackBufferHeight;

    // Set the filter modes
    m_pd3dDevice->SetTexture( 0, &ZBufferTexture );
    m_pd3dDevice->SetTexture( 1, m_pTextureFocusRange );

    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP );

    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    
    m_pd3dDevice->SetPixelShader( m_dwDepthLookupPixelShader );

    // Set render state
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE,          FALSE ); 
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE,  FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_FILLMODE,         D3DFILL_SOLID );

    // Render the screen-aligned quadrilateral
    m_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW|D3DFVF_TEX1 );
    m_pd3dDevice->DrawPrimitiveUP( D3DPT_QUADSTRIP, 1, v, sizeof(BACKGROUNDVERTEX) );

    // Reset render states
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE,  FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE,          TRUE );
    m_pd3dDevice->SetPixelShader( 0 );
    m_pd3dDevice->SetTexture( 0, NULL );
    m_pd3dDevice->SetTexture( 1, NULL );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: DrawFocusRange()
// Desc: Choose the focus range by mapping z to a focus value using pixel
//       shader arithmetic.  See media/shaders/focus.psh for more details.
//
//       High focus values leave the back-buffer unchanged.
//       Low focus values blend in the blurred texture computed by Blur().
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::DrawFocusEffectUsingRange()
{
    // Make a D3DTexture wrapper around the depth buffer surface
    D3DTexture ZBufferTexture;
    XGSetTextureHeader( m_dwBackBufferWidth, m_dwBackBufferHeight, 1, 0, 
                        D3DFMT_LIN_A8B8G8R8, 0, &ZBufferTexture,
                        m_pDepthBuffer->Data, m_dwBackBufferWidth * 4 );

    // Get size of blur texture for setting texture coords of final blur
    D3DSURFACE_DESC descBlur;
    m_pBlur->GetLevelDesc(0, &descBlur);
    float fOffsetX = 0.0f;
    float fOffsetY = 0.5f / (float)descBlur.Height; // vertical blur
    struct VERTEX 
    {
        D3DXVECTOR4 p;
        FLOAT tu0, tv0;
        FLOAT tu1, tv1;
    } v[4];
    v[0].p = D3DXVECTOR4( -0.5f,                      -0.5f,                       1.0f, 1.0f );
    v[1].p = D3DXVECTOR4( m_dwBackBufferWidth - 0.5f, -0.5f,                       1.0f, 1.0f );
    v[2].p = D3DXVECTOR4( -0.5f,                      m_dwBackBufferHeight - 0.5f, 1.0f, 1.0f );
    v[3].p = D3DXVECTOR4( m_dwBackBufferWidth - 0.5f, m_dwBackBufferHeight - 0.5f, 1.0f, 1.0f );
    v[0].tu0 = 0.0f;                       v[0].tv0 = 0.0f;
    v[1].tu0 = (float)m_dwBackBufferWidth; v[1].tv0 = 0.0f;
    v[2].tu0 = 0.0f;                       v[2].tv0 = (float)m_dwBackBufferHeight;
    v[3].tu0 = (float)m_dwBackBufferWidth; v[3].tv0 = (float)m_dwBackBufferHeight;
    v[0].tu1 = fOffsetX;                         v[0].tv1 = fOffsetY;
    v[1].tu1 = fOffsetX + (float)descBlur.Width; v[1].tv1 = fOffsetY;
    v[2].tu1 = fOffsetX;                         v[2].tv1 = fOffsetY + (float)descBlur.Height;
    v[3].tu1 = fOffsetX + (float)descBlur.Width; v[3].tv1 = fOffsetY + (float)descBlur.Height;
    
    // Set pixel shader state
    float fAlphaOffset, fAlphaSlope, fBlueOffset, fBlueSlope;
    CalculateDepthMapping( m_fDepth0, m_fDepth1, &fAlphaOffset, &fAlphaSlope, &fBlueOffset, &fBlueSlope );
    float Constants[] = 
    {
        0.0f, 0.0f, fBlueOffset, fAlphaOffset,      // offset
        0.0f, 0.0f, fBlueSlope, 0.0f,               // 1x
        0.0f, 0.0f, 0.0f, 0.0f,                     // 4x
        0.0f, 0.0f, 0.0f, fAlphaSlope,              // 16x
    };
    m_pd3dDevice->SetPixelShader( m_dwFocusPixelShader );
    m_pd3dDevice->SetPixelShaderConstant( 0, Constants, 4 );
    
    // Set render state
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE,          FALSE ); 
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE,  TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHAREF,         0 );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHAFUNC,        D3DCMP_GREATER );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND,         D3DBLEND_ONE );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND,        D3DBLEND_INVSRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_FILLMODE,         D3DFILL_SOLID );

    // Set texture state
    m_pd3dDevice->SetTexture( 0, &ZBufferTexture );
    m_pd3dDevice->SetTexture( 1, m_pBlur );
    m_pd3dDevice->SetTexture( 2, NULL );
    m_pd3dDevice->SetTexture( 3, NULL );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1 );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 2, D3DTSS_COLOROP, D3DTOP_DISABLE );
    m_pd3dDevice->SetTextureStageState( 2, D3DTSS_ALPHAOP, D3DTOP_DISABLE );
    m_pd3dDevice->SetTextureStageState( 3, D3DTSS_COLOROP, D3DTOP_DISABLE );
    m_pd3dDevice->SetTextureStageState( 3, D3DTSS_ALPHAOP, D3DTOP_DISABLE );

    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP );

    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    
    // Render the screen-aligned quadrilateral
    m_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW|D3DFVF_TEX4 );
    m_pd3dDevice->DrawPrimitiveUP( D3DPT_QUADSTRIP, 1, v, sizeof(VERTEX) );

    // Reset render states
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE,  FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE,          TRUE );
    m_pd3dDevice->SetPixelShader( NULL );
    m_pd3dDevice->SetTexture( 0, NULL );
    m_pd3dDevice->SetTexture( 1, NULL );
    m_pd3dDevice->SetTexture( 2, NULL );
    m_pd3dDevice->SetTexture( 3, NULL );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: DrawFocusLookup()
// Desc: Choose the focus range by mapping z through a lookup texture.
//
//       See media/shaders/focuslookup.psh for more detail.
//
//       This technique has lower performance than using DrawFocus(),
//       but the focus values can be arbitrary, rather than the
//       limited types of z-to-focus value mappings available with
//       pixel shader arithmetic.
//
//       High focus values leave the back-buffer unchanged.
//       Low focus values blend in the blurred texture computed by Blur().
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::DrawFocusEffectUsingLookup()
{
    // Make a D3DTexture wrapper around the depth buffer surface
    D3DTexture ZBufferTexture;
    XGSetTextureHeader( m_dwBackBufferWidth, m_dwBackBufferHeight, 1, 0, 
                        D3DFMT_LIN_A8R8G8B8, 0, &ZBufferTexture, 
                        m_pDepthBuffer->Data, m_dwBackBufferWidth * 4 );

    // Get size of blur texture for setting texture coords of final blur
    D3DSURFACE_DESC descBlur;
    m_pBlur->GetLevelDesc(0, &descBlur);
    FLOAT fOffsetX = 0.0f;
    FLOAT fOffsetY = 0.5f / (FLOAT)descBlur.Height; // vertical blur

    // Define a set of vertices to draw a quad in screenspace
    struct VERTEX 
    {
        D3DXVECTOR4 p;
        FLOAT tu0, tv0;
        FLOAT tu1, tv1;
        FLOAT tu2, tv2;
        FLOAT tu3, tv3;
    } v[4];
    v[0].p = D3DXVECTOR4( -0.5f,                      -0.5f,                       1.0f, 1.0f );
    v[1].p = D3DXVECTOR4( m_dwBackBufferWidth - 0.5f, -0.5f,                       1.0f, 1.0f );
    v[2].p = D3DXVECTOR4( -0.5f,                      m_dwBackBufferHeight - 0.5f, 1.0f, 1.0f );
    v[3].p = D3DXVECTOR4( m_dwBackBufferWidth - 0.5f, m_dwBackBufferHeight - 0.5f, 1.0f, 1.0f );
    v[0].tu0 = 0.0f;                       v[0].tv0 = 0.0f;
    v[1].tu0 = (float)m_dwBackBufferWidth; v[1].tv0 = 0.0f;
    v[2].tu0 = 0.0f;                       v[2].tv0 = (float)m_dwBackBufferHeight;
    v[3].tu0 = (float)m_dwBackBufferWidth; v[3].tv0 = (float)m_dwBackBufferHeight;
    // tu1 and tv1 are ignored
    // offset final set of texture coords to apply an additional blur
    v[0].tu2 = -fOffsetX;                         v[0].tv2 = -fOffsetY;
    v[1].tu2 = -fOffsetX + (FLOAT)descBlur.Width; v[1].tv2 = -fOffsetY;
    v[2].tu2 = -fOffsetX;                         v[2].tv2 = -fOffsetY + (FLOAT)descBlur.Height;
    v[3].tu2 = -fOffsetX + (FLOAT)descBlur.Width; v[3].tv2 = -fOffsetY + (FLOAT)descBlur.Height;
    v[0].tu3 =  fOffsetX;                         v[0].tv3 =  fOffsetY;
    v[1].tu3 =  fOffsetX + (FLOAT)descBlur.Width; v[1].tv3 =  fOffsetY;
    v[2].tu3 =  fOffsetX;                         v[2].tv3 =  fOffsetY + (FLOAT)descBlur.Height;
    v[3].tu3 =  fOffsetX + (FLOAT)descBlur.Width; v[3].tv3 =  fOffsetY + (FLOAT)descBlur.Height;

    // Set pixel shader
    m_pd3dDevice->SetPixelShader( m_dwFocusLookupPixelShader );

    // Set texture state
    m_pd3dDevice->SetTexture( 0, &ZBufferTexture );
    m_pd3dDevice->SetTexture( 1, m_pTextureFocusRange );
    m_pd3dDevice->SetTexture( 2, m_pBlur );
    m_pd3dDevice->SetTexture( 3, m_pBlur );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 2, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 2, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 3, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 3, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 2, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 2, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 3, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 3, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP );
    
    // Set render state
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, FALSE ); 
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHAREF, 0 );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHAFUNC, D3DCMP_GREATER );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_SRCBLEND, D3DBLEND_ONE );
    m_pd3dDevice->SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
    m_pd3dDevice->SetRenderState( D3DRS_FILLMODE, D3DFILL_SOLID);

    // Render the screen-aligned quadrilateral
    m_pd3dDevice->SetVertexShader( D3DFVF_XYZRHW|D3DFVF_TEX4 );
    m_pd3dDevice->DrawPrimitiveUP( D3DPT_QUADSTRIP, 1, v, sizeof(VERTEX) );

    // Reset render states
    m_pd3dDevice->SetRenderState( D3DRS_ALPHATESTENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );
    m_pd3dDevice->SetPixelShader( NULL );
    m_pd3dDevice->SetTexture( 0, NULL );
    m_pd3dDevice->SetTexture( 1, NULL );
    m_pd3dDevice->SetTexture( 2, NULL );
    m_pd3dDevice->SetTexture( 3, NULL );

    return S_OK;
}
