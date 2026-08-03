//-----------------------------------------------------------------------------
// File: XBHelp.h
//
// Desc: Support class for rendering a help image, which is an image of an Xbox
//       game pad, with labelled call outs to each of the game pad's controls.
//
// Hist: 11.01.00 - New for November XDK release
//       12.15.00 - Changes for December XDK release
//       03.06.01 - Changes for April XDK release
//       04.15.01 - Using packed resources for May XDK
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef XBHELP_H
#define XBHELP_H
#include "XBFont.h"
#include "XBResource.h"
#include "XBUtil.h"




//-----------------------------------------------------------------------------
// Name: struct XBHELP_CALLOUT
// Desc: Structure for call out information, used to label controls when
//       rendering an image of an Xbox game pad. An app will define an array of
//       of these, one for each game pad control used.
//-----------------------------------------------------------------------------
struct XBHELP_CALLOUT
{
    WORD     wControl;    // An index to identify a control, as enum'ed below
    WORD     wPlacement;  // An offset to pick from one of the possible placements
    WCHAR*   strText;     // Text to draw when rendering this call out
};




//-----------------------------------------------------------------------------
// Name: class CXBHelp
// Desc: Class for rendering a help image of a game pad with labelled call outs.
//-----------------------------------------------------------------------------
class CXBHelp
{
    CXBPackedResource m_xprResource;
    CXBFont*          m_pFont;
    D3DTexture*       m_pGamepadTexture;
    D3DTexture*       m_pLineTexture;
    D3DVertexBuffer*  m_pVB;
    FLOAT             m_fXScale;
    FLOAT             m_fYScale;

public:
    // Constructor/destructor
    CXBHelp();
    ~CXBHelp();

    // Functions to create and destroy the internal objects
    HRESULT Create( CHAR* pResource );
    HRESULT Destroy();

    // Renders the help screen (using builtin, or caller-supplied, font)
    HRESULT Render( XBHELP_CALLOUT* tags, DWORD dwNumCallouts );
    HRESULT Render( CXBFont* pFont, XBHELP_CALLOUT* tags, DWORD dwNumCallouts );
};




//-----------------------------------------------------------------------------
// A bunch of constants used to identify call out positions
//-----------------------------------------------------------------------------
enum
{   
    XBHELP_LEFTSTICK,
    XBHELP_RIGHTSTICK,
    XBHELP_DPAD,
    XBHELP_BACK_BUTTON,
    XBHELP_START_BUTTON,
    XBHELP_X_BUTTON,
    XBHELP_Y_BUTTON,
    XBHELP_A_BUTTON,
    XBHELP_B_BUTTON,
    XBHELP_WHITE_BUTTON,
    XBHELP_BLACK_BUTTON,
    XBHELP_LEFT_BUTTON,
    XBHELP_RIGHT_BUTTON,
    XBHELP_MISC_CALLOUT,
    XBHELP_MISC_CALLOUT_2,
    XBHELP_MISC_CALLOUT_3,
};




//-----------------------------------------------------------------------------
// Placement options for each call out, used as an offset into the enumerated
// list above.
//-----------------------------------------------------------------------------
#define XBHELP_PLACEMENT_1      0   // Call out has one line of text
#define XBHELP_PLACEMENT_2      1   // Call out has two lines of text




#endif
