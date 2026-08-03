//-----------------------------------------------------------------------------
// File: UIXKeyboardFeature.cpp
//
// Desc: Implements the UIX keyboard feature. See header file for details.
//
// Hist: 11.25.03 - New for December 2003 XDK release
//
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#include "UIXKeyboardFeature.h"
#include "UIXKeyboardScreen.h"
#include "UIXHelpScreen.h"




//-----------------------------------------------------------------------------
// UIX Feature support
//-----------------------------------------------------------------------------
extern const UIX_FEATURE    UIX_KEYBOARD_FEATURE;


__declspec(selectany) CUIXKeyboardFeature g_UIXKeyboardFeature;

const UIX_FEATURE UIX_KEYBOARD_FEATURE = (UIX_FEATURE)(&g_UIXKeyboardFeature);




//-----------------------------------------------------------------------------
// Name: ActivateFeature()
// Desc: Called when the app calls ILiveEngine::StartFeature()
//-----------------------------------------------------------------------------
HRESULT CUIXKeyboardFeature::ActivateFeature( const VOID* pParams )
{
    // Check the parameters
    UIX_KEYBOARD_DATA* pKeyboardData = (UIX_KEYBOARD_DATA*)pParams;
    if( NULL == pKeyboardData->strBuffer || 0L == pKeyboardData->dwBufferSize )
        return E_INVALIDARG;
    
    // Create the needed UIX screens
    m_pKeyboardScreen = new CUIXKeyboardScreen( this, m_pContext, 
                                                pKeyboardData );
    
    m_pUIXHelpScreen = new CUIXHelpScreen( m_pContext );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: HibernateFeature()
// Desc: 
//-----------------------------------------------------------------------------
VOID CUIXKeyboardFeature::HibernateFeature()
{
    if( m_pKeyboardScreen ) 
        delete m_pKeyboardScreen;
    m_pKeyboardScreen = NULL;

    if( m_pUIXHelpScreen )
        delete m_pUIXHelpScreen;
    m_pUIXHelpScreen = NULL;
}




//-----------------------------------------------------------------------------
// Name: ReceiveMessage()
// Desc: Let the feature handle various messages
//-----------------------------------------------------------------------------
HRESULT CUIXKeyboardFeature::ReceiveMessage( UIX_FEATUREMSG_TYPE Msg,
                                             const VOID* pParam, VOID* pResult,
                                             IUIXFeature* pFromFeature )
{
    switch( Msg ) 
    {
        // Disaply the help screen
        case UIX_KEYBOARD_FEATUREMSG_DISPLAYHELP:
        {
            // Set the help strings
            WCHAR** strHelpStringTable = (WCHAR**)pParam;
            m_pUIXHelpScreen->SetStringTable( strHelpStringTable );

            // Show the help screen
            IUIXEngineInternal* pInt = m_pContext->pEngineInternal;
            pInt->EnableScreenInput( m_pUIXHelpScreen->GetScreenObject(), TRUE );
            pInt->ShowScreen( m_pUIXHelpScreen->GetScreenObject(), FALSE );

            break;
        }

        // Get exit info for the feature
        case UIX_FEATUREMSG_GET_EXIT_INFO:
        {
            // TODO: We should extract the text string from the screen
            UIX_EXIT_INFO* pExitInfo = (UIX_EXIT_INFO*)pResult;
            pExitInfo->ExitCode  = UIX_EXIT_KEYBOARD_OK;
            pExitInfo->hr        = S_OK;
            pExitInfo->pExitData = NULL;
            break;
        }
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Remaining IUIXFeature methods
//-----------------------------------------------------------------------------

HRESULT CUIXKeyboardFeature::CreateFeature()
{
    return S_OK;
}


VOID CUIXKeyboardFeature::DestroyFeature()
{
}


VOID CUIXKeyboardFeature::PumpTasks()
{
}


VOID CUIXKeyboardFeature::Reset()
{
}


HRESULT CUIXKeyboardFeature::GetCurrentScreen( UIX_SCREEN* pScreen )
{
    (*pScreen) = m_pKeyboardScreen->GetScreenObject();
    return S_OK;
}


HRESULT CUIXKeyboardFeature::GetType( UIX_FEATURE_TYPE* pType )
{
    (*pType) = UIX_FEATURE_TYPE_EXTENSION;
    return S_OK;
}


HRESULT CUIXKeyboardFeature::GetFeatureInterface( const VOID* pParam, VOID** ppInterface )
{
    (*ppInterface) = (VOID*)this;
    return S_OK;
}


HRESULT CUIXKeyboardFeature::SetContext( UIX_FEATURE_CONTEXT* pContext )
{
    m_pContext = pContext;
    return S_OK;
}
