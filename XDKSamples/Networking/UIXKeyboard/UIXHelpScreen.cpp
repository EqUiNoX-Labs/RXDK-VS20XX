//-----------------------------------------------------------------------------
// File: UIXHelpScreen.cpp
//
// Desc: Implements the UIX help screen. See the header file for details.
//
// Hist: 11.25.03 - New for December 2003 XDK release
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "UIXHelpScreen.h"
#include "sk_res.h"




//-----------------------------------------------------------------------------
// Name: CUIXHelpScreen()
// Desc: 
//-----------------------------------------------------------------------------
CUIXHelpScreen::CUIXHelpScreen( UIX_FEATURE_CONTEXT* pContext )
{
    m_pContext = pContext;

    m_pContext->pEngineInternal->CreateScreen( &m_ScreenObj, SECTION_HELP_SCREEN, this );
}




//-----------------------------------------------------------------------------
// Name: ~CUIXKeyboardScreen()
// Desc: 
//-----------------------------------------------------------------------------
CUIXHelpScreen::~CUIXHelpScreen()
{
    m_pContext->pEngineInternal->DestroyScreen( m_ScreenObj );
}




//-----------------------------------------------------------------------------
// Name: 
// Desc: 
//-----------------------------------------------------------------------------
HRESULT CUIXHelpScreen::CreateScreen()
{
    IUIXEngineInternal* pInt = m_pContext->pEngineInternal;

    // Create the background object
    pInt->CreateObject( &m_BackgroundObj, m_ScreenObj, UIX_OBJECT_BACKGROUND, HELP_SCREEN_SCREEN );

    // Create the text objects
    pInt->CreateObject( &m_TextObjArray[UIXHELP_STR_LEFT_THUMBSTICK],     m_ScreenObj, UIX_OBJECT_TEXTBOX, HELP_SCREEN_LEFTTHUMBSTICK_TEXT );
    pInt->CreateObject( &m_TextObjArray[UIXHELP_STR_RIGHT_THUMBSTICK],    m_ScreenObj, UIX_OBJECT_TEXTBOX, HELP_SCREEN_RIGHTTHUMBSTICK_TEXT );
    pInt->CreateObject( &m_TextObjArray[UIXHELP_STR_DPAD],                m_ScreenObj, UIX_OBJECT_TEXTBOX, HELP_SCREEN_DPAD_TEXT );
    pInt->CreateObject( &m_TextObjArray[UIXHELP_STR_BACK_BUTTON],         m_ScreenObj, UIX_OBJECT_TEXTBOX, HELP_SCREEN_BACKBUTTON_TEXT );
    pInt->CreateObject( &m_TextObjArray[UIXHELP_STR_START_BUTTON],        m_ScreenObj, UIX_OBJECT_TEXTBOX, HELP_SCREEN_STARTBUTTON_TEXT );
    pInt->CreateObject( &m_TextObjArray[UIXHELP_STR_A_BUTTON],            m_ScreenObj, UIX_OBJECT_TEXTBOX, HELP_SCREEN_ABUTTON_TEXT );
    pInt->CreateObject( &m_TextObjArray[UIXHELP_STR_B_BUTTON],            m_ScreenObj, UIX_OBJECT_TEXTBOX, HELP_SCREEN_BBUTTON_TEXT );
    pInt->CreateObject( &m_TextObjArray[UIXHELP_STR_X_BUTTON],            m_ScreenObj, UIX_OBJECT_TEXTBOX, HELP_SCREEN_XBUTTON_TEXT );
    pInt->CreateObject( &m_TextObjArray[UIXHELP_STR_Y_BUTTON],            m_ScreenObj, UIX_OBJECT_TEXTBOX, HELP_SCREEN_YBUTTON_TEXT );
    pInt->CreateObject( &m_TextObjArray[UIXHELP_STR_WHITE_BUTTON],        m_ScreenObj, UIX_OBJECT_TEXTBOX, HELP_SCREEN_WHITEBUTTON_TEXT );
    pInt->CreateObject( &m_TextObjArray[UIXHELP_STR_BLACK_BUTTON],        m_ScreenObj, UIX_OBJECT_TEXTBOX, HELP_SCREEN_BLACKBUTTON_TEXT );
    pInt->CreateObject( &m_TextObjArray[UIXHELP_STR_LEFT_TRIGGER],        m_ScreenObj, UIX_OBJECT_TEXTBOX, HELP_SCREEN_LEFTTRIGGER_TEXT );
    pInt->CreateObject( &m_TextObjArray[UIXHELP_STR_RIGHT_TRIGGER],       m_ScreenObj, UIX_OBJECT_TEXTBOX, HELP_SCREEN_RIGHTTRIGGER_TEXT );
    pInt->CreateObject( &m_TextObjArray[UIXHELP_STR_LEFT_MISC_CALLOUT],   m_ScreenObj, UIX_OBJECT_TEXTBOX, HELP_SCREEN_LEFTCALLOUT_TEXT );
    pInt->CreateObject( &m_TextObjArray[UIXHELP_STR_CENTER_MISC_CALLOUT], m_ScreenObj, UIX_OBJECT_TEXTBOX, HELP_SCREEN_CENTERCALLOUT_TEXT );
    pInt->CreateObject( &m_TextObjArray[UIXHELP_STR_RIGHT_MISC_CALLOUT],  m_ScreenObj, UIX_OBJECT_TEXTBOX, HELP_SCREEN_RIGHTCALLOUT_TEXT );

    // Store convenient access to the layout info for each of the callouts
    m_pContext->pPluginSupport->GetLayout( SECTION_HELP_SCREEN, HELP_SCREEN_LEFTTHUMBSTICK,  &m_pCallouts[UIXHELP_STR_LEFT_THUMBSTICK] );
    m_pContext->pPluginSupport->GetLayout( SECTION_HELP_SCREEN, HELP_SCREEN_RIGHTTHUMBSTICK, &m_pCallouts[UIXHELP_STR_RIGHT_THUMBSTICK] );
    m_pContext->pPluginSupport->GetLayout( SECTION_HELP_SCREEN, HELP_SCREEN_DPAD,            &m_pCallouts[UIXHELP_STR_DPAD] );
    m_pContext->pPluginSupport->GetLayout( SECTION_HELP_SCREEN, HELP_SCREEN_STARTBUTTON,     &m_pCallouts[UIXHELP_STR_START_BUTTON] );
    m_pContext->pPluginSupport->GetLayout( SECTION_HELP_SCREEN, HELP_SCREEN_BACKBUTTON,      &m_pCallouts[UIXHELP_STR_BACK_BUTTON] );
    m_pContext->pPluginSupport->GetLayout( SECTION_HELP_SCREEN, HELP_SCREEN_ABUTTON,         &m_pCallouts[UIXHELP_STR_A_BUTTON] );
    m_pContext->pPluginSupport->GetLayout( SECTION_HELP_SCREEN, HELP_SCREEN_BBUTTON,         &m_pCallouts[UIXHELP_STR_B_BUTTON] );
    m_pContext->pPluginSupport->GetLayout( SECTION_HELP_SCREEN, HELP_SCREEN_XBUTTON,         &m_pCallouts[UIXHELP_STR_X_BUTTON] );
    m_pContext->pPluginSupport->GetLayout( SECTION_HELP_SCREEN, HELP_SCREEN_YBUTTON,         &m_pCallouts[UIXHELP_STR_Y_BUTTON] );
    m_pContext->pPluginSupport->GetLayout( SECTION_HELP_SCREEN, HELP_SCREEN_WHITEBUTTON,     &m_pCallouts[UIXHELP_STR_WHITE_BUTTON] );
    m_pContext->pPluginSupport->GetLayout( SECTION_HELP_SCREEN, HELP_SCREEN_BLACKBUTTON,     &m_pCallouts[UIXHELP_STR_BLACK_BUTTON] );
    m_pContext->pPluginSupport->GetLayout( SECTION_HELP_SCREEN, HELP_SCREEN_LEFTTRIGGER,     &m_pCallouts[UIXHELP_STR_LEFT_TRIGGER] );
    m_pContext->pPluginSupport->GetLayout( SECTION_HELP_SCREEN, HELP_SCREEN_RIGHTTRIGGER,    &m_pCallouts[UIXHELP_STR_RIGHT_TRIGGER] );
    m_pContext->pPluginSupport->GetLayout( SECTION_HELP_SCREEN, HELP_SCREEN_LEFTCALLOUT,     &m_pCallouts[UIXHELP_STR_LEFT_MISC_CALLOUT] );
    m_pContext->pPluginSupport->GetLayout( SECTION_HELP_SCREEN, HELP_SCREEN_CENTERCALLOUT,   &m_pCallouts[UIXHELP_STR_CENTER_MISC_CALLOUT] );
    m_pContext->pPluginSupport->GetLayout( SECTION_HELP_SCREEN, HELP_SCREEN_RIGHTCALLOUT,    &m_pCallouts[UIXHELP_STR_RIGHT_MISC_CALLOUT] );

    // Create a texture for drawing the callout lines
    m_pContext->pPluginSupport->GetImage( IMG_HELP_CALLOUT, &m_pCalloutTexture );

    // Enable screen input
    pInt->EnableScreenInput( m_ScreenObj, TRUE );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Output()
// Desc: This is the call that renders the UIX screen
//-----------------------------------------------------------------------------
VOID CUIXHelpScreen::SetStringTable( WCHAR** strStringTable )
{
    memcpy( m_strStringTable, strStringTable, sizeof(m_strStringTable) );

    // Set the text for each ofthe objects
    for( DWORD i=0; i<UIXHELP_STR_MAX; i++ )
    {
        m_pContext->pEngineInternal->SetText( m_ScreenObj, m_TextObjArray[i], m_strStringTable[i] );
    }
}




//-----------------------------------------------------------------------------
// Name: Output()
// Desc: This is the call that renders the UIX screen
//-----------------------------------------------------------------------------
VOID CUIXHelpScreen::Output()
{
    ITitleUIPlugin* pUI = m_pContext->pUIPlugin;

    // Draw the background
    pUI->RenderObject( m_BackgroundObj );

    // Draw the array of text callouts
    for( DWORD i=0; i<UIXHELP_STR_MAX; i++ )
    {
        if( m_strStringTable[i] )
        {
            pUI->RenderObject( m_TextObjArray[i] );
        }
    }

    // Set state to draw the lines
    D3DDevice::SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
    D3DDevice::SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    D3DDevice::SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
    D3DDevice::SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE );
    D3DDevice::SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
    D3DDevice::SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
    D3DDevice::SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );
    D3DDevice::SetTextureStageState( 1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
    D3DDevice::SetTextureStageState( 0, D3DTSS_ADDRESSU,  D3DTADDRESS_CLAMP );
    D3DDevice::SetTextureStageState( 0, D3DTSS_ADDRESSV,  D3DTADDRESS_CLAMP );
    D3DDevice::SetTextureStageState( 0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR );
    D3DDevice::SetTextureStageState( 0, D3DTSS_MINFILTER, D3DTEXF_LINEAR );
    D3DDevice::SetRenderState( D3DRS_ZENABLE,      FALSE );
    D3DDevice::SetRenderState( D3DRS_FOGENABLE,    FALSE );
    D3DDevice::SetRenderState( D3DRS_FOGTABLEMODE, D3DFOG_NONE );
    D3DDevice::SetRenderState( D3DRS_FILLMODE,     D3DFILL_SOLID );
    D3DDevice::SetRenderState( D3DRS_CULLMODE,     D3DCULL_CCW );
    D3DDevice::SetRenderState( D3DRS_ALPHABLENDENABLE, TRUE );
    D3DDevice::SetRenderState( D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA );
    D3DDevice::SetRenderState( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
    D3DDevice::SetTexture( 0, m_pCalloutTexture );
    D3DDevice::SetVertexShader( D3DFVF_XYZRHW );
    D3DDevice::Begin( D3DPT_QUADLIST );

    for( DWORD i=0; i<UIXHELP_STR_MAX; i++ )
    {
        // If there's an associated string
        if( m_strStringTable[i] )
        {
            // Determine the line start and end positions
            D3DXVECTOR2 a ( m_pCallouts[i]->X,     m_pCallouts[i]->Y );
            D3DXVECTOR2 b ( m_pCallouts[i]->Width, m_pCallouts[i]->Height );
            D3DXVECTOR2 c ( b.y-a.y, a.x-b.x );
            D3DXVec2Normalize( &c, &c );

            // Draw the callout line
            D3DDevice::SetVertexData2f( D3DVSDE_TEXCOORD0, 0.0f, 0.0f );
            D3DDevice::SetVertexData4f( D3DVSDE_VERTEX,    a.x-2*c.x, a.y-2*c.y, 0.0f, 0.0f );
            D3DDevice::SetVertexData2f( D3DVSDE_TEXCOORD0, 1.0f, 0.0f );
            D3DDevice::SetVertexData4f( D3DVSDE_VERTEX,    a.x+2*c.x, a.y+2*c.y, 0.0f, 0.0f );
            D3DDevice::SetVertexData2f( D3DVSDE_TEXCOORD0, 1.0f, 1.0f );
            D3DDevice::SetVertexData4f( D3DVSDE_VERTEX,    b.x+2*c.x, b.y+2*c.y, 0.0f, 0.0f );
            D3DDevice::SetVertexData2f( D3DVSDE_TEXCOORD0, 0.0f, 1.0f );
            D3DDevice::SetVertexData4f( D3DVSDE_VERTEX,    b.x-2*c.x, b.y-2*c.y, 0.0f, 0.0f );
        }
    }

    // Finish drawing the lines
    D3DDevice::End();
}




//-----------------------------------------------------------------------------
// Name: Input()
// Desc: Called when the app calls ILiveEngine::DoWork() and valid input is
//       waiting
//-----------------------------------------------------------------------------
VOID CUIXHelpScreen::Input( DWORD Port, UIX_INPUT_TYPE InputKey )
{
    // Any button exits the help screen
    m_pContext->pEngineInternal->HideTopScreen();
}




