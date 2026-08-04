//-----------------------------------------------------------------------------
// File: UIXKeyboardScreen.cpp
//
// Desc: Implements the UIX keyboard screen. See header file for details.
//
// Hist: 11.25.03 - New for December 2003 XDK release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "UIXCustomUIPlugin.h"
#include "UIXKeyboardFeature.h"
#include "UIXKeyboardScreen.h"
#include "UIXHelpScreen.h"
#include "XBKeyboard.h"
#include "sk_res.h"




//-----------------------------------------------------------------------------
// Timer functions for the blinking caret
//-----------------------------------------------------------------------------
__forceinline __int64 GetMachineTime()          { __asm rdtsc }
__forceinline __int64 GetTimeInMicroSeconds()   { return GetMachineTime()*3/2200;}
__forceinline DOUBLE  GetTimeInSeconds()        { return GetTimeInMicroSeconds() / 1000000.0;}




//-----------------------------------------------------------------------------
// Name: class CUIXKeyboardFont
// Desc: CXBKeyboardFont-dreived class for the keyboard to render text
//-----------------------------------------------------------------------------
class CUIXKeyboardFont : public CXBKeyboardFont
{
    ITitleFontRenderer* m_pFont;

public:
    CUIXKeyboardFont( ITitleFontRenderer* pFont )
    {
        m_pFont = pFont;
    }

    VOID DrawCenteredText( DWORD x, DWORD y, DWORD Height, 
                           DWORD Color, WCHAR* strText )
    {
        DWORD dwWidth, dwHeight;
        m_pFont->SetColor( Color );
        m_pFont->SetHeight( Height );
        m_pFont->GetTextSize( strText, &dwWidth, &dwHeight );
        m_pFont->DrawText( strText, x-dwWidth/2, y-dwHeight/2, 0xffffffff );
    }
};




