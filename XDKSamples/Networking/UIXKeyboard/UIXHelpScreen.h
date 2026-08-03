//-----------------------------------------------------------------------------
// File: UIXHelpScreen.h
//
// Desc: A UIX screen for rendering text.
//
//       To use this object, simply construct a new CUIXHelpScreen object with
//       the constructor, as in:
//          m_pUIXHelpScreen = new CUIXHelpScreen( pFeature, pContext );
//
//       This will load a background image and layouts for all the controls
//       from the skin file. 
//
//       To populate the help strings, simply pass an array of strings to the
//       object as in:
//           m_pUIXHelpScreen->SetStringTable( strStrings[UIXHELP_STR_MAX] );
//
//       The update and rendering of the help screen thereafter behaves like
//       any other UIX screen.
//
// Hist: 11.25.03 - New for December 2003 XDK release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef UIX_HELP_SCREEN_H
#define UIX_HELP_SCREEN_H

#include <xtl.h>
#include <uix.h>




//-----------------------------------------------------------------------------
// Name: struct UIXHELP_CALLOUT
// Desc: Structure for call out information, used to label controls when
//       rendering an image of an Xbox game pad. An app will define an array of
//       of these, one for each game pad control used.
//-----------------------------------------------------------------------------
struct UIXHELP_CALLOUT
{
    WORD     wControl;    // An index to identify a control, as enum'ed below
    WORD     wPlacement;  // An offset to pick from one of the possible placements
    WCHAR*   strText;     // Text to draw when rendering this call out
};




//-----------------------------------------------------------------------------
// A bunch of constants used to identify call out positions
//-----------------------------------------------------------------------------
enum
{   
    UIXHELP_STR_LEFT_THUMBSTICK,
    UIXHELP_STR_RIGHT_THUMBSTICK,
    UIXHELP_STR_DPAD,
    UIXHELP_STR_BACK_BUTTON,
    UIXHELP_STR_START_BUTTON,
    UIXHELP_STR_A_BUTTON,
    UIXHELP_STR_B_BUTTON,
    UIXHELP_STR_X_BUTTON,
    UIXHELP_STR_Y_BUTTON,
    UIXHELP_STR_WHITE_BUTTON,
    UIXHELP_STR_BLACK_BUTTON,
    UIXHELP_STR_LEFT_TRIGGER,
    UIXHELP_STR_RIGHT_TRIGGER,
    UIXHELP_STR_LEFT_MISC_CALLOUT,
    UIXHELP_STR_CENTER_MISC_CALLOUT,
    UIXHELP_STR_RIGHT_MISC_CALLOUT,
    UIXHELP_STR_MAX,
};




//-----------------------------------------------------------------------------
// Name: class CUIXHelpScreen
// Desc: 
//-----------------------------------------------------------------------------
class CUIXHelpScreen : public IUIXScreen
{
    UIX_FEATURE_CONTEXT*  m_pContext;

    UIX_SCREEN            m_ScreenObj;
    DWORD                 m_BackgroundObj;
    DWORD                 m_TextObjArray[UIXHELP_STR_MAX];

    WCHAR*                m_strStringTable[UIXHELP_STR_MAX];
    UIX_SKIN_LAYOUT_INFO* m_pCallouts[UIXHELP_STR_MAX];
    D3DTexture*           m_pCalloutTexture;

public:
    VOID    SetStringTable( WCHAR** strStringTable );

    // IUIXScreen methods
    HRESULT __stdcall CreateScreen();
    VOID    __stdcall Output();
    VOID    __stdcall Input( DWORD Port, UIX_INPUT_TYPE InputKey );
    HRESULT __stdcall ReceiveMessage( UIX_SCREENMSG_TYPE Msg, const VOID* pParam ) { return S_OK; }

    UIX_SCREEN GetScreenObject() { return m_ScreenObj; }

    CUIXHelpScreen( UIX_FEATURE_CONTEXT* pContext  );
    ~CUIXHelpScreen();
};




#endif // UIX_HELP_SCREEN_H
