//-----------------------------------------------------------------------------
// File: UIXKeyboardScreen.h
//
// Desc: Implements the UIX keyboard screen.
//
//       This screen is used by the UIX keybaord feature to create, render, and
//       handle input for the UIX keyboard.
//
//       The primary purpose of this screen is to serve as a middleman between
//       UIX and the underlying (non-UIX) keyboard object. The UIX screen is
//       responsible for reading layout information and localized strings from
//       the UIX skin file. The information is then past on to the underlying
//       CXBKeyboard object, which is otherwise completely unaware that UIX
//       exists.
//
//       Please note that special care was given to keep the CXBKeyboard class
//       completely separate from UIX. The UIX Keyboard screen shows how to
//       interface UIX with non-UIX objects. While the underlying CXBKeyboard
//       class does all the work related to a keyboard, the UIX keyboard screen
//       class does all the interface work, especially with the skin file and
//       user input.
//
// Hist: 11.25.03 - New for December 2003 XDK release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef UIX_KEYBOARD_SCREEN_H
#define UIX_KEYBOARD_SCREEN_H

#include <xtl.h>
#include <uix.h>




// Forward class references
class CUIXKeyboardFont;
class CXBKeyboard;

struct UIX_KEYBOARD_DATA;


//-----------------------------------------------------------------------------
// Name: class CUIXKeyboardScreen
// Desc: 
//-----------------------------------------------------------------------------
class CUIXKeyboardScreen : public IUIXScreen
{
    CUIXKeyboardFont*     m_pKeyboardFont;     // A font for the keyboard to use
    CXBKeyboard*          m_pKeyboard;         // The underlying keyboard object

    UIX_KEYBOARD_DATA*    m_pKeyboardData;

    UIX_FEATURE_CONTEXT*  m_pContext;          // UIX context interface
    IUIXFeature*          m_pFeature;          // UIX feature interface
    UIX_SKIN_LAYOUT_INFO* m_pKeyboardLayout;   // Layout info for the keybaord

    UIX_SCREEN            m_KeyboardScreenObj; // UIX screens

    DWORD                 m_BackgroundObj;     // UIX objects
    DWORD                 m_OutputTextBoxObj;
    DWORD                 m_SelectTextBoxObj;
    DWORD                 m_HelpTextBoxObj;
    DWORD                 m_BackTextBoxObj;

private:
    // Helper to get a string from the IPluginSupport object
    WCHAR* GetString( DWORD dwStringResID )
    {
        const WCHAR* str = NULL;
        m_pContext->pPluginSupport->GetString( dwStringResID, &str );
        return (WCHAR*)str;
    }

public:
    // IUIXScreen methods
    HRESULT __stdcall CreateScreen();
    VOID    __stdcall Output();
    VOID    __stdcall Input( DWORD Port, UIX_INPUT_TYPE InputKey );
    HRESULT __stdcall ReceiveMessage( UIX_SCREENMSG_TYPE Msg, const VOID* pParam ) { return S_OK; }

    UIX_SCREEN GetScreenObject() { return m_KeyboardScreenObj; }

    CUIXKeyboardScreen( IUIXFeature* pFeature, UIX_FEATURE_CONTEXT* pContext,
                        UIX_KEYBOARD_DATA* pParams );
    ~CUIXKeyboardScreen();
};




#endif // UIX_KEYBOARD_SCREEN_H