//-----------------------------------------------------------------------------
// Name: CUIXKeyboardScreen()
// Desc: 
//-----------------------------------------------------------------------------
CUIXKeyboardScreen::CUIXKeyboardScreen( IUIXFeature* pFeature, 
                                        UIX_FEATURE_CONTEXT* pContext,
                                        UIX_KEYBOARD_DATA* pParams )
{
    // Store the feature and context
    m_pFeature = pFeature;
    m_pContext = pContext;

    // Set up variables for the text buffer
    m_pKeyboardData = pParams;

    // Create the UIX keyboard screen
    m_pContext->pEngineInternal->CreateScreen( &m_KeyboardScreenObj, SECTION_KEYBOARD_SCREEN, this );

    // Save convenient access to the keyboard layout info
    m_pContext->pPluginSupport->GetLayout( SECTION_KEYBOARD_SCREEN, KEYBOARD_SCREEN_KEYBOARD, &m_pKeyboardLayout );

    // Get the texture for the keyboard keys
    D3DTexture* pKeyTexture = NULL;
    m_pContext->pPluginSupport->GetImage( IMG_KEYBOARD_KEY, &pKeyTexture );

    // Get colors from layout info for the various key states
    UIX_SKIN_LAYOUT_INFO* pNormalKeyLayout;
    UIX_SKIN_LAYOUT_INFO* pPressedKeyLayout;
    UIX_SKIN_LAYOUT_INFO* pDisabledKeyLayout;
    m_pContext->pPluginSupport->GetLayout( SECTION_KEYBOARD_SCREEN, KEYBOARD_SCREEN_NORMALKEY,   &pNormalKeyLayout );
    m_pContext->pPluginSupport->GetLayout( SECTION_KEYBOARD_SCREEN, KEYBOARD_SCREEN_PRESSEDKEY,  &pPressedKeyLayout );
    m_pContext->pPluginSupport->GetLayout( SECTION_KEYBOARD_SCREEN, KEYBOARD_SCREEN_DISABLEDKEY, &pDisabledKeyLayout );

    // Create a color table for the various key states
    static DWORD dwKeyboardColorTable[XBKEYBOARD_COLOR_MAX];
    dwKeyboardColorTable[XBKEYBOARD_COLOR_NORMAL_KEY]             = pNormalKeyLayout->BackColor;
    dwKeyboardColorTable[XBKEYBOARD_COLOR_NORMAL_TEXT]            = pNormalKeyLayout->TextColor;
    dwKeyboardColorTable[XBKEYBOARD_COLOR_NORMAL_KEY_SELECTED]    = pNormalKeyLayout->SelectionBackColor;
    dwKeyboardColorTable[XBKEYBOARD_COLOR_NORMAL_TEXT_SELECTED]   = pNormalKeyLayout->HighlightedTextColor;
    dwKeyboardColorTable[XBKEYBOARD_COLOR_PRESSED_KEY]            = pPressedKeyLayout->BackColor;
    dwKeyboardColorTable[XBKEYBOARD_COLOR_PRESSED_TEXT]           = pPressedKeyLayout->TextColor;
    dwKeyboardColorTable[XBKEYBOARD_COLOR_PRESSED_KEY_SELECTED]   = pPressedKeyLayout->SelectionBackColor;
    dwKeyboardColorTable[XBKEYBOARD_COLOR_PRESSED_TEXT_SELECTED]  = pPressedKeyLayout->HighlightedTextColor;
    dwKeyboardColorTable[XBKEYBOARD_COLOR_DISABLED_KEY]           = pDisabledKeyLayout->BackColor;
    dwKeyboardColorTable[XBKEYBOARD_COLOR_DISABLED_TEXT]          = pDisabledKeyLayout->TextColor;
    dwKeyboardColorTable[XBKEYBOARD_COLOR_DISABLED_KEY_SELECTED]  = pDisabledKeyLayout->SelectionBackColor;
    dwKeyboardColorTable[XBKEYBOARD_COLOR_DISABLED_TEXT_SELECTED] = pDisabledKeyLayout->HighlightedTextColor;

    // Create a string table for the keys' labels
    static WCHAR* strKeyboardStringTable[XBKEYBOARD_STR_MAX];
    strKeyboardStringTable[XBKEYBOARD_STR_KEY_SPACE]      = GetString( STR_KEYBOARD_KEY_SPACE );
    strKeyboardStringTable[XBKEYBOARD_STR_KEY_BACKSPACE]  = GetString( STR_KEYBOARD_KEY_BACKSPACE );
    strKeyboardStringTable[XBKEYBOARD_STR_KEY_ARROWLEFT]  = GetString( STR_KEYBOARD_KEY_ARROWLEFT );
    strKeyboardStringTable[XBKEYBOARD_STR_KEY_ARROWRIGHT] = GetString( STR_KEYBOARD_KEY_ARROWRIGHT );
    strKeyboardStringTable[XBKEYBOARD_STR_KEY_SHIFT]      = GetString( STR_KEYBOARD_KEY_SHIFT );
    strKeyboardStringTable[XBKEYBOARD_STR_KEY_CAPSLOCK]   = GetString( STR_KEYBOARD_KEY_CAPSLOCK );
    strKeyboardStringTable[XBKEYBOARD_STR_KEY_ALPHABET]   = GetString( STR_KEYBOARD_KEY_ALPHABET );
    strKeyboardStringTable[XBKEYBOARD_STR_KEY_SYMBOLS]    = GetString( STR_KEYBOARD_KEY_SYMBOLS );
    strKeyboardStringTable[XBKEYBOARD_STR_KEY_ACCENTS]    = GetString( STR_KEYBOARD_KEY_ACCENTS );
    strKeyboardStringTable[XBKEYBOARD_STR_KEY_DONE]       = GetString( STR_KEYBOARD_KEY_DONE );

    // Setup a keyboard font (this is done this way so that the underlying
    // CXBKeyboard class is font-agnostic and independant of UIX).
    m_pKeyboardFont = new CUIXKeyboardFont( pParams->pKeyboardFont );

    // Finally, create the underlying CXBKeyboard class
    m_pKeyboard = new CXBKeyboard( XBKEYBOARD_TYPE_LATIN, m_pKeyboardFont, pKeyTexture, 
                                   strKeyboardStringTable, dwKeyboardColorTable );
}




