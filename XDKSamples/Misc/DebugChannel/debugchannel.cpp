//-----------------------------------------------------------------------------
// File: DebugChannel.cpp
//
// Desc: Sample to demonstrate how to communicate between the Xbox and a
//       remote debug console running on the dev machine.
//
//       This app runs on the Xbox and needs the DebugConsole sample to be
//       running on the dev machine.
//
//       See the DebugCmd.cpp file comments for how the API is used.
//
// Hist: 02.05.01 - Initial creation for March XDK release
//       08.21.02 - Revision and code cleanup
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <XBResource.h>
#include <xgraphics.h>
#include "DebugCmd.h"




//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_A_BUTTON,     XBHELP_PLACEMENT_2, L"Display\nframerate\nto console" },
    { XBHELP_B_BUTTON,     XBHELP_PLACEMENT_2, L"Display texture\nto console" },
    { XBHELP_BACK_BUTTON,  XBHELP_PLACEMENT_2, L"Display help" },
    { XBHELP_START_BUTTON, XBHELP_PLACEMENT_1, L"Pause" },
    { XBHELP_MISC_CALLOUT, XBHELP_PLACEMENT_2, L"Note: This sample requires DebugConsole.exe\nto be running on the PC" },
};

#define NUM_HELP_CALLOUTS 5




//-----------------------------------------------------------------------------
// Globally accessed varaibles
//-----------------------------------------------------------------------------
CXBPackedResource       g_xprResource;          // Packed resources for the app
FLOAT                   g_fRadians;             // Radians of rotation

LPDIRECT3DTEXTURE8      g_pTexture;             // Texture for quad
BOOL                    g_bForward;             // TRUE if we should spin CW
FLOAT                   g_fRotationSpeed;       // Rotation speed in radians per second
CHAR                    g_strTextureName[MAX_PATH]; // Filename of texture
D3DLIGHT8               g_Light;                // Light




//-----------------------------------------------------------------------------
// Array of structures to expose varaibles to the debug console's "set" command
//-----------------------------------------------------------------------------
const REMOTE_VARIABLE g_RemoteVariables[] = 
{
    // Var name,  Variable address,   Type,       Handler
    { "bForward", &g_bForward,        SDOS_BOOL,  NULL            },
    { "red",      &g_Light.Diffuse.r, SDOS_FLOAT, RCmdLightChange },
    { "green",    &g_Light.Diffuse.g, SDOS_FLOAT, RCmdLightChange },
    { "blue",     &g_Light.Diffuse.b, SDOS_FLOAT, RCmdLightChange },
};

const DWORD g_dwNumRemoteVariables = (sizeof(g_RemoteVariables)/sizeof(REMOTE_VARIABLE));




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
public:
    CXBFont m_Font;                 // Font class
    CXBHelp m_Help;                 // Help class
    BOOL    m_bDrawHelp;            // Whether to draw help

    HRESULT DrawObject();           // Draws a simple, textured quad

public:
    HRESULT Initialize();
    HRESULT Render();
    HRESULT FrameMove();

    CXBoxSample();
};




