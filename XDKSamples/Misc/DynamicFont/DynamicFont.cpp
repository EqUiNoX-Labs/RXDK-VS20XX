//-----------------------------------------------------------------------------
// File: DynamicFont.cpp
//
// Desc: Shows how to dynamically create a texture-based font. An app would
//       potentially need to re-create it's fonts after downloading new text
//       strings. This is a solution that uses the flexibility of the XFont
//       API (which allows any .ttf font file to be used) with the performance
//       of a texture-based font like the CXBFont base class provides.
//
// Hist: 09.08.03 - New for November 2003 XDK release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#define XFONT_TRUETYPE
#include <XBApp.h>
#include <XBFont.h>
#include <XBMesh.h>
#include <XBHelp.h>
#include <xgraphics.h>
#include <xfont.h>
#include "XBDynamicFont.h"


// Global space to hold list of valid glyphs, used to create a font
static BYTE g_ValidGlyphs[65536] = { 0 };

// Some localized text files for the app to test out
static CHAR* g_LocalizedTextFilenames[] =
{
    "d:\\media\\Strings_Chinese.inf",
    "d:\\media\\Strings_German.inf",
    "d:\\media\\Strings_Spanish.inf",
    "d:\\media\\Strings_Japanese.inf",
    "d:\\media\\Strings_Korean.inf",
};



//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_A_BUTTON,     XBHELP_PLACEMENT_2, L"Load new\ntext file" },
    { XBHELP_BACK_BUTTON,  XBHELP_PLACEMENT_2, L"Display\nhelp" },
};

#define NUM_HELP_CALLOUTS (sizeof(g_HelpCallouts)/sizeof(g_HelpCallouts[0]))




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
    CXBFont                 m_Font;
    CXBHelp                 m_Help;
    BOOL                    m_bDrawHelp;

    CXBDynamicFont          m_DynamicFont;

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
    // Allow unlimited frame rate
    m_d3dpp.FullScreen_PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;

    m_bDrawHelp         = FALSE;
}




//-----------------------------------------------------------------------------
// Name: Initialize()
// Desc: 
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    // Create a static font
    if( FAILED( m_Font.Create( "Font.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create the help
    if( FAILED( m_Help.Create( "Gamepad.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create a dynamically-generated font
    {
        // Create an XFont
        XFONT* pXFont;
        if( FAILED( XFONT_OpenTrueTypeFont( L"D:\\media\\arialuni.ttf",
                                            16 * 1024, &pXFont ) ) )
            return E_FAIL;
        pXFont->SetTextHeight( 20 ); // 16-point font
        pXFont->SetTextStyle( XFONT_BOLD );
        pXFont->SetTextAntialiasLevel( 4 );

        // Build a list a valid glyphs. An app would do this based on a list
        // of localized strings
        ZeroMemory( g_ValidGlyphs, 65536 );
        for( DWORD c=32; c<=127; c++ )
            g_ValidGlyphs[c] = 1;

        // Create the dynamic font
        if( FAILED( m_DynamicFont.Create( pXFont, g_ValidGlyphs, 
                                          256, 256, TRUE, TRUE ) ) )
            return XBAPPERR_MEDIANOTFOUND;

        // Done with the XFont
        pXFont->Release();
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    // Toggle help
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_BACK )
        m_bDrawHelp = !m_bDrawHelp;

    // Create a dynamically generated font
    if( m_DefaultGamepad.bPressedAnalogButtons[XINPUT_GAMEPAD_A] )
    {
        // Select a filename
        static DWORD dwFile = 0x0000ffff;
        if( ++dwFile >= sizeof(g_LocalizedTextFilenames)/sizeof(CHAR*) )
            dwFile = 0;

        // Load the localized strings
        VOID* pFileData;
        DWORD dwFileSize;
        if( SUCCEEDED( XBUtil_LoadFile( g_LocalizedTextFilenames[dwFile],
                                        &pFileData, &dwFileSize ) ) )
        {
            // Build a list a valid glyphs based on a list of localized strings
            WCHAR* pLocalizationData = (WCHAR*)pFileData;
            DWORD  dwNumChars        = dwFileSize / sizeof(WCHAR);

            ZeroMemory( g_ValidGlyphs, 65536 );
            for( DWORD i=1; i<dwNumChars-1; i++ )
                if( pLocalizationData[i] < 0x8000 )
                    g_ValidGlyphs[pLocalizationData[i]] = 1;

            // Create an XFont
            XFONT* pXFont;
            if( FAILED( XFONT_OpenTrueTypeFont( L"D:\\media\\arialuni.ttf",
                                                16 * 1024, &pXFont ) ) )
                return E_FAIL;
            pXFont->SetTextHeight( 20 ); // 16-point font
            pXFont->SetTextStyle( XFONT_BOLD );
            pXFont->SetTextAntialiasLevel( 4 );

            // Create the dynamic font
            if( FAILED( m_DynamicFont.Create( pXFont, g_ValidGlyphs, 
                                              512, 256, TRUE, TRUE ) ) )
                return XBAPPERR_MEDIANOTFOUND;

            pXFont->Release();
        }
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Sets up render states, clears the viewport, and renders the scene.
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Render()
{
    // Draw a gradient filled background
    RenderGradientBackground( 0xff0000ff, 0xff00ffff );

    if( FALSE == m_bDrawHelp )
    {
        // Draw a message using the dynamically-generated font
        m_Font.DrawText( 60, 100, 0xffffffff, L"The dynamic font contains the following glyphs:" );

        // Render a list of all available glyphs using the dynamic font
        m_DynamicFont.Begin();
        {
            FLOAT sx = 100;
            FLOAT sy = 125;
            for( DWORD i=0; i<65536; i++ )
            {
                if( g_ValidGlyphs[i] )
                {
                    WCHAR str[2] = { (WCHAR)i, 0 };
                    m_DynamicFont.DrawText( sx, sy, 0xffffffff, str );

                    sx += 25;
                    if( sx > 520 )
                    {
                        sx = 100;
                        sy += 25;
                    }
                }
            }
        }
        m_DynamicFont.End();
    }

    // Show title, frame rate, and help
    if( m_bDrawHelp )
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    else
    {
        m_Font.Begin();
        m_Font.SetScaleFactors( 1.2f, 1.2f );
        m_Font.DrawText( 48, 36, 0xffffffff,  L"DynamicFont" );
        m_Font.SetScaleFactors( 1.0f, 1.0f );
        m_Font.DrawText( 592, 38, 0xffffff00, m_strFrameRate, XBFONT_RIGHT );
        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}




