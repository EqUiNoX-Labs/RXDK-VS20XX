//-----------------------------------------------------------------------------
// File: UserInterface.h
//
// Desc: ContentDownload rendering functions
//
// Hist: 09.07.01 - Updated for Nov release
//       04.05.02 - Updated for May release; Added billable content and content
//                                           details.  Updated for the new
//                                           HD/DVD content enumeration API
//       06.05.02 - Updated for June release; Updated billing stuctures
//                                            Added removal of "bad" content
// Copyright (c) Microsoft Corporation. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef USERINTERFACE_H
#define USERINTERFACE_H

#include "Common.h"
#include "XBFont.h"
#include "XBOnline.h"




static const D3DCOLOR COLOR_HIGHLIGHT = 0xffffff00; // Yellow
static const D3DCOLOR COLOR_GREEN     = 0xff00ff00; // Green
static const D3DCOLOR COLOR_NORMAL    = 0xffffffff; // White




//-----------------------------------------------------------------------------
// Name: class UserInterface
// Desc: Main UI class for ContentDownload sample
//-----------------------------------------------------------------------------
class UserInterface
{
    mutable CXBFont     m_Font;                      // Game font
    mutable CXBFont     m_OnlineIconsFont;
    
    VOID RenderProgress( const D3DXVECTOR4&, FLOAT );

public:
    UserInterface();

    HRESULT Initialize();

    // UI functions
    VOID RenderCreateAccount( BOOL bHasMachineAccount );
    VOID RenderSelectAccount( DWORD, const XBUserList& );
    VOID RenderSelectDevice( DWORD );
    VOID RenderSelectContent( const ContentList&, DWORD, DWORD );
    VOID RenderContentDetails( ContentInfo&, BOOL bBillingEnabled );
    VOID RenderContentMetadata( ContentInfo& );
    VOID RenderLoggingOn();
    VOID RenderConfirm( const WCHAR* strMessage, DWORD dwCurrItem );
    VOID RenderInstallContent( FLOAT, DWORD, DWORD );
    VOID RenderMessage( WCHAR* strMessage );
    VOID RenderMessage( const WCHAR* strMessage, 
                        BOOL bCancellable, BOOL bContinue );
};


#endif // USERINTERFACE_H