//-----------------------------------------------------------------------------
// Name: ~CUIXKeyboardScreen()
// Desc: 
//-----------------------------------------------------------------------------
CUIXKeyboardScreen::~CUIXKeyboardScreen()
{
    // Destroy any allocated resources
    if( m_pKeyboard )
        delete m_pKeyboard;

    if( m_pKeyboardFont )
        delete m_pKeyboardFont;

    m_pContext->pEngineInternal->DestroyScreen( m_KeyboardScreenObj );
}




//-----------------------------------------------------------------------------
// Name: 
// Desc: 
//-----------------------------------------------------------------------------
HRESULT CUIXKeyboardScreen::CreateScreen()
{
    IUIXEngineInternal* pInt = m_pContext->pEngineInternal;

    // Create objects
    pInt->CreateObject( &m_BackgroundObj,    m_KeyboardScreenObj, UIX_OBJECT_BACKGROUND, KEYBOARD_SCREEN_SCREEN );
    pInt->CreateObject( &m_OutputTextBoxObj, m_KeyboardScreenObj, UIX_OBJECT_TEXTBOX,    KEYBOARD_SCREEN_TEXTBOX );
    pInt->CreateObject( &m_SelectTextBoxObj, m_KeyboardScreenObj, UIX_OBJECT_TEXTBOX,    KEYBOARD_SCREEN_SELECTBUTTON );
    pInt->CreateObject( &m_HelpTextBoxObj,   m_KeyboardScreenObj, UIX_OBJECT_TEXTBOX,    KEYBOARD_SCREEN_HELPBUTTON );
    pInt->CreateObject( &m_BackTextBoxObj,   m_KeyboardScreenObj, UIX_OBJECT_TEXTBOX,    KEYBOARD_SCREEN_BACKBUTTON );

    // Populate objects
    pInt->SetTextWithResID( m_KeyboardScreenObj, m_SelectTextBoxObj, STR_KEYBOARD_HINT_SELECT );
    pInt->SetTextWithResID( m_KeyboardScreenObj, m_HelpTextBoxObj,   STR_KEYBOARD_HINT_HELP );
    pInt->SetTextWithResID( m_KeyboardScreenObj, m_BackTextBoxObj,   STR_KEYBOARD_HINT_BACK );

    // Enable screen input
    pInt->EnableScreenInput( m_KeyboardScreenObj, TRUE );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Output()
// Desc: This is the call that renders the UIX screen
//-----------------------------------------------------------------------------
VOID CUIXKeyboardScreen::Output()
{
    ITitleUIPlugin* pUI  = m_pContext->pUIPlugin;

    // Render the static objects
    pUI->RenderObject( m_BackgroundObj );
    pUI->RenderObject( m_SelectTextBoxObj );
    pUI->RenderObject( m_HelpTextBoxObj );
    pUI->RenderObject( m_BackTextBoxObj );

    // Show text and blinking caret
    {
        const double CARET_BLINK_RATE = 1.00;
        const double CARET_ON_RATIO   = 0.75;
        
        UIX_SKIN_ICON_INFO IconInfo;
        IconInfo.InsertPosInText = m_pKeyboardData->dwCursorPos;
        IconInfo.IconResID       = IMG_CURSOR;
        IconInfo.Flags           = UIX_ICON_CURSOR;

        // RXDK: const-correct (a WCHAR* cannot bind the L" " literal in standard C++)
        const WCHAR* strText = m_pKeyboardData->strBuffer[0] ? m_pKeyboardData->strBuffer : L" ";

        // Set the text with or without the cursor icon 
        if( fmod( GetTimeInSeconds(), CARET_BLINK_RATE ) < CARET_ON_RATIO )
            pUI->SetText( m_OutputTextBoxObj, 0L, strText, 1, &IconInfo );
        else
            pUI->SetText( m_OutputTextBoxObj, 0L, strText, 0, &IconInfo );

        // Render the text
        pUI->RenderObject( m_OutputTextBoxObj );
    }

    // Render the keyboard
    m_pKeyboard->RenderKeyboard( m_pKeyboardLayout->X, m_pKeyboardLayout->Y );
}




//-----------------------------------------------------------------------------
// Name: Input()
// Desc: Called when the app calls ILiveEngine::DoWork() and valid input is
//       waiting
//-----------------------------------------------------------------------------
VOID CUIXKeyboardScreen::Input( DWORD dwPort, UIX_INPUT_TYPE InputKey )
{
    // Only handle input from a supported port
    if( m_pKeyboardData->dwLockInputToPort != dwPort && 
        m_pKeyboardData->dwLockInputToPort != UIX_INVALID_VALUE )
        return;

    IUIXEngineInternal* pInt = m_pContext->pEngineInternal;

    // TODO: Rather than put code here that noone will use, controller in/out
    // events are not handled by this sample. You will need to put hooks into 
    // your own controller-management code here.
    if( InputKey == UIX_INPUT_CONTROLLER_OUT )
        return;
    if( InputKey == UIX_INPUT_ALL_CONTROLLERS_OUT )
        return;
    if( InputKey == UIX_INPUT_CONTROLLER_IN )
        return;

    if( InputKey == UIX_INPUT_Y )
    {
        // Show help when user presses the Y button
        pInt->PlaySound( AUDIO_KEYBOARD_ACTION );

        // Create the UIX help screen
        WCHAR* strHelpStringTable[UIXHELP_STR_MAX] = {0};
        strHelpStringTable[UIXHELP_STR_A_BUTTON]            = GetString( STR_KEYBOARD_HELP_SELECT );
        strHelpStringTable[UIXHELP_STR_B_BUTTON]            = GetString( STR_KEYBOARD_HELP_CANCEL );
        strHelpStringTable[UIXHELP_STR_X_BUTTON]            = GetString( STR_KEYBOARD_HELP_TOGGLE_MODE );
        strHelpStringTable[UIXHELP_STR_Y_BUTTON]            = GetString( STR_KEYBOARD_HELP_DISPLAY_HELP );
        strHelpStringTable[UIXHELP_STR_WHITE_BUTTON]        = GetString( STR_KEYBOARD_HELP_SPACE );
        strHelpStringTable[UIXHELP_STR_BLACK_BUTTON]        = GetString( STR_KEYBOARD_HELP_BACKSPACE );
        strHelpStringTable[UIXHELP_STR_LEFT_MISC_CALLOUT]   = GetString( STR_KEYBOARD_HELP_TRIGGERS_MOVE_CURSOR );

        m_pFeature->ReceiveMessage( UIX_KEYBOARD_FEATUREMSG_DISPLAYHELP, strHelpStringTable, NULL, m_pFeature );
        return;
    }
    else if( InputKey == UIX_INPUT_BACK || InputKey == UIX_INPUT_B )
    {
        // Exit the keyboard feature when the user hits BACK or B
        pInt->PlaySound( AUDIO_KEYBOARD_BACK );
        m_pContext->pEngine->EndFeature();
        return;
    }
    else
    {
        // Convert the input event code to something the keyboard understands
        XBKEYBOARD_EVENT ev = XBKEYBOARD_EVENT_NONE;
        switch( InputKey )
        {
            case UIX_INPUT_START:           ev = XBKEYBOARD_EVENT_SELECT; break;
            case UIX_INPUT_A:               ev = XBKEYBOARD_EVENT_SELECT; break;
            case UIX_INPUT_X:               ev = XBKEYBOARD_EVENT_TOGGLEKEYS; break;
            case UIX_INPUT_UP:              ev = XBKEYBOARD_EVENT_UP; break;
            case UIX_INPUT_DPAD_UP:         ev = XBKEYBOARD_EVENT_UP; break;
            case UIX_INPUT_DOWN:            ev = XBKEYBOARD_EVENT_DOWN; break;
            case UIX_INPUT_DPAD_DOWN:       ev = XBKEYBOARD_EVENT_DOWN; break;
            case UIX_INPUT_LEFT:            ev = XBKEYBOARD_EVENT_LEFT; break;
            case UIX_INPUT_DPAD_LEFT:       ev = XBKEYBOARD_EVENT_LEFT; break;
            case UIX_INPUT_RIGHT:           ev = XBKEYBOARD_EVENT_RIGHT; break;
            case UIX_INPUT_DPAD_RIGHT:      ev = XBKEYBOARD_EVENT_RIGHT; break;
            case UIX_INPUT_LEFT_TRIGGER:    ev = XBKEYBOARD_EVENT_CURSORLEFT; break;
            case UIX_INPUT_RIGHT_TRIGGER:   ev = XBKEYBOARD_EVENT_CURSORRIGHT; break;
            case UIX_INPUT_WHITE:           ev = XBKEYBOARD_EVENT_SPACE; break;
            case UIX_INPUT_BLACK:           ev = XBKEYBOARD_EVENT_BACKSPACE; break;
        }

        // Pass input to the keyboard
        DWORD dwKeyPress = 0;
        if( S_OK == m_pKeyboard->HandleEvent( ev, &dwKeyPress ) )
        {
            switch( dwKeyPress )
            {
                case XK_NULL:
                    break;

                case XK_UP:
                case XK_DOWN:
                case XK_LEFT:
                case XK_RIGHT:
                    // Currently selected key has changed
                    pInt->PlaySound( AUDIO_KEYBOARD_KEYPRESS );
                    break;

                case XK_BACKSPACE:
                    // Backspace over the current character in the text string
                    if( m_pKeyboardData->dwCursorPos > 0 )
                    {
                        m_pKeyboardData->dwCursorPos--; // move the caret
                        wcscpy( m_pKeyboardData->strBuffer+m_pKeyboardData->dwCursorPos, m_pKeyboardData->strBuffer+m_pKeyboardData->dwCursorPos+1 ); 
                    }
                    pInt->PlaySound( AUDIO_KEYBOARD_KEYPRESS );
                    break;

                case XK_DELETE:
                    // Delete the current character from the text string
                    if( wcslen( m_pKeyboardData->strBuffer ) > 0 )
                    {
                        wcscpy( m_pKeyboardData->strBuffer+m_pKeyboardData->dwCursorPos, m_pKeyboardData->strBuffer+m_pKeyboardData->dwCursorPos+1 ); 
                    }
                    pInt->PlaySound( AUDIO_KEYBOARD_KEYPRESS );
                    break;

                case XK_ARROWLEFT:
                    // Move the cursor to the left
                    if( m_pKeyboardData->dwCursorPos > 0 )
                        m_pKeyboardData->dwCursorPos--;
                    pInt->PlaySound( AUDIO_KEYBOARD_KEYPRESS );
                    break;

                case XK_ARROWRIGHT:
                    // Move the cursor to the right
                    if( m_pKeyboardData->dwCursorPos < wcslen( m_pKeyboardData->strBuffer ) )
                        m_pKeyboardData->dwCursorPos++;
                    pInt->PlaySound( AUDIO_KEYBOARD_KEYPRESS );
                    break;

                case XK_OK:
                    // User hit "DONE"
                    pInt->PlaySound( AUDIO_KEYBOARD_DONE );
                    m_pContext->pEngine->EndFeature();
                    break;

                default:
                    // Add the character to the text sting
                    if( wcslen( m_pKeyboardData->strBuffer ) < m_pKeyboardData->dwBufferSize-1 )
                    {
                        for( UINT i = wcslen(m_pKeyboardData->strBuffer)+1; i > m_pKeyboardData->dwCursorPos; i-- )
                            m_pKeyboardData->strBuffer[i] = m_pKeyboardData->strBuffer[i-1];
                        m_pKeyboardData->strBuffer[m_pKeyboardData->dwCursorPos++] = (WCHAR)dwKeyPress;
                    }
                    pInt->PlaySound( AUDIO_KEYBOARD_KEYPRESS );
                    break;
            }
        }
    }
}




