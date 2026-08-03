//-----------------------------------------------------------------------------
// File: CrashDump.cpp
//
// Desc: This application generates a crash dump. It's useful for learning
//       how to debug crash dumps.
//
// Hist: 7.27.02 - Created
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include <XBApp.h>
#include <XBFont.h>
#include <XBHelp.h>
#include <xgraphics.h>


//-----------------------------------------------------------------------------
// Callouts for labelling the gamepad on the help screen
//-----------------------------------------------------------------------------
XBHELP_CALLOUT g_HelpCallouts[] = 
{
    { XBHELP_A_BUTTON,  XBHELP_PLACEMENT_2, L"Generate\nCrash Dump" },
    { XBHELP_BACK_BUTTON,  XBHELP_PLACEMENT_2, L"Display help" },
};

#define NUM_HELP_CALLOUTS ( sizeof( g_HelpCallouts ) / sizeof( g_HelpCallouts[0] ) )




//-----------------------------------------------------------------------------
// Name: class CXBoxSample
// Desc: Main class to run this application. Most functionality is inherited
//       from the CXBApplication base class.
//-----------------------------------------------------------------------------
class CXBoxSample : public CXBApplication
{
public:
    CXBoxSample();

    virtual HRESULT Initialize();
    virtual HRESULT Render();
    virtual HRESULT FrameMove();
    virtual VOID    VerticalBlankCallback(D3DVBLANKDATA* pData);

    CXBFont     m_Font;             // Font object
    CXBHelp     m_Help;             // Help object
    BOOL        m_bDrawHelp;        // TRUE to draw help screen

    LONG        m_lCrash;           // non-zero to generate a crash dump
    LONG        m_lVerticalBlankCount;  // Keep track of vertical blanks

    static CXBoxSample* gSingleton; // Allows the LowLevelVerticalBlankCallback function to call back to the CXboxSample object
};

CXBoxSample* CXBoxSample::gSingleton;    

//-----------------------------------------------------------------------------
// Name: LowLevelVerticalBlankCallback()
// Desc: receives vertical blank callback from D3D, routes it to the CXboxSample.
//-----------------------------------------------------------------------------
VOID __cdecl LowLevelVerticalBlankCallback(D3DVBLANKDATA* pData)
{
    CXBoxSample::gSingleton->VerticalBlankCallback(pData);
}




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
// Name: CXBoxSample (constructor)
// Desc: Constructor for CXBoxSample class
//-----------------------------------------------------------------------------
CXBoxSample::CXBoxSample() 
            :CXBApplication()
{
    m_bDrawHelp = FALSE;

    // The vertical blank interrupt callback has not been installed yet,
    // so it's safe to access these variables directly.
    m_lCrash = 0;
    m_lVerticalBlankCount = 0;
    gSingleton = this;
}




//-----------------------------------------------------------------------------
// Name: Initialize
// Desc: Performs initialization
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Initialize()
{
    // Create a font
    if( FAILED( m_Font.Create( "Font.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Create help
    if( FAILED( m_Help.Create( "Gamepad.xpr" ) ) )
        return XBAPPERR_MEDIANOTFOUND;

    // Register the vertical blank callback
    m_pd3dDevice->SetVerticalBlankCallback(&LowLevelVerticalBlankCallback);

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FrameMove
// Desc: Performs per-frame updates
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::FrameMove()
{
    // Toggle help
    if( m_DefaultGamepad.wPressedButtons & XINPUT_GAMEPAD_BACK ) 
    {
        m_bDrawHelp = !m_bDrawHelp;
    }
    
    // Set Crash value
    if( m_DefaultGamepad.bAnalogButtons[XINPUT_GAMEPAD_A] > 0x00 ) 
    {
        LONG oldValue = m_lCrash;
        LONG newValue = 1;
        InterlockedCompareExchange(&m_lCrash, newValue, oldValue);
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Render
// Desc: Renders the scene
//-----------------------------------------------------------------------------
HRESULT CXBoxSample::Render()
{
    // Draw a gradient filled background
    RenderGradientBackground( 0xff404040, 0xff404080 );

    // Show title, frame rate, vertical blank count, and help
    if( m_bDrawHelp )
        m_Help.Render( &m_Font, g_HelpCallouts, NUM_HELP_CALLOUTS );
    else
    {
        m_Font.Begin();
        m_Font.DrawText(  64, 50, 0xffffffff, L"CrashDump" );
        m_Font.DrawText( 450, 50, 0xffffff00, m_strFrameRate );

        wchar_t buf[40];
        DWORD vblCount = (DWORD) m_lVerticalBlankCount;
        wsprintfW(buf, L"Vertical Blank Count: %lu", vblCount);
        m_Font.DrawText(  64, 75, 0xffffffff, buf );

        m_Font.DrawText(  64, 100, 0xffffffff, L"Press A to generate a crash dump" );

        m_Font.End();
    }

    // Present the scene
    m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: VerticalBlankCallback
// Desc: Performs vertical blank processing
//-----------------------------------------------------------------------------
VOID CXBoxSample::VerticalBlankCallback(D3DVBLANKDATA* pData)
{
    LONG oldVBL = m_lVerticalBlankCount;
    LONG newVBL = (LONG) pData->VBlank;
    InterlockedCompareExchange(&m_lVerticalBlankCount, newVBL, oldVBL);

    LONG crash = m_lCrash;
    if(crash)
    {
        DWORD* lpdWord = (DWORD*) (crash-1);
        *lpdWord = 0;   // Crash (assignment to a null pointer)
        m_lCrash = *lpdWord; // This assignment makes sure the compiler doesn't optimize away the previous lines of code.
    }
}