//-----------------------------------------------------------------------------
// Name: main()
// Desc: Entry point to the program.
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
// Desc: Constructor for Application class
//-----------------------------------------------------------------------------
CXBoxSample::CXBoxSample()
            :CXBApplication()
{
    m_bDrawHelp         = FALSE;

    g_bForward          = TRUE;
    g_pTexture          = NULL;
    g_strTextureName[0] = 0;
    g_fRadians          = 0.0f;
    g_fRotationSpeed    = 0.0f;
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: Performs whatever initialization is necessary
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    // Create the resources
    if( FAILED( g_xprResource.Create( "Resource.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create a font
    if( FAILED( m_Font.Create( "Font.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Initialize the help system
    if( FAILED( m_Help.Create( "Gamepad.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Set up our view & projection matrices
    D3DXMATRIX matView;
    D3DXMatrixTranslation( &matView, 0.0f, 0.0f, 1.0f );
    m_pd3dDevice->SetTransform( D3DTS_VIEW, &matView );

    D3DXMATRIX matProj;
    D3DXMatrixPerspectiveFovLH( &matProj, D3DX_PI/3, 1.0f, 1.0f, 10.0f );
    m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &matProj );

    // Set up our light
    ZeroMemory( &g_Light, sizeof(D3DLIGHT8) );
    g_Light.Type = D3DLIGHT_DIRECTIONAL;
    g_Light.Diffuse.r = 1.0f;
    g_Light.Diffuse.g = 1.0f;
    g_Light.Diffuse.b = 1.0f;
    g_Light.Diffuse.a = 1.0f;
    g_Light.Direction = D3DXVECTOR3( 0.0f, 0.0f, 1.0f );
    m_pd3dDevice->SetLight( 0, &g_Light );
    m_pd3dDevice->LightEnable( 0, TRUE );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Performs all per-frame calculations to update the application state
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    if( g_bForward )
        g_fRadians = fmodf( g_fRadians - g_fRotationSpeed * m_fElapsedAppTime, 2*D3DX_PI );
    else
        g_fRadians = fmodf( g_fRadians + g_fRotationSpeed * m_fElapsedAppTime, 2*D3DX_PI );

    D3DXMATRIX matWorld;
    D3DXMatrixIdentity( &matWorld );
    D3DXMatrixRotationZ( &matWorld, g_fRadians );
    m_pd3dDevice->SetTransform( D3DTS_WORLD, &matWorld );

    // Toggle help
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_BACK ) 
    {
        m_bDrawHelp = !m_bDrawHelp;
    }

    // Handle requests for debug output
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
    {
        DebugConsolePrintf( "Framerate is %0.02f fps\n", m_fFPS );
    }
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_B]  )
    {
        if( g_strTextureName && g_strTextureName[0] )
            DebugConsolePrintf( "Current texture is %s\n", g_strTextureName );
        else
            DebugConsolePrintf( "No texture is currently set\n" );
    }

    // Process any pending commands from the remote debug console
    DebugConsoleHandleCommands();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: DrawObject()
// Desc: Draws a simple, textured quad.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::DrawObject()
{
    // Vertex format for rendering a simple, textures polygon
    struct CUSTOMVERTEX
    {
        D3DXVECTOR3 p;          // Position
        D3DXVECTOR3 n;          // normal
        D3DCOLOR    diffuse;    // Diffuse color
        FLOAT       tu, tv;     // Texture coordinates
    };

    CUSTOMVERTEX v[4];

    v[0].p  = D3DXVECTOR3( -0.5f, -0.5f, 1.0f );
    v[0].n  = D3DXVECTOR3( 0.0f, 0.0f, -1.0f );
    v[0].diffuse = 0xffffffff;
    v[0].tu = 0.0f; 
    v[0].tv = 1.0f;

    v[1].p  = D3DXVECTOR3( 0.5f, -0.5f, 1.0f );
    v[1].n  = D3DXVECTOR3( 0.0f, 0.0f, -1.0f );
    v[1].diffuse = 0xffffffff;
    v[1].tu = 1.0f; 
    v[1].tv = 1.0f;

    v[2].p  = D3DXVECTOR3( 0.5f, 0.5f, 1.0f );
    v[2].n  = D3DXVECTOR3( 0.0f, 0.0f, -1.0f );
    v[2].diffuse = 0xffffffff;
    v[2].tu = 1.0f; 
    v[2].tv = 0.0f;

    v[3].p  = D3DXVECTOR3( -0.5f, 0.5f, 1.0f );
    v[3].n  = D3DXVECTOR3( 0.0f, 0.0f, -1.0f );
    v[3].diffuse = 0xffffffff;
    v[3].tu = 0.0f; 
    v[3].tv = 0.0f;

    // Render the quad
    m_pd3dDevice->SetVertexShader( D3DFVF_XYZ|D3DFVF_NORMAL|D3DFVF_DIFFUSE|D3DFVF_TEX1 );
    m_pd3dDevice->DrawVerticesUP( D3DPT_QUADLIST, 4, v, sizeof(CUSTOMVERTEX) );

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
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,
                         0x00000080, 1.0f, 0L );

    // Set rendering state
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE,  TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );
    m_pd3dDevice->SetRenderState( D3DRS_AMBIENT,  0xffffffff );
    m_pd3dDevice->SetRenderState( D3DRS_LIGHTING, TRUE );

    // Set our texture stages up
    if( g_pTexture )
    {
        m_pd3dDevice->SetTexture( 0, g_pTexture );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_MODULATE );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_TEXTURE );
    }
    else
    {
        m_pd3dDevice->SetTexture( 0, NULL );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1 );
        m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_DIFFUSE );
    }

    // Draw the quad
    DrawObject();

    // Show title, frame rate, and help
    if( m_bDrawHelp )
    {
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    }
    else
    {
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 48, 36, 0xffffffff,  L"DebugChannel" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 592, 38, 0xffffff00, m_strFrameRate, XBFONT_RIGHT );
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: RCmdTexture()
// Desc: Callback handler for the remote "texture" command. Sets the new
//       texture to be used
//-----------------------------------------------------------------------------
VOID RCmdTexture( int argc, char* argv[] )
{
    // Check our arguments
    if( argc < 2 )
    {
        DebugConsolePrintf("ERROR: Need to specify a texture resource name\n");
        DebugConsolePrintf("Avaialable textures are:\n");

        DWORD       dwNumResourceTags;
        XBRESOURCE* pResourceTags;
        g_xprResource.GetResourceTags( &dwNumResourceTags, &pResourceTags );

        for( DWORD i=0; i<dwNumResourceTags; i++ )
            DebugConsolePrintf( "   %s\n", pResourceTags[i] );

        return;
    }

    // Try to grab the new texture
    lstrcpyA( g_strTextureName, argv[1] );

    g_pTexture = g_xprResource.GetTexture( g_strTextureName );
    
    if( NULL == g_pTexture )
    {
        DebugConsolePrintf( "ERROR: Couldn't find %s", g_strTextureName );
        return;
    }
}




//-----------------------------------------------------------------------------
// Name: RCmdSpin()
// Desc: Callback handler for the remote "spin" command. Sets the new spin
//       velocity, limited to 2pi in either direction
//-----------------------------------------------------------------------------
VOID RCmdSpin( int argc, char* argv[] )
{
    // Check our arguments
    if( argc < 2 )
    {
        DebugConsolePrintf( "ERROR: Need to specify a velocity\n" );
        return;
    }

    FLOAT fVelocity = (FLOAT)atof( argv[1] );

    if( fabs( fVelocity ) > D3DX_PI * 2 )
    {
        DebugConsolePrintf( "ERROR: Velocity should be between +/- 2 * pi\n" );
        return;
    }

    // Set our state
    g_fRotationSpeed = (FLOAT)fabs( fVelocity );
    g_bForward       = ( fVelocity > 0 );
}




//-----------------------------------------------------------------------------
// Name: RCmdLightChange()
// Desc: Called after changing one of our lighting values, so that we can
//       reset the light
//-----------------------------------------------------------------------------
VOID RCmdLightChange( void* pAddr )
{
    g_pd3dDevice->SetLight( 0, &g_Light );
}

