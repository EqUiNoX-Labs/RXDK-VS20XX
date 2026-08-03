//-----------------------------------------------------------------------------
// File: UIXKeyboardFeature.h
//
// Desc: Implements the UIX keyboard feature.
//
//       When an app wants to display the UIX Keyboard, they make the following
//       call:
//          ILiveEngine::StartFeature( UIX_KEYBOARD_FEATURE, &m_UIXKeyboardData );
//
//       The UIX_KEYBOARD_DATA structure is used for communication between the
//       app and the UIX Keyboard.

//       The first member is an ITitleFontRenderer, supplied by the app, so the
//       keyboard can render text. Note that the UIX UI plugin object only has
//       has access to one font, which unfortunately would mean the UIX keyboard
//       would have to share the same font as ALL other UIX features. Since this
//       looks horrible, this hook allows the app to specify a font more
//       suitable for rendering the keyboard.
//
//       The remaining members are to describe a persistent text buffer so the
//       app can supply an initial text string to edit, and retrieve the editted
//       contents of that string.
//
//       Outside of this structure, the UIX Keybaord feature behaves like any
//       other UIX feature.
//
// Hist: 11.25.03 - New for December 2003 XDK release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef UIX_KEYBOARD_FEATURE_H
#define UIX_KEYBOARD_FEATURE_H

#include <xtl.h>
#include <uix.h>




//-----------------------------------------------------------------------------
// UIX Feature support
//-----------------------------------------------------------------------------
extern const UIX_FEATURE UIX_KEYBOARD_FEATURE;

// Note: Make sure that exit codes for custom features do not overlap with exit
// codes for the built-in UIX features.
#define UIX_EXIT_KEYBOARD_OK            50
#define UIX_EXIT_KEYBOARD_FAIL          51

#define UIX_KEYBOARD_FEATUREMSG_DISPLAYHELP 100



//-----------------------------------------------------------------------------
// Custom creation parameters for the UIX Keyboard feature
//-----------------------------------------------------------------------------
struct UIX_KEYBOARD_DATA
{
    ITitleFontRenderer* pKeyboardFont;     // A font used to render the keyboard
    WCHAR*              strBuffer;         // A pre-allocated buffer to store input
    DWORD               dwBufferSize;      // Size of the pre-allocated buffer
    DWORD               dwCursorPos;       // Current cursor position in the buffer
    DWORD               dwLockInputToPort; // Which controller port to use
};



// Privately-accessed classes
class CUIXKeyboardScreen;
class CUIXHelpScreen;




//-----------------------------------------------------------------------------
// Name: class CUIXKeyboardFeature
// Desc: Custom UIX feature to implement a virtual keyboard
//-----------------------------------------------------------------------------
class CUIXKeyboardFeature : public IUIXFeature
{
    UIX_FEATURE_CONTEXT* m_pContext;

    CUIXKeyboardScreen*  m_pKeyboardScreen;
    CUIXHelpScreen*      m_pUIXHelpScreen;

public:
    // IUIXFeature methods
    HRESULT __stdcall CreateFeature();
    VOID    __stdcall DestroyFeature();
    HRESULT __stdcall ActivateFeature( const VOID* pParams );
    VOID    __stdcall HibernateFeature();
    VOID    __stdcall PumpTasks();
    VOID    __stdcall Reset();
    HRESULT __stdcall ReceiveMessage( UIX_FEATUREMSG_TYPE Msg, const VOID* pParam,
                                      VOID* pResult, IUIXFeature* pFromFeature );
    HRESULT __stdcall GetCurrentScreen( UIX_SCREEN* pScreen );
    HRESULT __stdcall GetType( UIX_FEATURE_TYPE* pType );
    HRESULT __stdcall GetFeatureInterface( const VOID* pParam, VOID** ppInterface );
    HRESULT __stdcall SetContext( UIX_FEATURE_CONTEXT* pContext );
};




#endif // UIX_KEYBOARD_FEATURE_H
